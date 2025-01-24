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
#include "mini_redis_demo/registry.h"

#define MAX_COMMAND_SIZE 20
#define MAX_COMMAND_ARGUMENT_SIZE 400
#define MAX_MINI_REDIS_REQUEST_SIZE (MAX_COMMAND_SIZE + (2 * MAX_COMMAND_ARGUMENT_SIZE))

typedef enum
{
        HELP = 0,  // No arguments
        GET = 2,   // Requires 2 arguments: filename, saving-path
        SET = 3,   // Requires 2 arguments: filename, path-to-file
        DEL = 1,   // Requires 1 argument: filename
        CACHE = 1, // Requires 1 argument: filename
        LIST = 0,  // No arguments
        INFO = 1,  // Requires 1 argument: filename
        SIZE = 1   // Requires 1 argument: filename
} mini_redis_commands_argument_count_t;

static const char *get_available_commands(void)
{
        static const char available_commands_menu[] =
            "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n"
            "┃      Available commands                  ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->HELP                                    ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->GET <filename> <filesize> <saving-path> ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->SET <filename> <path-to-file>           ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->DEL <filename>                          ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->CACHE <filename>                        ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->LIST                                    ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->INFO <filename>                         ┃\n"
            "┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\n"
            "┃->SIZE <filename>                         ┃\n"
            "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n";
        return available_commands_menu;
}

static inline void display_available_commands(void)
{
        printf("%s", get_available_commands());
}

static inline void prompt_to_configure_microtcp(void)
{
        const char *const prompt = "Would you like to configure MicroTCP [y/N]? ";
        char *answer_buffer = NULL;
        const char default_answer = 'N';
        while (true)
        {
                PROMPT_WITH_YES_NO(prompt, default_answer, answer_buffer);
                to_lowercase(answer_buffer);
                if (strcmp(answer_buffer, "yes") == 0 || strcmp(answer_buffer, "y") == 0)
                {
                        configure_microtcp_settings();
                        break;
                }
                if (strcmp(answer_buffer, "no") == 0 || strcmp(answer_buffer, "n") == 0)
                        break;
                clear_line();
        }
        free(answer_buffer);
}

static inline status_t mini_redis_establish_connection(microtcp_sock_t *_utcp_socket, struct sockaddr_in *_client_address)
{
        struct sockaddr_in server_address = {.sin_family = AF_INET,
                                             .sin_addr = request_server_ipv4(),
                                             .sin_port = request_server_port()}; /* Required for bind(), after that variable can be safely destroyed. */
        (*_utcp_socket) = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_utcp_socket->state == INVALID)
                return FAILURE;
        if (microtcp_bind(_utcp_socket, (const struct sockaddr *)&server_address, sizeof(server_address)) == MICROTCP_BIND_FAILURE)
                return FAILURE;
        if (microtcp_accept(_utcp_socket, (struct sockaddr *)_client_address, sizeof(*_client_address)) == MICROTCP_ACCEPT_FAILURE)
                return FAILURE;
        return SUCCESS;
}

static status_t mini_redis_terminate_connection(microtcp_sock_t *_utcp_socket)
{
        if (microtcp_shutdown(_utcp_socket, SHUT_RDWR) == MICROTCP_SHUTDOWN_FAILURE)
                return FAILURE;
        microtcp_close(_utcp_socket);
        return SUCCESS;
}

static __always_inline void respond_to_help(microtcp_sock_t *_socket)
{
        microtcp_send(_socket, get_available_commands(), 1772, 0);
}

static __always_inline void mini_redis_request_executor(microtcp_sock_t *_socket, const char *const _request)
{
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
        if (strcmp(command_buffer, STRINGIFY(HELP)) == 0 && args_parsed == HELP)
                respond_to_help(_socket);
        else if (strcmp(command_buffer, STRINGIFY(GET)) == 0 && args_parsed == GET)
                ;
        else if (strcmp(command_buffer, STRINGIFY(SET)) == 0 && args_parsed == SET)
                ;
        else if (strcmp(command_buffer, STRINGIFY(DEL)) == 0 && args_parsed == DEL)
                ;
        else if (strcmp(command_buffer, STRINGIFY(CACHE)) == 0 && args_parsed == CACHE)
                ;
        else if (strcmp(command_buffer, STRINGIFY(LIST)) == 0 && args_parsed == LIST)
                ;
        else if (strcmp(command_buffer, STRINGIFY(INFO)) == 0 && args_parsed == INFO)
                ;
        else if (strcmp(command_buffer, STRINGIFY(SIZE)) == 0 && args_parsed == SIZE)
                ;
        else
                ;
        // send_error_response("Unknown or malformed command_buffer"); // TODO:
}

static inline void mini_redis_request_handlder(microtcp_sock_t *_socket)
{
        char request_buffer[MAX_MINI_REDIS_REQUEST_SIZE];
        _Static_assert(MICROTCP_RECV_TIMEOUT == 0, "Unexpected return value(" STRINGIFY_EXPANDED(MICROTCP_RECV_TIMEOUT) ") for timeout in microtcp_recv().");
        _Static_assert(MICROTCP_RECV_FAILURE == -1, "Unexpected return value(" STRINGIFY_EXPANDED(MICROTCP_RECV_FAILURE) ") for failure in microtcp_recv().");

        while (true)
        {
                printf("BEFORE SOCKET STATE== %d\n", _socket->state);
                ssize_t recv_ret_val = microtcp_recv(_socket, request_buffer, MAX_MINI_REDIS_REQUEST_SIZE, 0); /* 0 as flag argument, for DEFAULT recv() mode. */
                printf("AFTER SOCKET STATE== %d\n", _socket->state);
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

                mini_redis_request_executor(_socket, request_buffer);
        }
}

static void mini_redis_server_manager(registry_t *_registry)
{
        struct sockaddr_in client_address = {0}; /* Acquired by microtcp_accept() internally (by recvfrom()). */
        microtcp_sock_t utcp_socket = {0};

        if (mini_redis_establish_connection(&utcp_socket, &client_address) == FAILURE)
                LOG_ERROR("Failed establishing connection.");

        mini_redis_request_handlder(&utcp_socket);

        if (mini_redis_terminate_connection(&utcp_socket) == FAILURE)
                LOG_ERROR("Failed terminating connection.");
}

#define REGISTRY_INITIAL_CAPACITY (100)
#define REGISTRY_CACHE_SIZE_LIMIT (500000)
int main(void)
{
        display_startup_message("Welcome to MINI-REDIS file server.");
        prompt_to_configure_microtcp();
        registry_t *mr_registry = registry_create(REGISTRY_INITIAL_CAPACITY, REGISTRY_CACHE_SIZE_LIMIT);
        if (mr_registry == NULL)
                LOG_ERROR_RETURN(EXIT_FAILURE, "Registry creation failed, mini_redis_server EXIT_FAILURE.");
        mini_redis_server_manager(mr_registry);
        registry_destroy(&mr_registry);
        return EXIT_SUCCESS;
}

#undef REGISTRY_INITIAL_CAPACITY
#undef REGISTRY_CACHE_SIZE_LIMIT
