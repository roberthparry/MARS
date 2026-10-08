/**
 * @file checks_process.c
 * @brief Compiler word splitting and synchronous child execution without a shell.
 *
 * POSIX spawn redirects descriptors to private files; all capture content is
 * subsequently read through file.h. Owned process groups are killed after normal
 * completion as well as errors and timeouts, before the direct child is reaped.
 * Private capture trees are removed on every return path.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "checks_process.h"

extern char **environ;

static volatile sig_atomic_t checks_process_signal;
static struct sigaction checks_process_previous_int;
static struct sigaction checks_process_previous_term;
static bool checks_process_interrupt_active;

static void checks_process_record_signal(int signal_number)
{
    if (!checks_process_signal)
        checks_process_signal = signal_number;
}

/* Install command-scoped handlers without performing cleanup in signal context. */
bool checks_process_interrupt_begin(void)
{
    if (checks_process_interrupt_active) {
        errno = EBUSY;
        return false;
    }
    struct sigaction action = {.sa_handler = checks_process_record_signal};
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    checks_process_signal = 0;
    if (sigaction(SIGINT, &action, &checks_process_previous_int))
        return false;
    if (sigaction(SIGTERM, &action, &checks_process_previous_term)) {
        int error = errno;
        sigaction(SIGINT, &checks_process_previous_int, NULL);
        errno = error;
        return false;
    }
    checks_process_interrupt_active = true;
    return true;
}

/* Restore the caller's dispositions after all child and fixture cleanup. */
void checks_process_interrupt_end(void)
{
    if (!checks_process_interrupt_active)
        return;
    sigaction(SIGINT, &checks_process_previous_int, NULL);
    sigaction(SIGTERM, &checks_process_previous_term, NULL);
    checks_process_interrupt_active = false;
    checks_process_signal = 0;
}

/* Expose the recorded interruption to command dispatch and README iteration. */
int checks_process_interrupt_signal(void)
{
    return (int)checks_process_signal;
}

/* Parse compiler options without shell expansion. */
bool checks_shell_words(const string_t *text, checks_strings_t *words)
{
    string_t *word = checks_text("");
    unsigned char quote = 0;
    bool active = false;
    size_t length = string_byte_length(text), start = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = checks_byte(text, i);
        if (c >= 128) {
            start = i;
            while (i + 1 < length && checks_byte(text, i + 1) >= 128)
                ++i;
            string_t *part = checks_slice(text, start, i - start + 1);
            checks_append(word, part);
            string_free(part);
            active = true;
        } else if (c == '\\' && quote != '\'') {
            if (++i == length) {
                string_free(word);
                return false;
            }
            c = checks_byte(text, i);
            if (quote == '"' && c != '"' && c != '\\')
                string_append_char(word, '\\');
            if (c >= 128) {
                start = i;
                while (i + 1 < length && checks_byte(text, i + 1) >= 128)
                    ++i;
                string_t *part = checks_slice(text, start, i - start + 1);
                checks_append(word, part);
                string_free(part);
            } else {
                string_append_char(word, (char)c);
            }
            active = true;
        } else if (quote) {
            if (c == quote)
                quote = 0;
            else
                string_append_char(word, (char)c);
        } else if (c == '\'' || c == '"') {
            quote = c;
            active = true;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (active) {
                checks_strings_add(words, word);
                word = checks_text("");
                active = false;
            }
        } else {
            string_append_char(word, (char)c);
            active = true;
        }
    }
    if (!quote && active)
        checks_strings_add(words, word);
    else
        string_free(word);
    return !quote;
}

static double checks_process_monotonic_seconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        checks_fatal("reading monotonic clock");
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

