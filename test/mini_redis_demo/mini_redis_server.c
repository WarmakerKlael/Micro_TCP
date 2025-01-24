#include <stdio.h>
#include "microtcp.h"
#include "allocator/allocator_macros.h"
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include "microtcp_prompt_util.h"
#include "settings/microtcp_settings_prompts.h"

static void to_lowercase(char *str);

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

static inline void prompt_to_configure_microtcp(void)
{
        const char *const prompt = "Would you like to configure MicroTCP? [y/N]";
        char *answer_buffer = NULL;
        while (true)
        {
        PROMPT_WITH_READ_STRING(prompt, answer_buffer);
        scanf("%s", answer_buffer);
        to_lowercase(answer_buffer);
                if (strcmp(answer_buffer, "yes") == 0 || strcmp(answer_buffer, "y") == 0)
                {
                        configure_microtcp_settings();
                        break;
                }
                if (strcmp(answer_buffer, "no") == 0 || strcmp(answer_buffer, "n") == 0)
                        break;
        }
        free(answer_buffer);
}

int main(void)
{
        prompt_to_configure_microtcp();
        display_available_commands();
        return 0;
}

static void to_lowercase(char *str)
{
        for (int i = 0; str[i] != '\0'; i++)
        {
                str[i] = tolower((unsigned char)str[i]);
        }
}
