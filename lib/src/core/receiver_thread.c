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
#include "segment_io_internal.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"

/* TODO: Check the validity of the is_ functions... */
static inline _Bool is_old_packet(uint32_t _seq_number, uint32_t _ack_number, uint32_t _remaining_space);
static inline _Bool is_segment_within_window(uint32_t _seq_number, uint32_t _ack_number);
static inline ssize_t receive_data_segment(microtcp_sock_t *const _socket);

#define RECEIVER_FATAL_ERROR -1
#define RECEIVER_FINACK_RECEIVED 0
#define RECEIVER_RST_RECEIVED 0

#define SEQ_NUM_WRAP_THRESHOLD INT32_MAX

void *microtcp_receiver_thread(void *_args)
{
        microtcp_sock_t *_socket = ((struct microtcp_receiver_thread_args *)_args)->_socket;
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ESTABLISHED);
#endif /* DEBUG_MODE */

        set_socket_recvfrom_to_block(_socket);
        const uint32_t rrb_size = get_bytestream_rrb_size();

        while (TRUE)
        {
                ssize_t recv_data_ret_val = receive_data_segment(_socket);
                if (recv_data_ret_val == RECV_SEGMENT_ERROR)
                        continue; /* Faulty segment, discard it. */
                if (recv_data_ret_val == RECV_SEGMENT_FATAL_ERROR)
                        LOG_ERROR_RETURN((void *)RECEIVER_FATAL_ERROR, "Fatal-Error occured while loading the assembly-buffer.");

                const microtcp_header_t new_header = _socket->segment_build_buffer->header;

                if (new_header.control == (FIN_BIT | ACK_BIT))
                        return (void *)RECEIVER_FINACK_RECEIVED;
                if (new_header.control & RST_BIT)
                        return (void *)RECEIVER_RST_RECEIVED;

                /* Handles out-of-order, out-of-bounds (old and far ahead). */
                uint32_t appended_bytes = rrb_append(_socket->bytestream_rrb, _socket->segment_receive_buffer);
                _socket->curr_win_size -= appended_bytes;
                _socket->ack_number += appended_bytes;
                send_ack_control_segment(_socket, )
        }
}

/* Just receive a data segment, and pass it to the receive buffer... not your job to pass it to assembly. */
static inline ssize_t receive_data_segment(microtcp_sock_t *const _socket)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket->peer_address);
#endif /* DEBUG_MODE */
        struct sockaddr *peer_address = _socket->peer_address;
        socklen_t peer_address_len = sizeof(*peer_address);
        const ssize_t maximum_segment_size = MICROTCP_MSS;
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        const int flags = MSG_TRUNC;

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
