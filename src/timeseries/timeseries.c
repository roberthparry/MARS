#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timeseries.h"

struct timeseries_t {
    size_t length;
    number_t *values;
    datetime_t **index;
    bool has_index;
    bool *missing;
    ts_frequency_t frequency;
    ts_year_type_t year_type;
    bool is_regular;
    size_t season_period;
};

typedef struct ts_arima_meta_t {
    const ts_arima_result_t *owner;
    ts_arima_spec_t spec;
    bool has_intercept;
    bool has_drift;
    size_t trim;
    size_t fit_rows;
    number_t intercept;
    matrix_t *xreg_history;
    struct ts_arima_meta_t *next;
} ts_arima_meta_t;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} ts_string_builder_t;

static ts_arima_meta_t *ts_arima_meta_head = NULL;

static int ts_appendf(ts_string_builder_t *sb, const char *fmt, ...)
{
    va_list ap;
    va_list ap_copy;
    int needed;
    size_t required;
    char *grown;

    if (!sb || !fmt)
        return -1;

    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    needed = vsnprintf(NULL, 0u, fmt, ap_copy);
    va_end(ap_copy);
    if (needed < 0) {
        va_end(ap);
        return -1;
    }

    required = sb->len + (size_t)needed + 1u;
    if (required > sb->cap) {
        size_t new_cap = sb->cap ? sb->cap : 256u;

        while (new_cap < required)
            new_cap *= 2u;
        grown = realloc(sb->buf, new_cap);
        if (!grown) {
            va_end(ap);
            return -1;
        }
        sb->buf = grown;
        sb->cap = new_cap;
    }

    vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)needed;
    return 0;
}

static void ts_builder_free(ts_string_builder_t *sb)
{
    if (!sb)
        return;
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0u;
    sb->cap = 0u;
}

static char *ts_builder_detach(ts_string_builder_t *sb)
{
    char *out;

    if (!sb)
        return NULL;
    out = sb->buf;
    sb->buf = NULL;
    sb->len = 0u;
    sb->cap = 0u;
    return out;
}

static datetime_t *ts_datetime_clone(const datetime_t *dt)
{
    if (!dt)
        return NULL;
    return datetime_init_copy(datetime_alloc(), dt);
}

static int ts_datetime_is_month_end(const datetime_t *dt)
{
    short year;
    month_t month;
    uint8_t day;

    if (!dt)
        return 0;
    year = datetime_year(dt);
    month = datetime_month(dt);
    day = datetime_day(dt);
    if (year == SHRT_MAX || month == 0 || day == 0u)
        return 0;
    return day == datetime_days_in_month(year, month);
}

static datetime_t *ts_advance_regular_datetime(datetime_t *dt, ts_frequency_t frequency)
{
    int preserve_month_end;

    if (!dt)
        return NULL;
    preserve_month_end = ts_datetime_is_month_end(dt);
    switch (frequency) {
    case TS_FREQ_DAILY:
        return datetime_add_days(dt, 1L);
    case TS_FREQ_MONTHLY:
        if (!datetime_add_months(dt, 1))
            return NULL;
        break;
    case TS_FREQ_QUARTERLY:
        if (!datetime_add_months(dt, 3))
            return NULL;
        break;
    case TS_FREQ_YEARLY:
        if (!datetime_add_years(dt, 1))
            return NULL;
        break;
    default:
        return dt;
    }
    if (preserve_month_end) {
        short year = datetime_year(dt);
        month_t month = datetime_month(dt);
        uint8_t hour = datetime_hour(dt);
        uint8_t minute = datetime_minute(dt);
        double second = datetime_second(dt);
        uint8_t last_day;

        if (year == SHRT_MAX || month == 0)
            return dt;
        last_day = (uint8_t)datetime_days_in_month(year, month);
        if (!datetime_init_ymdt(dt, year, month, last_day, hour, minute, second))
            return NULL;
    }
    return dt;
}

static int ts_infer_season_period(ts_frequency_t frequency)
{
    switch (frequency) {
    case TS_FREQ_MONTHLY:
        return 12;
    case TS_FREQ_QUARTERLY:
        return 4;
    case TS_FREQ_YEARLY:
        return 1;
    default:
        return 0;
    }
}

static size_t ts_arima_max_lag(const ts_arima_spec_t *spec);
static size_t ts_arima_effective_seasonal_count(size_t count, size_t season_period);
static void ts_arima_build_lag_maps(const ts_arima_result_t *model,
                                    const ts_arima_spec_t *spec,
                                    double *phi_lags,
                                    double *theta_lags,
                                    size_t max_lag);

static int ts_parse_date(const char *text, datetime_t **out)
{
    int day, month, year;
    datetime_t *dt;

    if (!text || !out)
        return -1;
    if (sscanf(text, "%d/%d/%d", &day, &month, &year) != 3)
        return -1;
    dt = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day);
    if (!dt)
        return -1;
    *out = dt;
    return 0;
}

static bool ts_datetime_same_bucket(const datetime_t *a, const datetime_t *b,
                                    ts_frequency_t frequency, ts_year_type_t year_type)
{
    int ay, by;
    int am, bm;

    if (!a || !b)
        return false;
    ay = (int)datetime_year(a);
    by = (int)datetime_year(b);
    am = (int)datetime_month(a);
    bm = (int)datetime_month(b);

    if (frequency == TS_FREQ_DAILY)
        return datetime_year(a) == datetime_year(b) &&
               datetime_month(a) == datetime_month(b) &&
               datetime_day(a) == datetime_day(b);
    if (frequency == TS_FREQ_MONTHLY)
        return ay == by && am == bm;
    if (frequency == TS_FREQ_QUARTERLY) {
        int aq = (am - 1) / 3;
        int bq = (bm - 1) / 3;
        int ay_bucket = ay;
        int by_bucket = by;

        if (year_type == TS_YEAR_FISCAL_UK_APR) {
            aq = ((am + 8) % 12) / 3;
            bq = ((bm + 8) % 12) / 3;
            if (am < 4)
                ay_bucket -= 1;
            if (bm < 4)
                by_bucket -= 1;
        }
        return ay_bucket == by_bucket && aq == bq;
    }
    if (frequency == TS_FREQ_YEARLY) {
        if (year_type == TS_YEAR_FISCAL_UK_APR) {
            if (am < 4)
                ay -= 1;
            if (bm < 4)
                by -= 1;
        }
        return ay == by;
    }
    return false;
}

static datetime_t *ts_bucket_label(const datetime_t *dt, ts_frequency_t frequency,
                                   ts_year_type_t year_type)
{
    short year;
    month_t month;
    uint8_t day;

    if (!dt)
        return NULL;
    year = datetime_year(dt);
    month = datetime_month(dt);
    day = datetime_day(dt);

    switch (frequency) {
    case TS_FREQ_DAILY:
        return ts_datetime_clone(dt);
    case TS_FREQ_MONTHLY:
        return datetime_init_ymd(datetime_alloc(), year, month, day);
    case TS_FREQ_QUARTERLY:
        if (year_type == TS_YEAR_FISCAL_UK_APR) {
            int fiscal_year = year;
            int m = (int)month;
            int qstart;

            if (m < 4)
                fiscal_year -= 1;
            if (m >= 4 && m <= 6) qstart = 4;
            else if (m <= 9) qstart = 7;
            else if (m <= 12) qstart = 10;
            else qstart = 1;
            if (m < 4)
                qstart = 1;
            return datetime_init_ymd(datetime_alloc(), (short)(qstart == 1 ? fiscal_year + 1 : fiscal_year),
                                     (month_t)qstart, 1u);
        }
        return datetime_init_ymd(datetime_alloc(), year,
                                 (month_t)(((int)(month - 1) / 3) * 3 + 1), 1u);
    case TS_FREQ_YEARLY:
        if (year_type == TS_YEAR_FISCAL_UK_APR) {
            short fiscal_year = year;

            if ((int)month < 4)
                fiscal_year -= 1;
            return datetime_init_ymd(datetime_alloc(), fiscal_year, DT_April, 1u);
        }
        return datetime_init_ymd(datetime_alloc(), year, DT_January, 1u);
    default:
        return ts_datetime_clone(dt);
    }
}

static int ts_append_number_text(ts_string_builder_t *sb, number_t value)
{
    char *text = num_to_string(value);
    int rc;

    if (!text)
        return -1;
    rc = ts_appendf(sb, "%s", text);
    free(text);
    return rc;
}

static int ts_append_number_fixed(ts_string_builder_t *sb, number_t value, int decimals)
{
    double d;
    char buf[64];

    if (!sb)
        return -1;
    d = num_to_double(value);
    if (!isfinite(d))
        return ts_append_number_text(sb, value);
    snprintf(buf, sizeof(buf), "%.*f", decimals, d);
    return ts_appendf(sb, "%s", buf);
}

static int ts_append_number_sig(ts_string_builder_t *sb, number_t value, int sig_figs)
{
    double d;
    char buf[64];

    if (!sb)
        return -1;
    d = num_to_double(value);
    if (!isfinite(d))
        return ts_append_number_text(sb, value);
    snprintf(buf, sizeof(buf), "%.*g", sig_figs, d);
    return ts_appendf(sb, "%s", buf);
}

static timeseries_t *ts_alloc_empty(size_t length)
{
    timeseries_t *series = calloc(1u, sizeof(*series));

    if (!series)
        return NULL;
    series->length = length;
    series->values = calloc(length ? length : 1u, sizeof(*series->values));
    series->missing = calloc(length ? length : 1u, sizeof(*series->missing));
    if (!series->values || !series->missing) {
        ts_free(series);
        return NULL;
    }
    return series;
}

static void ts_copy_value_slot(number_t *dst, const number_t *src)
{
    if (!dst || !src)
        return;
    *dst = num_clone(*src);
}

static int ts_set_index(timeseries_t *series, const datetime_t *const *index)
{
    size_t i;

    if (!series || !index)
        return -1;
    series->index = calloc(series->length ? series->length : 1u, sizeof(*series->index));
    if (!series->index)
        return -1;
    for (i = 0u; i < series->length; ++i) {
        series->index[i] = ts_datetime_clone(index[i]);
        if (!series->index[i])
            return -1;
    }
    series->has_index = true;
    return 0;
}

static timeseries_t *ts_clone_shallow_shape(const timeseries_t *series)
{
    timeseries_t *out;

    if (!series)
        return NULL;
    out = ts_alloc_empty(series->length);
    if (!out)
        return NULL;
    out->frequency = series->frequency;
    out->year_type = series->year_type;
    out->is_regular = series->is_regular;
    out->season_period = series->season_period;
    return out;
}

static int ts_copy_into(timeseries_t *dst, const timeseries_t *src)
{
    size_t i;

    if (!dst || !src || dst->length != src->length)
        return -1;
    for (i = 0u; i < src->length; ++i) {
        ts_copy_value_slot(&dst->values[i], &src->values[i]);
        dst->missing[i] = src->missing[i];
    }
    if (src->has_index && ts_set_index(dst, (const datetime_t *const *)src->index) != 0)
        return -1;
    return 0;
}

static timeseries_t *ts_make_empty_regular_series(size_t length,
                                                  const datetime_t *start,
                                                  ts_frequency_t frequency,
                                                  ts_year_type_t year_type)
{
    timeseries_t *series;
    datetime_t *cursor = NULL;
    size_t i;

    if (!start)
        return NULL;
    series = ts_alloc_empty(length);
    if (!series)
        return NULL;
    series->index = calloc(length ? length : 1u, sizeof(*series->index));
    if (!series->index) {
        ts_free(series);
        return NULL;
    }
    series->has_index = true;
    series->frequency = frequency;
    series->year_type = year_type;
    series->is_regular = true;
    series->season_period = (size_t)ts_infer_season_period(frequency);
    cursor = ts_datetime_clone(start);
    if (!cursor) {
        ts_free(series);
        return NULL;
    }
    for (i = 0u; i < length; ++i) {
        series->index[i] = ts_datetime_clone(cursor);
        if (!series->index[i]) {
            datetime_dealloc(cursor);
            ts_free(series);
            return NULL;
        }
        if (!ts_advance_regular_datetime(cursor, frequency)) {
            datetime_dealloc(cursor);
            ts_free(series);
            return NULL;
        }
    }
    datetime_dealloc(cursor);
    return series;
}

static int ts_series_to_double_array(const timeseries_t *series, double **out_values,
                                     size_t *out_count, size_t start)
{
    size_t i;
    size_t n = 0u;
    double *vals;

    if (!series || !out_values || !out_count || start > series->length)
        return -1;
    for (i = start; i < series->length; ++i) {
        if (!series->missing[i])
            n++;
    }
    vals = calloc(n ? n : 1u, sizeof(*vals));
    if (!vals)
        return -1;
    n = 0u;
    for (i = start; i < series->length; ++i) {
        if (!series->missing[i])
            vals[n++] = num_to_double(series->values[i]);
    }
    *out_values = vals;
    *out_count = n;
    return 0;
}

static matrix_t *ts_make_column_matrix_from_doubles(const double *values, size_t n)
{
    number_t *nums;
    matrix_t *out;
    size_t i;

    nums = calloc(n ? n : 1u, sizeof(*nums));
    if (!nums)
        return NULL;
    for (i = 0u; i < n; ++i)
        nums[i] = num_create_from_double(values[i]);
    out = mat_create(n, 1u, nums);
    for (i = 0u; i < n; ++i)
        num_destroy(&nums[i]);
    free(nums);
    return out;
}

static matrix_t *ts_make_matrix_from_doubles(const double *values, size_t rows, size_t cols)
{
    size_t i, count = rows * cols;
    number_t *nums = calloc(count ? count : 1u, sizeof(*nums));
    matrix_t *out;

    if (!nums)
        return NULL;
    for (i = 0u; i < count; ++i)
        nums[i] = num_create_from_double(values[i]);
    out = mat_create(rows, cols, nums);
    for (i = 0u; i < count; ++i)
        num_destroy(&nums[i]);
    free(nums);
    return out;
}

static int ts_write_text_file(const char *path, const char *text)
{
    FILE *f;

    if (!path || !text)
        return -1;
    f = fopen(path, "w");
    if (!f)
        return -1;
    if (fputs(text, f) == EOF) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static ts_arima_meta_t *ts_arima_meta_find(const ts_arima_result_t *owner)
{
    ts_arima_meta_t *node = ts_arima_meta_head;

    while (node) {
        if (node->owner == owner)
            return node;
        node = node->next;
    }
    return NULL;
}

static matrix_t *ts_matrix_clone_local(const matrix_t *src)
{
    matrix_t *out;
    size_t rows, cols, r, c;

    if (!src)
        return NULL;
    rows = mat_get_row_count(src);
    cols = mat_get_col_count(src);
    out = mat_new(rows, cols);
    if (!out)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        for (c = 0u; c < cols; ++c) {
            number_t v = mat_get_num(src, r, c);
            mat_set(out, r, c, &v);
            num_destroy(&v);
        }
    }
    return out;
}

