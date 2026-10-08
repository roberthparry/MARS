/**
 * @file tba_series_forecast.c
 * @brief Forecast-end choices and non-destructive outlier treatment.
 *
 * Driver-backed date choices require all selected numeric columns. Adjustments
 * remain in the in-memory CSV; callers publish a private fit copy, never overwrite
 * the uploaded or original source. Exclusion interpolates unflagged neighbours.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tba_series_internal.h"

static int tba_series_end_compare(const void *a, const void *b)
{
    int left = *(const int *)a, right = *(const int *)b;
    return (left > right) - (left < right);
}

/* Enumerate supported future endpoints in ascending order. */
json_t *tba_series_ends(const json_t *target, const tba_series_t *drivers, const char *selection, bool fiscal)
{
    json_t *result = json_new_array();
    int after = tba_date_cstr(tba_json_text(target, "usable_end_date_iso"));
    if (!after)
        after = tba_date_cstr(tba_json_text(target, "end_date_iso"));
    const char *frequency = tba_json_text(target, "detected_frequency");
    if (!after || !result)
        return result;
    bool ok = true;
    if (drivers && selection && *selection) {
        size_t count = 0, n = 0;
        size_t *columns = tba_series_columns(drivers, selection, &count);
        size_t rows = tba_csv_rows(drivers->csv);
        int *dates = malloc((rows ? rows : 1) * sizeof *dates);
        ok = columns && count && dates;
        for (size_t r = 0; ok && r < rows; ++r) {
            bool usable = drivers->dates[r] > after;
            for (size_t c = 0; usable && c < count; ++c) {
                double value;
                usable = tba_text_number(tba_csv_cell(drivers->csv, r, columns[c]), &value);
            }
            int date = usable ? tba_date_end(drivers->dates[r], frequency, fiscal) : 0;
            if (date > after)
                dates[n++] = date;
        }
        if (ok)
            qsort(dates, n, sizeof *dates, tba_series_end_compare);
        for (size_t i = 0; ok && i < n && json_array_size(result) < 1200; ++i) {
            if (i && dates[i] == dates[i - 1])
                continue;
            string_t *date = tba_date_text(dates[i], false);
            ok = date && tba_json_append(result, string_c_str(date));
            string_free(date);
        }
        free(columns);
        free(dates);
    } else {
        int date = after;
        for (size_t i = 0; ok && i < 24; ++i) {
            int next = tba_date_end(tba_date_next(date, frequency), frequency, fiscal);
            if (next <= date)
                break;
            string_t *text = tba_date_text(next, false);
            ok = text && tba_json_append(result, string_c_str(text));
            string_free(text);
            date = next;
        }
    }
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

/* Apply capped or interpolated fit values while retaining the original file. */
string_t *tba_series_adjust(tba_series_t *series, const char *target, const char *mode, const char *dates, string_t **note)
{
    *note = string_new();
    if (!series || !*note)
        return NULL;
    if (!mode || (!strcmp(mode, "flag") || !strcmp(mode, "none") || !*mode) || !dates || !*dates)
        return tba_csv_text(series->csv);
    if (strcmp(mode, "cap") && strcmp(mode, "exclude"))
        return NULL;
    size_t column = tba_csv_column(series->csv, target);
    if (column == SIZE_MAX)
        return NULL;
    json_t *selected = json_new_object();
    string_t *selection = string_new_with(dates);
    size_t n = 0;
    string_t **parts = selection ? string_split(selection, ",", &n) : NULL;
    bool ok = selected && selection;
    for (size_t i = 0; ok && i < n; ++i) {
        int date = tba_date_parse(parts[i]);
        string_t *key = date ? tba_date_text(date, false) : NULL;
        ok = key && tba_json_flag(selected, string_c_str(key), true);
        string_free(key);
    }
    string_split_free(parts, n);
    string_free(selection);
    size_t rows = tba_csv_rows(series->csv), populated = 0, flagged = 0;
    double *values = malloc((rows ? rows : 1) * sizeof *values);
    double *finite = malloc((rows ? rows : 1) * sizeof *finite);
    bool *flags = calloc(rows ? rows : 1, sizeof *flags);
    ok = ok && values && finite && flags;
    for (size_t r = 0; ok && r < rows; ++r) {
        values[r] = NAN;
        if (tba_text_number(tba_csv_cell(series->csv, r, column), &values[r]))
            finite[populated++] = values[r];
        string_t *key = tba_date_text(series->dates[r], false);
        ok = key != NULL;
        flags[r] = key && tba_json_bool(selected, string_c_str(key));
        flagged += flags[r];
        string_free(key);
    }
    double median = tba_series_median(finite, populated);
    for (size_t i = 0; ok && i < populated; ++i)
        finite[i] = fabs(finite[i] - median);
    double mad = tba_series_median(finite, populated);
    double scale = 1.4826 * (mad > 0 ? mad : 1);
    for (size_t r = 0; ok && populated && r < rows; ++r) {
        if (!flags[r])
            continue;
        if (!strcmp(mode, "cap")) {
            if (!isfinite(values[r]))
                continue;
            values[r] = fmin(fmax(values[r], median - 3.5 * scale), median + 3.5 * scale);
        } else {
            size_t left = r, right = r + 1;
            while (left && (flags[left - 1] || !isfinite(values[left - 1])))
                left--;
            while (right < rows && (flags[right] || !isfinite(values[right])))
                right++;
            if (left && right < rows)
                values[r] = values[left - 1] + (values[right] - values[left - 1]) *
                            (double)(r - left + 1) / (double)(right - left + 1);
            else if (left)
                values[r] = values[left - 1];
            else if (right < rows)
                values[r] = values[right];
        }
        if (isfinite(values[r])) {
            string_t *text = string_sprintf("%.17g", values[r]);
            ok = text && tba_csv_set(series->csv, r, column, text);
            string_free(text);
        }
    }
    if (ok && flagged)
        ok = string_append_format(*note, "Outlier handling: %s applied to %zu selected point%s.", mode, flagged,
                                  flagged == 1 ? "" : "s") >= 0;
    string_t *result = ok ? tba_csv_text(series->csv) : NULL;
    json_free(selected);
    free(values);
    free(finite);
    free(flags);
    return result;
}
