/**
 * @file tba_series.c
 * @brief Date-column discovery, usable ranges and forecasting metadata.
 *
 * Scans each dated CSV once to index dates, and uses direct column lookup for
 * selected observations. Candidate scoring is bounded to 24 rows per column;
 * frequency detection sorts unique dates rather than assuming input order.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tba_series_internal.h"

typedef struct {
    size_t index;
    unsigned parsed, hint;
    const char *name;
} tba_series_candidate_t;

static int tba_series_candidate_compare(const void *a, const void *b)
{
    const tba_series_candidate_t *left = a, *right = b;
    if (left->hint != right->hint)
        return left->hint > right->hint ? -1 : 1;
    if (left->parsed != right->parsed)
        return left->parsed > right->parsed ? -1 : 1;
    return strcmp(left->name, right->name);
}

static int tba_series_date_compare(const void *a, const void *b)
{
    int left = *(const int *)a, right = *(const int *)b;
    return (left > right) - (left < right);
}

static bool tba_series_detect(tba_series_t *series, const char *requested)
{
    tba_series_candidate_t candidates[256];
    size_t count = 0, rows = tba_csv_rows(series->csv);
    for (size_t c = 0; c < tba_csv_columns(series->csv); ++c) {
        const char *name = tba_csv_header(series->csv, c);
        string_t *lower = string_new_with(name);
        if (!lower)
            return false;
        string_to_lower(lower);
        bool hint = string_find(lower, "date") >= 0 || string_find(lower, "month") >= 0 ||
                    string_find(lower, "period") >= 0 || string_find(lower, "time") >= 0 ||
                    string_find(lower, "year") >= 0;
        string_free(lower);
        unsigned seen = 0, parsed = 0;
        for (size_t r = 0; r < rows && r < 24; ++r) {
            const string_t *cell = tba_csv_cell(series->csv, r, c);
            if (cell && !string_view_is_empty(string_view_trim(string_view_all(cell)))) {
                seen++;
                parsed += tba_date_parse(cell) != 0;
            }
        }
        if (parsed && (parsed * 5 >= seen * 3 || hint))
            candidates[count++] = (tba_series_candidate_t){c, parsed, hint, name};
    }
    qsort(candidates, count, sizeof *candidates, tba_series_candidate_compare);
    for (size_t i = 0; i < count; ++i)
        if (!tba_json_append(series->date_candidates, candidates[i].name))
            return false;
    size_t chosen = requested && *requested ? tba_csv_column(series->csv, requested) : SIZE_MAX;
    if (requested && *requested && chosen == SIZE_MAX)
        return false;
    series->date_column = chosen != SIZE_MAX ? chosen : count ? candidates[0].index : 0;
    return true;
}

static bool tba_series_index_dates(tba_series_t *series)
{
    size_t rows = tba_csv_rows(series->csv), count = 0;
    series->dates = calloc(rows ? rows : 1, sizeof *series->dates);
    int *ordered = malloc((rows ? rows : 1) * sizeof *ordered);
    if (!series->dates || !ordered) {
        free(ordered);
        return false;
    }
    for (size_t r = 0; r < rows; ++r) {
        int date = tba_date_parse(tba_csv_cell(series->csv, r, series->date_column));
        series->dates[r] = date;
        if (date)
            ordered[count++] = date;
    }
    qsort(ordered, count, sizeof *ordered, tba_series_date_compare);
    if (count) {
        series->first = ordered[0];
        series->last = ordered[count - 1];
    }
    bool daily = true, monthly = true, quarterly = true, yearly = true;
    size_t distinct = count ? 1 : 0;
    for (size_t i = 1; i < count; ++i) {
        int a = ordered[i - 1], b = ordered[i];
        if (a == b)
            continue;
        distinct++;
        int step = (b / 10000 - a / 10000) * 12 + b / 100 % 100 - a / 100 % 100;
        daily = daily && tba_date_jdn(b) - tba_date_jdn(a) == 1;
        monthly = monthly && step == 1;
        quarterly = quarterly && step == 3;
        yearly = yearly && step == 12;
    }
    series->frequency = distinct < 2 ? "unknown" : daily ? "daily" : monthly ? "monthly" :
                        quarterly ? "quarterly" : yearly ? "yearly" : "unknown";
    free(ordered);
    return true;
}

/* Load one owned dated document. */
tba_series_t *tba_series_open(const char *path, const char *date_column)
{
    tba_series_t *series = calloc(1, sizeof *series);
    if (!series)
        return NULL;
    series->path = tba_path(path);
    series->csv = series->path ? tba_csv_open(string_c_str(series->path)) : NULL;
    series->date_candidates = json_new_array();
    if (!series->csv || !series->date_candidates || !tba_series_detect(series, date_column) ||
        !tba_series_index_dates(series)) {
        tba_series_free(series);
        return NULL;
    }
    return series;
}

/* Release the document and date index. */
void tba_series_free(tba_series_t *series)
{
    if (series) {
        tba_csv_free(series->csv);
        json_free(series->date_candidates);
        string_free(series->path);
        free(series->dates);
        free(series);
    }
}

/* Store both browser date representations. */
bool tba_series_date_fields(json_t *object, const char *stem, int date)
{
    string_t *iso = tba_date_text(date, false), *uk = tba_date_text(date, true);
    string_t *key = string_sprintf("%s_iso", stem);
    bool ok = iso && uk && key && tba_json_string(object, stem, string_c_str(uk)) &&
              tba_json_string(object, string_c_str(key), string_c_str(iso));
    string_free(iso);
    string_free(uk);
    string_free(key);
    return ok;
}

