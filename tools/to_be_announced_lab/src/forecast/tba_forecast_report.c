/**
 * @file tba_forecast_report.c
 * @brief Escaped forecast summaries and indexed multi-model table joins.
 *
 * Keeps native worker text authoritative. Results are joined by date using JSON
 * object indexes, with one pair of mean/error columns per successful model.
 * No source CSV, model name or worker diagnostic is inserted as raw markup.
 */
#include <stdlib.h>
#include <string.h>

#include "tba_csv.h"
#include "tba_forecast.h"

/* Render every native diagnostic without interpreting it as HTML. */
string_t *tba_forecast_summary(const char *text, const char *drivers)
{
    (void)drivers;
    string_t *source = string_new_with(text ? text : ""), *result = string_new();
    size_t count = 0;
    string_t **lines = source ? string_split(source, "\n", &count) : NULL;
    bool ok = source && result;
    static const char *const ratings[] = {"exceptional", "excellent", "very good",      "good",
                                          "mediocre",    "poor",      "comparison only"};
    static const char *const classes[] = {"exceptional", "excellent", "very-good", "good",
                                          "mediocre",    "poor",      "comparison"};
    for (size_t i = 0; ok && i < count; ++i) {
        string_t *escaped = tba_text_html(string_c_str(lines[i]));
        string_t *lower = string_new_with(string_c_str(lines[i]));
        if (lower)
            string_to_lower(lower);
        const char *rating = "";
        for (size_t r = 0; lower && r < 7; ++r) {
            string_t *marker = string_sprintf("(%s", ratings[r]);
            bool match = marker && string_find(lower, string_c_str(marker)) >= 0;
            string_free(marker);
            if (match) {
                rating = classes[r];
                break;
            }
        }
        ok = escaped && lower &&
             string_append_format(result, "<div class=\"summary-line%s%s\">%S</div>", *rating ? " rating-" : "", rating,
                                  escaped) >= 0;
        string_free(escaped);
        string_free(lower);
    }
    string_split_free(lines, count);
    string_free(source);
    if (!ok) {
        string_free(result);
        result = NULL;
    }
    return result;
}

static bool tba_forecast_csv_field(string_t *text, const char *value, bool comma)
{
    string_t *escaped = string_new_with(value ? value : "");
    bool quote = escaped && (string_find(escaped, ",") >= 0 || string_find(escaped, "\"") >= 0 ||
                             string_find(escaped, "\n") >= 0 || string_find(escaped, "\r") >= 0);
    bool ok =
        escaped && string_replace(escaped, "\"", "\"\"") >= 0 &&
        string_append_format(text, "%s%s%S%s", comma ? "," : "", quote ? "\"" : "", escaped, quote ? "\"" : "") >= 0;
    string_free(escaped);
    return ok;
}

static const char *tba_forecast_cell(const tba_csv_t *csv, size_t row, const char *header)
{
    const string_t *text = tba_csv_cell(csv, row, tba_csv_column(csv, header));
    return text ? string_c_str(text) : "";
}

