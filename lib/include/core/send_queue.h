#ifndef CORE_SEND_QUEUE_H
#define CORE_SEND_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "microtcp_defines.h"

typedef struct send_queue send_queue_t;

send_queue_t *sq_create(void);
void sq_destroy(send_queue_t **_sq);
void sq_enqueue(send_queue_t *_sq, uint32_t _seq_number, uint32_t _segment_size, const void *_buffer);
status_t sq_dequeue(send_queue_t *_sq, uint32_t _ack_number);
size_t sq_size(const send_queue_t *_sq);

#endif /* CORE_SEND_QUEUE_H */