#include "lib/microtcp.h"
#include "logging/logger.h"

int main(int argc, char **argv)
{
        microtcp_sock_t new_micro_socket = microtcp_socket(2, 2, 17);
        struct sockaddr ss;
        int microtcp_bind_result = microtcp_bind(&new_micro_socket, &ss, sizeof(ss));
}