static string_t *tba_forecast_join_csv(const json_t *results)
{
    json_t *index = json_new_object(), *dates = json_new_array();
    string_t *text = string_new_with("date,actual");
    bool ok = index && dates && text;
    for (size_t i = 0; ok && i < json_array_size(results); ++i) {
        const json_t *result = json_array_get(results, i);
        if (!tba_json_bool(result, "ok"))
            continue;
        const char *model = tba_json_text(result, "model");
        string_t *mean_label = string_sprintf("%s mean", model), *error_label = string_sprintf("%s stderr", model);
        ok = mean_label && error_label && tba_forecast_csv_field(text, string_c_str(mean_label), true) &&
             tba_forecast_csv_field(text, string_c_str(error_label), true);
        string_free(mean_label);
        string_free(error_label);
        string_t *source = string_new_with(tba_json_text(result, "forecast_csv"));
        tba_csv_t *csv = source ? tba_csv_parse(source) : NULL;
        ok = ok && csv;
        for (size_t r = 0; ok && r < tba_csv_rows(csv); ++r) {
            const char *date = tba_forecast_cell(csv, r, "date");
            if (!*date)
                continue;
            string_t *key = string_new_with(date);
            json_t *row = key ? json_object_get_mutable(index, key) : NULL;
            if (!row && key) {
                json_t *empty = json_new_object();
                ok = empty && json_object_set(index, key, empty) && tba_json_append(dates, date);
                json_free(empty);
                row = json_object_get_mutable(index, key);
            }
            json_t *entry = json_new_object();
            ok = ok && row && entry && tba_json_string(entry, "mean", tba_forecast_cell(csv, r, "mean")) &&
                 tba_json_string(entry, "stderr", tba_forecast_cell(csv, r, "stderr")) &&
                 tba_json_set(row, model, entry);
            if (ok && !*tba_json_text(row, "actual"))
                ok = tba_json_string(row, "actual", tba_forecast_cell(csv, r, "actual"));
            json_free(entry);
            string_free(key);
        }
        tba_csv_free(csv);
        string_free(source);
    }
    ok = ok && string_append_cstr(text, "\n") == 0;
    for (size_t d = 0; ok && d < json_array_size(dates); ++d) {
        const char *date = string_c_str(json_string_value(json_array_get(dates, d)));
        const json_t *row = tba_json_get(index, date);
        ok = tba_forecast_csv_field(text, date, false) &&
             tba_forecast_csv_field(text, tba_json_text(row, "actual"), true);
        for (size_t i = 0; ok && i < json_array_size(results); ++i) {
            const json_t *result = json_array_get(results, i);
            if (!tba_json_bool(result, "ok"))
                continue;
            const json_t *entry = tba_json_get(row, tba_json_text(result, "model"));
            ok = tba_forecast_csv_field(text, tba_json_text(entry, "mean"), true) &&
                 tba_forecast_csv_field(text, tba_json_text(entry, "stderr"), true);
        }
        ok = ok && string_append_cstr(text, "\n") == 0;
    }
    json_free(index);
    json_free(dates);
    if (!ok) {
        string_free(text);
        text = NULL;
    }
    return text;
}

/* Preserve partial model failures while presenting all successful forecasts. */
json_t *tba_forecast_combine(const json_t *results)
{
    const json_t *first = NULL;
    for (size_t i = 0; !first && i < json_array_size(results); ++i)
        if (tba_json_bool(json_array_get(results, i), "ok"))
            first = json_array_get(results, i);
    json_t *combined =
        first ? json_clone(first) : tba_json_error("None of the selected models completed successfully.");
    string_t *summary = string_new_with("Multi-model comparison summary\n");
    string_t *tabs =
        string_new_with("<div class=\"comparison-tabs\"><div class=\"comparison-tab-list\" role=\"tablist\">");
    string_t *panels = string_new();
    bool ok = combined && summary && tabs && panels && tba_json_flag(combined, "multi", true) &&
              tba_json_set(combined, "results", results);
    for (size_t i = 0; ok && i < json_array_size(results); ++i) {
        const json_t *result = json_array_get(results, i);
        bool success = tba_json_bool(result, "ok");
        const char *label = tba_json_text(result, "model");
        const char *body = tba_json_text(result, success ? "summary_text" : "error");
        string_t *escaped = tba_text_html(label);
        string_t *rendered = tba_forecast_summary(body, tba_json_text(result, "xreg_columns_used"));
        ok = escaped && rendered &&
             string_append_format(summary, "\n=== %s ===\nRun status: %s\n%s\n", label, success ? "OK" : "Failed",
                                  body) >= 0 &&
             string_append_format(tabs,
                                  "<button type=\"button\" class=\"comparison-tab%s\" role=\"tab\" "
                                  "aria-selected=\"%s\" data-tab-target=\"comparison-summary-panel-%zu\">%S</button>",
                                  i ? "" : " is-active", i ? "false" : "true", i + 1, escaped) >= 0 &&
             string_append_format(panels,
                                  "<div id=\"comparison-summary-panel-%zu\" class=\"comparison-tab-panel%s\" "
                                  "role=\"tabpanel\"><section class=\"comparison-run\"><h4>%S</h4>%S</section></div>",
                                  i + 1, i ? "" : " is-active", escaped, rendered) >= 0;
        string_free(escaped);
        string_free(rendered);
    }
    string_t *csv = first ? tba_forecast_join_csv(results) : string_new();
    ok = ok && csv && string_append_format(tabs, "</div>%S</div>", panels) >= 0 &&
         tba_json_string(combined, "summary_text", string_c_str(summary)) &&
         tba_json_string(combined, "summary_html", string_c_str(tabs)) &&
         tba_json_string(combined, "forecast_csv", string_c_str(csv)) &&
         tba_json_string(combined, "model", "Multiple models") && tba_json_string(combined, "fit_rows", "Varies");
    string_free(csv);
    string_free(summary);
    string_free(tabs);
    string_free(panels);
    if (!ok) {
        json_free(combined);
        combined = NULL;
    }
    return combined;
}
