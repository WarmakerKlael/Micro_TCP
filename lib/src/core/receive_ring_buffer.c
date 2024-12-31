#include <stdint.h>
#include <allocator/allocator_macros.h>
#include <sys/socket.h>
#include "microtcp.h"
#include "smart_assert.h"
#include "core/segment_processing.h"
#include "microtcp_helper_macros.h"
#include "core/receive_ring_buffer.h"
#include "microtcp_defines.h"
#include "core/segment_processing.h"

typedef struct rrb_block rrb_block_t;
struct rrb_block
{
        uint32_t seq_number;
        uint32_t size;
        rrb_block_t *next;
};

/* `receive_ring_bufffer_t` is defined in equivilant header_file. */
struct receive_ring_buffer
{
        void *buffer;
        uint32_t buffer_size;
        uint32_t last_consumed_seq_number;
        uint32_t consumable_bytes;
        rrb_block_t *rrb_block_list_head;
};

/* Inner helper functions. */
static void rrb_block_list_destroy(rrb_block_t **_head_address);
static inline _Bool is_in_bounds(uint32_t _rrb_last_consumed_seq_number, uint32_t _rrb_size, uint32_t _segment_seq_number);
static inline uint32_t free_space(uint32_t _rrb_last_consumed_seq_number, uint32_t _rrb_size, uint32_t _segment_seq_number);
static void rrb_block_list_insert(rrb_block_t **_head_address, uint32_t _seq_number, uint32_t _size);
static uint32_t join_rrb_blocks(receive_ring_buffer_t *_rrb);
static inline rrb_block_t *create_rrb_block(uint32_t _seq_number, uint32_t _size, rrb_block_t *_next);
static inline void merge_with_left(rrb_block_t *_prev_left_node, rrb_block_t *_left_node, uint32_t _extend_size, rrb_block_t **_head_address);
static inline void merge_with_right(rrb_block_t *_prev_right_node, rrb_block_t *_right_node, uint32_t _new_seq_number, uint32_t _extend_size);
static void debug_print_rrb_block_list(const rrb_block_t *head);
static void debug_print_rrb(const receive_ring_buffer_t *_rrb);

receive_ring_buffer_t *rrb_create(const size_t _rrb_size, const uint32_t _current_seq_number)
{
        SMART_ASSERT(_rrb_size > 0, _rrb_size <= UINT32_MAX);
        receive_ring_buffer_t *rrb = MALLOC_LOG(rrb, sizeof(receive_ring_buffer_t));
        if (rrb == NULL)
                return NULL;

        rrb->buffer = MALLOC_LOG(rrb->buffer, _rrb_size);
        if (rrb->buffer == NULL)
        {
                FREE_NULLIFY_LOG(rrb);
                return NULL;
        }
        rrb->rrb_block_list_head = NULL;
        rrb->buffer_size = _rrb_size;
        rrb->consumable_bytes = 0;
        rrb->last_consumed_seq_number = _current_seq_number;
        return rrb;
}

/* We request a double pointer, in order to NULLIFY user's circle buffer pointer. */
void rrb_destroy(receive_ring_buffer_t **const _rrb_address)
{
        SMART_ASSERT(_rrb_address != NULL); /* receive_window's pointer can be NULL, but not its address. */

#define RRB (*_rrb_address)
        if (RRB == NULL)
                return;

        /* Proceed with destruction. */
        FREE_NULLIFY_LOG(RRB->buffer);
        rrb_block_list_destroy(&(RRB->rrb_block_list_head));
        FREE_NULLIFY_LOG(RRB);
#undef RRB
}

/**
 * @returns Number of bytes, appended to the Receive-Ring-Buffer
 * @note Due to the checksum function we need to first process the segment in continuous buffer.
 * Thus any attempt to directly write new segment into RRB is failed (by Initial DESIGN).
 */
