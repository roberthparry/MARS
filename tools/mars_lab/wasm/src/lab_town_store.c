/**
 * @file lab_town_store.c
 * @brief Shared native town records, jurisdiction lists and direct spatial buckets.
 *
 * dictionary_t provides selection-key lookup, while array_t owns record pointers
 * and jurisdiction/bucket sequences. JavaScript receives temporary display copies,
 * never native pointers or retained bridge handles. An 8 MiB admission budget
 * accounts for copied text plus a conservative per-record index allowance.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_town_store.h"

typedef struct _town_record_t {
    char *key;
    char code[7];
    char *fields[8];
    double latitude, longitude;
    bool preferred;
    size_t bytes;
} town_record_t;

static const char *const field_names[] = {
    "name", "latitude", "longitude", "elevation", "timezone", "value", "detail", "country_code"
};

typedef struct _town_store_t {
    lab_serial_t serial;
    array_t *records;
    array_t *buckets[37 * 72];
    dictionary_t *by_key;
    dictionary_t *by_country;
    dictionary_t *long_names;
    size_t text_bytes;
    const size_t text_limit;
} town_store_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static town_store_t *town_store(void)
{
    static town_store_t instance = {.serial = LAB_SERIAL_INITIALIZER, .text_limit = 8 * 1048576};
    return &instance;
}

static int lab_town_store_add_owned(jsref code_value, jsref rows);
static jsref lab_town_store_snapshot_owned(jsref code_value);
static void lab_town_store_list_owned(jsref code_value);
static void lab_town_store_get_owned(jsref code_value, jsref key_value);
static void lab_town_store_region_owned(double west, double south, double east, double north);
static void lab_town_store_clear_owned(void);

static const struct { const char *key, *name; } name_exceptions[] = {
    {"GB-WLS:Llanfairpwllgwyngyll", "Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch"}
};

static int town_compare(const void *left, const void *right)
{
    const town_record_t *a = *(town_record_t *const *)left, *b = *(town_record_t *const *)right;
    if (a->preferred != b->preferred) return b->preferred - a->preferred;
    unsigned mark = lab_dom_mark();
    int result = lab_dom_locale_compare(lab_dom_utf8_string((const unsigned char *)a->fields[0], strlen(a->fields[0])),
                                      lab_dom_utf8_string((const unsigned char *)b->fields[0], strlen(b->fields[0])), false);
    lab_dom_release(mark);
    return result;
}

static size_t text_hash(const void *key)
{
    const unsigned char *p = (const unsigned char *)*(const char *const *)key;
    size_t hash = 2166136261u;
    while (*p) hash = (hash ^ *p++) * 16777619u;
    return hash;
}

static int text_compare(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static char *copy_text(jsref value)
{
    if (lab_dom_type(value) != LAB_VALUE_STRING) value = lab_dom_string("");
    unsigned length = lab_dom_length(value);
    if (length > 65536) return NULL;
    char *text = malloc(length * 3 + 1);
    if (!text) return NULL;
    int bytes = lab_dom_utf8_copy(value, (unsigned char *)text, length * 3);
    if (bytes < 0) { free(text); return NULL; }
    text[bytes] = 0;
    if (strlen(text) != (unsigned)bytes) { free(text); return NULL; }
    char *compact = realloc(text, (size_t)bytes + 1);
    if (!compact) free(text);
    return compact;
}

static bool copy_code(jsref value, char code[7])
{
    int length = lab_dom_utf8_copy(value, (unsigned char *)code, 6);
    if (length < 2 || length > 6) return false;
    code[length] = 0;
    for (int i = 0; i < length; ++i)
        if (!((code[i] >= 'A' && code[i] <= 'Z') || (i == 2 && code[i] == '-') ||
              (i > 2 && code[i] >= '0' && code[i] <= '9'))) return false;
    return length == 2 || (length >= 4 && code[2] == '-');
}

static char *record_key(const char *code, const char *value)
{
    size_t a = strlen(code), b = strlen(value);
    char *key = malloc(a + b + 2);
    if (key) { memcpy(key, code, a); key[a] = ':'; memcpy(key + a + 1, value, b + 1); }
    return key;
}

static jsref text_value(const char *text)
{
    return lab_dom_utf8_string((const unsigned char *)text, strlen(text));
}

static jsref town_value(const town_record_t *record, bool place)
{
    town_store_t *state = town_store();
    jsref town = lab_dom_object(LAB_VALUE_OBJECT);
    for (unsigned i = 0; i < sizeof field_names / sizeof *field_names; ++i)
        lab_dom_set(town, field_names[i], text_value(record->fields[i]));
    lab_dom_set(town, "default", lab_dom_scalar(LAB_VALUE_BOOLEAN, record->preferred));
    if (!place) return town;
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "town", town);
    lab_dom_set(result, "jurisdiction", text_value(record->code));
    char *key = record_key(record->code, record->fields[0]);
    const char *full = record->fields[0];
    if (key && state->long_names) dictionary_get(state->long_names, &key, &full);
    lab_dom_set(result, "fullName", text_value(full));
    free(key);
    return result;
}

static void release_record(town_record_t *record)
{
    for (unsigned i = 0; i < sizeof field_names / sizeof *field_names; ++i) free(record->fields[i]);
    free(record->key);
    free(record);
}

static array_t **town_bucket(const town_record_t *record)
{
    town_store_t *state = town_store();
    if (!isfinite(record->latitude) || !isfinite(record->longitude) || fabs(record->latitude) > 90 ||
        fabs(record->longitude) > 180) return NULL;
    int y = (int)floor(record->latitude / 5) + 18;
    int x = (int)floor((record->longitude == 180 ? -180 : record->longitude) / 5) + 36;
    return &state->buckets[y * 72 + x];
}

static bool initialise(void)
{
    town_store_t *state = town_store();
    if (!state->long_names) {
        state->long_names = dictionary_create(sizeof(char *), sizeof(char *), text_hash, text_compare,
                                       NULL, NULL, NULL, NULL, NULL);
        if (!state->long_names) return false;
        for (size_t i = 0; i < sizeof name_exceptions / sizeof *name_exceptions; ++i) {
            if (!dictionary_set(state->long_names, &name_exceptions[i].key, &name_exceptions[i].name)) {
                dictionary_destroy(state->long_names); state->long_names = NULL; return false;
            }
        }
    }
    if (!state->records) state->records = array_create(sizeof(town_record_t *), NULL, NULL);
    if (!state->by_key) state->by_key = dictionary_create(sizeof(char *), sizeof(town_record_t *), text_hash, text_compare,
                                             NULL, NULL, NULL, NULL, NULL);
    if (!state->by_country) state->by_country = dictionary_create(sizeof(char *), sizeof(array_t *), text_hash, text_compare,
                                                     NULL, NULL, NULL, NULL, NULL);
    return state->records && state->by_key && state->by_country;
}

/* Append only fully copied records; roll back indexes if an allocation fails. */
static int lab_town_store_add_owned(jsref code_value, jsref rows)
{
    town_store_t *state = town_store();
    char code[7];
    if (!copy_code(code_value, code) || !initialise() || lab_dom_type(rows) != LAB_VALUE_ARRAY) {
        return -1;
    }
    const char *country_key = code;
    char *created_key = NULL;
    array_t *country = NULL;
    if (!dictionary_get(state->by_country, &country_key, &country)) {
        char *owned = copy_text(code_value);
        country = array_create(sizeof(town_record_t *), NULL, NULL);
        if (!owned || !country || !dictionary_set(state->by_country, &owned, &country)) {
            free(owned); array_destroy(country); return -1;
        }
        created_key = owned;
    }
    int added = 0;
    size_t initial_count = array_size(state->records);
    unsigned mark = lab_dom_mark();
    for (unsigned index = 0; index < lab_dom_count(rows); ++index) {
        lab_dom_release(mark);
        jsref row = lab_dom_item(rows, index);
        town_record_t *record = calloc(1, sizeof *record), *existing = NULL;
        if (!record) goto rollback;
        record->bytes = 512; /* Conservative allowance for record, index and array capacity overhead. */
        memcpy(record->code, code, strlen(code) + 1);
        bool valid = true;
        for (unsigned i = 0; i < sizeof field_names / sizeof *field_names; ++i) {
            record->fields[i] = copy_text(lab_dom_get(row, field_names[i]));
            if (!record->fields[i]) valid = false;
            else record->bytes += strlen(record->fields[i]) + 1;
        }
        if (!valid) { release_record(record); goto rollback; }
        record->key = record_key(code, record->fields[5]);
        if (!record->key) { release_record(record); goto rollback; }
        record->bytes += strlen(record->key) + 1;
        if (dictionary_get(state->by_key, &record->key, &existing)) { release_record(record); continue; }
        record->latitude = lab_dom_to_number(lab_dom_get(row, "latitude"));
        record->longitude = lab_dom_to_number(lab_dom_get(row, "longitude"));
        record->preferred = lab_dom_truth(lab_dom_get(row, "default"));
        array_t **bucket = town_bucket(record);
        if (bucket && !*bucket) *bucket = array_create(sizeof(town_record_t *), NULL, NULL);
        bool indexed = false, owned = false, listed = false, spatial = false;
        if (state->text_bytes + record->bytes <= state->text_limit && (!bucket || *bucket)) {
            indexed = dictionary_set(state->by_key, &record->key, &record);
            if (indexed) owned = array_append_carray(state->records, &record, 1);
            if (owned) listed = array_append_carray(country, &record, 1);
            if (listed) spatial = !bucket || array_append_carray(*bucket, &record, 1);
        }
        if (!spatial) {
            if (listed) array_remove(country, array_size(country) - 1);
            if (owned) array_remove(state->records, array_size(state->records) - 1);
            if (indexed) dictionary_remove(state->by_key, &record->key);
            release_record(record); goto rollback;
        }
        state->text_bytes += record->bytes;
        ++added;
    }
    lab_dom_release(mark);
    if (added) array_sort(country, town_compare);
    return added;

rollback:
    lab_dom_release(mark);
    while (array_size(state->records) > initial_count) {
        town_record_t *record = *(town_record_t **)array_get(state->records, array_size(state->records) - 1);
        array_t **bucket = town_bucket(record);
        if (bucket) array_remove(*bucket, array_size(*bucket) - 1);
        array_remove(country, array_size(country) - 1);
        array_remove(state->records, array_size(state->records) - 1);
        dictionary_remove(state->by_key, &record->key);
        state->text_bytes -= record->bytes;
        release_record(record);
    }
    if (created_key) {
        dictionary_remove(state->by_country, &created_key);
        free(created_key); array_destroy(country);
    }
    return -1;
}

