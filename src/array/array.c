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
    size_t refcount;
    int destroyed;
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

/* --- Internal helpers (caller must hold arr->mutex where noted) --- */

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

// Requires arr->mutex held.
static bool array_grow_to(array_t *arr, size_t needed) {
    if (needed <= arr->capacity) return true;
    size_t new_cap = arr->capacity ? arr->capacity * 2 : ARRAY_INIT_CAPACITY;
    while (new_cap < needed) new_cap *= 2;
    void *new_arena = realloc(arr->arena, new_cap * arr->elem_size);
    if (!new_arena) return false;
    arr->arena = new_arena;
    arr->capacity = new_cap;
    return true;
}

// Requires arr->mutex held.
static void array_copy_elems(array_t *arr, size_t dst_idx, const void *src, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const void *s = (const char*)src + i * arr->elem_size;
        void *d = (char*)arr->arena + (dst_idx + i) * arr->elem_size;
        if (arr->clone)
            arr->clone(d, s);
        else
            memcpy(d, s, arr->elem_size);
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
    if (arr->destroyed || !array_grow_to(arr, arr->size + 1)) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    array_copy_elems(arr, arr->size, elem, 1);
    arr->size++;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_insert(array_t *arr, size_t index, const void *elem) {
    if (!arr || !elem) return false;
    pthread_mutex_lock(&arr->mutex);
    if (arr->destroyed || index > arr->size || !array_grow_to(arr, arr->size + 1)) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    if (index < arr->size) {
        memmove((char*)arr->arena + (index + 1) * arr->elem_size,
                (char*)arr->arena + index * arr->elem_size,
                (arr->size - index) * arr->elem_size);
    }
    array_copy_elems(arr, index, elem, 1);
    arr->size++;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_insert_array(array_t *dst, size_t index, const array_t *src) {
    if (!dst || !src || dst->elem_size != src->elem_size) return false;
    if (index > dst->size) return false;
    if (src->size == 0) return true;
    pthread_mutex_lock(&dst->mutex);
    pthread_mutex_lock((pthread_mutex_t *)&src->mutex);
    bool ok = !dst->destroyed && array_grow_to(dst, dst->size + src->size);
    if (ok) {
        if (index < dst->size) {
            memmove((char*)dst->arena + (index + src->size) * dst->elem_size,
                    (char*)dst->arena + index * dst->elem_size,
                    (dst->size - index) * dst->elem_size);
        }
        array_copy_elems(dst, index, src->arena, src->size);
        dst->size += src->size;
    }
    pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
    pthread_mutex_unlock(&dst->mutex);
    return ok;
}

bool array_insert_carray(array_t *dst, size_t index, const void *src, size_t n) {
    if (!dst || !src || n == 0) return false;
    if (index > dst->size) return false;
    pthread_mutex_lock(&dst->mutex);
    bool ok = !dst->destroyed && array_grow_to(dst, dst->size + n);
    if (ok) {
        if (index < dst->size) {
            memmove((char*)dst->arena + (index + n) * dst->elem_size,
                    (char*)dst->arena + index * dst->elem_size,
                    (dst->size - index) * dst->elem_size);
        }
        array_copy_elems(dst, index, src, n);
        dst->size += n;
    }
    pthread_mutex_unlock(&dst->mutex);
    return ok;
}

bool array_append_array(array_t *dst, const array_t *src) {
    if (!dst || !src || dst->elem_size != src->elem_size) return false;
    if (src->size == 0) return true;
    pthread_mutex_lock(&dst->mutex);
    pthread_mutex_lock((pthread_mutex_t *)&src->mutex);
    bool ok = !dst->destroyed && array_grow_to(dst, dst->size + src->size);
    if (ok) {
        array_copy_elems(dst, dst->size, src->arena, src->size);
        dst->size += src->size;
    }
    pthread_mutex_unlock((pthread_mutex_t *)&src->mutex);
    pthread_mutex_unlock(&dst->mutex);
    return ok;
}

bool array_append_carray(array_t *dst, const void *src, size_t n) {
    if (!dst || !src || n == 0) return false;
    pthread_mutex_lock(&dst->mutex);
    bool ok = !dst->destroyed && array_grow_to(dst, dst->size + n);
    if (ok) {
        array_copy_elems(dst, dst->size, src, n);
        dst->size += n;
    }
    pthread_mutex_unlock(&dst->mutex);
    return ok;
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
    arr->size--;
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
    if (arr->destroy) {
        for (size_t i = 0; i < count; ++i)
            arr->destroy((char*)arr->arena + (index + i) * arr->elem_size);
    }
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
    if (!arr->destroyed)
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
    void *buf = arr->elem_size <= sizeof(tmp) ? tmp : malloc(arr->elem_size);
    if (!buf) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
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
    void *buf = arr->elem_size <= sizeof(tmp) ? tmp : malloc(arr->elem_size);
    if (!buf) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    memcpy(buf, arr->arena, arr->elem_size);
    memmove(arr->arena, (char*)arr->arena + arr->elem_size, (arr->size - 1) * arr->elem_size);
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
    void *buf = arr->elem_size <= sizeof(tmp) ? tmp : malloc(arr->elem_size);
    if (!buf) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    memcpy(buf, (char*)arr->arena + (arr->size - 1) * arr->elem_size, arr->elem_size);
    memmove((char*)arr->arena + arr->elem_size, arr->arena, (arr->size - 1) * arr->elem_size);
    memcpy(arr->arena, buf, arr->elem_size);
    if (buf != tmp) free(buf);
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

/* --- Slicing --- */

static size_t array_slice_real_index(const array_slice_t *slice, size_t index) {
    return slice->indices ? slice->indices[index] : (slice->start + index);
}

// Requires slice->parent->mutex held.
static void array_slice_materialize_indices(array_slice_t *slice) {
    if (slice->indices) return;
    slice->indices = malloc(sizeof(size_t) * slice->size);
    if (!slice->indices) return;
    for (size_t i = 0; i < slice->size; ++i)
        slice->indices[i] = slice->start + i;
}

void *array_slice_get(const array_slice_t *slice, size_t index) {
    if (!slice || !slice->parent) return NULL;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    void *result = NULL;
    if (!arr->destroyed && index < slice->size && (slice->start + index) < arr->size) {
        size_t real_index = array_slice_real_index(slice, index);
        result = (char*)arr->arena + real_index * arr->elem_size;
    }
    pthread_mutex_unlock(&arr->mutex);
    return result;
}

void array_slice_destroy(array_slice_t *slice) {
    if (!slice) return;
    free(slice->indices);
    array_t *parent = slice->parent;
    free(slice);
    array_release(parent);
}

struct cmp_ctx {
    const void *arena;
    size_t elem_size;
    array_cmp_fn cmp;
};

static int cmp_indices_r(const void *a, const void *b, void *ctx) {
    struct cmp_ctx *c = ctx;
    size_t ia = *(const size_t*)a, ib = *(const size_t*)b;
    return c->cmp((const char*)c->arena + ia * c->elem_size,
                  (const char*)c->arena + ib * c->elem_size);
}

void array_slice_sort(array_slice_t *slice, array_cmp_fn cmp) {
    if (!slice || !cmp || slice->size < 2) return;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    array_slice_materialize_indices(slice);
    struct cmp_ctx ctx = { arr->arena, arr->elem_size, cmp };
    qsort_r(slice->indices, slice->size, sizeof(size_t), cmp_indices_r, &ctx);
    pthread_mutex_unlock(&arr->mutex);
}

bool array_slice_swap(array_slice_t *slice, size_t i, size_t j) {
    if (!slice || !slice->parent) return false;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    if (i >= slice->size || j >= slice->size) {
        pthread_mutex_unlock(&arr->mutex);
        return false;
    }
    if (i != j) {
        array_slice_materialize_indices(slice);
        size_t tmp = slice->indices[i];
        slice->indices[i] = slice->indices[j];
        slice->indices[j] = tmp;
    }
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_slice_rotate_left(array_slice_t *slice) {
    if (!slice || !slice->parent || slice->size < 2) return false;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    array_slice_materialize_indices(slice);
    size_t first = slice->indices[0];
    memmove(&slice->indices[0], &slice->indices[1], (slice->size - 1) * sizeof(size_t));
    slice->indices[slice->size - 1] = first;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

bool array_slice_rotate_right(array_slice_t *slice) {
    if (!slice || !slice->parent || slice->size < 2) return false;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    array_slice_materialize_indices(slice);
    size_t last = slice->indices[slice->size - 1];
    memmove(&slice->indices[1], &slice->indices[0], (slice->size - 1) * sizeof(size_t));
    slice->indices[0] = last;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}

array_t *array_from_slice(const array_slice_t *slice, array_clone_fn clone, array_destroy_fn destroy) {
    if (!slice || !slice->parent) return NULL;
    array_t *arr = array_create(slice->parent->elem_size, clone, destroy);
    if (!arr) return NULL;
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
    array_t *a = (array_t *)arr;
    pthread_mutex_lock(&a->mutex);
    if (a->destroyed || start >= a->size) {
        pthread_mutex_unlock(&a->mutex);
        return NULL;
    }
    if (start + count > a->size) count = a->size - start;
    array_ref(a);
    array_slice_t *slice = malloc(sizeof(array_slice_t));
    if (!slice) {
        array_release(a);
        pthread_mutex_unlock(&a->mutex);
        return NULL;
    }
    slice->parent = a;
    slice->start = start;
    slice->size = count;
    slice->indices = NULL;
    pthread_mutex_unlock(&a->mutex);
    return slice;
}

size_t array_slice_size(const array_slice_t *slice) {
    if (!slice) return 0;
    pthread_mutex_lock(&slice->parent->mutex);
    size_t sz = slice->size;
    pthread_mutex_unlock(&slice->parent->mutex);
    return sz;
}

size_t array_slice_elem_size(const array_slice_t *slice) {
    if (!slice) return 0;
    pthread_mutex_lock(&slice->parent->mutex);
    size_t sz = slice->parent->elem_size;
    pthread_mutex_unlock(&slice->parent->mutex);
    return sz;
}

array_slice_t *array_slice_subslice(const array_slice_t *slice, size_t start, size_t count) {
    if (!slice || !slice->parent) return NULL;
    array_t *arr = slice->parent;
    pthread_mutex_lock(&arr->mutex);
    if (start >= slice->size) {
        pthread_mutex_unlock(&arr->mutex);
        return NULL;
    }
    if (start + count > slice->size) count = slice->size - start;
    array_ref(arr);
    array_slice_t *sub = malloc(sizeof(array_slice_t));
    if (!sub) {
        array_release(arr);
        pthread_mutex_unlock(&arr->mutex);
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
            pthread_mutex_unlock(&arr->mutex);
            return NULL;
        }
        for (size_t i = 0; i < count; ++i)
            sub->indices[i] = slice->indices[start + i];
    } else {
        sub->indices = NULL;
    }
    pthread_mutex_unlock(&arr->mutex);
    return sub;
}

/* --- Stack --- */

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

void stack_destroy(stack_t *stack) {
    if (!stack) return;
    array_destroy(stack->array);
    free(stack);
}

void *stack_pop(stack_t *stack) {
    if (!stack) return NULL;
    array_t *arr = stack->array;
    pthread_mutex_lock(&arr->mutex);
    if (arr->size == 0) {
        pthread_mutex_unlock(&arr->mutex);
        return NULL;
    }
    void *buf = malloc(arr->elem_size);
    if (!buf) {
        pthread_mutex_unlock(&arr->mutex);
        return NULL;
    }
    memcpy(buf, (char*)arr->arena + (arr->size - 1) * arr->elem_size, arr->elem_size);
    arr->size--;

    size_t unused = arr->capacity - arr->size;
    if (unused > STACK_CHUNK_SIZE) {
        size_t new_cap = arr->size + STACK_CHUNK_SIZE;
        void *new_arena = realloc(arr->arena, new_cap * arr->elem_size);
        if (new_arena) {
            arr->arena = new_arena;
            arr->capacity = new_cap;
        }
    }
    pthread_mutex_unlock(&arr->mutex);
    return buf;
}

bool stack_push(stack_t *stack, const void *elem) {
    if (!stack) return false;
    array_t *arr = stack->array;
    pthread_mutex_lock(&arr->mutex);
    if (arr->size == arr->capacity) {
        size_t new_cap = arr->capacity ? arr->capacity + STACK_CHUNK_SIZE : ARRAY_INIT_CAPACITY;
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
    arr->size++;
    pthread_mutex_unlock(&arr->mutex);
    return true;
}
