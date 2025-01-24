#include <errno.h>
#include "microtcp.h"
#include <unistd.h>
#include "core/misc.h"
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "settings/microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "microtcp_defines.h"
#include "smart_assert.h"
#include "microtcp_core_macros.h"
#include "fsm_common.h"
#include "logging/microtcp_fsm_logger.h"

#define DUPLICATE_ACK_COUNT_FOR_FAST_RETRANSMIT 3

typedef enum
{
        ALGORITHM_SLOW_START,
        ALGORITHM_CONGESTION_AVOIDANCE
} send_algorithm_t;

typedef enum
{
        SEND_DATA_ROUND_SUBSTATE, /* Entry point. */
        RECV_ACK_ROUND_SUBSTATE,
        RETRANSMISSIONS_SUBSTATE,
        FINACK_RECEPTION_SUBSTATE,
        RST_RECEPTION_SUBSTATE,
        EXIT_SUCCESS_SUBSTATE,
        EXIT_FAILURE_SUBSTATE,
        CONTINUE_SUBSTATE,
} send_fsm_substates_t;

typedef struct
{
        const uint8_t *buffer;
        size_t remaining;
        uint8_t duplicate_ack_count;
        send_algorithm_t current_send_algorithm;
} fsm_context_t;

static const char *convert_substate_to_string(send_fsm_substates_t _substate);
static __always_inline send_fsm_substates_t respond_to_triple_dup_ack(microtcp_sock_t *_socket, fsm_context_t *_context);
static __always_inline void respond_to_timeout(microtcp_sock_t *_socket, fsm_context_t *_context);
static __always_inline void handle_cwnd_increment(microtcp_sock_t *_socket, fsm_context_t *_context, size_t _acked_segments);
static __always_inline void handle_seq_number_increment(microtcp_sock_t *_socket, uint32_t _received_ack_number, size_t _acked_segments);
static __always_inline void handle_peer_win_size(microtcp_sock_t *_socket);
static __always_inline uint32_t get_most_recent_ack(uint32_t _ack1, uint32_t _ack2);

static __always_inline ssize_t error_tolerant_send_data(microtcp_sock_t *_socket, const void *const _buffer, size_t _segment_size, uint32_t _seq_number)
{
        while (true)
        {
                ssize_t segment_bytes_sent = send_data_segment(_socket, _buffer, _segment_size, _seq_number);
                if (RARE_CASE(segment_bytes_sent == SEND_SEGMENT_FATAL_ERROR))
                        return RECV_SEGMENT_FATAL_ERROR;
                if (RARE_CASE(segment_bytes_sent == SEND_SEGMENT_ERROR))
                        continue;
                return segment_bytes_sent;
        }
}

static inline void handle_zero_peer_window(void)
{
        /* TODO: With current setup peer_window can never be 0. */
        /* As microtcp_recv reads bytes into rrb, only when, */
        /* User of microtcp_recv requests so... Thus there is a buffer to copy.. */
        ;
        SMART_ASSERT(false);
}

/* FAST_RETRANSMIT: response to 3 dup ACK. */
static __always_inline send_fsm_substates_t respond_to_triple_dup_ack(microtcp_sock_t *const _socket, fsm_context_t *const _context)
{
        const send_queue_node_t *retransmission_node = sq_front(_socket->send_queue);
        const ssize_t send_data_ret_val = error_tolerant_send_data(_socket, retransmission_node->buffer, retransmission_node->segment_size, retransmission_node->seq_number);
        if (RARE_CASE(send_data_ret_val == SEND_SEGMENT_FATAL_ERROR))
                return EXIT_FAILURE_SUBSTATE;

        _socket->ssthresh = MAX(MICROTCP_MSS, _socket->cwnd / 2);
        _socket->cwnd = MICROTCP_MSS;
        _context->duplicate_ack_count = 0;
        _context->current_send_algorithm = ALGORITHM_CONGESTION_AVOIDANCE;
        return CONTINUE_SUBSTATE;
}

static __always_inline void respond_to_timeout(microtcp_sock_t *const _socket, fsm_context_t *const _context)
{
        _socket->ssthresh = MAX(_socket->cwnd / 2, MICROTCP_MSS);
        _socket->cwnd = MICROTCP_MSS;
        _context->duplicate_ack_count = 0;
        _context->current_send_algorithm = ALGORITHM_SLOW_START;
}