/* Resolve selected headers once; invalid selections fail instead of silently dropping drivers. */
size_t *tba_series_columns(const tba_series_t *series, const char *selection, size_t *count)
{
    *count = 0;
    string_t *text = string_new_with(selection ? selection : "");
    size_t n = 0;
    string_t **parts = text ? string_split(text, ",", &n) : NULL;
    size_t *columns = n <= 256 ? calloc(n ? n : 1, sizeof *columns) : NULL;
    bool ok = series && text && columns;
    for (size_t i = 0; ok && i < n; ++i) {
        string_trim(parts[i]);
        if (!string_byte_length(parts[i]))
            continue;
        size_t index = tba_csv_column(series->csv, string_c_str(parts[i]));
        ok = index != SIZE_MAX && index != series->date_column;
        if (ok)
            columns[(*count)++] = index;
    }
    string_split_free(parts, n);
    string_free(text);
    if (!ok) {
        free(columns);
        *count = 0;
        return NULL;
    }
    return columns;
}

static bool tba_series_range(const tba_series_t *series, const size_t *columns, size_t count, int *first, int *last)
{
    *first = *last = 0;
    for (size_t r = 0; count && r < tba_csv_rows(series->csv); ++r) {
        bool valid = series->dates[r] != 0;
        for (size_t c = 0; valid && c < count; ++c) {
            double value;
            valid = tba_text_number(tba_csv_cell(series->csv, r, columns[c]), &value);
        }
        if (valid) {
            if (!*first || series->dates[r] < *first)
                *first = series->dates[r];
            if (series->dates[r] > *last)
                *last = series->dates[r];
        }
    }
    return *first != 0;
}

static json_t *tba_series_driver(const tba_series_t *series, size_t column)
{
    int first, last;
    bool usable = tba_series_range(series, &column, 1, &first, &last);
    for (size_t r = 0; usable && r < tba_csv_rows(series->csv); ++r) {
        double value;
        if (series->dates[r] >= first && series->dates[r] <= last &&
            !tba_text_number(tba_csv_cell(series->csv, r, column), &value))
            usable = false;
    }
    json_t *result = json_new_object();
    string_t *start = tba_date_text(first, true), *end = tba_date_text(last, true);
    string_t *reason = start && end ? string_sprintf(usable ? "Usable from %S to %S." :
                           first ? "Unusable: this column has gaps between %S and %S." :
                                   "Unusable: this column has no numeric values.", start, end) : NULL;
    bool ok = result && reason && tba_json_string(result, "name", tba_csv_header(series->csv, column)) &&
              tba_json_flag(result, "usable", usable) && tba_json_string(result, "reason", string_c_str(reason)) &&
              tba_series_date_fields(result, "start_date", first) && tba_series_date_fields(result, "end_date", last);
    string_free(start);
    string_free(end);
    string_free(reason);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

/* Describe dates and observation quality in the existing browser contract. */
json_t *tba_series_details(const tba_series_t *series, const char *target, const char *drivers)
{
    if (!series)
        return json_new_object();
    json_t *result = json_new_object(), *headers = json_new_array(), *values = json_new_array();
    json_t *details = json_new_array();
    bool ok = result && headers && values && details;
    for (size_t c = 0; ok && c < tba_csv_columns(series->csv); ++c) {
        ok = tba_json_append(headers, tba_csv_header(series->csv, c));
        if (c == series->date_column)
            continue;
        json_t *detail = target && *target ? json_new_object() : tba_series_driver(series, c);
        ok = ok && detail && tba_json_append(values, tba_csv_header(series->csv, c)) && json_array_append(details, detail);
        json_free(detail);
    }
    size_t count = 0;
    size_t *columns = tba_series_columns(series, target && *target ? target : drivers, &count);
    int first = 0, last = 0;
    if (columns)
        tba_series_range(series, columns, count, &first, &last);
    ok = ok && columns && tba_json_set(result, "header", headers) && tba_json_set(result, "value_columns", values) &&
         tba_json_set(result, "value_column_details", details) && tba_json_set(result, "date_candidates", series->date_candidates) &&
         tba_json_string(result, "date_column", tba_csv_header(series->csv, series->date_column)) &&
         tba_json_flag(result, "date_column_in_first_position", series->date_column == 0) &&
         tba_json_string(result, "detected_frequency", series->frequency) &&
         tba_json_string(result, "detected_frequency_label", tba_date_frequency_label(series->frequency)) &&
         tba_series_date_fields(result, "start_date", series->first) && tba_series_date_fields(result, "end_date", series->last) &&
         tba_series_date_fields(result, "usable_start_date", first) && tba_series_date_fields(result, "usable_end_date", last) &&
         tba_json_string(result, "path", string_c_str(series->path)) &&
         tba_json_string(result, "value_column", target ? target : "") &&
         tba_json_string(result, "selected_columns_text", drivers ? drivers : "") &&
         tba_series_statistics(series, target && *target ? tba_csv_column(series->csv, target) : SIZE_MAX, result);
    free(columns);
    json_free(headers);
    json_free(values);
    json_free(details);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

/* Build per-target cached browser metadata. */
json_t *tba_series_targets(const tba_series_t *series)
{
    json_t *result = json_new_object();
    for (size_t c = 0; result && series && c < tba_csv_columns(series->csv); ++c) {
        if (c == series->date_column)
            continue;
        const char *name = tba_csv_header(series->csv, c);
        json_t *detail = tba_series_details(series, name, "");
        bool ok = detail && tba_json_set(result, name, detail);
        json_free(detail);
        if (!ok) {
            json_free(result);
            return NULL;
        }
    }
    return result;
}
