#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_settings.h"
#include "microtcp_defines.h"
#include "microtcp_core.h"
#include "logging/microtcp_logger.h"

typedef enum
{
        CLOSED_SUBSTATE = CLOSED, /* Initial substate. FSM entry point. */
        SYN_SENT_SUBSTATE,
        SYNACK_RECEIVED_SUBSTATE,
        ACK_SENT_SUBSTATE,
        CONNESTION_ESTABLISHED_SUBSTATE = ESTABLISHED, /* Terminal substate (success). FSM exit point. */
        EXIT_FAILURE_SUBSTATE = -1                     /* Terminal substate (failure). FSM exit point. */
} connect_fsm_substates;

typedef struct
{
        ssize_t send_syn_ret_val;
        ssize_t recv_synack_ret_val;
        ssize_t send_ack_ret_val;

        ssize_t socket_init_seq_num;
        /* By adding the socket's initial sequence number in the
         * FSM's context, we avoid having to do errorneous
         * subtractions, like seq_number -= 1, in order to match ISN.
         */
        ssize_t rst_retries_counter;
} fsm_context_t;

// clang-format off
static const char *convert_substate_to_string(connect_fsm_substates _substate)
{
        switch (_substate)
        {
        case CLOSED_SUBSTATE:                   return STRINGIFY(CLOSED_SUBSTATE);
        case SYN_SENT_SUBSTATE:                 return STRINGIFY(SYN_SENT_SUBSTATE);
        case SYNACK_RECEIVED_SUBSTATE:          return STRINGIFY(SYNACK_RECEIVED_SUBSTATE);
        case ACK_SENT_SUBSTATE:                 return STRINGIFY(ACK_SENT_SUBSTATE);
        case CONNESTION_ESTABLISHED_SUBSTATE:   return STRINGIFY(CONNESTION_ESTABLISHED_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:             return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                                return "??CONNECT_SUBSTATE??";
        }
}
// clang-format on

static connect_fsm_substates execute_closed_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                     socklen_t _address_len, fsm_context_t *_context)
{
        /* When ever we start, we reset socket's sequence number, as it might be a retry. */
        _socket->seq_number = _context->socket_init_seq_num;
        _context->send_syn_ret_val = send_syn_control_segment(_socket, _address, _address_len);
        switch (_context->send_syn_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;
        case SEND_SEGMENT_ERROR:
                return CLOSED_SUBSTATE;
        default:
                /* In TCP, segments containing control flags (e.g., SYN, FIN),
                 * other than pure ACKs, are treated as carrying a virtual payload.
                 * As a result, they are incrementing the sequence number by 1. */
                _socket->seq_number += SENT_SYN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_syn_ret_val);
                return SYN_SENT_SUBSTATE;
        }
}

static connect_fsm_substates execute_syn_sent_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                       socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_synack_ret_val = receive_synack_control_segment(_socket, (struct sockaddr *)_address, _address_len);
        switch (_context->recv_synack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_syn_ret_val);
                return CLOSED_SUBSTATE;

        case RECV_SEGMENT_RST_BIT:
                LOG_WARNING("Expected = `SYN|ACK` flag; Received = `RST`; Retries left = %d", _context->rst_retries_counter);
                if (_context->rst_retries_counter == 0)
                        LOG_ERROR_RETURN(EXIT_FAILURE_SUBSTATE, "Connect exhausted its retries for attempting to connect, after receiving `RST` flag.");
                _context->rst_retries_counter--;
                return CLOSED_SUBSTATE;

        default:
                update_socket_received_counters(_socket, _context->recv_synack_ret_val);
                return SYNACK_RECEIVED_SUBSTATE;
        }
}

static connect_fsm_substates execute_synack_received_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                              socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return EXIT_FAILURE_SUBSTATE;
        case SEND_SEGMENT_ERROR:
                return SYNACK_RECEIVED_SUBSTATE;
        default:
                update_socket_sent_counters(_socket, _context->send_ack_ret_val);
                return ACK_SENT_SUBSTATE;
        }
}

static connect_fsm_substates execute_ack_sent_substate(microtcp_sock_t *_socket, const struct sockaddr *const _address,
                                                       socklen_t _address_len, fsm_context_t *_context)
{
        _socket->peer_socket_address = (struct sockaddr *)_address;
        _socket->state = ESTABLISHED;
        // TODO: LOG (just like accept()'s FSM)
        return CONNESTION_ESTABLISHED_SUBSTATE;
}

/* Argument check is for the most part redundant are FSM callers, have validated their input arguments. */
int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *const _address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(*_address));

        fsm_context_t context = {.socket_init_seq_num = _socket->seq_number,
                                 .rst_retries_counter = get_connect_rst_retries()};
        connect_fsm_substates current_substate = CLOSED_SUBSTATE;
        while (TRUE)
        {
                switch (current_substate)
                {
                case CLOSED_SUBSTATE:
                        current_substate = execute_closed_substate(_socket, _address, _address_len, &context);
                        break;
                case SYN_SENT_SUBSTATE:
                        current_substate = execute_syn_sent_substate(_socket, _address, _address_len, &context);
                        break;
                case SYNACK_RECEIVED_SUBSTATE:
                        current_substate = execute_synack_received_substate(_socket, _address, _address_len, &context);
                        break;
                case ACK_SENT_SUBSTATE:
                        current_substate = execute_ack_sent_substate(_socket, _address, _address_len, &context);
                case CONNESTION_ESTABLISHED_SUBSTATE:
                        return MICROTCP_CONNECT_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        return MICROTCP_CONNECT_FAILURE;
                default:
                        LOG_ERROR("Connect()'s FSM entered an `undefined` substate. Prior substate = %s",
                                  convert_substate_to_string(current_substate));
                        current_substate = EXIT_FAILURE_SUBSTATE;
                        break;
                }
        }
}