#include <string.h>
#include "miniredis_demo/miniredis_commands.h"

const char *get_command_name(const enum miniredis_command_codes _command_code)
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

enum miniredis_command_codes get_command_code(const char *const _command_name)
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
