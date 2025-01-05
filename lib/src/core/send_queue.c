#include <stdint.h>
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "allocator/allocator_macros.h"
#include "microtcp_defines.h"
#include "microtcp.h"
#include "smart_assert.h"

struct send_queue
{
        send_queue_node_t *front;
        send_queue_node_t *rear;
        size_t size;
};

static inline _Bool _sq_is_empty(const send_queue_t *_sq);

send_queue_t *sq_create(void)
{
        send_queue_t *sq = MALLOC_LOG(sq, sizeof(send_queue_t));
        sq->front = NULL;
        sq->rear = NULL;
        return sq;
}

void sq_destroy(send_queue_t **const _sq)
{
#define SQ (*_sq)
        SMART_ASSERT(_sq != NULL);
        send_queue_node_t *curr_node = SQ->front;
        if (SQ->front != NULL)
                LOG_ERROR("For correct protocol function Send-Queue should be empty when passed to `%s`", __func__);
        while (curr_node != NULL)
        {
                send_queue_node_t *next = curr_node->next;
                FREE_NULLIFY_LOG(curr_node);
                curr_node = next;
        }
        FREE_NULLIFY_LOG(SQ);
#undef SQ
}

void sq_enqueue(send_queue_t *const _sq, const uint32_t _seq_number, const uint32_t _segment_size, const void *_buffer)
{
        DEBUG_SMART_ASSERT(_sq != NULL, _segment_size > 0, _buffer != NULL);
        DEBUG_SMART_ASSERT((_sq->front == NULL && _sq->rear == NULL) || (_sq->front != NULL && _sq->rear != NULL));
        send_queue_node_t *new_node = MALLOC_LOG(new_node, sizeof(send_queue_node_t));
        new_node->seq_number = _seq_number;
        new_node->segment_size = _segment_size;
        new_node->buffer = _buffer;
        new_node->next = NULL;

        if (_sq->front == NULL)

                _sq->front = _sq->rear = new_node;
        else
                _sq->rear = _sq->rear->next = new_node;
        _sq->size++;
}

/**
 * @returns number of dequeued nodes.
 */
size_t sq_dequeue(send_queue_t *const _sq, const uint32_t _ack_number)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        DEBUG_SMART_ASSERT((_sq->front == NULL && _sq->rear == NULL) || (_sq->front != NULL && _sq->rear != NULL));
        size_t dequeued_node_counter = 0;

        if (_sq->front == NULL)
                LOG_ERROR_RETURN(dequeued_node_counter, "Send-Queue is empty, dequeuing impossible.");

        /* Find requested node */
        send_queue_node_t *curr_node = _sq->front;
        while (curr_node != NULL && curr_node->seq_number + curr_node->segment_size != _ack_number)
                curr_node = curr_node->next;

        /* Not FOUND! Hint that something went wrong with protocol. */
        if (curr_node == NULL)
                LOG_ERROR_RETURN(dequeued_node_counter, "out-of-sync: No match for ACK number = %u", _ack_number);

        /* ACK number matched. */
        while (TRUE)
        {
                uint32_t calculated_ack_number = _sq->front->seq_number + _sq->front->segment_size;
                send_queue_node_t *old_front = _sq->front;
                _sq->front = _sq->front->next;
                FREE_NULLIFY_LOG(old_front);
                _sq->size--;
                dequeued_node_counter++;

                if (calculated_ack_number == _ack_number)
                        break;
                DEBUG_SMART_ASSERT(_sq->front != NULL); /* If in this while loop, means we found the node; If assert fails something went very wrong... */
        }
        return dequeued_node_counter;
}

size_t sq_size(const send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq);
        return _sq->size;
}

_Bool sq_is_empty(const send_queue_t *const _sq)
{
        return _sq_is_empty(_sq);
}

send_queue_node_t *sq_front(send_queue_t *const _sq)
{
        return _sq->front;
}

static inline _Bool _sq_is_empty(const send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq);
        return _sq->front == NULL;
}