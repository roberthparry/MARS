/**
 * @file test_database_history.c
 * @brief Transactional lifetime test statistics and exact duration distributions.
 *
 * Detailed reports have bounded retention. Counts, extrema, means and counted duration samples survive pruning,
 * preserving an exact lifetime median. Skips never count as executions; memory-tool runs update their own dates
 * and counts without entering ordinary timing statistics. All work uses the raw test-management connection.
 */
#include <stdio.h>
#include <string.h>

#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

/* Update one case only while its containing report transaction remains rollback-safe. */
bool test_database_history_record(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  bool timing_outlier)
{
    if (!result || !result->finished_utc || !result->outcome)
        return test_database_fail(database, "A completed test statistic requires a result and UTC timestamp.");
    if (!result->enabled || !strcmp(result->outcome, "skip"))
        return true;
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_case_statistics(\n"
        "    source, path, run_count, last_run_utc, last_outcome, last_elapsed_ms,\n"
        "    normal_count, total_normal_ms, fastest_ms, slowest_ms,\n"
        "    last_normal_utc, last_normal_ms, last_normal_outcome, memory_test_count, last_memory_test_utc,\n"
        "    timing_outlier_count, last_timing_outlier_utc, last_timing_outlier_ms, last_normal_outlier\n"
        ") VALUES (\n"
        "    ?1, ?2, 1, ?3, ?4, ?5,\n"
        "    NOT ?6 AND NOT ?7, CASE WHEN ?6 OR ?7 THEN 0 ELSE ?5 END,\n"
        "    CASE WHEN NOT ?6 THEN ?5 END, CASE WHEN NOT ?6 THEN ?5 END,\n"
        "    CASE WHEN NOT ?6 THEN ?3 END, CASE WHEN NOT ?6 THEN ?5 END,\n"
        "    CASE WHEN NOT ?6 THEN ?4 END, ?6, CASE WHEN ?6 THEN ?3 END,\n"
        "    ?7, CASE WHEN ?7 THEN ?3 END, CASE WHEN ?7 THEN ?5 END, CASE WHEN NOT ?6 THEN ?7 END\n"
        ") ON CONFLICT(source, path) DO UPDATE SET\n"
        "    run_count = test_case_statistics.run_count + 1,\n"
        "    last_run_utc = max(last_run_utc, excluded.last_run_utc),\n"
        "    last_outcome = CASE WHEN excluded.last_run_utc >= last_run_utc\n"
        "                        THEN excluded.last_outcome ELSE last_outcome END,\n"
        "    last_elapsed_ms = CASE WHEN excluded.last_run_utc >= last_run_utc\n"
        "                           THEN excluded.last_elapsed_ms ELSE last_elapsed_ms END,\n"
        "    normal_count = test_case_statistics.normal_count + excluded.normal_count,\n"
        "    total_normal_ms = test_case_statistics.total_normal_ms + excluded.total_normal_ms,\n"
        "    fastest_ms = CASE WHEN excluded.fastest_ms IS NULL THEN fastest_ms\n"
        "                      WHEN fastest_ms IS NULL THEN excluded.fastest_ms\n"
        "                      ELSE min(fastest_ms, excluded.fastest_ms) END,\n"
        "    slowest_ms = CASE WHEN excluded.slowest_ms IS NULL THEN slowest_ms\n"
        "                      WHEN slowest_ms IS NULL THEN excluded.slowest_ms\n"
        "                      ELSE max(slowest_ms, excluded.slowest_ms) END,\n"
        "    previous_normal_utc = CASE\n"
        "        WHEN ?6 THEN previous_normal_utc\n"
        "        WHEN last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc THEN last_normal_utc\n"
        "        WHEN previous_normal_utc IS NULL OR excluded.last_normal_utc > previous_normal_utc\n"
        "             THEN excluded.last_normal_utc ELSE previous_normal_utc END,\n"
        "    previous_normal_ms = CASE\n"
        "        WHEN ?6 THEN previous_normal_ms\n"
        "        WHEN last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc THEN last_normal_ms\n"
        "        WHEN previous_normal_utc IS NULL OR excluded.last_normal_utc > previous_normal_utc\n"
        "             THEN excluded.last_normal_ms ELSE previous_normal_ms END,\n"
        "    last_normal_utc = CASE WHEN NOT ?6\n"
        "                               AND (last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc)\n"
        "                           THEN excluded.last_normal_utc ELSE last_normal_utc END,\n"
        "    last_normal_ms = CASE WHEN NOT ?6\n"
        "                              AND (last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc)\n"
        "                          THEN excluded.last_normal_ms ELSE last_normal_ms END,\n"
        "    last_normal_outcome = CASE WHEN NOT ?6\n"
        "                                   AND (last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc)\n"
        "                               THEN excluded.last_normal_outcome ELSE last_normal_outcome END,\n"
        "    memory_test_count = test_case_statistics.memory_test_count + excluded.memory_test_count,\n"
        "    last_memory_test_utc = CASE WHEN excluded.last_memory_test_utc IS NULL THEN last_memory_test_utc\n"
        "                                WHEN last_memory_test_utc IS NULL THEN excluded.last_memory_test_utc\n"
        "                                ELSE max(last_memory_test_utc, excluded.last_memory_test_utc) END,\n"
        "    timing_outlier_count = test_case_statistics.timing_outlier_count + excluded.timing_outlier_count,\n"
        "    last_timing_outlier_utc = CASE\n"
        "        WHEN NOT ?7 THEN last_timing_outlier_utc\n"
        "        WHEN last_timing_outlier_utc IS NULL OR excluded.last_timing_outlier_utc >= last_timing_outlier_utc\n"
        "             THEN excluded.last_timing_outlier_utc ELSE last_timing_outlier_utc END,\n"
        "    last_timing_outlier_ms = CASE\n"
        "        WHEN NOT ?7 THEN last_timing_outlier_ms\n"
        "        WHEN last_timing_outlier_utc IS NULL OR excluded.last_timing_outlier_utc >= last_timing_outlier_utc\n"
        "             THEN excluded.last_timing_outlier_ms ELSE last_timing_outlier_ms END,\n"
        "    last_normal_outlier = CASE WHEN NOT ?6\n"
        "                                   AND (last_normal_utc IS NULL OR excluded.last_normal_utc >= last_normal_utc)\n"
        "                               THEN excluded.last_normal_outlier ELSE last_normal_outlier END");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, result->finished_utc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, result->outcome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 5, result->elapsed_ms);
    sqlite3_bind_int(statement, 6, memory_test);
    sqlite3_bind_int(statement, 7, timing_outlier);
    bool ok = test_database_write(database, statement);
    if (!ok || memory_test || timing_outlier)
        return ok;
    statement = test_database_prepare(database,
        "INSERT INTO test_timing_sample(source, path, elapsed_ms, sample_count)\n"
        "VALUES (?1, ?2, ?3, 1)\n"
        "ON CONFLICT(source, path, elapsed_ms) DO UPDATE SET sample_count = sample_count + 1");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 3, result->elapsed_ms);
    return test_database_write(database, statement);
}

