#ifndef CORE_SEGMENT_PROCESSING_H
#define CORE_SEGMENT_PROCESSING_H

#include <stddef.h>
#include <stdint.h>
#include "microtcp.h"

struct microtcp_segment
{
        microtcp_header_t header;
        uint8_t *raw_payload_bytes;
};

typedef struct
{
        uint8_t *raw_bytes;
        uint16_t size;
} microtcp_payload_t;

microtcp_segment_t *construct_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload);
void *serialize_microtcp_segment(microtcp_sock_t *_socket, microtcp_segment_t *_segment);
_Bool is_valid_microtcp_bytestream(void *_bytestream_buffer, ssize_t _bytestream_buffer_length);
void extract_microtcp_segment(microtcp_segment_t **_segment_buffer, void *_bytestream_buffer, size_t _bytestream_buffer_length);

#endif /* CORE_SEGMENT_PROCESSING_H */