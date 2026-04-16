# `array_t`

`array_t` is a generic, opaque, appendable, and sortable array container with dense element storage and caller-defined ownership rules.

## Capabilities

- Append, insert, remove, and access elements by index
- Unsorted iteration over a dense arena
- Bulk insert and remove operations
- Optional deep or shallow ownership via user-supplied `clone_fn` and `destroy_fn` callbacks
- In-place sorting with user-supplied comparison function

## Ownership Models

### Shallow Ownership

Store borrowed pointers or POD types. The array owns only its internal storage; it does not duplicate or free the pointed-to objects.

```c
#include <stdio.h>
#include "array.h"

int main(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    int vals[] = {5, 2, 9, 1};
    for (int i = 0; i < 4; ++i)
        array_add(arr, &vals[i]);

    for (size_t i = 0; i < array_size(arr); ++i)
        printf("%d ", *(int*)array_get(arr, i));
    printf("\n");

    array_destroy(arr);
    return 0;
}
```

Expected output:

```text
5 2 9 1
```

### Deep Ownership

Deep-copy inserted objects so the array owns the elements and releases them on destruction.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"

struct deep {
    char *name;
    int value;
};

static void deep_clone(void *dst, const void *src) {
    const struct deep *s = src;
    struct deep *d = dst;
    d->value = s->value;
    d->name = malloc(strlen(s->name) + 1);
    strcpy(d->name, s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

int main(void) {
    array_t *arr = array_create(sizeof(struct deep), deep_clone, deep_destroy);

    struct deep a = { strdup("alpha"), 1 };
    struct deep b = { strdup("beta"),  2 };
    struct deep c = { strdup("gamma"), 3 };

    array_add(arr, &a);
    array_add(arr, &b);
    array_add(arr, &c);

    for (size_t i = 0; i < array_size(arr); ++i) {
        struct deep *p = array_get(arr, i);
        printf("%s: %d\n", p->name, p->value);
    }

    // Clean up original structs
    free(a.name);
    free(b.name);
    free(c.name);

    array_destroy(arr);
    return 0;
}
```

Expected output:

```text
alpha: 1
beta: 2
gamma: 3
```

## Design Notes

### Storage Model

Elements are stored inline in a dense contiguous arena (no holes, compacted on removal). This keeps iteration cache-friendly and enables fast access by index.

### Insertion and Removal

- **Insertion:** Elements can be appended or inserted at any index. The arena grows as needed.
- **Removal:** Removing an element compacts the arena to keep storage dense. Bulk removal is supported.

### Bulk Operations

- **Bulk Insert:** Insert all elements from another `array_t` or a C array at any index.
- **Bulk Remove:** Remove a range of elements efficiently.

### Sorting

The array can be sorted in-place using a user-supplied comparison function (compatible with `qsort`).

## Tradeoffs

The design favors compact storage and explicit ownership over stable element addresses. Inserting or removing elements may move other elements.

---

## API Reference

All declarations are in `include/array.h`.

### Callback Types

```c
typedef void (*array_clone_fn)(void *dst, const void *src);
typedef void (*array_destroy_fn)(void *elem);
typedef int  (*array_cmp_fn)(const void *a, const void *b);
```

- **`array_clone_fn`** — copy one element from `src` to `dst` (both point to arena storage or caller storage). If NULL, `memcpy` is used.
- **`array_destroy_fn`** — destroy a single element in the arena. If NULL, no per-element cleanup is done.
- **`array_cmp_fn`** — three-way comparison (< 0 / 0 / > 0). Used for sorting.

### Construction and Lifetime

- `array_t *array_create(size_t elem_size, array_clone_fn clone, array_destroy_fn destroy)` — create a new array. Returns NULL on allocation failure.
- `void array_destroy(array_t *arr)` — destroy the array; calls `destroy_fn` on each element if provided. Safe to call with NULL.
- `void array_clear(array_t *arr)` — remove all elements (calling `destroy_fn` if provided) without freeing the array itself.

### Size

- `size_t array_size(const array_t *arr)` — number of elements currently stored

### Element Access

- `void *array_get(const array_t *arr, size_t index)` — pointer to the element at `index` (or NULL if out of range). The pointer is invalidated by any mutation.

### Insertion and Removal

- `bool array_add(array_t *arr, const void *elem)` — append an element to the end. Uses `clone_fn` or `memcpy`.
- `bool array_insert(array_t *arr, size_t index, const void *elem)` — insert an element at the given index.
- `bool array_remove(array_t *arr, size_t index)` — remove the element at the given index.
- `bool array_remove_elements(array_t *arr, size_t index, size_t count)` — remove a range of elements starting at `index`.

### Bulk Operations

- `bool array_append_array(array_t *dst, const array_t *src)` — append all elements from another array (deep copy).
- `bool array_append_carray(array_t *dst, const void *src, size_t n)` — append `n` elements from a C array.
- `bool array_insert_array(array_t *dst, size_t index, const array_t *src)` — insert all elements from another array at a given index.
- `bool array_insert_carray(array_t *dst, size_t index, const void *src, size_t n)` — insert `n` elements from a C array at a given index.

### Sorting

- `void array_sort(array_t *arr, array_cmp_fn cmp)` — sort the array in-place using the provided comparison function.

---