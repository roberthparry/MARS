# `dictionary_t`

`dictionary_t` is a generic key/value container with caller-defined hashing,
comparison, cloning, and destruction rules.

## Capabilities

- generic keys and values (any fixed-size type)
- shallow or deep ownership via optional `clone_fn` and `destroy_fn` callbacks
- insert, replace, remove, and one-off lookup
- stable opaque entry handles for efficient repeated access and in-place update
- unsorted iteration via index or `dictionary_foreach()`
- lazy sorted views by key or by value
- full-dictionary clone

## Ownership Models

### Shallow Ownership

Store borrowed string pointers. The dictionary owns only its internal storage;
it does not duplicate or free the pointed-to strings.

```c
#include <stdio.h>
#include <string.h>
#include "dictionary.h"

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
    dictionary_t *dict = dictionary_create(
        sizeof(char *), sizeof(char *),
        str_hash, str_cmp, NULL, NULL,
        str_cmp, NULL, NULL
    );

    const char *k1 = "alpha";
    const char *v1 = "one";
    const char *out = NULL;

    dictionary_set(dict, &k1, &v1);

    if (dictionary_get(dict, &k1, &out))
        printf("Value for '%s': %s\n", k1, out);

    dictionary_destroy(dict);
    return 0;
}
```

Expected output:

```text
Value for 'alpha': one
```

### Deep Ownership

Deep-copy keys and values into the dictionary so it owns the storage and
releases it on destruction or removal.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dictionary.h"

struct item { char *name; int value; };

static size_t item_hash(const void *p) {
    const struct item *d = p;
    size_t h = 146527;
    const char *s = d->name;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h ^ (size_t)d->value;
}

static int item_cmp(const void *a, const void *b) {
    const struct item *da = a, *db = b;
    int c = strcmp(da->name, db->name);
    return c ? c : (da->value > db->value) - (da->value < db->value);
}

static void item_clone(void *dst, const void *src) {
    const struct item *s = src;
    struct item *d = dst;
    d->value = s->value;
    d->name = strdup(s->name);
}

static void item_destroy(void *elem) {
    free(((struct item *)elem)->name);
}

