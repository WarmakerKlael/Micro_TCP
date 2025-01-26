#ifndef CORE_RECV_IMPL_H
#define CORE_RECV_IMPL_H
#include "microtcp.h"

#define ARE_VALID_MICROTCP_RECV_FLAGS(_flags) ({                                                          \
        /* Conflicting flag check */                                                                      \
        const _Bool waitall = (_Bool)((_flags) & MSG_WAITALL);                                            \
        const _Bool dontwait = (_Bool)((_flags) & MSG_DONTWAIT);                                          \
        if (waitall && dontwait)                                                                          \
                LOG_ERROR("Flags passed to %s are conflicting; Asked to WAITALL and DONTWAIT", __func__); \
        (waitall && dontwait ? FAILURE : SUCCESS);                                                        \
})

ssize_t microtcp_recv_impl(microtcp_sock_t *_socket, uint8_t *_buffer, size_t _length, int _flags);
ssize_t microtcp_recv_timed_impl(microtcp_sock_t *_socket, uint8_t *_buffer, size_t _length, struct timeval _max_idle_time);

#endif /* CORE_RECV_IMPL_H */
