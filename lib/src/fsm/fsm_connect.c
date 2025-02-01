#include <stddef.h>          // for size_t
#include <sys/socket.h>      // for socklen_t, sockaddr
#include <sys/types.h>       // for ssize_t
#include "core/segment_io.h" // for SEND_SEGMENT_ERROR, SEND_SE...
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"   // for update_socket_received_coun...
#include "fsm/microtcp_fsm.h"            // for microtcp_connect_fsm
#include "fsm_common.h"                  // for SENT_SYN_SEQUENCE_NUMBER_IN...
#include "logging/microtcp_fsm_logger.h" // for LOG_FSM_CONNECT
#include "logging/microtcp_logger.h"     // for LOG_ERROR, LOG_INFO
#include "microtcp.h"                    // for microtcp_sock_t, CLOSED
#include "microtcp_core_macros.h"        // for RETURN_ERROR_IF_MICROTCP_SO...
#include "microtcp_defines.h"            // for MICROTCP_CONNECT_FAILURE
#include "microtcp_helper_macros.h"      // for STRINGIFY
#include "settings/microtcp_settings.h"  // for get_connect_rst_retries

typedef enum
{
        CLOSED_SUBSTATE = CLOSED, /* Initial substate. FSM entry point. */
        SYN_SENT_SUBSTATE,
        SYNACK_RECEIVED_SUBSTATE,
        ACK_SENT_SUBSTATE,
        CONNECTION_ESTABLISHED_SUBSTATE = ESTABLISHED, /* Terminal substate (success). FSM exit point. */
        EXIT_FAILURE_SUBSTATE = -1                     /* Terminal substate (failure). FSM exit point. */
} connect_fsm_substates_t;

typedef enum
{
        NO_ERROR,
        RST_RETRIES_EXHAUSTED,
        FATAL_ERROR, /* Set when fatal errors occur on host's side. */
} connect_fsm_errno_t;

typedef struct
{
        ssize_t send_syn_ret_val;
        ssize_t recv_synack_ret_val;
        ssize_t send_ack_ret_val;

        ssize_t rst_retries_counter;
        connect_fsm_errno_t errno;

} fsm_context_t;

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
static const char *convert_substate_to_string(connect_fsm_substates_t _substate);
static void log_errno_status(connect_fsm_errno_t _errno);
static inline connect_fsm_substates_t handle_fatal_error(fsm_context_t *_context);

static connect_fsm_substates_t execute_closed_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                       socklen_t _address_len, fsm_context_t *_context)
{

        _context->send_syn_ret_val = send_syn_control_segment(_socket, _address, _address_len);
        switch (_context->send_syn_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case SEND_SEGMENT_ERROR:
                return CLOSED_SUBSTATE;
        default:
                return SYN_SENT_SUBSTATE;
        }
}

static connect_fsm_substates_t execute_syn_sent_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                         socklen_t _address_len, fsm_context_t *_context)
{
        const uint32_t required_ack_number = _socket->seq_number + SYN_SEQ_NUMBER_INCREMENT;
        _context->recv_synack_ret_val = receive_synack_control_segment(_socket, (struct sockaddr *)_address, _address_len, required_ack_number);
        switch (_context->recv_synack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case RECV_SEGMENT_SYN_EXPECTED:
                LOG_WARNING_RETURN(CLOSED_SUBSTATE, "Server is not expecting to establish a connection.");

        /* Actions on the following three cases are the same. */
        case RECV_SEGMENT_FINACK_UNEXPECTED:
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_syn_ret_val);
                return CLOSED_SUBSTATE;

        case RECV_SEGMENT_RST_RECEIVED:
                if (_context->rst_retries_counter == 0)
                {
                        _context->errno = RST_RETRIES_EXHAUSTED;
                        return EXIT_FAILURE_SUBSTATE;
                }
                _context->rst_retries_counter--;
                LOG_ERROR_RETURN(CLOSED_SUBSTATE, "Connection with the server refused!");

        default:
                _socket->seq_number = required_ack_number;
                _socket->ack_number = _socket->segment_receive_buffer->header.seq_number + SYN_SEQ_NUMBER_INCREMENT;
                return SYNACK_RECEIVED_SUBSTATE;
        }
}

