static shutdown_active_fsm_substates_t execute_time_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        /* In TIME_WAIT state, we set our timer to expire after 2*MSL (according TCP protocol). */
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

        /* The following two cases result to going to `CLOSED_1_SUBSTATE`. If RST received you also log a warning message. */
        case RECV_SEGMENT_RST_BIT:
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                _context->errno = RST_EXPECTED_TIMEOUT;
        case RECV_SEGMENT_TIMEOUT: /* Timeout: Peer has likely completed shutdown_active. Transition to CLOSED_1_SUBSTATE. */
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_ERROR: /* Heard junk... Re enter current substate, if peer sent an ack and was corrupted, it will resend it, after its timeout. */
                return TIME_WAIT_SUBSTATE;
        default:
                update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                update_socket_received_counters(_socket, _context->recv_finack_ret_val);
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
        /* In case host's LAST ACK gets lost, and peer's FIN-ACK gets lost.
         * The connection from the perspective of the host is considered
         * closed, as for the host there is no away to identify such scenario. */
}