

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
        while (curr_node != NULL && _seq_number <= curr_node->seq_number) /* Check if you can merge. */
        {
                /* Case 1: Continuation of current node. */
                if (curr_node->seq_number + curr_node->size == _seq_number)
                {
                        merge_with_left(curr_node, _size);
                        return;
                }
                /* Case 2: Merge with the current block (less than) */
                if (_seq_number < curr_node->seq_number && _seq_number + _size == curr_node->seq_number)
                {
                        merge_with_right(prev_node, curr_node, _seq_number, _size);
                        return;
                }
                prev_node = curr_node;
                curr_node = curr_node->next;
        }
        rrb_block_t *new_block = create_rrb_block(_seq_number, _size, prev_node);
        if (prev_node == NULL) /* Means curr_node == HEAD. */
                HEAD = new_block;
        else
                prev_node->next = new_block;
#undef HEAD
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
        while (curr_node != NULL)
        {
                /* Case 1: Continuation of current node. */
                if (curr_node->seq_number + curr_node->size == _seq_number)
                {
                        merge_with_left(curr_node, _size);
                        return;
                }
                /* Case 2: Merge with the current block (less than) */
                if (_seq_number < curr_node->seq_number && _seq_number + _size == curr_node->seq_number)
                {
                        merge_with_right(prev_node, curr_node, _seq_number, _size);
                        return;
                }

                /* Case 3: Insert before the current block (less than, no merge) */
                if (_seq_number < curr_node->seq_number)
                {
                        rrb_block_t *new_block = create_rrb_block(_seq_number, _size, curr_node);
                        if (prev_node == NULL) /* Means curr_node == HEAD. */
                                HEAD = new_block;
                        else
                                prev_node->next = new_block;
                        return;
                }
                prev_node = curr_node;
                curr_node = curr_node->next;
        }
        prev_node->next = create_rrb_block(_seq_number, _size, NULL);
#undef HEAD
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
        while (curr_node != NULL)
        {
                /* Case 1: Continuation of current node. */
                if (curr_node->seq_number + curr_node->size == _seq_number)
                {
                        merge_with_left(curr_node, _size);
                        return;
                }
                /* Case 2: Merge with the current block (less than) */
                if (_seq_number < curr_node->seq_number && _seq_number + _size == curr_node->seq_number)
                {
                        merge_with_right(prev_node, curr_node, _seq_number, _size);
                        return;
                }
                prev_node = curr_node;
                curr_node = curr_node->next;
        }
        curr_node = HEAD;
        prev_node = NULL;
        while (curr_node != NULL)
        {
                /* Case 3: Insert before the current block (less than, no merge) */
                if (_seq_number < curr_node->seq_number)
                {
                        rrb_block_t *new_block = create_rrb_block(_seq_number, _size, curr_node);
                        if (prev_node == NULL) /* Means curr_node == HEAD. */
                                HEAD = new_block;
                        else
                                prev_node->next = new_block;
                        return;
                }
                prev_node = curr_node;
                curr_node = curr_node->next;
        }
        prev_node->next = create_rrb_block(_seq_number, _size, NULL);
#undef HEAD
}