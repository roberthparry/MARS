/**
 * @file tba_forecast_view.c
 * @brief Native table rendering and structured chart data for Protobuf responses.
 *
 * Parses exported CSV with the shared string-cursor CSV module, escapes every
 * displayed cell, and joins model series by indexed dates. Browser code receives
 * finite numeric values or nulls, never mathematics or CSV to reinterpret.
 */
#include <string.h>

#include "tba_csv.h"
#include "tba_dates.h"
#include "tba_forecast.h"

static bool tba_forecast_view_cell(string_t *html, const char *tag, const char *text)
{
    string_t *escaped = tba_text_html(text ? text : "");
    bool ok = escaped && string_append_format(html, "<%s>%S</%s>", tag, escaped, tag) >= 0;
    string_free(escaped);
    return ok;
}

static bool tba_forecast_view_table(json_t *result)
{
    string_t *source = string_new_with(tba_json_text(result, "forecast_csv"));
    tba_csv_t *csv = source ? tba_csv_parse(source) : NULL;
    string_t *head = string_new_with("<tr>"), *body = string_new();
    bool ok = csv && head && body;
    for (size_t c = 0; ok && c < tba_csv_columns(csv); ++c)
        ok = tba_forecast_view_cell(head, "th", tba_csv_header(csv, c));
    ok = ok && string_append_cstr(head, "</tr>") == 0;
    for (size_t r = 0; ok && r < tba_csv_rows(csv); ++r) {
        ok = string_append_cstr(body, "<tr>") == 0;
        for (size_t c = 0; ok && c < tba_csv_columns(csv); ++c) {
            const string_t *cell = tba_csv_cell(csv, r, c);
            ok = tba_forecast_view_cell(body, "td", cell ? string_c_str(cell) : "");
        }
        ok = ok && string_append_cstr(body, "</tr>") == 0;
    }
    ok = ok && tba_json_string(result, "table_head", string_c_str(head)) &&
         tba_json_string(result, "table_body", string_c_str(body));
    tba_csv_free(csv);
    string_free(source);
    string_free(head);
    string_free(body);
    return ok;
}

static bool tba_forecast_view_number(json_t *array, const string_t *text)
{
    double value;
    json_t *number = text && tba_text_number(text, &value) ? json_new_number(text) : json_new_null();
    bool ok = number && json_array_append(array, number);
    json_free(number);
    return ok;
}

static json_t *tba_forecast_view_index(const json_t *result, json_t *dates, json_t *seen)
{
    string_t *source = string_new_with(tba_json_text(result, "forecast_csv"));
    tba_csv_t *csv = source ? tba_csv_parse(source) : NULL;
    json_t *index = json_new_object();
    size_t date_column = tba_csv_column(csv, "date");
    static const char *const names[] = {"actual", "mean", "lower", "upper"};
    size_t columns[4];
    for (size_t i = 0; i < 4; ++i)
        columns[i] = tba_csv_column(csv, names[i]);
    bool ok = csv && index && date_column != SIZE_MAX;
    for (size_t r = 0; ok && r < tba_csv_rows(csv); ++r) {
        const string_t *date = tba_csv_cell(csv, r, date_column);
        if (!date || !string_byte_length(date))
            continue;
        json_t *row = json_new_object();
        ok = row != NULL;
        for (size_t i = 0; ok && i < 4; ++i) {
            const string_t *cell = tba_csv_cell(csv, r, columns[i]);
            ok = tba_json_string(row, names[i], cell ? string_c_str(cell) : "");
        }
        if (ok && !json_object_get(seen, date))
            ok = json_object_set(seen, date, row) && tba_json_append(dates, string_c_str(date));
        json_t *first = json_object_get_mutable(seen, date);
        if (ok && first && !*tba_json_text(first, "actual"))
            ok = tba_json_string(first, "actual", tba_json_text(row, "actual"));
        ok = ok && json_object_set(index, date, row);
        json_free(row);
    }
    tba_csv_free(csv);
    string_free(source);
    if (!ok) {
        json_free(index);
        index = NULL;
    }
    return index;
}

static bool tba_forecast_view_chart(json_t *result)
{
    const json_t *results = tba_json_get(result, "results");
    size_t count = tba_json_bool(result, "multi") ? json_array_size(results) : 1;
    if (count > 6)
        return false;
    json_t *chart = json_new_object(), *dates = json_new_array(), *seen = json_new_object();
    json_t *actual = json_new_array(), *times = json_new_array(), *series = json_new_array();
    json_t *indexes[6] = {0};
    bool ok = chart && dates && seen && actual && times && series;
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *item = results ? json_array_get(results, i) : result;
        if (tba_json_bool(item, "ok")) {
            indexes[i] = tba_forecast_view_index(item, dates, seen);
            ok = indexes[i] != NULL;
        }
    }
    for (size_t d = 0; ok && d < json_array_size(dates); ++d) {
        const string_t *date = json_string_value(json_array_get(dates, d));
        const json_t *row = json_object_get(seen, date);
        string_t *value = string_new_with(tba_json_text(row, "actual"));
        ok = tba_forecast_view_number(actual, value);
        string_free(value);
        int parsed = tba_date_parse(date);
        string_t *stamp = parsed ? string_sprintf("%.0f", (tba_date_jdn(parsed) - 2440588.0) * 86400000.0) : NULL;
        json_t *time = stamp ? json_new_number(stamp) : json_new_null();
        ok = ok && time && json_array_append(times, time);
        string_free(stamp);
        json_free(time);
    }
    static const char *const fields[] = {"mean", "lower", "upper"};
    static const char *const keys[] = {"values", "lower", "upper"};
    for (size_t i = 0; ok && i < count; ++i) {
        if (!indexes[i])
            continue;
        const json_t *item = results ? json_array_get(results, i) : result;
        json_t *model = json_new_object();
        ok = model && tba_json_string(model, "label", tba_json_text(item, "model"));
        for (size_t f = 0; ok && f < 3; ++f) {
            json_t *values = json_new_array();
            ok = values != NULL;
            for (size_t d = 0; ok && d < json_array_size(dates); ++d) {
                const string_t *date = json_string_value(json_array_get(dates, d));
                const json_t *row = json_object_get(indexes[i], date);
                string_t *value = string_new_with(tba_json_text(row, fields[f]));
                ok = tba_forecast_view_number(values, value);
                string_free(value);
            }
            ok = ok && tba_json_set(model, keys[f], values);
            json_free(values);
        }
        ok = ok && json_array_append(series, model);
        json_free(model);
    }
    int end = tba_date_cstr(tba_json_text(result, "forecast_end_date"));
    ok = ok && tba_json_set(chart, "labels", dates) && tba_json_set(chart, "actual", actual) &&
         tba_json_set(chart, "dates", times) && tba_json_set(chart, "series", series) &&
         tba_json_number(chart, "end", end ? (tba_date_jdn(end) - 2440588.0) * 86400000.0 : 0) &&
         tba_json_string(chart, "forecastEndLabel", tba_json_text(result, "forecast_end_label")) &&
         tba_json_set(result, "chart", chart);
    for (size_t i = 0; i < 6; ++i)
        json_free(indexes[i]);
    json_free(chart);
    json_free(dates);
    json_free(seen);
    json_free(actual);
    json_free(times);
    json_free(series);
    return ok;
}

/* Supply ready-to-display tables and typed numeric chart series. */
bool tba_forecast_view(json_t *result)
{
    return json_type(result) == JSON_OBJECT && tba_forecast_view_table(result) && tba_forecast_view_chart(result);
}
