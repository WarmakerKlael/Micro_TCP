
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "settings/microtcp_settings.h"
#include "microtcp_common_macros.h"
#include "microtcp.h"
#include "microtcp_settings_common.h"
#include "cli_color.h"
#include "smart_assert.h"

static FILE *prompt_stream = NULL;
static FILE *error_stream = NULL;

__attribute__((constructor)) void initilize_output_streams(void)
{
        prompt_stream = stdout;
        error_stream = stderr;
}

static inline void handle_eof(void)
{
        clearerr(stdin);
        fprintf(prompt_stream, "\033[7m\033[1mEOF\033[0m");
        fflush(prompt_stream);
        printf("\033[1G\033[2K");
}

static inline void handle_empty_line(void)
{
        /* We still clear line "\033[2K]" why?
         * Well although spaces are invisible..
         * Maybe some uncommon terminal mark
         * white spaces. */
        printf("\033[1G\033[2K");
        printf("\033[1F");
}

static inline void clean_line(void)
{
        printf("\033[1F");
        printf("\033[1G\033[2K");
}

static inline size_t count_format_specifiers(const char *_format)
{
        SMART_ASSERT(_format != NULL);
        size_t count = 0;
        while (*_format != '\0')
        {
                if (*(_format) == '%' && *(_format + 1) != '%' && *(_format + 1) != '\0' && *(_format + 1) != ' ')
                        count++;
                if (*(_format) == '%' && *(_format + 1) == '%')
                        _format++; /* There was a %%, we go past it. */
                _format++;
        }
        return count;
}

/**
 * @brief This macro prints a prompt, then repeatedly reads and parses user input
 *        according to the given format string. If the format contains no valid specifiers,
 *        it may cause the macro to become stuck in a loop waiting for valid input
 */
#define PROMPT_WITH_READLINE(_prompt, _format, ...)                                                                                                                            \
        do                                                                                                                                                                     \
        {                                                                                                                                                                      \
                const char extension[] = " %c";                                                                                                                                \
                size_t extended_format_length = strlen(_format) + sizeof(extension);                                                                                           \
                char *extended_format = calloc(extended_format_length, sizeof(char));                                                                                          \
                                                                                                                                                                               \
                if (extended_format == NULL)                                                                                                                                   \
                        break;                                                                                                                                                 \
                                                                                                                                                                               \
                strcat(extended_format, _format);                                                                                                                              \
                strcat(extended_format, extension);                                                                                                                            \
                                                                                                                                                                               \
                char *line = NULL;                                                                                                                                             \
                size_t line_length = 0;                                                                                                                                        \
                for (;;)                                                                                                                                                       \
                {                                                                                                                                                              \
                        fprintf(prompt_stream, "%s>> %s%s", MAGENTA_COLOR, _prompt, RESET_COLOR); /* Passing _prompt as argument, as _prompt is variable (compiler warning) */ \
                        int getline_ret = getline(&line, &line_length, stdin);                                                                                                 \
                        if (getline_ret == -1 && feof(stdin))                                                                                                                  \
                        {                                                                                                                                                      \
                                handle_eof();                                                                                                                                  \
                                continue;                                                                                                                                      \
                        }                                                                                                                                                      \
                        else if (getline_ret == -1)                                                                                                                            \
                                break;                                                                                                                                         \
                        else if (getline_ret == 1)                                                                                                                             \
                        {                                                                                                                                                      \
                                handle_empty_line();                                                                                                                           \
                                continue;                                                                                                                                      \
                        }                                                                                                                                                      \
                        char extra = '\0'; /* We use this extra to detect more wanted registrations. */                                                                        \
                        int parsed_count = sscanf(line, extended_format, ##__VA_ARGS__, &extra);                                                                               \
                        /* If there's no extra character, parsing was successful, and the line ends with a newline, we're done. */                                             \
                        if (extra == '\0' && parsed_count == count_format_specifiers(_format) && line[getline_ret - 1] == '\n')                                                \
                                break;                                                                                                                                         \
                        /* Move the cursor up and clear the line to reprint the prompt. */                                                                                     \
                        if (line[getline_ret - 1] == '\n')                                                                                                                     \
                                printf("\033[1F");                                                                                                                             \
                        printf("\033[2K"); /* Reset cursor to prompt line. */                                                                                                  \
                }                                                                                                                                                              \
                free(extended_format);                                                                                                                                         \
        } while (0)

void prompt_set_assembly_buffer_length(void)
{
        const char *prompt = "Enter assembly buffer length <bytes> (Default " STRINGIFY_EXPANDED(MICROTCP_RECVBUF_LEN) "): ";
        long assembly_buffer_length = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &assembly_buffer_length);
                if (assembly_buffer_length <= 0)
                        clean_line();
        } while (assembly_buffer_length <= 0);
        set_bytestream_assembly_buffer_len(assembly_buffer_length);
}

void prompt_set_ack_timeout(void)
{
        const char *prompt = "Enter ACK timeout interval <sec μsec> (Default " STRINGIFY_EXPANDED(DEFAULT_MICROTCP_ACK_TIMEOUT_SEC) " " STRINGIFY_EXPANDED(DEFAULT_MICROTCP_ACK_TIMEOUT_USEC) "): ";
        struct timeval ack_timeout_interval = {0};
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld%ld", &ack_timeout_interval.tv_sec, &ack_timeout_interval.tv_usec);
                if (ack_timeout_interval.tv_sec < 0 || ack_timeout_interval.tv_usec < 0)
                        clean_line();
        } while (ack_timeout_interval.tv_sec < 0 || ack_timeout_interval.tv_usec < 0);
        set_microtcp_ack_timeout(ack_timeout_interval);
}

void prompt_set_connect_retries(void)
{
        const char *prompt = "Enter connect's connection retries, when RST received (Default " STRINGIFY_EXPANDED(DEFAULT_CONNECT_RST_RETRIES) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clean_line();
        } while (retries < 0);
        set_connect_rst_retries(retries);
}

void prompt_set_accept_retries()
{
        const char *prompt = "Enter accept's SYN|ACK retries, when waiting for final ACK (Default " STRINGIFY_EXPANDED(LINUX_DEFAULT_ACCEPT_TIMEOUTS) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clean_line();
        } while (retries < 0);
        set_accept_synack_retries(retries);
}

void prompt_set_shutdown_retries()
{
        const char *prompt = "Enter shutdown's FIN|ACK retries, when FIN|ACK isnt ACKed  (Default " STRINGIFY_EXPANDED(TCP_RETRIES2) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clean_line();
        } while (retries < 0);
        set_shutdown_finack_retries(retries);
}

void prompt_set_shutdown_time_wait_period()
{
        const char *prompt = "Enter shutdown's TIME-WAIT period <sec μsec> (Default " STRINGIFY_EXPANDED(DEFAULT_SHUTDOWN_TIME_WAIT_SEC) " " STRINGIFY_EXPANDED(DEFAULT_SHUTDOWN_TIME_WAIT_USEC) "): ";
        struct timeval time_wait_period = {0};
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld%ld", &time_wait_period.tv_sec, &time_wait_period.tv_usec);
                if (time_wait_period.tv_sec < 0 || time_wait_period.tv_usec < 0)
                        clean_line();
        } while (time_wait_period.tv_sec < 0 || time_wait_period.tv_usec < 0);
        set_shutdown_time_wait_period(time_wait_period);
}