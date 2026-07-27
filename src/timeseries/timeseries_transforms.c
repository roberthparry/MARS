#define MARS_TIMESERIES_INTERNAL_ACCESS
#include "timeseries_internal.h"

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

typedef struct {
    double sum;
    double sumsq;
    double minv;
    double maxv;
    size_t count;
} ts_roll_window_t;

typedef double (*ts_roll_eval_fn)(const ts_roll_window_t *window);

static double ts_roll_eval_mean(const ts_roll_window_t *window)
{
    return window->sum / (double)window->count;
}

static double ts_roll_eval_sum(const ts_roll_window_t *window)
{
    return window->sum;
}

static double ts_roll_eval_var(const ts_roll_window_t *window)
{
    double mean = window->sum / (double)window->count;

    return (window->sumsq / (double)window->count) - mean * mean;
}

static double ts_roll_eval_std(const ts_roll_window_t *window)
{
    double var = ts_roll_eval_var(window);

    return sqrt(var < 0.0 ? 0.0 : var);
}

static timeseries_t *ts_roll_generic(const timeseries_t *series, size_t window, ts_roll_eval_fn eval)
{
    timeseries_t *out = ts_clone(series);
    size_t i;

    if (!out || window == 0u || !eval)
        return NULL;
    for (i = 0u; i < out->length; ++i) {
        size_t start = i + 1u >= window ? i + 1u - window : 0u;
        size_t j;
        ts_roll_window_t state = {0};

        for (j = start; j <= i; ++j) {
            double v;
            if (series->missing[j])
                continue;
            v = num_to_double(series->values[j]);
            if (state.count == 0u) {
                state.minv = state.maxv = v;
            } else {
                if (v < state.minv) state.minv = v;
                if (v > state.maxv) state.maxv = v;
            }
            state.sum += v;
            state.sumsq += v * v;
            state.count++;
        }
        num_destroy(&out->values[i]);
        if (state.count == 0u) {
            out->values[i] = num_clone(NUM_NAN);
            out->missing[i] = true;
        } else {
            double result = eval(&state);
            out->values[i] = num_create_from_double(result);
            out->missing[i] = false;
        }
    }
    return out;
}

timeseries_t *ts_roll_mean(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, ts_roll_eval_mean); }
timeseries_t *ts_roll_sum(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, ts_roll_eval_sum); }
timeseries_t *ts_roll_var(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, ts_roll_eval_var); }
timeseries_t *ts_roll_std(const timeseries_t *series, size_t window) { return ts_roll_generic(series, window, ts_roll_eval_std); }

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

typedef struct {
    double sum;
    double minv;
    double maxv;
    size_t count;
} ts_aggregate_bucket_t;

typedef double (*ts_aggregate_eval_fn)(const ts_aggregate_bucket_t *bucket);

static double ts_aggregate_eval_sum(const ts_aggregate_bucket_t *bucket)
{
    return bucket->sum;
}

static double ts_aggregate_eval_mean(const ts_aggregate_bucket_t *bucket)
{
    return bucket->sum / (double)bucket->count;
}

static double ts_aggregate_eval_min(const ts_aggregate_bucket_t *bucket)
{
    return bucket->minv;
}

static double ts_aggregate_eval_max(const ts_aggregate_bucket_t *bucket)
{
    return bucket->maxv;
}

static timeseries_t *ts_aggregate_generic(const timeseries_t *series,
                                          ts_frequency_t target_frequency,
                                          ts_year_type_t year_type,
                                          ts_aggregate_eval_fn eval)
{
    size_t i = 0u, cap = 16u, len = 0u;
    number_t *values = calloc(cap, sizeof(*values));
    datetime_t **index = calloc(cap, sizeof(*index));
    bool *missing = calloc(cap, sizeof(*missing));
    timeseries_t *out;

    if (!series || !series->has_index || !values || !index || !missing || !eval)
        goto fail;
    while (i < series->length) {
        size_t j = i;
        ts_aggregate_bucket_t bucket = {0};

        while (j < series->length &&
               ts_datetime_same_bucket(series->index[i], series->index[j],
                                       target_frequency, year_type)) {
            if (!series->missing[j]) {
                double v = num_to_double(series->values[j]);
                if (bucket.count == 0u) bucket.minv = bucket.maxv = v;
                else {
                    if (v < bucket.minv) bucket.minv = v;
                    if (v > bucket.maxv) bucket.maxv = v;
                }
                bucket.sum += v;
                bucket.count++;
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
        if (bucket.count == 0u) {
            values[len] = num_clone(NUM_NAN);
            missing[len] = true;
        } else {
            double result = eval(&bucket);
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
    return ts_aggregate_generic(series, target_frequency, series->year_type, ts_aggregate_eval_mean);
}

timeseries_t *ts_aggregate_sum(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, ts_aggregate_eval_sum); }
timeseries_t *ts_aggregate_mean(const timeseries_t *series,
                                ts_frequency_t target_frequency,
                                ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, ts_aggregate_eval_mean); }
timeseries_t *ts_aggregate_min(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, ts_aggregate_eval_min); }
timeseries_t *ts_aggregate_max(const timeseries_t *series,
                               ts_frequency_t target_frequency,
                               ts_year_type_t year_type) { return ts_aggregate_generic(series, target_frequency, year_type, ts_aggregate_eval_max); }
