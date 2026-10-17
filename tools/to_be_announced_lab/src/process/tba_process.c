/**
 * @file tba_process.c
 * @brief Bounded Linux child-process execution for private Lab network inspection.
 *
 * Uses GNU/Linux spawn actions, a non-blocking capture pipe and monotonic deadlines.
 * POSIX descriptor I/O is required because the public file API cannot adopt pipes.
 * Each call owns its descriptors and child; failures terminate the process group
 * and reap the direct child. Callers must preserve normal SIGCHLD reaping semantics
 * and must not wait for this helper's children themselves.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "string-t.h"
#include "tba_process.h"

extern char **environ;

static const volatile sig_atomic_t *tba_process_cancel_flag;

static bool tba_proc_cancelled(void)
{
    return tba_process_cancel_flag && *tba_process_cancel_flag;
}

/* Register the worker-owned flag before running child processes. */
void tba_proc_set_cancel_flag(const volatile sig_atomic_t *flag)
{
    tba_process_cancel_flag = flag;
}

/* Keep pipe descriptors clear of all standard descriptors, even when those are closed. */
static bool tba_proc_pipe(int descriptors[2])
{
    if (pipe2(descriptors, O_CLOEXEC) < 0)
        return false;
    for (size_t i = 0; i < 2; ++i) {
        if (descriptors[i] >= STDERR_FILENO + 1)
            continue;
        int replacement = fcntl(descriptors[i], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        if (replacement < 0)
            return false;
        close(descriptors[i]);
        descriptors[i] = replacement;
    }
    int flags = fcntl(descriptors[0], F_GETFL);
    return flags >= 0 && fcntl(descriptors[0], F_SETFL, flags | O_NONBLOCK) == 0;
}

static int tba_proc_spawn(pid_t *child, const char *const argv[], const char *cwd, int writer, const char *input_path)
{
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int error = posix_spawn_file_actions_init(&actions);
    if (error)
        return error;
    error = posix_spawnattr_init(&attributes);
    if (error) {
        posix_spawn_file_actions_destroy(&actions);
        return error;
    }
    error = posix_spawnattr_setpgroup(&attributes, 0);
    if (!error)
        error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    if (!error && cwd)
        error = posix_spawn_file_actions_addchdir_np(&actions, cwd);
    if (!error)
        error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, input_path, O_RDONLY | O_NOFOLLOW, 0);
    if (!error)
        error = posix_spawn_file_actions_adddup2(&actions, writer, STDOUT_FILENO);
    if (!error)
        error = posix_spawn_file_actions_adddup2(&actions, writer, STDERR_FILENO);
    if (!error)
        error = posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);
    if (!error)
        error = posix_spawnp(child, argv[0], &actions, &attributes, (char *const *)argv, environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    return error;
}

