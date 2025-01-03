#include <stdint.h>
#include "core/send_queue.h"
#include "allocator/allocator_macros.h"
#include "microtcp_defines.h"
#include "smart_assert.h"

typedef struct send_queue_node send_queue_node_t;
struct send_queue_node
{
        uint32_t seq_number;
        uint32_t segment_size;
        const void *buffer;
        send_queue_node_t *next;
};

struct send_queue
{
        send_queue_node_t *front;
        send_queue_node_t *rear;
        size_t size;
};

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
#ifdef DEBUG_MODE
        SMART_ASSERT(_sq != NULL, _segment_size > 0, _buffer != NULL);
        SMART_ASSERT((_sq->front == NULL && _sq->rear == NULL) || (_sq->front != NULL && _sq->rear != NULL));
#endif /* DEBUG_MODE */
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

status_t sq_dequeue(send_queue_t *const _sq, const uint32_t _ack_number)
{
#ifdef DEBUG_MODE
        SMART_ASSERT(_sq != NULL);
        SMART_ASSERT((_sq->front == NULL && _sq->rear == NULL) || (_sq->front != NULL && _sq->rear != NULL));
#endif /* DEBUG_MODE */

        if (_sq->front == NULL)
                LOG_ERROR_RETURN(FAILURE, "Send-Queue is empty, dequeuing impossible.");

        /* Find requested node */
        send_queue_node_t *curr_node = _sq->front;
        while (curr_node != NULL && curr_node->seq_number + curr_node->segment_size != _ack_number)
                curr_node = curr_node->next;

        /* Not FOUND! Hint that something went wrong with protocol. */
        if (curr_node == NULL)
                LOG_ERROR_RETURN(FAILURE, "out-of-sync: No match for ACK number = %u", _ack_number);

        /* ACK number matched. */
        while (TRUE)
        {
                uint32_t calculated_ack_number = _sq->front->seq_number + _sq->front->segment_size;
                send_queue_node_t *old_front = _sq->front;
                _sq->front = old_front->next;
                FREE_NULLIFY_LOG(old_front);
                _sq->size--;

                if (calculated_ack_number == _ack_number)
                        break;
#ifdef DEBUG_MODE
                SMART_ASSERT(_sq->front != NULL);
#endif /* DEBUG_MODE */
        }
        return SUCCESS;
}

size_t sq_size(const send_queue_t *const _sq)
{
        return _sq->size;
}
