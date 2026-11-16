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

-- Scaled Bessel bases: expected remaining constants and two native formula fragments, or rejection.
INSERT INTO diffequation_cases(name, operation, input, expected) VALUES
    ('bessel_half', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0', '2|cos|sin'),
    ('bessel_half_decimal', 'bessel', 'x^2y''''+xy''+(x^2-0.25)y=0', '2|cos|sin'),
    ('bessel_normal_form', 'bessel', 'y''''+y''/x+(1-1/(4x^2))y=0', '2|cos|sin'),
    ('bessel_common_multiplier', 'bessel', '(x+1)*(x^2y''''+xy''+(x^2-1/4)y)=0', '2|cos|sin'),
    ('bessel_zero_order', 'bessel', 'x^2y''''+xy''+x^2y=0', '2|J|Y'),
    ('bessel_integer_order', 'bessel', 'x^2y''''+xy''+(4x^2-4)y=0', '2|J|Y'),
    ('bessel_fractional_order', 'bessel', 'x^2y''''+xy''+(x^2-1/9)y=0', '2|J|Y'),
    ('bessel_scaled', 'bessel', '3x^2y''''+3xy''+(12x^2-3/4)y=0', '2|cos|sin'),
    ('bessel_renamed', 'bessel', 't^2Dtt(u)+t*Dt(u)+(9t^2-1/4)u=0', '2|cos|sin'),
    ('bessel_envelope', 'bessel', 'x^2y''''+3xy''+(4x^2+3/4)y=0', '2|cos|sin'),
    ('bessel_modified_half', 'bessel', 'x^2y''''+xy''-(4x^2+1/4)y=0', '2|cosh|sinh'),
    ('bessel_modified_integer', 'bessel', 'x^2y''''+xy''-(x^2+4)y=0', '2|I|K'),
    ('bessel_euler_distinct', 'bessel', 'x^2y''''+3xy''-3y=0', '2|C₁|C₂'),
    ('bessel_euler_repeated', 'bessel', 'x^2y''''+3xy''+y=0', '2|ln|C₂'),
    ('bessel_euler_oscillatory', 'bessel', 'x^2y''''+3xy''+2y=0', '2|cos|sin'),
    ('bessel_position', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(1)=2', '1|cos|sin'),
    ('bessel_initial', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(1)=2; y''(1)=-1', '0|cos|sin'),
    ('bessel_negative_initial', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(-1)=2; y''(-1)=1', '0|cos|sin'),
    ('bessel_boundary', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(1)=2; y(2)=3', '0|cos|sin'),
    ('bessel_forced_reject', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=x', 'reject'),
    ('bessel_linear_term_reject', 'bessel', 'x^2y''''+xy''+(x^2+x-1/4)y=0', 'reject'),
    ('bessel_nonlinear_reject', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y^2=0', 'reject'),
    ('bessel_unknown_order_reject', 'bessel', 'x^2y''''+xy''+(x^2-a)y=0', 'reject'),
    ('bessel_imaginary_order_reject', 'bessel', 'x^2y''''+xy''+(x^2+1)y=0', 'reject'),
    ('bessel_singular_data_reject', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(0)=1; y''(0)=0', 'reject'),
    ('bessel_cross_origin_reject', 'bessel', 'x^2y''''+xy''+(x^2-1/4)y=0; y(-1)=1; y(1)=1', 'reject')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'diffequation.bessel', 'tests/diffequation/test_diffequation.c', 'test_diffequ_bessel', 'diffequation_cases', 'bessel')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'diffequation.solution_notation', 'tests/diffequation/test_diffequation.c', 'test_diffequ_solution_notation',
    'diffequation_cases', 'solution_notation')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO diffequation_cases(name, operation, input, expected) VALUES
    ('solution_notation_bessel', 'solution_notation', 'x^2y''''+xy''+(x^2-1/4)y=0', 'y\left(x\right) &= \frac{C_{1}'),
    ('solution_notation_conditioned', 'solution_notation', 'x^2y''''+xy''+(x^2-1/4)y=0; y(1)=2; y''(1)=-1', 'y\left(x\right) &= \frac{2'),
    ('solution_notation_renamed', 'solution_notation', 't^2Dtt(u)+t*Dt(u)+(t^2-1/4)u=0', 'u\left(t\right) &= \frac{C_{1}'),
    ('solution_notation_first_order', 'solution_notation', 'y''=y; y(0)=1', 'y\left(x\right) &= '),
    ('solution_notation_time', 'solution_notation', 'x''''+x=0', 'x\left(t\right) &= '),
    ('solution_notation_partial', 'solution_notation', 'u_t+u_x=0', 'u &= ')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
