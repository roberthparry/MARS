/**
 * @file lab_location_search.c
 * @brief Native projection and selection of bounded worldwide place-search results.
 *
 * Search results remain separate from worksheet controls until explicitly selected. Existing native town indexes
 * retain their original identities and metadata; country labels disambiguate the suggestions. The browser owns
 * request cancellation and timers, whilst C validates records, builds rows and applies the selected reference.
 * A per-response native dictionary identifies town names that also name a returned area or geographic alias;
 * those towns receive a jurisdiction qualifier without changing their identities or flags. Scoped handles never
 * survive a call, and no mathematics or browser-owned promise is interpreted here.
 */
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_location.h"
#include "lab_menu_index.h"
#include "lab_select.h"
#include "lab_town_store.h"

static node_jsref search_control(node_jsref select, const char *selector)
{
    node_jsref panel = lab_dom_closest(select, ".mode-panel");
    return panel ? lab_dom_query(panel, selector) : 0;
}

static size_t search_name_hash(const void *key)
{
    const unsigned char *text = (const unsigned char *)*(const char *const *)key;
    size_t hash = 2166136261u;
    while (*text) hash = (hash ^ *text++) * 16777619u;
    return hash;
}

static int search_name_compare(const void *left, const void *right)
{
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* Normalise through the browser's Unicode capability, then retain only owned native text for dictionary keys. */
static char *search_name_key(jsref value)
{
    value = lab_dom_normalize(value);
    unsigned length = lab_dom_length(value);
    if (length > 65536) return NULL;
    unsigned char *bytes = malloc(length * 3 + 1);
    if (!bytes) return NULL;
    int count = lab_dom_utf8_copy(value, bytes, length * 3);
    if (count >= 0) bytes[count] = 0;
    if (count < 0) { free(bytes); return NULL; }
    return (char *)bytes;
}

static void search_placeholder(node_jsref select)
{
    int index = lab_menu_index_key(select, lab_dom_string(""));
    if (index >= 0) {
        int option = lab_dom_item(lab_dom_get(select, "options"), (unsigned)index);
        lab_dom_set(option, "hidden", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        lab_dom_write(option, LAB_DOM_TEXT, "", lab_dom_string("Find a place anywhere…"));
    }
    lab_dom_write(select, LAB_DOM_VALUE, "", lab_dom_string(""));
}

/* Keep pending, empty and unavailable searches outside the actual observer state. */
void lab_location_search_status(node_jsref select, lab_location_search_state_t state)
{
    static const char *const messages[] = {
        [LAB_LOCATION_SEARCH_EMPTY] = "Type at least two characters",
        [LAB_LOCATION_SEARCH_PENDING] = "Searching all countries…",
        [LAB_LOCATION_SEARCH_NOT_FOUND] = "No matching places",
        [LAB_LOCATION_SEARCH_UNAVAILABLE] = "Place search is unavailable; please try again",
    };
    if (!select || (unsigned)state >= sizeof messages / sizeof *messages) return;
    lab_location_populate(select, lab_dom_object(LAB_VALUE_ARRAY), false);
    search_placeholder(select);
    int shell = lab_dom_closest(select, ".select-shell");
    lab_dom_write(lab_dom_query(shell, ".select-empty"), LAB_DOM_TEXT, "", lab_dom_string(messages[state]));
    lab_select_rebuild(select, true);
}

/* Render prepared results without moving the observer or changing the selected country. */
bool lab_location_search_results(node_jsref select, jsref response)
{
    int rows = lab_dom_get(response, "matches"), country = search_control(select, "select[id$='Jurisdiction']");
    if (!select || !country || !lab_dom_truth(lab_dom_get(response, "available")) ||
        lab_dom_type(rows) != LAB_VALUE_ARRAY || lab_dom_count(rows) > 25) return false;
    int labels = lab_dom_get(country, "labJurisdictionLabels"), prepared = lab_dom_object(LAB_VALUE_ARRAY);
    static const char *const fields[] = {
        "name", "latitude", "longitude", "elevation", "timezone", "detail", "country_code", "default", "jurisdiction"
    };
    dictionary_t *names = dictionary_create(sizeof(char *), sizeof(bool), search_name_hash, search_name_compare,
                                            NULL, NULL, NULL, NULL, NULL);
    char *keys[25] = {0};
    bool ok = names != NULL, present = true;
    unsigned mark = lab_dom_mark();
    /* Index each returned area name once; town disambiguation then uses native hash lookup. */
    for (unsigned i = 0; ok && i < lab_dom_count(rows); ++i) {
        int alias = lab_dom_get(lab_dom_item(rows, i), "matched_name");
        if (lab_dom_length(alias)) {
            keys[i] = search_name_key(alias);
            ok = keys[i] && dictionary_set(names, &keys[i], &present);
        }
        lab_dom_release(mark);
    }
    for (unsigned i = 0; ok && i < lab_dom_count(rows); ++i) {
        int row = lab_dom_item(rows, i), code = lab_dom_get(row, "jurisdiction"), value = lab_dom_get(row, "value");
        int batch = lab_dom_object(LAB_VALUE_ARRAY);
        if (lab_dom_type(code) != LAB_VALUE_STRING || !lab_dom_length(value) ||
            lab_menu_index_key(country, code) < 0) { ok = false; break; }
        lab_dom_push(batch, row);
        if (lab_town_store_add(code, batch) < 0) { ok = false; break; }
        int result = lab_dom_object(LAB_VALUE_OBJECT);
        for (unsigned field = 0; field < sizeof fields / sizeof *fields; ++field)
            lab_dom_set(result, fields[field], lab_dom_get(row, fields[field]));
        int root = lab_dom_slice(code, 0, 2), detail = lab_dom_key_get(labels, code);
        if (!lab_dom_length(detail)) detail = lab_dom_key_get(labels, root);
        if (!lab_dom_length(detail)) detail = root;
        int alias = lab_dom_get(row, "matched_name"), name = lab_location_town_text(lab_dom_get(row, "name"));
        if (lab_dom_length(alias)) {
            if (lab_dom_length(alias) > lab_dom_length(name) + 2 &&
                lab_dom_equal(lab_dom_slice(alias, 0, lab_dom_length(name) + 2), lab_dom_join(name, 0, ", ")))
                name = alias;
            else if (!lab_dom_equal(alias, name))
                name = lab_dom_join(lab_dom_join(alias, 0, " — "), name, "");
        } else if (dictionary_size(names)) {
            char *key = search_name_key(name);
            bool ambiguous = false;
            if (!key) { ok = false; break; }
            dictionary_get(names, &key, &ambiguous);
            free(key);
            if (ambiguous) {
                int parts = lab_dom_split(detail, " - ");
                int qualifier = lab_dom_item(parts, lab_dom_count(parts) - 1);
                name = lab_dom_join(lab_dom_join(name, 0, ", "), qualifier, "");
            }
        }
        lab_dom_set(result, "name", name);
        lab_dom_set(result, "value", lab_dom_join(lab_dom_join(code, 0, ":"), value, ""));
        lab_dom_set(result, "townValue", value);
        lab_dom_set(result, "searchDetail", detail);
        lab_dom_push(prepared, result);
        lab_dom_release(mark);
    }
    lab_dom_release(mark);
    dictionary_destroy(names);
    for (unsigned i = 0; i < 25; ++i) free(keys[i]);
    if (!ok) return false;
    lab_location_populate(select, prepared, false);
    search_placeholder(select);
    int shell = lab_dom_closest(select, ".select-shell");
    lab_dom_write(lab_dom_query(shell, ".select-empty"), LAB_DOM_TEXT, "", lab_dom_string("No matching places"));
    lab_select_rebuild(select, true);
    return true;
}

/* Apply reference identity only; the existing town-change workflow owns offsets, history and evaluation. */
bool lab_location_search_choose(node_jsref select)
{
    int option = lab_dom_selected(select), code = lab_dom_read(option, LAB_DOM_DATASET, "jurisdiction");
    int value = lab_dom_read(option, LAB_DOM_DATASET, "townValue");
    int country = search_control(select, "select[id$='Jurisdiction']");
    int town = search_control(select, "select[id$='Town']");
    int index = country ? lab_menu_index_key(country, code) : -1;
    if (!country || !town || !lab_dom_length(value) || index < 0) return false;
    int country_option = lab_dom_item(lab_dom_get(country, "options"), (unsigned)index);
    int group = lab_dom_read(country_option, LAB_DOM_DATASET, "countryParent");
    int rows = lab_town_store_snapshot(lab_dom_length(group) ? group : lab_dom_slice(code, 0, 2));
    if (lab_dom_type(rows) != LAB_VALUE_ARRAY) return false;
    lab_dom_write(country, LAB_DOM_VALUE, "", code);
    lab_location_populate(town, rows, false);
    if (lab_menu_index_key(town, value) < 0) return false;
    lab_dom_write(town, LAB_DOM_VALUE, "", value);
    lab_select_sync(country);
    lab_select_rebuild(town, true);
    lab_location_search_status(select, LAB_LOCATION_SEARCH_EMPTY);
    return true;
}
