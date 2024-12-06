#define DEBUG_MODE // TODO: REMOVE THIS LINE, added for quick and dirty...

#ifdef DEBUG_MODE
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "circle_buffer.h"
#include "cli_color.h"

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
                printf("\n\n");\
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
                printf("\n\n");\
                printf("\n\n");\
                printf(RESET_COLOR);                                                                                         \
        } while (0)

_Bool unit_test_1(void)
{
        PRINT_UNIT_TEST_HEADER();
        
                circle_buffer_t *cb2 = CB_CREATE(cb1, 7);

                CB_PUSH_BACK(cb2, static_buffer5, 7);
                CB_POP_FRONT(cb2, 1);
                CB_PUSH_BACK(cb2, static_buffer5, 1);
                CB_POP_FRONT(cb2, 3);
                CB_PUSH_BACK(cb2, static_buffer5, 3);

                // CB_PUSH_BACK(cb2, static_buffer8, 1);

                cb_destroy(&cb2);
        PRINT_UNIT_TEST_FOOTER();
}

int main()
{
        for (register int i = 0; i < STATIC_BUFFER_SIZE; i++)
        {
                static_buffer3[i] = 3;
                static_buffer5[i] = 5;
                static_buffer8[i] = 8;
        }

        unit_test_1();
}

#else

int main() { ; }

#endif /* DEBUG_MODE */
