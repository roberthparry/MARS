/**
 * @file lab_forms.c
 * @brief Native parsing of integrator rows, clock entry and legacy town keys.
 *
 * Uses string_t slices/cursors throughout; no byte-buffer text parser or browser
 * expression interpretation is involved. Authored bounds and names remain text.
 * The expression module supplies canonical binding identity, including Greek and
 * indexed aliases, without serialising a rewritten expression into the response.
 * Each request owns its temporary objects and enforces small explicit limits.
 */
#include <ctype.h>
#include <math.h>

#include "expression.h"
#include "number.h"
#include "ustring.h"
#include "lab_forms.h"

static const json_t *lab_forms_get(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_forms_put(json_t *object, const char *name, json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    json_free(value);
    return ok;
}

static bool lab_forms_equal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

static bool lab_forms_text_valid(const string_t *text)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool ok = cursor != NULL;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte;
        ok = !string_cursor_peek_ascii(cursor, &byte) || byte != 0;
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return ok;
}

/* Length-aware splitting preserves empty legacy fields, including trailing ones. */
static string_t **lab_forms_split(const string_t *text, const char *delimiter, size_t *count)
{
    string_t *separator = string_new_with(delimiter);
    string_t **parts = separator ? string_split_string(text, separator, count) : NULL;
    string_free(separator);
    return parts;
}

static string_t *lab_forms_text(const json_t *object, const char *name, const char *fallback, size_t *budget)
{
    const json_t *value = lab_forms_get(object, name);
    const string_t *text = json_string_value(value);
    if (value && !text)
        return NULL;
    size_t size = text ? string_byte_length(text) : 0;
    if (size > *budget || (text && !lab_forms_text_valid(text)))
        return NULL;
    *budget -= size;
    string_t *copy = text ? string_clone(text) : fallback ? string_new_with(fallback) : NULL;
    if (copy)
        string_trim(copy);
    return copy;
}

static string_t *lab_forms_slice(const string_t *text, size_t begin, size_t end)
{
    string_view_t view = string_view(text, begin, end - begin);
    view = string_view_trim(view);
    return string_from_view(&view);
}

/* One bounded syntax scan; bracketed names and nested bound arguments stay opaque. */
static bool lab_forms_separator(const string_t *text, const char *separator, size_t *position, bool *found)
{
    string_cursor_t *cursor = string_cursor_new(text);
    unsigned char stack[128];
    size_t depth = 0;
    bool ok = cursor != NULL;
    *found = false;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        string_cursor_peek_ascii(cursor, &byte);
        if (!depth && string_cursor_match(cursor, separator)) {
            if (*found) {
                ok = false;
                break;
            }
            *found = true;
            *position = string_cursor_position(cursor);
            string_cursor_consume(cursor, separator);
            continue;
        }
        if (depth && stack[depth - 1] == '[') {
            if (byte == ']')
                --depth;
        } else if (byte == '(' || byte == '[' || byte == '{') {
            if (depth == sizeof(stack)) {
                ok = false;
                break;
            }
            stack[depth++] = byte;
        } else if (byte == ')' || byte == ']' || byte == '}') {
            unsigned char wanted = byte == ')' ? '(' : byte == ']' ? '[' : '{';
            if (!depth || stack[--depth] != wanted) {
                ok = false;
                break;
            }
        }
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return ok && !depth;
}

static void lab_forms_clean_bound(string_t *text)
{
    string_t *words = string_new();
    string_cursor_t *cursor = string_cursor_new(text);
    bool space = false;
    while (words && cursor && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        if (string_cursor_peek_ascii(cursor, &byte) && isspace(byte))
            space = string_byte_length(words) != 0;
        else {
            if (space)
                string_append_char(words, ' ');
            string_append_rune(words, string_cursor_peek(cursor));
            space = false;
        }
        string_cursor_next(cursor);
    }
    if (words)
        string_to_lower(words);
    if (lab_forms_equal(words, "blank for none") || lab_forms_equal(words, "blank for antiderivative"))
        string_clear(text);
    string_cursor_free(cursor);
    string_free(words);
}

