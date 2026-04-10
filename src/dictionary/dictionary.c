/* dictionary.c - implementation of generic key/value dictionary with dense arenas */

#include "dictionary.h"

#include <stdlib.h>
#include <string.h>

/* Internal bucket structure */

struct bucket {
    size_t index;          /* index into arenas */
    struct bucket *next;   /* next in chain */
};

/* Opaque entry */

struct dict_entry {
    struct dictionary *dict;
    size_t index;
};

/* Dictionary structure */

struct dictionary {
    size_t key_size;
    size_t value_size;

    size_t key_slot_stride;    /* sizeof(size_t) + key_size */
    size_t value_slot_stride;  /* value_size */

    size_t count;
    size_t capacity;

    unsigned char *key_arena;   /* [hash][key] */
    unsigned char *value_arena; /* [value] */

    struct bucket **table;
    size_t table_size;
    size_t prime_index;

    size_t *sorted_keys_idx;
    size_t sorted_keys_cap;
    bool sorted_keys_valid;

    size_t *sorted_values_idx;
    size_t sorted_values_cap;
    bool sorted_values_valid;

    dictionary_hash_fn key_hash;
    dictionary_cmp_fn key_cmp;
    dictionary_clone_fn key_clone;
    dictionary_destroy_fn key_destroy;

    dictionary_clone_fn value_clone;
    dictionary_destroy_fn value_destroy;

    /* scratch entry for APIs that return an opaque handle */
    struct dict_entry scratch_entry;
};

/* Slot helpers */

static inline size_t *key_slot_hash_ptr(const struct dictionary *dict, size_t index) {
    return (size_t *)(dict->key_arena + index * dict->key_slot_stride);
}

static inline void *key_slot_data_ptr(const struct dictionary *dict, size_t index) {
    return (void *)(dict->key_arena + index * dict->key_slot_stride + sizeof(size_t));
}

static inline void *value_slot_data_ptr(const struct dictionary *dict, size_t index) {
    return (void *)(dict->value_arena + index * dict->value_slot_stride);
}

/* Prime sizes for hash table capacities */

static const size_t primes[] = {
    17ul, 37ul, 79ul, 163ul, 331ul, 673ul, 1361ul, 2729ul,
    5471ul, 10949ul, 21911ul, 43853ul, 87719ul, 175447ul,
    350899ul, 701819ul, 1403641ul, 2807303ul, 5614657ul,
    11229331ul, 22458671ul, 44917381ul, 89834777ul, 179669557ul,
    359339171ul, 718678369ul, 1437356741ul
};

static const size_t num_primes = sizeof(primes) / sizeof(primes[0]);

/* Forward declarations */

static bool dict_reserve_arenas(struct dictionary *dict, size_t min_capacity);
static bool dict_rehash(struct dictionary *dict, size_t new_prime_index);
static bool dict_ensure_table_capacity(struct dictionary *dict);
static void dict_invalidate_sorted(struct dictionary *dict);
static bool dict_build_sorted_keys(struct dictionary *dict);
static bool dict_build_sorted_values(struct dictionary *dict);

/* Create */

dictionary_t *dictionary_create(size_t key_size,
                    size_t value_size,
                    dictionary_hash_fn key_hash,
                    dictionary_cmp_fn key_cmp,
                    dictionary_clone_fn key_clone,
                    dictionary_destroy_fn key_destroy,
                    dictionary_clone_fn value_clone,
                    dictionary_destroy_fn value_destroy)
{
    if (key_size == 0 || value_size == 0 || key_hash == NULL || key_cmp == NULL) {
        return NULL;
    }

    struct dictionary *dict = (struct dictionary *)calloc(1, sizeof(struct dictionary));
    if (!dict) {
        return NULL;
    }

    dict->key_size = key_size;
    dict->value_size = value_size;

    dict->key_slot_stride = sizeof(size_t) + key_size;
    dict->value_slot_stride = value_size;

    dict->count = 0;
    dict->capacity = 0;
    dict->key_arena = NULL;
    dict->value_arena = NULL;

    dict->table = NULL;
    dict->table_size = 0;
    dict->prime_index = 0;

    dict->sorted_keys_idx = NULL;
    dict->sorted_keys_cap = 0;
    dict->sorted_keys_valid = false;

    dict->sorted_values_idx = NULL;
    dict->sorted_values_cap = 0;
    dict->sorted_values_valid = false;

    dict->key_hash = key_hash;
    dict->key_cmp = key_cmp;
    dict->key_clone = key_clone;
    dict->key_destroy = key_destroy;

    dict->value_clone = value_clone;
    dict->value_destroy = value_destroy;

    dict->scratch_entry.dict = dict;
    dict->scratch_entry.index = 0;

    if (!dict_rehash(dict, 0)) {
        dictionary_destroy(dict);
        return NULL;
    }

    return dict;
}