static __always_inline void handle_cwnd_increment(microtcp_sock_t *const _socket, fsm_context_t *const _context, const size_t _acked_segments)
{
        DEBUG_SMART_ASSERT(_context->current_send_algorithm == ALGORITHM_SLOW_START ||
                           _context->current_send_algorithm == ALGORITHM_CONGESTION_AVOIDANCE);
        if (_context->current_send_algorithm == ALGORITHM_SLOW_START)
        {
                _socket->cwnd += _acked_segments * MICROTCP_MSS;
                if (_socket->cwnd > _socket->ssthresh)
                        _context->current_send_algorithm = ALGORITHM_CONGESTION_AVOIDANCE;
        }
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

static __always_inline uint32_t get_most_recent_ack(const uint32_t _ack1, const uint32_t _ack2)
{
        return (int32_t)(_ack1 - _ack2) > 0 ? _ack1 : _ack2;
}

static __always_inline send_fsm_substates_t handle_ack_reception(microtcp_sock_t *_socket, fsm_context_t *_context)
{
        const uint32_t received_ack_number = _socket->segment_receive_buffer->header.ack_number;
        const microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        DEBUG_SMART_ASSERT(sq_front(_socket->send_queue) != NULL);

        if (sq_front(_socket->send_queue)->seq_number == received_ack_number) /* check for DUPLICATE ACK */
        {
                if (++_context->duplicate_ack_count == DUPLICATE_ACK_COUNT_FOR_FAST_RETRANSMIT)
                {
                        if (respond_to_triple_dup_ack(_socket, _context) == EXIT_FAILURE_SUBSTATE)
                                return EXIT_FAILURE_SUBSTATE;
                        LOG_WARNING("Received 3DUP_ACK"); /* TRIPLE DUPLICATE ACK */ /* TODO: REMOVE */
                }
                return CONTINUE_SUBSTATE;
        }
        _context->duplicate_ack_count = 0;
        _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);
        const size_t pre_dequeue_bytes = sq_stored_bytes(_socket->send_queue);
        const size_t acked_segments = sq_dequeue(_socket->send_queue, received_ack_number);
        const size_t post_dequeue_bytes = sq_stored_bytes(_socket->send_queue);
        _context->remaining -= (pre_dequeue_bytes - post_dequeue_bytes);
        _context->buffer += (pre_dequeue_bytes - post_dequeue_bytes);
        handle_seq_number_increment(_socket, received_ack_number, acked_segments);
        handle_cwnd_increment(_socket, _context, acked_segments);
        handle_peer_win_size(_socket);
        if (RARE_CASE(_socket->segment_receive_buffer->header.window == 0)) /* IMPOSSIBLE_CASE ACTUALLY...*/
                handle_zero_peer_window();
        return CONTINUE_SUBSTATE;
}

static inline send_fsm_substates_t execute_send_data_round_substate(microtcp_sock_t *const _socket, fsm_context_t *const _context)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _context != NULL);
        DEBUG_SMART_ASSERT(_socket->state == ESTABLISHED, _context->buffer != NULL, sq_is_empty(_socket->send_queue));
        if (_context->remaining == 0)
                return EXIT_SUCCESS_SUBSTATE; /* EXIT point. */

        uint32_t bytes_to_send = MIN(MIN(_socket->cwnd, _socket->peer_win_size), _context->remaining);
        uint32_t total_data_bytes_sent = 0;
        while (total_data_bytes_sent != bytes_to_send)
        {
                const size_t payload_size = MIN(bytes_to_send - total_data_bytes_sent, MICROTCP_MSS);
                const uint32_t segment_seq_number = _socket->seq_number + total_data_bytes_sent;

                const ssize_t segment_bytes_sent = error_tolerant_send_data(_socket, _context->buffer + total_data_bytes_sent, payload_size, segment_seq_number);
                if (RARE_CASE(segment_bytes_sent == SEND_SEGMENT_FATAL_ERROR))
                        return EXIT_FAILURE_SUBSTATE; /* EXIT point. */

                DEBUG_SMART_ASSERT((size_t)segment_bytes_sent == payload_size + MICROTCP_HEADER_SIZE);

                sq_enqueue(_socket->send_queue, segment_seq_number, payload_size, _context->buffer + total_data_bytes_sent);
                total_data_bytes_sent += (segment_bytes_sent - MICROTCP_HEADER_SIZE);
        }
        return RECV_ACK_ROUND_SUBSTATE;
}

