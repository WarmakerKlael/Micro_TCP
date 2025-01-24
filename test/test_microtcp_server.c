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
#include <stdbool.h>

static inline void print_welcome_message(void)
{
    CLEAR_SCREEN();
    printf("==== Server-Side MicroTCP Demo #1 ====\n");
}

static inline uint16_t request_listening_port(void)
{
    long port_number = -1; /* Default invalid port. */
    const char *prompt = "Enter server's listening port number: ";
    while (true)
    {
        PROMPT_WITH_READLINE(prompt, "%ld", &port_number);
        if (port_number >= 0 && port_number <= UINT16_MAX)
            return (uint16_t)port_number;
        clear_line();
    }
}

static inline struct in_addr request_listening_ipv4(void)
{
#define INET_PTON_SUCCESS (1)
    char *ip_line = NULL;
    struct in_addr ipv4_address;
    const char *prompt = "Enter server's listening IPv4 address (ANY = 0): ";
    while (true)
    {
        PROMPT_WITH_READ_STRING(prompt, ip_line);
        if (inet_pton(AF_INET, ip_line, &ipv4_address) == INET_PTON_SUCCESS)
            break;
        clear_line();
    }
    free(ip_line);
    return ipv4_address;
#undef INET_PTON_SUCCESS
}



int main(void)
{
    print_welcome_message();
    // configure_microtcp_settings();

    struct sockaddr_in server_listening_address = {.sin_family = AF_INET,
                                                   .sin_addr = request_listening_ipv4(),
                                                   .sin_port = htons(request_listening_port())};
    struct sockaddr_in client_address; /* Acquired by microtcp_accept() internally (by recvfrom()). */
    microtcp_sock_t utcp_socket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    microtcp_bind(&utcp_socket, (const struct sockaddr *)&server_listening_address, sizeof(server_listening_address));
    microtcp_accept(&utcp_socket, (struct sockaddr *)&client_address, sizeof(client_address));
    char ff[2000];
    ssize_t bytes = microtcp_recv(&utcp_socket, ff, 2000, 0);
    microtcp_shutdown(&utcp_socket, SHUT_RDWR);
    microtcp_close(&utcp_socket);

    return EXIT_SUCCESS;
}
