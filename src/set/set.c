/* set.c - implementation of generic value-set container with dense arena and lazy sorting */

#include "set.h"

#include <stdlib.h>
#include <string.h>

/* Internal structures */

struct bucket {
    size_t index;          /* index into arena */
    struct bucket *next;   /* next in chain */
};

struct set {
    size_t elem_size;      /* size of each element */
    size_t slot_stride;    /* bytes per slot: sizeof(size_t) + elem_size */

    size_t count;          /* number of elements stored */
    size_t capacity;       /* number of slots allocated in arena */

    unsigned char *arena;  /* contiguous storage for slots */

    struct bucket **table; /* hash table: array of bucket* chains */
    size_t table_size;     /* number of buckets in table */

    size_t prime_index;    /* index into primes[] for table sizing */

    size_t *sorted_idx;    /* lazily built array of indices for sorted view */
    size_t sorted_cap;     /* capacity of sorted_idx */
    bool sorted_valid;     /* whether sorted_idx is up-to-date */

    set_hash_fn hash_fn;
    set_cmp_fn cmp_fn;
    set_clone_fn clone_fn;
    set_destroy_fn destroy_fn;
};

/* Slot layout:
 *
 * Each slot in the arena is laid out as:
 *
 *   struct {
 *       size_t hash;
 *       unsigned char data[elem_size];
 *   }
 *
 * We implement this via a stride and pointer arithmetic.
 */

static inline size_t *slot_hash_ptr(const struct set *set, size_t index) {
    return (size_t *)(set->arena + index * set->slot_stride);
}

