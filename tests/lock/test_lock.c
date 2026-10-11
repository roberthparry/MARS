/**
 * @file test_lock.c
 * @brief Class-cluster ownership, wait, reader and scope-cleanup regressions.
 *
 * Each case completes before the next begins. Native workers deliberately overlap
 * inside individual cases, using completion events rather than scheduling sleeps.
 * README examples run last and exercise the documented GCC cleanup declaration.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lock-t.h"
#include "thread-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void lock_test_require(bool success)
{
    if (!success)
        abort();
}

static void test_lock_invalid(void)
{
    TEST_ASSERT_TRUE(!lock_new(LOCK_INVALID, 0) && errno == EINVAL, "invalid kind");
    TEST_ASSERT_TRUE(!lock_new((lock_kind_t)99, 0) && errno == EINVAL, "unknown kind");
    TEST_ASSERT_TRUE(!lock_new(LOCK_MUTEX, 1) && errno == EINVAL, "mutex initial state");
    TEST_ASSERT_TRUE(!lock_new(LOCK_RECURSIVE, 1) && errno == EINVAL, "recursive initial state");
    TEST_ASSERT_TRUE(!lock_new(LOCK_READ_WRITE, 1) && errno == EINVAL, "rw initial state");
    TEST_ASSERT_TRUE(!lock_new(LOCK_SEMAPHORE, 0) && errno == EINVAL, "positive bounded permits");
    TEST_ASSERT_TRUE(!lock_new(LOCK_EVENT, 2) && errno == EINVAL, "binary event state");
    TEST_ASSERT_TRUE(!lock_clone(NULL) && errno == EINVAL, "NULL clone");
    TEST_ASSERT_TRUE(lock_kind(NULL) == LOCK_INVALID && errno == EINVAL, "NULL kind");
    TEST_ASSERT_TRUE(!lock_wait(NULL) && errno == EINVAL, "NULL wait");
    TEST_ASSERT_TRUE(!lock_wait_for(NULL, 1) && errno == EINVAL, "NULL timed wait");
    TEST_ASSERT_TRUE(!lock_read(NULL, 0) && errno == EINVAL, "NULL read");
    TEST_ASSERT_TRUE(!lock_try(NULL) && errno == EINVAL, "NULL try");
    TEST_ASSERT_TRUE(!lock_check(NULL) && errno == EINVAL, "NULL check");
    TEST_ASSERT_TRUE(!lock_unlock(NULL) && errno == EINVAL, "NULL unlock");
    TEST_ASSERT_TRUE(!lock_signal(NULL) && errno == EINVAL, "NULL signal");
    TEST_ASSERT_TRUE(!lock_reset(NULL) && errno == EINVAL, "NULL reset");
    TEST_ASSERT_TRUE(lock_free(NULL), "NULL destructor");
    errno = ERANGE;
    lock_cleanup(NULL);
    lock_t *empty = NULL;
    lock_cleanup(&empty);
    TEST_ASSERT_TRUE(errno == ERANGE && !empty, "NULL cleanup preserves errno");
}

static void test_lock_mutex_lifetime(void)
{
    lock_t *root = lock_new(LOCK_MUTEX, 0);
    lock_test_require(root != NULL);
    lock_t *other = lock_clone(root);
    lock_test_require(other != NULL);
    bool ok = lock_kind(root) == LOCK_MUTEX && lock_check(root) && lock_try(root);
    ok = !lock_check(other) && errno == EBUSY && ok;
    ok = !lock_try(other) && errno == EBUSY && ok;
    ok = !lock_wait(root) && errno == EDEADLK && ok;
    ok = !lock_read(root, 0) && errno == ENOTSUP && ok;
    ok = !lock_signal(root) && errno == ENOTSUP && ok;
    ok = !lock_reset(root) && errno == ENOTSUP && ok;
    ok = !lock_unlock(other) && errno == EPERM && ok;
    ok = lock_free(root) && ok; /* Releases ownership but retains other's state. */
    ok = lock_check(other) && lock_wait(other) && lock_unlock(other) && ok;
    ok = !lock_unlock(other) && errno == EPERM && ok;
    ok = lock_free(other) && ok;
    TEST_ASSERT_TRUE(ok, "destruction releases only owned acquisitions and retains cloned state");
}

typedef struct {
    lock_t *resource;
    lock_t *ready;
    lock_t *release;
    bool read;
} lock_test_worker_t;

static void *lock_test_acquire(void *argument)
{
    lock_test_worker_t *work = argument;
    bool acquired = work->read ? lock_read(work->resource, 10000) : lock_wait_for(work->resource, 10000);
    lock_test_require(acquired);
    lock_test_require(lock_signal(work->ready));
    if (work->release)
        lock_test_require(lock_wait_for(work->release, 10000));
    lock_test_require(lock_free(work->resource));
    return NULL;
}

