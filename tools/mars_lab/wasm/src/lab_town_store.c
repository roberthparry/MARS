/**
 * @file lab_town_store.c
 * @brief Shared native town records, jurisdiction lists and direct spatial buckets.
 *
 * dictionary_t provides selection-key lookup, while array_t owns record pointers
 * and jurisdiction/bucket sequences. Direct country slots collect regional memberships;
 * country snapshots merge duplicate keys without changing the retained local rule identity.
 * JavaScript receives temporary display copies,
 * never native pointers or retained bridge handles. A 16 MiB admission budget
 * accounts for copied text plus a conservative per-record index allowance. Native country LRU lists release
 * inactive catalogues under pressure; selected countries remain pinned and dropped catalogues can be reloaded.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 * Place snapshots provide formatted display names separately from unchanged catalogue names and selection keys.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_location.h"
#include "lab_serial.h"
#include "lab_town_store.h"

typedef struct _town_record_t {
    char *key;
    char code[7];
    char *fields[8];
    double latitude, longitude;
    bool preferred;
    size_t bytes;
    size_t slots[4];
} town_record_t;

enum { TOWN_OWNER_SLOT, TOWN_GROUP_SLOT, TOWN_COUNTRY_SLOT, TOWN_BUCKET_SLOT };

static const char *const field_names[] = {
    "name", "latitude", "longitude", "elevation", "timezone", "value", "detail", "country_code"
};

typedef struct _town_store_t {
    lab_serial_t serial;
    array_t *records;
    array_t *buckets[37 * 72];
    array_t *countries[26 * 26];
    dictionary_t *by_key;
    dictionary_t *by_country;
    dictionary_t *long_names;
    size_t text_bytes;
    const size_t text_limit;
    unsigned previous[26 * 26], next[26 * 26], oldest, newest;
    bool linked[26 * 26], pinned[26 * 26], discarded[26 * 26];
    unsigned evicted[26 * 26], evicted_count;
} town_store_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static town_store_t *town_store(void)
{
    static town_store_t instance = {.serial = LAB_SERIAL_INITIALIZER, .text_limit = 16 * 1048576};
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
    lab_dom_set(town, "jurisdiction", text_value(record->code));
    if (!place) return town;
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "town", town);
    lab_dom_set(result, "jurisdiction", text_value(record->code));
    lab_dom_set(result, "displayName", lab_location_town_text(text_value(record->fields[0])));
    char *key = record_key(record->code, record->fields[0]);
    const char *full = record->fields[0];
    if (key && state->long_names) dictionary_get(state->long_names, &key, &full);
    lab_dom_set(result, "fullName", lab_location_town_text(text_value(full)));
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

static unsigned country_index(const char *code)
{
    return (unsigned)(code[0] - 'A') * 26 + (unsigned)(code[1] - 'A');
}

static void country_unlink(unsigned index)
{
    town_store_t *state = town_store();
    if (!state->linked[index]) return;
    unsigned previous = state->previous[index], next = state->next[index];
    if (previous) state->next[previous - 1] = next;
    else state->oldest = next;
    if (next) state->previous[next - 1] = previous;
    else state->newest = previous;
    state->linked[index] = false;
}

static void country_touch(unsigned index)
{
    town_store_t *state = town_store();
    country_unlink(index);
    state->previous[index] = state->newest; state->next[index] = 0;
    if (state->newest) state->next[state->newest - 1] = index + 1;
    else state->oldest = index + 1;
    state->newest = index + 1; state->linked[index] = true;
}

static void release_empty_group(const char *code)
{
    town_store_t *state = town_store();
    const char *key = code;
    dictionary_entry_t *entry = NULL;
    if (!state->by_country || !dictionary_get_entry(state->by_country, &key, &entry)) return;
    array_t *group = *(array_t *const *)dictionary_entry_value(entry);
    if (array_size(group)) return;
    char *owned = *(char *const *)dictionary_entry_key(entry);
    dictionary_remove(state->by_country, &key);
    array_destroy(group); free(owned);
}

/* Swap the last pointer into each vacated slot; no owner, jurisdiction or spatial list is searched. */
static void discard_record(town_record_t *record)
{
    town_store_t *state = town_store();
    const char *key = record->code;
    array_t *group = NULL, **bucket = town_bucket(record);
    dictionary_get(state->by_country, &key, &group);
    array_t *lists[] = {state->records, group, state->countries[country_index(record->code)], bucket ? *bucket : NULL};
    for (unsigned i = 0; i < sizeof lists / sizeof *lists; ++i) {
        if (!lists[i]) continue;
        size_t last = array_size(lists[i]) - 1;
        town_record_t *moved = *(town_record_t **)array_get(lists[i], last);
        *(town_record_t **)array_get(lists[i], record->slots[i]) = moved;
        moved->slots[i] = record->slots[i];
        array_remove(lists[i], last);
    }
    release_empty_group(record->code);
    dictionary_remove(state->by_key, &record->key);
    state->text_bytes -= record->bytes;
    release_record(record);
}

