#ifndef CORE_RECEIVER_THREAD_H
#define CORE_RECEIVER_THREAD_H

#include "microtcp.h"

void *microtcp_receiver_thread(void *_args);

struct microtcp_receiver_thread_args
{
        microtcp_sock_t *_socket;
};


#endif /* CORE_RECEIVER_THREAD_H */