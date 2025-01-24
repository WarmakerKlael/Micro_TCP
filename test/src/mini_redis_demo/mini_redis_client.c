#include "microtcp.h"
#include "demo_common.h"

int main(void)
{
        display_startup_message("CLIENT SIDE MINI REDIS");
        struct sockaddr_in servaddr = {.sin_family = AF_INET,
                                       .sin_addr = request_server_ipv4(),
                                       .sin_port = request_server_port()};

        microtcp_sock_t utcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int connect_ret_val = microtcp_connect(&utcpsocket, (const struct sockaddr *)&servaddr, sizeof(servaddr));
        if (connect_ret_val == MICROTCP_CONNECT_FAILURE)
        {
                printf("Demo1: Connection with server failed.\n");
                return EXIT_FAILURE;
        }

#include <unistd.h>

        microtcp_send(&utcpsocket, "HELP", 5, 0);

        int shutdown_ret_val = microtcp_shutdown(&utcpsocket, SHUT_RDWR);
        if (shutdown_ret_val == MICROTCP_SHUTDOWN_FAILURE)
        {
                printf("Demo1: Connection shutdown failed.\n");
                return EXIT_FAILURE;
        }
}
