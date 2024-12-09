
#include "state_machines/state_machines.h"
#include "state_machines_common.h"
#include "microtcp_core.h"
#include "logging/logger.h"

typedef enum
{
        START_STATE, /* Start state */
        FIN_WAIT_1_STATE,
        FIN_WAIT_2_STATE,
        TIME_WAIT_STATE,
        CLOSE_WAIT_STATE,
        CLOSED_STATE,      /* End state */
        EXIT_FAILURE_STATE /* ?? */
} shutdown_internal_states;

typedef struct
{
        ssize_t send_finack_ret_val;

        ssize_t socket_shutdown_isn;
} state_machine_context_t;

// clang-format off
static const char *get_shutdown_state_to_string(shutdown_internal_states _state)
{
        switch (_state)
        {
        case START_STATE:               return STRINGIFY(START_STATE);
        case FIN_WAIT_1_STATE:          return STRINGIFY(FIN_WAIT_1_STATE);
        case FIN_WAIT_2_STATE:          return STRINGIFY(FIN_WAIT_2_STATE);
        case TIME_WAIT_STATE:           return STRINGIFY(TIME_WAIT_STATE);
        case CLOSE_WAIT_STATE:          return STRINGIFY(CLOSE_WAIT_STATE);
        case CLOSED_STATE:              return STRINGIFY(CLOSED_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??SHUTDOWN_STATE??";
        }
}
// clang-format on

static shutdown_internal_states execute_start_state(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                    socklen_t _address_len, state_machine_context_t *_context)
{
        _context->send_finack_ret_val = send_finack_segment(_socket,_address, _address_len);
        if (_context->send_finack_ret_val == SEND_SEGMENT_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_finack_ret_val == SEND_SEGMENT_ERROR)
                return START_STATE;

        /* In TCP, segments containing control flags (e.g., SYN, FIN),
         * other than pure ACKs, are treated as carrying a virtual payload.
         * As a result, they are incrementing the sequence number by 1. */
        _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
        update_socket_sent_counters(_socket, _context->);
        return SYN_SENT_STATE;
        
}

int microtcp_shutdown_state_machine(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address));

        state_machine_context_t context = {0};
        context.socket_shutdown_isn = _socket->seq_number;
        shutdown_internal_states current_state = START_STATE;
        while (TRUE)
        {
                switch (current_state)
                {
                case START_STATE:
                case FIN_WAIT_1_STATE:
                case FIN_WAIT_2_STATE:
                case TIME_WAIT_STATE:
                case CLOSE_WAIT_STATE:
                case CLOSED_STATE:
                case EXIT_FAILURE_STATE:
                default:
                        LOG_ERROR("Shutdown's state machine entered an undefined state. Prior state = %s",
                                  get_shutdown_state_to_string(current_state));
                        current_state = EXIT_FAILURE_STATE;
                        break;
                }
        }
}