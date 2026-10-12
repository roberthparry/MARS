/**
 * @file test_computation_module_readme.c
 * @brief Executable module-guide examples for optional computation scopes.
 *
 * These complete programs mirror each module guide and run after ordinary tests.
 * The Almanac example requires the same configured data as its normal test suite.
 */
#include <stdio.h>

#include "computation-t.h"
#include "csv-t.h"
#include "sqlite-t.h"
#include "equation-t.h"
#include "matrix-t.h"
#include "diffequation-t.h"
#include "timeseries-t.h"
#include "almanac-t.h"
#include "test_harness.h"
#include "test_computation_support.h"

/* README example: equation guide, optional computation control. */
typedef struct {
    equation_t *input;
    equation_solutions_t *result;
} equation_example_job_t;

static bool equation_example_calculate(void *argument)
{
    equation_example_job_t *job = argument;
    job->result = equ_derive_solutions(job->input);
    return job->result != NULL;
}

static int equation_example_main(void)
{
    equation_example_job_t job = {.input = equ_from_string("x^5 - x - 1 = 0")};
    if (!job.input)
        return 1;
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, equation_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    equ_solutions_free(job.result);
    equ_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_equation(void)
{
    TEST_ASSERT_TRUE(equation_example_main() == 0, "README controlled equation output");
}

/* README example: matrix guide, optional computation control. */
typedef struct {
    matrix_t *input;
    mat_qr_factor_t result;
} matrix_example_job_t;

static bool matrix_example_calculate(void *argument)
{
    matrix_example_job_t *job = argument;
    return mat_qr_factor(job->input, &job->result) == 0;
}

static int matrix_example_main(void)
{
    matrix_example_job_t job = {.input = mat_new(3, 3)};
    if (!job.input)
        return 1;
    for (size_t i = 0; i < 3; ++i)
        mat_set(job.input, i, i, &NUM_ONE);
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, matrix_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    mat_qr_factor_free(&job.result);
    mat_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_matrix(void)
{
    TEST_ASSERT_TRUE(matrix_example_main() == 0, "README controlled matrix output");
}

/* README example: diffequation guide, optional computation control. */
typedef struct {
    diffequ_t *input;
    diffequ_solve_result_t *result;
} diffequation_example_job_t;

static bool diffequation_example_calculate(void *argument)
{
    diffequation_example_job_t *job = argument;
    job->result = de_solve(job->input);
    return job->result != NULL;
}

static int diffequation_example_main(void)
{
    diffequation_example_job_t job = {.input = de_from_string("y\' = y")};
    if (!job.input)
        return 1;
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, diffequation_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    de_solve_result_free(job.result);
    de_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_diffequation(void)
{
    TEST_ASSERT_TRUE(diffequation_example_main() == 0, "README controlled diffequation output");
}

/* README example: timeseries guide, optional computation control. */
typedef struct {
    timeseries_t *input;
    ts_arima_spec_t spec;
    ts_arima_result_t result;
} timeseries_example_job_t;

static bool timeseries_example_calculate(void *argument)
{
    timeseries_example_job_t *job = argument;
    return ts_auto_arima(job->input, NULL, 1, 0, 0, 0, 0, 0, 0,
                         TS_IC_AIC, NULL, &job->spec, &job->result) == 0;
}

static int timeseries_example_main(void)
{
    const double values[] = {2, 4, 3, 7, 5, 9, 8, 11, 10, 13, 12, 15};
    timeseries_example_job_t job = {.input = ts_new_from_doubles(values, 12)};
    if (!job.input)
        return 1;
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, timeseries_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    ts_arima_result_clear(&job.result);
    ts_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_timeseries(void)
{
    TEST_ASSERT_TRUE(timeseries_example_main() == 0, "README controlled timeseries output");
}

/* README example: csv guide, optional computation control. */
typedef struct {
    csv_t *input;
} csv_example_job_t;

static bool csv_example_calculate(void *argument)
{
    csv_example_job_t *job = argument;
    csv_status_t status;
    while ((status = csv_next(job->input)) == CSV_ROW) {
        /* Consume only complete records here. */
    }
    return status == CSV_END;
}

static int csv_example_main(void)
{
    csv_example_job_t job = {.input = csv_reader_new(NULL, NULL)};
    string_t *text = string_new_with("one,two\nthree,four\n");
    if (!job.input || !text || !csv_feed(job.input, text, true)) {
        csv_free(job.input);
        string_free(text);
        return 1;
    }
    string_free(text);
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, csv_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    csv_free(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_csv(void)
{
    TEST_ASSERT_TRUE(csv_example_main() == 0, "README controlled csv output");
}

/* README example: almanac guide, optional computation control. */
typedef struct {
    almanac_t *input;
    datetime_t *after;
    almanac_moon_phase_event_t result;
} almanac_example_job_t;

static bool almanac_example_calculate(void *argument)
{
    almanac_example_job_t *job = argument;
    return almanac_next_moon_phase_exact(job->input, job->after, ALMANAC_MOON_PHASE_NEW, &job->result);
}

static int almanac_example_main(void)
{
    /* Requires the configured Almanac database, as do ordinary event searches. */
    almanac_example_job_t job = {.input = almanac_open(), .after = datetime_alloc()};
    if (!job.input || !job.after || !datetime_init_jd(job.after, 2461041.5)) {
        almanac_close(job.input);
        datetime_dealloc(job.after);
        return 1;
    }
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, almanac_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    datetime_dealloc(job.after);
    almanac_close(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_almanac(void)
{
    TEST_ASSERT_TRUE(almanac_example_main() == 0, "README controlled almanac output");
}

/* README example: sqlite guide, optional computation control. */
typedef struct {
    sqlite_t *input;
    csv_t *reader;
    string_t *table;
    sqlite_csv_report_t report;
} sqlite_example_job_t;

static bool sqlite_example_calculate(void *argument)
{
    sqlite_example_job_t *job = argument;
    sqlite_csv_column_t column = {.type = SQLITE_CSV_INTEGER};
    sqlite_csv_options_t options = sqlite_csv_options_default();
    options.columns = &column;
    options.column_count = 1;
    return sqlite_import_csv_reader(job->input, job->reader, job->table, &options, NULL, NULL, &job->report);
}

static int sqlite_example_main(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    csv_options_t csv_options = csv_options_default();
    csv_options.first_row_header = true;
    sqlite_example_job_t job = {.input = sqlite_open_encrypted(path, key),
                 .reader = csv_reader_new(NULL, &csv_options), .table = string_new_with("items")};
    string_t *text = string_new_with("n\n1\n2\n3\n");
    string_free(path);
    string_free(key);
    if (!job.input || !job.reader || !job.table || !text || !csv_feed(job.reader, text, true)) {
        csv_free(job.reader);
        string_free(job.table);
        sqlite_close(job.input);
        string_free(text);
        return 1;
    }
    string_free(text);
    computation_options_t options = {.work_limit = 4};
    computation_t *control = computation_new(&options);
    bool ok = control && computation_run(control, sqlite_example_calculate, &job);
    bool stopped = !ok && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Stopped: %d\n", stopped);
    csv_free(job.reader);
    string_free(job.table);
    sqlite_close(job.input);
    computation_free(control);
    return stopped ? 0 : 1;
}

static void example_computation_sqlite(void)
{
    TEST_ASSERT_TRUE(sqlite_example_main() == 0, "README controlled sqlite output");
}

/* Register complete guide programs in the final output lane. */
void test_computation_module_readme(void)
{
    TEST_RUN_OUTPUT(example_computation_equation);
    TEST_RUN_OUTPUT(example_computation_matrix);
    TEST_RUN_OUTPUT(example_computation_diffequation);
    TEST_RUN_OUTPUT(example_computation_timeseries);
    TEST_RUN_OUTPUT(example_computation_csv);
    TEST_RUN_OUTPUT(example_computation_almanac);
    TEST_RUN_OUTPUT(example_computation_sqlite);
}