static json_t *lab_forms_row(string_t *name, string_t *lower, string_t *upper, bool free_row)
{
    if (!name || !lower || !upper || string_byte_length(name) > 256)
        return NULL;
    if (!string_byte_length(name) && string_append_cstr(name, "x"))
        return NULL;
    lab_forms_clean_bound(lower);
    lab_forms_clean_bound(upper);
    json_t *row = json_new_object();
    string_t *kind = string_new_with(free_row ? "free" : "bound");
    bool ok = row && kind && lab_forms_put(row, "kind", json_new_string(kind)) &&
              lab_forms_put(row, "name", json_new_string(name)) && lab_forms_put(row, "lo", json_new_string(lower)) &&
              lab_forms_put(row, "hi", json_new_string(upper));
    string_free(kind);
    if (!ok) {
        json_free(row);
        row = NULL;
    }
    return row;
}

static json_t *lab_forms_line(const string_t *line)
{
    string_t *name = NULL, *lower = string_new(), *upper = NULL;
    string_t *folded = string_clone(line);
    if (folded)
        string_to_lower(folded);
    string_cursor_t *cursor = folded ? string_cursor_new(folded) : NULL;
    bool free_row = false, ok = cursor && lower;
    if (ok && string_cursor_consume(cursor, "free")) {
        unsigned char next = 0;
        free_row = string_cursor_peek_ascii(cursor, &next) && (isspace(next) || next == ':');
        if (free_row) {
            string_cursor_skip_spaces(cursor);
            string_cursor_consume(cursor, ":");
            name = lab_forms_slice(line, string_cursor_position(cursor), string_byte_length(line));
            upper = string_new();
            ok = name && string_byte_length(name);
        }
    }
    string_cursor_free(cursor);
    string_free(folded);
    if (ok && !free_row) {
        size_t split = 0, colon = 0;
        bool equals = false, has_colon = false;
        ok = lab_forms_separator(line, "=", &split, &equals) &&
             lab_forms_separator(line, ":", &colon, &has_colon) && !(equals && has_colon);
        if (!equals && has_colon)
            split = colon;
        name = lab_forms_slice(line, 0, equals || has_colon ? split : string_byte_length(line));
        upper = equals || has_colon ? lab_forms_slice(line, split + 1, string_byte_length(line)) : string_new();
        ok = ok && name && upper && string_byte_length(name) &&
             (!(equals || has_colon) || string_byte_length(upper));
        size_t range = 0;
        bool found = false;
        ok = ok && lab_forms_separator(upper, "..", &range, &found);
        if (ok && found) {
            string_free(lower);
            lower = lab_forms_slice(upper, 0, range);
            string_t *right = lab_forms_slice(upper, range + 2, string_byte_length(upper));
            string_free(upper);
            upper = right;
            ok = lower && upper && string_byte_length(lower) && string_byte_length(upper);
        }
        if (ok && !(equals || has_colon)) {
            ok = lab_forms_separator(name, "..", &range, &found) && !found;
        }
    }
    json_t *row = ok ? lab_forms_row(name, lower, upper, free_row) : NULL;
    string_free(name);
    string_free(lower);
    string_free(upper);
    return row;
}

