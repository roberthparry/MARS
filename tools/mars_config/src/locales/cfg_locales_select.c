/**
 * @file cfg_locales_select.c
 * @brief Native territory defaults and supported-language selection.
 *
 * Uses indexed resolved locale lookups, CLDR official status and population
 * fallback ordering. Explicit supplements stay separate from CLDR data. The
 * final territory's assignment deliberately wins for shared language labels.
 */
#include <stdio.h>
#include <stdlib.h>

#include "cfg_locales_internal.h"

static const string_t *cfg_locales_resolve(cfg_locales_state *state, const string_t *request, bool *ok)
{
    const json_t *value = request ? json_object_get(cfg_locales_get(state->raw, "resolutions"), request) : NULL;
    *ok = *ok && value && (json_type(value) == JSON_STRING || json_type(value) == JSON_NULL);
    if (!*ok)
        string_fprintf(stderr, "Missing or invalid pinned locale resolution: %S.\n", request);
    return json_string_value(value);
}

static bool cfg_locales_activate(cfg_locales_state *state, const string_t *id)
{
    const string_t *language = cfg_locales_text(json_object_get(state->records, id), "language");
    return language && cfg_locales_set(state->active, id, language);
}

static const string_t *cfg_locales_for_language(cfg_locales_state *state, const string_t *language,
                                                const string_t *country, bool *ok)
{
    string_t *request = string_sprintf("%s_%.2s", string_c_str(language), string_c_str(country));
    const string_t *result = cfg_locales_resolve(state, request, ok);
    string_free(request);
    if (*ok && !result)
        result = cfg_locales_resolve(state, language, ok);
    return result;
}

static long double cfg_locales_population(const json_t *entry)
{
    const string_t *text = json_number_text(cfg_locales_get(entry, "population_percent"));
    string_view_t view = string_view_all(text);
    long double value = 0, scale = 1;
    bool fractional = false;
    for (size_t i = 0; text && i < string_byte_length(text); ++i) {
        unsigned char ch = 0;
        if (!string_view_peek_ascii(view, i, &ch))
            return -1;
        if (ch == '.') {
            fractional = true;
            continue;
        }
        if (ch == 'e' || ch == 'E') {
            unsigned exponent = 0;
            bool negative = false;
            for (++i; i < string_byte_length(text); ++i) {
                if (!string_view_peek_ascii(view, i, &ch))
                    return -1;
                if (ch == '-' || ch == '+')
                    negative = ch == '-';
                else if (ch >= '0' && ch <= '9' && exponent < 300)
                    exponent = exponent * 10 + ch - '0';
                else
                    return -1;
            }
            if (exponent > 300)
                return -1;
            for (unsigned j = 0; j < exponent; ++j)
                value *= negative ? 0.1L : 10.0L;
            return value;
        }
        if (ch < '0' || ch > '9')
            return -1;
        if (fractional) {
            scale *= 0.1L;
            value += (ch - '0') * scale;
        } else
            value = value * 10 + ch - '0';
    }
    return value;
}

typedef struct cfg_locales_candidate {
    const string_t *language;
    bool official;
    long double population;
} cfg_locales_candidate;

static int cfg_locales_rank(const void *a, const void *b)
{
    const cfg_locales_candidate *left = a, *right = b;
    if (left->official != right->official)
        return left->official ? -1 : 1;
    if (left->population != right->population)
        return left->population > right->population ? -1 : 1;
    return string_compare(left->language, right->language);
}