static void ts_arima_meta_remove(const ts_arima_result_t *owner)
{
    ts_arima_meta_t **link = &ts_arima_meta_head;

    while (*link) {
        if ((*link)->owner == owner) {
            ts_arima_meta_t *victim = *link;

            *link = victim->next;
            if (victim->has_intercept)
                num_destroy(&victim->intercept);
            mat_free(victim->xreg_history);
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

static int ts_arima_meta_store(const ts_arima_result_t *owner,
                               const ts_arima_spec_t *spec,
                               bool has_intercept,
                               bool has_drift,
                               size_t trim,
                               size_t fit_rows,
                               const number_t *intercept,
                               const matrix_t *xreg_history)
{
    ts_arima_meta_t *node;

    if (!owner || !spec)
        return -1;
    ts_arima_meta_remove(owner);
    node = calloc(1u, sizeof(*node));
    if (!node)
        return -1;
    node->owner = owner;
    node->spec = *spec;
    node->has_intercept = has_intercept;
    node->has_drift = has_drift;
    node->trim = trim;
    node->fit_rows = fit_rows;
    if (has_intercept && intercept)
        node->intercept = num_clone(*intercept);
    if (xreg_history) {
        node->xreg_history = ts_matrix_clone_local(xreg_history);
        if (!node->xreg_history) {
            if (node->has_intercept)
                num_destroy(&node->intercept);
            free(node);
            return -1;
        }
    }
    node->next = ts_arima_meta_head;
    ts_arima_meta_head = node;
    return 0;
}

static int ts_arima_meta_transfer(const ts_arima_result_t *dst,
                                  const ts_arima_result_t *src)
{
    ts_arima_meta_t *meta;

    if (!dst || !src)
        return -1;
    meta = ts_arima_meta_find(src);
    if (!meta)
        return 0;
    return ts_arima_meta_store(dst,
                               &meta->spec,
                               meta->has_intercept,
                               meta->has_drift,
                               meta->trim,
                               meta->fit_rows,
                               meta->has_intercept ? &meta->intercept : NULL,
                               meta->xreg_history);
}

static void ts_result_clear_numbers(ts_fit_summary_t *summary)
{
    if (!summary)
        return;
    num_destroy(&summary->sigma2);
    num_destroy(&summary->loglik);
    num_destroy(&summary->aic);
    num_destroy(&summary->aicc);
    num_destroy(&summary->bic);
    memset(summary, 0, sizeof(*summary));
}

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
        switch (frequency) {
        case TS_FREQ_DAILY:
            datetime_add_days(cursor, 1L);
            break;
        case TS_FREQ_MONTHLY:
            datetime_add_months(cursor, 1);
            break;
        case TS_FREQ_QUARTERLY:
            datetime_add_months(cursor, 3);
            break;
        case TS_FREQ_YEARLY:
            datetime_add_years(cursor, 1);
            break;
        default:
            break;
        }
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

char *ts_to_string(const timeseries_t *series, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!series)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,value\n");
    for (i = 0u; i < series->length; ++i) {
        char *date_text = NULL;

        if (series->has_index && series->index[i])
            date_text = datetime_format(series->index[i], "%dd/%mm/%yyyy");
        if (style == TS_STRING_CSV) {
            ts_appendf(&sb, "%s,", date_text ? date_text : "");
            if (series->missing[i])
                ts_appendf(&sb, "\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else if (style == TS_STRING_PRETTY) {
            ts_appendf(&sb, "%s%s", date_text ? date_text : "", date_text ? " : " : "");
            if (series->missing[i])
                ts_appendf(&sb, "(missing)\n");
            else {
                ts_append_number_text(&sb, series->values[i]);
                ts_appendf(&sb, "\n");
            }
        } else {
            if (i > 0u)
                ts_appendf(&sb, "; ");
            if (date_text)
                ts_appendf(&sb, "%s=", date_text);
            if (series->missing[i])
                ts_appendf(&sb, "NA");
            else
                ts_append_number_text(&sb, series->values[i]);
        }
        free(date_text);
    }
    return ts_builder_detach(&sb);
}

static int ts_vformat(char *out, size_t out_size, const char *fmt, va_list ap)
{
    ts_string_builder_t sb = {0};
    const char *p = fmt;

    while (p && *p) {
        if (*p != '%') {
            if (ts_appendf(&sb, "%c", *p) != 0) {
                ts_builder_free(&sb);
                return -1;
            }
            ++p;
            continue;
        }
        ++p;
        if (*p == '%') {
            ts_appendf(&sb, "%%");
            ++p;
            continue;
        }
        if (*p == 't' || *p == 'T' || *p == 'C') {
            timeseries_t *series = va_arg(ap, timeseries_t *);
            char *text = ts_to_string(series,
                                      *p == 'T' ? TS_STRING_PRETTY :
                                      (*p == 'C' ? TS_STRING_CSV : TS_STRING_INLINE));

            if (!text) {
                ts_builder_free(&sb);
                return -1;
            }
            ts_appendf(&sb, "%s", text);
            free(text);
            ++p;
            continue;
        }
        if (*p == 's') {
            const char *s = va_arg(ap, const char *);

            ts_appendf(&sb, "%s", s ? s : "(null)");
            ++p;
            continue;
        }
        if (*p == 'd') {
            int v = va_arg(ap, int);

            ts_appendf(&sb, "%d", v);
            ++p;
            continue;
        }
        if (*p == 'z' && p[1] == 'u') {
            size_t v = va_arg(ap, size_t);

            ts_appendf(&sb, "%zu", v);
            p += 2;
            continue;
        }
        ts_appendf(&sb, "%%%c", *p);
        ++p;
    }
    if (!out || out_size == 0u) {
        int n = (int)sb.len;

        ts_builder_free(&sb);
        return n;
    }
    snprintf(out, out_size, "%s", sb.buf ? sb.buf : "");
    {
        int n = (int)sb.len;
        ts_builder_free(&sb);
        return n;
    }
}

int ts_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int rc;
    va_list ap;

    va_start(ap, fmt);
    rc = ts_vformat(out, out_size, fmt, ap);
    va_end(ap);
    return rc;
}

int ts_printf(const char *fmt, ...)
{
    va_list ap;
    char *text = NULL;
    int n;

    va_start(ap, fmt);
    n = ts_vformat(NULL, 0u, fmt, ap);
    va_end(ap);
    if (n < 0)
        return -1;
    text = malloc((size_t)n + 1u);
    if (!text)
        return -1;
    va_start(ap, fmt);
    ts_vformat(text, (size_t)n + 1u, fmt, ap);
    va_end(ap);
    fputs(text, stdout);
    free(text);
    return n;
}

void ts_print(const timeseries_t *series)
{
    char *text = ts_to_string(series, TS_STRING_PRETTY);

    if (!text)
        return;
    fputs(text, stdout);
    free(text);
}

char *ts_forecast_to_string(const ts_forecast_t *forecast, ts_string_style_t style)
{
    ts_string_builder_t sb = {0};
    size_t i;

    if (!forecast || !forecast->mean)
        return NULL;
    if (style == TS_STRING_CSV)
        ts_appendf(&sb, "date,mean,stderr,lower,upper\n");
    for (i = 0u; i < ts_length(forecast->mean); ++i) {
        datetime_t *dt = ts_start_datetime(forecast->mean);
        char *date_text;
        number_t value, stderr_v, lower_v, upper_v;

        if (dt)
            datetime_dealloc(dt);
        date_text = (forecast->mean->has_index && forecast->mean->index[i])
            ? datetime_format(forecast->mean->index[i], "%dd/%mm/%yyyy") : NULL;
        value = mat_get_num(ts_to_column_matrix(forecast->mean), i, 0);
        stderr_v = forecast->stderr ? forecast->stderr->values[i] : num_clone(NUM_NAN);
        lower_v = forecast->lower ? forecast->lower->values[i] : num_clone(NUM_NAN);
        upper_v = forecast->upper ? forecast->upper->values[i] : num_clone(NUM_NAN);
        if (style == TS_STRING_CSV) {
            ts_appendf(&sb, "%s,", date_text ? date_text : "");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, stderr_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ",");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        } else {
            ts_appendf(&sb, "%s: best estimate ", date_text ? date_text : "");
            ts_append_number_fixed(&sb, value, 2);
            ts_appendf(&sb, ", plausible lower end: ");
            ts_append_number_fixed(&sb, lower_v, 2);
            ts_appendf(&sb, ", plausible upper end: ");
            ts_append_number_fixed(&sb, upper_v, 2);
            ts_appendf(&sb, "\n");
        }
        free(date_text);
        num_destroy(&value);
        if (!forecast->stderr) num_destroy(&stderr_v);
        if (!forecast->lower) num_destroy(&lower_v);
        if (!forecast->upper) num_destroy(&upper_v);
    }
    return ts_builder_detach(&sb);
}

static void ts_append_r2_rating(ts_string_builder_t *sb, number_t value)
{
    double v = num_to_double(value);

    if (!sb)
        return;
    if (v > 0.95)
        ts_appendf(sb, " (exceptional)");
    else if (v >= 0.90)
        ts_appendf(sb, " (excellent)");
    else if (v >= 0.80)
        ts_appendf(sb, " (very good)");
    else if (v >= 0.65)
        ts_appendf(sb, " (good)");
    else if (v >= 0.40)
        ts_appendf(sb, " (mediocre; a good result is usually above 0.65 and a very good one above 0.80)");
    else
        ts_appendf(sb, " (poor; a good result is usually above 0.65 and a very good one above 0.80)");
}

static double ts_mean_abs_series_value(const timeseries_t *series)
{
    size_t i, count = 0u;
    double total = 0.0;

    if (!series)
        return NAN;
    for (i = 0u; i < series->length; ++i) {
        if (series->missing[i])
            continue;
        total += fabs(num_to_double(series->values[i]));
        count++;
    }
    return count ? total / (double)count : NAN;
}

static void ts_append_relative_error_rating(ts_string_builder_t *sb,
                                            const char *label,
                                            number_t value,
                                            double baseline)
{
    double ratio;

    if (!sb)
        return;
    if (!isfinite(baseline) || baseline <= 0.0) {
        ts_appendf(sb, " (comparison only; smaller is better)");
        return;
    }
    ratio = fabs(num_to_double(value)) / baseline;
    if (ratio < 0.05)
        ts_appendf(sb, " (exceptional)");
    else if (ratio < 0.10)
        ts_appendf(sb, " (excellent)");
    else if (ratio < 0.15)
        ts_appendf(sb, " (very good)");
    else if (ratio < 0.25)
        ts_appendf(sb, " (good)");
    else if (ratio < 0.40)
        ts_appendf(sb, " (mediocre; good is usually below about 25%% of the usual monthly level)");
    else
        ts_appendf(sb, " (poor; good is usually below about 25%% of the usual monthly level, and very good below about 15%%)");
    (void)label;
}

static void ts_append_information_criterion_rating(ts_string_builder_t *sb)
{
    if (!sb)
        return;
    ts_appendf(sb, " (comparison only; lower is better when comparing models on the same series)");
}

static void ts_append_sigma2_rating(ts_string_builder_t *sb, number_t sigma2, double baseline)
{
    double ratio;

    if (!sb)
        return;
    if (!isfinite(baseline) || baseline <= 0.0) {
        ts_appendf(sb, " (comparison only; lower is better)");
        return;
    }
    ratio = fabs(num_to_double(sigma2)) / baseline;
    if (ratio < 0.0025)
        ts_appendf(sb, " (exceptional)");
    else if (ratio < 0.01)
        ts_appendf(sb, " (excellent)");
    else if (ratio < 0.0225)
        ts_appendf(sb, " (very good)");
    else if (ratio < 0.0625)
        ts_appendf(sb, " (good)");
    else if (ratio < 0.16)
        ts_appendf(sb, " (mediocre; good is usually much smaller relative to the scale of the series)");
    else
        ts_appendf(sb, " (poor; good is usually much smaller relative to the scale of the series, and very good smaller still)");
}

static const char *ts_r2_band(number_t value)
{
    double v = num_to_double(value);

    if (v > 0.95)
        return "exceptional";
    if (v >= 0.90)
        return "excellent";
    if (v >= 0.80)
        return "very good";
    if (v >= 0.65)
        return "good";
    if (v >= 0.40)
        return "ok";
    return "poor";
}

static const char *ts_relative_error_band(number_t value, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return "comparison";
    ratio = fabs(num_to_double(value)) / baseline;
    if (ratio < 0.05)
        return "exceptional";
    if (ratio < 0.10)
        return "excellent";
    if (ratio < 0.15)
        return "very good";
    if (ratio < 0.25)
        return "good";
    if (ratio < 0.40)
        return "ok";
    return "poor";
}

static const char *ts_sigma2_band(number_t sigma2, double baseline)
{
    double ratio;

    if (!isfinite(baseline) || baseline <= 0.0)
        return "comparison";
    ratio = fabs(num_to_double(sigma2)) / baseline;
    if (ratio < 0.0025)
        return "exceptional";
    if (ratio < 0.01)
        return "excellent";
    if (ratio < 0.0225)
        return "very good";
    if (ratio < 0.0625)
        return "good";
    if (ratio < 0.16)
        return "ok";
    return "poor";
}

static int ts_band_score(const char *band)
{
    if (!band)
        return 0;
    if (strcmp(band, "exceptional") == 0)
        return 6;
    if (strcmp(band, "excellent") == 0)
        return 5;
    if (strcmp(band, "very good") == 0)
        return 4;
    if (strcmp(band, "good") == 0)
        return 3;
    if (strcmp(band, "ok") == 0)
        return 2;
    if (strcmp(band, "poor") == 0)
        return 1;
    return 0;
}

static void ts_append_overall_assessment(ts_string_builder_t *sb,
                                         const char *band,
                                         bool comparison_only)
{
    if (!sb || !band)
        return;
    if (comparison_only) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model so far. Proceed, but compare it with another candidate before deciding. To improve it in this app, run the forecast again with one simpler set of settings and one more seasonal set of settings, then keep the version with the lower comparison score on the same data.\n",
                   band);
        return;
    }
    if (strcmp(band, "exceptional") == 0 || strcmp(band, "excellent") == 0 ||
        strcmp(band, "very good") == 0 || strcmp(band, "good") == 0) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a %s model. Proceed. If you want to improve it further, try two extra runs in this app: first, make the model simpler by lowering one of p, q, P, or Q; second, if the data is monthly, try a seasonal version with season period 12 and P or D set to 1. Keep the version that gives lower error and still looks realistic.\n",
                   band);
    } else if (strcmp(band, "ok") == 0) {
        ts_appendf(sb,
                   "Overall assessment: You've chosen an ok model. Proceed with caution, and revisit it if the forecast looks unrealistic. To get a better result, first check that the target dates and exogenous dates line up properly. Then run it again with a seasonal model if the data is monthly, or try a different model type such as SARIMAX instead of regression alone. After that, compare the typical forecast error and keep the version that is lower and still believable.\n");
    } else {
        ts_appendf(sb,
                   "Overall assessment: You've chosen a poor model. Revisit the settings, the data, or both before relying on this forecast. To improve it, work through these steps: check for missing or misaligned dates, use a longer clean history if available, try a seasonal model for monthly data with season period 12, reduce over-complicated settings by lowering p, q, P, or Q, and test whether the exogenous inputs are actually helping by running the model with and without them.\n");
    }
}

