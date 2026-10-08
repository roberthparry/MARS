/**
 * @file checks_fixtures.c
 * @brief Loopback listener and native fixture ownership for README executions.
 *
 * Reuses the unrelated HTTP, gRPC and WebSocket test servers. Only descriptor
 * three reaches a spawned fixture; other listeners retain close-on-exec. Peer
 * cleanup is bounded and escalates to SIGKILL before reaping when necessary.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* The build supplies an absolute path for its selected configuration. */
#ifndef MARS_HTTP_FIXTURE_PATH
#define MARS_HTTP_FIXTURE_PATH "tests/build/release/http/fixtures/http_fixture"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "checks_process.h"
#include "checks_fixtures.h"
#include "checks_fixtures_internal.h"

extern char **environ;

struct peer {
    pid_t process;
    int listener;
    unsigned port;
};

struct checks_fixtures {
    string_t *root;
    string_t *executable;
    struct peer peers[3];
};

/* Own a lazy set of protocol fixtures. */
checks_fixtures_t *checks_fixtures_new(const string_t *root)
{
    string_t *executable = checks_text(MARS_HTTP_FIXTURE_PATH);
    checks_fixtures_t *fixtures = checks_fixtures_new_with_executable(root, executable);
    string_free(executable);
    return fixtures;
}

/* Own an explicit executable path for isolated lifecycle tests. */
checks_fixtures_t *checks_fixtures_new_with_executable(const string_t *root, const string_t *executable)
{
    checks_fixtures_t *fixtures = calloc(1, sizeof(*fixtures));
    if (!fixtures)
        checks_fatal("allocating HTTP fixtures");
    fixtures->root = string_clone(root);
    fixtures->executable = string_clone(executable);
    if (!fixtures->root || !fixtures->executable)
        checks_fatal("copying HTTP fixture paths");
    for (size_t i = 0; i < 3; ++i)
        fixtures->peers[i].listener = -1;
    return fixtures;
}

/* Stop, reap and release owned local fixture processes. */
void checks_fixtures_free(checks_fixtures_t *fixtures)
{
    if (!fixtures)
        return;
    for (size_t i = 0; i < 3; ++i) {
        struct peer *peer = &fixtures->peers[i];
        if (peer->process > 0) {
            kill(peer->process, SIGTERM);
            bool reaped = false;
            for (unsigned attempt = 0; attempt < 500; ++attempt) {
                pid_t result = waitpid(peer->process, NULL, WNOHANG);
                if (result == peer->process || (result < 0 && errno == ECHILD)) {
                    reaped = true;
                    break;
                }
                struct timespec pause = {.tv_nsec = 10000000};
                nanosleep(&pause, NULL);
            }
            if (!reaped) {
                kill(peer->process, SIGKILL);
                while (waitpid(peer->process, NULL, 0) < 0 && errno == EINTR) {
                }
            }
        }
        if (peer->listener >= 0)
            close(peer->listener);
    }
    string_free(fixtures->root);
    string_free(fixtures->executable);
    free(fixtures);
}

static bool checks_fixtures_start_peer(checks_fixtures_t *fixtures, size_t index)
{
    if (checks_process_interrupt_signal()) {
        errno = ECANCELED;
        return false;
    }
    static const char *const protocols[] = {"http", "grpc", "websocket"};
    struct peer *peer = &fixtures->peers[index];
    if (peer->process > 0)
        return true;
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listener < 0)
        return false;
    /* Keep the source above the child's reserved descriptor even if stdin is closed. */
    int inherited = fcntl(listener, F_DUPFD_CLOEXEC, 4);
    int duplicate_error = errno;
    close(listener);
    if (inherited < 0) {
        errno = duplicate_error;
        return false;
    }
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    socklen_t size = sizeof(address);
    if (bind(inherited, (struct sockaddr *)&address, size) || listen(inherited, 8) ||
        getsockname(inherited, (struct sockaddr *)&address, &size)) {
        int error = errno;
        close(inherited);
        errno = error;
        return false;
    }
    char *argv[] = {(char *)string_c_str(fixtures->executable), (char *)protocols[index], "3", NULL};
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    bool ready = !error;
    pid_t process = 0;
    if (!error)
        error = posix_spawn_file_actions_adddup2(&actions, inherited, 3);
    if (!error)
        error = posix_spawn_file_actions_addclosefrom_np(&actions, 4);
    if (!error)
        error = posix_spawn(&process, argv[0], &actions, NULL, argv, environ);
    if (ready)
        posix_spawn_file_actions_destroy(&actions);
    if (error)
        close(inherited);
    else {
        peer->listener = inherited;
        peer->port = ntohs(address.sin_port);
        peer->process = process;
    }
    errno = error;
    return !error;
}

static bool checks_fixtures_add_url(checks_fixtures_t *fixtures, size_t index, const char *route,
                                    checks_strings_t *arguments)
{
    if (!checks_fixtures_start_peer(fixtures, index))
        return false;
    checks_strings_add(arguments, string_sprintf("http://127.0.0.1:%u%s", fixtures->peers[index].port, route));
    return true;
}

/* Supply protocol peers and disposable file operands exactly where documented. */
bool checks_fixtures_arguments(checks_fixtures_t *fixtures, const string_t *path, const string_t *code,
                               checks_strings_t *arguments)
{
    if (string_find(code, "http_request_set_grpc(") >= 0) {
        if (!checks_fixtures_add_url(fixtures, 1, "/mars.Test/Echo", arguments))
            return false;
    } else if (string_find(code, "http_websocket_open(") >= 0) {
        if (!checks_fixtures_add_url(fixtures, 2, "/echo", arguments))
            return false;
    }
    if (checks_equal(path, "docs/file.md")) {
        string_t *arity = NULL;
        if (checks_match(code, "argc[[:space:]]*(==|!=)[[:space:]]*([0-9]+)", 2, &arity)) {
            size_t count = 0;
            string_cursor_t *cursor = string_cursor_new(arity);
            bool valid = cursor && !string_cursor_done(cursor);
            /* README programmes take a small, bounded number of operands. */
            while (valid && !string_cursor_done(cursor)) {
                unsigned char digit;
                valid = string_cursor_peek_ascii(cursor, &digit) && digit >= '0' && digit <= '9';
                if (valid) {
                    count = count * 10 + (size_t)(digit - '0');
                    valid = count <= 256 && string_cursor_next(cursor) == 0;
                }
            }
            string_cursor_free(cursor);
            if (!valid || !count) {
                string_free(arity);
                return false;
            }
            --count;
            if (string_find(code, "symlink_listing_example") >= 0) {
                checks_strings_add(arguments, checks_text("directory"));
                checks_strings_add(arguments, checks_text("directory/documents"));
                checks_strings_add(arguments, checks_text("directory/shortcut"));
            } else {
                for (size_t i = 0; i < count; ++i)
                    checks_strings_add(arguments, string_sprintf("disposable-%zu", i));
            }
        }
        string_free(arity);
    }
    if (string_find(code, "https://httpbin.org/") >= 0)
        return checks_fixtures_add_url(
            fixtures, 0, string_find(code, "/get?message=MARS") >= 0 ? "/get?message=MARS" : "/post", arguments);
    return true;
}
