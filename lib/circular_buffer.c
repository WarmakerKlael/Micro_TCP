#include <stddef.h>
#include "allocator/allocator.h"
#include "logging/logger.h"

#define GUARD_BYTE_SIZE 1
/* Declarations of static functions. */
static inline size_t cb_remaining_space(circle_buffer_t *_cb);

typedef struct
{
        void *buffer;
        size_t buffer_size; /* Includes Guard-BYTE */
        size_t head_index;  /* From HEAD you read. */
        size_t tail_index;  /* From TAIL you write.*/
} circle_buffer_t;

circle_buffer_t *cb_create(size_t _buffer_size)
{
        _buffer_size += GUARD_BYTE_SIZE; /* Adjust _buffer_size to include a GUARD BYTE. */
        circle_buffer_t *new_cb = MALLOC_LOG(new_cb, sizeof(circle_buffer_t));
        if (new_cb == NULL)
                return NULL;

        new_cb->buffer = MALLOC_LOG(new_cb->buffer, _buffer_size);
        if (new_cb->buffer == NULL)
        {
                FREE_NULLIFY_LOG(new_cb);
                return NULL;
        }
        new_cb->buffer_size = _buffer_size;
        new_cb->head_index = 0;
        new_cb->tail_index = 0;
        return new_cb;
}

/**
 * @returns bytes it appended to circular buffer.
 */
size_t cb_append(circle_buffer_t *_cb, void *_data, size_t _data_len)
{
        if (_data_len > cb_remaining_space(_cb))
        
}

/* We request a double pointer, in order to NULLIFY user's circle buffer pointer. */
void cb_destroy(circle_buffer_t **_cb_address)
{
        if (*_cb_address == NULL)
                return;
        FREE_NULLIFY_LOG((*_cb_address)->buffer);
        (*_cb_address)->buffer_size = 0;
        (*_cb_address)->head_index = 0;
        (*_cb_address)->tail_index = 0;
        FREE_NULLIFY_LOG(*_cb_address);
}

static inline size_t cb_remaining_space(circle_buffer_t *_cb)
{
        size_t remaining_space = _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;
        if (_cb->head_index <= _cb->tail_index)
                remaining_space += _cb->buffer_size;

        return remaining_space;
}

/*
        if (_cb->head_index <= _cb->tail_index)
                remaining_space = _cb->buffer_size - (_cb->tail_index - _cb->head_index) - GUARD_BYTE_SIZE;
        else
                remaining_space = _cb->buffer_size - (_cb->buffer_size - _cb->head_index) - _cb->tail_index - GUARD_BYTE_SIZE;

        EQUALS:

        if (_cb->head_index <= _cb->tail_index)
                remaining_space = _cb->buffer_size - (_cb->tail_index - _cb->head_index) - GUARD_BYTE_SIZE;
        else
                remaining_space = _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;

        EQUALS:

        if (_cb->head_index <= _cb->tail_index)
                remaining_space = _cb->buffer_size - _cb->tail_index + _cb->head_index - GUARD_BYTE_SIZE;
        else
                remaining_space =                  - _cb->tail_index + _cb->head_index - GUARD_BYTE_SIZE;

        EQUALS:

        size_t remaining_space = _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;

        if (_cb->head_index <= _cb->tail_index)
                remaining_space += _cb->buffer_size;

        EQUALS:

        return _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE + (_cb->head_index <= _cb->tail_index ? _cb->buffer_size : 0);


*/
