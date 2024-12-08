#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "logging/logger.h"
#include "logging/logger_options.h"
#include "allocator/allocator.h"
#include "microtcp_defines.h"

extern FILE *print_stream;

/* Global Variables */
pthread_mutex_t *mutex_logger = NULL;
/* ---------------- */

/* Declarations of static functions implemented in this file: */

/**
 * @brief Initializes the logging system.
 *
 * This function sets up the logger, enabling logging for info, warning,
 * and error levels. By default, log messages are output to `stdout`
 * via `print_stream`. Thread safety is ensured with a dynamically
 * allocated mutex.
 *
 * In debug mode (`DEBUG_MODE`), logging of allocated and deallocated
 * memory is also enabled. If initialization fails, the program exits
 * with an error.
 *
 * @note Thread safety relies on a mutex, and failure to initialize
 *       the mutex results in termination.
 */
__attribute__((constructor(LOGGER_CONSTRUCTOR_PRIORITY))) static void logger_initialize();

/**
 * @brief Deinitializes the logging system.
 *
 * Releases resources allocated for logging. Logs a warning if cleanup is incomplete.
 */
__attribute__((destructor(LOGGER_DESTRUCTOR_PRIORITY))) static void logger_shutdown();

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
static void log_message_forward_non_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, va_list arg_list);

/**
 * @brief Returns a color-coded string representation of a log tag.
 *
 * @param _log_tag The log tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 *
 * @return A string containing the color-coded log tag with the project name.
 *         For unrecognized tags, returns a default color-coded string
 *         with `"??LOG??"`.
 */
static const char *get_colored_log_tag_string(enum log_tag _log_tag);

static void logger_initialize(void)
{
#ifdef DEBUG_MODE
	logger_set_allocator_enabled(TRUE);
#elif VERBOSE_MODE
	logger_set_info_enabled(TRUE);
#endif /* DEBUG_MODE  or VERBOSE_MODE */
	logger_set_enabled(TRUE);
	logger_set_warning_enabled(TRUE);
	logger_set_error_enabled(TRUE);

#define PTHREAD_MUTEX_INIT_SUCCESS 0 /* Specified in man pages. */
	print_stream = stdout;
	mutex_logger = CALLOC_LOG(mutex_logger, sizeof(pthread_mutex_t));
	if (mutex_logger == NULL || pthread_mutex_init(mutex_logger, NULL) != PTHREAD_MUTEX_INIT_SUCCESS)
	{
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_ERROR, "Logger failed to initialize.");
		exit(EXIT_FAILURE);
	}
	LOG_MESSAGE_NON_THREAD_SAFE(LOG_INFO, "Logger initialized.");
}

static void logger_shutdown(void)
{
#define PTHREAD_MUTEX_DESTROY_SUCCESS 0 /* Specified in man pages. */
	if (mutex_logger != NULL || pthread_mutex_destroy(mutex_logger) == PTHREAD_MUTEX_DESTROY_SUCCESS)
	{
		free(mutex_logger);
		mutex_logger = NULL;
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_INFO, "Logger destroyed.");
	}
	else
		LOG_MESSAGE_NON_THREAD_SAFE(LOG_WARNING, "Logger could not be destroyed.");
}

void log_message_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, ...)
{
	va_list args;
	va_start(args, _format_message);
	pthread_mutex_lock(mutex_logger);
	log_message_forward_non_thread_safe(_log_tag, _file, _line, _func, _format_message, args);
	pthread_mutex_unlock(mutex_logger);
	va_end(args);
}

void log_message_non_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, ...)
{
	va_list args;
	va_start(args, _format_message);
	log_message_forward_non_thread_safe(_log_tag, _file, _line, _func, _format_message, args);
	va_end(args);
}

static void log_message_forward_non_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, va_list arg_list)
{
	if (!logger_is_enabled())
		return;
	if (!logger_is_info_enabled() && _log_tag == LOG_INFO)
		return;
	if (!logger_is_warning_enabled() && _log_tag == LOG_WARNING)
		return;
	if (!logger_is_error_enabled() && _log_tag == LOG_ERROR)
		return;

	const char *colored_string_log_tag = get_colored_log_tag_string(_log_tag);
	fprintf(print_stream, "[%s][FILE %s][LINE %d][FUNCTION %s()]: %s", colored_string_log_tag, _file, _line, _func, LOG_MESSAGE_COLOR);
	vfprintf(print_stream, _format_message, arg_list);
	fprintf(print_stream, "%s\n", RESET_COLOR);
}

// clang-format off
static const char *get_colored_log_tag_string(enum log_tag _log_tag)
{
	switch (_log_tag)
	{
	case LOG_INFO:		return LOG_INFO_COLOR	        PROJECT_NAME " " "INFO"		RESET_COLOR;
	case LOG_WARNING:	return LOG_WARNING_COLOR        PROJECT_NAME " " "WARNING"	RESET_COLOR;
	case LOG_ERROR:		return LOG_ERROR_COLOR 	        PROJECT_NAME " " "ERROR" 	RESET_COLOR;
	default:		return RESET_COLOR 	        PROJECT_NAME " " "??LOG??"	RESET_COLOR;
	}
}
// clang-format on