/**
 * @file test_executor.c
 * @brief Bounded execution, ownership, cooperative stopping and multi-producer regressions.
 *
 * Deterministic lock events arrange queue saturation and cancellation without
 * scheduling assumptions. Cases run sequentially; README examples are last.
 */
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "thread-t.h"
#include "executor-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static atomic_size_t executor_calloc_fail_at, executor_calloc_count, executor_thread_fail_at, executor_thread_count;

void *__real_calloc(size_t count, size_t size);
thread_t *__real_thread_new(thread_callback_t callback, void *argument);

/* Link-time allocation failures apply only inside explicitly bracketed constructor tests. */
void *__wrap_calloc(size_t count, size_t size)
{
    size_t fail_at = atomic_load(&executor_calloc_fail_at);
    if (fail_at && atomic_fetch_add(&executor_calloc_count, 1) + 1 == fail_at) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_calloc(count, size);
}

/* Fail native worker startup without exhausting the machine's actual thread resources. */
thread_t *__wrap_thread_new(thread_callback_t callback, void *argument)
{
    size_t fail_at = atomic_load(&executor_thread_fail_at);
    if (fail_at && atomic_fetch_add(&executor_thread_count, 1) + 1 == fail_at) {
        errno = EAGAIN;
        return NULL;
    }
    return __real_thread_new(callback, argument);
}

static void executor_require(bool ok)
{
    if (!ok)
        abort();
}

static bool executor_identity(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)diagnostic;
    *result = argument;
    return true;
}

static bool executor_ignore(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)argument;
    (void)result;
    (void)diagnostic;
    return true;
}

static void test_executor_invalid(void)
{
    TEST_ASSERT_TRUE(!executor_new(0, 1) && errno == EINVAL, "zero workers");
    TEST_ASSERT_TRUE(!executor_new(1, 0) && errno == EINVAL, "zero queue");
    TEST_ASSERT_TRUE(!executor_new(SIZE_MAX, 1) && errno == EOVERFLOW, "worker overflow");
    TEST_ASSERT_TRUE(!executor_new(1, SIZE_MAX) && errno == EOVERFLOW, "queue overflow");
    TEST_ASSERT_TRUE(!executor_submit(NULL, executor_identity, NULL, NULL) && errno == EINVAL, "NULL submit");
    TEST_ASSERT_TRUE(!executor_shutdown(NULL, false) && errno == EINVAL, "NULL shutdown");
    TEST_ASSERT_TRUE(!executor_wait(NULL, 0) && errno == EINVAL, "NULL pool wait");
    TEST_ASSERT_TRUE(!executor_stats(NULL, NULL) && errno == EINVAL, "NULL stats");
    TEST_ASSERT_TRUE(executor_free(NULL), "NULL pool free");
    TEST_ASSERT_TRUE(!executor_task_retain(NULL) && errno == EINVAL, "NULL retain");
    TEST_ASSERT_TRUE(executor_task_status(NULL) == EXECUTOR_TASK_INVALID, "NULL status");
    TEST_ASSERT_TRUE(!executor_task_wait(NULL, 0) && errno == EINVAL, "NULL task wait");
    void *result = &result;
    TEST_ASSERT_TRUE(!executor_task_result(NULL, &result) && result == &result && errno == EINVAL, "NULL result");
    TEST_ASSERT_TRUE(executor_task_error(NULL) == EINVAL, "NULL task error");
    TEST_ASSERT_TRUE(!executor_task_diagnostic(NULL) && errno == EINVAL, "NULL diagnostic");
    TEST_ASSERT_TRUE(!executor_task_computation(NULL) && errno == EINVAL, "NULL computation");
    executor_task_cancel(NULL);
    executor_task_free(NULL);
}

