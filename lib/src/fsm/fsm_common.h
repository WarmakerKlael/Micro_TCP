#ifndef STATE_MACHINES_COMMON_H
#define STATE_MACHINES_COMMON_H

#include <time.h>  // for time_t
struct timeval;

#define SENT_SYN_SEQUENCE_NUMBER_INCREMENT 1
#define SENT_FIN_SEQUENCE_NUMBER_INCREMENT 1

void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend);
time_t timeval_to_us(const struct timeval _timeval);

#endif /* STATE_MACHINES_COMMON_H */