#include <stdio.h>
#include "allocator/allocator_macros.h"
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "status.h"
#include "registry.h"

typedef struct
{
        char *file_name; /* Node KEY */
        size_t file_size;
        size_t download_count;
        time_t time_of_storage;
        _Bool cached;
} registry_node_t;

struct registry
{
        registry_node_t *node_array;
        size_t size;
        size_t capacity;
};

static status_t registry_expand(registry_t *_registry);
static __always_inline void initialize_registry_node(registry_node_t *_registry_node, const char *_file_name);

registry_t *registry_create(const size_t _capacity)
{
        registry_t *registry = MALLOC_LOG(registry, sizeof(registry_t));
        if (registry == NULL)
                return NULL;
        registry->node_array = CALLOC_LOG(registry->node_array, sizeof(registry_node_t) * _capacity);
        if (registry->node_array == NULL)
        {
                free(registry);
                return NULL;
        }
        registry->capacity = _capacity;
        registry->size = 0;

        return registry;
}

void registry_destroy(registry_t **_registry_address)
{
        SMART_ASSERT(_registry_address != NULL);
#define REGISTRY (*_registry_address)
        FREE_NULLIFY_LOG(REGISTRY->node_array);
        FREE_NULLIFY_LOG(REGISTRY);
#undef REGISTRY
}

status_t registry_append(registry_t *_registry, const char *const _file_name)
{
        if (access(_file_name, F_OK) != 0) /* File doesn't exist. */
        {
                LOG_ERROR("Unable to access `_file_name`, append failed.");
                return FAILURE;
        }

        if (_registry->size == _registry->capacity) /* Expansion needed. */
                registry_expand(_registry);

        initialize_registry_node(&_registry->node_array[_registry->size], _file_name);
        return SUCCESS;
}

static __always_inline void initialize_registry_node(registry_node_t *const _registry_node, const char *const _file_name)
{
        struct stat stat_buffer;
        DEBUG_SMART_ASSERT(stat(_file_name, &stat_buffer) == 0);

        _registry_node->cached = false;
        _registry_node->download_count = 0;
        _registry_node->time_of_storage = time(NULL);
        _registry_node->file_size = stat_buffer.st_size;

        /* Allocate memory to save file_name string. */
        _registry_node->file_name = MALLOC_LOG(_registry_node->file_name, strlen(_file_name) + 1); /* +1 for '\0' */
        strcpy(_registry_node->file_name, _file_name);
}

static status_t registry_expand(registry_t *const _registry)
{
#define EXPANSION_MULTIPLIER 2
        SMART_ASSERT(_registry->size == _registry->capacity);
        const size_t new_capacity = _registry->capacity * EXPANSION_MULTIPLIER;

        registry_node_t *new_node_array = CALLOC_LOG(new_node_array, sizeof(registry_node_t) * new_capacity);
        if (new_node_array == NULL)
                return FAILURE;
        memcpy(new_node_array, _registry->node_array, _registry->capacity * sizeof(registry_node_t));
        FREE_NULLIFY_LOG(_registry->node_array);
        _registry->node_array = new_node_array;
        return SUCCESS;
#undef EXPANSION_MULTIPLIER
}
