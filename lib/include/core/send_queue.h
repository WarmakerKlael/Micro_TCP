#ifndef CORE_SEND_QUEUE_H
#define CORE_SEND_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "microtcp_defines.h"

typedef struct send_queue_node send_queue_node_t;
struct send_queue_node
{
        uint32_t seq_number;
        uint32_t segment_size;
        const void *buffer;
        send_queue_node_t *next;
};

typedef struct send_queue send_queue_t;

send_queue_t *sq_create(void);
void sq_destroy(send_queue_t **_sq);
void sq_enqueue(send_queue_t *_sq, uint32_t _seq_number, uint32_t _segment_size, const void *_buffer);
size_t sq_dequeue(send_queue_t *_sq, uint32_t _ack_number);
size_t sq_size(const send_queue_t *_sq);
_Bool sq_is_empty(const send_queue_t *_sq);
send_queue_node_t *sq_front(send_queue_t *_sq);

#endif /* CORE_SEND_QUEUE_H */