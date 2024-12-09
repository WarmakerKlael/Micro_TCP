
#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_core.h"
#include "logging/logger.h"

#define TIME_WAIT_PERIOD 2 * MICROTCP_MSL_SECONDS

typedef enum
{
        INITIAL_STATE, /* Start state. */ /* You enter from the MicroTCP's `Established` state. */
        FIN_WAIT_1_STATE,
        FIN_WAIT_2_RECV_STATE, /* Sub state. */
        FIN_WAIT_2_SEND_STATE, /* Sub state. */
        TIME_WAIT_STATE,
        CLOSED_STATE, /* End state. */
        EXIT_FAILURE_STATE
} shutdown_fsm_states;

typedef struct
{
        ssize_t send_finack_ret_val;
        ssize_t recv_ack_ret_val;
        ssize_t recv_finack_ret_val;
        ssize_t send_ack_ret_val;

        ssize_t socket_shutdown_isn;
} fsm_context_t;

// clang-format off
static const char *convert_state_to_string(shutdown_fsm_states _state)
{
        switch (_state)
        {
        case INITIAL_STATE:             return STRINGIFY(INITIAL_STATE);
        case FIN_WAIT_1_STATE:          return STRINGIFY(FIN_WAIT_1_STATE);
        case FIN_WAIT_2_RECV_STATE:     return STRINGIFY(FIN_WAIT_2_RECV_STATE);
        case FIN_WAIT_2_SEND_STATE:     return STRINGIFY(FIN_WAIT_2_SEND_STATE);
        case TIME_WAIT_STATE:           return STRINGIFY(TIME_WAIT_STATE);
        case CLOSED_STATE:              return STRINGIFY(CLOSED_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??SHUTDOWN_STATE??";
        }
}
// clang-format on

static shutdown_fsm_states execute_initial_state(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                                      socklen_t _address_len, fsm_context_t *_context)
{
        /* TODO? do you reset  your shutdown ISN here? */
        _socket->seq_number = _context->socket_shutdown_isn;
        _context->send_finack_ret_val = send_finack_segment(_socket, _address, _address_len);
        if (_context->send_finack_ret_val == SEND_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_finack_ret_val == SEND_SEGMENT_ERROR)
                return INITIAL_STATE;

        /* In TCP, segments containing control flags (e.g., SYN, FIN),
         * other than pure ACKs, are treated as carrying a virtual payload.
         * As a result, they are incrementing the sequence number by 1. */
        _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
        update_socket_sent_counters(_socket, _context->send_finack_ret_val);
        return FIN_WAIT_1_STATE;
}

static shutdown_fsm_states execute_fin_wait_1_state(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                         socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_ack_ret_val = receive_ack_segment(_socket, _address, _address_len);
        if (_context->recv_ack_ret_val == RECV_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->recv_ack_ret_val == RECV_SEGMENT_TIMEOUT || /* Timeout occurred. */
            _context->recv_ack_ret_val == RECV_SEGMENT_ERROR)     /* Corrupt packet, etc */
        {
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                return INITIAL_STATE;
        }
        update_socket_received_counters(_socket, _context->recv_ack_ret_val);
        return FIN_WAIT_2_RECV_STATE;
}

static shutdown_fsm_states execute_fin_wait_2_recv_state(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                              socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_finack_ret_val = receive_finack_segment(_socket, _address, _address_len);
        if (_context->recv_finack_ret_val == RECV_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->recv_finack_ret_val == RECV_SEGMENT_TIMEOUT || /* Timeout occurred. */
            _context->recv_finack_ret_val == RECV_SEGMENT_ERROR)     /* Corrupt packet, etc */
                return FIN_WAIT_2_RECV_STATE;

        update_socket_received_counters(_socket, _context->recv_finack_ret_val);
        return FIN_WAIT_2_SEND_STATE;
}

static shutdown_fsm_states execute_fin_wait_2_send_state(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                                              socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_segment(_socket, _address, _address_len);
        if (_context->send_ack_ret_val == SEND_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_ack_ret_val == SEND_SEGMENT_ERROR)
                return FIN_WAIT_2_SEND_STATE;
        /* If while we experience errors sending out ACK segment,
         * we re-receive peer's FIN-ACK segment its alright.
         * As once we are able to send our ACK problem will be solved. */
        update_socket_sent_counters(_socket, _context->send_ack_ret_val);
        return TIME_WAIT_STATE;
}

static shutdown_fsm_states execute_time_wait_state(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                        socklen_t _address_len, fsm_context_t *_context)
{
        /* In TIME_WAIT state, we set our timer to expire after 2*MSL (per TCP protocol). */
        if (set_socket_timeout(_socket, TIME_WAIT_PERIOD, 0) == POSIX_SETSOCKOPT_FAILURE)
                LOG_ERROR_RETURN(CLOSED_STATE, "Failed to set timeout on socket descriptor. Ignoring %s state.",
                                 convert_state_to_string(TIME_WAIT_STATE));

        _context->recv_finack_ret_val = receive_finack_segment(_socket, _address, _address_len);
        if (_context->recv_finack_ret_val == RECV_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->recv_finack_ret_val == RECV_SEGMENT_TIMEOUT || /* Timeout occurred. */
            _context->recv_finack_ret_val == RECV_SEGMENT_ERROR)     /* Corrupt packet, etc */
                return CLOSED_STATE;                                 /* Healthy netowork case. */

        update_socket_lost_counters(_socket, _context->send_ack_ret_val);
        update_socket_received_counters(_socket, _context->recv_finack_ret_val);
        return FIN_WAIT_2_SEND_STATE;

        /* In case host's LAST ACK gets lost, and peer's FIN-ACK gets lost.
         * The connection from the perspective of the host is considered
         * closed, as for the host there is no away to identify such scenario. */
}

int microtcp_shutdown_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address));

        fsm_context_t context = {0};
        context.socket_shutdown_isn = _socket->seq_number;
        shutdown_fsm_states current_state = INITIAL_STATE;
        while (TRUE)
        {
                switch (current_state)
                {
                case INITIAL_STATE:
                        /* If we are in shutdown's FSM, that means that host (local) called shutdown not peer. */
                        _socket->state = CLOSING_BY_HOST;
                        current_state = execute_initial_state(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_1_STATE:
                        current_state = execute_fin_wait_1_state(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_RECV_STATE:
                        current_state = execute_fin_wait_2_recv_state(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_SEND_STATE:
                        current_state = execute_fin_wait_2_send_state(_socket, _address, _address_len, &context);
                        break;
                case TIME_WAIT_STATE:
                        current_state = execute_time_wait_state(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_STATE:
                        _socket->state = CLOSED;
                        return MICROTCP_SHUTDOWN_SUCCESS;
                case EXIT_FAILURE_STATE:
                        _socket->state = ESTABLISHED; /* Shutdown failed, thus connection remains ESTABLISHED. */
                        return MICROTCP_SHUTDOWN_FAILURE;
                default:
                        LOG_ERROR("Shutdown's FSM entered an `undefined` state. Prior state = %s",
                                  convert_state_to_string(current_state));
                        current_state = EXIT_FAILURE_STATE;
                        break;
                }
        }
}