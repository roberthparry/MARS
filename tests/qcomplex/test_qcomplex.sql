-- Implicit imaginary coefficients use native values; explicit tuple expectations also check signed components.
CREATE TABLE IF NOT EXISTS qcomplex_cases (
    name TEXT PRIMARY KEY,
    operation TEXT NOT NULL,
    input TEXT NOT NULL,
    expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 31 CHECK (precision BETWEEN 1 AND 31),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS qcomplex_cases_operation ON qcomplex_cases(operation,name);

INSERT INTO test_case_runner(module,source,parent,table_name,operation) VALUES (
    'qcomplex.native_coefficients','tests/qcomplex/test_qcomplex.c','tests.test_native_coefficients',
    'qcomplex_cases','parse')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name,operation=excluded.operation;

INSERT OR IGNORE INTO qcomplex_cases(name,operation,input,expected) VALUES
    ('native_unit_i','parse','i','(0,1)'),
    ('native_unit_plus_i','parse','+i','(0,1)'),
    ('native_unit_minus_i','parse','-i','(0,-1)'),
    ('native_unit_j','parse','j','(0,1)'),
    ('native_unit_plus_j','parse','+j','(0,1)'),
    ('native_unit_minus_j','parse','-j','(0,-1)'),
    ('native_unit_spaces','parse','  - j  ','(0,-1)'),
    ('native_zero_i','parse','0i','(0,0)'),
    ('native_zero_plus_i','parse','+0i','(0,0)'),
    ('native_zero_minus_i','parse','-0i','(0,-0)'),
    ('native_numeric_i','parse','1.25i','(0,1.25)'),
    ('native_numeric_j','parse','1e2j','(0,100)'),
    ('native_real_plus_i','parse','2+i','(2,1)'),
    ('native_real_minus_i','parse','2-i','(2,-1)'),
    ('native_i_plus_real','parse','i+2','(2,1)'),
    ('native_minus_i_plus_real','parse','-i+2','(2,-1)'),
    ('native_plus_j_minus_real','parse','+j-2','(-2,1)'),
    ('native_minus_j_minus_real','parse','-j-2','(-2,-1)'),
    ('native_negative_zero_plus_i','parse','-0+i','(-0,1)'),
    ('native_negative_zero_minus_i','parse','-0-i','(-0,-1)');
