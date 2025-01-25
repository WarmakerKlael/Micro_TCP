#include <unistd.h>
#include "microtcp.h"
#include "demo_common.h"
#include "miniredis_demo/miniredis.h"
#include "miniredis_demo/miniredis_commands.h"
#include "miniredis_demo/registry.h"
#include "logging/microtcp_logger.h"
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "allocator/allocator_macros.h"
#include "common_source_code.h"

static inline status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_server_address)
{
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_connect(_utcp_socket, (struct sockaddr *)_server_address, sizeof(*_server_address)) == MICROTCP_ACCEPT_FAILURE)
                return FAILURE;
        return SUCCESS;
}

static status_t miniredis_terminate_connection(microtcp_sock_t *_utcp_socket)
{
        if (microtcp_shutdown(_utcp_socket, SHUT_RDWR) == MICROTCP_SHUTDOWN_FAILURE)
                return FAILURE;
        microtcp_close(_utcp_socket);
        return SUCCESS;
}

static void execute_set(microtcp_sock_t *_socket, const char *const _file_name)
{
        uint8_t *message_buffer = NULL; /* Initiliazing first thing, so that free() in cleanup: wont lead to undefined. */
        FILE *file_ptr = NULL;          /* Same here. */

        if (access(_file_name, F_OK) != 0)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "File: %s not found, errno(%d): %s.", _file_name, errno, strerror(errno));
        if (access(_file_name, R_OK) != 0)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "File: %s read-permission missing, errno(%d): %s.", _file_name, errno, strerror(errno));

        file_ptr = fopen(_file_name, "rb");
        if (file_ptr == NULL)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "File: %s failed read-open, errno(%d): %s.", _file_name, errno, strerror(errno));

        struct stat file_stats;
        if (stat(_file_name, &file_stats) != 0)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "File: %s failed stats-read, errno(%d): %s.", _file_name, errno, strerror(errno));

        miniredis_header_t header = {
            .command_code = CMND_SET_CODE,
            .operation_status = FAILURE, /* We successful reception and storing by server, we expect this header returned, but operation status == SUCCESS. */
            .message_size = 0,
            .file_name_size = strlen(_file_name),
            .file_size = file_stats.st_size};

        message_buffer = MALLOC_LOG(message_buffer, MAX_FILE_CHUNK);
        if (message_buffer == NULL)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "Failed allocating memory for reading file.");

        /* Firstly, copy miniredis header. */
        memcpy(message_buffer, &header, sizeof(header));
        size_t message_buffer_bytes = sizeof(header);
        /* Secondly, copy filename. */
        memcpy(message_buffer + message_buffer_bytes, _file_name, strlen(_file_name));
        message_buffer_bytes += strlen(_file_name);

        size_t file_bytes_sent = 0;
        size_t bytes_read = 0;
        while ((bytes_read = fread(message_buffer + message_buffer_bytes, 1, MAX_FILE_CHUNK - message_buffer_bytes, file_ptr)) > 0)
        {
                message_buffer_bytes += bytes_read;
                if (microtcp_send(_socket, message_buffer, message_buffer_bytes, 0) == MICROTCP_SEND_FAILURE)
                        LOG_APP_ERROR_GOTO(execute_set_cleanup, "microtcp_send() failed sending file-chunk, aborting.");
                file_bytes_sent += bytes_read;
                message_buffer_bytes = 0;
                LOG_APP_INFO("File: %s, (%zu/%zu bytes sent)", _file_name, file_bytes_sent, file_stats.st_size);
        }
        if (ferror(file_ptr))
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "Error while reading file: %s", _file_name);

        if (microtcp_recv(_socket, &header, sizeof(header), MSG_WAITALL) == MICROTCP_RECV_FAILURE)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "Failed receive operation status from server.");
        if (header.operation_status == FAILURE)
                LOG_APP_ERROR_GOTO(execute_set_cleanup, "Server was unable to %s %s.", CMND_SET_NAME, _file_name);
        LOG_APP_INFO("Server was able to %s %s.", CMND_SET_NAME, _file_name);

execute_set_cleanup:
        if (file_ptr != NULL)
                fclose(file_ptr);
        free(message_buffer);
}

static status_t miniredis_client_manager(void)
{
        struct sockaddr_in server_address = {
            .sin_family = AF_INET,
            .sin_port = htons(50503)};
        inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
        microtcp_sock_t utcp_socket = {0};
        char command_buffer[MAX_COMMAND_SIZE] = {0};
        char argument_buffer1[MAX_COMMAND_ARGUMENT_SIZE] = {0};
        char argument_buffer2[MAX_COMMAND_ARGUMENT_SIZE] = {0};

        if (miniredis_establish_connection(&utcp_socket, &server_address) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed establishing connection.");

        while (true)
        {
                char *prompt_answer_buffer = NULL;
                PROMPT_WITH_READ_STRING("Enter prompt (HELP to request help, QUIT to exit): ", prompt_answer_buffer);

                int args_parsed = sscanf(prompt_answer_buffer, sscanf_command_format, command_buffer, argument_buffer1, argument_buffer2);
                printf("\n\nARGS_PARSED -> %d\n\n", args_parsed);
                if (strcmp(command_buffer, CMND_QUIT_NAME) == 0 && args_parsed == CMND_QUIT_ARGS)
                        break;
                else if (strcmp(command_buffer, CMND_HELP_NAME) == 0 && args_parsed == CMND_HELP_ARGS)
                        display_help();
                else if (strcmp(command_buffer, CMND_SET_NAME) == 0 && args_parsed == CMND_SET_ARGS)
                        execute_set(&utcp_socket, argument_buffer1);
                free(prompt_answer_buffer);
        }

        if (miniredis_terminate_connection(&utcp_socket) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed terminating connection.");
        return SUCCESS;
}

int main(void)
{
        display_startup_message("Welcome to MINI-REDIS client application.");
        prompt_to_configure_microtcp();
        miniredis_client_manager();
        return EXIT_SUCCESS;
}
