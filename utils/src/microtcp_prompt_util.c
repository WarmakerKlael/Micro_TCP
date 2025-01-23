#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "cli_color.h"
#include "microtcp.h"
#include "microtcp_helper_functions.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"
#include "microtcp_prompt_util.h"

FILE *prompt_stream = NULL;
FILE *error_stream = NULL;

__attribute__((constructor)) void initilize_output_streams(void)
{
        prompt_stream = stdout;
        error_stream = stderr;
}

void handle_eof(void)
{
        clearerr(stdin);
        fprintf(prompt_stream, "\033[7m\033[1mEOF\033[0m");
        fflush(prompt_stream);
        printf("\033[1G\033[2K");
}

void handle_empty_line(void)
{
        /* We still clear line "\033[2K]" why?
         * Well although spaces are invisible..
         * Maybe some uncommon terminal mark
         * white spaces. */
        printf("\033[1G\033[2K");
        printf("\033[1F");
}

void clean_line(void)
{
        printf("\033[1F");
        printf("\033[1G\033[2K");
}

int count_format_specifiers(const char *_format)
{
        SMART_ASSERT(_format != NULL);
        int count = 0;
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
