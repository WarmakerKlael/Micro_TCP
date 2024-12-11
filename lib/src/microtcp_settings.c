#include <sys/time.h>
#include "smart_assert.h"
#include "microtcp.h"
#include "microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "microtcp_common_macros.h"

/* ----------------------------------------- MicroTCP general configuration variables ----------------------------------------- */
static size_t microtcp_recvbuf_len = MICROTCP_RECVBUF_LEN;
static size_t microtcp_ack_timeout_usec = MICROTCP_ACK_TIMEOUT_US;

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

/* ----------------------------------------- MicroTCP general configurators ------------------------------------------ */
size_t get_microtcp_ack_timeout_us(void)
{
        return microtcp_ack_timeout_usec;
}

void set_microtcp_ack_timeout_usec(size_t _usec)
{
        if (_usec >= 1000000)
        {
                LOG_ERROR("ACK timeout value range = [0,999999]; You enter = %lu. ACK timeout value remains.", _usec);
                return;
        }
        if (_usec != MICROTCP_ACK_TIMEOUT_US)
                LOG_WARNING("Setting ACK timeout value to %lu μseconds. Default: %s = %lu",
                            _usec, STRINGIFY(MICROTCP_ACK_TIMEOUT_US), MICROTCP_ACK_TIMEOUT_US);
        microtcp_ack_timeout_usec = _usec;
}

size_t get_microtcp_recvbuf_len(void)
{
        return microtcp_recvbuf_len;
}

void set_microtcp_recvbuf_len(size_t _length)
{
        if (_length < MICROTCP_RECVBUF_LEN)
                LOG_WARNING("Setting `recvbuf` length to %dbytes; Less than default: %s = %d bytes",
                            _length, STRINGIFY(MICROTCP_RECVBUF_LEN), MICROTCP_RECVBUF_LEN);
        else
                LOG_INFO("Setting `recvbuf` length to %d bytes", _length);
        microtcp_recvbuf_len = _length;
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

size_t get_shutdown_time_wait_period_sec(void)
{
        return shutdown_time_wait_period.tv_sec;
}

size_t get_shutdown_time_wait_period_usec(void)
{
        return shutdown_time_wait_period.tv_usec;
}

void set_shutdown_time_wait_period(time_t _sec, time_t _usec)
{
        SMART_ASSERT(_sec >= 0, _usec >= 0);
        const time_t usec_per_sec = 1000000;
        shutdown_time_wait_period.tv_sec = _sec + (_usec / usec_per_sec);
        shutdown_time_wait_period.tv_usec = _usec % usec_per_sec;
}