static bool test_history_column(test_database_t *database, const char *table, const char *column, bool *present)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT count(*) FROM pragma_table_info(?1) WHERE name = ?2");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, table, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, column, -1, SQLITE_STATIC);
    bool ok = sqlite3_step(statement) == SQLITE_ROW;
    *present = ok && sqlite3_column_int(statement, 0) > 0;
    sqlite3_finalize(statement);
    return ok;
}

static bool test_history_add_column(test_database_t *database, const char *table, const char *column, const char *sql)
{
    bool present = false;
    return test_history_column(database, table, column, &present) && (present || test_database_exec(database, sql));
}

/* Replay only when upgrading an older schema; ordinary seed synchronisation never counts a run twice. */
bool test_database_history_upgrade(test_database_t *database)
{
    bool memory_column = false, time_column = false;
    bool ok = test_history_column(database, "test_run", "memory_test", &memory_column) &&
              test_history_column(database, "test_result", "finished_utc", &time_column);
    if (ok && !memory_column)
        ok = test_database_exec(database, "ALTER TABLE test_run ADD COLUMN memory_test INTEGER NOT NULL DEFAULT 0");
    if (ok && !time_column)
        ok = test_database_exec(database,
            "ALTER TABLE test_result ADD COLUMN finished_utc TEXT;\n"
            "UPDATE test_result\n"
            "SET finished_utc = (SELECT coalesce(finished_utc, started_utc) FROM test_run WHERE id = run_id);");
    const struct { const char *table, *column, *sql; } additions[] = {
        {"test_result", "timing_outlier", "ALTER TABLE test_result ADD COLUMN timing_outlier INTEGER NOT NULL DEFAULT 0"},
        {"test_result", "baseline_median_ms", "ALTER TABLE test_result ADD COLUMN baseline_median_ms REAL"},
        {"test_result", "outlier_threshold_ms", "ALTER TABLE test_result ADD COLUMN outlier_threshold_ms REAL"},
        {"test_case_statistics", "timing_outlier_count",
         "ALTER TABLE test_case_statistics ADD COLUMN timing_outlier_count INTEGER NOT NULL DEFAULT 0"},
        {"test_case_statistics", "last_timing_outlier_utc",
         "ALTER TABLE test_case_statistics ADD COLUMN last_timing_outlier_utc TEXT"},
        {"test_case_statistics", "last_timing_outlier_ms",
         "ALTER TABLE test_case_statistics ADD COLUMN last_timing_outlier_ms REAL"},
        {"test_case_statistics", "last_normal_outlier",
         "ALTER TABLE test_case_statistics ADD COLUMN last_normal_outlier INTEGER"}
    };
    for (size_t i = 0; ok && i < sizeof additions / sizeof *additions; ++i)
        ok = test_history_add_column(database, additions[i].table, additions[i].column, additions[i].sql);
    if (!ok || (memory_column && time_column))
        return ok;
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT r.source, r.path, r.outcome, r.enabled, r.elapsed_ms, r.finished_utc, u.memory_test\n"
        "FROM test_result r\n"
        "JOIN test_run u ON u.id = r.run_id\n"
        "ORDER BY r.finished_utc, r.run_id, r.ordinal");
    ok = statement != NULL;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(statement)) == SQLITE_ROW) {
        test_database_result_t result = {
            .source = (const char *)sqlite3_column_text(statement, 0),
            .path = (const char *)sqlite3_column_text(statement, 1),
            .outcome = (const char *)sqlite3_column_text(statement, 2),
            .enabled = sqlite3_column_int(statement, 3) != 0,
            .elapsed_ms = sqlite3_column_double(statement, 4),
            .finished_utc = (const char *)sqlite3_column_text(statement, 5)};
        ok = test_database_history_record(database, &result, sqlite3_column_int(statement, 6) != 0, false);
    }
    sqlite3_finalize(statement);
    return (ok && status == SQLITE_DONE) || test_database_fail(database, "Cannot upgrade existing report statistics.");
}

