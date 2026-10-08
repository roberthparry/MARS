/**
 * @file tba_page.c
 * @brief Safe initial HTML and client-configuration assembly.
 *
 * Native metadata populates the legacy workbench controls. All external strings
 * are escaped before insertion; the template engine consumes only original text.
 */
#include <stdlib.h>
#include <string.h>

#include "tba_forecast.h"
#include "tba_page.h"
#include "tba_series.h"

/* Substitute original placeholders, never placeholders inside replacement data. */
string_t *tba_page_expand(const string_t *text, const json_t *values)
{
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    string_t *result = string_new();
    bool ok = cursor && result;
    while (ok && !string_cursor_done(cursor)) {
        string_pos_t start = string_cursor_position(cursor);
        if (string_cursor_consume(cursor, "__")) {
            unsigned char first;
            if (!string_cursor_peek_ascii(cursor, &first) || first < 'A' || first > 'Z') {
                ok = string_append_cstr(result, "__") == 0;
                continue;
            }
            string_t *key = string_new();
            unsigned char ch;
            while (key && !string_cursor_done(cursor) && !string_cursor_match(cursor, "__") &&
                   string_byte_length(key) < 128 && string_cursor_peek_ascii(cursor, &ch) &&
                   ((ch >= 'A' && ch <= 'Z') || ch == '_')) {
                if (string_append_char(key, (char)ch))
                    break;
                string_cursor_next(cursor);
            }
            const json_t *value = key ? json_object_get(values, key) : NULL;
            const string_t *replacement = json_string_value(value);
            ok = replacement && string_cursor_consume(cursor, "__") && string_append_string(result, replacement) == 0;
            string_free(key);
        } else {
            string_cursor_next(cursor);
            ok = string_cursor_append_slice_between(result, start, string_cursor_position(cursor), cursor) == 0;
        }
    }
    string_cursor_free(cursor);
    if (!ok) {
        string_free(result);
        result = NULL;
    }
    return result;
}

static bool tba_page_escaped(json_t *values, const char *key, const char *text)
{
    string_t *escaped = tba_text_html(text);
    bool ok = escaped && tba_json_string(values, key, string_c_str(escaped));
    string_free(escaped);
    return ok;
}

static bool tba_page_owned(json_t *values, const char *key, string_t *text)
{
    bool ok = text && tba_json_string(values, key, string_c_str(text));
    string_free(text);
    return ok;
}

static string_t *tba_page_options(const json_t *items, const char *chosen, bool dates)
{
    string_t *result = string_new();
    if (!json_array_size(items)) {
        if (result)
            string_append_cstr(result, "<option value=\"\">Choose a value</option>");
        return result;
    }
    for (size_t i = 0; result && i < json_array_size(items); ++i) {
        const string_t *item = json_string_value(json_array_get(items, i));
        const char *value = item ? string_c_str(item) : "";
        string_t *escaped = tba_text_html(value);
        string_t *label = dates ? tba_date_text(tba_date_parse(item), true) : tba_text_html(value);
        bool ok = escaped && label &&
                  string_append_format(result, "<option value=\"%S\"%s>%S</option>", escaped,
                                       !strcmp(value, chosen ? chosen : "") ? " selected" : "", label) >= 0;
        string_free(escaped);
        string_free(label);
        if (!ok) {
            string_free(result);
            result = NULL;
        }
    }
    return result;
}

static json_t *tba_page_selection(const char *text)
{
    json_t *result = json_new_object();
    string_t *source = string_new_with(text ? text : "");
    size_t count = 0;
    string_t **parts = source ? string_split(source, ",", &count) : NULL;
    for (size_t i = 0; result && i < count; ++i) {
        string_trim(parts[i]);
        if (string_byte_length(parts[i]) && !tba_json_flag(result, string_c_str(parts[i]), true)) {
            json_free(result);
            result = NULL;
        }
    }
    string_split_free(parts, count);
    string_free(source);
    return result;
}

