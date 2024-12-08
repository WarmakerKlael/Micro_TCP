#include "state_machines/state_machines.h"
#include "state_machines_common.h"
#include "microtcp_core_utils.h"
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

        ssize_t socket_init_seq_num;
        /* By adding the socket's initial sequence number in the
         * state machine's context, we avoid having to do errorneous
         * subtractions, like seq_number -= 1, in order to match ISN.
         */
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

        /* If received SYN segment was corrupted, or for any other reason caused errors;
         * We discard it. If client still wants to make a connection with the server
         * it will resend a SYN packet after a timeout occurs on its side. */
        if (_context->recv_syn_ret_val == MICROTCP_RECV_SYN_TIMEOUT ||
            _context->recv_syn_ret_val == MICROTCP_RECV_SYN_ERROR)
                return START_STATE;
        update_socket_received_counters(_socket, _context->recv_syn_ret_val);
        return SYN_RECEIVED_STATE;
}

static accept_internal_states execute_syn_received_state(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                         socklen_t _address_len, state_machine_context_t *_context)
{
        /* When ever we receive a SYN, we reset socket's sequence number, as it might be a retry. */
        _socket->seq_number = _context->socket_init_seq_num;

        _context->send_synack_ret_val = send_synack_segment(_socket, _address, _address_len);
        if (_context->send_synack_ret_val == MICROTCP_SEND_SYN_ACK_FATAL_ERROR)
                return EXIT_FAILURE_STATE;
        if (_context->send_synack_ret_val == MICROTCP_SEND_SYN_ACK_ERROR)
                return SYN_RECEIVED_STATE;

        /* In TCP, segments containing control flags (e.g., SYN, FIN),
         * other than pure ACKs, are treated as carrying a virtual payload.
         * As a result, they are incrementing the sequence number by 1. */
        _socket->seq_number += SENT_SYN_SEQUENCE_NUMBER_INCREMENT;
        update_socket_sent_counters(_socket, _context->send_synack_ret_val);
        return SYNACK_SENT_STATE;
}

static accept_internal_states execute_synack_sent_state(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                        socklen_t _address_len, state_machine_context_t *_context)
{
        _context->recv_ack_ret_val = receive_ack_segment(_socket, _address, _address_len);

        if (_context->recv_ack_ret_val == MICROTCP_RECV_ACK_FATAL_ERROR)
                return EXIT_FAILURE_STATE;

        /* If received ACK segment was corrupted, or for any other reason caused errors;
         * We discard it. We resend SYN-ACK segment. */
        if (_context->recv_syn_ret_val == MICROTCP_RECV_SYN_TIMEOUT ||
            _context->recv_syn_ret_val == MICROTCP_RECV_SYN_ERROR)
        {
                update_socket_lost_counters(_socket, _context->send_synack_ret_val);
                return SYN_RECEIVED_STATE;
        }
        update_socket_received_counters(_socket, _context->recv_ack_ret_val);
        return ACK_RECEIVED_STATE;

}

int microtcp_accept_state_machine(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len)
{

        state_machine_context_t context = {0};
        context.socket_init_seq_num = _socket->seq_number;
        accept_internal_states current_connection_state = START_STATE;
        while (TRUE)
        {
                switch (current_connection_state)
                {
                case START_STATE:
                        current_connection_state = execute_start_state(_socket, _address, _address_len, &context);
                        break;
                case SYN_RECEIVED_STATE:
                        current_connection_state = execute_syn_received_state(_socket,_address, _address_len, &context);
                        break;
                case SYNACK_SENT_STATE:
                        current_connection_state = execute_synack_sent_state(_socket, _address, _address_len, &context);
                        break;
                case ACK_RECEIVED_STATE:
                        _socket->state = ESTABLISHED;
                case EXIT_FAILURE_STATE:
                default:
                        LOG_ERROR("Connect state machine entered an undefined state. Prior state = %s",
                                  get_accept_state_to_string(current_connection_state));
                        current_connection_state = EXIT_FAILURE_STATE;
                        break;
                }
        }
}