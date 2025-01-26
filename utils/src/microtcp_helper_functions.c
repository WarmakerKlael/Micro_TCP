#include "microtcp_helper_functions.h"
#include <sys/time.h>
#include <time.h>
#include "smart_assert.h"

/**
 * @note Procomputed strings why? Cant you automate it you dumbo? Well ...
 * There were more automated and modular ways to assemble
 * the following strings, it was problematic, when this function
 * was called twice or more on the same line. E.g. inside of
 * printf() placeholders for example). So `static string IT IS`.
 * (NO dynamic allocation wasn't worth it; created deallocating memory headches).
 */

// clang-format off
const char *get_microtcp_state_to_string(mircotcp_state_t _state)
{
        #define CASE_RETURN_STRING(_case) case _case: return #_case
        switch ((int)_state)
        {
        /* Single States */
        CASE_RETURN_STRING(RESET);
        CASE_RETURN_STRING(INVALID);
        CASE_RETURN_STRING(CLOSED);
        CASE_RETURN_STRING(LISTEN);
        CASE_RETURN_STRING(ESTABLISHED);
        CASE_RETURN_STRING(CLOSING_BY_HOST);
        CASE_RETURN_STRING(CLOSING_BY_PEER);

        /* Two-State Combinations */
        CASE_RETURN_STRING(INVALID | CLOSED);
        CASE_RETURN_STRING(INVALID | LISTEN);
        CASE_RETURN_STRING(INVALID | ESTABLISHED);
        CASE_RETURN_STRING(INVALID | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | LISTEN);
        CASE_RETURN_STRING(CLOSED | ESTABLISHED);
        CASE_RETURN_STRING(CLOSED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(CLOSED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(LISTEN | ESTABLISHED);
        CASE_RETURN_STRING(LISTEN | CLOSING_BY_HOST);
        CASE_RETURN_STRING(LISTEN | CLOSING_BY_PEER);
        CASE_RETURN_STRING(ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSING_BY_HOST | CLOSING_BY_PEER);

        /* Three-State Combinations */
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN);
        CASE_RETURN_STRING(INVALID | CLOSED | ESTABLISHED);
        CASE_RETURN_STRING(INVALID | CLOSED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | CLOSED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | LISTEN | ESTABLISHED);
        CASE_RETURN_STRING(INVALID | LISTEN | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | LISTEN | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | LISTEN | ESTABLISHED);
        CASE_RETURN_STRING(CLOSED | LISTEN | CLOSING_BY_HOST);
        CASE_RETURN_STRING(CLOSED | LISTEN | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(CLOSED | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(LISTEN | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(LISTEN | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(LISTEN | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);

        /* Four-State Combinations */
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | ESTABLISHED);
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | CLOSED | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | CLOSED | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | CLOSED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | LISTEN | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | LISTEN | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | LISTEN | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | LISTEN | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(LISTEN | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);

        /* Five-State Combinations */
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_HOST);
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | CLOSED | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(INVALID | LISTEN | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
        CASE_RETURN_STRING(CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);

        /* Six-State Combination. */
        CASE_RETURN_STRING(INVALID | CLOSED | LISTEN | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);

        default: return "??MICROTCP_STATE??";
    }
}

const char *get_microtcp_control_to_string(const uint16_t _control)
{
        switch ((uint16_t)_control)
        {
        case SYN_BIT: return "SYN";
        case ACK_BIT: return "ACK";
        case RST_BIT: return "RST";
        case FIN_BIT: return "FIN";

        case SYN_BIT | ACK_BIT: return "SYN|ACK";
        case SYN_BIT | RST_BIT: return "SYN|RST";
        case SYN_BIT | FIN_BIT: return "SYN|FIN";
        case ACK_BIT | RST_BIT: return "RST|ACK";
        case ACK_BIT | FIN_BIT: return "FIN|ACK";
        case RST_BIT | FIN_BIT: return "FIN|RST";

        case SYN_BIT | ACK_BIT | RST_BIT: return "SYN|ACK|RST";
        case SYN_BIT | ACK_BIT | FIN_BIT: return "SYN|ACK|FIN";
        case SYN_BIT | RST_BIT | FIN_BIT: return "SYN|RST|FIN";
        case ACK_BIT | RST_BIT | FIN_BIT: return "ACK|RST|FIN";

        case SYN_BIT | ACK_BIT | RST_BIT | FIN_BIT: return "SYN|ACK|RST|FIN";
        default: return "??MICROTCP_CONTROL??";
        }
}
// clang-format on

#define USEC_PER_SEC 1000000
void normalize_timeval(struct timeval *_tv)
{
        SMART_ASSERT(_tv != NULL);
        SMART_ASSERT(_tv->tv_sec >= 0, _tv->tv_usec >= 0);
        _tv->tv_sec += (_tv->tv_usec / USEC_PER_SEC);
        _tv->tv_usec %= USEC_PER_SEC;
}


/* Helper function: Safely subtract _minuend from _subtrahend and store the result in t1 */
void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend)
{
        DEBUG_SMART_ASSERT(_subtrahend != NULL);
        DEBUG_SMART_ASSERT(_subtrahend->tv_sec >= 0, _subtrahend->tv_usec >= 0,
                           _minuend.tv_sec >= 0, _minuend.tv_usec >= 0);
        _subtrahend->tv_sec -= _minuend.tv_sec;
        _subtrahend->tv_usec -= _minuend.tv_usec;

        /* Normalize if microseconds are out of range */
        if (_subtrahend->tv_usec < 0)
        {
                time_t borrow = (-_subtrahend->tv_usec / USEC_PER_SEC) + 1; /* Calculate how many seconds to borrow */
                _subtrahend->tv_sec -= borrow;
                _subtrahend->tv_usec += borrow * USEC_PER_SEC;
        }

        /* Ensure the result is non-negative */
        if (_subtrahend->tv_sec < 0)
        {
                _subtrahend->tv_sec = 0;
                _subtrahend->tv_usec = 0;
        }
}

static __always_inline time_t timeval_to_us_internal(const struct timeval _timeval)
{
        const time_t result = (_timeval.tv_sec * USEC_PER_SEC) + _timeval.tv_usec;
        DEBUG_SMART_ASSERT(_timeval.tv_sec >= 0, _timeval.tv_usec >= 0, result >= 0);
        return result;
}

/* Helper function: Convert timeval to microseconds */
time_t timeval_to_us(const struct timeval _timeval)
{
        return timeval_to_us_internal(_timeval);
}

struct timeval us_to_timeval(const time_t _us)
{
        return (struct timeval){.tv_sec = _us / USEC_PER_SEC, .tv_usec = _us % USEC_PER_SEC};
}

time_t elapsed_time_us(struct timeval _start_time)
{
        DEBUG_SMART_ASSERT(_start_time.tv_sec >= 0, _start_time.tv_usec >= 0, _start_time.tv_usec < USEC_PER_SEC);
        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        return timeval_to_us_internal(current_time) - timeval_to_us_internal(_start_time);
}

struct timeval get_current_timeval(void)
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv;
}
