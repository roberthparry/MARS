-- Portable test definitions, persistent statistics and bounded detailed reports; used only by test tools.
PRAGMA application_id = 1297371988;
PRAGMA user_version = 8;

-- Derived verification checkpoints, never new measurements or replacements for recorded test history.
CREATE TABLE IF NOT EXISTS test_checkpoint (
    scope TEXT PRIMARY KEY,
    fingerprint TEXT NOT NULL CHECK (length(fingerprint) = 64),
    passed_count INTEGER CHECK (passed_count >= 0),
    finished_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
) WITHOUT ROWID;

-- Only a successfully committed, complete catalogue import records this content fingerprint.
CREATE TABLE IF NOT EXISTS test_source_sync (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    source_sha256 TEXT NOT NULL CHECK (length(source_sha256) = 64),
    synced_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS test_catalogue (
    source TEXT NOT NULL,
    path TEXT NOT NULL,
    name TEXT NOT NULL,
    parent TEXT NOT NULL DEFAULT '',
    tags TEXT NOT NULL DEFAULT '',
    kind TEXT NOT NULL CHECK (kind IN ('test', 'group', 'readme')),
    declaration_line INTEGER NOT NULL DEFAULT 0,
    confirmed INTEGER NOT NULL DEFAULT 1 CHECK (confirmed IN (0, 1)),
    PRIMARY KEY (source, path)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS test_profile (
    name TEXT PRIMARY KEY,
    default_enabled INTEGER NOT NULL CHECK (default_enabled IN (0, 1))
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS test_selection (
    profile TEXT NOT NULL REFERENCES test_profile(name) ON DELETE CASCADE,
    source TEXT NOT NULL,
    path TEXT NOT NULL,
    enabled INTEGER NOT NULL CHECK (enabled IN (0, 1)),
    PRIMARY KEY (profile, source, path)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS test_run (
    id INTEGER PRIMARY KEY,
    executable TEXT NOT NULL,
    profile TEXT NOT NULL DEFAULT '',
    started_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    finished_utc TEXT,
    exit_code INTEGER,
    elapsed_ms REAL,
    memory_test INTEGER NOT NULL DEFAULT 0 CHECK (memory_test IN (0, 1))
);

CREATE TABLE IF NOT EXISTS test_result (
    run_id INTEGER NOT NULL REFERENCES test_run(id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    source TEXT NOT NULL,
    path TEXT NOT NULL,
    outcome TEXT NOT NULL CHECK (outcome IN ('pass', 'fail', 'skip')),
    enabled INTEGER NOT NULL CHECK (enabled IN (0, 1)),
    elapsed_ms REAL NOT NULL,
    finished_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    timing_outlier INTEGER NOT NULL DEFAULT 0 CHECK (timing_outlier IN (0, 1)),
    baseline_median_ms REAL,
    outlier_threshold_ms REAL,
    detail TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (run_id, ordinal)
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS test_result_case ON test_result(source, path, run_id);
CREATE INDEX IF NOT EXISTS test_result_outcome ON test_result(outcome, run_id);

-- Lifetime statistics are not pruned with the bounded, detailed run reports.
CREATE TABLE IF NOT EXISTS test_case_statistics (
    source TEXT NOT NULL,
    path TEXT NOT NULL,
    run_count INTEGER NOT NULL,
    last_run_utc TEXT NOT NULL,
    last_outcome TEXT NOT NULL,
    last_elapsed_ms REAL NOT NULL,
    normal_count INTEGER NOT NULL DEFAULT 0,
    total_normal_ms REAL NOT NULL DEFAULT 0,
    fastest_ms REAL,
    slowest_ms REAL,
    last_normal_utc TEXT,
    last_normal_ms REAL,
    last_normal_outcome TEXT,
    previous_normal_utc TEXT,
    previous_normal_ms REAL,
    memory_test_count INTEGER NOT NULL DEFAULT 0,
    last_memory_test_utc TEXT,
    timing_outlier_count INTEGER NOT NULL DEFAULT 0,
    last_timing_outlier_utc TEXT,
    last_timing_outlier_ms REAL,
    last_normal_outlier INTEGER,
    PRIMARY KEY (source, path)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS test_timing_policy (
    name TEXT PRIMARY KEY,
    minimum_samples INTEGER NOT NULL CHECK (minimum_samples >= 3),
    median_multiplier REAL NOT NULL CHECK (median_multiplier > 1),
    deviation_multiplier REAL NOT NULL CHECK (deviation_multiplier > 0),
    minimum_change_ms REAL NOT NULL CHECK (minimum_change_ms >= 0),
    window_samples INTEGER NOT NULL DEFAULT 31 CHECK (window_samples >= 3)
) WITHOUT ROWID;
INSERT OR IGNORE INTO test_timing_policy(name,minimum_samples,median_multiplier,deviation_multiplier,minimum_change_ms)
VALUES ('default', 7, 1.5, 6, 0.05);

-- Case identities are stored once; legacy estimates preserve warnings during the rolling-window transition.
CREATE TABLE IF NOT EXISTS test_timing_window (
    id INTEGER PRIMARY KEY,
    source TEXT NOT NULL,
    path TEXT NOT NULL,
    legacy_median_ms REAL,
    legacy_mad_ms REAL,
    legacy_sample_count INTEGER NOT NULL DEFAULT 0,
    UNIQUE (source, path)
);

-- Only the most recently recorded accepted observations are retained for each case.
CREATE TABLE IF NOT EXISTS test_timing_observation (
    case_id INTEGER NOT NULL REFERENCES test_timing_window(id) ON DELETE CASCADE,
    sample_ordinal INTEGER NOT NULL CHECK (sample_ordinal > 0),
    elapsed_ms REAL NOT NULL CHECK (elapsed_ms >= 0),
    PRIMARY KEY (case_id, sample_ordinal)
) WITHOUT ROWID;

-- Completed legacy reports are fingerprinted so an interrupted cleanup never imports them twice.
CREATE TABLE IF NOT EXISTS test_imported_report (
    fingerprint TEXT PRIMARY KEY,
    run_id INTEGER REFERENCES test_run(id) ON DELETE SET NULL
) WITHOUT ROWID;

DROP VIEW IF EXISTS test_case_metrics;
CREATE VIEW test_case_metrics AS
WITH ranked AS (
    SELECT case_id, elapsed_ms,
           row_number() OVER (PARTITION BY case_id ORDER BY elapsed_ms, sample_ordinal) AS rank,
           count(*) OVER (PARTITION BY case_id) AS sample_count
    FROM test_timing_observation
), medians AS (
    SELECT case_id, avg(elapsed_ms) AS median_ms, max(sample_count) AS median_samples
    FROM ranked WHERE rank IN ((sample_count + 1) / 2, (sample_count + 2) / 2)
    GROUP BY case_id
)
SELECT c.source, c.path, c.name, c.kind,
       coalesce(s.run_count, 0) AS run_count,
       s.last_run_utc, s.last_outcome, s.last_elapsed_ms,
       coalesce(s.normal_count, 0) AS normal_count,
       coalesce(s.normal_count + s.timing_outlier_count, 0) AS ordinary_count,
       s.last_normal_utc, s.last_normal_outcome, s.last_normal_ms,
       s.previous_normal_ms,
       s.last_normal_ms - s.previous_normal_ms AS change_ms,
       CASE WHEN s.previous_normal_ms > 0
            THEN 100.0 * (s.last_normal_ms - s.previous_normal_ms) / s.previous_normal_ms END AS change_percent,
       CASE WHEN s.previous_normal_ms IS NOT NULL
            THEN s.last_normal_ms > s.previous_normal_ms END AS took_longer,
       s.fastest_ms, s.slowest_ms,
       CASE WHEN s.normal_count > 0 THEN s.total_normal_ms / s.normal_count END AS mean_ms,
       m.median_ms, coalesce(m.median_samples, 0) AS median_samples,
       coalesce(s.timing_outlier_count, 0) AS timing_outlier_count,
       s.last_normal_outlier,
       s.last_timing_outlier_utc, s.last_timing_outlier_ms,
       coalesce(s.memory_test_count, 0) AS memory_test_count,
       s.last_memory_test_utc
FROM test_catalogue c
LEFT JOIN test_case_statistics s ON s.source = c.source AND s.path = c.path
LEFT JOIN test_timing_window w ON w.source = c.source AND w.path = c.path
LEFT JOIN medians m ON m.case_id = w.id;
INSERT OR IGNORE INTO test_profile(name, default_enabled) VALUES ('all', 1);

-- Module SQL files register their indexed case table and optional operation group.
CREATE TABLE IF NOT EXISTS test_case_runner (
    module TEXT PRIMARY KEY,
    source TEXT NOT NULL,
    parent TEXT NOT NULL,
    table_name TEXT,
    operation TEXT
) WITHOUT ROWID;
