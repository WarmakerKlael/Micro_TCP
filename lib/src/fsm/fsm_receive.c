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
        TIMEOUT_OCCURED_SUBSTATE,
        EXIT_SUCCESS_SUBSTATE,

        EXIT_FAILURE_STATE
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

static receive_internal_states execute_receive_substate(microtcp_sock_t *_socket, void *_input_buffer, size_t _input_buffer_length, fsm_context_t *_context)
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

ssize_t receive_fsm(microtcp_sock_t *const _socket, void *_input_buffer, size_t _input_buffer_length)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket->peer_address);
#endif /* DEBUG_MODE */

        /* Passing function arguments to FSM's 'context'
         * this reduces function's stack frame size, making
         * function calls faster. */
        fsm_context_t context = {.loaded_bytes = 0};

        receive_internal_states current_state = RECEIVE_SUBSTATE;
        while (TRUE)
        {
                switch (current_state)
                {
                case RECEIVE_SUBSTATE:
                        current_state = execute_receive_substate(_socket, _input_buffer, _input_buffer_length, &context);
                        break;
                }
        }
}

ssize_t load_assembly_buffer(microtcp_sock_t *_socket)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_RECV_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_RECV_FAILURE, _socket->peer_address);
#endif /* DEBUG_MODE */

        ssize_t loaded_bytes = 0;
        while (TRUE)
        {
                if (_socket->buf_fill_level == get_bytestream_assembly_buffer_len())
                        return loaded_bytes;

                ssize_t recv_data_ret_val = receive_data_segment(_socket);
                if (recv_data_ret_val == RECV_SEGMENT_ERROR)
                        continue; /* Faulty segment, discard it. */
                if (recv_data_ret_val == RECV_SEGMENT_TIMEOUT)
                        return MICROTCP_RECV_TIMEOUT;
                if (recv_data_ret_val == RECV_SEGMENT_FATAL_ERROR)
                        LOG_ERROR_RETURN(MICROTCP_RECV_FAILURE, "Fatal-Error occured while loading the assembly-buffer.");

                if (_socket->segment_receive_buffer->header.control == (FIN_BIT | ACK_BIT))
                        return MICROTCP_RECV_SHUTDOWN;
        }
}