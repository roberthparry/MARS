/* dictionary.c - generic key/value dictionary with open-addressing hash table
 *                and dense arena storage
 *
 * Data layout:
 *   Keys and values are stored in two parallel arenas (key_arena, value_arena).
 *   Each slot in key_arena holds a size_t hash followed by the key bytes;
 *   each slot in value_arena holds the value bytes directly.
 *   A separate hash table (table[]) holds singly-linked bucket chains that
 *   index into the arenas by slot number.
 *
 * Sorted views:
 *   sorted_keys_idx and sorted_values_idx are lazily built index arrays
 *   (sorted_keys_valid / sorted_values_valid track staleness).  They are
 *   rebuilt on the next sorted-iteration request after any mutation.
 *
 * Rehashing:
 *   When load factor exceeds ~0.6, the table is rebuilt at the next prime
 *   capacity from the primes[] table.  Arenas are not moved; only the
 *   bucket chains are reconstructed.
 *
 * Ownership:
 *   If clone/destroy callbacks are provided, the dictionary owns deep copies
 *   of keys and values.  Without them, it stores the raw bytes passed in.
 */

#include <stdlib.h>
#include <string.h>

#include "dictionary.h"

/* Internal bucket structure */

struct bucket {
    size_t          index;
    struct bucket  *next;
};

/* Opaque entry */

struct dict_entry {
    struct dictionary *dict;
    size_t             index;
};

/* Dictionary structure */

struct dictionary {
    size_t key_size;
    size_t value_size;
    size_t key_slot_stride;    /* sizeof(size_t) + key_size */
    size_t value_slot_stride;  /* value_size */

    size_t count;
    size_t capacity;

    unsigned char *key_arena;   /* [hash][key] per slot */
    unsigned char *value_arena; /* [value]     per slot */

    struct bucket **table;
    size_t          table_size;
    size_t          prime_index;

    size_t *sorted_keys_idx;
    size_t  sorted_keys_cap;
    bool    sorted_keys_valid;

    size_t *sorted_values_idx;
    size_t  sorted_values_cap;
    bool    sorted_values_valid;

    dictionary_hash_fn    key_hash;
    dictionary_cmp_fn     key_cmp;
    dictionary_clone_fn   key_clone;
    dictionary_destroy_fn key_destroy;

    dictionary_cmp_fn     value_cmp;
    dictionary_clone_fn   value_clone;
    dictionary_destroy_fn value_destroy;

    /* scratch entry for APIs that return an opaque handle */
    struct dict_entry scratch_entry;
};

/* -------------------------------------------------------------------------
 * Slot helpers
 * ---------------------------------------------------------------------- */

static inline size_t *key_slot_hash_ptr(const struct dictionary *dict, size_t index)
{
    return (size_t *)(dict->key_arena + index * dict->key_slot_stride);
}

static inline void *key_slot_data_ptr(const struct dictionary *dict, size_t index)
{
    return (void *)(dict->key_arena + index * dict->key_slot_stride + sizeof(size_t));
}

static inline void *value_slot_data_ptr(const struct dictionary *dict, size_t index)
{
    return (void *)(dict->value_arena + index * dict->value_slot_stride);
}

/* -------------------------------------------------------------------------
 * Clone / destroy helpers  (eliminate the repeated clone-or-memcpy pattern)
 * ---------------------------------------------------------------------- */

static inline void slot_copy_key(const struct dictionary *dict,
                                 void *dst, const void *src)
{
    if (dict->key_clone) dict->key_clone(dst, src);
    else                 memcpy(dst, src, dict->key_size);
}

static inline void slot_copy_value(const struct dictionary *dict,
                                   void *dst, const void *src)
{
    if (dict->value_clone) dict->value_clone(dst, src);
    else                   memcpy(dst, src, dict->value_size);
}

static inline void slot_destroy_key(const struct dictionary *dict, void *key)
{
    if (dict->key_destroy) dict->key_destroy(key);
}

static inline void slot_destroy_value(const struct dictionary *dict, void *val)
{
    if (dict->value_destroy) dict->value_destroy(val);
}