static void test_lock_recursive_release(void)
{
    lock_t *root = lock_new(LOCK_RECURSIVE, 0);
    lock_test_require(root != NULL);
    lock_test_worker_t work = {.resource = lock_clone(root), .ready = lock_new(LOCK_EVENT, 0)};
    lock_test_require(work.resource && work.ready);
    lock_test_require(lock_wait(root) && lock_wait(root));
    lock_t *local = lock_clone(root);
    lock_test_require(local && lock_wait(local));
    lock_test_require(lock_free(local));
    thread_t *worker = thread_new(lock_test_acquire, &work);
    lock_test_require(worker != NULL);
    bool blocked = !lock_wait_for(work.ready, 2) && errno == ETIMEDOUT;
    lock_test_require(lock_free(root)); /* All remaining recursive levels leave together. */
    bool done = lock_wait_for(work.ready, 10000) && thread_join(worker, NULL);
    lock_test_require(thread_free(worker) && lock_free(work.ready));
    TEST_ASSERT_TRUE(blocked && done, "recursive destruction wakes another owner's clone");
}

static void test_lock_readers(void)
{
    lock_t *root = lock_new(LOCK_READ_WRITE, 0), *release = lock_new(LOCK_EVENT, 0);
    lock_test_require(root && release);
    lock_test_worker_t work[2];
    thread_t *workers[2];
    for (unsigned i = 0; i < 2; ++i) {
        work[i] = (lock_test_worker_t){.resource = lock_clone(root), .ready = lock_new(LOCK_EVENT, 0),
                                      .release = release, .read = true};
        lock_test_require(work[i].resource && work[i].ready);
        workers[i] = thread_new(lock_test_acquire, &work[i]);
        lock_test_require(workers[i] != NULL);
    }
    bool ok = lock_wait_for(work[0].ready, 10000) && lock_wait_for(work[1].ready, 10000);
    ok = !lock_try(root) && errno == EBUSY && ok;
    ok = !lock_wait_for(root, 2) && errno == ETIMEDOUT && ok;
    lock_test_require(lock_signal(release));
    for (unsigned i = 0; i < 2; ++i) {
        lock_test_require(thread_join(workers[i], NULL) && thread_free(workers[i]));
        lock_test_require(lock_free(work[i].ready));
    }
    ok = lock_try(root) && ok;
    lock_t *reader = lock_clone(root);
    lock_test_require(reader != NULL);
    ok = !lock_read(reader, 0) && errno == EBUSY && ok;
    ok = !lock_read(root, LOCK_WAIT_FOREVER) && errno == EDEADLK && ok;
    lock_test_require(lock_free(root));
    ok = lock_read(reader, 0) && lock_read(reader, 0) && ok;
    ok = !lock_wait(reader) && errno == EDEADLK && ok;
    lock_test_require(lock_free(reader) && lock_free(release));
    TEST_ASSERT_TRUE(ok, "concurrent readers exclude writers; destruction releases reader and writer ownership");
}

static void test_lock_semaphore(void)
{
    lock_t *root = lock_new(LOCK_SEMAPHORE, 2);
    lock_test_require(root != NULL);
    lock_t *other = lock_clone(root);
    lock_test_require(other != NULL);
    bool ok = lock_kind(root) == LOCK_SEMAPHORE && lock_wait(root) && lock_try(root);
    ok = !lock_check(other) && errno == EBUSY && ok;
    ok = !lock_try(other) && errno == EBUSY && ok;
    ok = !lock_wait_for(other, 2) && errno == ETIMEDOUT && ok;
    ok = lock_unlock(root) && lock_try(other) && ok;
    ok = lock_free(root) && lock_try(other) && ok;
    ok = !lock_try(other) && errno == EBUSY && ok;
    lock_t *remaining = lock_clone(other);
    lock_test_require(remaining != NULL);
    ok = lock_free(other) && lock_try(remaining) && lock_try(remaining) && ok;
    ok = !lock_try(remaining) && errno == EBUSY && ok;
    ok = lock_free(remaining) && ok;
    TEST_ASSERT_TRUE(ok, "bounded permits are returned exactly once on unlock and destruction");
}

static void test_lock_events(void)
{
    lock_t *event = lock_new(LOCK_EVENT, 0);
    lock_test_require(event != NULL);
    bool ok = !lock_check(event) && errno == EBUSY;
    ok = !lock_wait_for(event, 0) && errno == EBUSY && ok;
    ok = !lock_wait_for(event, 2) && errno == ETIMEDOUT && ok;
    ok = lock_signal(event) && lock_check(event) && lock_wait(event) && lock_try(event) && ok;
    ok = !lock_unlock(event) && errno == ENOTSUP && ok;
    ok = lock_reset(event) && ok;
    lock_t *other = lock_clone(event);
    lock_test_require(other != NULL);
    ok = lock_free(event) && ok;
    ok = !lock_check(other) && errno == EBUSY && ok;
    ok = lock_signal(other) && lock_wait_for(other, 1) && lock_free(other) && ok;
    event = lock_new(LOCK_EVENT, 1);
    lock_test_require(event != NULL);
    ok = lock_try(event) && lock_free(event) && ok;
    TEST_ASSERT_TRUE(ok, "completion persists until reset; destruction does not fabricate completion");
}