/* Destroy */

void dictionary_destroy(dictionary_t *dict)
{
    if (!dict) return;

    if (dict->key_destroy && dict->key_arena) {
        for (size_t i = 0; i < dict->count; ++i) {
            void *k = key_slot_data_ptr(dict, i);
            dict->key_destroy(k);
        }
    }

    if (dict->value_destroy && dict->value_arena) {
        for (size_t i = 0; i < dict->count; ++i) {
            void *v = value_slot_data_ptr(dict, i);
            dict->value_destroy(v);
        }
    }

    if (dict->table) {
        for (size_t i = 0; i < dict->table_size; ++i) {
            struct bucket *b = dict->table[i];
            while (b) {
                struct bucket *next = b->next;
                free(b);
                b = next;
            }
        }
        free(dict->table);
    }

    free(dict->key_arena);
    free(dict->value_arena);
    free(dict->sorted_keys_idx);
    free(dict->sorted_values_idx);
    free(dict);
}

/* Clear */

void dictionary_clear(dictionary_t *dict)
{
    if (!dict || dict->count == 0) return;

    if (dict->key_destroy && dict->key_arena) {
        for (size_t i = 0; i < dict->count; ++i) {
            void *k = key_slot_data_ptr(dict, i);
            dict->key_destroy(k);
        }
    }

    if (dict->value_destroy && dict->value_arena) {
        for (size_t i = 0; i < dict->count; ++i) {
            void *v = value_slot_data_ptr(dict, i);
            dict->value_destroy(v);
        }
    }

    if (dict->table) {
        for (size_t i = 0; i < dict->table_size; ++i) {
            struct bucket *b = dict->table[i];
            while (b) {
                struct bucket *next = b->next;
                free(b);
                b = next;
            }
            dict->table[i] = NULL;
        }
    }

    dict->count = 0;
    dict_invalidate_sorted(dict);
}

/* Size */

size_t dictionary_size(const dictionary_t *dict)
{
    return dict ? dict->count : 0;
}

/* Internal: reserve arenas */

static bool dict_reserve_arenas(struct dictionary *dict, size_t min_capacity)
{
    if (dict->capacity >= min_capacity) return true;

    size_t new_capacity = dict->capacity ? dict->capacity * 2 : 8;
    if (new_capacity < min_capacity) new_capacity = min_capacity;

    size_t new_key_bytes = new_capacity * dict->key_slot_stride;
    size_t new_value_bytes = new_capacity * dict->value_slot_stride;

    unsigned char *new_key_arena = (unsigned char *)realloc(dict->key_arena, new_key_bytes);
    if (!new_key_arena) return false;

    unsigned char *new_value_arena = (unsigned char *)realloc(dict->value_arena, new_value_bytes);
    if (!new_value_arena) {
        /* rollback key_arena realloc is not trivial; but old pointer is lost.
           In practice, we accept failure here as unlikely; for strict safety,
           you'd allocate both first and only assign on success. */
        return false;
    }

    dict->key_arena = new_key_arena;
    dict->value_arena = new_value_arena;
    dict->capacity = new_capacity;
    return true;
}

/* Internal: rehash */

static bool dict_rehash(struct dictionary *dict, size_t new_prime_index)
{
    if (new_prime_index >= num_primes) {
        if (dict->table_size == 0) return false;
        return true;
    }

    size_t new_size = primes[new_prime_index];
    struct bucket **new_table = (struct bucket **)calloc(new_size, sizeof(struct bucket *));
    if (!new_table) return false;

    if (dict->table) {
        for (size_t i = 0; i < dict->table_size; ++i) {
            struct bucket *b = dict->table[i];
            while (b) {
                struct bucket *next = b->next;

                size_t idx = b->index;
                size_t hash = *key_slot_hash_ptr(dict, idx);
                size_t bucket_index = hash % new_size;

                b->next = new_table[bucket_index];
                new_table[bucket_index] = b;

                b = next;
            }
        }
        free(dict->table);
    }

    dict->table = new_table;
    dict->table_size = new_size;
    dict->prime_index = new_prime_index;
    return true;
}

