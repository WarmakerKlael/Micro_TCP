static shutdown_active_fsm_substates_t execute_fin_double_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                   socklen_t _address_len, fsm_context_t *_context)
{
        _context->errno = DOUBLE_FIN;
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case RECV_SEGMENT_UNEXPECTED_FINACK:
                update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
                update_socket_sent_counters(_socket, _context->send_ack_ret_val);
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                _socket->seq_number = _context->socket_shutdown_isn;
                _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
                _socket->seq_number += SENT_FIN_SEQUENCE_NUMBER_INCREMENT;
                update_socket_sent_counters(_socket, _context->send_finack_ret_val);
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_RST_BIT:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                _context->errno = RST_EXPECTED_ACK;
                return CLOSED_1_SUBSTATE;
        default:
                update_socket_received_counters(_socket, _context->recv_ack_ret_val);
                return TIME_WAIT_SUBSTATE;
        }
}