/**
 * @file lab_flag_cache.c
 * @brief Bounded dictionary-backed country metadata for browser flag consumers.
 *
 * Copies strings into native memory rather than retaining scoped JavaScript
 * handles. Dictionary lookup and linked FIFO eviction avoid linear cache scans.
 * Request cancellation and image decoding remain browser responsibilities.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 */
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_flag_cache.h"

typedef struct _flag_entry_t {
    struct _flag_entry_t *previous, *next;
    char code[7];
    unsigned char *name, *flag;
    unsigned name_size, flag_size;
} flag_entry_t;

typedef struct _flag_cache_t {
    lab_serial_t serial;
    dictionary_t *flags;
    flag_entry_t *oldest;
    flag_entry_t *newest;
    size_t text_bytes;
    const unsigned text_limit;
} flag_cache_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static flag_cache_t *flag_cache(void)
{
    static flag_cache_t instance = {.serial = LAB_SERIAL_INITIALIZER, .text_limit = 1048576};
    return &instance;
}

static void lab_flag_cache_get_owned(jsref code);
static bool lab_flag_cache_put_owned(jsref code, jsref name, jsref flag);
static void lab_flag_cache_clear_owned(void);
static void lab_flag_cache_stats_owned(void);

static size_t flag_hash(const void *key)
{
    const unsigned char *text = key;
    size_t hash = 2166136261u;
    while (*text) hash = (hash ^ *text++) * 16777619u;
    return hash;
}

static int flag_compare(const void *a, const void *b)
{
    return strcmp(a, b);
}

static bool flag_code(jsref value, char code[7])
{
    int size = lab_dom_utf8_copy(value, (unsigned char *)code, 6);
    if (size != 2 && (size < 4 || size > 6 || code[2] != '-')) return false;
    for (int i = 0; i < size; ++i) {
        if (i == 2) continue;
        if ((code[i] < 'A' || code[i] > 'Z') && (i < 3 || code[i] < '0' || code[i] > '9')) return false;
    }
    code[size] = 0;
    return true;
}

static unsigned char *flag_copy(jsref value, unsigned *size)
{
    flag_cache_t *state = flag_cache();
    unsigned length = lab_dom_length(value);
    if (length > state->text_limit) return NULL;
    unsigned capacity = length > state->text_limit / 3 ? state->text_limit : length * 3;
    unsigned char *text = malloc(capacity + 1);
    if (!text) return NULL;
    int used = lab_dom_utf8_copy(value, text, capacity);
    if (used < 0) { free(text); return NULL; }
    text[used] = 0;
    *size = (unsigned)used;
    unsigned char *compact = realloc(text, *size + 1);
    if (!compact) free(text);
    return compact;
}

static void flag_remove(flag_entry_t *entry)
{
    flag_cache_t *state = flag_cache();
    dictionary_remove(state->flags, entry->code);
    if (entry->previous) entry->previous->next = entry->next;
    else state->oldest = entry->next;
    if (entry->next) entry->next->previous = entry->previous;
    else state->newest = entry->previous;
    state->text_bytes -= entry->name_size + entry->flag_size;
    free(entry->name); free(entry->flag); free(entry);
}

/* Return newly scoped strings, never persistent browser references. */
static void lab_flag_cache_get_owned(jsref code)
{
    flag_cache_t *state = flag_cache();
    char key[7];
    flag_entry_t *entry = NULL;
    if (!flag_code(code, key) || !state->flags || !dictionary_get(state->flags, key, &entry)) {
        lab_dom_return(0);
        return;
    }
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "name", lab_dom_utf8_string(entry->name, entry->name_size));
    lab_dom_set(result, "flag", lab_dom_utf8_string(entry->flag, entry->flag_size));
    lab_dom_return(result);
}

