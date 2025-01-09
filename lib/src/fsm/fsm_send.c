#include <errno.h>
#include "microtcp.h"
#include <unistd.h>
#include "core/misc.h"
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "settings/microtcp_settings.h"
#include "fsm_common.h"
#include "logging/microtcp_logger.h"
#include "microtcp_defines.h"
#include "smart_assert.h"
#include "microtcp_core_macros.h"
#include "logging/microtcp_fsm_logger.h"

#define RECVFROM_SHUTDOWN (0)
#define RECVFROM_ERROR (-1)
#define DUPLICATE_ACK_COUNT_FOR_FAST_RETRANSMIT 3

// typedef enum
// {
//         CONTINUE_SUBSTATE,
//         SLOW_START_SUBSTATE, /* ENTRY_POINT */
//         CONGESTION_AVOIDANCE_SUBSTATE,
//         RWND_PROBING_SUBSTATE,
//         RECEIVED_FINACK_SUBSTATE,
//         RECEIVED_RST_SUBSTATE,
//         EXIT_SUCCESS_SUBSTATE,      /* Terminal substate (success). FSM exit point. */
//         EXIT_FAILURE_SUBSTATE = -1, /* Terminal substate (failure). FSM exit point. */
// } send_fsm_substates_t;

typedef enum
{
        ALGORITHM_SLOW_START,
        ALGORITHM_CONGESTION_AVOIDANCE
} send_algorithm_t;

typedef enum
{
        SS_COMPLETION,
        SS_FAILURE,
        SS_FINACK_RECEPTION,
        SS_RST_RECEPTION
} send_status_t;

typedef struct
{
        const void *buffer;
        size_t remaining;
        uint8_t duplicate_ack_count;
        send_algorithm_t current_send_algorithm;

} send_context_t;

static __always_inline void respond_to_triple_dup_ack(microtcp_sock_t *_socket, send_context_t *_context);
static __always_inline void respond_to_timeout(microtcp_sock_t *_socket, send_context_t *_context);
static __always_inline void handle_cwnd_increment(microtcp_sock_t *_socket, const send_context_t *_context, size_t _acked_segments);
static __always_inline void handle_seq_number_increment(microtcp_sock_t *_socket, uint32_t _received_ack_number, size_t _acked_segments);
static __always_inline void handle_peer_win_size(microtcp_sock_t *_socket);
static __always_inline uint32_t get_most_recent_ack(uint32_t _ack1, uint32_t _ack2);

/* FAST_RETRANSMIT: response to 3 dup ACK. */
static __always_inline void respond_to_triple_dup_ack(microtcp_sock_t *const _socket, send_context_t *const _context)
{
        const send_queue_node_t *retransmission_node = sq_front(_socket->send_queue);
        send_data_segment(_socket, retransmission_node->buffer, retransmission_node->segment_size, retransmission_node->seq_number);
        _socket->ssthresh = MAX(MICROTCP_MSS, _socket->cwnd / 2);
        _socket->cwnd = MICROTCP_MSS;
        _context->duplicate_ack_count = 0;
        _context->current_send_algorithm = ALGORITHM_SLOW_START;
}

static __always_inline void respond_to_timeout(microtcp_sock_t *const _socket, send_context_t *const _context)
{
        _socket->ssthresh = MAX(_socket->cwnd / 2, MICROTCP_MSS);
        _socket->cwnd = MICROTCP_MSS;
        _context->duplicate_ack_count = 0;
        _context->current_send_algorithm = ALGORITHM_SLOW_START;
}

