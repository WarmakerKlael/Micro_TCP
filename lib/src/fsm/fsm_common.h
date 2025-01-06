#ifndef STATE_MACHINES_COMMON_H
#define STATE_MACHINES_COMMON_H

#include <time.h> // for time_t
struct timeval;


void subtract_timeval(struct timeval *_subtrahend, struct timeval _minuend);
time_t timeval_to_us(struct timeval _timeval);
struct timeval us_to_timeval(time_t _us);
time_t elapsed_time_us(struct timeval _start_time);

#endif /* STATE_MACHINES_COMMON_H */