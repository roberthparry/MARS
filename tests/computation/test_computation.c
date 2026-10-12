/**
 * @file test_computation.c
 * @brief Shared budget, lifecycle and cooperative integrator control regressions.
 *
 * Uses native thread and lock APIs to test concurrent owners within sequential
 * test cases. Covers sticky stop reasons, monotonic deadlines, re-entrant
 * cancellation from progress observers and nested numerical work. README
 * examples exercise the public interface and run after ordinary tests.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "computation-t.h"
#include "thread-t.h"
#include "integrator-t.h"
#include "test_harness.h"
#include "test_computation_support.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void computation_require(bool ok)
{
    if (!ok)
        abort();
}

static void test_computation_lifecycle(void)
{
    computation_t *control = computation_new(NULL);
    computation_require(control != NULL);
    computation_t *owner = computation_retain(control);
    computation_free(control);
    TEST_ASSERT_TRUE(computation_checkpoint(owner, 10), "retained owner outlives original reference");
    TEST_ASSERT_TRUE(computation_work_used(owner) == 10, "shared work retained");
    TEST_ASSERT_TRUE(computation_complete(owner), "top-level owner completes");
    computation_cancel(owner);
    computation_fail(owner);
    TEST_ASSERT_TRUE(computation_status(owner) == COMPUTATION_COMPLETED, "first terminal status wins");
    TEST_ASSERT_TRUE(!computation_checkpoint(owner, 0) && errno == EALREADY, "completion prevents more work");
    TEST_ASSERT_TRUE(!computation_complete(owner) && errno == EALREADY, "completion is one transition");
    computation_free(owner);
    control = computation_new(NULL);
    computation_require(control != NULL);
    computation_fail(control);
    computation_cancel(control);
    TEST_ASSERT_TRUE(!computation_checkpoint(control, 1) && errno == EIO, "mathematical failure stays distinct");
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_FAILED, "failure preserved");
    computation_free(control);
}

static void test_computation_budget(void)
{
    computation_options_t options = {.work_limit = 3};
    computation_t *control = computation_new(&options);
    computation_require(control != NULL);
    options.work_limit = 100;
    TEST_ASSERT_TRUE(computation_checkpoint(control, 3), "last permitted unit is available");
    TEST_ASSERT_TRUE(computation_checkpoint(control, 0), "polling at the limit is allowed");
    TEST_ASSERT_TRUE(!computation_checkpoint(control, 2) && errno == E2BIG, "oversized reservation stops atomically");
    TEST_ASSERT_TRUE(computation_work_used(control) == 3, "failed reservations do not consume work");
    computation_cancel(control);
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED, "budget reason is sticky");
    computation_free(control);
    control = computation_new(NULL);
    computation_require(control != NULL);
    TEST_ASSERT_TRUE(computation_checkpoint(control, UINT64_MAX) && computation_checkpoint(control, 1),
                     "unlimited work saturates instead of wrapping");
    TEST_ASSERT_TRUE(computation_work_used(control) == UINT64_MAX, "saturated counter");
    computation_free(control);
    options.work_limit = UINT64_MAX;
    control = computation_new(&options);
    computation_require(control != NULL);
    TEST_ASSERT_TRUE(computation_checkpoint(control, UINT64_MAX), "largest finite budget is representable");
    TEST_ASSERT_TRUE(!computation_checkpoint(control, 1) && errno == E2BIG, "largest finite budget does not wrap");
    computation_free(control);
}

static void test_computation_invalid(void)
{
    computation_options_t options = {.timeout_ms = UINT64_MAX};
    TEST_ASSERT_TRUE(!computation_new(&options) && errno == EOVERFLOW, "reject overflowing deadline");
    TEST_ASSERT_TRUE(!computation_retain(NULL) && errno == EINVAL, "missing retain");
    TEST_ASSERT_TRUE(!computation_checkpoint(NULL, 1) && errno == EINVAL, "missing checkpoint");
    TEST_ASSERT_TRUE(!computation_complete(NULL) && errno == EINVAL, "missing completion");
    TEST_ASSERT_TRUE(computation_status(NULL) == COMPUTATION_INVALID, "missing status");
    TEST_ASSERT_TRUE(computation_work_used(NULL) == 0 && errno == EINVAL, "missing counter");
    TEST_ASSERT_TRUE(!computation_report(NULL, "stage", 0, 0) && errno == EINVAL, "missing reporter");
    TEST_ASSERT_TRUE(!intg_set_computation(NULL, NULL) && errno == EINVAL, "missing integrator");
    computation_free(NULL);
    computation_cancel(NULL);
    computation_fail(NULL);
}

static void test_computation_deadline(void)
{
    computation_options_t options = {.timeout_ms = 1};
    computation_t *control = computation_new(&options);
    lock_t *delay = lock_new(LOCK_EVENT, 0);
    computation_require(control && delay);
    computation_require(!lock_wait_for(delay, 5) && errno == ETIMEDOUT);
    TEST_ASSERT_TRUE(!computation_checkpoint(control, 1) && errno == ETIMEDOUT, "deadline precedes reservation");
    TEST_ASSERT_TRUE(computation_work_used(control) == 0, "expired work is not reserved");
    TEST_ASSERT_TRUE(!computation_complete(control) && errno == ETIMEDOUT, "deadline prevents success");
    computation_cancel(control);
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_TIMED_OUT, "deadline reason preserved");
    computation_free(control);
    computation_require(lock_free(delay));
}

static void *computation_count_worker(void *argument)
{
    computation_t *control = argument;
    while (computation_checkpoint(control, 1)) {
    }
    computation_free(control);
    return NULL;
}

static void test_computation_concurrent_budget(void)
{
    computation_options_t options = {.work_limit = 10000};
    computation_t *control = computation_new(&options);
    computation_require(control != NULL);
    thread_t *workers[2];
    for (size_t i = 0; i < 2; ++i) {
        computation_t *owner = computation_retain(control);
        computation_require(owner != NULL);
        workers[i] = thread_new(computation_count_worker, owner);
        computation_require(workers[i] != NULL);
    }
    for (size_t i = 0; i < 2; ++i)
        computation_require(thread_join(workers[i], NULL) && thread_free(workers[i]));
    TEST_ASSERT_TRUE(computation_work_used(control) == options.work_limit, "concurrent reservations never exceed limit");
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED, "shared budget stops both workers");
    computation_free(control);
}

typedef struct {
    computation_t *control;
    lock_t *ready;
    lock_t *proceed;
    bool stopped;
} computation_worker_t;

static void *computation_cancel_worker(void *argument)
{
    computation_worker_t *worker = argument;
    computation_require(lock_signal(worker->ready) && lock_wait_for(worker->proceed, 10000));
    worker->stopped = !computation_checkpoint(worker->control, 1) && errno == ECANCELED;
    computation_free(worker->control);
    computation_require(lock_free(worker->ready) && lock_free(worker->proceed));
    return NULL;
}

static void test_computation_thread_cancel(void)
{
    computation_t *control = computation_new(NULL);
    lock_t *ready = lock_new(LOCK_EVENT, 0), *proceed = lock_new(LOCK_EVENT, 0);
    computation_require(control && ready && proceed);
    computation_worker_t worker = {computation_retain(control), lock_clone(ready), lock_clone(proceed), false};
    computation_require(worker.control && worker.ready && worker.proceed);
    thread_t *thread = thread_new(computation_cancel_worker, &worker);
    computation_require(thread && lock_wait_for(ready, 10000));
    computation_cancel(control);
    computation_free(control);
    computation_require(lock_signal(proceed) && thread_join(thread, NULL) && thread_free(thread));
    TEST_ASSERT_TRUE(worker.stopped, "cross-thread cancellation survives releasing the original owner");
    computation_require(lock_free(ready) && lock_free(proceed));
}

typedef struct {
    computation_t *control;
    const char *cancel_stage;
    uint64_t last_work;
    unsigned calls;
    bool monotonic;
} computation_observer_t;

static void computation_observe(const computation_progress_t *progress, void *argument)
{
    computation_observer_t *observer = argument;
    observer->monotonic = observer->monotonic && progress->work_used >= observer->last_work;
    observer->last_work = progress->work_used;
    ++observer->calls;
    if (observer->cancel_stage && strcmp(progress->stage, observer->cancel_stage) == 0)
        computation_cancel(observer->control);
}

static void test_computation_progress(void)
{
    computation_observer_t observer = {.cancel_stage = "stop", .monotonic = true};
    computation_options_t options = {.progress = computation_observe, .argument = &observer};
    observer.control = computation_new(&options);
    computation_require(observer.control != NULL);
    TEST_ASSERT_TRUE(!computation_report(observer.control, NULL, 0, 0) && errno == EINVAL, "missing stage rejected");
    TEST_ASSERT_TRUE(!computation_report(observer.control, "bad", 2, 1) && errno == EINVAL, "invalid count rejected");
    TEST_ASSERT_TRUE(observer.calls == 0, "invalid reports do not call observer");
    TEST_ASSERT_TRUE(computation_checkpoint(observer.control, 7), "reserve reportable work");
    TEST_ASSERT_TRUE(computation_report(observer.control, "work", 1, 2), "known stage total");
    TEST_ASSERT_TRUE(!computation_report(observer.control, "stop", 4, 0) && errno == ECANCELED,
                     "observer may cancel with unknown total");
    TEST_ASSERT_TRUE(!computation_report(observer.control, "final", 4, 0), "explicit final notification");
    TEST_ASSERT_TRUE(observer.monotonic && observer.calls == 3 && observer.last_work == 7, "progress snapshot accounting");
    computation_free(observer.control);
}

static void test_computation_integrator_shared(void)
{
    computation_options_t options = {.work_limit = 2};
    computation_t *control = computation_new(&options);
    integrator_t *first = intg_new(), *second = intg_new();
    expr_t *x = expr_new_var(NUM_ZERO), *formula = expr_sin(x);
    number_t value = NUM_NAN, error = NUM_NAN;
    computation_require(control && first && second && x && formula);
    computation_require(intg_set_computation(first, control) && intg_set_computation(second, control));
    TEST_ASSERT_TRUE(intg_integral(first, formula, x, NUM_ZERO, NUM_PI, &value, &error) == 0,
                     "controlled symbolic fast path succeeds");
    TEST_ASSERT_TRUE(intg_get_interval_count_used(first) == 1 && num_eq(value, NUM_TWO),
                     "fast path returns the expected value without quadrature");
    num_destroy(&value);
    num_destroy(&error);
    TEST_ASSERT_TRUE(intg_integral(second, formula, x, NUM_ZERO, NUM_PI, &value, &error) == 0,
                     "a second integrator shares the remaining budget");
    num_destroy(&value);
    num_destroy(&error);
    TEST_ASSERT_TRUE(computation_work_used(control) == 2 && computation_status(control) == COMPUTATION_ACTIVE,
                     "nested consumers do not complete shared control");
    TEST_ASSERT_TRUE(intg_integral(first, formula, x, NUM_ZERO, NUM_PI, &value, &error) == -2,
                     "third attempt exceeds shared budget");
    TEST_ASSERT_TRUE(num_is_nan(value) && num_is_nan(error) && !intg_get_exact_result(first),
                     "stopped call exposes neither stale exact result nor plausible numeric zero");
    num_destroy(&value);
    num_destroy(&error);
    computation_free(control);
    computation_require(intg_set_computation(first, NULL));
    TEST_ASSERT_TRUE(intg_integral(first, formula, x, NUM_ZERO, NUM_PI, &value, NULL) == 0,
                     "detaching restores normal integration");
    num_destroy(&value);
    expr_free(formula);
    expr_free(x);
    intg_free(second);
    intg_free(first);
}

static void test_computation_integrator_numerical(void)
{
    size_t precision = num_get_default_prec_bits();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x + y*y + z*z)", &bindings);
    expr_t *vars[] = {expr_bindings_get(bindings, "x"), expr_bindings_get(bindings, "y"),
                      expr_bindings_get(bindings, "z")};
    number_t lo[] = {NUM_ZERO, NUM_ZERO, NUM_ZERO}, hi[] = {NUM_ONE, NUM_ONE, NUM_ONE};
    integrator_t *ig = intg_new();
    computation_require(formula && ig && vars[0] && vars[1] && vars[2]);
    for (size_t ndim = 1; ndim <= 3; ++ndim) {
        computation_options_t options = {.work_limit = 6};
        computation_t *control = computation_new(&options);
        computation_require(control && intg_set_computation(ig, control));
        expr_set_val(vars[1], NUM_ZERO);
        expr_set_val(vars[2], NUM_ZERO);
        number_t value = NUM_NAN, error = NUM_NAN;
        int status = intg_integral_multi(ig, formula, ndim, vars, lo, hi, &value, &error);
        TEST_ASSERT_TRUE(status == -2 && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                         "all nested quadrature shares one finite budget");
        TEST_ASSERT_TRUE(computation_work_used(control) == 6 && num_is_nan(value) && num_is_nan(error),
                         "work charged across dimensions without exposing partial results");
        TEST_ASSERT_TRUE(num_get_default_prec_bits() == precision, "stopping restores numerical precision");
        num_destroy(&value);
        num_destroy(&error);
        computation_free(control);
    }
    intg_free(ig);
    expr_free(formula);
    expr_bindings_free(bindings);
}

static void test_computation_integrator_observer(void)
{
    size_t precision = num_get_default_prec_bits();
    computation_observer_t observer = {.cancel_stage = "integration.tanh_sinh", .monotonic = true};
    computation_options_t options = {.progress = computation_observe, .argument = &observer};
    observer.control = computation_new(&options);
    integrator_t *ig = intg_new();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    computation_require(observer.control && ig && formula && x && intg_set_computation(ig, observer.control));
    number_t value = NUM_NAN;
    TEST_ASSERT_TRUE(intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, NULL) == -2,
                     "callback stops before quadrature nodes");
    TEST_ASSERT_TRUE(computation_status(observer.control) == COMPUTATION_CANCELLED && observer.calls >= 3,
                     "cancellation followed by final report");
    TEST_ASSERT_TRUE(observer.monotonic && observer.last_work == 1, "no fallback evaluation after cancellation");
    TEST_ASSERT_TRUE(num_is_nan(value) && num_get_default_prec_bits() == precision, "stopped state is cleaned up");
    num_destroy(&value);
    computation_free(observer.control);
    intg_free(ig);
    expr_free(formula);
    expr_bindings_free(bindings);
}

static void test_computation_integrator_exact_and_wrappers(void)
{
    integrator_t *ig = intg_new();
    computation_t *control = computation_new(NULL);
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("exp(-x*x*y*z)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x"), *y = expr_bindings_get(bindings, "y");
    expr_t *z = expr_bindings_get(bindings, "z");
    computation_require(ig && control && formula && x && y && z && intg_set_computation(ig, control));
    number_t value = NUM_NAN, error = NUM_NAN;
    int status = intg_triple_integral(ig, formula, x, NUM_ZERO, NUM_ONE, y, NUM_ZERO, NUM_ONE,
                                     z, NUM_ZERO, NUM_ONE, &value, &error);
    TEST_ASSERT_TRUE(status == 0 && intg_get_exact_result(ig), "recognised exact-result cache is populated");
    num_destroy(&value);
    num_destroy(&error);
    computation_cancel(control);
    status = intg_double_integral(ig, formula, x, NUM_ZERO, NUM_ONE, y, NUM_ZERO, NUM_ONE, &value, &error);
    TEST_ASSERT_TRUE(status == -2 && num_is_nan(value) && num_is_nan(error) && !intg_get_exact_result(ig),
                     "pre-cancelled double integration clears stale exact result");
    num_destroy(&value);
    num_destroy(&error);
    status = intg_triple_integral(ig, formula, x, NUM_ZERO, NUM_ONE, y, NUM_ZERO, NUM_ONE,
                                  z, NUM_ZERO, NUM_ONE, &value, NULL);
    TEST_ASSERT_TRUE(status == -2 && num_is_nan(value), "triple wrapper observes the same stop");
    TEST_ASSERT_TRUE(computation_work_used(control) == 1, "pre-cancelled wrappers reserve no work");
    num_destroy(&value);
    expr_free(formula);
    expr_bindings_free(bindings);
    intg_free(ig);
    computation_free(control);
}

static void test_computation_integrator_success(void)
{
    computation_options_t options = {.work_limit = 10000};
    computation_t *control = computation_new(&options);
    integrator_t *ig = intg_new();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    computation_require(control && ig && formula && x && intg_set_computation(ig, control));
    number_t tolerance = num_create_from_string("1e-8");
    number_t expected = num_create_from_string("0.3102683017233811018");
    number_t value = NUM_NAN, error = NUM_NAN;
    intg_set_tolerance(ig, tolerance, tolerance);
    int status = intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, &error);
    number_t delta = num_sub(value, expected), distance = num_abs(delta);
    TEST_ASSERT_TRUE(status == 0 && num_lt(distance, tolerance), "controlled numerical path preserves convergence");
    TEST_ASSERT_TRUE(computation_work_used(control) > 1 && computation_complete(control),
                     "caller explicitly completes after numerical success");
    num_destroy(&distance);
    num_destroy(&delta);
    num_destroy(&value);
    num_destroy(&error);
    num_destroy(&expected);
    num_destroy(&tolerance);
    computation_require(intg_set_computation(ig, NULL));
    computation_free(control);
    control = computation_new(NULL);
    computation_require(control && intg_set_computation(ig, control));
    intg_set_interval_count_max(ig, 1);
    status = intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, &error);
    TEST_ASSERT_TRUE(status == 1 && computation_status(control) == COMPUTATION_ACTIVE,
                     "subdivision cap is distinct from the shared work limit");
    num_destroy(&value);
    num_destroy(&error);
    expr_free(formula);
    expr_bindings_free(bindings);
    intg_free(ig);
    computation_free(control);
}

static void computation_delay_observer(const computation_progress_t *progress, void *argument)
{
    if (strcmp(progress->stage, "integration.tanh_sinh") == 0)
        computation_require(!lock_wait_for(argument, 5) && errno == ETIMEDOUT);
}

static void test_computation_integrator_deadline(void)
{
    size_t precision = num_get_default_prec_bits();
    lock_t *delay = lock_new(LOCK_EVENT, 0);
    computation_options_t options = {.timeout_ms = 1, .progress = computation_delay_observer, .argument = delay};
    computation_t *control = computation_new(&options);
    integrator_t *ig = intg_new();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    computation_require(delay && control && ig && formula && x && intg_set_computation(ig, control));
    number_t value = NUM_NAN;
    TEST_ASSERT_TRUE(intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, NULL) == -2,
                     "deadline is checked after observer returns");
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_TIMED_OUT && num_is_nan(value),
                     "timeout does not produce a plausible partial result");
    TEST_ASSERT_TRUE(num_get_default_prec_bits() == precision, "deadline restores caller precision");
    num_destroy(&value);
    expr_free(formula);
    expr_bindings_free(bindings);
    intg_free(ig);
    computation_free(control);
    computation_require(lock_free(delay));
}

/* README example: explicit reports and top-level completion use one shared work limit. */
static void computation_example_progress(const computation_progress_t *progress, void *argument)
{
    (void)argument;
    printf("%s: %" PRIu64 "/%" PRIu64 "\n", progress->stage, progress->completed, progress->total);
}

