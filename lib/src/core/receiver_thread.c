#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "core/misc.h"
#include "core/receiver_thread.h"
#include "core/segment_io.h"
#include "core/segment_processing.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "segment_io_internal.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"

/* TODO: Check the validity of the is_ functions... */
static inline _Bool is_old_packet(uint32_t _seq_number, uint32_t _ack_number, uint32_t _remaining_space);
static inline _Bool is_segment_within_window(uint32_t _seq_number, uint32_t _ack_number);
static inline ssize_t receive_data_segment(microtcp_sock_t *const _socket);

#define RECEIVER_FATAL_ERROR -1
#define RECEIVER_FINACK_RECEIVED 0
#define PASS()

#define SEQ_NUM_WRAP_THRESHOLD INT32_MAX

void *microtcp_receiver_thread(void *_args)
{
        microtcp_sock_t *_socket = ((struct microtcp_receiver_thread_args *)_args)->_socket;
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ESTABLISHED);
#endif /* DEBUG_MODE */

        set_socket_recvfrom_to_block(_socket);
        const uint32_t assembly_buffer_len = get_bytestream_assembly_buffer_len();

        while (TRUE)
        {
                ssize_t recv_data_ret_val = receive_data_segment(_socket);
                if (recv_data_ret_val == RECV_SEGMENT_ERROR)
                        continue; /* Faulty segment, discard it. */
                if (recv_data_ret_val == RECV_SEGMENT_FATAL_ERROR)
                        LOG_ERROR_RETURN((void *)RECEIVER_FATAL_ERROR, "Fatal-Error occured while loading the assembly-buffer.");

                const microtcp_header_t new_header = _socket->segment_build_buffer->header;

                /* Check if packet is old */
                if (is_old_packet(new_header.seq_number, _socket->ack_number, assembly_buffer_len - _socket->buf_fill_level))
                        continue; /* Old retransmition, discard it. */
                /* Check if packet is within bounds. */
                if (!is_segment_within_window(new_header.seq_number, _socket->ack_number))
                {
                        LOG_WARNING("Received segment ahead of receive window: Socket's ack_number = %lu, segment's seq_number = %lu",
                                    _socket->ack_number, new_header.seq_number);
                        continue; /* Packet sequence number is ahead of receive window. */
                }

                if (new_header.control == (FIN_BIT | ACK_BIT))
                        return (void *)RECEIVER_FINACK_RECEIVED;

                /* If control flow is here; received segment is valid data segment. */
                if (new_header.seq_number == _socket->ack_number)
                {
                        /* To write data into buffer you must acquire the mutex first. */
                        pthread_mutex_lock(&(_socket->assembly_buffer_mutex));
                        size_t bytes_to_copy = MIN((uint32_t)recv_data_ret_val, assembly_buffer_len - (uint32_t)_socket->buf_fill_level);
                        memcpy(_socket->bytestream_assembly_buffer + _socket->buf_fill_level,
                               _socket->segment_receive_buffer->raw_payload_bytes,
                               bytes_to_copy);
                        _socket->curr_win_size -= bytes_to_copy;
                        _socket->buf_fill_level += bytes_to_copy;
                        _socket->ack_number += bytes_to_copy;
                        pthread_mutex_unlock(&(_socket->assembly_buffer_mutex));
                        send_ack_control_segment(_socket, _socket->peer_socket_address, sizeof(*(_socket->peer_socket_address)));
                }
                else
                {
                        /* segment is in valid range, but came out of order. */
                        /* currenly we discard it. */
                        PASS();
                }
        }
}

#define SEQ_GEQ(a, b) ((int32_t)((a) - (b)) >= 0)
#define SEQ_LT(a, b) ((int32_t)((a) - (b)) < 0)

static inline _Bool is_old_packet(uint32_t _seq_number, uint32_t _ack_number, uint32_t _remaining_space)
{
        uint32_t end = _ack_number + _remaining_space; // Wraps automatically in uint32_t

        // We want to check: _ack_number <= _seq_number < _ack_number + _remaining_space, in wrap-aware sense
        return SEQ_GEQ(_seq_number, _ack_number) && SEQ_LT(_seq_number, end);
}

static inline _Bool is_segment_within_window(uint32_t _seq_number, uint32_t _ack_number)
{
        // "old" = behind _ack_number (in wrap sense) and within some threshold
        int32_t diff = (int32_t)(_ack_number - _seq_number);

        // 1) diff > 0  means _seq_number < _ack_number in the wrap sense
        // 2) diff < SEQ_NUM_WRAP_THRESHOLD  means within your old-age threshold
        return diff > 0 && diff < SEQ_NUM_WRAP_THRESHOLD;
}

#undef SEQ_GEQ
#undef SEQ_LT

/* Just receive a data segment, and pass it to the receive buffer... not your job to pass it to assembly. */
static inline ssize_t receive_data_segment(microtcp_sock_t *const _socket)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket->peer_socket_address);
#endif /* DEBUG_MODE */
        struct sockaddr *peer_address = _socket->peer_socket_address;
        socklen_t peer_address_len = sizeof(*peer_address);
        const ssize_t maximum_segment_size = MICROTCP_MSS;
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        int flags = MSG_TRUNC;

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, maximum_segment_size, flags, peer_address, &peer_address_len);
        if (recvfrom_ret_val == RECVFROM_SHUTDOWN)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned 0, which points a closed connection; but underlying protocol is UDP, so this should not happen.");
        if (recvfrom_ret_val == RECVFROM_ERROR)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned %d, errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));

        if (recvfrom_ret_val > maximum_segment_size)
                LOG_ERROR_RETURN(RECV_SEGMENT_ERROR, "Received illegal segment; Segment's size = %d;  %s = %d .",
                                 recvfrom_ret_val, STRINGIFY(maximum_segment_size), maximum_segment_size);

        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");
        extract_microtcp_segment(&(_socket->segment_receive_buffer), bytestream_buffer, recvfrom_ret_val);
        microtcp_segment_t *data_segment = _socket->segment_receive_buffer;
        if (data_segment == NULL)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Extracting data segment resulted to a NULL pointer.");

#ifdef DEBUG_MODE
        SMART_ASSERT(recvfrom_ret_val > 0);
#endif /* DEBUG_MODE*/

        LOG_INFO_RETURN(data_segment->header.data_len, "data segment received; seq_number = %lu, data_len = %lu",
                        data_segment->header.seq_number, data_segment->header.data_len);
}
