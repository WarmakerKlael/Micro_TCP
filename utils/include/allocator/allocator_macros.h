#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdlib.h>
#include "logging/microtcp_logger.h"
#include "logging/logger_options.h"

/**
 * @brief Allocates memory and logs the allocation result.
 *
 * A wrapper around `malloc` that logs the allocation's success or failure.
 * Includes contextual information such as the allocated size and the variable name.
 *
 * @param _passed_memory_ptr The variable to which the allocated memory is assigned (used for logging).
 * @param _size_in_bytes The number of bytes to allocate.
 *
 * @return A pointer to the allocated memory, or `NULL` if the allocation fails.
 *
 * @note Logs are output only if the allocator logger is enabled.
 */
#define MALLOC_LOG(_passed_memory_ptr, _size_in_bytes) ({                                                             \
        void *malloc_result_ptr = malloc((_size_in_bytes));                                                           \
        const char *message = "Allocation of %d bytes to '%s'; %s";                                                   \
        enum log_tag log_tag = malloc_result_ptr ? LOG_INFO : LOG_ERROR;                                              \
        const char *message_suffix = malloc_result_ptr ? "SUCCEEDED" : "FAILED";                                      \
        if (logger_is_allocator_enabled())                                                                            \
                LOG_MESSAGE_NON_THREAD_SAFE(log_tag, message, (_size_in_bytes), #_passed_memory_ptr, message_suffix); \
        ((_passed_memory_ptr) = malloc_result_ptr);                                                                   \
})

/**
 * @brief Allocates and zeroes memory, logging the allocation result.
 *
 * A wrapper around `calloc` that logs the allocation's success or failure.
 * Includes contextual information such as the allocated size and the variable name.
 *
 * @param _passed_memory_ptr The variable to which the allocated memory is assigned (used for logging).
 * @param _size_in_bytes The number of bytes to allocate.
 *
 * @return A pointer to the allocated memory, or `NULL` if the allocation fails.
 *
 * @note Logs are output only if the allocator logger is enabled.
 */
#define CALLOC_LOG(_passed_memory_ptr, _size_in_bytes) ({                                                             \
        void *calloc_result_ptr = calloc((_size_in_bytes), 1);                                                        \
        const char *message = "Allocation of %d bytes to '%s'; %s";                                                   \
        enum log_tag log_tag = calloc_result_ptr ? LOG_INFO : LOG_ERROR;                                              \
        const char *message_suffix = calloc_result_ptr ? "SUCCEEDED" : "FAILED";                                      \
        if (logger_is_allocator_enabled())                                                                            \
                LOG_MESSAGE_NON_THREAD_SAFE(log_tag, message, (_size_in_bytes), #_passed_memory_ptr, message_suffix); \
        (_passed_memory_ptr = calloc_result_ptr);                                                                     \
})

/**
 * @brief Frees memory, nullifies the pointer, and logs the operation.
 *
 * A wrapper around `free` that ensures the pointer is set to `NULL` after being freed.
 * Logs an attempt to free a null pointer as a warning and logs successful deallocations.
 *
 * @param _memory_ptr The pointer to be freed and nullified.
 *
 * @note Logs are output only if the allocator logger is enabled.
 */
#define FREE_NULLIFY_LOG(_memory_ptr)                                                                    \
        do                                                                                               \
        {                                                                                                \
                if (logger_is_allocator_enabled() && (_memory_ptr) == NULL)                              \
                        LOG_WARNING("Attempting to free() '%s'; %s = NULL", #_memory_ptr, #_memory_ptr); \
                else                                                                                     \
                {                                                                                        \
                        free((_memory_ptr));                                                             \
                        (_memory_ptr) = NULL;                                                            \
                        if (logger_is_allocator_enabled())                                               \
                                LOG_INFO("Successful free() on '%s'; Pointer zeroed.", #_memory_ptr);    \
                }                                                                                        \
        } while (0)

#endif /* ALLOCATOR_H */
