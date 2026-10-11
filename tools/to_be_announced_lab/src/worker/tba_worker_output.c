/**
 * @file tba_worker_output.c
 * @brief Historical and forecast series export formatting.
 *
 * Produces matching CSV and readable text from the native timeseries results.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime_t.h"
#include "matrix_t.h"
#include "number_t.h"
#include "tba_worker_internal.h"

static int tba_worker_appendf(char **buf, size_t *len, size_t *cap, const char *fmt, ...)
{
    va_list ap;
    string_t *piece;
    const char *piece_text;
    size_t piece_len;
    size_t required;
    char *grown;

    if (!buf || !len || !cap || !fmt)
        return -1;
    va_start(ap, fmt);
    piece = string_vsprintf(fmt, ap);
    va_end(ap);
    if (!piece)
        return -1;

    piece_text = string_c_str(piece);
    piece_len = strlen(piece_text);
    required = *len + piece_len + 1u;
    if (required > *cap) {
        size_t new_cap = *cap ? *cap : 256u;

        while (new_cap < required)
            new_cap *= 2u;
        grown = realloc(*buf, new_cap);
        if (!grown) {
            string_free(piece);
            return -1;
        }
        *buf = grown;
        *cap = new_cap;
    }
    memcpy(*buf + *len, piece_text, piece_len + 1u);
    *len += piece_len;
    string_free(piece);
    return 0;
}

static char *tba_worker_series_date_text(const timeseries_t *series, size_t index)
{
    datetime_t *dt;
    char *text;

    if (!series)
        return NULL;
    dt = datetime_alloc();
    if (!dt)
        return NULL;
    if (ts_get_datetime(series, index, dt) != 0) {
        datetime_dealloc(dt);
        return NULL;
    }
    text = datetime_format(dt, "%dd/%mm/%yyyy");
    datetime_dealloc(dt);
    return text;
}

static char *tba_worker_series_value_text(const timeseries_t *series, size_t index)
{
    number_t value = NUM_ZERO;
    char buf[64];
    double d;
    char *text;

    if (!series || ts_get_value(series, index, &value) != 0)
        return NULL;
    d = num_to_double(value);
    if (isfinite(d)) {
        snprintf(buf, sizeof(buf), "%.2f", d);
        text = strdup(buf);
    } else {
        string_t *value_text = num_to_string(value);

        text = value_text ? strdup(string_c_str(value_text)) : NULL;
        string_free(value_text);
    }
    num_destroy(&value);
    return text;
}

static int tba_worker_append_combined_csv_row(char **buf, size_t *len, size_t *cap, const char *date_text,
                                                   const char *actual_text, const char *mean_text,
                                                   const char *stderr_text, const char *lower_text,
                                                   const char *upper_text)
{
    return tba_worker_appendf(buf, len, cap, "%s,%s,%s,%s,%s,%s\n", date_text ? date_text : "",
                                   actual_text ? actual_text : "", mean_text ? mean_text : "",
                                   stderr_text ? stderr_text : "", lower_text ? lower_text : "",
                                   upper_text ? upper_text : "");
}

static int tba_worker_append_combined_text_row(char **buf, size_t *len, size_t *cap, const char *date_text,
                                                    const char *actual_text, const char *mean_text,
                                                    const char *stderr_text, const char *lower_text,
                                                    const char *upper_text)
{
    return tba_worker_appendf(buf, len, cap, "%s: actual %s, mean %s%s%s%s%s%s%s\n", date_text ? date_text : "",
                                   actual_text ? actual_text : "n/a", mean_text ? mean_text : "n/a",
                                   stderr_text ? ", stderr " : "", stderr_text ? stderr_text : "",
                                   lower_text ? ", lower " : "", lower_text ? lower_text : "",
                                   upper_text ? ", upper " : "", upper_text ? upper_text : "");
}

/* Format matched historical and future series for downloads. */
int tba_worker_build_results_outputs(const timeseries_t *actual, const timeseries_t *fitted,
                                                 const ts_forecast_t *forecast, char **csv_out, char **text_out)
{
    char *csv = NULL, *text = NULL;
    size_t csv_len = 0u, csv_cap = 0u, text_len = 0u, text_cap = 0u;
    size_t i;

    if (!csv_out || !text_out)
        return -1;
    if (tba_worker_appendf(&csv, &csv_len, &csv_cap, "date,actual,mean,stderr,lower,upper\n") != 0)
        goto fail;
    for (i = 0u; actual && fitted && i < ts_length(actual) && i < ts_length(fitted); ++i) {
        char *date_text = tba_worker_series_date_text(actual, i);
        char *actual_text = tba_worker_series_value_text(actual, i);
        char *mean_text = tba_worker_series_value_text(fitted, i);

        if (tba_worker_append_combined_csv_row(&csv, &csv_len, &csv_cap, date_text, actual_text, mean_text, NULL,
                                                    NULL, NULL) != 0 ||
            tba_worker_append_combined_text_row(&text, &text_len, &text_cap, date_text, actual_text, mean_text,
                                                     NULL, NULL, NULL) != 0) {
            free(date_text);
            free(actual_text);
            free(mean_text);
            goto fail;
        }
        free(date_text);
        free(actual_text);
        free(mean_text);
    }
    for (i = 0u; forecast && forecast->mean && i < ts_length(forecast->mean); ++i) {
        char *date_text = tba_worker_series_date_text(forecast->mean, i);
        char *mean_text = tba_worker_series_value_text(forecast->mean, i);
        char *stderr_text = forecast->stderr ? tba_worker_series_value_text(forecast->stderr, i) : NULL;
        char *lower_text = forecast->lower ? tba_worker_series_value_text(forecast->lower, i) : NULL;
        char *upper_text = forecast->upper ? tba_worker_series_value_text(forecast->upper, i) : NULL;

        if (tba_worker_append_combined_csv_row(&csv, &csv_len, &csv_cap, date_text, NULL, mean_text, stderr_text,
                                                    lower_text, upper_text) != 0 ||
            tba_worker_append_combined_text_row(&text, &text_len, &text_cap, date_text, NULL, mean_text,
                                                     stderr_text, lower_text, upper_text) != 0) {
            free(date_text);
            free(mean_text);
            free(stderr_text);
            free(lower_text);
            free(upper_text);
            goto fail;
        }
        free(date_text);
        free(mean_text);
        free(stderr_text);
        free(lower_text);
        free(upper_text);
    }
    *csv_out = csv;
    *text_out = text;
    return 0;

fail:
    free(csv);
    free(text);
    return -1;
}
