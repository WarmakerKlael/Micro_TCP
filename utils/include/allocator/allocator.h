#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "logging/logger.h"

#define MALLOC_LOG(_passed_memory_ptr, _size_in_bytes) ({                                         \
        void *_malloc_result_ptr = malloc(_size_in_bytes);                                        \
        const char *message = "Allocation of %d bytes to '%s'; %s";                               \
        log_tag_t log_tag = _malloc_result_ptr ? LOG_INFO : LOG_ERROR;                            \
        const char *message_suffix = _malloc_result_ptr ? "SUCCEEDED" : "FAILED";                 \
        _UNSAFE_PRINT_LOG(log_tag, message, _size_in_bytes, #_passed_memory_ptr, message_suffix); \
        (_malloc_result_ptr);                                                                     \
})

#define FREE_NULLIFY_LOG(_memory_ptr)                                                                              \
        do                                                                                                 \
        {                                                                                                  \
                if (_memory_ptr == NULL)                                                                   \
                        PRINT_WARNING("Attempting to free() '%s'; %s = NULL", #_memory_ptr, #_memory_ptr); \
                else                                                                                       \
                {                                                                                          \
                        free(_memory_ptr);                                                                 \
                        _memory_ptr = NULL;                                                                \
                        PRINT_INFO("Successful free() on '%s'; Pointer zeroed.", #_memory_ptr);            \
                }                                                                                          \
        } while (0)

#endif /* ALLOCATOR_H */