#ifndef MICROTCP_SETTINGS_H
#define MICROTCP_SETTINGS_H

#include <stddef.h>
#include <time.h>

size_t get_microtcp_ack_timeout_us(void);
void set_microtcp_ack_timeout_usec(size_t _usec);

size_t get_microtcp_recvbuf_len(void);
void set_microtcp_recvbuf_len(size_t _length);


/* Connect()'s FSM configurators. */
size_t get_connect_rst_retries(void);
void set_connect_rst_retries(size_t _retries_count);

/* Accept()'s FSM configurators. */
size_t get_accept_synack_retries(void);
void set_accept_synack_retries(size_t _retries_count);


/* Shutdown()'s FSM configurators. */
size_t get_shutdown_finack_retries(void);
void set_shutdown_finack_retries(size_t _retries_count);
size_t get_shutdown_time_wait_period_sec(void);
size_t get_shutdown_time_wait_period_usec(void);
void set_shutdown_time_wait_period(time_t _sec, time_t _usec);

#endif /* MICROTCP_SETTINGS_H */