#ifndef MICROTCP_SETTINGS_H
#define MICROTCP_SETTINGS_H

#include <stddef.h>
#include <sys/time.h>
struct timeval;

/* MicroTCP socket configurators. */
struct timeval get_microtcp_ack_timeout(void);
void set_microtcp_ack_timeout(struct timeval _tv);

size_t get_bytestream_assembly_buffer_len(void);
void set_bytestream_assembly_buffer_len(size_t _length);


/* Connect()'s FSM configurators. */
size_t get_connect_rst_retries(void);
void set_connect_rst_retries(size_t _retries_count);

/* Accept()'s FSM configurators. */
size_t get_accept_synack_retries(void);
void set_accept_synack_retries(size_t _retries_count);


/* Shutdown()'s FSM configurators. */
size_t get_shutdown_finack_retries(void);
void set_shutdown_finack_retries(size_t _retries_count);
struct timeval get_shutdown_time_wait_period(void);
void set_shutdown_time_wait_period(struct timeval _tv);

#endif /* MICROTCP_SETTINGS_H */