

#include "fsm/microtcp_fsm.h"
#include "microtcp.h"
#include "microtcp_defines.h"
#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"
#include "core/misc.h"
#include "core/microtcp_recv_impl.h"
#include "core/segment_processing.h"
#include "core/segment_io.h"
#include <threads.h>
#include <limits.h>
#include "microtcp_helper_functions.h"
#include "settings/microtcp_settings.h"

static __always_inline ssize_t handle_finack_reception(microtcp_sock_t *const _socket, const size_t _bytes_received)
{
        _socket->ack_number += FIN_SEQ_NUMBER_INCREMENT;
        if (_bytes_received == 0)
        {
                _socket->state = CLOSING_BY_PEER;
                return MICROTCP_RECV_FAILURE;
        }
        _socket->data_reception_with_finack = true;
        DEBUG_SMART_ASSERT(_bytes_received < SSIZE_MAX);
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
        const _Bool block = !(_flags & MSG_DONTWAIT);

        if (_socket->data_reception_with_finack == true) /* Received finack on previous called, but there was data available. */
        {
                _socket->state = CLOSING_BY_PEER;
                return MICROTCP_RECV_FAILURE;
        }

        size_t bytes_received = rrb_pop(bytestream_rrb, _buffer, _length); /* Pop any leftover bytes in RRB. */
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
                case RECV_SEGMENT_WINACK_RECEIVED:
                        if (send_ack_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address)) == SEND_SEGMENT_FATAL_ERROR)
                                return MICROTCP_RECV_FAILURE;
                        break;
                case RECV_SEGMENT_TIMEOUT:
                        bytes_received += rrb_pop(bytestream_rrb, _buffer + bytes_received, _length - bytes_received); /* Pop any remaining bytes.*/
                        if (_flags & MSG_WAITALL)
                                break;

                        DEBUG_SMART_ASSERT(bytes_received < SSIZE_MAX);
                        return (ssize_t)bytes_received;
                default:
                {
                        uint32_t appended_bytes = rrb_append(bytestream_rrb, _socket->segment_receive_buffer);
                        if (RARE_CASE(appended_bytes == 0))
                                break;

                        _socket->ack_number = rrb_last_consumed_seq_number(bytestream_rrb) + rrb_consumable_bytes(bytestream_rrb) + 1;
                        bytes_received += rrb_pop(bytestream_rrb, _buffer + bytes_received, _length - bytes_received);
                        _socket->curr_win_size = cached_rrb_size - rrb_consumable_bytes(bytestream_rrb);
                        send_ack_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address)); /* If curr_win_size == 0, we still send ACK. */
                        break;
                }
                }
        }
        DEBUG_SMART_ASSERT(bytes_received < SSIZE_MAX);
        return (ssize_t)bytes_received;
}

ssize_t microtcp_recv_timed_impl(microtcp_sock_t *const _socket, uint8_t *const _buffer,
                                 const size_t _length, const struct timeval _max_idle_time)
{

        const time_t microtcp_recv_timeout_usec = timeval_to_usec(get_microtcp_ack_timeout());
        const time_t max_idle_time_usec = timeval_to_usec(_max_idle_time);
        DEBUG_SMART_ASSERT(_socket != NULL, _buffer != NULL);
        DEBUG_SMART_ASSERT(_length > 0, _length < SSIZE_MAX, max_idle_time_usec > 0);

        if (max_idle_time_usec < microtcp_recv_timeout_usec)
                LOG_WARNING("Argument `%s` [%lldusec] < timeout of `%s()` [%lldusec]. Timeout of `%s()'s` will be respected.",
                            STRINGIFY(_max_idle_time), max_idle_time_usec,
                            STRINGIFY(microtcp_recv), microtcp_recv_timeout_usec,
                            STRINGIFY(microtcp_recv));

        time_t current_idle_time_usec = 0;
        size_t bytes_received = 0;
        while (bytes_received != _length)
        {
                ssize_t recv_ret_val = microtcp_recv_impl(_socket, _buffer + bytes_received, _length - bytes_received, 0);
                if (RARE_CASE(recv_ret_val == MICROTCP_RECV_FAILURE))
                        return (ssize_t)(bytes_received > 0 ? bytes_received : MICROTCP_RECV_FAILURE);
                if (RARE_CASE(recv_ret_val == MICROTCP_RECV_TIMEOUT))
                {
                        current_idle_time_usec += microtcp_recv_timeout_usec;
                        if (current_idle_time_usec >= max_idle_time_usec) /* Max time reached (or exceeded). */
                                LOG_WARNING_RETURN(bytes_received, "%s() `_max_idle_time` timer exhausted; `bytes_received = %zd`",
                                                   __func__, bytes_received);
                        continue;
                }
                DEBUG_SMART_ASSERT(recv_ret_val > 0);
                bytes_received += recv_ret_val;
                current_idle_time_usec = 0; /* Reset idle time counter. */
        }
        DEBUG_SMART_ASSERT(bytes_received <= _length); /* We should never received more bytes than asked... (Just a final silly check). */
        return (ssize_t)bytes_received;
}
