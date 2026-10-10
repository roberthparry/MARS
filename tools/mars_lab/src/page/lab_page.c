/**
 * @file lab_page.c
 * @brief Render the Lab page and build its separately transported configuration.
 *
 * Substitution is a single pass over the template, so user text cannot introduce
 * another substitution. The page contains no embedded configuration script;
 * bootstrap values are served through the versioned transport adapter. Native
 * catalogue tables supply defaults and presentation constants internally.
 * Geographic choices are served from the jurisdiction API.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "lab_mobile.h"
#include "lab_page.h"

static const json_t *lab_page_member(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_page_set_text(json_t *object, const char *name, const char *text)
{
    string_t *key = string_new_with(name), *s = string_new_with(text);
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = key && value && json_object_set(object, key, value);
    json_free(value);
    string_free(s);
    string_free(key);
    return ok;
}

static const char *lab_page_text_value(const json_t *value)
{
    const string_t *s = json_string_value(value);
    return s ? string_c_str(s) : "";
}

static string_t *lab_page_read_template(void)
{
    const char *configured = getenv("MARS_LAB_ASSET_FILE"), *root = getenv("MARS_ROOT");
    string_t *path = string_new();
    if (!path)
        return NULL;
    int error = 0;
    if (configured && *configured)
        error = string_append_cstr(path, configured);
    else if (root && *root)
        error = string_append_format(path, "%s/tools/mars_lab/assets/index.html", root) < 0;
    else
        error = string_append_cstr(path, MARS_LAB_ASSET_PATH);
    file_t *file = error ? NULL : file_new(path);
    string_t *asset = file ? file_read_all_text(file) : NULL;
    file_free(file);
    string_free(path);
    return asset;
}

static json_t *lab_page_defaults_from(const json_t *data)
{
    return json_clone(lab_page_member(data, "defaults"));
}

/* Supply the same native defaults to the page and persistent state store. */
json_t *lab_page_defaults(void)
{
    json_t *data = lab_page_catalogue();
    json_t *defaults = data ? lab_page_defaults_from(data) : NULL;
    json_free(data);
    return defaults;
}

static string_t *lab_page_escaped(const char *text, bool script)
{
    string_t *source = string_new_with(text), *out = string_new();
    string_cursor_t *cursor = source ? string_cursor_new(source) : NULL;
    if (!out || !cursor)
        goto fail;
    string_pos_t start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor)) {
        string_pos_t position = string_cursor_position(cursor);
        rune_t rune = string_cursor_peek(cursor);
        uint32_t value = rune_value(rune);
        const char *replacement = NULL;
        if (value == '<')
            replacement = script ? "\\u003c" : "&lt;";
        else if (value == '>')
            replacement = script ? "\\u003e" : "&gt;";
        else if (value == '&')
            replacement = script ? "\\u0026" : "&amp;";
        else if (!script && value == '"')
            replacement = "&quot;";
        else if (!script && value == '\'')
            replacement = "&#39;";
        else if (script && value == 0x2028)
            replacement = "\\u2028";
        else if (script && value == 0x2029)
            replacement = "\\u2029";
        string_cursor_next(cursor);
        if (replacement) {
            if (string_cursor_append_slice_between(out, start, position, cursor) ||
                string_append_cstr(out, replacement))
                goto fail;
            start = string_cursor_position(cursor);
        }
    }
    if (string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor))
        goto fail;
    string_cursor_free(cursor);
    string_free(source);
    return out;
fail:
    string_cursor_free(cursor);
    string_free(source);
    string_free(out);
    return NULL;
}

static bool lab_page_put_html(json_t *values, const char *key, const char *text)
{
    string_t *value = lab_page_escaped(text, false);
    bool ok = value && lab_page_set_text(values, key, string_c_str(value));
    string_free(value);
    return ok;
}

static bool lab_page_put_json(json_t *values, const char *key, const json_t *value)
{
    string_t *serialised = value ? json_to_string(value) : NULL;
    string_t *safe = serialised ? lab_page_escaped(string_c_str(serialised), true) : NULL;
    bool ok = safe && lab_page_set_text(values, key, string_c_str(safe));
    string_free(safe);
    string_free(serialised);
    return ok;
}

static bool lab_page_put_json_text(json_t *values, const char *key, const char *text)
{
    string_t *s = string_new_with(text);
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = value && lab_page_put_json(values, key, value);
    json_free(value);
    string_free(s);
    return ok;
}

static bool lab_page_mobile_values(json_t *values, const json_t *state)
{
    const json_t *mobile = lab_page_member(state, "mobile");
    const json_t *title = lab_page_member(mobile, "title"), *hint = lab_page_member(mobile, "hint"),
                 *url = lab_page_member(mobile, "url");
    const string_t *url_text = json_string_value(url);
    /* Never trust injected SVG: regenerate geometry from the URL with the native encoder. */
    string_t *qr = lab_mobile_qr(url_text);
    bool ok = qr && lab_page_put_html(values, "MOBILE_TITLE", title ? lab_page_text_value(title) : "Local access") &&
              lab_page_put_html(values, "MOBILE_HINT",
                                hint ? lab_page_text_value(hint) : "This Lab is available on this computer only.") &&
              lab_page_put_html(values, "MOBILE_URL", lab_page_text_value(url)) &&
              lab_page_set_text(values, "MOBILE_QR_SVG", string_c_str(qr)) &&
              lab_page_put_html(values, "MOBILE_CARD_CLASS", "");
    string_free(qr);
    return ok;
}

