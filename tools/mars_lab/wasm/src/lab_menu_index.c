/**
 * @file lab_menu_index.c
 * @brief Dictionary, coordinate and substring indexes for browser option lists.
 *
 * Keys use dictionary_t. Two sorted coordinate arrays permit binary range lookup
 * on the more selective axis, followed by exact tolerance checks. Sorted suffixes
 * preserve arbitrary substring matching, including short and Unicode queries.
 * Only matching and previously visible rows cross the bridge when filtering.
 * Indexes are bounded to 8 MiB each and 16 MiB together; allocation failures let
 * callers retain their safe, slower display path. No DOM handles are retained.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_forms.h"
#include "lab_menu_index.h"

typedef struct _menu_row_t {
    char *key, *text;
    double latitude, longitude;
    unsigned index, seen;
} menu_row_t;

typedef struct _menu_suffix_t { const char *text; unsigned row; } menu_suffix_t;

typedef struct _menu_index_t {
    unsigned version, count, epoch;
    unsigned coordinate_candidates, text_candidates;
    char *query;
    size_t bytes;
    dictionary_t *keys;
    array_t *rows, *latitude, *longitude, *suffixes, *visible;
} menu_index_t;

typedef struct _menu_store_t {
    lab_serial_t serial;
    dictionary_t *indexes;
    unsigned next_id;
    size_t retained_bytes;
} menu_store_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static menu_store_t *menu_store(void)
{
    static menu_store_t instance = {.serial = LAB_SERIAL_INITIALIZER};
    return &instance;
}

static double lab_menu_index_number_owned(jsref value);
static int lab_menu_index_key_owned(node_jsref select, jsref value);
static int lab_menu_index_coordinates_owned(node_jsref select, double latitude, double longitude);
static jsref lab_menu_index_filter_owned(node_jsref select, jsref query_value);
static void lab_menu_index_stats_owned(node_jsref select);
static void lab_menu_index_invalidate_owned(node_jsref select);
static void lab_menu_index_clear_owned(void);

static size_t id_hash(const void *key) { return *(const unsigned *)key; }
static int id_compare(const void *a, const void *b)
{
    unsigned left = *(const unsigned *)a, right = *(const unsigned *)b;
    return (left > right) - (left < right);
}

static size_t key_hash(const void *key)
{
    const unsigned char *p = (const unsigned char *)*(const char *const *)key;
    size_t hash = 2166136261u;
    while (*p) hash = (hash ^ *p++) * 16777619u;
    return hash;
}

static int key_compare(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int suffix_compare(const void *a, const void *b)
{
    return strcmp(((const menu_suffix_t *)a)->text, ((const menu_suffix_t *)b)->text);
}

static int latitude_compare(const void *a, const void *b)
{
    double left = (*(menu_row_t *const *)a)->latitude, right = (*(menu_row_t *const *)b)->latitude;
    return (left > right) - (left < right);
}

static int longitude_compare(const void *a, const void *b)
{
    double left = (*(menu_row_t *const *)a)->longitude, right = (*(menu_row_t *const *)b)->longitude;
    return (left > right) - (left < right);
}

static char *copy_text(jsref value)
{
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

/* Preserve rejection of empty, malformed and non-finite coordinate strings. */
static double lab_menu_index_number_owned(jsref value)
{
    if (lab_dom_type(value) == LAB_VALUE_NUMBER) return lab_dom_to_number(value);
    jsref input = lab_dom_create("input");
    lab_dom_write(input, LAB_DOM_ATTRIBUTE, "type", lab_dom_string("number"));
    lab_dom_write(input, LAB_DOM_VALUE, "", lab_dom_clean(value, LAB_DOM_CLEAN_TRIM));
    return lab_dom_to_number(lab_dom_get(input, "valueAsNumber"));
}

static void destroy_index(menu_index_t *index)
{
    if (!index) return;
    dictionary_destroy(index->keys);
    if (index->rows) for (size_t i = 0; i < array_size(index->rows); ++i) {
        menu_row_t *row = *(menu_row_t **)array_get(index->rows, i);
        free(row->key); free(row->text); free(row);
    }
    array_destroy(index->rows); array_destroy(index->latitude); array_destroy(index->longitude);
    array_destroy(index->suffixes); array_destroy(index->visible);
    free(index->query);
    free(index);
}

