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
#include "miniredis_demo/miniredis_commands.h"
#include "allocator/allocator_macros.h"
#include "common_source_code.h"
#include "smart_assert.h"

/* INLINE HELPERS for `execute_set()`. */
static __always_inline miniredis_header_t *create_miniredis_header(enum miniredis_command_codes _command_code, const char *_file_name,
                                                                   const char *_message);
static __always_inline status_t receive_server_response_header(microtcp_sock_t *_socket, miniredis_header_t *_header_ptr,
                                                               enum miniredis_command_codes _expected_command_code);
static __always_inline void receive_and_display_failure_response_message(microtcp_sock_t *_socket, const miniredis_header_t *_header_ptr,
                                                                         enum miniredis_command_codes _expected_command_code, const char *_file_name);
static __always_inline status_t receive_and_display_registry_list(microtcp_sock_t *_socket, uint8_t *_message_buffer,
                                                                  size_t message_buffer_size, size_t _response_message_size);

/* Static functions: */
static status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_server_address);
static status_t miniredis_client_manager(void);

/* Command functions: */
static void request_set(microtcp_sock_t *_socket, const char *_file_name);
static void request_get(microtcp_sock_t *_socket, const char *_file_name);
static void request_del(microtcp_sock_t *_socket, const char *_file_name);
static void request_list(microtcp_sock_t *_socket);

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MAIN() <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
int main(void)
{
        display_startup_message(STARTUP_CLIENT_LOGO);
        display_current_path();
        prompt_to_configure_microtcp();
        miniredis_client_manager();
        return EXIT_SUCCESS;
}

static status_t miniredis_establish_connection(microtcp_sock_t *const _utcp_socket, struct sockaddr_in *const _server_address)
{
        LOG_APP_INFO("MiniRedis Client-side attempting to connect to server...");
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_connect(_utcp_socket, (struct sockaddr *)_server_address, sizeof(*_server_address)) == MICROTCP_ACCEPT_FAILURE)
                return FAILURE;
        LOG_APP_INFO_RETURN(SUCCESS, "MiniRedis Client-side connected to server.");
}

/* TODO: avoid big switch... */
static void interactive_command_handler(microtcp_sock_t *_socket)
{
        char command_buffer[MAX_COMMAND_SIZE + 1] = {0};            /* +1 for '\0'. */
        char argument_buffer1[MAX_COMMAND_ARGUMENT_SIZE + 1] = {0}; /* +1 for '\0'. */
        char argument_buffer2[MAX_COMMAND_ARGUMENT_SIZE + 1] = {0}; /* +1 for '\0'. */

        _Bool exit_loop_flag = false;
        while (!exit_loop_flag && _socket->state == ESTABLISHED)
        {
                char *prompt_answer_buffer = NULL;
                PROMPT_WITH_READ_STRING("Enter prompt (type `HELP` for help ;) ): ", prompt_answer_buffer);
                int args_parsed = sscanf(prompt_answer_buffer, sscanf_command_format, command_buffer, argument_buffer1, argument_buffer2);
#define LOG_COMMAND_ARGUMENT_WARNING(_cmnd_name, _cmnd_args) \
        LOG_APP_WARNING("Command: %s, expects %d arguments.", (_cmnd_name), (_cmnd_args - 1)) /* -1 as one argument is the command itself. */

                switch (get_command_code(command_buffer))
                {
                case CMND_CODE_HELP:
                        display_help(); /* In help we don't do argument check, maybe the user is distressed. */
                        break;
                case CMND_CODE_QUIT:
                        if (args_parsed == CMND_ARGS_QUIT)
                                exit_loop_flag = true;
                        else
                                LOG_COMMAND_ARGUMENT_WARNING(CMND_NAME_QUIT, CMND_ARGS_QUIT);
                        break;
                case CMND_CODE_SET:
                        if (args_parsed == CMND_ARGS_SET)
                                request_set(_socket, argument_buffer1);
                        else
                                LOG_COMMAND_ARGUMENT_WARNING(CMND_NAME_SET, CMND_ARGS_SET);
                        break;
                case CMND_CODE_GET:
                        if (args_parsed == CMND_ARGS_GET)
                                request_get(_socket, argument_buffer1);
                        else
                                LOG_COMMAND_ARGUMENT_WARNING(CMND_NAME_GET, CMND_ARGS_GET);
                        break;
                case CMND_CODE_DEL:
                        if (args_parsed == CMND_ARGS_DEL)
                                request_del(_socket, argument_buffer1);
                        else
                                LOG_COMMAND_ARGUMENT_WARNING(CMND_NAME_DEL, CMND_ARGS_DEL);
                        break;
                case CMND_CODE_LIST:
                        if (args_parsed == CMND_ARGS_LIST)
                                request_list(_socket);
                        else
                                LOG_COMMAND_ARGUMENT_WARNING(CMND_NAME_LIST, CMND_ARGS_LIST);
                        break;
                default: /* Unknown command. */
                        LOG_APP_WARNING("Either unsupported command, or command contained wrong number of arguements. ");
                        break;
                }
                FREE_NULLIFY_LOG(prompt_answer_buffer);
        }
#undef LOG_COMMAND_ARGUMENT_WARNING
}

