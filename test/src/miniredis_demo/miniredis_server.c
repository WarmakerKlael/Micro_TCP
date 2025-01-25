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

static inline respond_to_set()

static __always_inline void miniredis_request_executor(microtcp_sock_t *_socket, registry_t *_registry, const char *const _request)
{
        printf("EXECUTOR GOT REQUEST: |%s|\n", _request);
        char command_buffer[MAX_COMMAND_SIZE] = {0};
        char argument_buffer1[MAX_COMMAND_ARGUMENT_SIZE] = {0};
        char argument_buffer2[MAX_COMMAND_ARGUMENT_SIZE] = {0};
        // clang-format off
        const char sscanf_format[] = "%" STRINGIFY_EXPANDED(MAX_COMMAND_SIZE) "s "
                                     "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s "
                                     "%" STRINGIFY_EXPANDED(MAX_COMMAND_ARGUMENT_SIZE) "s";
        // clang-format on

        int args_parsed = sscanf(_request, sscanf_format, command_buffer, argument_buffer1, argument_buffer2);

        /* Determine which command was received */
        if (strcmp(command_buffer, CMND_SET_NAME) == 0 && args_parsed == CMND_SET_ARGS)
                respond_to_set();
        else if (strcmp(command_buffer, CMND_GET_NAME) == 0 && args_parsed == CMND_GET_ARGS)
                ;
        else if (strcmp(command_buffer, CMND_DEL_NAME) == 0 && args_parsed == CMND_DEL_ARGS)
                ;
        else if (strcmp(command_buffer, CMND_CACHE_NAME) == 0 && args_parsed == CMND_CACHE_ARGS)
                ;
        else if (strcmp(command_buffer, CMND_LIST_NAME) == 0 && args_parsed == CMND_LIST_ARGS)
                ;
        else if (strcmp(command_buffer, CMND_INFO_NAME) == 0 && args_parsed == CMND_INFO_ARGS)
                ;
        else if (strcmp(command_buffer, CMND_SIZE_NAME) == 0 && args_parsed == CMND_SIZE_ARGS)
                ;
        else
                ;
        // send_error_response("Unknown or malformed command_buffer"); // TODO:
}

static inline void miniredis_request_handlder(microtcp_sock_t *_socket, registry_t *_registry)
{
        char request_buffer[MAX_MINI_REDIS_REQUEST_SIZE];
        while (true)
        {
                ssize_t recv_ret_val = microtcp_recv(_socket, request_buffer, MAX_MINI_REDIS_REQUEST_SIZE, 0); /* 0 as flag argument, for DEFAULT recv() mode. */
                printf("SERVER RECEIVED %zd BYTES\n", recv_ret_val);
                if (recv_ret_val == MICROTCP_RECV_TIMEOUT)
                {
                        LOG_WARNING("NO_REQUEST->TIMEOUT"); // TODO remove
                        continue;
                }
                if (recv_ret_val == MICROTCP_RECV_FAILURE)
                {
                        if (RARE_CASE(_socket->state != CLOSING_BY_PEER))
                                LOG_ERROR("microtcp_recv() returned failure code, without setting socket to CLOSING_BY_PEER.");
                        return;
                }

                miniredis_request_executor(_socket, _registry, request_buffer);
        }
}

static status_t miniredis_server_manager(registry_t *_registry)
{
        struct sockaddr_in client_address = {0}; /* Acquired by microtcp_accept() internally (by recvfrom()). */
        microtcp_sock_t utcp_socket = {0};

        if (miniredis_establish_connection(&utcp_socket, &client_address) == FAILURE)
                LOG_ERROR_RETURN(FAILURE, "Failed establishing connection.");
        miniredis_request_handlder(&utcp_socket, _registry);
        if (miniredis_terminate_connection(&utcp_socket) == FAILURE)
                LOG_ERROR_RETURN(FAILURE, "Failed terminating connection.");
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
                LOG_ERROR_RETURN(EXIT_FAILURE, "Registry creation failed, miniredis_server EXIT_FAILURE.");
        miniredis_server_manager(mr_registry);
        registry_destroy(&mr_registry);
        return EXIT_SUCCESS;
}

#undef REGISTRY_INITIAL_CAPACITY
#undef REGISTRY_CACHE_SIZE_LIMIT
