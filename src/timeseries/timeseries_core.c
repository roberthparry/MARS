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

timeseries_t *ts_new_regular(const number_t *values, size_t length,
                             const datetime_t *start,
                             ts_frequency_t frequency,
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

timeseries_t *ts_new_indexed(const number_t *values, const datetime_t *const *index,
                             size_t length, ts_frequency_t frequency,
                             ts_year_type_t year_type)
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

timeseries_t *ts_new_regular_from_doubles(const double *values, size_t length,
                                          const datetime_t *start,
                                          ts_frequency_t frequency,
                                          ts_year_type_t year_type)
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

timeseries_t *ts_new_indexed_from_doubles(const double *values,
                                          const datetime_t *const *index,
                                          size_t length,
                                          ts_frequency_t frequency,
                                          ts_year_type_t year_type)
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

struct ts_builder_t {
    number_t *values;
    datetime_t **index;
    bool *missing;
    size_t length;
    size_t capacity;
    bool has_index;
    bool shape_known;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
};

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

ts_builder_t *ts_builder_new(ts_frequency_t frequency,
                             ts_year_type_t year_type)
{
    ts_builder_t *builder = calloc(1u, sizeof(*builder));

    if (!builder)
        return NULL;
    builder->frequency = frequency;
    builder->year_type = year_type;
    return builder;
}

int ts_builder_append(ts_builder_t *builder,
                      const datetime_t *datetime,
                      const number_t *value)
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

int ts_builder_append_double(ts_builder_t *builder,
                             const datetime_t *datetime,
                             double value)
{
    number_t number = num_create_from_double(value);
    int rc = ts_builder_append(builder, datetime, &number);

    num_destroy(&number);
    return rc;
}

int ts_builder_append_date_text_double(ts_builder_t *builder,
                                       const string_t *date_text,
                                       double value)
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

int ts_builder_append_date_string_double(ts_builder_t *builder,
                                         const char *date_text,
                                         double value)
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
        series = ts_new_indexed(builder->values,
                                (const datetime_t *const *)builder->index,
                                builder->length,
                                builder->frequency,
                                builder->year_type);
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

static string_t **ts_split_csv_line(const string_t *line, size_t *out_count)
{
    string_t *comma;
    string_t **fields;

    if (out_count)
        *out_count = 0u;
    if (!line)
        return NULL;

    comma = string_new_with(",");
    if (!comma)
        return NULL;

    fields = string_split_string(line, comma, out_count);
    string_free(comma);
    return fields;
}

static int ts_find_csv_column(const string_t *header_line, const string_t *name)
{
    string_t **fields;
    size_t count = 0u;
    int found = -1;

    if (!header_line || !name)
        return -1;
    if (string_length(name) == 0u)
        return 0;

    fields = ts_split_csv_line(header_line, &count);
    if (!fields)
        return -1;

    for (size_t i = 0u; i < count; ++i) {
        string_trim(fields[i]);
        if (string_compare(fields[i], name) == 0) {
            found = (int)i;
            break;
        }
    }

    string_split_free(fields, count);
    return found;
}

static string_t *ts_csv_field(string_t **fields, size_t count, int index)
{
    if (!fields || index < 0 || (size_t)index >= count)
        return NULL;

    string_trim(fields[(size_t)index]);
    return fields[(size_t)index];
}

static bool ts_grow_csv_series_storage(number_t **values,
                                       datetime_t ***index,
                                       bool **missing,
                                       size_t old_capacity,
                                       size_t new_capacity)
{
    number_t *new_values;
    datetime_t **new_index;
    bool *new_missing;

    if (!values || !index || !missing || new_capacity <= old_capacity)
        return false;

    new_values = calloc(new_capacity, sizeof(*new_values));
    new_index = calloc(new_capacity, sizeof(*new_index));
    new_missing = calloc(new_capacity, sizeof(*new_missing));
    if (!new_values || !new_index || !new_missing) {
        free(new_values);
        free(new_index);
        free(new_missing);
        return false;
    }

    memcpy(new_values, *values, old_capacity * sizeof(*new_values));
    memcpy(new_index, *index, old_capacity * sizeof(*new_index));
    memcpy(new_missing, *missing, old_capacity * sizeof(*new_missing));
    free(*values);
    free(*index);
    free(*missing);
    *values = new_values;
    *index = new_index;
    *missing = new_missing;
    return true;
}

timeseries_t *ts_from_csv_text(const string_t *path,
                               const string_t *date_column,
                               const string_t *value_column,
                               ts_frequency_t frequency,
                               ts_year_type_t year_type,
                               ts_missing_policy_t missing_policy)
{
    FILE *f;
    char *line = NULL;
    size_t line_cap = 0u;
    ssize_t line_len;
    string_t *header_text = NULL;
    int date_idx, value_idx;
    size_t cap = 32u, len = 0u;
    number_t *values = calloc(cap, sizeof(*values));
    datetime_t **index = calloc(cap, sizeof(*index));
    bool *missing = calloc(cap, sizeof(*missing));
    timeseries_t *series = NULL;

    if (!path || !date_column || !value_column || !values || !index || !missing) {
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    f = fopen(string_c_str(path), "r");
    if (!f) {
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    if (getline(&line, &line_cap, f) < 0) {
        fclose(f);
        free(line);
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    header_text = string_new_with(line);
    if (!header_text) {
        fclose(f);
        free(line);
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    date_idx = ts_find_csv_column(header_text, date_column);
    value_idx = ts_find_csv_column(header_text, value_column);
    string_free(header_text);
    if (date_idx < 0 || value_idx < 0) {
        fclose(f);
        free(line);
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    while ((line_len = getline(&line, &line_cap, f)) >= 0) {
        string_t *line_text = NULL;
        string_t **fields = NULL;
        size_t field_count = 0u;
        string_t *date_text;
        string_t *value_text;

        if (line_len == 0)
            continue;
        line_text = string_new_with(line);
        fields = line_text ? ts_split_csv_line(line_text, &field_count) : NULL;
        string_free(line_text);
        if (!fields)
            break;

        date_text = ts_csv_field(fields, field_count, date_idx);
        value_text = ts_csv_field(fields, field_count, value_idx);
        if (date_text && string_length(date_text) != 0u) {
            if (len == cap) {
                size_t new_cap = cap * 2u;

                if (!ts_grow_csv_series_storage(&values,
                                                &index,
                                                &missing,
                                                cap,
                                                new_cap)) {
                    string_split_free(fields, field_count);
                    break;
                }
                cap = new_cap;
            }
            if (ts_parse_date_text(date_text, &index[len]) != 0) {
                string_split_free(fields, field_count);
                continue;
            }
            if (!value_text || string_length(value_text) == 0u) {
                values[len] = num_clone(NUM_NAN);
                missing[len] = true;
            } else {
                if (num_set_from_text(&values[len], value_text) != 0) {
                    values[len] = num_clone(NUM_NAN);
                    missing[len] = true;
                }
            }
            ++len;
        }
        string_split_free(fields, field_count);
    }
    fclose(f);
    free(line);

    series = ts_alloc_empty(len);
    if (!series) {
        size_t i;

        for (i = 0u; i < len; ++i) {
            num_destroy(&values[i]);
            datetime_dealloc(index[i]);
        }
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    free(series->values);
    free(series->missing);
    series->values = values;
    series->missing = missing;
    series->index = index;
    series->has_index = true;
    series->frequency = frequency;
    series->year_type = year_type;
    series->is_regular = (frequency != TS_FREQ_IRREGULAR && frequency != TS_FREQ_UNKNOWN);
    series->season_period = (size_t)ts_infer_season_period(frequency);

    if (missing_policy == TS_MISSING_DROP) {
        timeseries_t *dropped = ts_drop_missing(series);

        ts_free(series);
        return dropped;
    }
    if (missing_policy != TS_MISSING_KEEP && missing_policy != TS_MISSING_ERROR) {
        timeseries_t *filled = ts_fill_missing(series, missing_policy);

        ts_free(series);
        return filled;
    }
    return series;
}

timeseries_t *ts_from_csv(const char *path,
                          const char *date_column,
                          const char *value_column,
                          ts_frequency_t frequency,
                          ts_year_type_t year_type,
                          ts_missing_policy_t missing_policy)
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
        series = ts_from_csv_text(path_text,
                                  date_column_text,
                                  value_column_text,
                                  frequency,
                                  year_type,
                                  missing_policy);
    }
    string_free(path_text);
    string_free(date_column_text);
    string_free(value_column_text);
    return series;
}

matrix_t *ts_matrix_from_csv_text(const string_t *path,
                                  const string_t *date_column,
                                  const string_t *const *value_columns,
                                  size_t value_column_count,
                                  ts_frequency_t frequency,
                                  ts_missing_policy_t missing_policy)
{
    FILE *f;
    char *line = NULL;
    size_t line_cap = 0u;
    ssize_t line_len;
    string_t *header_text = NULL;
    int *indices = NULL;
    size_t rows = 0u, cap = 32u;
    double *vals = NULL;
    matrix_t *out = NULL;
    size_t i;

    (void)frequency;
    if (!path || !date_column || !value_columns || value_column_count == 0u)
        return NULL;
    for (i = 0u; i < value_column_count; ++i) {
        if (!value_columns[i])
            return NULL;
    }

    f = fopen(string_c_str(path), "r");
    if (!f)
        goto fail_before_open;
    if (getline(&line, &line_cap, f) < 0) {
        fclose(f);
        free(line);
        line = NULL;
        goto fail_before_open;
    }
    indices = calloc(value_column_count + 1u, sizeof(*indices));
    vals = calloc(cap * value_column_count, sizeof(*vals));
    header_text = string_new_with(line);
    if (!indices || !vals || !header_text) {
        fclose(f);
        free(line);
        line = NULL;
        goto fail_before_open;
    }
    indices[0] = ts_find_csv_column(header_text, date_column);
    for (i = 0u; i < value_column_count; ++i) {
        indices[i + 1u] = ts_find_csv_column(header_text, value_columns[i]);
    }
    string_free(header_text);
    header_text = NULL;
    if (indices[0] < 0) {
        fclose(f);
        free(line);
        line = NULL;
        goto fail_before_open;
    }
    while ((line_len = getline(&line, &line_cap, f)) >= 0) {
        string_t *line_text = NULL;
        string_t **fields = NULL;
        size_t field_count = 0u;
        bool keep = true;
        string_t **selected = calloc(value_column_count + 1u, sizeof(*selected));

        line_text = string_new_with(line);
        fields = line_text ? ts_split_csv_line(line_text, &field_count) : NULL;
        string_free(line_text);
        if (!fields || !selected) {
            string_split_free(fields, field_count);
            free(selected);
            break;
        }

        selected[0] = ts_csv_field(fields, field_count, indices[0]);
        for (i = 0u; i < value_column_count; ++i)
            selected[i + 1u] = ts_csv_field(fields, field_count, indices[i + 1u]);

        if (!selected[0] || string_length(selected[0]) == 0u)
            keep = false;
        for (i = 0u; i < value_column_count && keep; ++i) {
            if (!selected[i + 1u] || string_length(selected[i + 1u]) == 0u) {
                if (missing_policy == TS_MISSING_DROP || missing_policy == TS_MISSING_ERROR)
                    keep = false;
            }
        }
        if (keep) {
            if (rows == cap) {
                size_t new_cap = cap * 2u;
                double *grown = realloc(vals, new_cap * value_column_count * sizeof(*vals));

                if (!grown) {
                    string_split_free(fields, field_count);
                    free(selected);
                    break;
                }
                vals = grown;
                cap = new_cap;
            }
            for (i = 0u; i < value_column_count; ++i) {
                const string_t *field = selected[i + 1u];

                if (!field || string_length(field) == 0u) {
                    vals[rows * value_column_count + i] = NAN;
                } else {
                    number_t number = num_create_from_text(field);

                    vals[rows * value_column_count + i] = num_to_double(number);
                    num_destroy(&number);
                }
            }
            ++rows;
        }
        string_split_free(fields, field_count);
        free(selected);
    }
    fclose(f);
    free(line);
    free(indices);
    out = ts_make_matrix_from_doubles(vals, rows, value_column_count);
    free(vals);
    return out;

fail_before_open:
    string_free(header_text);
    free(indices);
    free(vals);
    free(line);
    return NULL;
}

matrix_t *ts_matrix_from_csv(const char *path,
                             const char *date_column,
                             const char *const *value_columns,
                             size_t value_column_count,
                             ts_frequency_t frequency,
                             ts_missing_policy_t missing_policy)
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

    matrix = ts_matrix_from_csv_text(path_text,
                                     date_column_text,
                                     (const string_t *const *)value_column_texts,
                                     value_column_count,
                                     frequency,
                                     missing_policy);

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
