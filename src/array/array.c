#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "array.h"

struct array {
    size_t elem_size;
    size_t size;
    size_t capacity;
    void *arena;
    array_clone_fn clone;
    array_destroy_fn destroy;
    pthread_mutex_t mutex;
};

#define ARRAY_INIT_CAPACITY 8

/* --- Lifecycle --- */
array_t *array_create(size_t elem_size,
                      array_clone_fn clone,
                      array_destroy_fn destroy)
{
    if (elem_size == 0) return NULL;
    array_t *arr = malloc(sizeof(array_t));
    if (!arr) return NULL;
    arr->elem_size = elem_size;
    arr->size = 0;
    arr->capacity = ARRAY_INIT_CAPACITY;
    arr->arena = malloc(arr->capacity * elem_size);
    if (!arr->arena) {
        free(arr);
        return NULL;
    }
    arr->clone = clone;
    arr->destroy = destroy;
    pthread_mutex_init(&arr->mutex, NULL);
    return arr;
}

void array_destroy(array_t *arr) {
    if (!arr) return;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroy) {
        for (size_t i = 0; i < arr->size; ++i)
            arr->destroy((char*)arr->arena + i * arr->elem_size);
    }
    free(arr->arena);
    pthread_mutex_unlock(&arr->mutex);
    pthread_mutex_destroy(&arr->mutex);
    free(arr);
}

void array_clear(array_t *arr) {
    if (!arr) return;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroy) {
        for (size_t i = 0; i < arr->size; ++i)
            arr->destroy((char*)arr->arena + i * arr->elem_size);
    }
    arr->size = 0;
    pthread_mutex_unlock(&arr->mutex);
}

/* --- Capacity/size --- */
size_t array_size(const array_t *arr) {
    if (!arr) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&arr->mutex);
    size_t sz = arr->size;
    pthread_mutex_unlock((pthread_mutex_t *)&arr->mutex);
    return sz;
}

/* --- Element access --- */
void *array_get(const array_t *arr, size_t index) {
    if (!arr) return NULL;
    pthread_mutex_lock((pthread_mutex_t *)&arr->mutex);
    void *result = NULL;
    if (index < arr->size)
        result = (char*)arr->arena + index * arr->elem_size;
    pthread_mutex_unlock((pthread_mutex_t *)&arr->mutex);
    return result;
}

