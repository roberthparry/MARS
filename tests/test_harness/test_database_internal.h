/**
 * @file test_database_internal.h
 * @brief Private raw-SQLite storage boundary for test management, profiles and reporting.
 *
 * Consumed only by the database support files in tests/test_harness. Authorised fixtures use the narrower tests/internal
 * boundary; ordinary test suites and repository commands use test_database.h.
 * This code deliberately never includes sqlite-t.h or calls the MARS sqlite module. A connection is owned by
 * its opaque test-storage object and must be closed before invoking any case under test.
 */
#ifndef MARS_TEST_DATABASE_INTERNAL_H
#define MARS_TEST_DATABASE_INTERNAL_H

#if !defined(MARS_TEST_DATABASE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEST_DATABASE_INTERNAL_ACCESS."
#endif

#include "test_database.h"
#define MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS
#include "internal/test_database_internal.h"

/**
 * @brief Relocate renamed regression source identities without losing selections or recorded timings.
 * @param[in,out] database Caller-owned compatible storage, used sequentially.
 * @return True when no relocation is needed or all updates succeed; false on storage/identity conflicts.
 * @note Creates no files. Joins the caller's transaction or owns one transaction when none exists. Conflicts
 * never discard either identity; the caller must roll back its transaction on failure. All input remains owned.
 */
bool test_database_relocate_sources(test_database_t *database);

/** @brief Owned storage state, accessed only by the test-management implementation. */
struct _test_database_t {
    sqlite3 *connection; /**< Owned raw connection, closed on release. */
    string_t *error; /**< Owned latest diagnostic, or NULL. */
    sqlite3_int64 run_id; /**< Current transactional report identifier, or zero. */
    size_t ordinal; /**< Next per-run result index. */
    bool importing; /**< Whether catalogue updates must also record identities seen in this import. */
    bool memory_test; /**< Whether the current report is excluded from ordinary timing statistics. */
};

/** @brief Robust timing comparison made against prior accepted ordinary runs. */
typedef struct {
    bool available; /**< Whether the configured minimum number of baseline samples exists. */
    bool outlier; /**< Whether this duration exceeds the robust, relative and absolute thresholds. */
    double median_ms; /**< Baseline median at the time of assessment; meaningful only when available. */
    double threshold_ms; /**< Slowdown threshold used for this result; meaningful only when available. */
} test_timing_assessment_t;

/** @brief Statistics presentation; automatic output uses the native grid independently of installed readers. */
typedef enum {
    TEST_DATABASE_STATS_AUTO,     /**< Native Unicode grid; an explicit spelling of the default presentation. */
    TEST_DATABASE_STATS_GRID,     /**< Aligned UTF-8 grid, with long text wrapped inside its cells. */
    TEST_DATABASE_STATS_MARKDOWN, /**< Markdown tables suitable for a terminal reader or saving to a document. */
    TEST_DATABASE_STATS_TSV       /**< All metrics as tab-separated fields, retaining the original column names. */
} test_database_stats_format_t;

/**
 * @brief Print a single snapshot of per-case statistics using the requested presentation.
 * @param[in,out] database Borrowed open storage; no definitions, selections or statistics are changed.
 * @param[in] prefix Optional borrowed source-path prefix; NULL selects every recorded case.
 * @param[in] format Requested output style; automatic mode selects the native Unicode grid.
 * @param[in] details True includes every metric as a per-case field/value table; TSV always includes every field.
 * @return True after writing the report; false on SQLite, allocation, locale or output failure, with a diagnostic.
 * @note Uses one SQL query and owns/releases its display snapshot. Terminal widths are local to this call;
 * the process locale is not changed. Call sequentially; no reader is installed or launched.
 * Headers use bold, source headings use bold bright gold and test names use bright cyan. Mean values use bold light
 * cyan and median values use bold yellow. Change percentages use bright green below -5%, red above +5%, and RGB
 * amber (#FF8C00) within that
 * inclusive band. Grid values carry bold filled triangles for faster (up) or slower (down), or a bar for little change,
 * including when colours are disabled; missing comparisons stay uncoloured and unmarked.
 * This display band never changes the timing-outlier policy. Markdown and TSV percentages stay numeric.
 * Human outcomes use bold bright green PASS, bright red FAIL or bright yellow SKIPPED. Terminal styling honours
 * NO_COLOR, TERM and CLICOLOR_FORCE. The grid retains colours through supported terminals and ANSI-aware pagers.
 * Markdown uses standard strong emphasis only; Markdown and TSV never contain terminal escape sequences,
 * including when terminal styling is forced. TSV retains the original stored outcome spelling.
 */
bool test_database_stats_print(test_database_t *database, const char *prefix, test_database_stats_format_t format,
                               bool details);