static __always_inline void handle_cwnd_increment(microtcp_sock_t *const _socket, const send_context_t *const _context, const size_t _acked_segments)
{
        DEBUG_SMART_ASSERT(_context->current_send_algorithm == ALGORITHM_SLOW_START ||
                           _context->current_send_algorithm == ALGORITHM_CONGESTION_AVOIDANCE);
        if (_context->current_send_algorithm == ALGORITHM_SLOW_START)
                _socket->cwnd += _acked_segments * MICROTCP_MSS;
        else if (_context->current_send_algorithm == ALGORITHM_CONGESTION_AVOIDANCE)
                for (size_t i = 0; i < _acked_segments; i++)
                        _socket->cwnd += MAX((MICROTCP_MSS * MICROTCP_MSS) / _socket->cwnd, 1); /* If CWND > MSS^2, increament by 1 byte (tahoe). */
}

static __always_inline void handle_seq_number_increment(microtcp_sock_t *const _socket, const uint32_t _received_ack_number, const size_t _acked_segments)
{
        if (_acked_segments > 0) /* That means received_ack_number matched to a sent segment. */
                _socket->seq_number = _received_ack_number;
}

static __always_inline void handle_peer_win_size(microtcp_sock_t *const _socket)
{
        _socket->peer_win_size = _socket->segment_receive_buffer->header.window - sq_stored_bytes(_socket->send_queue);
}

static inline status_t execute_send_data_round(microtcp_sock_t *const _socket, send_context_t *const _context)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _context != NULL);
        DEBUG_SMART_ASSERT(_socket->state == ESTABLISHED, _context->buffer != NULL, sq_is_empty(_socket->send_queue));

        uint32_t bytes_to_send = MIN(MIN(_socket->cwnd, _socket->peer_win_size), _context->remaining);
        uint32_t total_data_bytes_sent = 0;
        while (total_data_bytes_sent != bytes_to_send)
        {
                const size_t payload_size = MIN(bytes_to_send - total_data_bytes_sent, MICROTCP_MSS);
                const uint32_t segment_seq_number = _socket->seq_number + total_data_bytes_sent;
                const ssize_t segment_bytes_sent = send_data_segment(_socket, _context->buffer + total_data_bytes_sent, payload_size, segment_seq_number);
                if (RARE_CASE(segment_bytes_sent == SEND_SEGMENT_FATAL_ERROR))
                        return FAILURE;
                if (RARE_CASE(segment_bytes_sent == SEND_SEGMENT_ERROR))
                        continue;

                DEBUG_SMART_ASSERT((size_t)segment_bytes_sent == payload_size + MICROTCP_HEADER_SIZE);

                sq_enqueue(_socket->send_queue, segment_seq_number, payload_size, _context->buffer + total_data_bytes_sent);
                total_data_bytes_sent += (segment_bytes_sent - MICROTCP_HEADER_SIZE);
        }
        return SUCCESS;
}

static inline send_status_t receive_and_process_ack(microtcp_sock_t *const _socket, send_context_t *const _context, const _Bool _block)
{
        ssize_t recv_ack_ret_val = receive_data_ack_segment(_socket, _block);
        switch (recv_ack_ret_val)
        {
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_CARRIES_DATA:
                break;
        case RECV_SEGMENT_TIMEOUT:
                if (_block == TRUE) /* If in block, timeout timer expired. */
                {
                        respond_to_timeout(_socket, _context);
                        LOG_WARNING("Timeout occured!"); /* TIMEOUT */
                }
                break; /* Currently no UDP packets available. */
        case RECV_SEGMENT_FINACK_UNEXPECTED:
                return SS_FINACK_RECEPTION;
        case RECV_SEGMENT_RST_RECEIVED:
                return SS_RST_RECEPTION;
        case RECV_SEGMENT_FATAL_ERROR:
                return SS_FAILURE;
        default: /* Received an ACK. */
        {
                const uint32_t received_ack_number = _socket->segment_receive_buffer->header.ack_number;
                const microtcp_segment_t *control_segment = _socket->segment_receive_buffer;

                if (sq_front(_socket->send_queue)->seq_number == received_ack_number) /* check for DUPLICATE ACK */
                {
                        if (++_context->duplicate_ack_count == DUPLICATE_ACK_COUNT_FOR_FAST_RETRANSMIT)
                        {
                                respond_to_triple_dup_ack(_socket, _context);
                                LOG_WARNING("Received 3DUP_ACK"); /* TRIPLE DUPLICATE ACK */
                        }
                        break;
                }
                _context->duplicate_ack_count = 0;
                _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);
                const size_t pre_dequeue_bytes = sq_stored_bytes(_socket->send_queue);
                const size_t acked_segments = sq_dequeue(_socket->send_queue, received_ack_number);
                const size_t post_dequeue_bytes = sq_stored_bytes(_socket->send_queue);
                _context->remaining -= (pre_dequeue_bytes - post_dequeue_bytes);
                handle_seq_number_increment(_socket, received_ack_number, acked_segments);
                handle_cwnd_increment(_socket, _context, acked_segments);
                handle_peer_win_size(_socket);
        }
        }
        return SS_COMPLETION;
}

