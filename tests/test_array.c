// test_array.c — tests for the generic array_t container using the new test harness

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

#include "array.h"

/* -------------------------------------------------------------
 * strdup replacement for strict C99
 * ------------------------------------------------------------- */
static char *strclone(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* -------------------------------------------------------------
 * Comparison for int
 * ------------------------------------------------------------- */
static int int_cmp(const void *a, const void *b) {
    int x, y;
    memcpy(&x, a, sizeof(int));
    memcpy(&y, b, sizeof(int));
    return (x > y) - (x < y);
}

/* -------------------------------------------------------------
 * Comparison for char*
 * ------------------------------------------------------------- */
static int str_cmp(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

/* Clone/destroy for char* */
static void str_clone(void *dst, const void *src) {
    const char *s = *(const char * const *)src;
    char *copy = strclone(s);
    memcpy(dst, &copy, sizeof(char *));
}

static void str_destroy(void *elem) {
    char *s = *(char **)elem;
    free(s);
}

/* -------------------------------------------------------------
 * Deep struct test (README-style example)
 * ------------------------------------------------------------- */

struct deep {
    char *name;
    int value;
};

static void deep_clone(void *dst, const void *src) {
    const struct deep *s = src;
    struct deep *d = dst;
    d->value = s->value;
    d->name = malloc(strlen(s->name) + 1);
    strcpy(d->name, s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

/* -------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------- */

void test_ints(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);
    ASSERT_TRUE(arr);

    int vals[] = {5, 2, 9, 1};
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(array_add(arr, &vals[i]));

    ASSERT_EQ_INT(array_size(arr), 4);
    for (size_t i = 0; i < 4; ++i)
        ASSERT_EQ_INT(*(int*)array_get(arr, i), vals[i]);

    // Test append_carray
    int more[] = {7, 8};
    ASSERT_TRUE(array_append_carray(arr, more, 2));
    ASSERT_EQ_INT(array_size(arr), 6);
    ASSERT_EQ_INT(*(int*)array_get(arr, 4), 7);
    ASSERT_EQ_INT(*(int*)array_get(arr, 5), 8);

    // Test sort
    array_sort(arr, int_cmp);
    int sorted[] = {1, 2, 5, 7, 8, 9};
    for (size_t i = 0; i < 6; ++i)
        ASSERT_EQ_INT(*(int*)array_get(arr, i), sorted[i]);

    // Test append_array
    array_t *arr2 = array_create(sizeof(int), NULL, NULL);
    int extra[] = {42, 99};
    ASSERT_TRUE(array_append_carray(arr2, extra, 2));
    ASSERT_TRUE(array_append_array(arr, arr2));
    ASSERT_EQ_INT(array_size(arr), 8);
    ASSERT_EQ_INT(*(int*)array_get(arr, 6), 42);
    ASSERT_EQ_INT(*(int*)array_get(arr, 7), 99);

    // Test clear
    array_clear(arr);
    ASSERT_EQ_INT(array_size(arr), 0);

    array_destroy(arr2);
    array_destroy(arr);
}

void test_strings(void) {
    array_t *arr = array_create(sizeof(char *), str_clone, str_destroy);
    ASSERT_TRUE(arr);

    const char *a = "hello";
    const char *b = "world";
    const char *c = "foo";

    ASSERT_TRUE(array_add(arr, &a));
    ASSERT_TRUE(array_add(arr, &b));
    ASSERT_TRUE(array_add(arr, &c));

    ASSERT_EQ_INT(array_size(arr), 3);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 0), a) == 0);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 1), b) == 0);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 2), c) == 0);

    // Test sort
    array_sort(arr, str_cmp);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 0), "foo") == 0);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 1), "hello") == 0);
    ASSERT_TRUE(strcmp(*(char **)array_get(arr, 2), "world") == 0);

    array_clear(arr);
    ASSERT_EQ_INT(array_size(arr), 0);

    array_destroy(arr);
}

void test_append_array(void) {
    array_t *a1 = array_create(sizeof(int), NULL, NULL);
    array_t *a2 = array_create(sizeof(int), NULL, NULL);

    int v1[] = {1, 2, 3};
    int v2[] = {4, 5};

    ASSERT_TRUE(array_append_carray(a1, v1, 3));
    ASSERT_TRUE(array_append_carray(a2, v2, 2));
    ASSERT_TRUE(array_append_array(a1, a2));

    ASSERT_EQ_INT(array_size(a1), 5);
    for (int i = 0; i < 5; ++i)
        ASSERT_EQ_INT(*(int*)array_get(a1, i), i + 1);

    array_destroy(a1);
    array_destroy(a2);
}