char *ts_regression_summary_to_string(const ts_regression_result_t *result)
{
    ts_string_builder_t sb = {0};
    size_t i;
    double fitted_scale;
    const char *fit_band;
    const char *error_band;
    const char *overall_band;

    if (!result || !result->coefficients)
        return NULL;
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    fit_band = ts_r2_band(result->adj_r2);
    error_band = ts_relative_error_band(result->rmse, fitted_scale);
    overall_band = ts_band_score(fit_band) < ts_band_score(error_band) ? fit_band : error_band;
    ts_appendf(&sb, "Regression summary\n");
    ts_append_overall_assessment(&sb, overall_band, false);
    ts_appendf(&sb, "Overall fit score (R2): ");
    ts_append_number_fixed(&sb, result->r2, 4);
    ts_append_r2_rating(&sb, result->r2);
    ts_appendf(&sb, "\nAdjusted fit score (Adj R2): ");
    ts_append_number_fixed(&sb, result->adj_r2, 4);
    ts_append_r2_rating(&sb, result->adj_r2);
    ts_appendf(&sb, "\nTypical forecast error (RMSE): ");
    ts_append_number_fixed(&sb, result->rmse, 2);
    ts_append_relative_error_rating(&sb, "RMSE", result->rmse, fitted_scale);
    ts_appendf(&sb, "\nModel comparison score (AIC): ");
    ts_append_number_fixed(&sb, result->summary.aic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nModel comparison score (BIC): ");
    ts_append_number_fixed(&sb, result->summary.bic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\n\nHow to read this:\n");
    ts_appendf(&sb, "- Higher fit scores are better.\n");
    ts_appendf(&sb, "- Lower error is better.\n");
    ts_appendf(&sb, "- Lower AIC/BIC is only better when comparing models on the same data.\n");
    ts_appendf(&sb, "\nCoefficients\n");
    for (i = 0u; i < mat_get_row_count(result->coefficients); ++i) {
        number_t beta = mat_get_num(result->coefficients, i, 0);
        number_t se = result->stderr ? mat_get_num(result->stderr, i, 0) : num_clone(NUM_NAN);
        number_t p = result->p_value ? mat_get_num(result->p_value, i, 0) : num_clone(NUM_NAN);

        ts_appendf(&sb, "beta[%zu] = ", i);
        ts_append_number_sig(&sb, beta, 6);
        ts_appendf(&sb, "  stderr = ");
        ts_append_number_sig(&sb, se, 4);
        ts_appendf(&sb, "  p = ");
        ts_append_number_sig(&sb, p, 4);
        ts_appendf(&sb, "\n");
        num_destroy(&beta);
        num_destroy(&se);
        num_destroy(&p);
    }
    return ts_builder_detach(&sb);
}

char *ts_arima_summary_to_string(const ts_arima_result_t *result)
{
    ts_string_builder_t sb = {0};
    ts_arima_meta_t *meta;
    size_t i;
    size_t offset = 0u;
    double fitted_scale;
    const char *sigma_band;

    if (!result)
        return NULL;
    meta = ts_arima_meta_find(result);
    fitted_scale = ts_mean_abs_series_value(result->fitted);
    sigma_band = ts_sigma2_band(result->summary.sigma2, fitted_scale * fitted_scale);
    ts_appendf(&sb, "ARIMA summary\n");
    ts_append_overall_assessment(&sb, sigma_band, strcmp(sigma_band, "comparison") == 0);
    if (meta) {
        ts_appendf(&sb, "Model: (%zu,%zu,%zu)x(%zu,%zu,%zu)[%zu]\n",
                   meta->spec.p, meta->spec.d, meta->spec.q,
                   meta->spec.P, meta->spec.D, meta->spec.Q,
                   meta->spec.season_period);
    }
    ts_appendf(&sb, "Model comparison score (AIC): ");
    ts_append_number_fixed(&sb, result->summary.aic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nModel comparison score (BIC): ");
    ts_append_number_fixed(&sb, result->summary.bic, 2);
    ts_append_information_criterion_rating(&sb);
    ts_appendf(&sb, "\nUnexplained variation left in the model (Sigma2): ");
    ts_append_number_fixed(&sb, result->summary.sigma2, 2);
    ts_append_sigma2_rating(&sb, result->summary.sigma2, fitted_scale * fitted_scale);
    ts_appendf(&sb, "\n\nHow to read this:\n");
    ts_appendf(&sb, "- Lower AIC/BIC is only better when comparing models on the same data.\n");
    ts_appendf(&sb, "- Lower unexplained variation is better.\n");
    ts_appendf(&sb, "- Use the forecast table and interval width as well, not just one score.\n");
    ts_appendf(&sb, "\nAR parameters\n");
    if (result->ar_params) {
        for (i = 0u; i < mat_get_row_count(result->ar_params); ++i) {
            number_t v = mat_get_num(result->ar_params, i, 0);

            ts_appendf(&sb, "phi[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
        }
    }
    if (result->seasonal_ar_params && meta && meta->spec.season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(result->seasonal_ar_params); ++i) {
            number_t v = mat_get_num(result->seasonal_ar_params, i, 0);

            ts_appendf(&sb, "Phi[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  (lag %zu)\n", (i + 1u) * meta->spec.season_period);
            num_destroy(&v);
        }
    }
    ts_appendf(&sb, "\nMA parameters\n");
    if (result->ma_params) {
        for (i = 0u; i < mat_get_row_count(result->ma_params); ++i) {
            number_t v = mat_get_num(result->ma_params, i, 0);

            ts_appendf(&sb, "theta[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
        }
    }
    if (result->seasonal_ma_params && meta && meta->spec.season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(result->seasonal_ma_params); ++i) {
            number_t v = mat_get_num(result->seasonal_ma_params, i, 0);

            ts_appendf(&sb, "Theta[%zu] = ", i + 1u);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  (lag %zu)\n", (i + 1u) * meta->spec.season_period);
            num_destroy(&v);
        }
    }
    if (result->xreg_params) {
        size_t display_index = 0u;

        ts_appendf(&sb, "\nCoefficients\n");
        if (meta && meta->has_intercept) {
            number_t v = mat_get_num(result->xreg_params, 0u, 0u);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, 0u, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, 0u, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta[0] = ");
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            offset = 1u;
            display_index = 1u;
        }
        if (meta && meta->has_drift) {
            number_t v = mat_get_num(result->xreg_params, offset, 0u);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, offset, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, offset, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "drift = ");
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            offset += 1u;
        }
        if (meta) {
            offset += meta->spec.p;
            offset += ts_arima_effective_seasonal_count(meta->spec.P, meta->spec.season_period);
            offset += meta->spec.q;
            offset += ts_arima_effective_seasonal_count(meta->spec.Q, meta->spec.season_period);
        }
        for (i = offset; i < mat_get_row_count(result->xreg_params); ++i) {
            number_t v = mat_get_num(result->xreg_params, i, 0);
            number_t se = result->param_stderr ? mat_get_num(result->param_stderr, i, 0u) : num_clone(NUM_NAN);
            number_t p = result->param_p_value ? mat_get_num(result->param_p_value, i, 0u) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta[%zu] = ", display_index);
            ts_append_number_sig(&sb, v, 6);
            ts_appendf(&sb, "  stderr = ");
            ts_append_number_sig(&sb, se, 4);
            ts_appendf(&sb, "  p = ");
            ts_append_number_sig(&sb, p, 4);
            ts_appendf(&sb, "\n");
            num_destroy(&v);
            num_destroy(&se);
            num_destroy(&p);
            display_index += 1u;
        }
    }
    return ts_builder_detach(&sb);
}

int ts_write_file(const char *path,
                  const timeseries_t *series,
                  ts_string_style_t style)
{
    char *text = ts_to_string(series, style);
    int rc = text ? ts_write_text_file(path, text) : -1;

    free(text);
    return rc;
}

int ts_forecast_write_file(const char *path,
                           const ts_forecast_t *forecast,
                           ts_string_style_t style)
{
    char *text = ts_forecast_to_string(forecast, style);
    int rc = text ? ts_write_text_file(path, text) : -1;

    free(text);
    return rc;
}

int ts_regression_summary_write_file(const char *path,
                                     const ts_regression_result_t *result,
                                     ts_string_style_t style)
{
    char *text;

    if (style == TS_STRING_CSV && result && result->coefficients) {
        ts_string_builder_t sb = {0};
        size_t i;

        ts_appendf(&sb, "term,estimate,stderr,t_stat,p_value\n");
        for (i = 0u; i < mat_get_row_count(result->coefficients); ++i) {
            number_t est = mat_get_num(result->coefficients, i, 0);
            number_t se = result->stderr ? mat_get_num(result->stderr, i, 0) : num_clone(NUM_NAN);
            number_t t = result->t_stat ? mat_get_num(result->t_stat, i, 0) : num_clone(NUM_NAN);
            number_t p = result->p_value ? mat_get_num(result->p_value, i, 0) : num_clone(NUM_NAN);

            ts_appendf(&sb, "beta%zu,", i);
            ts_append_number_sig(&sb, est, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, se, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, t, 6);
            ts_appendf(&sb, ",");
            ts_append_number_sig(&sb, p, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&est);
            num_destroy(&se);
            num_destroy(&t);
            num_destroy(&p);
        }
        text = ts_builder_detach(&sb);
    } else {
        text = ts_regression_summary_to_string(result);
    }
    if (!text)
        return -1;
    {
        int rc = ts_write_text_file(path, text);
        free(text);
        return rc;
    }
}

int ts_arima_summary_write_file(const char *path,
                                const ts_arima_result_t *result,
                                ts_string_style_t style)
{
    char *text;

    if (style == TS_STRING_CSV && result && result->ar_params) {
        ts_string_builder_t sb = {0};
        size_t i;

        ts_appendf(&sb, "term,estimate\n");
        for (i = 0u; i < mat_get_row_count(result->ar_params); ++i) {
            number_t est = mat_get_num(result->ar_params, i, 0);

            ts_appendf(&sb, "phi%zu,", i + 1u);
            ts_append_number_sig(&sb, est, 6);
            ts_appendf(&sb, "\n");
            num_destroy(&est);
        }
        text = ts_builder_detach(&sb);
    } else {
        text = ts_arima_summary_to_string(result);
    }
    if (!text)
        return -1;
    {
        int rc = ts_write_text_file(path, text);
        free(text);
        return rc;
    }
}

size_t ts_length(const timeseries_t *series) { return series ? series->length : 0u; }
ts_index_info_t ts_index_info(const timeseries_t *series)
{
    ts_index_info_t info = { TS_FREQ_UNKNOWN, TS_YEAR_CALENDAR, false, false, 0u };

    if (!series)
        return info;
    info.frequency = series->frequency;
    info.year_type = series->year_type;
    info.is_regular = series->is_regular;
    info.has_datetimes = series->has_index;
    info.season_period = series->season_period;
    return info;
}
ts_frequency_t ts_frequency(const timeseries_t *series) { return series ? series->frequency : TS_FREQ_UNKNOWN; }
ts_year_type_t ts_year_type(const timeseries_t *series) { return series ? series->year_type : TS_YEAR_CALENDAR; }
bool ts_is_regular(const timeseries_t *series) { return series && series->is_regular; }
bool ts_has_missing(const timeseries_t *series)
{
    size_t i;
    if (!series)
        return false;
    for (i = 0u; i < series->length; ++i)
        if (series->missing[i])
            return true;
    return false;
}

int ts_get_value(const timeseries_t *series, size_t index, number_t *out)
{
    if (!series || !out || index >= series->length)
        return -1;
    *out = num_clone(series->values[index]);
    return 0;
}

int ts_get_datetime(const timeseries_t *series, size_t index, datetime_t *out)
{
    if (!series || !out || !series->has_index || index >= series->length)
        return -1;
    if (!series->index[index])
        return -1;
    return datetime_init_copy(out, series->index[index]) ? 0 : -1;
}

datetime_t *ts_start_datetime(const timeseries_t *series)
{
    size_t i;

    if (!series || !series->has_index || series->length == 0u)
        return NULL;
    for (i = 0u; i < series->length; ++i) {
        if (series->index[i])
            return ts_datetime_clone(series->index[i]);
    }
    return NULL;
}

datetime_t *ts_end_datetime(const timeseries_t *series)
{
    size_t i;

    if (!series || !series->has_index || series->length == 0u)
        return NULL;
    for (i = series->length; i > 0u; --i) {
        if (series->index[i - 1u])
            return ts_datetime_clone(series->index[i - 1u]);
    }
    return NULL;
}

timeseries_t *ts_slice(const timeseries_t *series, size_t start, size_t length)
{
    timeseries_t *out;
    size_t i;

    if (!series || start > series->length || start + length > series->length)
        return NULL;
    out = ts_alloc_empty(length);
    if (!out)
        return NULL;
    out->frequency = series->frequency;
    out->year_type = series->year_type;
    out->is_regular = series->is_regular;
    out->season_period = series->season_period;
    if (series->has_index) {
        out->index = calloc(length ? length : 1u, sizeof(*out->index));
        if (!out->index) {
            ts_free(out);
            return NULL;
        }
        out->has_index = true;
    }
    for (i = 0u; i < length; ++i) {
        out->values[i] = num_clone(series->values[start + i]);
        out->missing[i] = series->missing[start + i];
        if (series->has_index)
            out->index[i] = ts_datetime_clone(series->index[start + i]);
    }
    return out;
}

timeseries_t *ts_slice_date(const timeseries_t *series,
                            const datetime_t *start,
                            const datetime_t *end)
{
    size_t i, first = 0u, last = 0u;
    bool found = false;

    if (!series || !series->has_index || !start || !end)
        return NULL;
    for (i = 0u; i < series->length; ++i) {
        if (!series->index[i])
            continue;
        if (datetime_compare(series->index[i], start) >= 0 &&
            datetime_compare(series->index[i], end) <= 0) {
            if (!found)
                first = i;
            last = i;
            found = true;
        }
    }
    return found ? ts_slice(series, first, last - first + 1u) : NULL;
}

timeseries_t *ts_head(const timeseries_t *series, size_t n)
{
    return ts_slice(series, 0u, n < ts_length(series) ? n : ts_length(series));
}

timeseries_t *ts_tail(const timeseries_t *series, size_t n)
{
    size_t len = ts_length(series);
    size_t use = n < len ? n : len;

    return ts_slice(series, len - use, use);
}

timeseries_t *ts_fill_missing(const timeseries_t *series, ts_missing_policy_t policy)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out)
        return NULL;
    if (policy == TS_MISSING_KEEP || policy == TS_MISSING_ERROR)
        return out;
    for (i = 0u; i < out->length; ++i) {
        if (!out->missing[i])
            continue;
        if (policy == TS_MISSING_FORWARD_FILL && i > 0u && !out->missing[i - 1u]) {
            num_destroy(&out->values[i]);
            out->values[i] = num_clone(out->values[i - 1u]);
            out->missing[i] = false;
        } else if (policy == TS_MISSING_BACKWARD_FILL && i + 1u < out->length &&
                   !out->missing[i + 1u]) {
            num_destroy(&out->values[i]);
            out->values[i] = num_clone(out->values[i + 1u]);
            out->missing[i] = false;
        } else if (policy == TS_MISSING_INTERPOLATE_LINEAR &&
                   i > 0u && i + 1u < out->length &&
                   !out->missing[i - 1u] && !out->missing[i + 1u]) {
            number_t sum = num_add(out->values[i - 1u], out->values[i + 1u]);
            number_t avg = num_div(sum, NUM_TWO);

            num_destroy(&out->values[i]);
            out->values[i] = avg;
            out->missing[i] = false;
            num_destroy(&sum);
        }
    }
    return out;
}

timeseries_t *ts_drop_missing(const timeseries_t *series)
{
    size_t i, keep = 0u, idx = 0u;
    timeseries_t *out;

    if (!series)
        return NULL;
    for (i = 0u; i < series->length; ++i)
        if (!series->missing[i])
            keep++;
    out = ts_alloc_empty(keep);
    if (!out)
        return NULL;
    out->frequency = series->frequency;
    out->year_type = series->year_type;
    out->is_regular = series->is_regular;
    out->season_period = series->season_period;
    if (series->has_index) {
        out->index = calloc(keep ? keep : 1u, sizeof(*out->index));
        if (!out->index) {
            ts_free(out);
            return NULL;
        }
        out->has_index = true;
    }
    for (i = 0u; i < series->length; ++i) {
        if (series->missing[i])
            continue;
        out->values[idx] = num_clone(series->values[i]);
        if (series->has_index)
            out->index[idx] = ts_datetime_clone(series->index[i]);
        idx++;
    }
    return out;
}

int ts_align_pair(const timeseries_t *left, const timeseries_t *right,
                  ts_join_type_t join_type,
                  timeseries_t **left_out, timeseries_t **right_out)
{
    size_t i = 0u, j = 0u, cap = 16u, len = 0u;
    number_t *lvals = calloc(cap, sizeof(*lvals));
    number_t *rvals = calloc(cap, sizeof(*rvals));
    datetime_t **index = calloc(cap, sizeof(*index));
    bool *lmiss = calloc(cap, sizeof(*lmiss));
    bool *rmiss = calloc(cap, sizeof(*rmiss));
    timeseries_t *L = NULL, *R = NULL;

    if (!left || !right || !left_out || !right_out ||
        !left->has_index || !right->has_index ||
        !lvals || !rvals || !index || !lmiss || !rmiss)
        goto fail;
    while (i < left->length || j < right->length) {
        int cmp;
        bool take_left_only = false, take_right_only = false, take_both = false;
        const datetime_t *left_dt = i < left->length ? left->index[i] : NULL;
        const datetime_t *right_dt = j < right->length ? right->index[j] : NULL;

        if (i < left->length && !left_dt) {
            ++i;
            continue;
        }
        if (j < right->length && !right_dt) {
            ++j;
            continue;
        }

        if (i < left->length && j < right->length)
            cmp = datetime_compare(left_dt, right_dt);
        else if (i < left->length)
            cmp = -1;
        else
            cmp = 1;

        if (cmp == 0)
            take_both = true;
        else if (cmp < 0)
            take_left_only = (join_type == TS_JOIN_LEFT || join_type == TS_JOIN_OUTER);
        else
            take_right_only = (join_type == TS_JOIN_RIGHT || join_type == TS_JOIN_OUTER);

        if (take_both || take_left_only || take_right_only) {
            if (len == cap) {
                size_t new_cap = cap * 2u;
                number_t *nl = realloc(lvals, new_cap * sizeof(*nl));
                number_t *nr = realloc(rvals, new_cap * sizeof(*nr));
                datetime_t **ni = realloc(index, new_cap * sizeof(*ni));
                bool *nlm = realloc(lmiss, new_cap * sizeof(*nlm));
                bool *nrm = realloc(rmiss, new_cap * sizeof(*nrm));

                if (!nl || !nr || !ni || !nlm || !nrm) {
                    free(nl); free(nr); free(ni); free(nlm); free(nrm);
                    goto fail;
                }
                lvals = nl; rvals = nr; index = ni; lmiss = nlm; rmiss = nrm;
                cap = new_cap;
            }
            if (take_both) {
                lvals[len] = num_clone(left->values[i]);
                rvals[len] = num_clone(right->values[j]);
                lmiss[len] = left->missing[i];
                rmiss[len] = right->missing[j];
                index[len] = ts_datetime_clone(left_dt);
                ++i; ++j;
            } else if (take_left_only) {
                lvals[len] = num_clone(left->values[i]);
                rvals[len] = num_clone(NUM_NAN);
                lmiss[len] = left->missing[i];
                rmiss[len] = true;
                index[len] = ts_datetime_clone(left_dt);
                ++i;
            } else {
                lvals[len] = num_clone(NUM_NAN);
                rvals[len] = num_clone(right->values[j]);
                lmiss[len] = true;
                rmiss[len] = right->missing[j];
                index[len] = ts_datetime_clone(right_dt);
                ++j;
            }
            if (!index[len])
                goto fail;
            ++len;
        } else {
            if (cmp < 0) ++i;
            else if (cmp > 0) ++j;
        }
    }

    L = ts_alloc_empty(len);
    R = ts_alloc_empty(len);
    if (!L || !R)
        goto fail;
    free(L->values); L->values = lvals; lvals = NULL;
    free(R->values); R->values = rvals; rvals = NULL;
    free(L->missing); L->missing = lmiss; lmiss = NULL;
    free(R->missing); R->missing = rmiss; rmiss = NULL;
    L->index = index; R->index = calloc(len ? len : 1u, sizeof(*R->index));
    if (!R->index)
        goto fail;
    for (i = 0u; i < len; ++i)
        R->index[i] = ts_datetime_clone(index[i]);
    L->has_index = R->has_index = true;
    L->frequency = left->frequency; R->frequency = right->frequency;
    L->year_type = left->year_type; R->year_type = right->year_type;
    *left_out = L;
    *right_out = R;
    free(lvals); free(rvals); free(lmiss); free(rmiss);
    return 0;

fail:
    if (lvals) {
        for (i = 0u; i < len; ++i) num_destroy(&lvals[i]);
        free(lvals);
    }
    if (rvals) {
        for (i = 0u; i < len; ++i) num_destroy(&rvals[i]);
        free(rvals);
    }
    if (index) {
        for (i = 0u; i < len; ++i) datetime_dealloc(index[i]);
        free(index);
    }
    free(lmiss);
    free(rmiss);
    ts_free(L);
    ts_free(R);
    return -1;
}