static void *lock_test_wrong_owner(void *argument)
{
    lock_t *owned = argument;
    bool ok = !lock_unlock(owned) && errno == EPERM;
    ok = !lock_free(owned) && errno == EPERM && ok;
    ok = !lock_try(owned) && errno == EPERM && ok;
    lock_cleanup(&owned);
    ok = owned == argument && errno == EPERM && ok;
    return ok ? NULL : argument;
}

static void test_lock_wrong_owner(void)
{
    lock_t *root = lock_new(LOCK_MUTEX, 0);
    lock_test_require(root && lock_wait(root));
    thread_t *worker = thread_new(lock_test_wrong_owner, root);
    lock_test_require(worker != NULL);
    void *result = root;
    lock_test_require(thread_join(worker, &result) && thread_free(worker));
    bool ok = !result && !lock_check(root) && errno == EBUSY;
    lock_test_require(lock_free(root));
    TEST_ASSERT_TRUE(ok, "wrong-thread release cannot destroy or unlock another owner's handle");
}

static bool lock_test_scoped(lock_t *root, bool early)
{
    LOCK_AUTO(guard) = lock_clone(root);
    if (!guard || !lock_try(guard))
        return false;
    if (early)
        return true;
    errno = ENOENT;
    return true;
}

static void test_lock_cleanup(void)
{
    lock_t *root = lock_new(LOCK_MUTEX, 0);
    lock_test_require(root != NULL);
    bool ok = lock_test_scoped(root, true) && lock_check(root);
    ok = lock_test_scoped(root, false) && errno == ENOENT && lock_check(root) && ok;
    {
        LOCK_AUTO(empty) = NULL;
        LOCK_AUTO(guard) = lock_clone(root);
        lock_test_require(guard && lock_wait(guard));
        lock_cleanup(&guard);
        ok = !empty && !guard && lock_check(root) && ok;
    }
    lock_test_require(lock_wait(root));
    ok = !lock_test_scoped(root, true) && errno == EBUSY && ok;
    ok = !lock_check(root) && errno == EBUSY && ok;
    lock_cleanup(&root);
    TEST_ASSERT_TRUE(ok && !root, "early/normal scope exits unlock; failed guards do not unlock other handles");
}

/* README example: a scoped clone releases its acquisition on early return. */
static bool lock_example_increment(lock_t *mutex, int *value)
{
    LOCK_AUTO(guard) = lock_clone(mutex);
    if (!guard || !lock_wait(guard))
        return false;
    ++*value;
    return true;
}

static void example_lock_scope(void)
{
    lock_t *mutex = lock_new(LOCK_MUTEX, 0);
    int value = 41;
    bool ok = mutex && lock_example_increment(mutex, &value) && lock_try(mutex);
    if (ok)
        printf("Value: %d; lock available again\n", value);
    ok = lock_free(mutex) && ok;
    TEST_ASSERT_TRUE(ok && value == 42, "README scoped unlock output");
}

/* README example: a manual-reset event is a persistent completion state. */
static void example_lock_event(void)
{
    lock_t *event = lock_new(LOCK_EVENT, 0);
    bool ok = event && !lock_check(event) && errno == EBUSY;
    if (ok)
        puts("Pending");
    ok = ok && lock_signal(event) && lock_wait_for(event, 1000);
    if (ok)
        puts("Complete");
    ok = lock_free(event) && ok;
    TEST_ASSERT_TRUE(ok, "README completion output");
}

int tests_main(void)
{
    alarm(30); /* Bound broken synchronisation regressions without a second test runner. */
    TEST_RUN_CASE(test_lock_invalid, NULL);
    TEST_RUN_CASE(test_lock_mutex_lifetime, NULL);
    TEST_RUN_CASE(test_lock_recursive_release, NULL);
    TEST_RUN_CASE(test_lock_readers, NULL);
    TEST_RUN_CASE(test_lock_semaphore, NULL);
    TEST_RUN_CASE(test_lock_events, NULL);
    TEST_RUN_CASE(test_lock_wrong_owner, NULL);
    TEST_RUN_CASE(test_lock_cleanup, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_lock_scope);
    TEST_RUN_OUTPUT(example_lock_event);
    alarm(0);
    return TEST_EXIT_CODE();
}
