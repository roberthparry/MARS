#define MARS_TIMESERIES_INTERNAL_ACCESS
#include "timeseries_internal.h"

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

int ts_regression_fit_internal(const timeseries_t *y,
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
    ts_step_regular_datetime(start, frequency);
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