static json_t *lab_page_substitutions(const json_t *data, const json_t *defaults, const json_t *state)
{
    json_t *values = json_new_object();
    const json_t *constants = lab_page_member(data, "constants");
    if (!values)
        return NULL;
    for (size_t i = 0; i < json_object_size(constants); ++i)
        if (!lab_page_put_json(values, string_c_str(json_object_key_at(constants, i)),
                               json_object_value_at(constants, i)))
            goto fail;
    const struct {
        const char *placeholder;
        const char *key;
    } mappings[] = {{"DEFAULT_EXPRESSION", "expression"},
                    {"DEFAULT_EQUATION", "equation"},
                    {"DEFAULT_DIFFEQUATION", "diffequation"},
                    {"DEFAULT_EQUATION_VARIABLE", "equation_variable"},
                    {"DEFAULT_MATRIX", "matrix"},
                    {"DEFAULT_INTEGRATOR", "integrator_expression"},
                    {"DEFAULT_INTEGRATOR_BOUNDS", "integrator_bounds"},
                    {"DEFAULT_INTEGRATOR_INTERVAL_CAP", "integrator_interval_cap"},
                    {"DEFAULT_DATETIME_DATE", "datetime_date"},
                    {"DEFAULT_DATETIME_JURISDICTION", "datetime_jurisdiction"},
                    {"DEFAULT_DATETIME_LATITUDE", "datetime_latitude"},
                    {"DEFAULT_DATETIME_LONGITUDE", "datetime_longitude"},
                    {"DEFAULT_DATETIME_ELEVATION", "datetime_elevation"},
                    {"DEFAULT_DATETIME_GMT_OFFSET", "datetime_gmt_offset"},
                    {"DEFAULT_ALMANAC_DATE", "almanac_date"},
                    {"DEFAULT_ALMANAC_TIME", "almanac_time"},
                    {"DEFAULT_ALMANAC_ZONE", "almanac_zone"},
                    {"DEFAULT_ALMANAC_LATITUDE", "almanac_latitude"},
                    {"DEFAULT_ALMANAC_LONGITUDE", "almanac_longitude"},
                    {"DEFAULT_ALMANAC_ELEVATION", "almanac_elevation"},
                    {"DEFAULT_ALMANAC_VISIBILITY", "almanac_visibility"}};
    for (size_t i = 0; i < sizeof mappings / sizeof *mappings; ++i)
        if (!lab_page_put_json(values, mappings[i].placeholder, lab_page_member(defaults, mappings[i].key)))
            goto fail;
    const char *name = getenv("MARS_LAB_APP_NAME"), *subtitle = getenv("MARS_LAB_SUBTITLE");
    const char *query = getenv("MARS_LAB_CONTROL_QUERY_PARAM");
    string_t *prefix = string_new_with(query ? query : "mars_lab_control");
    bool prefix_ok = prefix && !string_append_char(prefix, '=') &&
                     lab_page_put_json_text(values, "CONTROL_QUERY_PREFIX", string_c_str(prefix));
    string_free(prefix);
    const json_t *expression = lab_page_member(state, "expression");
    if (!expression || json_type(expression) != JSON_STRING)
        expression = lab_page_member(defaults, "expression");
    if (!prefix_ok || !lab_page_put_html(values, "LAB_NAME", name && *name ? name : "MARS Lab") ||
        !lab_page_put_html(values, "LAB_SUBTITLE",
                           subtitle && *subtitle ? subtitle
                                                 : lab_page_text_value(lab_page_member(constants, "LAB_SUBTITLE"))) ||
        !lab_page_put_html(values, "THEME_COLOR", "#071913") ||
        !lab_page_put_html(values, "INITIAL_EXPRESSION", lab_page_text_value(expression)) ||
        !lab_page_mobile_values(values, state) ||
        !lab_page_put_json_text(values, "CONTROL_TOKEN",
                                lab_page_text_value(lab_page_member(state, "control_token"))) ||
        !lab_page_set_text(values, "ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_MS", "22000") ||
        !lab_page_set_text(values, "HOLIDAY_JURISDICTION_OPTIONS", ""))
        goto fail;
    const char *notes[] = {"ALMANAC_COVERAGE_TEXT", "ALMANAC_ACCURACY_NOTE"};
    for (size_t i = 0; i < sizeof notes / sizeof *notes; ++i) {
        string_t *key = string_sprintf("%s_JS", notes[i]);
        bool ok = key && lab_page_put_json(values, string_c_str(key), lab_page_member(constants, notes[i])) &&
                  lab_page_put_html(values, notes[i], lab_page_text_value(lab_page_member(constants, notes[i])));
        string_free(key);
        if (!ok)
            goto fail;
    }
    return values;
fail:
    json_free(values);
    return NULL;
}