static inline send_fsm_substates_t receive_and_process_ack(microtcp_sock_t *const _socket, fsm_context_t *const _context, const _Bool _block)
{
        ssize_t recv_ack_ret_val = receive_data_ack_segment(_socket, _block);
        switch (recv_ack_ret_val)
        {
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_CARRIES_DATA:
                break;
        case RECV_SEGMENT_TIMEOUT:
                if (_block == true) /* If in block, timeout timer expired. */
                {
                        respond_to_timeout(_socket, _context);
                        LOG_WARNING("Timeout occured!"); /* TIMEOUT */ /* TODO: REMOVE */
                        return RETRANSMISSIONS_SUBSTATE;
                }
                break;
        case RECV_SEGMENT_FINACK_UNEXPECTED:
                return FINACK_RECEPTION_SUBSTATE;
        case RECV_SEGMENT_RST_RECEIVED:
                return RST_RECEPTION_SUBSTATE;
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;
        default: /* Received an ACK. */
                return handle_ack_reception(_socket, _context);
        }
        return CONTINUE_SUBSTATE;
}

static inline send_fsm_substates_t execute_recv_ack_round_substate(microtcp_sock_t *_socket, fsm_context_t *_context)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _context != NULL);
        DEBUG_SMART_ASSERT(_socket->state == ESTABLISHED, _context->buffer != NULL);

        while (!sq_is_empty(_socket->send_queue))
        {
                const send_fsm_substates_t next_substate = receive_and_process_ack(_socket, _context, true);
                if (next_substate != CONTINUE_SUBSTATE)
                        return next_substate;
        }
        return SEND_DATA_ROUND_SUBSTATE;
}

static inline send_fsm_substates_t execute_retransmissions_substate(microtcp_sock_t *const _socket, fsm_context_t *const _context)
{
        uint32_t bytes_resent = 0;
        send_queue_node_t *curr_node = sq_front(_socket->send_queue);
        while (curr_node != NULL)
        {
                if (bytes_resent + curr_node->segment_size > _socket->cwnd) /* Hit transmission limit. */
                        break;
                update_socket_lost_counters(_socket, curr_node->segment_size);
                ssize_t send_dat_ret_val = error_tolerant_send_data(_socket, curr_node->buffer, curr_node->segment_size, curr_node->seq_number);
                if (RARE_CASE(send_dat_ret_val == SEND_SEGMENT_FATAL_ERROR))
                        return EXIT_FAILURE_SUBSTATE;

                const size_t stored_segments_pre_ack = sq_stored_segments(_socket->send_queue);
                const send_fsm_substates_t next_substate = receive_and_process_ack(_socket, _context, false);
                if (next_substate != CONTINUE_SUBSTATE)
                        return next_substate;
                if (stored_segments_pre_ack == sq_stored_segments(_socket->send_queue)) /* No ACK, or ACK didn't match. SEND-NEXT */
                        curr_node = curr_node->next;
                else
                        curr_node = sq_front(_socket->send_queue); /* ACK, match segment in send_queue. FAST-FORWARD. */
        }
        return RECV_ACK_ROUND_SUBSTATE; /* We performed the interleaved (with ack reception) retransmissions. Now listen for ACKs */
}

static __always_inline ssize_t execute_exit_success_substate(const size_t _bytes_sent)
{
        DEBUG_SMART_ASSERT(_bytes_sent < ((size_t)-1) >> 1);
        return (ssize_t)_bytes_sent;
}

static __always_inline ssize_t execute_exit_failure_substate(microtcp_sock_t *const _socket, const size_t _bytes_sent)
{
        DEBUG_SMART_ASSERT(_bytes_sent < ((size_t)-1) >> 1);
        LOG_ERROR("EXIT FAILURE in microtcp_send_fsm()");
        _socket->state = INVALID;
        return (ssize_t)(_bytes_sent > 0 ? _bytes_sent : MICROTCP_SEND_FAILURE);
}

