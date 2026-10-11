/**
 * @file test_tba_browser.c
 * @brief Real Firefox regressions for private Lab WASM and encrypted Protobuf.
 *
 * Copies assets into a private temporary root, injects browser assertions and
 * starts two loopback workers. No installed Lab, user settings or public service
 * is used. The bounded fixture reaps every child and removes its private files.
 */
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "webserver.h"
#include "tba_server.h"
#include "test_harness.h"
#include "test_tba_support.h"

extern char **environ;
static const char *tba_browser_executable;
TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static file_t *tba_browser_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool tba_browser_prepare(const char *directory, uint16_t report_port)
{
    file_t *assets = tba_browser_file(directory, "tools/to_be_announced_lab/assets");
    file_t *scripts = assets ? tba_browser_file(file_path(assets), "js") : NULL;
    bool ok = scripts && file_create_directory(scripts, 0700, true);
    static const char *const names[] = {"index.html", "index.css", "js/transport.js"};
    for (size_t i = 0; ok && i < sizeof names / sizeof *names; ++i) {
        file_t *source = tba_browser_file("tools/to_be_announced_lab/assets", names[i]);
        file_t *target = tba_browser_file(file_path(assets), names[i]);
        ok = source && target && file_copy(source, target, false);
        file_free(source);
        file_free(target);
    }
    file_t *page = assets ? tba_browser_file(file_path(assets), "index.html") : NULL;
    file_t *checks = file_new_cstr("tools/to_be_announced_lab/tests/browser/checks.js");
    string_t *html = ok && page ? file_read_all_text(page) : NULL;
    string_t *script = checks ? file_read_all_text(checks) : NULL;
    string_t *injection = script ? string_sprintf("<script type=\"module\">\nconst reportPort = %u;\n%S\n</script></body>",
                                                 report_port, script) : NULL;
    ok = html && injection && string_replace(html, "</body>", string_c_str(injection)) == 0 &&
         file_write_all_text(page, html) && !setenv("MARS_ROOT", directory, 1);
    string_free(injection);
    string_free(script);
    string_free(html);
    file_free(checks);
    file_free(page);
    file_free(scripts);
    file_free(assets);
    return ok;
}

static bool tba_browser_report(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    string_t *report = context;
    return string_append_utf8_exact(report, websrv_request_body(request), websrv_request_body_size(request)) == 0 &&
           websrv_response_status(response, 204);
}

static pid_t tba_browser_worker(tba_server_t *server)
{
    pid_t child = fork();
    if (!child) {
        for (;;) {
            int status = tba_svr_once(server);
            if (status == 2)
                _exit(0);
            if (status < 0 && errno == EBADF)
                _exit(1);
        }
    }
    return child;
}