uint32_t rrb_append(receive_ring_buffer_t *const _rrb, const microtcp_segment_t *const _segment)
{
        if (!is_in_bounds(_rrb->last_consumed_seq_number, _rrb->buffer_size, _segment->header.seq_number))
                LOG_ERROR_RETURN(0, "RRB out-of-bounds segment: {`last_consumed_seq_number` = %u, `buffer_size` = %u, `seq_number` = %u}.",
                                 _rrb->last_consumed_seq_number, _rrb->buffer_size, _segment->header.seq_number);

        const uint32_t available_space = free_space(_rrb->last_consumed_seq_number, _rrb->buffer_size, _segment->header.seq_number);
        const uint32_t bytes_to_copy = MIN(available_space, _segment->header.data_len);
        if (bytes_to_copy == 0)
                return 0;

        if (_rrb->last_consumed_seq_number + _rrb->consumable_bytes + 1 == _segment->header.seq_number)
                _rrb->consumable_bytes += bytes_to_copy;
        else
                rrb_block_list_insert(&(_rrb->rrb_block_list_head), _segment->header.seq_number, bytes_to_copy);

        /* Check if you can grow consumable bytes (using block_list). */
        _rrb->consumable_bytes += join_rrb_blocks(_rrb);

        /* Write segment's data onto RRB: */
        /* Right Side of RRB: */
        const uint32_t begin_pos = _segment->header.seq_number % _rrb->buffer_size; /* TODO: optimize, by getting buffer_size by global variable. */
        const uint32_t bytes_fitting_right_side = _rrb->buffer_size - begin_pos;
        if (bytes_to_copy <= bytes_fitting_right_side)
        {
                memcpy(_rrb->buffer + begin_pos, _segment->raw_payload_bytes, bytes_to_copy);
        }
        else
        {
                memcpy(_rrb->buffer + begin_pos, _segment->raw_payload_bytes, bytes_fitting_right_side);
                memcpy(_rrb->buffer, _segment->raw_payload_bytes + bytes_fitting_right_side, bytes_to_copy - bytes_fitting_right_side);
        }
        return bytes_to_copy;
}

static void rrb_block_list_destroy(rrb_block_t **const _head_address)
{
        SMART_ASSERT(_head_address != NULL); /* head pointer can be NULL, but not its address. */

#define HEAD (*_head_address)
        while (HEAD != NULL)
        {
                rrb_block_t *next = HEAD->next;
                FREE_NULLIFY_LOG(HEAD);
                HEAD = next;
        }
#undef HEAD
}

static inline _Bool is_in_bounds(uint32_t _rrb_last_consumed_seq_number, uint32_t _rrb_size, uint32_t _segment_seq_number) // CHECK!!
{
        SMART_ASSERT(_rrb_size > 0, _rrb_size < INT32_MAX);

        _Bool wrap_around_occurs = _rrb_last_consumed_seq_number > _rrb_last_consumed_seq_number + _rrb_size;
        /* Check if in first range: */
        const uint32_t min1_ex = _rrb_last_consumed_seq_number;
        const uint32_t max1_in = wrap_around_occurs ? UINT32_MAX : _rrb_last_consumed_seq_number + _rrb_size;

        if (!wrap_around_occurs && _segment_seq_number > min1_ex && _segment_seq_number <= max1_in)
                return TRUE;

        /*Check wrap_arround.*/
        const uint32_t min2_in = 0;
        const uint32_t max2_in = _rrb_last_consumed_seq_number + _rrb_size;
        if (wrap_around_occurs && ((_segment_seq_number > min1_ex && _segment_seq_number <= max1_in) || (_segment_seq_number >= min2_in && _segment_seq_number <= max2_in)))
                return TRUE;

        return FALSE;
}

