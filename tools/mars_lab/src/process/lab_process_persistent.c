/**
 * @file lab_process_persistent.c
 * @brief Persistent calculation ownership, bounded request transport and output supervision.
 *
 * Each prefork HTTP process owns one small single-threaded supervisor and one calculation process. Native
 * calculations are C calls in that resident process, never executable launches. Private Protobuf messages carry
 * arguments and the current environment; anonymous input descriptors and a bounded output pipe need no disk files.
 * The supervisor alone replaces failed or oversized calculation processes. Requests in one HTTP process are serial.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
/* POSIX and MARS both declare stack_t; keep their independent types distinct. */
#define stack_t posix_signal_stack_t
#include <signal.h>
#include <sys/wait.h>
#undef stack_t
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "array-t.h"
#include "protobuf-t.h"
#include "string-t.h"
#include "lab_process.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_session.h"

extern char **environ;

static struct {
    pid_t owner, supervisor, calculation;
    int channel;
    bool started;
    atomic_bool busy;
} persistent = {.channel = -1};

/* Derive a conservative ceiling independently of transient desktop memory pressure. */
size_t lab_proc_memory_budget(void)
{
    long pages = sysconf(_SC_PHYS_PAGES), page_size = sysconf(_SC_PAGESIZE);
    uint64_t mib = 1024u * 1024u, budget = 256u * mib;
    if (pages > 0 && page_size > 0 && (uint64_t)pages <= UINT64_MAX / (uint64_t)page_size)
        budget = (uint64_t)pages * (uint64_t)page_size / 32u;
    if (budget < 128u * mib) budget = 128u * mib;
    if (budget > 512u * mib) budget = 512u * mib;
    return (size_t)(budget / mib * mib);
}

/* Bound allocations and mappings without increasing a limit inherited from the launcher. */
bool lab_proc_memory_limit(size_t bytes)
{
    if (!bytes) { errno = EINVAL; return false; }
    struct rlimit limit;
    if (getrlimit(RLIMIT_AS, &limit)) return false;
    rlim_t bound = (rlim_t)bytes;
    if (limit.rlim_max != RLIM_INFINITY && bound > limit.rlim_max) bound = limit.rlim_max;
    limit.rlim_cur = limit.rlim_max = bound;
    return setrlimit(RLIMIT_AS, &limit) == 0;
}

static void persistent_inherit(void)
{
    if (persistent.owner == getpid()) return;
    if (persistent.channel >= 0) close(persistent.channel);
    persistent.owner = getpid();
    persistent.supervisor = persistent.calculation = 0;
    persistent.channel = -1;
    persistent.started = false;
    atomic_store(&persistent.busy, false);
}

/* Release a service owned by this PID; forked fixtures must not stop their parent's calculations. */
void lab_proc_workers_stop(void)
{
    persistent_inherit();
    if (persistent.channel >= 0) { shutdown(persistent.channel, SHUT_RDWR); close(persistent.channel); }
    if (persistent.supervisor > 0) {
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            pid_t reaped = waitpid(persistent.supervisor, NULL, WNOHANG);
            if (reaped == persistent.supervisor || (reaped < 0 && errno == ECHILD)) break;
            if (attempt == 99) {
                kill(persistent.supervisor, SIGKILL);
                while (waitpid(persistent.supervisor, NULL, 0) < 0 && errno == EINTR) {}
            } else poll(NULL, 0, 5);
        }
    }
    persistent.channel = -1;
    persistent.supervisor = persistent.calculation = 0;
    persistent.started = false;
}

/* Pre-create process ownership before request handlers or render executors start threads. */
bool lab_proc_workers_start(size_t memory_limit, unsigned max_jobs)
{
    persistent_inherit();
    if (persistent.started) {
        if (persistent.channel >= 0) return true;
        errno = EIO;
        return false;
    }
    if (!memory_limit) memory_limit = lab_proc_memory_budget();
    if (!max_jobs) max_jobs = 256;
    if (memory_limit < 64u * 1024u * 1024u || max_jobs > 1000000u) { errno = EINVAL; return false; }
    int channels[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, channels)) return false;
    fflush(NULL);
    pid_t owner = getpid(), supervisor = fork();
    if (!supervisor) {
        close(channels[0]);
        signal(SIGTERM, SIG_DFL); signal(SIGINT, SIG_DFL); signal(SIGPIPE, SIG_IGN);
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) || getppid() != owner) _exit(1);
        if (channels[1] > 3) close_range(3u, (unsigned)channels[1] - 1u, 0);
        close_range((unsigned)channels[1] + 1u, ~0u, 0);
        lab_proc_session_supervise(channels[1], memory_limit, max_jobs);
        _exit(0);
    }
    close(channels[1]);
    if (supervisor < 0) { close(channels[0]); return false; }
    persistent.channel = channels[0]; persistent.supervisor = supervisor; persistent.started = true;
    struct pollfd ready = {.fd = persistent.channel, .events = POLLIN};
    lab_proc_session_message_t message;
    int descriptors[LAB_SESSION_FDS];
    bool ok = poll(&ready, 1, 5000) > 0 &&
              lab_proc_session_receive(persistent.channel, &message, descriptors) == 1 &&
              message.kind == LAB_SESSION_READY && !message.status;
    if (ok) persistent.calculation = (pid_t)message.pid;
    else { lab_proc_workers_stop(); errno = EIO; }
    return ok;
}

