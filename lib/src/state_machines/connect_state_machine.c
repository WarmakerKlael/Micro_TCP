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
} connect_internal_states;

typedef struct
{
        ssize_t send_syn_ret_val;
        ssize_t recv_synack_ret_val;
        ssize_t send_ack_ret_val;
} state_machine_context_t;

// clang-format off
static const char *get_connect_state_to_string(connect_internal_states _state)
{
        switch (_state)
        {
        case START_STATE:               return STRINGIFY(START_STATE);
        case SYN_RECEIVED_STATE:            return STRINGIFY(SYN_RECEIVED_STATE);
        case SYNACK_SENT_STATE:     return STRINGIFY(SYNACK_SENT_STATE);
        case ACK_RECEIVED_STATE:            return STRINGIFY(ACK_RECEIVED_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??CONNECT_STATE??";
        }
}
// clang-format on

/* TODO: inline actions of states of state machine. See what happens. */

static connect_internal_states execute_start_state(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                   socklen_t _address_len, state_machine_context_t *_context)
{
        _context->send_syn_ret_val = send_syn_segment(_socket, _address, _address_len);
        if (_context->send_syn_ret_val == MICROTCP_SEND_SYN_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_syn_ret_val == MICROTCP_SEND_SYN_ERROR)
                return START_STATE;
        update_socket_sent_counters(_socket, _context->send_syn_ret_val);
        return SYN_RECEIVED_STATE;
}

static connect_internal_states execute_syn_sent_state(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                      socklen_t _address_len, state_machine_context_t *_context)
{
        _context->recv_synack_ret_val = receive_synack_segment(_socket, (struct sockaddr *)_address, _address_len);
        update_socket_lost_counters(_socket, _context->recv_synack_ret_val);
        if (_context->recv_synack_ret_val == MICROTCP_RECV_SYN_ACK_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->recv_synack_ret_val == MICROTCP_RECV_SYN_ACK_TIMEOUT || /* Timeout occurred. */
            _context->recv_synack_ret_val == MICROTCP_RECV_SYN_ACK_ERROR)     /* Corrupt packet, etc */
                return START_STATE;
        update_socket_received_counters(_socket, _context->recv_synack_ret_val);
        return SYNACK_SENT_STATE;
}

static connect_internal_states execute_synack_received_state(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                             socklen_t _address_len, state_machine_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_segment(_socket, _address, _address_len);
        if (_context->send_ack_ret_val == MICROTCP_SEND_SYN_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_ack_ret_val == MICROTCP_SEND_SYN_ERROR)
                return SYNACK_SENT_STATE;
        update_socket_sent_counters(_socket, _context->send_ack_ret_val);
        return ACK_RECEIVED_STATE;
}

int microtcp_connect_state_machine(microtcp_sock_t *_socket, const struct sockaddr *const _address, socklen_t _address_len)
{
        state_machine_context_t context = {0};
        connect_internal_states current_connection_state = START_STATE;
        while (TRUE)
        {
                switch (current_connection_state)
                {
                case START_STATE:
                        current_connection_state = execute_start_state(_socket, _address, _address_len, &context);
                        break;
                case SYN_RECEIVED_STATE:
                        current_connection_state = execute_syn_sent_state(_socket, _address, _address_len, &context);
                        break;
                case SYNACK_SENT_STATE:
                        current_connection_state = execute_synack_received_state(_socket, _address, _address_len, &context);
                        break;
                case ACK_RECEIVED_STATE:
                        _socket->state = ESTABLISHED;
                        return MICROTCP_CONNECT_SUCCESS;
                case EXIT_FAILURE_STATE:
                        deallocate_receive_buffer(_socket->recvbuf);
                        return MICROTCP_CONNECT_FAILURE;
                default:
                        LOG_ERROR("State machine entered an undefined state. Prior state = %s",
                                  get_connect_state_to_string(current_connection_state));
                        current_connection_state = EXIT_FAILURE_STATE;
                        break;
                }
        }
}