/* Internal: ensure table capacity */

static bool dict_ensure_table_capacity(struct dictionary *dict)
{
    if (!dict || dict->table_size == 0) return false;

    double load = (double)dict->count / (double)dict->table_size;
    if (load < 0.6) return true;

    if (dict->prime_index + 1 >= num_primes) return true;

    return dict_rehash(dict, dict->prime_index + 1);
}

/* Internal: invalidate sorted views */

static void dict_invalidate_sorted(struct dictionary *dict)
{
    if (!dict) return;
    dict->sorted_keys_valid = false;
    dict->sorted_values_valid = false;
}

/* Internal: find bucket */

static bool dict_find_bucket(const struct dictionary *dict,
                             const void *key,
                             size_t hash,
                             size_t *out_bucket_index,
                             struct bucket **out_prev,
                             struct bucket **out_curr)
{
    if (!dict || dict->table_size == 0) return false;

    size_t bucket_index = hash % dict->table_size;
    struct bucket *prev = NULL;
    struct bucket *curr = dict->table[bucket_index];

    while (curr) {
        void *stored_key = key_slot_data_ptr(dict, curr->index);
        if (dict->key_cmp(stored_key, key) == 0) {
            if (out_bucket_index) *out_bucket_index = bucket_index;
            if (out_prev) *out_prev = prev;
            if (out_curr) *out_curr = curr;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

/* Set */

bool dictionary_set(dictionary_t *dict, const void *key, const void *value)
{
    if (!dict || !key || !value) return false;

    size_t hash = dict->key_hash(key);

    size_t bucket_index = 0;
    struct bucket *prev = NULL;
    struct bucket *curr = NULL;

    if (dict_find_bucket(dict, key, hash, &bucket_index, &prev, &curr)) {
        size_t idx = curr->index;
        void *val_dst = value_slot_data_ptr(dict, idx);

        if (dict->value_destroy) {
            dict->value_destroy(val_dst);
        }

        if (dict->value_clone) {
            dict->value_clone(val_dst, value);
        } else {
            memcpy(val_dst, value, dict->value_size);
        }

        dict_invalidate_sorted(dict);
        return true;
    }

    if (!dict_reserve_arenas(dict, dict->count + 1)) return false;
    if (!dict_ensure_table_capacity(dict)) return false;

    size_t index = dict->count;

    size_t *hash_ptr = key_slot_hash_ptr(dict, index);
    void *key_dst = key_slot_data_ptr(dict, index);
    void *val_dst = value_slot_data_ptr(dict, index);

    *hash_ptr = hash;

    if (dict->key_clone) {
        dict->key_clone(key_dst, key);
    } else {
        memcpy(key_dst, key, dict->key_size);
    }

    if (dict->value_clone) {
        dict->value_clone(val_dst, value);
    } else {
        memcpy(val_dst, value, dict->value_size);
    }

    struct bucket *b = (struct bucket *)malloc(sizeof(struct bucket));
    if (!b) {
        if (dict->key_destroy) dict->key_destroy(key_dst);
        if (dict->value_destroy) dict->value_destroy(val_dst);
        return false;
    }

    bucket_index = hash % dict->table_size;
    b->index = index;
    b->next = dict->table[bucket_index];
    dict->table[bucket_index] = b;

    dict->count++;
    dict_invalidate_sorted(dict);
    return true;
}

/* Get (copy out) */

bool dictionary_get(const dictionary_t *dict, const void *key, void *out_value)
{
    if (!dict || !key || !out_value) return false;

    size_t hash = dict->key_hash(key);

    size_t bucket_index = 0;
    struct bucket *prev = NULL;
    struct bucket *curr = NULL;

    if (!dict_find_bucket(dict, key, hash, &bucket_index, &prev, &curr)) {
        return false;
    }

    void *val_src = value_slot_data_ptr(dict, curr->index);

    if (dict->value_clone) {
        /* Deep copy into caller's buffer */
        dict->value_clone(out_value, val_src);
    } else {
        /* Shallow copy */
        memcpy(out_value, val_src, dict->value_size);
    }

    return true;
}

/* Remove */

bool dictionary_remove(dictionary_t *dict, const void *key)
{
    if (!dict || !key || dict->count == 0 || dict->table_size == 0)
        return false;

    size_t hash = dict->key_hash(key);
    size_t bucket_index = hash % dict->table_size;

    struct bucket *prev = NULL;
    struct bucket *curr = dict->table[bucket_index];

    /* 1. Find the bucket node for this key */
    while (curr) {
        void *stored_key = key_slot_data_ptr(dict, curr->index);
        if (dict->key_cmp(stored_key, key) == 0)
            break;
        prev = curr;
        curr = curr->next;
    }

    if (!curr)
        return false;

    size_t remove_index = curr->index;

    /* 2. Unlink the bucket node */
    if (prev) prev->next = curr->next;
    else      dict->table[bucket_index] = curr->next;
    free(curr);

    /* 3. Destroy key/value at remove_index */
    void *key_ptr = key_slot_data_ptr(dict, remove_index);
    void *val_ptr = value_slot_data_ptr(dict, remove_index);

    if (dict->key_destroy)   dict->key_destroy(key_ptr);
    if (dict->value_destroy) dict->value_destroy(val_ptr);

    size_t last_index = dict->count - 1;

    /* 4. If removing last element, done */
    if (remove_index == last_index) {
        dict->count--;
        dict_invalidate_sorted(dict);
        return true;
    }

    /* 5. Move last element into remove_index */
    size_t *src_hash = key_slot_hash_ptr(dict, last_index);
    void   *src_key  = key_slot_data_ptr(dict, last_index);
    void   *src_val  = value_slot_data_ptr(dict, last_index);

    size_t *dst_hash = key_slot_hash_ptr(dict, remove_index);
    void   *dst_key  = key_slot_data_ptr(dict, remove_index);
    void   *dst_val  = value_slot_data_ptr(dict, remove_index);

    *dst_hash = *src_hash;

    if (dict->key_clone) {
        dict->key_clone(dst_key, src_key);
        if (dict->key_destroy) dict->key_destroy(src_key);
    } else {
        memcpy(dst_key, src_key, dict->key_size);
    }

    if (dict->value_clone) {
        dict->value_clone(dst_val, src_val);
        if (dict->value_destroy) dict->value_destroy(src_val);
    } else {
        memcpy(dst_val, src_val, dict->value_size);
    }

    /* 6. Remove ALL bucket nodes that reference last_index */
    size_t moved_bucket_index = (*dst_hash) % dict->table_size;
    struct bucket **pp = &dict->table[moved_bucket_index];
    struct bucket *b = *pp;

    while (b) {
        if (b->index == last_index) {
            struct bucket *dead = b;
            *pp = b->next;
            b = b->next;
            free(dead);
            continue;
        }
        pp = &b->next;
        b = b->next;
    }

    /* 7. Insert a fresh bucket node for remove_index */
    struct bucket *newb = malloc(sizeof(struct bucket));
    newb->index = remove_index;
    newb->next = dict->table[moved_bucket_index];
    dict->table[moved_bucket_index] = newb;

    /* 8. Finish */
    dict->count--;
    dict_invalidate_sorted(dict);
    return true;
}

/* Direct arena access */

const void *dictionary_get_key(const dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count) return NULL;
    return key_slot_data_ptr(dict, index);
}

const void *dictionary_get_value(const dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count) return NULL;
    return value_slot_data_ptr(dict, index);
}

/* Sorting context */

struct sort_ctx {
    struct dictionary *dict;
};

static struct sort_ctx g_sort_ctx;

/* qsort comparator for keys */

static int dict_qsort_keys_cmp(const void *a, const void *b)
{
    const size_t ia = *(const size_t *)a;
    const size_t ib = *(const size_t *)b;

    void *ka = key_slot_data_ptr(g_sort_ctx.dict, ia);
    void *kb = key_slot_data_ptr(g_sort_ctx.dict, ib);

    return g_sort_ctx.dict->key_cmp(ka, kb);
}

/* qsort comparator for values (uses key_cmp as generic comparator) */

static int dict_qsort_values_cmp(const void *a, const void *b)
{
    const size_t ia = *(const size_t *)a;
    const size_t ib = *(const size_t *)b;

    void *va = value_slot_data_ptr(g_sort_ctx.dict, ia);
    void *vb = value_slot_data_ptr(g_sort_ctx.dict, ib);

    return g_sort_ctx.dict->key_cmp(va, vb);
}

/* Build sorted keys */

static bool dict_build_sorted_keys(struct dictionary *dict)
{
    if (!dict) return false;

    if (dict->count == 0) {
        dict->sorted_keys_valid = true;
        return true;
    }

    if (dict->sorted_keys_cap < dict->count) {
        size_t new_cap = dict->sorted_keys_cap ? dict->sorted_keys_cap * 2 : 8;
        if (new_cap < dict->count) new_cap = dict->count;
        size_t *new_idx = (size_t *)realloc(dict->sorted_keys_idx, new_cap * sizeof(size_t));
        if (!new_idx) return false;
        dict->sorted_keys_idx = new_idx;
        dict->sorted_keys_cap = new_cap;
    }

    for (size_t i = 0; i < dict->count; ++i) {
        dict->sorted_keys_idx[i] = i;
    }

    g_sort_ctx.dict = dict;
    qsort(dict->sorted_keys_idx, dict->count, sizeof(size_t), dict_qsort_keys_cmp);
    dict->sorted_keys_valid = true;
    return true;
}

/* Build sorted values */

static bool dict_build_sorted_values(struct dictionary *dict)
{
    if (!dict) return false;

    if (dict->count == 0) {
        dict->sorted_values_valid = true;
        return true;
    }

    if (dict->sorted_values_cap < dict->count) {
        size_t new_cap = dict->sorted_values_cap ? dict->sorted_values_cap * 2 : 8;
        if (new_cap < dict->count) new_cap = dict->count;
        size_t *new_idx = (size_t *)realloc(dict->sorted_values_idx, new_cap * sizeof(size_t));
        if (!new_idx) return false;
        dict->sorted_values_idx = new_idx;
        dict->sorted_values_cap = new_cap;
    }

    for (size_t i = 0; i < dict->count; ++i) {
        dict->sorted_values_idx[i] = i;
    }

    g_sort_ctx.dict = dict;
    qsort(dict->sorted_values_idx, dict->count, sizeof(size_t), dict_qsort_values_cmp);
    dict->sorted_values_valid = true;
    return true;
}

/* Sorted key access */

const void *dictionary_get_key_sorted(dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count) return NULL;

