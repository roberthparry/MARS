/**
 * @file test_database_command.c
 * @brief Explicit native commands for test catalogue synchronisation, selection profiles and run inspection.
 *
 * Linked into mars_checks as test-only developer support. Calling help creates nothing; database operations
 * occur only after an explicit command. All external names and values are bound SQL parameters. Schema and
 * module case definitions, run reports and lifetime statistics share tests/tests.db.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

static void test_database_usage(void)
{
    puts("Usage: mars_checks test-db [--database PATH] COMMAND\n"
         "  init | sync                         Import the configured test/group catalogue\n"
         "  sync-if-needed                      Sync changed sources only when the database already exists\n"
         "  list [SOURCE_PREFIX]                List catalogued tests and groups\n"
         "  profile-create NAME [all|none]       Create a saved selection (default: none)\n"
         "  profile-save NAME                   Snapshot the current JSON selection\n"
         "  profile-set NAME SOURCE PATH on|off  Change one test/group rule\n"
         "  runs [COUNT]                        Show recent run reports (default: 20)\n"
         "  stats [SOURCE_PREFIX] [--details] [--format=auto|grid|markdown|tsv]\n"
         "                                      Show aligned per-test timings; --details includes every metric\n"
         "  help                                Show this help without creating storage\n"
         "Stats defaults to the Unicode grid, independently of installed Markdown readers.\n"
         "Pipe the grid to less -R to retain its bold headings and coloured results.\n"
         "Use --format=grid to choose the grid, or --format=markdown|tsv for explicit exports; no reader is launched.\n"
         "Definitions, profiles, results and statistics: tests/tests.db; survives make clean.\n"
         "Sync retains missing identities as unconfirmed; it never removes or disables tests.\n"
         "Release synchronises existing test storage when SQL or catalogue contents change.\n"
         "Normal builds, release, installation and applications create no test database.");
}

static int test_database_conditional_sync(const string_t *root, const string_t *path)
{
    string_t *location = path ? string_clone(path) :
        string_sprintf("%s/tests/tests.db", root ? string_c_str(root) : ".");
    file_t *file = location ? file_new(location) : NULL;
    bool located = file != NULL;
    bool available = file && file_exists(file);
    file_free(file);
    string_free(location);
    if (!located) {
        fputs("test-db: Cannot locate existing test storage.\n", stderr);
        return 1;
    }
    if (!available) {
        puts("Test database: not present; synchronisation skipped.");
        return 0;
    }
    string_t *error = NULL;
    /* Open without SQLITE_OPEN_CREATE, including if the file disappears after the existence check. */
    test_database_t *database = test_database_open(root, path, false, &error);
    bool current = false;
    bool ok = database && test_database_sync_current(database, root, &current);
    if (ok && !current)
        ok = test_database_import(database, root, NULL, true);
    if (ok)
        puts(current ? "Test database: up to date; synchronisation skipped." : "Test database: sync complete.");
    else
        fprintf(stderr, "test-db: %s\n", error ? string_c_str(error) : test_database_error(database));
    string_free(error);
    test_database_close(database);
    return ok ? 0 : 1;
}

static bool test_database_list(test_database_t *database, const char *prefix)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT source, path, kind, confirmed FROM test_catalogue\n"
        "WHERE ?1 = '' OR substr(source, 1, length(?1)) = ?1 ORDER BY source, path");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, prefix ? prefix : "", -1, SQLITE_TRANSIENT);
    int status;
    while ((status = sqlite3_step(statement)) == SQLITE_ROW)
        printf("%s\t%s\t%s\t%s\n", sqlite3_column_text(statement, 0), sqlite3_column_text(statement, 1),
               sqlite3_column_text(statement, 2), sqlite3_column_int(statement, 3) ? "confirmed" : "unconfirmed");
    sqlite3_finalize(statement);
    return status == SQLITE_DONE || test_database_fail(database, NULL);
}