static status_t miniredis_client_manager(void)
{
        static microtcp_sock_t utcp_socket = {0};
        struct sockaddr_in server_address = {
            .sin_family = AF_INET,
            .sin_port = request_server_port(),
            .sin_addr = request_server_ipv4()};

        if (miniredis_establish_connection(&utcp_socket, &server_address) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed establishing connection.");

        interactive_command_handler(&utcp_socket);

        if (miniredis_terminate_connection(&utcp_socket) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed terminating connection.");
        return SUCCESS;
}

static void request_set(microtcp_sock_t *const _socket, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _file_name != NULL);
        FILE *file_ptr = NULL;                 /* Requires deallocation */
        uint8_t *message_buffer = NULL;        /* Requires deallocation */
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation */
        const size_t message_buffer_size = _socket->peer_win_size;

        if ((file_ptr = open_file_for_binary_io(_file_name, IO_READ)) == NULL)
                goto cleanup_label;
        if ((header_ptr = create_miniredis_header(CMND_CODE_SET, _file_name, NULL)) == NULL)
                goto cleanup_label;
        if ((message_buffer = allocate_message_buffer(message_buffer_size)) == NULL)
                goto cleanup_label;
        if (send_request_header(_socket, header_ptr) == FAILURE)
                goto cleanup_label;
        if (send_filename(_socket, _file_name) == FAILURE)
                goto cleanup_label;
        if (send_file(_socket, message_buffer, message_buffer_size, file_ptr, _file_name) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_SET) == FAILURE)
                goto cleanup_label;
        if (header_ptr->response_status == FAILURE)
                receive_and_display_failure_response_message(_socket, header_ptr, CMND_CODE_SET, _file_name);
cleanup_label:
        cleanup_file_sending_resources(&file_ptr, &message_buffer, &(char *){NULL}, &header_ptr);
}

static void request_get(microtcp_sock_t *const _socket, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _file_name != NULL);
        FILE *file_ptr = NULL;                 /* Requires deallocation */
        uint8_t *message_buffer = NULL;        /* Requires deallocation */
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation */
        const size_t message_buffer_size = get_microtcp_bytestream_rrb_size();

        if ((header_ptr = create_miniredis_header(CMND_CODE_GET, _file_name, NULL)) == NULL)
                goto cleanup_label;
        if (send_request_header(_socket, header_ptr) == FAILURE)
                goto cleanup_label;
        if (send_filename(_socket, _file_name) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_GET) == FAILURE)
                goto cleanup_label;
        if (header_ptr->response_status == FAILURE)
        {
                receive_and_display_failure_response_message(_socket, header_ptr, CMND_CODE_GET, _file_name);
                goto cleanup_label;
        }
        if ((file_ptr = open_file_for_binary_io(STAGING_FILE_NAME, IO_WRITE)) == NULL)
                goto cleanup_label;
        if ((message_buffer = allocate_message_buffer(message_buffer_size)) == NULL)
                goto cleanup_label;
        if (receive_file(_socket, message_buffer, message_buffer_size, file_ptr, header_ptr->file_size, _file_name) == FAILURE)
                goto cleanup_label;
        if (finalize_file(&file_ptr, STAGING_FILE_NAME, _file_name) == FAILURE)
                goto cleanup_label;
        LOG_APP_INFO("Client received and stored `%s`.", _file_name);

