#include "microtcp_core_utils.h"
#include "state_machines/state_machines.h"
#include "logging/logger.h"
typedef enum
{
        START_STATE,
        SYN_RECEIVED_STATE,
        SYNACK_SENT_STATE,
        ACK_RECEIVED_STATE,
        EXIT_FAILURE_STATE
} accept_internal_states;

typedef struct
{
        ssize_t recv_syn_ret_val;
        ssize_t send_synack_ret_val;
        ssize_t recv_ack_ret_val;
} state_machine_context_t;

// clang-format off
static const char *get_accept_state_to_string(accept_internal_states _state)
{
        switch (_state)
        {
        case START_STATE:               return STRINGIFY(START_STATE);
        case SYN_RECEIVED_STATE:        return STRINGIFY(SYN_RECEIVED_STATE);
        case SYNACK_SENT_STATE:         return STRINGIFY(SYNACK_SENT_STATE);
        case ACK_RECEIVED_STATE:        return STRINGIFY(ACK_RECEIVED_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??ACCEPT_STATE??";
        }
}
// clang-format on

static accept_internal_states execute_start_state(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                  socklen_t _address_len, state_machine_context_t *_context)
{
        _context->recv_syn_ret_val = receive_syn_segment(_socket, _address, _address_len);
        if (_context->recv_syn_ret_val == MICROTCP_RECV_SYN_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->recv_syn_ret_val == MICROTCP_RECV_SYN_ERROR)
                return START_STATE;
        update_socket_received_counters(_socket, _context->recv_syn_ret_val);
        return SYN_RECEIVED_STATE;
}

int microtcp_accept_state_machine(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len)
{

        state_machine_context_t context = {0};
        accept_internal_states current_connection_state = START_STATE;
        while (TRUE)
        {
                switch (current_connection_state)
                {
                case START_STATE:
                case SYN_RECEIVED_STATE:
                case SYNACK_SENT_STATE:
                case ACK_RECEIVED_STATE:
                case EXIT_FAILURE_STATE:
                default:
                        LOG_ERROR("Connect state machine entered an undefined state. Prior state = %s",
                                  get_accept_state_to_string(current_connection_state));
                        current_connection_state = EXIT_FAILURE_STATE;
                        break;
                }
        }
}