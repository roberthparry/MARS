# `set_t`

`set_t` is a generic hash set with dense element storage and caller-defined
ownership rules.

## Capabilities

- add, remove, and membership test
- unsorted iteration over a dense arena
- lazy sorted traversal via `set_get_sorted()`
- clone, union, intersection, and difference
- shallow or deep ownership via optional `clone_fn` and `destroy_fn` callbacks

## Ownership Models

### Shallow Ownership

Store borrowed pointers. The set owns only its internal storage; it does not
duplicate or free the pointed-to strings.

```c
#include <stdio.h>
#include <string.h>
#include "set.h"

static size_t str_hash(const void *p) {
    const char *s = *(const char * const *)p;
    size_t h = 146527;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h;
}

static int str_cmp(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

int main(void) {
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, NULL, NULL);

    const char *a = "hello";
    const char *b = "world";
    const char *c = "hello";

    set_add(s, &a);
    set_add(s, &b);

    if (set_contains(s, &a))
        printf("'hello' is in the set\n");

    if (!set_add(s, &c))
        printf("Duplicate 'hello' was not added\n");

    set_remove(s, &a);

    if (!set_contains(s, &a))
        printf("'hello' was removed\n");

    set_destroy(s);
    return 0;
}
```

Expected output:

```text
'hello' is in the set
Duplicate 'hello' was not added
'hello' was removed
```

### Deep Ownership

Deep-copy inserted strings so the set owns the elements and releases them on
destruction.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "set.h"

static size_t str_hash(const void *p) {
    const char *s = *(const char * const *)p;
    size_t h = 146527;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h;
}

static int str_cmp(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

static void str_clone(void *dst, const void *src) {
    char *copy = strdup(*(const char * const *)src);
    memcpy(dst, &copy, sizeof(char *));
}

static void str_destroy(void *elem) {
    free(*(char **)elem);
}

int main(void) {
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, str_clone, str_destroy);

    const char *a = "hello";
    const char *b = "world";

    set_add(s, &a);
    set_add(s, &b);

    /* Iterate in sorted order */
    for (size_t i = 0; i < set_get_size(s); i++) {
        const char *elem = *(const char **)set_get_sorted(s, i);
        printf("[%zu] %s\n", i, elem);
    }

    set_destroy(s);
    return 0;
}
```

## Design Notes

### Storage Model

Elements are stored inline in a dense contiguous arena rather than one
allocation per element. This keeps iteration cache-friendly. Each slot also
stores the element's precomputed hash, avoiding rehashing during lookups.

### Lookup Model

A hash table maps each element to its arena index. On collision, the
implementation walks the hash table until it finds a matching hash and element
(via `cmp`), or an empty slot.

### Removal

When an element is removed the last element in the arena is moved into the
vacated slot to keep storage dense. The hash table is updated accordingly.
Arena indices are therefore not stable across removals.

### Sorted Views

A sorted index array is built lazily the first time `set_get_sorted()` is
called after any mutation. It is invalidated by the next `set_add()`,
`set_remove()`, or `set_clear()` and rebuilt on the following sorted access.

## Tradeoffs

The design favours compact storage and explicit ownership over stable element
positions.

---

## API Reference

All declarations are in `include/set.h`.

### Callback Types

```c
typedef size_t (*set_hash_fn)(const void *elem);
typedef int    (*set_cmp_fn)(const void *a, const void *b);
typedef void   (*set_clone_fn)(void *dst, const void *src);
typedef void   (*set_destroy_fn)(void *elem);
```

- **`set_hash_fn`** — hash an element by value (pointer to element bytes). Must be consistent with `cmp`: `cmp(a,b)==0` implies `hash(a)==hash(b)`.
- **`set_cmp_fn`** — three-way comparison (< 0 / 0 / > 0). Used for equality tests and sorted iteration.
- **`set_clone_fn`** — copy one element from `src` to `dst` (both point to arena storage or caller storage). If NULL, `memcpy` is used.
- **`set_destroy_fn`** — destroy a single element in the arena. If NULL, no per-element cleanup is done.

### Construction and Lifetime

- `set_t *set_create(size_t elem_size, set_hash_fn hash, set_cmp_fn cmp, set_clone_fn clone, set_destroy_fn destroy)` — create a new set. `hash` and `cmp` are required. Returns NULL on allocation failure.
- `void set_destroy(set_t *set)` — destroy the set; calls `destroy_fn` on each element if provided. Safe to call with NULL.
- `void set_clear(set_t *set)` — remove all elements (calling `destroy_fn` if provided) without freeing the set itself.

### Size

- `size_t set_get_size(const set_t *set)` — number of elements currently stored

### Membership

- `bool set_contains(const set_t *set, const void *elem)` — true if an equal element is present (equality via `cmp(a,b)==0`)

### Insertion and Removal

- `bool set_add(set_t *set, const void *elem)` — insert `elem` if not already present. Returns true if newly inserted, false if a duplicate was found. Uses `clone_fn` or `memcpy` to copy the element into the arena.
- `bool set_remove(set_t *set, const void *elem)` — remove an equal element. Returns true if found and removed. Calls `destroy_fn` on the removed element.

### Iteration

- `const void *set_get(const set_t *set, size_t index)` — element at `index` in arena (insertion) order. Returns NULL if out of range. The pointer is invalidated by any subsequent mutation.
- `const void *set_get_sorted(set_t *set, size_t index)` — element at `index` in sorted order. Rebuilds the sorted index array on first call after a mutation. Returns NULL if out of range. The pointer is invalidated by any subsequent mutation.

### Set Operations (return new owning sets)

- `set_t *set_clone(const set_t *set)` — deep or shallow clone of the entire set, using the same callbacks as the original
- `set_t *set_union(const set_t *a, const set_t *b)` — all elements in `a` or `b`; uses `a`'s callbacks
- `set_t *set_intersection(const set_t *a, const set_t *b)` — elements in both `a` and `b`
- `set_t *set_difference(const set_t *a, const set_t *b)` — elements in `a` that are not in `b`
- `bool set_is_subset(const set_t *a, const set_t *b)` — true if every element of `a` is also in `b`
