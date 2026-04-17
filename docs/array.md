# `array_t`, `array_slice_t`, and `stack_t`

Generic, opaque containers with dense element storage and caller-defined ownership rules.

---

## `array_t`

An appendable, sortable array. Elements are stored inline in a contiguous arena; iteration is cache-friendly and access by index is O(1). Inserting or removing elements may shift others, so pointers returned by `array_get` are invalidated by any mutation.

### Ownership Models

Pass `NULL` for both callbacks to use shallow (memcpy) ownership — suitable for POD types or borrowed pointers. Supply `clone_fn` and `destroy_fn` to have the array deep-copy elements on insert and free them on remove or destroy.

#### Shallow ownership

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

```text
5 2 9 1
```

#### Deep ownership

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

    free(a.name);
    free(b.name);
    free(c.name);

    array_destroy(arr);
    return 0;
}
```

```text
alpha: 1
beta: 2
gamma: 3
```

### API Reference

#### Callback types

```c
typedef void (*array_clone_fn)(void *dst, const void *src);
typedef void (*array_destroy_fn)(void *elem);
typedef int  (*array_cmp_fn)(const void *a, const void *b);
```

- **`array_clone_fn`** — copy one element from `src` to `dst`. If NULL, `memcpy` is used.
- **`array_destroy_fn`** — destroy a single element. If NULL, no per-element cleanup is done.
- **`array_cmp_fn`** — three-way comparison (< 0 / 0 / > 0). Used for sorting.

#### Lifecycle

- `array_t *array_create(size_t elem_size, array_clone_fn clone, array_destroy_fn destroy)` — create a new array. Returns NULL on allocation failure.
- `void array_destroy(array_t *arr)` — destroy the array and call `destroy_fn` on each element if provided. Safe to call with NULL.
- `void array_clear(array_t *arr)` — remove all elements (calling `destroy_fn` if provided) without freeing the array itself.

#### Size and access

- `size_t array_size(const array_t *arr)` — number of elements currently stored.
- `void *array_get(const array_t *arr, size_t index)` — pointer to the element at `index`, or NULL if out of range. Invalidated by any mutation.

#### Insertion and removal

- `bool array_add(array_t *arr, const void *elem)` — append an element to the end.
- `bool array_insert(array_t *arr, size_t index, const void *elem)` — insert an element at the given index.
- `bool array_remove(array_t *arr, size_t index)` — remove the element at the given index.
- `bool array_remove_elements(array_t *arr, size_t index, size_t count)` — remove a range of elements starting at `index`.

#### Bulk operations

- `bool array_append_array(array_t *dst, const array_t *src)` — append all elements from `src`.
- `bool array_append_carray(array_t *dst, const void *src, size_t n)` — append `n` elements from a C array.
- `bool array_insert_array(array_t *dst, size_t index, const array_t *src)` — insert all elements from `src` at `index`.
- `bool array_insert_carray(array_t *dst, size_t index, const void *src, size_t n)` — insert `n` elements from a C array at `index`.

#### Sorting and reordering

- `void array_sort(array_t *arr, array_cmp_fn cmp)` — sort in-place.
- `bool array_swap(array_t *arr, size_t i, size_t j)` — swap two elements.
- `bool array_rotate_left(array_t *arr)` — move the first element to the end.
- `bool array_rotate_right(array_t *arr)` — move the last element to the front.

---

## `array_slice_t`

A lightweight view into a subrange of an `array_t`. Slices do not copy elements — they hold a reference to the parent array and an index mapping. View operations (sort, swap, rotate) reorder the mapping without touching the underlying array.

### Examples

#### Reading a subrange

```c
#include <stdio.h>
#include "array.h"

int main(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    for (int i = 0; i < 8; ++i)
        array_add(arr, &i);

    // View elements [2, 6)
    array_slice_t *slice = array_slice(arr, 2, 4);

    for (size_t i = 0; i < array_slice_size(slice); ++i)
        printf("%d ", *(int*)array_slice_get(slice, i));
    printf("\n");

    array_slice_destroy(slice);
    array_destroy(arr);
    return 0;
}
```

```text
2 3 4 5
```

#### Sorting a slice without affecting the array

```c
#include <stdio.h>
#include "array.h"

static int cmp_int(const void *a, const void *b) {
    return *(const int*)a - *(const int*)b;
}

