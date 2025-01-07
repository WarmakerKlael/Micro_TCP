#include <stdint.h>
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "allocator/allocator_macros.h"
#include "microtcp_defines.h"
#include "microtcp.h"
#include "smart_assert.h"
#include <pthread.h>
#include <errno.h>

struct send_queue
{
        send_queue_node_t *front;
        send_queue_node_t *rear;
        size_t stored_segments;
        size_t stored_bytes;
        pthread_mutex_t access_mutex; /* TODO? Will we go multithreading?  MULTITHREAD BABY MULTITHREAD.. (GEORGE 2025) */
};

send_queue_t *sq_create(void)
{
        send_queue_t *sq = MALLOC_LOG(sq, sizeof(send_queue_t));
        sq->front = NULL;
        sq->rear = NULL;
        sq->stored_segments = 0;
        sq->stored_bytes = 0;
        pthread_mutex_init(&sq->access_mutex, NULL);
        return sq;
}

status_t sq_destroy(send_queue_t **const _sq_address)
{
        SMART_ASSERT(_sq_address != NULL);

#define SQ (*_sq_address)
        if (SQ == NULL)
                return SUCCESS;
        if (pthread_mutex_destroy(&SQ->access_mutex) == EBUSY)
                return FAILURE;

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
        return FAILURE;
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
        pthread_mutex_lock(&_sq->access_mutex);
        if (_sq->front == NULL)

                _sq->front = _sq->rear = new_node;
        else
                _sq->rear = _sq->rear->next = new_node;
        _sq->stored_segments++;
        _sq->stored_bytes += _segment_size;
        pthread_mutex_unlock(&_sq->access_mutex);
}

/**
 * @returns number of dequeued nodes.
 */
size_t sq_dequeue(send_queue_t *const _sq, const uint32_t _ack_number)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        DEBUG_SMART_ASSERT((_sq->front == NULL && _sq->rear == NULL) || (_sq->front != NULL && _sq->rear != NULL));
        size_t dequeued_node_counter = 0;
        pthread_mutex_lock(&_sq->access_mutex);

        if (_sq->front == NULL)
        {
                pthread_mutex_unlock(&_sq->access_mutex);
                LOG_ERROR_RETURN(dequeued_node_counter, "Send-Queue is empty, dequeuing impossible.");
        }

        /* Find requested node */
        send_queue_node_t *curr_node = _sq->front;
        while (curr_node != NULL && curr_node->seq_number + curr_node->segment_size != _ack_number)
                curr_node = curr_node->next;

        /* Not FOUND! Hint that something went wrong with protocol. */
        if (curr_node == NULL)
        {
                pthread_mutex_unlock(&_sq->access_mutex);
                LOG_ERROR_RETURN(dequeued_node_counter, "out-of-sync: No match for ACK number = %u", _ack_number);
        }

        /* ACK number matched. */
        while (TRUE)
        {
                uint32_t calculated_ack_number = _sq->front->seq_number + _sq->front->segment_size;
                send_queue_node_t *old_front = _sq->front;
                _sq->stored_segments--;
                _sq->stored_bytes -= _sq->front->segment_size;
                _sq->front = _sq->front->next;
                FREE_NULLIFY_LOG(old_front);
                dequeued_node_counter++;

                if (calculated_ack_number == _ack_number)
                        break;
                DEBUG_SMART_ASSERT(_sq->front != NULL); /* If in this while loop, means we found the node; If assert fails something went very wrong... */
        }
        pthread_mutex_unlock(&_sq->access_mutex);
        return dequeued_node_counter;
}

size_t sq_stored_segments(send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        pthread_mutex_lock(&_sq->access_mutex);
        const size_t stored_segments = _sq->stored_segments;
        pthread_mutex_unlock(&_sq->access_mutex);
        return stored_segments;
}

size_t sq_stored_bytes(send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        pthread_mutex_lock(&_sq->access_mutex);
        const size_t store_bytes = _sq->stored_bytes;
        pthread_mutex_unlock(&_sq->access_mutex);
        return store_bytes;
}

_Bool sq_is_empty(send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        pthread_mutex_lock(&_sq->access_mutex);
        const _Bool is_empty = _sq->front == NULL;
        pthread_mutex_unlock(&_sq->access_mutex);
        return is_empty;
}

send_queue_node_t *sq_front(send_queue_t *const _sq)
{
        DEBUG_SMART_ASSERT(_sq != NULL);
        pthread_mutex_lock(&_sq->access_mutex);
        send_queue_node_t *const front = _sq->front;
        pthread_mutex_unlock(&_sq->access_mutex);
        return front;
}