// test_set.c — tests for the generic value-set container using the new test harness

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_harness.h"

#include "set.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* -------------------------------------------------------------
 * strdup replacement for strict C99
 * ------------------------------------------------------------- */

static char *strclone(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

/* -------------------------------------------------------------
 * Hash and compare for int
 * ------------------------------------------------------------- */

static size_t int_hash(const void *p)
{
    int v;
    memcpy(&v, p, sizeof(int));
    return (size_t)v * 2654435761u;
}

static int int_cmp(const void *a, const void *b)
{
    int x, y;
    memcpy(&x, a, sizeof(int));
    memcpy(&y, b, sizeof(int));
    return (x > y) - (x < y);
}

/* -------------------------------------------------------------
 * Hash and compare for char*
 * ------------------------------------------------------------- */

static size_t str_hash(const void *p)
{
    const char *s = *(const char *const *)p;
    size_t h = 146527;
    for (size_t i = 0u; s[i] != '\0'; ++i)
        h = (h * 33) ^ (unsigned char)s[i];
    return h;
}

static int str_cmp(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

/* Clone/destroy for char* */
static void str_clone(void *dst, const void *src)
{
    const char *s = *(const char *const *)src;
    char *copy = strclone(s);
    memcpy(dst, &copy, sizeof(char *));
}

static void str_destroy(void *elem)
{
    char *s = *(char **)elem;
    free(s);
}

/* -------------------------------------------------------------
 * Deep struct
 * ------------------------------------------------------------- */

struct deep {
    char *name;
    int value;
};

static size_t deep_hash(const void *p)
{
    const struct deep *d = p;
    size_t h = 146527;
    const char *s = d->name;
    for (size_t i = 0u; s[i] != '\0'; ++i)
        h = (h * 33) ^ (unsigned char)s[i];
    return h ^ (size_t)d->value;
}

static int deep_cmp(const void *a, const void *b)
{
    const struct deep *da = a;
    const struct deep *db = b;
    int c = strcmp(da->name, db->name);
    if (c != 0)
        return c;
    return (da->value > db->value) - (da->value < db->value);
}

static void deep_clone(void *dst, const void *src)
{
    const struct deep *s = src;
    struct deep *d = dst;
    d->value = s->value;
    d->name = strclone(s->name);
}

static void deep_destroy(void *elem)
{
    struct deep *d = elem;
    free(d->name);
}

/* -------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------- */

static size_t aligned_hash(const void *value)
{
    return (size_t)*(const long double *)value;
}

static int aligned_cmp(const void *a, const void *b)
{
    long double left = *(const long double *)a, right = *(const long double *)b;
    return (left > right) - (left < right);
}

static size_t bytes_hash(const void *value)
{
    return *(const unsigned char *)value;
}

static int bytes_cmp(const void *a, const void *b)
{
    return memcmp(a, b, 3u);
}

static void test_slot_alignment(void)
{
    set_t *set = set_create(sizeof(long double), aligned_hash, aligned_cmp, NULL, NULL);
    ASSERT_NOT_NULL(set);
    for (size_t i = 0u; i < 40u; ++i) {
        long double value = (long double)i;
        ASSERT_TRUE(set_add(set, &value));
    }
    for (size_t i = 0u; i < 40u; i += 2u) {
        long double value = (long double)i;
        ASSERT_TRUE(set_remove(set, &value));
    }
    set_t *copy = set_clone(set);
    ASSERT_NOT_NULL(copy);
    for (size_t i = 0u; i < set_get_size(copy); ++i) {
        const void *value = set_get_sorted(copy, i);
        ASSERT_TRUE((uintptr_t)value % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE(*(const long double *)value == (long double)(2u * i + 1u));
    }
    set_destroy(copy);
    set_destroy(set);
    set = set_create(3u, bytes_hash, bytes_cmp, NULL, NULL);
    ASSERT_NOT_NULL(set);
    for (unsigned char i = 0u; i < 40u; ++i) {
        unsigned char value[3] = {i, 1u, 2u};
        ASSERT_TRUE(set_add(set, value));
        ASSERT_TRUE((uintptr_t)set_get(set, i) % _Alignof(max_align_t) == 0u);
        ASSERT_TRUE(memcmp(set_get(set, i), value, sizeof(value)) == 0);
    }
    set_destroy(set);
}

static void test_size_overflow(void)
{
    ASSERT_TRUE(!set_create(SIZE_MAX, int_hash, int_cmp, NULL, NULL));
    ASSERT_TRUE(!set_create(SIZE_MAX - sizeof(size_t), int_hash, int_cmp, NULL, NULL));
}

void test_ints(void)
{
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    int a = 5, b = 10, c = 5;

    set_add(s, &a);
    set_add(s, &b);

    ASSERT_TRUE(set_contains(s, &a));
    ASSERT_TRUE(set_contains(s, &b));
    ASSERT_TRUE(!set_add(s, &c)); // duplicate should fail
    ASSERT_TRUE(set_remove(s, &a));
    ASSERT_TRUE(!set_contains(s, &a));

    set_destroy(s);
}

void test_strings(void)
{
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, str_clone, str_destroy);

    const char *a = "hello";
    const char *b = "world";
    const char *c = "hello";

    set_add(s, &a);
    set_add(s, &b);

    ASSERT_TRUE(set_contains(s, &a));
    ASSERT_TRUE(!set_add(s, &c)); // duplicate
    ASSERT_TRUE(set_remove(s, &a));
    ASSERT_TRUE(!set_contains(s, &a));

    set_destroy(s);
}

void test_deep(void)
{
    set_t *s = set_create(sizeof(struct deep), deep_hash, deep_cmp, deep_clone, deep_destroy);

    struct deep a = {strclone("alpha"), 1};
    struct deep b = {strclone("beta"), 2};
    struct deep c = {strclone("alpha"), 1};

    set_add(s, &a);
    set_add(s, &b);

    ASSERT_TRUE(set_contains(s, &a));
    ASSERT_TRUE(!set_add(s, &c)); // duplicate
    ASSERT_TRUE(set_remove(s, &a));
    ASSERT_TRUE(!set_contains(s, &a));

    free(a.name);
    free(b.name);
    free(c.name);

    set_destroy(s);
}

void test_sorted(void)
{
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    int vals[] = {5, 1, 3, 4, 2};
    for (int i = 0; i < 5; ++i)
        set_add(s, &vals[i]);

    for (size_t i = 0; i < 5; ++i) {
        const int *p = set_get_sorted(s, i);
        ASSERT_EQ_INT(*p, (int)(i + 1));
    }

    set_destroy(s);
}

void test_fuzz(void)
{
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    srand((unsigned)time(NULL));

    for (int i = 0; i < 5000; ++i) {
        int v = rand() % 2000;
        set_add(s, &v);
    }

    for (int i = 0; i < 2000; ++i) {
        int v = i;
        if (set_contains(s, &v)) {
            ASSERT_TRUE(set_remove(s, &v));
        }
    }

    set_destroy(s);
}

static void example_set_strings(void)
{
    /* Create a set of strings with deep‑copy semantics */
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, str_clone, str_destroy);

    const char *a = "hello";
    const char *b = "world";
    const char *c = "hello"; /* duplicate */

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
}

static void example_set_deep_strings(void)
{
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, str_clone, str_destroy);

    const char *a = "hello";
    const char *b = "world";

    set_add(s, &a);
    set_add(s, &b);

    for (size_t i = 0; i < set_get_size(s); i++) {
        const char *elem = *(const char **)set_get_sorted(s, i);
        printf("[%zu] %s\n", i, elem);
    }

    set_destroy(s);
}

/* -------------------------------------------------------------
 * tests_main() — the harness entry point
 * ------------------------------------------------------------- */

int tests_main(void)
{

    TEST_SECTION("Integer Tests");
    TEST_RUN_IN_GROUP(test_ints, tests, NULL);
    TEST_RUN_IN_GROUP(test_slot_alignment, tests, "set,alignment");
    TEST_RUN_IN_GROUP(test_size_overflow, tests, "set,bounds");

    TEST_SECTION("String Tests");
    TEST_RUN_IN_GROUP(test_strings, tests, NULL);

    TEST_SECTION("Deep Struct Tests");
    TEST_RUN_IN_GROUP(test_deep, tests, NULL);

    TEST_SECTION("Sorted Order Tests");
    TEST_RUN_IN_GROUP(test_sorted, tests, NULL);

    TEST_SECTION("Fuzz Tests");
    TEST_RUN_IN_GROUP(test_fuzz, tests, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_set_strings, readme_examples, "set,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_set_deep_strings, readme_examples, "set,readme,output");

    return TESTS_EXIT_CODE();
}