/* Compare with prior accepted measurements; never let a flagged slowdown redefine the baseline. */
bool test_database_history_assess(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  test_timing_assessment_t *assessment)
{
    *assessment = (test_timing_assessment_t){0};
    if (memory_test || !result->enabled || !strcmp(result->outcome, "skip"))
        return true;
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT coalesce(s.normal_count, 0), p.minimum_samples\n"
        "FROM test_timing_policy p\n"
        "LEFT JOIN test_case_statistics s ON s.source = ?1 AND s.path = ?2\n"
        "WHERE p.name = 'default'");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_int64 count = ok ? sqlite3_column_int64(statement, 0) : 0;
    sqlite3_int64 minimum = ok ? sqlite3_column_int64(statement, 1) : 0;
    sqlite3_finalize(statement);
    if (!ok)
        return test_database_fail(database, "The default timing-outlier policy is missing.");
    if (count < minimum)
        return true;
    statement = test_database_prepare(database,
        "WITH ranked AS (\n"
        "    SELECT elapsed_ms, sample_count,\n"
        "           sum(sample_count) OVER (ORDER BY elapsed_ms ROWS UNBOUNDED PRECEDING) AS through_count\n"
        "    FROM test_timing_sample WHERE source = ?1 AND path = ?2\n"
        "), centre AS (\n"
        "    SELECT sum(\n"
        "        CASE WHEN through_count - sample_count < (?3 + 1) / 2 AND through_count >= (?3 + 1) / 2\n"
        "             THEN elapsed_ms ELSE 0 END\n"
        "      + CASE WHEN through_count - sample_count < (?3 + 2) / 2 AND through_count >= (?3 + 2) / 2\n"
        "             THEN elapsed_ms ELSE 0 END) / 2 AS median_ms\n"
        "    FROM ranked\n"
        "), deviations AS (\n"
        "    SELECT abs(r.elapsed_ms - c.median_ms) AS deviation, r.sample_count,\n"
        "           sum(r.sample_count) OVER (\n"
        "               ORDER BY abs(r.elapsed_ms - c.median_ms), r.elapsed_ms ROWS UNBOUNDED PRECEDING\n"
        "           ) AS through_count\n"
        "    FROM ranked r CROSS JOIN centre c\n"
        "), spread AS (\n"
        "    SELECT sum(\n"
        "        CASE WHEN through_count - sample_count < (?3 + 1) / 2 AND through_count >= (?3 + 1) / 2\n"
        "             THEN deviation ELSE 0 END\n"
        "      + CASE WHEN through_count - sample_count < (?3 + 2) / 2 AND through_count >= (?3 + 2) / 2\n"
        "             THEN deviation ELSE 0 END) / 2 AS mad_ms\n"
        "    FROM deviations\n"
        ")\n"
        "SELECT c.median_ms, max(\n"
        "    c.median_ms * p.median_multiplier,\n"
        "    c.median_ms + s.mad_ms * p.deviation_multiplier,\n"
        "    c.median_ms + p.minimum_change_ms\n"
        ")\n"
        "FROM centre c CROSS JOIN spread s CROSS JOIN test_timing_policy p\n"
        "WHERE p.name = 'default'");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 3, count);
    ok = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_type(statement, 0) != SQLITE_NULL &&
         sqlite3_column_type(statement, 1) != SQLITE_NULL;
    if (ok) {
        assessment->available = true;
        assessment->median_ms = sqlite3_column_double(statement, 0);
        assessment->threshold_ms = sqlite3_column_double(statement, 1);
        assessment->outlier = result->elapsed_ms > assessment->threshold_ms;
    }
    sqlite3_finalize(statement);
    return ok || test_database_fail(database, "Cannot assess the prior timing baseline.");
}

/* Warnings identify candidates for investigation; successful assertions keep their actual outcome. */
void test_database_history_warn(test_database_t *database, sqlite3_int64 run_id)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT source, path, elapsed_ms, baseline_median_ms, outlier_threshold_ms\n"
        "FROM test_result WHERE run_id = ?1 AND timing_outlier = 1 ORDER BY ordinal");
    if (!statement)
        return;
    sqlite3_bind_int64(statement, 1, run_id);
    while (sqlite3_step(statement) == SQLITE_ROW)
        fprintf(stderr, "Timing outlier: %s :: %s took %.6g ms; baseline median %.6g ms, threshold %.6g ms. "
                        "Review recent changes; this duration is excluded from the baseline.\n",
                sqlite3_column_text(statement, 0), sqlite3_column_text(statement, 1),
                sqlite3_column_double(statement, 2), sqlite3_column_double(statement, 3),
                sqlite3_column_double(statement, 4));
    sqlite3_finalize(statement);
}
