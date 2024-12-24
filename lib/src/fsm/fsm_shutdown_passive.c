#include "microtcp_core.h"
#include "fsm_common.h"

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

        size_t socket_shutdown_isn;
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
        _socket->seq_number = _context->socket_shutdown_isn;
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case SEND_SEGMENT_ERROR:
                return FINACK_RECEIVED_SUBSTATE;
        default:
                update_socket_sent_counters(_socket, _context->send_ack_ret_val);
                return CLOSE_WAIT_SUBTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_close_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                    socklen_t _address_len, fsm_context_t *_context)
{
        /* TODO: PLACEHOLDER: Send any data left, for graceful closure. */

        _socket->seq_number = _context->socket_shutdown_isn;
        _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
        switch (_context->send_finack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case SEND_SEGMENT_ERROR:
                return FINACK_RECEIVED_SUBSTATE;
        default:
                _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_finack_ret_val);
                return LAST_ACK_SUBSTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_last_ack_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case RECV_SEGMENT_UNEXPECTED_FINACK:
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

        case RECV_SEGMENT_RST_BIT:
                _context->errno = RST_EXPECTED_ACK;
        default:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                return CLOSED_1_SUBSTATE;
        }
}

static shutdown_passive_fsm_substates_t execute_closed_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        release_and_reset_handshake_resources(_socket, CLOSED);
        return CLOSED_2_SUBSTATE;
}

int microtcp_shutdown_passive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);                  /* Validate socket state. */
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);                                     /* Validate address structure. */
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address)); /* Validate address length. */

        /* Initialize FSM's context. */
        fsm_context_t context = {.socket_shutdown_isn = _socket->seq_number,
                                 .last_ack_wait_time_timer = get_shutdown_time_wait_period(),
                                 .recvfrom_timeout = get_socket_recvfrom_timeout(_socket),
                                 .errno = NO_ERROR};

        /* If we are in shutdown()'s passive FSM, that means that microtcp intercepted a FIN-ACK packet. */
        _socket->state = CLOSING_BY_PEER;

        shutdown_passive_fsm_substates_t current_substate = FINACK_RECEIVED_SUBSTATE; /* Initial state in shutdown. */
        while (TRUE)
        {
                switch (current_substate)
                {
                case FINACK_RECEIVED_SUBSTATE:
                        break;
                case CLOSE_WAIT_SUBTATE:
                        break;
                case LAST_ACK_SUBSTATE:
                        break;
                case CLOSED_1_SUBSTATE:
                        break;
                case CLOSED_2_SUBSTATE:
                        break;
                case EXIT_FAILURE_SUBSTATE:
                        break;
                default:
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
                LOG_INFO("Shutdown()'s FSM: closed connection gracefully.");
                break;
        case RST_EXPECTED_ACK:
                LOG_WARNING("Shutdown()'s FSM: received `RST` while expecting `ACK` from peer.");
                break;
        case LAST_ACK_TIMEOUT:
                LOG_WARNING("Shutdown()'s FSM: timed out waiting for `ACK` from peer.");
                break;
        case HOST_FATAL_ERROR:
                LOG_ERROR("Shutdown()'s FSM: encountered a fatal error on the host's side.");
                break;
        default:
                LOG_ERROR("Shutdown()'s FSM: encountered an `unknown` error code: %d.", _errno); /* Log unknown errors for debugging purposes. */
                break;
        }
}
static inline shutdown_passive_fsm_substates_t handle_fatal_error(fsm_context_t *_context)
{
        _context->errno = HOST_FATAL_ERROR;
        return EXIT_FAILURE_SUBSTATE;
}