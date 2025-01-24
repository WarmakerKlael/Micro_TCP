#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "microtcp.h"
#include "microtcp_prompt_util.h"
#include "settings/microtcp_settings_prompts.h"
#include "demo_common.h"

static inline void write_file(const uint8_t *const input_buffer, size_t buffer_size)
{
    FILE *outfile = fopen("received_file.txt", "wb");
    fwrite(input_buffer, 1, buffer_size, outfile);
    fclose(outfile);
}

int main(void)
{
    // configure_microtcp_settings();

    struct sockaddr_in server_listening_address = {.sin_family = AF_INET,
                                                   .sin_addr = request_server_ipv4(),
                                                   .sin_port = request_server_port()};
    struct sockaddr_in client_address; /* Acquired by microtcp_accept() internally (by recvfrom()). */
    microtcp_sock_t utcp_socket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    microtcp_bind(&utcp_socket, (const struct sockaddr *)&server_listening_address, sizeof(server_listening_address));
    microtcp_accept(&utcp_socket, (struct sockaddr *)&client_address, sizeof(client_address));
#define FILE_BUFFER_SIZE 1000000 /* 1MB */
    char file_buffer[FILE_BUFFER_SIZE]; 
    ssize_t bytes = microtcp_recv(&utcp_socket, file_buffer, FILE_BUFFER_SIZE, MSG_WAITALL);
    write_file((uint8_t *)file_buffer, bytes);
    microtcp_shutdown(&utcp_socket, SHUT_RDWR);
    microtcp_close(&utcp_socket);

    return EXIT_SUCCESS;
}
