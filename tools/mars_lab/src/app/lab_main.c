/**
 * @file lab_main.c
 * @brief Native Linux MARS Lab launcher and bounded worker supervisor.
 *
 * Owns an IP listener and a fixed pool of separate request processes. Mathematical
 * workers and TeX utilities remain isolated child programmes. No Python interpreter
 * is loaded or launched. Uses the compiled repository location unless MARS_ROOT
 * supplies an explicit alternative.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "lab_process.h"
#include "lab_runtime.h"
#include "lab_server.h"

static volatile sig_atomic_t stopping;

typedef struct {
    const char *option, *environment;
} worker_option_t;

static int lab_app_compare_option(const void *key, const void *entry)
{
    return strcmp(key, ((const worker_option_t *)entry)->option);
}

static bool lab_app_worker_option(const char *name, const char *value)
{
    static const worker_option_t options[] = {{"--almanac-binary", "MARS_LAB_ALMANAC_BINARY"},
                                              {"--binary", "MARS_LAB_BINARY"},
                                              {"--datetime-binary", "MARS_LAB_DATETIME_BINARY"},
                                              {"--diffequation-binary", "MARS_LAB_DIFFEQUATION_BINARY"},
                                              {"--equation-binary", "MARS_LAB_EQUATION_BINARY"},
                                              {"--holiday-binary", "MARS_LAB_HOLIDAY_BINARY"},
                                              {"--integrator-binary", "MARS_LAB_INTEGRATOR_BINARY"},
                                              {"--matrix-binary", "MARS_LAB_MATRIX_BINARY"}};
    const worker_option_t *found =
        bsearch(name, options, sizeof(options) / sizeof(*options), sizeof(*options), lab_app_compare_option);
    return found && value && *value && setenv(found->environment, value, 1) == 0;
}

static void lab_app_stop(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static bool lab_app_option(const char *text, const char *expected)
{
    string_t *a = string_new_with(text), *b = string_new_with(expected);
    bool match = a && b && !string_compare(a, b);
    string_free(a);
    string_free(b);
    return match;
}

static bool lab_app_integer(const char *text, unsigned maximum, unsigned *value)
{
    string_t *input = string_new_with(text);
    string_view_t view = string_view_all(input);
    bool ok = input && string_view_length(view) != 0;
    unsigned result = 0;
    for (size_t i = 0; ok && i < string_view_length(view); ++i) {
        unsigned char digit;
        ok = string_view_peek_ascii(view, i, &digit) && digit >= '0' && digit <= '9';
        if (ok) {
            unsigned n = digit - '0';
            ok = n <= maximum && result <= (maximum - n) / 10;
            if (ok)
                result = result * 10 + n;
        }
    }
    string_free(input);
    if (ok)
        *value = result;
    return ok;
}

static int lab_app_worker(lab_server_t *server)
{
    lab_proc_set_cancel_flag(&stopping);
    while (!stopping) {
        int rc = lab_svr_serve_once(server, 200);
        if (rc < 0 && errno != EINTR && errno != EAGAIN && errno != ETIMEDOUT && errno != EPIPE && errno != ECONNRESET)
            fprintf(stderr, "mars-lab: connection failed (errno %d)\n", errno);
    }
    lab_svr_free(server);
    return 0;
}

static pid_t lab_app_start_worker(lab_server_t *server)
{
    pid_t parent = getpid();
    pid_t pid = fork();
    if (pid == 0) {
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() != parent)
            _exit(1);
        _exit(lab_app_worker(server));
    }
    return pid;
}

/* Parse options, start request workers and synchronously reap them on shutdown. */
int main(int argc, char **argv)
{
    const char *host = "127.0.0.1", *browser = "xdg-open";
    unsigned port = 0, workers = 4;
    bool no_browser = false;
    for (int i = 1; i < argc; ++i) {
        if (lab_app_option(argv[i], "--help")) {
            puts("MARS Lab (native C)\nUsage: mars_lab [--host IP] [--port 0..65535] [--workers 1..8]\n"
                 "                       [--no-browser] [--browser PROGRAM]\n"
                 "Uses the compiled MARS repository unless MARS_ROOT is set. Default: IPv4 loopback, four workers.");
            return 0;
        }
        if (lab_app_option(argv[i], "--no-browser")) {
            no_browser = true;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing option value: %s\n", argv[i]);
            return 2;
        }
        if (lab_app_option(argv[i], "--host"))
            host = argv[++i];
        else if (lab_app_option(argv[i], "--browser"))
            browser = argv[++i];
        else if (lab_app_option(argv[i], "--port")) {
            if (!lab_app_integer(argv[++i], 65535, &port)) {
                fputs("Invalid port\n", stderr);
                return 2;
            }
        } else if (lab_app_option(argv[i], "--workers")) {
            if (!lab_app_integer(argv[++i], 8, &workers) || !workers) {
                fputs("Invalid worker count\n", stderr);
                return 2;
            }
        } else if (lab_app_worker_option(argv[i], argv[i + 1]))
            ++i;
        else {
            fprintf(stderr, "Unknown or invalid option: %s\n", argv[i]);
            return 2;
        }
    }
    const char *root = getenv("MARS_ROOT");
    if (!root || !*root)
        root = MARS_LAB_ROOT_DIR;
    if (chdir(root)) {
        perror("MARS_ROOT");
        return 1;
    }
    if (setenv("MARS_LAB_BIND_HOST", host, 1))
        return 1;
    if (!lab_runtime_prepare()) {
        fputs("Cannot initialise private Lab cache configuration\n", stderr);
        return 1;
    }
    struct sigaction action = {.sa_handler = lab_app_stop};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL))
        return 1;
    signal(SIGPIPE, SIG_IGN);
    string_t *address = string_new_with(host);
    lab_server_t *server = address ? lab_svr_new(address, (uint16_t)port, 180000) : NULL;
    string_free(address);
    if (!server) {
        perror("MARS Lab listener");
        return 1;
    }
    pid_t children[8] = {0};
    bool failed = false;
    for (unsigned i = 0; i < workers; ++i) {
        children[i] = lab_app_start_worker(server);
        if (children[i] < 0) {
            failed = true;
            stopping = 1;
            break;
        }
    }
    string_t *host_text = string_new_with(host);
    bool ipv6 = host_text && string_find(host_text, ":") >= 0;
    const char *access = lab_app_option(host, "0.0.0.0") ? "127.0.0.1" : lab_app_option(host, "::") ? "::1" : host;
    string_t *url = string_sprintf(ipv6 ? "http://[%s]:%u/" : "http://%s:%u/", access, lab_svr_port(server));
    string_free(host_text);
    if (!stopping && url) {
        string_printf("MARS Lab (native C) running at %S\n", url);
        fflush(stdout);
        if (!no_browser) {
            const char *args[] = {browser, string_c_str(url), NULL};
            string_t *output = NULL;
            int status;
            lab_proc_set_cancel_flag(&stopping);
            if (!lab_proc_run(args, NULL, 5000, 65536, &output, &status) || status)
                fputs("Could not open the browser; open the URL above manually.\n", stderr);
            string_free(output);
        }
    }
    string_free(url);
    while (!stopping) {
        int status;
        pid_t done = waitpid(-1, &status, 0);
        if (done < 0 && errno == EINTR)
            continue;
        if (done < 0) {
            failed = true;
            break;
        }
        /* A worker failure closes the service instead of spawning an unbounded restart loop. */
        for (unsigned i = 0; i < workers; ++i)
            if (children[i] == done)
                children[i] = 0;
        failed = true;
        break;
    }
    for (unsigned i = 0; i < workers; ++i)
        if (children[i] > 0)
            kill(children[i], SIGTERM);
    /* Allow cancellation-aware helpers to reap their children, but do not wait
       for a slow HTTP peer's complete request deadline during shutdown. */
    for (unsigned attempt = 0; attempt < 600; ++attempt) {
        bool remaining = false;
        for (unsigned i = 0; i < workers; ++i) {
            if (children[i] <= 0)
                continue;
            pid_t done = waitpid(children[i], NULL, WNOHANG);
            if (done == children[i] || (done < 0 && errno == ECHILD))
                children[i] = 0;
            else
                remaining = true;
        }
        if (!remaining)
            break;
        struct timespec interval = {.tv_nsec = 10000000};
        nanosleep(&interval, NULL);
    }
    for (unsigned i = 0; i < workers; ++i) {
        if (children[i] <= 0)
            continue;
        kill(children[i], SIGKILL);
        while (waitpid(children[i], NULL, 0) < 0 && errno == EINTR) {
        }
    }
    lab_svr_free(server);
    return failed ? 1 : 0;
}
