#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#include "../logging/logger.h"
#define STRINGIFY(_var_name) #_var_name

#define MALLOC_LOG(_memory_ptr, _size_in_bytes)                                                              \
        do                                                                                                   \
        {                                                                                                    \
                _memory_ptr = malloc(_size_in_bytes);                                                        \
                const char *message = "Allocation of %d bytes to %s %s.";                                     \
                log_tag_t log_tag = _memory_ptr ? LOG_INFO : LOG_ERROR;                                      \
                const char *message_suffix = _memory_ptr ? "succeeded" : "failed";                           \
                _UNSAFE_PRINT_LOG(log_tag, message, _size_in_bytes, STRINGIFY(_memory_ptr), message_suffix); \
        } while (0)

#endif /* ALLOCATOR_H */