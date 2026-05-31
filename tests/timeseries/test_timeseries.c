#include "test_timeseries.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static matrix_t *test_submatrix_rows_local(const matrix_t *A, size_t start_row, size_t row_count)
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

static matrix_t *test_aligned_xreg_from_columns(const timeseries_t *target,
                                                const char *csv_path,
                                                const char *date_column,
                                                const char *const *columns,
                                                size_t column_count)
{
    timeseries_t **aligned_columns = NULL;
    matrix_t *out = NULL;
    size_t i;

    if (!target || !csv_path || !columns || column_count == 0u)
        return NULL;
    aligned_columns = calloc(column_count, sizeof(*aligned_columns));
    if (!aligned_columns)
        return NULL;
    for (i = 0u; i < column_count; ++i) {
        timeseries_t *raw = ts_from_csv(csv_path, date_column, columns[i],
                                        TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                        TS_MISSING_DROP);
        timeseries_t *y_aligned = NULL;

        if (!raw || ts_align_pair(target, raw, TS_JOIN_INNER, &y_aligned, &aligned_columns[i]) != 0) {
            ts_free(raw);
            ts_free(y_aligned);
            goto fail;
        }
        ts_free(raw);
        ts_free(y_aligned);
    }
    out = ts_bind_columns(aligned_columns, column_count, TS_JOIN_INNER);

fail:
    if (aligned_columns) {
        for (i = 0u; i < column_count; ++i)
            ts_free(aligned_columns[i]);
    }
    free(aligned_columns);
    return out;
}

static void test_csv_load_and_slice(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    datetime_t *start = NULL;
    datetime_t *end = NULL;
    timeseries_t *head = NULL;

    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_TRUE(ts_length(y) > 20u, "loaded monthly series");
    TEST_ASSERT_TRUE(ts_frequency(y) == TS_FREQ_MONTHLY, "frequency recorded");
    TEST_ASSERT_TRUE(ts_year_type(y) == TS_YEAR_FISCAL_UK_APR, "year type recorded");

    start = ts_start_datetime(y);
    end = ts_end_datetime(y);
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_TRUE(datetime_compare(start, end) < 0, "date range ordered");

    head = ts_head(y, 3u);
    TEST_ASSERT_NOT_NULL(head);
    TEST_ASSERT_TRUE(ts_length(head) == 3u, "head length");

    ts_free(head);
    datetime_dealloc(start);
    datetime_dealloc(end);
    ts_free(y);
}

static void test_transforms_and_aggregation(void)
{
    number_t vals[6] = {
        num_create_from_long(10),
        num_create_from_long(12),
        num_create_from_long(14),
        num_create_from_long(16),
        num_create_from_long(18),
        num_create_from_long(20)
    };
    datetime_t *start = datetime_init_ymd(datetime_alloc(), 2024, DT_January, 31u);
    timeseries_t *s = ts_new_regular(vals, 6u, start, TS_FREQ_MONTHLY, TS_YEAR_CALENDAR);
    timeseries_t *lag1 = NULL;
    timeseries_t *diff1 = NULL;
    timeseries_t *roll = NULL;
    timeseries_t *q = NULL;
    number_t v = NUM_ZERO;

    for (size_t i = 0u; i < 6u; ++i)
        num_destroy(&vals[i]);
    datetime_dealloc(start);

    TEST_ASSERT_NOT_NULL(s);
    lag1 = ts_lag(s, 1u);
    diff1 = ts_diff(s, 1u);
    roll = ts_roll_mean(s, 2u);
    q = ts_aggregate_mean(s, TS_FREQ_QUARTERLY, TS_YEAR_CALENDAR);
    TEST_ASSERT_NOT_NULL(lag1);
    TEST_ASSERT_NOT_NULL(diff1);
    TEST_ASSERT_NOT_NULL(roll);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(ts_get_value(diff1, 3u, &v) == 0, "diff value available");
    TEST_ASSERT_TRUE(fabs(num_to_double(v) - 2.0) < 1e-9, "first difference");
    num_destroy(&v);
    TEST_ASSERT_TRUE(ts_length(q) == 2u, "quarter aggregation length");

    ts_free(q);
    ts_free(roll);
    ts_free(diff1);
    ts_free(lag1);
    ts_free(s);
}

