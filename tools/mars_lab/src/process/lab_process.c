/**
 * @file lab_process.c
 * @brief Linux spawn, bounded output collection and child cleanup for native MARS Lab.
 *
 * Implements the private lab_process.h interface using GNU/Linux spawn actions,
 * a non-blocking capture pipe and monotonic deadlines. Child output is retained
 * in the public string module's storage. POSIX descriptor I/O is necessary here:
 * the current public file.h API has no descriptor-adoption operation and opens
 * regular files only, so it cannot represent this anonymous pipe. Worker resolution
 * uses a fixed sorted table to share build defaults and environment overrides
 * between mathematical and calendar adapters without probing the filesystem.
 *
 * Each invocation owns its descriptors and direct child. An exited child remains
 * waitable until collection ends, protecting its process-group identifier from
 * reuse while descendants may still hold the pipe. No global signal handlers or
 * working-directory changes are installed. Callers must preserve normal SIGCHLD
 * reaping semantics and must not wait for this helper's child themselves.
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

#include "file.h"
#include "lab_process.h"
#include "ustring.h"

extern char **environ;

#ifndef MARS_LAB_WORKER_DIR
#define MARS_LAB_WORKER_DIR "build/release/scratch"
#endif

typedef struct {
    const char *name;
    const char *environment;
} lab_process_worker_t;

static int lab_proc_compare_worker(const void *name, const void *entry)
{
    return strcmp(name, ((const lab_process_worker_t *)entry)->name);
}

/* Resolve a worker without building it or interpreting any shell syntax. */
string_t *lab_proc_worker_path(const char *name)
{
    static const lab_process_worker_t workers[] = {{"almanac_event_lab", "MARS_LAB_ALMANAC_EVENT_BINARY"},
                                                   {"almanac_lab", "MARS_LAB_ALMANAC_BINARY"},
                                                   {"datetime_lab", "MARS_LAB_DATETIME_BINARY"},
                                                   {"diffequation_lab", "MARS_LAB_DIFFEQUATION_BINARY"},
                                                   {"equation_lab", "MARS_LAB_EQUATION_BINARY"},
                                                   {"holiday_lab", "MARS_LAB_HOLIDAY_BINARY"},
                                                   {"integrator_lab", "MARS_LAB_INTEGRATOR_BINARY"},
                                                   {"mars_lab", "MARS_LAB_BINARY"},
                                                   {"matrix_lab", "MARS_LAB_MATRIX_BINARY"},
                                                   {"ophelia", "MARS_LAB_OPHELIA_BINARY"}};
    const lab_process_worker_t *worker =
        name ? bsearch(name, workers, sizeof(workers) / sizeof(*workers), sizeof(*workers), lab_proc_compare_worker)
             : NULL;
    if (!worker) {
        errno = EINVAL;
        return NULL;
    }
    const char *override = getenv(worker->environment);
    string_t *path =
        override && *override ? string_new_with(override) : string_sprintf("%s/%s", MARS_LAB_WORKER_DIR, worker->name);
    if (!path)
        errno = ENOMEM;
    return path;
}

static const volatile sig_atomic_t *lab_process_cancel_flag;

/* Register the worker-owned flag before running child processes. */
void lab_proc_set_cancel_flag(const volatile sig_atomic_t *flag)
{
    lab_process_cancel_flag = flag;
}

/* Keep pipe descriptors clear of all standard descriptors, even when those are closed. */
static bool lab_proc_pipe(int descriptors[2])
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

