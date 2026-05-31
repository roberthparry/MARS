#include "timeseries_internal.h"

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
