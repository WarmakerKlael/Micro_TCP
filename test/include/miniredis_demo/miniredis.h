#ifndef MINIREDIS_H
#define MINIREDIS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "microtcp_helper_macros.h"

/* TODO: SET to UINT16_MAX not 2 */
#define MAX_FILE_CHUNK (2 * MICROTCP_MSS)

#define MAX_COMMAND_SIZE 20
#define MAX_COMMAND_ARGUMENT_SIZE 400
#define MAX_REQUEST_SIZE (MAX_COMMAND_SIZE + (2 * MAX_COMMAND_ARGUMENT_SIZE))

_Static_assert(MAX_FILE_CHUNK > MAX_REQUEST_SIZE, "Helps avoid dynamic memory allocation for  filename buffers. JUST DO IT.");

// clang-format off
static const char sscanf_command_format[] = "%" STRINGIFY_EXPANDED(MAX_COMMAND_SIZE) "s "
                                            "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s "
                                            "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s";
// clang-format on
static const char available_commands_text[] =
    "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n"
    "┃      Available commands                  ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->QUIT                                    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->HELP                                    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->GET <filename>            <saving-path> ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->SET <filename> <path-to-file>           ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->DEL <filename>                          ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->CACHE <filename>                        ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->LIST                                    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->INFO <filename>                         ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->SIZE <filename>                         ┃\n"
    "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n";

static inline void display_help(void)
{
    printf("%s", available_commands_text);
}

typedef struct __attribute__((packed))
{
    int8_t command_code;
    status_t operation_status;
    size_t message_size;
    size_t file_name_size;
    size_t file_size;
} miniredis_header_t;

#endif /* MINI_REDIS_H */
