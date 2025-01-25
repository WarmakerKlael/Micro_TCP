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
#define STAGING_FILE_NAME ".__filepart__.dat" /* Hiddden, internal filename until stored in `_registry`. */

/* INLINE HELPERS for `perform_set()`. */
static __always_inline char *receive_file_name(microtcp_sock_t *_socket, size_t _file_name_size);
static __always_inline status_t receive_and_write_file_part(microtcp_sock_t *_socket, FILE *_file_ptr, const char *_file_name,
                                                            uint8_t *_file_buffer, size_t _file_part_size);
static __always_inline status_t finalize_file(FILE **_file_ptr_address, const char *_staging_file_name, const char *_export_file_name);
static __always_inline void perform_set_resource_cleanup(FILE **const _file_ptr_address,
                                                         uint8_t **const _file_buffer_address,
                                                         char **const _file_name_address);

/* Other INLINE HELPERS. */
static __always_inline void miniredis_request_distributor(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header);

static inline status_t miniredis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_client_address)
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

static status_t miniredis_terminate_connection(microtcp_sock_t *_utcp_socket)
{
        if (microtcp_shutdown(_utcp_socket, SHUT_RDWR) == MICROTCP_SHUTDOWN_FAILURE)
                return FAILURE;
        microtcp_close(_utcp_socket);
        return SUCCESS;
}

static __always_inline status_t receive_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer,
                                             FILE *const _file_ptr, const size_t _file_size, const char *const _file_name)
{
        size_t written_bytes_count = 0;
        while (written_bytes_count != _file_size)
        {
                const size_t file_part_size = MIN(_file_size - written_bytes_count, MAX_FILE_PART);
                if (receive_and_write_file_part(_socket, _file_ptr, _file_name, _message_buffer, file_part_size) == FAILURE)
                        return FAILURE;
                written_bytes_count += file_part_size;
                LOG_APP_INFO("File: %s, (%zu/%zu bytes received)", _file_name, written_bytes_count, _file_size);
        }
        return SUCCESS;
}

static inline void perform_set(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *const _request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_SET_CODE,
                           _request_header->operation_status == FAILURE,
                           _request_header->message_size == 0,
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
        _request_header->operation_status = SUCCESS; /* Update operation status (to end back to client).*/
        LOG_APP_INFO("Server stored and registered %s.", file_name);

cleanup_label:
        perform_set_resource_cleanup(&file_ptr, &message_buffer, &file_name);
        microtcp_send(_socket, _request_header, sizeof(*_request_header), 0); /* Send operation status response. */
}

static inline void perform_get(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header)
{
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_GET_CODE,
                           _request_header->operation_status == SUCCESS,
                           _request_header->message_size == 0,
                           _request_header->file_name_size <= MAX_COMMAND_ARGUMENT_SIZE);
}

static inline void miniredis_request_handlder(microtcp_sock_t *_socket, registry_t *_registry)
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

static status_t miniredis_server_manager(registry_t *_registry)
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

#define REGISTRY_INITIAL_CAPACITY (100)
#define REGISTRY_CACHE_SIZE_LIMIT (500000)
int main(void)
{
        display_startup_message("Welcome to MINI-REDIS file server.");
        prompt_to_configure_microtcp();
        registry_t *mr_registry = registry_create(REGISTRY_INITIAL_CAPACITY, REGISTRY_CACHE_SIZE_LIMIT);
        if (mr_registry == NULL)
                LOG_APP_ERROR_RETURN(EXIT_FAILURE, "Registry creation failed, miniredis_server EXIT_FAILURE.");
        miniredis_server_manager(mr_registry);
        registry_destroy(&mr_registry);
        return EXIT_SUCCESS;
}

#undef REGISTRY_INITIAL_CAPACITY
#undef REGISTRY_CACHE_SIZE_LIMIT

/* INLINE HELPERS: */
static __always_inline char *receive_file_name(microtcp_sock_t *const _socket, const size_t _file_name_size)
{
        DEBUG_SMART_ASSERT(_file_name_size < SIZE_MAX / 2);
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

static __always_inline status_t receive_and_write_file_part(microtcp_sock_t *const _socket, FILE *const _file_ptr, const char *const _file_name,
                                                            uint8_t *const _file_buffer, const size_t _file_part_size)
{
        const ssize_t received_bytes = microtcp_recv(_socket, _file_buffer, _file_part_size, MSG_WAITALL);
        if (received_bytes == MICROTCP_RECV_FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed receiving file-chunk.");
        if (fwrite(_file_buffer, 1, received_bytes, _file_ptr) == 0)
                LOG_APP_ERROR_RETURN(FAILURE, "File: %s failed writing file-part, errno(%d): %s.", _file_name, errno, strerror(errno));
        if (fflush(_file_ptr) != 0)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed to flush file-part to disk, errno(%d): %s.", errno, strerror(errno));
        return SUCCESS;
}

static __always_inline status_t finalize_file(FILE **const _file_ptr_address, const char *const _staging_file_name, const char *const _export_file_name)
{
        DEBUG_SMART_ASSERT(_file_ptr_address != NULL);
        DEBUG_SMART_ASSERT(*_file_ptr_address != NULL);
        fclose(*_file_ptr_address);
        *_file_ptr_address = NULL;
        if (rename(_staging_file_name, _export_file_name) != 0)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed renaming from %s to %s, errno(%d): %s.",
                                     _staging_file_name, _export_file_name, errno, strerror(errno));
        return SUCCESS;
}

static __always_inline void perform_set_resource_cleanup(FILE **const _file_ptr_address,
                                                         uint8_t **const _file_buffer_address,
                                                         char **const _file_name_address)
{
        if (*_file_ptr_address != NULL)
                fclose(*_file_ptr_address);
        FREE_NULLIFY_LOG(*_file_buffer_address);

        if (access(STAGING_FILE_NAME, F_OK) == 0)
                if (remove(STAGING_FILE_NAME) != 0) /* We remove `staging_file_name` file. */
                        LOG_APP_ERROR("Failed to remove() %s (in cleanup_state), errno(%d): %s.", STAGING_FILE_NAME, errno, strerror(errno));
        FREE_NULLIFY_LOG(*_file_name_address);
}

static __always_inline void miniredis_request_distributor(microtcp_sock_t *const _socket, registry_t *const _registry, miniredis_header_t *const _request_header)
{
        /* Determine which command was received */
        switch (_request_header->command_code)
        {
        case CMND_GET_CODE:
                perform_get(_socket, _registry, _request_header);
                break;

        case CMND_SET_CODE:
                perform_set(_socket, _registry, _request_header);
                break;
        }
}
