#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "bitset.h"

#define BITSET_WORD_BITS 64u
#define BITSET_INIT_WORDS 1u

typedef struct _bitset_t {
    uint64_t       *words;
    size_t          word_count;
    pthread_mutex_t mutex;
} bitset_t;

static inline size_t words_for_bits(size_t bits) {
    return (bits + BITSET_WORD_BITS - 1) / BITSET_WORD_BITS;
}

/* Requires mutex held. Grows to hold at least `needed_words` words. */
static bool bitset_grow_to(bitset_t *bs, size_t needed_words) {
    if (needed_words <= bs->word_count) return true;
    size_t new_count = bs->word_count ? bs->word_count * 2 : BITSET_INIT_WORDS;
    while (new_count < needed_words) new_count *= 2;
    uint64_t *new_words = realloc(bs->words, new_count * sizeof(uint64_t));
    if (!new_words) return false;
    memset(new_words + bs->word_count, 0,
           (new_count - bs->word_count) * sizeof(uint64_t));
    bs->words      = new_words;
    bs->word_count = new_count;
    return true;
}

bitset_t *bitset_create(size_t initial_capacity) {
    bitset_t *bs = malloc(sizeof(bitset_t));
    if (!bs) return NULL;

    size_t init_words = initial_capacity
        ? words_for_bits(initial_capacity)
        : BITSET_INIT_WORDS;

    bs->words = calloc(init_words, sizeof(uint64_t));
    if (!bs->words) { free(bs); return NULL; }
    bs->word_count = init_words;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&bs->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    return bs;
}

void bitset_destroy(bitset_t *bs) {
    if (!bs) return;
    free(bs->words);
    pthread_mutex_destroy(&bs->mutex);
    free(bs);
}

void bitset_clear(bitset_t *bs) {
    if (!bs) return;
    pthread_mutex_lock(&bs->mutex);
    memset(bs->words, 0, bs->word_count * sizeof(uint64_t));
    pthread_mutex_unlock(&bs->mutex);
}

bitset_t *bitset_clone(const bitset_t *bs) {
    if (!bs) return NULL;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    bitset_t *copy = bitset_create(bs->word_count * BITSET_WORD_BITS);
    if (copy) {
        /* grow_to already allocated enough words; word_count may differ */
        if (!bitset_grow_to(copy, bs->word_count)) {
            pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
            bitset_destroy(copy);
            return NULL;
        }
        memcpy(copy->words, bs->words, bs->word_count * sizeof(uint64_t));
    }
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return copy;
}

size_t bitset_capacity(const bitset_t *bs) {
    if (!bs) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    size_t cap = bs->word_count * BITSET_WORD_BITS;
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return cap;
}

bool bitset_set(bitset_t *bs, size_t index) {
    if (!bs) return false;
    pthread_mutex_lock(&bs->mutex);
    size_t word = index / BITSET_WORD_BITS;
    if (!bitset_grow_to(bs, word + 1)) {
        pthread_mutex_unlock(&bs->mutex);
        return false;
    }
    bs->words[word] |= (uint64_t)1 << (index % BITSET_WORD_BITS);
    pthread_mutex_unlock(&bs->mutex);
    return true;
}

void bitset_unset(bitset_t *bs, size_t index) {
    if (!bs) return;
    pthread_mutex_lock(&bs->mutex);
    size_t word = index / BITSET_WORD_BITS;
    if (word < bs->word_count)
        bs->words[word] &= ~((uint64_t)1 << (index % BITSET_WORD_BITS));
    pthread_mutex_unlock(&bs->mutex);
}

bool bitset_toggle(bitset_t *bs, size_t index) {
    if (!bs) return false;
    pthread_mutex_lock(&bs->mutex);
    size_t word = index / BITSET_WORD_BITS;
    if (!bitset_grow_to(bs, word + 1)) {
        pthread_mutex_unlock(&bs->mutex);
        return false;
    }
    bs->words[word] ^= (uint64_t)1 << (index % BITSET_WORD_BITS);
    pthread_mutex_unlock(&bs->mutex);
    return true;
}

bool bitset_test(const bitset_t *bs, size_t index) {
    if (!bs) return false;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    size_t word = index / BITSET_WORD_BITS;
    bool result = (word < bs->word_count) &&
                  ((bs->words[word] >> (index % BITSET_WORD_BITS)) & 1u);
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return result;
}

bool bitset_set_range(bitset_t *bs, size_t start, size_t end) {
    if (!bs || start >= end) return false;
    pthread_mutex_lock(&bs->mutex);
    size_t last_word = (end - 1) / BITSET_WORD_BITS;
    if (!bitset_grow_to(bs, last_word + 1)) {
        pthread_mutex_unlock(&bs->mutex);
        return false;
    }
    for (size_t i = start; i < end; ) {
        size_t word  = i / BITSET_WORD_BITS;
        size_t bit   = i % BITSET_WORD_BITS;
        size_t avail = BITSET_WORD_BITS - bit;
        size_t rem   = end - i;
        if (rem >= avail) {
            bs->words[word] |= ~(uint64_t)0 << bit;
            i += avail;
        } else {
            bs->words[word] |= ((((uint64_t)1 << rem) - 1) << bit);
            i += rem;
        }
    }
    pthread_mutex_unlock(&bs->mutex);
    return true;
}