/* Reuse the native substitution values; no configuration is embedded as executable text. */
json_t *lab_page_bootstrap(const json_t *state)
{
    static const char *const keys[] = {
        "ALMANAC_ACCURACY_NOTE_JS",
        "ALMANAC_COVERAGE_TEXT_JS",
        "ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_MS",
        "ALMANAC_WORKSHEET_TITLE",
        "CONTROL_QUERY_PREFIX",
        "CONTROL_TOKEN",
        "DEFAULT_ALMANAC_DATE",
        "DEFAULT_ALMANAC_ELEVATION",
        "DEFAULT_ALMANAC_LATITUDE",
        "DEFAULT_ALMANAC_LONGITUDE",
        "DEFAULT_ALMANAC_TEXT",
        "DEFAULT_ALMANAC_TIME",
        "DEFAULT_ALMANAC_VISIBILITY",
        "DEFAULT_ALMANAC_ZONE",
        "DEFAULT_DATETIME_DATE",
        "DEFAULT_DATETIME_ELEVATION",
        "DEFAULT_DATETIME_GMT_OFFSET",
        "DEFAULT_DATETIME_JURISDICTION",
        "DEFAULT_DATETIME_LATITUDE",
        "DEFAULT_DATETIME_LONGITUDE",
        "DEFAULT_DATETIME_TEXT",
        "DEFAULT_DIFFEQUATION",
        "DEFAULT_EQUATION",
        "DEFAULT_EQUATION_VARIABLE",
        "DEFAULT_EXPRESSION",
        "DEFAULT_INTEGRATOR",
        "DEFAULT_INTEGRATOR_BOUNDS",
        "DEFAULT_INTEGRATOR_INTERVAL_CAP",
        "DEFAULT_MATRIX",
    };
    json_t *data = lab_page_catalogue();
    json_t *defaults = data ? lab_page_defaults_from(data) : NULL;
    json_t *values = defaults ? lab_page_substitutions(data, defaults, state) : NULL;
    json_t *result = values ? json_new_object() : NULL;
    /* This is compiled into the server, unlike assets read from disk after a rebuild.
     * Keep it in step with the WASM ABI; the browser integration test checks both. */
    if (result && !lab_page_set_text(result, "BROWSER_ABI_VERSION", "30")) {
        json_free(result);
        result = NULL;
    }
    /* Enumerate the bounded bootstrap schema once; member access remains indexed. */
    for (size_t i = 0; result && i < sizeof keys / sizeof *keys; ++i) {
        const string_t *text = json_string_value(lab_page_member(values, keys[i]));
        json_t *value = text ? json_from_text(text) : NULL;
        string_t *key = string_new_with(keys[i]);
        bool ok = value && key && json_object_set(result, key, value);
        json_free(value);
        string_free(key);
        if (!ok) {
            json_free(result);
            result = NULL;
        }
    }
    json_free(values);
    json_free(defaults);
    json_free(data);
    return result;
}

/* Render only original template tokens; never reinterpret substituted user text. */
string_t *lab_page_render(const json_t *state)
{
    string_t *asset = lab_page_read_template(), *out = NULL;
    string_cursor_t *cursor = NULL;
    json_t *data = lab_page_catalogue();
    json_t *defaults = data ? lab_page_defaults_from(data) : NULL;
    json_t *values = defaults ? lab_page_substitutions(data, defaults, state) : NULL;
    if (!values)
        goto done;
    out = string_new();
    cursor = string_cursor_new(asset);
    if (!out || !cursor)
        goto fail;
    string_pos_t pending = string_cursor_position(cursor);
    while (!string_cursor_done(cursor)) {
        string_pos_t start = string_cursor_position(cursor);
        if (!string_cursor_consume(cursor, "__")) {
            string_cursor_next(cursor);
            continue;
        }
        string_pos_t key_start = string_cursor_position(cursor);
        unsigned char ch;
        while (!string_cursor_match(cursor, "__") && string_cursor_peek_ascii(cursor, &ch) &&
               ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'))
            string_cursor_next(cursor);
        string_pos_t key_end = string_cursor_position(cursor);
        if (key_end == key_start || !string_cursor_consume(cursor, "__"))
            continue;
        string_t *key = string_cursor_slice_between(key_start, key_end, cursor);
        if (!key)
            goto fail;
        const json_t *value = json_object_get(values, key);
        string_free(key);
        if (!value || string_cursor_append_slice_between(out, pending, start, cursor) ||
            string_append_cstr(out, lab_page_text_value(value)))
            goto fail;
        pending = string_cursor_position(cursor);
    }
    if (string_cursor_append_slice_between(out, pending, string_cursor_position(cursor), cursor))
        goto fail;
    goto done;
fail:
    string_free(out);
    out = NULL;
done:
    string_cursor_free(cursor);
    json_free(values);
    json_free(defaults);
    json_free(data);
    string_free(asset);
    return out;
}
