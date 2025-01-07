#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp.h"
#include "microtcp_core.h"
#include "microtcp_defines.h"
#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"

/* Very ealy version, made just for connection termination. */
typedef enum
{
        RECEIVE_SUBSTATE, /* Start state. */
        RECEIVED_FINACK_SUBSTATE,
        RECEIVED_RST_SUBSTATE,
        TIMEOUT_OCCURED_SUBSTATE,
        EXIT_SUCCESS_SUBSTATE,

        EXIT_FAILURE_SUBSTATE
} receive_internal_states;

typedef struct
{
        ssize_t recv_data_ret_val;
        ssize_t send_ack_ret_val;

        size_t loaded_bytes;
} fsm_context_t;

// clang-format off
static const char *convert_state_to_string(receive_internal_states _state)
{
        switch (_state)
        {
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??RECEIVE_STATE??";
        }
}
// clang-format on

static void fill_rrb(microtcp_sock_t *_socket)
{
#ifdef DEBUG_MODE
        SMART_ASSERT(_context != NULL);
        SMART_ASSERT(_context->loaded_bytes <= _context->_input_buffer_length);
#endif /* DEBUG_MODE*/

        size_t pending_bytes = _input_buffer_length;
        while (pending_bytes != 0)
        {
                ssize_t recv_data_ret_val = receive_data_segment(_socket);
                if (recv_data_ret_val == RECV_SEGMENT_ERROR)
                        continue; /* Faulty segment, discard it. */
                if (recv_data_ret_val == RECV_SEGMENT_TIMEOUT)
                        return TIMEOUT_OCCURED_SUBSTATE;
                if (recv_data_ret_val == RECV_SEGMENT_FATAL_ERROR)
                        return EXIT_FAILURE_STATE;
                LOG_ERROR_RETURN(MICROTCP_RECV_FAILURE, "Fatal-Error occured while loading the assembly-buffer.");

                if (_socket->segment_receive_buffer->header.control == (FIN_BIT | ACK_BIT))
                        return RECEIVED_FINACK_SUBSTATE;

                if (_socket->segment_receive_buffer->header.seq_number == _socket->ack_number)
                {
                        size_t bytes_to_copy = MIN(pending_bytes, recv_data_ret_val);
                        memcpy(_input_buffer + _context->loaded_bytes, _socket->segment_receive_buffer->raw_payload_bytes, bytes_to_copy);
                        _context->loaded_bytes += bytes_to_copy;
                        if (pending_bytes < recv_data_ret_val)
                        {
                                SMART_ASSERT(_socket->buf_fill_level == 0);
                                // Store extra bytes.
                                memcpy(_socket->bytestream_assembly_buffer, _socket->segment_build_buffer->raw_payload_bytes + bytes_to_copy, recv_data_ret_val - bytes_to_copy);
                                _socket->buf_fill_level = recv_data_ret_val - bytes_to_copy;
                                _socket->
                        }
                        pending_bytes -= bytes_to_copy;
                        // Update receive window.
                }
                else
                {
                        //
                }
                update_socket_received_counters(_socket, recv_data_ret_val);
        }
}

ssize_t till_timeout(microtcp_sock_t *const _socket, void *_buffer, const size_t _length)
{
        receive_ring_buffer_t *const bytestream_rrb = _socket->bytestream_rrb; /* Create local pointer to avoid dereferencing. */
        const size_t cached_rrb_size = rrb_size(bytestream_rrb);
        size_t bytes_copied = 0;
        while (bytes_copied != _length)
        {
                ssize_t receive_data_ret_val = receive_data_segment(_socket, TRUE);
                switch (receive_data_ret_val)
                {
                case RECV_SEGMENT_ERROR:
                        break; /* Faulty segment, ignore it. */
                case RECV_SEGMENT_FATAL_ERROR:
                        return EXIT_FAILURE_SUBSTATE;
                case RECV_SEGMENT_FINACK_UNEXPECTED:
                        return RECEIVED_FINACK_SUBSTATE;
                case RECV_SEGMENT_RST_RECEIVED:
                        return RECEIVED_RST_SUBSTATE;
                case RECV_SEGMENT_TIMEOUT:
                        return TIMEOUT_OCCURED_SUBSTATE;
                default:
                        uint32_t stored_bytes = rrb_append(bytestream_rrb, _socket->segment_receive_buffer);
                        if (RARE_CASE(stored_bytes == 0))
                                break;
                        _socket->ack_number = rrb_last_consumed_seq_number(bytestream_rrb) + rrb_consumable_bytes(bytestream_rrb) + 1;
                        if (rrb_consumable_bytes(bytestream_rrb) == cached_rrb_size)
                                bytes_copied += rrb_pop(bytestream_rrb, _buffer + bytes_copied, _length - bytes_copied);
                        _socket->curr_win_size = cached_rrb_size - rrb_consumable_bytes(bytestream_rrb); /* TODO: is this correct? Is it based on stored? or consumable? */
                        send_ack_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address));
                }
        }
}