static void example_computation_progress(void)
{
    computation_options_t options = {.work_limit = 3, .progress = computation_example_progress};
    computation_t *control = computation_new(&options);
    computation_require(control != NULL);
    for (uint64_t i = 0; i < 3; ++i) {
        computation_require(computation_checkpoint(control, 1));
        computation_require(computation_report(control, "items", i + 1, 3));
    }
    bool completed = computation_complete(control);
    printf("Completed: %d; work: %" PRIu64 "\n", completed, computation_work_used(control));
    TEST_ASSERT_TRUE(completed && computation_work_used(control) == 3, "README progress and completion");
    computation_free(control);
}

/* README example: a budget-stopped integrator does not publish an unconverged value as success. */
static void example_computation_integrator(void)
{
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    integrator_t *ig = intg_new();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    computation_require(control && ig && formula && x && intg_set_computation(ig, control));
    number_t value = NUM_NAN;
    int result = intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, NULL);
    bool stopped = result == -2 && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Budget exhausted: %d; work: %" PRIu64 "; value unavailable: %d\n",
           stopped, computation_work_used(control), num_is_nan(value));
    TEST_ASSERT_TRUE(stopped && computation_work_used(control) == 1 && num_is_nan(value), "README guarded integration");
    num_destroy(&value);
    expr_free(formula);
    expr_bindings_free(bindings);
    intg_free(ig);
    computation_free(control);
}

