#define DEBUG_MODE // TODO: REMOVE THIS LINE, added for quick and dirty...

#ifdef DEBUG_MODE
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "circle_buffer.h"
#include "cli_color.h"
#include "logging/logger_options.h"

#define STATIC_BUFFER_SIZE 20
/* GLOBAL static buffers: */
uint8_t static_buffer3[STATIC_BUFFER_SIZE];
uint8_t static_buffer5[STATIC_BUFFER_SIZE];
uint8_t static_buffer8[STATIC_BUFFER_SIZE];

#define LINE_WIDTH 80
#define LINE_TOKEN '-'
#define HEADER_COLOR MAGENTA_COLOR
#define HEADER_PREFIX "START: "
#define HEADER_SUFFIX "()"
#define FOOTER_COLOR CYAN_COLOR
#define FOOTER_PREFIX "END: "
#define FOOTER_SUFFIX HEADER_SUFFIX

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define TEST_FAILED false
#define TEST_PASSED true

#define PRINT_UNIT_TEST_HEADER()                                                                                             \
        do                                                                                                                   \
        {                                                                                                                    \
                int line_width = LINE_WIDTH;                                                                                 \
                /* From sizeof() we remove 1, as sizeof() accounts for '\0' too when determining a literal's string size. */ \
                int header_message_size = sizeof(HEADER_PREFIX) - 1 + sizeof(__func__) - 1 + sizeof(HEADER_SUFFIX) - 1;      \
                int dash_count = line_width - header_message_size;                                                           \
                                                                                                                             \
                /* Left line takes 1 more dash, if dash_count odd number. */                                                 \
                int left_dash_line_width = dash_count / 2 + dash_count % 2;                                                  \
                int right_dash_line_width = dash_count / 2;                                                                  \
                printf(HEADER_COLOR);                                                                                        \
                for (int i = 0; i < left_dash_line_width; i++)                                                               \
                        printf("%c", LINE_TOKEN);                                                                            \
                printf("%s%s%s", HEADER_PREFIX, __func__, HEADER_SUFFIX);                                                    \
                for (int i = 0; i < right_dash_line_width; i++)                                                              \
                        printf("%c", LINE_TOKEN);                                                                            \
                printf(RESET_COLOR);                                                                                         \
                printf("\n");                                                                                                \
        } while (0)

#define PRINT_UNIT_TEST_FOOTER()                                                                                             \
        do                                                                                                                   \
        {                                                                                                                    \
                int line_width = LINE_WIDTH;                                                                                 \
                /* From sizeof() we remove 1, as sizeof() accounts for '\0' too when determining a literal's string size. */ \
                int footer_message_size = sizeof(FOOTER_PREFIX) - 1 + sizeof(__func__) - 1 + sizeof(FOOTER_SUFFIX) - 1;      \
                int dash_count = line_width - footer_message_size;                                                           \
                                                                                                                             \
                /* Left line takes 1 more dash, if dash_count odd number. */                                                 \
                int left_dash_line_width = dash_count / 2 + dash_count % 2;                                                  \
                int right_dash_line_width = dash_count / 2;                                                                  \
                printf(FOOTER_COLOR);                                                                                        \
                for (int i = 0; i < left_dash_line_width; i++)                                                               \
                        printf("%c", LINE_TOKEN);                                                                            \
                printf("%s%s%s", FOOTER_PREFIX, __func__, FOOTER_SUFFIX);                                                    \
                for (int i = 0; i < right_dash_line_width; i++)                                                              \
                        printf("%c", LINE_TOKEN);                                                                            \
                printf("\n");                                                                                                \
                printf(RESET_COLOR);                                                                                         \
        } while (0)

#define ASSERT(_test_result, _condition) ({                                                                \
        _Bool assertion_result = TEST_PASSED;                                                              \
        if (_condition)                                                                                    \
        {                                                                                                  \
                printf("Assertion succeeded. %s%s%s:%s%u%s --> %s%s()%s: Condition succeeded [%s%s%s].\n", \
                       GREEN_COLOR, __FILENAME__, RESET_COLOR,                                             \
                       GREEN_COLOR, __LINE__, RESET_COLOR,                                                 \
                       GREEN_COLOR, __func__, RESET_COLOR,                                                 \
                       GREEN_COLOR, #_condition, RESET_COLOR);                                             \
                assertion_result = TEST_PASSED;                                                            \
        }                                                                                                  \
        else                                                                                               \
        {                                                                                                  \
                printf("Assertion failed. %s%s%s:%s%u%s --> %s%s()%s: Condition fails [%s%s%s].\n",        \
                       RED_COLOR, __FILENAME__, RESET_COLOR,                                               \
                       RED_COLOR, __LINE__, RESET_COLOR,                                                   \
                       RED_COLOR, __func__, RESET_COLOR,                                                   \
                       RED_COLOR, #_condition, RESET_COLOR);                                               \
                assertion_result = TEST_FAILED;                                                            \
        }                                                                                                  \
        (_test_result = test_result && assertion_result);                                                  \
})

