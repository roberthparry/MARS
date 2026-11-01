/**
 * @file checks_sqlcipher_fixture.c
 * @brief Simulate old or feature-limited SQLCipher runtimes in dependency-check regressions.
 *
 * The test linker wraps version reporting, the cipher-version query and SQL execution. All other operations
 * use the installed SQLCipher library, including allocation and statement handling. Overrides belong
 * solely to disposable test processes; the installed library and user databases are never modified.
 */
#include <stdlib.h>
#include <string.h>

#include <sqlcipher/sqlite3.h>

int __real_sqlite3_libversion_number(void);
const char *__real_sqlite3_libversion(void);
int __real_sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void *, int, char **, char **),
                       void *context, char **error);
int __real_sqlite3_prepare_v2(sqlite3 *db, const char *sql, int length, sqlite3_stmt **query, const char **tail);

/* Model an unsupported cipher release independently of the embedded SQLite version. */
int __wrap_sqlite3_prepare_v2(sqlite3 *db, const char *sql, int length, sqlite3_stmt **query, const char **tail)
{
    const char *mode = getenv("MARS_TEST_SQLCIPHER");
    if (mode && !strcmp(mode, "cipher") && !strcmp(sql, "PRAGMA cipher_version"))
        return __real_sqlite3_prepare_v2(db, "SELECT '4.0.1 community'", -1, query, tail);
    return __real_sqlite3_prepare_v2(db, sql, length, query, tail);
}

/* Return an older runtime version only in the corresponding child-process fixture. */
int __wrap_sqlite3_libversion_number(void)
{
    const char *mode = getenv("MARS_TEST_SQLCIPHER");
    return mode && !strcmp(mode, "old") ? 3015002 : __real_sqlite3_libversion_number();
}

/* Keep the displayed version consistent with the simulated runtime version number. */
const char *__wrap_sqlite3_libversion(void)
{
    const char *mode = getenv("MARS_TEST_SQLCIPHER");
    return mode && !strcmp(mode, "old") ? "3.15.2" : __real_sqlite3_libversion();
}

/* Reject one required feature, leaving all ordinary database execution with the real runtime. */
int __wrap_sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void *, int, char **, char **),
                       void *context, char **error)
{
    const char *mode = getenv("MARS_TEST_SQLCIPHER");
    const char *failure = mode && !strcmp(mode, "fts5") && strstr(sql, "USING fts5") ? "no such module: fts5"
        : mode && !strcmp(mode, "window") && strstr(sql, "row_number") ? "window functions disabled" : NULL;
    if (!failure)
        return __real_sqlite3_exec(db, sql, callback, context, error);
    if (error)
        *error = sqlite3_mprintf("%s", failure);
    return SQLITE_ERROR;
}
