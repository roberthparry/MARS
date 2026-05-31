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

int ts_builder_append_date_string_double(ts_builder_t *builder,
                                         const char *date_text,
                                         double value)
{
    datetime_t *datetime = NULL;
    int rc;

    if (!builder || !date_text)
        return -1;
    if (ts_parse_date(date_text, &datetime) != 0)
        return -1;
    rc = ts_builder_append_double(builder, datetime, value);
    datetime_dealloc(datetime);
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

static char *ts_trim(char *text)
{
    char *end;

    if (!text)
        return NULL;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        ++text;
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static int ts_find_csv_column(char *header_line, const char *name)
{
    int idx = 0;
    char *save = NULL;
    char *tok;

    if (!header_line || !name)
        return -1;
    if (name[0] == '\0')
        return 0;
    tok = strtok_r(header_line, ",", &save);
    while (tok) {
        if (strcmp(ts_trim(tok), name) == 0)
            return idx;
        ++idx;
        tok = strtok_r(NULL, ",", &save);
    }
    return -1;
}

timeseries_t *ts_from_csv(const char *path,
                          const char *date_column,
                          const char *value_column,
                          ts_frequency_t frequency,
                          ts_year_type_t year_type,
                          ts_missing_policy_t missing_policy)
{
    FILE *f;
    char *line = NULL;
    size_t line_cap = 0u;
    ssize_t line_len;
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
    f = fopen(path, "r");
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
    {
        char *header_copy = strdup(line);

        if (!header_copy) {
            fclose(f);
            free(line);
            free(values);
            free(index);
            free(missing);
            return NULL;
        }
        date_idx = ts_find_csv_column(header_copy, date_column);
        strcpy(header_copy, line);
        value_idx = ts_find_csv_column(header_copy, value_column);
        free(header_copy);
    }
    if (date_idx < 0 || value_idx < 0) {
        fclose(f);
        free(line);
        free(values);
        free(index);
        free(missing);
        return NULL;
    }
    while ((line_len = getline(&line, &line_cap, f)) >= 0) {
        char *copy;
        char *save = NULL;
        char *tok;
        int col = 0;
        char *date_text = NULL;
        char *value_text = NULL;

        if (line_len == 0)
            continue;
        copy = strdup(line);
        if (!copy)
            break;
        tok = strtok_r(copy, ",", &save);
        while (tok) {
            if (col == date_idx)
                date_text = tok;
            if (col == value_idx)
                value_text = tok;
            ++col;
            tok = strtok_r(NULL, ",", &save);
        }
        if (date_text && ts_trim(date_text)[0] != '\0') {
            if (len == cap) {
                size_t new_cap = cap * 2u;
                number_t *new_values = realloc(values, new_cap * sizeof(*new_values));
                datetime_t **new_index = realloc(index, new_cap * sizeof(*new_index));
                bool *new_missing = realloc(missing, new_cap * sizeof(*new_missing));

                if (!new_values || !new_index || !new_missing) {
                    free(copy);
                    free(new_values);
                    free(new_index);
                    free(new_missing);
                    break;
                }
                values = new_values;
                index = new_index;
                missing = new_missing;
                memset(values + cap, 0, (new_cap - cap) * sizeof(*values));
                memset(index + cap, 0, (new_cap - cap) * sizeof(*index));
                memset(missing + cap, 0, (new_cap - cap) * sizeof(*missing));
                cap = new_cap;
            }
            if (ts_parse_date(ts_trim(date_text), &index[len]) != 0) {
                free(copy);
                continue;
            }
            if (!value_text || ts_trim(value_text)[0] == '\0') {
                values[len] = num_clone(NUM_NAN);
                missing[len] = true;
            } else {
                if (num_set_from_string(&values[len], ts_trim(value_text)) != 0) {
                    values[len] = num_clone(NUM_NAN);
                    missing[len] = true;
                }
            }
            ++len;
        }
        free(copy);
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

matrix_t *ts_matrix_from_csv(const char *path,
                             const char *date_column,
                             const char *const *value_columns,
                             size_t value_column_count,
                             ts_frequency_t frequency,
                             ts_missing_policy_t missing_policy)
{
    FILE *f;
    char *line = NULL;
    size_t line_cap = 0u;
    ssize_t line_len;
    int *indices = NULL;
    size_t rows = 0u, cap = 32u;
    double *vals = NULL;
    matrix_t *out = NULL;
    char *header_copy = NULL;
    size_t i;

    (void)frequency;
    if (!path || !date_column || !value_columns || value_column_count == 0u)
        return NULL;
    f = fopen(path, "r");
    if (!f)
        return NULL;
    if (getline(&line, &line_cap, f) < 0) {
        fclose(f);
        free(line);
        return NULL;
    }
    indices = calloc(value_column_count + 1u, sizeof(*indices));
    vals = calloc(cap * value_column_count, sizeof(*vals));
    header_copy = strdup(line);
    if (!indices || !vals || !header_copy) {
        fclose(f);
        free(line);
        free(indices);
        free(vals);
        free(header_copy);
        return NULL;
    }
    indices[0] = ts_find_csv_column(header_copy, date_column);
    for (i = 0u; i < value_column_count; ++i) {
        strcpy(header_copy, line);
        indices[i + 1u] = ts_find_csv_column(header_copy, value_columns[i]);
    }
    if (indices[0] < 0) {
        fclose(f);
        free(line);
        free(indices);
        free(vals);
        free(header_copy);
        return NULL;
    }
    while ((line_len = getline(&line, &line_cap, f)) >= 0) {
        char *copy = strdup(line);
        char *save = NULL;
        char *tok;
        int col = 0;
        bool keep = true;
        char **selected = calloc(value_column_count + 1u, sizeof(*selected));

        if (!copy || !selected) {
            free(copy);
            free(selected);
            break;
        }
        tok = strtok_r(copy, ",", &save);
        while (tok) {
            if (col == indices[0])
                selected[0] = tok;
            for (i = 0u; i < value_column_count; ++i) {
                if (col == indices[i + 1u])
                    selected[i + 1u] = tok;
            }
            ++col;
            tok = strtok_r(NULL, ",", &save);
        }
        if (!selected[0] || ts_trim(selected[0])[0] == '\0')
            keep = false;
        for (i = 0u; i < value_column_count && keep; ++i) {
            if (!selected[i + 1u] || ts_trim(selected[i + 1u])[0] == '\0') {
                if (missing_policy == TS_MISSING_DROP || missing_policy == TS_MISSING_ERROR)
                    keep = false;
            }
        }
        if (keep) {
            if (rows == cap) {
                size_t new_cap = cap * 2u;
                double *grown = realloc(vals, new_cap * value_column_count * sizeof(*vals));

                if (!grown) {
                    free(copy);
                    free(selected);
                    break;
                }
                vals = grown;
                cap = new_cap;
            }
            for (i = 0u; i < value_column_count; ++i) {
                char *field = selected[i + 1u] ? ts_trim(selected[i + 1u]) : NULL;

                vals[rows * value_column_count + i] =
                    (!field || field[0] == '\0') ? NAN : strtod(field, NULL);
            }
            ++rows;
        }
        free(copy);
        free(selected);
    }
    fclose(f);
    free(line);
    free(indices);
    free(header_copy);
    out = ts_make_matrix_from_doubles(vals, rows, value_column_count);
    free(vals);
    return out;
}
