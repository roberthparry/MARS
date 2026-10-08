/**
 * @file tba_main.c
 * @brief Linux native forecasting Lab command line and prefork supervision.
 *
 * Starts four single-threaded HTTP workers, preserving existing desktop options
 * and port 8766. Signal-driven shutdown cancels active native forecast processes
 * and reaps children. It never starts Python or changes Tailscale configuration.
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

#include "lab_process.h"
#include "tba_server.h"

extern char **environ;
static volatile sig_atomic_t tba_main_stopping;

static void tba_main_signal(int signal_number)
{
    (void)signal_number;
    tba_main_stopping = 1;
}

static pid_t tba_main_spawn_worker(tba_server_t *server, tba_app_t *app)
{
    pid_t child = fork();
    if (!child) {
        lab_proc_set_cancel_flag(&tba_main_stopping);
        int result = 0;
        while (!tba_main_stopping) {
            int served = tba_svr_once(server);
            if (served < 0 && errno == EBADF) {
                result = 1;
                break;
            }
        }
        tba_svr_free(server);
        tba_app_free(app);
        exit(result);
    }
    return child;
}

static void tba_main_browser(const char *browser, const char *url)
{
    char *const argv[] = {(char *)(browser && *browser ? browser : "xdg-open"), (char *)url, NULL};
    pid_t child;
    int status = posix_spawnp(&child, argv[0], NULL, NULL, argv, environ);
    if (status)
        string_fprintf(stderr, "Could not open the browser: %s\n", strerror(status));
}

/* Launch the native workbench without an interpreter or shell command construction. */
int main(int argc, char **argv)
{
    const char *host = "::", *browser = "", *binary = NULL, *base = NULL;
    bool open_browser = true;
    uint16_t port = 8766;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-browser")) {
            open_browser = false;
            continue;
        }
        if (!strcmp(argv[i], "--help")) {
            string_printf("To-Be-Announced Lab (native C)\n"
                          "Options: --host ADDRESS --port PORT --no-browser --browser PROGRAM --binary PATH --base-path PATH\n");
            return 0;
        }
        const char *option = argv[i];
        if (++i == argc) {
            string_fprintf(stderr, "Missing value for %s\n", option);
            return 2;
        }
        if (!strcmp(option, "--host")) host = argv[i];
        else if (!strcmp(option, "--browser")) browser = argv[i];
        else if (!strcmp(option, "--binary")) binary = argv[i];
        else if (!strcmp(option, "--base-path")) base = argv[i];
        else if (!strcmp(option, "--port")) {
            string_t *text = string_new_with(argv[i]);
            double value;
            bool ok = text && tba_text_number(text, &value) && value >= 0 && value <= 65535 && value == (unsigned)value;
            string_free(text);
            if (!ok) {
                string_fprintf(stderr, "Invalid listening port.\n");
                return 2;
            }
            port = (uint16_t)value;
        } else {
            string_fprintf(stderr, "Unknown option: %s\n", option);
            return 2;
        }
    }
    if (setenv("MARS_LAB_BIND_HOST", host, 1))
        return 1;
    tba_app_t *app = tba_app_new(binary, base);
    tba_server_t *server = app ? tba_svr_new(app, host, port) : NULL;
    if (!server) {
        string_fprintf(stderr, "Could not start the native forecasting Lab: %s\n", strerror(errno));
        tba_app_free(app);
        return 1;
    }
    struct sigaction action = {.sa_handler = tba_main_signal};
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    string_t *url = string_sprintf("http://localhost:%u%s/", tba_svr_port(server), tba_app_base(app));
    if (!url) {
        tba_svr_free(server);
        tba_app_free(app);
        return 1;
    }
    string_printf("To-Be-Announced Lab running at %S\n", url);
    fflush(stdout);
    pid_t workers[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        workers[i] = tba_main_spawn_worker(server, app);
        if (workers[i] < 0)
            tba_main_stopping = 1;
    }
    if (open_browser && !tba_main_stopping)
        tba_main_browser(browser, string_c_str(url));
    while (!tba_main_stopping) {
        int status;
        pid_t child = waitpid(-1, &status, WNOHANG);
        if (child > 0) {
            for (size_t i = 0; i < 4; ++i)
                if (workers[i] == child) {
                    workers[i] = tba_main_spawn_worker(server, app);
                    if (workers[i] < 0)
                        tba_main_stopping = 1;
                    break;
                }
        } else {
            struct timespec pause = {.tv_nsec = 100000000};
            nanosleep(&pause, NULL);
        }
    }
    for (size_t i = 0; i < 4; ++i)
        if (workers[i] > 0)
            kill(workers[i], SIGTERM);
    for (size_t i = 0; i < 4; ++i)
        if (workers[i] > 0)
            while (waitpid(workers[i], NULL, 0) < 0 && errno == EINTR) {}
    string_free(url);
    tba_svr_free(server);
    tba_app_free(app);
    return 0;
}
