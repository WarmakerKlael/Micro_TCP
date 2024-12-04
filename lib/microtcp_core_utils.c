#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "microtcp_core_utils.h"

#include "logging/logger.h"

/* Declarations of static functions implemented in this file: */
__attribute__((constructor(RNG_CONSTRUCTOR_PRIORITY))) static void seed_random_number_generator(void);

static void seed_random_number_generator(void)
{
#define CLOCK_GETTIME_FAILURE -1 /* Specified in man pages. */
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == CLOCK_GETTIME_FAILURE)
        {
                PRINT_ERROR("RNG seeding failed; %s() -> ERRNO %d: %s", STRINGIFY(clock_gettime), errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
        unsigned int seed = ts.tv_sec ^ ts.tv_nsec;
        srand(seed);
        PRINT_INFO("RNG seeding successed; Seed = %u", seed);
}

uint32_t generate_initial_sequence_number(void)
{
        uint32_t high = rand() & 0xFFFF; /* Get only the 16 lower bits. */
        uint32_t low = rand() & 0xFFFF;  /* Get only the 16 lower bits. */
        uint32_t isn = (high << 16) | low;
        return isn;
}
