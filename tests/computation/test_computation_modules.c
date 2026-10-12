/**
 * @file test_computation_modules.c
 * @brief Cooperative cancellation and result ownership across native module boundaries.
 *
 * Tests stage-triggered cancellation and budget exhaustion without worker processes.
 * Reuses inputs afterwards to detect damaged bindings, factors or model state.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    computation_t *control;
    const char *stage;
    unsigned hits;
    unsigned stop_after;
} module_observer_t;

static void module_observe(const computation_progress_t *progress, void *argument)
{
    module_observer_t *observer = argument;
    if (strcmp(progress->stage, observer->stage) == 0 && ++observer->hits >= observer->stop_after)
        computation_cancel(observer->control);
}

static computation_t *module_control(module_observer_t *observer, const char *stage, unsigned after)
{
    *observer = (module_observer_t){.stage = stage, .stop_after = after};
    computation_options_t options = {.progress = module_observe, .argument = observer};
    observer->control = computation_new(&options);
    if (!observer->control)
        abort();
    return observer->control;
}

typedef struct {
    equation_t *equation;
    equation_solutions_t *result;
} equation_job_t;

static bool module_equation(void *argument)
{
    equation_job_t *job = argument;
    job->result = equ_derive_solutions(job->equation);
    return job->result != NULL;
}

static void test_computation_equation_stop(void)
{
    equation_job_t job = {.equation = equ_from_string("x^5 - x - 1 = 0")};
    TEST_ASSERT_NOT_NULL(job.equation);
    for (uint64_t budget = 1; budget <= 24; ++budget) {
        computation_options_t options = {.work_limit = budget};
        computation_t *control = computation_new(&options);
        TEST_ASSERT_TRUE(!computation_run(control, module_equation, &job), "bounded solving stops");
        TEST_ASSERT_TRUE(!job.result && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                         "no partial root set escapes");
        computation_free(control);
    }
    module_observer_t observer;
    computation_t *control = module_control(&observer, "equation.newton", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_equation, &job) && !job.result && observer.hits == 2,
                     "Newton cancellation stops refinement and prevents fallback");
    computation_free(control);
    TEST_ASSERT_TRUE(module_equation(&job), "same equation solves outside a scope");
    equ_solutions_free(job.result);
    equ_free(job.equation);
}

typedef struct {
    matrix_t *input;
    mat_qr_factor_t result;
} matrix_job_t;

static bool module_matrix(void *argument)
{
    matrix_job_t *job = argument;
    return mat_qr_factor(job->input, &job->result) == 0;
}

static void test_computation_matrix_stop(void)
{
    matrix_job_t job = {.input = mat_new(3, 3)};
    TEST_ASSERT_NOT_NULL(job.input);
    for (size_t i = 0; i < 3; ++i)
        mat_set(job.input, i, i, &NUM_ONE);
    module_observer_t observer;
    computation_t *control = module_control(&observer, "matrix.qr", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_matrix, &job), "stop inside factorisation");
    TEST_ASSERT_TRUE(!job.result.Q && !job.result.R && observer.hits == 2,
                     "incomplete factors are discarded");
    TEST_ASSERT_TRUE(module_matrix(&job), "input remains reusable");
    mat_qr_factor_free(&job.result);
    mat_free(job.input);
    computation_free(control);
}

typedef struct {
    diffequ_t *equation;
    diffequ_solve_result_t *result;
} diffequation_job_t;

static bool module_diffequation(void *argument)
{
    diffequation_job_t *job = argument;
    job->result = de_solve(job->equation);
    return job->result != NULL;
}

static void test_computation_diffequation_stop(void)
{
    diffequation_job_t job = {.equation = de_from_string("y' = y")};
    TEST_ASSERT_NOT_NULL(job.equation);
    for (uint64_t budget = 1; budget <= 12; ++budget) {
        computation_options_t options = {.work_limit = budget};
        computation_t *control = computation_new(&options);
        TEST_ASSERT_TRUE(!computation_run(control, module_diffequation, &job), "nested symbolic work stops");
        TEST_ASSERT_TRUE(!job.result && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                         "no stopped solution is published");
        computation_free(control);
    }
    TEST_ASSERT_TRUE(module_diffequation(&job), "differential equation remains reusable");
    de_solve_result_free(job.result);
    de_free(job.equation);
}

typedef struct {
    timeseries_t *series;
    ts_arima_spec_t spec;
    ts_arima_result_t result;
} timeseries_job_t;

static bool module_timeseries(void *argument)
{
    timeseries_job_t *job = argument;
    return ts_auto_arima(job->series, NULL, 1, 0, 0, 0, 0, 0, 0,
                         TS_IC_AIC, NULL, &job->spec, &job->result) == 0;
}

static void test_computation_timeseries_stop(void)
{
    const double values[] = {2, 4, 3, 7, 5, 9, 8, 11, 10, 13, 12, 15};
    timeseries_job_t job = {.series = ts_new_from_doubles(values, 12)};
    TEST_ASSERT_NOT_NULL(job.series);
    module_observer_t observer;
    computation_t *control = module_control(&observer, "timeseries.candidate", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_timeseries, &job), "stop after an earlier candidate fitted");
    TEST_ASSERT_TRUE(!job.result.fitted && !job.result.xreg_params && observer.hits == 2,
                     "best-so-far is not mistaken for a complete search");
    TEST_ASSERT_TRUE(module_timeseries(&job), "search succeeds with the unchanged series");
    ts_arima_result_clear(&job.result);
    computation_free(control);
    ts_free(job.series);
}

typedef struct {
    timeseries_t *series;
    ts_arima_result_t model;
    ts_forecast_t result;
} forecast_job_t;

static bool module_forecast(void *argument)
{
    forecast_job_t *job = argument;
    number_t level = num_create_from_double(0.95);
    int status = ts_arima_forecast(&job->model, job->series, NULL, 8u, level, &job->result);
    num_destroy(&level);
    return status == 0;
}

static void test_computation_forecast_uncertainty_stop(void)
{
    const double values[] = {2, 4, 3, 7, 5, 9, 8, 11, 10, 13, 12, 15};
    const char *stages[] = {"timeseries.impulse", "timeseries.uncertainty", "timeseries.reintegrate"};
    datetime_t *start = datetime_alloc();
    TEST_ASSERT_TRUE(start && datetime_init_jd(start, 2461041.5), "dated history start");
    forecast_job_t job = {.series = ts_new_regular_from_doubles(values, 12, start, TS_FREQ_DAILY, TS_YEAR_CALENDAR)};
    datetime_dealloc(start);
    TEST_ASSERT_NOT_NULL(job.series);
    for (size_t kind = 0; kind < 3; ++kind) {
        ts_arima_spec_t spec = {.p = 1, .d = kind == 1, .D = kind == 2, .season_period = 2,
                                .include_intercept = true, .include_mean = true};
        TEST_ASSERT_TRUE(ts_arima_fit(job.series, NULL, &spec, NULL, &job.model) == 0, "fit before cancellation");
        TEST_ASSERT_TRUE(module_forecast(&job), "baseline forecast");
        number_t expected = NUM_ZERO;
        TEST_ASSERT_TRUE(ts_get_value(job.result.stderr, 7, &expected) == 0, "baseline final uncertainty");
        ts_forecast_clear(&job.result);
        for (size_t stage = 0; stage < (kind ? 3u : 2u); ++stage) {
            module_observer_t observer;
            computation_t *control = module_control(&observer, stages[stage], 2);
            TEST_ASSERT_TRUE(!computation_run(control, module_forecast, &job) && observer.hits == 2,
                             "cancel inside prediction uncertainty or reintegration");
            TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_CANCELLED &&
                             !job.result.mean && !job.result.stderr && !job.result.lower && !job.result.upper,
                             "interrupted prediction publishes no partial outputs");
            computation_free(control);
            TEST_ASSERT_TRUE(module_forecast(&job), "fitted model remains reusable");
            number_t actual = NUM_ZERO;
            TEST_ASSERT_TRUE(ts_get_value(job.result.stderr, 7, &actual) == 0 && num_eq(actual, expected),
                             "cancellation does not alter fitted metadata or uncertainty");
            num_destroy(&actual);
            ts_forecast_clear(&job.result);
        }
        num_destroy(&expected);
        ts_arima_result_clear(&job.model);
    }
    ts_free(job.series);
}

typedef struct {
    csv_t *reader;
    csv_status_t status;
} csv_job_t;

static bool module_csv(void *argument)
{
    csv_job_t *job = argument;
    while ((job->status = csv_next(job->reader)) == CSV_ROW) {}
    return job->status == CSV_END;
}

static void test_computation_csv_stop(void)
{
    csv_job_t job = {.reader = csv_reader_new(NULL, NULL)};
    string_t *input = string_new_with("one,two\nthree,four\nfive,six\n");
    TEST_ASSERT_TRUE(job.reader && input && csv_feed(job.reader, input, true), "fed reader");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "csv.read", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_csv, &job), "stop between records");
    TEST_ASSERT_TRUE(job.status == CSV_ERROR && csv_next(job.reader) == CSV_ERROR,
                     "reader retains a sticky stop, not a partial next record");
    csv_free(job.reader);
    string_free(input);
    computation_free(control);
}

typedef struct {
    sqlite_t *db;
    csv_t *reader;
    string_t *table;
    sqlite_csv_report_t report;
} sqlite_job_t;

static bool module_sqlite(void *argument)
{
    sqlite_job_t *job = argument;
    sqlite_csv_column_t column = {.type = SQLITE_CSV_INTEGER};
    sqlite_csv_options_t options = sqlite_csv_options_default();
    options.columns = &column;
    options.column_count = 1;
    return sqlite_import_csv_reader(job->db, job->reader, job->table, &options, NULL, NULL, &job->report);
}

static void test_computation_sqlite_rollback(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("test key");
    sqlite_job_t job = {.db = sqlite_open_encrypted(path, key), .table = string_new_with("imported")};
    string_free(path);
    string_free(key);
    TEST_ASSERT_NOT_NULL(job.db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(job.db, "BEGIN; CREATE TABLE retained(n); INSERT INTO retained VALUES(42)"),
                     "outer transaction starts");
    csv_options_t options = csv_options_default();
    options.first_row_header = true;
    job.reader = csv_reader_new(NULL, &options);
    string_t *input = string_new_with("n\n1\n2\n3\n");
    TEST_ASSERT_TRUE(job.reader && csv_feed(job.reader, input, true), "input prepared");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "sqlite.csv.insert", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_sqlite, &job), "stop after first insert");
    TEST_ASSERT_TRUE(job.report.rows == 0 && job.report.code == ECANCELED, "failed import reports no committed rows");
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(job.db,
        "SELECT (SELECT count(*) FROM sqlite_master WHERE name='imported'), (SELECT n FROM retained)");
    TEST_ASSERT_TRUE(stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW, "outer transaction remains usable");
    TEST_ASSERT_TRUE(sqlite_stmt_column_int64(stmt, 0) == 0 && sqlite_stmt_column_int64(stmt, 1) == 42,
                     "import schema rolled back without losing caller data");
    sqlite_stmt_finalize(stmt);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(job.db, "COMMIT"), "outer transaction was preserved");
    csv_free(job.reader);
    string_free(input);
    string_free(job.table);
    sqlite_close(job.db);
    computation_free(control);
}


typedef struct {
    almanac_t *engine;
    datetime_t *after;
    almanac_moon_phase_event_t result;
} almanac_job_t;

static bool module_almanac(void *argument)
{
    almanac_job_t *job = argument;
    return almanac_next_moon_phase_exact(job->engine, job->after, ALMANAC_MOON_PHASE_NEW, &job->result);
}

static void test_computation_almanac_stop(void)
{
    almanac_job_t job = {.engine = almanac_open(), .after = datetime_alloc()};
    TEST_ASSERT_TRUE(job.engine && job.after && datetime_init_jd(job.after, 2461041.5), "configured engine");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "almanac.phase", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_almanac, &job) && observer.hits == 2,
                     "phase refinement observes cancellation");
    TEST_ASSERT_TRUE(module_almanac(&job), "engine and date remain reusable");
    computation_free(control);
    datetime_dealloc(job.after);
    almanac_close(job.engine);
}

typedef struct {
    matrix_t *input;
    unsigned operation;
    bool empty;
} factor_job_t;

static bool module_svd(factor_job_t *job)
{
    mat_svd_factor_t result = {0};
    int rc = mat_svd_factor(job->input, &result);
    job->empty = !result.U && !result.S && !result.V;
    mat_svd_factor_free(&result);
    return rc == 0;
}

static bool module_cholesky(factor_job_t *job)
{
    mat_cholesky_t result = {0};
    int rc = mat_cholesky(job->input, &result);
    job->empty = !result.L;
    mat_cholesky_free(&result);
    return rc == 0;
}

static bool module_schur(factor_job_t *job)
{
    mat_schur_factor_t result = {0};
    int rc = mat_schur_factor(job->input, &result);
    job->empty = !result.Q && !result.T;
    mat_schur_factor_free(&result);
    return rc == 0;
}

static bool module_eigen(factor_job_t *job)
{
    number_t values[3] = {NUM_ZERO, NUM_ZERO, NUM_ZERO};
    matrix_t *vectors = NULL;
    int rc = mat_eigendecompose(job->input, values, &vectors);
    job->empty = !vectors && num_is_zero(values[0]) && num_is_zero(values[1]) && num_is_zero(values[2]);
    for (size_t i = 0; i < 3; ++i)
        num_destroy(&values[i]);
    mat_free(vectors);
    return rc == 0;
}

static bool module_factor(void *argument)
{
    static bool (*const operations[])(factor_job_t *) = {
        module_svd, module_cholesky, module_schur, module_eigen
    };
    factor_job_t *job = argument;
    return operations[job->operation](job);
}

static void test_computation_matrix_factors(void)
{
    factor_job_t job = {.input = mat_new(3, 3)};
    TEST_ASSERT_NOT_NULL(job.input);
    const long entries[3][3] = {{4, 1, 1}, {1, 5, 1}, {1, 1, 6}};
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            number_t value = num_create_from_long(entries[i][j]);
            mat_set(job.input, i, j, &value);
            num_destroy(&value);
        }
    for (job.operation = 0; job.operation < 4; ++job.operation) {
        for (uint64_t budget = 1; budget <= 48; ++budget) {
            computation_options_t options = {.work_limit = budget};
            computation_t *control = computation_new(&options);
            bool ok = computation_run(control, module_factor, &job);
            TEST_ASSERT_TRUE(ok || (computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED && job.empty),
                             "stopped factor/eigen work leaves no published components");
            computation_free(control);
        }
        TEST_ASSERT_TRUE(module_factor(&job), "factor succeeds without control after interrupted runs");
    }
    mat_free(job.input);
}

static void test_computation_equation_bindings(void)
{
    equation_job_t job = {.equation = equ_from_string("log(x) + x = 0")};
    TEST_ASSERT_NOT_NULL(job.equation);
    expr_t *x = expr_bindings_get(equ_bindings(job.equation), "x");
    TEST_ASSERT_NOT_NULL(x);
    expr_set_val(x, NUM_TWO);
    for (uint64_t budget = 1; budget <= 32; ++budget) {
        computation_options_t options = {.work_limit = budget};
        computation_t *control = computation_new(&options);
        bool ok = computation_run(control, module_equation, &job);
        if (!ok) {
            number_t actual = expr_eval(x);
            TEST_ASSERT_TRUE(num_eq(actual, NUM_TWO), "stopped binding snapshots do not replace the seed with NaN");
            num_destroy(&actual);
        }
        equ_solutions_free(job.result);
        job.result = NULL;
        computation_free(control);
        expr_set_val(x, NUM_TWO);
    }
    equ_free(job.equation);
}


static bool module_export(void *argument)
{
    sqlite_job_t *job = argument;
    return sqlite_export_csv(job->db, job->table, job->reader, false, &job->report);
}

static void test_computation_sqlite_export_stop(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("test key");
    sqlite_job_t job = {.db = sqlite_open_encrypted(path, key),
                        .table = string_new_with("SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3"),
                        .reader = csv_writer_new(NULL, NULL)};
    TEST_ASSERT_TRUE(job.db && job.reader && job.table, "export setup");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "csv.write", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_export, &job), "stop while exporting second record");
    TEST_ASSERT_TRUE(job.report.rows == 1 && job.report.code == ECANCELED &&
                     string_view_equals_literal(string_view_all(csv_output(job.reader)), "1\r\n"),
                     "only complete earlier records remain; report counts their publication");
    computation_free(control);
    csv_free(job.reader);
    sqlite_close(job.db);
    string_free(job.table);
    string_free(path);
    string_free(key);
}

typedef struct {
    sqlite_job_t job;
    computation_t *control;
    unsigned refills;
} refill_job_t;

static bool module_refill(csv_t *reader, void *argument)
{
    refill_job_t *job = argument;
    const char *bytes = job->refills++ == 0 ? "n\n1\n" : "2\n";
    if (job->refills == 2)
        computation_cancel(job->control);
    string_t *chunk = string_new_with(bytes);
    bool ok = chunk && csv_feed(reader, chunk, job->refills == 2);
    string_free(chunk);
    return ok;
}

static bool module_import_refill(void *argument)
{
    refill_job_t *job = argument;
    sqlite_csv_column_t column = {.type = SQLITE_CSV_INTEGER};
    sqlite_csv_options_t options = sqlite_csv_options_default();
    options.columns = &column;
    options.column_count = 1;
    return sqlite_import_csv_reader(job->job.db, job->job.reader, job->job.table, &options,
                                    module_refill, job, &job->job.report);
}

static void test_computation_sqlite_callback_stop(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("test key");
    csv_options_t csv_options = csv_options_default();
    csv_options.first_row_header = true;
    refill_job_t job = {.job = {.db = sqlite_open_encrypted(path, key),
                               .reader = csv_reader_new(NULL, &csv_options),
                               .table = string_new_with("streamed")},
                        .control = computation_new(NULL)};
    TEST_ASSERT_TRUE(job.job.db && job.job.reader && job.job.table && job.control, "stream setup");
    TEST_ASSERT_TRUE(!computation_run(job.control, module_import_refill, &job), "refill may cancel its scope");
    TEST_ASSERT_TRUE(job.refills == 2 && job.job.report.rows == 0 && job.job.report.code == ECANCELED,
                     "refill cancellation is observed before the next row is inserted");
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(job.job.db, "SELECT count(*) FROM sqlite_master WHERE name='streamed'");
    TEST_ASSERT_TRUE(stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(stmt, 0) == 0,
                     "callback-driven import schema is rolled back");
    sqlite_stmt_finalize(stmt);
    computation_free(job.control);
    csv_free(job.job.reader);
    sqlite_close(job.job.db);
    string_free(job.job.table);
    string_free(path);
    string_free(key);
}

static bool module_import_file(void *argument)
{
    sqlite_job_t *job = argument;
    file_t *file = file_new_cstr(test_case_temp_path("controlled.csv"));
    const char input[] = "n\n1\n2\n3\n";
    bool opened = file && file_write_all_bytes(file, input, sizeof(input) - 1) &&
                  file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ);
    bool ok = opened && sqlite_import_csv(job->db, file, job->table, NULL, &job->report);
    int64_t position = -1;
    if (opened)
        file_tell(file, &position);
    file_close(file);
    file_delete(file);
    file_free(file);
    /* Store the cursor check without a test assertion inside the callback. */
    job->report.column = position == 0 ? 0 : 1;
    return ok;
}

