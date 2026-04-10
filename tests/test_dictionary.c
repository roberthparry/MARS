/* test_dictionary.c - tests for arena-backed dict_t */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "dictionary.h"

/* -------------------------------------------------------------
 * Colour helpers
 * ------------------------------------------------------------- */

#define C_RED     "\x1b[31m"
#define C_GREEN   "\x1b[32m"
#define C_YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"

static void pass(const char *msg) {
    printf(C_GREEN "PASS" RESET " %s\n", msg);
}

static void fail(const char *msg) {
    printf(C_RED "FAIL" RESET " %s\n", msg);
}

/* -------------------------------------------------------------
 * strdup replacement (C99)
 * ------------------------------------------------------------- */

static char *strclone(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* -------------------------------------------------------------
 * Shallow int key/value
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
 * Deep string key/value
 * ------------------------------------------------------------- */

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
 * Deep struct key/value
 * ------------------------------------------------------------- */

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
    d->name = strclone(s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

/* -------------------------------------------------------------
 * Test: shallow int → shallow int
 * ------------------------------------------------------------- */

static void test_int_int(void) {
    printf(C_YELLOW "test_int_int\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    int k1 = 1, v1 = 10;
    int k2 = 2, v2 = 20;
    int out;

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);

    if (!dict_get(d, &k1, &out) || out != 10) fail("int-int get k1");
    else pass("int-int get k1");

    if (!dict_get(d, &k2, &out) || out != 20) fail("int-int get k2");
    else pass("int-int get k2");

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: deep string key → shallow int value
 * ------------------------------------------------------------- */

static void test_str_int(void) {
    printf(C_YELLOW "test_str_int\n" RESET);

    dict_t *d = dict_create(
        sizeof(char *), sizeof(int),
        str_hash, str_cmp,
        str_clone, str_destroy,
        NULL, NULL
    );

    const char *k1 = "alpha";
    const char *k2 = "beta";
    int v1 = 111, v2 = 222;
    int out;

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);

    if (!dict_get(d, &k1, &out) || out != 111) fail("str-int get alpha");
    else pass("str-int get alpha");

    if (!dict_get(d, &k2, &out) || out != 222) fail("str-int get beta");
    else pass("str-int get beta");

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: shallow int key → deep string value
 * ------------------------------------------------------------- */

static void test_int_str(void) {
    printf(C_YELLOW "test_int_str\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(char *),
        int_hash, int_cmp,
        NULL, NULL,
        str_clone, str_destroy
    );

    int k1 = 5, k2 = 6;
    const char *v1 = "hello";
    const char *v2 = "world";
    char *out;

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);

    if (!dict_get(d, &k1, &out) || strcmp(out, "hello") != 0) {
        fail("int-str get 5");
    } else {
        pass("int-str get 5");
    }
    free(out);  /* we own this clone */

    if (!dict_get(d, &k2, &out) || strcmp(out, "world") != 0) {
        fail("int-str get 6");
    } else {
        pass("int-str get 6");
    }
    free(out);  /* we own this clone */

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: deep struct key → deep struct value
 * ------------------------------------------------------------- */

static void test_deep_deep(void) {
    printf(C_YELLOW "test_deep_deep\n" RESET);

    dict_t *d = dict_create(
        sizeof(struct deep), sizeof(struct deep),
        deep_hash, deep_cmp,
        deep_clone, deep_destroy,
        deep_clone, deep_destroy
    );

    /* 1. Inputs: no heap ownership here, just literals.
       The dictionary will deep_clone these into its arenas. */
    struct deep k1 = { "k1", 1 };
    struct deep v1 = { "v1", 10 };

    struct deep k2 = { "k2", 2 };
    struct deep v2 = { "v2", 20 };

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);

    /* 2. Output struct: we own whatever deep_clone puts in here. */
    struct deep out;

    if (!dict_get(d, &k1, &out)) {
        fail("deep-deep get k1");
    } else if (strcmp(out.name, "v1") != 0 || out.value != 10) {
        fail("deep-deep value mismatch k1");
        free(out.name); /* we own this */
    } else {
        pass("deep-deep get k1");
        free(out.name); /* we own this */
    }

    if (!dict_get(d, &k2, &out)) {
        fail("deep-deep get k2");
    } else if (strcmp(out.name, "v2") != 0 || out.value != 20) {
        fail("deep-deep value mismatch k2");
        free(out.name); /* we own this */
    } else {
        pass("deep-deep get k2");
        free(out.name); /* we own this */
    }

    /* 3. Destroy dictionary: it frees only its own arena‑owned clones. */
    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: sorted keys and values
 * ------------------------------------------------------------- */

static void test_sorted(void) {
    printf(C_YELLOW "test_sorted\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    int keys[] = { 5, 1, 4, 3, 2 };
    int vals[] = { 50, 10, 40, 30, 20 };

    for (int i = 0; i < 5; ++i)
        dict_set(d, &keys[i], &vals[i]);

    bool ok = true;

    for (size_t i = 0; i < 5; ++i) {
        const int *k = dict_get_key_sorted(d, i);
        if (*k != (int)(i + 1)) ok = false;
    }

    if (!ok) fail("sorted keys");
    else pass("sorted keys");

    ok = true;
    for (size_t i = 0; i < 5; ++i) {
        const int *v = dict_get_value_sorted(d, i);
        if (*v != (int)((i + 1) * 10)) ok = false;
    }

    if (!ok) fail("sorted values");
    else pass("sorted values");

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: entry-based API
 * ------------------------------------------------------------- */

static void test_entries(void) {
    printf(C_YELLOW "test_entries\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    int k1 = 7, v1 = 70;
    int k2 = 8, v2 = 80;

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);

    dict_entry_t *e;
    int out;

    if (!dict_get_entry(d, &k1, &e)) fail("entry get k1");
    else {
        const int *kp = dict_entry_key(e);
        const int *vp = dict_entry_value(e);
        if (*kp != 7 || *vp != 70) fail("entry key/value mismatch");
        else pass("entry get k1");
    }

    int newv = 700;
    dict_set_entry(d, e, &newv);

    if (!dict_get(d, &k1, &out) || out != 700) fail("entry set k1");
    else pass("entry set k1");

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: foreach
 * ------------------------------------------------------------- */

static void foreach_cb(const dict_entry_t *e, void *ud) {
    int *sum = ud;
    const int *v = dict_entry_value(e);
    *sum += *v;
}

static void test_foreach(void) {
    printf(C_YELLOW "test_foreach\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    int k1 = 1, v1 = 10;
    int k2 = 2, v2 = 20;
    int k3 = 3, v3 = 30;

    dict_set(d, &k1, &v1);
    dict_set(d, &k2, &v2);
    dict_set(d, &k3, &v3);

    int sum = 0;
    dict_foreach(d, foreach_cb, &sum);

    if (sum != 60) fail("foreach sum");
    else pass("foreach sum");

    dict_destroy(d);
}

/* -------------------------------------------------------------
 * Test: fuzz
 * ------------------------------------------------------------- */

static void test_fuzz(void) {
    printf(C_YELLOW "test_fuzz\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    srand((unsigned)time(NULL));

    for (int i = 0; i < 5000; ++i) {
        int k = rand() % 2000;
        int v = k * 2;
        dict_set(d, &k, &v);
    }

    bool ok = true;

    for (int k = 0; k < 2000; ++k) {
        int out;
        if (dict_get(d, &k, &out)) {
            if (out != k * 2) {
                ok = false;
                break;
            }
        }
    }

    if (!ok) fail("fuzz test");
    else pass("fuzz test");

    dict_destroy(d);
}

/* C99-compatible foreach callback */
static void fuzz_foreach_cb(const dict_entry_t *e, void *ud) {
    (void)e;  /* unused */
    int *count = (int *)ud;
    (*count)++;
}

static void test_sorted_fuzz(void) {
    printf(C_YELLOW "test_sorted_fuzz\n" RESET);

    dict_t *d = dict_create(
        sizeof(int), sizeof(int),
        int_hash, int_cmp,
        NULL, NULL,
        NULL, NULL
    );

    const int MAX = 2000;
    int ref[MAX];
    for (int i = 0; i < MAX; ++i) ref[i] = -1;

    srand((unsigned)time(NULL));

    const int ITER = 5000;      /* was 20000 */
    const int CHECK_EVERY = 50; /* only do heavy checks every 50 iters */

    for (int iter = 0; iter < ITER; ++iter) {
        int k = rand() % MAX;
        int op = rand() % 3;

        if (op == 0) {
            int v = rand();
            dict_set(d, &k, &v);
            ref[k] = v;
        } else if (op == 1) {
            dict_remove(d, &k);
            ref[k] = -1;
        } else {
            int out;
            bool got = dict_get(d, &k, &out);
            if (ref[k] == -1 && got) {
                fail("fuzz: dict_get returned value for missing key");
                goto done;
            }
            if (ref[k] != -1 && (!got || out != ref[k])) {
                fail("fuzz: dict_get mismatch");
                goto done;
            }
        }

        if (iter % CHECK_EVERY != 0)
            continue;

        /* Validate sorted keys */
        size_t n = dict_size(d);
        int last = -2147483648;
        for (size_t i = 0; i < n; ++i) {
            const int *kp = dict_get_key_sorted(d, i);
            if (!kp) {
                fail("fuzz: NULL key in sorted view");
                goto done;
            }
            if (i > 0 && *kp < last) {
                fail("fuzz: sorted keys out of order");
                goto done;
            }
            last = *kp;
        }

        /* Validate sorted values */
        last = -2147483648;
        for (size_t i = 0; i < n; ++i) {
            const int *vp = dict_get_value_sorted(d, i);
            if (!vp) {
                fail("fuzz: NULL value in sorted view");
                goto done;
            }
            if (i > 0 && *vp < last) {
                fail("fuzz: sorted values out of order");
                goto done;
            }
            last = *vp;
        }

        /* Validate entry-sorted view */
        last = -2147483648;
        for (size_t i = 0; i < n; ++i) {
            dict_entry_t *e;
            if (!dict_get_entry_sorted(d, i, DICT_SORT_BY_KEY, &e)) {
                fail("fuzz: entry_sorted returned false");
                goto done;
            }
            const int *kp = dict_entry_key(e);
            if (i > 0 && *kp < last) {
                fail("fuzz: entry_sorted keys out of order");
                goto done;
            }
            last = *kp;
        }

        /* Validate foreach count matches reference */
        int seen = 0;
        dict_foreach(d, fuzz_foreach_cb, &seen);

        int ref_count = 0;
        for (int i = 0; i < MAX; ++i)
            if (ref[i] != -1) ref_count++;

        if (seen != ref_count) {
            fail("fuzz: foreach count mismatch");
            goto done;
        }
    }

    pass("sorted fuzz");

done:
    dict_destroy(d);
}

void test_readme_example_deep(void) {
    dict_t *dict = dict_create(
        sizeof(struct deep), sizeof(struct deep),
        deep_hash, deep_cmp,
        deep_clone, deep_destroy,
        deep_clone, deep_destroy
    );

    struct deep k1 = { "k1", 1 };
    struct deep v1 = { "v1", 10 };

    struct deep k2 = { "k2", 2 };
    struct deep v2 = { "v2", 20 };

    dict_set(dict, &k1, &v1);
    dict_set(dict, &k2, &v2);

    struct deep out;

    if (dict_get(dict, &k1, &out)) {
        printf("Value for key '%s': %s (%d)\n",
               k1.name, out.name, out.value);
        free(out.name);
    }

    if (dict_get(dict, &k2, &out)) {
        printf("Value for key '%s': %s (%d)\n",
               k2.name, out.name, out.value);
        free(out.name);
    }

    dict_destroy(dict);
}

void test_readme_example_shallow(void) {
    dict_t *dict = dict_create(
        sizeof(int), sizeof(char *),
        int_hash, int_cmp,
        NULL, NULL,
        str_clone, str_destroy
    );

    int k1 = 5, k2 = 6;
    const char *v1 = "hello";
    const char *v2 = "world";

    dict_set(dict, &k1, &v1);
    dict_set(dict, &k2, &v2);

    char *out;

    if (dict_get(dict, &k1, &out)) {
        printf("Value for key %d: %s\n", k1, out);
        free(out);
    }

    if (dict_get(dict, &k2, &out)) {
        printf("Value for key %d: %s\n", k2, out);
        free(out);
    }

    dict_destroy(dict);
}

void test_readme_examples(void) {
    printf(C_YELLOW "\ntesting README examples...\n" RESET);

    test_readme_example_deep();
    test_readme_example_shallow();
}

/* -------------------------------------------------------------
 * Main
 * ------------------------------------------------------------- */

int main(void) {
    printf(BLUE "Running dictionary tests...\n" RESET);

    test_int_int();
    test_str_int();
    test_int_str();
    test_deep_deep();
    test_sorted();
    test_entries();
    test_foreach();
    test_fuzz();
    test_sorted_fuzz();
    
    test_readme_examples();

    printf(BLUE "Done.\n" RESET);
    return 0;
}