static inline void *slot_data_ptr(const struct set *set, size_t index) {
    return (void *)(set->arena + index * set->slot_stride + sizeof(size_t));
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

/* Forward declarations of internal helpers */
static bool set_reserve_arena(struct set *set, size_t min_capacity);
static bool set_rehash(struct set *set, size_t new_prime_index);
static bool set_ensure_table_capacity(struct set *set);
static void set_invalidate_sorted(struct set *set);
static bool set_build_sorted(struct set *set);

/* Create a new set */
set_t *set_create(size_t elem_size,
                  set_hash_fn hash,
                  set_cmp_fn cmp,
                  set_clone_fn clone,
                  set_destroy_fn destroy)
{
    if (elem_size == 0 || hash == NULL || cmp == NULL) {
        return NULL;
    }

    struct set *set = (struct set *)calloc(1, sizeof(struct set));
    if (!set) {
        return NULL;
    }

    set->elem_size = elem_size;
    set->slot_stride = sizeof(size_t) + elem_size;
    set->count = 0;
    set->capacity = 0;
    set->arena = NULL;

    set->table = NULL;
    set->table_size = 0;
    set->prime_index = 0;

    set->sorted_idx = NULL;
    set->sorted_cap = 0;
    set->sorted_valid = false;

    set->hash_fn = hash;
    set->cmp_fn = cmp;
    set->clone_fn = clone;
    set->destroy_fn = destroy;

    /* Initialize hash table with smallest prime */
    if (!set_rehash(set, 0)) {
        set_destroy(set);
        return NULL;
    }

    return set;
}

/* Destroy a set */
void set_destroy(set_t *set)
{
    if (!set) {
        return;
    }

    if (set->destroy_fn && set->arena) {
        for (size_t i = 0; i < set->count; ++i) {
            void *elem = slot_data_ptr(set, i);
            set->destroy_fn(elem);
        }
    }

    if (set->table) {
        for (size_t i = 0; i < set->table_size; ++i) {
            struct bucket *b = set->table[i];
            while (b) {
                struct bucket *next = b->next;
                free(b);
                b = next;
            }
        }
        free(set->table);
    }

    free(set->arena);
    free(set->sorted_idx);
    free(set);
}

/* Clear all elements from the set */
void set_clear(set_t *set)
{
    if (!set || set->count == 0) {
        return;
    }

    if (set->destroy_fn && set->arena) {
        for (size_t i = 0; i < set->count; ++i) {
            void *elem = slot_data_ptr(set, i);
            set->destroy_fn(elem);
        }
    }

    /* Clear buckets */
    if (set->table) {
        for (size_t i = 0; i < set->table_size; ++i) {
            struct bucket *b = set->table[i];
            while (b) {
                struct bucket *next = b->next;
                free(b);
                b = next;
            }
            set->table[i] = NULL;
        }
    }

    set->count = 0;
    set_invalidate_sorted(set);
}

/* Get size */
size_t set_get_size(const set_t *set)
{
    return set ? set->count : 0;
}

/* Internal: find bucket and previous bucket for an element */
static bool set_find_bucket(const struct set *set,
                            const void *elem,
                            size_t hash,
                            size_t *out_bucket_index,
                            struct bucket **out_prev,
                            struct bucket **out_curr)
{
    if (!set || set->table_size == 0) {
        return false;
    }

    size_t bucket_index = hash % set->table_size;
    struct bucket *prev = NULL;
    struct bucket *curr = set->table[bucket_index];

    while (curr) {
        void *stored = slot_data_ptr(set, curr->index);
        if (set->cmp_fn(stored, elem) == 0) {
            if (out_bucket_index) {
                *out_bucket_index = bucket_index;
            }
            if (out_prev) {
                *out_prev = prev;
            }
            if (out_curr) {
                *out_curr = curr;
            }
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

/* Check containment */
bool set_contains(const set_t *set, const void *elem)
{
    if (!set || !elem || set->table_size == 0) {
        return false;
    }

    size_t hash = set->hash_fn(elem);
    return set_find_bucket(set, elem, hash, NULL, NULL, NULL);
}

/* Internal: ensure arena capacity */
static bool set_reserve_arena(struct set *set, size_t min_capacity)
{
    if (set->capacity >= min_capacity) {
        return true;
    }

    size_t new_capacity = set->capacity ? set->capacity * 2 : 8;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }

    size_t new_size_bytes = new_capacity * set->slot_stride;
    unsigned char *new_arena = (unsigned char *)realloc(set->arena, new_size_bytes);
    if (!new_arena) {
        return false;
    }

    set->arena = new_arena;
    set->capacity = new_capacity;
    return true;
}

/* Internal: rehash table to new prime index */
static bool set_rehash(struct set *set, size_t new_prime_index)
{
    if (new_prime_index >= num_primes) {
        /* Cannot grow further */
        if (set->table_size == 0) {
            return false;
        }
        return true;
    }

    size_t new_size = primes[new_prime_index];
    struct bucket **new_table = (struct bucket **)calloc(new_size, sizeof(struct bucket *));
    if (!new_table) {
        return false;
    }

    /* Reinsert existing indices */
    if (set->table) {
        for (size_t i = 0; i < set->table_size; ++i) {
            struct bucket *b = set->table[i];
            while (b) {
                struct bucket *next = b->next;

                size_t idx = b->index;
                size_t hash = *slot_hash_ptr(set, idx);
                size_t bucket_index = hash % new_size;

                b->next = new_table[bucket_index];
                new_table[bucket_index] = b;

                b = next;
            }
        }
        free(set->table);
    }

    set->table = new_table;
    set->table_size = new_size;
    set->prime_index = new_prime_index;
    return true;
}

/* Internal: ensure table capacity based on load factor */
static bool set_ensure_table_capacity(struct set *set)
{
    if (!set || set->table_size == 0) {
        return false;
    }

    /* Load factor threshold: 0.6 */
    double load = (double)set->count / (double)set->table_size;
    if (load < 0.6) {
        return true;
    }

    if (set->prime_index + 1 >= num_primes) {
        /* Cannot grow further */
        return true;
    }

    return set_rehash(set, set->prime_index + 1);
}

/* Invalidate sorted view */
static void set_invalidate_sorted(struct set *set)
{
    if (!set) {
        return;
    }
    set->sorted_valid = false;
}

/* Add element */
bool set_add(set_t *set, const void *elem)
{
    if (!set || !elem) {
        return false;
    }

    size_t hash = set->hash_fn(elem);

    /* Check if already present */
    if (set_find_bucket(set, elem, hash, NULL, NULL, NULL)) {
        return false;
    }

    /* Ensure arena capacity */
    if (!set_reserve_arena(set, set->count + 1)) {
        return false;
    }

    /* Ensure table capacity (may rehash) */
    if (!set_ensure_table_capacity(set)) {
        return false;
    }

    size_t index = set->count;
    size_t *hash_ptr = slot_hash_ptr(set, index);
    void *dst = slot_data_ptr(set, index);

    *hash_ptr = hash;
    if (set->clone_fn) {
        set->clone_fn(dst, elem);
    } else {
        memcpy(dst, elem, set->elem_size);
    }

    /* Insert into hash table */
    size_t bucket_index = hash % set->table_size;
    struct bucket *b = (struct bucket *)malloc(sizeof(struct bucket));
    if (!b) {
        /* Roll back element construction */
        if (set->destroy_fn) {
            set->destroy_fn(dst);
        }
        return false;
    }
    b->index = index;
    b->next = set->table[bucket_index];
    set->table[bucket_index] = b;

    set->count++;
    set_invalidate_sorted(set);
    return true;
}

/* Remove element */
bool set_remove(set_t *set, const void *elem)
{
    if (!set || !elem || set->count == 0 || set->table_size == 0) {
        return false;
    }

    size_t hash = set->hash_fn(elem);
    size_t bucket_index = 0;
    struct bucket *prev = NULL;
    struct bucket *curr = NULL;

    if (!set_find_bucket(set, elem, hash, &bucket_index, &prev, &curr)) {
        return false;
    }

    size_t remove_index = curr->index;

    /* Remove bucket from chain */
    if (prev) {
        prev->next = curr->next;
    } else {
        set->table[bucket_index] = curr->next;
    }
    free(curr);

    /* Destroy element if needed */
    void *remove_elem = slot_data_ptr(set, remove_index);
    if (set->destroy_fn) {
        set->destroy_fn(remove_elem);
    }

    /* If removing last element, just decrement count */
    size_t last_index = set->count - 1;
    if (remove_index != last_index) {
        /* Move last element into removed slot */
        size_t *dst_hash = slot_hash_ptr(set, remove_index);
        void *dst_data = slot_data_ptr(set, remove_index);

        size_t *src_hash = slot_hash_ptr(set, last_index);
        void *src_data = slot_data_ptr(set, last_index);

        *dst_hash = *src_hash;
        if (set->clone_fn) {
            /* Re-clone from src_data into dst_data */
            set->clone_fn(dst_data, src_data);
            /* Destroy the moved-from last element if needed */
            if (set->destroy_fn) {
                set->destroy_fn(src_data);
            }
        } else {
            memcpy(dst_data, src_data, set->elem_size);
        }

        /* Update bucket that pointed to last_index to now point to remove_index */
        size_t moved_hash = *dst_hash;
        size_t moved_bucket_index = moved_hash % set->table_size;
        struct bucket *b = set->table[moved_bucket_index];
        while (b) {
            if (b->index == last_index) {
                b->index = remove_index;
                break;
            }
            b = b->next;
        }
    }

    set->count--;
    set_invalidate_sorted(set);
    return true;
}

/* Get element by arena index */
const void *set_get(const set_t *set, size_t index)
{
    if (!set || index >= set->count) {
        return NULL;
    }
    return slot_data_ptr(set, index);
}

/* Global/static pointer used for qsort comparator */
struct sort_ctx {
    struct set *set;
};

static struct sort_ctx g_sort_ctx;

/* Comparator for qsort over indices */
static int set_qsort_index_cmp(const void *a, const void *b)
{
    const size_t ia = *(const size_t *)a;
    const size_t ib = *(const size_t *)b;

    void *ea = slot_data_ptr(g_sort_ctx.set, ia);
    void *eb = slot_data_ptr(g_sort_ctx.set, ib);

    return g_sort_ctx.set->cmp_fn(ea, eb);
}

/* Build sorted index array */
static bool set_build_sorted(struct set *set)
{
    if (!set) {
        return false;
    }

    if (set->count == 0) {
        set->sorted_valid = true;
        return true;
    }

    if (set->sorted_cap < set->count) {
        size_t new_cap = set->sorted_cap ? set->sorted_cap * 2 : 8;
        if (new_cap < set->count) {
            new_cap = set->count;
        }
        size_t *new_idx = (size_t *)realloc(set->sorted_idx, new_cap * sizeof(size_t));
        if (!new_idx) {
            return false;
        }
        set->sorted_idx = new_idx;
        set->sorted_cap = new_cap;
    }

    for (size_t i = 0; i < set->count; ++i) {
        set->sorted_idx[i] = i;
    }

    g_sort_ctx.set = set;
    qsort(set->sorted_idx, set->count, sizeof(size_t), set_qsort_index_cmp);
    set->sorted_valid = true;
    return true;
}

/* Get element by sorted index */
const void *set_get_sorted(set_t *set, size_t index)
{
    if (!set || index >= set->count) {
        return NULL;
    }

    if (!set->sorted_valid) {
        if (!set_build_sorted(set)) {
            return NULL;
        }
    }

    size_t arena_index = set->sorted_idx[index];
    return slot_data_ptr(set, arena_index);
}

/* Clone a set */
set_t *set_clone(const set_t *set)
{
    if (!set) {
        return NULL;
    }

    struct set *clone = set_create(set->elem_size,
                                   set->hash_fn,
                                   set->cmp_fn,
                                   set->clone_fn,
                                   set->destroy_fn);
    if (!clone) {
        return NULL;
    }

    if (set->count == 0) {
        return clone;
    }

    if (!set_reserve_arena(clone, set->count)) {
        set_destroy(clone);
        return NULL;
    }

    /* Ensure table size at least as large as original's prime index */
    if (clone->prime_index < set->prime_index) {
        if (!set_rehash(clone, set->prime_index)) {
            set_destroy(clone);
            return NULL;
        }
    }

    /* Copy elements */
    for (size_t i = 0; i < set->count; ++i) {
        size_t *src_hash = slot_hash_ptr(set, i);
        void *src_data = slot_data_ptr(set, i);

        size_t *dst_hash = slot_hash_ptr(clone, i);
        void *dst_data = slot_data_ptr(clone, i);

        *dst_hash = *src_hash;
        if (set->clone_fn) {
            set->clone_fn(dst_data, src_data);
        } else {
            memcpy(dst_data, src_data, set->elem_size);
        }
    }

    clone->count = set->count;

    /* Rebuild hash table for clone */
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
        size_t hash = *slot_hash_ptr(clone, i);
        size_t bucket_index = hash % clone->table_size;

        struct bucket *b = (struct bucket *)malloc(sizeof(struct bucket));
        if (!b) {
            set_destroy(clone);
            return NULL;
        }
        b->index = i;
        b->next = clone->table[bucket_index];
        clone->table[bucket_index] = b;
    }

    set_invalidate_sorted(clone);
    return clone;
}

/* Union of two sets */
set_t *set_union(const set_t *a, const set_t *b)
{
    if (!a || !b) {
        return NULL;
    }

    struct set *res = set_create(a->elem_size,
                                 a->hash_fn,
                                 a->cmp_fn,
                                 a->clone_fn,
                                 a->destroy_fn);
    if (!res) {
        return NULL;
    }

    /* Reserve approximate capacity */
    size_t total = a->count + b->count;
    if (!set_reserve_arena(res, total)) {
        set_destroy(res);
        return NULL;
    }

    /* Insert all from a */
    for (size_t i = 0; i < a->count; ++i) {
        const void *elem = slot_data_ptr(a, i);
        if (!set_add(res, elem)) {
            /* set_add returns false if duplicate; that's fine */
        }
    }

    /* Insert all from b */
    for (size_t i = 0; i < b->count; ++i) {
        const void *elem = slot_data_ptr(b, i);
        if (!set_add(res, elem)) {
            /* duplicate; ignore */
        }
    }

    return res;
}

/* Intersection of two sets */
set_t *set_intersection(const set_t *a, const set_t *b)
{
    if (!a || !b) {
        return NULL;
    }

    struct set *res = set_create(a->elem_size,
                                 a->hash_fn,
                                 a->cmp_fn,
                                 a->clone_fn,
                                 a->destroy_fn);
    if (!res) {
        return NULL;
    }

    /* Iterate over smaller set for efficiency */
    const struct set *small = a;
    const struct set *large = b;
    if (b->count < a->count) {
        small = b;
        large = a;
    }

    if (!set_reserve_arena(res, small->count)) {
        set_destroy(res);
        return NULL;
    }

    for (size_t i = 0; i < small->count; ++i) {
        const void *elem = slot_data_ptr(small, i);
        if (set_contains(large, elem)) {
            if (!set_add(res, elem)) {
                /* duplicate; ignore */
            }
        }
    }

    return res;
}

/* Difference a \ b */
set_t *set_difference(const set_t *a, const set_t *b)
{
    if (!a || !b) {
        return NULL;
    }

    struct set *res = set_create(a->elem_size,
                                 a->hash_fn,
                                 a->cmp_fn,
                                 a->clone_fn,
                                 a->destroy_fn);
    if (!res) {
        return NULL;
    }

    if (!set_reserve_arena(res, a->count)) {
        set_destroy(res);
        return NULL;
    }

    for (size_t i = 0; i < a->count; ++i) {
        const void *elem = slot_data_ptr(a, i);
        if (!set_contains(b, elem)) {
            if (!set_add(res, elem)) {
                /* should not happen; ignore */
            }
        }
    }

    return res;
}

/* Subset check */
bool set_is_subset(const set_t *a, const set_t *b)
{
    if (!a || !b) {
        return false;
    }

    if (a->count > b->count) {
        return false;
    }

    for (size_t i = 0; i < a->count; ++i) {
        const void *elem = slot_data_ptr(a, i);
        if (!set_contains(b, elem)) {
            return false;
        }
    }

    return true;
}