static json_t *lab_forms_rows(const json_t *payload, size_t *budget)
{
    const json_t *source = lab_forms_get(payload, "rows");
    json_t *rows = json_new_array();
    bool ok = rows != NULL;
    if (source) {
        ok = ok && json_type(source) == JSON_ARRAY && json_array_size(source) <= 256;
        for (size_t i = 0; ok && i < json_array_size(source); ++i) {
            const json_t *entry = json_array_get(source, i);
            string_t *kind = lab_forms_text(entry, "kind", "bound", budget);
            string_t *name = lab_forms_text(entry, "name", "x", budget);
            string_t *lower = lab_forms_text(entry, "lo", "", budget);
            string_t *upper = lab_forms_text(entry, "hi", "", budget);
            if (kind)
                string_to_lower(kind);
            json_t *row = json_type(entry) == JSON_OBJECT && kind ?
                          lab_forms_row(name, lower, upper, lab_forms_equal(kind, "free")) : NULL;
            ok = row && json_array_append(rows, row);
            json_free(row);
            string_free(kind);
            string_free(name);
            string_free(lower);
            string_free(upper);
        }
    } else {
        string_t *text = lab_forms_text(payload, "text", NULL, budget);
        size_t count = 0;
        string_t **lines = text ? lab_forms_split(text, "\n", &count) : NULL;
        ok = ok && text && (lines || !string_byte_length(text));
        for (size_t i = 0; ok && i < count; ++i) {
            string_trim(lines[i]);
            if (!string_byte_length(lines[i]))
                continue;
            json_t *row = lab_forms_line(lines[i]);
            ok = row && json_array_size(rows) < 256 && json_array_append(rows, row);
            json_free(row);
        }
        string_split_free(lines, count);
        string_free(text);
    }
    if (ok && !json_array_size(rows)) {
        string_t *line = string_new_with("x = 0 .. 1");
        json_t *row = line ? lab_forms_line(line) : NULL;
        ok = row && json_array_append(rows, row);
        json_free(row);
        string_free(line);
    }
    if (!ok) {
        json_free(rows);
        rows = NULL;
    }
    return rows;
}

/* Strip only the outer binding envelope so unused declarations do not count as references. */
static string_t *lab_forms_expression_body(const string_t *source)
{
    string_t *body = string_clone(source);
    if (!body)
        return NULL;
    string_trim(body);
    if (string_starts_with(body, "{") && string_ends_with(body, "}")) {
        string_t *inside = lab_forms_slice(body, 1, string_byte_length(body) - 1);
        string_free(body);
        body = inside;
        size_t separator = 0;
        bool found = false;
        if (!body || !lab_forms_separator(body, "|", &separator, &found)) {
            string_free(body);
            return NULL;
        }
        if (found) {
            inside = lab_forms_slice(body, 0, separator);
            string_free(body);
            body = inside;
        }
    }
    return body;
}

static bool lab_forms_integrator(const json_t *payload, json_t *result, size_t *budget)
{
    json_t *rows = lab_forms_rows(payload, budget);
    string_t *expression = lab_forms_text(payload, "expression", "", budget);
    string_t *body = expression ? lab_forms_expression_body(expression) : NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *parsed = body && string_byte_length(body) ? expr_from_text(body, &bindings) : NULL;
    bool valid = body && (!string_byte_length(body) || parsed);
    string_t *text = string_new();
    bool ok = rows && expression && text;
    for (size_t i = 0; ok && i < json_array_size(rows); ++i) {
        json_t *row = (json_t *)json_array_get(rows, i);
        const string_t *name = json_string_value(lab_forms_get(row, "name"));
        const string_t *lower = json_string_value(lab_forms_get(row, "lo"));
        const string_t *upper = json_string_value(lab_forms_get(row, "hi"));
        bool free_row = lab_forms_equal(json_string_value(lab_forms_get(row, "kind")), "free");
        bool referenced = !valid || (bindings && expr_bindings_get_text(bindings, name));
        string_t *line = free_row ? string_sprintf("free %S", name) :
                         string_byte_length(upper) ? (string_byte_length(lower) ?
                         string_sprintf("%S = %S .. %S", name, lower, upper) : string_sprintf("%S = %S", name, upper)) :
                         string_clone(name);
        ok = line && lab_forms_put(row, "referenced", json_new_bool(referenced)) &&
             lab_forms_put(row, "text", json_new_string(line)) &&
             (!i || string_append_char(text, '\n') == 0) && string_append_string(text, line) == 0;
        string_free(line);
    }
    ok = ok && lab_forms_put(result, "text", json_new_string(text)) &&
         lab_forms_put(result, "references_valid", json_new_bool(valid));
    bool attached = lab_forms_put(result, "rows", rows);
    expr_free(parsed);
    expr_bindings_free(bindings);
    string_free(body);
    string_free(expression);
    string_free(text);
    return ok && attached;
}

