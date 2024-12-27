#include "fsm_common.h"
#include <sys/time.h>
#include "smart_assert.h"

/* Helper function: Safely subtract _minuend from _subtrahend and store the result in t1 */
const time_t usec_per_sec = 1000000;
void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend)
{
        SMART_ASSERT(_subtrahend != NULL);
        _subtrahend->tv_sec -= _minuend.tv_sec;
        _subtrahend->tv_usec -= _minuend.tv_usec;

        /* Normalize if microseconds are out of range */
        if (_subtrahend->tv_usec < 0)
        {
                time_t borrow = (-_subtrahend->tv_usec / usec_per_sec) + 1; /* Calculate how many seconds to borrow */
                _subtrahend->tv_sec -= borrow;
                _subtrahend->tv_usec += borrow * usec_per_sec;
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

time_t elapsed_time_us(struct timeval _start_time)
{
        SMART_ASSERT(_start_time.tv_sec >= 0, _start_time.tv_usec >= 0, _start_time.tv_usec < usec_per_sec);
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
                     
        return timeval_to_us(current_time) - timeval_to_us(_start_time);
}