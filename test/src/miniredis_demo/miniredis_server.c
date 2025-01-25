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

static inline void perform_set(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header)
{
        printf("PERFORME SET\n");
        printf("PERFORME SET\n");
        DEBUG_SMART_ASSERT(_request_header->command_code == CMND_SET_CODE,
                           _request_header->operation_status == SUCCESS,
                           _request_header->message_size == 0,
                           _request_header->filename_size <= MAX_COMMAND_ARGUMENT_SIZE);
        uint8_t *file_buffer = NULL; /* Initiliazing first thing, so that free() in cleanup: wont lead to undefined. */
        FILE *file_ptr = NULL;       /* Same here. */

        char file_name_buffer[MAX_COMMAND_ARGUMENT_SIZE + 1]; /* +1 for '\0'. */
        /* Read filename. */
        if (microtcp_recv(_socket, file_name_buffer, _request_header->filename_size, MSG_WAITALL) != (ssize_t) _request_header->filename_size)
                LOG_APP_ERROR_GOTO(perform_set_cleanup, "Failed receiving filename.");
        file_name_buffer[_request_header->filename_size] = '\0'; /* Append '\0'. */

        file_ptr = fopen(file_name_buffer, "wb");
        if (file_ptr == NULL)
                LOG_APP_ERROR_GOTO(perform_set_cleanup, "File: %s failed write-open, errno(%d): %s.", file_name_buffer, errno, strerror(errno));

        file_buffer = MALLOC_LOG(file_buffer, MAX_FILE_CHUNK);
        if (file_buffer == NULL)
                LOG_APP_ERROR_GOTO(perform_set_cleanup, "Failed allocating memory for writing file.");

        size_t total_written_bytes = 0;
        while (total_written_bytes != _request_header->file_size)
        {
                const size_t remaining_file_bytes = _request_header->file_size - total_written_bytes;
                const size_t required_reception_size = MIN(remaining_file_bytes, MAX_FILE_CHUNK);
                const ssize_t received_bytes = microtcp_recv(_socket, file_buffer, required_reception_size, MSG_WAITALL);
                if (received_bytes == MICROTCP_RECV_FAILURE)
                        LOG_APP_ERROR_GOTO(perform_set_cleanup, "Failed receiving file-chunk.");
                const size_t written_bytes = fwrite(file_buffer, 1, received_bytes, file_ptr);
                if (written_bytes == 0)
                        LOG_APP_ERROR_GOTO(perform_set_cleanup, "File: %s failed writing file-chunk, errno(%d): %s.", file_name_buffer, errno, strerror(errno));
                total_written_bytes += written_bytes;
                LOG_APP_INFO("File: %s, (%zu/%zu bytes received)", file_name_buffer, total_written_bytes, _request_header->file_size);
        }
        registry_append(_registry, file_name_buffer);


perform_set_cleanup:
        if (file_ptr != NULL)
                fclose(file_ptr);
        free(file_buffer);
}

static __always_inline void miniredis_request_executor(microtcp_sock_t *_socket, registry_t *_registry, miniredis_header_t *_request_header)
{
        printf("EXECUTOR GOT REQUEST: |???|\n");

        /* Determine which command was received */
        switch (_request_header->command_code)
        {
        case CMND_SET_CODE:
                perform_set(_socket, _registry, _request_header);
                break;
        }
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

                miniredis_request_executor(_socket, _registry, (miniredis_header_t *)request_header_buffer);
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
