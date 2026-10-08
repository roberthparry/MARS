/**
 * @file cfg_locales_tables.c
 * @brief Derive shared calendar name sets and stable date-pattern identifiers.
 *
 * Hash-indexed exact-byte signatures deduplicate pair arrays. Sorted locale
 * traversal determines first owners; final name sets sort by owner, matching
 * the historical generator without storing preselected output rows as input.
 */
#include <stdlib.h>

#include "cfg_calendar.h"
#include "cfg_locales_internal.h"

static bool cfg_locales_row(json_t *table, size_t count, const string_t *const fields[])
{
    json_t *row = json_new_array();
    bool ok = row != NULL;
    for (size_t i = 0; i < count && ok; ++i)
        ok = cfg_locales_push(row, fields[i]);
    ok = ok && json_array_append(table, row);
    json_free(row);
    return ok;
}

static bool cfg_locales_pair_array(const json_t *pairs, size_t count)
{
    if (json_type(pairs) != JSON_ARRAY || json_array_size(pairs) != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const json_t *pair = json_array_get(pairs, i);
        if (json_array_size(pair) != 2)
            return false;
        for (size_t j = 0; j < 2; ++j) {
            const string_t *text = json_string_value(json_array_get(pair, j));
            if (!text || !string_byte_length(text))
                return false;
        }
    }
    return true;
}

static json_t *cfg_locales_labels(cfg_locales_state *state, const string_t *id, unsigned context)
{
    const json_t *supp = json_object_get(cfg_locales_get(state->supp, "locales"), id);
    const json_t *data = supp ? supp : json_object_get(state->records, id);
    const char *key = context == 2 ? "weekdays" : context == 1 && !supp ? "standalone" : "months";
    const json_t *source = cfg_locales_get(data, key);
    if (!cfg_locales_pair_array(source, context == 2 ? 7 : 12))
        return NULL;
    if (context != 2 || supp)
        return json_clone(source);
    const string_t *language = cfg_locales_text(data, "language");
    const string_t *prefix =
        json_string_value(json_object_get(cfg_locales_get(state->supp, "weekday_prefixes"), language));
    if (!prefix || !string_byte_length(prefix))
        return json_clone(source);
    json_t *labels = json_new_array();
    bool ok = labels != NULL;
    for (size_t i = 0; i < 7 && ok; ++i) {
        const json_t *pair = json_array_get(source, i);
        const string_t *name = json_string_value(json_array_get(pair, 0));
        const string_t *short_name = json_string_value(json_array_get(pair, 1));
        size_t skip = string_view_starts_with(string_view_all(name), prefix, false) ? string_byte_length(prefix) : 0;
        string_t *trimmed = string_new();
        ok = trimmed && !string_append_utf8_exact(trimmed, string_c_str(name) + skip, string_byte_length(name) - skip);
        const string_t *fields[] = {trimmed, short_name};
        ok = ok && cfg_locales_row(labels, 2, fields);
        string_free(trimmed);
    }
    if (!ok) {
        json_free(labels);
        labels = NULL;
    }
    return labels;
}

static const string_t *cfg_locales_share(cfg_locales_state *state, const string_t *id, const string_t *language,
                                         unsigned context, json_t *owners, json_t *sets)
{
    json_t *values = cfg_locales_labels(state, id, context);
    json_t *base = cfg_locales_labels(state, language, context);
    string_t *signature = values ? cfg_locales_signature(values) : NULL;
    string_t *base_signature = base ? cfg_locales_signature(base) : NULL;
    bool ok = signature && base_signature;
    const string_t *owner = ok ? json_string_value(json_object_get(owners, signature)) : NULL;
    if (ok && !owner) {
        const string_t *candidate = !string_compare(signature, base_signature) ? language : id;
        string_t *name = string_sprintf("%s%s", string_c_str(candidate), context == 1 ? "_standalone" : "");
        ok = name && cfg_locales_set(owners, signature, name);
        if (ok && !json_object_get(sets, name))
            ok = json_object_set(sets, name, values);
        string_free(name);
        owner = ok ? json_string_value(json_object_get(owners, signature)) : NULL;
    }
    json_free(values);
    json_free(base);
    string_free(signature);
    string_free(base_signature);
    return ok ? owner : NULL;
}

static bool cfg_locales_names_tables(cfg_locales_state *state, const string_t *const *ids)
{
    json_t *month_owners = json_new_object(), *day_owners = json_new_object();
    json_t *month_sets = json_new_object(), *day_sets = json_new_object();
    bool ok = month_owners && day_owners && month_sets && day_sets;
    for (size_t i = 0; i < json_object_size(state->active) && ok; ++i) {
        const string_t *language = json_string_value(json_object_get(state->active, ids[i]));
        const string_t *month = cfg_locales_share(state, ids[i], language, 0, month_owners, month_sets);
        const string_t *standalone = cfg_locales_share(state, ids[i], language, 1, month_owners, month_sets);
        const string_t *day = cfg_locales_share(state, ids[i], language, 2, day_owners, day_sets);
        const string_t *fields[] = {ids[i], month, day, standalone};
        ok = month && standalone && day && cfg_locales_row(state->tables[4], 4, fields);
    }
    const json_t *sets[] = {month_sets, day_sets};
    for (size_t k = 0; k < 2 && ok; ++k) {
        const string_t **keys = cfg_locales_keys(sets[k]);
        ok = keys != NULL;
        for (size_t i = 0; i < json_object_size(sets[k]) && ok; ++i) {
            const json_t *values = json_object_get(sets[k], keys[i]);
            for (size_t j = 0; j < json_array_size(values) && ok; ++j) {
                const json_t *pair = json_array_get(values, j);
                json_t *row = json_new_array();
                ok = row && cfg_locales_push(row, keys[i]) && cfg_locales_number(row, j + (k == 0)) &&
                     json_array_append(row, json_array_get(pair, 0)) &&
                     json_array_append(row, json_array_get(pair, 1)) && json_array_append(state->tables[k + 7], row);
                json_free(row);
            }
        }
        free(keys);
    }
    json_free(month_owners);
    json_free(day_owners);
    json_free(month_sets);
    json_free(day_sets);
    return ok;
}

