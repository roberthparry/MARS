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