void bitset_unset_range(bitset_t *bs, size_t start, size_t end) {
    if (!bs || start >= end) return;
    pthread_mutex_lock(&bs->mutex);
    for (size_t i = start; i < end && i / BITSET_WORD_BITS < bs->word_count; ) {
        size_t word  = i / BITSET_WORD_BITS;
        size_t bit   = i % BITSET_WORD_BITS;
        size_t avail = BITSET_WORD_BITS - bit;
        size_t rem   = end - i;
        if (rem >= avail) {
            bs->words[word] &= ~(~(uint64_t)0 << bit);
            i += avail;
        } else {
            bs->words[word] &= ~((((uint64_t)1 << rem) - 1) << bit);
            i += rem;
        }
    }
    pthread_mutex_unlock(&bs->mutex);
}

size_t bitset_popcount(const bitset_t *bs) {
    if (!bs) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    size_t count = 0;
    for (size_t w = 0; w < bs->word_count; w++)
        count += (size_t)__builtin_popcountll(bs->words[w]);
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return count;
}

bool bitset_any(const bitset_t *bs) {
    if (!bs) return false;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    bool found = false;
    for (size_t w = 0; w < bs->word_count && !found; w++)
        found = bs->words[w] != 0;
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return found;
}

bool bitset_none(const bitset_t *bs) {
    return !bitset_any(bs);
}

size_t bitset_next_set(const bitset_t *bs, size_t from) {
    if (!bs) return BITSET_NPOS;
    pthread_mutex_lock((pthread_mutex_t *)&bs->mutex);
    size_t result = BITSET_NPOS;
    size_t word   = from / BITSET_WORD_BITS;
    size_t bit    = from % BITSET_WORD_BITS;
    while (word < bs->word_count) {
        uint64_t w = bs->words[word] >> bit;
        if (w) {
            result = word * BITSET_WORD_BITS + bit
                   + (size_t)__builtin_ctzll(w);
            break;
        }
        word++;
        bit = 0;
    }
    pthread_mutex_unlock((pthread_mutex_t *)&bs->mutex);
    return result;
}

/* --- Bitwise operations --- */

bool bitset_and(bitset_t *dst, const bitset_t *src) {
    if (!dst || !src) return false;
    /* Lock both; use pointer order to avoid deadlock */
    pthread_mutex_t *first  = dst < src ? &dst->mutex : (pthread_mutex_t *)&src->mutex;
    pthread_mutex_t *second = dst < src ? (pthread_mutex_t *)&src->mutex : &dst->mutex;
    pthread_mutex_lock(first);
    pthread_mutex_lock(second);

    size_t min_words = dst->word_count < src->word_count
                     ? dst->word_count : src->word_count;
    for (size_t w = 0; w < min_words; w++)
        dst->words[w] &= src->words[w];
    /* Bits beyond src capacity are defined as 0 by AND */
    for (size_t w = min_words; w < dst->word_count; w++)
        dst->words[w] = 0;

    pthread_mutex_unlock(second);
    pthread_mutex_unlock(first);
    return true;
}

bool bitset_or(bitset_t *dst, const bitset_t *src) {
    if (!dst || !src) return false;
    pthread_mutex_t *first  = dst < src ? &dst->mutex : (pthread_mutex_t *)&src->mutex;
    pthread_mutex_t *second = dst < src ? (pthread_mutex_t *)&src->mutex : &dst->mutex;
    pthread_mutex_lock(first);
    pthread_mutex_lock(second);

    bool ok = bitset_grow_to(dst, src->word_count);
    if (ok) {
        for (size_t w = 0; w < src->word_count; w++)
            dst->words[w] |= src->words[w];
    }

    pthread_mutex_unlock(second);
    pthread_mutex_unlock(first);
    return ok;
}

bool bitset_xor(bitset_t *dst, const bitset_t *src) {
    if (!dst || !src) return false;
    pthread_mutex_t *first  = dst < src ? &dst->mutex : (pthread_mutex_t *)&src->mutex;
    pthread_mutex_t *second = dst < src ? (pthread_mutex_t *)&src->mutex : &dst->mutex;
    pthread_mutex_lock(first);
    pthread_mutex_lock(second);

    bool ok = bitset_grow_to(dst, src->word_count);
    if (ok) {
        for (size_t w = 0; w < src->word_count; w++)
            dst->words[w] ^= src->words[w];
    }

    pthread_mutex_unlock(second);
    pthread_mutex_unlock(first);
    return ok;
}

void bitset_not(bitset_t *bs) {
    if (!bs) return;
    pthread_mutex_lock(&bs->mutex);
    for (size_t w = 0; w < bs->word_count; w++)
        bs->words[w] = ~bs->words[w];
    pthread_mutex_unlock(&bs->mutex);
}
