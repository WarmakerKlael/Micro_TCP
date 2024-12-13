#include <stdio.h>
#include <sys/time.h>
#include "microtcp_settings.h"
#include <stdarg.h>
#include <string.h>


/* Function to prompt and set ACK timeout */
static void prompt_and_set_ack_timeout()
{
        char *line = NULL;
        size_t line_length = 0;
        struct timeval tv;
        while (1)
        {
                printf("Enter ACK timeout (sec usec, e.g., 0 200000): ");
                getline(&line, &line_length, stdin);
                sscanf(line, " %ldsec %ldusec", &tv.tv_sec, &tv.tv_usec);

        }

        printf("RECEIVED: sec == %ld, usec == %ld, ", tv.tv_sec, tv.tv_usec);
        // set_microtcp_ack_timeout(tv);

        free(line);
}

// Function to prompt and set bytestream assembly buffer length
void prompt_and_set_buffer_length()
{
        // size_t length;
        printf("Enter bytestream assembly buffer length: ");
        // scanf("%zu", &length);
        // set_bytestream_assembly_buffer_len(length);
}

// Function to prompt and set connect FSM RST retries
void prompt_and_set_connect_retries()
{
        // size_t retries;
        printf("Enter connect FSM RST retries count: ");
        // scanf("%zu", &retries, stdin);
        // set_connect_rst_retries(retries);
}

// Function to prompt and set accept FSM SYN-ACK retries
void prompt_and_set_accept_retries()
{
        // size_t retries;
        printf("Enter accept FSM SYN-ACK retries count: ");
        // scanf("%zu", &retries);
        // set_accept_synack_retries(retries);
}

// Function to prompt and set shutdown FSM FIN-ACK retries
void prompt_and_set_shutdown_retries()
{
        // size_t retries;
        printf("Enter shutdown FSM FIN-ACK retries count: ");
        // scanf("%zu", &retries);
        // set_shutdown_finack_retries(retries);
}

// Function to prompt and set shutdown FSM TIME_WAIT period
void prompt_and_set_time_wait_period()
{
        // struct timeval tv;
        printf("Enter shutdown TIME_WAIT period (seconds milliseconds): ");
        // scanf("%ld %ld", &tv.tv_sec, &tv.tv_usec);
        // set_shutdown_time_wait_period(tv);
}

int main()
{
        prompt_and_set_ack_timeout();
}