static string_t *tba_page_checks(const json_t *items, const char *selection, const char *kind)
{
    json_t *selected = tba_page_selection(selection);
    string_t *result = string_new();
    for (size_t i = 0; result && selected && i < json_array_size(items); ++i) {
        const json_t *item = json_array_get(items, i);
        bool driver = !strcmp(kind, "xreg-column-choice"), outlier = !strcmp(kind, "outlier-choice");
        const char *value = driver    ? tba_json_text(item, "name")
                            : outlier ? tba_json_text(item, "date_iso")
                                      : string_c_str(json_string_value(item));
        bool usable = !driver || tba_json_bool(item, "usable");
        bool checked = usable && (tba_json_bool(selected, value) || (outlier && !json_object_size(selected)));
        string_t *escaped = tba_text_html(value);
        string_t *label = tba_text_html(driver    ? value
                                        : outlier ? tba_json_text(item, "date")
                                                  : tba_forecast_label(value));
        string_t *title = tba_text_html(driver ? tba_json_text(item, "reason") : value);
        bool ok = escaped && label && title &&
                  string_append_format(
                      result,
                      "<label class=\"multi-select-item%s\" title=\"%S\"><input type=\"checkbox\" name=\"%s\" "
                      "value=\"%S\"%s%s><span>%S</span></label>",
                      usable ? "" : " is-disabled", title, kind, escaped, checked ? " checked" : "",
                      usable ? "" : " disabled", label) >= 0;
        string_free(escaped);
        string_free(label);
        string_free(title);
        if (!ok) {
            string_free(result);
            result = NULL;
        }
    }
    if (!selected) {
        string_free(result);
        result = NULL;
    }
    json_free(selected);
    return result;
}

static string_t *tba_page_metadata(const json_t *meta)
{
    static const char *const keys[] = {
        "detected_frequency_label", "date_column",     "value_column",     "start_date", "end_date",
        "usable_start_date",        "usable_end_date", "seasonality_label"};
    static const char *const labels[] = {"Detected frequency", "Date column",  "Series",     "File start",
                                         "File end",           "Usable start", "Usable end", "Seasonality"};
    string_t *result = string_new();
    for (size_t i = 0; result && i < 8; ++i) {
        string_t *value = tba_text_html(tba_json_text(meta, keys[i]));
        bool ok = value && string_append_format(result, "<div><strong>%s:</strong> %S</div>", labels[i], value) >= 0;
        string_free(value);
        if (!ok) {
            string_free(result);
            result = NULL;
        }
    }
    return result;
}

