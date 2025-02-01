#ifndef MINIREDIS_COMMANDS_H
#define MINIREDIS_COMMANDS_H

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
#define CMND_ARGS_LIST 1

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

const char *get_command_name(enum miniredis_command_codes _command_code);

enum miniredis_command_codes get_command_code(const char *_command_name);

#endif /* MINIREDIS_COMMANDS_H */
