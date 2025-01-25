#ifndef MINIREDIS_COMMON_SOURCE_COD_H
#define MINIREDIS_COMMON_SOURCE_COD_H

#include "demo_common.h"
#include "microtcp_prompt_util.h"

static inline void prompt_to_configure_microtcp(void)
{
        const char *const prompt = "Would you like to configure MicroTCP [y/N]? ";
        char *prompt_answer_buffer = NULL;
        const char default_answer = 'N';
        while (true)
        {
                PROMPT_WITH_YES_NO(prompt, default_answer, prompt_answer_buffer);
                to_lowercase(prompt_answer_buffer);
                if (strcmp(prompt_answer_buffer, "yes") == 0 || strcmp(prompt_answer_buffer, "y") == 0)
                {
                        configure_microtcp_settings();
                        break;
                }
                if (strcmp(prompt_answer_buffer, "no") == 0 || strcmp(prompt_answer_buffer, "n") == 0)
                        break;
                clear_line();
        }
        free(prompt_answer_buffer);
}

#define LOG_APP_ERROR_GOTO(_goto_label, _format_message, ...)  \
        do                                                     \
        {                                                      \
                LOG_APP_ERROR(_format_message, ##__VA_ARGS__); \
                goto _goto_label;                              \
        } while (0)


#endif /* MINIREDIS_COMMON_SOURCE_COD_H */
