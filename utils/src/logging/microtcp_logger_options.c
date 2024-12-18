#include <stdio.h>
#include "microtcp_defines.h"
#include "logging/logger_options.h"

/**
 * @brief A stream to which the logger outputs its messages.
 */
FILE *microtcp_log_stream;

_Bool logger_enabled = FALSE;
_Bool logger_info_enabled = FALSE;
_Bool logger_warning_enabled = FALSE;
_Bool logger_error_enabled = FALSE;
_Bool logger_allocator_enabled = FALSE;

/* Logger Option Getters: */
_Bool logger_is_enabled(void)
{
        return logger_enabled;
}
_Bool logger_is_info_enabled(void)
{
        return logger_info_enabled;
}
_Bool logger_is_warning_enabled(void)
{
        return logger_warning_enabled;
}
_Bool logger_is_error_enabled(void)
{
        return logger_error_enabled;
}
_Bool logger_is_allocator_enabled(void)
{
        return logger_allocator_enabled;
}

/* Logger Option Setters: */
void logger_set_enabled(_Bool _enabled)
{
        logger_enabled = _enabled;
}
void logger_set_info_enabled(_Bool _enabled)
{
        logger_info_enabled = _enabled;
}
void logger_set_warning_enabled(_Bool _enabled)
{
        logger_warning_enabled = _enabled;
}
void logger_set_error_enabled(_Bool _enabled)
{
        logger_error_enabled = _enabled;
}
void logger_set_allocator_enabled(_Bool _enabled)
{
        logger_allocator_enabled = _enabled;
}