/* Observe the most recently selected worker, without changing service ownership. */
pid_t lab_proc_persistent_pid(void)
{
    return persistent.owner == getpid() && persistent.calculation > 0 ? persistent.calculation : -1;
}

static int persistent_bytes(const void *data, size_t size)
{
    int descriptor = memfd_create("mars-calculation", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (descriptor < 0) return -1;
    const unsigned char *bytes = data;
    size_t offset = 0;
    while (offset < size) {
        ssize_t written = write(descriptor, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { int error = errno; close(descriptor); errno = error; return -1; }
        offset += (size_t)written;
    }
    if (lseek(descriptor, 0, SEEK_SET) < 0 ||
        fcntl(descriptor, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) {
        int error = errno; close(descriptor); errno = error; return -1;
    }
    return descriptor;
}

static array_t *persistent_request(const char *const argv[], unsigned timeout_ms)
{
    protobuf_t *request = protobuf_new(LAB_SESSION_MAX_REQUEST, 4608);
    bool ok = request != NULL;
    size_t argc = 0, environment_count = 0;
    for (; ok && argv[argc] && argc < 255; ++argc)
        ok = protobuf_add_bytes(request, 1, argv[argc], strlen(argv[argc]));
    ok = ok && argc && !argv[argc];
    for (; ok && environ && environ[environment_count] && environment_count < 4096; ++environment_count)
        ok = protobuf_add_bytes(request, 2, environ[environment_count], strlen(environ[environment_count]));
    ok = ok && (!environ || !environ[environment_count]) &&
         protobuf_add_integer(request, 3, PROTOBUF_VARINT, timeout_ms);
    array_t *bytes = ok ? protobuf_encode(request) : NULL;
    protobuf_free(request);
    return bytes;
}

static bool persistent_stop_elapsed(const struct timespec *start)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return true;
    int64_t elapsed = (int64_t)(now.tv_sec - start->tv_sec) * 1000000000 + now.tv_nsec - start->tv_nsec;
    return elapsed >= 750000000;
}

static int persistent_collect(int reader, const struct timespec *start, unsigned timeout_ms,
                               size_t maximum, string_t *output, int *exit_status)
{
    bool eof = false, finished = false;
    size_t captured = 0, pending = 0;
    int error = 0;
    char buffer[4099];
    struct timespec stop_start = {0};
    while (!eof || !finished) {
        int interval = error ? 10 : lab_proc_interval(start, timeout_ms);
        int stopped = !error && lab_proc_cancelled() ? ECANCELED : interval < 0 ? errno : 0;
        if (!error && stopped) {
            error = stopped;
            clock_gettime(CLOCK_MONOTONIC, &stop_start);
            lab_proc_session_message_t cancel = {.kind = LAB_SESSION_CANCEL, .status = (unsigned)error};
            if (!lab_proc_session_send(persistent.channel, &cancel, NULL)) break;
        }
        if (error && persistent_stop_elapsed(&stop_start)) { lab_proc_workers_stop(); break; }
        struct pollfd peers[] = {{.fd = persistent.channel, .events = POLLIN},
                                {.fd = eof ? -1 : reader, .events = POLLIN}};
        int ready = poll(peers, 2, interval > 0 ? interval : 10);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { if (!error) error = errno; break; }
        if (peers[0].revents & POLLIN) {
            lab_proc_session_message_t message;
            int unused[LAB_SESSION_FDS];
            int received = lab_proc_session_receive(persistent.channel, &message, unused);
            lab_proc_session_close(unused);
            if (received != 1 || (message.kind != LAB_SESSION_STARTED && message.kind != LAB_SESSION_FINISHED)) {
                if (!error) error = EIO;
                break;
            }
            if (message.pid > 0) persistent.calculation = (pid_t)message.pid;
            if (message.kind == LAB_SESSION_FINISHED) { finished = true; *exit_status = (int)message.status; }
        } else if (peers[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            if (!error) error = EIO;
            break;
        }
        if (!eof && peers[1].revents) {
            size_t capacity = sizeof(buffer) - pending;
            if (maximum - captured < capacity) capacity = maximum - captured + 1;
            ssize_t count = read(reader, buffer + pending, capacity);
            if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            int output_error = 0;
            if (count < 0) output_error = errno;
            else if (!count) { eof = true; if (pending) output_error = EILSEQ; }
            else {
                size_t accepted = (size_t)count;
                bool overflow = accepted > maximum - captured;
                if (overflow) accepted = maximum - captured;
                output_error = lab_proc_append(output, buffer, pending + accepted, &pending);
                captured += accepted;
                if (overflow) output_error = EFBIG;
            }
            if (output_error && !error) {
                error = output_error;
                clock_gettime(CLOCK_MONOTONIC, &stop_start);
                lab_proc_session_message_t cancel = {.kind = LAB_SESSION_CANCEL, .status = (unsigned)error};
                if (!lab_proc_session_send(persistent.channel, &cancel, NULL)) break;
            }
        }
    }
    if ((!eof || !finished) && persistent.channel >= 0) {
        lab_proc_workers_stop();
        /* Recovery forks belong to the supervisor, never a threaded HTTP handler. */
        persistent.started = true;
    }
    return error;
}

/* Run one job using bounded anonymous ownership; every descriptor is closed on every return. */
bool lab_proc_persistent_run(const char *const argv[], const char *cwd, const string_t *input,
                              unsigned timeout_ms, size_t max_output, string_t **output, int *exit_status)
{
    persistent_inherit();
    if (max_output > 64u * 1024u * 1024u || (input && string_byte_length(input) > LAB_SESSION_MAX_REQUEST)) {
        errno = E2BIG; return false;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&persistent.busy, &expected, true)) { errno = EBUSY; return false; }
    struct timespec start;
    int descriptors[LAB_SESSION_FDS] = {-1, -1, -1, -1}, pipe_descriptors[2] = {-1, -1};
    array_t *request = NULL;
    int error = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) error = errno;
    if (!error && lab_proc_cancelled()) error = ECANCELED;
    if (!error && !lab_proc_workers_start(0, 0)) error = errno;
    if (!error) {
        request = persistent_request(argv, timeout_ms);
        if (!request) error = E2BIG;
    }
    if (!error) {
        descriptors[0] = persistent_bytes(array_get(request, 0), array_size(request));
        if (descriptors[0] < 0) error = errno;
    }
    if (!error) {
        descriptors[1] = persistent_bytes(input ? string_c_str(input) : NULL, input ? string_byte_length(input) : 0);
        if (descriptors[1] < 0) error = errno;
    }
    if (!error) {
        descriptors[3] = open(cwd ? cwd : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (descriptors[3] < 0 || pipe2(pipe_descriptors, O_CLOEXEC)) error = errno;
    }
    if (!error) {
        descriptors[2] = pipe_descriptors[1]; pipe_descriptors[1] = -1;
        int flags = fcntl(pipe_descriptors[0], F_GETFL);
        if (flags < 0 || fcntl(pipe_descriptors[0], F_SETFL, flags | O_NONBLOCK)) error = errno;
    }
    if (!error && lab_proc_interval(&start, timeout_ms) < 0) error = errno;
    if (!error) {
        *output = string_new();
        if (!*output) error = ENOMEM;
    }
    if (!error) {
        lab_proc_session_message_t message = {.kind = LAB_SESSION_JOB};
        if (!lab_proc_session_send(persistent.channel, &message, descriptors)) error = errno;
        close(descriptors[2]); descriptors[2] = -1;
        if (!error) error = persistent_collect(pipe_descriptors[0], &start, timeout_ms,
                                                max_output, *output, exit_status);
    }
    lab_proc_session_close(descriptors);
    for (size_t i = 0; i < 2; ++i) if (pipe_descriptors[i] >= 0) close(pipe_descriptors[i]);
    array_destroy(request);
    atomic_store(&persistent.busy, false);
    errno = error;
    return error == 0;
}
