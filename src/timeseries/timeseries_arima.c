#include "timeseries_internal.h"

static size_t ts_arima_max_lag(const ts_arima_spec_t *spec);
static void ts_arima_build_lag_maps(const ts_arima_result_t *model,
                                    const ts_arima_spec_t *spec,
                                    double *phi_lags,
                                    double *theta_lags,
                                    size_t max_lag);

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

size_t ts_arima_effective_seasonal_count(size_t count, size_t season_period)
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
