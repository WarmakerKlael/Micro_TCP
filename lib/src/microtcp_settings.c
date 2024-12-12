#include <sys/time.h>
#include "smart_assert.h"
#include "microtcp.h"
#include "microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "microtcp_common_macros.h"
#include "microtcp_helper_functions.h"

/* Constants used in this file. */
static const time_t default_microtcp_ack_timeout_sec = 0;
static const time_t default_microtcp_ack_timeout_usec = MICROTCP_ACK_TIMEOUT_US;

/* ----------------------------------------- MicroTCP general configuration variables ----------------------------------------- */
static size_t microtcp_bytestream_assembly_buffer_len = MICROTCP_RECVBUF_LEN;
static struct timeval microtcp_ack_timeout = {.tv_sec = default_microtcp_ack_timeout_sec, .tv_usec = default_microtcp_ack_timeout_usec};

/* ----------------------------------------- Connect()'s FSM configuration variables ------------------------------------------ */
#define DEFAULT_RETRIES_AFTER_RST 3
static size_t connect_rst_retries = DEFAULT_RETRIES_AFTER_RST; /* Default. Can be changed from following "API". */

/* ------------------------------------------ Accept()'s FSM configuration variables ------------------------------------------ */
#define LINUX_DEFAULT_ACCEPT_TIMEOUTS 5
static size_t accept_synack_retries = LINUX_DEFAULT_ACCEPT_TIMEOUTS; /* Default. Can be changed from following "API". */

/* ----------------------------------------- Shutdown()'s FSM configuration variables ----------------------------------------- */
#define MICROTCP_MSL_SECONDS 10 /* Maximum Segment Lifetime. Used for transitioning from TIME_WAIT -> CLOSED */
#define TCP_RETRIES2 15         /* TCP default value for `TCP_RETRIES2` variable.*/
static struct timeval shutdown_time_wait_period = {.tv_sec = 2 * MICROTCP_MSL_SECONDS, .tv_usec = 0};
static size_t shutdown_finack_retries = TCP_RETRIES2; /* Default. Can be changed from following "API". */

/* ___________________________________________________________________________________________________________________________ */
/* ___________________________________________________________________________________________________________________________ */

/* ----------------------------------------- MicroTCP socket configurators ------------------------------------------ */
struct timeval get_microtcp_ack_timeout(void)
{
        return microtcp_ack_timeout;
}

void set_microtcp_ack_timeout(struct timeval _tv)
{
        SMART_ASSERT(_tv.tv_sec >= 0, _tv.tv_usec >= 0);
        normalize_timeval(&_tv);

        if (_tv.tv_sec != default_microtcp_ack_timeout_sec || _tv.tv_usec != default_microtcp_ack_timeout_usec)
                LOG_WARNING("Settings: Required ACK timeout value to [%lu sec, %lu μsec]; Default: [%lu sec, %lu μsec].",
                            _tv.tv_sec, _tv.tv_usec, default_microtcp_ack_timeout_sec, default_microtcp_ack_timeout_usec);

        microtcp_ack_timeout = _tv;
}

size_t get_bytestream_assembly_buffer_len(void)
{
        return microtcp_bytestream_assembly_buffer_len;
}

void set_bytestream_assembly_buffer_len(size_t _length)
{
        if (_length < MICROTCP_RECVBUF_LEN)
                LOG_WARNING("Setting `bytestream_assembly_buffer` length to %dbytes; Less than default: %s = %d bytes",
                            _length, STRINGIFY(MICROTCP_RECVBUF_LEN), MICROTCP_RECVBUF_LEN);
        else
                LOG_INFO("Setting `bytestream_assembly_buffer` length to %d bytes", _length);
        microtcp_bytestream_assembly_buffer_len = _length;
}

/* ----------------------------------------- Connect()'s FSM configurators ------------------------------------------ */
size_t get_connect_rst_retries(void)
{
        return connect_rst_retries;
}

void set_connect_rst_retries(size_t _retries_count)
{
        connect_rst_retries = _retries_count;
}

/* ------------------------------------------ Accept()'s FSM configurators ------------------------------------------ */
size_t get_accept_synack_retries(void)
{
        return accept_synack_retries;
}

void set_accept_synack_retries(size_t _retries_count)
{
        accept_synack_retries = _retries_count;
}

/* ----------------------------------------- Shutdown()'s FSM configurators ----------------------------------------- */
size_t get_shutdown_finack_retries(void)
{
        return shutdown_finack_retries;
}

void set_shutdown_finack_retries(size_t _retries_count)
{
        shutdown_finack_retries = _retries_count;
}

struct timeval get_shutdown_time_wait_period(void)
{
        return shutdown_time_wait_period;
}

void set_shutdown_time_wait_period(struct timeval _tv)
{
        SMART_ASSERT(_tv.tv_sec >= 0, _tv.tv_usec >= 0);
        normalize_timeval(&_tv);
        shutdown_time_wait_period = _tv;
}