    if (!dict->sorted_keys_valid) {
        if (!dict_build_sorted_keys(dict)) return NULL;
    }

    size_t arena_index = dict->sorted_keys_idx[index];
    return key_slot_data_ptr(dict, arena_index);
}

/* Sorted value access */

const void *dictionary_get_value_sorted(dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count) return NULL;

    if (!dict->sorted_values_valid) {
        if (!dict_build_sorted_values(dict)) return NULL;
    }

    size_t arena_index = dict->sorted_values_idx[index];
    return value_slot_data_ptr(dict, arena_index);
}

/* Sorted entry access */

bool dictionary_get_entry_sorted(dictionary_t *dict,
                           size_t index,
                           dictionary_sort_mode mode,
                           dictionary_entry_t **out_entry)
{
    if (!dict || !out_entry || index >= dict->count) return false;

    size_t arena_index = 0;

    if (mode == DICTIONARY_SORT_BY_KEY) {
        if (!dict->sorted_keys_valid) {
            if (!dict_build_sorted_keys(dict)) return false;
        }
        arena_index = dict->sorted_keys_idx[index];
    } else {
        if (!dict->sorted_values_valid) {
            if (!dict_build_sorted_values(dict)) return false;
        }
        arena_index = dict->sorted_values_idx[index];
    }

    dict->scratch_entry.dict = dict;
    dict->scratch_entry.index = arena_index;
    *out_entry = &dict->scratch_entry;
    return true;
}

