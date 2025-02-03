#include "core/segment_processing.h"
#include <string.h>
#include "crc32.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"

/* No memory allocation occurs, we just overwrite socket's segment_build_buffer. */
microtcp_segment_t *construct_microtcp_segment(microtcp_sock_t *_socket, uint32_t _seq_number, uint16_t _control, microtcp_payload_t _payload)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ~INVALID);
        RETURN_ERROR_IF_MICROTCP_PAYLOAD_INVALID(NULL, _payload);

        microtcp_segment_t *new_segment = _socket->segment_build_buffer;
        if (new_segment == NULL)
                return NULL;

        /* Header initialization. */
        new_segment->header.seq_number = _seq_number;
        new_segment->header.ack_number = _socket->ack_number;
        new_segment->header.control = _control;
        new_segment->header.window = _socket->curr_win_size; /* As sender we advertise our receive window, so opposite host wont overflow us .*/
        new_segment->header.data_len = _payload.size;
#ifndef OPTIMIZED_MODE
        new_segment->header.future_use0 = 0;
        new_segment->header.future_use1 = 0;
        new_segment->header.future_use2 = 0;
#endif /* OPTIMIZED_MODE */

        new_segment->header.checksum = 0; /* CRC32 checksum is calculated after linearizing this packet. */

        /* Set the payload pointer. We do not do deep copy, to avoid wasting memory.
         * MicroTCP will have to create a bitstream, where header and payload will have
         * to be compacted together. In that case deep copying is unavoidable.
         */
        new_segment->raw_payload_bytes = _payload.raw_bytes;

        return new_segment;
}

void *serialize_microtcp_segment(microtcp_sock_t *const _socket, microtcp_segment_t *const _segment)
{
        SMART_ASSERT(_socket != NULL, _segment != NULL);
        SMART_ASSERT(_socket->bytestream_build_buffer);

        if (_segment->header.checksum != 0)
        {
                _segment->header.checksum = 0;
                LOG_WARNING("Checksum should be zeroed before serialization; Checksum field zeroed.");
        }

        const uint16_t header_length = sizeof(_segment->header); /* Valid segments contain at lease sizeof(microtcp_header_t) bytes. */
        const uint16_t payload_length = _segment->header.data_len;

        const uint16_t bytestream_buffer_length = header_length + payload_length;
        void *bytestream_buffer = _socket->bytestream_build_buffer;

        memcpy(bytestream_buffer, &(_segment->header), header_length);
        memcpy((uint8_t *)bytestream_buffer + header_length, _segment->raw_payload_bytes, payload_length);

        /* Calculate crc32 checksum: */
        const uint32_t checksum_result = crc32(bytestream_buffer, bytestream_buffer_length);

        /* Implant the CRC checksum. */
        ((microtcp_header_t *)bytestream_buffer)->checksum = checksum_result;

        return bytestream_buffer;
}

_Bool is_valid_microtcp_bytestream(void *_bytestream_buffer, const ssize_t _bytestream_length)
{
        DEBUG_SMART_ASSERT(_bytestream_buffer != NULL);
        if (RARE_CASE(_bytestream_length < (ssize_t)MICROTCP_HEADER_SIZE || _bytestream_length > (ssize_t)MICROTCP_MTU))
                LOG_ERROR_RETURN(false, "Invalid bytestream due to `_bytestream_length` = %zd; Limits = [%zu, %zu]",
                                 _bytestream_length, MICROTCP_HEADER_SIZE, MICROTCP_MTU);

        uint32_t extracted_checksum = ((microtcp_header_t *)_bytestream_buffer)->checksum;

        /* Zeroing checksum to calculate crc. */
        ((microtcp_header_t *)_bytestream_buffer)->checksum = 0;
        uint32_t calculated_checksum = crc32(_bytestream_buffer, _bytestream_length);
        ((microtcp_header_t *)_bytestream_buffer)->checksum = extracted_checksum;

        return calculated_checksum == extracted_checksum;
}

void extract_microtcp_segment(microtcp_segment_t **_segment_buffer, void *_bytestream_buffer, size_t _bytestream_buffer_length)
{

        DEBUG_SMART_ASSERT(_segment_buffer != NULL);
        DEBUG_SMART_ASSERT(*_segment_buffer != NULL,
                           _bytestream_buffer != NULL,
                           _bytestream_buffer_length >= MICROTCP_HEADER_SIZE,
                           _bytestream_buffer_length <= MICROTCP_MTU);

        const size_t payload_size = _bytestream_buffer_length - MICROTCP_HEADER_SIZE;

        memcpy(&((*_segment_buffer)->header), _bytestream_buffer, sizeof(microtcp_header_t));

        (*_segment_buffer)->raw_payload_bytes = (payload_size > 0 ? (uint8_t *)_bytestream_buffer + MICROTCP_HEADER_SIZE : NULL);
}
