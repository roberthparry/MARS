/**
 * @file lab_surface_service.c
 * @brief Bounded local IPC for a single resident native shoreline dataset.
 *
 * The launcher forks this lazy owner before its HTTP workers. Each worker has
 * a separate anonymous channel, preventing response interleaving without locks.
 * Polling serves ready channels fairly; malformed or disconnected peers lose
 * their channel. Other HTTP routes never wait for geometry preparation.
 */
/* This unit needs ordinary signals, not the XSI alternate-stack typedef which
 * collides with the library's public stack_t container name. */
#undef _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "array-t.h"
#include "lab_calendar.h"
#include "lab_wire.h"
#include "lab_surface_service.h"

static int surface_channels[8];
static unsigned surface_channel_count;
static int surface_channel = -1;
static bool surface_selected;

static bool surface_transfer(int fd, void *buffer, size_t size, bool sending)
{
    unsigned char *bytes = buffer;
    while (size) {
        ssize_t count = sending ? send(fd, bytes, size, MSG_NOSIGNAL) : recv(fd, bytes, size, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        bytes += count;
        size -= (size_t)count;
    }
    return true;
}

static bool surface_send(int fd, const json_t *value, unsigned status)
{
    bool failed = false;
    array_t *bytes = lab_wire_encode_response(value, &failed);
    size_t size = bytes ? array_size(bytes) : 0;
    uint32_t header[] = {(uint32_t)size, failed ? 500 : status};
    bool ok = size && size <= 8388608 &&
              surface_transfer(fd, header, sizeof header, true) &&
              surface_transfer(fd, array_get(bytes, 0), size, true);
    array_destroy(bytes);
    return ok;
}

static json_t *surface_receive(int fd, unsigned *status, size_t maximum)
{
    uint32_t header[2];
    if (!surface_transfer(fd, header, sizeof header, false) || !header[0] || header[0] > maximum) return NULL;
    unsigned char *bytes = malloc(header[0]);
    json_t *value = bytes && surface_transfer(fd, bytes, header[0], false) ? lab_wire_decode(bytes, header[0]) : NULL;
    free(bytes);
    if (value && status) *status = header[1];
    return value;
}

static void surface_run(struct pollfd peers[8], unsigned workers)
{
    string_t *route = string_new_with("/almanac-globe");
    unsigned remaining = workers;
    bool session = lab_cal_surface_open();
    while (route && session && remaining) {
        int ready = poll(peers, workers, -1);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) break;
        for (unsigned i = 0; i < workers; ++i) {
            if (peers[i].fd < 0 || !peers[i].revents) continue;
            unsigned status = 200;
            json_t *payload = surface_receive(peers[i].fd, NULL, 8388608);
            json_t *result = payload ? lab_cal_request(route, payload, &status) : NULL;
            bool ok = result && surface_send(peers[i].fd, result, status);
            json_free(result);
            json_free(payload);
            if (!ok) {
                close(peers[i].fd);
                peers[i].fd = -1;
                --remaining;
            }
        }
    }
    string_free(route);
    lab_cal_surface_close();
    for (unsigned i = 0; i < workers; ++i) if (peers[i].fd >= 0) close(peers[i].fd);
}

/* Create private channels before the launcher forks request processes. */
pid_t lab_surface_service_start(unsigned workers)
{
    if (!workers || workers > 8 || surface_channel_count) return -1;
    struct pollfd peers[8];
    for (unsigned i = 0; i < workers; ++i) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair)) {
            for (unsigned j = 0; j < i; ++j) { close(surface_channels[j]); close(peers[j].fd); }
            return -1;
        }
        /* Bound both a dead peer and a blocked response; this is not a geometry job deadline. */
        struct timeval timeout = {.tv_sec = 180};
        for (unsigned j = 0; j < 2; ++j) {
            setsockopt(pair[j], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
            setsockopt(pair[j], SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
        }
        surface_channels[i] = pair[0];
        peers[i] = (struct pollfd){.fd = pair[1], .events = POLLIN};
    }
    pid_t parent = getpid(), child = fork();
    if (!child) {
        signal(SIGTERM, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) || getppid() != parent) _exit(1);
        for (unsigned i = 0; i < workers; ++i) close(surface_channels[i]);
        surface_run(peers, workers);
        _exit(0);
    }
    for (unsigned i = 0; i < workers; ++i) {
        close(peers[i].fd);
        if (child < 0) close(surface_channels[i]);
    }
    if (child > 0) surface_channel_count = workers;
    return child;
}

/* A request process owns exactly one stream; the supervisor retains none. */
void lab_surface_service_select(unsigned worker)
{
    surface_selected = true;
    for (unsigned i = 0; i < surface_channel_count; ++i) {
        if (i == worker) surface_channel = surface_channels[i];
        else close(surface_channels[i]);
    }
    surface_channel_count = 0;
}

/* Keep direct adapter tests independent of the launcher's process topology. */
json_t *lab_surface_service_request(const json_t *payload, unsigned *status)
{
    if (!surface_selected) {
        string_t *route = string_new_with("/almanac-globe");
        json_t *result = route ? lab_cal_request(route, payload, status) : NULL;
        string_free(route);
        return result;
    }
    json_t *result = surface_channel >= 0 && surface_send(surface_channel, payload, 200)
        ? surface_receive(surface_channel, status, 32 * 1048576) : NULL;
    if (!result && surface_channel >= 0) {
        close(surface_channel);
        surface_channel = -1;
    }
    return result;
}
