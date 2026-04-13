# `dictionary_t`

`dictionary_t` is a generic key/value container with caller-defined hashing,
comparison, cloning, and destruction rules.

## Capabilities

- generic keys and values
- shallow or deep ownership policies
- insert, replace, remove, and lookup
- cloning
- lazy sorted views
- opaque entry handles for repeated access

## Example: Shallow Ownership

This example stores borrowed string pointers. The dictionary owns only its
internal storage; it does not duplicate or free the pointed-to strings.

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
        str_hash, str_cmp,
        NULL, NULL,
        NULL, NULL
    );

    const char *k1 = "alpha";
    const char *v1 = "one";
    const char *lookup = "alpha";
    const char *out = NULL;

    dictionary_set(dict, &k1, &v1);

    if (dictionary_get(dict, &lookup, &out))
        printf("Value for '%s': %s\n", lookup, out);

    dictionary_destroy(dict);
    return 0;
}
```

Expected output:

```text
Value for 'alpha': one
```

## Example: Deep Ownership

This example deep-copies both keys and values, so the dictionary owns the
duplicated storage and releases it on destruction.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dictionary.h"

struct deep {
    char *name;
    int value;
};

static size_t deep_hash(const void *p) {
    const struct deep *d = p;
    size_t h = 146527;
    const char *s = d->name;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h ^ (size_t)d->value;
}

static int deep_cmp(const void *a, const void *b) {
    const struct deep *da = a;
    const struct deep *db = b;
    int c = strcmp(da->name, db->name);
    if (c != 0) return c;
    return (da->value > db->value) - (da->value < db->value);
}

static void deep_clone(void *dst, const void *src) {
    const struct deep *s = src;
    struct deep *d = dst;
    d->value = s->value;
    d->name = strdup(s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

int main(void) {
    dictionary_t *dict = dictionary_create(
        sizeof(struct deep), sizeof(struct deep),
        deep_hash, deep_cmp,
        deep_clone, deep_destroy,
        deep_clone, deep_destroy
    );

    struct deep k1 = { "k1", 1 };
    struct deep v1 = { "v1", 10 };

    dictionary_set(dict, &k1, &v1);

    struct deep out;
    if (dictionary_get(dict, &k1, &out)) {
        printf("Value for key '%s': %s (%d)\n", k1.name, out.name, out.value);
        free(out.name);
    }

    dictionary_destroy(dict);
    return 0;
}
```

Expected output:

```text
Value for key 'k1': v1 (10)
```

## API Reference

The full public API is declared in `include/dictionary.h`. The functions below
are the main entry points most users will want first.

### Construction and Lifetime

- `dictionary_create(...)`  
  Create a dictionary with caller-supplied size, hashing, comparison, clone,
  and destroy callbacks.

- `dictionary_destroy(dictionary_t *dict)`  
  Release the dictionary and all owned entries.

### Core Operations

- `dictionary_set(dictionary_t *dict, const void *key, const void *value)`  
  Insert or replace a key/value pair.

- `dictionary_get(const dictionary_t *dict, const void *key, void *out_value)`  
  Look up a value by key.

- `dictionary_remove(dictionary_t *dict, const void *key)`  
  Remove an entry by key.

### Utility Operations

- `dictionary_clone(const dictionary_t *dict)`  
  Clone a dictionary using its configured ownership rules.

## Design Notes

### Storage Model

Keys and values are stored separately so they can use independent ownership
policies. This allows:

- deep-copied keys with shallow values
- shallow keys with deep-copied values
- deep copies for both

### Lookup Model

A hash table maps logical entries to stored key and value slots. Cached hashes
help keep repeated lookups efficient.

### Views and Handles

The container can expose opaque handles for repeated access without exposing
internal storage details. Sorted views are rebuilt lazily after mutation.

## Tradeoffs

The design favors explicit ownership and generic behavior over a minimal API
surface.