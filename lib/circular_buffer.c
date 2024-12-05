#include <stddef.h>
#include "allocator/allocator.h"
#include "logging/logger.h"

#define GUARD_BYTE_SIZE 1
/* Declarations of static functions. */
static inline size_t cb_remaining_space(circle_buffer_t *_cb);
static struct range;

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

/**
 * @returns bytes it appended to circular buffer.
 */
size_t cb_append(circle_buffer_t *_cb, void *_data, size_t _data_len)
{
        if (_data_len == 0)
                PRINT_WARNING_RETURN(0, "Argument %s value was 0, nothing happened.");

        size_t remaining_space = cb_remaining_space(_cb);
        if (remaining_space < _data_len)
                PRINT_ERROR_RETURN(0, "Given data length greater than available space of circular buffer. ");

        /* If we are here... We know there is the required space on the buffer. */
        /* Any range we equal start and end is invalid.. Indecies point to same byte, so not valid data segment there. */
        struct range r1 = {0};
        struct range r2 = {0};

        r1.start = _cb->tail_index;
        r1.end = _cb->buffer_size - (_cb->head_index == 0 ? GUARD_BYTE_SIZE : 0); /* If first byte is reserved, then last byte is GUARD. */
        r2.start = 0;
        r2.end = (_cb->head_index == 0 ? 0 : _cb->head_index - 1); /* If head_index == 1, sizeof(second segment) = 0, but TAIL should be index 0.*/

        size_t segment_size1 = r1.end - r1.start;
        if (_data_len <= segment_size1)
        {
                memcpy(_cb->buffer + _cb->tail_index, _data, _data_len);
                _cb->tail_index += (_data_len - 1);
        }
        else
        {
                memcpy(_cb->buffer + _cb->tail_index, _data, segment_size1);

                /* We wrap-around. In wrap-around buffer[0] is certainly free, so we start writing at buffer[0]. */
                memcpy(_cb->buffer, _data + segment_size1, _data_len - segment_size1);
                _cb->tail_index = _data_len;
        }
        return _data_len;
}

static inline size_t cb_remaining_space(circle_buffer_t *_cb)
{
        size_t remaining_space = _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;
        if (_cb->head_index <= _cb->tail_index)
                remaining_space += _cb->buffer_size;

        return remaining_space;
}

static struct range
{
        size_t start;
        size_t end;
};

/* --------------------------- For cb_remaining_space() --------------------------- */
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

/* --------------------------- For cb_append() --------------------------- */
/*         if (_cb->head_index == _cb->tail_index)
        {
                /* Buffer is empty reset indecies. * /
                _cb->head_index = _cb->tail_index = 0;

                r1.start = _cb->tail_index;
                r1.end = _cb->buffer_size - GUARD_BYTE_SIZE; /* .end is exclusive; tail can point there, but no data should be there. * /
        }
        else if (_cb->head_index < _cb->tail_index)
        {
                r1.start = _cb->tail_index;
                r1.end = _cb->buffer_size - (_cb->head_index == 0 ? GUARD_BYTE_SIZE : 0); /* If first byte is reserved, then last byte is GUARD. * /

                /* If head_index < tail_index then second's segment .start is always 0.* /
                /* If second's segment .end  is also 0, then segment is of size 0. * /
                r2.start = 0;
                r2.end = (_cb->head_index == 0 ? 0 : _cb->head_index - 1); /* If head_index == 1, sizeof(second segment) = 0, but TAIL should be index 0.* /
        }
        else
        {
                /* In this scenario there is no r2. * /
                r1.start = _cb->tail_index;
                r1.end = _cb->head_index - 1;
        }
 */