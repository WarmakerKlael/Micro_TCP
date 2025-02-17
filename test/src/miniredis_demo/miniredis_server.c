#include <stdio.h>
#include "microtcp.h"
#include "allocator/allocator_macros.h"
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include "miniredis_demo/miniredis_commands.h"
#include "microtcp_prompt_util.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings_prompts.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "demo_common.h"
#include "miniredis_demo/registry.h"
#include "miniredis_demo/miniredis_commands.h"
#include "miniredis_demo/miniredis.h"
#include "common_source_code.h"
#include <pthread.h>
#include <stdatomic.h> // For thread-safe stop flag

struct timeval max_response_idle_time = MIN_RESPONSE_IDLE_TIME;

#define SET_MESSAGE_AND_GOTO(_goto_label, _message_variable, _message) \
        do                                                             \
        {                                                              \
                (_message_variable) = (_message);                      \
                goto _goto_label;                                      \
        } while (0)

/* INLINE HELPERS for `perform_set()`. */
static __always_inline char *receive_file_name(microtcp_sock_t *_socket, size_t _file_name_size);

/* Other INLINE HELPERS. */
static __always_inline void miniredis_request_distributor(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);
static __always_inline status_t send_server_response(microtcp_sock_t *_socket, miniredis_header_t *_response_header, const char *_response_message);

/* Static Functions: */
static status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_client_address, struct sockaddr_in *_server_address);
static status_t miniredis_server_manager(registry_t *_registry);
static void miniredis_request_handler(microtcp_sock_t *_socket, registry_t *_registry);
static void move_into_registry_directory(void);

/* Command functions: */
static void execute_set(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);
static void execute_get(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);
static void execute_del(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MAIN() <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/


int main(void)
{
        display_startup_message(STARTUP_SERVER_LOGO);
        display_current_path();
        prompt_to_configure_microtcp();
        set_max_response_idle_time(SERVER_MAX_IDLE_TIME_MULTIPLIER);
        create_directory(DIRECTORY_NAME_FOR_REGISTRY);
        move_into_registry_directory();
        registry_t *mr_registry = registry_create(REGISTRY_INITIAL_ENTRIES_CAPACITY, REGISTRY_CACHE_SIZE_LIMIT);
        if (mr_registry == NULL)
                LOG_APP_ERROR_RETURN(EXIT_FAILURE, "Registry creation failed, miniredis_server EXIT_FAILURE.");
        miniredis_server_manager(mr_registry);
        registry_destroy(&mr_registry);
        return EXIT_SUCCESS;
}

static __always_inline status_t send_server_response(microtcp_sock_t *const _socket, miniredis_header_t *const _response_header, const char *const _response_message)
{
        const size_t response_message_size = (_response_message == NULL) ? 0 : strlen(_response_message);
        DEBUG_SMART_ASSERT(response_message_size < SSIZE_MAX);
        _response_header->response_message_size = response_message_size;
        if (microtcp_send(_socket, _response_header, sizeof(*_response_header), 0) != sizeof(*_response_header))
                LOG_APP_ERROR_RETURN(FAILURE, "Server failed sending `_response_header`, sending response failed.");
        if (response_message_size != 0)
                if (microtcp_send(_socket, _response_message, response_message_size, 0) != (ssize_t)response_message_size)
                        LOG_APP_ERROR_RETURN(FAILURE, "Server failed sending `_response_message`, sending response failed.");
        LOG_APP_INFO_RETURN(SUCCESS, "Server successfully sent its response");
}

static status_t miniredis_establish_connection(microtcp_sock_t *const _utcp_socket, struct sockaddr_in *const _client_address,
                                               struct sockaddr_in *const _server_address)
{
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_bind(_utcp_socket, (struct sockaddr *)_server_address, sizeof(*_server_address)) == MICROTCP_BIND_FAILURE)
                return FAILURE;
        if (microtcp_accept(_utcp_socket, (struct sockaddr *)_client_address, sizeof(*_client_address)) == MICROTCP_ACCEPT_FAILURE)
                return FAILURE;
        return SUCCESS;
}