/* Supply the existing control and client-state contracts from native data. */
string_t *tba_page_render(const tba_app_t *app, const json_t *mobile)
{
    json_t *state = tba_app_state(app), *values = json_new_object(), *config = json_new_object();
    tba_series_t *target =
        state ? tba_series_open(tba_json_text(state, "target_path"), tba_json_text(state, "target_date_column")) : NULL;
    tba_series_t *drivers =
        state ? tba_series_open(tba_json_text(state, "xreg_path"), tba_json_text(state, "xreg_date_column")) : NULL;
    json_t *target_meta = tba_series_details(target, tba_json_text(state, "target_value_column"), "");
    json_t *driver_meta = tba_series_details(drivers, "", tba_json_text(state, "xreg_columns"));
    json_t *target_map = tba_series_targets(target);
    json_t *empty = json_new_array(), *models = json_new_array();
    bool ok = state && values && config && target_meta && driver_meta && target_map && empty && models;
    for (size_t i = 0; ok && i < 6; ++i)
        ok = tba_json_append(models, tba_forecast_model(i));
    const json_t *target_columns = tba_json_get(target_meta, "value_columns");
    const json_t *driver_columns = tba_json_get(driver_meta, "value_column_details");
    const json_t *outliers = tba_json_get(target_meta, "outliers");
    if (!target_columns)
        target_columns = empty;
    if (!driver_columns)
        driver_columns = empty;
    if (!outliers)
        outliers = empty;
    json_t *ends = tba_series_ends(target_meta, drivers, tba_json_text(state, "xreg_columns"),
                                   !strcmp(tba_json_text(state, "year_type"), "fiscal"));
    static const struct {
        const char *token, *key;
    } fields[] = {{"TARGET_PATH", "target_path"},
                  {"TARGET_DISPLAY_NAME", "target_display_name"},
                  {"TARGET_UPLOAD_LABEL", "target_display_name"},
                  {"TARGET_VALUE_COLUMN", "target_value_column"},
                  {"XREG_PATH", "xreg_path"},
                  {"XREG_DISPLAY_NAME", "xreg_display_name"},
                  {"XREG_UPLOAD_LABEL", "xreg_display_name"},
                  {"XREG_COLUMNS", "xreg_columns"},
                  {"MODELS", "models"},
                  {"MODEL_SUMMARY", "models"},
                  {"OUTLIER_DATES", "outlier_dates"},
                  {"FREQUENCY", "frequency"},
                  {"LEVEL", "level"},
                  {"P", "p"},
                  {"D_LOWER", "d"},
                  {"Q", "q"},
                  {"P_SEASONAL", "P"},
                  {"D_SEASONAL", "D"},
                  {"Q_SEASONAL", "Q"}};
    for (size_t i = 0; ok && i < sizeof fields / sizeof *fields; ++i)
        ok = tba_page_escaped(values, fields[i].token, tba_json_text(state, fields[i].key));
    ok = ok && ends && tba_json_string(config, "base_path", tba_app_base(app)) &&
         tba_json_set(config, "state", state) && tba_json_set(config, "target_columns", target_columns) &&
         tba_json_set(config, "target_meta_map", target_map) && tba_json_set(config, "xreg_columns", driver_columns) &&
         tba_json_set(config, "target_meta", target_meta) && tba_json_set(config, "xreg_meta", driver_meta) &&
         tba_json_set(config, "outliers", outliers) && tba_json_string(config, "control_token", "") &&
         tba_page_owned(values, "CLIENT_CONFIG", tba_text_script(config)) &&
         tba_page_escaped(values, "BASE_PATH", tba_app_base(app)) &&
         tba_page_escaped(values, "MOBILE_TITLE", tba_json_text(mobile, "title")) &&
         tba_page_escaped(values, "MOBILE_HINT", tba_json_text(mobile, "hint")) &&
         tba_page_escaped(values, "MOBILE_URL", tba_json_text(mobile, "url")) &&
         tba_json_string(values, "MOBILE_QR_SVG", tba_json_text(mobile, "qr")) &&
         tba_page_owned(values, "TARGET_META_HTML", tba_page_metadata(target_meta)) &&
         tba_page_owned(values, "XREG_META_HTML", tba_page_metadata(driver_meta)) &&
         tba_page_owned(values, "TARGET_VALUE_OPTIONS",
                        tba_page_options(target_columns, tba_json_text(state, "target_value_column"), false)) &&
         tba_page_owned(values, "TARGET_DATE_OPTIONS",
                        tba_page_options(tba_json_get(target_meta, "date_candidates"),
                                         tba_json_text(target_meta, "date_column"), false)) &&
         tba_page_owned(values, "XREG_DATE_OPTIONS",
                        tba_page_options(tba_json_get(driver_meta, "date_candidates"),
                                         tba_json_text(driver_meta, "date_column"), false)) &&
         tba_page_owned(values, "FORECAST_END_OPTIONS",
                        tba_page_options(ends, tba_json_text(state, "forecast_end_date"), true)) &&
         tba_page_owned(values, "MODEL_PICKER_HTML",
                        tba_page_checks(models, tba_json_text(state, "models"), "model-choice")) &&
         tba_page_owned(values, "XREG_PICKER_HTML",
                        tba_page_checks(driver_columns, tba_json_text(state, "xreg_columns"), "xreg-column-choice")) &&
         tba_page_owned(values, "OUTLIER_PICKER_HTML",
                        tba_page_checks(outliers, tba_json_text(state, "outlier_dates"), "outlier-choice")) &&
         tba_json_string(values, "OUTLIER_PANEL_CLASS", json_array_size(outliers) ? "" : "no-outliers") &&
         tba_json_string(values, "OUTLIER_STATUS",
                         json_array_size(outliers) ? "Review the flagged observations." : "No outliers detected.") &&
         tba_page_escaped(values, "DETECTED_FREQUENCY_LABEL", tba_json_text(target_meta, "detected_frequency_label")) &&
         tba_page_escaped(values, "TARGET_PICKER_STATUS", tba_json_text(state, "target_value_column")) &&
         tba_page_escaped(values, "XREG_SUMMARY_HTML", tba_json_text(state, "xreg_columns")) &&
         tba_json_string(values, "XREG_PICKER_STATUS", "Choose the exogenous columns to use.");
    json_t *seasons = json_new_array();
    static const char *const season_values[] = {"0", "1", "2", "3", "4", "6", "7", "12", "14", "30", "365"};
    for (size_t i = 0; ok && i < sizeof season_values / sizeof *season_values; ++i)
        ok = seasons && tba_json_append(seasons, season_values[i]);
    ok = ok && tba_page_owned(values, "SEASON_PERIOD_OPTIONS",
                              tba_page_options(seasons, tba_json_text(state, "season_period"), false));
    string_t *path = tba_asset_path("index.html");
    string_t *template = path ? tba_file_read(string_c_str(path), 1024 * 1024) : NULL;
    string_t *page = ok && template ? tba_page_expand(template, values) : NULL;
    string_free(template);
    string_free(path);
    json_free(seasons);
    json_free(ends);
    json_free(state);
    json_free(values);
    json_free(config);
    json_free(target_meta);
    json_free(driver_meta);
    json_free(target_map);
    json_free(empty);
    json_free(models);
    tba_series_free(target);
    tba_series_free(drivers);
    return page;
}
