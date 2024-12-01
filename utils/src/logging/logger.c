#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "allocator/allocator.h"
#include "logging/logger.h"
#include "cli_color.h"
#include "microtcp_defines.h"

/* Global Variables */
pthread_mutex_t *mutex_logger = NULL;
FILE *print_stream;
/* ---------------- */

static void logger_init() __attribute__((constructor));
static void logger_destroy() __attribute__((destructor));
static const char *get_colored_string_log_tag(enum log_tag _log_tag);
static void _unsafe_print_log_forward(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, va_list arg_list);

static void logger_init()
{
	print_stream = stdout;
	MALLOC_LOG(mutex_logger, sizeof(pthread_mutex_t));
#define PTHREAD_MUTEX_INIT_SUCCESS 0 /* Specified in man pages. */
	if (mutex_logger == NULL || pthread_mutex_init(mutex_logger, NULL) != PTHREAD_MUTEX_INIT_SUCCESS)
	{
		_UNSAFE_PRINT_LOG(LOG_ERROR, "Logger failed to initialize.");
		exit(EXIT_FAILURE);
	}
	_UNSAFE_PRINT_LOG(LOG_INFO, "Logger initialized.");
}

static void logger_destroy()
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
	const char *colored_string_log_tag = get_colored_string_log_tag(_log_tag);
	fprintf(print_stream, "[%s][FILE %s][LINE %d][FUNCTION %s()]: " MAGENTA_COLOR, colored_string_log_tag, _file, _line, _func);
	vfprintf(print_stream, _message, arg_list);
	fprintf(print_stream, "\n" RESET_COLOR);
}

// clang-format off
static const char *get_colored_string_log_tag(enum log_tag _log_tag)
{
	switch (_log_tag)
	{
	case LOG_INFO:		return BLUE_COLOR 	PROJECT_NAME " " "INFO"		RESET_COLOR;
	case LOG_WARNING:	return YELLOW_COLOR 	PROJECT_NAME " " "WARNING"	RESET_COLOR;
	case LOG_ERROR:		return RED_COLOR 	PROJECT_NAME " " "ERROR" 	RESET_COLOR;
	default:		return RESET_COLOR 	PROJECT_NAME " " "??LOG??"	RESET_COLOR;
	}
}
// clang-format on