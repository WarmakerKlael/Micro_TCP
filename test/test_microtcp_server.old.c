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

    printf("Binding...\n");
    microtcp_bind(&tcpsocket, (const struct sockaddr*) &servaddr, sizeof(servaddr));
    printf("Bind successful\n");

    printf("Waiting for connection request...\n");
    microtcp_accept(&tcpsocket, (struct sockaddr*) &clientaddr, sizeof(clientaddr));
    printf("Connected\n");

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

    return EXIT_SUCCESS;
}
