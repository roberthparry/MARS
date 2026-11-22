-- equation regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS equation_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS equation_cases_operation ON equation_cases(operation, name);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.zeta_trivial_window', 'tests/equation/test_equation.c',
    'tests.test_equation_zeta_trivial_window', 'equation_cases', 'zeta_trivial_window')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('trivial_window_default', 'zeta_trivial_window', 'zeta(z)=0',
     '{"lower":"-10","upper":"1","maximum":256,"roots":[-2,-4,-6,-8,-10],"limited":false}', 78),
    ('trivial_window_affine_seeded', 'zeta_trivial_window', '{3*zeta(s)+2=2 | s=0.5+14i}',
     '{"lower":"-22","upper":"-16","maximum":256,"roots":[-16,-18,-20,-22],"limited":false}', 78),
    ('trivial_window_constants', 'zeta_trivial_window', '{a*zeta(s)=0 | s=?; a=sqrt(2)}',
     '{"lower":"-8","upper":"-1","maximum":3,"roots":[-2,-4,-6],"limited":true}', 78),
    ('trivial_window_exact_capacity', 'zeta_trivial_window', '0=zeta(s)',
     '{"lower":"-6","upper":"-1","maximum":3,"roots":[-2,-4,-6],"limited":false}', 32),
    ('trivial_window_positive_empty', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"0","upper":"100","maximum":256,"roots":[],"limited":false}', 32),
    ('trivial_window_gap_empty', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-3","upper":"-2.1","maximum":256,"roots":[],"limited":false}', 32),
    ('trivial_window_near_endpoints', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-3.9999999999999999999999999999999999","upper":"-2.0000000000000000000000000000000001","maximum":256,"roots":[],"limited":false}', 78),
    ('trivial_window_narrow_included', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-2.0000000000000000000000000000000001","upper":"-1.9999999999999999999999999999999999","maximum":256,"roots":[-2],"limited":false}', 78),
    ('trivial_window_narrow_excluded', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-1.9999999999999999999999999999999999","upper":"-1.9999999999999999999999999999999998","maximum":256,"roots":[],"limited":false}', 78),
    ('trivial_window_boundary', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-10000","upper":"-9995","maximum":256,"roots":[-9996,-9998,-10000],"limited":false}', 78),
    ('trivial_window_nonzero_target', 'zeta_trivial_window', 'zeta(s)=1',
     '{"lower":"-10","upper":"1","maximum":256,"error":"ENOTSUP"}', 32),
    ('trivial_window_series_domain', 'zeta_trivial_window', 'sum(n,1,inf,1/n^s)=0',
     '{"lower":"-10","upper":"1","maximum":256,"error":"ENOTSUP"}', 32),
    ('trivial_window_pole_control', 'zeta_trivial_window', '1/zeta(s)=0',
     '{"lower":"-10","upper":"1","maximum":256,"error":"ENOTSUP"}', 32),
    ('trivial_window_other_equation', 'zeta_trivial_window', 'sin(s)=0',
     '{"lower":"-10","upper":"1","maximum":256,"error":"ENOTSUP"}', 32),
    ('trivial_window_multiple_coordinates', 'zeta_trivial_window', 'zeta(s)+t=0',
     '{"lower":"-10","upper":"1","maximum":256,"error":"ENOTSUP"}', 32),
    ('trivial_window_invalid_order', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"1","upper":"-10","maximum":256,"error":"EINVAL"}', 32),
    ('trivial_window_invalid_complex', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-10+i","upper":"1","maximum":256,"error":"EINVAL"}', 32),
    ('trivial_window_invalid_budget', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-10","upper":"1","maximum":0,"error":"EINVAL"}', 32),
    ('trivial_window_outside_limit', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-10001","upper":"1","maximum":256,"error":"EINVAL"}', 32),
    ('trivial_window_narrow_outside_limit', 'zeta_trivial_window', 'zeta(s)=0',
     '{"lower":"-10000.0000000000000000000000000000000001","upper":"-9999","maximum":256,"error":"EINVAL"}', 78),
    ('zeta_affine_bound_parameter', 'solve_count', '{a*zeta(s)=0 | s=?; a=sqrt(2)}', '40', 32),
    ('zeta_affine_bound_offset', 'solve_count', '{a*zeta(s)+b=b | s=?; a=sqrt(2),b=3}', '40', 32)
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input,
    expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.rational_relations', 'tests/equation/test_equation.c',
    'tests.test_equation_rational_relations', 'equation_cases', 'rational_relation')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO equation_cases(name, operation, input, expected) VALUES
    ('relation_circle_slice', 'rational_relation', '(4949*y^2+4949*z^2+3025)/2371600=1',
     'y^2+z^2=2368575/4949'),
    ('relation_egg_fractions', 'rational_relation', '(49*(x+100)*(y^2+1)+3025*x^2)/2371600=1',
     '49*(x+100)*(y^2+1)=3025*(784-x^2)'),
    ('relation_factored_reversed', 'rational_relation', '1=(49*(x+100)*(y^2+1)+3025*x^2)/2371600',
     '3025*(784-x^2)=49*(x+100)*(y^2+1)'),
    ('relation_factored_rationals', 'rational_relation', '(u+2)*(v^2+1)/6+w^2/4=5/3',
     '2*(u+2)*(v^2+1)=20-3*w^2'),
    ('relation_factored_negative_term', 'rational_relation', '(u+2)*(v^2+1)-3*w^2=5',
     '(u+2)*(v^2+1)=5+3*w^2'),
    ('relation_factored_negative_product', 'rational_relation', '-(u+2)*(v^2+1)/6+w^2/4=5/3',
     '2*(u+2)*(v^2+1)=3*w^2-20'),
    ('relation_factored_zero_target', 'rational_relation', '2*(u+2)*(v^2+1)+4*w^2=0',
     '(u+2)*(v^2+1)=-2*w^2'),
    ('relation_factored_integer_content', 'rational_relation', '6*(u+2)*(v^2+1)+12*w^2=24',
     '(u+2)*(v^2+1)=2*(2-w^2)'),
    ('relation_shifted_product', 'rational_relation', 'u^2+(v+3)*(w^2+1)=10',
     '(v+3)*(w^2+1)=10-u^2'),
    ('relation_subtracted_shift', 'rational_relation', '(3-v)*(w^2+1)+u^2=10',
     '(3-v)*(w^2+1)=10-u^2'),
    ('relation_negative_shift', 'rational_relation', 'u^2+(v-3)*(w^2+1)=10',
     '(v-3)*(w^2+1)=10-u^2'),
    ('relation_ellipse_fractions', 'rational_relation', '(16*x^2+9*y^2)/144=1', 'x^2/9+y^2/16=1'),
    ('relation_circle_reversed', 'rational_relation', '1=(4949*y^2+4949*z^2+3025)/2371600',
     '2368575/4949=y^2+z^2'),
    ('relation_integer_content', 'rational_relation', '4*y^2+8*z^2+3=23', 'y^2+2*z^2=5'),
    ('relation_equal_rationals', 'rational_relation', '2*y/3+2*z/3+1/7=5', 'y+z=51/7'),
    ('relation_unequal_rationals', 'rational_relation', 'y/3+z/5+1=2', 'y/3+z/5=1'),
    ('relation_exact_fraction_offset', 'rational_relation', 'x^2+y^2+1/9=4', 'x^2+y^2=35/9'),
    ('relation_trigonometric_terms', 'rational_relation', '4*sin(t)+4*cos(t)+1=7', 'sin(t)+cos(t)=3/2'),
    ('relation_symbolic_divisor_control', 'rational_relation', '(2*y+2*z+1)/a=3', '(2*y+2*z+1)/a=3'),
    ('relation_numeric_truth_control', 'rational_relation', '4=5', '4=5'),
    ('relation_named_constant_control', 'rational_relation', '{2*x+1=a | x=?; a=15}', '{2*x+1=a | x=?; a=15}'),
    ('relation_symbolic_sides_control', 'rational_relation', 'y=2*x+3', 'y=2*x+3')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.algebraic_cubics', 'tests/equation/test_equation.c',
    'tests.test_equation_algebraic_cubics', 'equation_cases', 'algebraic_cubic')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('algebraic_cubic_three_real', 'algebraic_cubic', 'x^3-3*sqrt(3)*x^2-3*x+sqrt(3)=0',
     '{"count":3,"real":3,"trigonometric":true,"centre":"sqrt(3)","compact_limit":600,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 70),
    ('algebraic_cubic_three_real_high_precision', 'algebraic_cubic', 'x^3-3*sqrt(3)*x^2-3*x+sqrt(3)=0',
     '{"count":3,"real":3,"trigonometric":true,"centre":"sqrt(3)","compact_limit":600,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 309),
    ('algebraic_cubic_scaled', 'algebraic_cubic', '-2*x^3+6*sqrt(3)*x^2+6*x-2*sqrt(3)=0',
     '{"count":3,"real":3,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 70),
    ('algebraic_cubic_radical_leading', 'algebraic_cubic', 'sqrt(2)*x^3-3*sqrt(6)*x^2-3*sqrt(2)*x+sqrt(6)=0',
     '{"count":3,"real":3,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 70),
    ('algebraic_cubic_fractional_scale', 'algebraic_cubic', 'x^3/7-3*sqrt(3)*x^2/7-3*x/7+sqrt(3)/7=0',
     '{"count":3,"real":3,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 70),
    ('algebraic_cubic_bound_coefficient', 'algebraic_cubic', '{x^3-3*a*x^2-3*x+a=0 | x=?; a=sqrt(3)}',
     '{"count":3,"real":3,"roots":["-tan(2*pi/9)","tan(pi/9)","tan(4*pi/9)"]}', 70),
    ('algebraic_cubic_complex_pair', 'algebraic_cubic', 'x^3+sqrt(2)*x+sqrt(5)=0',
     '{"count":3,"real":1}', 70),
    ('algebraic_cubic_negative_cardano_argument', 'algebraic_cubic', 'x^3-sqrt(2)*x+sqrt(5)=0',
     '{"count":3,"real":1}', 70),
    ('algebraic_cubic_repeated', 'algebraic_cubic', '(x-sqrt(2))^2*(x+2*sqrt(2))=0',
     '{"count":2,"real":2,"roots":["sqrt(2)","-2*sqrt(2)"]}', 70),
    ('algebraic_cubic_triple', 'algebraic_cubic', '(x-sqrt(3))^3=0',
     '{"count":1,"real":1,"roots":["sqrt(3)"]}', 70),
    ('algebraic_cubic_angle_negative', 'algebraic_cubic', 'x^3-3*x+sqrt(3)=0',
     '{"count":3,"real":3,"trigonometric":true}', 70),
    ('algebraic_cubic_angle_rational', 'algebraic_cubic', 'x^3-3*x-1=0',
     '{"count":3,"real":3,"trigonometric":true}', 70),
    ('algebraic_cubic_angle_shifted', 'algebraic_cubic', '(x-2)^3-3*(x-2)-1=0',
     '{"count":3,"real":3,"trigonometric":true,"centre":"2"}', 70),
    ('algebraic_cubic_angle_constructible', 'algebraic_cubic', 'x^3-3*x-sqrt(2)=0',
     '{"count":3,"real":3}', 70),
    ('algebraic_cubic_angle_unsupported', 'algebraic_cubic', 'x^3-3*x+1/5=0',
     '{"count":3,"real":3,"trigonometric":false}', 70),
    ('algebraic_cubic_angle_nearby', 'algebraic_cubic', 'x^3-3*x-sqrt(3)+1/10^120=0',
     '{"count":3,"real":3,"trigonometric":false,"numerical_fallback":true}', 70)
ON CONFLICT(name) DO UPDATE SET operation=excluded.operation, input=excluded.input,
    expected=excluded.expected, precision=excluded.precision;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.even_quadratic', 'tests/equation/test_equation.c',
    'tests.test_equation_even_quadratic', 'equation_cases', 'even_quadratic')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('even_quadratic_tapered', 'even_quadratic', 'x^2/784+(y^2+z^2)/484*(1+0.01*x)=1',
     '{"coordinate":"y","samples":[{"x":"10","z":"3"},{"x":"-101","z":"0"},{"x":"1","z":"100"}]}', 64),
    ('even_quadratic_complex_coefficient', 'even_quadratic', '(a+1)*y^2+b=0',
     '{"coordinate":"y","samples":[{"a":"2","b":"-12"},{"a":"-3","b":"8"},{"a":"i","b":"2"}]}', 64);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.solution_pairs', 'tests/equation/test_equation.c',
    'tests.test_equation_solution_pairs', 'equation_cases', 'solution_pair')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES
    ('solution_pair_scaled_ellipsoid', 'solution_pair', 'x^2/9+y^2/16+z^2/25=1', '3'),
    ('solution_pair_scaled_closed_root', 'solution_pair', 'x^2/9=2', '1'),
    ('solution_pair_shifted_ellipse', 'solution_pair', '(x-2)^2/9+y^2/16=1', '2'),
    ('solution_pair_sphere', 'solution_pair', 'x^2+y^2+z^2=1', '3'),
    ('solution_pair_exact_roots', 'solution_pair', 'x^2=4', '1'),
    ('solution_pair_different_roots', 'solution_pair', '(x-2)*(x-3)=0', '0'),
    ('solution_pair_different_coordinates', 'solution_pair', 'x+y=0', '0');

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

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'equation.guarded_inverse', 'tests/equation/test_equation.c',
    'tests.test_equation_guarded_inverse', 'equation_cases', 'guarded_inverse')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('guarded_inverse_integral_constants', 'guarded_inverse',
     'x=-(2*atanh(sqrt(C+2*exp(y))/sqrt(C)))/sqrt(C)+D',
     '{"coordinate":"y","constants":["C","D"],"formula":"ln(-C/(2*cosh(sqrt(C)*(x-D)/2)^2))","samples":[{"bindings":{"C":"-1","D":"1","x":"1"},"valid":true},{"bindings":{"C":"-1","D":"1","x":"2"},"valid":true},{"bindings":{"C":"-1","D":"1","x":"0"},"valid":false},{"bindings":{"C":"-1","D":"1","x":"5"},"valid":false},{"bindings":{"C":"0","D":"1","x":"2"},"valid":false}]}', 64),
    ('guarded_inverse_renamed_scaled', 'guarded_inverse',
     'p=-(3*atanh(sqrt(a+5*exp(q))/sqrt(a)))/sqrt(a)+b',
     '{"coordinate":"q","constants":["a","b"],"formula":"ln(a*(tanh(sqrt(a)*(p-b)/3)^2-1)/5)","samples":[{"bindings":{"a":"-4","b":"2","p":"2"},"valid":true},{"bindings":{"a":"-4","b":"2","p":"3"},"valid":true},{"bindings":{"a":"-4","b":"2","p":"1"},"valid":false},{"bindings":{"a":"-4","b":"2","p":"8"},"valid":false}]}', 64),
    ('guarded_inverse_atanh', 'guarded_inverse', 'atanh(y)=x',
     '{"coordinate":"y","formula":"tanh(x)","samples":[{"bindings":{"x":"-2"},"valid":true},{"bindings":{"x":"2"},"valid":true},{"bindings":{"x":"1+i/2"},"valid":true},{"bindings":{"x":"3*i"},"valid":false}]}', 64),
    ('guarded_inverse_root', 'guarded_inverse', 'sqrt(y)=x',
     '{"coordinate":"y","formula":"x^2","samples":[{"bindings":{"x":"3"},"valid":true},{"bindings":{"x":"0"},"valid":true},{"bindings":{"x":"i"},"valid":true},{"bindings":{"x":"-3"},"valid":false},{"bindings":{"x":"-i"},"valid":false}]}', 64),
    ('guarded_inverse_shifted_root', 'guarded_inverse', 'sqrt(y)+2=x',
     '{"coordinate":"y","formula":"(x-2)^2","samples":[{"bindings":{"x":"3"},"valid":true},{"bindings":{"x":"2"},"valid":true},{"bindings":{"x":"2+i"},"valid":true},{"bindings":{"x":"1"},"valid":false},{"bindings":{"x":"2-i"},"valid":false}]}', 64),
    ('guarded_inverse_log_affine', 'guarded_inverse', 'a+2*ln(y)=x',
     '{"coordinate":"y","constants":["a"],"formula":"exp((x-a)/2)","samples":[{"bindings":{"a":"1","x":"3"},"valid":true},{"bindings":{"a":"1","x":"-1"},"valid":true},{"bindings":{"a":"1","x":"-8*i"},"valid":false}]}', 64),
    ('guarded_inverse_exp_affine', 'guarded_inverse', 'a+2*exp(y)=x',
     '{"coordinate":"y","constants":["a"],"formula":"ln((x-a)/2)","samples":[{"bindings":{"a":"1","x":"3"},"valid":true},{"bindings":{"a":"1","x":"-1"},"valid":true},{"bindings":{"a":"1","x":"1"},"valid":false}]}', 64),
    ('guarded_inverse_denominator', 'guarded_inverse', 'x=atanh(y)/a',
     '{"coordinate":"y","constants":["a"],"formula":"tanh(a*x)","samples":[{"bindings":{"a":"2","x":"1"},"valid":true},{"bindings":{"a":"-2","x":"1"},"valid":true},{"bindings":{"a":"0","x":"1"},"valid":false}]}', 64);

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES
    ('inverse_atanh_pole', 'test_equation_preserves_principal_root_domain_sources', 'atanh(y)=i*pi/2', '');

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('inverse_atanh_pole_after_precision_change', 'test_equation_preserves_principal_root_domain_sources',
     'atanh(y)=i*pi/2', '', 309);

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('guarded_inverse_upper_boundary', 'guarded_inverse', 'atanh(y)=1+i*pi/2',
     '{"coordinate":"y","formula":"coth(1)","samples":[{"bindings":{},"valid":true}]}', 64),
    ('guarded_inverse_upper_negative_boundary', 'guarded_inverse', 'atanh(y)=-1+i*pi/2',
     '{"coordinate":"y","formula":"-coth(1)","samples":[{"bindings":{},"valid":true}]}', 64),
    ('guarded_inverse_indexed_constants', 'guarded_inverse',
     'x=-(2*atanh(sqrt(C1+2*exp(y))/sqrt(C1)))/sqrt(C1)+D1',
     '{"coordinate":"y","constants":["C₁","D₁"],"formula":"ln(-C1/(2*cosh(sqrt(C1)*(x-D1)/2)^2))","samples":[{"bindings":{"C₁":"-4","D₁":"2","x":"3"},"valid":true}]}', 64);

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected) VALUES
    ('inverse_atanh_lower_cut', 'test_equation_preserves_principal_root_domain_sources', 'atanh(y)=1-i*pi/2', '');

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('guarded_inverse_euler_power', 'guarded_inverse',
     'x=-(2*atanh(sqrt(C+2*e^y)/sqrt(C)))/sqrt(C)+D',
     '{"coordinate":"y","constants":["C","D"],"formula":"ln(-C/(2*cosh(sqrt(C)*(x-D)/2)^2))","samples":[{"bindings":{"C":"-1","D":"1","x":"1"},"valid":true},{"bindings":{"C":"-1","D":"1","x":"2"},"valid":true},{"bindings":{"C":"-1","D":"1","x":"0"},"valid":false}]}', 64),
    ('guarded_inverse_complex_parameters', 'guarded_inverse',
     'x=-(2*atanh(sqrt(C+2*exp(y))/sqrt(C)))/sqrt(C)+D',
     '{"coordinate":"y","constants":["C","D"],"formula":"ln(-C/(2*cosh(sqrt(C)*(x-D)/2)^2))","samples":[{"bindings":{"C":"4","D":"0","x":"-1"},"valid":true},{"bindings":{"C":"4","D":"0","x":"0"},"valid":true},{"bindings":{"C":"4","D":"0","x":"1"},"valid":false},{"bindings":{"C":"1+i","D":"2-i","x":"2-i-2*(1+i/4)/sqrt(1+i)"},"valid":true},{"bindings":{"C":"1+i","D":"2-i","x":"2-i"},"valid":true},{"bindings":{"C":"1+i","D":"2-i","x":"2-i-2*(1+2*i)/sqrt(1+i)"},"valid":false}]}', 64),
    ('guarded_inverse_definition_collision', 'guarded_inverse',
     't=-(2*atanh(sqrt(C+2*exp(u))/sqrt(C)))/sqrt(C)+D',
     '{"coordinate":"u","constants":["C","D"],"formula":"ln(-C/(2*cosh(sqrt(C)*(t-D)/2)^2))","display_fragments":["v &=","w &=","\\operatorname{arg}\\left(w\\right)"],"samples":[{"bindings":{"C":"-1","D":"1","t":"2"},"valid":true},{"bindings":{"C":"-1","D":"1","t":"0"},"valid":false}]}', 64),
    ('guarded_root_polynomial_branches', 'solve_count', '{sqrt(y^2)=a | y=?; a=?}', '2', 64);

INSERT OR IGNORE INTO equation_cases(name, operation, input, expected, precision) VALUES
    ('complex_power_guarded_parameter_collision', 'complex_power_family',
     '{n*sqrt(i^x)/n=2 | x=?; n=3}', 'x=4*m-2*i*ln(4)/pi', 78);
