#ifndef MINIREDIS_COMMANDS_H
#define MINIREDIS_COMMANDS_H
#include "logging/microtcp_logger.h"

enum miniredis_command_codes
{
        FIRST_PLACEHOLDER_COMMAND_CODE = 0,

        CMND_CODE_QUIT,
#define CMND_NAME_QUIT "QUIT"
#define CMND_ARGS_QUIT 1

        CMND_CODE_HELP,
#define CMND_NAME_HELP "HELP"
#define CMND_ARGS_HELP 1

        CMND_CODE_GET,
#define CMND_NAME_GET "GET"
#define CMND_ARGS_GET 2

        CMND_CODE_SET,
#define CMND_NAME_SET "SET"
#define CMND_ARGS_SET 2

        CMND_CODE_DEL,
#define CMND_NAME_DEL "DEL"
#define CMND_ARGS_DEL 2

        CMND_CODE_CACHE,
#define CMND_NAME_CACHE "CACHE"
#define CMND_ARGS_CACHE 2

        CMND_CODE_LIST,
#define CMND_NAME_LIST "LIST"
#define CMND_ARGS_LIST 2

        CMND_CODE_INFO,
#define CMND_NAME_INFO "INFO"
#define CMND_ARGS_INFO 2

        CMND_CODE_SIZE,
#define CMND_NAME_SIZE "SIZE"
#define CMND_ARGS_SIZE 2

        CMND_CODE_INVALID,
#define CMND_NAME_INVALID "INVALID"

        LAST_PLACEHOLDER_COMMAND_CODE
};

static const char *get_command_name(enum miniredis_command_codes _command_code)
{
        // clang-format off
        switch (_command_code)
        {
        case CMND_CODE_QUIT:    return CMND_NAME_QUIT;
        case CMND_CODE_HELP:    return CMND_NAME_HELP;
        case CMND_CODE_GET:     return CMND_NAME_GET;
        case CMND_CODE_SET:     return CMND_NAME_SET;
        case CMND_CODE_DEL:     return CMND_NAME_DEL;
        case CMND_CODE_CACHE:   return CMND_NAME_CACHE;
        case CMND_CODE_LIST:    return CMND_NAME_LIST;
        case CMND_CODE_INFO:    return CMND_NAME_INFO;
        case CMND_CODE_SIZE:    return CMND_NAME_SIZE;
        default:
                return CMND_NAME_INVALID;
        }
        // clang-format on
}

static enum miniredis_command_codes get_command_code(const char *const _command_name)
{
        // clang-format off
        if (strcmp(_command_name, CMND_NAME_QUIT) == 0)         return CMND_CODE_QUIT;
        if (strcmp(_command_name, CMND_NAME_HELP) == 0)         return CMND_CODE_HELP;
        if (strcmp(_command_name, CMND_NAME_GET) == 0)          return CMND_CODE_GET;
        if (strcmp(_command_name, CMND_NAME_SET) == 0)          return CMND_CODE_SET;
        if (strcmp(_command_name, CMND_NAME_DEL) == 0)          return CMND_CODE_DEL;
        if (strcmp(_command_name, CMND_NAME_CACHE) == 0)        return CMND_CODE_CACHE;
        if (strcmp(_command_name, CMND_NAME_LIST) == 0)         return CMND_CODE_LIST;
        if (strcmp(_command_name, CMND_NAME_INFO) == 0)         return CMND_CODE_INFO;
        if (strcmp(_command_name, CMND_NAME_SIZE) == 0)         return CMND_CODE_SIZE;
        return CMND_CODE_INVALID;

        // clang-format on
}

#endif /* MINIREDIS_COMMANDS_H */