static __always_inline ssize_t execute_finack_reception_substate(microtcp_sock_t *const _socket, const size_t _bytes_sent)
{
        DEBUG_SMART_ASSERT(_bytes_sent < ((size_t)-1) >> 1);
        _socket->state = CLOSING_BY_PEER;
        LOG_ERROR("%s reception during microtcp_send_fsm(): microtcp socket in %s state",
                  get_microtcp_control_to_string(FIN_BIT | ACK_BIT), get_microtcp_state_to_string(_socket->state));
        return (ssize_t)(_bytes_sent > 0 ? _bytes_sent : MICROTCP_SEND_FAILURE);
}

static __always_inline ssize_t execute_rst_reception_substate(microtcp_sock_t *const _socket, const size_t _bytes_sent)
{
        DEBUG_SMART_ASSERT(_bytes_sent < ((size_t)-1) >> 1);
        _socket->state = RESET;
        LOG_ERROR("%s reception during microtcp_send_fsm(): microtcp socket in %s state",
                  get_microtcp_control_to_string(RST_BIT), get_microtcp_state_to_string(_socket->state));
        return (ssize_t)(_bytes_sent > 0 ? _bytes_sent : MICROTCP_SEND_FAILURE);
}

ssize_t microtcp_send_fsm(microtcp_sock_t *const _socket, const void *const _buffer, const size_t _length)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SEND_FAILURE, _socket, ESTABLISHED);
        fsm_context_t context = {.buffer = _buffer,
                                 .remaining = _length,
                                 .duplicate_ack_count = 0,
                                 .current_send_algorithm = ALGORITHM_SLOW_START};

        send_fsm_substates_t current_substate = SEND_DATA_ROUND_SUBSTATE;
        while (true)
        {
                LOG_FSM_SEND("Entering %s", convert_substate_to_string(current_substate));
                switch (current_substate)
                {
                case SEND_DATA_ROUND_SUBSTATE:
                        current_substate = execute_send_data_round_substate(_socket, &context);
                        continue;
                case RECV_ACK_ROUND_SUBSTATE:
                        current_substate = execute_recv_ack_round_substate(_socket, &context);
                        continue;
                case RETRANSMISSIONS_SUBSTATE:
                        current_substate = execute_retransmissions_substate(_socket, &context);
                        continue;
                case CONTINUE_SUBSTATE:
                        LOG_ERROR("Logic error occured, CONTINUE_SUBSTATE is not meant to be returned in FSM substate runner. ");
                        current_substate = EXIT_FAILURE_SUBSTATE;
                        continue;
                case FINACK_RECEPTION_SUBSTATE:
                        return execute_finack_reception_substate(_socket, _length - context.remaining);
                case RST_RECEPTION_SUBSTATE:
                        return execute_rst_reception_substate(_socket, _length - context.remaining);
                case EXIT_FAILURE_SUBSTATE:
                        return execute_exit_failure_substate(_socket, _length - context.remaining);
                case EXIT_SUCCESS_SUBSTATE:
                        return execute_exit_success_substate(_length - context.remaining);
                default:
                        FSM_DEFAULT_CASE_HANDLER(convert_substate_to_string, current_substate, EXIT_FAILURE_SUBSTATE);
                        continue;
                }
        }
}

// clang-format off
static const char *convert_substate_to_string(const send_fsm_substates_t _substate)
{
        switch (_substate)
        {
        case SEND_DATA_ROUND_SUBSTATE:  return STRINGIFY(SEND_DATA_ROUND_SUBSTATE);
        case RECV_ACK_ROUND_SUBSTATE:   return STRINGIFY(RECV_ACK_ROUND_SUBSTATE);
        case RETRANSMISSIONS_SUBSTATE:  return STRINGIFY(RETRANSMISSIONS_SUBSTATE);
        case FINACK_RECEPTION_SUBSTATE: return STRINGIFY(FINACK_RECEPTION_SUBSTATE);
        case RST_RECEPTION_SUBSTATE:    return STRINGIFY(RST_RECEPTION_SUBSTATE);
        case EXIT_SUCCESS_SUBSTATE:     return STRINGIFY(EXIT_SUCCESS_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:     return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        case CONTINUE_SUBSTATE:         return STRINGIFY(CONTINUE_SUBSTATE);
        default:                        return "??CONNECT_SUBSTATE??";
        }
}
// clang-format on
