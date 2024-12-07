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
__attribute__((constructor(LOGGER_CONSTRUCTOR_PRIORITY))) static void logger_init();
__attribute__((destructor(LOGGER_DESTRUCTOR_PRIORITY))) static void logger_destroy();
static void _unsafe_print_log_forward(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, va_list arg_list);
static const char *get_colored_string_log_tag(enum log_tag _log_tag);

static void logger_init(void)
{
#ifdef DEBUG_MODE
	logger_set_allocator_enabled(TRUE);
#endif /* DEBUG_MODE */
	logger_set_enabled(TRUE);
	logger_set_info_enabled(TRUE);
	logger_set_warning_enabled(TRUE);
	logger_set_error_enabled(TRUE);

#define PTHREAD_MUTEX_INIT_SUCCESS 0 /* Specified in man pages. */
	print_stream = stdout;
	mutex_logger = CALLOC_LOG(mutex_logger, sizeof(pthread_mutex_t));
	if (mutex_logger == NULL || pthread_mutex_init(mutex_logger, NULL) != PTHREAD_MUTEX_INIT_SUCCESS)
	{
		_UNSAFE_PRINT_LOG(LOG_ERROR, "Logger failed to initialize.");
		exit(EXIT_FAILURE);
	}
	_UNSAFE_PRINT_LOG(LOG_INFO, "Logger initialized.");
}

static void logger_destroy(void)
{
#define PTHREAD_MUTEX_DESTROY_SUCCESS 0 /* Specified in man pages. */
	if (mutex_logger != NULL || pthread_mutex_destroy(mutex_logger) == PTHREAD_MUTEX_DESTROY_SUCCESS)
	{
		free(mutex_logger);
		mutex_logger = NULL;
		_UNSAFE_PRINT_LOG(LOG_INFO, "Logger destroyed.");
	}
	else
		_UNSAFE_PRINT_LOG(LOG_WARNING, "Logger could not be destroyed.");
}

void _unsafe_print_log(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, ...)
{
	va_list args;
	va_start(args, _message);
	_unsafe_print_log_forward(_log_tag, _file, _line, _func, _message, args);
	va_end(args);
}

void _safe_print_log(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, ...)
{
	va_list args;
	va_start(args, _message);
	pthread_mutex_lock(mutex_logger);
	_unsafe_print_log_forward(_log_tag, _file, _line, _func, _message, args);
	pthread_mutex_unlock(mutex_logger);
	va_end(args);
}

static void _unsafe_print_log_forward(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, va_list arg_list)
{
	if (!logger_is_enabled())
		return;
	if (!logger_is_info_enabled() && _log_tag == LOG_INFO)
		return;
	if (!logger_is_warning_enabled() && _log_tag == LOG_WARNING)
		return;
	if (!logger_is_error_enabled() && _log_tag == LOG_ERROR)
		return;

	const char *colored_string_log_tag = get_colored_string_log_tag(_log_tag);
	fprintf(print_stream, "[%s][FILE %s][LINE %d][FUNCTION %s()]: %s", colored_string_log_tag, _file, _line, _func, LOG_MESSAGE_COLOR);
	vfprintf(print_stream, _message, arg_list);
	fprintf(print_stream, "%s\n", RESET_COLOR);
}

// clang-format off
static const char *get_colored_string_log_tag(enum log_tag _log_tag)
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