/* Execute a child and preserve separate standard streams. */
bool checks_process_run(const checks_strings_t *arguments, const string_t *cwd, double timeout, string_t **output,
                        string_t **errors, int *status)
{
    *output = *errors = NULL;
    *status = -1;
    if (checks_process_interrupt_signal()) {
        errno = ECANCELED;
        return false;
    }
    size_t count = checks_strings_count(arguments);
    if (!count || !string_byte_length(checks_strings_get(arguments, 0))) {
        errno = EINVAL;
        return false;
    }
    char directory[] = "/tmp/mars-checks-capture-XXXXXX";
    if (!mkdtemp(directory))
        return false;
    string_t *root = checks_text(directory);
    string_t *out_path = checks_path(root, "stdout"), *err_path = checks_path(root, "stderr");
    char **argv = calloc(count + 1, sizeof(*argv));
    if (!argv)
        checks_fatal("allocating argument vector");
    for (size_t i = 0; i < count; ++i)
        argv[i] = (char *)string_c_str(checks_strings_get(arguments, i));
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int error = posix_spawn_file_actions_init(&actions);
    bool actions_ready = !error, attributes_ready = false;
    if (!error) {
        error = posix_spawnattr_init(&attributes);
        attributes_ready = !error;
    }
    if (!error)
        error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (!error)
        error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, string_c_str(out_path),
                                                 O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (!error)
        error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, string_c_str(err_path),
                                                 O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (!error && cwd)
        error = posix_spawn_file_actions_addchdir_np(&actions, string_c_str(cwd));
    if (!error)
        error = posix_spawn_file_actions_addclosefrom_np(&actions, 3);
    if (!error)
        error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    if (!error)
        error = posix_spawnattr_setpgroup(&attributes, 0);
    pid_t child = -1;
    if (!error)
        error = posix_spawnp(&child, argv[0], &actions, &attributes, argv, environ);
    if (actions_ready)
        posix_spawn_file_actions_destroy(&actions);
    if (attributes_ready)
        posix_spawnattr_destroy(&attributes);
    free(argv);
    if (!error) {
        double started = checks_process_monotonic_seconds();
        int result = 0;
        for (;;) {
            if (checks_process_interrupt_signal()) {
                error = ECANCELED;
                break;
            }
            siginfo_t information = {0};
            int waited = waitid(P_PID, (id_t)child, &information, WEXITED | WNOHANG | WNOWAIT);
            if (!waited && information.si_pid == child)
                break;
            if (waited < 0 && errno != EINTR) {
                error = errno;
                break;
            }
            if (timeout > 0 && checks_process_monotonic_seconds() - started >= timeout) {
                error = ETIMEDOUT;
                break;
            }
            struct timespec delay = {.tv_nsec = 10000000};
            nanosleep(&delay, NULL);
        }
        /* Retain the leader's PID until the group has been signalled, preventing
         * PID reuse from redirecting cleanup to an unrelated process group. */
        if (kill(-child, SIGKILL) < 0 && errno != ESRCH && !error)
            error = errno;
        pid_t reaped;
        do {
            reaped = waitpid(child, &result, 0);
        } while (reaped < 0 && errno == EINTR);
        if (reaped == child)
            *status = WIFEXITED(result) ? WEXITSTATUS(result) : -WTERMSIG(result);
        else if (!error)
            error = errno;
        *output = checks_read(out_path);
        *errors = checks_read(err_path);
        if ((!*output || !*errors) && !error)
            error = errno ? errno : EIO;
    }
    checks_remove_tree(root);
    string_free(out_path);
    string_free(err_path);
    string_free(root);
    errno = error;
    return !error;
}

/* Read the Git index using NUL-delimited records. */
checks_strings_t *checks_git_paths(const string_t *root, bool staged)
{
    checks_strings_t *arguments = checks_strings_new();
    const char *const all[] = {"git", "ls-files", "-z"};
    const char *const changed[] = {"git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"};
    size_t count = staged ? sizeof(changed) / sizeof(*changed) : sizeof(all) / sizeof(*all);
    for (size_t i = 0; i < count; ++i)
        checks_strings_add(arguments, checks_text(staged ? changed[i] : all[i]));
    string_t *output = NULL, *errors = NULL;
    int status;
    bool ok = checks_process_run(arguments, root, 0, &output, &errors, &status);
    checks_strings_t *paths = ok && !status ? checks_split(output, 0) : NULL;
    if (!paths && errors)
        string_fprintf(stderr, "%s", string_c_str(errors));
    if (paths)
        checks_strings_sort(paths);
    checks_strings_free(arguments);
    string_free(output);
    string_free(errors);
    return paths;
}
