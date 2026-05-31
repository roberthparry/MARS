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
    TEST_ASSERT_TRUE(ts_arima_forecast(&fit, y, NULL, 3u, level, &fc) == 0,
                     "arima forecast");
    TEST_ASSERT_NOT_NULL(fc.mean);
    TEST_ASSERT_TRUE(ts_length(fc.mean) == 3u, "arima horizon");

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
