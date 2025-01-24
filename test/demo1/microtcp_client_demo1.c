#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "demo1_common.h"
#include "microtcp_prompt_util.h"

#include "microtcp.h"
#include "smart_assert.h"

static inline size_t file_size(FILE *file)
{
    SMART_ASSERT(file != NULL);
    fseek(file, 0, SEEK_END);
    size_t filesize_ = ftell(file); /* MAX 2GB. */
    rewind(file);
    return filesize_;
}

static inline const char *request_file_to_send(void)
{
    const char *prompt = "Enter path to file: ";
    char *filepath = NULL;

    while (true)
    {
        PROMPT_WITH_READ_STRING(prompt, filepath);
        FILE *file = fopen(filepath, "rb");
        if (file != NULL)
        {
            fclose(file);
            return filepath;
        }
        clear_line();
    }
}

static inline void send_file(microtcp_sock_t *_socket, const char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    SMART_ASSERT(file != NULL);
    size_t filesize_ = file_size(file);
    uint8_t *file_chunk_buffer = malloc(filesize_);
    fread(file_chunk_buffer, 1, filesize_, file);
    ssize_t send_ret_val = microtcp_send(_socket, file_chunk_buffer, filesize_, 0);
    if (send_ret_val == MICROTCP_SEND_FAILURE)
        printf("Demo1: Sent over connection failed.\n");
    else
        printf("Demo1: Sent %zd bytes.\n", send_ret_val);

    fclose(file);
    free(file_chunk_buffer);
}

int main(void)
{
    struct sockaddr_in servaddr = {.sin_family = AF_INET,
                                   .sin_addr = request_listening_ipv4(),
                                   .sin_port = request_listening_port()};

    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int connect_ret_val = microtcp_connect(&tcpsocket, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (connect_ret_val == MICROTCP_CONNECT_FAILURE)
    {
        printf("Demo1: Connection with server failed.\n");
        return EXIT_FAILURE;
    }

    send_file(&tcpsocket, request_file_to_send());

    int shutdown_ret_val = microtcp_shutdown(&tcpsocket, SHUT_RDWR);
    if (shutdown_ret_val == MICROTCP_SHUTDOWN_FAILURE)
    {
        printf("Demo1: Connection shutdown failed.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
