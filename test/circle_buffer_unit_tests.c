#define DEBUG_MODE // TODO: REMOVE THIS LINE, added for quick and dirty...

#ifdef DEBUG_MODE
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include "circle_buffer.h"
#include "cli_color.h"

#define STATIC_BUFFER_SIZE 20
/* GLOBAL static buffers: */
uint8_t static_buffer3[STATIC_BUFFER_SIZE];
uint8_t static_buffer5[STATIC_BUFFER_SIZE];
uint8_t static_buffer8[STATIC_BUFFER_SIZE];

#define LINE_WIDTH 50
#define LINE_TOKEN '-'
#define HEADER_COLOR MAGENTA_COLOR
#define HEADER_PREFIX "START: "
#define HEADER_SUFFIX "()"
#define FOOTER_COLOR CYAN_COLOR
#define FOOTER_PREFIX "END: "
#define FOOTER_SUFFIX HEADER_SUFFIX

#define PRINT_UNIT_TEST_HEADER()                                                                       \
        do                                                                                             \
        {                                                                                              \
                int line_width = LINE_WIDTH;                                                           \
                int header_message = sizeof(HEADER_PREFIX) + sizeof(__func__) + sizeof(HEADER_SUFFIX); \
                int dash_count = line_width - header_message;                                          \
                                                                                                       \
                /* Left line takes 1 more dash, if dash_count odd number. */                           \
                int left_dash_line_width = dash_count / 2 + dash_count % 2;                            \
                int right_dash_line_width = dash_count / 2;                                            \
                printf(HEADER_COLOR);                                                                  \
                for (int i = 0; i < left_dash_line_width; i++)                                         \
                        printf("%c", LINE_TOKEN);                                                      \
                printf("%s%s%s", HEADER_PREFIX, __func__, HEADER_SUFFIX);                              \
                for (int i = 0; i < right_dash_line_width; i++)                                        \
                        printf("%c", LINE_TOKEN);                                                      \
                printf(RESET_COLOR);                                                                   \
        } while (0)

#define PRINT_UNIT_TEST_FOOTER()                                                                       \
        do                                                                                             \
        {                                                                                              \
                int line_width = LINE_WIDTH;                                                           \
                int footer_message = sizeof(FOOTER_PREFIX) + sizeof(__func__) + sizeof(FOOTER_SUFFIX); \
                int dash_count = line_width - footer_message;                                          \
                                                                                                       \
                /* Left line takes 1 more dash, if dash_count odd number. */                           \
                int left_dash_line_width = dash_count / 2 + dash_count % 2;                            \
                int right_dash_line_width = dash_count / 2;                                            \
                printf(FOOTER_COLOR);                                                                  \
                for (int i = 0; i < left_dash_line_width; i++)                                         \
                        printf("%c", LINE_TOKEN);                                                      \
                printf("%s%s%s", FOOTER_PREFIX, __func__, FOOTER_SUFFIX);                              \
                for (int i = 0; i < right_dash_line_width; i++)                                        \
                        printf("%c", LINE_TOKEN);                                                      \
                printf(RESET_COLOR);                                                                   \
        } while (0)

int main()
{
        for (register int i = 0; i < STATIC_BUFFER_SIZE; i++)
        {
                static_buffer3[i] = 3;
                static_buffer5[i] = 5;
                static_buffer8[i] = 8;
        }

        printf("------------------------------------------THE BEGINS HERE------------------------------------------\n");

        printf("sizeof var == %d\n", sizeof(__CONCAT(__func__, "  ")));
        circle_buffer_t *cb1 = CB_CREATE(cb1, 7);

        CB_PUSH_BACK(cb1, static_buffer5, 7);
        CB_POP_FRONT(cb1, 1);
        CB_PUSH_BACK(cb1, static_buffer5, 1);
        CB_POP_FRONT(cb1, 3);
        CB_PUSH_BACK(cb1, static_buffer5, 3);

        // CB_PUSH_BACK(cb1, static_buffer8, 1);

        printf("-------------------------------------------THE ENDS HERE-------------------------------------------\n");
        cb_destroy(&cb1);
}

_Bool unit_test_1(void)
{
}

#else

int main() { ; }

#endif /* DEBUG_MODE */