static const string_t *cfg_locales_default(cfg_locales_state *state, const string_t *country, bool *ok)
{
    string_t *request = string_sprintf("und_%s", string_c_str(country));
    const string_t *id = cfg_locales_resolve(state, request, ok);
    string_free(request);
    if (id || !*ok)
        return id;
    const json_t *languages = json_object_get(cfg_locales_get(state->raw, "territories"), country);
    size_t count = json_object_size(languages);
    cfg_locales_candidate *candidates = calloc(count + 1, sizeof(*candidates));
    *ok = candidates != NULL;
    for (size_t i = 0; i < count && *ok; ++i) {
        const json_t *entry = json_object_value_at(languages, i);
        candidates[i] = (cfg_locales_candidate){
            json_object_key_at(languages, i), cfg_locales_equal(cfg_locales_text(entry, "official_status"), "official"),
            cfg_locales_population(entry)};
        *ok = candidates[i].population >= 0;
    }
    if (*ok)
        qsort(candidates, count, sizeof(*candidates), cfg_locales_rank);
    for (size_t i = 0; i < count && *ok && !id; ++i)
        id = cfg_locales_for_language(state, candidates[i].language, country, ok);
    free(candidates);
    if (*ok && !id)
        id = cfg_locales_text(cfg_locales_get(state->raw, "resolutions"), "en_GB");
    *ok = *ok && id;
    return id;
}

static json_t *cfg_locales_countries(const string_t *root)
{
    string_t *path = cfg_locales_path(root, "packaging/jurisdiction-db/mars_country_jurisdictions.sql");
    string_t *text = path ? cfg_locales_read(path) : NULL;
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    json_t *countries = cursor ? json_new_object() : NULL;
    bool ok = countries != NULL;
    while (ok && !string_cursor_done(cursor)) {
        string_pos_t position = string_cursor_position(cursor);
        unsigned char a = 0, b = 0;
        if (string_cursor_match_at(cursor, position, "('") && string_cursor_peek_ascii_at(cursor, position + 2, &a) &&
            a >= 'A' && a <= 'Z' && string_cursor_peek_ascii_at(cursor, position + 3, &b) && b >= 'A' && b <= 'Z' &&
            string_cursor_match_at(cursor, position + 4, "', 'country'")) {
            string_t *country = string_cursor_slice_between(position + 2, position + 4, cursor);
            ok = country && cfg_locales_set(countries, country, country);
            string_free(country);
        }
        ok = !string_cursor_next(cursor) && ok;
    }
    string_cursor_free(cursor);
    string_free(text);
    string_free(path);
    if (!ok || !json_object_size(countries)) {
        json_free(countries);
        countries = NULL;
    }
    return countries;
}

static bool cfg_locales_merge(json_t *out, const json_t *in, bool replace)
{
    bool ok = true;
    for (size_t i = 0; i < json_object_size(in) && ok; ++i) {
        const string_t *key = json_object_key_at(in, i);
        if (replace || !json_object_get(out, key))
            ok = json_object_set(out, key, json_object_value_at(in, i));
    }
    return ok;
}

static bool cfg_locales_choice(cfg_locales_state *state, const string_t *territory, const string_t *language,
                               const string_t *status)
{
    const json_t *supplement = json_object_get(cfg_locales_get(state->supp, "locales"), language);
    bool ok = status != NULL;
    const string_t *id = supplement ? language : cfg_locales_for_language(state, language, territory, &ok);
    const json_t *data = supplement ? supplement : json_object_get(state->records, id);
    const string_t *english = cfg_locales_text(data, supplement ? "english_name" : "english");
    if (!english || !string_byte_length(english))
        english = json_string_value(json_object_get(cfg_locales_get(state->raw, "english"), language));
    if (!english)
        english = language;
    const string_t *native = cfg_locales_text(data, supplement ? "native_name" : "native");
    if (!native || !string_byte_length(native))
        native = english;
    if (id)
        ok = ok && (supplement ? cfg_locales_set(state->active, id, language) : cfg_locales_activate(state, id));
    json_t *names = json_new_array(), *row = json_new_array();
    ok = ok && names && row && cfg_locales_push(names, english) && cfg_locales_push(names, native) &&
         json_object_set(state->names, language, names) && cfg_locales_push(row, territory) &&
         cfg_locales_push(row, language) && cfg_locales_push(row, id) && cfg_locales_push(row, status) &&
         json_array_append(state->choices, row);
    json_free(row);
    json_free(names);
    return ok;
}

