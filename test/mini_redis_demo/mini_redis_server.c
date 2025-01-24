#include <stdio.h>
#include "microtcp.h"
#include "allocator/allocator_macros.h"
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static const char *get_available_commands(void)
{
        static const char available_commands_menu[] =
            "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n"
            "┃      Available commands       ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->HELP                         ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->GET <filename> <saving-path> ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->SET <filename> <path-to-file>┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->DEL <filename>               ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->CACHE <filename>             ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->LIST                         ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->INFO <filename>              ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->SIZE <filename>              ┃\n"
            "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n";
        return available_commands_menu;
}

static inline void display_available_commands(void)
{
        printf("%s", get_available_commands());
}

int main(void)
{
        display_available_commands();
        return 0;
}