static bool test_database_runs(test_database_t *database, long count)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT id, finished_utc, exit_code, elapsed_ms, executable, profile, memory_test\n"
        "FROM test_run ORDER BY id DESC LIMIT ?1");
    if (!statement)
        return false;
    sqlite3_bind_int64(statement, 1, count);
    puts("Run\tFinished (UTC)\tExit\tMilliseconds\tExecutable\tProfile\tMemory test");
    int status;
    while ((status = sqlite3_step(statement)) == SQLITE_ROW)
        printf("%lld\t%s\t%d\t%.3f\t%s\t%s\t%s\n", (long long)sqlite3_column_int64(statement, 0),
               sqlite3_column_text(statement, 1), sqlite3_column_int(statement, 2), sqlite3_column_double(statement, 3),
               sqlite3_column_text(statement, 4), sqlite3_column_text(statement, 5),
               sqlite3_column_int(statement, 6) ? "yes" : "no");
    sqlite3_finalize(statement);
    return status == SQLITE_DONE || test_database_fail(database, NULL);
}

/* No initialisation occurs until the caller explicitly requests an administration operation. */
int test_database_command(const string_t *root, int argc, char **argv)
{
    string_t *path = NULL;
    if (argc >= 2 && !strcmp(argv[0], "--database")) {
        path = string_new_with(argv[1]);
        argc -= 2;
        argv += 2;
    }
    if (!argc || !strcmp(argv[0], "help")) {
        test_database_usage();
        string_free(path);
        return argc ? 0 : 2;
    }
    const char *command = argv[0];
    if (argc == 1 && !strcmp(command, "sync-if-needed")) {
        int status = test_database_conditional_sync(root, path);
        string_free(path);
        return status;
    }
    bool importing = argc == 1 && (!strcmp(command, "init") || !strcmp(command, "sync"));
    bool listing = argc <= 2 && !strcmp(command, "list");
    bool saving = argc == 2 && !strcmp(command, "profile-save");
    bool creating = (argc == 2 || argc == 3) && !strcmp(command, "profile-create") &&
                    (argc == 2 || !strcmp(argv[2], "all") || !strcmp(argv[2], "none"));
    bool setting = argc == 5 && !strcmp(command, "profile-set") &&
                   (!strcmp(argv[4], "on") || !strcmp(argv[4], "off"));
    bool runs = argc <= 2 && !strcmp(command, "runs");
    bool stats = !strcmp(command, "stats"), details = false, format_seen = false;
    const char *stats_prefix = NULL;
    test_database_stats_format_t format = TEST_DATABASE_STATS_AUTO;
    if (stats) {
        for (int i = 1; i < argc && stats; ++i) {
            if (!strcmp(argv[i], "--details") && !details)
                details = true;
            else if (!strncmp(argv[i], "--format=", 9) && !format_seen) {
                const char *value = argv[i] + 9;
                format_seen = true;
                if (!strcmp(value, "auto")) format = TEST_DATABASE_STATS_AUTO;
                else if (!strcmp(value, "grid")) format = TEST_DATABASE_STATS_GRID;
                else if (!strcmp(value, "markdown")) format = TEST_DATABASE_STATS_MARKDOWN;
                else if (!strcmp(value, "tsv")) format = TEST_DATABASE_STATS_TSV;
                else stats = false;
            } else if (argv[i][0] != '-' && !stats_prefix)
                stats_prefix = argv[i];
            else
                stats = false;
        }
    }
    long limit = 20;
    if (runs && argc == 2) {
        char *end = NULL;
        limit = strtol(argv[1], &end, 10);
        runs = end && end != argv[1] && !*end && limit > 0 && limit <= 200;
    }
    if (!importing && !listing && !saving && !creating && !setting && !runs && !stats) {
        test_database_usage();
        string_free(path);
        return 2;
    }
    string_t *error = NULL;
    test_database_t *database = test_database_open(root, path, true, &error);
    bool ok = database != NULL;
    if (ok && (importing || saving))
        ok = test_database_import(database, root, saving ? argv[1] : NULL, importing);
    else if (ok && listing)
        ok = test_database_list(database, argc == 2 ? argv[1] : NULL);
    else if (ok && creating)
        ok = test_database_profile_create(database, argv[1], argc == 3 && !strcmp(argv[2], "all"));
    else if (ok && setting)
        ok = test_database_profile_set(database, argv[1], argv[2], argv[3], !strcmp(argv[4], "on"));
    else if (ok && runs)
        ok = test_database_runs(database, limit);
    else if (ok && stats)
        ok = test_database_stats_print(database, stats_prefix, format, details);
    if (!ok)
        fprintf(stderr, "test-db: %s\n", error ? string_c_str(error) : test_database_error(database));
    else if (!listing && !runs && !stats)
        printf("Test database: %s complete.\n", command);
    string_free(error);
    string_free(path);
    test_database_close(database);
    return ok ? 0 : 1;
}
