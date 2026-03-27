/* test_set.c - tests for the generic value-set container */

#include "set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

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
 * Colour helpers
 * ------------------------------------------------------------- */

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"

static void pass(const char *msg) {
    printf(GREEN "PASS" RESET " %s\n", msg);
}

static void fail(const char *msg) {
    printf(RED "FAIL" RESET " %s\n", msg);
}

/* -------------------------------------------------------------
 * Hash and compare for int
 * ------------------------------------------------------------- */

static size_t int_hash(const void *p) {
    int v;
    memcpy(&v, p, sizeof(int));
    return (size_t)v * 2654435761u;
}

static int int_cmp(const void *a, const void *b) {
    int x, y;
    memcpy(&x, a, sizeof(int));
    memcpy(&y, b, sizeof(int));
    return (x > y) - (x < y);
}

/* -------------------------------------------------------------
 * Hash and compare for char*
 * ------------------------------------------------------------- */

static size_t str_hash(const void *p) {
    const char *s = *(const char * const *)p;
    size_t h = 146527;
    while (*s) {
        h = (h * 33) ^ (unsigned char)*s++;
    }
    return h;
}

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
 * Deep struct
 * ------------------------------------------------------------- */

struct deep {
    char *name;
    int value;
};

static size_t deep_hash(const void *p) {
    const struct deep *d = p;
    size_t h = 146527;
    const char *s = d->name;
    while (*s) {
        h = (h * 33) ^ (unsigned char)*s++;
    }
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
    d->name = strclone(s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

/* -------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------- */

static void test_ints() {
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    int a = 5, b = 10, c = 5;

    set_add(s, &a);
    set_add(s, &b);

    if (!set_contains(s, &a)) fail("int contains a");
    else pass("int contains a");

    if (!set_contains(s, &b)) fail("int contains b");
    else pass("int contains b");

    if (set_add(s, &c)) fail("duplicate int add");
    else pass("duplicate int add");

    if (!set_remove(s, &a)) fail("remove int a");
    else pass("remove int a");

    if (set_contains(s, &a)) fail("int a removed");
    else pass("int a removed");

    set_destroy(s);
}

static void test_strings() {
    set_t *s = set_create(sizeof(char *), str_hash, str_cmp, str_clone, str_destroy);

    const char *a = "hello";
    const char *b = "world";
    const char *c = "hello";

    set_add(s, &a);
    set_add(s, &b);

    if (!set_contains(s, &a)) fail("string contains a");
    else pass("string contains a");

    if (set_add(s, &c)) fail("duplicate string add");
    else pass("duplicate string add");

    if (!set_remove(s, &a)) fail("remove string a");
    else pass("remove string a");

    if (set_contains(s, &a)) fail("string a removed");
    else pass("string a removed");

    set_destroy(s);
}

static void test_deep() {
    set_t *s = set_create(sizeof(struct deep), deep_hash, deep_cmp, deep_clone, deep_destroy);

    struct deep a = { strclone("alpha"), 1 };
    struct deep b = { strclone("beta"), 2 };
    struct deep c = { strclone("alpha"), 1 };

    set_add(s, &a);
    set_add(s, &b);

    if (!set_contains(s, &a)) fail("deep contains a");
    else pass("deep contains a");

    if (set_add(s, &c)) fail("duplicate deep add");
    else pass("duplicate deep add");

    if (!set_remove(s, &a)) fail("remove deep a");
    else pass("remove deep a");

    if (set_contains(s, &a)) fail("deep a removed");
    else pass("deep a removed");

    free(a.name);
    free(b.name);
    free(c.name);

    set_destroy(s);
}

static void test_sorted() {
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    int vals[] = { 5, 1, 3, 4, 2 };
    for (int i = 0; i < 5; ++i) set_add(s, &vals[i]);

    bool ok = true;
    for (size_t i = 0; i < 5; ++i) {
        const int *p = set_get_sorted(s, i);
        if (*p != (int)(i + 1)) ok = false;
    }

    if (!ok) fail("sorted ints");
    else pass("sorted ints");

    set_destroy(s);
}

static void test_fuzz() {
    set_t *s = set_create(sizeof(int), int_hash, int_cmp, NULL, NULL);

    srand((unsigned)time(NULL));

    for (int i = 0; i < 5000; ++i) {
        int v = rand() % 2000;
        set_add(s, &v);
    }

    for (int i = 0; i < 2000; ++i) {
        int v = i;
        if (set_contains(s, &v)) {
            if (!set_remove(s, &v)) {
                fail("fuzz remove");
                set_destroy(s);
                return;
            }
        }
    }

    pass("fuzz test");
    set_destroy(s);
}

/* -------------------------------------------------------------
 * Main
 * ------------------------------------------------------------- */

int main(void) {
    printf(BLUE "Running set tests...\n" RESET);

    test_ints();
    test_strings();
    test_deep();
    test_sorted();
    test_fuzz();

    printf(BLUE "Done.\n" RESET);
    return 0;
}