static void test_output_and_write_file(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    char *text = NULL;
    const char *csv_path = test_case_temp_path("timeseries.csv");
    FILE *f = NULL;
    char buf[128] = {0};
    size_t used = 0u;
    const char *cols[] = { "All" };
    matrix_t *x = NULL;
    ts_regression_result_t fit = {0};
    char *summary = NULL;

    TEST_ASSERT_NOT_NULL(y);
    text = ts_to_string(ts_head(y, 2u), TS_STRING_CSV);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(strstr(text, "date,value") != NULL, "csv header present");
    free(text);

    TEST_ASSERT_TRUE(ts_write_file(csv_path, ts_head(y, 2u), TS_STRING_CSV) == 0,
                     "write csv file");
    f = fopen(csv_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    used = fread(buf, 1u, sizeof(buf) - 1u, f);
    buf[used] = '\0';
    fclose(f);
    TEST_ASSERT_TRUE(strstr(buf, "date,value") != NULL, "written csv header");

    x = ts_matrix_from_csv("sample_data/Monthly Population.csv",
                           "DATE", cols, 1u,
                           TS_FREQ_MONTHLY, TS_MISSING_DROP);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_TRUE(ts_regression_fit(y, x, NULL, &fit) == 0, "regression fit for summary");
    summary = ts_regression_summary_to_string(&fit);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_TRUE(strstr(summary, "Model comparison score (AIC):") != NULL,
                     "summary uses plain-language labels");
    TEST_ASSERT_TRUE(strstr(summary, "(comparison only; lower is better") != NULL,
                     "summary explains comparison-only metrics");
    TEST_ASSERT_TRUE(strstr(summary, "Overall fit score (R2):") != NULL && strstr(summary, "(") != NULL,
                     "summary includes inline ratings");
    TEST_ASSERT_TRUE(strstr(summary, "Overall assessment: You've chosen") != NULL,
                     "summary includes a friendly overall assessment");
    TEST_ASSERT_TRUE(strstr(summary, "improve") != NULL || strstr(summary, "better result") != NULL,
                     "summary assessment includes improvement guidance");

    free(summary);
    ts_regression_result_clear(&fit);
    mat_free(x);
    ts_free(y);
}

static void test_regression_and_forecast(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    const char *cols[] = { "All" };
    matrix_t *x = ts_matrix_from_csv("sample_data/Monthly Population.csv",
                                     "DATE", cols, 1u,
                                     TS_FREQ_MONTHLY, TS_MISSING_DROP);
    ts_regression_result_t fit = {0};
    ts_forecast_t fc = {0};
    matrix_t *future_x = NULL;
    number_t level = num_create_from_double(0.95);

    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_TRUE(ts_regression_fit(y, x, NULL, &fit) == 0, "regression fit");
    TEST_ASSERT_NOT_NULL(fit.coefficients);
    TEST_ASSERT_NOT_NULL(fit.fitted);
    TEST_ASSERT_TRUE(ts_length(fit.fitted) == ts_length(y), "fitted length");

    future_x = test_submatrix_rows_local(x, mat_get_row_count(x) - 3u, 3u);
    TEST_ASSERT_NOT_NULL(future_x);
    TEST_ASSERT_TRUE(ts_regression_forecast(&fit, future_x, y, TS_FREQ_MONTHLY,
                                            TS_YEAR_FISCAL_UK_APR, level, &fc) == 0,
                     "regression forecast");
    TEST_ASSERT_NOT_NULL(fc.mean);
    TEST_ASSERT_TRUE(ts_length(fc.mean) == 3u, "forecast length");

    num_destroy(&level);
    mat_free(future_x);
    ts_forecast_clear(&fc);
    ts_regression_result_clear(&fit);
    mat_free(x);
    ts_free(y);
}

static void test_arima_smoke(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    ts_arima_spec_t spec = {0};
    ts_arima_result_t fit = {0};
    ts_forecast_t fc = {0};
    number_t level = num_create_from_double(0.95);
    number_t last_actual = NUM_ZERO;
    number_t first_forecast = NUM_ZERO;
    number_t third_forecast = NUM_ZERO;
    number_t first_lower = NUM_ZERO;
    number_t last_fitted = NUM_ZERO;
    double last_value;
    double first_value;
    double third_value;
    double last_fitted_value;
    datetime_t *forecast_date = NULL;
    char *forecast_date_text = NULL;

    TEST_ASSERT_NOT_NULL(y);
    spec.p = 1u;
    spec.d = 1u;
    spec.q = 0u;
    spec.P = 0u;
    spec.D = 0u;
    spec.Q = 0u;
    spec.season_period = 12u;
    spec.include_intercept = true;
    spec.include_mean = true;

    TEST_ASSERT_TRUE(ts_arima_fit(y, NULL, &spec, NULL, &fit) == 0, "arima fit");
    TEST_ASSERT_NOT_NULL(fit.residuals);
    TEST_ASSERT_NOT_NULL(fit.fitted);
    TEST_ASSERT_TRUE(ts_get_value(fit.fitted, ts_length(fit.fitted) - 1u, &last_fitted) == 0,
                     "last fitted value available");
    TEST_ASSERT_TRUE(ts_arima_forecast(&fit, y, NULL, 3u, level, &fc) == 0,
                     "arima forecast");
    TEST_ASSERT_NOT_NULL(fc.mean);
    TEST_ASSERT_TRUE(ts_length(fc.mean) == 3u, "arima horizon");
    TEST_ASSERT_TRUE(ts_get_value(y, ts_length(y) - 1u, &last_actual) == 0, "last actual available");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 0u, &first_forecast) == 0, "first forecast available");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 2u, &third_forecast) == 0, "third forecast available");
    TEST_ASSERT_TRUE(ts_get_value(fc.lower, 0u, &first_lower) == 0, "first lower bound available");
    last_value = num_to_double(last_actual);
    first_value = num_to_double(first_forecast);
    third_value = num_to_double(third_forecast);
    last_fitted_value = num_to_double(last_fitted);
    TEST_ASSERT_TRUE(fabs(last_fitted_value - last_value) < fabs(last_value) * 0.75,
                     "arima fitted values remain on the original level scale");
    TEST_ASSERT_TRUE(fabs(first_value - last_value) < fabs(last_value) * 0.75,
                     "arima forecast remains on the original level scale");
    TEST_ASSERT_TRUE(isfinite(first_value) && isfinite(third_value),
                     "arima multi-step forecast remains finite");
    TEST_ASSERT_TRUE(isfinite(num_to_double(first_lower)) && num_to_double(first_lower) >= 0.0,
                     "arima lower bounds for nonnegative series remain finite and nonnegative");
    forecast_date = datetime_alloc();
    TEST_ASSERT_NOT_NULL(forecast_date);
    TEST_ASSERT_TRUE(ts_get_datetime(fc.mean, 1u, forecast_date) == 0,
                     "second forecast date available");
    forecast_date_text = datetime_format(forecast_date, "%dd/%mm/%yyyy");
    TEST_ASSERT_NOT_NULL(forecast_date_text);
    TEST_ASSERT_TRUE(strcmp(forecast_date_text, "31/07/2026") == 0,
                     "monthly forecasts preserve month-end dates");
    num_destroy(&last_actual);
    num_destroy(&first_forecast);
    num_destroy(&third_forecast);
    num_destroy(&first_lower);
    num_destroy(&last_fitted);
    datetime_dealloc(forecast_date);
    free(forecast_date_text);

    num_destroy(&level);
    ts_forecast_clear(&fc);
    ts_arima_result_clear(&fit);
    ts_free(y);
}