/* Build an independent snapshot within the caller's bridge scope. */
static jsref lab_town_store_snapshot_owned(jsref code_value)
{
    town_store_t *state = town_store();
    char code[7];
    const char *key = code;
    array_t *country = NULL;
    if (!copy_code(code_value, code) || !state->by_country || !dictionary_get(state->by_country, &key, &country)) {
        return 0;
    }
    jsref result = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (size_t i = 0; i < array_size(country); ++i) {
        lab_dom_push(result, town_value(*(town_record_t **)array_get(country, i), false));
        lab_dom_release(mark);
    }
    return result;
}

/* Copy a jurisdiction list for a menu. */
static void lab_town_store_list_owned(jsref code_value)
{
    lab_dom_return(lab_town_store_snapshot_owned(code_value));
}

/* Selection lookup does not scan the catalogue. */
static void lab_town_store_get_owned(jsref code_value, jsref key_value)
{
    town_store_t *state = town_store();
    char code[7];
    char *value = copy_text(key_value);
    char *key = value && copy_code(code_value, code) ? record_key(code, value) : NULL;
    town_record_t *record = NULL;
    bool found = key && state->by_key && dictionary_get(state->by_key, &key, &record);
    free(value); free(key);
    lab_dom_return(found ? town_value(record, true) : 0);
}

