#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "microtcp.h"

#define PORT 54321

int
main(int argc, char **argv)
{
    struct sockaddr_in servaddr;

    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Attemting to connect...\n");
    int connect_ret_val = microtcp_connect(&tcpsocket, (const struct sockaddr*) &servaddr, sizeof(servaddr));
    printf("Connect returned: %d\n", connect_ret_val);


    printf("Closing connection...\n");
    int shutdown_ret_val = microtcp_shutdown(&tcpsocket, SHUT_RDWR);
    printf("Shutdown returned: %d\n", shutdown_ret_val);
    
    return EXIT_SUCCESS;
}
