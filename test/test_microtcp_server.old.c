/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */

/**
 * CS335 - Project Phase A
 *
 * Ioannis Spyropoulos - csd5072
 * Georgios Evangelinos - csd4624
 * Niki Psoma - csd5038
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "microtcp.h"
#include "settings/microtcp_settings_prompts.h"
#include "core/segment_io.h"
#include <stdlib.h>
#include <time.h>

#define PORT 54321
#define TIME_100MS 100000

int getRandomNumber()
{
    // Static pointer, initialized once with a side effect (calling srand)

    // Generate and return the random number
    return 100000 + rand() % (300000 - 100000 + 1);
}

int main(int argc, char **argv)
{
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&clientaddr, 0, sizeof(clientaddr));
    // configure_microtcp_settings();

    srand(time(NULL));
    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    printf("Binding...\n");
    microtcp_bind(&tcpsocket, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("Bind successful\n");

    printf("Waiting for connection request...\n");
    microtcp_accept(&tcpsocket, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
    printf("Server Connected\n");

#define ARRAY_SIZE 150000

    char chars[ARRAY_SIZE] = {0};
    microtcp_recv(&tcpsocket, chars, ARRAY_SIZE, MSG_WAITALL);

    for (int i = 0; i < ARRAY_SIZE; i++)
        printf("%d\t%c\n", i, chars[i]);

    char *array = malloc(ARRAY_SIZE);
    for (int i = 0; i < ARRAY_SIZE; i++) /* DOUBLE OF `i` */
        array[i] = 'A' + (i % ('Z' - 'A' + 1));

        array[70000] = '0';

    for (int i = 0; i < ARRAY_SIZE; i++)
        if (array[i] != chars[i])
            printf("(array[%d] = %c) != (chars[%d] = %c)\n", i, array[i], i, chars[i]);

    // while (TRUE)
    // {
    // int sleep_time_us = getRandomNumber();
    // printf("Gonna Sleep for: %d\n", sleep_time_us);
    // // usleep(sleep_time_us);
    // send_ack_control_segment(&tcpsocket, tcpsocket.peer_address, sizeof(*tcpsocket.peer_address));
    // }
    // const char* rmsg = "Message received";

    // char buff[1024] = {0};
    // do
    // {
    //     microtcp_recv(&tcpsocket, buff, 1024, NO_FLAGS_BITS);
    //     if (tcpsocket.state == ESTABLISHED)
    //     {
    //         printf("From client: %s", buff);
    //         microtcp_send(&tcpsocket, rmsg, 17, NO_FLAGS_BITS);
    //     }
    // } while (tcpsocket.cliaddr != NULL);

    // printf("Connection closed by peer\n");

    // char ff[2000];
    // ssize_t bytes = microtcp_recv(&tcpsocket, ff, 2000, 0);
    // printf("recv finished, bytes read = %zd\n", bytes);
    // if (bytes == 0)
    microtcp_shutdown(&tcpsocket, SHUT_RDWR);
    // printf("ANOTHER HERE\n");
    return EXIT_SUCCESS;
}
