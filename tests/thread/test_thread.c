/**
 * @file test_thread.c
 * @brief Callback lifetimes, lock-backed completion and shared-state examples.
 *
 * Runs cases sequentially, using lock_t events for deterministic worker ordering.
 * Lifecycle operations on one thread handle never overlap. README examples run
 * after ordinary tests and use only the public thread and lock APIs.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void thread_test_require(bool success)
{
    if (!success)
        abort();
}

static void *thread_test_identity(void *argument)
{
    return argument;
}

static void test_thread_lifecycle(void)
{
    int value = 42;
    void *result = NULL;
    thread_t *thread = thread_new(thread_test_identity, &value);
    thread_test_require(thread != NULL);
    bool ok = !thread_free(thread) && errno == EBUSY;
    ok = thread_wait(thread, 10000) && thread_check(thread) && thread_join(thread, &result) && ok;
    ok = result == &value && thread_check(thread) && ok;
    ok = !thread_join(thread, NULL) && errno == EINVAL && ok;
    ok = !thread_detach(thread) && errno == EINVAL && ok;
    ok = thread_free(thread) && ok;
    thread = thread_new(thread_test_identity, NULL);
    thread_test_require(thread != NULL);
    result = &value;
    ok = thread_join(thread, &result) && !result && thread_free(thread) && ok;
    TEST_ASSERT_TRUE(ok, "join exactly once, retain completion state, and preserve callback results");
}

static void test_thread_invalid(void)
{
    int value = 0;
    void *result = &value;
    TEST_ASSERT_TRUE(!thread_new(NULL, NULL) && errno == EINVAL, "reject absent callback");
    TEST_ASSERT_TRUE(!thread_join(NULL, &result) && errno == EINVAL && result == &value, "preserve failed output");
    TEST_ASSERT_TRUE(!thread_detach(NULL) && errno == EINVAL, "reject absent detach handle");
    TEST_ASSERT_TRUE(!thread_wait(NULL, 0) && errno == EINVAL, "NULL wait");
    TEST_ASSERT_TRUE(!thread_check(NULL) && errno == EINVAL, "NULL check");
    TEST_ASSERT_TRUE(thread_free(NULL), "NULL destructor");
}

typedef struct {
    lock_t *gate;
    lock_t *done;
    thread_t *self;
    int join_error;
    int wait_error;
} thread_test_state_t;

static void *thread_test_gated(void *argument)
{
    thread_test_state_t *state = argument;
    thread_test_require(lock_wait_for(state->gate, 10000));
    thread_test_require(lock_signal(state->done));
    return NULL;
}

static void test_thread_wait_and_detach(void)
{
    thread_test_state_t state = {.gate = lock_new(LOCK_EVENT, 0), .done = lock_new(LOCK_EVENT, 0)};
    thread_test_require(state.gate && state.done);
    thread_t *thread = thread_new(thread_test_gated, &state);
    thread_test_require(thread != NULL);
    bool ok = !thread_check(thread) && errno == EBUSY;
    ok = !thread_wait(thread, 0) && errno == EBUSY && ok;
    ok = !thread_wait(thread, 2) && errno == ETIMEDOUT && ok;
    ok = thread_detach(thread) && ok;
    ok = !thread_join(thread, NULL) && errno == EINVAL && ok;
    ok = !thread_detach(thread) && errno == EINVAL && ok;
    ok = lock_signal(state.gate) && thread_wait(thread, 10000) && thread_check(thread) && ok;
    thread_test_require(thread_free(thread));
    thread_test_require(lock_free(state.gate) && lock_free(state.done));
    TEST_ASSERT_TRUE(ok, "detached threads retain wait/check support until their handles are freed");
}

typedef struct {
    lock_t *gate;
    lock_t *done;
} thread_test_detached_t;

static void *thread_test_detached_callback(void *argument)
{
    thread_test_detached_t *work = argument;
    lock_t *gate = work->gate, *done = work->done;
    free(work);
    thread_test_require(lock_wait_for(gate, 10000));
    thread_test_require(lock_signal(done));
    thread_test_require(lock_free(gate) && lock_free(done));
    return NULL;
}

static void test_thread_free_while_detached(void)
{
    lock_t *gate = lock_new(LOCK_EVENT, 0), *done = lock_new(LOCK_EVENT, 0);
    thread_test_require(gate && done);
    thread_test_detached_t *work = calloc(1, sizeof(*work));
    thread_test_require(work != NULL);
    work->gate = lock_clone(gate);
    work->done = lock_clone(done);
    thread_test_require(work->gate && work->done);
    thread_t *thread = thread_new(thread_test_detached_callback, work);
    thread_test_require(thread && thread_detach(thread) && thread_free(thread));
    bool ok = lock_signal(gate) && lock_wait_for(done, 10000);
    thread_test_require(lock_free(gate) && lock_free(done));
    TEST_ASSERT_TRUE(ok, "worker references outlive a detached thread handle");
}

static void *thread_test_self(void *argument)
{
    thread_test_state_t *state = argument;
    thread_test_require(lock_wait_for(state->gate, 10000));
    state->join_error = thread_join(state->self, NULL) ? 0 : errno;
    state->wait_error = thread_wait(state->self, LOCK_WAIT_FOREVER) ? 0 : errno;
    thread_test_require(lock_signal(state->done));
    return NULL;
}

static void test_thread_self_join(void)
{
    thread_test_state_t state = {.gate = lock_new(LOCK_EVENT, 0), .done = lock_new(LOCK_EVENT, 0)};
    thread_test_require(state.gate && state.done);
    state.self = thread_new(thread_test_self, &state);
    thread_test_require(state.self != NULL);
    thread_test_require(lock_signal(state.gate) && lock_wait_for(state.done, 10000));
    /* Completion of both self-operations is published before the owner's join. */
    thread_test_require(thread_join(state.self, NULL) && thread_free(state.self));
    thread_test_require(lock_free(state.gate) && lock_free(state.done));
    TEST_ASSERT_TRUE(state.join_error == EDEADLK && state.wait_error == EDEADLK, "reject blocking self-completion");
}

