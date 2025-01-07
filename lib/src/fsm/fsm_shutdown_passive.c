#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "core/segment_io.h"
#include "core/misc.h"
#include "core/resource_allocation.h"
#include "core/socket_stats_updater.h"
#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "logging/microtcp_fsm_logger.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings.h"

typedef enum
{
        FINACK_RECEIVED_SUBSTATE = CLOSING_BY_PEER,
        CLOSE_WAIT_SUBTATE,
        LAST_ACK_SUBSTATE,
        CLOSED_1_SUBSTATE,
        CLOSED_2_SUBSTATE = CLOSED,
        EXIT_FAILURE_SUBSTATE = -1 /* Terminal substate (failure). FSM exit point. */
} shutdown_passive_fsm_substates_t;

typedef enum
{
        NO_ERROR,
        RST_EXPECTED_ACK, /* RST after host sent FIN|ACK and expected peer's ACK. */
        LAST_ACK_TIMEOUT,
        HOST_FATAL_ERROR, /* Set when fatal errors occur on host's side. */
} shutdown_fsm_errno_t;

typedef struct
{
        size_t recv_finack_ret_val;
        size_t send_ack_ret_val;
        size_t send_finack_ret_val;
        size_t recv_ack_ret_val;

        struct timeval last_ack_wait_time_timer;
        struct timeval recvfrom_timeout;
        shutdown_fsm_errno_t errno;
} fsm_context_t;

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
static inline shutdown_passive_fsm_substates_t handle_fatal_error(fsm_context_t *_context);
static const char *convert_substate_to_string(shutdown_passive_fsm_substates_t _state);
static void log_errno_status(shutdown_fsm_errno_t _errno);

static shutdown_passive_fsm_substates_t execute_finack_received_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                         socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case SEND_SEGMENT_ERROR:
                return FINACK_RECEIVED_SUBSTATE;
        default:
                return CLOSE_WAIT_SUBTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_close_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                    socklen_t _address_len, fsm_context_t *_context)
{
        /* TODO: PLACEHOLDER: Send any data left, for graceful closure. */

        _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
        switch (_context->send_finack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case SEND_SEGMENT_ERROR:
                return FINACK_RECEIVED_SUBSTATE;
        default:
                return LAST_ACK_SUBSTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_last_ack_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        const uint32_t required_ack_number = _socket->seq_number + FIN_SEQ_NUMBER_INCREMENT;
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len, required_ack_number);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case RECV_SEGMENT_FINACK_UNEXPECTED:
                return FINACK_RECEIVED_SUBSTATE;

        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                if (timeval_to_us(_context->last_ack_wait_time_timer) > 0) /* Retry receiving LAST ACK until the timeout expires. */
                {
                        subtract_timeval(&(_context->last_ack_wait_time_timer), _context->recvfrom_timeout);
                        return CLOSE_WAIT_SUBTATE;
                }
                send_rstack_control_segment(_socket, _address, _address_len); /* Final attempt to notify peer for closure. */
                _context->errno = LAST_ACK_TIMEOUT;
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_RST_RECEIVED:
                _context->errno = RST_EXPECTED_ACK;
        default:
                return CLOSED_1_SUBSTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_closed_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        release_and_reset_connection_resources(_socket, CLOSED);
        return CLOSED_2_SUBSTATE;
}

int microtcp_shutdown_passive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, CLOSING_BY_PEER);              /* Validate socket state. */
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);                                     /* Validate address structure. */
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address)); /* Validate address length. */

        /* Initialize FSM's context. */
        fsm_context_t context = {.last_ack_wait_time_timer = get_shutdown_time_wait_period(),
                                 .recvfrom_timeout = get_socket_recvfrom_timeout(_socket),
                                 .errno = NO_ERROR};

        /* If we are in shutdown()'s passive FSM, that means that microtcp intercepted a FIN-ACK packet. */
        shutdown_passive_fsm_substates_t current_substate = FINACK_RECEIVED_SUBSTATE; /* Initial state in shutdown. */
        while (TRUE)
        {
                LOG_FSM_SHUTDOWN("Entering %s", convert_substate_to_string(current_substate));
                switch (current_substate)
                {
                case FINACK_RECEIVED_SUBSTATE:
                        current_substate = execute_finack_received_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSE_WAIT_SUBTATE:
                        current_substate = execute_close_wait_substate(_socket, _address, _address_len, &context);
                        break;
                case LAST_ACK_SUBSTATE:
                        current_substate = execute_last_ack_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_1_SUBSTATE:
                        current_substate = execute_closed_1_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_2_SUBSTATE:
                        log_errno_status(context.errno);
                        return MICROTCP_SHUTDOWN_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        log_errno_status(context.errno);
                        return MICROTCP_SHUTDOWN_FAILURE;
                default:
                        LOG_ERROR("shutdown_passive() FSM entered an `undefined` substate. Prior substate = %s",
                                  convert_substate_to_string(current_substate));
                        current_substate = EXIT_FAILURE_SUBSTATE;
                        break;
                }
        }
}

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
// clang-format off
static const char *convert_substate_to_string(shutdown_passive_fsm_substates_t _state)
{
        switch (_state)
        {
        case FINACK_RECEIVED_SUBSTATE:  return STRINGIFY(FINACK_RECEIVED_SUBSTATE);
        case CLOSE_WAIT_SUBTATE:        return STRINGIFY(CLOSE_WAIT_SUBTATE);
        case LAST_ACK_SUBSTATE:         return STRINGIFY(LAST_ACK_SUBSTATE);
        case CLOSED_1_SUBSTATE:         return STRINGIFY(CLOSED_1_SUBSTATE);
        case CLOSED_2_SUBSTATE:         return STRINGIFY(CLOSED_2_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:     return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                        return "??SHUTDOWN_PASSIVE_SUBSTATE??";
        }
}
// clang-format on

static void log_errno_status(shutdown_fsm_errno_t _errno)
{
        switch (_errno)
        {
        case NO_ERROR:
                LOG_INFO("shutdown_passive() FSM: closed connection gracefully.");
                break;
        case RST_EXPECTED_ACK:
                LOG_WARNING("shutdown_passive() FSM: received `RST` while expecting `ACK` from peer.");
                break;
        case LAST_ACK_TIMEOUT:
                LOG_WARNING("shutdown_passive() FSM: timed out waiting for `ACK` from peer.");
                break;
        case HOST_FATAL_ERROR:
                LOG_ERROR("shutdown_passive() FSM: encountered a fatal error on the host's side.");
                break;
        default:
                LOG_ERROR("shutdown_passive() FSM: encountered an `unknown` error code: %d.", _errno); /* Log unknown errors for debugging purposes. */
                break;
        }
}
static inline shutdown_passive_fsm_substates_t handle_fatal_error(fsm_context_t *_context)
{
        _context->errno = HOST_FATAL_ERROR;
        return EXIT_FAILURE_SUBSTATE;
}
