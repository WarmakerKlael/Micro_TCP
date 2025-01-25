#ifndef MINIREDIS_COMMANDS_H
#define MINIREDIS_COMMANDS_H

enum miniredis_commands
{
        CMND_QUIT_CODE = 0,
#define CMND_QUIT_NAME "QUIT"
#define CMND_QUIT_ARGS 1

        CMND_HELP_CODE,
#define CMND_HELP_NAME "HELP"
#define CMND_HELP_ARGS 1

        CMND_GET_CODE,
#define CMND_GET_NAME "GET"
#define CMND_GET_ARGS 2

        CMND_SET_CODE,
#define CMND_SET_NAME "SET"
#define CMND_SET_ARGS 2

        CMND_DEL_CODE,
#define CMND_DEL_NAME "DEL"
#define CMND_DEL_ARGS 2

        CMND_CACHE_CODE,
#define CMND_CACHE_NAME "CACHE"
#define CMND_CACHE_ARGS 2

        CMND_LIST_CODE,
#define CMND_LIST_NAME "LIST"
#define CMND_LIST_ARGS 2

        CMND_INFO_CODE,
#define CMND_INFO_NAME "INFO"
#define CMND_INFO_ARGS 2

        CMND_SIZE_CODE
#define CMND_SIZE_NAME "SIZE"
#define CMND_SIZE_ARGS 2
};

#endif /* MINIREDIS_COMMANDS_H */