static inline send_status_t execute_receive_ack_round(microtcp_sock_t *_socket, send_context_t *_context)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _context != NULL);
        DEBUG_SMART_ASSERT(_socket->state == ESTABLISHED, _context->buffer != NULL);

        while (!sq_is_empty(_socket->send_queue))
        {
                send_status_t ss = receive_and_process_ack(_socket, _context, TRUE);
                if (ss != SS_COMPLETION)
                        return ss;
        }
        return SS_COMPLETION;

}

static inline status_t execute_interleaved_retransmissions_round(microtcp_sock_t *const _socket, send_context_t *const _context)
{
        uint32_t bytes_resent = 0;
        send_queue_node_t *curr_node = sq_front(_socket->send_queue);
        while (curr_node != NULL)
        {
                if (bytes_resent + curr_node->segment_size > _socket->cwnd) /* Hit transmission limit. */
                        break;
                update_socket_lost_counters(_socket, curr_node->segment_size);
                send_data_segment(_socket, curr_node->buffer, curr_node->segment_size, curr_node->seq_number);

                const size_t stored_segments_pre_ack = sq_stored_segments(_socket->send_queue);
                send_status_t ss = receive_and_process_ack(_socket, _context, FALSE);
                if (ss != SS_COMPLETION)
                        return ss;
                if (stored_segments_pre_ack == sq_stored_segments(_socket->send_queue)) /* No ACK, or ACK didn't match. SEND-NEXT */
                        curr_node = curr_node->next;
                else
                        curr_node = sq_front(_socket->send_queue); /* ACK, match segment in send_queue. FAST-FORWARD. */
        }
        return execute_receive_ack_round(_socket, _context);
}

// static inline int handle_data_transmission(microtcp_sock_t *_socket, send_context_t *_context);
static inline int handle_data_transmission(microtcp_sock_t *const _socket, const void *const _buffer, const size_t _length)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, ESTABLISHED);
        DEBUG_SMART_ASSERT(_buffer != NULL, _socket->send_queue != NULL);

        send_context_t context = {.buffer = _buffer,
                                  .remaining = _length,
                                  .duplicate_ack_count = 0,
                                  .current_send_algorithm = ALGORITHM_SLOW_START};

        while (context.remaining != 0)
        {
                /* Retransmit (if-any) any remaining segments in send-queue, in interleaved send/receive fashion. */
                execute_interleaved_retransmissions_round(_socket, &context);

                execute_send_data_round(_socket, &context);
                execute_receive_ack_round(_socket, &context);

                if (context.current_send_algorithm == ALGORITHM_SLOW_START && _socket->cwnd > _socket->ssthresh)
                        context.current_send_algorithm = ALGORITHM_CONGESTION_AVOIDANCE;
        }
        return _length - context.remaining; /* Return bytes-sent. */
}

static __always_inline uint32_t get_most_recent_ack(const uint32_t _ack1, const uint32_t _ack2)
{
        return (int32_t)(_ack1 - _ack2) > 0 ? _ack1 : _ack2;
}