/* Tests state of cb after creation. */
_Bool unit_test_0(void)
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        _Bool test_result = TEST_PASSED;
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 0);
        ASSERT(test_result, _cb_true_buffer_size(cb) == cb_size + 1);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == cb_size);
        ASSERT(test_result, _cb_free_space(cb) == cb_size);
        ASSERT(test_result, _cb_used_space(cb) == 0);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == _cb_free_space(cb));

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

/* Tests state of cb after single push_back(). */
_Bool unit_test_1()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        CB_PUSH_BACK(cb, static_buffer3, 1);
        _Bool test_result = TEST_PASSED;
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_true_buffer_size(cb) == 8);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 6);
        ASSERT(test_result, _cb_used_space(cb) == 1);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == _cb_free_space(cb) + 1);

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

/* Tests cb after multiple push_back() and pop_front(), some of which should do nothing. */
_Bool unit_test_2()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        CB_PUSH_BACK(cb, static_buffer3, 7);
        _Bool test_result = TEST_PASSED;
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_true_buffer_size(cb) == 8);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);
        ASSERT(test_result, _cb_usable_buffer_size(cb) == _cb_free_space(cb) + _cb_used_space(cb));

        CB_POP_FRONT(cb, 2);
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 2);
        ASSERT(test_result, _cb_used_space(cb) == 5);

        CB_PUSH_BACK(cb, static_buffer8, 3); /* Should fail, and change nothing. */
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 2);
        ASSERT(test_result, _cb_used_space(cb) == 5);

        CB_PUSH_BACK(cb, static_buffer8, 1);
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 0);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_PUSH_BACK(cb, static_buffer8, 1);
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_PUSH_BACK(cb, static_buffer8, 1); /* Should fail and change nothing. */
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_POP_FRONT(cb, 8); /* Should fail and change nothing. */
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_PUSH_BACK(cb, static_buffer8, 1); /* Should fail and change nothing. */
        ASSERT(test_result, _cb_head(cb) == 2);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_POP_FRONT(cb, 1);
        ASSERT(test_result, _cb_head(cb) == 3);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_PUSH_BACK(cb, static_buffer8, 2); /* Should fail and change nothing. */
        ASSERT(test_result, _cb_head(cb) == 3);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_PUSH_BACK(cb, static_buffer8, 0); /* Should change nothing. */
        ASSERT(test_result, _cb_head(cb) == 3);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_POP_FRONT(cb, 0); /* Should change nothing. */
        ASSERT(test_result, _cb_head(cb) == 3);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_POP_FRONT(cb, 4);
        ASSERT(test_result, _cb_head(cb) == 7);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 5);
        ASSERT(test_result, _cb_used_space(cb) == 2);

        CB_POP_FRONT(cb, 1);
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 6);
        ASSERT(test_result, _cb_used_space(cb) == 1);

        CB_POP_FRONT(cb, 1);
        ASSERT(test_result, _cb_head(cb) == 1);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 7);
        ASSERT(test_result, _cb_used_space(cb) == 0);

        CB_PUSH_BACK(cb, static_buffer8, 0); /* Should change nothing. */
        ASSERT(test_result, _cb_head(cb) == 1);
        ASSERT(test_result, _cb_tail(cb) == 1);
        ASSERT(test_result, _cb_free_space(cb) == 7);
        ASSERT(test_result, _cb_used_space(cb) == 0);

        CB_PUSH_BACK(cb, static_buffer8, 7);
        ASSERT(test_result, _cb_head(cb) == 1);
        ASSERT(test_result, _cb_tail(cb) == 0);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

/* Tests the output of pop_front(). */
_Bool unit_test_3()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 10; // Test with a larger buffer size for data integrity.
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        uint8_t input_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        uint8_t output_data[10] = {0};

        CB_PUSH_BACK(cb, input_data, cb_size); // Fill the buffer completely.
        _Bool test_result = TEST_PASSED;

        ASSERT(test_result, _cb_free_space(cb) == 0); // Buffer should be full.
        ASSERT(test_result, _cb_used_space(cb) == cb_size);

        uint8_t *popped_data = (uint8_t *)CB_POP_FRONT(cb, cb_size);
        if (popped_data != NULL)
                memcpy(output_data, popped_data, cb_size); // Copy

        ASSERT(test_result, _cb_free_space(cb) == cb_size); // Buffer should be empty now.
        ASSERT(test_result, _cb_used_space(cb) == 0);

        // Verify the data integrity.
        for (size_t i = 0; i < cb_size; i++)
        {
                ASSERT(test_result, input_data[i] == output_data[i]);
        }

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

