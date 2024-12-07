#include "microtcp.h"
#include "logging/logger.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h> // For htons() and inet_pton()

int main(int argc, char **argv)
{
        struct sockaddr_in addr_in;
        memset(&addr_in, 0, sizeof(addr_in));               // Zero out the structure
        addr_in.sin_family = AF_INET;                       // IPv4
        addr_in.sin_port = htons(8080);                     // Port number in network byte order
        inet_pton(AF_INET, "127.0.0.1", &addr_in.sin_addr); // Convert IP address to binary form
        microtcp_sock_t new_micro_socket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        // Cast to generic struct sockaddr
        struct sockaddr *generic_addr = (struct sockaddr *)&addr_in;
        microtcp_connect(&new_micro_socket, generic_addr, sizeof(addr_in));


}