static void test_executor_startup_failures(void)
{
    bool ok = true;
    size_t failures = 0;
    for (size_t index = 1; index <= 48; ++index) {
        atomic_store(&executor_calloc_count, 0);
        atomic_store(&executor_calloc_fail_at, index);
        executor_t *pool = executor_new(3, 4);
        int error = errno;
        atomic_store(&executor_calloc_fail_at, 0);
        if (pool)
            executor_require(executor_free(pool));
        else {
            ++failures;
            ok = error == ENOMEM && ok;
        }
    }
    for (size_t index = 1; index <= 3; ++index) {
        atomic_store(&executor_thread_count, 0);
        atomic_store(&executor_thread_fail_at, index);
        executor_t *pool = executor_new(3, 4);
        int error = errno;
        atomic_store(&executor_thread_fail_at, 0);
        ok = !pool && error == EAGAIN && ok;
        if (pool)
            executor_require(executor_free(pool));
    }
    TEST_ASSERT_TRUE(ok && failures > 20, "allocation and partial native startup failures unwind without hanging");
}

static atomic_int argument_frees, result_frees;

static void executor_drop_argument(void *argument)
{
    free(argument);
    atomic_fetch_add(&argument_frees, 1);
}

static void executor_drop_result(void *result)
{
    free(result);
    atomic_fetch_add(&result_frees, 1);
}

static bool executor_owned(void *argument, void **result, diagnostic_t **diagnostic)
{
    int *value = malloc(sizeof(*value));
    if (!value)
        return false;
    *value = *(int *)argument * 2;
    *result = value;
    *diagnostic = diagnostic_new(DIAGNOSTIC_NOTE, "example", 0, "Calculated", NULL, NULL);
    return *diagnostic != NULL;
}

static void test_executor_ownership(void)
{
    atomic_store(&argument_frees, 0);
    atomic_store(&result_frees, 0);
    executor_t *pool = executor_new(2, 4);
    int *input = malloc(sizeof(*input));
    executor_require(pool && input);
    *input = 21;
    executor_task_options_t options = {.argument_free = executor_drop_argument, .result_free = executor_drop_result};
    executor_task_t *task = executor_submit(pool, executor_owned, input, &options);
    executor_require(task != NULL);
    executor_task_t *retained = executor_task_retain(task);
    executor_task_free(task);
    executor_require(executor_free(pool));
    void *result = NULL;
    bool ok = executor_task_wait(retained, 0) && executor_task_result(retained, &result) && *(int *)result == 42 &&
              atomic_load(&argument_frees) == 1 && atomic_load(&result_frees) == 0;
    diagnostic_t *diagnostic = executor_task_diagnostic(retained);
    computation_t *control = executor_task_computation(retained);
    ok = ok && diagnostic && computation_status(control) == COMPUTATION_ACTIVE;
    executor_task_free(retained);
    ok = ok && atomic_load(&result_frees) == 1 && !strcmp(diagnostic_message(diagnostic), "Calculated");
    diagnostic_free(diagnostic);
    computation_free(control);
    TEST_ASSERT_TRUE(ok, "argument/result ownership and independently retained output survive pool destruction");
}

typedef struct {
    lock_t *entered;
    lock_t *release;
    size_t *order;
    size_t *used;
    size_t value;
    atomic_int *calls;
} executor_fixture_t;

static bool executor_gated(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)diagnostic;
    executor_fixture_t *fixture = argument;
    if (fixture->calls)
        atomic_fetch_add(fixture->calls, 1);
    if (fixture->entered)
        executor_require(lock_signal(fixture->entered));
    if (fixture->release)
        executor_require(lock_wait_for(fixture->release, 10000));
    if (fixture->order)
        fixture->order[(*fixture->used)++] = fixture->value;
    *result = fixture;
    return true;
}

