#include <sys/time.h>
#include "smart_assert.h"
#include "microtcp.h"
#include "settings/microtcp_settings.h"
#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "microtcp_helper_functions.h"
#include "microtcp_settings_common.h"

/* ----------------------------------------- MicroTCP general configuration variables ----------------------------------------- */
static size_t microtcp_bytestream_rrb_size = MICROTCP_RECVBUF_LEN;
static struct timeval microtcp_ack_timeout = DEFAULT_MICROTCP_ACK_TIMEOUT;
static struct timeval microtcp_stall_time_limit = DEFAULT_MICROTCP_STALL_TIME_LIMIT;

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
size_t get_microtcp_bytestream_rrb_size(void)
{
        return microtcp_bytestream_rrb_size;
}

void set_microtcp_bytestream_rrb_size(size_t _bytstream_rrb_size)
{
        SMART_ASSERT(IS_POWER_OF_2(MICROTCP_RECVBUF_LEN), IS_POWER_OF_2(_bytstream_rrb_size));
        if (_bytstream_rrb_size != MICROTCP_RECVBUF_LEN)
                LOG_WARNING("Setting `microtcp_bytestream_rrb_size` to %d bytes; Default: %s = %d bytes",
                            _bytstream_rrb_size, STRINGIFY(MICROTCP_RECVBUF_LEN), MICROTCP_RECVBUF_LEN);
        else
                LOG_INFO("Setting `microtcp_bytestream_rrb_size` to %d bytes", _bytstream_rrb_size);
        microtcp_bytestream_rrb_size = _bytstream_rrb_size;
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

void set_microtcp_stall_time_limit(const struct timeval _time_limit)
{
        if (timeval_to_usec(_time_limit) <= timeval_to_usec(get_microtcp_ack_timeout()))
        {
                LOG_ERROR("MicroTCP's stall time limit must be greater than MicroTCP's timeout. Stall time limit remains at default.");
                return;
        }
        microtcp_stall_time_limit = _time_limit;
        LOG_INFO("MIcroTCP stall time limit updated to [%lldsec, %lldusec].", _time_limit.tv_sec, _time_limit.tv_usec);
}

struct timeval get_microtcp_stall_time_limit(void)
{
        return microtcp_stall_time_limit;
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
