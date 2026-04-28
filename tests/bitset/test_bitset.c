// test_bitset.c — tests for the dynamic bitset_t container

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

#include "bitset.h"

/* -------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------- */

void test_create_and_destroy(void) {
    bitset_t *bs = bitset_create(0);
    ASSERT_TRUE(bs);
    bitset_destroy(bs);

    bitset_t *bs2 = bitset_create(200);
    ASSERT_TRUE(bs2);
    ASSERT_TRUE(bitset_capacity(bs2) >= 200);
    bitset_destroy(bs2);

    bitset_destroy(NULL); /* must not crash */
}

void test_set_and_test(void) {
    bitset_t *bs = bitset_create(0);
    ASSERT_TRUE(bs);

    /* Out-of-range test returns false */
    ASSERT_TRUE(!bitset_test(bs, 0));
    ASSERT_TRUE(!bitset_test(bs, 127));

    ASSERT_TRUE(bitset_set(bs, 0));
    ASSERT_TRUE(bitset_set(bs, 63));
    ASSERT_TRUE(bitset_set(bs, 64));
    ASSERT_TRUE(bitset_set(bs, 127));
    ASSERT_TRUE(bitset_set(bs, 1000));

    ASSERT_TRUE(bitset_test(bs, 0));
    ASSERT_TRUE(bitset_test(bs, 63));
    ASSERT_TRUE(bitset_test(bs, 64));
    ASSERT_TRUE(bitset_test(bs, 127));
    ASSERT_TRUE(bitset_test(bs, 1000));

    ASSERT_TRUE(!bitset_test(bs, 1));
    ASSERT_TRUE(!bitset_test(bs, 999));
    ASSERT_TRUE(!bitset_test(bs, 1001));

    bitset_destroy(bs);
}

void test_unset(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    bitset_set(bs, 5);
    bitset_set(bs, 10);
    ASSERT_TRUE(bitset_test(bs, 5));
    ASSERT_TRUE(bitset_test(bs, 10));

    bitset_unset(bs, 5);
    ASSERT_TRUE(!bitset_test(bs, 5));
    ASSERT_TRUE(bitset_test(bs, 10));

    /* Unset out of range: no-op, no crash */
    bitset_unset(bs, 9999);

    bitset_destroy(bs);
}

void test_toggle(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    ASSERT_TRUE(bitset_toggle(bs, 7));
    ASSERT_TRUE(bitset_test(bs, 7));

    ASSERT_TRUE(bitset_toggle(bs, 7));
    ASSERT_TRUE(!bitset_test(bs, 7));

    bitset_destroy(bs);
}

void test_clear(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    bitset_set(bs, 0);
    bitset_set(bs, 31);
    bitset_set(bs, 63);

    bitset_clear(bs);

    ASSERT_TRUE(!bitset_test(bs, 0));
    ASSERT_TRUE(!bitset_test(bs, 31));
    ASSERT_TRUE(!bitset_test(bs, 63));

    bitset_destroy(bs);
}

void test_set_range(void) {
    bitset_t *bs = bitset_create(128);
    ASSERT_TRUE(bs);

    ASSERT_TRUE(bitset_set_range(bs, 10, 20)); /* [10, 20) */

    for (size_t i = 10; i < 20; i++)
        ASSERT_TRUE(bitset_test(bs, i));

    ASSERT_TRUE(!bitset_test(bs, 9));
    ASSERT_TRUE(!bitset_test(bs, 20));

    /* Range spanning a word boundary */
    ASSERT_TRUE(bitset_set_range(bs, 60, 70));
    for (size_t i = 60; i < 70; i++)
        ASSERT_TRUE(bitset_test(bs, i));

    /* Invalid range */
    ASSERT_TRUE(!bitset_set_range(bs, 50, 50));

    bitset_destroy(bs);
}

void test_unset_range(void) {
    bitset_t *bs = bitset_create(128);
    ASSERT_TRUE(bs);

    bitset_set_range(bs, 0, 64);
    bitset_unset_range(bs, 16, 32);

    for (size_t i = 0; i < 16; i++)
        ASSERT_TRUE(bitset_test(bs, i));
    for (size_t i = 16; i < 32; i++)
        ASSERT_TRUE(!bitset_test(bs, i));
    for (size_t i = 32; i < 64; i++)
        ASSERT_TRUE(bitset_test(bs, i));

    bitset_destroy(bs);
}

void test_popcount(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    ASSERT_EQ_INT((int)bitset_popcount(bs), 0);

    bitset_set(bs, 0);
    bitset_set(bs, 7);
    bitset_set(bs, 63);
    ASSERT_EQ_INT((int)bitset_popcount(bs), 3);

    bitset_set_range(bs, 10, 20);
    ASSERT_EQ_INT((int)bitset_popcount(bs), 13);

    bitset_destroy(bs);
}