int tests_main(void)
{
    alarm(60);
    TEST_RUN_CASE(test_computation_lifecycle, NULL);
    TEST_RUN_CASE(test_computation_budget, NULL);
    TEST_RUN_CASE(test_computation_invalid, NULL);
    TEST_RUN_CASE(test_computation_deadline, NULL);
    TEST_RUN_CASE(test_computation_concurrent_budget, NULL);
    TEST_RUN_CASE(test_computation_thread_cancel, NULL);
    TEST_RUN_CASE(test_computation_progress, NULL);
    TEST_RUN_CASE(test_computation_integrator_shared, NULL);
    TEST_RUN_CASE(test_computation_integrator_numerical, NULL);
    TEST_RUN_CASE(test_computation_integrator_observer, NULL);
    TEST_RUN_CASE(test_computation_integrator_exact_and_wrappers, NULL);
    TEST_RUN_CASE(test_computation_integrator_success, NULL);
    TEST_RUN_CASE(test_computation_integrator_deadline, NULL);
    test_computation_expression_cases();
    test_computation_module_cases();
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_computation_progress);
    TEST_RUN_OUTPUT(example_computation_integrator);
    test_computation_expression_readme();
    test_computation_module_readme();
    alarm(0);
    return TEST_EXIT_CODE();
}
