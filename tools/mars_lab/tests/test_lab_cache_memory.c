/**
 * @file test_lab_cache_memory.c
 * @brief Standalone native ownership checks for the browser metadata caches.
 *
 * Runs the production flag and country stores with the real dictionary and serial
 * dispatcher under Valgrind. A bounded, allocation-free bridge fixture supplies
 * borrowed strings; it does not replace any cache allocation or dictionary logic.
 * This single-threaded executable checks replacement, FIFO eviction, byte limits,
 * rejected input and repeated teardown. It does not model browser handle lifetimes.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lab_dom.h"
#include "lab_flag_cache.h"
#include "lab_country_cache.h"

typedef struct _cache_fixture_t {
    const char *input[4];
    jsref returned;
    double numeric, entries, bytes;
} cache_fixture_t;

static cache_fixture_t *fixture(void)
{
    static cache_fixture_t instance;
    return &instance;
}

static const char *input(jsref value)
{
    assert(value > 0 && value < 4);
    return fixture()->input[value];
}

/* Supply borrowed ASCII strings without allocating test-side memory. */
unsigned lab_dom_length(jsref value) { return (unsigned)strlen(input(value)); }

/* Reject truncation just as the browser bridge does. */
int lab_dom_utf8_copy(jsref value, unsigned char *target, unsigned capacity)
{
    size_t size = strlen(input(value));
    if (size > capacity) return -1;
    memcpy(target, input(value), size);
    return (int)size;
}

/* Check the production flag prefix validation. */
bool lab_dom_starts_with(jsref value, const char *prefix)
{
    return strncmp(input(value), prefix, strlen(prefix)) == 0;
}

/* Output handles are scoped sentinels, never heap allocations. */
jsref lab_dom_object(lab_value_kind_t kind) { (void)kind; return 10; }

/* Read returned bytes so Valgrind also detects dangling output text. */
jsref lab_dom_utf8_string(const unsigned char *source, unsigned length)
{
    assert(strlen((const char *)source) == length);
    return 11;
}

/* Record diagnostic numeric output. */
jsref lab_dom_numeric(double value) { fixture()->numeric = value; return 12; }

/* Capture bounded-cache counters without retaining production pointers. */
void lab_dom_set(jsref object, const char *key, jsref value)
{
    assert(object == 10);
    if (value == 12 && !strcmp(key, "entries")) fixture()->entries = fixture()->numeric;
    if (value == 12 && !strcmp(key, "bytes")) fixture()->bytes = fixture()->numeric;
}

/* Capture a scoped result for hit/miss assertions. */
void lab_dom_return(jsref value) { fixture()->returned = value; }

static void populate(void)
{
    char code[3] = {0}, key[32];
    for (unsigned i = 0; i < 100; ++i) {
        code[0] = (char)('A' + i / 26); code[1] = (char)('A' + i % 26);
        fixture()->input[1] = code;
        fixture()->input[2] = "Original name";
        fixture()->input[3] = "data:image/svg+xml;base64,PHN2Zy8+";
        assert(lab_flag_cache_put(1, 2, 3));
        fixture()->input[2] = "Replacement name";
        assert(lab_flag_cache_put(1, 2, 3));
        lab_flag_cache_get(1); assert(fixture()->returned == 10);
        snprintf(key, sizeof key, "town-%u", i);
        fixture()->input[1] = key; fixture()->input[3] = code;
        assert(lab_country_cache_put(1, 2, 3));
        assert(lab_country_cache_put(1, 2, 3));
        lab_country_cache_get(1); assert(fixture()->returned == 10);
    }
    fixture()->input[1] = "AA";
    lab_flag_cache_get(1); assert(!fixture()->returned);
    fixture()->input[1] = "town-0";
    lab_country_cache_get(1); assert(!fixture()->returned);
    lab_flag_cache_stats(); assert(fixture()->entries == 32 && fixture()->bytes > 0);
}

/* Exercise actual production ownership repeatedly, then leave no cache allocations live. */
int main(void)
{
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        populate();
        lab_flag_cache_clear(); lab_country_cache_clear();
        lab_flag_cache_clear(); lab_country_cache_clear();
        lab_flag_cache_stats(); assert(fixture()->entries == 0 && fixture()->bytes == 0);
    }
    char *large = malloc(600001);
    assert(large);
    memset(large, 'x', 600000); large[600000] = 0;
    fixture()->input[1] = "GB"; fixture()->input[2] = large; fixture()->input[3] = "";
    assert(lab_flag_cache_put(1, 2, 3));
    fixture()->input[1] = "FR"; assert(lab_flag_cache_put(1, 2, 3));
    fixture()->input[1] = "GB";
    lab_flag_cache_get(1); assert(!fixture()->returned);
    fixture()->input[3] = "GB";
    assert(!lab_country_cache_put(1, 2, 3));
    fixture()->input[1] = "invalid";
    assert(!lab_flag_cache_put(1, 2, 3));
    fixture()->input[1] = "GB"; fixture()->input[3] = "invalid image";
    assert(!lab_flag_cache_put(1, 2, 3));
    free(large);
    memset(fixture(), 0, sizeof *fixture());
    lab_flag_cache_clear(); lab_country_cache_clear();
    puts("PASS: metadata cache replacement, eviction, rejection and repeated teardown");
    return 0;
}