static void discard_country(unsigned index)
{
    town_store_t *state = town_store();
    array_t *family = state->countries[index];
    bool populated = array_size(family) != 0;
    while (array_size(family)) discard_record(*(town_record_t **)array_get(family, array_size(family) - 1));
    array_destroy(family); state->countries[index] = NULL;
    country_unlink(index);
    if (populated && !state->discarded[index]) {
        state->discarded[index] = true; state->evicted[state->evicted_count++] = index;
    }
}

/* At most 676 directly indexed country nodes can be considered; active countries and the incoming batch stay pinned. */
static bool make_room(unsigned incoming, size_t bytes)
{
    town_store_t *state = town_store();
    if (bytes > state->text_limit) return false;
    while (state->text_bytes + bytes > state->text_limit) {
        unsigned victim = state->oldest;
        while (victim && (victim - 1 == incoming || state->pinned[victim - 1])) victim = state->next[victim - 1];
        if (!victim) return false;
        discard_country(victim - 1);
    }
    return true;
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
    unsigned root = country_index(code);
    array_t **national = &state->countries[root];
    if (!*national) *national = array_create(sizeof(town_record_t *), NULL, NULL);
    if (!*national) return -1;
    country_touch(root);
    const char *country_key = code;
    array_t *country = NULL;
    if (!dictionary_get(state->by_country, &country_key, &country)) {
        char *owned = copy_text(code_value);
        country = array_create(sizeof(town_record_t *), NULL, NULL);
        if (!owned || !country || !dictionary_set(state->by_country, &owned, &country)) {
            free(owned); array_destroy(country); return -1;
        }
    }
    int added = 0;
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
        bool indexed = false, owned = false, listed = false, spatial = false, grouped = false;
        if (make_room(root, record->bytes) && (!bucket || *bucket)) {
            record->slots[TOWN_OWNER_SLOT] = array_size(state->records);
            record->slots[TOWN_GROUP_SLOT] = array_size(country);
            record->slots[TOWN_COUNTRY_SLOT] = array_size(*national);
            record->slots[TOWN_BUCKET_SLOT] = bucket ? array_size(*bucket) : 0;
            indexed = dictionary_set(state->by_key, &record->key, &record);
            if (indexed) owned = array_append_carray(state->records, &record, 1);
            if (owned) listed = array_append_carray(country, &record, 1);
            if (listed) spatial = !bucket || array_append_carray(*bucket, &record, 1);
            if (spatial) grouped = array_append_carray(*national, &record, 1);
        }
        if (!grouped) {
            if (spatial && bucket) array_remove(*bucket, array_size(*bucket) - 1);
            if (listed) array_remove(country, array_size(country) - 1);
            if (owned) array_remove(state->records, array_size(state->records) - 1);
            if (indexed) dictionary_remove(state->by_key, &record->key);
            release_record(record); goto rollback;
        }
        state->text_bytes += record->bytes;
        ++added;
    }
    lab_dom_release(mark);
    if (added) {
        array_sort(country, town_compare);
        for (size_t i = 0; i < array_size(country); ++i)
            (*(town_record_t **)array_get(country, i))->slots[TOWN_GROUP_SLOT] = i;
    }
    return added;

rollback:
    lab_dom_release(mark);
    while (added > 0) {
        --added;
        discard_record(*(town_record_t **)array_get(country, array_size(country) - 1));
    }
    release_empty_group(code);
    if (!array_size(*national)) discard_country(root);
    return -1;
}

typedef struct _country_town_t {
    town_record_t *record;
    bool preferred;
} country_town_t;

static int country_town_compare(const void *left, const void *right)
{
    const country_town_t *a = left, *b = right;
    if (a->preferred != b->preferred) return b->preferred - a->preferred;
    unsigned mark = lab_dom_mark();
    int result = lab_dom_locale_compare(text_value(a->record->fields[0]), text_value(b->record->fields[0]), false);
    lab_dom_release(mark);
    return result;
}

