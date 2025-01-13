

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

#define HANDLE_FINACK_RECEPTION_AND_RETURN(_return_value, _socket)                                                 \
        do                                                                                                         \
        {                                                                                                          \
                const microtcp_segment_t *const finack_segment = (_socket)->segment_receive_buffer;                \
                                                                                                                   \
                if (finack_segment->header.seq_number == (_socket)->ack_number)                                    \
                        (_socket)->state = CLOSING_BY_PEER;                                                        \
                else                                                                                               \
                        LOG_ERROR("Protocol lost sychronization, received FIN|ACK, with mismatched `seq_number`"); \
                return (_return_value);                                                                            \
        } while (0)

#define HANDLE_RST_RECEPTION_AND_RETURN(_return_value, _socket)                                                                                \
        do                                                                                                                                     \
        {                                                                                                                                      \
                (_socket)->state = RESET;                                                                                                      \
                LOG_ERROR_RETURN((_return_value), "Peer sent an RST. Socket enters %s state", get_microtcp_state_to_string((_socket)->state)); \
        } while (0)

/* _flags are validated by the caller. microtcp_recv() */
ssize_t microtcp_recv_impl(microtcp_sock_t *const _socket, void *const _buffer, const size_t _length, const int _flags)
{
        receive_ring_buffer_t *const bytestream_rrb = _socket->bytestream_rrb; /* Create local pointer to avoid dereferencing. */
        const size_t cached_rrb_size = rrb_size(bytestream_rrb);
        size_t bytes_copied = 0;
        const _Bool block = !(_flags & MSG_DONTWAIT);

        bytes_copied += rrb_pop(bytestream_rrb, _buffer + bytes_copied, _length - bytes_copied); /* Pop any leftover bytes in RRB. */
        while (bytes_copied != _length)
        {
                ssize_t receive_data_ret_val = receive_data_segment(_socket, block);
                switch (receive_data_ret_val)
                {
                case RECV_SEGMENT_ERROR:
                        break; /* Faulty segment, ignore it. */
                case RECV_SEGMENT_FATAL_ERROR:
                        return -1;
                case RECV_SEGMENT_FINACK_UNEXPECTED:
                        HANDLE_FINACK_RECEPTION_AND_RETURN(bytes_copied, _socket);
                case RECV_SEGMENT_RST_RECEIVED:
                        HANDLE_RST_RECEPTION_AND_RETURN(-1, _socket);
                case RECV_SEGMENT_TIMEOUT:
                        bytes_copied += rrb_pop(bytestream_rrb, _buffer + bytes_copied, _length - bytes_copied); /* Pop any remaining bytes.*/
                        if (_flags & MSG_WAITALL)
                                break;
                        return bytes_copied;
                default:
                {
                        uint32_t appended_bytes = rrb_append(bytestream_rrb, _socket->segment_receive_buffer);
                        if (RARE_CASE(appended_bytes == 0))
                                break;

                        _socket->ack_number = rrb_last_consumed_seq_number(bytestream_rrb) + rrb_consumable_bytes(bytestream_rrb) + 1; /* TODO: optimize... */
                                                                                                                                       // if (rrb_consumable_bytes(bytestream_rrb) == cached_rrb_size || rrb_consumable_bytes(bytestream_rrb) + bytes_copied >= _length)
                        bytes_copied += rrb_pop(bytestream_rrb, _buffer + bytes_copied, _length - bytes_copied);
                        _socket->curr_win_size = cached_rrb_size - rrb_consumable_bytes(bytestream_rrb);
                        send_ack_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address)); /* If curr_win_size == 0, we still send ACK. */
                }
                }
        }
        return bytes_copied;
}