static inline uint32_t free_space(const uint32_t _rrb_last_consumed_seq_number, const uint32_t _rrb_size, const uint32_t _segment_seq_number)
{
        SMART_ASSERT(is_in_bounds(_rrb_last_consumed_seq_number, _rrb_size, _segment_seq_number));

        _Bool wrap_around_occurs = _rrb_last_consumed_seq_number > _rrb_last_consumed_seq_number + _rrb_size;

        if (wrap_around_occurs && _segment_seq_number > _rrb_last_consumed_seq_number)
                return (UINT32_MAX - _segment_seq_number + 1) + (_rrb_last_consumed_seq_number + _rrb_size + 1);
        return (_rrb_last_consumed_seq_number + _rrb_size) - _segment_seq_number + 1; /* +1 because _segment_seq_number is pointing to consumable data. */
}

/* Does not account for overlap, as it require EXTRA logic (a lot !). */
static void rrb_block_list_insert(rrb_block_t **const _head_address, const uint32_t _seq_number, const uint32_t _size)
{
        SMART_ASSERT(_size > 0, _size < INT32_MAX); /* WRAP-AROUND threshold. Shouldn't worry.. size shouldn't exceeds a few kbytes */
#define HEAD (*_head_address)
        /* Case 0: Empty list, insert as head */
        if (HEAD == NULL)
        {
                HEAD = create_rrb_block(_seq_number, _size, NULL);
                return;
        }

        rrb_block_t *curr_node = HEAD;
        rrb_block_t *prev_node = NULL;
        _Bool flag = FALSE;
        rrb_block_t *flag_prev_node = NULL;
        rrb_block_t *flag_curr_node = NULL;
        while (curr_node != NULL)
        {
                /* Case 1: Continuation of current node. */
                if (curr_node->seq_number + curr_node->size == _seq_number)
                {
                        merge_with_left(prev_node, curr_node, _size, _head_address);
                        return;
                }
                /* Case 2: Merge with the current block (less than) */
                if (_seq_number + _size == curr_node->seq_number)
                {
                        merge_with_right(prev_node, curr_node, _seq_number, _size);
                        return;
                }
                if (flag == FALSE && _seq_number < curr_node->seq_number)
                {

                        flag = TRUE;
                        flag_prev_node = prev_node;
                        flag_curr_node = curr_node;
                }
                prev_node = curr_node;
                curr_node = curr_node->next;
        }
        /* Case 3: Insert before the current block (less than, no merge) */
        if (flag == TRUE && _seq_number < flag_curr_node->seq_number)
        {
                rrb_block_t *new_block = create_rrb_block(_seq_number, _size, flag_curr_node);
                if (flag_prev_node == NULL) /* Means curr_node == HEAD. */
                        HEAD = new_block;
                else
                        flag_prev_node->next = new_block;
                return;
        }
        prev_node->next = create_rrb_block(_seq_number, _size, NULL);
#undef HEAD
}

static uint32_t join_rrb_blocks(receive_ring_buffer_t *const _rrb)
{
        uint32_t growth_counter = 0;
        rrb_block_t *prev_node = NULL;
        rrb_block_t *curr_node = _rrb->rrb_block_list_head;
        _Bool found_match = FALSE;
        do
        {
                found_match = FALSE;
                for (; curr_node != NULL; curr_node = curr_node->next)
                {
                        if (_rrb->last_consumed_seq_number + _rrb->consumable_bytes + 1 == curr_node->seq_number)
                                break;
                        prev_node = curr_node;
                }
                if (curr_node == NULL) /* Didn't find match. */
                        break;

                if (prev_node == NULL) /* curr_node == HEAD. */
                        _rrb->rrb_block_list_head = curr_node->next;
                else
                        prev_node->next = curr_node->next;
                growth_counter += curr_node->size;
                FREE_NULLIFY_LOG(curr_node);
                found_match = TRUE;
        } while (found_match == TRUE);
        return growth_counter;
}

static inline rrb_block_t *create_rrb_block(uint32_t _seq_number, uint32_t _size, rrb_block_t *_next)
{
        rrb_block_t *new_block = MALLOC_LOG(new_block, sizeof(rrb_block_t));
        new_block->seq_number = _seq_number;
        new_block->size = _size;
        new_block->next = _next;
        return new_block;
}

