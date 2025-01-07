#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "core/misc.h"
#include "core/receiver_thread.h"
#include "core/segment_io.h"
#include "core/segment_processing.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"
#include "stdatomic.h"
#include "core/send_queue.h"

static __always_inline void handle_seq_number_increment(microtcp_sock_t *_socket, uint32_t _received_ack_number, size_t _acked_segments);

void *receiver_thread(void *_data)
{
        receiver_data_t *data = (receiver_data_t *)_data;
        microtcp_sock_t *_socket = data->_socket;
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ESTABLISHED);
        const uint32_t rrb_size = get_bytestream_rrb_size();

        while (TRUE)
        {
                ssize_t receive_data_ret_val = receive_rt_segment(_socket, TRUE);
                switch (receive_data_ret_val)
                {
                case RECV_SEGMENT_ERROR:
                        break; /* Faulty segment, ignore it. */
                case RECV_SEGMENT_FATAL_ERROR:
                        LOG_ERROR("Receiver thread encountered fatal-error while receiving data segment");
                        data->return_code = FATAL_ERROR;
                        break;
                case RECV_SEGMENT_FINACK_UNEXPECTED:
                        LOG_WARNING("Receiver thread received FIN|ACK");
                        data->return_code = RECEIVED_FINACK;
                        break;
                case RECV_SEGMENT_RST_RECEIVED:
                        LOG_WARNING("Receiver thread received RST");
                        data->return_code = RECEIVED_RST;
                        break;
                case RECV_SEGMENT_TIMEOUT:
                        LOG_WARNING("Receiver thread TIMEOUT"); /* TODO: REMOVE THIS, just for debugging purposes. */
                        break;
                default:
                {
                        /* TODO: Are synced? USE ATOMICS*/
                        microtcp_segment_t *const segment = _socket->segment_receive_buffer;
                        if (segment->header.data_len > 0) /* DATA_PACKET. */
                        {
                                uint32_t appended_bytes = rrb_append(_socket->bytestream_rrb, _socket->segment_receive_buffer);
                                _socket->curr_win_size -= appended_bytes;
                                _socket->ack_number += appended_bytes;
                        }
                        else /* data_len == 0*/
                        {
                                const uint32_t received_ack_number = _socket->segment_receive_buffer->header.ack_number;
                                const size_t acked_segments = sq_dequeue(_socket->send_queue, received_ack_number);
                                handle_seq_number_increment(_socket, received_ack_number, acked_segments);
                        }
                }
                }

                /* Handles out-of-order, out-of-bounds (old and far ahead). */
                uint32_t appended_bytes = rrb_append(_socket->bytestream_rrb, _socket->segment_receive_buffer);
                // send_ack_control_segment(_socket, )
        }
}

static __always_inline void handle_seq_number_increment(microtcp_sock_t *const _socket, const uint32_t _received_ack_number, const size_t _acked_segments)
{
        if (_acked_segments > 0) /* That means received_ack_number matched to a sent segment. */
                _socket->seq_number = _received_ack_number;
}