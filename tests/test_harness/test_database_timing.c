/**
 * @file test_database_timing.c
 * @brief Bounded per-case timing windows, robust baselines and distribution migration.
 *
 * Stores accepted ordinary timings under compact case identifiers. Indexed pruning retains the latest configured
 * observations without changing lifetime aggregates or admitting memory runs and outliers into a baseline. Older
 * counted distributions become constant-size fallback estimates plus recent observations from retained reports.
 * All mutations join the caller's test-management transaction; this file never accesses application databases.
 */
#include <string.h>

#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

/* One mapping row avoids repeating long source and group paths for every observation. */
bool test_database_timing_record(test_database_t *database, const test_database_result_t *result)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_timing_window(source,path) VALUES (?1,?2) ON CONFLICT(source,path) DO NOTHING");
    if (!statement) return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    if (!test_database_write(database, statement)) return false;
    statement = test_database_prepare(database,
        "INSERT INTO test_timing_observation(case_id,sample_ordinal,elapsed_ms)\n"
        "SELECT w.id,s.normal_count,?3 FROM test_timing_window w\n"
        "JOIN test_case_statistics s USING(source,path) WHERE w.source=?1 AND w.path=?2");
    if (!statement) return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 3, result->elapsed_ms);
    if (!test_database_write(database, statement)) return false;
    statement = test_database_prepare(database,
        "DELETE FROM test_timing_observation\n"
        "WHERE case_id=(SELECT id FROM test_timing_window WHERE source=?1 AND path=?2)\n"
        "  AND sample_ordinal <= (SELECT s.normal_count-max(p.window_samples,p.minimum_samples)\n"
        "      FROM test_case_statistics s CROSS JOIN test_timing_policy p\n"
        "      WHERE s.source=?1 AND s.path=?2 AND p.name='default')");
    if (!statement) return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    return test_database_write(database, statement);
}

/* Preserve existing warning strength when retained reports cannot initialise a complete recent baseline. */
bool test_database_timing_upgrade(test_database_t *database)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='test_timing_sample'");
    bool ok = statement && sqlite3_step(statement) == SQLITE_ROW;
    bool needed = ok && sqlite3_column_int(statement, 0) != 0;
    sqlite3_finalize(statement);
    if (!ok || !needed) return ok;
    return test_database_exec(database,
        "WITH ranked AS (\n"
        "    SELECT source,path,elapsed_ms,sample_count,\n"
        "        sum(sample_count) OVER (PARTITION BY source,path ORDER BY elapsed_ms) AS through_count,\n"
        "        sum(sample_count) OVER (PARTITION BY source,path) AS total_count\n"
        "    FROM test_timing_sample\n"
        "), centres AS (\n"
        "    SELECT source,path,max(total_count) AS total_count,\n"
        "        sum(CASE WHEN through_count-sample_count < (total_count+1)/2\n"
        "                      AND through_count >= (total_count+1)/2 THEN elapsed_ms ELSE 0 END\n"
        "          + CASE WHEN through_count-sample_count < (total_count+2)/2\n"
        "                      AND through_count >= (total_count+2)/2 THEN elapsed_ms ELSE 0 END)/2 AS median_ms\n"
        "    FROM ranked GROUP BY source,path\n"
        "), deviations AS (\n"
        "    SELECT r.source,r.path,r.sample_count,c.total_count,abs(r.elapsed_ms-c.median_ms) AS deviation,\n"
        "        sum(r.sample_count) OVER (PARTITION BY r.source,r.path\n"
        "            ORDER BY abs(r.elapsed_ms-c.median_ms),r.elapsed_ms ROWS UNBOUNDED PRECEDING) AS through_count\n"
        "    FROM ranked r JOIN centres c USING(source,path)\n"
        "), spreads AS (\n"
        "    SELECT source,path,\n"
        "        sum(CASE WHEN through_count-sample_count < (total_count+1)/2\n"
        "                      AND through_count >= (total_count+1)/2 THEN deviation ELSE 0 END\n"
        "          + CASE WHEN through_count-sample_count < (total_count+2)/2\n"
        "                      AND through_count >= (total_count+2)/2 THEN deviation ELSE 0 END)/2 AS mad_ms\n"
        "    FROM deviations GROUP BY source,path\n"
        ")\n"
        "INSERT INTO test_timing_window(source,path,legacy_median_ms,legacy_mad_ms,legacy_sample_count)\n"
        "SELECT c.source,c.path,c.median_ms,d.mad_ms,c.total_count\n"
        "FROM centres c JOIN spreads d USING(source,path) WHERE true\n"
        "ON CONFLICT(source,path) DO UPDATE SET legacy_median_ms=excluded.legacy_median_ms,\n"
        "    legacy_mad_ms=excluded.legacy_mad_ms,legacy_sample_count=excluded.legacy_sample_count;\n"
        "INSERT INTO test_timing_window(source,path)\n"
        "SELECT source,path FROM test_case_statistics WHERE normal_count>0\n"
        "ON CONFLICT(source,path) DO NOTHING;\n"
        "WITH recent AS (\n"
        "    SELECT w.id,s.normal_count,r.elapsed_ms,\n"
        "        row_number() OVER (PARTITION BY w.id\n"
        "            ORDER BY coalesce(r.finished_utc,u.finished_utc,u.started_utc) DESC,u.id DESC,r.ordinal DESC) AS recency\n"
        "    FROM test_timing_window w JOIN test_case_statistics s USING(source,path)\n"
        "    JOIN test_result r USING(source,path) JOIN test_run u ON u.id=r.run_id\n"
        "    WHERE r.enabled=1 AND r.outcome<>'skip' AND r.timing_outlier=0 AND u.memory_test=0\n"
        ")\n"
        "INSERT INTO test_timing_observation(case_id,sample_ordinal,elapsed_ms)\n"
        "SELECT r.id,r.normal_count-r.recency+1,r.elapsed_ms FROM recent r CROSS JOIN test_timing_policy p\n"
        "WHERE p.name='default' AND r.recency<=max(p.window_samples,p.minimum_samples) AND r.recency<=r.normal_count\n"
        "ON CONFLICT(case_id,sample_ordinal) DO NOTHING;\n"
        "DROP TABLE test_timing_sample;");
}