static bool lab_forms_time(const json_t *payload, json_t *result, size_t *budget)
{
    string_t *source = lab_forms_text(payload, "text", NULL, budget);
    string_t *clock = string_new(), *fraction = string_new();
    string_cursor_t *cursor = source ? string_cursor_new(source) : NULL;
    bool decimal = false, ok = cursor && clock && fraction;
    unsigned digits = 0;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        bool ascii = string_cursor_peek_ascii(cursor, &byte);
        if (ascii && (byte == '.' || byte == ','))
            decimal = true;
        else if (ascii && isdigit(byte)) {
            if (decimal)
                ok = string_append_char(fraction, (char)byte) == 0;
            else if (digits < 6) {
                if (digits == 2 || digits == 4)
                    ok = string_append_char(clock, ':') == 0;
                ok = ok && string_append_char(clock, (char)byte) == 0;
                ++digits;
            }
        }
        string_cursor_next(cursor);
    }
    if (ok && decimal && digits == 6)
        ok = string_append_char(clock, '.') == 0 && string_append_string(clock, fraction) == 0;
    ok = ok && lab_forms_put(result, "text", json_new_string(clock));
    string_cursor_free(cursor);
    string_free(source);
    string_free(clock);
    string_free(fraction);
    return ok;
}

static json_t *lab_forms_town_parts(const string_t *source)
{
    size_t count = 0;
    string_t **parts = lab_forms_split(source, "|", &count);
    json_t *town = json_new_object();
    static const char *const keys[] = {"name", "latitude", "longitude", "elevation"};
    bool ok = town && (parts || !string_byte_length(source));
    for (size_t i = 0; ok && i < 4; ++i) {
        string_t *value = i < count ? string_clone(parts[i]) : string_new();
        if (value)
            string_trim(value);
        ok = value && lab_forms_put(town, keys[i], json_new_string(value));
        string_free(value);
    }
    string_split_free(parts, count);
    if (!ok) {
        json_free(town);
        town = NULL;
    }
    return town;
}

static double lab_forms_coordinate(const string_t *text)
{
    if (!text || !string_byte_length(text))
        return NAN;
    number_t number = num_create_from_text(text);
    double value = num_is_real(number) && num_is_finite(number) ? num_to_double(number) : NAN;
    num_destroy(&number);
    return value;
}

static bool lab_forms_town_match(const json_t *wanted, const json_t *candidate)
{
    const string_t *name = json_string_value(lab_forms_get(wanted, "name"));
    const string_t *other = json_string_value(lab_forms_get(candidate, "name"));
    if (!name || !other || !string_byte_length(name) || string_compare(name, other))
        return false;
    static const char *const coordinates[] = {"latitude", "longitude"};
    for (size_t i = 0; i < 2; ++i) {
        double left = lab_forms_coordinate(json_string_value(lab_forms_get(wanted, coordinates[i])));
        double right = lab_forms_coordinate(json_string_value(lab_forms_get(candidate, coordinates[i])));
        if (!isfinite(left) || !isfinite(right) || fabs(left - right) > 0.000001)
            return false;
    }
    const string_t *height = json_string_value(lab_forms_get(wanted, "elevation"));
    const string_t *other_height = json_string_value(lab_forms_get(candidate, "elevation"));
    return !string_byte_length(height) || !string_byte_length(other_height) || !string_compare(height, other_height);
}

static string_t *lab_forms_town_detail(const json_t *town)
{
    double latitude = lab_forms_coordinate(json_string_value(lab_forms_get(town, "latitude")));
    double longitude = lab_forms_coordinate(json_string_value(lab_forms_get(town, "longitude")));
    return isfinite(latitude) && isfinite(longitude) ?
           string_sprintf("%+08.4f  %+09.4f", latitude, longitude) : string_new();
}

