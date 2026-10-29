/**
 * @file mars_settings_process.c
 * @brief Process isolation for encrypted configuration during bulk database work.
 *
 * Starts one helper before the application opens stores or creates threads. Only that helper reads
 * encrypted settings and publishes setting updates. A bounded, private socket carries copied values;
 * credentials never enter command arguments, temporary files or diagnostic output. SQLCipher's global
 * memory-locking allocator therefore remains confined to the small settings process. Requests are
 * serialised, helper failures fail closed, and the owning process closes and reaps its helper at exit.
 */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sodium.h>

#include "string-t.h"
#include "mars_settings.h"
#define MARS_SETTINGS_INTERNAL_ACCESS
#include "mars_settings_internal.h"

static const uint64_t settings_message_limit = 16777216;
static int settings_socket = -1;
static pid_t settings_owner, settings_child;
static bool settings_isolated;
static pthread_mutex_t settings_process_lock = PTHREAD_MUTEX_INITIALIZER;

/* Socket I/O is private process communication, not filesystem persistence. */
static bool settings_transfer(int socket, void *buffer, size_t size, bool sending)
{
    unsigned char *bytes = buffer;
    while (size) {
        ssize_t count = sending ? send(socket, bytes, size, MSG_NOSIGNAL) : recv(socket, bytes, size, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        bytes += count;
        size -= (size_t)count;
    }
    return true;
}

static void settings_release(string_t *text)
{
    if (text) sodium_memzero((void *)string_c_str(text), string_byte_length(text));
    string_free(text);
}

/* The two processes share the native integer representation; this is not a persisted file format. */
static bool settings_send(int socket, uint64_t code, const string_t *text)
{
    uint64_t header[] = {code, text ? string_byte_length(text) : 0};
    return header[1] <= settings_message_limit && settings_transfer(socket, header, sizeof header, true) &&
           (!header[1] || settings_transfer(socket, (void *)string_c_str(text), (size_t)header[1], true));
}

static bool settings_receive(int socket, uint64_t *code, string_t **text)
{
    uint64_t header[2];
    *text = NULL;
    if (!settings_transfer(socket, header, sizeof header, false) || header[1] > settings_message_limit) return false;
    *code = header[0];
    if (!header[1]) return true;
    size_t size = (size_t)header[1];
    char *bytes = malloc(size);
    bool ok = bytes && settings_transfer(socket, bytes, size, false) && !memchr(bytes, 0, size);
    string_t *value = ok ? string_new() : NULL;
    ok = value && !string_append_utf8_exact(value, bytes, size);
    if (bytes) sodium_memzero(bytes, size);
    free(bytes);
    if (ok) *text = value;
    else settings_release(value);
    return ok;
}

static bool settings_load_request(const string_t *input, string_t **output)
{
    if (input) return false;
    *output = mars_settings_snapshot();
    return *output != NULL;
}

static bool settings_resource_request(const string_t *input, string_t **output)
{
    if (!input || string_byte_length(input) > 128) return false;
    *output = mars_settings_resource(string_c_str(input));
    return true;
}

static bool settings_sync_request(const string_t *input, string_t **output)
{
    (void)output;
    return input && mars_settings_sync(input);
}

static void settings_serve(int socket)
{
    static bool (*const handlers[])(const string_t *, string_t **) = {
        [MARS_SETTINGS_LOAD]     = settings_load_request,
        [MARS_SETTINGS_RESOURCE] = settings_resource_request,
        [MARS_SETTINGS_SYNC]     = settings_sync_request
    };
    for (;;) {
        uint64_t request;
        string_t *input = NULL, *output = NULL;
        if (!settings_receive(socket, &request, &input)) break;
        bool ok = request < MARS_SETTINGS_REQUEST_COUNT && handlers[request](input, &output);
        bool sent = settings_send(socket, ok, ok ? output : NULL);
        settings_release(input);
        settings_release(output);
        if (!sent) break;
    }
    close(socket);
    mars_settings_key_provider(NULL);
    _exit(0);
}

/* Shut down only this process's helper; inherited client descriptors never confer ownership. */
void mars_settings_isolation_close(void)
{
    if (settings_owner != getpid()) return;
    pthread_mutex_lock(&settings_process_lock);
    if (settings_socket >= 0) {
        shutdown(settings_socket, SHUT_RDWR);
        close(settings_socket);
        settings_socket = -1;
    }
    if (settings_child > 0) {
        while (waitpid(settings_child, NULL, 0) < 0 && errno == EINTR) {}
        settings_child = 0;
    }
    pthread_mutex_unlock(&settings_process_lock);
}

/* Fork exactly once, before caller-owned database connections or worker threads exist. */
bool mars_settings_isolate(void)
{
    if (settings_isolated) return settings_owner == getpid() && settings_socket >= 0;
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets)) return false;
    pid_t child = fork();
    if (!child) {
        close(sockets[0]);
        settings_serve(sockets[1]);
    }
    close(sockets[1]);
    if (child < 0) { close(sockets[0]); return false; }
    settings_socket = sockets[0];
    settings_owner = getpid();
    settings_child = child;
    settings_isolated = true;
    if (atexit(mars_settings_isolation_close)) {
        mars_settings_isolation_close();
        return false;
    }
    return true;
}

/* Retain the isolation decision after failure, so access never falls back to in-process decryption. */
bool mars_settings_process_enabled(void)
{
    return settings_isolated;
}

/* Keep a complete request and its reply under one lock; a failed channel is permanently closed. */
bool mars_settings_process_request(mars_settings_request_t request, const string_t *input, string_t **output)
{
    *output = NULL;
    if ((unsigned)request >= MARS_SETTINGS_REQUEST_COUNT || settings_owner != getpid()) return false;
    pthread_mutex_lock(&settings_process_lock);
    uint64_t status = 0;
    bool transported = settings_socket >= 0 && settings_send(settings_socket, request, input) &&
                       settings_receive(settings_socket, &status, output);
    if (!transported && settings_socket >= 0) {
        close(settings_socket);
        settings_socket = -1;
        if (settings_child > 0) kill(settings_child, SIGTERM);
    }
    bool ok = transported && status == 1;
    if (!ok) { settings_release(*output); *output = NULL; }
    pthread_mutex_unlock(&settings_process_lock);
    return ok;
}