static connect_fsm_substates_t execute_synack_received_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                                socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case SEND_SEGMENT_ERROR:
                return SYNACK_RECEIVED_SUBSTATE;

        default:
                return ACK_SENT_SUBSTATE;
        }
}

static connect_fsm_substates_t execute_ack_sent_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                         socklen_t _address_len, fsm_context_t *_context)
{
        _socket->peer_address = (struct sockaddr *)_address;
        _socket->state = ESTABLISHED;
        return CONNECTION_ESTABLISHED_SUBSTATE;
}

/* Argument check is for the most part redundant are FSM callers, have validated their input arguments. */
int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *const _address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(*_address));

        fsm_context_t context = {.rst_retries_counter = get_connect_rst_retries(),
                                 .errno = NO_ERROR};

        connect_fsm_substates_t current_substate = CLOSED_SUBSTATE;
        while (true)
        {
                LOG_FSM_CONNECT("Entering %s", convert_substate_to_string(current_substate));
                switch (current_substate)
                {
                case CLOSED_SUBSTATE:
                        current_substate = execute_closed_substate(_socket, _address, _address_len, &context);
                        break;
                case SYN_SENT_SUBSTATE:
                        current_substate = execute_syn_sent_substate(_socket, _address, _address_len, &context);
                        break;
                case SYNACK_RECEIVED_SUBSTATE:
                        current_substate = execute_synack_received_substate(_socket, _address, _address_len, &context);
                        break;
                case ACK_SENT_SUBSTATE:
                        current_substate = execute_ack_sent_substate(_socket, _address, _address_len, &context);
                        break;
                case CONNECTION_ESTABLISHED_SUBSTATE:
                        log_errno_status(context.errno);
                        return MICROTCP_CONNECT_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        log_errno_status(context.errno);
                        return MICROTCP_CONNECT_FAILURE;
                default:
                        FSM_DEFAULT_CASE_HANDLER(convert_substate_to_string, current_substate, EXIT_FAILURE_SUBSTATE);
                        break;
                }
        }
}

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
// clang-format off
static const char *convert_substate_to_string(connect_fsm_substates_t _substate)
{
        switch (_substate)
        {
        case CLOSED_SUBSTATE:                   return STRINGIFY(CLOSED_SUBSTATE);
        case SYN_SENT_SUBSTATE:                 return STRINGIFY(SYN_SENT_SUBSTATE);
        case SYNACK_RECEIVED_SUBSTATE:          return STRINGIFY(SYNACK_RECEIVED_SUBSTATE);
        case ACK_SENT_SUBSTATE:                 return STRINGIFY(ACK_SENT_SUBSTATE);
        case CONNECTION_ESTABLISHED_SUBSTATE:   return STRINGIFY(CONNECTION_ESTABLISHED_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:             return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                                return "??CONNECT_SUBSTATE??";
        }
}
// clang-format on

static void log_errno_status(connect_fsm_errno_t _errno)
{
        switch (_errno)
        {
        case NO_ERROR:
                LOG_INFO("connect() FSM: completed handshake successfully; No errors.");
                break;
        case RST_RETRIES_EXHAUSTED:
                LOG_ERROR("connect() FSM: exhausted connection attempts while handling RST segments.");
                break;
        case FATAL_ERROR:
                LOG_ERROR("connect() FSM: encountered a fatal error on the host's side.");
                break;
        default:
                LOG_ERROR("connect() FSM: encountered an `unknown` error code: %d.", _errno); /* Log unknown errors for debugging purposes. */
                break;
        }
}

static inline connect_fsm_substates_t handle_fatal_error(fsm_context_t *_context)
{
        _context->errno = FATAL_ERROR;
        return EXIT_FAILURE_SUBSTATE;
}