void test_readme_examples(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; ++i)
        array_add(arr, &vals[i]);
    ASSERT_EQ_INT(array_size(arr), 3);
    for (size_t i = 0; i < 3; ++i)
        ASSERT_EQ_INT(*(int*)array_get(arr, i), vals[i]);
    array_destroy(arr);
}

void test_readme_deep_struct(void) {
    array_t *arr = array_create(sizeof(struct deep), deep_clone, deep_destroy);
    ASSERT_TRUE(arr);

    struct deep a = { strclone("alpha"), 1 };
    struct deep b = { strclone("beta"),  2 };
    struct deep c = { strclone("gamma"), 3 };

    ASSERT_TRUE(array_add(arr, &a));
    ASSERT_TRUE(array_add(arr, &b));
    ASSERT_TRUE(array_add(arr, &c));

    ASSERT_EQ_INT(array_size(arr), 3);

    struct deep *p = array_get(arr, 1);
    ASSERT_TRUE(strcmp(p->name, "beta") == 0);
    ASSERT_EQ_INT(p->value, 2);

    array_clear(arr);
    ASSERT_EQ_INT(array_size(arr), 0);

    // Clean up original structs
    free(a.name);
    free(b.name);
    free(c.name);

    array_destroy(arr);
}

void example_array_primitive(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    int vals[] = {5, 2, 9, 1};
    for (int i = 0; i < 4; ++i)
        array_add(arr, &vals[i]);

    array_sort(arr, int_cmp);

    for (size_t i = 0; i < array_size(arr); ++i)
        printf("%d ", *(int*)array_get(arr, i));
    printf("\n");

    array_destroy(arr);
}

void example_array_deep_struct(void) {
    array_t *arr = array_create(sizeof(struct deep), deep_clone, deep_destroy);

    struct deep a = { strclone("alpha"), 1 };
    struct deep b = { strclone("beta"),  2 };
    struct deep c = { strclone("gamma"), 3 };

    array_add(arr, &a);
    array_add(arr, &b);
    array_add(arr, &c);

    for (size_t i = 0; i < array_size(arr); ++i) {
        struct deep *p = array_get(arr, i);
        printf("%s: %d\n", p->name, p->value);
    }

    // Clean up original structs
    free(a.name);
    free(b.name);
    free(c.name);

    array_destroy(arr);
}

#define THREAD_COUNT 8
#define PER_THREAD 1000

struct thread_args {
    array_t *arr;
    int base;
};

static void *thread_append(void *arg) {
    struct thread_args *args = arg;
    for (int i = 0; i < PER_THREAD; ++i) {
        int val = args->base + i;
        array_add(args->arr, &val);
    }
    return NULL;
}

void test_thread_safety_append(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);

    pthread_t threads[THREAD_COUNT];
    struct thread_args args[THREAD_COUNT];

    for (int t = 0; t < THREAD_COUNT; ++t) {
        args[t].arr = arr;
        args[t].base = t * PER_THREAD;
        pthread_create(&threads[t], NULL, thread_append, &args[t]);
    }
    for (int t = 0; t < THREAD_COUNT; ++t)
        pthread_join(threads[t], NULL);

    // Check total size
    ASSERT_EQ_INT(array_size(arr), THREAD_COUNT * PER_THREAD);

    // Check for all expected values (order not guaranteed)
    int *seen = calloc(THREAD_COUNT * PER_THREAD, sizeof(int));
    for (size_t i = 0; i < array_size(arr); ++i) {
        int v = *(int*)array_get(arr, i);
        ASSERT_TRUE(v >= 0 && v < THREAD_COUNT * PER_THREAD);
        seen[v]++;
    }
    for (int i = 0; i < THREAD_COUNT * PER_THREAD; ++i)
        ASSERT_EQ_INT(seen[i], 1);

    free(seen);
    array_destroy(arr);
}

