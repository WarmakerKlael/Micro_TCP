#ifndef CORE_RECEIVE_RING_BUFFER_H
#define CORE_RECEIVE_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include "microtcp_defines.h"
#include "microtcp.h"

typedef struct receive_ring_buffer receive_ring_buffer_t;
typedef struct microtcp_segment microtcp_segment_t;

receive_ring_buffer_t *rrb_create(size_t _rrb_size, uint32_t _current_seq_number);
status_t rrb_destroy(receive_ring_buffer_t **_rrb_address);

/**
 * @returns Number of bytes, appended to the Receive-Ring-Buffer
 */
uint32_t rrb_append(receive_ring_buffer_t *_rrb, const microtcp_segment_t *_segment);
uint32_t rrb_pop(receive_ring_buffer_t *_rrb, void *_buffer, uint32_t _buffer_size);

uint32_t rrb_size(const receive_ring_buffer_t *_rrb);
uint32_t rrb_consumable_bytes(receive_ring_buffer_t *_rrb);

#endif /* CORE_RECEIVE_RING_BUFFER_H */