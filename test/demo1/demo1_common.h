#ifndef DEMO1_COMMON_H
#define DEMO1_COMMON_H

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

static inline void print_welcome_message(void)
{
        CLEAR_SCREEN();
        printf("==== Server-Side MicroTCP Demo #1 ====\n");
}

/**
 * @note Already converted to network byte-order. (htons())
 */
static inline uint16_t request_listening_port(void)
{
        long port_number = -1; /* Default invalid port. */
        const char *prompt = "Enter server's listening port number: ";
        while (TRUE)
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &port_number);
                if (port_number >= 0 && port_number <= UINT16_MAX)
                        return htons((uint16_t)port_number);
                clear_line();
        }
}

static inline struct in_addr request_listening_ipv4(void)
{
#define INET_PTON_SUCCESS (1)
        char *ip_line = NULL;
        struct in_addr ipv4_address;
        const char *prompt = "Enter server's listening IPv4 address (ANY = 0): ";
        while (TRUE)
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

#endif /* DEMO1_COMMON_H */
