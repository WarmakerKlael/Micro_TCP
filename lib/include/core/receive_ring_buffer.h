#ifndef CORE_RECEIVE_RING_BUFFER_H
#define CORE_RECEIVE_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct receive_ring_buffer receive_ring_buffer_t;

receive_ring_buffer_t *rrb_create(size_t _rrb_size, uint32_t _current_seq_number);
void rrb_destroy(receive_ring_buffer_t **_rrb_address);

/**
 * @returns Number of bytes, appended to the Receive-Ring-Buffer
 */
uint32_t rrb_append(receive_ring_buffer_t *_rrb, const microtcp_segment_t *_segment);

#endif /* CORE_RECEIVE_RING_BUFFER_H */