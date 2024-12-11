
#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_core.h"
#include "microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "sys/time.h"

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */

static inline void subtract_and_normalize_timeval(struct timeval *_subtrahend, const struct timeval _minuend);
static inline time_t timeval_to_us(const struct timeval _timeval);

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
} shutdown_fsm_substates;

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
} fsm_context_t;

// clang-format off
static const char *convert_substate_to_string(shutdown_fsm_substates _state)
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

static shutdown_fsm_substates execute_connection_established_substate(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                                                      socklen_t _address_len, fsm_context_t *_context)
{
        _socket->seq_number = _context->socket_shutdown_isn;
        _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
        switch (_context->send_finack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        case SEND_SEGMENT_ERROR:
                return CONNECTION_ESTABLISHED_SUBSTATE;

        default:
                /* In TCP, segments containing control flags (e.g., SYN, FIN),
                 * other than pure ACKs, are treated as carrying a virtual payload.
                 * As a result, they are incrementing the sequence number by 1. */
                _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_finack_ret_val);
                return FIN_WAIT_1_SUBSTATE;
        }
}

static shutdown_fsm_substates execute_fin_wait_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                          socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                if (_context->finack_retries_counter == 0) /* Run out of retries. */
                        return CLOSED_1_SUBSTATE;          /* 4th step termination process failed, closing forcibly. */
                _context->finack_retries_counter--;
                return CONNECTION_ESTABLISHED_SUBSTATE;
        case RECV_SEGMENT_RST_BIT:
                SMART_ASSERT(0, "NOT IMPLEMENTED YET."); // TODO: Well implement it..
                /* DO we return success on shutdown? or shutdown failed... Peer told us to close forcibly... */

        default:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                return FIN_WAIT_2_RECV_SUBSTATE;
        }
}

static shutdown_fsm_substates execute_fin_wait_2_recv_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                               socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_finack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                if (timeval_to_us(_context->finack_wait_time_timer) > 0)
                {
                        subtract_and_normalize_timeval(&(_context->finack_wait_time_timer), _context->recvfrom_timeout);
                        return FIN_WAIT_2_RECV_SUBSTATE;
                }
                _context->finack_wait_time_timer.tv_sec = get_shutdown_time_wait_period_sec();   /* Reset counters. */
                _context->finack_wait_time_timer.tv_usec = get_shutdown_time_wait_period_usec(); /* Reset counters. */
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                SMART_ASSERT(0, "NOT IMPLEMENTED YET."); // TODO: Well implement it..
                /* DO we return success on shutdown? or shutdown failed... Peer told us to close forcibly... */

        default:
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
}

static shutdown_fsm_substates execute_fin_wait_2_send_substate(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                                               socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        case SEND_SEGMENT_ERROR:
                return FIN_WAIT_2_SEND_SUBSTATE;
                /* If while we experience errors sending out ACK segment,
                 * we re-receive peer's FIN-ACK segment its alright.
                 * As once we are able to send our ACK problem will be resolved. */

        default:
                update_socket_sent_counters(_socket, _context->send_ack_ret_val);
                return TIME_WAIT_SUBSTATE;
        }
}

static shutdown_fsm_substates execute_time_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                         socklen_t _address_len, fsm_context_t *_context)
{
        /* In TIME_WAIT state, we set our timer to expire after 2*MSL (per TCP protocol). */
        if (set_recvfrom_timeout(_socket, get_shutdown_time_wait_period_sec(), get_shutdown_time_wait_period_usec()) == POSIX_SETSOCKOPT_FAILURE)
                LOG_ERROR_RETURN(CLOSED_1_SUBSTATE, "Failed to set timeout on socket descriptor. Ignoring %s state.",
                                 convert_substate_to_string(TIME_WAIT_SUBSTATE));

        _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_finack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT: /* Timeout occurred ; Healthy netowork case. */
                return CLOSED_1_SUBSTATE;
                /* We want timeout to occur... Means peer sent its FIN-ACK and stopped sending (should receive our ACK though). */

        default:
                update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
        /* In case host's LAST ACK gets lost, and peer's FIN-ACK gets lost.
         * The connection from the perspective of the host is considered
         * closed, as for the host there is no away to identify such scenario. */
}

static shutdown_fsm_substates execute_closed_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                        socklen_t _address_len, fsm_context_t *_context)
{
        _socket->state = CLOSED;
        // LOG // TODO:
        return CLOSED_2_SUBSTATE;
}

int microtcp_shutdown_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address));

        /* Initialize FSM's context. */
        fsm_context_t context = {
            .socket_shutdown_isn = _socket->seq_number,
            .finack_retries_counter = get_shutdown_finack_retries(),
            .finack_wait_time_timer = {
                .tv_sec = get_shutdown_time_wait_period_sec(),
                .tv_usec = get_shutdown_time_wait_period_usec()}};

        get_recvfrom_timeout(_socket, &(context.recvfrom_timeout.tv_sec), &(context.recvfrom_timeout.tv_usec));

        /* If we are in shutdown()'s FSM, that means that host (local) called shutdown not peer. */
        _socket->state = CLOSING_BY_HOST;
        shutdown_fsm_substates current_substate = CONNECTION_ESTABLISHED_SUBSTATE;
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
}

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
/* Helper function: Safely subtract _minuend from _subtrahend and store the result in t1 */
static inline void subtract_and_normalize_timeval(struct timeval *_subtrahend, const struct timeval _minuend)
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
        return (_timeval.tv_sec * 1000000) + _timeval.tv_usec;
}