/* Destroy every live key+value in the arenas (does not free the arenas). */
static void dict_destroy_all_slots(struct dictionary *dict)
{
    for (size_t i = 0; i < dict->count; ++i) {
        slot_destroy_key(dict,   key_slot_data_ptr(dict, i));
        slot_destroy_value(dict, value_slot_data_ptr(dict, i));
    }
}

/* Free every bucket node in the table (does not free the table array). */
static void dict_free_all_buckets(struct dictionary *dict)
{
    if (!dict->table) return;
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

/* -------------------------------------------------------------------------
 * Prime sizes for hash table capacities
 * ---------------------------------------------------------------------- */

static const size_t primes[] = {
    17ul, 37ul, 79ul, 163ul, 331ul, 673ul, 1361ul, 2729ul,
    5471ul, 10949ul, 21911ul, 43853ul, 87719ul, 175447ul,
    350899ul, 701819ul, 1403641ul, 2807303ul, 5614657ul,
    11229331ul, 22458671ul, 44917381ul, 89834777ul, 179669557ul,
    359339171ul, 718678369ul, 1437356741ul
};

static const size_t num_primes = sizeof(primes) / sizeof(primes[0]);

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */

static bool dict_reserve_arenas(struct dictionary *dict, size_t min_capacity);
static bool dict_rehash(struct dictionary *dict, size_t new_prime_index);
static bool dict_ensure_table_capacity(struct dictionary *dict);
static void dict_invalidate_sorted(struct dictionary *dict);
static bool dict_build_sorted_idx(struct dictionary *dict,
                                  size_t **idx, size_t *cap, bool *valid,
                                  int (*cmp)(const struct dictionary *, size_t, size_t));

/* -------------------------------------------------------------------------
 * Create
 * ---------------------------------------------------------------------- */

dictionary_t *dictionary_create(size_t key_size,
                                size_t value_size,
                                dictionary_hash_fn key_hash,
                                dictionary_cmp_fn key_cmp,
                                dictionary_clone_fn key_clone,
                                dictionary_destroy_fn key_destroy,
                                dictionary_cmp_fn value_cmp,
                                dictionary_clone_fn value_clone,
                                dictionary_destroy_fn value_destroy)
{
    if (key_size == 0 || value_size == 0 || !key_hash || !key_cmp)
        return NULL;

    struct dictionary *dict = (struct dictionary *)calloc(1, sizeof(*dict));
    if (!dict) return NULL;

    dict->key_size          = key_size;
    dict->value_size        = value_size;
    dict->key_slot_stride   = sizeof(size_t) + key_size;
    dict->value_slot_stride = value_size;

    dict->key_hash    = key_hash;
    dict->key_cmp     = key_cmp;
    dict->key_clone   = key_clone;
    dict->key_destroy = key_destroy;

    dict->value_cmp     = value_cmp;
    dict->value_clone   = value_clone;
    dict->value_destroy = value_destroy;

    dict->scratch_entry.dict = dict;

    if (!dict_rehash(dict, 0)) {
        dictionary_destroy(dict);
        return NULL;
    }

    return dict;
}

/* -------------------------------------------------------------------------
 * Destroy
 * ---------------------------------------------------------------------- */

void dictionary_destroy(dictionary_t *dict)
{
    if (!dict) return;

    dict_destroy_all_slots(dict);
    dict_free_all_buckets(dict);

    free(dict->table);
    free(dict->key_arena);
    free(dict->value_arena);
    free(dict->sorted_keys_idx);
    free(dict->sorted_values_idx);
    free(dict);
}

/* -------------------------------------------------------------------------
 * Clear
 * ---------------------------------------------------------------------- */

void dictionary_clear(dictionary_t *dict)
{
    if (!dict || dict->count == 0) return;

    dict_destroy_all_slots(dict);
    dict_free_all_buckets(dict);

    dict->count = 0;
    dict_invalidate_sorted(dict);
}

/* -------------------------------------------------------------------------
 * Size
 * ---------------------------------------------------------------------- */

size_t dictionary_size(const dictionary_t *dict)
{
    return dict ? dict->count : 0;
}

/* -------------------------------------------------------------------------
 * Internal: reserve arenas
 * ---------------------------------------------------------------------- */

static bool dict_reserve_arenas(struct dictionary *dict, size_t min_capacity)
{
    if (dict->capacity >= min_capacity) return true;

    size_t new_capacity = dict->capacity ? dict->capacity * 2 : 8;
    if (new_capacity < min_capacity) new_capacity = min_capacity;

    size_t new_key_bytes   = new_capacity * dict->key_slot_stride;
    size_t new_value_bytes = new_capacity * dict->value_slot_stride;

    /* Allocate both before committing, so we can roll back on failure. */
    unsigned char *new_key_arena   = (unsigned char *)realloc(dict->key_arena,   new_key_bytes);
    if (!new_key_arena) return false;
    dict->key_arena = new_key_arena;

    unsigned char *new_value_arena = (unsigned char *)realloc(dict->value_arena, new_value_bytes);
    if (!new_value_arena) return false;
    dict->value_arena = new_value_arena;

    dict->capacity = new_capacity;
    return true;
}

/* -------------------------------------------------------------------------
 * Internal: rehash
 * ---------------------------------------------------------------------- */

static bool dict_rehash(struct dictionary *dict, size_t new_prime_index)
{
    if (new_prime_index >= num_primes)
        return dict->table_size != 0;

    size_t new_size = primes[new_prime_index];
    struct bucket **new_table = (struct bucket **)calloc(new_size, sizeof(*new_table));
    if (!new_table) return false;

    if (dict->table) {
        for (size_t i = 0; i < dict->table_size; ++i) {
            struct bucket *b = dict->table[i];
            while (b) {
                struct bucket *next    = b->next;
                size_t         hash    = *key_slot_hash_ptr(dict, b->index);
                size_t         slot    = hash % new_size;
                b->next                = new_table[slot];
                new_table[slot]        = b;
                b = next;
            }
        }
        free(dict->table);
    }

    dict->table      = new_table;
    dict->table_size = new_size;
    dict->prime_index = new_prime_index;
    return true;
}

/* -------------------------------------------------------------------------
 * Internal: ensure table capacity
 * ---------------------------------------------------------------------- */

static bool dict_ensure_table_capacity(struct dictionary *dict)
{
    if (!dict || dict->table_size == 0) return false;

    double load = (double)dict->count / (double)dict->table_size;
    if (load < 0.6 || dict->prime_index + 1 >= num_primes) return true;

    return dict_rehash(dict, dict->prime_index + 1);
}

/* -------------------------------------------------------------------------
 * Internal: invalidate sorted views
 * ---------------------------------------------------------------------- */

static void dict_invalidate_sorted(struct dictionary *dict)
{
    if (!dict) return;
    dict->sorted_keys_valid   = false;
    dict->sorted_values_valid = false;
}

/* -------------------------------------------------------------------------
 * Internal: find bucket
 * ---------------------------------------------------------------------- */

static bool dict_find_bucket(const struct dictionary *dict,
                             const void *key,
                             size_t hash,
                             size_t *out_bucket_index,
                             struct bucket **out_prev,
                             struct bucket **out_curr)
{
    if (!dict || dict->table_size == 0) return false;

    size_t         slot = hash % dict->table_size;
    struct bucket *prev = NULL;
    struct bucket *curr = dict->table[slot];

    while (curr) {
        void *stored_key = key_slot_data_ptr(dict, curr->index);
        if (dict->key_cmp(stored_key, key) == 0) {
            if (out_bucket_index) *out_bucket_index = slot;
            if (out_prev)         *out_prev         = prev;
            if (out_curr)         *out_curr         = curr;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

/* -------------------------------------------------------------------------
 * Set
 * ---------------------------------------------------------------------- */

bool dictionary_set(dictionary_t *dict, const void *key, const void *value)
{
    if (!dict || !key || !value) return false;

    size_t         hash         = dict->key_hash(key);
    size_t         bucket_index = 0;
    struct bucket *prev         = NULL;
    struct bucket *curr         = NULL;

    if (dict_find_bucket(dict, key, hash, &bucket_index, &prev, &curr)) {
        /* Update existing value. */
        void *val_dst = value_slot_data_ptr(dict, curr->index);
        slot_destroy_value(dict, val_dst);
        slot_copy_value(dict, val_dst, value);
        dict_invalidate_sorted(dict);
        return true;
    }

    if (!dict_reserve_arenas(dict, dict->count + 1)) return false;
    if (!dict_ensure_table_capacity(dict))           return false;

    size_t  index    = dict->count;
    void   *key_dst  = key_slot_data_ptr(dict, index);
    void   *val_dst  = value_slot_data_ptr(dict, index);

    *key_slot_hash_ptr(dict, index) = hash;
    slot_copy_key(dict,   key_dst,  key);
    slot_copy_value(dict, val_dst, value);

    struct bucket *b = (struct bucket *)malloc(sizeof(*b));
    if (!b) {
        slot_destroy_key(dict,   key_dst);
        slot_destroy_value(dict, val_dst);
        return false;
    }

    bucket_index   = hash % dict->table_size;
    b->index       = index;
    b->next        = dict->table[bucket_index];
    dict->table[bucket_index] = b;

    dict->count++;
    dict_invalidate_sorted(dict);
    return true;
}

/* -------------------------------------------------------------------------
 * Get (copy out)
 * ---------------------------------------------------------------------- */

bool dictionary_get(const dictionary_t *dict, const void *key, void *out_value)
{
    if (!dict || !key || !out_value) return false;

    size_t         hash = dict->key_hash(key);
    struct bucket *curr = NULL;

    if (!dict_find_bucket(dict, key, hash, NULL, NULL, &curr))
        return false;

    void *val_src = value_slot_data_ptr(dict, curr->index);
    slot_copy_value(dict, out_value, val_src);
    return true;
}

/* -------------------------------------------------------------------------
 * Remove
 * ---------------------------------------------------------------------- */

bool dictionary_remove(dictionary_t *dict, const void *key)
{
    if (!dict || !key || dict->count == 0 || dict->table_size == 0) return false;

    size_t         hash         = dict->key_hash(key);
    size_t         bucket_index = hash % dict->table_size;
    struct bucket *prev         = NULL;
    struct bucket *curr         = dict->table[bucket_index];

    /* 1. Find the bucket node. */
    while (curr) {
        if (dict->key_cmp(key_slot_data_ptr(dict, curr->index), key) == 0) break;
        prev = curr;
        curr = curr->next;
    }
    if (!curr) return false;

    size_t remove_index = curr->index;

    /* 2. Unlink and free the bucket node. */
    if (prev) prev->next             = curr->next;
    else      dict->table[bucket_index] = curr->next;
    free(curr);

    /* 3. Destroy the slot's key and value. */
    slot_destroy_key(dict,   key_slot_data_ptr(dict,   remove_index));
    slot_destroy_value(dict, value_slot_data_ptr(dict, remove_index));

    size_t last_index = dict->count - 1;

    if (remove_index != last_index) {
        /* 4. Move the last slot into the gap. */
        size_t *src_hash = key_slot_hash_ptr(dict, last_index);
        void   *src_key  = key_slot_data_ptr(dict,  last_index);
        void   *src_val  = value_slot_data_ptr(dict, last_index);
        size_t *dst_hash = key_slot_hash_ptr(dict, remove_index);
        void   *dst_key  = key_slot_data_ptr(dict,  remove_index);
        void   *dst_val  = value_slot_data_ptr(dict, remove_index);

        *dst_hash = *src_hash;

        /* Move key: clone into dst then destroy src (avoids aliasing). */
        slot_copy_key(dict, dst_key, src_key);
        if (dict->key_destroy) dict->key_destroy(src_key);

        /* Move value: same. */
        slot_copy_value(dict, dst_val, src_val);
        if (dict->value_destroy) dict->value_destroy(src_val);

        /* 5. Retarget all bucket nodes for last_index -> remove_index. */
        size_t moved_slot = (*dst_hash) % dict->table_size;
        struct bucket **pp = &dict->table[moved_slot];
        while (*pp) {
            if ((*pp)->index == last_index) {
                struct bucket *dead = *pp;
                *pp    = dead->next;
                free(dead);
                /* keep iterating — there should be exactly one, but be safe */
                continue;
            }
            pp = &(*pp)->next;
        }

        /* 6. Insert a fresh bucket node for the moved slot. */
        struct bucket *nb = (struct bucket *)malloc(sizeof(*nb));
        if (!nb) {
            /* count is already stale — best-effort: mark corrupt? */
            dict->count--;
            dict_invalidate_sorted(dict);
            return false;
        }
        nb->index              = remove_index;
        nb->next               = dict->table[moved_slot];
        dict->table[moved_slot] = nb;
    }

    dict->count--;
    dict_invalidate_sorted(dict);
    return true;
}

/* -------------------------------------------------------------------------
 * Direct arena access
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * Internal: build a sorted index array (thread-safe via qsort_r)
 * ---------------------------------------------------------------------- */

struct sort_ctx {
    struct dictionary *dict;
    int (*cmp)(const struct dictionary *, size_t, size_t);
};

static int sort_thunk(const void *a, const void *b, void *arg)
{
    struct sort_ctx *ctx = (struct sort_ctx *)arg;
    return ctx->cmp(ctx->dict, *(const size_t *)a, *(const size_t *)b);
}

static int sort_cmp_keys_ctx(const struct dictionary *dict, size_t ia, size_t ib)
{
    return dict->key_cmp(key_slot_data_ptr(dict, ia),
                         key_slot_data_ptr(dict, ib));
}

static int sort_cmp_values_ctx(const struct dictionary *dict, size_t ia, size_t ib)
{
    if (dict->value_cmp)
        return dict->value_cmp(value_slot_data_ptr(dict, ia),
                               value_slot_data_ptr(dict, ib));
    return memcmp(value_slot_data_ptr(dict, ia),
                  value_slot_data_ptr(dict, ib),
                  dict->value_size);
}

static bool dict_build_sorted_idx(struct dictionary *dict,
                                  size_t **idx, size_t *cap, bool *valid,
                                  int (*cmp)(const struct dictionary *, size_t, size_t))
{
    if (!dict) return false;

    if (dict->count == 0) { *valid = true; return true; }

    if (*cap < dict->count) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        if (new_cap < dict->count) new_cap = dict->count;
        size_t *new_idx = (size_t *)realloc(*idx, new_cap * sizeof(size_t));
        if (!new_idx) return false;
        *idx = new_idx;
        *cap = new_cap;
    }

    for (size_t i = 0; i < dict->count; ++i) (*idx)[i] = i;

    struct sort_ctx ctx = { .dict = dict, .cmp = cmp };
    qsort_r(*idx, dict->count, sizeof(size_t), sort_thunk, &ctx);

    *valid = true;
    return true;
}

/* -------------------------------------------------------------------------
 * Sorted key/value access
 * ---------------------------------------------------------------------- */

const void *dictionary_get_key_sorted(dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count) return NULL;

    if (!dict->sorted_keys_valid &&
        !dict_build_sorted_idx(dict,
                               &dict->sorted_keys_idx, &dict->sorted_keys_cap,
                               &dict->sorted_keys_valid, sort_cmp_keys_ctx))
        return NULL;

    return key_slot_data_ptr(dict, dict->sorted_keys_idx[index]);
}

const void *dictionary_get_value_sorted(dictionary_t *dict, size_t index)
{
    if (!dict || index >= dict->count || !dict->value_cmp) return NULL;

    if (!dict->sorted_values_valid &&
        !dict_build_sorted_idx(dict,
                               &dict->sorted_values_idx, &dict->sorted_values_cap,
                               &dict->sorted_values_valid, sort_cmp_values_ctx))
        return NULL;

    return value_slot_data_ptr(dict, dict->sorted_values_idx[index]);
}

/* -------------------------------------------------------------------------
 * Sorted entry access
 * ---------------------------------------------------------------------- */

bool dictionary_get_entry_sorted(dictionary_t *dict,
                                 size_t index,
                                 dictionary_sort_mode mode,
                                 dictionary_entry_t **out_entry)
{
    if (!dict || !out_entry || index >= dict->count) return false;

    size_t arena_index = 0;

    if (mode == DICTIONARY_SORT_BY_KEY) {
        if (!dict->sorted_keys_valid &&
            !dict_build_sorted_idx(dict,
                                   &dict->sorted_keys_idx, &dict->sorted_keys_cap,
                                   &dict->sorted_keys_valid, sort_cmp_keys_ctx))
            return false;
        arena_index = dict->sorted_keys_idx[index];
    } else {
        if (!dict->value_cmp) return false;
        if (!dict->sorted_values_valid &&
            !dict_build_sorted_idx(dict,
                                   &dict->sorted_values_idx, &dict->sorted_values_cap,
                                   &dict->sorted_values_valid, sort_cmp_values_ctx))
            return false;
        arena_index = dict->sorted_values_idx[index];
    }

    dict->scratch_entry.dict  = dict;
    dict->scratch_entry.index = arena_index;
    *out_entry = &dict->scratch_entry;
    return true;
}

/* -------------------------------------------------------------------------
 * Get entry by key
 * ---------------------------------------------------------------------- */

bool dictionary_get_entry(const dictionary_t *dict,
                          const void *key,
                          dictionary_entry_t **out_entry)
{
    if (!dict || !key || !out_entry) return false;

    size_t         hash = dict->key_hash(key);
    struct bucket *curr = NULL;

    if (!dict_find_bucket(dict, key, hash, NULL, NULL, &curr))
        return false;

    struct dictionary *md     = (struct dictionary *)dict;
    md->scratch_entry.dict    = md;
    md->scratch_entry.index   = curr->index;
    *out_entry = &md->scratch_entry;
    return true;
}

/* -------------------------------------------------------------------------
 * Set value via entry
 * ---------------------------------------------------------------------- */

bool dictionary_set_entry(dictionary_t *dict,
                          dictionary_entry_t *entry,
                          const void *value)
{
    if (!dict || !entry || !value)         return false;
    if (entry->dict  != dict)              return false;
    if (entry->index >= dict->count)       return false;

    void *val_dst = value_slot_data_ptr(dict, entry->index);
    slot_destroy_value(dict, val_dst);
    slot_copy_value(dict, val_dst, value);
    dict_invalidate_sorted(dict);
    return true;
}

/* -------------------------------------------------------------------------
 * Entry accessors
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * Foreach
 * ---------------------------------------------------------------------- */

void dictionary_foreach(const dictionary_t *dict,
                        dictionary_foreach_fn fn,
                        void *user_data)
{
    if (!dict || !fn) return;

    struct dict_entry entry = { .dict = (struct dictionary *)dict };

    for (size_t i = 0; i < dict->count; ++i) {
        entry.index = i;
        fn(&entry, user_data);
    }
}

/* -------------------------------------------------------------------------
 * Clone
 * ---------------------------------------------------------------------- */

dictionary_t *dictionary_clone(const dictionary_t *dict)
{
    if (!dict) return NULL;

    struct dictionary *clone = dictionary_create(dict->key_size,
                                                 dict->value_size,
                                                 dict->key_hash,
                                                 dict->key_cmp,
                                                 dict->key_clone,
                                                 dict->key_destroy,
                                                 dict->value_cmp,
                                                 dict->value_clone,
                                                 dict->value_destroy);
    if (!clone) return NULL;

    if (dict->count == 0) return clone;

    if (!dict_reserve_arenas(clone, dict->count) ||
        (clone->prime_index < dict->prime_index &&
         !dict_rehash(clone, dict->prime_index))) {
        dictionary_destroy(clone);
        return NULL;
    }

    for (size_t i = 0; i < dict->count; ++i) {
        *key_slot_hash_ptr(clone, i) = *key_slot_hash_ptr(dict, i);
        slot_copy_key(clone,   key_slot_data_ptr(clone, i),   key_slot_data_ptr(dict, i));
        slot_copy_value(clone, value_slot_data_ptr(clone, i), value_slot_data_ptr(dict, i));
    }

    clone->count = dict->count;

    /* Rebuild bucket chains from scratch. */
    dict_free_all_buckets(clone);

    for (size_t i = 0; i < clone->count; ++i) {
        size_t         hash = *key_slot_hash_ptr(clone, i);
        size_t         slot = hash % clone->table_size;
        struct bucket *b    = (struct bucket *)malloc(sizeof(*b));
        if (!b) {
            dictionary_destroy(clone);
            return NULL;
        }
        b->index            = i;
        b->next             = clone->table[slot];
        clone->table[slot]  = b;
    }

    return clone;
}
