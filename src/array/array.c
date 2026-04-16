#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "array.h"

#define ARRAY_INIT_CAPACITY 8
#define STACK_CHUNK_SIZE 32

typedef struct _array_t {
    size_t elem_size;
    size_t size;
    size_t capacity;
    void *arena;
    array_clone_fn clone;
    array_destroy_fn destroy;
    pthread_mutex_t mutex;
    size_t refcount;      // 1 for the array itself, +1 for each live slice
    int destroyed;        // set to 1 when array_destroy is called
} array_t;

typedef struct _array_slice_t {
    array_t *parent;
    size_t start;
    size_t size;
    size_t *indices; // NULL if identity mapping, else maps logical index to physical
} array_slice_t;

typedef struct _stack_t {
    array_t *array;
} stack_t;

static void array_ref(array_t *arr) {
    pthread_mutex_lock(&arr->mutex);
    arr->refcount++;
    pthread_mutex_unlock(&arr->mutex);
}

static void array_release(array_t *arr) {
    int do_free = 0;
    pthread_mutex_lock(&arr->mutex);
    if (--arr->refcount == 0) do_free = 1;
    pthread_mutex_unlock(&arr->mutex);
    if (do_free) {
        if (arr->destroy) {
            for (size_t i = 0; i < arr->size; ++i)
                arr->destroy((char*)arr->arena + i * arr->elem_size);
        }
        free(arr->arena);
        pthread_mutex_destroy(&arr->mutex);
        free(arr);
    }
}

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
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&arr->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    arr->refcount = 1;
    arr->destroyed = 0;
    return arr;
}