static void test_exogenous_alignment_for_future_forecasts(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    timeseries_t *x = ts_from_csv("sample_data/Monthly Population.csv",
                                  "DATE", "All",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    timeseries_t *y_aligned = NULL;
    timeseries_t *x_aligned = NULL;
    timeseries_t *x_future = NULL;
    datetime_t *future_start = NULL;
    datetime_t *future_end = NULL;
    matrix_t *future_x = NULL;
    timeseries_t *cols[1] = { NULL };

    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_TRUE(ts_align_pair(y, x, TS_JOIN_INNER, &y_aligned, &x_aligned) == 0,
                     "target and exogenous alignment");
    TEST_ASSERT_NOT_NULL(y_aligned);
    TEST_ASSERT_NOT_NULL(x_aligned);
    TEST_ASSERT_TRUE(ts_length(y_aligned) > 0u, "aligned target retains a useful history");
    TEST_ASSERT_TRUE(ts_length(y_aligned) <= ts_length(y), "alignment does not expand the target");
    TEST_ASSERT_TRUE(ts_length(x_aligned) == ts_length(y_aligned), "aligned exogenous matches target");

    future_start = ts_end_datetime(y_aligned);
    future_end = ts_end_datetime(x);
    TEST_ASSERT_NOT_NULL(future_start);
    TEST_ASSERT_NOT_NULL(future_end);
    datetime_add_months(future_start, 1);
    datetime_init_ymd(future_start, datetime_year(future_start), datetime_month(future_start),
                      (uint8_t)datetime_days_in_month(datetime_year(future_start), datetime_month(future_start)));

    x_future = ts_slice_date(x, future_start, future_end);
    TEST_ASSERT_NOT_NULL(x_future);
    TEST_ASSERT_TRUE(ts_length(x_future) >= 6u, "future exogenous rows available");

    cols[0] = ts_head(x_future, 6u);
    TEST_ASSERT_NOT_NULL(cols[0]);
    future_x = ts_bind_columns(cols, 1u, TS_JOIN_INNER);
    TEST_ASSERT_NOT_NULL(future_x);
    TEST_ASSERT_TRUE(mat_get_row_count(future_x) == 6u, "future exogenous matrix rows");
    TEST_ASSERT_TRUE(mat_get_col_count(future_x) == 1u, "future exogenous matrix cols");

    mat_free(future_x);
    ts_free(cols[0]);
    ts_free(x_future);
    datetime_dealloc(future_start);
    datetime_dealloc(future_end);
    ts_free(x_aligned);
    ts_free(y_aligned);
    ts_free(x);
    ts_free(y);
}

static void test_arimax_with_moving_average_terms(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "18-24 LD No Health contrib Tot.",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    const char *cols[] = { "18", "19", "21" };
    matrix_t *x = test_aligned_xreg_from_columns(y,
                                                 "sample_data/Monthly Population.csv",
                                                 "DATE", cols, 3u);
    ts_arima_spec_t spec = {0};
    ts_arima_result_t fit = {0};
    ts_forecast_t fc = {0};
    matrix_t *future_x = NULL;
    number_t level = num_create_from_double(0.95);
    number_t first_forecast = NUM_ZERO;
    number_t second_stderr = NUM_ZERO;

    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(x);
    spec.p = 1u;
    spec.d = 1u;
    spec.q = 1u;
    spec.P = 1u;
    spec.D = 0u;
    spec.Q = 1u;
    spec.season_period = 12u;
    spec.include_intercept = true;
    spec.include_mean = true;
    spec.estimation = TS_EST_CSS;

    TEST_ASSERT_TRUE(ts_arima_fit(y, x, &spec, NULL, &fit) == 0, "arimax fit with MA terms");
    TEST_ASSERT_NOT_NULL(fit.ma_params);
    TEST_ASSERT_TRUE(mat_get_row_count(fit.ma_params) == 1u, "nonseasonal MA parameter populated");
    TEST_ASSERT_NOT_NULL(fit.seasonal_ar_params);
    TEST_ASSERT_TRUE(mat_get_row_count(fit.seasonal_ar_params) == 1u, "seasonal AR parameter populated");
    TEST_ASSERT_NOT_NULL(fit.seasonal_ma_params);
    TEST_ASSERT_TRUE(mat_get_row_count(fit.seasonal_ma_params) == 1u, "seasonal MA parameter populated");

    future_x = test_submatrix_rows_local(x, mat_get_row_count(x) - 3u, 3u);
    TEST_ASSERT_NOT_NULL(future_x);
    TEST_ASSERT_TRUE(ts_arima_forecast(&fit, y, future_x, 3u, level, &fc) == 0,
                     "arimax forecast with MA terms");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 0u, &first_forecast) == 0, "first arimax forecast available");
    TEST_ASSERT_TRUE(ts_get_value(fc.stderr, 1u, &second_stderr) == 0, "second arimax stderr available");
    TEST_ASSERT_TRUE(isfinite(num_to_double(first_forecast)), "arimax first forecast finite");
    TEST_ASSERT_TRUE(isfinite(num_to_double(second_stderr)), "arimax stderr finite");
    TEST_ASSERT_TRUE(num_to_double(second_stderr) >= 0.0, "arimax stderr nonnegative");

    num_destroy(&first_forecast);
    num_destroy(&second_stderr);
    num_destroy(&level);
    mat_free(future_x);
    ts_forecast_clear(&fc);
    ts_arima_result_clear(&fit);
    mat_free(x);
    ts_free(y);
}