static timeseries_t *ts_unary_map(const timeseries_t *series, number_t (*fn)(const number_t))
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out || !fn)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        number_t mapped;

        if (out->missing[i])
            continue;
        mapped = fn(out->values[i]);
        num_destroy(&out->values[i]);
        out->values[i] = mapped;
    }
    return out;
}

timeseries_t *ts_lag(const timeseries_t *series, size_t lag)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out)
        return NULL;
    for (i = out->length; i-- > 0u;) {
        num_destroy(&out->values[i]);
        if (i >= lag) {
            out->values[i] = num_clone(series->values[i - lag]);
            out->missing[i] = series->missing[i - lag];
        } else {
            out->values[i] = num_clone(NUM_NAN);
            out->missing[i] = true;
        }
    }
    return out;
}

timeseries_t *ts_lead(const timeseries_t *series, size_t lead)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        num_destroy(&out->values[i]);
        if (i + lead < series->length) {
            out->values[i] = num_clone(series->values[i + lead]);
            out->missing[i] = series->missing[i + lead];
        } else {
            out->values[i] = num_clone(NUM_NAN);
            out->missing[i] = true;
        }
    }
    return out;
}

timeseries_t *ts_diff(const timeseries_t *series, size_t differences)
{
    timeseries_t *work = ts_clone(series);
    size_t d;

    if (!work)
        return NULL;
    for (d = 0u; d < differences; ++d) {
        size_t i;

        for (i = work->length; i-- > 0u;) {
            number_t next;

            if (i == 0u || work->missing[i] || work->missing[i - 1u]) {
                num_destroy(&work->values[i]);
                work->values[i] = num_clone(NUM_NAN);
                work->missing[i] = true;
                continue;
            }
            next = num_sub(work->values[i], work->values[i - 1u]);
            num_destroy(&work->values[i]);
            work->values[i] = next;
            work->missing[i] = false;
        }
    }
    return work;
}

timeseries_t *ts_seasonal_diff(const timeseries_t *series,
                               size_t differences, size_t season_period)
{
    timeseries_t *work = ts_clone(series);
    size_t d;

    if (!work)
        return NULL;
    for (d = 0u; d < differences; ++d) {
        size_t i;

        for (i = work->length; i-- > 0u;) {
            number_t next;

            if (i < season_period || work->missing[i] || work->missing[i - season_period]) {
                num_destroy(&work->values[i]);
                work->values[i] = num_clone(NUM_NAN);
                work->missing[i] = true;
                continue;
            }
            next = num_sub(work->values[i], work->values[i - season_period]);
            num_destroy(&work->values[i]);
            work->values[i] = next;
            work->missing[i] = false;
        }
    }
    return work;
}

timeseries_t *ts_cumsum(const timeseries_t *series)
{
    timeseries_t *out = ts_clone(series);
    size_t i;
    number_t running = num_clone(NUM_ZERO);

    if (!out)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        number_t next;

        if (out->missing[i])
            continue;
        next = num_add(running, out->values[i]);
        num_destroy(&running);
        running = num_clone(next);
        num_destroy(&out->values[i]);
        out->values[i] = next;
    }
    num_destroy(&running);
    return out;
}

timeseries_t *ts_log(const timeseries_t *series) { return ts_unary_map(series, num_log); }
timeseries_t *ts_exp(const timeseries_t *series) { return ts_unary_map(series, num_exp); }

timeseries_t *ts_box_cox(const timeseries_t *series, const number_t *lambda)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out || !lambda)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        if (out->missing[i])
            continue;
        if (num_is_zero(*lambda)) {
            number_t mapped = num_log(out->values[i]);
            num_destroy(&out->values[i]);
            out->values[i] = mapped;
        } else {
            number_t p = num_pow(out->values[i], *lambda);
            number_t nume = num_sub(p, NUM_ONE);
            number_t mapped = num_div(nume, *lambda);

            num_destroy(&out->values[i]);
            out->values[i] = mapped;
            num_destroy(&p);
            num_destroy(&nume);
        }
    }
    return out;
}

static timeseries_t *ts_roll_generic(const timeseries_t *series, size_t window, int mode)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out || window == 0u)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        size_t start = i + 1u >= window ? i + 1u - window : 0u;
        size_t j;
        double sum = 0.0, sumsq = 0.0, minv = 0.0, maxv = 0.0;
        size_t count = 0u;

        for (j = start; j <= i; ++j) {
            double v;
            if (series->missing[j])
                continue;
            v = num_to_double(series->values[j]);
            if (count == 0u) {
                minv = maxv = v;
            } else {
                if (v < minv) minv = v;
                if (v > maxv) maxv = v;
            }
            sum += v;
            sumsq += v * v;
            count++;
        }
        num_destroy(&out->values[i]);
        if (count == 0u) {
            out->values[i] = num_clone(NUM_NAN);
            out->missing[i] = true;
        } else {
            double result;
            if (mode == 0) result = sum / (double)count;
            else if (mode == 1) result = sum;
            else if (mode == 2) result = (sumsq / (double)count) - (sum / (double)count) * (sum / (double)count);
            else if (mode == 3) {
                double var = (sumsq / (double)count) - (sum / (double)count) * (sum / (double)count);
                result = sqrt(var < 0.0 ? 0.0 : var);
            } else if (mode == 4) result = minv;
            else result = maxv;
            out->values[i] = num_create_from_double(result);
            out->missing[i] = false;
        }
    }
    return out;
}

timeseries_t *ts_roll_mean(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, 0); }
timeseries_t *ts_roll_sum(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, 1); }
timeseries_t *ts_roll_var(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, 2); }
timeseries_t *ts_roll_std(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, 3); }

matrix_t *ts_to_column_matrix(const timeseries_t *series)
{
    if (!series)
        return NULL;
    return mat_create(series->length, 1u, series->values);
}

matrix_t *ts_to_row_matrix(const timeseries_t *series)
{
    if (!series)
        return NULL;
    return mat_create(1u, series->length, series->values);
}

matrix_t *ts_design_matrix_lags(const timeseries_t *series, size_t max_lag,
                                bool include_intercept)
{
    size_t rows, cols, r, c;
    double *vals;

    if (!series || max_lag >= series->length)
        return NULL;
    rows = series->length - max_lag;
    cols = max_lag + (include_intercept ? 1u : 0u);
    vals = calloc((rows && cols) ? rows * cols : 1u, sizeof(*vals));
    if (!vals)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        size_t base = r + max_lag;
        size_t col = 0u;
        if (include_intercept)
            vals[r * cols + col++] = 1.0;
        for (c = 1u; c <= max_lag; ++c)
            vals[r * cols + col++] = num_to_double(series->values[base - c]);
    }
    {
        matrix_t *out = ts_make_matrix_from_doubles(vals, rows, cols);
        free(vals);
        return out;
    }
}

matrix_t *ts_design_matrix_trend(const timeseries_t *series,
                                 bool include_intercept,
                                 bool include_linear,
                                 bool include_quadratic)
{
    size_t rows, cols = 0u, r;
    double *vals;
    size_t col;

    if (!series)
        return NULL;
    rows = series->length;
    cols += include_intercept ? 1u : 0u;
    cols += include_linear ? 1u : 0u;
    cols += include_quadratic ? 1u : 0u;
    vals = calloc((rows && cols) ? rows * cols : 1u, sizeof(*vals));
    if (!vals)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        col = 0u;
        if (include_intercept) vals[r * cols + col++] = 1.0;
        if (include_linear) vals[r * cols + col++] = (double)r;
        if (include_quadratic) vals[r * cols + col++] = (double)r * (double)r;
    }
    {
        matrix_t *out = ts_make_matrix_from_doubles(vals, rows, cols);
        free(vals);
        return out;
    }
}

matrix_t *ts_design_matrix_seasonal_dummies(const timeseries_t *series,
                                            size_t season_period,
                                            bool drop_first)
{
    size_t rows, cols, r, c;
    double *vals;

    if (!series || season_period == 0u)
        return NULL;
    rows = series->length;
    cols = season_period - (drop_first ? 1u : 0u);
    vals = calloc((rows && cols) ? rows * cols : 1u, sizeof(*vals));
    if (!vals)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        size_t bucket = r % season_period;
        for (c = 0u; c < cols; ++c) {
            size_t season = drop_first ? c + 1u : c;
            vals[r * cols + c] = (bucket == season) ? 1.0 : 0.0;
        }
    }
    {
        matrix_t *out = ts_make_matrix_from_doubles(vals, rows, cols);
        free(vals);
        return out;
    }
}

matrix_t *ts_bind_columns(timeseries_t *const *series,
                          size_t count,
                          ts_join_type_t join_type)
{
    size_t i, j, rows;
    timeseries_t *base = NULL;
    timeseries_t **aligned = NULL;
    double *vals = NULL;
    matrix_t *out = NULL;

    if (!series || count == 0u)
        return NULL;
    aligned = calloc(count, sizeof(*aligned));
    if (!aligned)
        return NULL;
    base = ts_clone(series[0]);
    if (!base)
        goto cleanup;
    aligned[0] = base;
    for (i = 1u; i < count; ++i) {
        timeseries_t *new_base = NULL;
        timeseries_t *other = NULL;

        if (ts_align_pair(base, series[i], join_type, &new_base, &other) != 0)
            goto cleanup;
        ts_free(base);
        for (j = 1u; j < i; ++j) {
            timeseries_t *tmp_left = NULL;
            timeseries_t *tmp_right = NULL;

            if (ts_align_pair(new_base, aligned[j], TS_JOIN_INNER, &tmp_left, &tmp_right) != 0) {
                ts_free(new_base);
                ts_free(other);
                goto cleanup;
            }
            ts_free(new_base);
            ts_free(aligned[j]);
            new_base = tmp_left;
            aligned[j] = tmp_right;
        }
        base = new_base;
        aligned[0] = base;
        aligned[i] = other;
    }
    rows = base->length;
    vals = calloc((rows && count) ? rows * count : 1u, sizeof(*vals));
    if (!vals)
        goto cleanup;
    for (i = 0u; i < rows; ++i)
        for (j = 0u; j < count; ++j)
            vals[i * count + j] = aligned[j]->missing[i] ? NAN : num_to_double(aligned[j]->values[i]);
    out = ts_make_matrix_from_doubles(vals, rows, count);

cleanup:
    if (aligned) {
        for (i = 0u; i < count; ++i)
            ts_free(aligned[i]);
    }
    free(aligned);
    free(vals);
    return out;
}

static timeseries_t *ts_aggregate_generic(const timeseries_t *series,
                                          ts_frequency_t target_frequency,
                                          ts_year_type_t year_type,
                                          int mode)
{
    size_t i = 0u, cap = 16u, len = 0u;
    number_t *values = calloc(cap, sizeof(*values));
    datetime_t **index = calloc(cap, sizeof(*index));
    bool *missing = calloc(cap, sizeof(*missing));
    timeseries_t *out;

    if (!series || !series->has_index || !values || !index || !missing)
        goto fail;
    while (i < series->length) {
        size_t j = i;
        double agg = 0.0, minv = 0.0, maxv = 0.0;
        size_t count = 0u;

        while (j < series->length &&
               ts_datetime_same_bucket(series->index[i], series->index[j],
                                       target_frequency, year_type)) {
            if (!series->missing[j]) {
                double v = num_to_double(series->values[j]);
                if (count == 0u) minv = maxv = v;
                else {
                    if (v < minv) minv = v;
                    if (v > maxv) maxv = v;
                }
                agg += v;
                count++;
            }
            ++j;
        }
        if (len == cap) {
            size_t new_cap = cap * 2u;
            number_t *nv = realloc(values, new_cap * sizeof(*nv));
            datetime_t **ni = realloc(index, new_cap * sizeof(*ni));
            bool *nm = realloc(missing, new_cap * sizeof(*nm));

            if (!nv || !ni || !nm) {
                free(nv); free(ni); free(nm);
                goto fail;
            }
            values = nv; index = ni; missing = nm; cap = new_cap;
        }
        index[len] = ts_bucket_label(series->index[i], target_frequency, year_type);
        if (count == 0u) {
            values[len] = num_clone(NUM_NAN);
            missing[len] = true;
        } else {
            double result = mode == 0 ? agg : mode == 1 ? agg / (double)count : mode == 2 ? minv : maxv;
            values[len] = num_create_from_double(result);
            missing[len] = false;
        }
        ++len;
        i = j;
    }
    out = ts_alloc_empty(len);
    if (!out)
        goto fail;
    free(out->values); out->values = values; values = NULL;
    free(out->missing); out->missing = missing; missing = NULL;
    out->index = index; index = NULL;
    out->has_index = true;
    out->frequency = target_frequency;
    out->year_type = year_type;
    out->is_regular = true;
    out->season_period = (size_t)ts_infer_season_period(target_frequency);
    return out;

fail:
    if (values) {
        for (i = 0u; i < cap; ++i) num_destroy(&values[i]);
        free(values);
    }
    if (index) {
        for (i = 0u; i < cap; ++i) datetime_dealloc(index[i]);
        free(index);
    }
    free(missing);
    return NULL;
}

timeseries_t *ts_as_frequency(const timeseries_t *series,
                              ts_frequency_t target_frequency,
                              ts_missing_policy_t missing_policy)
{
    (void)missing_policy;
    if (!series)
        return NULL;
    if (series->frequency == target_frequency)
        return ts_clone(series);
    return ts_aggregate_generic(series, target_frequency, series->year_type, 1);
}

timeseries_t *ts_aggregate_sum(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, 0); }
timeseries_t *ts_aggregate_mean(const timeseries_t *series,
                                ts_frequency_t target_frequency,
                                ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, 1); }
timeseries_t *ts_aggregate_min(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, 2); }
timeseries_t *ts_aggregate_max(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, 3); }

int ts_acf(const timeseries_t *series, size_t max_lag, matrix_t **out)
{
    double *vals = NULL, mean = 0.0, denom = 0.0;
    size_t n = 0u, i, lag;
    double *acf = NULL;

    if (!series || !out || ts_series_to_double_array(series, &vals, &n, 0u) != 0 || n == 0u)
        return -1;
    if (max_lag >= n)
        max_lag = n - 1u;
    for (i = 0u; i < n; ++i)
        mean += vals[i];
    mean /= (double)n;
    for (i = 0u; i < n; ++i) {
        double d = vals[i] - mean;
        denom += d * d;
    }
    acf = calloc(max_lag + 1u, sizeof(*acf));
    if (!acf) {
        free(vals);
        return -1;
    }
    for (lag = 0u; lag <= max_lag; ++lag) {
        double num = 0.0;

        for (i = lag; i < n; ++i)
            num += (vals[i] - mean) * (vals[i - lag] - mean);
        acf[lag] = denom == 0.0 ? 0.0 : num / denom;
    }
    *out = ts_make_column_matrix_from_doubles(acf, max_lag + 1u);
    free(vals);
    free(acf);
    return *out ? 0 : -1;
}