static void execute_set(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_SET,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE);
        uint8_t *message_buffer = NULL; /* Requires deallocation. */
        FILE *file_ptr = NULL;          /* Requires cleanup. */
        char *file_name = NULL;         /* Requires deallocation. */
        char *file_name_base = NULL;    /* Does not require deallocation (part of file_name). */
        char *response_message = NULL;  /* Does not require deallocation (stores string literals). */
        const size_t message_buffer_size = get_microtcp_bytestream_rrb_size();

        if ((file_name = receive_file_name(_socket, _request_header->file_name_size)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Filename reception failed.");
        file_name_base = basename(file_name); /* In case received `file_name` contains paths, only the basename is kept. */
        if ((file_ptr = open_file_for_binary_io(STAGING_FILE_NAME, IO_WRITE)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        if ((message_buffer = allocate_message_buffer(message_buffer_size)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        if (receive_file(_socket, message_buffer, message_buffer_size, file_ptr, _request_header->file_size, file_name_base) == FAILURE)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "File reception failed.");
        if (finalize_file(&file_ptr, STAGING_FILE_NAME, file_name_base) == FAILURE)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        if (registry_append(_registry, file_name_base) == FAILURE)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        _request_header->response_status = SUCCESS; /* Update operation status (to end back to client).*/
        LOG_APP_INFO("Server stored and registered `%s`.", file_name_base);

cleanup_label:
        cleanup_file_receiving_resources(&file_ptr, &message_buffer, &file_name, &((miniredis_header_t *){NULL}));
        send_server_response(_socket, _request_header, response_message);
}

static void execute_get(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_GET,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE,
                           _request_header->file_size == 0);
        uint8_t *message_buffer = NULL; /* Requires deallocation. */
        FILE *file_ptr = NULL;          /* Requires deallocation. */
        char *file_name = NULL;         /* Requires deallocation. */
        char *response_message = NULL;  /* Does not require deallocation (stores string literals). */
        const size_t message_buffer_size = _socket->peer_win_size;

        if ((file_name = receive_file_name(_socket, _request_header->file_name_size)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Filename reception failed.");
        registry_node_t *registry_node = registry_find(_registry, file_name);
        if (registry_node == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "File not found.");
        if ((file_ptr = open_file_for_binary_io(file_name, IO_READ)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        if ((message_buffer = allocate_message_buffer(message_buffer_size)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Internal server error.");
        /* Modify request header, to send it back as response. */

        _request_header->file_size = registry_node_file_size(registry_node);
        registry_node_increment_get_count(registry_node);
        _request_header->response_status = SUCCESS;

        if (send_request_header(_socket, _request_header) == FAILURE)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed sending request_header back to client.");
        if (send_file(_socket, message_buffer, message_buffer_size, file_ptr, file_name) == FAILURE)
                goto cleanup_label;

cleanup_label:
        cleanup_file_sending_resources(&file_ptr, &message_buffer, &file_name, &(miniredis_header_t *){NULL});
        if (_request_header->response_status == FAILURE)
                send_server_response(_socket, _request_header, response_message);
}

static void execute_del(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_DEL,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE,
                           _request_header->file_size == 0);
        char *file_name = NULL;        /* Requires deallocation. */
        char *response_message = NULL; /* Does not require deallocation (stores string literals). */
        if ((file_name = receive_file_name(_socket, _request_header->file_name_size)) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Filename reception failed.");
        if (registry_find(_registry, file_name) == NULL)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "File not found.");
        if (registry_pop(_registry, file_name) == FAILURE)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Failed deleting file from registry.");
        if (remove(file_name) != 0)
                SET_MESSAGE_AND_GOTO(cleanup_label, response_message, "Failed deleting file from storage (internal error).");
        _request_header->response_status = SUCCESS;

cleanup_label:
        if (file_name != NULL)
                FREE_NULLIFY_LOG(file_name);
        send_server_response(_socket, _request_header, response_message);
}

static size_t registry_serialized_size(const registry_t *const _registry)
{
        DEBUG_SMART_ASSERT(_registry != NULL);
        /* To avoid dereference in for-loop. */
        const size_t registry_size = _registry->size;
        const registry_node_t *registry_node_array = _registry->node_array;

        size_t serialized_registry_size = registry_size * sizeof(struct registry_node_serialization_info);
        for (size_t i = 0; i < registry_size; i++)
                serialized_registry_size += strlen(registry_node_array[i].file_name);
        return serialized_registry_size;
}

static status_t send_registry_serialized_description(microtcp_sock_t *const _socket, const registry_t *const _registry,
                                                     uint8_t *const _message_buffer, const size_t _message_buffer_size)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _registry != NULL, _message_buffer != NULL);
        const size_t registry_size = _registry->size;
        const registry_node_t *registry_node_array = _registry->node_array;
        size_t message_buffer_copied_bytes = 0;
        for (size_t i = 0; i < registry_size; i++)
        {
                const registry_node_t current_node = registry_node_array[i];
                const struct registry_node_serialization_info rnsi = {
                    .file_name_size = strlen(current_node.file_name),
                    .file_size = current_node.file_size,
                    .time_of_arrival = current_node.time_of_arrival,
                    .download_counter = current_node.download_counter};
                const size_t serialized_entry_size = sizeof(rnsi) + rnsi.file_name_size;
                if (message_buffer_copied_bytes + serialized_entry_size > _message_buffer_size)
                {
                        if (microtcp_send(_socket, _message_buffer, message_buffer_copied_bytes, 0) != (ssize_t)message_buffer_copied_bytes)
                                return FAILURE;
                        message_buffer_copied_bytes = 0;
                }

                memcpy(_message_buffer + message_buffer_copied_bytes, &rnsi, sizeof(rnsi));
                message_buffer_copied_bytes += sizeof(rnsi);
                memcpy(_message_buffer + message_buffer_copied_bytes, current_node.file_name, rnsi.file_name_size);
                message_buffer_copied_bytes += rnsi.file_name_size;
        }
        if (message_buffer_copied_bytes != 0)
                if (microtcp_send(_socket, _message_buffer, message_buffer_copied_bytes, 0) != (ssize_t)message_buffer_copied_bytes)
                        return FAILURE;
        return SUCCESS;
}

static void execute_list(microtcp_sock_t *const _socket, const registry_t *const _registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_LIST,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size == 0,
                           _request_header->file_size == 0);
        uint8_t *message_buffer = NULL; /* Requires deallocation. */
        const size_t message_buffer_size = _socket->peer_win_size;
        message_buffer = allocate_message_buffer(message_buffer_size);
        _request_header->response_message_size = registry_serialized_size(_registry);
        _request_header->response_status = SUCCESS;
        if (send_request_header(_socket, _request_header) == FAILURE)
                goto cleanup_label;
        if (send_registry_serialized_description(_socket, _registry, message_buffer, message_buffer_size) == FAILURE)
                goto cleanup_label;
cleanup_label:
        if (message_buffer != NULL)
                FREE_NULLIFY_LOG(message_buffer);
}

static _Atomic _Bool server_safe_termination_flag = false;

static void miniredis_request_handler(microtcp_sock_t *const _socket, registry_t *const _registry)
{
        miniredis_header_t request_header = {0}; /* Zeroing is not really requires, but anyway... */
        while (server_safe_termination_flag == false)
        {
                ssize_t recv_ret_val = microtcp_recv(_socket, &request_header, sizeof(request_header), 0); /* 0 as flag argument, for DEFAULT recv() mode. */
                if (recv_ret_val == MICROTCP_RECV_TIMEOUT)
                {
                        LOG_APP_INFO("Waiting request.");
                        continue;
                }
                if (recv_ret_val == MICROTCP_RECV_FAILURE)
                {
                        if (RARE_CASE(_socket->state != CLOSING_BY_PEER))
                                LOG_APP_ERROR("microtcp_recv() returned failure code, without setting socket to CLOSING_BY_PEER.");
                        return;
                }

                miniredis_request_distributor(_socket, _registry, &request_header);
        }
}

void *miniredis_connection_handler(void *_registry)
{
        LOG_APP_INFO("Connection handler activated, and running.");
        struct sockaddr_in server_address = {
            .sin_family = AF_INET,
            .sin_port = request_server_port(),
            .sin_addr = request_server_ipv4()};  /* Required for bind(), after that variable can be safely destroyed. */
        struct sockaddr_in client_address = {0}; /* Acquired by microtcp_accept() internally (by recvfrom()). */
        microtcp_sock_t utcp_socket = {0};
        while (true)
        {
                if (server_safe_termination_flag == true)
                        return (void *)SUCCESS;
                if (miniredis_establish_connection(&utcp_socket, &client_address, &server_address) == FAILURE)
                        LOG_APP_ERROR_RETURN((void *)FAILURE, "Failed establishing connection.");
                miniredis_request_handler(&utcp_socket, (registry_t *)_registry);
                if (miniredis_terminate_connection(&utcp_socket) == FAILURE)
                        LOG_APP_ERROR_RETURN((void *)FAILURE, "Failed terminating connection.");
        }
}

static void signal_handler(__attribute__((unused)) int _sig)
{
        server_safe_termination_flag = true;
        LOG_APP_WARNING("Server safe termination flag raised.");
}

static status_t miniredis_server_manager(registry_t *const _registry)
{
        pthread_t connection_handler_pid;
        pthread_create(&connection_handler_pid, NULL, &miniredis_connection_handler, _registry);

        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGINT, &sa, NULL);
        void *thread_ret_val;

        pthread_join(connection_handler_pid, &thread_ret_val);
        return (_Bool)thread_ret_val;
}

/* INLINE HELPERS: */
static __always_inline char *receive_file_name(microtcp_sock_t *const _socket, const size_t _file_name_size)
{
        DEBUG_SMART_ASSERT(_file_name_size < SSIZE_MAX);
        char *file_name = MALLOC_LOG(file_name, _file_name_size + 1);
        if (file_name == NULL)
                return NULL;

        /* Read filename. */
        if (microtcp_recv_timed(_socket, file_name, _file_name_size, max_response_idle_time) != (ssize_t)_file_name_size)
        {
                FREE_NULLIFY_LOG(file_name);
                return NULL;
        }

        file_name[_file_name_size] = '\0'; /* Append '\0'. */
        return file_name;
}

static __always_inline void miniredis_request_distributor(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        LOG_APP_INFO("Received request: %s", get_command_name(_request_header->command_code));
        /* Determine which command was received */
        switch (_request_header->command_code)
        {
        case CMND_CODE_SET:
                execute_set(_socket, _registry, _request_header);
                break;
        case CMND_CODE_GET:
                execute_get(_socket, _registry, _request_header);
                break;
        case CMND_CODE_DEL:
                execute_del(_socket, _registry, _request_header);
                break;
        case CMND_CODE_LIST:
                execute_list(_socket, _registry, _request_header);
                break;
        }
}

static void move_into_registry_directory(void)
{
        struct stat st;
        if (stat(DIRECTORY_NAME_FOR_REGISTRY, &st) != 0 || !S_ISDIR(st.st_mode))
                LOG_APP_ERROR("Cannot move into `%s`: Directory does not exist or is not a valid directory.", DIRECTORY_NAME_FOR_REGISTRY);
        else if (chdir(DIRECTORY_NAME_FOR_REGISTRY) == 0)
                LOG_APP_INFO("Successfully moved into directory `%s`.", DIRECTORY_NAME_FOR_REGISTRY);
        else
                LOG_APP_ERROR("Failed to move into directory `%s`, errno(%d): %s.", DIRECTORY_NAME_FOR_REGISTRY, errno, strerror(errno));
}
