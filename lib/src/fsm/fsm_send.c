#include <errno.h>
#include "microtcp.h"
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "smart_assert.h"

typedef enum
{
        SLOW_START_SUBSTATE, /* ENTRY_POINT */
        CONGESTION_AVOIDANCE_SUBSTATE,
        FAST_RETRANSMIT_SUBSTATE,
        EXIT_FAILURE_SUBSTATE = -1 /* Terminal substate (failure). FSM exit point. */
} send_fsm_substates_t;

typedef struct
{
        const void *buffer;
        size_t remaining;
} fsm_context_t;


static uint32_t execute_send_round(microtcp_sock_t *_socket, fsm_context_t *_context)
{
#define MAX_PAYLOAD_SIZE (MICROTCP_MSS - sizeof(microtcp_header_t))
        send_queue_t *send_queue = sq_create();
        uint32_t bytes_to_send = MIN(MIN(_socket->cwnd, _socket->peer_win_size), _context->remaining);
        uint32_t initial_seq_number = _socket->seq_number;
        uint32_t total_data_bytes_sent = 0;
        while (bytes_to_send != 0)
        {
                const size_t payload_size = MIN(_context->remaining, MAX_PAYLOAD_SIZE);
                const ssize_t data_bytes_sent = send_data_segment(_socket, _context->buffer + total_data_bytes_sent, payload_size);

                if (data_bytes_sent == 0)
                        /* TODO: HANDLE ERROR*/;

                DEBUG_SMART_ASSERT(data_bytes_sent == payload_size);

                sq_enqueue(send_queue, _socket->seq_number, payload_size, _context->buffer + total_data_bytes_sent);
                total_data_bytes_sent += data_bytes_sent;
                _socket->seq_number += data_bytes_sent;
        }

        /* RECEIVE ACKs */
        /* Expected ACK count: */
        socklen_t peer_address_length = sizeof(_socket->peer_address);
        size_t send_queue_size = sq_size(send_queue);
        for (size_t i = 0; i < send_queue_size; i++)
        {
                ssize_t recvfrom_ret_val = recvfrom(_socket->sd, _socket->bytestream_receive_buffer, MICROTCP_MSS, MSG_TRUNC, _socket->peer_address, &peer_address_length);
                if (recvfrom_ret_val == -1)
                        ; /* ERROR or TIMEOUT */
                if (recvfrom_ret_val < sizeof(microtcp_header_t))
                        ;
                if (recvfrom_ret_val > MICROTCP_MSS)
                        ;
                if (!is_valid_microtcp_bytestream(_socket->bytestream_receive_buffer, recvfrom_ret_val))
                        ;

                extract_microtcp_segment(_socket->segment_receive_buffer, _socket->bytestream_receive_buffer, recvfrom_ret_val);
                microtcp_segment_t *received_segment = _socket->segment_receive_buffer;

                /* NO PARTIAL STORING... */

                /* if ack older than current seq ignore. */

                /* Duplicate ACK. */
                if (received_segment->header.ack_number == _socket->seq_number) /* _socket->seq_number, stores next available seq_number. */
                        send_data_segment(_socket, _context->buffer, MIN(MAX_PAYLOAD_SIZE, _context->remaining));

                /* if ack is new, mark it.  (update socket->seq_number) */
                /*IS ACK NEW? */

                /* IRRELEVANT (RECEIVER ACKS WHOLE OR NOTHING) with the new ACKs.. did receiver acked all? Do we need to resend some again? */

                /* Check ack numbers: (And update your sequence numbers) */
                if (received_segment->header.ack_number /**/)
                        ;

                /* check for RSTs or FINs*/
        }
#undef MAX_PAYLOAD_SIZE
}

static send_fsm_substates_t execute_slow_start_substate(microtcp_sock_t *_socket, fsm_context_t *_context)
{
}

/* Argument check is for the most part redundant as FSM callers, have validated their input arguments. */
int microtcp_send_fsm(microtcp_sock_t *_socket, const void *_buffer, size_t _length)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, ESTABLISHED);
        fsm_context_t context = {.buffer = _buffer, .remaining = _length};

        send_fsm_substates_t current_substate = (_socket->cwnd <= _socket->ssthresh ? SLOW_START_SUBSTATE : CONGESTION_AVOIDANCE_SUBSTATE);
        while (TRUE)
        {
                // LOG_FSM_CONNECT("Entering %s", convert_substate_to_string(current_substate));
                switch (current_substate)
                {
                case SLOW_START_SUBSTATE:
                        current_substate = execute_slow_start_substate(_socket, &context);
                        break;
                }
        }
}