int ts_pacf(const timeseries_t *series, size_t max_lag, matrix_t **out)
{
    matrix_t *acf_mat = NULL;
    double *acf = NULL;
    double *phi = NULL;
    double *pacf = NULL;
    size_t i, k;

    if (!series || !out || ts_acf(series, max_lag, &acf_mat) != 0)
        return -1;
    acf = calloc(max_lag + 1u, sizeof(*acf));
    phi = calloc((max_lag + 1u) * (max_lag + 1u), sizeof(*phi));
    pacf = calloc(max_lag + 1u, sizeof(*pacf));
    if (!acf || !phi || !pacf) {
        mat_free(acf_mat);
        free(acf); free(phi); free(pacf);
        return -1;
    }
    for (i = 0u; i <= max_lag; ++i) {
        number_t v = mat_get_num(acf_mat, i, 0);
        acf[i] = num_to_double(v);
        num_destroy(&v);
    }
    pacf[0] = 1.0;
    for (k = 1u; k <= max_lag; ++k) {
        double num = acf[k];
        double den = 1.0;

        for (i = 1u; i < k; ++i) {
            num -= phi[(k - 1u) * (max_lag + 1u) + i] * acf[k - i];
            den -= phi[(k - 1u) * (max_lag + 1u) + i] * acf[i];
        }
        phi[k * (max_lag + 1u) + k] = den == 0.0 ? 0.0 : num / den;
        for (i = 1u; i < k; ++i) {
            phi[k * (max_lag + 1u) + i] =
                phi[(k - 1u) * (max_lag + 1u) + i] -
                phi[k * (max_lag + 1u) + k] *
                phi[(k - 1u) * (max_lag + 1u) + (k - i)];
        }
        pacf[k] = phi[k * (max_lag + 1u) + k];
    }
    *out = ts_make_column_matrix_from_doubles(pacf, max_lag + 1u);
    mat_free(acf_mat);
    free(acf); free(phi); free(pacf);
    return *out ? 0 : -1;
}

int ts_ccf(const timeseries_t *x, const timeseries_t *y, size_t max_lag, matrix_t **out)
{
    timeseries_t *xa = NULL, *ya = NULL;
    size_t n, lag;
    double *vals = NULL;

    if (!x || !y || !out || ts_align_pair(x, y, TS_JOIN_INNER, &xa, &ya) != 0)
        return -1;
    n = xa->length;
    if (max_lag >= n)
        max_lag = n - 1u;
    vals = calloc(max_lag + 1u, sizeof(*vals));
    if (!vals) {
        ts_free(xa); ts_free(ya);
        return -1;
    }
    for (lag = 0u; lag <= max_lag; ++lag) {
        double sx = 0.0, sy = 0.0, sxy = 0.0;
        size_t count = 0u, i;

        for (i = lag; i < n; ++i) {
            if (xa->missing[i] || ya->missing[i - lag])
                continue;
            sx += num_to_double(xa->values[i]);
            sy += num_to_double(ya->values[i - lag]);
            sxy += num_to_double(xa->values[i]) * num_to_double(ya->values[i - lag]);
            count++;
        }
        vals[lag] = count == 0u ? NAN : (sxy / (double)count) - (sx / (double)count) * (sy / (double)count);
    }
    *out = ts_make_column_matrix_from_doubles(vals, max_lag + 1u);
    free(vals);
    ts_free(xa); ts_free(ya);
    return *out ? 0 : -1;
}

int ts_ljung_box(const timeseries_t *series, size_t max_lag,
                 number_t *statistic, number_t *p_value)
{
    matrix_t *acf = NULL;
    double q = 0.0;
    size_t n = 0u, h;
    double *vals = NULL;

    if (!series || !statistic || !p_value || ts_series_to_double_array(series, &vals, &n, 0u) != 0)
        return -1;
    free(vals);
    if (ts_acf(series, max_lag, &acf) != 0)
        return -1;
    for (h = 1u; h <= max_lag; ++h) {
        number_t rh = mat_get_num(acf, h, 0);
        double r = num_to_double(rh);

        q += (r * r) / (double)(n - h);
        num_destroy(&rh);
    }
    q *= (double)n * (double)(n + 2u);
    *statistic = num_create_from_double(q);
    *p_value = num_create_from_double(exp(-0.5 * q));
    mat_free(acf);
    return 0;
}

int ts_adf(const timeseries_t *series,
           number_t *statistic, number_t *p_value)
{
    timeseries_t *d = NULL;
    size_t n, i;
    double sxy = 0.0, sxx = 0.0;

    if (!series || !statistic || !p_value || series->length < 2u)
        return -1;
    d = ts_diff(series, 1u);
    if (!d)
        return -1;
    n = 0u;
    for (i = 1u; i < series->length; ++i) {
        if (d->missing[i] || series->missing[i - 1u])
            continue;
        sxy += num_to_double(series->values[i - 1u]) * num_to_double(d->values[i]);
        sxx += num_to_double(series->values[i - 1u]) * num_to_double(series->values[i - 1u]);
        n++;
    }
    *statistic = num_create_from_double(sxx == 0.0 ? 0.0 : sxy / sxx);
    *p_value = num_create_from_double(exp(-fabs(num_to_double(*statistic))));
    ts_free(d);
    return 0;
}

int ts_kpss(const timeseries_t *series,
            number_t *statistic, number_t *p_value)
{
    double *vals = NULL, mean = 0.0, cum = 0.0, sumsq = 0.0, eta = 0.0;
    size_t n = 0u, i;

    if (!series || !statistic || !p_value || ts_series_to_double_array(series, &vals, &n, 0u) != 0 || n == 0u)
        return -1;
    for (i = 0u; i < n; ++i)
        mean += vals[i];
    mean /= (double)n;
    for (i = 0u; i < n; ++i) {
        double e = vals[i] - mean;
        cum += e;
        eta += cum * cum;
        sumsq += e * e;
    }
    *statistic = num_create_from_double((n * n) == 0u || sumsq == 0.0 ? 0.0 : eta / ((double)n * (double)n * sumsq));
    *p_value = num_create_from_double(exp(-10.0 * num_to_double(*statistic)));
    free(vals);
    return 0;
}

static matrix_t *ts_augmented_xreg(const matrix_t *xreg, size_t rows, bool include_intercept,
                                   bool include_trend, size_t trend_start)
{
    size_t base_cols = xreg ? mat_get_col_count(xreg) : 0u;
    size_t cols = base_cols + (include_intercept ? 1u : 0u) + (include_trend ? 1u : 0u);
    matrix_t *X = mat_new(rows, cols);
    size_t r, c, col;

    if (!X)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        col = 0u;
        if (include_intercept) {
            number_t one = NUM_ONE;
            mat_set(X, r, col++, &one);
        }
        if (include_trend) {
            number_t t = num_create_from_double((double)(trend_start + r));
            mat_set(X, r, col++, &t);
            num_destroy(&t);
        }
        for (c = 0u; c < base_cols; ++c) {
            number_t v = mat_get_num(xreg, r, c);
            mat_set(X, r, col++, &v);
            num_destroy(&v);
        }
    }
    return X;
}

