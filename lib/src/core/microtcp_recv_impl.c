

#include "fsm/microtcp_fsm.h"
#include "microtcp.h"
#include "microtcp_core.h"
#include "microtcp_defines.h"
#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"
#include "core/misc.h"
#include "core/microtcp_recv_impl.h"
#include <threads.h>
#include "microtcp_helper_functions.h"

static __always_inline ssize_t handle_finack_reception(microtcp_sock_t *const _socket, const size_t _bytes_received)
{
        const microtcp_segment_t *const finack_segment = _socket->segment_receive_buffer;

        _socket->ack_number++;
        if (_bytes_received == 0)
        {
                _socket->state = CLOSING_BY_PEER;
                return MICROTCP_RECV_FAILURE;
        }
        _socket->data_reception_with_finack = true;
        DEBUG_SMART_ASSERT(_bytes_received < SIZE_MAX / 2);
        return (ssize_t)_bytes_received;
}

static __always_inline ssize_t handle_rst_reception(microtcp_sock_t *const _socket)
{
        _socket->state = RESET;
        LOG_ERROR_RETURN(MICROTCP_RECV_FAILURE, "Peer sent an RST. Socket enters %s state", get_microtcp_state_to_string(_socket->state));
}

/* _flags are validated by the caller. microtcp_recv() */
ssize_t microtcp_recv_impl(microtcp_sock_t *const _socket, uint8_t *const _buffer, const size_t _length, const int _flags)
{
        receive_ring_buffer_t *const bytestream_rrb = _socket->bytestream_rrb; /* Create local pointer to avoid dereferencing. */
        const size_t cached_rrb_size = rrb_size(bytestream_rrb);
        size_t bytes_received = 0;
        const _Bool block = !(_flags & MSG_DONTWAIT);

        if (_socket->data_reception_with_finack == true) /* Received finack on previous called, but there was data available. */
        {
                _socket->state = CLOSING_BY_PEER;
                return MICROTCP_RECV_FAILURE;
        }

        bytes_received += rrb_pop(bytestream_rrb, _buffer + bytes_received, _length - bytes_received); /* Pop any leftover bytes in RRB. */
        while (bytes_received != _length)
        {
                ssize_t receive_data_ret_val = receive_data_segment(_socket, block);
                switch (receive_data_ret_val)
                {
                case RECV_SEGMENT_ERROR:
                        break; /* Faulty segment, ignore it. */
                case RECV_SEGMENT_FATAL_ERROR:
                        return MICROTCP_RECV_FAILURE;
                case RECV_SEGMENT_FINACK_UNEXPECTED:
                        if (_socket->segment_receive_buffer->header.seq_number == _socket->ack_number)
                                return handle_finack_reception(_socket, bytes_received);
                        LOG_WARNING("Protocol lost sychronization, received FIN|ACK, with mismatched `seq_number`; Could also be out-of-order (ignored)");
                        break;
                case RECV_SEGMENT_RST_RECEIVED:
                        return handle_rst_reception(_socket);
                case RECV_SEGMENT_TIMEOUT:
                        bytes_received += rrb_pop(bytestream_rrb, _buffer + bytes_received, _length - bytes_received); /* Pop any remaining bytes.*/
                        if (_flags & MSG_WAITALL)
                                break;
                        return bytes_received;
                default:
                {
                        uint32_t appended_bytes = rrb_append(bytestream_rrb, _socket->segment_receive_buffer);
                        if (RARE_CASE(appended_bytes == 0))
                                break;

                        _socket->ack_number = rrb_last_consumed_seq_number(bytestream_rrb) + rrb_consumable_bytes(bytestream_rrb) + 1; /* TODO: optimize... */
                                                                                                                                       // if (rrb_consumable_bytes(bytestream_rrb) == cached_rrb_size || rrb_consumable_bytes(bytestream_rrb) + bytes_received >= _length)
                        bytes_received += rrb_pop(bytestream_rrb, _buffer + bytes_received, _length - bytes_received);
                        _socket->curr_win_size = cached_rrb_size - rrb_consumable_bytes(bytestream_rrb);
                        send_ack_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address)); /* If curr_win_size == 0, we still send ACK. */
                        printf("SENT ACK , ack_number sent == %u\n", _socket->ack_number);
                }
                }
        }
        return bytes_received;
}
