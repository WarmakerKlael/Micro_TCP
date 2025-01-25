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
#include "smart_assert.h"

static __always_inline FILE *open_file_for_binary_reading(const char *_file_name);
static __always_inline miniredis_header_t *create_miniredis_header(enum miniredis_commands _command_code, const char *_file_name);

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

static __always_inline status_t send_header_and_filename(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
                                                         const miniredis_header_t *const _header_ptr, const char *const _file_name)
{
        size_t packed_bytes_count = 0;

        /* Firstly, copy miniredis header. */
        memcpy(_message_buffer, _header_ptr, sizeof(*_header_ptr));
        packed_bytes_count += sizeof(*_header_ptr);

        /* Secondly, copy filename. */
        memcpy(_message_buffer + packed_bytes_count, _file_name, strlen(_file_name));
        packed_bytes_count += strlen(_file_name);
        ssize_t send_ret_val = microtcp_send(_socket, _message_buffer, packed_bytes_count, 0);

        return send_ret_val == packed_bytes_count ? SUCCESS : FAILURE;
}

static __always_inline status_t send_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
                                          FILE *const _file_ptr, const char *const _file_name)
{
        size_t file_bytes_sent_count = 0;
        size_t file_bytes_read;
        while ((file_bytes_read = fread(_message_buffer, 1, MAX_FILE_PART, _file_ptr)) > 0)
        {
                if (microtcp_send(_socket, _message_buffer, file_bytes_read, 0) != file_bytes_read)
                        LOG_APP_ERROR_RETURN(FAILURE, "microtcp_send() failed sending file-chunk, aborting.");
                file_bytes_sent_count += file_bytes_read;
                LOG_APP_INFO("File: %s, (%zu/%zu bytes sent)", _file_name, file_bytes_sent_count, file_stats.st_size);
        }
        if (ferror(_file_ptr))
                LOG_APP_ERROR_RETURN(FAILURE, "Error occurred while reading file '%s', errno(%d): %s", _file_name, errno, strerror(errno));
        LOG_APP_INFO_RETURN(SUCCESS, "File `%s` sent.", _file_name);
}

static void execute_set(microtcp_sock_t *_socket, const char *const _file_name)
{
        FILE *file_ptr = NULL;                 /* Requires deallocation */
        uint8_t *message_buffer = NULL;        /* Requires deallocation */
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation */

        if ((file_ptr = open_file_for_binary_reading(_file_name)) == NULL)
                goto cleanup_label;
        if ((header_ptr = create_miniredis_header(CMND_SET_CODE, _file_name)) == NULL)
                goto cleanup_label;
        if ((message_buffer = MALLOC_LOG(message_buffer, MAX_FILE_PART)) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed allocating memory for reading file.");

        if (send_header_and_filename(_socket, header_ptr, _file_name, message_buffer) == FAILURE)
                goto cleanup_label;
        if (send_file(_socket, message_buffer, file_ptr, _file_name) == FAILURE)
                goto cleanup_label;

        if (microtcp_recv(_socket, header_ptr, sizeof(*header_ptr), MSG_WAITALL) == MICROTCP_RECV_FAILURE)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed receive operation status from server.");
        if (header_ptr->operation_status == FAILURE)
                LOG_APP_ERROR_GOTO(cleanup_label, "Server was unable to %s %s.", CMND_SET_NAME, _file_name);
        LOG_APP_INFO("Server was able to %s %s.", CMND_SET_NAME, _file_name);

cleanup_label:
        if (file_ptr != NULL)
                fclose(file_ptr);
        FREE_NULLIFY_LOG(header_ptr);
        FREE_NULLIFY_LOG(message_buffer);
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

static __always_inline FILE *open_file_for_binary_reading(const char *const _file_name)
{
        if (access(_file_name, F_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s not found, errno(%d): %s.", _file_name, errno, strerror(errno));
        if (access(_file_name, R_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s read-permission missing, errno(%d): %s.", _file_name, errno, strerror(errno));

        FILE *file_ptr = fopen(_file_name, "rb");
        if (file_ptr == NULL)
                LOG_APP_ERROR_RETURN(NULL, "File: %s failed read-open, errno(%d): %s.", _file_name, errno, strerror(errno));
        return file_ptr;
}

static __always_inline miniredis_header_t *create_miniredis_header(const enum miniredis_commands _command_code, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_command_code > FIRST_PLACEHOLDER_COMMAND_CODE,
                           _command_code < LAST_PLACEHOLDER_COMMAND_CODE,
                           _file_name != NULL);

        struct stat file_stats;
        if (stat(_file_name, &file_stats) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s failed stats-read, errno(%d): %s.", _file_name, errno, strerror(errno));

        miniredis_header_t *header_ptr = MALLOC_LOG(header_ptr, sizeof(miniredis_header_t));

        header_ptr->command_code = _command_code;
        header_ptr->operation_status = FAILURE;
        header_ptr->message_size = 0;
        header_ptr->file_name_size = strlen(_file_name);
        header_ptr->file_size = file_stats.st_size;

        return header_ptr;
}
