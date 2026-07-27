#define MARS_TIMESERIES_INTERNAL_ACCESS
#include "timeseries_internal.h"

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
