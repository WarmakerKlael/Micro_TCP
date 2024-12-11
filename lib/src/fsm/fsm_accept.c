#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_core.h"
#include "logging/microtcp_logger.h"

#define LINUX_DEFAULT_ACCEPT_TIMEOUTS 5
static size_t accept_synack_retries = LINUX_DEFAULT_ACCEPT_TIMEOUTS; /* Default. Can be changed from following "API". */

size_t get_accept_synack_retries(void)
{
        return accept_synack_retries;
}

void set_accept_synack_retries(size_t _retries_count)
{
        accept_synack_retries = _retries_count;
}

typedef enum
{
        LISTEN_SUBSTATE = LISTEN, /* Initial substate. FSM entry point. */
        SYN_RECEIVED_SUBSTATE,
        SYNACK_SENT_SUBSTATE,
        ACK_RECEIVED_SUBSTATE,
        CONNECTION_ESTABLISHED_SUBSTATE = ESTABLISHED, /* Terminal substate (success). FSM exit point. */
        EXIT_FAILURE_SUBSTATE = -1,                    /* Terminal substate (failure). FSM exit point. */
} accept_fsm_substates;

typedef struct
{
        ssize_t recv_syn_ret_val;
        ssize_t send_synack_ret_val;
        ssize_t recv_ack_ret_val;

        ssize_t socket_init_seq_num;
        /* By adding the socket's initial sequence number in the
         * FSM's context, we avoid having to do errorneous
         * subtractions, like seq_number -= 1, in order to match ISN.
         */
        ssize_t synack_retries_counter;
} fsm_context_t;

// clang-format off
static const char *convert_substate_to_string(accept_fsm_substates _substate)
{
        switch (_substate)
        {
        case LISTEN_SUBSTATE:                   return STRINGIFY(LISTEN_SUBSTATE);
        case SYN_RECEIVED_SUBSTATE:             return STRINGIFY(SYN_RECEIVED_SUBSTATE);
        case SYNACK_SENT_SUBSTATE:              return STRINGIFY(SYNACK_SENT_SUBSTATE);
        case ACK_RECEIVED_SUBSTATE:             return STRINGIFY(ACK_RECEIVED_SUBSTATE);
        case CONNECTION_ESTABLISHED_SUBSTATE:   return STRINGIFY(CONNECTION_ESTABLISHED_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:             return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                                return "??ACCEPT_SUBSTATE??";
        }
}
// clang-format on

static accept_fsm_substates execute_listen_substate(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                    socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_syn_ret_val = receive_syn_control_segment(_socket, _address, _address_len);
        switch (_context->recv_syn_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        /* Actions on the following three cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_RST_BIT:
                /* So far same action with cases above... Maybe you want to print a specific log? (Here and let it fall through..)*/
        case RECV_SEGMENT_TIMEOUT:
                /* If received SYN segment was corrupted, or for any other reason caused errors;
                 * We discard it. If client still wants to make a connection with the server
                 * it will resend a SYN packet after a timeout occurs on its side. */
                return LISTEN_SUBSTATE;

        default:
                update_socket_received_counters(_socket, _context->recv_syn_ret_val);
                return SYN_RECEIVED_SUBSTATE;
        }
}

static accept_fsm_substates execute_syn_received_substate(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                          socklen_t _address_len, fsm_context_t *_context)
{
        /* When ever we receive a SYN, we reset socket's sequence number, as it might be a retry. */
        _socket->seq_number = _context->socket_init_seq_num;

        _context->send_synack_ret_val = send_synack_control_segment(_socket, _address, _address_len);
        switch (_context->send_synack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        case SEND_SEGMENT_ERROR:
                return SYN_RECEIVED_SUBSTATE;

        default:
                /* In TCP, segments containing control flags (e.g., SYN, FIN),
                 * other than pure ACKs, are treated as carrying a virtual payload.
                 * As a result, they are incrementing the sequence number by 1. */
                _socket->seq_number += SENT_SYN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_synack_ret_val);
                return SYNACK_SENT_SUBSTATE;
        }
}

static accept_fsm_substates execute_synack_sent_substate(microtcp_sock_t *_socket, struct sockaddr *const _address,
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
                update_socket_lost_counters(_socket, _context->send_synack_ret_val);
                if (_context->synack_retries_counter > 0)
                {
                        LOG_WARNING("Waiting for FINAL `ACK`.\n");
                        _context->synack_retries_counter--;
                        return SYN_RECEIVED_SUBSTATE; /* go resend SYN|ACK as it was might lost */
                }
                LOG_WARNING("Synack retries exhausted; Going back in waiting for `SYN`.\n");
                _context->synack_retries_counter = get_accept_synack_retries(); /* Reset contex's counter. */
                return LISTEN_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                /* So far same action with cases above... Maybe you want to print a specific log? (Here and let it fall through..)*/
                return LISTEN_SUBSTATE;

        default:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                return ACK_RECEIVED_SUBSTATE;
        }
}

static accept_fsm_substates execute_ack_received_substate(microtcp_sock_t *_socket, struct sockaddr *const _address,
                                                          socklen_t _address_len, fsm_context_t *_context)
{
        _socket->peer_socket_address = _address;
        _socket->state = ESTABLISHED;
        // TODO: LOG FSM's result (Like: "FSM succeded: Established connection.") or similar.
        return CONNECTION_ESTABLISHED_SUBSTATE;
}

/* Argument check is for the most part redundant are FSM callers, have validated their input arguments. */
int microtcp_accept_fsm(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_ACCEPT_FAILURE, _socket, LISTEN);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_ACCEPT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_ACCEPT_FAILURE, _address_len, sizeof(*_address));
        /* No argument validation needed. FSMs are called from their
         * respective functions which already vildated their input arguments. */
        fsm_context_t context = {.socket_init_seq_num = _socket->seq_number,
                                 .synack_retries_counter = get_accept_synack_retries()};

        accept_fsm_substates current_substate = LISTEN_SUBSTATE;
        while (TRUE)
        {
                switch (current_substate)
                {
                case LISTEN_SUBSTATE:
                        current_substate = execute_listen_substate(_socket, _address, _address_len, &context);
                        break;
                case SYN_RECEIVED_SUBSTATE:
                        current_substate = execute_syn_received_substate(_socket, _address, _address_len, &context);
                        break;
                case SYNACK_SENT_SUBSTATE:
                        current_substate = execute_synack_sent_substate(_socket, _address, _address_len, &context);
                        break;
                case ACK_RECEIVED_SUBSTATE:
                        current_substate = execute_ack_received_substate(_socket, _address, _address_len, &context);
                        break;
                case CONNECTION_ESTABLISHED_SUBSTATE:
                        return MICROTCP_ACCEPT_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        return MICROTCP_ACCEPT_FAILURE;
                default:
                        LOG_ERROR("Accept()'s FSM entered an `undefined` substate. Prior substate = %s",
                                  convert_substate_to_string(current_substate));
                        current_substate = EXIT_FAILURE_SUBSTATE;
                        break;
                }
        }
}