void test_remove_and_remove_elements(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);
    int vals[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; ++i)
        array_add(arr, &vals[i]);

    // Remove middle element (30)
    ASSERT_TRUE(array_remove(arr, 2));
    ASSERT_EQ_INT(array_size(arr), 4);
    ASSERT_EQ_INT(*(int*)array_get(arr, 0), 10);
    ASSERT_EQ_INT(*(int*)array_get(arr, 1), 20);
    ASSERT_EQ_INT(*(int*)array_get(arr, 2), 40);
    ASSERT_EQ_INT(*(int*)array_get(arr, 3), 50);

    // Remove first two elements (10, 20)
    ASSERT_TRUE(array_remove_elements(arr, 0, 2));
    ASSERT_EQ_INT(array_size(arr), 2);
    ASSERT_EQ_INT(*(int*)array_get(arr, 0), 40);
    ASSERT_EQ_INT(*(int*)array_get(arr, 1), 50);

    // Remove last element (50)
    ASSERT_TRUE(array_remove(arr, 1));
    ASSERT_EQ_INT(array_size(arr), 1);
    ASSERT_EQ_INT(*(int*)array_get(arr, 0), 40);

    // Remove out of bounds
    ASSERT_TRUE(!array_remove(arr, 5));
    ASSERT_TRUE(!array_remove_elements(arr, 1, 2));

    array_destroy(arr);
}

void test_insert_and_insert_carray(void) {
    array_t *arr = array_create(sizeof(int), NULL, NULL);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; ++i)
        array_add(arr, &vals[i]);

    // Insert at beginning
    int v0 = 0;
    ASSERT_TRUE(array_insert(arr, 0, &v0));
    ASSERT_EQ_INT(array_size(arr), 4);
    ASSERT_EQ_INT(*(int*)array_get(arr, 0), 0);

    // Insert at end
    int v4 = 4;
    ASSERT_TRUE(array_insert(arr, 4, &v4));
    ASSERT_EQ_INT(array_size(arr), 5);
    ASSERT_EQ_INT(*(int*)array_get(arr, 4), 4);

    // Insert in the middle
    int v99 = 99;
    ASSERT_TRUE(array_insert(arr, 2, &v99));
    ASSERT_EQ_INT(array_size(arr), 6);
    ASSERT_EQ_INT(*(int*)array_get(arr, 2), 99);

    // Insert C array in the middle
    int mid[] = {7, 8};
    ASSERT_TRUE(array_insert_carray(arr, 3, mid, 2));
    ASSERT_EQ_INT(array_size(arr), 8);
    ASSERT_EQ_INT(*(int*)array_get(arr, 3), 7);
    ASSERT_EQ_INT(*(int*)array_get(arr, 4), 8);

    array_destroy(arr);
}

void test_insert_array(void) {
    array_t *a1 = array_create(sizeof(int), NULL, NULL);
    array_t *a2 = array_create(sizeof(int), NULL, NULL);

    int v1[] = {1, 2, 3, 4};
    int v2[] = {100, 200};

    ASSERT_TRUE(array_append_carray(a1, v1, 4));
    ASSERT_TRUE(array_append_carray(a2, v2, 2));

    // Insert a2 into a1 at position 2
    ASSERT_TRUE(array_insert_array(a1, 2, a2));
    ASSERT_EQ_INT(array_size(a1), 6);
    ASSERT_EQ_INT(*(int*)array_get(a1, 0), 1);
    ASSERT_EQ_INT(*(int*)array_get(a1, 1), 2);
    ASSERT_EQ_INT(*(int*)array_get(a1, 2), 100);
    ASSERT_EQ_INT(*(int*)array_get(a1, 3), 200);
    ASSERT_EQ_INT(*(int*)array_get(a1, 4), 3);
    ASSERT_EQ_INT(*(int*)array_get(a1, 5), 4);

    array_destroy(a1);
    array_destroy(a2);
}

/* -------------------------------------------------------------
 * tests_main() — the harness entry point
 * ------------------------------------------------------------- */

int tests_main(void) {
    printf(C_BOLD C_CYAN "=== Integer Array Tests ===\n" C_RESET);
    RUN_TEST(test_ints, NULL);

    printf(C_BOLD C_CYAN "=== String Array Tests ===\n" C_RESET);
    RUN_TEST(test_strings, NULL);

    printf(C_BOLD C_CYAN "=== Append Array Tests ===\n" C_RESET);
    RUN_TEST(test_append_array, NULL);

    printf(C_BOLD C_CYAN "\n=== Thread Safety Tests ===\n" C_RESET);
    RUN_TEST(test_thread_safety_append, NULL);

    printf(C_BOLD C_CYAN "=== Remove/Insert Tests ===\n" C_RESET);
    RUN_TEST(test_remove_and_remove_elements, NULL);
    RUN_TEST(test_insert_and_insert_carray, NULL);
    RUN_TEST(test_insert_array, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST(test_readme_examples, NULL);
    RUN_TEST(test_readme_deep_struct, NULL);

    printf(C_BOLD C_GREEN "\n=== README Output Examples ===\n" C_RESET);
    example_array_primitive();
    example_array_deep_struct();

    return tests_failed;
}

