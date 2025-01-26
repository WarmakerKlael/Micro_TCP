#include <stdio.h>
#include "microtcp.h"
#include "allocator/allocator_macros.h"
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include "microtcp_prompt_util.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings_prompts.h"
#include <errno.h>
#include "demo_common.h"
#include "miniredis_demo/registry.h"
#include "miniredis_demo/miniredis_commands.h"
#include "miniredis_demo/miniredis.h"
#include "common_source_code.h"

/* INLINE HELPERS for `perform_set()`. */
static __always_inline char *receive_file_name(microtcp_sock_t *_socket, size_t _file_name_size);

/* Other INLINE HELPERS. */
static __always_inline void miniredis_request_distributor(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);

/* Static Functions: */
static status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_client_address);
static status_t miniredis_terminate_connection(microtcp_sock_t *_utcp_socket);
static status_t miniredis_server_manager(registry_t *_registry);
static void miniredis_request_handlder(microtcp_sock_t *_socket, registry_t *_registry);

/* Command functions: */
static void execute_set(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);
static void execute_get(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MAIN() <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

int main(void)
{
        display_startup_message(STARTUP_SERVER_LOGO);
        prompt_to_configure_microtcp();
        registry_t *mr_registry = registry_create(REGISTRY_INITIAL_ENTRIES_CAPACITY, REGISTRY_CACHE_SIZE_LIMIT);
        if (mr_registry == NULL)
                LOG_APP_ERROR_RETURN(EXIT_FAILURE, "Registry creation failed, miniredis_server EXIT_FAILURE.");
        miniredis_server_manager(mr_registry);
        registry_destroy(&mr_registry);
        return EXIT_SUCCESS;
}

static status_t miniredis_establish_connection(microtcp_sock_t *const _utcp_socket, struct sockaddr_in *const _client_address)
{
        struct sockaddr_in server_address = {
            .sin_family = AF_INET,
            .sin_port = htons(50503)}; /* Required for bind(), after that variable can be safely destroyed. */
        inet_pton(AF_INET, "0.0.0.0", &server_address.sin_addr);
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_bind(_utcp_socket, (const struct sockaddr *)&server_address, sizeof(server_address)) == MICROTCP_BIND_FAILURE)
                return FAILURE;
        if (microtcp_accept(_utcp_socket, (struct sockaddr *)_client_address, sizeof(*_client_address)) == MICROTCP_ACCEPT_FAILURE)
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

static void execute_set(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_SET,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE);
        uint8_t *message_buffer = NULL; /* Requires deallocation. */
        FILE *file_ptr = NULL;          /* Requires deallocation. */
        char *file_name = NULL;         /* Requires deallocation. */

        if ((file_name = receive_file_name(_socket, _request_header->file_name_size)) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed receiving filename.");
        if ((file_ptr = fopen(STAGING_FILE_NAME, "wb")) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "File: %s failed write-open, errno(%d): %s.", file_name, errno, strerror(errno));
        if ((message_buffer = MALLOC_LOG(message_buffer, MAX_FILE_PART)) == NULL)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed allocating memory for writing file.");
        if (receive_file(_socket, message_buffer, file_ptr, _request_header->file_size, file_name) == FAILURE)
                goto cleanup_label;
        if (finalize_file(&file_ptr, STAGING_FILE_NAME, file_name) == FAILURE)
                goto cleanup_label;
        if (registry_append(_registry, file_name) == FAILURE)
                LOG_APP_ERROR_GOTO(cleanup_label, "Failed appending %s to `registry`.", file_name);
        _request_header->response_status = SUCCESS; /* Update operation status (to end back to client).*/
        LOG_APP_INFO("Server stored and registered `%s`.", file_name);

cleanup_label:
        cleanup_write_file_resources(&file_ptr, &message_buffer, &file_name);
        microtcp_send(_socket, _request_header, sizeof(*_request_header), 0); /* Send operation status response. */
}

static void execute_get(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_CODE_GET,
                           _request_header->response_status == FAILURE, /* We set it to SUCCESS if we can accomplish request. */
                           _request_header->response_message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE);
}

static void miniredis_request_handlder(microtcp_sock_t *const _socket, registry_t *const _registry)
{
        char request_header_buffer[sizeof(miniredis_header_t)];
        while (true)
        {
                ssize_t recv_ret_val = microtcp_recv(_socket, request_header_buffer, sizeof(miniredis_header_t), 0); /* 0 as flag argument, for DEFAULT recv() mode. */
                printf("SERVER RECEIVED %zd BYTES\n", recv_ret_val);
                if (recv_ret_val == MICROTCP_RECV_TIMEOUT)
                {
                        LOG_APP_WARNING("NO_REQUEST->TIMEOUT"); // TODO remove
                        continue;
                }
                if (recv_ret_val == MICROTCP_RECV_FAILURE)
                {
                        if (RARE_CASE(_socket->state != CLOSING_BY_PEER))
                                LOG_APP_ERROR("microtcp_recv() returned failure code, without setting socket to CLOSING_BY_PEER.");
                        return;
                }

                miniredis_request_distributor(_socket, _registry, (miniredis_header_t *)request_header_buffer);
        }
}

static status_t miniredis_server_manager(registry_t *const _registry)
{
        struct sockaddr_in client_address = {0}; /* Acquired by microtcp_accept() internally (by recvfrom()). */
        microtcp_sock_t utcp_socket = {0};

        if (miniredis_establish_connection(&utcp_socket, &client_address) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed establishing connection.");
        miniredis_request_handlder(&utcp_socket, _registry);
        if (miniredis_terminate_connection(&utcp_socket) == FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed terminating connection.");
        return SUCCESS;
}

/* INLINE HELPERS: */
static __always_inline char *receive_file_name(microtcp_sock_t *const _socket, const size_t _file_name_size)
{
        DEBUG_SMART_ASSERT(_file_name_size < ((size_t)-1) >> 1);
        char *file_name = MALLOC_LOG(file_name, _file_name_size + 1);
        if (file_name == NULL)
                return NULL;

        /* Read filename. */
        if (microtcp_recv(_socket, file_name, _file_name_size, MSG_WAITALL) != (ssize_t)_file_name_size)
        {
                FREE_NULLIFY_LOG(file_name);
                return NULL;
        }

        file_name[_file_name_size] = '\0'; /* Append '\0'. */
        return file_name;
}

static __always_inline void miniredis_request_distributor(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        /* Determine which command was received */
        switch (_request_header->command_code)
        {
        case CMND_CODE_GET:
                execute_get(_socket, _registry, _request_header);
                break;

        case CMND_CODE_SET:
                execute_set(_socket, _registry, _request_header);
                break;
        }
}
