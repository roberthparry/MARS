/**
 * @file lab_calendar_presentation.c
 * @brief Native almanac worksheet display and clipboard policy.
 *
 * Builds both local-toggle variants without recomputing astronomy. Parses civil
 * timestamps with string_t cursors, supplies event dates and compact times,
 * classifies visible bodies and land searches, and preserves original action
 * objects. Bounded rows and events limit duplicated wire metadata. No DOM, I/O,
 * workers or mutable shared state are used; publication is allocation-checked.
 */
#include <string.h>

#include "ustring.h"
#include "lab_calendar_internal.h"

static bool lab_cal_pres_put(json_t *object, const char *name, json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    json_free(value);
    return ok;
}

static bool lab_cal_pres_text(json_t *object, const char *name, const char *text)
{
    string_t *value = string_new_with(text);
    bool ok = value && lab_cal_pres_put(object, name, json_new_string(value));
    string_free(value);
    return ok;
}

static string_t *lab_cal_pres_trim(const json_t *object, const char *name)
{
    string_t *text = string_new_with(lab_cal_text(object, name));
    if (text) string_trim(text);
    return text;
}

static bool lab_cal_pres_digits(string_cursor_t *cursor, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        unsigned char ch = 0;
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9') return false;
        string_cursor_next(cursor);
    }
    return true;
}

static bool lab_cal_pres_date(string_cursor_t *cursor)
{
    return lab_cal_pres_digits(cursor, 4u) && string_cursor_consume(cursor, "-") &&
           lab_cal_pres_digits(cursor, 2u) && string_cursor_consume(cursor, "-") &&
           lab_cal_pres_digits(cursor, 2u);
}

static string_t *lab_cal_pres_event_date(const json_t *event)
{
    static const char *const candidates[] = {"greatest", "time", "first_contact", "fourth_contact", "gmt_time"};
    /* Five ordered native event fields preserve the legacy date precedence. */
    for (size_t i = 0; i < sizeof(candidates) / sizeof(*candidates); ++i) {
        string_t *text = lab_cal_pres_trim(event, candidates[i]);
        string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
        if (!cursor) { string_free(text); return NULL; }
        bool match = lab_cal_pres_date(cursor);
        unsigned char next = 0;
        if (match && string_cursor_peek_ascii(cursor, &next))
            match = !((next >= '0' && next <= '9') || (next >= 'a' && next <= 'z') ||
                      (next >= 'A' && next <= 'Z') || next == '_');
        string_t *date = match ? string_cursor_slice_between(0u, 10u, cursor) : NULL;
        string_cursor_free(cursor);
        string_free(text);
        if (match) return date;
    }
    return string_new();
}

static string_t *lab_cal_pres_time(const char *value, bool gmt)
{
    string_t *text = string_new_with(value);
    if (!text) return NULL;
    string_trim(text);
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor) { string_free(text); return NULL; }
    bool match = lab_cal_pres_date(cursor);
    size_t before = string_cursor_position(cursor);
    string_cursor_skip_spaces(cursor);
    match = match && string_cursor_position(cursor) > before;
    size_t start = string_cursor_position(cursor);
    match = match && lab_cal_pres_digits(cursor, 2u) && string_cursor_consume(cursor, ":") &&
            lab_cal_pres_digits(cursor, 2u) && string_cursor_consume(cursor, ":") && lab_cal_pres_digits(cursor, 2u);
    size_t end = string_cursor_position(cursor);
    if (match && !string_cursor_done(cursor)) {
        before = string_cursor_position(cursor);
        string_cursor_skip_spaces(cursor);
        match = string_cursor_position(cursor) > before && string_cursor_consume(cursor, "GMT");
        if (match && !string_cursor_done(cursor)) {
            match = string_cursor_consume(cursor, "+") || string_cursor_consume(cursor, "-");
            match = match && lab_cal_pres_digits(cursor, 1u);
            if (match) {
                unsigned char ch = 0;
                if (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') string_cursor_next(cursor);
                if (string_cursor_consume(cursor, ":")) match = lab_cal_pres_digits(cursor, 2u);
            }
        }
    }
    match = match && string_cursor_done(cursor);
    string_t *result = match ? string_cursor_slice_between(start, end, cursor) : string_clone(text);
    /* GMT labels without a full date retain their authored clock but lose the suffix. */
    if (!match && gmt && result && string_ends_with(result, "GMT")) {
        size_t length = string_byte_length(result);
        string_cursor_seek(cursor, length - 3u);
        string_t *prefix = string_cursor_slice_between(0u, length - 3u, cursor);
        if (!prefix) { string_free(result); result = NULL; }
        else {
            size_t original = string_byte_length(prefix);
            string_trim(prefix);
            if (string_byte_length(prefix) < original) { string_free(result); result = prefix; }
            else string_free(prefix);
        }
    }
    string_cursor_free(cursor);
    string_free(text);
    return result;
}

