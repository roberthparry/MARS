/**
 * @file tba_series_statistics.c
 * @brief Robust outlier and seasonal-lag diagnostics for dated observations.
 *
 * Preserves the workbench's median absolute deviation threshold and candidate
 * seasonal periods. Correlations are descriptive diagnostics, not model fits.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tba_series_internal.h"

static int tba_series_point_compare(const void *a, const void *b)
{
    const tba_series_point_t *left = a, *right = b;
    if (left->date != right->date)
        return (left->date > right->date) - (left->date < right->date);
    return (left->row > right->row) - (left->row < right->row);
}

/* Collect finite, dated observations in stable date order. */
tba_series_point_t *tba_series_points(const tba_series_t *series, size_t column, size_t *count)
{
    *count = 0;
    if (!series || column >= tba_csv_columns(series->csv))
        return NULL;
    size_t rows = tba_csv_rows(series->csv);
    tba_series_point_t *points = malloc((rows ? rows : 1) * sizeof *points);
    if (!points)
        return NULL;
    for (size_t r = 0; r < rows; ++r) {
        double value;
        if (series->dates[r] && tba_text_number(tba_csv_cell(series->csv, r, column), &value))
            points[(*count)++] = (tba_series_point_t){series->dates[r], r, value};
    }
    qsort(points, *count, sizeof *points, tba_series_point_compare);
    return points;
}

static int tba_series_number_compare(const void *a, const void *b)
{
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

/* Obtain a median without mutating the caller's values. */
double tba_series_median(const double *values, size_t count)
{
    if (!values || !count)
        return NAN;
    double *copy = malloc(count * sizeof *copy);
    if (!copy)
        return NAN;
    memcpy(copy, values, count * sizeof *copy);
    qsort(copy, count, sizeof *copy, tba_series_number_compare);
    double median = count % 2 ? copy[count / 2] : copy[count / 2 - 1] / 2 + copy[count / 2] / 2;
    free(copy);
    return median;
}

/* Compute Pearson correlation with extended accumulation. */
double tba_series_correlation(const double *a, const double *b, size_t count)
{
    if (count < 3)
        return 0;
    long double ma = 0, mb = 0, aa = 0, bb = 0, ab = 0;
    for (size_t i = 0; i < count; ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= count;
    mb /= count;
    for (size_t i = 0; i < count; ++i) {
        long double x = a[i] - ma, y = b[i] - mb;
        aa += x * x;
        bb += y * y;
        ab += x * y;
    }
    double result = aa > 0 && bb > 0 ? (double)(ab / sqrtl(aa * bb)) : 0;
    return isfinite(result) ? fmax(-1, fmin(1, result)) : 0;
}

typedef struct {
    const char *frequency;
    unsigned lag;
    const char *name;
} tba_series_season_t;

static const tba_series_season_t tba_series_seasons[] = {
    {"monthly", 3, "quarterly"}, {"monthly", 4, "four-monthly"}, {"monthly", 6, "bi-annual"},
    {"monthly", 12, "annual"}, {"quarterly", 2, "bi-annual"}, {"quarterly", 4, "annual"},
    {"yearly", 2, "two-year"}, {"yearly", 3, "three-year"}, {"daily", 7, "weekly"},
    {"daily", 14, "fortnightly"}, {"daily", 30, "monthly"}, {"daily", 365, "annual"}
};

/* Add robust outliers and the strongest supported seasonal lag. */
bool tba_series_statistics(const tba_series_t *series, size_t column, json_t *metadata)
{
    json_t *outliers = json_new_array();
    size_t count = 0;
    tba_series_point_t *points = tba_series_points(series, column, &count);
    double *values = calloc(count ? count : 1, sizeof *values);
    double *deviations = calloc(count ? count : 1, sizeof *deviations);
    bool ok = outliers && values && deviations && (column == SIZE_MAX || points);
    for (size_t i = 0; ok && i < count; ++i)
        values[i] = points[i].value;
    double median = tba_series_median(values, count);
    for (size_t i = 0; ok && i < count; ++i)
        deviations[i] = fabs(values[i] - median);
    double mad = tba_series_median(deviations, count);
    for (size_t i = 0; ok && count >= 5 && mad > 0 && i < count; ++i) {
        double score = 0.6745 * (values[i] - median) / mad;
        if (!isfinite(score) || fabs(score) < 3.5)
            continue;
        json_t *entry = json_new_object();
        ok = entry && tba_series_date_fields(entry, "date", points[i].date) &&
             tba_json_number(entry, "value", values[i]) && tba_json_number(entry, "score", round(score * 100) / 100) &&
             json_array_append(outliers, entry);
        json_free(entry);
    }
    double best = 0;
    unsigned lag = 0;
    const char *pattern = "";
    /* Twelve fixed candidate definitions; no unbounded key scan. */
    for (size_t i = 0; ok && i < sizeof tba_series_seasons / sizeof *tba_series_seasons; ++i) {
        const tba_series_season_t *season = &tba_series_seasons[i];
        if (strcmp(season->frequency, series->frequency) || count < season->lag * 2 + 2)
            continue;
        double score = tba_series_correlation(values + season->lag, values, count - season->lag);
        if (score > best) {
            best = score;
            lag = season->lag;
            pattern = season->name;
        }
    }
    const char *strength = best >= .75 ? "very strong" : best >= .55 ? "strong" : best >= .35 ? "moderate" :
                           best >= .20 ? "weak" : "none";
    string_t *label = best >= .20 ? string_sprintf("%s %s seasonality detected.", strength, pattern)
                                 : string_new_with("No seasonality detected.");
    ok = ok && label && tba_json_set(metadata, "outliers", outliers) &&
         tba_json_string(metadata, "seasonality_label", string_c_str(label)) &&
         tba_json_string(metadata, "seasonality_pattern", best >= .20 ? pattern : "") &&
         tba_json_string(metadata, "seasonality_strength", strength) &&
         tba_json_number(metadata, "seasonality_score", round(best * 1000) / 1000) &&
         tba_json_number(metadata, "seasonality_lag", lag);
    string_free(label);
    json_free(outliers);
    free(points);
    free(values);
    free(deviations);
    return ok;
}