/* Share identical searchable text between the indexed and bounded fallback paths. */
jsref lab_menu_index_search_text(node_jsref option, bool details)
{
    jsref text = lab_dom_join(lab_dom_read(option, LAB_DOM_TEXT, ""), lab_dom_string(" "), "");
    text = lab_dom_join(text, lab_dom_read(option, LAB_DOM_DATASET, "latitude"), " ");
    text = lab_dom_join(text, lab_dom_read(option, LAB_DOM_DATASET, "longitude"), " ");
    text = lab_dom_join(text, details ? lab_dom_read(option, LAB_DOM_DATASET, "detail") : 0, " ");
    text = lab_dom_join(text, lab_dom_read(option, LAB_DOM_VALUE, ""), "");
    if (lab_dom_truth(lab_dom_read(option, LAB_DOM_DATASET, "countryOption"))) {
        char code[7] = {0};
        int length = lab_dom_utf8_copy(lab_dom_read(option, LAB_DOM_DATASET, "countryCode"),
                                       (unsigned char *)code, sizeof code - 1);
        if (length >= 0 && (!strcmp(code, "GB") || !strcmp(code, "GB-ENG") ||
                            !strcmp(code, "GB-WLS") || !strcmp(code, "GB-SCT") || !strcmp(code, "GB-NIR")))
            text = lab_dom_join(text, lab_dom_string("United Kingdom UK"), " ");
    }
    return lab_dom_normalize(text);
}

static menu_index_t *build_index(node_jsref select, unsigned version)
{
    menu_store_t *state = menu_store();
    menu_index_t *index = calloc(1, sizeof *index);
    if (!index) return NULL;
    index->version = version;
    index->bytes = 4096; /* Fixed containers, query cache and allocator overhead. */
    if (state->retained_bytes + index->bytes > 16 * 1048576) { free(index); return NULL; }
    index->keys = dictionary_create(sizeof(char *), sizeof(unsigned), key_hash, key_compare,
                                    NULL, NULL, NULL, NULL, NULL);
    index->rows = array_create(sizeof(menu_row_t *), NULL, NULL);
    index->latitude = array_create(sizeof(menu_row_t *), NULL, NULL);
    index->longitude = array_create(sizeof(menu_row_t *), NULL, NULL);
    index->suffixes = array_create(sizeof(menu_suffix_t), NULL, NULL);
    index->visible = array_create(sizeof(unsigned), NULL, NULL);
    if (!index->keys || !index->rows || !index->latitude || !index->longitude || !index->suffixes || !index->visible)
        goto failed;
    jsref options = lab_dom_get(select, "options");
    jsref shell = lab_dom_closest(select, ".select-shell");
    bool searchable = shell && lab_dom_query(shell, ".select-search");
    index->count = lab_dom_count(options);
    for (unsigned i = 0; i < index->count; ++i) {
        unsigned mark = lab_dom_mark();
        jsref option = lab_dom_item(options, i), value = lab_dom_read(option, LAB_DOM_VALUE, "");
        jsref text = searchable ? lab_menu_index_search_text(option,
            lab_dom_truth(lab_dom_get(select, "labNativeMenuDetails"))) : lab_dom_string("");
        menu_row_t *row = calloc(1, sizeof *row);
        if (!row) { lab_dom_release(mark); goto failed; }
        row->index = i;
        row->key = copy_text(value);
        row->text = copy_text(text);
        row->latitude = lab_menu_index_number_owned(lab_dom_read(option, LAB_DOM_DATASET, "latitude"));
        row->longitude = lab_menu_index_number_owned(lab_dom_read(option, LAB_DOM_DATASET, "longitude"));
        lab_dom_release(mark);
        if (!row->key || !row->text || !array_append_carray(index->rows, &row, 1)) {
            free(row->key); free(row->text); free(row); goto failed;
        }
        /* Include spare array capacity and dictionary bookkeeping in the admission estimate. */
        index->bytes += 256 + strlen(row->key) + 1 + (strlen(row->text) + 1) * (1 + 2 * sizeof(menu_suffix_t));
        if (strlen(row->text) > 1024 || index->bytes > 8 * 1048576 || state->retained_bytes + index->bytes > 16 * 1048576)
            goto failed;
        unsigned existing;
        if (!dictionary_get(index->keys, &row->key, &existing) && !dictionary_set(index->keys, &row->key, &i)) goto failed;
        if (isfinite(row->latitude) && isfinite(row->longitude)) {
            if (!array_append_carray(index->latitude, &row, 1) || !array_append_carray(index->longitude, &row, 1)) goto failed;
        }
        for (const char *p = row->text; *p; ++p) {
            menu_suffix_t suffix = {p, i};
            if (!array_append_carray(index->suffixes, &suffix, 1)) goto failed;
        }
        if (!array_append_carray(index->visible, &i, 1)) goto failed;
    }
    array_sort(index->latitude, latitude_compare);
    array_sort(index->longitude, longitude_compare);
    array_sort(index->suffixes, suffix_compare);
    return index;
failed:
    destroy_index(index);
    return NULL;
}