cleanup_label:
        /* 3rd argument: Dummy file_name address which points to NULL, as in client side `_file_name` is statically allocated. */
        cleanup_file_receiving_resources(&file_ptr, &message_buffer, &(char *){NULL}, &header_ptr);
}

static void request_del(microtcp_sock_t *const _socket, const char *const _file_name)
{
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation */
        if ((header_ptr = create_miniredis_header(CMND_CODE_DEL, _file_name, NULL)) == NULL)
                goto cleanup_label;
        if (send_request_header(_socket, header_ptr) == FAILURE)
                goto cleanup_label;
        if (send_filename(_socket, _file_name) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_DEL) == FAILURE)
                goto cleanup_label;
        if (header_ptr->response_status == FAILURE)
                receive_and_display_failure_response_message(_socket, header_ptr, CMND_CODE_DEL, _file_name);
cleanup_label:
        if (header_ptr != NULL)
                FREE_NULLIFY_LOG(header_ptr);
}

static __always_inline status_t receive_and_display_registry_list(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
                                                                  const size_t message_buffer_size, const size_t _response_message_size)
{
        size_t file_counter = 0;
        size_t displayed_bytes_count = 0;
        printf(COLOR_FG_GREEN "============================REGISTRY_LIST_BEGIN============================\n");
        while (displayed_bytes_count != _response_message_size)
        {
                /* Receive the rnsi: */
                struct registry_node_serialization_info rnsi;
                ssize_t received_bytes = microtcp_recv_timed(_socket, &rnsi, sizeof(rnsi), MAX_RESPONSE_IDLE_TIME);
                if (received_bytes != sizeof(rnsi))
                        LOG_APP_ERROR_RETURN(FAILURE, "Failed receiving response-part (rnsi) of file #%zu.", file_counter);

                /* Receive file-name: */
                SMART_ASSERT(rnsi.file_name_size < message_buffer_size); /* Leads to Buffer-Overflow. (safe assertion for any kind of filename). */
                received_bytes = microtcp_recv_timed(_socket, _message_buffer, rnsi.file_name_size, MAX_RESPONSE_IDLE_TIME);
                if (received_bytes != (ssize_t)rnsi.file_name_size)
                        LOG_APP_ERROR_RETURN(FAILURE, "Failed receiving response-part (file-name) of file #%zu.", file_counter);

                /* Display file-stats: */

                printf(COLOR_FG_MATRIX "%zu. Name: %.*s Size: %zu Downloads: %zu TOA: %ld\n",
                       file_counter,
                       (int)rnsi.file_name_size, _message_buffer,
                       rnsi.file_size,
                       rnsi.download_counter,
                       rnsi.time_of_arrival);

                /* Move to next file: */
                displayed_bytes_count += sizeof(rnsi) + rnsi.file_name_size;
                file_counter++;
        }
        printf("=============================REGISTRY_LIST_END=============================\n" SGR_RESET);
        return SUCCESS;
}

static void request_list(microtcp_sock_t *const _socket)
{
        uint8_t *message_buffer = NULL;        /* Requires deallocation */
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation. */
        const size_t message_buffer_size = get_microtcp_bytestream_rrb_size();
        if ((header_ptr = create_miniredis_header(CMND_CODE_LIST, NULL, NULL)) == NULL)
                goto cleanup_label;
        if (send_request_header(_socket, header_ptr) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_LIST) == FAILURE)
                goto cleanup_label;
        if (header_ptr->response_status == FAILURE)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed receiving server's registry list");
        if ((message_buffer = allocate_message_buffer(message_buffer_size)) == NULL)
                goto cleanup_label;
        if (receive_and_display_registry_list(_socket, message_buffer, message_buffer_size, header_ptr->response_message_size) == FAILURE)
                goto cleanup_label;
cleanup_label:
        if (message_buffer != NULL)
                FREE_NULLIFY_LOG(message_buffer);
        if (header_ptr != NULL)
                FREE_NULLIFY_LOG(header_ptr);
}