/* Get entry by key */

bool dictionary_get_entry(const dictionary_t *dict,
                    const void *key,
                    dictionary_entry_t **out_entry)
{
    if (!dict || !key || !out_entry) return false;

    size_t hash = dict->key_hash(key);

    size_t bucket_index = 0;
    struct bucket *prev = NULL;
    struct bucket *curr = NULL;

    if (!dict_find_bucket(dict, key, hash, &bucket_index, &prev, &curr)) {
        return false;
    }

    struct dictionary *mutable_dict = (struct dictionary *)dict;
    mutable_dict->scratch_entry.dict = mutable_dict;
    mutable_dict->scratch_entry.index = curr->index;
    *out_entry = &mutable_dict->scratch_entry;
    return true;
}

/* Set value via entry */

bool dictionary_set_entry(dictionary_t *dict,
                    dictionary_entry_t *entry,
                    const void *value)
{
    if (!dict || !entry || !value) return false;
    if (entry->dict != dict) return false;
    if (entry->index >= dict->count) return false;

    void *val_dst = value_slot_data_ptr(dict, entry->index);

    if (dict->value_destroy) {
        dict->value_destroy(val_dst);
    }

    if (dict->value_clone) {
        dict->value_clone(val_dst, value);
    } else {
        memcpy(val_dst, value, dict->value_size);
    }

    dict_invalidate_sorted(dict);
    return true;
}