static void test_computation_sqlite_file_stop(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("test key");
    sqlite_job_t job = {.db = sqlite_open_encrypted(path, key), .table = string_new_with("replayed")};
    TEST_ASSERT_TRUE(job.db && job.table, "file import setup");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "sqlite.csv.infer", 2);
    TEST_ASSERT_TRUE(!computation_run(control, module_import_file, &job), "inference stops");
    TEST_ASSERT_TRUE(job.report.rows == 0 && job.report.column == 0 && job.report.code == ECANCELED,
                     "failed file inference restores the starting cursor");
    computation_free(control);
    sqlite_close(job.db);
    string_free(job.table);
    string_free(path);
    string_free(key);
}


static void test_computation_csv_long_record(void)
{
    csv_job_t job = {.reader = csv_reader_new(NULL, NULL)};
    string_t *text = string_new();
    TEST_ASSERT_TRUE(job.reader && text, "long-record setup");
    for (size_t i = 0; i < 8192; ++i)
        TEST_ASSERT_TRUE(string_append_cstr(text, "a") == 0, "build test field");
    TEST_ASSERT_TRUE(string_append_cstr(text, "\n") == 0 && csv_feed(job.reader, text, true), "feed long field");
    module_observer_t observer;
    computation_t *control = module_control(&observer, "csv.bytes", 1);
    TEST_ASSERT_TRUE(!computation_run(control, module_csv, &job), "large records are interruptible");
    TEST_ASSERT_TRUE(observer.hits == 1 && job.status == CSV_ERROR && csv_record_count(job.reader) == 0,
                     "no partial record is published");
    computation_free(control);
    csv_free(job.reader);
    string_free(text);
}

/* Register module-specific cases before the suite's README output lane. */
void test_computation_module_cases(void)
{
    TEST_RUN_CASE(test_computation_equation_stop, NULL);
    TEST_RUN_CASE(test_computation_equation_bindings, NULL);
    TEST_RUN_CASE(test_computation_matrix_factors, NULL);
    TEST_RUN_CASE(test_computation_almanac_stop, NULL);
    TEST_RUN_CASE(test_computation_matrix_stop, NULL);
    TEST_RUN_CASE(test_computation_diffequation_stop, NULL);
    TEST_RUN_CASE(test_computation_timeseries_stop, NULL);
    TEST_RUN_CASE(test_computation_forecast_uncertainty_stop, NULL);
    TEST_RUN_CASE(test_computation_csv_stop, NULL);
    TEST_RUN_CASE(test_computation_csv_long_record, NULL);
    TEST_RUN_CASE(test_computation_sqlite_rollback, NULL);
    TEST_RUN_CASE(test_computation_sqlite_export_stop, NULL);
    TEST_RUN_CASE(test_computation_sqlite_callback_stop, NULL);
    TEST_RUN_CASE(test_computation_sqlite_file_stop, NULL);
}