int main(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    int vals[] = {7, 3, 9, 1, 5};
    for (int i = 0; i < 5; ++i)
        array_add(arr, &vals[i]);

    array_slice_t *slice = array_slice(arr, 0, 5);
    array_slice_sort(slice, cmp_int);

    printf("slice (sorted): ");
    for (size_t i = 0; i < array_slice_size(slice); ++i)
        printf("%d ", *(int*)array_slice_get(slice, i));
    printf("\n");

    printf("array (unchanged): ");
    for (size_t i = 0; i < array_size(arr); ++i)
        printf("%d ", *(int*)array_get(arr, i));
    printf("\n");

    array_slice_destroy(slice);
    array_destroy(arr);
    return 0;
}
```

```text
slice (sorted): 1 3 5 7 9
array (unchanged): 7 3 9 1 5
```

#### Materialising a slice into a new array

```c
#include <stdio.h>
#include "array.h"

int main(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    for (int i = 0; i < 6; ++i)
        array_add(arr, &i);

    array_slice_t *slice = array_slice(arr, 1, 4);    // [1, 2, 3, 4]
    array_t *copy = array_from_slice(slice, NULL, NULL);

    array_slice_destroy(slice);
    array_destroy(arr);

    for (size_t i = 0; i < array_size(copy); ++i)
        printf("%d ", *(int*)array_get(copy, i));
    printf("\n");

    array_destroy(copy);
    return 0;
}
```

```text
1 2 3 4
```

### API Reference

- `array_slice_t *array_slice(const array_t *arr, size_t start, size_t count)` — create a slice `[start, start+count)`. Returns NULL if out of bounds. Must be freed with `array_slice_destroy`.
- `void array_slice_destroy(array_slice_t *slice)` — release the slice and its reference to the parent array.
- `size_t array_slice_size(const array_slice_t *slice)` — number of elements in the slice.
- `size_t array_slice_elem_size(const array_slice_t *slice)` — element size of the parent array.
- `void *array_slice_get(const array_slice_t *slice, size_t index)` — pointer to element at `index`, or NULL if out of range.
- `array_slice_t *array_slice_subslice(const array_slice_t *slice, size_t start, size_t count)` — create a sub-view into the slice. Returns NULL if out of bounds.
- `array_t *array_from_slice(const array_slice_t *slice, array_clone_fn clone, array_destroy_fn destroy)` — materialise a slice into a new independent array.
- `void array_slice_sort(array_slice_t *slice, array_cmp_fn cmp)` — sort the slice view without affecting the underlying array.
- `bool array_slice_swap(array_slice_t *slice, size_t i, size_t j)` — swap two elements in the slice view.
- `bool array_slice_rotate_left(array_slice_t *slice)` — move the first element of the view to the end.
- `bool array_slice_rotate_right(array_slice_t *slice)` — move the last element of the view to the front.

---

## `stack_t`

A LIFO stack backed by an `array_t`. Supports the same ownership model as `array_t` via `clone_fn` and `destroy_fn`. `stack_pop` returns a heap-allocated copy of the top element; the caller is responsible for freeing it.

### Examples

#### Basic push and pop

```c
#include <stdio.h>
#include <stdlib.h>
#include "array.h"

int main(void) {
    stack_t *s = stack_create(sizeof(int), NULL, NULL);

    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; ++i)
        stack_push(s, &vals[i]);

    int *v;
    while ((v = stack_pop(s)) != NULL) {
        printf("%d\n", *v);
        free(v);
    }

    stack_destroy(s);
    return 0;
}
```

```text
30
20
10
```

#### Deep ownership

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"

static void str_clone(void *dst, const void *src) {
    const char **s = (const char**)src;
    *(char**)dst = malloc(strlen(*s) + 1);
    strcpy(*(char**)dst, *s);
}

static void str_destroy(void *elem) {
    free(*(char**)elem);
}

int main(void) {
    stack_t *s = stack_create(sizeof(char*), str_clone, str_destroy);

    const char *words[] = {"first", "second", "third"};
    for (int i = 0; i < 3; ++i)
        stack_push(s, &words[i]);

    char **top;
    while ((top = stack_pop(s)) != NULL) {
        printf("%s\n", *top);
        free(*top);
        free(top);
    }

    stack_destroy(s);
    return 0;
}
```

```text
third
second
first
```

### API Reference

- `stack_t *stack_create(size_t elem_size, array_clone_fn clone, array_destroy_fn destroy)` — create a new stack. Returns NULL on allocation failure.
- `void stack_destroy(stack_t *s)` — destroy the stack and free all memory. Calls `destroy_fn` on each element if provided. Safe to call with NULL.
- `bool stack_push(stack_t *s, const void *elem)` — push an element onto the stack. Returns false on allocation failure.
- `void *stack_pop(stack_t *s)` — pop the top element and return a heap-allocated copy of it, or NULL if empty. The caller must free the returned pointer.
