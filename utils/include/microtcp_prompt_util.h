#ifndef UTILS_MICROTCP_PROMPT_UTIL_H
#define UTILS_MICROTCP_PROMPT_UTIL_H

#include <stdlib.h>
#include <string.h>
#include "cli_color.h"

void handle_eof(void);
void handle_empty_line(void);
void clean_line(void);
int count_format_specifiers(const char *_format);
extern FILE *error_stream;
extern FILE *prompt_stream;

/**
 * @brief This macro prints a prompt, then repeatedly reads and parses user input
 *        according to the given format string. If the format contains no valid specifiers,
 *        it may cause the macro to become stuck in a loop waiting for valid input
 */
#define PROMPT_WITH_READ_STRING(_prompt, _string_line)                                                                                                                                  \
        do                                                                                                                                                                              \
        {                                                                                                                                                                               \
                size_t line_length = 0;                                                                                                                                                 \
                while (TRUE)                                                                                                                                                            \
                {                                                                                                                                                                       \
                        fprintf(prompt_stream, "%s>> %s%s", COLOR_ASCII_FG_MAGENTA, (_prompt), SGR_RESET); /* Passing _prompt as argument, as _prompt is variable (compiler warning) */ \
                        int getline_ret = getline(&(_string_line), &line_length, stdin);                                                                                                \
                        if (getline_ret == -1 && feof(stdin))                                                                                                                           \
                        {                                                                                                                                                               \
                                handle_eof();                                                                                                                                           \
                                continue;                                                                                                                                               \
                        }                                                                                                                                                               \
                        else if (getline_ret == 1)                                                                                                                                      \
                        {                                                                                                                                                               \
                                handle_empty_line();                                                                                                                                    \
                                continue;                                                                                                                                               \
                        }                                                                                                                                                               \
                        _string_line[strcspn(_string_line, "\n")] = '\0'; /* Remove trailing \n (captured by scanf). */                                                                 \
                        break;                                                                                                                                                          \
                }                                                                                                                                                                       \
        } while (0)
/**
 * @brief This macro prints a prompt, then repeatedly reads and parses user input
 *        according to the given format string. If the format contains no valid specifiers,
 *        it may cause the macro to become stuck in a loop waiting for valid input
 */
#define PROMPT_WITH_READLINE(_prompt, _format, ...)                                                                                                                                     \
        do                                                                                                                                                                              \
        {                                                                                                                                                                               \
                const char extension[] = " %c";                                                                                                                                         \
                size_t extended_format_length = strlen((_format)) + sizeof(extension);                                                                                                  \
                char *extended_format = calloc(extended_format_length, sizeof(char));                                                                                                   \
                                                                                                                                                                                        \
                if (extended_format == NULL)                                                                                                                                            \
                        break;                                                                                                                                                          \
                                                                                                                                                                                        \
                strcat(extended_format, (_format));                                                                                                                                     \
                strcat(extended_format, extension);                                                                                                                                     \
                                                                                                                                                                                        \
                char *line = NULL;                                                                                                                                                      \
                size_t line_length = 0;                                                                                                                                                 \
                while (TRUE)                                                                                                                                                            \
                {                                                                                                                                                                       \
                        fprintf(prompt_stream, "%s>> %s%s", COLOR_ASCII_FG_MAGENTA, (_prompt), SGR_RESET); /* Passing _prompt as argument, as _prompt is variable (compiler warning) */ \
                        int getline_ret = getline(&line, &line_length, stdin);                                                                                                          \
                        if (getline_ret == -1 && feof(stdin))                                                                                                                           \
                        {                                                                                                                                                               \
                                handle_eof();                                                                                                                                           \
                                continue;                                                                                                                                               \
                        }                                                                                                                                                               \
                        else if (getline_ret == -1)                                                                                                                                     \
                                break;                                                                                                                                                  \
                        else if (getline_ret == 1)                                                                                                                                      \
                        {                                                                                                                                                               \
                                handle_empty_line();                                                                                                                                    \
                                continue;                                                                                                                                               \
                        }                                                                                                                                                               \
                        char extra = '\0'; /* We use this extra to detect more wanted registrations. */                                                                                 \
                        int parsed_count = sscanf(line, extended_format, ##__VA_ARGS__, &extra);                                                                                        \
                        /* If there's no extra character, parsing was successful, and the line ends with a newline, we're done. */                                                      \
                        if (extra == '\0' && parsed_count == count_format_specifiers((_format)) && line[getline_ret - 1] == '\n')                                                       \
                                break;                                                                                                                                                  \
                        /* Move the cursor up and clear the line to reprint the prompt. */                                                                                              \
                        if (line[getline_ret - 1] == '\n')                                                                                                                              \
                                printf("\033[1F");                                                                                                                                      \
                        printf("\033[2K"); /* Reset cursor to prompt line. */                                                                                                           \
                }                                                                                                                                                                       \
                free(extended_format);                                                                                                                                                  \
        } while (0)

#endif /* UTILS_MICROTCP_PROMPT_UTIL_H */
