#ifndef STATE_MACHINES_COMMON_H
#define STATE_MACHINES_COMMON_H

#define SENT_SYN_SEQUENCE_NUMBER_INCREMENT 1
#define SENT_FIN_SEQUENCE_NUMBER_INCREMENT 1

/* TODO: inline actions of states of state machine. See what happens. */

#include <sys/time.h>
#include <time.h>

void subtract_timeval(struct timeval *_subtrahend, const struct timeval _minuend);
time_t timeval_to_us(const struct timeval _timeval);

#endif /* STATE_MACHINES_COMMON_H */