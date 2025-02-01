#include "logging/microtcp_logger.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "allocator/allocator_macros.h"
#include "cli_color.h"
#include "logging/logger_options.h"
#include "microtcp_defines.h"
#include "microtcp_logging_colors.h"

extern FILE *microtcp_log_stream;
extern pthread_mutex_t *mutex_logger;
#define LOGGER_TAG "LOGGER"

/* Declarations of static functions implemented in this file: */

/**
 * @brief Initializes the logging system.
 *
 * This function sets up the logger, enabling logging for info, warning,
 * and error levels. By default, log messages are output to `stdout`
 * via `microtcp_log_stream`. Thread safety is ensured with a dynamically
 * allocated mutex.
 *
 * In debug mode (`DEBUG_MODE`), logging of allocated and deallocated
 * memory is also enabled. If initialization fails, the program exits
 * with an error.
 *
 * @note Thread safety relies on a mutex, and failure to initialize
 *       the mutex results in termination.
 */
__attribute__((constructor(LOGGER_CONSTRUCTOR_PRIORITY))) static void logger_constructor(void);

/**
 * @brief Deinitializes the logging system.
 *
 * Releases resources allocated for logging. Logs a warning if cleanup is incomplete.
 */
__attribute__((destructor(LOGGER_DESTRUCTOR_PRIORITY))) static void logger_destructor(void);

/**
 * @brief Outputs a formatted log message to the log stream. Includes file,
 *       line, and function information for better debugging context.
 *
 * @param _log_tag The log level tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 * @param _file The source file where the log originates.
 * @param _line The line number where the log originates.
 * @param _func The function name where the log originates.
 * @param _format_message The format string for the log message.
 * @param arg_list The list of arguments for the format string.
 *
 * @note Outputs messages only if the log level is enabled.
 */
static void log_message_forward_non_thread_safe(enum log_tag _log_tag, const char *_project_name, const char *_file, int _line, const char *_func, const char *_format_message, va_list arg_list);

static const char *get_tag_color(enum log_tag _log_tag);
static const char *get_string_tag(enum log_tag _log_tag);

static void logger_constructor(void)
{
#ifdef DEBUG_MODE
	logger_set_allocator_enabled(true);
	logger_set_info_enabled(true);
#endif /* DEBUG_MODE  */
#ifdef VERBOSE_MODE
	logger_set_info_enabled(true);
#endif /* VERBOSE_MODE  */
	logger_set_enabled(true);
	logger_set_warning_enabled(true);
	logger_set_error_enabled(true);

#define PTHREAD_MUTEX_INIT_SUCCESS 0 /* Specified in man pages. */
	microtcp_log_stream = stdout;
	mutex_logger = CALLOC_LOG(mutex_logger, sizeof(pthread_mutex_t));
	if (mutex_logger == NULL || pthread_mutex_init(mutex_logger, NULL) != PTHREAD_MUTEX_INIT_SUCCESS)
	{
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_ERROR, LOGGER_TAG, "Logger failed to initialize.");
		exit(EXIT_FAILURE);
	}
	LOG_MESSAGE_NON_THREAD_SAFE(LOG_INFO, LOGGER_TAG, "Logger initialized.");
}

static void logger_destructor(void)
{
#define PTHREAD_MUTEX_DESTROY_SUCCESS 0 /* Specified in man pages. */
	if (mutex_logger != NULL && pthread_mutex_destroy(mutex_logger) == PTHREAD_MUTEX_DESTROY_SUCCESS)
	{
		free(mutex_logger);
		mutex_logger = NULL;
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_INFO, LOGGER_TAG, "Logger destroyed.");
	}
	else
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_WARNING, LOGGER_TAG, "Logger could not be destroyed.");
	fprintf(microtcp_log_stream, SGR_RESET); /* Reset print style to system's default. */
}

void log_message_thread_safe(enum log_tag _log_tag, const char *_project_name, const char *_file, int _line, const char *_func, const char *_format_message, ...)
{
	va_list args;
	va_start(args, _format_message);
	pthread_mutex_lock(mutex_logger);
	log_message_forward_non_thread_safe(_log_tag, _project_name, _file, _line, _func, _format_message, args);
	pthread_mutex_unlock(mutex_logger);
	va_end(args);
}

void log_message_non_thread_safe(enum log_tag _log_tag, const char *_project_name, const char *_file, int _line, const char *_func, const char *_format_message, ...)
{
	va_list args;
	va_start(args, _format_message);
	log_message_forward_non_thread_safe(_log_tag, _project_name, _file, _line, _func, _format_message, args);
	va_end(args);
}

static void log_message_forward_non_thread_safe(enum log_tag _log_tag, const char *_project_name, const char *_file, int _line, const char *_func, const char *_format_message, va_list arg_list)
{
	if (!logger_is_enabled())
		return;
	if (!logger_is_info_enabled() && _log_tag == LOG_INFO)
		return;
	if (!logger_is_warning_enabled() && _log_tag == LOG_WARNING)
		return;
	if (!logger_is_error_enabled() && _log_tag == LOG_ERROR)
		return;
	fprintf(microtcp_log_stream, LOG_DEFAULT_COLOR); /* Set DEFAULT logging color. */
#ifdef DEBUG_MODE
	static struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	long milliseconds = tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
	fprintf(microtcp_log_stream, "[%s%s %s%s][%s:%d][%s()][%lddu] ",
		get_tag_color(_log_tag), _project_name, get_string_tag(_log_tag), LOG_DEFAULT_COLOR, /* 1st brackets. */
		_file, _line,									     /* 2nd brackets. */
		_func,										     /* 3rd brackets. */
		milliseconds);									     /* 4th brackets. */

#else  /*ifndef DEBUG_MODE */
	fprintf(microtcp_log_stream, "[%s%s %s%s] ",
		get_tag_color(_log_tag), _project_name, get_string_tag(_log_tag), LOG_DEFAULT_COLOR);
#endif /* DEBUG_MODE */

	fprintf(microtcp_log_stream, LOG_MESSAGE_COLOR); /* Set logging message color. */
	vfprintf(microtcp_log_stream, _format_message, arg_list);
	fprintf(microtcp_log_stream, "%s\n", LOG_DEFAULT_COLOR); /* Reset to DEFAULT logging color. */
	fflush(microtcp_log_stream);				 /* We flush even though fprintf ends with '\n', in case stdout is fully-buffered. (file, pipe). */
}

static const char *get_string_tag(enum log_tag _log_tag)
{
	switch (_log_tag)
	{
	case LOG_INFO:
	case LOG_INFO_APP:
		return "INFO";
	case LOG_WARNING:
	case LOG_WARNING_APP:
		return "WARNING";
	case LOG_ERROR:
	case LOG_ERROR_APP:
		return "ERROR";
	default:
		return "??LOG??";
	}
}

static const char *get_tag_color(enum log_tag _log_tag)
{
	switch (_log_tag)
	{
	case LOG_INFO:
	case LOG_INFO_APP:
		return LOG_INFO_COLOR;
	case LOG_WARNING:
	case LOG_WARNING_APP:
		return LOG_WARNING_COLOR;
	case LOG_ERROR:
	case LOG_ERROR_APP:
		return LOG_ERROR_COLOR;
	default:
		return LOG_DEFAULT_COLOR;
	}
}