static void test_executor_queue(void)
{
    executor_t *pool = executor_new(1, 2);
    lock_t *entered = lock_new(LOCK_EVENT, 0), *release = lock_new(LOCK_EVENT, 0);
    executor_require(pool && entered && release);
    size_t order[3] = {0}, used = 0;
    executor_fixture_t fixtures[3] = {
        {.entered = entered, .release = release, .order = order, .used = &used, .value = 1},
        {.order = order, .used = &used, .value = 2},
        {.order = order, .used = &used, .value = 3},
    };
    executor_task_t *tasks[3];
    tasks[0] = executor_submit(pool, executor_gated, &fixtures[0], NULL);
    executor_require(tasks[0] && lock_wait_for(entered, 10000));
    for (size_t i = 1; i < 3; ++i)
        executor_require((tasks[i] = executor_submit(pool, executor_gated, &fixtures[i], NULL)) != NULL);
    executor_stats_t stats;
    bool ok = executor_stats(pool, &stats) && stats.queued == 2 && stats.running == 1 && stats.accepting;
    int *rejected = malloc(sizeof(*rejected));
    executor_require(rejected != NULL);
    atomic_store(&argument_frees, 0);
    executor_task_options_t options = {.argument_free = executor_drop_argument};
    ok = !executor_submit(pool, executor_identity, rejected, &options) && errno == EAGAIN &&
         atomic_load(&argument_frees) == 0 && ok;
    free(rejected);
    void *result = &stats;
    ok = !executor_task_result(tasks[1], &result) && errno == EBUSY && result == &stats && ok;
    ok = !executor_task_diagnostic(tasks[1]) && errno == EBUSY && ok;
    ok = !executor_task_wait(tasks[1], 2) && errno == ETIMEDOUT && ok;
    ok = !executor_wait(pool, 0) && errno == EBUSY && ok;
    ok = executor_shutdown(pool, false) && ok;
    ok = !executor_submit(pool, executor_identity, NULL, NULL) && errno == ESHUTDOWN && ok;
    executor_require(lock_signal(release) && executor_wait(pool, 10000));
    ok = executor_stats(pool, &stats) && stats.completed == 3 && !stats.running && !stats.queued && !stats.accepting && ok;
    executor_require(executor_free(pool));
    for (size_t i = 0; i < 3; ++i) {
        ok = executor_task_status(tasks[i]) == EXECUTOR_TASK_SUCCEEDED && order[i] == i + 1 && ok;
        executor_task_free(tasks[i]);
    }
    lock_free(entered);
    lock_free(release);
    TEST_ASSERT_TRUE(ok && used == 3, "bounded queue, untouched rejected ownership, FIFO drain and publication");
}

static void test_executor_cancellation(void)
{
    executor_t *pool = executor_new(1, 2);
    lock_t *entered = lock_new(LOCK_EVENT, 0), *release = lock_new(LOCK_EVENT, 0);
    executor_require(pool && entered && release);
    atomic_int calls = 0;
    executor_fixture_t running = {.entered = entered, .release = release, .calls = &calls};
    executor_fixture_t queued = {.calls = &calls};
    executor_task_t *first = executor_submit(pool, executor_gated, &running, NULL);
    executor_require(first && lock_wait_for(entered, 10000));
    executor_task_t *second = executor_submit(pool, executor_gated, &queued, NULL);
    executor_require(second != NULL);
    executor_task_cancel(second);
    bool ok = executor_task_status(second) == EXECUTOR_TASK_QUEUED;
    ok = executor_shutdown(pool, false) && executor_shutdown(pool, true) && ok;
    ok = !executor_task_wait(first, 0) && errno == EBUSY && ok;
    executor_require(lock_signal(release) && executor_free(pool));
    void *result = &calls;
    ok = executor_task_wait(first, 0) && executor_task_status(first) == EXECUTOR_TASK_CANCELLED &&
         executor_task_status(second) == EXECUTOR_TASK_CANCELLED && atomic_load(&calls) == 1 && ok;
    ok = !executor_task_result(first, &result) && errno == ECANCELED && result == &calls && ok;
    diagnostic_t *diagnostic = executor_task_diagnostic(second);
    ok = diagnostic && diagnostic_code(diagnostic) == ECANCELED && ok;
    diagnostic_free(diagnostic);
    executor_task_free(first);
    executor_task_free(second);
    lock_free(entered);
    lock_free(release);
    TEST_ASSERT_TRUE(ok, "shutdown escalation cancels running and queued tasks but completion waits for callbacks");
}

