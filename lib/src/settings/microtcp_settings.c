#include <sys/time.h>
#include "smart_assert.h"
#include "microtcp.h"
#include "settings/microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "microtcp_helper_functions.h"
#include "microtcp_settings_common.h"

/* ----------------------------------------- MicroTCP general configuration variables ----------------------------------------- */
static size_t microtcp_bytestream_assembly_buffer_len = MICROTCP_RECVBUF_LEN;
static struct timeval microtcp_ack_timeout = DEFAULT_MICROTCP_ACK_TIMEOUT;

/* ----------------------------------------- Connect()'s FSM configuration variables ------------------------------------------ */
static size_t connect_rst_retries = DEFAULT_CONNECT_RST_RETRIES; /* Default. Can be changed from following "API". */

/* ------------------------------------------ Accept()'s FSM configuration variables ------------------------------------------ */
static size_t accept_synack_retries = LINUX_DEFAULT_ACCEPT_TIMEOUTS; /* Default. Can be changed from following "API". */

/* ----------------------------------------- Shutdown()'s FSM configuration variables ----------------------------------------- */
static struct timeval shutdown_time_wait_period = {.tv_sec = DEFAULT_SHUTDOWN_TIME_WAIT_SEC, .tv_usec = DEFAULT_SHUTDOWN_TIME_WAIT_USEC};
static size_t shutdown_finack_retries = TCP_RETRIES2; /* Default. Can be changed from following "API". */

/* ___________________________________________________________________________________________________________________________ */
/* ___________________________________________________________________________________________________________________________ */

/* ----------------------------------------- MicroTCP socket configurators ------------------------------------------ */
size_t get_bytestream_assembly_buffer_len(void)
{
        return microtcp_bytestream_assembly_buffer_len;
}

void set_bytestream_assembly_buffer_len(size_t _bytstream_assembly_bufferlength)
{
        SMART_ASSERT(is_power_of_2(MICROTCP_RECVBUF_LEN), is_power_of_2(_bytstream_assembly_bufferlength));
        if (_bytstream_assembly_bufferlength != MICROTCP_RECVBUF_LEN)
                LOG_WARNING("Setting `bytestream_assembly_buffer` length to %d bytes; Default: %s = %d bytes",
                            _bytstream_assembly_bufferlength, STRINGIFY(MICROTCP_RECVBUF_LEN), MICROTCP_RECVBUF_LEN);
        else
                LOG_INFO("Setting `bytestream_assembly_buffer` length to %d bytes", _bytstream_assembly_bufferlength);
        microtcp_bytestream_assembly_buffer_len = _bytstream_assembly_bufferlength;
}

struct timeval get_microtcp_ack_timeout(void)
{
        return microtcp_ack_timeout;
}

void set_microtcp_ack_timeout(struct timeval _ack_timeout_tv)
{
        SMART_ASSERT(_ack_timeout_tv.tv_sec >= 0, _ack_timeout_tv.tv_usec >= 0);
        normalize_timeval(&_ack_timeout_tv);

        if (_ack_timeout_tv.tv_sec != DEFAULT_MICROTCP_ACK_TIMEOUT_SEC || _ack_timeout_tv.tv_usec != DEFAULT_MICROTCP_ACK_TIMEOUT_USEC)
                LOG_WARNING("Setting `microtcp_ack_timeout` value to [%ld sec, %ld μsec]; Defaultt: [%d sec, %d μsec].",
                            _ack_timeout_tv.tv_sec, _ack_timeout_tv.tv_usec, DEFAULT_MICROTCP_ACK_TIMEOUT_SEC, DEFAULT_MICROTCP_ACK_TIMEOUT_USEC);
        else
                LOG_INFO("Setting `microtcp_ack_timeout` value to [%ld sec, %ld μsec].",
                         _ack_timeout_tv.tv_sec, _ack_timeout_tv.tv_usec);

        microtcp_ack_timeout = _ack_timeout_tv;
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