/* --- Mutation: add, insert, remove, bulk ops --- */
bool array_add(array_t *arr, const void *elem) {
    if (!arr || !elem) return false;
    pthread_mutex_lock(&arr->mutex);
    if (arr->size == arr->capacity) {
        size_t new_cap = arr->capacity ? arr->capacity * 2 : ARRAY_INIT_CAPACITY;
        void *new_arena = realloc(arr->arena, new_cap * arr->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock(&arr->mutex);
            return false;
        }
        arr->arena = new_arena;
        arr->capacity = new_cap;
    }
    void *dst = (char*)arr->arena + arr->size * arr->elem_size;
    if (arr->clone)
        arr->clone(dst, elem);
    else
        memcpy(dst, elem, arr->elem_size);
    arr->size += 1;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_insert(array_t *arr, size_t index, const void *elem) {
    if (!arr || !elem) return false;
    pthread_mutex_lock(&arr->mutex);
    if (index > arr->size) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    if (arr->size == arr->capacity) {
        size_t new_cap = arr->capacity ? arr->capacity * 2 : ARRAY_INIT_CAPACITY;
        void *new_arena = realloc(arr->arena, new_cap * arr->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock(&arr->mutex);
            return false;
        }
        arr->arena = new_arena;
        arr->capacity = new_cap;
    }
    if (index < arr->size) {
        memmove((char*)arr->arena + (index + 1) * arr->elem_size,
                (char*)arr->arena + index * arr->elem_size,
                (arr->size - index) * arr->elem_size);
    }
    void *dst = (char*)arr->arena + index * arr->elem_size;
    if (arr->clone)
        arr->clone(dst, elem);
    else
        memcpy(dst, elem, arr->elem_size);
    arr->size += 1;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_insert_array(array_t *dst, size_t index, const array_t *src) {
    if (!dst || !src || dst->elem_size != src->elem_size) return false;
    if (index > dst->size) return false;
    if (src->size == 0) return true;
    pthread_mutex_lock(&dst->mutex);
    pthread_mutex_lock((pthread_mutex_t *)&src->mutex);
    size_t needed = dst->size + src->size;
    if (needed > dst->capacity) {
        size_t new_cap = dst->capacity ? dst->capacity * 2 : ARRAY_INIT_CAPACITY;
        while (new_cap < needed) new_cap *= 2;
        void *new_arena = realloc(dst->arena, new_cap * dst->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
            pthread_mutex_unlock(&dst->mutex);
            return false;
        }
        dst->arena = new_arena;
        dst->capacity = new_cap;
    }
    // Move elements up to make space
    if (index < dst->size) {
        memmove((char*)dst->arena + (index + src->size) * dst->elem_size,
                (char*)dst->arena + index * dst->elem_size,
                (dst->size - index) * dst->elem_size);
    }
    // Copy/clone new elements in
    for (size_t i = 0; i < src->size; ++i) {
        void *src_elem = (char*)src->arena + i * src->elem_size;
        void *dst_elem = (char*)dst->arena + (index + i) * dst->elem_size;
        if (dst->clone)
            dst->clone(dst_elem, src_elem);
        else
            memcpy(dst_elem, src_elem, dst->elem_size);
    }
    dst->size += src->size;
    pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
    pthread_mutex_unlock(&dst->mutex);
    return true;
}

bool array_insert_carray(array_t *dst, size_t index, const void *src, size_t n) {
    if (!dst || !src || n == 0) return false;
    if (index > dst->size) return false;
    pthread_mutex_lock(&dst->mutex);
    size_t needed = dst->size + n;
    if (needed > dst->capacity) {
        size_t new_cap = dst->capacity ? dst->capacity * 2 : ARRAY_INIT_CAPACITY;
        while (new_cap < needed) new_cap *= 2;
        void *new_arena = realloc(dst->arena, new_cap * dst->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock(&dst->mutex);
            return false;
        }
        dst->arena = new_arena;
        dst->capacity = new_cap;
    }
    // Move elements up to make space
    if (index < dst->size) {
        memmove((char*)dst->arena + (index + n) * dst->elem_size,
                (char*)dst->arena + index * dst->elem_size,
                (dst->size - index) * dst->elem_size);
    }
    // Copy/clone new elements in
    for (size_t i = 0; i < n; ++i) {
        const void *src_elem = (const char*)src + i * dst->elem_size;
        void *dst_elem = (char*)dst->arena + (index + i) * dst->elem_size;
        if (dst->clone)
            dst->clone(dst_elem, src_elem);
        else
            memcpy(dst_elem, src_elem, dst->elem_size);
    }
    dst->size += n;
    pthread_mutex_unlock(&dst->mutex);
    return true;
}

bool array_append_array(array_t *dst, const array_t *src) {
    if (!dst || !src || dst->elem_size != src->elem_size) return false;
    if (src->size == 0) return true;
    pthread_mutex_lock(&dst->mutex);
    pthread_mutex_lock((pthread_mutex_t *)&src->mutex);
    size_t needed = dst->size + src->size;
    if (needed > dst->capacity) {
        size_t new_cap = dst->capacity ? dst->capacity * 2 : ARRAY_INIT_CAPACITY;
        while (new_cap < needed) new_cap *= 2;
        void *new_arena = realloc(dst->arena, new_cap * dst->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
            pthread_mutex_unlock(&dst->mutex);
            return false;
        }
        dst->arena = new_arena;
        dst->capacity = new_cap;
    }
    for (size_t i = 0; i < src->size; ++i) {
        void *src_elem = (char*)src->arena + i * src->elem_size;
        void *dst_elem = (char*)dst->arena + (dst->size + i) * dst->elem_size;
        if (dst->clone)
            dst->clone(dst_elem, src_elem);
        else
            memcpy(dst_elem, src_elem, dst->elem_size);
    }
    dst->size += src->size;
    pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
    pthread_mutex_unlock(&dst->mutex);
    return true;
}

bool array_append_carray(array_t *dst, const void *src, size_t n) {
    if (!dst || !src || n == 0) return false;
    pthread_mutex_lock(&dst->mutex);
    size_t needed = dst->size + n;
    if (needed > dst->capacity) {
        size_t new_cap = dst->capacity ? dst->capacity * 2 : ARRAY_INIT_CAPACITY;
        while (new_cap < needed) new_cap *= 2;
        void *new_arena = realloc(dst->arena, new_cap * dst->elem_size);
        if (!new_arena) {
            pthread_mutex_unlock(&dst->mutex);
            return false;
        }
        dst->arena = new_arena;
        dst->capacity = new_cap;
    }
    for (size_t i = 0; i < n; ++i) {
        const void *src_elem = (const char*)src + i * dst->elem_size;
        void *dst_elem = (char*)dst->arena + (dst->size + i) * dst->elem_size;
        if (dst->clone)
            dst->clone(dst_elem, src_elem);
        else
            memcpy(dst_elem, src_elem, dst->elem_size);
    }
    dst->size += n;
    pthread_mutex_unlock(&dst->mutex);
    return true;
}

bool array_remove(array_t *arr, size_t index) {
    if (!arr) return false;
    pthread_mutex_lock(&arr->mutex);
    if (index >= arr->size) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    void *elem_ptr = (char*)arr->arena + index * arr->elem_size;
    if (arr->destroy)
        arr->destroy(elem_ptr);
    if (index < arr->size - 1) {
        memmove(elem_ptr,
                (char*)arr->arena + (index + 1) * arr->elem_size,
                (arr->size - index - 1) * arr->elem_size);
    }
    arr->size -= 1;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_remove_elements(array_t *arr, size_t index, size_t count) {
    if (!arr || count == 0) return false;
    pthread_mutex_lock(&arr->mutex);
    if (index >= arr->size || index + count > arr->size) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    // Destroy elements if needed
    if (arr->destroy) {
        for (size_t i = 0; i < count; ++i)
            arr->destroy((char*)arr->arena + (index + i) * arr->elem_size);
    }
    // Move elements down to fill the gap
    if (index + count < arr->size) {
        memmove((char*)arr->arena + index * arr->elem_size,
                (char*)arr->arena + (index + count) * arr->elem_size,
                (arr->size - index - count) * arr->elem_size);
    }
    arr->size -= count;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

/* --- Sorting --- */
void array_sort(array_t *arr, array_cmp_fn cmp) {
    if (!arr || !cmp || arr->size < 2) return;
    pthread_mutex_lock(&arr->mutex);
    qsort(arr->arena, arr->size, arr->elem_size, cmp);
    pthread_mutex_unlock(&arr->mutex);
}