static menu_index_t *menu_index(node_jsref select)
{
    menu_store_t *state = menu_store();
    if (!select) return NULL;
    unsigned version = lab_dom_option_version(select);
    unsigned id = (unsigned)lab_dom_number(lab_dom_get(select, "labNativeMenuIndex"));
    menu_index_t *index = NULL;
    if (state->indexes && id) dictionary_get(state->indexes, &id, &index);
    if (index && index->version == version) return index;
    if (index) {
        dictionary_remove(state->indexes, &id);
        state->retained_bytes -= index->bytes;
        destroy_index(index);
    }
    index = build_index(select, version);
    if (!index) return NULL;
    if (!state->indexes) state->indexes = dictionary_create(sizeof(unsigned), sizeof(menu_index_t *), id_hash, id_compare,
                                              NULL, NULL, NULL, NULL, NULL);
    if (!id) {
        if (state->next_id == UINT_MAX) { destroy_index(index); return NULL; }
        id = ++state->next_id;
    }
    if (!state->indexes || !dictionary_set(state->indexes, &id, &index)) { destroy_index(index); return NULL; }
    state->retained_bytes += index->bytes;
    lab_dom_set(select, "labNativeMenuIndex", lab_dom_numeric(id));
    return index;
}

/* Look up the first option with an exact selection key. */
static int lab_menu_index_key_owned(node_jsref select, jsref value)
{
    menu_index_t *index = menu_index(select);
    if (!index) return -2;
    char *key = copy_text(value);
    unsigned row = 0;
    bool found = key && dictionary_get(index->keys, &key, &row);
    free(key);
    return found ? (int)row : -1;
}

static size_t coordinate_bound(array_t *items, double value, bool longitude, bool upper)
{
    size_t low = 0, high = array_size(items);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        menu_row_t *row = *(menu_row_t **)array_get(items, middle);
        double coordinate = longitude ? row->longitude : row->latitude;
        if (coordinate < value || (upper && coordinate == value)) low = middle + 1;
        else high = middle;
    }
    return low;
}

/* Binary-search both axes and inspect only the smaller tolerance range. */
static int lab_menu_index_coordinates_owned(node_jsref select, double latitude, double longitude)
{
    if (!isfinite(latitude) || !isfinite(longitude)) return -1;
    menu_index_t *index = menu_index(select);
    if (!index) return -2;
    /* Outward rounding ensures subtraction's final tolerance check cannot be missed at a boundary. */
    index->coordinate_candidates = 0;
    size_t a = coordinate_bound(index->latitude, nextafter(latitude - 0.000001, -INFINITY), false, false);
    size_t b = coordinate_bound(index->latitude, nextafter(latitude + 0.000001, INFINITY), false, true);
    size_t c = coordinate_bound(index->longitude, nextafter(longitude - 0.000001, -INFINITY), true, false);
    size_t d = coordinate_bound(index->longitude, nextafter(longitude + 0.000001, INFINITY), true, true);
    array_t *items = index->latitude;
    if (d - c < b - a) { items = index->longitude; a = c; b = d; }
    unsigned best = UINT_MAX;
    for (size_t i = a; i < b; ++i) {
        ++index->coordinate_candidates;
        menu_row_t *row = *(menu_row_t **)array_get(items, i);
        if (row->index < best && lab_forms_nearly_equal(row->latitude, latitude, 0.000001) &&
            lab_forms_nearly_equal(row->longitude, longitude, 0.000001)) best = row->index;
        if (!best) break;
    }
    return best == UINT_MAX ? -1 : (int)best;
}

