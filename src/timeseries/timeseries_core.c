/**
 * @file timeseries_core.c
 * @brief Time-series construction and incremental typed observation building.
 *
 * Creates regular or explicitly indexed observations and owns series values. CSV adapters live in timeseries_csv.c.
 * Calendar-aware transformations and model fitting are implemented in companion units.
 *
 * This is part of the timeseries_t implementation. Maintain the association between values, date indices and model metadata when
 * constructing or transforming results.
 */

#define MARS_TIMESERIES_INTERNAL_ACCESS
#include "timeseries_internal.h"

timeseries_t *ts_new(const number_t *values, size_t length)
{
    size_t i;
    timeseries_t *series;

    series = ts_alloc_empty(length);
    if (!series)
        return NULL;
    for (i = 0u; i < length; ++i) {
        if (values)
            ts_copy_value_slot(&series->values[i], &values[i]);
        else
            series->values[i] = num_clone(NUM_ZERO);
    }
    series->frequency = TS_FREQ_UNKNOWN;
    series->year_type = TS_YEAR_CALENDAR;
    return series;
}

timeseries_t *ts_new_regular(const number_t *values, size_t length, const datetime_t *start, ts_frequency_t frequency,
                             ts_year_type_t year_type)
{
    timeseries_t *series;
    size_t i;
    datetime_t *cursor = NULL;

    if ((!values && length != 0u) || !start)
        return NULL;
    series = ts_new(values, length);
    if (!series)
        return NULL;
    series->index = calloc(length ? length : 1u, sizeof(*series->index));
    if (!series->index) {
        ts_free(series);
        return NULL;
    }
    cursor = ts_datetime_clone(start);
    if (!cursor) {
        ts_free(series);
        return NULL;
    }
    series->has_index = true;
    series->frequency = frequency;
    series->year_type = year_type;
    series->is_regular = true;
    series->season_period = (size_t)ts_infer_season_period(frequency);
    for (i = 0u; i < length; ++i) {
        series->index[i] = ts_datetime_clone(cursor);
        if (!series->index[i]) {
            datetime_dealloc(cursor);
            ts_free(series);
            return NULL;
        }
        ts_step_regular_datetime(cursor, frequency);
    }
    datetime_dealloc(cursor);
    return series;
}

timeseries_t *ts_new_indexed(const number_t *values, const datetime_t *const *index, size_t length,
                             ts_frequency_t frequency, ts_year_type_t year_type)
{
    timeseries_t *series = ts_new(values, length);

    if (!series)
        return NULL;
    if (index && ts_set_index(series, index) != 0) {
        ts_free(series);
        return NULL;
    }
    series->frequency = frequency;
    series->year_type = year_type;
    series->season_period = (size_t)ts_infer_season_period(frequency);
    return series;
}

timeseries_t *ts_new_from_doubles(const double *values, size_t length)
{
    timeseries_t *series;
    size_t i;

    if (!values && length != 0u)
        return NULL;
    series = ts_alloc_empty(length);
    if (!series)
        return NULL;
    for (i = 0u; i < length; ++i) {
        series->values[i] = num_create_from_double(values[i]);
        series->missing[i] = isnan(values[i]);
    }
    series->frequency = TS_FREQ_UNKNOWN;
    series->year_type = TS_YEAR_CALENDAR;
    return series;
}

timeseries_t *ts_new_regular_from_doubles(const double *values, size_t length, const datetime_t *start,
                                          ts_frequency_t frequency, ts_year_type_t year_type)
{
    timeseries_t *series;
    number_t *numbers;
    size_t i;

    if ((!values && length != 0u) || !start)
        return NULL;
    numbers = calloc(length ? length : 1u, sizeof(*numbers));
    if (!numbers)
        return NULL;
    for (i = 0u; i < length; ++i)
        numbers[i] = num_create_from_double(values[i]);
    series = ts_new_regular(numbers, length, start, frequency, year_type);
    if (series) {
        for (i = 0u; i < length; ++i)
            series->missing[i] = isnan(values[i]);
    }
    for (i = 0u; i < length; ++i)
        num_destroy(&numbers[i]);
    free(numbers);
    return series;
}

timeseries_t *ts_new_indexed_from_doubles(const double *values, const datetime_t *const *index, size_t length,
                                          ts_frequency_t frequency, ts_year_type_t year_type)
{
    timeseries_t *series;
    number_t *numbers;
    size_t i;

    if ((!values && length != 0u) || (!index && length != 0u))
        return NULL;
    numbers = calloc(length ? length : 1u, sizeof(*numbers));
    if (!numbers)
        return NULL;
    for (i = 0u; i < length; ++i)
        numbers[i] = num_create_from_double(values[i]);
    series = ts_new_indexed(numbers, index, length, frequency, year_type);
    if (series) {
        for (i = 0u; i < length; ++i)
            series->missing[i] = isnan(values[i]);
    }
    for (i = 0u; i < length; ++i)
        num_destroy(&numbers[i]);
    free(numbers);
    return series;
}