static void test_arimax_differences_exogenous_regressors(void)
{
    number_t y_vals[8];
    double x_vals[8] = {10.0, 13.0, 17.0, 18.0, 24.0, 25.0, 28.0, 34.0};
    double y_raw[8] = {100.0, 106.0, 114.0, 116.0, 128.0, 130.0, 136.0, 148.0};
    double future_x_raw[2] = {39.0, 45.0};
    datetime_t *start = datetime_init_ymd(datetime_alloc(), 2020, DT_January, 31u);
    timeseries_t *y = NULL;
    matrix_t *x = NULL;
    matrix_t *future_x = NULL;
    ts_arima_spec_t spec = {0};
    ts_arima_result_t fit = {0};
    ts_forecast_t fc = {0};
    number_t level = num_create_from_double(0.95);
    number_t beta = NUM_ZERO;
    number_t first_forecast = NUM_ZERO;
    number_t second_forecast = NUM_ZERO;
    size_t i;

    TEST_ASSERT_NOT_NULL(start);
    for (i = 0u; i < 8u; ++i)
        y_vals[i] = num_create_from_double(y_raw[i]);
    y = ts_new_regular(y_vals, 8u, start, TS_FREQ_MONTHLY, TS_YEAR_CALENDAR);
    for (i = 0u; i < 8u; ++i)
        num_destroy(&y_vals[i]);
    datetime_dealloc(start);

    x = mat_new(8u, 1u);
    future_x = mat_new(2u, 1u);
    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(future_x);
    for (i = 0u; i < 8u; ++i) {
        number_t v = num_create_from_double(x_vals[i]);
        mat_set(x, i, 0u, &v);
        num_destroy(&v);
    }
    for (i = 0u; i < 2u; ++i) {
        number_t v = num_create_from_double(future_x_raw[i]);
        mat_set(future_x, i, 0u, &v);
        num_destroy(&v);
    }

    spec.d = 1u;
    spec.estimation = TS_EST_CSS;
    TEST_ASSERT_TRUE(ts_arima_fit(y, x, &spec, NULL, &fit) == 0,
                     "arimax fit transforms exogenous regressors with differenced target");
    TEST_ASSERT_NOT_NULL(fit.xreg_params);
    beta = mat_get_num(fit.xreg_params, 0u, 0u);
    TEST_ASSERT_TRUE(fabs(num_to_double(beta) - 2.0) < 1e-6,
                     "differenced target is fitted against differenced exogenous values");

    TEST_ASSERT_TRUE(ts_arima_forecast(&fit, y, future_x, 2u, level, &fc) == 0,
                     "arimax forecast transforms future exogenous regressors across history boundary");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 0u, &first_forecast) == 0,
                     "first differenced-xreg forecast available");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 1u, &second_forecast) == 0,
                     "second differenced-xreg forecast available");
    TEST_ASSERT_TRUE(fabs(num_to_double(first_forecast) - 158.0) < 1e-6,
                     "first future raw xreg value is converted to first difference");
    TEST_ASSERT_TRUE(fabs(num_to_double(second_forecast) - 170.0) < 1e-6,
                     "second future raw xreg value is converted to second future difference");

    num_destroy(&beta);
    num_destroy(&first_forecast);
    num_destroy(&second_forecast);
    num_destroy(&level);
    ts_forecast_clear(&fc);
    ts_arima_result_clear(&fit);
    mat_free(future_x);
    mat_free(x);
    ts_free(y);
}