/**
 * @brief Store the latest diagnostic on a borrowed connection.
 * @param[in,out] database Borrowed connection; NULL is ignored.
 * @param[in] text Borrowed diagnostic copied before return; NULL uses SQLite's current error.
 * @return False, allowing direct propagation of an operation failure.
 */
bool test_database_fail(test_database_t *database, const char *text);

/**
 * @brief Merge the schema and automatically discovered per-module SQL files inside the caller's transaction.
 * @param[in,out] database Borrowed writable connection; caller commits or rolls back the transaction.
 * @param[in] root Borrowed repository root locating tests/test_database.sql; NULL means the current directory.
 * @return True on success; false on file, schema or SQL failure. Existing case definitions are never replaced.
 */
bool test_database_seed(test_database_t *database, const string_t *root);

/**
 * @brief Hash the shared schema, JSON catalogue and sorted module SQL names and contents.
 * @param[in] root Borrowed repository root; NULL means the current directory.
 * @return Owned SHA-256 hexadecimal string released with string_free(), or NULL on discovery or read failure.
 * @note The digest is independent of checkout location and modification times. No database is opened or created.
 */
string_t *test_database_sources_fingerprint(const string_t *root);

/**
 * @brief Compare current source contents with the last successfully committed complete synchronisation.
 * @param[in,out] database Borrowed compatible open connection, used only for reading.
 * @param[in] root Borrowed repository root locating the authored sources; NULL means the current directory.
 * @param[out] current Required destination; false for an older schema or a missing synchronisation record.
 * @return True on a completed comparison, or false with a retained diagnostic on read or SQL failure.
 * @note Source enumeration and hashing visit each authored file once; metadata lookup uses its primary key.
 */
bool test_database_sync_current(test_database_t *database, const string_t *root, bool *current);

/**
 * @brief Step an owned write statement once and finalise it on every path.
 * @param[in,out] database Borrowed connection.
 * @param[in,out] statement Owned statement consumed by the function; NULL returns false.
 * @return True for SQLITE_DONE, or false with a retained diagnostic.
 */
bool test_database_write(test_database_t *database, sqlite3_stmt *statement);

/**
 * @brief Insert or refresh one catalogue entry with bound parameter values.
 * @param[in,out] database Borrowed connection; any current transaction is retained.
 * @param[in] result Borrowed metadata; outcome and elapsed fields are ignored.
 * @return True on success, or false with a retained diagnostic.
 */
bool test_database_catalogue(test_database_t *database, const test_database_result_t *result);

/**
 * @brief Import module-row identities using the reviewable case-runner mapping.
 * @param[in,out] database Borrowed connection in an active catalogue-import transaction.
 * @return True on success; false on invalid module mappings or SQLite failure. No callback or test is executed.
 */
bool test_database_import_cases(test_database_t *database);

/**
 * @brief Add one executed case to lifetime statistics inside the caller's report transaction.
 * @param[in,out] database Borrowed writable connection in a transaction.
 * @param[in] result Borrowed completed result with a non-NULL UTC completion timestamp.
 * @param[in] memory_test True excludes this duration from ordinary statistics and records a memory-test date.
 * @param[in] timing_outlier True retains the execution but excludes its duration from the clean timing baseline.
 * @return True on success or for a skipped case; false with a retained diagnostic on SQL failure.
 */
bool test_database_history_record(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  bool timing_outlier);

/**
 * @brief Assess a duration against the prior accepted median and median absolute deviation.
 * @param[in,out] database Borrowed connection in the report transaction; assessment does not change samples.
 * @param[in] result Borrowed completed record being assessed.
 * @param[in] memory_test True bypasses ordinary timing classification.
 * @param[out] assessment Required destination, cleared before assessment. Insufficient history is not an error.
 * @return True on success, including insufficient history; false with a retained SQL diagnostic on failure.
 */
bool test_database_history_assess(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  test_timing_assessment_t *assessment);

/**
 * @brief Upgrade old report columns and populate lifetime statistics exactly once.
 * @param[in,out] database Borrowed connection inside a schema-import transaction.
 * @return True on success; false on schema or history replay failure. The caller rolls back on failure.
 */
bool test_database_history_upgrade(test_database_t *database);

/**
 * @brief Consolidate a validated legacy report database into the default test database.
 * @param[in,out] database Borrowed default connection, with no active transaction.
 * @param[in] root Borrowed repository root; NULL selects the current directory.
 * @return True if no legacy file exists or migration succeeds; false leaves the source file available for retry.
 * @note Reports are fingerprinted for idempotence. The source is removed only after a successful commit.
 */
bool test_database_reports_migrate(test_database_t *database, const string_t *root);

#endif /* MARS_TEST_DATABASE_INTERNAL_H */