timeseries_t *ts_clone(const timeseries_t *series)
{
    timeseries_t *out = ts_clone_shallow_shape(series);

    if (!out)
        return NULL;
    if (ts_copy_into(out, series) != 0) {
        ts_free(out);
        return NULL;
    }
    return out;
}

void ts_free(timeseries_t *series)
{
    size_t i;

    if (!series)
        return;
    if (series->values) {
        for (i = 0u; i < series->length; ++i)
            num_destroy(&series->values[i]);
    }
    if (series->index) {
        for (i = 0u; i < series->length; ++i)
            datetime_dealloc(series->index[i]);
    }
    free(series->values);
    free(series->index);
    free(series->missing);
    free(series);
}

typedef struct _ts_builder_t {
    number_t *values;
    datetime_t **index;
    bool *missing;
    size_t length;
    size_t capacity;
    bool has_index;
    bool shape_known;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
} ts_builder_t;

static int ts_builder_reserve(ts_builder_t *builder, size_t needed)
{
    size_t new_capacity;
    number_t *new_values;
    datetime_t **new_index;
    bool *new_missing;

    if (!builder)
        return -1;
    if (needed <= builder->capacity)
        return 0;
    new_capacity = builder->capacity ? builder->capacity * 2u : 16u;
    while (new_capacity < needed)
        new_capacity *= 2u;
    new_values = realloc(builder->values, new_capacity * sizeof(*builder->values));
    if (!new_values)
        return -1;
    builder->values = new_values;
    new_index = realloc(builder->index, new_capacity * sizeof(*builder->index));
    if (!new_index)
        return -1;
    builder->index = new_index;
    new_missing = realloc(builder->missing, new_capacity * sizeof(*builder->missing));
    if (!new_missing)
        return -1;
    builder->missing = new_missing;
    builder->capacity = new_capacity;
    return 0;
}

ts_builder_t *ts_builder_new(ts_frequency_t frequency, ts_year_type_t year_type)
{
    ts_builder_t *builder = calloc(1u, sizeof(*builder));

    if (!builder)
        return NULL;
    builder->frequency = frequency;
    builder->year_type = year_type;
    return builder;
}

int ts_builder_append(ts_builder_t *builder, const datetime_t *datetime, const number_t *value)
{
    bool row_has_index;
    number_t copy;
    datetime_t *datetime_copy = NULL;

    if (!builder || !value)
        return -1;
    row_has_index = datetime != NULL;
    if (!builder->shape_known) {
        builder->has_index = row_has_index;
        builder->shape_known = true;
    } else if (builder->has_index != row_has_index) {
        return -1;
    }
    if (ts_builder_reserve(builder, builder->length + 1u) != 0)
        return -1;
    if (row_has_index) {
        datetime_copy = ts_datetime_clone(datetime);
        if (!datetime_copy)
            return -1;
    }
    copy = num_clone(*value);
    builder->values[builder->length] = copy;
    builder->index[builder->length] = datetime_copy;
    builder->missing[builder->length] = num_is_nan(copy);
    builder->length++;
    return 0;
}

int ts_builder_append_double(ts_builder_t *builder, const datetime_t *datetime, double value)
{
    number_t number = num_create_from_double(value);
    int rc = ts_builder_append(builder, datetime, &number);

    num_destroy(&number);
    return rc;
}

int ts_builder_append_date_text_double(ts_builder_t *builder, const string_t *date_text, double value)
{
    datetime_t *datetime = NULL;
    int rc;

    if (!builder || !date_text)
        return -1;

    if (ts_parse_date_text(date_text, &datetime) != 0)
        return -1;

    rc = ts_builder_append_double(builder, datetime, value);
    datetime_dealloc(datetime);
    return rc;
}

int ts_builder_append_date_string_double(ts_builder_t *builder, const char *date_text, double value)
{
    string_t *date_string;
    int rc;

    if (!builder || !date_text)
        return -1;

    date_string = string_new_with(date_text);
    if (!date_string)
        return -1;

    rc = ts_builder_append_date_text_double(builder, date_string, value);
    string_free(date_string);
    return rc;
}

timeseries_t *ts_builder_build(const ts_builder_t *builder)
{
    timeseries_t *series;

    if (!builder)
        return NULL;
    if (builder->has_index) {
        series = ts_new_indexed(builder->values, (const datetime_t *const *)builder->index, builder->length,
                                builder->frequency, builder->year_type);
    } else {
        series = ts_new(builder->values, builder->length);
        if (series) {
            series->frequency = builder->frequency;
            series->year_type = builder->year_type;
            series->season_period = (size_t)ts_infer_season_period(builder->frequency);
        }
    }
    if (series) {
        size_t i;

        for (i = 0u; i < builder->length; ++i)
            series->missing[i] = builder->missing[i];
    }
    return series;
}

void ts_builder_destroy(ts_builder_t *builder)
{
    size_t i;

    if (!builder)
        return;
    for (i = 0u; i < builder->length; ++i) {
        num_destroy(&builder->values[i]);
        datetime_dealloc(builder->index[i]);
    }
    free(builder->values);
    free(builder->index);
    free(builder->missing);
    free(builder);
}
