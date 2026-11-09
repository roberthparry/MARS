-- sqlite regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS sql_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS sql_cases_operation ON sql_cases(operation, name);

INSERT OR IGNORE INTO sql_cases(name, operation, input, expected) VALUES ('catalogue_fixture_tables', 'scalar_query', 'SELECT count(*) FROM test_profile WHERE name = ''all''', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'sqlite', 'tests/sqlite/test_sqlite.c', 'tests.test_sqlite_database_cases', 'sql_cases', 'scalar_query')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
