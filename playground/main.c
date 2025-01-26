#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Enum definition
typedef enum
{
        MTCPERR_CLEAN = 0,
        MTCPERR_RST_RECEIVED,
        MTCPERR_ARG_CONFLICT,
        MTCPERR_MAX // Acts as an invalid value marker
} microtcp_errno_t;

// Thread-local storage for the error code
_Thread_local microtcp_errno_t microtcp_errno_internal = MTCPERR_CLEAN;

// Accessor for the thread-local variable
#define _microtcp_errno_accessor() (&microtcp_errno_internal)

// Macro to intercept and validate assignments
#define microtcp_errno                                                                        \
        (*(                                                                                   \
            {                                                                                 \
                    _Static_assert(MTCPERR_MAX <= 256, "Enum values must be in valid range"); \
                    static void _validate_errno_value(microtcp_errno_t v)                     \
                    {                                                                         \
                            if (v < MTCPERR_CLEAN || v >= MTCPERR_MAX)                        \
                            {                                                                 \
                                    fprintf(stderr, "Invalid errno value: %d\n", v);          \
                                    abort();                                                  \
                            }                                                                 \
                    }                                                                         \
                    _validate_errno_value,                                                    \
                        _microtcp_errno_accessor()                                            \
            }))

int main()
{
        // Valid assignment
        microtcp_errno = MTCPERR_RST_RECEIVED;
        printf("Current errno: %d\n", microtcp_errno);

        // Invalid assignment (uncomment to test)
        // microtcp_errno = 100; // This will fail and terminate the program

        return 0;
}
