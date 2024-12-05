#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "cli_color.h"
#include "microtcp_defines.h"

#define LOG_INFO_COLOR BLUE_COLOR
#define LOG_WARNING_COLOR YELLOW_COLOR
#define LOG_ERROR_COLOR RED_COLOR
#define LOG_MESSAGE_COLOR GREEN_COLOR

#define __FILENAME__ (strstr(__FILE__, PROJECT_NAME) ? strstr(__FILE__, PROJECT_NAME) : __FILE__)

typedef enum log_tag log_tag_t;

enum log_tag
{
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
};

extern FILE *print_stream;

void _safe_print_log(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, ...);
void _unsafe_print_log(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_message, ...);

#define _UNSAFE_PRINT_LOG(_log_tag, _message, ...) _unsafe_print_log(_log_tag, __FILENAME__, __LINE__, __func__, _message, ##__VA_ARGS__)
#define _SAFE_PRINT_LOG(_log_tag, _message, ...) _safe_print_log(_log_tag, __FILENAME__, __LINE__, __func__, _message, ##__VA_ARGS__)

#define PRINT_INFO_RETURN(_return_value, _message, ...)             \
	do                                                          \
	{                                                           \
		_SAFE_PRINT_LOG(LOG_INFO, _message, ##__VA_ARGS__); \
		return _return_value;                               \
	} while (0)

#define PRINT_WARNING_RETURN(_return_value, _message, ...)             \
	do                                                             \
	{                                                              \
		_SAFE_PRINT_LOG(LOG_WARNING, _message, ##__VA_ARGS__); \
		return _return_value;                                  \
	} while (0)

#define PRINT_ERROR_RETURN(_return_value, _message, ...)             \
	do                                                           \
	{                                                            \
		_SAFE_PRINT_LOG(LOG_ERROR, _message, ##__VA_ARGS__); \
		return _return_value;                                \
	} while (0)

#define PRINT_INFO(_message, ...) _SAFE_PRINT_LOG(LOG_INFO, _message, ##__VA_ARGS__);
#define PRINT_WARNING(_message, ...) _SAFE_PRINT_LOG(LOG_WARNING, _message, ##__VA_ARGS__)
#define PRINT_ERROR(_message, ...) _SAFE_PRINT_LOG(LOG_ERROR, _message, ##__VA_ARGS__)

#endif /* LOGGER_H */
