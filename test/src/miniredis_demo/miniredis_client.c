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

/* INLINE HELPERS for `execute_set()`. */
static __always_inline FILE *open_file_for_binary_io(const char *_file_name, enum io_type _io_type);
static __always_inline miniredis_header_t *create_miniredis_header(enum miniredis_command_codes _command_code, const char *_file_name, const char *_message);
static __always_inline status_t send_request_header_and_filename(microtcp_sock_t *_socket, uint8_t *_message_buffer,
                                                                 const miniredis_header_t *_header_ptr, const char *_file_name);
static __always_inline status_t send_file(microtcp_sock_t *_socket, uint8_t *_message_buffer,
                                          FILE *_file_ptr, size_t _file_size, const char *_file_name);
static __always_inline status_t receive_server_response_header(microtcp_sock_t *_socket, miniredis_header_t *_header_ptr,
                                                               enum miniredis_command_codes _expected_command_code, const char *_file_name);
static __always_inline void execute_set_resource_cleanup(FILE **_file_ptr_address,
                                                         uint8_t **_message_buffer_address,
                                                         miniredis_header_t **_header_ptr_address);

/* Static functions: */
static status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_server_address);
static status_t miniredis_terminate_connection(microtcp_sock_t *_utcp_socket);
static status_t miniredis_client_manager(void);

/* Command functions: */
static void request_set(microtcp_sock_t *_socket, const char *_file_name);
static void request_get(microtcp_sock_t *_socket, const char *_file_name);

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MAIN() <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
int main(void)
{
        display_startup_message(STARTUP_CLIENT_LOGO);
        prompt_to_configure_microtcp();
        miniredis_client_manager();
        return EXIT_SUCCESS;
}

static status_t miniredis_establish_connection(microtcp_sock_t *const _utcp_socket, struct sockaddr_in *const _server_address)
{
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_connect(_utcp_socket, (struct sockaddr *)_server_address, sizeof(*_server_address)) == MICROTCP_ACCEPT_FAILURE)
                return FAILURE;
        return SUCCESS;
}

static status_t miniredis_terminate_connection(microtcp_sock_t *const _utcp_socket)
{
        if (microtcp_shutdown(_utcp_socket, SHUT_RDWR) == MICROTCP_SHUTDOWN_FAILURE)
                return FAILURE;
        microtcp_close(_utcp_socket);
        return SUCCESS;
}

static void interactive_command_handler(microtcp_sock_t *_socket)
{
        char command_buffer[MAX_COMMAND_SIZE] = {0};
        char argument_buffer1[MAX_COMMAND_ARGUMENT_SIZE] = {0};
        char argument_buffer2[MAX_COMMAND_ARGUMENT_SIZE] = {0};

        _Bool exit_loop_flag = false;
        while (!exit_loop_flag && _socket->state == ESTABLISHED)
        {
                char *prompt_answer_buffer = NULL;
                PROMPT_WITH_READ_STRING("Enter prompt (type `HELP` for help ;) ): ", prompt_answer_buffer);
                int args_parsed = sscanf(prompt_answer_buffer, sscanf_command_format, command_buffer, argument_buffer1, argument_buffer2);

                switch (get_command_code(command_buffer))
                {
                case CMND_CODE_HELP:
                        display_help(); /* In help we don't do argument check, maybe the user is distressed. */
                        break;
                case CMND_CODE_QUIT:
                        if (args_parsed == CMND_ARGS_QUIT)
                                exit_loop_flag = true;
                        break;
                case CMND_CODE_GET:
                        if (args_parsed == CMND_ARGS_GET)
                                request_get(_socket, argument_buffer1);
                        break;
                case CMND_CODE_SET:
                        if (args_parsed == CMND_ARGS_SET)
                                request_set(_socket, argument_buffer1);
                        break;
                case CMND_CODE_DEL:
                        break;
                case CMND_CODE_CACHE:
                        break;
                case CMND_CODE_LIST:
                        break;
                case CMND_CODE_INFO:
                        break;
                case CMND_CODE_SIZE:
                        break;
                default:
                        break;
                }
                FREE_NULLIFY_LOG(prompt_answer_buffer);
        }
}

static status_t miniredis_client_manager(void)
{
        struct sockaddr_in server_address = {
            .sin_family = AF_INET,
            .sin_port = htons(50503)};
        inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
        microtcp_sock_t utcp_socket = {0};

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

        if ((file_ptr = open_file_for_binary_io(_file_name, IO_READ)) == NULL)
                goto cleanup_label;
        if ((header_ptr = create_miniredis_header(CMND_CODE_SET, _file_name, NULL)) == NULL)
                goto cleanup_label;
        if ((message_buffer = MALLOC_LOG(message_buffer, MAX_FILE_PART)) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed allocating memory for reading file `%s`.", _file_name);
        if (send_request_header_and_filename(_socket, message_buffer, header_ptr, _file_name) == FAILURE)
                goto cleanup_label;
        if (send_file(_socket, message_buffer, file_ptr, header_ptr->file_size, _file_name) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_SET, _file_name) == FAILURE)
                goto cleanup_label;
cleanup_label:
        execute_set_resource_cleanup(&file_ptr, &message_buffer, &header_ptr);
}