static __always_inline miniredis_header_t *create_miniredis_header(const enum miniredis_command_codes _command_code,
                                                                   const char *const _file_name, const char *const _message)
{
        DEBUG_SMART_ASSERT(_command_code > FIRST_PLACEHOLDER_COMMAND_CODE,
                           _command_code < LAST_PLACEHOLDER_COMMAND_CODE);
        miniredis_header_t *header_ptr = MALLOC_LOG(header_ptr, sizeof(miniredis_header_t));
        header_ptr->command_code = _command_code;
        header_ptr->response_status = FAILURE;
        header_ptr->response_message_size = 0;
        header_ptr->file_name_size = 0;
        header_ptr->file_size = 0;

        struct stat file_stats = {0};
        switch (_command_code)
        {
        case CMND_CODE_SET:
                DEBUG_SMART_ASSERT(_file_name != NULL, _message == NULL);
                if (stat(_file_name, &file_stats) != 0)
                        LOG_APP_ERROR_GOTO(cleanup_failure_label, "File: %s failed stats-read, errno(%d): %s.", _file_name, errno, strerror(errno));
                header_ptr->file_name_size = strlen(_file_name);
                header_ptr->file_size = file_stats.st_size;
                break;
        case CMND_CODE_GET:
                DEBUG_SMART_ASSERT(_file_name != NULL, _message == NULL);
                header_ptr->file_name_size = strlen(_file_name);
                break;
        case CMND_CODE_DEL:
                DEBUG_SMART_ASSERT(_file_name != NULL, _message == NULL);
                header_ptr->file_name_size = strlen(_file_name);
                break;
        case CMND_CODE_LIST:
                DEBUG_SMART_ASSERT(_file_name == NULL, _message == NULL);
                break;
        }

        return header_ptr;

cleanup_failure_label:
        FREE_NULLIFY_LOG(header_ptr);
        return NULL;
}

static __always_inline void receive_and_display_failure_response_message(microtcp_sock_t *const _socket, const miniredis_header_t *const _header_ptr,
                                                                         const enum miniredis_command_codes _expected_command_code, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _header_ptr != NULL, _file_name != NULL);
        /* Receive response message. */
        DEBUG_SMART_ASSERT(_header_ptr->response_message_size < MB);                                   /* Crazy ass failure response message.. Why is it so long? */
        char *response_message = MALLOC_LOG(response_message, _header_ptr->response_message_size + 1); /* +1 for '\0'. */
        response_message[_header_ptr->response_message_size] = '\0';                                   /* Added the NULL terminating string byte. */
        if (response_message == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed to allocate %zu bytes for storing server's response message.", _header_ptr->response_message_size + 1);
        if (microtcp_recv_timed(_socket, response_message, _header_ptr->response_message_size, MAX_RESPONSE_IDLE_TIME) != (ssize_t)_header_ptr->response_message_size)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed to receiving response message from server.");
        LOG_APP_ERROR("Server failed to complete request: `%s %s`. Server's failure reason: %s",
                      get_command_name(_expected_command_code), _file_name, response_message);
cleanup_label:
        if (response_message)
                FREE_NULLIFY_LOG(response_message);
}

static __always_inline status_t receive_server_response_header(microtcp_sock_t *const _socket, miniredis_header_t *const _header_ptr,
                                                               const enum miniredis_command_codes _expected_command_code)
{
        const size_t required_reception_length = sizeof(*_header_ptr);

        /* Receive response header. */
        if (microtcp_recv_timed(_socket, _header_ptr, required_reception_length, MAX_RESPONSE_IDLE_TIME) != required_reception_length)
                LOG_APP_ERROR_RETURN(FAILURE, "%s exceeded, failed receiving server response header.", STRINGIFY(MAX_RESPONSE_IDLE_TIME));
        if (_header_ptr->command_code != _expected_command_code)
                LOG_APP_ERROR_RETURN(FAILURE, "Expected command_code = `%s`, server response command_code = `%s`.",
                                     get_command_name(_expected_command_code), get_command_name(_header_ptr->command_code));
        LOG_APP_INFO_RETURN(SUCCESS, "Successfully received server response header.");
}