/* Return filter differences without scanning unrelated option text or mutating the DOM. */
static jsref lab_menu_index_filter_owned(node_jsref select, jsref query_value)
{
    menu_index_t *index = menu_index(select);
    if (!index) return 0;
    if (index->epoch == UINT_MAX) { lab_menu_index_invalidate_owned(select); return 0; }
    char *query = copy_text(query_value);
    array_t *visible = array_create(sizeof(unsigned), NULL, NULL);
    if (!query || !visible) {
        free(query); array_destroy(visible); lab_menu_index_invalidate_owned(select); return 0;
    }
    if (index->query && !strcmp(index->query, query)) {
        free(query); array_destroy(visible);
        index->text_candidates = 0;
        jsref unchanged = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(unchanged, "unchanged", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        return unchanged;
    }
    ++index->epoch;
    index->text_candidates = 0;
    size_t length = strlen(query), low = 0, high = array_size(index->suffixes);
    if (length) while (low < high) {
        size_t middle = low + (high - low) / 2;
        menu_suffix_t *suffix = array_get(index->suffixes, middle);
        if (strcmp(suffix->text, query) < 0) low = middle + 1;
        else high = middle;
    }
    size_t count = length ? array_size(index->suffixes) : index->count;
    for (size_t i = length ? low : 0; i < count; ++i) {
        menu_suffix_t *suffix = length ? array_get(index->suffixes, i) : NULL;
        if (suffix && strncmp(suffix->text, query, length)) break;
        ++index->text_candidates;
        unsigned row_number = suffix ? suffix->row : (unsigned)i;
        menu_row_t *row = *(menu_row_t **)array_get(index->rows, row_number);
        if (row->seen == index->epoch) continue;
        row->seen = index->epoch;
        if (!array_append_carray(visible, &row_number, 1)) {
            free(query); array_destroy(visible); lab_menu_index_invalidate_owned(select); return 0;
        }
    }
    free(index->query);
    index->query = length <= 1024 ? query : NULL;
    if (!index->query) free(query);
    if (length) array_sort(visible, id_compare);
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    jsref show = lab_dom_object(LAB_VALUE_ARRAY), hide = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (size_t i = 0; i < array_size(visible); ++i) {
        lab_dom_push(show, lab_dom_numeric(*(unsigned *)array_get(visible, i)));
        lab_dom_release(mark);
    }
    for (size_t i = 0; i < array_size(index->visible); ++i) {
        unsigned row_number = *(unsigned *)array_get(index->visible, i);
        menu_row_t *row = *(menu_row_t **)array_get(index->rows, row_number);
        if (row->seen != index->epoch) lab_dom_push(hide, lab_dom_numeric(row_number));
        lab_dom_release(mark);
    }
    array_destroy(index->visible);
    index->visible = visible;
    lab_dom_set(result, "visible", show); lab_dom_set(result, "hidden", hide);
    return result;
}

/* Query-cost counters support deterministic regression checks rather than timing thresholds. */
static void lab_menu_index_stats_owned(node_jsref select)
{
    menu_index_t *index = menu_index(select);
    if (!index) { lab_dom_return(0); return; }
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "rows", lab_dom_numeric(index->count));
    lab_dom_set(result, "coordinateCandidates", lab_dom_numeric(index->coordinate_candidates));
    lab_dom_set(result, "textCandidates", lab_dom_numeric(index->text_candidates));
    lab_dom_return(result);
}

/* Rendered rows start visible after rebuilding, even when option data did not change. */
static void lab_menu_index_invalidate_owned(node_jsref select)
{
    menu_store_t *state = menu_store();
    unsigned id = (unsigned)lab_dom_number(lab_dom_get(select, "labNativeMenuIndex"));
    menu_index_t *index = NULL;
    if (state->indexes && id && dictionary_get(state->indexes, &id, &index)) {
        dictionary_remove(state->indexes, &id);
        state->retained_bytes -= index->bytes;
        destroy_index(index);
    }
}