static bool cfg_locales_patterns_table(cfg_locales_state *state, const string_t *const *ids)
{
    json_t *owners = json_new_object();
    bool ok = owners != NULL;
    for (size_t i = 0; i < json_object_size(state->active) && ok; ++i) {
        const string_t *language = json_string_value(json_object_get(state->active, ids[i]));
        const json_t *supp = json_object_get(cfg_locales_get(state->supp, "locales"), ids[i]);
        const json_t *data = supp ? supp : json_object_get(state->records, ids[i]);
        const string_t *pattern = cfg_locales_text(data, supp ? "date_pattern" : "pattern");
        const string_t *display =
            json_string_value(json_object_get(cfg_locales_get(state->supp, "patterns"), language));
        const string_t *suffix =
            json_string_value(json_object_get(cfg_locales_get(state->supp, "first_day_suffixes"), language));
        json_t *parts =
            pattern ? cfg_locales_pattern(display ? display : pattern, cfg_locales_text(data, "era"), suffix) : NULL;
        string_t *signature = parts ? cfg_locales_signature(parts) : NULL;
        const json_t *number = signature ? json_object_get(owners, signature) : NULL;
        ok = signature != NULL;
        if (ok && !number) {
            size_t id = json_object_size(owners) + 1;
            json_t *holder = json_new_array();
            ok = holder && cfg_locales_number(holder, id) &&
                 json_object_set(owners, signature, json_array_get(holder, 0));
            json_free(holder);
            number = json_object_get(owners, signature);
            for (size_t j = 0; j < json_array_size(parts) && ok; ++j) {
                const json_t *pair = json_array_get(parts, j);
                json_t *row = json_new_array();
                ok = row && cfg_locales_number(row, id) && cfg_locales_number(row, j) &&
                     json_array_append(row, json_array_get(pair, 0)) &&
                     json_array_append(row, json_array_get(pair, 1)) && json_array_append(state->tables[6], row);
                json_free(row);
            }
        }
        json_t *row = json_new_array();
        ok = ok && row && cfg_locales_push(row, ids[i]) && json_array_append(row, number) &&
             cfg_locales_push(row, pattern) && json_array_append(state->tables[5], row);
        json_free(row);
        json_free(parts);
        string_free(signature);
    }
    json_free(owners);
    return ok;
}

static string_t *cfg_locales_town_key(const string_t *town)
{
    /* Generation and installation must use the same public catalogue key convention. */
    return cfg_calendar_key(town, false);
}

static bool cfg_locales_towns(cfg_locales_state *state)
{
    const json_t *towns = cfg_locales_get(state->supp, "towns");
    const string_t **countries = cfg_locales_keys(towns);
    string_t *status = string_new_with("additional");
    bool ok = countries && status;
    for (size_t i = 0; i < json_object_size(towns) && ok; ++i) {
        const json_t *languages = json_object_get(towns, countries[i]);
        const string_t **keys = cfg_locales_keys(languages);
        ok = keys != NULL;
        for (size_t j = 0; j < json_object_size(languages) && ok; ++j) {
            const json_t *places = json_object_get(languages, keys[j]);
            for (size_t k = 0; k < json_array_size(places) && ok; ++k) {
                string_t *key = cfg_locales_town_key(json_string_value(json_array_get(places, k)));
                const string_t *fields[] = {countries[i], key, keys[j], keys[j], status};
                ok = key && cfg_locales_row(state->tables[3], 5, fields);
                string_free(key);
            }
        }
        free(keys);
    }
    string_free(status);
    free(countries);
    return ok;
}

/* Construct stable relational output from selected source records. */
bool cfg_locales_tables(cfg_locales_state *state)
{
    bool ok = true;
    for (size_t i = 0; i < 9; ++i) {
        state->tables[i] = i == 2 ? json_clone(state->choices) : json_new_array();
        ok = state->tables[i] && ok;
    }
    for (size_t i = 0; i < json_array_size(state->order) && ok; ++i) {
        const string_t *territory = json_string_value(json_array_get(state->order, i));
        const string_t *locale = json_string_value(json_object_get(state->selected, territory));
        const string_t *fields[] = {territory, locale};
        if (locale)
            ok = cfg_locales_row(state->tables[0], 2, fields);
    }
    const string_t **languages = cfg_locales_keys(state->names), **ids = cfg_locales_keys(state->active);
    ok = ok && languages && ids;
    for (size_t i = 0; i < json_object_size(state->names) && ok; ++i) {
        const json_t *pair = json_object_get(state->names, languages[i]);
        const string_t *fields[] = {languages[i], json_string_value(json_array_get(pair, 0)),
                                    json_string_value(json_array_get(pair, 1))};
        ok = cfg_locales_row(state->tables[1], 3, fields);
    }
    ok = ok && cfg_locales_towns(state) && cfg_locales_names_tables(state, ids) &&
         cfg_locales_patterns_table(state, ids);
    free(languages);
    free(ids);
    return ok;
}
