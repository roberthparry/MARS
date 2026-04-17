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

## Slice Examples

### Reading a subrange

Slices are views into an existing array. They do not copy elements; mutations to the underlying array are visible through the slice.

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

Expected output:

```text
2 3 4 5
```

### Sorting a slice without affecting the array

Slice sort reorders the slice's index mapping without moving elements in the underlying array.

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

Expected output:

```text
slice (sorted): 1 3 5 7 9
array (unchanged): 7 3 9 1 5
```

### Materialising a slice into a new array

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

Expected output:

```text
1 2 3 4
```

---

## Stack Examples

### Basic push and pop

`stack_pop` returns a heap-allocated copy of the top element; the caller is responsible for freeing it.

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

Expected output:

```text
30
20
10
```

### Stack with deep ownership

Supply `clone_fn` and `destroy_fn` to have the stack own its elements.

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

Expected output:

```text
third
second
first
```

---

### Slicing

- `array_slice_t *array_slice(const array_t *arr, size_t start, size_t count)`  
  Create a slice `[start, start+count)` from the array. Returns NULL if out of bounds. The returned slice must be freed with `array_slice_destroy()`.

- `void array_slice_destroy(array_slice_t *slice)`  
  Destroy a slice created by `array_slice`.

- `size_t array_slice_size(const array_slice_t *slice)`  
  Get the number of elements in the slice.

- `void *array_slice_get(const array_slice_t *slice, size_t index)`  
  Get pointer to element at index in slice (returns NULL if out of bounds).

- `array_slice_t *array_slice_subslice(const array_slice_t *slice, size_t start, size_t count)`  
  Create a slice of a slice (view into a subrange). Returns NULL if out of bounds.

- `size_t array_slice_elem_size(const array_slice_t *slice)`  
  Get the element size of the slice.

- `array_t *array_from_slice(const array_slice_t *slice, array_clone_fn clone, array_destroy_fn destroy)`  
  Create a new array from a slice. The new array will contain copies of the slice's elements.

- `void array_slice_sort(array_slice_t *slice, array_cmp_fn cmp)`  
  Sort the slice view (does not affect the underlying array).

- `bool array_slice_swap(array_slice_t *slice, size_t i, size_t j)`  
  Swap two elements in the slice view.

- `bool array_slice_rotate_left(array_slice_t *slice)`  
  Rotate the slice view left (first element becomes last).

- `bool array_slice_rotate_right(array_slice_t *slice)`  
  Rotate the slice view right (last element becomes first).

---

### Stack

- `stack_t` — opaque type representing a LIFO stack built on top of `array_t`.

- `stack_t *stack_create(size_t elem_size, array_clone_fn clone, array_destroy_fn destroy)`  
  Create a new stack for elements of the given size. Returns NULL on allocation failure.

- `void stack_destroy(stack_t *s)`  
  Destroy the stack and free all memory. Calls destroy for each element if provided. Safe to call with NULL.

- `bool stack_push(stack_t *s, const void *elem)`  
  Push an element onto the stack. Returns true on success, false on allocation failure.

- `void *stack_pop(stack_t *s)`  
  Pop the top element from the stack. Returns a pointer to a heap-allocated copy of the popped value, or NULL if the stack is empty. The caller must free the returned pointer.

---