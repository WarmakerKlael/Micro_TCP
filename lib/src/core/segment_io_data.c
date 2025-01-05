#include <errno.h>
#include "microtcp.h"
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"
#include "core/send_queue.h"
#include "segment_io_internal.h"
#include "core/segment_io.h"
#include "smart_assert.h"
#include "logging/microtcp_logger.h"

size_t send_data_segment(microtcp_sock_t *const _socket, const void *const _buffer, const size_t _segment_size)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _buffer != NULL, _segment_size > 0);
        const microtcp_payload_t payload = {.raw_bytes = (uint8_t *)_buffer, .size = _segment_size};
        microtcp_segment_t *data_segment = construct_microtcp_segment(_socket, ACK_BIT, payload);
        const void *data_bytestream = serialize_microtcp_segment(_socket, data_segment);
        const size_t data_bytestream_size = _segment_size + sizeof(microtcp_header_t);

        ssize_t sendto_ret_val = sendto(_socket->sd, data_bytestream, data_bytestream_size,
                                        NO_SENDTO_FLAGS, _socket->peer_address, sizeof(*_socket->peer_address));
        if (sendto_ret_val == SENDTO_ERROR)
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Sending data segment failed. sendto() errno(%d):%s.",
                                 errno, strerror(errno));
        DEBUG_SMART_ASSERT(sendto_ret_val > 0);
        if ((size_t)sendto_ret_val != data_bytestream_size)
                LOG_WARNING_RETURN(SEND_SEGMENT_ERROR, "Failed sending data segment. sendto() sent %d bytes, but was asked to sent %d bytes",
                                   sendto_ret_val, data_bytestream_size);
        update_socket_sent_counters(_socket, sendto_ret_val);
        return _segment_size;
}