static bool executor_cancel_with_result(void *argument, void **result, diagnostic_t **diagnostic)
{
    bool ok = executor_owned(argument, result, diagnostic);
    computation_cancel(computation_current());
    return ok;
}

static void test_executor_cancelled_ownership(void)
{
    atomic_store(&argument_frees, 0);
    atomic_store(&result_frees, 0);
    executor_t *pool = executor_new(1, 4);
    computation_t *control = computation_new(NULL);
    int *first_argument = malloc(sizeof(int)), *second_argument = malloc(sizeof(int));
    executor_require(pool && control && first_argument && second_argument);
    *first_argument = *second_argument = 21;
    computation_cancel(control);
    executor_task_options_t options = {
        .computation = control, .argument_free = executor_drop_argument, .result_free = executor_drop_result};
    executor_task_t *first = executor_submit(pool, executor_owned, first_argument, &options);
    options.computation = NULL;
    executor_task_t *second = executor_submit(pool, executor_cancel_with_result, second_argument, &options);
    executor_require(first && second);
    executor_task_free(first);
    executor_task_free(second);
    executor_require(executor_free(pool));
    bool ok = atomic_load(&argument_frees) == 2 && atomic_load(&result_frees) == 1;
    computation_free(control);
    TEST_ASSERT_TRUE(ok, "abandoned cancelled tasks clean skipped arguments and discard late results exactly once");
}

static void test_executor_submission_failures(void)
{
    executor_t *pool = executor_new(1, 4);
    executor_require(pool != NULL);
    atomic_store(&argument_frees, 0);
    executor_task_options_t options = {.argument_free = executor_drop_argument};
    bool ok = true;
    size_t failures = 0, successes = 0;
    for (size_t index = 1; index <= 10; ++index) {
        int *argument = malloc(sizeof(*argument));
        executor_require(argument != NULL);
        atomic_store(&executor_calloc_count, 0);
        atomic_store(&executor_calloc_fail_at, index);
        executor_task_t *task = executor_submit(pool, executor_ignore, argument, &options);
        int error = errno;
        atomic_store(&executor_calloc_fail_at, 0);
        if (!task) {
            ++failures;
            ok = error == ENOMEM && ok;
            free(argument);
        } else {
            ++successes;
            executor_require(executor_wait(pool, 10000));
            executor_task_free(task);
        }
    }
    executor_require(executor_free(pool));
    TEST_ASSERT_TRUE(ok && failures >= 5 && atomic_load(&argument_frees) == (int)successes,
                     "failed submissions preserve caller ownership and leave the queue usable");
}

static bool executor_exhaust(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)argument;
    (void)result;
    (void)diagnostic;
    return computation_checkpoint(computation_current(), 3);
}

static bool executor_failure(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)argument;
    *result = malloc(1);
    *diagnostic = diagnostic_new(DIAGNOSTIC_ERROR, "calculation", 12, "Invalid input", NULL, NULL);
    errno = EDOM;
    return false;
}

static bool executor_unspecified_failure(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)argument;
    (void)result;
    (void)diagnostic;
    return false;
}

static void test_executor_failures(void)
{
    executor_t *pool = executor_new(1, 8);
    computation_options_t limits = {.work_limit = 2};
    computation_t *control = computation_new(&limits);
    executor_require(pool && control);
    executor_task_options_t options = {.computation = control};
    executor_task_t *budget = executor_submit(pool, executor_exhaust, NULL, &options);
    options = (executor_task_options_t){.result_free = executor_drop_result};
    atomic_store(&result_frees, 0);
    executor_task_t *failure = executor_submit(pool, executor_failure, NULL, &options);
    executor_task_t *unspecified = executor_submit(pool, executor_unspecified_failure, NULL, NULL);
    executor_task_t *empty = executor_submit(pool, executor_identity, NULL, NULL);
    executor_require(budget && failure && unspecified && empty && executor_free(pool));
    bool ok = executor_task_status(budget) == EXECUTOR_TASK_BUDGET_EXHAUSTED &&
              executor_task_error(budget) == E2BIG && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED &&
              executor_task_status(failure) == EXECUTOR_TASK_FAILED && executor_task_error(failure) == EDOM &&
              executor_task_error(unspecified) == EIO && atomic_load(&result_frees) == 1;
    diagnostic_t *diagnostic = executor_task_diagnostic(failure);
    ok = diagnostic && diagnostic_code(diagnostic) == 12 && ok;
    void *result = &options;
    ok = executor_task_result(empty, &result) && !result && ok;
    errno = ENOENT;
    ok = !executor_task_diagnostic(empty) && errno == ENOENT && ok;
    diagnostic_free(diagnostic);
    executor_task_free(budget);
    executor_task_free(failure);
    executor_task_free(unspecified);
    executor_task_free(empty);
    computation_free(control);
    TEST_ASSERT_TRUE(ok, "budget, explicit and default failures, partial result cleanup and successful NULL output");
}