/* Release page-owned native data; browser observer lifetimes belong to the bridge. */
static void lab_menu_index_clear_owned(void)
{
    menu_store_t *state = menu_store();
    if (state->indexes) for (size_t i = 0; i < dictionary_size(state->indexes); ++i)
        destroy_index(*(menu_index_t *const *)dictionary_get_value(state->indexes, i));
    dictionary_destroy(state->indexes); state->indexes = NULL; state->retained_bytes = 0;
}

typedef struct _menu_index_number_request_t {
    jsref value;
    double result;
} menu_index_number_request_t;

static void menu_index_number_dispatch(void *context)
{
    menu_index_number_request_t *request = context;
    request->result = lab_menu_index_number_owned(request->value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
double lab_menu_index_number(jsref value)
{
    menu_store_t *state = menu_store();
    menu_index_number_request_t request = {.value = value, .result = NAN};
    lab_serial_dispatch(&state->serial, menu_index_number_dispatch, &request);
    return request.result;
}

typedef struct _menu_index_key_request_t {
    node_jsref select;
    jsref value;
    int result;
} menu_index_key_request_t;

static void menu_index_key_dispatch(void *context)
{
    menu_index_key_request_t *request = context;
    request->result = lab_menu_index_key_owned(request->select, request->value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
int lab_menu_index_key(node_jsref select, jsref value)
{
    menu_store_t *state = menu_store();
    menu_index_key_request_t request = {.select = select, .value = value, .result = -2};
    lab_serial_dispatch(&state->serial, menu_index_key_dispatch, &request);
    return request.result;
}

typedef struct _menu_index_coordinates_request_t {
    node_jsref select;
    double latitude;
    double longitude;
    int result;
} menu_index_coordinates_request_t;

static void menu_index_coordinates_dispatch(void *context)
{
    menu_index_coordinates_request_t *request = context;
    request->result = lab_menu_index_coordinates_owned(request->select, request->latitude, request->longitude);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
int lab_menu_index_coordinates(node_jsref select, double latitude, double longitude)
{
    menu_store_t *state = menu_store();
    menu_index_coordinates_request_t request = {.select = select, .latitude = latitude, .longitude = longitude, .result = -2};
    lab_serial_dispatch(&state->serial, menu_index_coordinates_dispatch, &request);
    return request.result;
}

typedef struct _menu_index_filter_request_t {
    node_jsref select;
    jsref query_value;
    jsref result;
} menu_index_filter_request_t;

static void menu_index_filter_dispatch(void *context)
{
    menu_index_filter_request_t *request = context;
    request->result = lab_menu_index_filter_owned(request->select, request->query_value);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
jsref lab_menu_index_filter(node_jsref select, jsref query_value)
{
    menu_store_t *state = menu_store();
    menu_index_filter_request_t request = {.select = select, .query_value = query_value, .result = 0};
    lab_serial_dispatch(&state->serial, menu_index_filter_dispatch, &request);
    return request.result;
}

typedef struct _menu_index_stats_request_t {
    node_jsref select;
} menu_index_stats_request_t;

static void menu_index_stats_dispatch(void *context)
{
    menu_index_stats_request_t *request = context;
    lab_menu_index_stats_owned(request->select);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_menu_index_stats(node_jsref select)
{
    menu_store_t *state = menu_store();
    menu_index_stats_request_t request = {.select = select};
    if (!lab_serial_dispatch(&state->serial, menu_index_stats_dispatch, &request)) lab_dom_return(0);
}

typedef struct _menu_index_invalidate_request_t {
    node_jsref select;
} menu_index_invalidate_request_t;

static void menu_index_invalidate_dispatch(void *context)
{
    menu_index_invalidate_request_t *request = context;
    lab_menu_index_invalidate_owned(request->select);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_menu_index_invalidate(node_jsref select)
{
    menu_store_t *state = menu_store();
    menu_index_invalidate_request_t request = {.select = select};
    if (!lab_serial_dispatch(&state->serial, menu_index_invalidate_dispatch, &request)) lab_dom_return(0);
}

static void menu_index_clear_dispatch(void *context)
{
    (void)context;
    lab_menu_index_clear_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_menu_index_clear(void)
{
    menu_store_t *state = menu_store();
    if (!lab_serial_dispatch(&state->serial, menu_index_clear_dispatch, NULL)) lab_dom_return(0);
}