/* Fetch a bounded set of direct-indexed spatial buckets in one bridge call. */
static void lab_town_store_region_owned(double west, double south, double east, double north)
{
    town_store_t *state = town_store();
    jsref result = lab_dom_object(LAB_VALUE_ARRAY);
    if (!isfinite(west) || !isfinite(east) || !isfinite(south) || !isfinite(north) ||
        west > east || south > north || south > 90 || north < -90 || fabs(west) > 1080 || fabs(east) > 1080) {
        lab_dom_return(result); return;
    }
    int first = (int)floor(west / 5), last = (int)floor(east / 5);
    if (last - first >= 72) last = first + 71;
    for (int y = (int)floor(fmax(-90, south) / 5); y <= (int)floor(fmin(90, north) / 5); ++y) {
        for (int x = first; x <= last; ++x) {
            int wrapped = ((x + 36) % 72 + 72) % 72;
            array_t *bucket = state->buckets[(y + 18) * 72 + wrapped];
            if (!bucket) continue;
            unsigned mark = lab_dom_mark();
            for (size_t i = 0; i < array_size(bucket); ++i) {
                lab_dom_push(result, town_value(*(town_record_t **)array_get(bucket, i), true));
                lab_dom_release(mark);
            }
        }
    }
    lab_dom_return(result);
}

