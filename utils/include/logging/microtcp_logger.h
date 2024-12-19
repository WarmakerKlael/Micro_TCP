#ifndef MICROTCP_LOGGER_H
#define MICROTCP_LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "cli_color.h"
#include "microtcp_defines.h"

/**
 * @brief Resolves the file name relative to the project directory.
 *
 * If the file path contains the project name, this macro extracts the portion
 * of the path starting from the project name. Otherwise, it returns the full
 * file path.
 *
 * @note This macro helps generate shorter, more readable file paths in logs.
 */
// #define __FILENAME__ (strstr(__FILE__, PROJECT_TOP_LEVEL_DIRECTORY) ? strstr(__FILE__, PROJECT_TOP_LEVEL_DIRECTORY) : __FILE__)
#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif /* __FILENAME__ */

enum log_tag
{
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
};

/**
 * @brief Outputs a formatted log message with thread safety.
 *
 * @param _log_tag The log level tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 * @param _file The source file where the log originates.
 * @param _line The line number where the log originates.
 * @param _func The function name where the log originates.
 * @param _format_message The format string for the log message.
 * @param ... The variable arguments to format the log message.
 *
 * @note This function ensures thread-safe logging by locking a mutex during
 *       the log operation.
 */
void log_message_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, ...);

/**
 * @brief Outputs a formatted log message without thread safety.
 *
 * @param _log_tag The log level tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 * @param _file The source file where the log originates.
 * @param _line The line number where the log originates.
 * @param _func The function name where the log originates.
 * @param _format_message The format string for the log message.
 * @param ... The variable arguments to format the log message.
 *
 * @note This function does not ensure thread safety. Use `log_message_thread_safe`
 *       if concurrent access to the logger is possible.
 */
void log_message_non_thread_safe(enum log_tag _log_tag, const char *_file, int _line, const char *_func, const char *_format_message, ...);

/**
 * @brief Logs a message without thread safety, adding file, line, and function context.
 *
 * A convenience macro for logging messages with additional context such as
 * the file name, line number, and function name. This macro forwards the call
 * to `log_message_non_thread_safe`.
 *
 * @param _log_tag The log level tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments for formatting the message.
 *
 * @note This macro does not ensure thread safety. Use a thread-safe alternative
 *       for logging in multi-threaded environments.
 */
#define LOG_MESSAGE_NON_THREAD_SAFE(_log_tag, _format_message, ...) \
	log_message_non_thread_safe(_log_tag, __FILENAME__, __LINE__, __func__, _format_message, ##__VA_ARGS__)

/**
 * @brief Logs a message with thread safety, adding file, line, and function context.
 *
 * A convenience macro for logging messages with additional context such as
 * the file name, line number, and function name. This macro forwards the call
 * to `log_message_thread_safe`.
 *
 * @param _log_tag The log level tag (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety, making it suitable for multi-threaded environments.
 */
#define LOG_MESSAGE(_log_tag, _format_message, ...) \
	log_message_thread_safe(_log_tag, __FILENAME__, __LINE__, __func__, _format_message, ##__VA_ARGS__)

/**
 * @brief Logs an informational message.
 *
 * Logs a message with the `LOG_INFO` level, including contextual information
 * such as the file name, line number, and function name.
 *
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_INFO(_format_message, ...) LOG_MESSAGE(LOG_INFO, _format_message, ##__VA_ARGS__);

/**
 * @brief Logs a warning message.
 *
 * Logs a message with the `LOG_WARNING` level, including contextual information
 * such as the file name, line number, and function name.
 *
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_WARNING(_format_message, ...) LOG_MESSAGE(LOG_WARNING, _format_message, ##__VA_ARGS__)

/**
 * @brief Logs an error message.
 *
 * Logs a message with the `LOG_ERROR` level, including contextual information
 * such as the file name, line number, and function name.
 *
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_ERROR(_format_message, ...) LOG_MESSAGE(LOG_ERROR, _format_message, ##__VA_ARGS__)

/**
 * @brief Logs an informational message and returns a specified value.
 *
 * Logs a message with the `LOG_INFO` level, including contextual information
 * such as the file name, line number, and function name, and then returns
 * the provided value.
 *
 * @param _return_value The value to return after logging the message.
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_INFO_RETURN(_return_value, _format_message, ...) \
	do                                                   \
	{                                                    \
		LOG_INFO(_format_message, ##__VA_ARGS__);    \
		return _return_value;                        \
	} while (0)

/**
 * @brief Logs a warning message and returns a specified value.
 *
 * Logs a message with the `LOG_WARNING` level, including contextual information
 * such as the file name, line number, and function name, and then returns
 * the provided value.
 *
 * @param _return_value The value to return after logging the message.
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_WARNING_RETURN(_return_value, _format_message, ...) \
	do                                                      \
	{                                                       \
		LOG_WARNING(_format_message, ##__VA_ARGS__);    \
		return _return_value;                           \
	} while (0)
/**
 * @brief Logs an error message and returns a specified value.
 *
 * Logs a message with the `LOG_ERROR` level, including contextual information
 * such as the file name, line number, and function name, and then returns
 * the provided value.
 *
 * @param _return_value The value to return after logging the message.
 * @param _format_message The format string for the log message.
 * @param ... Additional arguments to format the log message.
 *
 * @note This macro ensures thread safety while logging the message.
 */
#define LOG_ERROR_RETURN(_return_value, _format_message, ...) \
	do                                                    \
	{                                                     \
		LOG_ERROR(_format_message, ##__VA_ARGS__);    \
		return _return_value;                         \
	} while (0)

#endif /* MICROTCP_LOGGER_H */