/* Testing a scanario I had problem with before (visualized on paper). */
_Bool unit_test_4()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        _Bool test_result = TEST_PASSED;

        CB_PUSH_BACK(cb, static_buffer3, 7);
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_POP_FRONT(cb, 7);
        ASSERT(test_result, _cb_head(cb) == 7);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 7);
        ASSERT(test_result, _cb_used_space(cb) == 0);

        CB_PUSH_BACK(cb, static_buffer8, 1);
        ASSERT(test_result, _cb_head(cb) == 7);
        ASSERT(test_result, _cb_tail(cb) == 0);
        ASSERT(test_result, _cb_free_space(cb) == 6);
        ASSERT(test_result, _cb_used_space(cb) == 1);

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

_Bool unit_test_5()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        _Bool test_result = TEST_PASSED;

        CB_PUSH_BACK(cb, static_buffer3, 7);
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_POP_FRONT(cb, 4);
        ASSERT(test_result, _cb_head(cb) == 4);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 4);
        ASSERT(test_result, _cb_used_space(cb) == 3);

        CB_PUSH_BACK(cb, static_buffer8, 4);
        ASSERT(test_result, _cb_head(cb) == 4);
        ASSERT(test_result, _cb_tail(cb) == 3);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

_Bool unit_test_6()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t cb_size = 7;
        circle_buffer_t *cb = CB_CREATE(cb1, cb_size);

        _Bool test_result = TEST_PASSED;

        CB_PUSH_BACK(cb, static_buffer3, 7);
        ASSERT(test_result, _cb_head(cb) == 0);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        CB_POP_FRONT(cb, 1);
        ASSERT(test_result, _cb_head(cb) == 1);
        ASSERT(test_result, _cb_tail(cb) == 7);
        ASSERT(test_result, _cb_free_space(cb) == 1);
        ASSERT(test_result, _cb_used_space(cb) == 6);

        CB_PUSH_BACK(cb, static_buffer8, 1);
        ASSERT(test_result, _cb_head(cb) == 1);
        ASSERT(test_result, _cb_tail(cb) == 0);
        ASSERT(test_result, _cb_free_space(cb) == 0);
        ASSERT(test_result, _cb_used_space(cb) == 7);

        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

_Bool unit_test_7()
{
        PRINT_UNIT_TEST_HEADER();
        const size_t loop_size = 1000000;
        circle_buffer_t *cb = CB_CREATE(cb1, 1);

        _Bool test_result = TEST_PASSED;
        uint8_t byte = 69;

        for (int i = 0; i < loop_size + 1; i++)
        {

                CB_PUSH_BACK(cb, &byte, 1);
                test_result = test_result &&
                              _cb_head(cb) == 0 &&
                              _cb_tail(cb) == 1 &&
                              _cb_free_space(cb) == 0 &&
                              _cb_used_space(cb) == 1;

                CB_POP_FRONT(cb, 1);
                test_result = test_result &&
                              _cb_head(cb) == 1 &&
                              _cb_tail(cb) == 1 &&
                              _cb_free_space(cb) == 1 &&
                              _cb_used_space(cb) == 0;

                CB_PUSH_BACK(cb, &byte, 1);
                test_result = test_result &&
                              _cb_head(cb) == 1 &&
                              _cb_tail(cb) == 0 &&
                              _cb_free_space(cb) == 0 &&
                              _cb_used_space(cb) == 1;

                CB_POP_FRONT(cb, 1);
                test_result = test_result &&
                              _cb_head(cb) == 0 &&
                              _cb_tail(cb) == 0 &&
                              _cb_free_space(cb) == 1 &&
                              _cb_used_space(cb) == 0;
        }
        cb_destroy(&cb);
        PRINT_UNIT_TEST_FOOTER();
        return test_result;
}

int main()
{
        logger_set_allocator_enabled(false);
        logger_set_info_enabled(false);
        logger_set_enabled(false);

        for (register int i = 0; i < STATIC_BUFFER_SIZE; i++)
        {
                static_buffer3[i] = 3;
                static_buffer5[i] = 5;
                static_buffer8[i] = 8;
        }
#define UNIT_TEST_COUNT 8
        _Bool (*unit_test_array[UNIT_TEST_COUNT])(void);
        _Bool unit_test_result_array[UNIT_TEST_COUNT];
        unit_test_array[0] = &unit_test_0;
        unit_test_array[1] = &unit_test_1;
        unit_test_array[2] = &unit_test_2;
        unit_test_array[3] = &unit_test_3;
        unit_test_array[4] = &unit_test_4;
        unit_test_array[5] = &unit_test_5;
        unit_test_array[6] = &unit_test_6;
        unit_test_array[7] = &unit_test_7;

        for (int i = 0; i < UNIT_TEST_COUNT; i++)
                unit_test_result_array[i] = unit_test_array[i]();

        for (int i = 0; i < UNIT_TEST_COUNT; i++)
        {
                if (unit_test_result_array[i] == TEST_PASSED)
                        printf("unit_test_%d(): %sTEST-PASSED%s.\n", i, GREEN_COLOR, RESET_COLOR);
                else
                        printf("unit_test_%d(): %sTEST-FAILED%s.\n", i, RED_COLOR, RESET_COLOR);
        }
}

#else

int main() { ; }

#endif /* DEBUG_MODE */
