/**
 * @file lab_process_supervisor.c
 * @brief Single-threaded supervision and bounded recycling of persistent calculations.
 *
 * This small process is created before HTTP threads start. It alone forks replacement calculation processes,
 * so recovery never forks a multithreaded request handler. Successful jobs retain the same calculation process;
 * cancellation, crashes, job-count limits and resident-memory high-water marks retire only that process.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "lab_process.h"
#include "lab_worker.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_session.h"

typedef struct {
    pid_t pid;
    int channel;
    unsigned jobs;
} session_child_t;

static void session_reap(session_child_t *child)
{
    if (child->pid > 0) {
        kill(-child->pid, SIGKILL);
        kill(child->pid, SIGKILL);
        while (waitpid(child->pid, NULL, 0) < 0 && errno == EINTR) {}
    }
    if (child->channel >= 0) close(child->channel);
    *child = (session_child_t){.channel = -1};
}

static bool session_start(session_child_t *child, size_t memory_limit)
{
    if (child->pid > 0) {
        siginfo_t state = {0};
        int observed;
        do { observed = waitid(P_PID, (id_t)child->pid, &state, WEXITED | WNOHANG | WNOWAIT); }
        while (observed < 0 && errno == EINTR);
        if (!observed && !state.si_pid) return true;
        session_reap(child);
    }
    int channels[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, channels)) return false;
    pid_t parent = getpid(), pid = fork();
    if (!pid) {
        close(channels[0]);
        if (setpgid(0, 0) || prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != parent) _exit(1);
        /* Keep no listener, configuration-helper or another HTTP process's descriptors. */
        if (channels[1] > 3) close_range(3u, (unsigned)channels[1] - 1u, 0);
        close_range((unsigned)channels[1] + 1u, ~0u, 0);
        lab_proc_session_worker(channels[1], memory_limit);
        _exit(0);
    }
    close(channels[1]);
    if (pid < 0) { close(channels[0]); return false; }
    *child = (session_child_t){.pid = pid, .channel = channels[0]};
    struct pollfd ready = {.fd = child->channel, .events = POLLIN};
    int descriptors[LAB_SESSION_FDS];
    lab_proc_session_message_t message;
    bool ok = poll(&ready, 1, 5000) > 0 &&
              lab_proc_session_receive(child->channel, &message, descriptors) == 1 &&
              message.kind == LAB_SESSION_READY && !message.status;
    if (!ok) session_reap(child);
    return ok;
}

static bool session_grace_expired(const struct timespec *start)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return true;
    int64_t elapsed = (int64_t)(now.tv_sec - start->tv_sec) * 1000000000 + now.tv_nsec - start->tv_nsec;
    return elapsed >= 250000000;
}

static bool session_forward(int channel, session_child_t *child, const int descriptors[LAB_SESSION_FDS],
                             size_t memory_limit, unsigned max_jobs)
{
    lab_proc_session_message_t request = {.kind = LAB_SESSION_JOB};
    bool started = session_start(child, memory_limit) && lab_proc_session_send(child->channel, &request, descriptors);
    lab_proc_session_message_t reply = {.kind = started ? LAB_SESSION_STARTED : LAB_SESSION_FINISHED,
        .pid = child->pid, .status = started ? 0u : LAB_WORKER_CONTROL_FAILED};
    if (!lab_proc_session_send(channel, &reply, NULL)) return false;
    if (!started) { session_reap(child); return true; }
    bool stopping = false, finished = false;
    struct timespec stop_start = {0};
    reply = (lab_proc_session_message_t){.kind = LAB_SESSION_FINISHED, .pid = child->pid};
    while (!finished) {
        struct pollfd peers[] = {{.fd = channel, .events = POLLIN}, {.fd = child->channel, .events = POLLIN}};
        int ready = poll(peers, 2, 10);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || peers[0].revents & (POLLHUP | POLLERR | POLLNVAL)) { session_reap(child); return false; }
        if (peers[0].revents & POLLIN) {
            int unused[LAB_SESSION_FDS];
            lab_proc_session_message_t cancel;
            int received = lab_proc_session_receive(channel, &cancel, unused);
            lab_proc_session_close(unused);
            if (received != 1 || cancel.kind != LAB_SESSION_CANCEL) { session_reap(child); return false; }
            stopping = true;
            clock_gettime(CLOCK_MONOTONIC, &stop_start);
            bool graceful = cancel.status == ECANCELED || cancel.status == ETIMEDOUT;
            kill(child->pid, graceful ? SIGTERM : SIGKILL);
            if (!graceful) kill(-child->pid, SIGKILL);
        }
        if (peers[1].revents & POLLIN) {
            int unused[LAB_SESSION_FDS];
            int received = lab_proc_session_receive(child->channel, &reply, unused);
            lab_proc_session_close(unused);
            if (received == 1 && reply.kind == LAB_SESSION_FINISHED) finished = true;
            else { reply.status = LAB_WORKER_CONTROL_FAILED; finished = true; }
        } else if (peers[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            siginfo_t state = {0};
            int observed;
            do { observed = waitid(P_PID, (id_t)child->pid, &state, WEXITED | WNOWAIT); }
            while (observed < 0 && errno == EINTR);
            reply.status = !observed && state.si_code == CLD_EXITED ? (unsigned)state.si_status :
                           !observed ? (unsigned)(128 + state.si_status) : LAB_WORKER_CONTROL_FAILED;
            finished = true;
            stopping = true;
        }
        if (!finished && stopping && session_grace_expired(&stop_start)) {
            kill(-child->pid, SIGKILL);
            kill(child->pid, SIGKILL);
        }
    }
    ++child->jobs;
    bool retire = stopping || child->jobs >= max_jobs || reply.resident_bytes >= memory_limit / 2u ||
                  (reply.status >= LAB_WORKER_CANCELLED && reply.status <= LAB_WORKER_CONTROL_FAILED);
    reply.kind = LAB_SESSION_FINISHED;
    bool sent = lab_proc_session_send(channel, &reply, NULL);
    if (retire || !sent) session_reap(child);
    return sent;
}

/* Keep all replacement forks in this dedicated single-threaded process. */
void lab_proc_session_supervise(int channel, size_t memory_limit, unsigned max_jobs)
{
    session_child_t child = {.channel = -1};
    bool ready = session_start(&child, memory_limit) && lab_proc_memory_limit(memory_limit);
    lab_proc_session_message_t message = {.kind = LAB_SESSION_READY, .pid = child.pid,
                                         .status = ready ? 0u : (unsigned)ENOMEM};
    bool running = lab_proc_session_send(channel, &message, NULL) && ready;
    while (running) {
        int descriptors[LAB_SESSION_FDS];
        int received = lab_proc_session_receive(channel, &message, descriptors);
        if (received != 1) break;
        if (message.kind == LAB_SESSION_CANCEL) session_reap(&child);
        else if (message.kind == LAB_SESSION_JOB)
            running = session_forward(channel, &child, descriptors, memory_limit, max_jobs);
        else running = false;
        lab_proc_session_close(descriptors);
    }
    session_reap(&child);
    close(channel);
}
