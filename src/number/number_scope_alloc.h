#ifndef NUMBER_SCOPE_ALLOC_INTERNAL_H
#define NUMBER_SCOPE_ALLOC_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

void *number_scope_mem_alloc(size_t size, size_t align);
void *number_scope_mem_calloc(size_t count, size_t size, size_t align);
void *number_scope_mem_alloc_heap(size_t size, size_t align);
void *number_scope_mem_calloc_heap(size_t count, size_t size, size_t align);
void *number_scope_mem_realloc(void *ptr, size_t size, size_t align);
void number_scope_mem_free(void *ptr);
bool number_scope_mem_is_arena_ptr(const void *ptr);

#endif