void array_destroy(array_t *arr) {
    if (!arr) return;
    pthread_mutex_lock(&arr->mutex);
    arr->destroyed = 1;
    pthread_mutex_unlock(&arr->mutex);
    array_release(arr);
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
    if (arr->destroyed) {
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
    if (arr->destroyed || index > arr->size) {
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
    if (dst->destroyed) {
        pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
        pthread_mutex_unlock(&dst->mutex);
        return false;
    }
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
    if (dst->destroyed) {
        pthread_mutex_unlock(&dst->mutex);
        return false;
    }
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
    if (dst->destroyed) {
        pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
        pthread_mutex_unlock(&dst->mutex);
        return false;
    }
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
    if (dst->destroyed) {
        pthread_mutex_unlock(&dst->mutex);
        return false;
    }
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
    if (arr->destroyed || index >= arr->size) {
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
    if (arr->destroyed || index >= arr->size || index + count > arr->size) {
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
    if (arr->destroyed) {
        pthread_mutex_unlock(&arr->mutex);
        return;
    }
    qsort(arr->arena, arr->size, arr->elem_size, cmp);
    pthread_mutex_unlock(&arr->mutex);
}

bool array_swap(array_t *arr, size_t i, size_t j) {
    if (!arr || i >= arr->size || j >= arr->size) return false;
    if (i == j) return true;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroyed) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    void *a = (char*)arr->arena + i * arr->elem_size;
    void *b = (char*)arr->arena + j * arr->elem_size;
    unsigned char tmp[256];
    void *buf = tmp;
    if (arr->elem_size > sizeof(tmp)) {
        buf = malloc(arr->elem_size);
        if (!buf) {
            pthread_mutex_unlock(&arr->mutex);
            return false;
        }
    }
    memcpy(buf, a, arr->elem_size);
    memcpy(a, b, arr->elem_size);
    memcpy(b, buf, arr->elem_size);
    if (buf != tmp) free(buf);
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_rotate_left(array_t *arr) {
    if (!arr || arr->size < 2) return false;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroyed) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    unsigned char tmp[256];
    void *buf = tmp;
    if (arr->elem_size > sizeof(tmp)) {
        buf = malloc(arr->elem_size);
        if (!buf) {
            pthread_mutex_unlock(&arr->mutex);
            return false;
        }
    }
    // Save the first element
    memcpy(buf, arr->arena, arr->elem_size);
    // Shift all elements down by one
    memmove(arr->arena, (char*)arr->arena + arr->elem_size, (arr->size - 1) * arr->elem_size);
    // Move the first element to the end
    memcpy((char*)arr->arena + (arr->size - 1) * arr->elem_size, buf, arr->elem_size);
    if (buf != tmp) free(buf);
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_rotate_right(array_t *arr) {
    if (!arr || arr->size < 2) return false;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroyed) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    unsigned char tmp[256];
    void *buf = tmp;
    if (arr->elem_size > sizeof(tmp)) {
        buf = malloc(arr->elem_size);
        if (!buf) {
            pthread_mutex_unlock(&arr->mutex);
            return false;
        }
    }
    // Save the last element
    memcpy(buf, (char*)arr->arena + (arr->size - 1) * arr->elem_size, arr->elem_size);
    // Shift all elements up by one
    memmove((char*)arr->arena + arr->elem_size, arr->arena, (arr->size - 1) * arr->elem_size);
    // Move the last element to the front
    memcpy(arr->arena, buf, arr->elem_size);
    if (buf != tmp) free(buf);
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

// --- Array accessors for stack delegation ---

static inline pthread_mutex_t *array_mutex(array_t *arr) { return arr ? &arr->mutex : NULL; }
static inline void array_set_size(array_t *arr, size_t sz) { if (arr) arr->size = sz; }
static inline size_t array_capacity(const array_t *arr) { return arr ? arr->capacity : 0; }
static inline void array_set_capacity(array_t *arr, size_t cap) { if (arr) arr->capacity = cap; }
static inline void *array_arena(const array_t *arr) { return arr ? arr->arena : NULL; }
static inline void array_set_arena(array_t *arr, void *arena) { if (arr) arr->arena = arena; }
static inline size_t array_elem_size(const array_t *arr) { return arr ? arr->elem_size : 0; }
static inline array_clone_fn array_clone_element_fn(const array_t *arr) { return arr ? arr->clone : NULL; }

/* --- Slicing --- */

static size_t array_slice_real_index(const array_slice_t *slice, size_t index) {
    return slice->indices ? slice->indices[index] : (slice->start + index);
}

void *array_slice_get(const array_slice_t *slice, size_t index) {
    if (!slice || !slice->parent) return NULL;
    pthread_mutex_lock(array_mutex(slice->parent));
    if (slice->parent->destroyed || index >= slice->size || slice->start + index >= array_size(slice->parent)) {
        pthread_mutex_unlock(array_mutex(slice->parent));
        return NULL;
    }
    size_t real_index = array_slice_real_index(slice, index);
    void *result = (char*)array_arena(slice->parent) + real_index * array_elem_size(slice->parent);
    pthread_mutex_unlock(array_mutex(slice->parent));
    return result;
}

void array_slice_destroy(array_slice_t *slice) {
    if (!slice) return;
    free(slice->indices);
    array_t *parent = slice->parent;
    free(slice);
    array_release(parent);
}

static void array_slice_ensure_indices(array_slice_t *slice) {
    if (!slice || !slice->parent) return;
    pthread_mutex_lock(array_mutex(slice->parent));
    if (!slice->indices) {
        slice->indices = malloc(sizeof(size_t) * slice->size);
        for (size_t i = 0; i < slice->size; ++i)
            slice->indices[i] = slice->start + i;
    }
    pthread_mutex_unlock(array_mutex(slice->parent));
}

struct cmp_ctx {
    const void *arena;
    size_t elem_size;
    array_cmp_fn cmp;
};
static struct cmp_ctx g_cmp_ctx;

static int cmp_indices_static(const void *a, const void *b) {
    struct cmp_ctx *c = &g_cmp_ctx;
    size_t ia = *(const size_t*)a, ib = *(const size_t*)b;
    const void *pa = (const char*)c->arena + ia * c->elem_size;
    const void *pb = (const char*)c->arena + ib * c->elem_size;
    return c->cmp(pa, pb);
}

void array_slice_sort(array_slice_t *slice, array_cmp_fn cmp) {
    if (!slice || !cmp || slice->size < 2) return;
    array_slice_ensure_indices(slice);
    pthread_mutex_lock(array_mutex(slice->parent));
    g_cmp_ctx.arena = array_arena(slice->parent);
    g_cmp_ctx.elem_size = array_elem_size(slice->parent);
    g_cmp_ctx.cmp = cmp;
    qsort(slice->indices, slice->size, sizeof(size_t), cmp_indices_static);
    pthread_mutex_unlock(array_mutex(slice->parent));
}

bool array_slice_swap(array_slice_t *slice, size_t i, size_t j) {
    if (!slice || !slice->parent) return false;
    pthread_mutex_lock(array_mutex(slice->parent));
    if (i >= slice->size || j >= slice->size) {
        pthread_mutex_unlock(array_mutex(slice->parent));
        return false;
    }
    if (i == j) {
        pthread_mutex_unlock(array_mutex(slice->parent));
        return true;
    }
    array_slice_ensure_indices(slice);
    size_t tmp = slice->indices[i];
    slice->indices[i] = slice->indices[j];
    slice->indices[j] = tmp;
    pthread_mutex_unlock(array_mutex(slice->parent));
    return true;
}

bool array_slice_rotate_left(array_slice_t *slice) {
    if (!slice || !slice->parent) return false;
    array_t *arr = slice->parent;
    pthread_mutex_lock(array_mutex(arr));
    if (slice->size < 2) {
        pthread_mutex_unlock(array_mutex(arr));
        return false;
    }
    array_slice_ensure_indices(slice);
    size_t first = slice->indices[0];
    memmove(&slice->indices[0], &slice->indices[1], (slice->size - 1) * sizeof(size_t));
    slice->indices[slice->size - 1] = first;
    pthread_mutex_unlock(array_mutex(arr));
    return true;
}

bool array_slice_rotate_right(array_slice_t *slice) {
    if (!slice || !slice->parent) return false;
    array_t *arr = slice->parent;
    pthread_mutex_lock(array_mutex(arr));
    if (slice->size < 2) {
        pthread_mutex_unlock(array_mutex(arr));
        return false;
    }
    array_slice_ensure_indices(slice);
    size_t last = slice->indices[slice->size - 1];
    memmove(&slice->indices[1], &slice->indices[0], (slice->size - 1) * sizeof(size_t));
    slice->indices[0] = last;
    pthread_mutex_unlock(array_mutex(arr));
    return true;
}

array_t *array_from_slice(const array_slice_t *slice, array_clone_fn clone, array_destroy_fn destroy) {
    if (!slice || !slice->parent) return NULL;
    array_t *arr = array_create(array_elem_size(slice->parent), clone, destroy);
    if (!arr) return NULL;
    if (slice->size == 0) return arr;
    for (size_t i = 0; i < slice->size; ++i) {
        const void *src_elem = array_slice_get(slice, i);
        if (!array_add(arr, src_elem)) {
            array_destroy(arr);
            return NULL;
        }
    }
    return arr;
}

array_slice_t *array_slice(const array_t *arr, size_t start, size_t count) {
    if (!arr) return NULL;
    pthread_mutex_lock(array_mutex((array_t *)arr));
    if (arr->destroyed || start >= array_size(arr)) {
        pthread_mutex_unlock(array_mutex((array_t *)arr));
        return NULL;
    }
    if (start + count > array_size(arr)) count = array_size(arr) - start;
    array_ref((array_t *)arr);
    array_slice_t *slice = malloc(sizeof(array_slice_t));
    if (!slice) {
        array_release((array_t *)arr);
        pthread_mutex_unlock(array_mutex((array_t *)arr));
        return NULL;
    }
    slice->parent = (array_t *)arr;
    slice->start = start;
    slice->size = count;
    slice->indices = NULL;
    pthread_mutex_unlock(array_mutex((array_t *)arr));
    return slice;
}

size_t array_slice_size(const array_slice_t *slice) {
    if (!slice) return 0;
    pthread_mutex_lock(array_mutex(slice->parent));
    size_t sz = slice->size;
    pthread_mutex_unlock(array_mutex(slice->parent));
    return sz;
}

size_t array_slice_elem_size(const array_slice_t *slice) {
    if (!slice) return 0;
    pthread_mutex_lock(array_mutex(slice->parent));
    size_t sz = array_elem_size(slice->parent);
    pthread_mutex_unlock(array_mutex(slice->parent));
    return sz;
}

array_slice_t *array_slice_subslice(const array_slice_t *slice, size_t start, size_t count) {
    if (!slice || !slice->parent) return NULL;
    array_t *arr = slice->parent;
    pthread_mutex_lock(array_mutex(arr));
    if (start >= slice->size) {
        pthread_mutex_unlock(array_mutex(arr));
        return NULL;
    }
    if (start + count > slice->size) count = slice->size - start;
    array_ref(arr);
    array_slice_t *sub = malloc(sizeof(array_slice_t));
    if (!sub) {
        array_release(arr);
        pthread_mutex_unlock(array_mutex(arr));
        return NULL;
    }
    sub->parent = arr;
    sub->start = slice->indices ? slice->indices[start] : (slice->start + start);
    sub->size = count;
    if (slice->indices) {
        sub->indices = malloc(sizeof(size_t) * count);
        if (!sub->indices) {
            free(sub);
            array_release(arr);
            pthread_mutex_unlock(array_mutex(arr));
            return NULL;
        }
        for (size_t i = 0; i < count; ++i)
            sub->indices[i] = slice->indices[start + i];
    } else {
        sub->indices = NULL;
    }
    pthread_mutex_unlock(array_mutex(arr));
    return sub;
}

stack_t *stack_create(size_t elem_size, array_clone_fn clone, array_destroy_fn destroy) {
    stack_t *s = malloc(sizeof(stack_t));
    if (!s) return NULL;
    s->array = array_create(elem_size, clone, destroy);
    if (!s->array) {
        free(s);
        return NULL;
    }
    return s;
}

void stack_destroy(stack_t *s) {
    if (!s) return;
    array_destroy(s->array);
    free(s);
}

void *stack_pop(stack_t *s) {
    if (!s) return NULL;
    pthread_mutex_t *mutex = array_mutex(s->array);
    pthread_mutex_lock(mutex);
    size_t sz = array_size(s->array);
    if (sz == 0) {
        pthread_mutex_unlock(mutex);
        return NULL;
    }
    size_t elem_size = array_elem_size(s->array);
    void *buf = malloc(elem_size);
    if (!buf) {
        pthread_mutex_unlock(mutex);
        return NULL;
    }
    void *src = (char*)array_arena(s->array) + (sz - 1) * elem_size;
    memcpy(buf, src, elem_size);

    array_set_size(s->array, sz - 1);

    // Shrink arena if too many unused slots
    size_t unused = array_capacity(s->array) - array_size(s->array);
    if (unused > STACK_CHUNK_SIZE) {
        size_t new_cap = array_size(s->array) + STACK_CHUNK_SIZE;
        void *new_arena = realloc(array_arena(s->array), new_cap * elem_size);
        if (new_arena) {
            array_set_arena(s->array, new_arena);
            array_set_capacity(s->array, new_cap);
        }
    }

    pthread_mutex_unlock(mutex);
    return buf;
}

bool stack_push(stack_t *s, const void *elem) {
    if (!s) return false;
    pthread_mutex_t *mutex = array_mutex(s->array);
    pthread_mutex_lock(mutex);

    // Grow by STACK_CHUNK_SIZE if needed
    if (array_size(s->array) == array_capacity(s->array)) {
        size_t new_cap = array_capacity(s->array) ? array_capacity(s->array) + STACK_CHUNK_SIZE : ARRAY_INIT_CAPACITY;
        void *new_arena = realloc(array_arena(s->array), new_cap * array_elem_size(s->array));
        if (!new_arena) {
            pthread_mutex_unlock(mutex);
            return false;
        }
        array_set_arena(s->array, new_arena);
        array_set_capacity(s->array, new_cap);
    }
    void *dst = (char*)array_arena(s->array) + array_size(s->array) * array_elem_size(s->array);
    array_clone_fn clone_element = array_clone_element_fn(s->array);
    if (clone_element)
        clone_element(dst, elem);
    else
        memcpy(dst, elem, array_elem_size(s->array));
    array_set_size(s->array, array_size(s->array) + 1);
    pthread_mutex_unlock(mutex);
    return true;
}

