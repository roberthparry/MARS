-- matrix regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS matrix_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS matrix_cases_operation ON matrix_cases(operation, name);

INSERT OR IGNORE INTO matrix_cases(name, operation, input, expected) VALUES ('identity_determinant', 'determinant', '(1,0;0,1)', '1');

INSERT OR IGNORE INTO matrix_cases(name, operation, input, expected) VALUES ('two_by_two_determinant', 'determinant', '(1,2;3,4)', '-2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'matrix', 'tests/matrix/test_matrix.c', 'tests.test_matrix_database_cases', 'matrix_cases', 'determinant')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