static int ts_regression_fit_internal(const timeseries_t *y,
                                      const matrix_t *xreg,
                                      bool include_intercept,
                                      bool include_trend,
                                      const ts_fit_options_t *options,
                                      ts_regression_result_t *out)
{
    matrix_t *Y = NULL, *X = NULL, *beta = NULL, *fit = NULL, *resid_col = NULL;
    matrix_t *Xt = NULL, *XtX = NULL, *XtX_inv = NULL;
    timeseries_t *fitted = NULL, *residuals = NULL;
    size_t n, k, i;
    double sse = 0.0, mean = 0.0, sst = 0.0;
    (void)options;

    if (!y || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    Y = ts_to_column_matrix(y);
    if (!Y)
        return -1;
    n = mat_get_row_count(Y);
    X = ts_augmented_xreg(xreg, n, include_intercept, include_trend, 0u);
    if (!X)
        goto fail;
    beta = mat_least_squares(X, Y);
    fit = beta ? mat_mul(X, beta) : NULL;
    resid_col = fit ? mat_sub(Y, fit) : NULL;
    Xt = mat_transpose(X);
    XtX = Xt ? mat_mul(Xt, X) : NULL;
    XtX_inv = XtX ? mat_inverse(XtX) : NULL;
    if (!beta || !fit || !resid_col)
        goto fail;
    fitted = ts_new_indexed(NULL, NULL, 0u, TS_FREQ_UNKNOWN, TS_YEAR_CALENDAR);
    residuals = ts_new_indexed(NULL, NULL, 0u, TS_FREQ_UNKNOWN, TS_YEAR_CALENDAR);
    ts_free(fitted);
    ts_free(residuals);
    fitted = ts_clone(y);
    residuals = ts_clone(y);
    if (!fitted || !residuals)
        goto fail;
    for (i = 0u; i < n; ++i) {
        number_t fv = mat_get_num(fit, i, 0);
        number_t rv = mat_get_num(resid_col, i, 0);
        double yd = num_to_double(y->values[i]);
        double rd = num_to_double(rv);

        num_destroy(&fitted->values[i]);
        num_destroy(&residuals->values[i]);
        fitted->values[i] = fv;
        residuals->values[i] = rv;
        sse += rd * rd;
        mean += yd;
    }
    mean /= (double)n;
    for (i = 0u; i < n; ++i) {
        double yd = num_to_double(y->values[i]);
        double diff = yd - mean;
        sst += diff * diff;
    }
    out->coefficients = beta; beta = NULL;
    out->fitted = fitted; fitted = NULL;
    out->residuals = residuals; residuals = NULL;
    out->sse = num_create_from_double(sse);
    out->mse = num_create_from_double(sse / (double)n);
    out->rmse = num_create_from_double(sqrt(sse / (double)n));
    out->r2 = num_create_from_double(sst == 0.0 ? 1.0 : 1.0 - sse / sst);
    k = out->coefficients ? mat_get_row_count(out->coefficients) : 1u;
    out->adj_r2 = num_create_from_double((n > k + 1u && sst != 0.0)
        ? 1.0 - ((sse / (double)(n - k)) / (sst / (double)(n - 1u))) : num_to_double(out->r2));
    out->summary.sigma2 = num_create_from_double(n > k ? sse / (double)(n - k) : sse);
    out->summary.loglik = num_create_from_double(-0.5 * (double)n * (log(2.0 * M_PI) + 1.0 + log(sse / (double)n + 1e-12)));
    out->summary.aic = num_create_from_double(2.0 * (double)k - 2.0 * num_to_double(out->summary.loglik));
    out->summary.aicc = num_create_from_double(num_to_double(out->summary.aic) +
                                               (2.0 * (double)k * ((double)k + 1.0)) /
                                               ((double)n - (double)k - 1.0 + 1e-12));
    out->summary.bic = num_create_from_double(log((double)n) * (double)k - 2.0 * num_to_double(out->summary.loglik));
    if (XtX_inv) {
        matrix_t *stderr = mat_new(k, 1u);
        matrix_t *t_stat = mat_new(k, 1u);
        matrix_t *p_val = mat_new(k, 1u);
        number_t sigma2 = out->summary.sigma2;

        if (!stderr || !t_stat || !p_val) {
            mat_free(stderr); mat_free(t_stat); mat_free(p_val);
        } else {
            for (i = 0u; i < k; ++i) {
                number_t v = mat_get_num(XtX_inv, i, i);
                number_t vv = num_mul(v, sigma2);
                number_t se = num_sqrt(vv);
                number_t b = mat_get_num(out->coefficients, i, 0);
                number_t t = num_div(b, se);
                number_t abs_t = num_abs(t);
                number_t cdf = num_normal_cdf(abs_t);
                number_t two = NUM_TWO;
                number_t p = num_mul(two, num_sub(NUM_ONE, cdf));

                mat_set(stderr, i, 0u, &se);
                mat_set(t_stat, i, 0u, &t);
                mat_set(p_val, i, 0u, &p);
                num_destroy(&v); num_destroy(&vv); num_destroy(&se); num_destroy(&b);
                num_destroy(&t); num_destroy(&abs_t); num_destroy(&cdf); num_destroy(&p);
            }
            out->stderr = stderr;
            out->t_stat = t_stat;
            out->p_value = p_val;
            out->vcov = XtX_inv;
            XtX_inv = NULL;
        }
    }
    mat_free(Y); mat_free(X); mat_free(fit); mat_free(resid_col); mat_free(Xt); mat_free(XtX); mat_free(XtX_inv);
    return 0;

fail:
    mat_free(Y); mat_free(X); mat_free(beta); mat_free(fit); mat_free(resid_col); mat_free(Xt); mat_free(XtX); mat_free(XtX_inv);
    ts_free(fitted); ts_free(residuals);
    return -1;
}

int ts_regression_fit(const timeseries_t *y,
                      const matrix_t *xreg,
                      const ts_fit_options_t *options,
                      ts_regression_result_t *out)
{
    return ts_regression_fit_internal(y, xreg, true, false, options, out);
}

int ts_regression_forecast(const ts_regression_result_t *model,
                           const matrix_t *future_xreg,
                           const timeseries_t *history,
                           ts_frequency_t frequency,
                           ts_year_type_t year_type,
                           number_t level,
                           ts_forecast_t *out)
{
    size_t rows, cols, i;
    number_t z = num_create_from_double(1.96);
    timeseries_t *mean = NULL, *stderr = NULL, *lower = NULL, *upper = NULL;
    datetime_t *start = NULL;
    size_t beta_rows;

    if (!model || !model->coefficients || !future_xreg || !history || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    rows = mat_get_row_count(future_xreg);
    cols = mat_get_col_count(future_xreg);
    beta_rows = mat_get_row_count(model->coefficients);
    if (beta_rows != cols + 1u)
        goto fail;
    start = ts_end_datetime(history);
    if (!start)
        goto fail;
    switch (frequency) {
    case TS_FREQ_DAILY: datetime_add_days(start, 1L); break;
    case TS_FREQ_MONTHLY: datetime_add_months(start, 1); break;
    case TS_FREQ_QUARTERLY: datetime_add_months(start, 3); break;
    case TS_FREQ_YEARLY: datetime_add_years(start, 1); break;
    default: break;
    }
    mean = ts_make_empty_regular_series(rows, start, frequency, year_type);
    stderr = ts_make_empty_regular_series(rows, start, frequency, year_type);
    lower = ts_make_empty_regular_series(rows, start, frequency, year_type);
    upper = ts_make_empty_regular_series(rows, start, frequency, year_type);
    if (!mean || !stderr || !lower || !upper)
        goto fail;
    for (i = 0u; i < rows; ++i) {
        number_t mu = mat_get_num(model->coefficients, 0u, 0u);
        number_t se = num_clone(model->rmse);
        number_t width = num_mul(z, se);
        size_t c;

        for (c = 0u; c < cols; ++c) {
            number_t beta = mat_get_num(model->coefficients, c + 1u, 0u);
            number_t xv = mat_get_num(future_xreg, i, c);
            number_t term = num_mul(beta, xv);
            number_t next = num_add(mu, term);

            num_destroy(&mu);
            mu = next;
            num_destroy(&beta);
            num_destroy(&xv);
            num_destroy(&term);
        }
        mean->values[i] = num_clone(mu);
        stderr->values[i] = se;
        lower->values[i] = num_sub(mu, width);
        upper->values[i] = num_add(mu, width);
        num_destroy(&mu);
        num_destroy(&width);
    }
    out->mean = mean;
    out->stderr = stderr;
    out->lower = lower;
    out->upper = upper;
    out->level = num_clone(level);
    datetime_dealloc(start);
    num_destroy(&z);
    return 0;

fail:
    datetime_dealloc(start);
    ts_free(mean); ts_free(stderr); ts_free(lower); ts_free(upper);
    num_destroy(&z);
    return -1;
}

void ts_regression_result_clear(ts_regression_result_t *out)
{
    if (!out)
        return;
    mat_free(out->coefficients);
    mat_free(out->stderr);
    mat_free(out->t_stat);
    mat_free(out->p_value);
    mat_free(out->vcov);
    ts_free(out->fitted);
    ts_free(out->residuals);
    num_destroy(&out->r2);
    num_destroy(&out->adj_r2);
    num_destroy(&out->sse);
    num_destroy(&out->mse);
    num_destroy(&out->rmse);
    ts_result_clear_numbers(&out->summary);
    memset(out, 0, sizeof(*out));
}

static matrix_t *ts_submatrix_rows(const matrix_t *A, size_t start_row, size_t row_count)
{
    size_t cols, r, c;
    matrix_t *out;

    if (!A || start_row + row_count > mat_get_row_count(A))
        return NULL;
    cols = mat_get_col_count(A);
    out = mat_new(row_count, cols);
    if (!out)
        return NULL;
    for (r = 0u; r < row_count; ++r) {
        for (c = 0u; c < cols; ++c) {
            number_t v = mat_get_num(A, start_row + r, c);
            mat_set(out, r, c, &v);
            num_destroy(&v);
        }
    }
    return out;
}

static timeseries_t *ts_build_arima_response(const timeseries_t *y, const ts_arima_spec_t *spec)
{
    timeseries_t *work = ts_clone(y);
    timeseries_t *tmp;

    if (!work || !spec)
        return NULL;
    if (spec->d > 0u) {
        tmp = ts_diff(work, spec->d);
        ts_free(work);
        work = tmp;
    }
    if (work && spec->D > 0u && spec->season_period > 0u) {
        tmp = ts_seasonal_diff(work, spec->D, spec->season_period);
        ts_free(work);
        work = tmp;
    }
    return work;
}

static void ts_double_diff_in_place(double *values, size_t length)
{
    size_t i;

    if (!values || length == 0u)
        return;
    for (i = length; i-- > 1u;)
        values[i] = values[i] - values[i - 1u];
}

static void ts_double_seasonal_diff_in_place(double *values, size_t length, size_t season_period)
{
    size_t i;

    if (!values || length == 0u || season_period == 0u || season_period >= length)
        return;
    for (i = length; i-- > season_period;)
        values[i] = values[i] - values[i - season_period];
}

static bool ts_arima_spec_differences_xreg(const ts_arima_spec_t *spec)
{
    return spec && (spec->d > 0u || (spec->D > 0u && spec->season_period > 0u));
}

static void ts_apply_arima_differences_to_array(double *values, size_t length,
                                                const ts_arima_spec_t *spec)
{
    size_t pass, i;

    if (!values || !spec)
        return;
    for (pass = 0u; pass < spec->d; ++pass) {
        if (length == 0u)
            break;
        for (i = length; i-- > 1u;)
            values[i] = values[i] - values[i - 1u];
        values[0u] = NAN;
    }
    if (spec->season_period == 0u)
        return;
    for (pass = 0u; pass < spec->D; ++pass) {
        if (length <= spec->season_period)
            break;
        for (i = length; i-- > spec->season_period;)
            values[i] = values[i] - values[i - spec->season_period];
        for (i = 0u; i < spec->season_period && i < length; ++i)
            values[i] = NAN;
    }
}

static matrix_t *ts_arima_transform_xreg_history(const matrix_t *xreg,
                                                 const ts_arima_spec_t *spec)
{
    matrix_t *out = NULL;
    double *values = NULL;
    size_t rows, cols, r, c;

    if (!xreg)
        return NULL;
    if (!ts_arima_spec_differences_xreg(spec))
        return ts_matrix_clone_local(xreg);
    rows = mat_get_row_count(xreg);
    cols = mat_get_col_count(xreg);
    out = mat_new(rows, cols);
    values = calloc(rows ? rows : 1u, sizeof(*values));
    if (!out || !values)
        goto fail;
    for (c = 0u; c < cols; ++c) {
        for (r = 0u; r < rows; ++r) {
            number_t v = mat_get_num(xreg, r, c);
            values[r] = num_to_double(v);
            num_destroy(&v);
        }
        ts_apply_arima_differences_to_array(values, rows, spec);
        for (r = 0u; r < rows; ++r) {
            number_t v = num_create_from_double(values[r]);
            mat_set(out, r, c, &v);
            num_destroy(&v);
        }
    }
    free(values);
    return out;

fail:
    mat_free(out);
    free(values);
    return NULL;
}

static matrix_t *ts_arima_transform_xreg_future(const matrix_t *history_xreg,
                                                const matrix_t *future_xreg,
                                                const ts_arima_spec_t *spec)
{
    matrix_t *out = NULL;
    double *values = NULL;
    size_t history_rows, future_rows, rows, cols, r, c;

    if (!future_xreg)
        return NULL;
    if (!ts_arima_spec_differences_xreg(spec))
        return ts_matrix_clone_local(future_xreg);
    if (!history_xreg)
        return NULL;
    history_rows = mat_get_row_count(history_xreg);
    future_rows = mat_get_row_count(future_xreg);
    rows = history_rows + future_rows;
    cols = mat_get_col_count(future_xreg);
    if (mat_get_col_count(history_xreg) != cols)
        return NULL;
    out = mat_new(future_rows, cols);
    values = calloc(rows ? rows : 1u, sizeof(*values));
    if (!out || !values)
        goto fail;
    for (c = 0u; c < cols; ++c) {
        for (r = 0u; r < history_rows; ++r) {
            number_t v = mat_get_num(history_xreg, r, c);
            values[r] = num_to_double(v);
            num_destroy(&v);
        }
        for (r = 0u; r < future_rows; ++r) {
            number_t v = mat_get_num(future_xreg, r, c);
            values[history_rows + r] = num_to_double(v);
            num_destroy(&v);
        }
        ts_apply_arima_differences_to_array(values, rows, spec);
        for (r = 0u; r < future_rows; ++r) {
            number_t v = num_create_from_double(values[history_rows + r]);
            mat_set(out, r, c, &v);
            num_destroy(&v);
        }
    }
    free(values);
    return out;

fail:
    mat_free(out);
    free(values);
    return NULL;
}

static int ts_arima_reintegrate_forecast_path(const double *history,
                                              size_t history_length,
                                              const ts_arima_spec_t *spec,
                                              const double *transformed_forecast,
                                              size_t horizon,
                                              double *out_levels)
{
    double **ord_stages = NULL;
    double **season_stages = NULL;
    double *current = NULL;
    size_t ord_stage_count, season_stage_count;
    size_t i, inv;

    if (!history || !spec || !transformed_forecast || !out_levels || history_length == 0u)
        return -1;
    if (spec->d == 0u && spec->D == 0u) {
        for (i = 0u; i < horizon; ++i)
            out_levels[i] = transformed_forecast[i];
        return 0;
    }

    ord_stage_count = spec->d + 1u;
    season_stage_count = spec->D + 1u;
    ord_stages = calloc(ord_stage_count ? ord_stage_count : 1u, sizeof(*ord_stages));
    season_stages = calloc(season_stage_count ? season_stage_count : 1u, sizeof(*season_stages));
    current = calloc(horizon ? horizon : 1u, sizeof(*current));
    if (!ord_stages || !season_stages || !current)
        goto fail;

    for (inv = 0u; inv < ord_stage_count; ++inv) {
        ord_stages[inv] = calloc(history_length ? history_length : 1u, sizeof(**ord_stages));
        if (!ord_stages[inv])
            goto fail;
    }
    for (inv = 0u; inv < season_stage_count; ++inv) {
        season_stages[inv] = calloc(history_length ? history_length : 1u, sizeof(**season_stages));
        if (!season_stages[inv])
            goto fail;
    }

    memcpy(ord_stages[0u], history, history_length * sizeof(*history));
    for (inv = 1u; inv < ord_stage_count; ++inv) {
        memcpy(ord_stages[inv], ord_stages[inv - 1u], history_length * sizeof(*history));
        ts_double_diff_in_place(ord_stages[inv], history_length);
    }

    memcpy(season_stages[0u], ord_stages[spec->d], history_length * sizeof(*history));
    for (inv = 1u; inv < season_stage_count; ++inv) {
        memcpy(season_stages[inv], season_stages[inv - 1u], history_length * sizeof(*history));
        ts_double_seasonal_diff_in_place(season_stages[inv], history_length, spec->season_period);
    }

    memcpy(current, transformed_forecast, horizon * sizeof(*current));

    for (inv = spec->D; inv > 0u; --inv) {
        double *reconstructed = calloc(horizon ? horizon : 1u, sizeof(*reconstructed));
        const double *base = season_stages[inv - 1u];

        if (!reconstructed)
            goto fail;
        for (i = 0u; i < horizon; ++i) {
            double lag_value;

            if (i >= spec->season_period) {
                lag_value = reconstructed[i - spec->season_period];
            } else if (history_length >= spec->season_period - i) {
                lag_value = base[history_length - spec->season_period + i];
            } else {
                lag_value = base[history_length - 1u];
            }
            reconstructed[i] = current[i] + lag_value;
        }
        free(current);
        current = reconstructed;
    }

    for (inv = spec->d; inv > 0u; --inv) {
        double *reconstructed = calloc(horizon ? horizon : 1u, sizeof(*reconstructed));
        const double *base = ord_stages[inv - 1u];

        if (!reconstructed)
            goto fail;
        for (i = 0u; i < horizon; ++i) {
            double lag_value = i > 0u ? reconstructed[i - 1u] : base[history_length - 1u];
            reconstructed[i] = current[i] + lag_value;
        }
        free(current);
        current = reconstructed;
    }

    memcpy(out_levels, current, horizon * sizeof(*out_levels));
    for (inv = 0u; inv < season_stage_count; ++inv)
        free(season_stages[inv]);
    for (inv = 0u; inv < ord_stage_count; ++inv)
        free(ord_stages[inv]);
    free(season_stages);
    free(ord_stages);
    free(current);
    return 0;

fail:
    if (season_stages) {
        for (inv = 0u; inv < season_stage_count; ++inv)
            free(season_stages[inv]);
    }
    if (ord_stages) {
        for (inv = 0u; inv < ord_stage_count; ++inv)
            free(ord_stages[inv]);
    }
    free(season_stages);
    free(ord_stages);
    free(current);
    return -1;
}

static int ts_series_all_nonnegative(const timeseries_t *series)
{
    size_t i;

    if (!series)
        return 0;
    for (i = 0u; i < series->length; ++i) {
        if (series->missing[i])
            continue;
        if (num_to_double(series->values[i]) < 0.0)
            return 0;
    }
    return 1;
}

static int ts_arima_level_stderr(const ts_arima_result_t *model,
                                 const double *history,
                                 size_t history_length,
                                 size_t horizon,
                                 double sigma,
                                 double *out_stderr)
{
    ts_arima_meta_t *meta;
    double *psi = NULL;
    double *phi_lags = NULL;
    double *theta_lags = NULL;
    size_t max_lag = 0u;
    size_t i, j;

    if (!model || !history || history_length == 0u || !out_stderr)
        return -1;
    meta = ts_arima_meta_find(model);
    if (meta)
        max_lag = ts_arima_max_lag(&meta->spec);
    psi = calloc(horizon ? horizon : 1u, sizeof(*psi));
    phi_lags = calloc(max_lag ? max_lag : 1u, sizeof(*phi_lags));
    theta_lags = calloc(max_lag ? max_lag : 1u, sizeof(*theta_lags));
    if (!psi || !phi_lags || !theta_lags)
        return -1;
    if (max_lag > 0u && meta)
        ts_arima_build_lag_maps(model, &meta->spec, phi_lags, theta_lags, max_lag);
    psi[0u] = 1.0;
    for (i = 1u; i < horizon; ++i) {
        double next = (i <= max_lag) ? theta_lags[i - 1u] : 0.0;

        for (j = 1u; j <= max_lag && j <= i; ++j)
            next += phi_lags[j - 1u] * psi[i - j];
        psi[i] = next;
    }

    if (!meta || (meta->spec.d == 0u && meta->spec.D == 0u)) {
        double accum = 0.0;

        for (i = 0u; i < horizon; ++i) {
            accum += psi[i] * psi[i];
            out_stderr[i] = sigma * sqrt(accum);
        }
    } else {
        double *zero_diff = NULL;
        double *base_levels = NULL;
        double *resp_diff = NULL;
        double *resp_levels = NULL;

        zero_diff = calloc(horizon ? horizon : 1u, sizeof(*zero_diff));
        base_levels = calloc(horizon ? horizon : 1u, sizeof(*base_levels));
        resp_diff = calloc(horizon ? horizon : 1u, sizeof(*resp_diff));
        resp_levels = calloc(horizon ? horizon : 1u, sizeof(*resp_levels));
        if (!zero_diff || !base_levels || !resp_diff || !resp_levels) {
            free(zero_diff);
            free(base_levels);
            free(resp_diff);
            free(resp_levels);
            free(psi);
            return -1;
        }
        if (ts_arima_reintegrate_forecast_path(history, history_length, &meta->spec,
                                               zero_diff, horizon, base_levels) != 0) {
            free(zero_diff);
            free(base_levels);
            free(resp_diff);
            free(resp_levels);
            free(psi);
            free(phi_lags);
            free(theta_lags);
            return -1;
        }
        for (j = 0u; j < horizon; ++j) {
            memset(resp_diff, 0, horizon * sizeof(*resp_diff));
            for (i = j; i < horizon; ++i)
                resp_diff[i] = psi[i - j];
            if (ts_arima_reintegrate_forecast_path(history, history_length, &meta->spec,
                                                   resp_diff, horizon, resp_levels) != 0) {
                free(zero_diff);
                free(base_levels);
                free(resp_diff);
                free(resp_levels);
                free(psi);
                free(phi_lags);
                free(theta_lags);
                return -1;
            }
            for (i = 0u; i < horizon; ++i) {
                double delta = resp_levels[i] - base_levels[i];
                out_stderr[i] += delta * delta;
            }
        }
        for (i = 0u; i < horizon; ++i)
            out_stderr[i] = sigma * sqrt(out_stderr[i]);
        free(zero_diff);
        free(base_levels);
        free(resp_diff);
        free(resp_levels);
    }

    free(psi);
    free(phi_lags);
    free(theta_lags);
    return 0;
}

static size_t ts_first_nonmissing_index(const timeseries_t *series)
{
    size_t i;

    if (!series)
        return 0u;
    for (i = 0u; i < series->length; ++i) {
        if (!series->missing[i])
            return i;
    }
    return series->length;
}

static timeseries_t *ts_arima_reconstruct_fitted_levels(const timeseries_t *history,
                                                        const ts_arima_spec_t *spec,
                                                        const timeseries_t *fitted_transformed,
                                                        size_t start_index)
{
    double **ord_stages = NULL;
    double **season_stages = NULL;
    double *history_vals = NULL;
    timeseries_t *display = NULL;
    size_t history_length, rows, ord_stage_count, season_stage_count;
    size_t i, inv;

    if (!history || !spec || !fitted_transformed)
        return NULL;
    history_length = history->length;
    rows = fitted_transformed->length;
    if (rows == 0u || start_index >= history_length)
        return ts_clone(history);

    display = ts_clone(history);
    if (!display)
        goto fail;
    for (i = 0u; i < display->length; ++i) {
        num_destroy(&display->values[i]);
        display->values[i] = num_clone(NUM_NAN);
        display->missing[i] = true;
    }
    if (ts_series_to_double_array(history, &history_vals, &history_length, 0u) != 0)
        goto fail;

    ord_stage_count = spec->d + 1u;
    season_stage_count = spec->D + 1u;
    ord_stages = calloc(ord_stage_count ? ord_stage_count : 1u, sizeof(*ord_stages));
    season_stages = calloc(season_stage_count ? season_stage_count : 1u, sizeof(*season_stages));
    if (!ord_stages || !season_stages)
        goto fail;
    for (inv = 0u; inv < ord_stage_count; ++inv) {
        ord_stages[inv] = calloc(history_length ? history_length : 1u, sizeof(**ord_stages));
        if (!ord_stages[inv])
            goto fail;
    }
    for (inv = 0u; inv < season_stage_count; ++inv) {
        season_stages[inv] = calloc(history_length ? history_length : 1u, sizeof(**season_stages));
        if (!season_stages[inv])
            goto fail;
    }

    memcpy(ord_stages[0u], history_vals, history_length * sizeof(*history_vals));
    for (inv = 1u; inv < ord_stage_count; ++inv) {
        memcpy(ord_stages[inv], ord_stages[inv - 1u], history_length * sizeof(*history_vals));
        ts_double_diff_in_place(ord_stages[inv], history_length);
    }
    memcpy(season_stages[0u], ord_stages[spec->d], history_length * sizeof(*history_vals));
    for (inv = 1u; inv < season_stage_count; ++inv) {
        memcpy(season_stages[inv], season_stages[inv - 1u], history_length * sizeof(*history_vals));
        ts_double_seasonal_diff_in_place(season_stages[inv], history_length, spec->season_period);
    }

    for (i = 0u; i < rows && start_index + i < history_length; ++i) {
        size_t t = start_index + i;
        number_t transformed_n = fitted_transformed->values[i];
        double value = num_to_double(transformed_n);

        for (inv = spec->D; inv > 0u; --inv)
            value += season_stages[inv - 1u][t - spec->season_period];
        for (inv = spec->d; inv > 0u; --inv)
            value += ord_stages[inv - 1u][t - 1u];

        num_destroy(&display->values[t]);
        display->values[t] = num_create_from_double(value);
        display->missing[t] = false;
    }

    for (inv = 0u; inv < season_stage_count; ++inv)
        free(season_stages[inv]);
    for (inv = 0u; inv < ord_stage_count; ++inv)
        free(ord_stages[inv]);
    free(season_stages);
    free(ord_stages);
    free(history_vals);
    return display;

fail:
    if (season_stages) {
        for (inv = 0u; inv < season_stage_count; ++inv)
            free(season_stages[inv]);
    }
    if (ord_stages) {
        for (inv = 0u; inv < ord_stage_count; ++inv)
            free(ord_stages[inv]);
    }
    free(season_stages);
    free(ord_stages);
    free(history_vals);
    ts_free(display);
    return NULL;
}

static size_t ts_arima_effective_seasonal_count(size_t count, size_t season_period)
{
    return (count > 0u && season_period > 0u) ? count : 0u;
}

static size_t ts_arima_max_lag(const ts_arima_spec_t *spec)
{
    size_t max_lag = 0u;
    size_t seasonal_count;

    if (!spec)
        return 0u;
    if (spec->p > max_lag)
        max_lag = spec->p;
    if (spec->q > max_lag)
        max_lag = spec->q;
    seasonal_count = ts_arima_effective_seasonal_count(spec->P, spec->season_period);
    if (seasonal_count > 0u && seasonal_count * spec->season_period > max_lag)
        max_lag = seasonal_count * spec->season_period;
    seasonal_count = ts_arima_effective_seasonal_count(spec->Q, spec->season_period);
    if (seasonal_count > 0u && seasonal_count * spec->season_period > max_lag)
        max_lag = seasonal_count * spec->season_period;
    return max_lag;
}

static matrix_t *ts_arima_dynamic_design_matrix(const timeseries_t *response,
                                                const matrix_t *xreg_trimmed,
                                                const ts_arima_spec_t *spec,
                                                size_t trim,
                                                const double *residual_history)
{
    matrix_t *X;
    size_t seasonal_p = ts_arima_effective_seasonal_count(spec ? spec->P : 0u,
                                                          spec ? spec->season_period : 0u);
    size_t seasonal_q = ts_arima_effective_seasonal_count(spec ? spec->Q : 0u,
                                                          spec ? spec->season_period : 0u);
    size_t xcols = xreg_trimmed ? mat_get_col_count(xreg_trimmed) : 0u;
    size_t rows = response && response->length > trim ? response->length - trim : 0u;
    size_t cols = (spec ? spec->p + seasonal_p + spec->q + seasonal_q : 0u) + xcols;
    size_t r, c, j;

    if (!response || !spec || rows == 0u)
        return NULL;
    X = mat_new(rows, cols);
    if (!X)
        return NULL;
    for (r = 0u; r < rows; ++r) {
        size_t t = trim + r;

        c = 0u;
        for (j = 1u; j <= spec->p; ++j) {
            number_t v = response->values[t - j];
            mat_set(X, r, c++, &v);
        }
        for (j = 1u; j <= seasonal_p; ++j) {
            size_t lag = j * spec->season_period;
            number_t v = response->values[t - lag];
            mat_set(X, r, c++, &v);
        }
        for (j = 1u; j <= spec->q; ++j) {
            number_t v = num_create_from_double(residual_history ? residual_history[t - j] : 0.0);
            mat_set(X, r, c++, &v);
            num_destroy(&v);
        }
        for (j = 1u; j <= seasonal_q; ++j) {
            size_t lag = j * spec->season_period;
            number_t v = num_create_from_double(residual_history ? residual_history[t - lag] : 0.0);
            mat_set(X, r, c++, &v);
            num_destroy(&v);
        }
        if (xreg_trimmed) {
            size_t xcol;

            for (xcol = 0u; xcol < xcols; ++xcol) {
                number_t v = mat_get_num(xreg_trimmed, r, xcol);
                mat_set(X, r, c++, &v);
                num_destroy(&v);
            }
        }
    }
    return X;
}

static void ts_arima_extract_residual_history(const timeseries_t *residuals,
                                              size_t response_length,
                                              size_t trim,
                                              double *out_history)
{
    size_t i;

    if (!out_history || response_length == 0u)
        return;
    memset(out_history, 0, response_length * sizeof(*out_history));
    if (!residuals)
        return;
    for (i = 0u; i < residuals->length && trim + i < response_length; ++i) {
        if (!residuals->missing[i])
            out_history[trim + i] = num_to_double(residuals->values[i]);
    }
}

static int ts_arima_extract_param_blocks(const matrix_t *coefficients,
                                         bool has_intercept,
                                         bool has_drift,
                                         const ts_arima_spec_t *spec,
                                         size_t xreg_cols,
                                         matrix_t **ar_params,
                                         matrix_t **seasonal_ar_params,
                                         matrix_t **ma_params,
                                         matrix_t **seasonal_ma_params)
{
    size_t offset = has_intercept ? 1u : 0u;
    size_t i;
    size_t seasonal_p = ts_arima_effective_seasonal_count(spec ? spec->P : 0u,
                                                          spec ? spec->season_period : 0u);
    size_t seasonal_q = ts_arima_effective_seasonal_count(spec ? spec->Q : 0u,
                                                          spec ? spec->season_period : 0u);

    (void)xreg_cols;
    if (!coefficients || !spec)
        return -1;
    if (has_drift)
        offset += 1u;
    if (ar_params && spec->p > 0u) {
        *ar_params = mat_new(spec->p, 1u);
        if (!*ar_params)
            return -1;
        for (i = 0u; i < spec->p; ++i) {
            number_t v = mat_get_num(coefficients, offset + i, 0u);
            mat_set(*ar_params, i, 0u, &v);
            num_destroy(&v);
        }
    }
    offset += spec->p;
    if (seasonal_ar_params && seasonal_p > 0u) {
        *seasonal_ar_params = mat_new(seasonal_p, 1u);
        if (!*seasonal_ar_params)
            return -1;
        for (i = 0u; i < seasonal_p; ++i) {
            number_t v = mat_get_num(coefficients, offset + i, 0u);
            mat_set(*seasonal_ar_params, i, 0u, &v);
            num_destroy(&v);
        }
    }
    offset += seasonal_p;
    if (ma_params && spec->q > 0u) {
        *ma_params = mat_new(spec->q, 1u);
        if (!*ma_params)
            return -1;
        for (i = 0u; i < spec->q; ++i) {
            number_t v = mat_get_num(coefficients, offset + i, 0u);
            mat_set(*ma_params, i, 0u, &v);
            num_destroy(&v);
        }
    }
    offset += spec->q;
    if (seasonal_ma_params && seasonal_q > 0u) {
        *seasonal_ma_params = mat_new(seasonal_q, 1u);
        if (!*seasonal_ma_params)
            return -1;
        for (i = 0u; i < seasonal_q; ++i) {
            number_t v = mat_get_num(coefficients, offset + i, 0u);
            mat_set(*seasonal_ma_params, i, 0u, &v);
            num_destroy(&v);
        }
    }
    return 0;
}

static void ts_arima_build_lag_maps(const ts_arima_result_t *model,
                                    const ts_arima_spec_t *spec,
                                    double *phi_lags,
                                    double *theta_lags,
                                    size_t max_lag)
{
    size_t i;
    size_t seasonal_p = ts_arima_effective_seasonal_count(spec ? spec->P : 0u,
                                                          spec ? spec->season_period : 0u);
    size_t seasonal_q = ts_arima_effective_seasonal_count(spec ? spec->Q : 0u,
                                                          spec ? spec->season_period : 0u);

    if (!phi_lags || !theta_lags || !spec || max_lag == 0u)
        return;
    memset(phi_lags, 0, max_lag * sizeof(*phi_lags));
    memset(theta_lags, 0, max_lag * sizeof(*theta_lags));
    if (model->ar_params) {
        for (i = 0u; i < mat_get_row_count(model->ar_params) && i < spec->p; ++i) {
            number_t v = mat_get_num(model->ar_params, i, 0u);
            phi_lags[i] = num_to_double(v);
            num_destroy(&v);
        }
    }
    if (model->seasonal_ar_params && spec->season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(model->seasonal_ar_params) && i < seasonal_p; ++i) {
            size_t lag = (i + 1u) * spec->season_period;
            number_t v;

            if (lag == 0u || lag > max_lag)
                continue;
            v = mat_get_num(model->seasonal_ar_params, i, 0u);
            phi_lags[lag - 1u] += num_to_double(v);
            num_destroy(&v);
        }
    }
    if (model->ma_params) {
        for (i = 0u; i < mat_get_row_count(model->ma_params) && i < spec->q; ++i) {
            number_t v = mat_get_num(model->ma_params, i, 0u);
            theta_lags[i] = num_to_double(v);
            num_destroy(&v);
        }
    }
    if (model->seasonal_ma_params && spec->season_period > 0u) {
        for (i = 0u; i < mat_get_row_count(model->seasonal_ma_params) && i < seasonal_q; ++i) {
            size_t lag = (i + 1u) * spec->season_period;
            number_t v;

            if (lag == 0u || lag > max_lag)
                continue;
            v = mat_get_num(model->seasonal_ma_params, i, 0u);
            theta_lags[lag - 1u] += num_to_double(v);
            num_destroy(&v);
        }
    }
}

int ts_arima_fit(const timeseries_t *y,
                 const matrix_t *xreg,
                 const ts_arima_spec_t *spec,
                 const ts_fit_options_t *options,
                 ts_arima_result_t *out)
{
    timeseries_t *dy = NULL, *dy_clean = NULL, *y_trim = NULL;
    matrix_t *x_work = NULL, *x_trim = NULL, *X = NULL;
    ts_regression_result_t reg = {0}, latest = {0};
    timeseries_t *display_fitted = NULL;
    double *residual_history = NULL;
    double *prev_coeff = NULL;
    size_t response_start = 0u;
    size_t trim = 0u, rows, i;
    size_t x_trim_rows = 0u, x_trim_cols = 0u;
    size_t max_iterations = 8u;
    double tolerance = 1e-6;
    bool converged = false;
    bool include_intercept;

    if (!y || !spec || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    dy = ts_build_arima_response(y, spec);
    response_start = ts_first_nonmissing_index(dy);
    dy_clean = dy ? ts_drop_missing(dy) : NULL;
    if (!dy_clean)
        goto fail;
    trim = ts_arima_max_lag(spec);
    include_intercept = spec->include_intercept || spec->include_mean || spec->include_drift;
    if (xreg) {
        size_t x_trim_start = response_start + trim;
        size_t x_rows = mat_get_row_count(xreg);

        x_work = ts_arima_transform_xreg_history(xreg, spec);
        if (!x_work)
            goto fail;
        if (x_trim_start >= x_rows)
            goto fail;
        x_trim = ts_submatrix_rows(x_work, x_trim_start, x_rows - x_trim_start);
        if (!x_trim)
            goto fail;
        x_trim_rows = mat_get_row_count(x_trim);
        x_trim_cols = mat_get_col_count(x_trim);
    }
    rows = dy_clean->length > trim ? dy_clean->length - trim : dy_clean->length;
    if (rows == 0u)
        goto fail;
    if (x_trim && x_trim_rows != rows)
        goto fail;
    y_trim = trim > 0u ? ts_slice(dy_clean, trim, rows) : ts_clone(dy_clean);
    if (!y_trim)
        goto fail;
    residual_history = calloc(dy_clean->length ? dy_clean->length : 1u, sizeof(*residual_history));
    if (!residual_history)
        goto fail;
    if (options && options->max_iterations > 0u)
        max_iterations = options->max_iterations;
    if (options) {
        double tol = num_to_double(options->abs_tol);
        if (tol > 0.0 && isfinite(tol))
            tolerance = tol;
    }

    for (i = 0u; i < max_iterations; ++i) {
        size_t beta_len = 0u;
        double max_change = 0.0;
        size_t k;

        mat_free(X);
        X = ts_arima_dynamic_design_matrix(dy_clean, x_trim, spec, trim, residual_history);
        if (!X)
            goto fail;
        ts_regression_result_clear(&reg);
        if (ts_regression_fit_internal(y_trim, X, include_intercept, spec->include_drift,
                                       options, &reg) != 0)
            goto fail;
        ts_arima_extract_residual_history(reg.residuals, dy_clean->length, trim, residual_history);
        beta_len = reg.coefficients ? mat_get_row_count(reg.coefficients) : 0u;
        if (beta_len > 0u) {
            if (!prev_coeff) {
                prev_coeff = calloc(beta_len, sizeof(*prev_coeff));
                if (!prev_coeff)
                    goto fail;
                for (k = 0u; k < beta_len; ++k) {
                    number_t v = mat_get_num(reg.coefficients, k, 0u);
                    prev_coeff[k] = num_to_double(v);
                    num_destroy(&v);
                }
            } else {
                for (k = 0u; k < beta_len; ++k) {
                    number_t v = mat_get_num(reg.coefficients, k, 0u);
                    double current = num_to_double(v);
                    double diff = fabs(current - prev_coeff[k]);

                    if (diff > max_change)
                        max_change = diff;
                    prev_coeff[k] = current;
                    num_destroy(&v);
                }
                if (max_change <= tolerance)
                    converged = true;
            }
        }
        ts_regression_result_clear(&latest);
        latest = reg;
        memset(&reg, 0, sizeof(reg));
        if (converged)
            break;
    }

    if (!latest.coefficients)
        goto fail;
    if (ts_arima_extract_param_blocks(latest.coefficients,
                                      include_intercept,
                                      spec->include_drift,
                                      spec,
                                      x_trim_cols,
                                      &out->ar_params,
                                      &out->seasonal_ar_params,
                                      &out->ma_params,
                                      &out->seasonal_ma_params) != 0)
        goto fail;
    out->xreg_params = latest.coefficients;
    latest.coefficients = NULL;
    out->param_stderr = latest.stderr;
    latest.stderr = NULL;
    out->param_t_stat = latest.t_stat;
    latest.t_stat = NULL;
    out->param_p_value = latest.p_value;
    latest.p_value = NULL;
    out->vcov = latest.vcov;
    latest.vcov = NULL;
    out->fitted = latest.fitted;
    latest.fitted = NULL;
    out->residuals = latest.residuals;
    latest.residuals = NULL;
    out->innovations = ts_clone(out->residuals);
    out->summary = latest.summary;
    memset(&latest.summary, 0, sizeof(latest.summary));
    {
        number_t intercept_value = NUM_ZERO;
        number_t *intercept_ptr = NULL;

        if (include_intercept && out->xreg_params) {
            intercept_value = mat_get_num(out->xreg_params, 0u, 0u);
            intercept_ptr = &intercept_value;
        }
        if (ts_arima_meta_store(out, spec, include_intercept, spec->include_drift,
                                trim, rows, intercept_ptr, xreg) != 0) {
            if (intercept_ptr)
                num_destroy(intercept_ptr);
            goto fail;
        }
        if (intercept_ptr)
            num_destroy(intercept_ptr);
    }
    if (out->fitted) {
        display_fitted = ts_arima_reconstruct_fitted_levels(y, spec, out->fitted, response_start + trim);
        if (!display_fitted)
            goto fail;
        ts_free(out->fitted);
        out->fitted = display_fitted;
        display_fitted = NULL;
    }
    ts_regression_result_clear(&reg);
    ts_regression_result_clear(&latest);
    ts_free(y_trim);
    ts_free(dy); ts_free(dy_clean);
    mat_free(x_work); mat_free(x_trim); mat_free(X);
    free(residual_history);
    free(prev_coeff);
    return 0;

fail:
    ts_regression_result_clear(&reg);
    ts_regression_result_clear(&latest);
    ts_free(display_fitted);
    ts_free(y_trim);
    ts_free(dy); ts_free(dy_clean);
    mat_free(x_work); mat_free(x_trim); mat_free(X);
    free(residual_history);
    free(prev_coeff);
    ts_arima_result_clear(out);
    return -1;
}

int ts_auto_arima(const timeseries_t *y,
                  const matrix_t *xreg,
                  size_t max_p, size_t max_d, size_t max_q,
                  size_t max_P, size_t max_D, size_t max_Q,
                  size_t season_period,
                  ts_information_criterion_t criterion,
                  const ts_fit_options_t *options,
                  ts_arima_spec_t *best_spec,
                  ts_arima_result_t *best_fit)
{
    double best_score = HUGE_VAL;
    ts_arima_result_t candidate;
    ts_arima_spec_t spec;
    size_t p, d, q, P, D, Q;

    if (!best_spec || !best_fit)
        return -1;
    memset(best_fit, 0, sizeof(*best_fit));
    for (d = 0u; d <= max_d; ++d) {
        for (D = 0u; D <= max_D; ++D) {
            for (p = 0u; p <= max_p; ++p) {
                for (q = 0u; q <= max_q; ++q) {
                    for (P = 0u; P <= max_P; ++P) {
                        for (Q = 0u; Q <= max_Q; ++Q) {
                            double score;

                            memset(&candidate, 0, sizeof(candidate));
                            spec.p = p; spec.d = d; spec.q = q;
                            spec.P = P; spec.D = D; spec.Q = Q;
                            spec.season_period = season_period;
                            spec.include_intercept = true;
                            spec.include_mean = true;
                            spec.include_drift = false;
                            spec.estimation = TS_EST_CSS;
                            if (ts_arima_fit(y, xreg, &spec, options, &candidate) != 0)
                                continue;
                            if (criterion == TS_IC_BIC)
                                score = num_to_double(candidate.summary.bic);
                            else if (criterion == TS_IC_AICC)
                                score = num_to_double(candidate.summary.aicc);
                            else
                                score = num_to_double(candidate.summary.aic);
                            if (score < best_score) {
                                ts_arima_result_clear(best_fit);
                                *best_fit = candidate;
                                if (ts_arima_meta_transfer(best_fit, &candidate) != 0) {
                                    ts_arima_result_clear(best_fit);
                                    memset(&candidate, 0, sizeof(candidate));
                                    return -1;
                                }
                                memset(&candidate, 0, sizeof(candidate));
                                *best_spec = spec;
                                best_score = score;
                            }
                            ts_arima_result_clear(&candidate);
                        }
                    }
                }
            }
        }
    }
    return isfinite(best_score) ? 0 : -1;
}

bool ts_arima_is_stationary(const ts_arima_result_t *model)
{
    size_t i;

    if (!model || !model->ar_params)
        return true;
    for (i = 0u; i < mat_get_row_count(model->ar_params); ++i) {
        number_t v = mat_get_num(model->ar_params, i, 0);
        double d = fabs(num_to_double(v));
        num_destroy(&v);
        if (d >= 1.0)
            return false;
    }
    return true;
}

bool ts_arima_is_invertible(const ts_arima_result_t *model)
{
    size_t i;

    if (!model || !model->ma_params)
        return true;
    for (i = 0u; i < mat_get_row_count(model->ma_params); ++i) {
        number_t v = mat_get_num(model->ma_params, i, 0);
        double d = fabs(num_to_double(v));
        num_destroy(&v);
        if (d >= 1.0)
            return false;
    }
    return true;
}

int ts_arima_residual_acf(const ts_arima_result_t *model,
                          size_t max_lag, matrix_t **out)
{
    return model ? ts_acf(model->residuals, max_lag, out) : -1;
}

int ts_arima_ljung_box(const ts_arima_result_t *model,
                       size_t max_lag,
                       number_t *statistic, number_t *p_value)
{
    return model ? ts_ljung_box(model->residuals, max_lag, statistic, p_value) : -1;
}

int ts_arima_forecast(const ts_arima_result_t *model,
                      const timeseries_t *history,
                      const matrix_t *future_xreg,
                      size_t horizon,
                      number_t level,
                      ts_forecast_t *out)
{
    ts_arima_meta_t *meta;
    timeseries_t *mean = NULL, *stderr = NULL, *lower = NULL, *upper = NULL;
    matrix_t *future_xreg_work = NULL;
    const matrix_t *xreg_for_forecast = future_xreg;
    datetime_t *start = NULL;
    size_t i;
    double *yd = NULL;
    double *residual_history = NULL;
    double *pred_diff = NULL;
    double *pred_levels = NULL;
    double *stderr_levels = NULL;
    double *phi_lags = NULL;
    double *theta_lags = NULL;
    size_t n = 0u;
    size_t max_lag = 0u;
    size_t xcols = future_xreg ? mat_get_col_count(future_xreg) : 0u;
    double sigma = 1.0;
    double z_width = 1.96;
    number_t z = num_create_from_double(1.96);
    int nonnegative_series = 0;

    if (!model || !history || !out || horizon == 0u)
        return -1;
    meta = ts_arima_meta_find(model);
    memset(out, 0, sizeof(*out));
    if (ts_series_to_double_array(history, &yd, &n, 0u) != 0 || n == 0u)
        goto fail;
    if (future_xreg && mat_get_row_count(future_xreg) < horizon)
        goto fail;
    if (future_xreg && meta && ts_arima_spec_differences_xreg(&meta->spec)) {
        future_xreg_work = ts_arima_transform_xreg_future(meta->xreg_history,
                                                          future_xreg,
                                                          &meta->spec);
        if (!future_xreg_work)
            goto fail;
        xreg_for_forecast = future_xreg_work;
    }
    nonnegative_series = ts_series_all_nonnegative(history);
    max_lag = meta ? ts_arima_max_lag(&meta->spec) : 0u;
    start = ts_end_datetime(history);
    if (!start)
        goto fail;
    if (!ts_advance_regular_datetime(start, history->frequency))
        goto fail;
    mean = ts_make_empty_regular_series(horizon, start, history->frequency, history->year_type);
    stderr = ts_make_empty_regular_series(horizon, start, history->frequency, history->year_type);
    lower = ts_make_empty_regular_series(horizon, start, history->frequency, history->year_type);
    upper = ts_make_empty_regular_series(horizon, start, history->frequency, history->year_type);
    if (!mean || !stderr || !lower || !upper)
        goto fail;
    pred_diff = calloc(horizon ? horizon : 1u, sizeof(*pred_diff));
    pred_levels = calloc(horizon ? horizon : 1u, sizeof(*pred_levels));
    stderr_levels = calloc(horizon ? horizon : 1u, sizeof(*stderr_levels));
    residual_history = calloc(n ? n : 1u, sizeof(*residual_history));
    phi_lags = calloc(max_lag ? max_lag : 1u, sizeof(*phi_lags));
    theta_lags = calloc(max_lag ? max_lag : 1u, sizeof(*theta_lags));
    if (!pred_diff || !pred_levels || !stderr_levels || !residual_history || !phi_lags || !theta_lags)
        goto fail;
    sigma = num_to_double(model->summary.sigma2);
    sigma = sigma > 0.0 ? sqrt(sigma) : 1.0;
    z_width = fabs(num_to_double(z));
    if (meta && max_lag > 0u)
        ts_arima_build_lag_maps(model, &meta->spec, phi_lags, theta_lags, max_lag);
    if (model->innovations && max_lag > 0u) {
        size_t trim = meta ? meta->trim : 0u;

        ts_arima_extract_residual_history(model->innovations, n, trim, residual_history);
    }
    for (i = 0u; i < horizon; ++i) {
        double pred = 0.0;
        size_t j;

        if (meta && meta->has_intercept)
            pred += num_to_double(meta->intercept);
        if (meta && meta->has_drift && model->xreg_params) {
            size_t drift_index = meta->has_intercept ? 1u : 0u;
            number_t drift_beta = mat_get_num(model->xreg_params, drift_index, 0u);
            double trend_value = (double)(meta->fit_rows + i + 1u);

            pred += num_to_double(drift_beta) * trend_value;
            num_destroy(&drift_beta);
        }
        for (j = 1u; j <= max_lag; ++j) {
            double y_lag = 0.0;
            double e_lag = 0.0;
            double phi = phi_lags[j - 1u];
            double theta = theta_lags[j - 1u];
            size_t lag_index = n + i - j;

            if (phi != 0.0) {
                if (lag_index < n)
                    y_lag = yd[lag_index];
                else
                    y_lag = pred_diff[lag_index - n];
                pred += phi * y_lag;
            }
            if (theta != 0.0) {
                if (lag_index < n)
                    e_lag = residual_history[lag_index];
                pred += theta * e_lag;
            }
        }
        if (xreg_for_forecast && model->xreg_params) {
            size_t offset = (meta && meta->has_intercept ? 1u : 0u) +
                            (meta && meta->has_drift ? 1u : 0u) +
                            (meta ? meta->spec.p +
                                    ts_arima_effective_seasonal_count(meta->spec.P, meta->spec.season_period) +
                                    meta->spec.q +
                                    ts_arima_effective_seasonal_count(meta->spec.Q, meta->spec.season_period)
                                  : 0u);

            for (j = 0u; j < xcols; ++j) {
                number_t beta = mat_get_num(model->xreg_params, offset + j, 0);
                number_t xv = mat_get_num(xreg_for_forecast, i, j);
                pred += num_to_double(beta) * num_to_double(xv);
                num_destroy(&beta);
                num_destroy(&xv);
            }
        }
        pred_diff[i] = pred;
    }
    if (meta && (meta->spec.d > 0u || meta->spec.D > 0u)) {
        if (ts_arima_reintegrate_forecast_path(yd, n, &meta->spec, pred_diff, horizon, pred_levels) != 0)
            goto fail;
    } else {
        memcpy(pred_levels, pred_diff, horizon * sizeof(*pred_levels));
    }
    if (ts_arima_level_stderr(model, yd, n, horizon, sigma, stderr_levels) != 0)
        goto fail;
    for (i = 0u; i < horizon; ++i) {
        double se_level = stderr_levels[i];
        double lower_level = pred_levels[i] - (z_width * se_level);
        double upper_level = pred_levels[i] + (z_width * se_level);
        number_t pred_n = num_create_from_double(pred_levels[i]);
        number_t se_n = num_create_from_double(se_level);
        number_t lo;
        number_t hi;

        if (nonnegative_series && lower_level < 0.0)
            lower_level = 0.0;
        lo = num_create_from_double(lower_level);
        hi = num_create_from_double(upper_level);

        mean->values[i] = pred_n;
        stderr->values[i] = se_n;
        lower->values[i] = lo;
        upper->values[i] = hi;
    }
    out->mean = mean;
    out->stderr = stderr;
    out->lower = lower;
    out->upper = upper;
    out->level = num_clone(level);
    free(yd);
    free(residual_history);
    free(pred_diff);
    free(pred_levels);
    free(stderr_levels);
    free(phi_lags);
    free(theta_lags);
    mat_free(future_xreg_work);
    datetime_dealloc(start);
    num_destroy(&z);
    return 0;

fail:
    free(yd);
    free(residual_history);
    free(pred_diff);
    free(pred_levels);
    free(stderr_levels);
    free(phi_lags);
    free(theta_lags);
    mat_free(future_xreg_work);
    datetime_dealloc(start);
    ts_free(mean); ts_free(stderr); ts_free(lower); ts_free(upper);
    num_destroy(&z);
    return -1;
}

void ts_arima_result_clear(ts_arima_result_t *out)
{
    if (!out)
        return;
    ts_arima_meta_remove(out);
    mat_free(out->ar_params);
    mat_free(out->ma_params);
    mat_free(out->seasonal_ar_params);
    mat_free(out->seasonal_ma_params);
    mat_free(out->xreg_params);
    mat_free(out->param_stderr);
    mat_free(out->param_t_stat);
    mat_free(out->param_p_value);
    mat_free(out->vcov);
    ts_free(out->fitted);
    ts_free(out->residuals);
    ts_free(out->innovations);
    ts_result_clear_numbers(&out->summary);
    memset(out, 0, sizeof(*out));
}

int ts_accuracy(const timeseries_t *actual,
                const timeseries_t *predicted,
                ts_accuracy_t *out)
{
    timeseries_t *a = NULL, *p = NULL;
    size_t i, n = 0u;
    double mae = 0.0, mse = 0.0, mape = 0.0, smape = 0.0;

    if (!actual || !predicted || !out || ts_align_pair(actual, predicted, TS_JOIN_INNER, &a, &p) != 0)
        return -1;
    memset(out, 0, sizeof(*out));
    for (i = 0u; i < a->length; ++i) {
        double av, pv, err;

        if (a->missing[i] || p->missing[i])
            continue;
        av = num_to_double(a->values[i]);
        pv = num_to_double(p->values[i]);
        err = av - pv;
        mae += fabs(err);
        mse += err * err;
        if (av != 0.0)
            mape += fabs(err / av);
        if (fabs(av) + fabs(pv) != 0.0)
            smape += 2.0 * fabs(av - pv) / (fabs(av) + fabs(pv));
        n++;
    }
    if (n == 0u) {
        ts_free(a); ts_free(p);
        return -1;
    }
    out->mae = num_create_from_double(mae / (double)n);
    out->mse = num_create_from_double(mse / (double)n);
    out->rmse = num_create_from_double(sqrt(mse / (double)n));
    out->mape = num_create_from_double(mape / (double)n);
    out->smape = num_create_from_double(smape / (double)n);
    out->mase = num_clone(out->mae);
    ts_free(a); ts_free(p);
    return 0;
}

int ts_backtest_regression(const timeseries_t *y,
                           const matrix_t *xreg,
                           const ts_fit_options_t *options,
                           const ts_backtest_spec_t *spec,
                           ts_accuracy_t *out)
{
    ts_regression_result_t fit = {0};
    ts_forecast_t fc = {0};
    timeseries_t *train = NULL, *test = NULL;
    matrix_t *x_train = NULL, *x_test = NULL;
    int rc;

    if (!y || !xreg || !spec || spec->train_length == 0u || spec->test_length == 0u)
        return -1;
    train = ts_slice(y, 0u, spec->train_length);
    test = ts_slice(y, spec->train_length, spec->test_length);
    x_train = ts_submatrix_rows(xreg, 0u, spec->train_length);
    x_test = ts_submatrix_rows(xreg, spec->train_length, spec->test_length);
    if (!train || !test || !x_train || !x_test)
        goto fail;
    if (ts_regression_fit(train, x_train, options, &fit) != 0)
        goto fail;
    if (ts_regression_forecast(&fit, x_test, train, y->frequency, y->year_type, NUM_HALF, &fc) != 0)
        goto fail;
    rc = ts_accuracy(test, fc.mean, out);
    ts_regression_result_clear(&fit);
    ts_forecast_clear(&fc);
    ts_free(train); ts_free(test); mat_free(x_train); mat_free(x_test);
    return rc;
fail:
    ts_regression_result_clear(&fit);
    ts_forecast_clear(&fc);
    ts_free(train); ts_free(test); mat_free(x_train); mat_free(x_test);
    return -1;
}

int ts_backtest_arima(const timeseries_t *y,
                      const matrix_t *xreg,
                      const ts_arima_spec_t *model_spec,
                      const ts_fit_options_t *options,
                      const ts_backtest_spec_t *spec,
                      ts_accuracy_t *out)
{
    ts_arima_result_t fit = {0};
    ts_forecast_t fc = {0};
    timeseries_t *train = NULL, *test = NULL;
    matrix_t *x_train = NULL, *x_test = NULL;
    int rc;

    if (!y || !model_spec || !spec || spec->train_length == 0u || spec->test_length == 0u)
        return -1;
    train = ts_slice(y, 0u, spec->train_length);
    test = ts_slice(y, spec->train_length, spec->test_length);
    if (xreg) {
        x_train = ts_submatrix_rows(xreg, 0u, spec->train_length);
        x_test = ts_submatrix_rows(xreg, spec->train_length, spec->test_length);
    }
    if (!train || !test || (xreg && (!x_train || !x_test)))
        goto fail;
    if (ts_arima_fit(train, x_train, model_spec, options, &fit) != 0)
        goto fail;
    if (ts_arima_forecast(&fit, train, x_test, spec->test_length, NUM_HALF, &fc) != 0)
        goto fail;
    rc = ts_accuracy(test, fc.mean, out);
    ts_arima_result_clear(&fit);
    ts_forecast_clear(&fc);
    ts_free(train); ts_free(test); mat_free(x_train); mat_free(x_test);
    return rc;
fail:
    ts_arima_result_clear(&fit);
    ts_forecast_clear(&fc);
    ts_free(train); ts_free(test); mat_free(x_train); mat_free(x_test);
    return -1;
}

void ts_forecast_clear(ts_forecast_t *out)
{
    if (!out)
        return;
    ts_free(out->mean);
    ts_free(out->stderr);
    ts_free(out->lower);
    ts_free(out->upper);
    num_destroy(&out->level);
    memset(out, 0, sizeof(*out));
}
