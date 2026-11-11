/**
 * @file lab_process_session.c
 * @brief Descriptor transport and reusable native calculation sessions.
 *
 * A private sequenced-packet socket passes a Protobuf request, anonymous standard input, an output pipe and an
 * opened working directory. Each job owns its numeric scope, environment snapshot, descriptors and control scope.
 * Only immutable or bounded module caches survive successful jobs. Descriptor I/O is required because the public
 * file API cannot adopt anonymous descriptors; request parsing uses the public Protobuf and string modules.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
/* POSIX and MARS both declare stack_t; keep their independent types distinct. */
#define stack_t posix_signal_stack_t
#include <signal.h>
#undef stack_t
#include <stdint.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "number-t.h"
#include "protobuf-t.h"
#include "string-t.h"
#include "lab_process.h"
#include "lab_worker.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_session.h"

/* Copy descriptors, never pointers, over the private local channel. */
bool lab_proc_session_send(int socket, const lab_proc_session_message_t *message, const int *descriptors)
{
    struct iovec data = {.iov_base = (void *)message, .iov_len = sizeof(*message)};
    union { struct cmsghdr alignment; unsigned char bytes[CMSG_SPACE(LAB_SESSION_FDS * sizeof(int))]; } control;
    memset(&control, 0, sizeof(control));
    struct msghdr packet = {.msg_iov = &data, .msg_iovlen = 1};
    if (descriptors) {
        packet.msg_control = control.bytes;
        packet.msg_controllen = sizeof(control.bytes);
        struct cmsghdr *rights = CMSG_FIRSTHDR(&packet);
        rights->cmsg_level = SOL_SOCKET; rights->cmsg_type = SCM_RIGHTS;
        rights->cmsg_len = CMSG_LEN(LAB_SESSION_FDS * sizeof(int));
        memcpy(CMSG_DATA(rights), descriptors, LAB_SESSION_FDS * sizeof(int));
    }
    ssize_t sent;
    do { sent = sendmsg(socket, &packet, MSG_NOSIGNAL); } while (sent < 0 && errno == EINTR);
    if (sent == (ssize_t)sizeof(*message)) return true;
    if (sent >= 0) errno = EPROTO;
    return false;
}

/* Dispose of every kernel-owned descriptor received with a job. */
void lab_proc_session_close(int descriptors[LAB_SESSION_FDS])
{
    if (!descriptors) return;
    for (size_t i = 0; i < LAB_SESSION_FDS; ++i) {
        if (descriptors[i] >= 0) close(descriptors[i]);
        descriptors[i] = -1;
    }
}

/* Validate framing before exposing any received ownership. */
int lab_proc_session_receive(int socket, lab_proc_session_message_t *message, int descriptors[LAB_SESSION_FDS])
{
    for (size_t i = 0; i < LAB_SESSION_FDS; ++i) descriptors[i] = -1;
    struct iovec data = {.iov_base = message, .iov_len = sizeof(*message)};
    union { struct cmsghdr alignment; unsigned char bytes[CMSG_SPACE(LAB_SESSION_FDS * sizeof(int))]; } control;
    memset(&control, 0, sizeof(control));
    struct msghdr packet = {.msg_iov = &data, .msg_iovlen = 1,
                            .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes)};
    ssize_t got;
    do { got = recvmsg(socket, &packet, MSG_CMSG_CLOEXEC); } while (got < 0 && errno == EINTR);
    if (got <= 0) return (int)got;
    size_t count = 0;
    bool valid = got == (ssize_t)sizeof(*message) && !(packet.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
    for (struct cmsghdr *rights = CMSG_FIRSTHDR(&packet); rights; rights = CMSG_NXTHDR(&packet, rights)) {
        if (rights->cmsg_level != SOL_SOCKET || rights->cmsg_type != SCM_RIGHTS ||
            rights->cmsg_len < CMSG_LEN(0)) { valid = false; continue; }
        size_t bytes = rights->cmsg_len - CMSG_LEN(0), received = bytes / sizeof(int);
        int *values = (int *)CMSG_DATA(rights);
        valid = valid && bytes % sizeof(int) == 0;
        for (size_t i = 0; i < received; ++i) {
            if (count < LAB_SESSION_FDS) descriptors[count++] = values[i];
            else { close(values[i]); valid = false; }
        }
    }
    valid = valid && message->kind >= LAB_SESSION_JOB && message->kind <= LAB_SESSION_READY &&
            count == (message->kind == LAB_SESSION_JOB ? LAB_SESSION_FDS : 0);
    if (valid) return 1;
    lab_proc_session_close(descriptors);
    errno = EPROTO;
    return -1;
}

static string_t *session_text(const protobuf_t *request, size_t index)
{
    size_t size = 0;
    const char *bytes = protobuf_bytes(request, index, &size);
    if (protobuf_wire(request, index) != PROTOBUF_BYTES || (size && (!bytes || memchr(bytes, 0, size))))
        return NULL;
    string_t *text = string_new();
    if (text && string_append_bytes(text, bytes, size)) { string_free(text); text = NULL; }
    return text;
}

