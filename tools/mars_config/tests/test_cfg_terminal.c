/**
 * @file test_cfg_terminal.c
 * @brief Real-terminal native calendar installation and cancellation regressions.
 *
 * Runs the built installer beneath an isolated home on a controlling PTY. Checks
 * pagination, Latin selection and byte-preserving cancellation, EOF and Ctrl-C.
 * Public opaque storage APIs deliberately keep the POSIX signal stack type apart
 * from the calendar implementation's container headers. No shell is invoked.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cfg_storage.h"
#include "file.h"
#include "sqlite.h"
#include "test_cfg_calendar_fixture.h"
#include "test_cfg_support.h"
#include "test_harness.h"

/** Register real-terminal installer cases in the ordinary sequential test groups. */
void test_cfg_terminal_cases(void);

static const char *test_calendar_password = "calendar-regression-only";

static bool test_calendar_text(const string_t *text, const char *expected)
{
    return expected ? text && string_view_equals_literal(string_view_all(text), expected) : text == NULL;
}

static bool test_calendar_query(sqlite_t *db, const char *sql, const char *expected)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    const char *raw = ok ? sqlite_stmt_column_text(stmt, 0) : NULL;
    string_t *actual = raw ? string_new_with(raw) : NULL;
    ok = ok && test_calendar_text(actual, expected) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    if (!ok)
        fprintf(stderr, "Calendar regression query mismatch: %s; expected '%s', received '%s'\n", sql,
                expected ? expected : "NULL", actual ? string_c_str(actual) : "NULL");
    string_free(actual);
    sqlite_stmt_finalize(stmt);
    return ok;
}

typedef struct {
    const char *prompt;
    const char *answer;
} test_calendar_dialogue_t;

/* Keep only recent complete scalars for prompt matching and bounded failure diagnostics. */
static bool test_calendar_pty_window(string_t **pending)
{
    size_t length = string_byte_length(*pending);
    if (length <= 4096)
        return true;
    string_view_t view = string_view_all(*pending);
    string_pos_t position = 0, next = 0;
    while (position < length - 4096) {
        if (!string_view_peek_rune_value(view, position, NULL, &next) || next <= position)
            return false;
        position = next;
    }
    string_view_t tail = string_view_slice(view, position, length - position);
    string_t *recent = string_from_view(&tail);
    if (!recent)
        return false;
    string_free(*pending);
    *pending = recent;
    return true;
}

/* Frame transport fragments; the exact append API validates every completed UTF-8 scalar. */
static bool test_calendar_pty_append(string_t *transcript, string_t **pending, char *bytes, size_t *used)
{
    size_t complete = 0;
    while (complete < *used) {
        unsigned char lead = (unsigned char)bytes[complete];
        size_t width = lead < 0x80                    ? 1
                       : lead >= 0xc2 && lead <= 0xdf ? 2
                       : lead >= 0xe0 && lead <= 0xef ? 3
                       : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                      : 0;
        if (!width)
            return false;
        if (width > *used - complete)
            break;
        complete += width;
    }
    if (string_byte_length(transcript) > 16u * 1024 * 1024 - complete ||
        string_append_utf8_exact(transcript, bytes, complete) || string_append_utf8_exact(*pending, bytes, complete) ||
        !test_calendar_pty_window(pending))
        return false;
    *used -= complete;
    memmove(bytes, bytes + complete, *used);
    return true;
}

