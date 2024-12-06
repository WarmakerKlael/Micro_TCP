#include <stddef.h>
#include <stdio.h>
// TODO: REMOVE NEXT include <stdint.h>
#include <stdint.h>

#include "allocator/allocator.h"
#include "logging/logger.h"
#include "microtcp_helper_macros.h"
#include "circle_buffer.h"

#define GUARD_BYTE_SIZE 1

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
size_t cb_push_back(circle_buffer_t *_cb, void *_data, size_t _data_len)
{
        if (_data_len == 0)
                PRINT_WARNING_RETURN(0, "Argument %s value was 0, nothing happened.", STRINGIFY(_data_len));

        size_t remaining_space = cb_empty_space(_cb);
        if (remaining_space < _data_len)
                PRINT_ERROR_RETURN(0, "Given data length greater than available space of circular buffer. ");

        /* If we are here... We know there is the required space on the buffer. */
        /* Any range with equal start and end is invalid.. Indecies point to same byte, so not valid data segment there. */
        size_t start = _cb->tail_index;
        size_t end;
        if (_cb->tail_index < _cb->head_index)
                end = _cb->head_index - 1;
        else
                end = _cb->buffer_size - (_cb->head_index == 0); /* If first byte is reserved, then last byte is GUARD. */
        size_t segment_size1 = end - start;

        if (_data_len <= segment_size1)
        {
                memcpy(_cb->buffer + _cb->tail_index, _data, _data_len);
                _cb->tail_index = (_cb->tail_index + _data_len) % _cb->buffer_size;
        }
        else
        {
                memcpy(_cb->buffer + _cb->tail_index, _data, segment_size1);

                /* We wrap-around. In wrap-around buffer[0] is certainly free, so we start writing at buffer[0]. */
                memcpy(_cb->buffer, _data + segment_size1, _data_len - segment_size1);
                _cb->tail_index = (_data_len - segment_size1) % _cb->buffer_size;
        }
        return _data_len;
}

void *cb_pop_front_copy(circle_buffer_t *_cb, size_t _requested_segment_size)
{
        if (_requested_segment_size == 0)
                PRINT_WARNING_RETURN(NULL, "Argument %s value was 0, nothing happened.", STRINGIFY(_requested_segment_size));

        size_t used_space = cb_used_space(_cb);
        if (used_space < _requested_segment_size)
                PRINT_ERROR_RETURN(NULL, "Buffer contains %d bytes, but %d where asked", used_space, _requested_segment_size);

        void *_requested_segment = CALLOC_LOG(_requested_segment, _requested_segment_size);

        if (_cb->head_index + _requested_segment_size <= _cb->buffer_size)
        {
                memcpy(_requested_segment, _cb->buffer + _cb->head_index, _requested_segment_size);
                _cb->head_index = (_cb->head_index + _requested_segment_size) % _cb->buffer_size;
        }
        else
        {
                /* Copy first segment. */
                size_t first_segment_size = _cb->buffer_size - _cb->head_index;
                memcpy(_requested_segment, _cb->buffer + _cb->head_index, first_segment_size);
                _cb->head_index = 0;
                /* Copy the remaining of the requested segment. */
                memcpy(_requested_segment + first_segment_size, _cb->buffer + _cb->head_index, _requested_segment_size - first_segment_size);
                _cb->head_index = _requested_segment_size - first_segment_size;
        }

        if (_cb->head_index == _cb->tail_index) /* Buffer is empty, we reset indicies. */
                _cb->head_index = _cb->tail_index = 0;

        return _requested_segment;
}

/**
 * @returns number of "poped" bytes. a.k.a how far the head moved.
 */
size_t cb_pop_front(circle_buffer_t *_cb, size_t _bytes_to_pop)
{
        if (cb_used_space(_cb) < _bytes_to_pop)
                return 0;

        _cb->head_index = (_cb->head_index + _bytes_to_pop) % _cb->buffer_size;

        return _bytes_to_pop;
}

size_t cb_empty_space(circle_buffer_t *_cb)
{
        size_t remaining_space = _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;
        if (_cb->head_index <= _cb->tail_index)
                remaining_space += _cb->buffer_size;

        return remaining_space;
}

size_t cb_used_space(circle_buffer_t *_cb)
{
        return _cb->buffer_size - GUARD_BYTE_SIZE - cb_empty_space(_cb);
}

size_t cb_first_empty_segment_size(circle_buffer_t *_cb)
{
        if (_cb->head_index <= _cb->tail_index)
                return _cb->buffer_size - _cb->tail_index - (_cb->head_index == 0);
        return _cb->head_index - _cb->tail_index - GUARD_BYTE_SIZE;
}

size_t cb_second_empty_segment_size(circle_buffer_t *_cb)
{
        return cb_empty_space(_cb) - cb_first_empty_segment_size(_cb);
}

/**
 * @returns new tail index , or -1 if failure occurs.
 */
ssize_t cb_move_tail(circle_buffer_t *_cb, size_t _n_bytes)
{
        if (cb_empty_space(_cb) < _n_bytes)
                return -1;
        _cb->tail_index = (_cb->tail_index + _n_bytes) % _cb->buffer_size;
}

