#ifndef MINIREDIS_H
#define MINIREDIS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include "microtcp_helper_macros.h"
#include "miniredis_demo/miniredis_commands.h"

#define MAX_COMMAND_SIZE 20
#define MAX_COMMAND_ARGUMENT_SIZE 400
#define MAX_REQUEST_SIZE (MAX_COMMAND_SIZE + (2 * MAX_COMMAND_ARGUMENT_SIZE))

#define REGISTRY_INITIAL_ENTRIES_CAPACITY (100)
#define REGISTRY_CACHE_SIZE_LIMIT (500000)
#define MIN_RESPONSE_IDLE_TIME ((struct timeval){.tv_sec = 10, .tv_usec = 0}) /* Minimum idle time before giving up is set to 10 seconds. */
#define CLIENT_MAX_IDLE_TIME_MULTIPLIER 1
#define SERVER_MAX_IDLE_TIME_MULTIPLIER 1

#define STAGING_FILE_NAME ".__filepart__.dat" /* Hiddden, internal filename until stored in `_registry`. */
#define DIRECTORY_NAME_FOR_REGISTRY "REGISTRY_SERVER_DIR"
#define DIRECTORY_NAME_FOR_DOWNLOADS "DOWNLOADS"

// clang-format off
static const char sscanf_command_format[] = "%" STRINGIFY_EXPANDED(MAX_COMMAND_SIZE) "s "
                                            "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s "
                                            "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s";
// clang-format on
static const char available_commands_text[] =
    "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n"
    "┃      Available commands            ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->QUIT                              ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->HELP                              ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->GET <filename>                    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->SET <filename>  <path-to-file>    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->DEL <filename>                    ┃\n"
    "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
    "┃->LIST                              ┃\n"
    "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n";

static inline void display_help(void)
{
    printf("%s", available_commands_text);
}

typedef struct __attribute__((packed))
{
    enum miniredis_command_codes command_code;
    status_t response_status;
    size_t response_message_size;
    size_t file_name_size;
    size_t file_size;
} miniredis_header_t;

#define STARTUP_CLIENT_LOGO COLOR_FG_SKY "\
Oo      oO ooOoOOo o.     O ooOoOOo `OooOOo.  o.OOoOoo o.OOOo.   ooOoOOo .oOOOo.             .oOOOo.   o      ooOoOOo o.OOoOoo o.     O oOoOOoOOo \n\
O O    o o    O    Oo     o    O     o     `o  O        O    `o     O    o     o            .O     o  O          O     O       Oo     o     o     \n\
o  o  O  O    o    O O    O    o     O      O  o        o      O    o    O.                 o         o          o     o       O O    O     o     \n\
O   Oo   O    O    O  o   o    O     o     .O  ooOO     O      o    O     `OOoo.            o         o          O     ooOO    O  o   o     O     \n\
O        o    o    O   o  O    o     OOooOO'   O        o      O    o          `O           o         O          o     O       O   o  O     o     \n\
o        O    O    o    O O    O     o    o    o        O      o    O           o           O         O          O     o       o    O O     O     \n\
o        O    O    o     Oo    O     O     O   O        o    .O'    O    O.    .O           `o     .o o     .    O     O       o     Oo     O     \n\
O        o ooOOoOo O     `o ooOOoOo  O      o ooOooOoO  OooOO'   ooOOoOo  `oooO'             `OoooO'  OOoOooO ooOOoOo ooOooOoO O     `o     o'    \n\
" SGR_RESET

#define STARTUP_SERVER_LOGO COLOR_FG_CRIMSON "\
Oo      oO ooOoOOo o.     O ooOoOOo `OooOOo.  o.OOoOoo o.OOOo.   ooOoOOo .oOOOo.            .oOOOo.  o.OOoOoo `OooOOo.  o      'O o.OOoOoo `OooOOo.  \n\
O O    o o    O    Oo     o    O     o     `o  O        O    `o     O    o     o            o     o   O        o     `o O       o  O        o     `o \n\
o  o  O  O    o    O O    O    o     O      O  o        o      O    o    O.                 O.        o        O      O o       O  o        O      O \n\
O   Oo   O    O    O  o   o    O     o     .O  ooOO     O      o    O     `OOoo.             `OOoo.   ooOO     o     .O o       o  ooOO     o     .O \n\
O        o    o    O   o  O    o     OOooOO'   O        o      O    o          `O                 `O  O        OOooOO'  O      O'  O        OOooOO'  \n\
o        O    O    o    O O    O     o    o    o        O      o    O           o                  o  o        o    o   `o    o    o        o    o   \n\
o        O    O    o     Oo    O     O     O   O        o    .O'    O    O.    .O           O.    .O  O        O     O   `o  O     O        O     O  \n\
O        o ooOOoOo O     `o ooOOoOo  O      o ooOooOoO  OooOO'   ooOOoOo  `oooO'             `oooO'  ooOooOoO  O      o   `o'     ooOooOoO  O      o \n\
" SGR_RESET

#endif /* MINI_REDIS_H */