static protobuf_t *session_request(int descriptor)
{
    struct stat info;
    if (fstat(descriptor, &info) || info.st_size <= 0 || info.st_size > LAB_SESSION_MAX_REQUEST) return NULL;
    size_t size = (size_t)info.st_size, offset = 0;
    unsigned char *bytes = malloc(size);
    while (bytes && offset < size) {
        ssize_t got = pread(descriptor, bytes + offset, size - offset, (off_t)offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        offset += (size_t)got;
    }
    protobuf_t *request = bytes && offset == size ? protobuf_decode(bytes, size, LAB_SESSION_MAX_REQUEST, 4608) : NULL;
    free(bytes);
    return request;
}

static bool session_environment(const protobuf_t *request, string_t **previous)
{
    string_t *snapshot = string_new();
    bool ok = snapshot != NULL;
    size_t count = protobuf_count(request), environment_count = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        if (protobuf_field(request, i) != 2) continue;
        string_t *text = session_text(request, i);
        ok = text && ++environment_count <= 4096 && string_find(text, "=") > 0 &&
             !string_append_string(snapshot, text) && !string_append_char(snapshot, '\0');
        string_free(text);
    }
    bool same = ok && *previous && string_byte_length(snapshot) == string_byte_length(*previous) &&
                !memcmp(string_c_str(snapshot), string_c_str(*previous), string_byte_length(snapshot));
    if (ok && !same) {
        ok = clearenv() == 0;
        for (size_t i = 0; ok && i < count; ++i) {
            if (protobuf_field(request, i) != 2) continue;
            string_t *text = session_text(request, i);
            string_offset_t split = text ? string_find(text, "=") : -1;
            string_t *name = split > 0 ? string_substr(text, 0, (size_t)split) : NULL;
            ok = name && !setenv(string_c_str(name), string_c_str(text) + split + 1, 1);
            string_free(name); string_free(text);
        }
        if (ok) { string_free(*previous); *previous = snapshot; snapshot = NULL; }
    }
    string_free(snapshot);
    return ok;
}

static int session_calculate(const protobuf_t *request, string_t **environment)
{
    string_t *arguments[256] = {0};
    char *controlled[260] = {0}, timeout_text[32];
    uint64_t timeout = 0;
    size_t count = protobuf_count(request), argc = 0;
    bool ok = count > 0, have_timeout = false;
    for (size_t i = 0; ok && i < count; ++i) {
        uint32_t field = protobuf_field(request, i);
        if (field == 1) {
            ok = argc < 255 && (arguments[argc] = session_text(request, i)) != NULL;
            if (ok) { controlled[argc + 2] = (char *)string_c_str(arguments[argc]); ++argc; }
        } else if (field == 3) {
            ok = !have_timeout && protobuf_integer(request, i, &timeout) && timeout <= UINT_MAX;
            have_timeout = true;
        } else if (field != 2) ok = false;
    }
    ok = ok && argc && have_timeout && session_environment(request, environment);
    int status = LAB_WORKER_CONTROL_FAILED;
    if (ok) {
        snprintf(timeout_text, sizeof(timeout_text), "%u", (unsigned)timeout);
        controlled[0] = timeout_text;
        const char *limit = getenv("MARS_LAB_WORK_LIMIT");
        controlled[1] = (char *)(limit && *limit ? limit : "0");
        size_t precision = num_get_default_prec_bits();
        {
            NUM_SCOPE(job);
            status = job ? lab_worker_dispatch_controlled((int)argc + 2, controlled) : LAB_WORKER_CONTROL_FAILED;
        }
        num_set_default_prec_bits(precision);
    }
    for (size_t i = 0; i < argc; ++i) string_free(arguments[i]);
    return status;
}

/* A successful calculation retains module caches while every request-owned numeric scope is released. */
void lab_proc_session_worker(int channel, size_t memory_limit)
{
    lab_proc_set_cancel_check(NULL, NULL);
    lab_proc_set_cancel_flag(NULL);
    bool ready = lab_proc_memory_limit(memory_limit);
    int idle = open("/dev/null", O_RDWR | O_CLOEXEC), base = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    lab_proc_session_message_t message = {.kind = LAB_SESSION_READY, .pid = getpid(),
                                         .status = ready && idle >= 0 && base >= 0 ? 0u : (unsigned)ENOMEM};
    bool running = lab_proc_session_send(channel, &message, NULL) && !message.status;
    string_t *environment = NULL;
    while (running) {
        int descriptors[LAB_SESSION_FDS];
        if (lab_proc_session_receive(channel, &message, descriptors) != 1) break;
        bool job = message.kind == LAB_SESSION_JOB;
        protobuf_t *request = job ? session_request(descriptors[0]) : NULL;
        bool installed = request && !fchdir(descriptors[3]) && dup2(descriptors[1], STDIN_FILENO) >= 0 &&
                         dup2(descriptors[2], STDOUT_FILENO) >= 0 && dup2(descriptors[2], STDERR_FILENO) >= 0;
        clearerr(stdin); clearerr(stdout); clearerr(stderr);
        __fpurge(stdin);
        if (installed) installed = fseeko(stdin, 0, SEEK_SET) == 0;
        int status = installed ? session_calculate(request, &environment) : LAB_WORKER_CONTROL_FAILED;
        fflush(stdout); fflush(stderr);
        bool restored = idle >= 0 && dup2(idle, STDIN_FILENO) >= 0 && dup2(idle, STDOUT_FILENO) >= 0 &&
                        dup2(idle, STDERR_FILENO) >= 0 && base >= 0 && !fchdir(base);
        clearerr(stdin); clearerr(stdout); clearerr(stderr);
        protobuf_free(request);
        lab_proc_session_close(descriptors);
        struct rusage usage = {0};
        getrusage(RUSAGE_SELF, &usage);
        message = (lab_proc_session_message_t){.kind = LAB_SESSION_FINISHED, .status = (unsigned)status,
            .pid = getpid(), .resident_bytes = (uint64_t)usage.ru_maxrss * 1024u};
        running = job && restored && lab_proc_session_send(channel, &message, NULL);
    }
    string_free(environment);
    if (idle >= 0) close(idle);
    if (base >= 0) close(base);
    close(channel);
}
