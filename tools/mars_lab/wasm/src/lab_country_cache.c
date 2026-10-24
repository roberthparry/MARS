/**
 * @file lab_country_cache.c
 * @brief Bounded native coordinate-to-country metadata with dictionary lookup.
 *
 * Owns UTF-8 copies and linked FIFO eviction. Requests and cancellation remain
 * browser-owned; no asynchronous resource or scoped handle is stored here.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 */
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_country_cache.h"

typedef struct _country_entry_t {
    struct _country_entry_t *previous, *next;
    char key[128], code[7], nautical_zone[8];
    unsigned char *name;
    size_t bytes;
} country_entry_t;

typedef struct _country_cache_t {
    lab_serial_t serial;
    dictionary_t *countries;
    country_entry_t *oldest;
    country_entry_t *newest;
    size_t text_bytes;
} country_cache_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static country_cache_t *country_cache(void)
{
    static country_cache_t instance = {.serial = LAB_SERIAL_INITIALIZER};
    return &instance;
}

static void lab_country_cache_get_owned(jsref key_value);
static bool lab_country_cache_put_owned(jsref key, jsref name, jsref code, jsref nautical_zone);
static void lab_country_cache_clear_owned(void);

static size_t country_hash(const void *key)
{
    const unsigned char *p = key;
    size_t hash = 2166136261u;
    while (*p) hash = (hash ^ *p++) * 16777619u;
    return hash;
}

static int country_compare(const void *a, const void *b)
{
    return strcmp(a, b);
}

static bool copy_key(jsref value, char *key, unsigned capacity)
{
    int length = lab_dom_utf8_copy(value, (unsigned char *)key, capacity - 1);
    if (length < 0) return false;
    key[length] = 0;
    return strlen(key) == (unsigned)length;
}

static void remove_entry(country_entry_t *entry)
{
    country_cache_t *state = country_cache();
    dictionary_remove(state->countries, entry->key);
    if (entry->previous) entry->previous->next = entry->next;
    else state->oldest = entry->next;
    if (entry->next) entry->next->previous = entry->previous;
    else state->newest = entry->previous;
    state->text_bytes -= entry->bytes;
    free(entry->name); free(entry);
}

/* Return metadata without retaining the browser result. */
static void lab_country_cache_get_owned(jsref key_value)
{
    country_cache_t *state = country_cache();
    char key[128];
    country_entry_t *entry = NULL;
    if (!copy_key(key_value, key, sizeof key) || !state->countries || !dictionary_get(state->countries, key, &entry)) {
        lab_dom_return(0); return;
    }
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "name", lab_dom_utf8_string(entry->name, strlen((const char *)entry->name)));
    lab_dom_set(result, "code", lab_dom_utf8_string((const unsigned char *)entry->code, strlen(entry->code)));
    lab_dom_set(result, "nauticalZone", lab_dom_utf8_string((const unsigned char *)entry->nautical_zone,
                                                           strlen(entry->nautical_zone)));
    lab_dom_return(result);
}

/* Validate and allocate replacement text before changing existing entries. */
static bool lab_country_cache_put_owned(jsref key, jsref name, jsref code, jsref nautical_zone)
{
    country_cache_t *state = country_cache();
    unsigned length = lab_dom_length(name);
    if (length > 1048576) return false;
    country_entry_t *entry = calloc(1, sizeof *entry);
    if (!entry) return false;
    unsigned capacity = length > 1048576 / 3 ? 1048576 : length * 3;
    entry->name = malloc(capacity + 1);
    int bytes = entry->name ? lab_dom_utf8_copy(name, entry->name, capacity) : -1;
    if (bytes < 0 || !copy_key(key, entry->key, sizeof entry->key) ||
        !copy_key(code, entry->code, sizeof entry->code) ||
        (nautical_zone && !copy_key(nautical_zone, entry->nautical_zone, sizeof entry->nautical_zone))) {
        free(entry->name); free(entry); return false;
    }
    entry->name[bytes] = 0;
    entry->bytes = capacity + 1 + strlen(entry->key) + strlen(entry->code) + strlen(entry->nautical_zone);
    if (entry->bytes > 1048576) { free(entry->name); free(entry); return false; }
    if (!state->countries) state->countries = dictionary_create(128, sizeof(country_entry_t *), country_hash, country_compare,
                                                   NULL, NULL, NULL, NULL, NULL);
    country_entry_t *previous = NULL;
    if (state->countries && dictionary_get(state->countries, entry->key, &previous)) remove_entry(previous);
    while (state->oldest && (dictionary_size(state->countries) >= 64 || state->text_bytes + entry->bytes > 1048576))
        remove_entry(state->oldest);
    if (!state->countries || !dictionary_set(state->countries, entry->key, &entry)) {
        free(entry->name); free(entry); return false;
    }
    entry->previous = state->newest;
    if (state->newest) state->newest->next = entry;
    else state->oldest = entry;
    state->newest = entry;
    state->text_bytes += entry->bytes;
    return true;
}

/* Release metadata when its page owner closes. */
static void lab_country_cache_clear_owned(void)
{
    country_cache_t *state = country_cache();
    while (state->oldest) remove_entry(state->oldest);
    dictionary_destroy(state->countries); state->countries = NULL;
}

typedef struct _country_cache_get_request_t {
    jsref key_value;
} country_cache_get_request_t;

static void country_cache_get_dispatch(void *context)
{
    country_cache_get_request_t *request = context;
    lab_country_cache_get_owned(request->key_value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_country_cache_get(jsref key_value)
{
    country_cache_t *state = country_cache();
    country_cache_get_request_t request = {.key_value = key_value};
    if (!lab_serial_dispatch(&state->serial, country_cache_get_dispatch, &request)) lab_dom_return(0);
}

typedef struct _country_cache_put_request_t {
    jsref key;
    jsref name;
    jsref code;
    jsref nautical_zone;
    bool result;
} country_cache_put_request_t;

static void country_cache_put_dispatch(void *context)
{
    country_cache_put_request_t *request = context;
    request->result = lab_country_cache_put_owned(request->key, request->name, request->code, request->nautical_zone);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
bool lab_country_cache_put(jsref key, jsref name, jsref code, jsref nautical_zone)
{
    country_cache_t *state = country_cache();
    country_cache_put_request_t request = {.key = key, .name = name, .code = code, .nautical_zone = nautical_zone, .result = 0};
    lab_serial_dispatch(&state->serial, country_cache_put_dispatch, &request);
    return request.result;
}

static void country_cache_clear_dispatch(void *context)
{
    (void)context;
    lab_country_cache_clear_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_country_cache_clear(void)
{
    country_cache_t *state = country_cache();
    if (!lab_serial_dispatch(&state->serial, country_cache_clear_dispatch, NULL)) lab_dom_return(0);
}