/* Run only the built executable on a real controlling terminal; never invoke a shell. */
static bool test_calendar_pty(const string_t *path, const test_calendar_dialogue_t *dialogue, size_t count,
                              int expected_status, bool interrupted, string_t **transcript)
{
    *transcript = string_new();
    string_t *pending = string_new();
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    bool ok = *transcript && pending && master >= 0 && !grantpt(master) && !unlockpt(master);
    char *slave = ok ? ptsname(master) : NULL;
    ok = ok && slave;
    fflush(NULL);
    pid_t child = ok ? fork() : -1;
    if (!child) {
        if (setsid() < 0)
            _exit(120);
        int terminal = open(slave, O_RDWR);
        if (terminal < 0 || ioctl(terminal, TIOCSCTTY, 0) < 0 || dup2(terminal, STDIN_FILENO) < 0 ||
            dup2(terminal, STDOUT_FILENO) < 0 || dup2(terminal, STDERR_FILENO) < 0)
            _exit(121);
        if (terminal > STDERR_FILENO && close(terminal))
            _exit(122);
        if (close(master))
            _exit(123);
        execl(MARS_CONFIG_PROGRAM, MARS_CONFIG_PROGRAM, "jurisdiction", "--db-path", string_c_str(path), "--db-key",
              test_calendar_password, (char *)NULL);
        _exit(124);
    }
    ok = ok && child > 0;
    struct timespec began, now;
    ok = ok && !clock_gettime(CLOCK_MONOTONIC, &began);
    size_t next = 0;
    char bytes[4096 + 4];
    size_t used = 0;
    int status = 0;
    bool reaped = false, drained = false;
    /* Full imports take minutes under the suite's 50% CPU limit. Keep a hard ten-minute bound. */
    while (ok && (!reaped || !drained)) {
        struct pollfd descriptor = {.fd = master, .events = POLLIN};
        int ready = poll(&descriptor, 1, 200);
        if (ready < 0 && errno != EINTR) {
            ok = false;
            break;
        }
        if (ready > 0) {
            ssize_t length = read(master, bytes + used, 4096);
            if (length > 0) {
                drained = false;
                used += (size_t)length;
                ok = test_calendar_pty_append(*transcript, &pending, bytes, &used);
                if (ok && next < count && string_find(pending, dialogue[next].prompt) >= 0) {
                    string_t *answer = string_new_with(dialogue[next].answer);
                    size_t size = answer ? string_byte_length(answer) : 0;
                    ok = answer && write(master, string_c_str(answer), size) == (ssize_t)size;
                    string_free(answer);
                    string_free(pending);
                    pending = string_new();
                    ok = ok && pending;
                    ++next;
                }
            } else if (!length || (length < 0 && errno == EIO)) {
                drained = true;
                ok = used == 0;
            } else if (length < 0 && errno != EINTR)
                ok = false;
        }
        if (!reaped) {
            pid_t result = waitpid(child, &status, WNOHANG);
            reaped = result == child;
            if (result < 0 && errno != EINTR)
                ok = false;
        }
        ok = ok && !clock_gettime(CLOCK_MONOTONIC, &now) && now.tv_sec - began.tv_sec < 600;
    }
    if (child > 0 && !reaped) {
        if (kill(child, SIGKILL) && errno != ESRCH)
            ok = false;
        pid_t result;
        do {
            result = waitpid(child, &status, 0);
        } while (result < 0 && errno == EINTR);
        reaped = result == child;
        ok = false;
    }
    ok = ok && reaped && next == count &&
         (interrupted ? WIFSIGNALED(status) && WTERMSIG(status) == SIGINT
                      : WIFEXITED(status) && WEXITSTATUS(status) == expected_status);
    if (master >= 0)
        ok = close(master) == 0 && ok;
    if (!ok && *transcript)
        fprintf(stderr,
                "Calendar PTY regression failed (status %d, dialogue %zu/%zu, %zu bytes retained). "
                "Recent output only:\n%s\n",
                status, next, count, string_byte_length(*transcript), pending ? string_c_str(pending) : "");
    string_free(pending);
    return ok;
}

static bool test_calendar_same_file(file_t *left, file_t *right)
{
    bool ok = left && right && file_open_read(left) && file_open_read(right);
    unsigned char a[8192], b[8192];
    size_t alen = 0, blen = 0;
    do {
        ok = ok && file_read(left, a, sizeof(a), &alen) && file_read(right, b, sizeof(b), &blen) && alen == blen &&
             !memcmp(a, b, alen);
    } while (ok && alen);
    if (left && file_is_open(left))
        ok = file_close(left) && ok;
    if (right && file_is_open(right))
        ok = file_close(right) && ok;
    return ok;
}

/* PTY cases exercise real menus and publication, not a second worldwide-rule import benchmark.
 * Keep every packaged town/language and the real schema/manual rules. The full immutable
 * baseline and database installation group separately retain generated-rule coverage. */
static bool test_calendar_pty_sources(const char *directory)
{
    string_t *root = string_sprintf("%s/pty-sources", directory);
    string_t *folder_path = root ? string_sprintf("%s/packaging/jurisdiction-db", string_c_str(root)) : NULL;
    file_t *folder = folder_path ? file_new(folder_path) : NULL;
    bool ok = folder && file_create_directory(folder, 0700, true);
    static const char *const sources[] = {"mars_holiday_rules.sql",
                                          "mars_country_jurisdictions.sql",
                                          "mars_target_subdivisions.sql",
                                          "mars_timezone_rules.sql",
                                          "mars_jurisdiction_location_defaults.sql",
                                          "mars_jurisdiction_towns.sql",
                                          "mars_manual_first_class_rules.sql",
                                          "mars_holiday_localized_names.sql",
                                          "mars_calendar_locale_names.sql",
                                          "mars_calendar_local.sql"};
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources) && ok; ++i) {
        string_t *source_path = string_sprintf("%s/packaging/jurisdiction-db/%s", MARS_CONFIG_ROOT_DIR, sources[i]);
        string_t *target_path = string_sprintf("%s/%s", string_c_str(folder_path), sources[i]);
        file_t *source = source_path ? file_new(source_path) : NULL;
        file_t *target = target_path ? file_new(target_path) : NULL;
        ok = source && target && file_copy(source, target, false);
        file_free(source);
        file_free(target);
        string_free(source_path);
        string_free(target_path);
    }
    string_t *generated_path =
        folder_path ? string_sprintf("%s/mars_generated_first_class_rules.sql", string_c_str(folder_path)) : NULL;
    file_t *generated = generated_path ? file_new(generated_path) : NULL;
    string_t *empty = string_new_with("-- PTY fixture: generated rules are covered by the full installation tests.\n");
    ok = ok && generated && empty && file_write_all_text(generated, empty) &&
         !setenv("MARS_ROOT", string_c_str(root), 1);
    string_free(empty);
    file_free(generated);
    string_free(generated_path);
    file_free(folder);
    string_free(folder_path);
    string_free(root);
    return ok;
}