/* Prepare catalogue text once on the server, sharing the legacy form formatter. */
bool lab_forms_town_presentation(json_t *town)
{
    if (json_type(town) != JSON_OBJECT)
        return false;
    size_t budget = 65536;
    string_t *name = lab_forms_text(town, "name", "", &budget);
    string_t *latitude = lab_forms_text(town, "latitude", "", &budget);
    string_t *longitude = lab_forms_text(town, "longitude", "", &budget);
    string_t *elevation = lab_forms_text(town, "elevation", "", &budget);
    string_t *value = name && latitude && longitude && elevation ?
                      string_sprintf("%S|%S|%S|%S", name, latitude, longitude, elevation) : NULL;
    string_t *detail = value ? lab_forms_town_detail(town) : NULL;
    bool ok = value && detail && lab_forms_put(town, "value", json_new_string(value)) &&
              lab_forms_put(town, "detail", json_new_string(detail));
    string_free(name);
    string_free(latitude);
    string_free(longitude);
    string_free(elevation);
    string_free(value);
    string_free(detail);
    return ok;
}

static bool lab_forms_town(const json_t *payload, json_t *result, size_t *budget)
{
    string_t *source = lab_forms_text(payload, "value", NULL, budget);
    json_t *town = source ? lab_forms_town_parts(source) : NULL;
    const json_t *candidates = lab_forms_get(payload, "candidates");
    json_t *details = json_new_array();
    bool ok = town && details &&
              (!candidates || (json_type(candidates) == JSON_ARRAY && json_array_size(candidates) <= 4096));
    long match = -1;
    /* Candidate matching requires inspecting each supplied option, at most 4096;
     * it is performed once at restoration, not repeatedly while rendering. */
    for (size_t i = 0; ok && candidates && i < json_array_size(candidates); ++i) {
        const string_t *value = json_string_value(json_array_get(candidates, i));
        if (!value || string_byte_length(value) > *budget || !lab_forms_text_valid(value)) {
            ok = false;
            break;
        }
        *budget -= string_byte_length(value);
        json_t *candidate = lab_forms_town_parts(value);
        ok = candidate != NULL;
        if (ok && match < 0 && lab_forms_town_match(town, candidate))
            match = (long)i;
        string_t *detail = candidate ? lab_forms_town_detail(candidate) : NULL;
        json_t *entry = detail ? json_new_string(detail) : NULL;
        ok = ok && entry && json_array_append(details, entry);
        json_free(entry);
        string_free(detail);
        json_free(candidate);
    }
    string_t *detail = lab_forms_town_detail(town);
    number_t index = num_create_from_long(match);
    ok = ok && detail && lab_forms_put(result, "detail", json_new_string(detail)) &&
         lab_forms_put(result, "match_index", json_new_number_value(index));
    num_destroy(&index);
    bool attached = lab_forms_put(result, "town", town);
    bool presented = lab_forms_put(result, "candidate_details", details);
    string_free(detail);
    string_free(source);
    return ok && attached && presented;
}

/* Handle native form preparation without mutating authored expressions or stored state. */
json_t *lab_forms_request(const json_t *payload, unsigned *status)
{
    if (status)
        *status = 400;
    json_t *result = json_new_object();
    if (!result) {
        if (status)
            *status = 500;
        return NULL;
    }
    const string_t *action = json_string_value(lab_forms_get(payload, "action"));
    size_t budget = 65536;
    bool ok = false;
    if (payload && json_type(payload) == JSON_OBJECT && action) {
        if (lab_forms_equal(action, "integrator"))
            ok = lab_forms_integrator(payload, result, &budget);
        else if (lab_forms_equal(action, "time"))
            ok = lab_forms_time(payload, result, &budget);
        else if (lab_forms_equal(action, "town"))
            ok = lab_forms_town(payload, result, &budget);
    }
    if (!ok) {
        json_free(result);
        result = json_new_object();
        string_t *error = string_new_with("Invalid or oversized form request; check its action, field types and syntax.");
        bool recorded = result && error && lab_forms_put(result, "error", json_new_string(error));
        string_free(error);
        if (!recorded) {
            json_free(result);
            result = NULL;
        }
    }
    if (!result || !lab_forms_put(result, "ok", json_new_bool(ok))) {
        json_free(result);
        if (status)
            *status = 500;
        return NULL;
    }
    if (status && ok)
        *status = 200;
    return result;
}