void test_any_none(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    ASSERT_TRUE(bitset_none(bs));
    ASSERT_TRUE(!bitset_any(bs));

    bitset_set(bs, 42);

    ASSERT_TRUE(!bitset_none(bs));
    ASSERT_TRUE(bitset_any(bs));

    bitset_destroy(bs);
}

void test_next_set(void) {
    bitset_t *bs = bitset_create(128);
    ASSERT_TRUE(bs);

    /* Empty bitset */
    ASSERT_EQ_INT((int)bitset_next_set(bs, 0), (int)BITSET_NPOS);

    bitset_set(bs, 5);
    bitset_set(bs, 10);
    bitset_set(bs, 70);

    ASSERT_EQ_INT((int)bitset_next_set(bs, 0),  5);
    ASSERT_EQ_INT((int)bitset_next_set(bs, 6),  10);
    ASSERT_EQ_INT((int)bitset_next_set(bs, 11), 70);
    ASSERT_EQ_INT((int)bitset_next_set(bs, 71), (int)BITSET_NPOS);

    bitset_destroy(bs);
}

void test_clone(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    bitset_set(bs, 3);
    bitset_set(bs, 60);

    bitset_t *copy = bitset_clone(bs);
    ASSERT_TRUE(copy);

    ASSERT_TRUE(bitset_test(copy, 3));
    ASSERT_TRUE(bitset_test(copy, 60));
    ASSERT_TRUE(!bitset_test(copy, 4));

    /* Modifications to copy don't affect original */
    bitset_set(copy, 4);
    ASSERT_TRUE(!bitset_test(bs, 4));

    bitset_destroy(bs);
    bitset_destroy(copy);
}

void test_bitwise_and(void) {
    bitset_t *a = bitset_create(64);
    bitset_t *b = bitset_create(64);
    ASSERT_TRUE(a && b);

    bitset_set(a, 1);
    bitset_set(a, 2);
    bitset_set(a, 3);

    bitset_set(b, 2);
    bitset_set(b, 3);
    bitset_set(b, 4);

    ASSERT_TRUE(bitset_and(a, b));

    ASSERT_TRUE(!bitset_test(a, 1));
    ASSERT_TRUE(bitset_test(a, 2));
    ASSERT_TRUE(bitset_test(a, 3));
    ASSERT_TRUE(!bitset_test(a, 4));

    bitset_destroy(a);
    bitset_destroy(b);
}

void test_bitwise_or(void) {
    bitset_t *a = bitset_create(64);
    bitset_t *b = bitset_create(64);
    ASSERT_TRUE(a && b);

    bitset_set(a, 1);
    bitset_set(b, 2);

    ASSERT_TRUE(bitset_or(a, b));

    ASSERT_TRUE(bitset_test(a, 1));
    ASSERT_TRUE(bitset_test(a, 2));
    ASSERT_TRUE(!bitset_test(a, 3));

    bitset_destroy(a);
    bitset_destroy(b);
}

void test_bitwise_xor(void) {
    bitset_t *a = bitset_create(64);
    bitset_t *b = bitset_create(64);
    ASSERT_TRUE(a && b);

    bitset_set(a, 1);
    bitset_set(a, 2);
    bitset_set(b, 2);
    bitset_set(b, 3);

    ASSERT_TRUE(bitset_xor(a, b));

    ASSERT_TRUE(bitset_test(a, 1));
    ASSERT_TRUE(!bitset_test(a, 2)); /* cancelled */
    ASSERT_TRUE(bitset_test(a, 3));

    bitset_destroy(a);
    bitset_destroy(b);
}

void test_bitwise_not(void) {
    bitset_t *bs = bitset_create(64);
    ASSERT_TRUE(bs);

    bitset_set(bs, 0);
    bitset_set(bs, 63);

    bitset_not(bs);

    ASSERT_TRUE(!bitset_test(bs, 0));
    ASSERT_TRUE(!bitset_test(bs, 63));
    ASSERT_TRUE(bitset_test(bs, 1));
    ASSERT_TRUE(bitset_test(bs, 62));

    bitset_destroy(bs);
}

void test_growth(void) {
    bitset_t *bs = bitset_create(0);
    ASSERT_TRUE(bs);

    /* Set a bit far beyond the initial capacity */
    ASSERT_TRUE(bitset_set(bs, 10000));
    ASSERT_TRUE(bitset_test(bs, 10000));
    ASSERT_TRUE(!bitset_test(bs, 9999));
    ASSERT_TRUE(bitset_capacity(bs) >= 10001);

    bitset_destroy(bs);
}