static bool test_calendar_pty_fixture(const char *directory)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = test_cfg_calendar_database(directory, &path, &key);
    bool ok = db != NULL;
    sqlite_close(db);
    ok = ok && test_calendar_pty_sources(directory);
    static const test_calendar_dialogue_t choose[] = {
        {"[leave blank to keep current key]: ", "\n"},
        {"Choice: ", "/\n"},
        {"Choice: ", "n\n"},
        {"Choice: ", "p\n"},
        {"Choice: ", "999\n"},
        {"Choice: ", "Vatican City\n"},
        {"Choice: ", "1\n"},
        {"Language number or name [Enter = default; q = cancel]: ", "2\n"}};
    string_t *output = NULL;
    ok = ok && test_calendar_pty(path, choose, sizeof(choose) / sizeof(*choose), 0, false, &output) &&
         string_find(output, "Towns — page 2/") >= 0 && string_find(output, "Choose one of the numbers shown.") >= 0 &&
         string_find(output, "1. Italian") >= 0 && string_find(output, "2. Latin") >= 0 &&
         string_find(output, test_calendar_password) < 0;
    string_free(output);
    db = ok ? sqlite_open_encrypted(path, key) : NULL;
    ok = ok && db &&
         test_calendar_query(db, "SELECT location||'|'||locale FROM calendar_local_settings", "Vatican City|la_VA");
    sqlite_close(db);
    string_t *config_path = string_sprintf("%s/config/jurisdiction-db.env", directory);
    string_t *saved_path = string_sprintf("%s/saved.db", directory),
             *saved_config = string_sprintf("%s/saved.env", directory);
    file_t *database = path ? file_new(path) : NULL, *config = config_path ? file_new(config_path) : NULL;
    file_t *copy = saved_path ? file_new(saved_path) : NULL,
           *config_copy = saved_config ? file_new(saved_config) : NULL;
    string_t *language = config ? cfg_storage_setting(config, "MARS_CALENDAR_LANGUAGE") : NULL;
    string_t *location = config ? cfg_storage_setting(config, "MARS_CALENDAR_LOCATION") : NULL;
    ok = ok && test_calendar_text(language, "la_VA") && test_calendar_text(location, "Vatican City, VA") && copy &&
         config_copy && file_copy(database, copy, false) && file_copy(config, config_copy, false);
    static const test_calendar_dialogue_t cancel[] = {
        {"[leave blank to keep current key]: ", "\n"},
        {"Choice: ", "\n"},
        {"Language number or name [Enter = default; q = cancel]: ", "q\n"}};
    static const test_calendar_dialogue_t eof[] = {{"[leave blank to keep current key]: ", "\n"}, {"Choice: ", "\004"}};
    static const test_calendar_dialogue_t interrupt[] = {{"[leave blank to keep current key]: ", "\n"},
                                                         {"Choice: ", "\003"}};
    const struct {
        const test_calendar_dialogue_t *steps;
        size_t count;
        bool signal;
    } failures[] = {{cancel, sizeof(cancel) / sizeof(*cancel), false},
                    {eof, sizeof(eof) / sizeof(*eof), false},
                    {interrupt, sizeof(interrupt) / sizeof(*interrupt), true}};
    for (size_t i = 0; i < sizeof(failures) / sizeof(*failures) && ok; ++i) {
        output = NULL;
        ok = test_calendar_pty(path, failures[i].steps, failures[i].count, 1, failures[i].signal, &output) &&
             test_calendar_same_file(database, copy) && test_calendar_same_file(config, config_copy);
        string_free(output);
    }
    string_free(language);
    string_free(location);
    file_free(config_copy);
    file_free(copy);
    file_free(config);
    file_free(database);
    string_free(config_path);
    string_free(saved_path);
    string_free(saved_config);
    string_free(path);
    string_free(key);
    return ok;
}

static void test_calendar_terminal(void)
{
    TEST_ASSERT_TRUE(
        test_cfg_calendar_isolated(test_calendar_pty_fixture),
        "real PTY selects Latin and q, EOF and interruption preserve installed database and configuration");
}

/* Called by the parent runner before all README example groups. */
void test_cfg_terminal_cases(void)
{
#ifndef HAVE_UNISTRING
    return;
#endif
    TEST_RUN_IN_GROUP(test_calendar_terminal, tests, NULL);
}