static void request_get(microtcp_sock_t *const _socket, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _file_name != NULL);
        FILE *file_ptr = NULL;                 /* Requires deallocation */
        uint8_t *message_buffer = NULL;        /* Requires deallocation */
        miniredis_header_t *header_ptr = NULL; /* Requires deallocation */
        if ((header_ptr = create_miniredis_header(CMND_CODE_GET, _file_name, NULL)) == NULL)
                goto cleanup_label;
        if ((message_buffer = MALLOC_LOG(message_buffer, MAX_FILE_PART)) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed allocating memory for writing file `%s`.", _file_name);
        if (send_request_header_and_filename(_socket, message_buffer, header_ptr, _file_name) == FAILURE)
                goto cleanup_label;
        if (receive_server_response_header(_socket, header_ptr, CMND_CODE_GET, _file_name) == FAILURE)
                goto cleanup_label;
        if ((file_ptr = open_file_for_binary_io(STAGING_FILE_NAME, IO_WRITE)) == NULL)
                goto cleanup_label;
        if (receive_file(_socket, message_buffer, file_ptr, header_ptr->file_size, _file_name) == FAILURE)
                goto cleanup_label;
        if (finalize_file(&file_ptr, STAGING_FILE_NAME, _file_name) == FAILURE)
                goto cleanup_label;
        LOG_APP_INFO("Client received and stored `%s`.", _file_name);

cleanup_label:
        /* 3rd argument: Dummy file_name address which points to NULL, as in client side `_file_name` is statically allocated. */
        cleanup_write_file_resources(&file_ptr, &message_buffer, &(char *){NULL});
}

static __always_inline FILE *open_file_for_binary_io(const char *const _file_name, const enum io_type _io_type)
{
        DEBUG_SMART_ASSERT(_file_name != NULL, _io_type == IO_READ || _io_type == IO_WRITE);
        if (_io_type == IO_READ && access(_file_name, F_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s not found, errno(%d): %s.", _file_name, errno, strerror(errno));
        if (_io_type == IO_READ && access(_file_name, R_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s read-permission missing, errno(%d): %s.", _file_name, errno, strerror(errno));

        const char *fopen_mode = _io_type == IO_READ ? "rb" : "wb";
        FILE *file_ptr = fopen(_file_name, fopen_mode);
        if (file_ptr == NULL)
                LOG_APP_ERROR_RETURN(NULL, "File: %s failed %s-open, errno(%d): %s.", _file_name,
                                     (_io_type == IO_READ ? "read" : "write"), errno, strerror(errno));
        return file_ptr;
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
                header_ptr->file_size = 0;
                break;
        }

        return header_ptr;

cleanup_failure_label:
        FREE_NULLIFY_LOG(header_ptr);
        return NULL;
}

static __always_inline status_t send_request_header_and_filename(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
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

        DEBUG_SMART_ASSERT(packed_bytes_count < ((size_t)-1) >> 1);
        return send_ret_val == (ssize_t)packed_bytes_count ? SUCCESS : FAILURE;
}

static __always_inline status_t send_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
                                          FILE *const _file_ptr, const size_t _file_size, const char *const _file_name)
{
        size_t file_bytes_sent_count = 0;
        size_t file_bytes_read;
        while ((file_bytes_read = fread(_message_buffer, 1, MAX_FILE_PART, _file_ptr)) > 0)
        {
                DEBUG_SMART_ASSERT(file_bytes_read < ((size_t)-1) >> 1);
                if (microtcp_send(_socket, _message_buffer, file_bytes_read, 0) != (ssize_t)file_bytes_read)
                        LOG_APP_ERROR_RETURN(FAILURE, "microtcp_send() failed sending file-chunk, aborting.");
                file_bytes_sent_count += file_bytes_read;
                LOG_APP_INFO("File: %s, (%zu/%zu bytes sent)", _file_name, file_bytes_sent_count, _file_size);
        }
        if (ferror(_file_ptr))
                LOG_APP_ERROR_RETURN(FAILURE, "Error occurred while reading file '%s', errno(%d): %s", _file_name, errno, strerror(errno));
        LOG_APP_INFO_RETURN(SUCCESS, "File `%s` sent.", _file_name);
}

static __always_inline status_t receive_server_response_header(microtcp_sock_t *const _socket, miniredis_header_t *const _header_ptr,
                                                               const enum miniredis_command_codes _expected_command_code, const char *const _file_name)
{
        const size_t required_reception_length = sizeof(*_header_ptr);
        if (microtcp_recv_timed(_socket, _header_ptr, required_reception_length, MAX_RESPONSE_IDLE_TIME) != required_reception_length)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed receiving server response.");
        if (_header_ptr->command_code != _expected_command_code)
                LOG_APP_ERROR_RETURN(FAILURE, "Expected command_code = `%s`, server response command_code = `%s`.",
                                     get_command_name(_expected_command_code), get_command_name(_header_ptr->command_code));
        if (_header_ptr->response_status == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Server failed to complete request: `%s %s`.", get_command_name(_expected_command_code), _file_name);
        LOG_APP_INFO_RETURN(SUCCESS, "Server completed request: `%s %s`.", get_command_name(_expected_command_code), _file_name);
}

static __always_inline void execute_set_resource_cleanup(FILE **const _file_ptr_address,
                                                         uint8_t **const _message_buffer_address,
                                                         miniredis_header_t **const _header_ptr_address)
{
        if (*_file_ptr_address != NULL)
        {
                fclose(*_file_ptr_address);
                *_file_ptr_address = NULL;
        }
        if (*_header_ptr_address != NULL)
                FREE_NULLIFY_LOG(*_header_ptr_address);
        if (*_message_buffer_address != NULL)
                FREE_NULLIFY_LOG(*_message_buffer_address);
}