static void test_executor_deadline(void)
{
    executor_t *pool = executor_new(1, 2);
    lock_t *entered = lock_new(LOCK_EVENT, 0), *release = lock_new(LOCK_EVENT, 0);
    executor_require(pool && entered && release);
    executor_fixture_t blocker = {.entered = entered, .release = release};
    executor_task_t *first = executor_submit(pool, executor_gated, &blocker, NULL);
    executor_require(first && lock_wait_for(entered, 10000));
    computation_options_t limits = {.timeout_ms = 1};
    computation_t *control = computation_new(&limits);
    executor_require(control != NULL);
    executor_task_options_t options = {.computation = control};
    executor_task_t *second = executor_submit(pool, executor_identity, NULL, &options);
    executor_require(second != NULL);
    /* A timed event wait provides a monotonic delay without polling or sleeping loops. */
    executor_require(!lock_wait_for(release, 5) && errno == ETIMEDOUT);
    executor_require(lock_signal(release) && executor_free(pool));
    bool ok = executor_task_status(second) == EXECUTOR_TASK_TIMED_OUT && executor_task_error(second) == ETIMEDOUT;
    executor_task_free(first);
    executor_task_free(second);
    computation_free(control);
    lock_free(entered);
    lock_free(release);
    TEST_ASSERT_TRUE(ok, "deadlines include queue time and prevent invocation after expiry");
}

typedef struct {
    executor_t *pool;
    lock_t *entered;
    lock_t *release;
    executor_task_t *other;
    bool protected;
} executor_self_t;

static bool executor_self(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)result;
    (void)diagnostic;
    executor_self_t *self = argument;
    executor_require(lock_signal(self->entered) && lock_wait_for(self->release, 10000));
    bool ok = !executor_free(self->pool) && errno == EDEADLK;
    ok = !executor_wait(self->pool, LOCK_WAIT_FOREVER) && errno == EDEADLK && ok;
    ok = !executor_task_wait(self->other, LOCK_WAIT_FOREVER) && errno == EDEADLK && ok;
    ok = !executor_task_wait(self->other, 0) && errno == EBUSY && ok;
    self->protected = ok;
    return ok;
}

static void test_executor_dependencies(void)
{
    executor_t *pool = executor_new(1, 2);
    executor_self_t self = {.pool = pool, .entered = lock_new(LOCK_EVENT, 0), .release = lock_new(LOCK_EVENT, 0)};
    executor_require(pool && self.entered && self.release);
    executor_task_t *first = executor_submit(pool, executor_self, &self, NULL);
    executor_require(first && lock_wait_for(self.entered, 10000));
    self.other = executor_submit(pool, executor_identity, NULL, NULL);
    executor_require(self.other && lock_signal(self.release) && executor_free(pool));
    bool ok = self.protected && executor_task_status(first) == EXECUTOR_TASK_SUCCEEDED &&
              executor_task_status(self.other) == EXECUTOR_TASK_SUCCEEDED;
    executor_task_free(first);
    executor_task_free(self.other);
    lock_free(self.entered);
    lock_free(self.release);
    TEST_ASSERT_TRUE(ok, "bounded workers cannot block on queued dependencies or destroy their own pool");
}