/* Assess before appending; flagged slowdowns never advance or change the accepted window. */
bool test_database_history_assess(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  test_timing_assessment_t *assessment)
{
    *assessment = (test_timing_assessment_t){0};
    if (memory_test || !result->enabled || !strcmp(result->outcome, "skip")) return true;
    sqlite3_stmt *statement = test_database_prepare(database,
        "WITH ranked AS (\n"
        "    SELECT elapsed_ms,row_number() OVER (ORDER BY elapsed_ms,sample_ordinal) AS sample_rank,\n"
        "        count(*) OVER () AS sample_count FROM test_timing_observation\n"
        "    WHERE case_id=(SELECT id FROM test_timing_window WHERE source=?1 AND path=?2)\n"
        "), centre AS (\n"
        "    SELECT avg(elapsed_ms) AS median_ms,max(sample_count) AS sample_count FROM ranked\n"
        "    WHERE sample_rank IN ((sample_count+1)/2,(sample_count+2)/2)\n"
        "), deviations AS (\n"
        "    SELECT abs(r.elapsed_ms-c.median_ms) AS deviation,r.sample_count,\n"
        "        row_number() OVER (ORDER BY abs(r.elapsed_ms-c.median_ms),r.sample_rank) AS sample_rank\n"
        "    FROM ranked r CROSS JOIN centre c\n"
        "), spread AS (\n"
        "    SELECT avg(deviation) AS mad_ms FROM deviations\n"
        "    WHERE sample_rank IN ((sample_count+1)/2,(sample_count+2)/2)\n"
        "), baseline AS (\n"
        "    SELECT CASE WHEN c.sample_count>=p.minimum_samples THEN c.median_ms ELSE w.legacy_median_ms END AS median_ms,\n"
        "        CASE WHEN c.sample_count>=p.minimum_samples THEN d.mad_ms ELSE w.legacy_mad_ms END AS mad_ms,\n"
        "        CASE WHEN c.sample_count>=p.minimum_samples THEN c.sample_count ELSE w.legacy_sample_count END AS samples\n"
        "    FROM test_timing_policy p LEFT JOIN test_timing_window w ON w.source=?1 AND w.path=?2\n"
        "    CROSS JOIN centre c CROSS JOIN spread d WHERE p.name='default'\n"
        ")\n"
        "SELECT coalesce(b.samples,0)>=p.minimum_samples,b.median_ms,\n"
        "    max(b.median_ms*p.median_multiplier,b.median_ms+b.mad_ms*p.deviation_multiplier,\n"
        "        b.median_ms+p.minimum_change_ms)\n"
        "FROM baseline b CROSS JOIN test_timing_policy p WHERE p.name='default'");
    if (!statement) return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(statement) == SQLITE_ROW;
    if (ok && sqlite3_column_int(statement, 0)) {
        ok = sqlite3_column_type(statement, 1) != SQLITE_NULL && sqlite3_column_type(statement, 2) != SQLITE_NULL;
        if (ok) {
            assessment->available = true;
            assessment->median_ms = sqlite3_column_double(statement, 1);
            assessment->threshold_ms = sqlite3_column_double(statement, 2);
            assessment->outlier = result->elapsed_ms > assessment->threshold_ms;
        }
    }
    sqlite3_finalize(statement);
    return ok || test_database_fail(database, "Cannot assess the prior timing baseline.");
}
