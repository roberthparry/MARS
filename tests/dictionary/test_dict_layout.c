#include <stdint.h>
#include <string.h>

#include "dictionary.h"
#include "test_harness.h"
#include "test_dict_layout.h"

static size_t long_double_hash(const void *key)
{
    return (size_t)*(const long double *)key;
}

static int long_double_cmp(const void *a, const void *b)
{
    long double left = *(const long double *)a, right = *(const long double *)b;
    return (left > right) - (left < right);
}

static size_t bytes_hash(const void *key)
{
    return *(const unsigned char *)key;
}

static int bytes_cmp(const void *a, const void *b)
{
    return memcmp(a, b, 3u);
}

/* Exercise padding, growth, rehashing and removal with maximally aligned and odd-sized elements. */
void test_dictionary_slot_alignment(void)
{
    dictionary_t *dict = dictionary_create(sizeof(long double), sizeof(long double), long_double_hash,
                                           long_double_cmp, NULL, NULL, long_double_cmp, NULL, NULL);
    ASSERT_NOT_NULL(dict);
    for (size_t i = 0u; i < 40u; ++i) {
        long double key = (long double)i, value = key + 0.5L;
        ASSERT_TRUE(dictionary_set(dict, &key, &value));
    }
    for (size_t i = 0u; i < 40u; i += 2u) {
        long double key = (long double)i;
        ASSERT_TRUE(dictionary_remove(dict, &key));
    }
    for (size_t i = 1u; i < 40u; i += 2u) {
        long double key = (long double)i;
        dictionary_entry_t *entry = NULL;
        ASSERT_TRUE(dictionary_get_entry(dict, &key, &entry));
        const void *stored_key = dictionary_entry_key(entry), *stored_value = dictionary_entry_value(entry);
        ASSERT_TRUE((uintptr_t)stored_key % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE((uintptr_t)stored_value % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE(*(const long double *)stored_key == key);
        ASSERT_TRUE(*(const long double *)stored_value == key + 0.5L);
    }
    dictionary_destroy(dict);

    dict = dictionary_create(3u, 3u, bytes_hash, bytes_cmp, NULL, NULL, bytes_cmp, NULL, NULL);
    ASSERT_NOT_NULL(dict);
    for (unsigned char i = 0u; i < 40u; ++i) {
        unsigned char key[3] = {i, 1u, 2u}, value[3] = {i, 3u, 4u};
        ASSERT_TRUE(dictionary_set(dict, key, value));
        dictionary_entry_t *entry = NULL;
        ASSERT_TRUE(dictionary_get_entry(dict, key, &entry));
        ASSERT_TRUE((uintptr_t)dictionary_entry_key(entry) % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE((uintptr_t)dictionary_entry_value(entry) % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE(memcmp(dictionary_entry_value(entry), value, sizeof(value)) == 0);
    }
    dictionary_destroy(dict);
}

/* Reject sizes whose header or alignment padding would wrap around. */
void test_dictionary_size_overflow(void)
{
    ASSERT_TRUE(!dictionary_create(SIZE_MAX, 1u, bytes_hash, bytes_cmp, NULL, NULL, NULL, NULL, NULL));
    ASSERT_TRUE(!dictionary_create(1u, SIZE_MAX, bytes_hash, bytes_cmp, NULL, NULL, NULL, NULL, NULL));
    ASSERT_TRUE(!dictionary_create(SIZE_MAX - sizeof(size_t), 1u, bytes_hash, bytes_cmp,
                                  NULL, NULL, NULL, NULL, NULL));
}