static bool lab_cal_pres_cell(string_t *out, const char *text, bool separator)
{
    string_t *value = string_new_with(text);
    if (!value) return false;
    string_trim(value);
    bool ok = (!separator || !string_append_cstr(out, " | ")) && !string_append_string(out, value);
    string_free(value);
    return ok;
}

static bool lab_cal_pres_copy_events(string_t *copy, const json_t *response, const json_t *events)
{
    bool ok = string_append_format(copy, "\n\n%s\nClass | Event | Kind | Magnitude | Obscuration | Date | "
                                  "First contact | Greatest eclipse | Fourth contact | Greatest GMT | Notes | "
                                  "Nearest totality", lab_cal_text(response, "event_title")) >= 0;
    static const char *const fields[] = {"category", "name", "kind", "magnitude", "obscuration", "date_text",
                                        "first_contact", "greatest", "fourth_contact", "gmt_time", "details",
                                        "nearest_totality"};
    for (size_t i = 0; ok && i < json_array_size(events); ++i) {
        const json_t *event = json_array_get(events, i);
        ok = !string_append_char(copy, '\n');
        for (size_t j = 0; ok && j < sizeof(fields) / sizeof(*fields); ++j) {
            const char *value = lab_cal_text(event, fields[j]);
            if (j == 7u && !*value) value = lab_cal_text(event, "time");
            ok = lab_cal_pres_cell(copy, value, j != 0u);
        }
    }
    if (ok && !json_array_size(events)) ok = !string_append_cstr(copy, "\nNo events found.");
    return ok;
}

static json_t *lab_cal_pres_events(const json_t *source)
{
    json_t *events = json_new_array();
    bool ok = events != NULL;
    static const char *const from[] = {"first_contact", "greatest", "fourth_contact", "gmt_time"};
    static const char *const to[] = {"first_text", "greatest_text", "fourth_text", "gmt_text"};
    for (size_t i = 0; ok && i < json_array_size(source); ++i) {
        const json_t *original = json_array_get(source, i);
        json_t *event = json_clone(original);
        string_t *date = lab_cal_pres_event_date(original);
        string_t *category = lab_cal_pres_trim(original, "category"), *name = lab_cal_pres_trim(original, "name");
        string_t *kind = lab_cal_pres_trim(original, "kind"), *nearest = lab_cal_pres_trim(original, "nearest_totality");
        ok = event && date && category && name && kind && nearest;
        bool land = ok && !strcmp(string_c_str(category), "Solar") && !strcmp(string_c_str(name), "Solar eclipse") &&
                    strcmp(string_c_str(kind), "total") && !string_byte_length(nearest);
        ok = ok && lab_cal_pres_text(event, "date_text", string_c_str(date)) &&
             lab_cal_pres_put(event, "needs_land_search", json_new_bool(land)) &&
             lab_cal_pres_text(event, "nearest_text",
                               land ? "Searching for nearest location on land..." : string_c_str(nearest));
        for (size_t j = 0; ok && j < 4u; ++j) {
            const char *value = lab_cal_text(original, from[j]);
            if (j == 1u && !*value) value = lab_cal_text(original, "time");
            string_t *clock = lab_cal_pres_time(value, j == 3u);
            ok = clock && lab_cal_pres_text(event, to[j], string_c_str(clock));
            string_free(clock);
        }
        ok = ok && json_array_append(events, event);
        json_free(event);
        string_free(date);
        string_free(category);
        string_free(name);
        string_free(kind);
        string_free(nearest);
    }
    if (!ok) { json_free(events); return NULL; }
    return events;
}

