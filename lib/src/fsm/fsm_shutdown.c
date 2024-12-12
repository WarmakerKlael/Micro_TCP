#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_core.h"
#include "microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "sys/time.h"
#include "time.h"

typedef enum
{
        CONNECTION_ESTABLISHED_SUBSTATE = ESTABLISHED, /* Initial substate. FSM entry point. */
        FIN_WAIT_1_SUBSTATE,
        FIN_WAIT_2_RECV_SUBSTATE, /* Sub state. */
        FIN_WAIT_2_SEND_SUBSTATE, /* Sub state. */
        TIME_WAIT_SUBSTATE,
        CLOSED_1_SUBSTATE,
        CLOSED_2_SUBSTATE = CLOSED, /* Terminal substate(success). FSM exit point. */
        EXIT_FAILURE_SUBSTATE = -1  /* Terminal substate (failure). FSM exit point. */
} shutdown_fsm_substates_t;

typedef enum
{
        NO_ERROR,
        PEER_ACK_TIMEOUT,     /* Sent FIN to peer, ack never received. */
        PEER_FIN_TIMEOUT,     /* Peer sent ACK, never sent FIN. */
        RST_EXPECTED_ACK,     /* RST received during termination process. */
        RST_EXPECTED_FINACK,  /* RST received during termination process. */
        RST_EXPECTED_TIMEOUT, /* RST received during termination process. */
        HOST_FATAL_ERROR,
        SOCKET_TIMEOUT_SETUP_ERROR
} shutdown_fsm_errno_t;

typedef struct
{
        ssize_t send_finack_ret_val;
        ssize_t recv_ack_ret_val;
        ssize_t recv_finack_ret_val;
        ssize_t send_ack_ret_val;

        ssize_t socket_shutdown_isn;
        ssize_t finack_retries_counter;
        struct timeval finack_wait_time_timer;
        struct timeval recvfrom_timeout;
        shutdown_fsm_errno_t errno;
} fsm_context_t;

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
static inline shutdown_fsm_substates_t handle_fatal_error(fsm_context_t *_context);
static inline void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend);
static inline time_t timeval_to_us(const struct timeval _timeval);
static const char *convert_substate_to_string(shutdown_fsm_substates_t _state);
static void log_errno_status(shutdown_fsm_errno_t _errno);

static shutdown_fsm_substates_t execute_connection_established_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address, // CHECK
                                                                        socklen_t _address_len, fsm_context_t *_context)
{
        _socket->seq_number = _context->socket_shutdown_isn;
        _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
        switch (_context->send_finack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case SEND_SEGMENT_ERROR:
                return CONNECTION_ESTABLISHED_SUBSTATE;

        default:
                _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_finack_ret_val);
                return FIN_WAIT_1_SUBSTATE;
        }
}

static shutdown_fsm_substates_t execute_fin_wait_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address, // CHECK
                                                            socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                if (_context->finack_retries_counter == 0) /* Exhausted attempts to resend FIN|ACK. */
                {
                        _context->errno = PEER_ACK_TIMEOUT;
                        return EXIT_FAILURE_SUBSTATE;
                }
                _context->finack_retries_counter--; /* Decrement retry counter and attempt to resend FIN|ACK. */
                return CONNECTION_ESTABLISHED_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                _context->errno = RST_EXPECTED_ACK;
                return CLOSED_1_SUBSTATE;

        default:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                return FIN_WAIT_2_RECV_SUBSTATE;
        }
}

static shutdown_fsm_substates_t execute_fin_wait_2_recv_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address, // CHECK
                                                                 socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_finack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                if (timeval_to_us(_context->finack_wait_time_timer) > 0) /* Retry receiving FIN|ACK until the timeout expires. */
                {
                        subtract_timeval(&(_context->finack_wait_time_timer), _context->recvfrom_timeout);
                        return FIN_WAIT_2_RECV_SUBSTATE;
                }
                /* Send RST to notify peer of connection closure. */
                _context->errno = PEER_FIN_TIMEOUT;
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                _context->errno = RST_EXPECTED_FINACK;
                return CLOSED_1_SUBSTATE;

        default:
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
}

