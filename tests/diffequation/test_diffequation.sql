-- diffequation regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS diffequation_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS diffequation_cases_operation ON diffequation_cases(operation, name);

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('distinct_root_velocity', 'partial_conditions', 'y''''-y=0; y''(2)=1', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('forced_harmonic_position', 'partial_conditions', 'y''''+y=x; y(0)=1', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('free_particle_velocity', 'partial_conditions', 'y''''=0; y''(0)=1', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('harmonic_boundary', 'partial_conditions', 'y''''+y=0; y(0)=1; y(@pi/2)=0', '0');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('harmonic_position', 'partial_conditions', 'y''''+y=0; y(0)=1', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('harmonic_velocity', 'partial_conditions', 'y''''+y=0; y''(0)=2', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('oscillator_position', 'partial_conditions', 'm*d²x/dt²+c*dx/dt+k*x=0; x(0)=1', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('redundant_data', 'partial_conditions', 'y''''''=0; y(0)=1; y(0)=1', '2');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('repeated_root_position', 'partial_conditions', 'y''''+2y''+y=0; y(1)=3', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('third_order_position', 'partial_conditions', 'y''''''=0; y(0)=1', '2');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('third_order_position_velocity', 'partial_conditions', 'y''''''=0; y(0)=1; y''(0)=2', '1');

INSERT OR IGNORE INTO diffequation_cases(name, operation, input, expected) VALUES ('variable_coefficient_position', 'partial_conditions', 'y''''-(x^2+1)*y=0; y(0)=1', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'diffequation', 'tests/diffequation/test_diffequation.c', 'test_diffequ_linear_partial_conditions', 'diffequation_cases', 'partial_conditions')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