void *cb_first_empty_segment_address(circle_buffer_t *_cb)
{
        return _cb->buffer + _cb->tail_index;
}

void *cb_second_empty_segment_address(circle_buffer_t *_cb)
{
        if (cb_second_empty_segment_size(_cb) == 0)
                return NULL;
        return _cb->buffer;
}

size_t cb_read_n_bytes(circle_buffer_t *_cb, size_t _bytes_to_read, void **_seg1, void **_seg2, size_t *_seg1_size, size_t *_seg2_size)
{
        *_seg1 = *_seg2 = NULL;
        *_seg1_size = *_seg2_size = 0;
        if (cb_used_space(_cb) < _bytes_to_read || _bytes_to_read == 0)
                return 0;
        /* If here, there are at least '_bytes_to_read' in _cb. */

        if (_cb->head_index <= _cb->tail_index)
        {
                printf("HE\n");
                /* When HEAD < TAIL: DATA_WHOLE. */
                *_seg1 = _cb->buffer + _cb->head_index;
                *_seg1_size = _bytes_to_read;
                return _bytes_to_read;
        }
        printf("HE\n");
        /* When HEAD < TAIL: might be SEGMENTED. */
        *_seg1 = _cb->buffer + _cb->head_index;
        if (_cb->head_index + _bytes_to_read <= _cb->buffer_size)
                *_seg1_size = _bytes_to_read;
        else
                *_seg1_size = _cb->buffer_size - _cb->head_index;

        if (*_seg1_size != _bytes_to_read)
        {
                *_seg2 = _cb->buffer + 0;
                *_seg2_size = _bytes_to_read - *_seg1_size;
        }
        return _bytes_to_read;
}

#ifdef DEBUG_MODE
void _cb_print_buffer(circle_buffer_t *_cb)
{
        printf("Head = %d\t", _cb->head_index);
        printf("Tail = %d\n", _cb->tail_index);

        for (size_t i = 0; i < _cb->buffer_size; ++i)
                printf("I=%d ", i);
        printf("\n");

        for (size_t i = 0; i < _cb->buffer_size; ++i)
        {
                uint8_t byte = ((uint8_t *)_cb->buffer)[i];
                printf("[%c] ", byte);
        }
        printf("\n");
}

size_t _cb_tail(circle_buffer_t *_cb)
{
        return _cb->tail_index;
}

size_t _cb_head(circle_buffer_t *_cb)
{
        return _cb->head_index;
}

size_t _cb_true_buffer_size(circle_buffer_t *_cb)
{
        return _cb->buffer_size;
}

size_t _cb_usable_buffer_size(circle_buffer_t *_cb)
{
        return _cb->buffer_size - GUARD_BYTE_SIZE;
}
#endif /* DEBUG_MODE */

#include <sys/socket.h>

/* Receive FROM with circular buffer. */
/* Asked to add at max '_max_read_size' bytes in '_cb_recvbuf'.  */
/* This function 'cb_recvfrom'; will append at most empty_space  */
/* bytes on the '_cb_recvbuf buffer, even if more were asked, as */
/* there is no available space on buffer.                        */
ssize_t cb_recvfrom(int _fd, circle_buffer_t *restrict _cb_recvbuf, size_t _max_read_size, int _flags, struct sockaddr *restrict _addr, socklen_t *restrict _addr_len)
{
        size_t available_read_size = MIN(cb_empty_space(_cb_recvbuf), _max_read_size);

        /* We first fill the first empty segment on the circular buffer.
         * If wrap-around is needed, because first empty segment_size is
         * insufficient, but circular buffer has more space. We then
         * continue with filling the second segment of the circular buffer.
         */
        void *first_empty_segment_address = cb_first_empty_segment_address(_cb_recvbuf);
        size_t first_empty_segment_size = cb_first_empty_segment_size(_cb_recvbuf);
        size_t first_segment_read_limit = MIN(first_empty_segment_size, available_read_size);

        ssize_t recvfrom_ret_val = recvfrom(_fd, first_empty_segment_address, first_segment_read_limit, _flags, _addr, _addr_len);
        if (recvfrom_ret_val <= 0)
                return recvfrom_ret_val;

        cb_move_tail(_cb_recvbuf, recvfrom_ret_val);
        if (recvfrom_ret_val < first_segment_read_limit)
                return recvfrom_ret_val;

        size_t total_bytes_read = recvfrom_ret_val;

        /* If first segment wasn'te enough: */
        size_t bytes_to_read_in_second_segment = available_read_size - first_segment_read_limit;
        if (bytes_to_read_in_second_segment > 0)
        {
                recvfrom_ret_val = recvfrom(_fd, cb_second_empty_segment_address(_cb_recvbuf), bytes_to_read_in_second_segment, _flags, _addr, _addr_len);
                if (recvfrom_ret_val > 0)
                {
                        total_bytes_read += recvfrom_ret_val;
                        cb_move_tail(_cb_recvbuf, recvfrom_ret_val);
                }
        }

        return total_bytes_read;
}