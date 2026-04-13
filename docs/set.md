# `set_t`

`set_t` is a generic hash set with dense element storage and caller-defined
ownership rules.

## Capabilities

- add, remove, and contains
- dense storage for iteration
- lazy sorted traversal
- clone, union, intersection, and difference
- shallow or deep ownership policies

## Example: Shallow Ownership

This example stores borrowed string pointers. The set owns only its internal
storage; it does not duplicate or free the pointed-to strings.

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
    set_t *s = set_create(
        sizeof(char *),
        str_hash,
        str_cmp,
        NULL,
        NULL
    );

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

## Example: Deep Ownership

This example deep-copies the inserted strings, so the set owns the duplicated
elements and releases them on destruction.

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
    const char *s = *(const char * const *)src;
    char *copy = strdup(s);
    memcpy(dst, &copy, sizeof(char *));
}

static void str_destroy(void *elem) {
    char *s = *(char **)elem;
    free(s);
}

int main(void) {
    set_t *s = set_create(
        sizeof(char *),
        str_hash,
        str_cmp,
        str_clone,
        str_destroy
    );

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

## API Reference

The full public API is declared in `include/set.h`. The functions below are the
main entry points most users will want first.

### Construction and Lifetime

- `set_create(...)`  
  Create a set with caller-supplied element size, hashing, comparison, clone,
  and destroy callbacks.

- `set_destroy(set_t *set)`  
  Release the set and all owned elements.

### Core Operations

- `set_add(set_t *set, const void *elem)`  
  Insert an element into the set.

- `set_remove(set_t *set, const void *elem)`  
  Remove an element from the set.

- `set_contains(const set_t *set, const void *elem)`  
  Test whether an element is present.

### Set Operations

- `set_clone(const set_t *set)`  
  Clone a set using its configured ownership rules.

- `set_union(const set_t *a, const set_t *b)`  
  Build the union of two sets.

- `set_intersection(const set_t *a, const set_t *b)`  
  Build the intersection of two sets.

- `set_difference(const set_t *a, const set_t *b)`  
  Build the set difference `a - b`.

## Design Notes

### Storage Model

Elements are stored in a compact dense array rather than as one allocation per
element. That keeps iteration straightforward and cache-friendly.

### Lookup Model

A hash table maps each element to its slot in dense storage. Cached hashes
reduce repeated work during lookup and insertion.

### Removal

Removal may move the last stored element into the vacated slot in order to keep
storage dense. Callers should therefore not assume that element indices remain
stable across removals.

### Views

Sorted traversals can be built lazily on top of the underlying set without
changing membership behavior.

## Tradeoffs

The design favors compact storage and explicit ownership over stable element
positions.