static void test_auto_arima_preserves_selected_model_and_scale(void)
{
    timeseries_t *y = ts_from_csv("sample_data/Monthly Target Numbers.csv",
                                  "", "65+ LD Supp Liv",
                                  TS_FREQ_MONTHLY, TS_YEAR_FISCAL_UK_APR,
                                  TS_MISSING_DROP);
    const char *cols[] = { "20", "21", "22", "23", "24" };
    matrix_t *x = test_aligned_xreg_from_columns(y,
                                                 "sample_data/Monthly Population.csv",
                                                 "DATE", cols, 5u);
    ts_arima_spec_t best_spec = {0};
    ts_arima_result_t fit = {0};
    ts_forecast_t fc = {0};
    matrix_t *future_x = NULL;
    number_t level = num_create_from_double(0.95);
    number_t last_actual = NUM_ZERO;
    number_t first_forecast = NUM_ZERO;
    char *summary = NULL;
    double last_value;
    double first_value;

    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_TRUE(ts_auto_arima(y, x,
                                   1u, 1u, 0u,
                                   1u, 0u, 0u,
                                   12u,
                                   TS_IC_AIC,
                                   NULL, &best_spec, &fit) == 0,
                     "auto arima fit with exogenous search");
    summary = ts_arima_summary_to_string(&fit);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_TRUE(strstr(summary, "Model: (") != NULL,
                     "auto arima summary keeps chosen model metadata");
    TEST_ASSERT_TRUE(strstr(summary, "stderr =") != NULL && strstr(summary, "p =") != NULL,
                     "auto arima summary includes coefficient quality details for exogenous drivers");

    future_x = test_submatrix_rows_local(x, mat_get_row_count(x) - 12u, 12u);
    TEST_ASSERT_NOT_NULL(future_x);
    TEST_ASSERT_TRUE(ts_arima_forecast(&fit, y, future_x, 12u, level, &fc) == 0,
                     "auto arima forecast");
    TEST_ASSERT_TRUE(ts_get_value(y, ts_length(y) - 1u, &last_actual) == 0, "last actual available");
    TEST_ASSERT_TRUE(ts_get_value(fc.mean, 0u, &first_forecast) == 0, "first auto forecast available");
    last_value = num_to_double(last_actual);
    first_value = num_to_double(first_forecast);
    TEST_ASSERT_TRUE(isfinite(first_value), "auto arima future forecast finite");
    TEST_ASSERT_TRUE(fabs(first_value - last_value) < 100.0,
                     "auto arima future forecast stays on the historical scale");

    free(summary);
    num_destroy(&last_actual);
    num_destroy(&first_forecast);
    num_destroy(&level);
    mat_free(future_x);
    ts_forecast_clear(&fc);
    ts_arima_result_clear(&fit);
    mat_free(x);
    ts_free(y);
}

void run_timeseries_core_tests(void)
{
    TEST_RUN_CASE(test_csv_load_and_slice, NULL);
    TEST_RUN_CASE(test_transforms_and_aggregation, NULL);
}

void run_timeseries_output_tests(void)
{
    TEST_RUN_CASE(test_output_and_write_file, NULL);
}

void run_timeseries_model_tests(void)
{
    TEST_RUN_CASE(test_regression_and_forecast, NULL);
    TEST_RUN_CASE(test_arima_smoke, NULL);
    TEST_RUN_CASE(test_arimax_with_moving_average_terms, NULL);
    TEST_RUN_CASE(test_arimax_differences_exogenous_regressors, NULL);
    TEST_RUN_CASE(test_auto_arima_preserves_selected_model_and_scale, NULL);
    TEST_RUN_CASE(test_exogenous_alignment_for_future_forecasts, NULL);
}

int tests_main(void)
{
    TEST_SECTION("Core");
    run_timeseries_core_tests();

    TEST_SECTION("Output");
    run_timeseries_output_tests();

    TEST_SECTION("Models");
    run_timeseries_model_tests();

    return TEST_EXIT_CODE();
}