static inline void merge_with_left(rrb_block_t *_prev_left_node, rrb_block_t *_left_node, uint32_t _extend_size, rrb_block_t **_head_address)
{
#define HEAD (*_head_address)
        _left_node->size += _extend_size;
        /* If reached another block to right, merge too. */
        if (_left_node->next != NULL && _left_node->next->seq_number == _left_node->seq_number + _left_node->size)
        {
                _left_node->size += _left_node->next->size;
                rrb_block_t *next_node_copy = _left_node->next;
                _left_node->next = _left_node->next->next;
                FREE_NULLIFY_LOG(next_node_copy);
        }
        else if (_left_node->seq_number + _left_node->size == HEAD->seq_number) /* Can we merge with HEAD? */
        {
                HEAD->seq_number = _left_node->seq_number;
                HEAD->size += _left_node->size;
                FREE_NULLIFY_LOG(_left_node);
                if (_prev_left_node != NULL)
                        _prev_left_node->next = NULL;
        }
#undef HEAD
}

static inline void merge_with_right(rrb_block_t *_prev_right_node, rrb_block_t *_right_node, uint32_t _new_seq_number, uint32_t _extend_size)
{
        _right_node->seq_number = _new_seq_number;
        _right_node->size += _extend_size;

        /* Merge `_prev_right_node` and `_right_node` if they are contiguous blocks. */
        if (_prev_right_node != NULL && _prev_right_node->seq_number + _prev_right_node->size == _right_node->seq_number)
        {
                _prev_right_node->size += _right_node->size;
                _prev_right_node->next = _right_node->next;
                FREE_NULLIFY_LOG(_right_node);
        }
}

static void debug_print_rrb_block_list(const rrb_block_t *head)
{
        static int call_counter = 0;
        call_counter++;
        printf("==============RRB_BLOCK_LIST==============%d\n", call_counter);
        while (head != NULL)
        {
                printf("SEQ_NUM = %u | SIZE = %u\n", head->seq_number, head->size);
                head = head->next;
        }
        printf("==========================================%d\n", call_counter);
}

static void debug_print_rrb(const receive_ring_buffer_t *_rrb)
{
        static int call_counter = 0;
        call_counter++;
        printf("********************************RRB********************************%d\n", call_counter);
        printf("rrb.buffer = %p\n", _rrb->buffer);
        printf("rrb.buffer_size = %u\n", _rrb->buffer_size);
        printf("rrb.last_consumed_seq_number = %u\n", _rrb->last_consumed_seq_number);
        printf("rrb.consumable_bytes = %u\n", _rrb->consumable_bytes);
        debug_print_rrb_block_list(_rrb->rrb_block_list_head);
        for (uint32_t i = 0; i < _rrb->buffer_size; i++)
        {
                // char ch = *((char *)_rrb->buffer + (_rrb->last_consumed_seq_number + i) % _rrb->buffer_size);
                char ch = *((char *)_rrb->buffer + i);
                printf("%c", ch == 0 ? '0' : ch);
        }
        printf("\n");
        printf("*******************************************************************%d\n", call_counter);
}

int main()
{
#define CURR_SEQ_NUM UINT32_MAX-1

        receive_ring_buffer_t *rrb = rrb_create(100, CURR_SEQ_NUM);
        char raw1[] = {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'};
        char raw2[] = {'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b'};
        char raw3[] = {'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c'};
        microtcp_segment_t t1 = {.header.seq_number = UINT32_MAX, .header.data_len = 1, .raw_payload_bytes = raw1};
        microtcp_segment_t t2 = {.header.seq_number = UINT32_MAX+1, .header.data_len = 1, .raw_payload_bytes = raw2};
        // microtcp_segment_t t3 = {.header.seq_number = 21, .header.data_len = 1, .raw_payload_bytes = raw3};

        rrb_append(rrb, &t1);
        rrb_append(rrb, &t2);
        // rrb_append(rrb, &t3);
        debug_print_rrb(rrb);

        rrb_destroy(&rrb);
}