static shutdown_fsm_substates_t execute_fin_wait_2_send_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address, // CHECK
                                                                 socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case SEND_SEGMENT_ERROR:
                return FIN_WAIT_2_SEND_SUBSTATE; /* Retry sending ACK. Peer re-sending FIN-ACK is acceptable until ACK succeeds. */

        default:
                update_socket_sent_counters(_socket, _context->send_ack_ret_val);
                return TIME_WAIT_SUBSTATE;
        }
}

static shutdown_fsm_substates_t execute_time_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address, // CHECK
                                                           socklen_t _address_len, fsm_context_t *_context)
{
        /* In TIME_WAIT state, we set our timer to expire after 2*MSL (per TCP protocol). */
        if (set_socket_recvfrom_timeout(_socket, get_shutdown_time_wait_period()) == POSIX_SETSOCKOPT_FAILURE)
        {
                _context->errno = SOCKET_TIMEOUT_SETUP_ERROR;
                return CLOSED_1_SUBSTATE;
        }

        _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_finack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        /* The following three cases result to going to `CLOSED_1_SUBSTATE`. If RST received you also log a warning message. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT: /* Timeout: Peer has likely completed shutdown. Transition to CLOSED_1_SUBSTATE. */
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                _context->errno = RST_EXPECTED_TIMEOUT;
                return CLOSED_1_SUBSTATE;

        default:
                update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
        /* In case host's LAST ACK gets lost, and peer's FIN-ACK gets lost.
         * The connection from the perspective of the host is considered
         * closed, as for the host there is no away to identify such scenario. */
}

static shutdown_fsm_substates_t execute_closed_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                          socklen_t _address_len, fsm_context_t *_context)
{
        release_and_reset_handshake_resources(_socket, CLOSED);
        return CLOSED_2_SUBSTATE;
}

int microtcp_shutdown_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);                  /* Validate socket state. */
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);                                     /* Validate address structure. */
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address)); /* Validate address length. */

        /* Initialize FSM's context. */
        fsm_context_t context = {.socket_shutdown_isn = _socket->seq_number,
                                 .finack_retries_counter = get_shutdown_finack_retries(),
                                 .finack_wait_time_timer = get_shutdown_time_wait_period(),
                                 .recvfrom_timeout = get_socket_recvfrom_timeout(_socket),
                                 .errno = NO_ERROR};

        /* If we are in shutdown()'s FSM, that means that host (local) called shutdown not peer. */
        _socket->state = CLOSING_BY_HOST;

        shutdown_fsm_substates_t current_substate = CONNECTION_ESTABLISHED_SUBSTATE; /* Initial state in shutdown. */
        while (TRUE)
        {
                switch (current_substate)
                {
                case CONNECTION_ESTABLISHED_SUBSTATE:
                        current_substate = execute_connection_established_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_1_SUBSTATE:
                        current_substate = execute_fin_wait_1_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_RECV_SUBSTATE:
                        current_substate = execute_fin_wait_2_recv_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_SEND_SUBSTATE:
                        current_substate = execute_fin_wait_2_send_substate(_socket, _address, _address_len, &context);
                        break;
                case TIME_WAIT_SUBSTATE:
                        current_substate = execute_time_wait_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_1_SUBSTATE:
                        current_substate = execute_closed_1_substate(_socket, _address, _address_len, &context);
                case CLOSED_2_SUBSTATE:
                        return MICROTCP_SHUTDOWN_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        _socket->state = ESTABLISHED; /* Shutdown failed, thus connection remains ESTABLISHED. */
                        return MICROTCP_SHUTDOWN_FAILURE;
                default:
                        LOG_ERROR("Shutdown()'s FSM entered an `undefined` substate. Prior substate = %s",
                                  convert_substate_to_string(current_substate));
                        current_substate = EXIT_FAILURE_SUBSTATE;
                        break;
                }
        }
        log_errno_status(context.errno);
}

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
/* Helper function: Safely subtract _minuend from _subtrahend and store the result in t1 */
static inline void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend)
{
        SMART_ASSERT(_subtrahend != NULL);
        _subtrahend->tv_sec -= _minuend.tv_sec;
        _subtrahend->tv_usec -= _minuend.tv_usec;

        /* Normalize if microseconds are out of range */
        if (_subtrahend->tv_usec < 0)
        {
                time_t borrow = (-_subtrahend->tv_usec / 1000000) + 1; /* Calculate how many seconds to borrow */
                _subtrahend->tv_sec -= borrow;
                _subtrahend->tv_usec += borrow * 1000000;
        }

        /* Ensure the result is non-negative */
        if (_subtrahend->tv_sec < 0)
        {
                _subtrahend->tv_sec = 0;
                _subtrahend->tv_usec = 0;
        }
}

