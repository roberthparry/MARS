/**
 * @file sqlcipher_probe.c
 * @brief Check the linked SQLCipher version and SQL features before building or updating databases.
 *
 * This standalone bootstrap probe links directly to the same SQLCipher flags as MARS; it cannot depend on the
 * library whose prerequisites it checks. It reports the runtime cipher and SQLite versions, checks the SQLite
 * headers, and exercises FTS5 with the packaged Unicode tokenizer and window functions in an in-memory database.
 * No installed database, settings, credentials or command-line SQLCipher programme is required or accessed.
 */
#include <stdbool.h>
#include <stdio.h>

#include <sqlcipher/sqlite3.h>

static bool probe_version(sqlite3 *db)
{
    sqlite3_stmt *query = NULL;
    bool found = sqlite3_prepare_v2(db, "PRAGMA cipher_version", -1, &query, NULL) == SQLITE_OK &&
                 sqlite3_step(query) == SQLITE_ROW && sqlite3_column_text(query, 0) &&
                 *sqlite3_column_text(query, 0);
    if (found) {
        printf("SQLCipher %s; SQLite runtime %s; SQLite headers %s.\n",
               sqlite3_column_text(query, 0), sqlite3_libversion(), SQLITE_VERSION);
        fflush(stdout);
    } else
        fputs("The linked library does not report a SQLCipher version.\n", stderr);
    sqlite3_finalize(query);
    bool runtime = sqlite3_libversion_number() >= 3027000;
    bool headers = SQLITE_VERSION_NUMBER >= 3027000;
    if (!runtime)
        fprintf(stderr, "SQLCipher embeds SQLite %s; MARS requires SQLite 3.27.0 or newer.\n", sqlite3_libversion());
    if (!headers)
        fprintf(stderr, "SQLCipher headers use SQLite %s; MARS requires SQLite 3.27.0 or newer.\n", SQLITE_VERSION);
    return found && runtime && headers;
}

static bool probe_features(sqlite3 *db)
{
    static const struct {
        const char *name;
        const char *sql;
    } features[] = {
        {"FTS5 with Unicode accent removal", "CREATE VIRTUAL TABLE temp.mars_fts5_probe USING "
            "fts5(name,tokenize='unicode61 remove_diacritics 2');"
            "INSERT INTO mars_fts5_probe VALUES('Lüderitz');"
            "SELECT name FROM mars_fts5_probe WHERE mars_fts5_probe MATCH '\"Luder\"*' ORDER BY rank;"},
        {"SQLite window functions", "SELECT row_number() OVER ()"}
    };
    bool ok = true;
    /* Execute every independent requirement, so one failure cannot conceal another missing feature. */
    for (size_t i = 0; i < sizeof features / sizeof *features; ++i) {
        char *error = NULL;
        if (sqlite3_exec(db, features[i].sql, NULL, NULL, &error) != SQLITE_OK) {
            fprintf(stderr, "SQLCipher lacks required %s: %s.\n", features[i].name,
                    error ? error : sqlite3_errmsg(db));
            ok = false;
        }
        sqlite3_free(error);
    }
    return ok;
}

/* Validate the selected runtime without opening a user database or changing the installed library. */
int main(void)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        fputs("Cannot open SQLCipher's in-memory dependency probe.\n", stderr);
        sqlite3_close(db);
        return 1;
    }
    bool version = probe_version(db);
    bool features = probe_features(db);
    bool closed = sqlite3_close(db) == SQLITE_OK;
    if (!version || !features || !closed) {
        fputs("Install or rebuild the SQLCipher library and development headers with SQLite >= 3.27.0,\n"
              "SQLITE_ENABLE_FTS5 enabled, and window functions enabled; then rerun make check-native-deps.\n"
              "Updating the separate sqlite3 package does not update the SQLCipher library linked by MARS.\n",
              stderr);
        return 1;
    }
    puts("SQLCipher version and required SQL features verified.");
    return 0;
}
