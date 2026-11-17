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

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'matrix.appell_family', 'tests/matrix/test_matrix_appell.c', 'tests.run_matrix_appell_tests',
    'matrix_cases', 'appell_family')
ON CONFLICT(module) DO UPDATE SET source=excluded.source, parent=excluded.parent,
    table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO matrix_cases(name, operation, input, expected, precision)
WITH families(family, parameters, factor) AS (
    VALUES (1, '1,1,1,2', '3/64'), (2, '1,1,1,1,1', '3/32'),
           (3, '1,1,1,1,2', '3/64'), (4, '1,1,1,1', '3/32')
)
SELECT 'appell_F' || family || '_nilpotent', 'appell_family',
    family || '##' || parameters || '##(0,1/16;0,0)##(0,1/32;0,0)',
    '1##' || factor || '##0##1', 80 FROM families;

INSERT INTO matrix_cases(name, operation, input, expected, precision) VALUES
    ('appell_F2_diagonal', 'appell_family', '2##1,1,1,1,1##(1/16,0;0,1/32)##(1/32,0;0,1/64)',
     '32/29##0##0##64/61', 80),
    ('appell_F3_diagonal', 'appell_family', '3##1,1,1,1,2##(1/16,0;0,1/32)##(1/32,0;0,1/64)',
     '-ln((1-1/16)*(1-1/32))/(1/16+1/32-1/512)##0##0##-ln((1-1/32)*(1-1/64))/(1/32+1/64-1/2048)', 80),
    ('appell_F4_diagonal', 'appell_family', '4##1,1,1,1##(1/16,0;0,1/32)##(1/32,0;0,1/64)',
     '1/sqrt((1-1/16-1/32)^2-4/512)##0##0##1/sqrt((1-1/32-1/64)^2-4/2048)', 80),
    ('appell_F2_jordan', 'appell_family', '2##1,1,1,1,1##(1/16,1/64;0,1/16)##(1/32,1/128;0,1/32)',
     '32/29##(1/64+1/128)*(32/29)^2##0##32/29', 80),
    ('appell_F2_noncommuting', 'appell_family', '2##1,1,1,1,1##(0,1/16;0,0)##(0,0;1/32,0)', 'NULL', 80),
    ('appell_F2_nonsquare', 'appell_family', '2##1,1,1,1,1##(0,0)##(0,0)', 'NULL', 80),
    ('appell_F2_pole', 'appell_family', '2##1,1,1,0,1##(0,1/16;0,0)##(0,1/32;0,0)', 'NULL', 80),
    ('appell_F4_outside', 'appell_family', '4##1,1,1,1##(1,1/16;0,1)##(1,1/32;0,1)', 'NULL', 80),
    ('appell_F2_polynomial', 'appell_family', '2##-1,1,1,1,1##(2,1/16;0,2)##(3,1/32;0,3)',
     '-4##-3/32##0##-4', 80)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;
