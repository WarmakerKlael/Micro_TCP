#include "fsm_common.h"

/* Helper function: Safely subtract _minuend from _subtrahend and store the result in t1 */
void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend)
{
        SMART_ASSERT(_subtrahend != NULL);
        _subtrahend->tv_sec -= _minuend.tv_sec;
        _subtrahend->tv_usec -= _minuend.tv_usec;

        /* Normalize if microseconds are out of range */
        if (_subtrahend->tv_usec < 0)
        {
                time_t borrow = (-_subtrahend->tv_usec / 1000000) + 1; /* Calculate how many seconds to borrow */
                _subtrahend->tv_sec -= borrow;
                _subtrahend->tv_usec += borrow * 1000000;
        }

        /* Ensure the result is non-negative */
        if (_subtrahend->tv_sec < 0)
        {
                _subtrahend->tv_sec = 0;
                _subtrahend->tv_usec = 0;
        }
}

/* Helper function: Convert timeval to microseconds */
time_t timeval_to_us(const struct timeval _timeval)
{
        const time_t usec_per_sec = 1000000;
        return (_timeval.tv_sec * usec_per_sec) + _timeval.tv_usec;
}