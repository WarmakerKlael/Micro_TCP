#include <stdio.h>
#include "allocator/allocator_macros.h"
#include <errno.h>
#include "logging/microtcp_logger.h"
#include "smart_assert.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "status.h"
#include "miniredis_demo/registry.h"

static status_t registry_expand(registry_t *_registry);
static __always_inline status_t initialize_registry_node(registry_node_t *_registry_node, const char *_file_name);

registry_t *registry_create(const size_t _capacity, const size_t _cache_size_limit)
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
        registry->cache_size_limit = _cache_size_limit;
        registry->size = 0;
        registry->cached_bytes = 0;

        return registry;
}

void registry_destroy(registry_t **const _registry_address)
{
        SMART_ASSERT(_registry_address != NULL);
#define REGISTRY (*_registry_address)

        size_t registry_size = REGISTRY->size;
        for (size_t i = 0; i < registry_size; i++)
        {
                FREE_NULLIFY_LOG(REGISTRY->node_array[i].file_name);
                if (REGISTRY->node_array[i].cache_buffer != NULL) /* Cache buffer isn't Implemented.. Its an artifact of time. */
                        FREE_NULLIFY_LOG(REGISTRY->node_array[i].cache_buffer);
        }
        FREE_NULLIFY_LOG(REGISTRY->node_array);
        FREE_NULLIFY_LOG(REGISTRY);
#undef REGISTRY
}

status_t registry_append(registry_t *const _registry, const char *const _file_name)
{
        if (access(_file_name, F_OK) != 0) /* File doesn't exist. */
        {
                LOG_APP_ERROR("File: %s not found.", _file_name);
                return FAILURE;
        }

        if (_registry->size == _registry->capacity) /* Expansion needed. */
                if (registry_expand(_registry) == FAILURE)
                        return FAILURE;

        if (initialize_registry_node(&_registry->node_array[_registry->size], _file_name) == FAILURE)
                return FAILURE;
        _registry->size++;

        return SUCCESS;
}

status_t registry_pop(registry_t *_registry, const char *const _file_name)
{

        registry_node_t *registry_node_array = _registry->node_array;
        size_t registry_size = _registry->size;
        for (size_t i = 0; i < registry_size; i++)
        {
                registry_node_t *curr_node = registry_node_array + i;
                if (strcmp(curr_node->file_name, _file_name) != 0)
                        continue;
                FREE_NULLIFY_LOG(curr_node->file_name);
                FREE_NULLIFY_LOG(curr_node->cache_buffer);

                /* Swap with last node (works even with same nodes). */
                registry_node_t *last_node = registry_node_array + _registry->size - 1;
                curr_node->file_name = last_node->file_name;
                curr_node->file_size = last_node->file_size;
                curr_node->download_counter = last_node->download_counter;
                curr_node->time_of_arrival = last_node->time_of_arrival;
                curr_node->cache_buffer = last_node->cache_buffer;

                _registry->size--;
                return SUCCESS;
        }
        LOG_APP_WARNING_RETURN(FAILURE, "File: `%s` not found.", _file_name);
}

status_t registry_cache(registry_t *const _registry, const char *const _file_name)
{
        registry_node_t *const registry_node_array = _registry->node_array;
        size_t registry_size = _registry->size;
        for (size_t i = 0; i < registry_size; i++)
        {
                registry_node_t *curr_node = (registry_node_array + i);
                if (strcmp(curr_node->file_name, _file_name) != 0)
                        continue;
                if (curr_node->cache_buffer != NULL)
                        LOG_APP_WARNING_RETURN(FAILURE, "File: `%s` already cached.", _file_name);
                if (curr_node->file_size + _registry->cached_bytes > _registry->cache_size_limit)
                        LOG_APP_WARNING_RETURN(FAILURE, "File: `%s` can not be cached, not enough cache space.", _file_name);

                /*If here, we found file, and we can cache it. */
                FILE *file = fopen(_file_name, "rb");
                if (file == NULL)
                        LOG_APP_ERROR_RETURN(FAILURE, "File: `%s` failed to open. errno(%d): %s", _file_name, errno, strerror(errno));

                /* We proceed with caching. */
                curr_node->cache_buffer = MALLOC_LOG(curr_node->cache_buffer, curr_node->file_size);
                size_t read_size = fread(curr_node->cache_buffer, 1, curr_node->file_size, file);
                fclose(file);
                if (read_size != curr_node->file_size)
                {
                        FREE_NULLIFY_LOG(curr_node->cache_buffer);
                        LOG_APP_ERROR_RETURN(FAILURE, "File: `%s` failed load into cache.");
                }
                LOG_APP_INFO_RETURN(SUCCESS, "File: `%s` succeeded load into cache.");
        }
        LOG_APP_WARNING_RETURN(FAILURE, "File `%s` not found in registry.");
}

registry_node_t *registry_find(const registry_t *const _registry, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_registry != NULL, _file_name != NULL);
        registry_node_t *const registry_node_array = _registry->node_array;
        size_t registry_size = _registry->size;
        for (size_t i = 0; i < registry_size; i++)
                if (strcmp(registry_node_array[i].file_name, _file_name) == 0)
                        return &registry_node_array[i];
        return NULL;
}

size_t registry_node_file_size(const registry_node_t *const _registry_node)
{
        DEBUG_SMART_ASSERT(_registry_node != NULL);
        return _registry_node->file_size;
}

void registry_node_increment_get_count(registry_node_t *const _registry_node)
{
        DEBUG_SMART_ASSERT(_registry_node != NULL);
        _registry_node->download_counter++;
}

static __always_inline status_t initialize_registry_node(registry_node_t *const _registry_node, const char *const _file_name)
{
        struct stat stat_buffer;
        if (stat(_file_name, &stat_buffer) != 0)
                return FAILURE;

        _registry_node->download_counter = 0;
        _registry_node->cache_buffer = NULL;
        _registry_node->time_of_arrival = time(NULL);
        _registry_node->file_size = stat_buffer.st_size;

        /* Allocate memory to save file_name string. */
        _registry_node->file_name = MALLOC_LOG(_registry_node->file_name, strlen(_file_name) + 1); /* +1 for '\0' */
        strcpy(_registry_node->file_name, _file_name);
        return SUCCESS;
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
