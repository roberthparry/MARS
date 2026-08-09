#define MARS_TIMESERIES_INTERNAL_ACCESS
#include "timeseries_internal.h"
#include "ustring.h"

static ts_arima_meta_t *ts_arima_meta_head = NULL;

int ts_appendf(ts_string_builder_t *sb, const char *fmt, ...)
{
    va_list ap;
    string_t *piece;
    int rc;

    if (!sb || !fmt)
        return -1;

    va_start(ap, fmt);
    piece = string_vsprintf(fmt, ap);
    va_end(ap);
    if (!piece)
        return -1;

    rc = ts_append_text(sb, piece);
    string_free(piece);
    return rc;
}

int ts_append_text(ts_string_builder_t *sb, const string_t *text)
{
    if (!sb || !text)
        return -1;

    if (!sb->text) {
        sb->text = string_new();
        if (!sb->text)
            return -1;
    }

    return string_append_string(sb->text, text);
}

size_t ts_builder_encoded_length(const ts_string_builder_t *sb)
{
    return (sb && sb->text) ? string_view_length(string_view_all(sb->text)) : 0u;
}

void ts_builder_free(ts_string_builder_t *sb)
{
    if (!sb)
        return;
    string_free(sb->text);
    sb->text = NULL;
}

string_t *ts_builder_detach_text(ts_string_builder_t *sb)
{
    string_t *out;

    if (!sb)
        return NULL;
    out = sb->text;
    sb->text = NULL;
    return out;
}

datetime_t *ts_datetime_clone(const datetime_t *dt)
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

typedef datetime_t *(*ts_datetime_step_fn)(datetime_t *dt);
typedef datetime_t *(*ts_bucket_label_fn)(const datetime_t *dt, ts_year_type_t year_type);

static datetime_t *ts_step_daily(datetime_t *dt)
{
    return datetime_add_days(dt, 1L);
}
static datetime_t *ts_step_monthly(datetime_t *dt)
{
    return datetime_add_months(dt, 1);
}
static datetime_t *ts_step_quarterly(datetime_t *dt)
{
    return datetime_add_months(dt, 3);
}
static datetime_t *ts_step_yearly(datetime_t *dt)
{
    return datetime_add_years(dt, 1);
}

static const ts_datetime_step_fn ts_frequency_step_dispatch[] = {
    [TS_FREQ_DAILY] = ts_step_daily,
    [TS_FREQ_MONTHLY] = ts_step_monthly,
    [TS_FREQ_QUARTERLY] = ts_step_quarterly,
    [TS_FREQ_YEARLY] = ts_step_yearly,
};

static const int ts_frequency_season_period_dispatch[] = {
    [TS_FREQ_MONTHLY] = 12,
    [TS_FREQ_QUARTERLY] = 4,
    [TS_FREQ_YEARLY] = 1,
};

static ts_datetime_step_fn ts_frequency_step(ts_frequency_t frequency)
{
    size_t idx = (size_t)frequency;

    if (idx >= sizeof(ts_frequency_step_dispatch) / sizeof(ts_frequency_step_dispatch[0]))
        return NULL;
    return ts_frequency_step_dispatch[idx];
}

int ts_step_regular_datetime(datetime_t *dt, ts_frequency_t frequency)
{
    ts_datetime_step_fn step = ts_frequency_step(frequency);

    if (!dt || !step)
        return 0;
    return step(dt) ? 1 : 0;
}