int main(void) {
    dictionary_t *dict = dictionary_create(
        sizeof(struct item), sizeof(struct item),
        item_hash, item_cmp, item_clone, item_destroy,
        item_cmp, item_clone, item_destroy
    );

    struct item k1 = { "k1", 1 };
    struct item v1 = { "v1", 10 };

    dictionary_set(dict, &k1, &v1);

    struct item out;
    if (dictionary_get(dict, &k1, &out)) {
        printf("Value for key '%s': %s (%d)\n", k1.name, out.name, out.value);
        free(out.name);   /* get() cloned the value */
    }

    dictionary_destroy(dict);
    return 0;
}
```

Expected output:

```text
Value for key 'k1': v1 (10)
```

## Design Notes

### Storage Model

Keys and values are stored separately with independent clone/destroy policies.
This allows combinations such as deep-copied keys with borrowed values, or
vice versa.

### Lookup Model

A hash table maps keys to their stored slots. Cached key hashes avoid rehashing
on repeated lookups. Collision resolution is internal.

### Entry Handles

An opaque `dictionary_entry_t *` provides a stable reference to a specific
key/value pair. Handles remain valid until the referenced entry is removed or
the dictionary is destroyed. Use `dictionary_set_entry()` to update a value
in-place without a key lookup.

### Sorted Views

A sorted index array is built lazily on the first call to
`dictionary_get_key_sorted()`, `dictionary_get_value_sorted()`, or
`dictionary_get_entry_sorted()` after any modification. It is invalidated by
the next mutation and rebuilt on demand.

## Tradeoffs

The design favours explicit ownership and generic behaviour over a minimal API
surface.

---

## API Reference

All declarations are in `include/dictionary.h`.

### Callback Types

```c
typedef size_t (*dictionary_hash_fn)(const void *key);
typedef int    (*dictionary_cmp_fn)(const void *a, const void *b);
typedef void   (*dictionary_clone_fn)(void *dst, const void *src);
typedef void   (*dictionary_destroy_fn)(void *elem);
```

- **`dictionary_hash_fn`** — stable hash for a key; must satisfy `cmp(a,b)==0 ⟹ hash(a)==hash(b)`
- **`dictionary_cmp_fn`** — three-way comparison (< 0 / 0 / > 0)
- **`dictionary_clone_fn`** — copy an element from `src` to `dst`; if NULL, `memcpy` is used
- **`dictionary_destroy_fn`** — called when an element is removed or replaced; if NULL, no cleanup

### Sorting Mode

```c
typedef enum {
    DICTIONARY_SORT_BY_KEY,
    DICTIONARY_SORT_BY_VALUE
} dictionary_sort_mode;
```

### Construction and Lifetime

- `dictionary_t *dictionary_create(size_t key_size, size_t value_size, dictionary_hash_fn key_hash, dictionary_cmp_fn key_cmp, dictionary_clone_fn key_clone, dictionary_destroy_fn key_destroy, dictionary_cmp_fn value_cmp, dictionary_clone_fn value_clone, dictionary_destroy_fn value_destroy)` — create a new dictionary. `key_hash` and `key_cmp` are required. `value_cmp` is required to use `DICTIONARY_SORT_BY_VALUE`. Returns NULL on allocation failure.
- `void dictionary_destroy(dictionary_t *dict)` — destroy the dictionary; calls destroy callbacks on all stored keys and values.
- `void dictionary_clear(dictionary_t *dict)` — remove all entries (calling destroy callbacks) without freeing the dictionary itself.

### Size

- `size_t dictionary_size(const dictionary_t *dict)` — number of key/value pairs

### Insertion, Lookup, and Removal

- `bool dictionary_set(dictionary_t *dict, const void *key, const void *value)` — insert or replace. If the key exists, the old value is destroyed and replaced. Returns true on success, false on allocation failure.
- `bool dictionary_get(const dictionary_t *dict, const void *key, void *out_value)` — one-off lookup. Copies the stored value into `*out_value` using `value_clone` (or `memcpy`). Returns true if found.
- `bool dictionary_remove(dictionary_t *dict, const void *key)` — remove by key, calling destroy callbacks. Returns true if the key was present.

### Unsorted Access by Index

- `const void *dictionary_get_key(const dictionary_t *dict, size_t index)` — key at `index` in unspecified order; NULL if out of range
- `const void *dictionary_get_value(const dictionary_t *dict, size_t index)` — value at `index` in unspecified order; NULL if out of range

### Sorted Views

Sorted views are rebuilt lazily after each modification.

- `const void *dictionary_get_key_sorted(dictionary_t *dict, size_t index)` — key at `index` in ascending key order; NULL on error
- `const void *dictionary_get_value_sorted(dictionary_t *dict, size_t index)` — value at `index` in ascending value order (requires `value_cmp`); NULL on error
- `bool dictionary_get_entry_sorted(dictionary_t *dict, size_t index, dictionary_sort_mode mode, dictionary_entry_t **out_entry)` — entry handle at `index` sorted by key or value; returns true on success

### Entry Handles

- `bool dictionary_get_entry(const dictionary_t *dict, const void *key, dictionary_entry_t **out_entry)` — look up an entry by key; sets `*out_entry` to the handle. Returns true if found.
- `bool dictionary_set_entry(dictionary_t *dict, dictionary_entry_t *entry, const void *value)` — replace the value of an existing entry without a key lookup. Returns true on success.
- `const void *dictionary_entry_key(const dictionary_entry_t *entry)` — key pointer from an entry handle; NULL if invalid
- `const void *dictionary_entry_value(const dictionary_entry_t *entry)` — value pointer from an entry handle; NULL if invalid

### Iteration

```c
typedef void (*dictionary_foreach_fn)(const dictionary_entry_t *entry, void *user_data);
```

- `void dictionary_foreach(const dictionary_t *dict, dictionary_foreach_fn fn, void *user_data)` — call `fn(entry, user_data)` for every entry in unspecified order. For sorted iteration use `dictionary_get_entry_sorted()`.

### Cloning

- `dictionary_t *dictionary_clone(const dictionary_t *dict)` — create a fully independent copy. Keys and values are copied using the configured clone functions (or `memcpy`). Returns NULL on allocation failure.