/* Drop indexes before their borrowed record keys. */
static void lab_town_store_clear_owned(void)
{
    town_store_t *state = town_store();
    if (state->by_country) {
        for (size_t i = 0; i < dictionary_size(state->by_country); ++i) {
            char *key = *(char *const *)dictionary_get_key(state->by_country, i);
            array_t *country = *(array_t *const *)dictionary_get_value(state->by_country, i);
            free(key); array_destroy(country);
        }
    }
    dictionary_destroy(state->by_country); state->by_country = NULL;
    dictionary_destroy(state->by_key); state->by_key = NULL;
    dictionary_destroy(state->long_names); state->long_names = NULL;
    for (unsigned i = 0; i < 37 * 72; ++i) { array_destroy(state->buckets[i]); state->buckets[i] = NULL; }
    if (state->records) for (size_t i = 0; i < array_size(state->records); ++i)
        release_record(*(town_record_t **)array_get(state->records, i));
    array_destroy(state->records); state->records = NULL; state->text_bytes = 0;
}

typedef struct _town_store_add_request_t {
    jsref code_value;
    jsref rows;
    int result;
} town_store_add_request_t;

static void town_store_add_dispatch(void *context)
{
    town_store_add_request_t *request = context;
    request->result = lab_town_store_add_owned(request->code_value, request->rows);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
int lab_town_store_add(jsref code_value, jsref rows)
{
    town_store_t *state = town_store();
    town_store_add_request_t request = {.code_value = code_value, .rows = rows, .result = -1};
    lab_serial_dispatch(&state->serial, town_store_add_dispatch, &request);
    return request.result;
}

typedef struct _town_store_snapshot_request_t {
    jsref code_value;
    jsref result;
} town_store_snapshot_request_t;

static void town_store_snapshot_dispatch(void *context)
{
    town_store_snapshot_request_t *request = context;
    request->result = lab_town_store_snapshot_owned(request->code_value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
jsref lab_town_store_snapshot(jsref code_value)
{
    town_store_t *state = town_store();
    town_store_snapshot_request_t request = {.code_value = code_value, .result = 0};
    lab_serial_dispatch(&state->serial, town_store_snapshot_dispatch, &request);
    return request.result;
}

typedef struct _town_store_list_request_t {
    jsref code_value;
} town_store_list_request_t;

static void town_store_list_dispatch(void *context)
{
    town_store_list_request_t *request = context;
    lab_town_store_list_owned(request->code_value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_town_store_list(jsref code_value)
{
    town_store_t *state = town_store();
    town_store_list_request_t request = {.code_value = code_value};
    if (!lab_serial_dispatch(&state->serial, town_store_list_dispatch, &request)) lab_dom_return(0);
}

typedef struct _town_store_get_request_t {
    jsref code_value;
    jsref key_value;
} town_store_get_request_t;

static void town_store_get_dispatch(void *context)
{
    town_store_get_request_t *request = context;
    lab_town_store_get_owned(request->code_value, request->key_value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_town_store_get(jsref code_value, jsref key_value)
{
    town_store_t *state = town_store();
    town_store_get_request_t request = {.code_value = code_value, .key_value = key_value};
    if (!lab_serial_dispatch(&state->serial, town_store_get_dispatch, &request)) lab_dom_return(0);
}

typedef struct _town_store_region_request_t {
    double west;
    double south;
    double east;
    double north;
} town_store_region_request_t;

static void town_store_region_dispatch(void *context)
{
    town_store_region_request_t *request = context;
    lab_town_store_region_owned(request->west, request->south, request->east, request->north);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_town_store_region(double west, double south, double east, double north)
{
    town_store_t *state = town_store();
    town_store_region_request_t request = {.west = west, .south = south, .east = east, .north = north};
    if (!lab_serial_dispatch(&state->serial, town_store_region_dispatch, &request)) lab_dom_return(0);
}

static void town_store_clear_dispatch(void *context)
{
    (void)context;
    lab_town_store_clear_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_town_store_clear(void)
{
    town_store_t *state = town_store();
    if (!lab_serial_dispatch(&state->serial, town_store_clear_dispatch, NULL)) lab_dom_return(0);
}