static int lab_proc_spawn(pid_t *child, const char *const argv[], const char *cwd, int writer, const char *input_path)
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
static int lab_proc_interval(const struct timespec *start, unsigned timeout_ms)
{
    if (lab_process_cancel_flag && *lab_process_cancel_flag) {
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

static int lab_proc_prepare_input(file_t *file, const string_t *input, const struct timespec *start,
                                  unsigned timeout_ms)
{
    if (!file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE))
        return errno;
    const char *bytes = string_c_str(input);
    size_t length = string_byte_length(input);
    size_t offset = 0;
    while (offset < length) {
        if (lab_proc_interval(start, timeout_ms) < 0)
            return errno;
        size_t count = length - offset;
        if (count > 4096)
            count = 4096;
        size_t written = 0;
        if (!file_write(file, bytes + offset, count, &written))
            return errno;
        if (!written)
            return EIO;
        offset += written;
    }
    return file_close(file) ? 0 : errno;
}

/* Retain at most three trailing bytes when a UTF-8 scalar spans pipe reads. */
static int lab_proc_append(string_t *output, char *buffer, size_t count, size_t *pending)
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

static int lab_proc_collect(pid_t child, int reader, const struct timespec *start, unsigned timeout_ms,
                            size_t max_output, string_t *output)
{
    size_t captured = 0;
    size_t pending = 0;
    bool eof = false;
    bool exited = false;
    char buffer[4096 + 3];
    for (;;) {
        if (lab_process_cancel_flag && *lab_process_cancel_flag)
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
        int interval = lab_proc_interval(start, timeout_ms);
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
        int append_error = lab_proc_append(output, buffer, pending + accepted, &pending);
        if (append_error)
            return overflow ? EFBIG : append_error;
        captured += accepted;
        if (overflow)
            return EFBIG;
    }
}

/* Run one argument vector and transfer captured output to the caller on success or failure. */
bool lab_proc_run(const char *const argv[], const char *cwd, unsigned timeout_ms, size_t max_output, string_t **output,
                  int *exit_status)
{
    return lab_proc_run_input(argv, cwd, NULL, timeout_ms, max_output, output, exit_status);
}

/* Prepare optional file-backed input, then run and always reap the direct child. */
bool lab_proc_run_input(const char *const argv[], const char *cwd, const string_t *input, unsigned timeout_ms,
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
    char directory[] = "/tmp/mars-lab-input-XXXXXX";
    bool directory_created = false;
    file_t *input_file = NULL;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0 || !lab_proc_pipe(descriptors)) {
        error = errno;
        goto cleanup;
    }
    if (lab_proc_interval(&start, timeout_ms) < 0) {
        error = errno;
        goto cleanup;
    }
    if (input) {
        if (!mkdtemp(directory)) {
            error = errno;
            goto cleanup;
        }
        directory_created = true;
        string_t *path = string_sprintf("%s/stdin", directory);
        input_file = path ? file_new(path) : NULL;
        string_free(path);
        if (!input_file) {
            error = ENOMEM;
            goto cleanup;
        }
        error = lab_proc_prepare_input(input_file, input, &start, timeout_ms);
        if (error)
            goto cleanup;
    }
    if (lab_proc_interval(&start, timeout_ms) < 0) {
        error = errno;
        goto cleanup;
    }
    error = lab_proc_spawn(&child, argv, cwd, descriptors[1], input_file ? file_path(input_file) : "/dev/null");
    if (error)
        goto cleanup;
    if (input_file && !file_delete(input_file))
        error = errno;
    close(descriptors[1]);
    descriptors[1] = -1;
    if (!error)
        error = lab_proc_collect(child, descriptors[0], &start, timeout_ms, max_output, *output);
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
    if (input_file) {
        file_close(input_file);
        if (!file_delete(input_file) && !error)
            error = errno;
        file_free(input_file);
    }
    if (directory_created) {
        file_t *folder = file_new_cstr(directory);
        bool removed = folder && file_remove_directory(folder);
        if (!removed && !error)
            error = folder ? errno : ENOMEM;
        file_free(folder);
    }
    if (descriptors[0] >= 0)
        close(descriptors[0]);
    if (descriptors[1] >= 0)
        close(descriptors[1]);
    errno = error;
    return error == 0;
}
