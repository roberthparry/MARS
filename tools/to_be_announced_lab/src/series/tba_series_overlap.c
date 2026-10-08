/**
 * @file tba_series_overlap.c
 * @brief Pairwise driver overlap and constant-column diagnostics.
 *
 * Uses only rows with valid dates and every selected driver present. Pairwise
 * comparisons are bounded to 32 selected drivers to bound request CPU and memory.
 */
#include <math.h>
#include <stdlib.h>

#include "tba_series_internal.h"

/* Describe shared-data quality and the strongest absolute driver correlation. */
json_t *tba_series_overlap(const tba_series_t *series, const char *drivers)
{
    json_t *result = json_new_object();
    if (!series)
        return result;
    size_t columns_count = 0;
    size_t *columns = tba_series_columns(series, drivers, &columns_count);
    if (!columns || columns_count > 32) {
        free(columns);
        json_free(result);
        return NULL;
    }
    if (columns_count < 2) {
        free(columns);
        return result;
    }
    size_t rows = tba_csv_rows(series->csv), count = 0;
    double *data = malloc((rows ? rows : 1) * columns_count * sizeof *data);
    if (!data || !result) {
        free(data);
        free(columns);
        json_free(result);
        return NULL;
    }
    int first = 0, last = 0;
    for (size_t r = 0; r < rows; ++r) {
        bool usable = series->dates[r] != 0;
        for (size_t c = 0; usable && c < columns_count; ++c)
            usable = tba_text_number(tba_csv_cell(series->csv, r, columns[c]), &data[c * rows + count]);
        if (usable) {
            count++;
            if (!first || series->dates[r] < first)
                first = series->dates[r];
            if (series->dates[r] > last)
                last = series->dates[r];
        }
    }
    double best = 0;
    size_t left = 0, right = 1;
    bool constant = false;
    for (size_t a = 0; a < columns_count; ++a) {
        long double mean = 0, variance = 0;
        for (size_t r = 0; r < count; ++r)
            mean += data[a * rows + r];
        mean /= count ? count : 1;
        for (size_t r = 0; r < count; ++r) {
            long double d = data[a * rows + r] - mean;
            variance += d * d;
        }
        constant = constant || (count > 1 && variance / (count - 1) <= 1e-24L);
        for (size_t b = a + 1; b < columns_count; ++b) {
            double correlation = fabs(tba_series_correlation(data + a * rows, data + b * rows, count));
            if (correlation > best) {
                best = correlation;
                left = a;
                right = b;
            }
        }
    }
    const char *level = count < 3 ? "mediocre" : constant || best >= .995 ? "poor" :
                        best >= .98 ? "mediocre" : best >= .95 ? "good" : "excellent";
    string_t *message = count < 3 ? string_new_with("Too few fully usable rows remain to check driver overlap confidently.") :
                        constant ? string_new_with("One or more selected drivers barely change over the shared period.") :
                        best >= .95 ? string_sprintf("The selected drivers %s and %s overlap %s strongly.",
                            tba_csv_header(series->csv, columns[left]), tba_csv_header(series->csv, columns[right]),
                            best >= .995 ? "almost completely and very" : "") :
                        string_new_with("The selected drivers do not show an obvious pairwise overlap warning.");
    string_t *start = tba_date_text(first, true), *end = tba_date_text(last, true);
    string_t *detail = start && end ? string_sprintf("Strongest absolute correlation: %.4f. Shared usable overlap: %S to %S across %zu rows.",
                                                   best, start, end, count) : NULL;
    bool ok = message && detail && tba_json_string(result, "level", level) &&
              tba_json_string(result, "title", "Driver overlap check") &&
              tba_json_string(result, "message", string_c_str(message)) &&
              tba_json_string(result, "detail", string_c_str(detail)) &&
              tba_json_string(result, "action", constant || best >= .98 ? "Remove a redundant driver and compare a simpler model." :
                              count < 3 ? "Try fewer drivers or check missing values." : "Interpret individual driver effects with care.");
    string_free(start);
    string_free(end);
    string_free(message);
    string_free(detail);
    free(columns);
    free(data);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}
