#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <stddef.h>

typedef struct circle_buffer
{
        void *buffer;
        size_t buffer_size; /* Includes Guard-BYTE */
        size_t head_index;  /* From HEAD you read. */
        size_t tail_index;  /* From TAIL you write.*/
} circle_buffer_t;

circle_buffer_t *cb_create(size_t _buffer_size);

/* We request a double pointer, in order to NULLIFY user's circle buffer pointer. */
void cb_destroy(circle_buffer_t **_cb_address);

/**
 * @returns bytes it appended to circular buffer.
 */
size_t cb_push_back(circle_buffer_t *_cb, void *_data, size_t _data_len);

void *cb_pop_front(circle_buffer_t *_cb, size_t _requested_segment_size);

/* Used for debugging: */
#ifdef DEBUG_MODE
void _cb_print_buffer(circle_buffer_t *_cb);
size_t _cb_free_space(circle_buffer_t *_cb);
size_t _cb_used_space(circle_buffer_t *_cb);
size_t _cb_tail(circle_buffer_t *_cb);
size_t _cb_head(circle_buffer_t *_cb);
size_t _cb_true_buffer_size(circle_buffer_t *_cb);
size_t _cb_usable_buffer_size(circle_buffer_t *_cb);
#ifdef VERBOSE
#define CB_CREATE(_cb_var, _size) ({                      \
        printf("Create buffer with of size %d\n", _size); \
        circle_buffer_t *_cb_var = cb_create(_size);      \
        _cb_print_buffer(_cb_var);                        \
        printf("\n\n");                                   \
        (_cb_var);                                        \
})

#define CB_PUSH_BACK(_cb, _buffer, _no_elements)                                     \
        do                                                                           \
        {                                                                            \
                printf("Push_back() %d elements from %s\n", _no_elements, #_buffer); \
                cb_push_back(_cb, _buffer, _no_elements);                            \
                _cb_print_buffer(_cb);                                               \
                printf("\n\n");                                                      \
        } while (0)

#define CB_POP_FRONT(_cb, _no_elements)                           \
        do                                                        \
        {                                                         \
                printf("Pop_front()  %d elements", _no_elements); \
                cb_pop_front(_cb, _no_elements);                  \
                _cb_print_buffer(_cb);                            \
                printf("\n\n");                                   \
        } while (0)
#else

#define CB_CREATE(_cb_var, _size) ({                 \
        circle_buffer_t *_cb_var = cb_create(_size); \
        (_cb_var);                                   \
})

#define CB_PUSH_BACK(_cb, _buffer, _no_elements)          \
        do                                                \
        {                                                 \
                cb_push_back(_cb, _buffer, _no_elements); \
        } while (0)

#define CB_POP_FRONT(_cb, _no_elements) ({ \
        cb_pop_front(_cb, _no_elements);   \
})

#endif /* VERBOSE */

#endif /* DEMUG_MODE */

#endif /* CIRCLE_BUFFER_H */