#ifndef MINI_REDIS_DEMO_REGISTRY_H
#define MINI_REDIS_DEMO_REGISTRY_H

#include <stddef.h>
#include "status.h"

typedef struct registry registry_t;
typedef struct registry_node registry_node_t;

registry_t *registry_create(size_t _capacity, size_t _cache_size_limit);
void registry_destroy(registry_t **_registry_address);
status_t registry_append(registry_t *_registry, const char *_file_name);
status_t registry_pop(registry_t *_registry, const char *_file_name);
status_t registry_cache(registry_t *_registry, const char *_file_name);
const registry_node_t *registry_find(registry_t *_registry, const char *_file_name);

size_t registry_node_file_size(const registry_node_t *_registry_node);

#endif /* MINI_REDIS_DEMO_REGISTRY_H */
