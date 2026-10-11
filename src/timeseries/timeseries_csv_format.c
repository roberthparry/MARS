/**
 * @file timeseries_csv_format.c
 * @brief Typed time-series CSV exports through the shared csv_t writer.
 *
 * Supplies dates, numeric spellings and schema names; csv_t alone performs
 * escaping and record construction. Text-returning APIs necessarily retain the
 * complete output, while temporary storage is bounded to one typed row.
 */
#define MARS_TIMESERIES_INTERNAL_ACCESS

#include "csv-t.h"
#include "timeseries_internal.h"

static csv_t *ts_csv_writer(const char *const *names, size_t count)
{
    csv_options_t options = csv_options_default();
    options.crlf = false;
    options.strict_columns = true;
    options.max_buffer_bytes = SIZE_MAX;
    csv_t *csv = csv_writer_new(NULL, &options);
    string_t *fields[5] = {0};
    bool ok = csv != NULL;
    for (size_t i = 0; i < count; ++i) {
        fields[i] = string_new_with(names[i]);
        ok = fields[i] && ok;
    }
    ok = ok && csv_set_header(csv, (const string_t *const *)fields, count);
    for (size_t i = 0; i < count; ++i)
        string_free(fields[i]);
    if (!ok) {
        csv_free(csv);
        csv = NULL;
    }
    return csv;
}

static string_t *ts_csv_date(const timeseries_t *series, size_t i)
{
    if (!series->has_index || !series->index[i])
        return string_new();
    string_t *format = string_new_with("%dd/%mm/%yyyy");
    string_t *date = format ? datetime_format_text(series->index[i], format) : NULL;
    string_free(format);
    return date;
}

static bool ts_csv_numbers(csv_t *csv, const string_t *label, const number_t *values, size_t count,
                            int format, bool missing)
{
    string_t *fields[5] = {0};
    fields[0] = label ? string_clone(label) : NULL;
    bool ok = fields[0] != NULL;
    for (size_t i = 0; i < count; ++i) {
        double value = format && !missing ? num_to_double(values[i]) : 0.0;
        if (missing)
            fields[i + 1] = string_new();
        else if (format == 2 && isfinite(value))
            fields[i + 1] = string_sprintf("%.2f", value);
        else if (format == 6 && isfinite(value))
            fields[i + 1] = string_sprintf("%.6g", value);
        else
            fields[i + 1] = num_to_string(values[i]);
        ok = fields[i + 1] && ok;
    }
    ok = ok && csv_write(csv, (const string_t *const *)fields, count + 1);
    for (size_t i = 0; i <= count; ++i)
        string_free(fields[i]);
    return ok;
}

static string_t *ts_csv_finish(csv_t *csv, bool ok)
{
    string_t *text = ok && csv_finish(csv) ? string_clone(csv_output(csv)) : NULL;
    csv_free(csv);
    return text;
}

/* Preserve the existing date,value schema and numerical spelling. */
string_t *ts_csv_series_text(const timeseries_t *series)
{
    if (!series)
        return NULL;
    const char *names[] = {"date", "value"};
    csv_t *csv = ts_csv_writer(names, 2);
    bool ok = csv != NULL;
    for (size_t i = 0; ok && i < series->length; ++i) {
        string_t *date = ts_csv_date(series, i);
        ok = ts_csv_numbers(csv, date, &series->values[i], 1, 0, series->missing[i]);
        string_free(date);
    }
    return ts_csv_finish(csv, ok);
}

/* Forecast columns are borrowed directly, without temporary column matrices. */
string_t *ts_csv_forecast_text(const ts_forecast_t *forecast)
{
    if (!forecast || !forecast->mean)
        return NULL;
    const timeseries_t *columns[] = {forecast->mean, forecast->stderr, forecast->lower, forecast->upper};
    size_t length = ts_length(forecast->mean);
    for (size_t j = 1; j < 4; ++j)
        if (columns[j] && ts_length(columns[j]) < length)
            return NULL;
    const char *names[] = {"date", "mean", "stderr", "lower", "upper"};
    csv_t *csv = ts_csv_writer(names, 5);
    bool ok = csv != NULL;
    for (size_t i = 0; ok && i < length; ++i) {
        number_t values[4];
        for (size_t j = 0; j < 4; ++j)
            values[j] = columns[j] ? columns[j]->values[i] : NUM_NAN;
        string_t *date = ts_csv_date(forecast->mean, i);
        ok = ts_csv_numbers(csv, date, values, 4, 2, false);
        string_free(date);
    }
    return ts_csv_finish(csv, ok);
}

/* Coefficient export retains its existing schema and six significant figures. */
string_t *ts_csv_regression_text(const ts_regression_result_t *result)
{
    if (!result || !result->coefficients)
        return NULL;
    const char *names[] = {"term", "estimate", "stderr", "t_stat", "p_value"};
    const matrix_t *columns[] = {result->coefficients, result->stderr, result->t_stat, result->p_value};
    csv_t *csv = ts_csv_writer(names, 5);
    bool ok = csv != NULL;
    for (size_t i = 0; ok && i < mat_get_row_count(result->coefficients); ++i) {
        number_t values[4];
        for (size_t j = 0; j < 4; ++j)
            values[j] = columns[j] ? mat_get_num(columns[j], i, 0) : num_clone(NUM_NAN);
        string_t *label = string_sprintf("beta%zu", i);
        ok = ts_csv_numbers(csv, label, values, 4, 6, false);
        string_free(label);
        for (size_t j = 0; j < 4; ++j)
            num_destroy(&values[j]);
    }
    return ts_csv_finish(csv, ok);
}

/* Preserve the AR-only summary contract; other summaries remain text output. */
string_t *ts_csv_arima_text(const ts_arima_result_t *result)
{
    if (!result || !result->ar_params)
        return NULL;
    const char *names[] = {"term", "estimate"};
    csv_t *csv = ts_csv_writer(names, 2);
    bool ok = csv != NULL;
    for (size_t i = 0; ok && i < mat_get_row_count(result->ar_params); ++i) {
        number_t value = mat_get_num(result->ar_params, i, 0);
        string_t *label = string_sprintf("phi%zu", i + 1);
        ok = ts_csv_numbers(csv, label, &value, 1, 6, false);
        string_free(label);
        num_destroy(&value);
    }
    return ts_csv_finish(csv, ok);
}
