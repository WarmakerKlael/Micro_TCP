#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "microtcp.h"

#define PORT 50503

int main(void)
{
    // configure_microtcp_settings();

    struct sockaddr_in servaddr;

    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Attemting to connect...\n");
    microtcp_connect(&tcpsocket, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("Client Connected\n");

#define ARRAY_SIZE 500000
    size_t *array = malloc(ARRAY_SIZE * sizeof(size_t));
    for (int i = 0; i < ARRAY_SIZE; i++) /* DOUBLE OF `i` */
        array[i] = 2 * i;

    microtcp_send(&tcpsocket, array, ARRAY_SIZE * sizeof(size_t), 0);

    printf("Closing connection...\n");
    microtcp_shutdown(&tcpsocket, SHUT_RDWR);
    printf("Connection closed\n");

    free(array);
    return EXIT_SUCCESS;
}