/* -------------------------------------------------------------
 * Thread-safety test
 * ------------------------------------------------------------- */

#define THREAD_COUNT 8
#define BITS_PER_THREAD 128

struct bs_thread_args {
    bitset_t *bs;
    size_t base;
};

static void *thread_set_bits(void *arg) {
    struct bs_thread_args *a = arg;
    for (size_t i = 0; i < BITS_PER_THREAD; i++)
        bitset_set(a->bs, a->base + i);
    return NULL;
}

void test_thread_safety(void) {
    bitset_t *bs = bitset_create(0);
    ASSERT_TRUE(bs);

    pthread_t threads[THREAD_COUNT];
    struct bs_thread_args args[THREAD_COUNT];

    for (int t = 0; t < THREAD_COUNT; t++) {
        args[t].bs   = bs;
        args[t].base = (size_t)t * BITS_PER_THREAD;
        pthread_create(&threads[t], NULL, thread_set_bits, &args[t]);
    }
    for (int t = 0; t < THREAD_COUNT; t++)
        pthread_join(threads[t], NULL);

    ASSERT_EQ_INT((int)bitset_popcount(bs), THREAD_COUNT * BITS_PER_THREAD);

    for (int t = 0; t < THREAD_COUNT; t++)
        for (size_t i = 0; i < BITS_PER_THREAD; i++)
            ASSERT_TRUE(bitset_test(bs, (size_t)t * BITS_PER_THREAD + i));

    bitset_destroy(bs);
}

/* -------------------------------------------------------------
 * README example functions
 * ------------------------------------------------------------- */

void example_bitset_basic(void) {
    bitset_t *bs = bitset_create(0);

    bitset_set(bs, 0);
    bitset_set(bs, 5);
    bitset_set(bs, 63);
    bitset_set(bs, 200);

    printf("popcount: %zu\n", bitset_popcount(bs));

    for (size_t i = bitset_next_set(bs, 0);
         i != BITSET_NPOS;
         i = bitset_next_set(bs, i + 1))
        printf("  bit %zu is set\n", i);

    bitset_destroy(bs);
}

void example_bitset_ops(void) {
    bitset_t *a = bitset_create(64);
    bitset_t *b = bitset_create(64);

    bitset_set_range(a, 0, 8);  /* 0–7 */
    bitset_set_range(b, 4, 12); /* 4–11 */

    bitset_and(a, b);

    printf("AND result:");
    for (size_t i = bitset_next_set(a, 0); i != BITSET_NPOS; i = bitset_next_set(a, i + 1))
        printf(" %zu", i);
    printf("\n");

    bitset_destroy(a);
    bitset_destroy(b);
}

int tests_main(void) {
    printf(C_BOLD C_CYAN "=== Lifecycle Tests ===\n" C_RESET);
    RUN_TEST(test_create_and_destroy, NULL);

    printf(C_BOLD C_CYAN "=== Single-Bit Tests ===\n" C_RESET);
    RUN_TEST(test_set_and_test, NULL);
    RUN_TEST(test_unset, NULL);
    RUN_TEST(test_toggle, NULL);
    RUN_TEST(test_clear, NULL);

    printf(C_BOLD C_CYAN "=== Range Tests ===\n" C_RESET);
    RUN_TEST(test_set_range, NULL);
    RUN_TEST(test_unset_range, NULL);

    printf(C_BOLD C_CYAN "=== Query Tests ===\n" C_RESET);
    RUN_TEST(test_popcount, NULL);
    RUN_TEST(test_any_none, NULL);
    RUN_TEST(test_next_set, NULL);

    printf(C_BOLD C_CYAN "=== Clone Test ===\n" C_RESET);
    RUN_TEST(test_clone, NULL);

    printf(C_BOLD C_CYAN "=== Bitwise Operation Tests ===\n" C_RESET);
    RUN_TEST(test_bitwise_and, NULL);
    RUN_TEST(test_bitwise_or, NULL);
    RUN_TEST(test_bitwise_xor, NULL);
    RUN_TEST(test_bitwise_not, NULL);

    printf(C_BOLD C_CYAN "=== Growth Test ===\n" C_RESET);
    RUN_TEST(test_growth, NULL);

    printf(C_BOLD C_CYAN "=== Thread Safety Test ===\n" C_RESET);
    RUN_TEST(test_thread_safety, NULL);

    printf(C_BOLD C_GREEN "\n=== README Output Examples ===\n" C_RESET);
    example_bitset_basic();
    example_bitset_ops();

    return tests_failed;
}
