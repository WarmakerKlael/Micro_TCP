#include <stdint.h>
#include <allocator/allocator_macros.h>
#include "smart_assert.h"
#define GUARD_BYTE 1

typedef struct index_node index_node_t;
struct index_node
{
        uint32_t start;
        uint32_t end;
        index_node_t *next;
};

typedef struct receive_window
{
        void *buffer;
        index_node_t *index_list_head;
        uint32_t buffer_size; /* Includes Guard-BYTE */
        uint32_t head_index;  /* From HEAD you read. */
        uint32_t head_seq_number;
        uint32_t index_list_size;

} receive_window_t;

static void destroy_index_list(index_node_t **_head_address);

receive_window_t *rw_create(size_t _rw_size, uint32_t _current_seq_number)
{
        SMART_ASSERT(_rw_size > 0, _rw_size <= UINT32_MAX - GUARD_BYTE);
        _rw_size += GUARD_BYTE; /* Adjust _rw_size to include a GUARD BYTE. */
        receive_window_t *rw = MALLOC_LOG(rw, sizeof(receive_window_t));
        if (rw == NULL)
                return NULL;

        rw->buffer = MALLOC_LOG(rw->buffer, _rw_size);
        if (rw->buffer == NULL)
        {
                FREE_NULLIFY_LOG(rw);
                return NULL;
        }
        rw->index_list_head = NULL;
        rw->buffer_size = _rw_size;
        rw->head_index = 0;
        rw->head_seq_number = _current_seq_number;
        rw->index_list_size = 0;
        return rw;
}

/* We request a double pointer, in order to NULLIFY user's circle buffer pointer. */
void rw_destroy(receive_window_t **_rw_address)
{
        SMART_ASSERT(_rw_address != NULL); /* receive_window's pointer can be NULL, but not its address. */

#define RW (*_rw_address)
        if (RW == NULL)
                return;

        /* Proceed with destruction. */
        FREE_NULLIFY_LOG(RW->buffer);
        destroy_index_list(&(RW->index_list_head));
        FREE_NULLIFY_LOG(RW);
#undef RW
}

int main()
{
        index_node_t *n1 = MALLOC_LOG(n1, sizeof(index_node_t));
        index_node_t *n2 = MALLOC_LOG(n2, sizeof(index_node_t));
        index_node_t *n3 = MALLOC_LOG(n3, sizeof(index_node_t));
        index_node_t *n4 = MALLOC_LOG(n4, sizeof(index_node_t));
        index_node_t *n5 = MALLOC_LOG(n5, sizeof(index_node_t));

        n1->next = n2;
        n2->next = n3;
        n3->next = n4;
        n4->next = n5;

        index_node_t *n6 = NULL;

        destroy_index_list(&n6);
        destroy_index_list(&n1);
}

static void destroy_index_list(index_node_t **_head_address)
{
        SMART_ASSERT(_head_address != NULL); /* head pointer can be NULL, but not its address. */

#define HEAD (*_head_address)
        while (HEAD != NULL)
        {
                index_node_t *next = HEAD->next;
                FREE_NULLIFY_LOG(HEAD);
                HEAD = next;
        }
#undef HEAD
}