static bool tba_browser_run(const char *directory)
{
    string_t *address = string_new_with("127.0.0.1"), *route = string_new_with("/result");
    string_t *report = string_new();
    websrv_limits_t limits = {.max_body_bytes = 8192, .timeout_ms = 1000};
    websrv_t *collector = address ? websrv_new(address, 0, &limits) : NULL;
    bool prepared = collector && route && report &&
                    websrv_route(collector, HTTP_POST, route, tba_browser_report, report) &&
                    tba_browser_prepare(directory, websrv_port(collector));
    string_free(address);
    string_free(route);
    file_t *profile = tba_browser_file(directory, "profile");
    bool profile_ready = prepared && profile && file_create_directory(profile, 0700, false);
    file_t *preferences = profile_ready ? tba_browser_file(file_path(profile), "user.js") : NULL;
    string_t *settings = string_new_with("user_pref(\"dom.ipc.processCount\", 1);\n"
                                         "user_pref(\"fission.autostart\", false);\n"
                                         "user_pref(\"browser.startup.page\", 0);\n"
                                         "user_pref(\"browser.shell.checkDefaultBrowser\", false);\n"
                                         "user_pref(\"browser.startup.homepage_override.mstone\", \"ignore\");\n"
                                         "user_pref(\"datareporting.policy.dataSubmissionEnabled\", false);\n"
                                         "user_pref(\"toolkit.telemetry.enabled\", false);\n");
    profile_ready = preferences && settings && file_write_all_text(preferences, settings);
    string_free(settings);
    file_free(preferences);
    tba_app_t *app = profile_ready ? tba_app_new("/to-be-announced") : NULL;
    tba_server_t *server = app ? tba_svr_new(app, "127.0.0.1", 0) : NULL;
    string_t *url = server ? string_sprintf("http://127.0.0.1:%u/to-be-announced/", tba_svr_port(server)) : NULL;
    pid_t workers[2] = {-1, -1};
    bool workers_ready = server != NULL;
    for (size_t i = 0; workers_ready && i < sizeof workers / sizeof *workers; ++i) {
        workers[i] = tba_browser_worker(server);
        workers_ready = workers[i] > 0;
    }
    pid_t child = -1;
    posix_spawnattr_t attr;
    bool attr_ready = !posix_spawnattr_init(&attr);
    bool ready = workers_ready && url && attr_ready && !posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP) &&
                 !posix_spawnattr_setpgroup(&attr, 0);
    if (ready) {
        char *const args[] = {
            (char *)tba_browser_executable, "--headless", "--no-remote", "--profile", (char *)file_path(profile),
            (char *)string_c_str(url),      NULL};
        ready = !posix_spawnp(&child, tba_browser_executable, NULL, &attr, args, environ);
    }
    if (attr_ready)
        posix_spawnattr_destroy(&attr);
    bool passed = false, reaped = false;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (ready) {
        websrv_serve_once(collector, 50);
        const char *result = string_c_str(report);
        bool finished = !strncmp(result, "BROWSER ", 8);
        if (finished) {
            passed = !strncmp(result, "BROWSER PASS:", 13);
            string_fprintf(stderr, "%s\n", result);
        }
        if (finished)
            break;
        for (size_t i = 0; i < sizeof workers / sizeof *workers; ++i)
            if (workers[i] > 0 && waitpid(workers[i], NULL, WNOHANG) == workers[i])
                workers[i] = tba_browser_worker(server);
        int status;
        if (waitpid(child, &status, WNOHANG) == child) {
            string_fprintf(stderr, "Browser exited before reporting: wait status=%d\n", status);
            reaped = true;
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 180)
            break;
        struct timespec pause = {.tv_nsec = 100000000};
        nanosleep(&pause, NULL);
    }
    for (size_t i = 0; i < sizeof workers / sizeof *workers; ++i) {
        if (workers[i] > 0) {
            kill(workers[i], SIGTERM);
            while (waitpid(workers[i], NULL, 0) < 0 && errno == EINTR) {
            }
        }
    }
    if (child > 0) {
        kill(-child, SIGTERM);
        struct timespec pause = {.tv_nsec = 20000000};
        for (unsigned attempt = 0; !reaped && attempt < 100; ++attempt) {
            reaped = waitpid(child, NULL, WNOHANG) == child;
            if (!reaped)
                nanosleep(&pause, NULL);
        }
        if (!reaped) {
            kill(-child, SIGKILL);
            while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {
            }
        }
    }
    string_free(url);
    tba_svr_free(server);
    tba_app_free(app);
    websrv_free(collector);
    string_free(report);
    file_free(profile);
    if (!passed)
        string_fprintf(stderr, "Browser suite failed or timed out; check the browser output above.\n");
    return passed;
}

static void test_tba_browser(void)
{
    const char *parent = getenv("TMPDIR");
    string_t *template = string_sprintf("%s/mars-tba-browser-XXXXXX", parent && *parent ? parent : "/tmp");
    char *directory = template ? strdup(string_c_str(template)) : NULL;
    string_free(template);
    TEST_ASSERT_TRUE(directory && mkdtemp(directory) != NULL, "private browser root created");
    string_t *state = string_sprintf("%s/state.pb", directory);
    string_t *uploads = string_sprintf("%s/uploads", directory);
    bool ready = state && uploads && !setenv("MARS_LAB_STATE_FILE", string_c_str(state), 1) &&
                 !setenv("TBA_LAB_UPLOAD_DIR", string_c_str(uploads), 1);
    tba_browser_executable = getenv("MARS_LAB_BROWSER");
    if (!tba_browser_executable || !*tba_browser_executable)
        tba_browser_executable = "firefox";
    bool ok = ready && tba_browser_run(directory);
    file_t *state_file = state ? file_new(state) : NULL;
    file_t *upload_dir = uploads ? file_new(uploads) : NULL;
    ok = state_file && upload_dir && !file_exists(state_file) && !file_exists(upload_dir) && ok;
    file_free(state_file);
    file_free(upload_dir);
    string_free(state);
    string_free(uploads);
    file_t *root = file_new_cstr(directory);
    bool removed = root && test_tba_cleanup(root);
    file_free(root);
    free(directory);
    TEST_ASSERT_TRUE(removed, "temporary browser fixture removed");
    TEST_ASSERT_TRUE(ok, "browser exercises WASM and native Protobuf routes");
}

/* This optional suite requires Firefox and never accesses a public service. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_tba_browser, tests, NULL);
    return TEST_EXIT_CODE();
}
