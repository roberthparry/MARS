/**
 * @file timeseries_csv.c
 * @brief Time-series and design-matrix adapters for the public CSV module.
 *
 * csv_t owns record syntax, UTF-8 validation and header lookup. This module
 * handles only dates, numeric conversion, missing policies and series shape.
 * File handles are opened and closed here; no CSV syntax is interpreted here.
 */
#define MARS_TIMESERIES_INTERNAL_ACCESS

#include "csv_t.h"
#include "timeseries_internal.h"

static bool ts_csv_column(csv_t *csv, const string_t *name, size_t *column)
{
    if (!name)
        return false;
    if (!string_byte_length(name)) {
        *column = 0;
        return csv_header_count(csv) != 0;
    }
    return csv_column(csv, name, column);
}

static string_t *ts_csv_field(csv_t *csv, size_t column)
{
    const string_t *field = csv_field(csv, column);
    string_t *copy = field ? string_clone(field) : string_new();
    if (copy)
        string_trim(copy);
    return copy;
}

/* Shared with object restoration; syntax always belongs to csv_t. */
timeseries_t *ts_csv_read_series(csv_t *csv, const string_t *date_name, const string_t *value_name,
                                 ts_frequency_t frequency, ts_year_type_t year_type,
                                 ts_missing_policy_t policy, bool strict_values)
{
    csv_status_t status = csv_next(csv);
    size_t date_column, value_column;
    if (status == CSV_ERROR || !ts_csv_column(csv, date_name, &date_column) ||
        !ts_csv_column(csv, value_name, &value_column))
        return NULL;
    ts_builder_t *builder = ts_builder_new(frequency, year_type);
    bool ok = builder != NULL;
    while (ok && status == CSV_ROW) {
        string_t *date = ts_csv_field(csv, date_column), *text = ts_csv_field(csv, value_column);
        datetime_t *when = NULL;
        ok = date && text;
        bool dated = ok && string_byte_length(date) && ts_parse_date_text(date, &when) == 0;
        number_t value = text && string_byte_length(text) ? num_create_from_text(text) : num_clone(NUM_NAN);
        if (strict_values && (!dated || (text && string_byte_length(text) && num_is_nan(value))))
            ok = false;
        if (ok && dated && (policy != TS_MISSING_DROP || !num_is_nan(value)))
            ok = ts_builder_append(builder, when, &value) == 0;
        num_destroy(&value);
        datetime_dealloc(when);
        string_free(date);
        string_free(text);
        if (ok)
            status = csv_next(csv);
    }
    timeseries_t *series = ok && status == CSV_END ? ts_builder_build(builder) : NULL;
    ts_builder_destroy(builder);
    if (series && policy != TS_MISSING_DROP && policy != TS_MISSING_KEEP && policy != TS_MISSING_ERROR) {
        timeseries_t *filled = ts_fill_missing(series, policy);
        ts_free(series);
        series = filled;
    }
    return series;
}

static csv_t *ts_csv_open(file_t *file)
{
    if (!file || !file_open_follow(file, FILE_MODE_OPEN, FILE_ACCESS_READ))
        return NULL;
    csv_options_t options = csv_options_default();
    options.first_row_header = true;
    options.trim_headers = true;
    return csv_reader_new(file, &options);
}

/* Load one dated column through the shared quoted-field reader. */
timeseries_t *ts_from_csv_text(const string_t *path, const string_t *date_column, const string_t *value_column,
                               ts_frequency_t frequency, ts_year_type_t year_type, ts_missing_policy_t missing_policy)
{
    if (!path || !date_column || !value_column)
        return NULL;
    file_t *file = file_new(path);
    csv_t *csv = ts_csv_open(file);
    timeseries_t *series = csv ? ts_csv_read_series(csv, date_column, value_column, frequency, year_type,
                                                   missing_policy, false) : NULL;
    csv_free(csv);
    if (file && !file_close(file)) {
        ts_free(series);
        series = NULL;
    }
    file_free(file);
    return series;
}

