/**
 * @file test_lab_browser.c
 * @brief Real-browser checks of the native Lab and its extracted browser scripts.
 *
 * Starts an ephemeral loopback server and headless Firefox with a private profile.
 * Browser assertions are injected into a disposable template, never the installed
 * page. Results return through the private fixture's state endpoint. No public
 * service or running user Lab is accessed. The bounded fixture removes its files.
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
#include "lab_server.h"
#include "lab_state.h"
#include "test_harness.h"
#include "test_lab_browser_fixtures.h"
#include "test_lab_support.h"

extern char **environ;
static const char *lab_browser_executable;
TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static file_t *lab_browser_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool lab_browser_prepare(const char *directory)
{
    file_t *source = file_new_cstr("tools/mars_lab/assets/index.html");
    file_t *checks = file_new_cstr("tools/mars_lab/tests/browser/checks.js");
    file_t *page = lab_browser_file(directory, "index.html");
    string_t *html = source ? file_read_all_text(source) : NULL;
    string_t *script = checks ? file_read_all_text(checks) : NULL;
    static const char *const modules[] = {"profile_checks.js",        "workspace_checks.js",
                                          "request_checks.js",        "forms_checks.js",
                                          "layout_checks.js",         "syntax_checks.js",
                                          "calendar_checks.js",       "persist_checks.js",
                                          "widgets_checks.js",        "evaluation_checks.js",
                                          "select_checks.js",         "projection_checks.js",
                                          "workspace_dom_checks.js",  "location_dom_checks.js",
                                          "result_dom_checks.js",     "binding_dom_checks.js",
                                          "payload_checks.js",        "picker_dom_checks.js",
                                          "storage_checks.js",        "evaluation_cards_checks.js",
                                          "event_checks.js",          "widget_event_checks.js",
                                          "select_event_checks.js",   "almanac_event_checks.js",
                                          "binding_sync_checks.js",   "evaluation_install_checks.js",
                                          "goal_checks.js",           "evaluation_setup_checks.js",
                                          "weather_checks.js",        "binding_commit_checks.js",
                                          "function_checks.js",       "result_flow_checks.js",
                                          "binding_flow_checks.js",   "location_flow_checks.js",
                                          "request_flow_checks.js",   "state_flow_checks.js",
                                          "transport_flow_checks.js", "editor_checks.js"};
    for (size_t i = 0; script && i < sizeof modules / sizeof *modules; ++i) {
        file_t *module = lab_browser_file("tools/mars_lab/tests/browser", modules[i]);
        string_t *text = module ? file_read_all_text(module) : NULL;
        bool ok = text && !string_append_char(script, '\n') && !string_append_cstr(script, string_c_str(text));
        string_free(text);
        file_free(module);
        if (!ok) {
            string_free(script);
            script = NULL;
        }
    }
    string_t *injection = script && lab_browser_almanac_fixture(script)
                              ? string_sprintf("<script>\n%s\n</script>\n</head>", string_c_str(script))
                              : NULL;
    bool replaced = html && injection && string_find(html, "</head>") >= 0 &&
                    string_replace(html, "</head>", string_c_str(injection)) == 0;
    bool written = replaced && page && file_write_all_text(page, html);
    bool configured = written && !setenv("MARS_LAB_ASSET_FILE", file_path(page), 1);
    json_t *initial = test_lab_json("{\"expression\":\"2+3\",\"expression_updated_at\":1,\"lab_mode\":\"expression\"}");
    bool saved = configured && initial && lab_state_save(initial);
    bool ok = configured && saved;
    if (!ok)
        string_fprintf(stderr, "Browser fixture: replaced=%d written=%d configured=%d saved=%d errno=%d\n", replaced,
                       written, configured, saved, errno);
    json_free(initial);
    string_free(injection);
    string_free(script);
    string_free(html);
    file_free(page);
    file_free(checks);
    file_free(source);
    return ok;
}

static bool lab_browser_run(const char *directory)
{
    if (!lab_browser_prepare(directory) || !test_lab_catalogue_database(directory))
        return false;
    file_t *profile = lab_browser_file(directory, "profile");
    bool profile_ready = profile && file_create_directory(profile, 0700, false);
    file_t *preferences = profile_ready ? lab_browser_file(file_path(profile), "user.js") : NULL;
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
    lab_server_t *server = profile_ready ? lab_svr_new(NULL, 0, 30000) : NULL;
    bool profiling = getenv("MARS_LAB_PROFILE") && !strcmp(getenv("MARS_LAB_PROFILE"), "1");
    string_t *url =
        server ? string_sprintf("http://127.0.0.1:%u/%s", lab_svr_port(server), profiling ? "?profile=1" : "") : NULL;
    pid_t workers[4] = {-1, -1, -1, -1};
    bool workers_ready = server != NULL;
    for (size_t i = 0; workers_ready && i < sizeof workers / sizeof *workers; ++i) {
        workers[i] = fork();
        if (!workers[i]) {
            for (;;) {
                if (lab_svr_serve_once(server, 1000) < 0 && errno == EBADF)
                    _exit(1);
            }
        }
        workers_ready = workers[i] > 0;
    }
    pid_t child = -1;
    posix_spawnattr_t attr;
    bool attr_ready = !posix_spawnattr_init(&attr);
    bool ready = workers_ready && url && attr_ready && !posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP) &&
                 !posix_spawnattr_setpgroup(&attr, 0);
    if (ready) {
        char *const args[] = {
            (char *)lab_browser_executable, "--headless", "--no-remote", "--profile", (char *)file_path(profile),
            (char *)string_c_str(url),      NULL};
        ready = !posix_spawnp(&child, lab_browser_executable, NULL, &attr, args, environ);
    }
    if (attr_ready)
        posix_spawnattr_destroy(&attr);
    bool passed = false, reaped = false;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (ready) {
        json_t *state = lab_state_load();
        const char *result = test_lab_text(state, "expression");
        bool finished = !strncmp(result, "BROWSER ", 8);
        if (finished) {
            passed = !strncmp(result, "BROWSER PASS:", 13);
            string_fprintf(stderr, "%s\n", result);
        }
        json_free(state);
        if (finished)
            break;
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
    lab_svr_free(server);
    file_free(profile);
    if (!passed)
        string_fprintf(stderr, "Browser suite failed or timed out; check the browser output above.\n");
    return passed;
}

static void test_lab_browser(void)
{
    lab_browser_executable = getenv("MARS_LAB_BROWSER");
    if (!lab_browser_executable || !*lab_browser_executable)
        lab_browser_executable = "firefox";
    TEST_ASSERT_TRUE(test_lab_isolated(lab_browser_run), "real browser loads assets and exercises the native Lab");
}

/* Run only when the browser-test target explicitly requests this optional dependency. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_lab_browser, tests, NULL);
    return 0;
}