/* Return a short polling interval, respecting the remaining monotonic deadline. */
static int tba_proc_interval(const struct timespec *start, unsigned timeout_ms)
{
    if (tba_proc_cancelled()) {
        errno = ECANCELED;
        return -1;
    }
    if (!timeout_ms)
        return 20;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return -1;
    int64_t elapsed = (int64_t)(now.tv_sec - start->tv_sec) * 1000000000 + now.tv_nsec - start->tv_nsec;
    int64_t remaining = (int64_t)timeout_ms * 1000000 - elapsed;
    if (remaining <= 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    return remaining >= 20000000 ? 20 : (int)((remaining + 999999) / 1000000);
}

/* Retain at most three trailing bytes when a UTF-8 scalar spans pipe reads. */
static int tba_proc_append(string_t *output, char *buffer, size_t count, size_t *pending)
{
    size_t complete = count;
    size_t lead = count;
    /* UTF-8 scalars contain at most four bytes, so this backwards scan is bounded. */
    while (lead && count - lead < 3 && ((unsigned char)buffer[lead - 1] & 0xc0) == 0x80)
        --lead;
    if (lead) {
        unsigned char byte = (unsigned char)buffer[lead - 1];
        size_t width = byte >= 0xc2 && byte <= 0xdf   ? 2
                       : byte >= 0xe0 && byte <= 0xef ? 3
                       : byte >= 0xf0 && byte <= 0xf4 ? 4
                                                      : 1;
        if (count - (lead - 1) < width)
            complete = lead - 1;
    }
    errno = 0;
    if (string_append_utf8_exact(output, buffer, complete) != 0)
        return errno == ENOMEM ? ENOMEM : EILSEQ;
    *pending = count - complete;
    memmove(buffer, buffer + complete, *pending);
    return 0;
}

static int tba_proc_collect(pid_t child, int reader, const struct timespec *start, unsigned timeout_ms,
                            size_t max_output, string_t *output)
{
    size_t captured = 0;
    size_t pending = 0;
    bool eof = false;
    bool exited = false;
    char buffer[4096 + 3];
    for (;;) {
        if (tba_proc_cancelled())
            return ECANCELED;
        if (!exited) {
            siginfo_t info = {0};
            if (waitid(P_PID, (id_t)child, &info, WEXITED | WNOHANG | WNOWAIT) < 0) {
                if (errno != EINTR)
                    return errno;
            } else {
                exited = info.si_pid == child;
            }
        }
        if (exited && eof)
            return 0;
        int interval = tba_proc_interval(start, timeout_ms);
        if (interval < 0)
            return errno;
        struct pollfd descriptor = {.fd = eof ? -1 : reader, .events = POLLIN};
        int ready = poll(&descriptor, 1, interval);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return errno;
        }
        if (!ready)
            continue;
        if (descriptor.revents & POLLNVAL)
            return EBADF;
        /* One read per iteration ensures continuously writing children cannot starve the deadline. */
        size_t capacity = sizeof(buffer) - pending;
        if (max_output - captured < capacity)
            capacity = max_output - captured + 1;
        ssize_t count = read(reader, buffer + pending, capacity);
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return errno;
        }
        if (!count) {
            if (pending)
                return EILSEQ;
            eof = true;
            continue;
        }
        size_t accepted = (size_t)count;
        bool overflow = accepted > max_output - captured;
        if (overflow)
            accepted = max_output - captured;
        int append_error = tba_proc_append(output, buffer, pending + accepted, &pending);
        if (append_error)
            return overflow ? EFBIG : append_error;
        captured += accepted;
        if (overflow)
            return EFBIG;
    }
}

/* Run one argument vector and transfer captured output to the caller, including on failure. */
bool tba_proc_run(const char *const argv[], const char *cwd, unsigned timeout_ms,
                  size_t max_output, string_t **output, int *exit_status)
{
    if (output)
        *output = NULL;
    if (exit_status)
        *exit_status = -1;
    if (!argv || !argv[0] || !argv[0][0] || !output || !exit_status) {
        errno = EINVAL;
        return false;
    }
    /* Leave headroom for the string module's doubling allocation policy. */
    if (max_output > SIZE_MAX / 2 - 1) {
        errno = EOVERFLOW;
        return false;
    }
    *output = string_new();
    if (!*output) {
        errno = ENOMEM;
        return false;
    }
    int descriptors[2] = {-1, -1};
    pid_t child = -1;
    struct timespec start;
    int error = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0 || !tba_proc_pipe(descriptors)) {
        error = errno;
        goto cleanup;
    }
    if (tba_proc_interval(&start, timeout_ms) < 0) {
        error = errno;
        goto cleanup;
    }
    error = tba_proc_spawn(&child, argv, cwd, descriptors[1], "/dev/null");
    if (error)
        goto cleanup;
    close(descriptors[1]);
    descriptors[1] = -1;
    error = tba_proc_collect(child, descriptors[0], &start, timeout_ms, max_output, *output);
    if (error) {
        kill(-child, SIGKILL);
        kill(child, SIGKILL);
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        if (!error)
            error = errno;
    } else if (WIFEXITED(status)) {
        *exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *exit_status = 128 + WTERMSIG(status);
    }

cleanup:
    if (descriptors[0] >= 0)
        close(descriptors[0]);
    if (descriptors[1] >= 0)
        close(descriptors[1]);
    errno = error;
    return error == 0;
}