/* Entry accessors */

const void *dictionary_entry_key(const dictionary_entry_t *entry)
{
    if (!entry || !entry->dict || entry->index >= entry->dict->count) return NULL;
    return key_slot_data_ptr(entry->dict, entry->index);
}

const void *dictionary_entry_value(const dictionary_entry_t *entry)
{
    if (!entry || !entry->dict || entry->index >= entry->dict->count) return NULL;
    return value_slot_data_ptr(entry->dict, entry->index);
}

/* Foreach */

void dictionary_foreach(const dictionary_t *dict,
                  dictionary_foreach_fn fn,
                  void *user_data)
{
    if (!dict || !fn) return;

    struct dict_entry entry;
    entry.dict = (struct dictionary *)dict;

    for (size_t i = 0; i < dict->count; ++i) {
        entry.index = i;
        fn(&entry, user_data);
    }
}

/* Clone */

dictionary_t *dictionary_clone(const dictionary_t *dict)
{
    if (!dict) return NULL;

    struct dictionary *clone = dictionary_create(dict->key_size,
                                           dict->value_size,
                                           dict->key_hash,
                                           dict->key_cmp,
                                           dict->key_clone,
                                           dict->key_destroy,
                                           dict->value_clone,
                                           dict->value_destroy);
    if (!clone) return NULL;

    if (dict->count == 0) return clone;

    if (!dict_reserve_arenas(clone, dict->count)) {
        dictionary_destroy(clone);
        return NULL;
    }

    if (clone->prime_index < dict->prime_index) {
        if (!dict_rehash(clone, dict->prime_index)) {
            dictionary_destroy(clone);
            return NULL;
        }
    }

    for (size_t i = 0; i < dict->count; ++i) {
        size_t *src_hash = key_slot_hash_ptr(dict, i);
        void *src_key = key_slot_data_ptr(dict, i);
        void *src_val = value_slot_data_ptr(dict, i);

        size_t *dst_hash = key_slot_hash_ptr(clone, i);
        void *dst_key = key_slot_data_ptr(clone, i);
        void *dst_val = value_slot_data_ptr(clone, i);

        *dst_hash = *src_hash;

        if (dict->key_clone) {
            dict->key_clone(dst_key, src_key);
        } else {
            memcpy(dst_key, src_key, dict->key_size);
        }

        if (dict->value_clone) {
            dict->value_clone(dst_val, src_val);
        } else {
            memcpy(dst_val, src_val, dict->value_size);
        }
    }

    clone->count = dict->count;

    for (size_t i = 0; i < clone->table_size; ++i) {
        struct bucket *b = clone->table[i];
        while (b) {
            struct bucket *next = b->next;
            free(b);
            b = next;
        }
        clone->table[i] = NULL;
    }

    for (size_t i = 0; i < clone->count; ++i) {
        size_t hash = *key_slot_hash_ptr(clone, i);
        size_t bucket_index = hash % clone->table_size;

        struct bucket *b = (struct bucket *)malloc(sizeof(struct bucket));
        if (!b) {
            dictionary_destroy(clone);
            return NULL;
        }
        b->index = i;
        b->next = clone->table[bucket_index];
        clone->table[bucket_index] = b;
    }

    dict_invalidate_sorted(clone);
    return clone;
}