static bool cfg_locales_territory(cfg_locales_state *state, const string_t *territory)
{
    const string_t *source = territory;
    string_t *country = NULL;
    if (!json_object_get(state->selected, territory)) {
        country = string_sprintf("%.2s", string_c_str(territory));
        source = country;
    }
    const json_t *raw = json_object_get(cfg_locales_get(state->raw, "territories"), source);
    json_t *official = json_new_object();
    bool ok = source && official;
    for (size_t i = 0; i < json_object_size(raw) && ok; ++i) {
        const string_t *status = cfg_locales_text(json_object_value_at(raw, i), "official_status");
        if (status && string_byte_length(status))
            ok = cfg_locales_set(official, json_object_key_at(raw, i), status);
    }
    const json_t *forced = json_object_get(cfg_locales_get(state->supp, "forced_official"), territory);
    if (forced) {
        json_free(official);
        official = json_clone(forced);
        ok = ok && official;
    }
    ok = ok &&
         cfg_locales_merge(official, json_object_get(cfg_locales_get(state->supp, "extra_official"), territory), true);
    if (ok && !json_object_size(official)) {
        const string_t *id = json_string_value(json_object_get(state->selected, source));
        const string_t *language = json_string_value(json_object_get(state->active, id));
        string_t *status = string_new_with("default");
        ok = language && status && cfg_locales_set(official, language, status);
        string_free(status);
    }
    json_t *supplements = json_new_object();
    const json_t *options = cfg_locales_get(state->supp, "options");
    ok = ok && supplements && cfg_locales_merge(supplements, json_object_get(options, source), true) &&
         cfg_locales_merge(supplements, json_object_get(options, territory), true) &&
         cfg_locales_merge(official, supplements, false);
    const string_t **languages = cfg_locales_keys(official);
    ok = ok && languages;
    for (size_t i = 0; i < json_object_size(official) && ok; ++i)
        ok = cfg_locales_choice(state, territory, languages[i],
                                json_string_value(json_object_get(official, languages[i])));
    free(languages);
    json_free(supplements);
    json_free(official);
    string_free(country);
    return ok;
}

/* Select in Python-compatible territory order; sorted language labels are emitted later. */
bool cfg_locales_select(cfg_locales_state *state, const string_t *root)
{
    state->selected = json_new_object();
    state->order = json_new_array();
    state->active = json_new_object();
    state->names = json_new_object();
    state->choices = json_new_array();
    json_t *countries = cfg_locales_countries(root);
    const string_t **keys = cfg_locales_keys(countries);
    string_t *initial = string_new_with("en_GB");
    bool ok = countries && keys && state->selected && state->order && state->active && state->names && state->choices &&
              initial && cfg_locales_activate(state, initial);
    if (!ok)
        fputs("Cannot initialise locale selection or read country source.\n", stderr);
    string_free(initial);
    for (size_t i = 0; i < json_object_size(countries) && ok; ++i) {
        const string_t *id = cfg_locales_default(state, keys[i], &ok);
        ok = ok && cfg_locales_activate(state, id) && cfg_locales_set(state->selected, keys[i], id) &&
             cfg_locales_push(state->order, keys[i]);
    }
    free(keys);
    json_free(countries);
    const json_t *defaults = cfg_locales_get(state->supp, "defaults");
    keys = cfg_locales_keys(defaults);
    ok = ok && keys;
    for (size_t i = 0; i < json_object_size(defaults) && ok; ++i) {
        const string_t *id = json_string_value(json_object_get(defaults, keys[i]));
        ok = id && cfg_locales_activate(state, id) &&
             (json_object_get(state->selected, keys[i]) || cfg_locales_push(state->order, keys[i])) &&
             cfg_locales_set(state->selected, keys[i], id);
    }
    free(keys);
    const json_t *options = cfg_locales_get(state->supp, "options");
    keys = cfg_locales_keys(options);
    ok = ok && keys;
    for (size_t i = 0; i < json_object_size(options) && ok; ++i)
        if (!json_object_get(state->selected, keys[i]))
            ok = cfg_locales_push(state->order, keys[i]);
    free(keys);
    for (size_t i = 0; i < json_array_size(state->order) && ok; ++i)
        ok = cfg_locales_territory(state, json_string_value(json_array_get(state->order, i)));
    return ok;
}
