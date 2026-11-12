-- Persistent benchmark definitions and observations; created only by explicit benchmark commands.
-- Allow short-lived readers and report writers to finish before a checkpoint fails.
PRAGMA busy_timeout = 5000;
PRAGMA application_id = 1296126546;
PRAGMA user_version = 4;

CREATE TABLE IF NOT EXISTS benchmark_operation (
    name TEXT PRIMARY KEY,
    arity INTEGER NOT NULL,
    arguments TEXT NOT NULL,
    complex_mask INTEGER NOT NULL,
    qfloat_supported INTEGER NOT NULL,
    qcomplex_supported INTEGER NOT NULL
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS benchmark_profile (
    backend TEXT NOT NULL,
    bits INTEGER NOT NULL CHECK (bits >= 0),
    PRIMARY KEY (backend, bits)
) WITHOUT ROWID;
INSERT OR IGNORE INTO benchmark_profile VALUES
    ('number-double', 53), ('qfloat', 106), ('qcomplex', 106), ('exact-integer', 0), ('exact-fraction', 0),
    ('mpfr', 256), ('mpfr', 512), ('mpfr', 768), ('mpfr', 1024), ('mpfr', 2048), ('mpfr', 4096),
    ('mpc', 256), ('mpc', 512), ('mpc', 768), ('mpc', 1024), ('mpc', 2048), ('mpc', 4096);

-- Exact arithmetic has different operands from floating-point measurements of the same operation.
CREATE TABLE IF NOT EXISTS benchmark_workload (
    operation TEXT NOT NULL REFERENCES benchmark_operation(name),
    backend TEXT NOT NULL CHECK (backend IN ('exact-integer', 'exact-fraction')),
    arguments TEXT NOT NULL,
    arguments_display TEXT,
    PRIMARY KEY (operation, backend)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS benchmark_run (
    id INTEGER PRIMARY KEY,
    started_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    finished_utc TEXT,
    status TEXT NOT NULL CHECK (status IN ('running', 'complete', 'interrupted')),
    machine TEXT NOT NULL,
    cpu TEXT NOT NULL,
    system TEXT NOT NULL,
    compiler TEXT NOT NULL,
    build_flags TEXT NOT NULL,
    library_sha256 TEXT NOT NULL,
    executable_sha256 TEXT NOT NULL,
    requested_samples INTEGER NOT NULL,
    case_timeout_seconds REAL NOT NULL,
    clock TEXT NOT NULL DEFAULT 'CLOCK_MONOTONIC',
    filter TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS benchmark_result (
    run_id INTEGER NOT NULL REFERENCES benchmark_run(id),
    operation TEXT NOT NULL REFERENCES benchmark_operation(name),
    backend TEXT NOT NULL,
    bits INTEGER NOT NULL,
    arguments TEXT NOT NULL,
    complex_mask INTEGER NOT NULL,
    sequence INTEGER NOT NULL,
    outcome TEXT NOT NULL DEFAULT 'pending'
        CHECK (outcome IN ('pending', 'pass', 'fail', 'unsupported', 'timeout')),
    finished_utc TEXT,
    iterations INTEGER,
    resolution_ns INTEGER,
    median_ns REAL,
    mean_ns REAL,
    minimum_ns REAL,
    maximum_ns REAL,
    mad_ns REAL,
    sample_count INTEGER,
    timing_outlier INTEGER NOT NULL DEFAULT 0 CHECK (timing_outlier IN (0, 1)),
    baseline_median_ns REAL,
    detail TEXT NOT NULL DEFAULT '',
    arguments_display TEXT,
    PRIMARY KEY (run_id, operation, backend, bits),
    FOREIGN KEY (backend, bits) REFERENCES benchmark_profile(backend, bits)
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS benchmark_result_comparison
    ON benchmark_result(operation, backend, bits, run_id);
CREATE INDEX IF NOT EXISTS benchmark_result_pending ON benchmark_result(run_id, outcome, sequence);
CREATE INDEX IF NOT EXISTS benchmark_result_missing_display
    ON benchmark_result(arguments, backend, complex_mask) WHERE arguments_display IS NULL;
CREATE INDEX IF NOT EXISTS benchmark_workload_missing_display
    ON benchmark_workload(arguments, backend) WHERE arguments_display IS NULL;

CREATE TABLE IF NOT EXISTS benchmark_sample (
    run_id INTEGER NOT NULL,
    operation TEXT NOT NULL,
    backend TEXT NOT NULL,
    bits INTEGER NOT NULL,
    ordinal INTEGER NOT NULL,
    nanoseconds_per_call REAL NOT NULL CHECK (nanoseconds_per_call >= 0),
    PRIMARY KEY (run_id, operation, backend, bits, ordinal),
    FOREIGN KEY (run_id, operation, backend, bits)
        REFERENCES benchmark_result(run_id, operation, backend, bits)
) WITHOUT ROWID;

-- Keep policy beside its observations. Catalogue synchronisation never resets user settings.
CREATE TABLE IF NOT EXISTS benchmark_retention (
    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
    max_mib INTEGER NOT NULL CHECK (max_mib BETWEEN 1 AND 4096),
    keep_runs INTEGER NOT NULL CHECK (keep_runs BETWEEN 1 AND 1000),
    max_days INTEGER NOT NULL CHECK (max_days BETWEEN 1 AND 3650),
    raw_runs INTEGER NOT NULL CHECK (raw_runs BETWEEN 0 AND keep_runs),
    last_run_id INTEGER NOT NULL CHECK (last_run_id >= 0)
);
INSERT OR IGNORE INTO benchmark_retention
    SELECT 1, 32, 20, 90, 5, coalesce(max(id), 0) FROM benchmark_run;

-- Compare only the same machine, CPU, system, compiler, flags, workload and backend precision.
-- Retained slowdown outliers do not silently become the next baseline.
CREATE VIEW IF NOT EXISTS benchmark_comparison AS
SELECT r.*, b.machine, b.cpu, b.system, b.compiler, b.build_flags, b.started_utc, b.status AS run_status
FROM benchmark_result AS r
JOIN benchmark_run AS b ON b.id = r.run_id;