/* Only this country's directly indexed members are materialised; a dictionary merges repeated selection keys. */
static jsref country_town_snapshot(array_t *family)
{
    dictionary_t *unique = dictionary_create(sizeof(char *), sizeof(country_town_t), text_hash, text_compare,
                                             NULL, NULL, NULL, NULL, NULL);
    array_t *ordered = array_create(sizeof(country_town_t), NULL, NULL);
    bool ok = unique && ordered;
    for (size_t i = 0; ok && i < array_size(family); ++i) {
        town_record_t *record = *(town_record_t **)array_get(family, i);
        const char *key = record->fields[5];
        country_town_t entry = {.record = record};
        if (dictionary_get(unique, &key, &entry)) {
            if (strlen(entry.record->code) == 2 && strlen(record->code) > 2) entry.record = record;
        }
        entry.preferred = entry.preferred || (strlen(record->code) == 2 && record->preferred);
        ok = dictionary_set(unique, &key, &entry);
    }
    for (size_t i = 0; ok && i < dictionary_size(unique); ++i)
        ok = array_append_carray(ordered, dictionary_get_value(unique, i), 1);
    if (ok) array_sort(ordered, country_town_compare);
    jsref result = ok ? lab_dom_object(LAB_VALUE_ARRAY) : 0;
    unsigned mark = lab_dom_mark();
    for (size_t i = 0; ok && i < array_size(ordered); ++i) {
        const country_town_t *entry = array_get(ordered, i);
        jsref town = town_value(entry->record, false);
        lab_dom_set(town, "default", lab_dom_scalar(LAB_VALUE_BOOLEAN, entry->preferred));
        lab_dom_push(result, town);
        lab_dom_release(mark);
    }
    dictionary_destroy(unique);
    array_destroy(ordered);
    return result;
}

/* Build an independent snapshot within the caller's bridge scope. */
static jsref lab_town_store_snapshot_owned(jsref code_value)
{
    town_store_t *state = town_store();
    char code[7];
    const char *key = code;
    array_t *country = NULL;
    if (!copy_code(code_value, code)) return 0;
    if (state->countries[country_index(code)]) country_touch(country_index(code));
    if (strlen(code) == 2) {
        array_t *family = state->countries[(code[0] - 'A') * 26 + code[1] - 'A'];
        if (family) return country_town_snapshot(family);
    }
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
    if (found) country_touch(country_index(code));
    free(value); free(key);
    lab_dom_return(found ? town_value(record, true) : 0);
}

/* Fetch a bounded set of direct-indexed spatial buckets in one bridge call. */
static void lab_town_store_region_owned(double west, double south, double east, double north)
{
    town_store_t *state = town_store();
    bool touched[26 * 26] = {0};
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
                town_record_t *record = *(town_record_t **)array_get(bucket, i);
                unsigned root = country_index(record->code);
                if (!touched[root]) { country_touch(root); touched[root] = true; }
                lab_dom_push(result, town_value(record, true));
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
    for (unsigned i = 0; i < 26 * 26; ++i) { array_destroy(state->countries[i]); state->countries[i] = NULL; }
    if (state->records) for (size_t i = 0; i < array_size(state->records); ++i)
        release_record(*(town_record_t **)array_get(state->records, i));
    array_destroy(state->records); state->records = NULL; state->text_bytes = 0;
    memset(state->linked, 0, sizeof state->linked); memset(state->pinned, 0, sizeof state->pinned);
    memset(state->discarded, 0, sizeof state->discarded); state->evicted_count = state->oldest = state->newest = 0;
}

static void town_store_policy_dispatch(void *context)
{
    town_store_t *state = town_store();
    jsref request = *(jsref *)context, pins = lab_dom_get(request, "pins");
    if (lab_dom_type(pins) == LAB_VALUE_ARRAY) {
        if (lab_dom_count(pins) > 8) { lab_dom_return(0); return; }
        bool pinned[26 * 26] = {0};
        for (unsigned i = 0; i < lab_dom_count(pins); ++i) {
            char code[7];
            if (!copy_code(lab_dom_item(pins, i), code)) { lab_dom_return(0); return; }
            pinned[country_index(code)] = true;
        }
        memcpy(state->pinned, pinned, sizeof pinned);
    }
    jsref result = lab_dom_object(LAB_VALUE_OBJECT), evicted = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < state->evicted_count; ++i) {
        unsigned index = state->evicted[i];
        char code[] = {(char)('A' + index / 26), (char)('A' + index % 26), 0};
        lab_dom_push(evicted, lab_dom_string(code)); state->discarded[index] = false;
    }
    state->evicted_count = 0;
    lab_dom_set(result, "evicted", evicted);
    lab_dom_set(result, "bytes", lab_dom_numeric(state->text_bytes));
    lab_dom_set(result, "limit", lab_dom_numeric(state->text_limit));
    lab_dom_set(result, "count", lab_dom_numeric(array_size(state->records)));
    lab_dom_return(result);
}

/* Update active-country pins and acknowledge discarded catalogue identities within one native operation. */
void lab_town_store_policy(jsref request)
{
    if (!lab_serial_dispatch(&town_store()->serial, town_store_policy_dispatch, &request)) lab_dom_return(0);
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