/* Helper function: Convert timeval to microseconds */
static inline time_t timeval_to_us(const struct timeval _timeval)
{
        const time_t usec_per_sec = 1000000;
        return (_timeval.tv_sec * usec_per_sec) + _timeval.tv_usec;
}

// clang-format off
static const char *convert_substate_to_string(shutdown_fsm_substates_t _state)
{
        switch (_state)
        {
        case CONNECTION_ESTABLISHED_SUBSTATE:   return STRINGIFY(CONNECTION_ESTABLISHED_SUBSTATE);
        case FIN_WAIT_1_SUBSTATE:               return STRINGIFY(FIN_WAIT_1_SUBSTATE);
        case FIN_WAIT_2_RECV_SUBSTATE:          return STRINGIFY(FIN_WAIT_2_RECV_SUBSTATE);
        case FIN_WAIT_2_SEND_SUBSTATE:          return STRINGIFY(FIN_WAIT_2_SEND_SUBSTATE);
        case TIME_WAIT_SUBSTATE:                return STRINGIFY(TIME_WAIT_SUBSTATE);
        case CLOSED_1_SUBSTATE:                 return STRINGIFY(CLOSED_1_SUBSTATE);
        case CLOSED_2_SUBSTATE:                 return STRINGIFY(CLOSED_2_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:             return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                                return "??SHUTDOWN_SUBSTATE??";
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
        case PEER_ACK_TIMEOUT:
                LOG_ERROR("Shutdown()'s FSM: timed out waiting for `ACK` from peer.");
                break;
        case PEER_FIN_TIMEOUT:
                LOG_WARNING("Shutdown()'s FSM: timed out waiting for `FIN` from peer.");
                break;
        case RST_EXPECTED_ACK:
                LOG_WARNING("Shutdown()'s FSM: received `RST` while expecting `ACK` from peer.");
                break;
        case RST_EXPECTED_FINACK:
                LOG_WARNING("Shutdown()'s FSM: received `RST` while expecting `FIN|ACK` from peer.");
                break;
        case RST_EXPECTED_TIMEOUT:
                LOG_WARNING("Shutdown()'s FSM: received `RST` while expecting connection to timeout.");
                break;
        case HOST_FATAL_ERROR:
                LOG_ERROR("Shutdown()'s FSM: encountered a `host-failure`.");
                break;
        case SOCKET_TIMEOUT_SETUP_ERROR:
                LOG_WARNING("Shutdown()'s FSM: failed to set socket's timeout for `recvfrom()`.");
                break;
        default:
                LOG_ERROR("Shutdown()'s FSM: encountered an `unknown` error."); /* Log unknown errors for debugging purposes. */
                break;
        }
}

static inline shutdown_fsm_substates_t handle_fatal_error(fsm_context_t *_context)
{
        _context->errno = HOST_FATAL_ERROR;
        return EXIT_FAILURE_SUBSTATE;
}
