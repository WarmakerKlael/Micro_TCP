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
    struct sockaddr_in clientaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&clientaddr, 0, sizeof(clientaddr));

    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    microtcp_bind(&tcpsocket, (const struct sockaddr*) &servaddr, sizeof(servaddr));
    printf("Bind successful\n");

    printf("Waiting for connection request...\n");
    microtcp_accept(&tcpsocket, (struct sockaddr*) &clientaddr, sizeof(clientaddr));
    printf("Connected\n");

    return EXIT_SUCCESS;
}