datetime_t *ts_advance_regular_datetime(datetime_t *dt, ts_frequency_t frequency)
{
    int preserve_month_end;

    if (!dt)
        return NULL;
    preserve_month_end = ts_datetime_is_month_end(dt);
    if (!ts_frequency_step(frequency))
        return dt;
    if (!ts_step_regular_datetime(dt, frequency))
        return NULL;
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

int ts_infer_season_period(ts_frequency_t frequency)
{
    size_t idx = (size_t)frequency;

    if (idx >= sizeof(ts_frequency_season_period_dispatch) / sizeof(ts_frequency_season_period_dispatch[0]))
        return 0;
    return ts_frequency_season_period_dispatch[idx];
}

int ts_parse_date_text(const string_t *text, datetime_t **out)
{
    string_t *trimmed;
    const char *raw;
    datetime_t *dt;

    if (!text || !out)
        return -1;

    trimmed = string_clone(text);
    if (!trimmed)
        return -1;
    string_trim(trimmed);

    raw = string_c_str(trimmed);
    dt = datetime_from_string(raw);
    string_free(trimmed);
    if (!dt)
        return -1;
    *out = dt;
    return 0;
}

bool ts_datetime_same_bucket(const datetime_t *a, const datetime_t *b, ts_frequency_t frequency,
                             ts_year_type_t year_type)
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
        return datetime_year(a) == datetime_year(b) && datetime_month(a) == datetime_month(b) &&
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

static datetime_t *ts_bucket_label_daily(const datetime_t *dt, ts_year_type_t year_type)
{
    (void)year_type;
    return ts_datetime_clone(dt);
}

static datetime_t *ts_bucket_label_monthly(const datetime_t *dt, ts_year_type_t year_type)
{
    short year;
    month_t month;
    uint8_t day;

    (void)year_type;
    if (!dt)
        return NULL;
    year = datetime_year(dt);
    month = datetime_month(dt);
    day = datetime_day(dt);
    return datetime_init_ymd(datetime_alloc(), year, month, day);
}

static datetime_t *ts_bucket_label_quarterly(const datetime_t *dt, ts_year_type_t year_type)
{
    short year;
    month_t month;

    if (!dt)
        return NULL;
    year = datetime_year(dt);
    month = datetime_month(dt);
    if (year_type == TS_YEAR_FISCAL_UK_APR) {
        int fiscal_year = year;
        int m = (int)month;
        int qstart;

        if (m < 4)
            fiscal_year -= 1;
        if (m >= 4 && m <= 6)
            qstart = 4;
        else if (m <= 9)
            qstart = 7;
        else if (m <= 12)
            qstart = 10;
        else
            qstart = 1;
        if (m < 4)
            qstart = 1;
        return datetime_init_ymd(datetime_alloc(), (short)(qstart == 1 ? fiscal_year + 1 : fiscal_year),
                                 (month_t)qstart, 1u);
    }
    return datetime_init_ymd(datetime_alloc(), year, (month_t)(((int)(month - 1) / 3) * 3 + 1), 1u);
}

static datetime_t *ts_bucket_label_yearly(const datetime_t *dt, ts_year_type_t year_type)
{
    short year;
    month_t month;

    if (!dt)
        return NULL;
    year = datetime_year(dt);
    month = datetime_month(dt);
    if (year_type == TS_YEAR_FISCAL_UK_APR) {
        short fiscal_year = year;

        if ((int)month < 4)
            fiscal_year -= 1;
        return datetime_init_ymd(datetime_alloc(), fiscal_year, DT_April, 1u);
    }
    return datetime_init_ymd(datetime_alloc(), year, DT_January, 1u);
}

static const ts_bucket_label_fn ts_bucket_label_dispatch[] = {
    [TS_FREQ_DAILY] = ts_bucket_label_daily,
    [TS_FREQ_MONTHLY] = ts_bucket_label_monthly,
    [TS_FREQ_QUARTERLY] = ts_bucket_label_quarterly,
    [TS_FREQ_YEARLY] = ts_bucket_label_yearly,
};

datetime_t *ts_bucket_label(const datetime_t *dt, ts_frequency_t frequency, ts_year_type_t year_type)
{
    size_t idx = (size_t)frequency;

    if (idx < sizeof(ts_bucket_label_dispatch) / sizeof(ts_bucket_label_dispatch[0]) && ts_bucket_label_dispatch[idx])
        return ts_bucket_label_dispatch[idx](dt, year_type);
    return ts_datetime_clone(dt);
}

int ts_append_number_text(ts_string_builder_t *sb, number_t value)
{
    string_t *text = num_to_string(value);
    int rc;

    if (!text)
        return -1;
    rc = ts_append_text(sb, text);
    string_free(text);
    return rc;
}

int ts_append_number_fixed(ts_string_builder_t *sb, number_t value, int decimals)
{
    double d;

    if (!sb)
        return -1;
    d = num_to_double(value);
    if (!isfinite(d))
        return ts_append_number_text(sb, value);
    return ts_appendf(sb, "%.*f", decimals, d);
}

int ts_append_number_sig(ts_string_builder_t *sb, number_t value, int sig_figs)
{
    double d;

    if (!sb)
        return -1;
    d = num_to_double(value);
    if (!isfinite(d))
        return ts_append_number_text(sb, value);
    return ts_appendf(sb, "%.*g", sig_figs, d);
}

timeseries_t *ts_alloc_empty(size_t length)
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

void ts_copy_value_slot(number_t *dst, const number_t *src)
{
    if (!dst || !src)
        return;
    *dst = num_clone(*src);
}

int ts_set_index(timeseries_t *series, const datetime_t *const *index)
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

timeseries_t *ts_clone_shallow_shape(const timeseries_t *series)
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

int ts_copy_into(timeseries_t *dst, const timeseries_t *src)
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

timeseries_t *ts_make_empty_regular_series(size_t length, const datetime_t *start, ts_frequency_t frequency,
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

int ts_series_to_double_array(const timeseries_t *series, double **out_values, size_t *out_count, size_t start)
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

matrix_t *ts_make_column_matrix_from_doubles(const double *values, size_t n)
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

matrix_t *ts_make_matrix_from_doubles(const double *values, size_t rows, size_t cols)
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

int ts_write_text_file(const string_t *path, const string_t *text)
{
    FILE *f;

    if (!path || !text)
        return -1;
    f = fopen(string_c_str(path), "w");
    if (!f)
        return -1;
    if (fputs(string_c_str(text), f) == EOF) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

ts_arima_meta_t *ts_arima_meta_find(const ts_arima_result_t *owner)
{
    ts_arima_meta_t *node = ts_arima_meta_head;

    while (node) {
        if (node->owner == owner)
            return node;
        node = node->next;
    }
    return NULL;
}

matrix_t *ts_matrix_clone_local(const matrix_t *src)
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

matrix_t *ts_submatrix_rows(const matrix_t *A, size_t start_row, size_t row_count)
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

void ts_arima_meta_remove(const ts_arima_result_t *owner)
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

int ts_arima_meta_store(const ts_arima_result_t *owner, const ts_arima_spec_t *spec, bool has_intercept, bool has_drift,
                        size_t trim, size_t fit_rows, const number_t *intercept, const matrix_t *xreg_history)
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

int ts_arima_meta_transfer(const ts_arima_result_t *dst, const ts_arima_result_t *src)
{
    ts_arima_meta_t *meta;

    if (!dst || !src)
        return -1;
    meta = ts_arima_meta_find(src);
    if (!meta)
        return 0;
    return ts_arima_meta_store(dst, &meta->spec, meta->has_intercept, meta->has_drift, meta->trim, meta->fit_rows,
                               meta->has_intercept ? &meta->intercept : NULL, meta->xreg_history);
}

void ts_result_clear_numbers(ts_fit_summary_t *summary)
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
