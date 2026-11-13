/**
 * @file sqlite_cache.c
 * @brief Configure bounded, connection-local SQLite page-cache allowances.
 *
 * Quotes schema identifiers through SQLCipher and validates sizes before constructing
 * PRAGMA statements. Applications choose allowances for their workloads; the library
 * does not install application-specific defaults or change other connections.
 */
#include <errno.h>
#include <limits.h>

#include "sqlite-t.h"
#define MARS_SQLITE_INTERNAL_ACCESS
#include "sqlite_internal.h"

/* Tune one schema without interpreting its name as SQL or changing persistent defaults. */
bool sqlite_set_cache_budget(sqlite_t *db, const char *schema, size_t kibibytes)
{
    if (!db || (schema && !*schema) || !kibibytes || kibibytes > INT_MAX) {
        sqlite_set_error(db, "cache budget requires an open database, a schema name and 1..INT_MAX KiB");
        errno = EINVAL;
        return false;
    }
    char *sql = sqlite3_mprintf("PRAGMA \"%w\".cache_size=-%d", schema ? schema : "main", (int)kibibytes);
    if (!sql) {
        sqlite_set_error(db, "could not allocate the cache-budget statement");
        errno = ENOMEM;
        return false;
    }
    bool ok = sqlite_exec_cstr(db, sql);
    sqlite3_free(sql);
    return ok;
}
