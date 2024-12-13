#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "smart_assert.h"

#define READ_PERCENT_c(_va_arg_list)                      \
        do                                                \
        {                                                 \
                char *arg = va_arg(_va_arg_list, char *); \
                scanf(" %c", arg);                        \
        } while (0)

#define READ_PERCENT_d(_va_arg_list)                    \
        do                                              \
        {                                               \
                int *arg = va_arg(_va_arg_list, int *); \
                scanf("%d", arg);                       \
        } while (0)

/* type(unsigned int) == type(unsigned) */
#define READ_PERCENT_u(_va_arg_list)                              \
        do                                                        \
        {                                                         \
                unsigned *arg = va_arg(_va_arg_list, unsigned *); \
                scanf("%u", arg);                                 \
        } while (0)

/* type(long int) == type(long) */
#define READ_PERCENT_ld(_va_arg_list)                     \
        do                                                \
        {                                                 \
                long *arg = va_arg(_va_arg_list, long *); \
                scanf("%ld", arg);                        \
        } while (0)

#define READ_PERCENT_lu(_va_arg_list)                                       \
        do                                                                  \
        {                                                                   \
                unsigned long *arg = va_arg(_va_arg_list, unsigned long *); \
                scanf("%lu", arg);                                          \
        } while (0)

#define READ_PERCENT_lld(_va_arg_list)                              \
        do                                                          \
        {                                                           \
                long long *arg = va_arg(_va_arg_list, long long *); \
                scanf("%lld", arg);                                 \
        } while (0)

#define READ_PERCENT_llu(_va_arg_list)                                                \
        do                                                                            \
        {                                                                             \
                unsigned long long *arg = va_arg(_va_arg_list, unsgined long long *); \
                scanf("%llu", arg);                                                   \
        } while (0)

#define READ_PERCENT_f(_va_arg_list)                        \
        do                                                  \
        {                                                   \
                float *arg = va_arg(_va_arg_list, float *); \
                scanf("%f", arg);                           \
        } while (0)

#define READ_PERCENT_lf(_va_arg_list)                         \
        do                                                    \
        {                                                     \
                double *arg = va_arg(_va_arg_list, double *); \
                scanf("%lf", arg);                            \
        } while (0)

#define READ_PERCENT_Lf(_va_arg_list)                                   \
        do                                                              \
        {                                                               \
                long double *arg = va_arg(_va_arg_list, long double *); \
                scanf("%Lf", arg);                                      \
        } while (0)

#define HANDLE_PERCENT_l \
        do               \
        {                \
        SMART_ASSERT()\
        } while (0)
#define HANDLE_PERCENT_ll \
        do                \
        {                 \
        } while (0)
#define HANDLE_PERCENT_L \
        do               \
        {                \
        } while (0)

#define HANDLE_PERCENT_SPECIFIER(_formats, _va_arg_list) \
        do                                               \
        {                                                \
                SMART_ASSERT(*_formats == '%');          \
                _formats++;                              \
                switch (*_formats)                       \
                {                                        \
                case 'u':                                \
                        break;                           \
                case 'd':                                \
                        break;                           \
                case 'f':                                \
                        break;                           \
                case 'c':                                \
                        break;                           \
                case 'L':                                \
                        break;                           \
                case 'l':                                \
                        break;                           \
                default:                                 \
                        break;                           \
                }                                        \
        } while (0)

static void arithemetic_value_request(const char *_prompt, const char *_formats, ...)
{
        va_list arguments;
        va_start(arguments, _formats);

        char *line = NULL;
        size_t line_length = 0;

        while (*_formats != '\0')
        {
                if (*_formats == '%')
                        HANDLE_PERCENT_SPECIFIER(_formats, arguments);
        }
}

static void prompt_and_set_ack_timeout()
{
        char *line = NULL;
        size_t line_length = 0;
        struct timeval tv;
        int counter = 0;
        printf("start line\n");
        while (++counter)
        {
                printf("-Enter ACK timeout (sec usec, e.g., 0 200000): ");
                int getline_ret = getline(&line, &line_length, stdin);

                if (getline_ret == -1 && feof(stdin))
                {
                        clearerr(stdin);
                        printf("\033[7m\033[1mEOF\033[0m");
                        fflush(stdout);
                        usleep(300000);
                        printf("\033[1G\033[2K");
                        continue;
                }
                else if (getline_ret == -1)
                        return;
                else if (getline_ret == 1)
                {
                        printf("\033[1G\033[2K");
                }

                char unwanted_extra;
                int parsed_arguments = sscanf(line, " %lu %ld %c", &tv.tv_sec, &tv.tv_usec, &unwanted_extra);
                if (parsed_arguments == 2 && tv.tv_sec >= 0 && tv.tv_usec >= 0)
                        break;
                printf("\033[1F");
                printf("\033[2K");
        }

        printf("end line\n");
        free(line);
}

// printf("RECEIVED: sec == %ld, usec == %ld, ", tv.tv_sec, tv.tv_usec);
// set_microtcp_ack_timeout(tv);

int main()
{
        int x;
        arithemetic_value_request("OKAY", "%d", &x);
        prompt_and_set_ack_timeout();
        return 0;
}