static void *thread_example_double(void *argument)
{
    int *value = argument;
    *value *= 2;
    return value;
}

/* README example: wait without consuming join ownership, then collect the result. */
static void example_thread_join(void)
{
    int value = 21;
    void *result = NULL;
    thread_t *thread = thread_new(thread_example_double, &value);
    bool completed = thread && thread_wait(thread, 1000);
    bool joined = thread && thread_join(thread, &result);
    bool ok = completed && joined;
    if (ok)
        printf("Result: %d\n", *(int *)result);
    ok = thread_free(thread) && ok;
    TEST_ASSERT_TRUE(ok && value == 42 && result == &value, "README callback result");
}

typedef struct {
    lock_t *mutex;
    int *value;
} thread_example_counter_t;

static void *thread_example_increment(void *argument)
{
    thread_example_counter_t *counter = argument;
    LOCK_AUTO(guard) = counter->mutex;
    for (unsigned i = 0; i < 1000; ++i) {
        if (!lock_wait(guard))
            return counter;
        ++*counter->value;
        if (!lock_unlock(guard))
            return counter;
    }
    return NULL;
}

/* README example: independently owned clones protect one shared counter. */
static void example_thread_counter(void)
{
    int value = 0;
    lock_t *mutex = lock_new(LOCK_MUTEX, 0);
    thread_test_require(mutex != NULL);
    thread_example_counter_t work[2] = {{lock_clone(mutex), &value}, {lock_clone(mutex), &value}};
    thread_test_require(work[0].mutex && work[1].mutex);
    thread_t *first = thread_new(thread_example_increment, &work[0]);
    if (!first)
        lock_free(work[0].mutex);
    thread_t *second = thread_new(thread_example_increment, &work[1]);
    if (!second)
        lock_free(work[1].mutex);
    bool ok = first && second;
    void *result = NULL;
    if (first) {
        bool joined = thread_join(first, &result);
        ok = joined && !result && ok;
        ok = thread_free(first) && ok;
    }
    if (second) {
        bool joined = thread_join(second, &result);
        ok = joined && !result && ok;
        ok = thread_free(second) && ok;
    }
    ok = lock_free(mutex) && ok;
    if (ok)
        printf("Counter: %d\n", value);
    TEST_ASSERT_TRUE(ok && value == 2000, "README lock-backed concurrent counter");
}

typedef struct {
    lock_t *ready;
    int value;
} thread_example_work_t;

static void *thread_example_publish(void *argument)
{
    thread_example_work_t *work = argument;
    LOCK_AUTO(ready) = work->ready;
    work->value = 6 * 7;
    return lock_signal(ready) ? NULL : work;
}

/* README example: publish through an application-owned persistent completion event. */
static void example_thread_event(void)
{
    lock_t *ready = lock_new(LOCK_EVENT, 0);
    thread_example_work_t work = {.ready = ready ? lock_clone(ready) : NULL};
    thread_t *thread = work.ready ? thread_new(thread_example_publish, &work) : NULL;
    if (!thread) {
        lock_free(work.ready);
        lock_free(ready);
        TEST_ASSERT_TRUE(false, "README completion fixture");
        return;
    }
    bool signalled = lock_wait_for(ready, 10000);
    void *result = NULL;
    /* Join even after a timeout: the callback still borrows the stack context. */
    if (!thread_join(thread, &result))
        abort();
    bool ok = signalled && !result && lock_check(ready) && work.value == 42;
    ok = thread_free(thread) && ok;
    ok = lock_free(ready) && ok;
    if (ok)
        printf("Completion signalled; result: %d\n", work.value);
    TEST_ASSERT_TRUE(ok, "README worker completion event");
}

int tests_main(void)
{
    alarm(30);
    TEST_RUN_CASE(test_thread_lifecycle, NULL);
    TEST_RUN_CASE(test_thread_invalid, NULL);
    TEST_RUN_CASE(test_thread_wait_and_detach, NULL);
    TEST_RUN_CASE(test_thread_free_while_detached, NULL);
    TEST_RUN_CASE(test_thread_self_join, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_thread_join);
    TEST_RUN_OUTPUT(example_thread_counter);
    TEST_RUN_OUTPUT(example_thread_event);
    alarm(0);
    return TEST_EXIT_CODE();
}
