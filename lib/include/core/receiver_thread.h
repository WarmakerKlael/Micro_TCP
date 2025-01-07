#ifndef CORE_RECEIVER_THREAD_H
#define CORE_RECEIVER_THREAD_H

#include "microtcp.h"

void *receiver_thread(void *_args);

typedef enum
{
        FATAL_ERROR,
        RECEIVED_RST,
        RECEIVED_FINACK
} receiver_return_codes_t;

typedef struct receiver_data
{
        microtcp_sock_t *_socket;
        receiver_return_codes_t return_code;
} receiver_data_t;

#endif /* CORE_RECEIVER_THREAD_H */