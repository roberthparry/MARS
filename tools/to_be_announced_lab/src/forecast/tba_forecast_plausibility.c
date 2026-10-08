/**
 * @file tba_forecast_plausibility.c
 * @brief Native plausibility warnings for non-negative historical series.
 *
 * Detects negative forecasts, collapsing levels with wide uncertainty and
 * repeated zero lower bounds. Warnings do not alter the worker's predictions.
 */
#include <math.h>
#include <stdlib.h>

#include "tba_csv.h"
#include "tba_forecast.h"

static int tba_forecast_value_compare(const void *a, const void *b)
{
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

static double tba_forecast_middle(double *values, size_t count)
{
    if (!count)
        return 0;
    qsort(values, count, sizeof *values, tba_forecast_value_compare);
    return count % 2 ? values[count / 2] : values[count / 2 - 1] / 2 + values[count / 2] / 2;
}

/* Return an empty diagnostic unless one of the established warning conditions holds. */
json_t *tba_forecast_plausibility(const char *source)
{
    json_t *result = json_new_object();
    string_t *text = string_new_with(source ? source : "");
    tba_csv_t *csv = text && string_byte_length(text) ? tba_csv_parse(text) : NULL;
    string_free(text);
    if (!csv)
        return result;
    size_t rows = tba_csv_rows(csv), means_n = 0, errors_n = 0, uppers_n = 0, lowers_n = 0;
    double *means = malloc((rows ? rows : 1) * sizeof *means);
    double *errors = malloc((rows ? rows : 1) * sizeof *errors);
    double *uppers = malloc((rows ? rows : 1) * sizeof *uppers);
    double recent[12] = {0}, final_mean = 0;
    size_t history = 0, negatives = 0, zeros = 0;
    bool nonnegative = true, ok = means && errors && uppers && result;
    size_t actual_c = tba_csv_column(csv, "actual"), mean_c = tba_csv_column(csv, "mean");
    size_t error_c = tba_csv_column(csv, "stderr"), upper_c = tba_csv_column(csv, "upper");
    size_t lower_c = tba_csv_column(csv, "lower");
    for (size_t r = 0; ok && r < rows; ++r) {
        double actual, mean, error, upper, lower;
        if (tba_text_number(tba_csv_cell(csv, r, actual_c), &actual)) {
            nonnegative = nonnegative && actual >= -1e-9;
            recent[history++ % 12] = actual;
        } else {
            if (tba_text_number(tba_csv_cell(csv, r, mean_c), &mean)) {
                means[means_n++] = mean;
                final_mean = mean;
                negatives += mean < -1e-9;
            }
            if (tba_text_number(tba_csv_cell(csv, r, error_c), &error))
                errors[errors_n++] = error;
            if (tba_text_number(tba_csv_cell(csv, r, upper_c), &upper))
                uppers[uppers_n++] = upper;
            if (tba_text_number(tba_csv_cell(csv, r, lower_c), &lower)) {
                lowers_n++;
                zeros += fabs(lower) <= 1e-9;
            }
        }
    }
    double level = tba_forecast_middle(recent, history < 12 ? history : 12);
    double middle = ok ? tba_forecast_middle(means, means_n) : 0;
    double error = ok ? tba_forecast_middle(errors, errors_n) : 0;
    double upper = ok ? tba_forecast_middle(uppers, uppers_n) : 0;
    bool collapse = level > 0 && middle < .35 * level && error > .35 * level;
    bool clipped = zeros >= (lowers_n / 2 > 2 ? lowers_n / 2 : 2) && level > 0 && upper > 1.8 * level;
    if (ok && history && means_n && nonnegative && (negatives || collapse || clipped)) {
        string_t *message = string_new();
        if (message && negatives)
            string_append_cstr(message, "Forecast means fall below zero despite non-negative historical observations. ");
        if (message && collapse)
            string_append_cstr(message, "The forecast level collapses while uncertainty remains wide. ");
        if (message && clipped)
            string_append_cstr(message, "Many lower bounds are clipped at zero while upper bounds remain wide.");
        string_t *detail = string_sprintf("Recent typical level: %.2f. Median future mean: %.2f. Final mean: %.2f. "
                                          "%zu negative future means; %zu zero lower bounds.",
                                          level, middle, final_mean, negatives, zeros);
        ok = message && detail && tba_json_string(result, "title", "Forecast plausibility warning") &&
             tba_json_string(result, "level", negatives || collapse ? "poor" : "mediocre") &&
             tba_json_string(result, "message", string_c_str(message)) &&
             tba_json_string(result, "detail", string_c_str(detail)) &&
             tba_json_string(result, "action", "Compare a simpler model, fewer seasonal terms or a regression model.");
        string_free(message);
        string_free(detail);
    }
    free(means);
    free(errors);
    free(uppers);
    tba_csv_free(csv);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}