/* Load selected numeric columns without duplicating CSV tokenisation. */
matrix_t *ts_matrix_from_csv_text(const string_t *path, const string_t *date_column,
                                  const string_t *const *value_columns, size_t value_column_count,
                                  ts_frequency_t frequency, ts_missing_policy_t missing_policy)
{
    (void)frequency;
    if (!path || !date_column || !value_columns || !value_column_count ||
        value_column_count > SIZE_MAX / (32 * sizeof(double)))
        return NULL;
    file_t *file = file_new(path);
    csv_t *csv = ts_csv_open(file);
    size_t *columns = calloc(value_column_count, sizeof(*columns));
    size_t date_index = 0, rows = 0, capacity = 32;
    double *values = malloc(capacity * value_column_count * sizeof(*values));
    csv_status_t status = csv ? csv_next(csv) : CSV_ERROR;
    bool ok = csv && columns && values && status != CSV_ERROR && ts_csv_column(csv, date_column, &date_index);
    for (size_t i = 0; ok && i < value_column_count; ++i)
        ok = ts_csv_column(csv, value_columns[i], &columns[i]);
    while (ok && status == CSV_ROW) {
        string_t *date = ts_csv_field(csv, date_index);
        bool keep = date && string_byte_length(date);
        ok = date != NULL;
        string_free(date);
        if (ok && rows == capacity) {
            if (capacity > SIZE_MAX / (2 * value_column_count * sizeof(*values)))
                ok = false;
            else {
                double *grown = realloc(values, 2 * capacity * value_column_count * sizeof(*values));
                if (!grown)
                    ok = false;
                else {
                    values = grown;
                    capacity *= 2;
                }
            }
        }
        for (size_t i = 0; ok && i < value_column_count; ++i) {
            string_t *field = ts_csv_field(csv, columns[i]);
            ok = field != NULL;
            bool empty = !field || !string_byte_length(field);
            if (empty && (missing_policy == TS_MISSING_DROP || missing_policy == TS_MISSING_ERROR))
                keep = false;
            number_t value = empty ? num_clone(NUM_NAN) : num_create_from_text(field);
            values[rows * value_column_count + i] = num_to_double(value);
            num_destroy(&value);
            string_free(field);
        }
        if (ok && keep)
            ++rows;
        if (ok)
            status = csv_next(csv);
    }
    csv_free(csv);
    if (file && !file_close(file))
        ok = false;
    file_free(file);
    matrix_t *matrix = ok && status == CSV_END ? ts_make_matrix_from_doubles(values, rows, value_column_count) : NULL;
    free(columns);
    free(values);
    return matrix;
}

/* Convert literal-friendly arguments at the public boundary. */
timeseries_t *ts_from_csv(const char *path, const char *date_column, const char *value_column, ts_frequency_t frequency,
                          ts_year_type_t year_type, ts_missing_policy_t missing_policy)
{
    string_t *path_text = NULL;
    string_t *date_column_text = NULL;
    string_t *value_column_text = NULL;
    timeseries_t *series = NULL;

    if (!path || !date_column || !value_column)
        return NULL;

    path_text = string_new_with(path);
    date_column_text = string_new_with(date_column);
    value_column_text = string_new_with(value_column);
    if (path_text && date_column_text && value_column_text) {
        series = ts_from_csv_text(path_text, date_column_text, value_column_text, frequency, year_type, missing_policy);
    }
    string_free(path_text);
    string_free(date_column_text);
    string_free(value_column_text);
    return series;
}


/* Convert the selected header names once before indexed lookup. */
matrix_t *ts_matrix_from_csv(const char *path, const char *date_column, const char *const *value_columns,
                             size_t value_column_count, ts_frequency_t frequency, ts_missing_policy_t missing_policy)
{
    string_t *path_text = NULL;
    string_t *date_column_text = NULL;
    string_t **value_column_texts = NULL;
    matrix_t *matrix = NULL;
    size_t i;

    if (!path || !date_column || !value_columns || value_column_count == 0u)
        return NULL;

    path_text = string_new_with(path);
    date_column_text = string_new_with(date_column);
    value_column_texts = calloc(value_column_count, sizeof(*value_column_texts));
    if (!path_text || !date_column_text || !value_column_texts)
        goto done;

    for (i = 0u; i < value_column_count; ++i) {
        value_column_texts[i] = string_new_with(value_columns[i] ? value_columns[i] : "");
        if (!value_column_texts[i])
            goto done;
    }

    matrix = ts_matrix_from_csv_text(path_text, date_column_text, (const string_t *const *)value_column_texts,
                                     value_column_count, frequency, missing_policy);

done:
    if (value_column_texts) {
        for (i = 0u; i < value_column_count; ++i)
            string_free(value_column_texts[i]);
    }
    free(value_column_texts);
    string_free(date_column_text);
    string_free(path_text);
    return matrix;
}