static json_t *lab_cal_pres_variant(const json_t *response, const json_t *source, const json_t *events, bool visible)
{
    const char *body = visible ? "Location of Navigational Bodies; visible bodies only" :
                                 "Location of Navigational Bodies; all bodies shown";
    json_t *variant = json_new_object(), *rows = json_new_array();
    string_t *copy = string_new();
    bool ok = variant && rows && copy;
    static const char *const headings[] = {"worksheet_title", "moment_text", "observer_text"};
    for (size_t i = 0; ok && i < 3u; ++i) {
        const char *value = lab_cal_text(response, headings[i]);
        if (!i && !*value) value = "AstroNav Navigation Almanac";
        string_t *trimmed = string_new_with(value);
        if (trimmed) string_trim(trimmed);
        ok = trimmed != NULL;
        if (ok && string_byte_length(trimmed)) ok = string_append_format(copy, "%s\n", value) >= 0;
        string_free(trimmed);
    }
    ok = ok && string_append_format(copy, "%s\nBody filter: %s\n\nBody | Declination | GHA | RA | Altitude | "
                                   "Azimuth | s.d. | Vmag.%s", body, visible ? "visible only" : "all bodies",
                                   visible ? "" : " | Visible") >= 0;
    static const char *const columns[] = {"name", "declination", "gha", "right_ascension", "altitude", "azimuth",
                                         "semi_diameter", "magnitude", "visible"};
    for (size_t i = 0; ok && i < json_array_size(source); ++i) {
        const json_t *original = json_array_get(source, i);
        string_t *flag = lab_cal_pres_trim(original, "visible");
        if (!flag) { ok = false; break; }
        string_to_upper(flag);
        bool shown = !strcmp(string_c_str(flag), "YES");
        string_free(flag);
        if (visible && !shown) continue;
        json_t *row = json_clone(original);
        ok = row && lab_cal_pres_put(row, "is_visible", json_new_bool(shown)) &&
             lab_cal_pres_text(row, "visible_label", shown ? "Visible" : "Not visible") &&
             lab_cal_pres_text(row, "visible_icon", shown ? "✓" : "✕") && json_array_append(rows, row);
        json_free(row);
        ok = ok && !string_append_char(copy, '\n');
        for (size_t j = 0; ok && j < (visible ? 8u : 9u); ++j) {
            const char *value = lab_cal_text(original, columns[j]);
            if (!j && !*value) value = lab_cal_text(original, "code");
            ok = lab_cal_pres_cell(copy, value, j != 0u);
        }
    }
    if (ok && !json_array_size(rows))
        ok = !string_append_cstr(copy, visible ? "\nNo bodies found for the current visibility filter. |  |  |  |  |  |  | " :
                                               "\nNo bodies found for the current visibility filter. |  |  |  |  |  |  |  | ");
    ok = ok && lab_cal_pres_copy_events(copy, response, events) &&
         lab_cal_pres_text(variant, "body_text", body) && lab_cal_pres_text(variant, "copy_text", string_c_str(copy)) &&
         lab_cal_pres_put(variant, "show_visible", json_new_bool(!visible));
    string_t *html = ok ? lab_cal_almanac_markup(response, rows, events, visible) : NULL;
    ok = ok && html && lab_cal_pres_text(variant, "html", string_c_str(html));
    string_free(html);
    if (!lab_cal_pres_put(variant, "rows", rows)) ok = false;
    string_free(copy);
    if (!ok) { json_free(variant); return NULL; }
    return variant;
}

/* Attach both local-toggle variants only after all presentation allocations succeed. */
bool lab_cal_almanac_presentation(json_t *response)
{
    if (!response || json_type(response) != JSON_OBJECT) return false;
    const json_t *rows = lab_cal_get(response, "all_rows"), *raw_events = lab_cal_get(response, "events");
    if (!rows || json_type(rows) != JSON_ARRAY) rows = lab_cal_get(response, "rows");
    if (json_array_size(rows) > 256u || json_array_size(raw_events) > 64u) return false;
    json_t *events = lab_cal_pres_events(raw_events), *metadata = json_new_object();
    bool ok = events && metadata &&
              lab_cal_pres_put(metadata, "all", lab_cal_pres_variant(response, rows, events, false)) &&
              lab_cal_pres_put(metadata, "visible", lab_cal_pres_variant(response, rows, events, true));
    if (!lab_cal_pres_put(metadata, "events", events)) ok = false;
    if (ok) return lab_cal_pres_put(response, "almanac_presentation", metadata);
    json_free(metadata);
    return false;
}