/* Validate and copy before changing resident entries; maintain constant-time FIFO eviction. */
static bool lab_flag_cache_put_owned(jsref code, jsref name, jsref flag)
{
    flag_cache_t *state = flag_cache();
    char key[7] = {0};
    if (!flag_code(code, key) || (lab_dom_length(flag) && !lab_dom_starts_with(flag, "data:image/svg+xml;base64,")))
        return false;
    flag_entry_t *entry = calloc(1, sizeof *entry);
    if (!entry) return false;
    memcpy(entry->code, key, sizeof key);
    entry->name = flag_copy(name, &entry->name_size);
    entry->flag = flag_copy(flag, &entry->flag_size);
    size_t bytes = entry->name_size + entry->flag_size;
    if (!entry->name || !entry->flag || bytes > state->text_limit) {
        free(entry->name); free(entry->flag); free(entry);
        return false;
    }
    if (!state->flags)
        state->flags = dictionary_create(7, sizeof(flag_entry_t *), flag_hash, flag_compare, NULL, NULL, NULL, NULL, NULL);
    flag_entry_t *previous = NULL;
    if (state->flags && dictionary_get(state->flags, key, &previous)) flag_remove(previous);
    while (state->oldest && (dictionary_size(state->flags) >= 32 || state->text_bytes + bytes > state->text_limit))
        flag_remove(state->oldest);
    if (!state->flags || !dictionary_set(state->flags, key, &entry)) {
        free(entry->name); free(entry->flag); free(entry);
        return false;
    }
    entry->previous = state->newest;
    if (state->newest) state->newest->next = entry;
    else state->oldest = entry;
    state->newest = entry;
    state->text_bytes += bytes;
    return true;
}

/* Release the cache at page-owner shutdown. */
static void lab_flag_cache_clear_owned(void)
{
    flag_cache_t *state = flag_cache();
    while (state->oldest) flag_remove(state->oldest);
    dictionary_destroy(state->flags);
    state->flags = NULL;
}

/* Report bounded native storage without exposing entry pointers. */
static void lab_flag_cache_stats_owned(void)
{
    flag_cache_t *state = flag_cache();
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "entries", lab_dom_numeric(state->flags ? dictionary_size(state->flags) : 0));
    lab_dom_set(result, "bytes", lab_dom_numeric(state->text_bytes));
    lab_dom_return(result);
}

typedef struct _flag_cache_get_request_t {
    jsref code;
} flag_cache_get_request_t;

static void flag_cache_get_dispatch(void *context)
{
    flag_cache_get_request_t *request = context;
    lab_flag_cache_get_owned(request->code);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_flag_cache_get(jsref code)
{
    flag_cache_t *state = flag_cache();
    flag_cache_get_request_t request = {.code = code};
    if (!lab_serial_dispatch(&state->serial, flag_cache_get_dispatch, &request)) lab_dom_return(0);
}

typedef struct _flag_cache_put_request_t {
    jsref code;
    jsref name;
    jsref flag;
    bool result;
} flag_cache_put_request_t;

static void flag_cache_put_dispatch(void *context)
{
    flag_cache_put_request_t *request = context;
    request->result = lab_flag_cache_put_owned(request->code, request->name, request->flag);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
bool lab_flag_cache_put(jsref code, jsref name, jsref flag)
{
    flag_cache_t *state = flag_cache();
    flag_cache_put_request_t request = {.code = code, .name = name, .flag = flag, .result = 0};
    lab_serial_dispatch(&state->serial, flag_cache_put_dispatch, &request);
    return request.result;
}

static void flag_cache_clear_dispatch(void *context)
{
    (void)context;
    lab_flag_cache_clear_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_flag_cache_clear(void)
{
    flag_cache_t *state = flag_cache();
    if (!lab_serial_dispatch(&state->serial, flag_cache_clear_dispatch, NULL)) lab_dom_return(0);
}

static void flag_cache_stats_dispatch(void *context)
{
    (void)context;
    lab_flag_cache_stats_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_flag_cache_stats(void)
{
    flag_cache_t *state = flag_cache();
    if (!lab_serial_dispatch(&state->serial, flag_cache_stats_dispatch, NULL)) lab_dom_return(0);
}
