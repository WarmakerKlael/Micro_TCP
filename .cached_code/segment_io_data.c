#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
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

/* Just receive a data segment, and pass it to the receive buffer... not your job to pass it to assembly. */
ssize_t receive_data_segment(microtcp_sock_t *const _socket)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket->peer_socket_address);
#endif /* DEBUG_MODE */
        struct sockaddr *peer_address = _socket->peer_socket_address;
        socklen_t peer_address_len = sizeof(*peer_address);
        const size_t maximum_segment_size = MICROTCP_MSS;
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        int flags = MSG_TRUNC;

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, maximum_segment_size, flags, peer_address, &peer_address_len);
        if (recvfrom_ret_val == RECVFROM_ERROR && (errno == EAGAIN || errno == EWOULDBLOCK))
                return RECV_SEGMENT_TIMEOUT;
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