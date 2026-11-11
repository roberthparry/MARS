-- equation regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS equation_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS equation_cases_operation ON equation_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.complex_power_families', 'tests/equation/test_equation.c',
    'tests.test_equation_complex_power_families', 'equation_cases', 'complex_power_family')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('complex_power_i', 'complex_power_family', 'i^x=2', 'x=4*n-2*i*ln(2)/pi', 78),
    ('complex_power_reversed', 'complex_power_family', '2=i^x', 'x=4*n-2*i*ln(2)/pi', 78),
    ('complex_power_seeded', 'complex_power_family', '{i^x=2 | x=-0.44i}', 'x=4*n-2*i*ln(2)/pi', 78),
    ('complex_power_negative_i', 'complex_power_family', '(-i)^x=2', 'x=-4*n+2*i*ln(2)/pi', 78),
    ('complex_power_negative_unit', 'complex_power_family', '(-1)^x=2', 'x=2*n-i*ln(2)/pi', 78),
    ('complex_power_negative_base', 'complex_power_family', '(-2)^x=3',
     'x=(ln(3)+2*pi*i*n)/(ln(2)+pi*i)', 78),
    ('complex_power_cartesian_base', 'complex_power_family', '(1+i)^x=2',
     'x=(ln(2)+2*pi*i*n)/ln(1+i)', 78),
    ('complex_power_scaled_imaginary', 'complex_power_family', '(2i)^x=3',
     'x=(ln(3)+2*pi*i*n)/(ln(2)+pi*i/2)', 78),
    ('complex_power_affine_exponent', 'complex_power_family', 'i^(2*x+1)=2',
     'x=2*n-1/2-i*ln(2)/pi', 78),
    ('complex_power_shifted_target', 'complex_power_family', 'i^x+1=3', 'x=4*n-2*i*ln(2)/pi', 78),
    ('complex_power_parameter_collision', 'complex_power_family', '{i^(x+n)=2 | x=?; n=3}',
     'x=4*m-3-2*i*ln(2)/pi', 78),
    ('complex_power_complex_target', 'complex_power_family', 'i^x=2i',
     'x=4*n+1-2*i*ln(2)/pi', 78),
    ('complex_power_nested_branches', 'complex_power_family', 'i^(i^x)=2',
     'x=4*m-2*i*ln(4*n-2*i*ln(2)/pi)/pi', 78);

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('linear_root', 'solve_count', '2*x=4', '1');

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('quadratic_roots', 'solve_count', 'x^2=4', '2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation', 'tests/equation/test_equation.c', 'tests.test_equation_database_cases', 'equation_cases', 'solve_count')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.test_equation_preserves_principal_root_domain_sources', 'tests/equation/test_equation.c', 'tests.test_equation_preserves_principal_root_domain', 'equation_cases', 'test_equation_preserves_principal_root_domain_sources')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/equation/test_equation.c: test_equation_preserves_principal_root_domain. Existing checks are retained in C.
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_5781e769a423fb04', 'test_equation_preserves_principal_root_domain_sources', 'sqrt(7^x) = -49', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_dd97730d916f04b3', 'test_equation_preserves_principal_root_domain_sources', 'sqrt(x) = -3', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_dd97cd0d916f9da1', 'test_equation_preserves_principal_root_domain_sources', 'sqrt(x) = -i', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_389115d6296ae31f', 'test_equation_preserves_principal_root_domain_sources', '7^x = 0', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES
    ('complex_power_zero_target', 'test_equation_preserves_principal_root_domain_sources', 'i^x=0', ''),
    ('negative_power_zero_target', 'test_equation_preserves_principal_root_domain_sources', '(-1)^x=0', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.test_equation_relation_separators_accepted', 'tests/equation/test_equation.c', 'tests.test_equation_relation_separators', 'equation_cases', 'test_equation_relation_separators_accepted')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/equation/test_equation.c: test_equation_relation_separators. Existing checks are retained in C.
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_3ab00752c8abf120', 'test_equation_relation_separators_accepted', '2*x + 3 == 7', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_f1e6f77299552e8c', 'test_equation_relation_separators_accepted', '{2*x+3==7 | x=?}', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.test_equation_relation_separators_rejected', 'tests/equation/test_equation.c', 'tests.test_equation_relation_separators', 'equation_cases', 'test_equation_relation_separators_rejected')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/equation/test_equation.c: test_equation_relation_separators. Existing checks are retained in C.
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_0e711620698875f8', 'test_equation_relation_separators_rejected', 'x<=1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_fe12d02060885c2a', 'test_equation_relation_separators_rejected', 'x>=1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_80152b20aa3131cb', 'test_equation_relation_separators_rejected', 'x!=1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_76e0acefb955a644', 'test_equation_relation_separators_rejected', 'x~~=1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_438037ee7a232475', 'test_equation_relation_separators_rejected', 'x~==1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_5f204a246a9b38b4', 'test_equation_relation_separators_rejected', '0<x<=1', '');
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES ('case_70c9008f08338f89', 'test_equation_relation_separators_rejected', 'x==y==z', '');