typedef struct {
    executor_t *pool;
    atomic_size_t calls;
} executor_producer_t;

static bool executor_count(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)result;
    (void)diagnostic;
    executor_producer_t *state = argument;
    atomic_fetch_add(&state->calls, 1);
    return true;
}

static void *executor_produce(void *argument)
{
    executor_producer_t *state = argument;
    for (size_t i = 0; i < 64; ++i) {
        executor_task_t *task = executor_submit(state->pool, executor_count, state, NULL);
        executor_require(task != NULL);
        executor_task_free(task);
    }
    return NULL;
}

static void test_executor_concurrent_producers(void)
{
    executor_producer_t state = {.pool = executor_new(3, 256)};
    atomic_init(&state.calls, 0);
    executor_require(state.pool != NULL);
    bool ok = true;
    for (size_t round = 0; round < 4; ++round) {
        thread_t *producers[4];
        for (size_t i = 0; i < 4; ++i)
            executor_require((producers[i] = thread_new(executor_produce, &state)) != NULL);
        for (size_t i = 0; i < 4; ++i)
            executor_require(thread_join(producers[i], NULL) && thread_free(producers[i]));
        executor_require(executor_wait(state.pool, 10000));
        executor_stats_t stats;
        ok = executor_stats(state.pool, &stats) && stats.completed == (round + 1) * 256 &&
             !stats.running && !stats.queued && ok;
    }
    executor_require(executor_free(state.pool));
    TEST_ASSERT_TRUE(ok && atomic_load(&state.calls) == 1024, "concurrent submit/free, ring wraparound and idle wakeups");
}

/* README example: drain the pool before collecting a task that outlives it. */
static void example_executor_result(void)
{
    executor_t *pool = executor_new(2, 8);
    int value = 42;
    executor_task_t *task = pool ? executor_submit(pool, executor_identity, &value, NULL) : NULL;
    bool ok = task != NULL;
    ok = executor_free(pool) && ok;
    void *result = NULL;
    if (ok && executor_task_result(task, &result))
        printf("Result: %d\n", *(int *)result);
    else
        ok = false;
    executor_task_free(task);
    TEST_ASSERT_TRUE(ok, "README borrowed result lifetime");
}

/* README example: transport the callback's structured error to the submitting thread. */
static void example_executor_diagnostic(void)
{
    executor_t *pool = executor_new(1, 4);
    executor_task_options_t options = {.result_free = free};
    executor_task_t *task = pool ? executor_submit(pool, executor_failure, NULL, &options) : NULL;
    bool ok = executor_free(pool) && task;
    diagnostic_t *error = ok ? executor_task_diagnostic(task) : NULL;
    string_t *text = error ? diagnostic_to_string(error) : NULL;
    if (text)
        printf("%s\n", string_c_str(text));
    ok = ok && text && executor_task_error(task) == EDOM;
    string_free(text);
    diagnostic_free(error);
    executor_task_free(task);
    TEST_ASSERT_TRUE(ok, "README diagnostic handoff");
}

int tests_main(void)
{
    alarm(60);
    TEST_RUN_CASE(test_executor_invalid, NULL);
    TEST_RUN_CASE(test_executor_startup_failures, NULL);
    TEST_RUN_CASE(test_executor_ownership, NULL);
    TEST_RUN_CASE(test_executor_submission_failures, NULL);
    TEST_RUN_CASE(test_executor_queue, NULL);
    TEST_RUN_CASE(test_executor_cancellation, NULL);
    TEST_RUN_CASE(test_executor_cancelled_ownership, NULL);
    TEST_RUN_CASE(test_executor_failures, NULL);
    TEST_RUN_CASE(test_executor_deadline, NULL);
    TEST_RUN_CASE(test_executor_dependencies, NULL);
    TEST_RUN_CASE(test_executor_concurrent_producers, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_executor_result);
    TEST_RUN_OUTPUT(example_executor_diagnostic);
    alarm(0);
    return TEST_EXIT_CODE();
}
