-- expression regression inputs and expectations. Checking logic remains in the module's C tests.
CREATE TABLE IF NOT EXISTS expression_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS expression_cases_operation ON expression_cases(operation, name);

-- Mathematical letter case is significant in the canonical special-function names.
INSERT INTO expression_cases(name, operation, input, expected) VALUES
    ('named_appell_F1_direct', 'numeric_callable', 'appell_F1(1,1,1,2,0,0)', '1'),
    ('named_bessel_I_direct', 'numeric_callable', 'bessel_I(0,0)', '1'),
    ('named_bessel_J_direct', 'numeric_callable', 'bessel_J(0,0)', '1'),
    ('named_bessel_K_direct', 'numeric_callable', 'bessel_K(0,1)', 'besselk(0,1)'),
    ('named_bessel_Y_direct', 'numeric_callable', 'bessel_Y(0,1)', 'bessely(0,1)'),
    ('named_lambert_W0_direct', 'numeric_callable', 'lambert_W0(0)', '0'),
    ('named_lambert_Wm1_direct', 'numeric_callable', 'lambert_Wm1(-0.2)', 'lambertwm1(-0.2)'),
    ('named_lambert_Wn_direct', 'numeric_callable', 'lambert_Wn(0,0.7)', 'lambertwn(0,0.7)'),
    ('named_lauricella_F_direct', 'numeric_callable', 'lauricella_F(1,1,1,2,0)', '1'),
    ('named_struve_H_direct', 'numeric_callable', 'struve_H(0,0)', '0'),
    ('named_struve_L_direct', 'numeric_callable', 'struve_L(0,0)', '0'),
    ('named_lommel_S_direct', 'numeric_callable', 'lommel_S(1,0,1)', 'lommels(1,0,1)'),
    ('named_appell_F1_binding', 'numeric_callable', '{x|x=appell_F1(1,1,1,2,0,0)}', '1'),
    ('named_bessel_I_binding', 'numeric_callable', '{x|x=bessel_I(0,0)}', '1'),
    ('named_bessel_J_binding', 'numeric_callable', '{x|x=bessel_J(0,0)}', '1'),
    ('named_bessel_K_binding', 'numeric_callable', '{x|x=bessel_K(0,1)}', 'besselk(0,1)'),
    ('named_bessel_Y_binding', 'numeric_callable', '{x|x=bessel_Y(0,1)}', 'bessely(0,1)'),
    ('named_lambert_W0_binding', 'numeric_callable', '{x|x=lambert_W0(0)}', '0'),
    ('named_lambert_Wm1_binding', 'numeric_callable', '{x|x=lambert_Wm1(-0.2)}', 'lambertwm1(-0.2)'),
    ('named_lambert_Wn_binding', 'numeric_callable', '{x|x=lambert_Wn(0,0.7)}', 'lambertwn(0,0.7)'),
    ('named_lauricella_F_binding', 'numeric_callable', '{x|x=lauricella_F(1,1,1,2,0)}', '1'),
    ('named_struve_H_binding', 'numeric_callable', '{x|x=struve_H(0,0)}', '0'),
    ('named_struve_L_binding', 'numeric_callable', '{x|x=struve_L(0,0)}', '0'),
    ('named_lommel_S_binding', 'numeric_callable', '{x|x=lommel_S(1,0,1)}', 'lommels(1,0,1)')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;

-- Canonical polynomial names work in expressions and in binding values, with the same real/complex conventions.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('poly_chebyshev_Tn_direct', 'numeric_callable', 'chebyshev_Tn(3,2)', '26'),
    ('poly_chebyshev_Un_direct', 'numeric_callable', 'chebyshev_Un(3,2)', '56'),
    ('poly_hermite_Hn_direct', 'numeric_callable', 'hermite_Hn(3,2)', '40'),
    ('poly_chebyshev_Tn_binding', 'numeric_callable', '{x|x=chebyshev_Tn(3,2)}', '26'),
    ('poly_chebyshev_Un_binding', 'numeric_callable', '{x|x=chebyshev_Un(3,2)}', '56'),
    ('poly_hermite_Hn_binding', 'numeric_callable', '{x|x=hermite_Hn(3,2)}', '40'),
    ('poly_chebyshev_Tn_complex', 'numeric_callable', 'chebyshev_Tn(3,i)', '-7i'),
    ('poly_chebyshev_Un_complex', 'numeric_callable', 'chebyshev_Un(3,i)', '-12i'),
    ('poly_hermite_Hn_complex', 'numeric_callable', 'hermite_Hn(3,i)', '-20i');

-- Numeric callable coverage includes exact-width operands, aliases and preserved binding function calls.
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.integral_exp_root_primitive', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'integral_exp_root_primitive')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('exp_root_primitive_symbolic_positive', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+C)|y=1;C=1}', 'formal'),
    ('exp_root_primitive_symbolic_negative', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+C)|y=1;C=-1}', 'formal'),
    ('exp_root_primitive_positive', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)+1)|y=1}', 'derivative'),
    ('exp_root_primitive_parameter_rate', 'integral_exp_root_primitive', '{1/sqrt(2exp(b*y)+1)|y=1;b=2}', 'formal'),
    ('exp_root_primitive_zero', 'integral_exp_root_primitive', '{1/sqrt(2exp(y))|y=1}', 'derivative'),
    ('exp_root_primitive_negative_literal', 'integral_exp_root_primitive', '{1/sqrt(2exp(y)-1)|y=1}', 'derivative'),
    ('exp_root_primitive_scaled_rate', 'integral_exp_root_primitive', '{3/sqrt(4exp(2y+1)+2)|y=1}', 'derivative')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT INTO expression_cases(name, operation, input, expected, precision) VALUES
    ('exp_root_small_positive_tail', 'integral_exp_root', '{ @S 1/sqrt(A*exp(y)+C) dy | y=1; A=1e80; C=1 }',
     '2*(atanh(1/sqrt(1e80+1))-atanh(1/sqrt(1e80*exp(1)+1)))', 53),
    ('exp_root_small_negative_tail', 'integral_exp_root', '{ @S 1/sqrt(A*exp(y)+C) dy | y=1; A=1e80; C=-1 }',
     '2*(atan(1/sqrt(1e80-1))-atan(1/sqrt(1e80*exp(1)-1)))', 53)
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected, precision=excluded.precision;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.integral_exp_root', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'integral_exp_root')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('exp_root_zero_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=0 }',
     'sqrt(2)*(1-exp(-1/2))'),
    ('exp_root_positive_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=1 }',
     'ln((sqrt(2*exp(1)+1)-1)/(sqrt(2*exp(1)+1)+1))-ln((sqrt(3)-1)/(sqrt(3)+1))'),
    ('exp_root_negative_offset', 'integral_exp_root', '{ @S 1/sqrt(2e^y+C) dy | y=1; C=-1 }',
     '2*(atan(sqrt(2*exp(1)-1))-atan(1))'),
    ('exp_root_reversed_interval', 'integral_exp_root', '@S_1^0 1/sqrt(2e^y+1) dy',
     'ln((sqrt(3)-1)/(sqrt(3)+1))-ln((sqrt(2*exp(1)+1)-1)/(sqrt(2*exp(1)+1)+1))'),
    ('exp_root_shifted_scaled', 'integral_exp_root', '@S_0^1 3/sqrt(4exp(2y+1)+2) dy',
     '3/sqrt(2)*(atanh(sqrt(2)/sqrt(4*exp(1)+2))-atanh(sqrt(2)/sqrt(4exp(3)+2)))'),
    ('exp_root_constant_base', 'integral_exp_root', '@S_0^1 1/sqrt(3*2^y) dy',
     '2/(sqrt(3)*ln(2))*(1-1/sqrt(2))'),
    ('exp_root_negative_amplitude', 'integral_exp_root', '@S_0^1 1/sqrt(10-exp(y)) dy',
     '(ln((sqrt(10)-sqrt(10-exp(1)))/(sqrt(10)+sqrt(10-exp(1))))-ln((sqrt(10)-3)/(sqrt(10)+3)))/sqrt(10)'),
    ('exp_root_negative_rate', 'integral_exp_root', '@S_0^1 1/sqrt(2exp(-y)) dy',
     'sqrt(2)*(exp(1/2)-1)'),
    ('exp_root_integrable_endpoint', 'integral_exp_root', '@S_0^1 1/sqrt(2exp(y)-2) dy',
     'sqrt(2)*atan(sqrt(exp(1)-1))'),
    ('exp_root_improper_zero_offset', 'integral_exp_root', '@S_0^inf 1/sqrt(2exp(y)) dy', 'sqrt(2)'),
    ('exp_root_improper_negative_offset', 'integral_exp_root', '@S_0^inf 1/sqrt(2exp(y)-1) dy', '@pi/2')
ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
-- This operation stores working bits; the small-tail cases deliberately exercise double precision.
UPDATE expression_cases SET precision=256 WHERE operation='integral_exp_root' AND precision<>53;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.numeric_callables', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'numeric_callable')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.numeric_rendering', 'tests/expression/test_expr_numeric.c', 'tests.test_expr_numeric_callables',
    'expression_cases', 'numeric_render')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('powmod_small', 'numeric_callable', 'powmod(4,13,497)', '445'),
    ('powmod_constant_integral', 'numeric_callable', '@S_0^1 powmod(2,3,5) dx', '3'),
    ('powmod_constant_derivative', 'numeric_callable', 'd/dx(powmod(2,3,5)*x)', '3'),
    ('gcd_constant_derivative', 'numeric_callable', 'd/dx(gcd(12,18)*x)', '6'),
    ('powmod_original_1024_bit_operands', 'numeric_callable', 'powmod(97128534091520311412078115857277804694847051212062325796074342369690095336188259720191897149915705664060543226969142780725822337496727884659619483452400511633617514882293623272276764857474830401626415841035971458657442133370777607803464071101158385124186229995073045427448918227959893082282919863982307657776,12498846048367045926370716602609982839721641429222959125376462582674869403443996972648628699047686839582654905384579341977328653591547736951147466435189662,6864797660130609714981900799081393217269435300143305409394463459185543183397656052122559640661454554977296311391480858037121987999716643812574028291115057151)', '2426708632600750408239409365812084203045954767825752904850432903124881973671736995843975761420957307038639263776889805180044277291952781094657794927246068639'),
    ('powmod_huge_exponent', 'numeric_callable', 'powmod(2,2^512,17)', '1'),
    ('powmod_negative_base', 'numeric_callable', 'powmod(-2,5,13)', '7'),
    ('powmod_zero_exponent', 'numeric_callable', 'powmod(9,0,7)', '1'),
    ('powmod_one_modulus', 'numeric_callable', 'powmod(9,0,1)', '0'),
    ('powmod_zero_modulus', 'numeric_callable', 'powmod(2,3,0)', 'NAN'),
    ('powmod_negative_exponent', 'numeric_callable', 'powmod(2,-1,7)', 'NAN'),
    ('powmod_fraction', 'numeric_callable', 'powmod(2,1/2,7)', 'NAN'),
    ('integer_bit_length', 'numeric_callable', 'bit_length(2^1024)', '1025'),
    ('integer_bit_length_zero', 'numeric_callable', 'bit_length(0)', '0'),
    ('integer_bit_length_negative', 'numeric_callable', 'bit_length(-8)', '4'),
    ('integer_bit_length_fraction', 'numeric_callable', 'bit_length(1/2)', 'NAN'),
    ('integer_set_bit', 'numeric_callable', 'set_bit(0,701)', '2^701'),
    ('integer_clear_bit', 'numeric_callable', 'clear_bit(2^701+5,701)', '5'),
    ('integer_test_bit', 'numeric_callable', 'test_bit(2^701,701)', '1'),
    ('integer_test_bit_absent', 'numeric_callable', 'test_bit(2^701,700)', '0'),
    ('integer_negative_index', 'numeric_callable', 'set_bit(0,-1)', 'NAN'),
    ('integer_fractional_index', 'numeric_callable', 'clear_bit(0,1/2)', 'NAN'),
    ('integer_index_overflow', 'numeric_callable', 'test_bit(0,2^128)', 'NAN'),
    ('integer_prove_prime', 'numeric_callable', 'prove_prime(97)', '1'),
    ('integer_prove_composite', 'numeric_callable', 'prove_prime(91)', '0'),
    ('integer_divmod_quotient', 'numeric_callable', 'divmod(17,5,0)', '3'),
    ('integer_divmod_remainder', 'numeric_callable', 'divmod(17,5,1)', '2'),
    ('integer_divmod_negative', 'numeric_callable', 'divmod(-17,5,0)', '-3'),
    ('integer_divmod_negative_remainder', 'numeric_callable', 'divmod(-17,5,1)', '-2'),
    ('integer_divmod_zero', 'numeric_callable', 'divmod(17,0,0)', 'NAN'),
    ('integer_divmod_selector', 'numeric_callable', 'divmod(17,5,2)', 'NAN'),
    ('integer_gcdext_gcd', 'numeric_callable', 'gcdext(30,21,0)', '3'),
    ('integer_gcdext_identity', 'numeric_callable', '30*gcdext(30,21,1)+21*gcdext(30,21,2)', '3'),
    ('integer_gcdext_selector', 'numeric_callable', 'gcdext(30,21,3)', 'NAN'),
    ('alias_bit_and', 'numeric_callable', 'bit_and(12,10)', '8'),
    ('alias_bit_or', 'numeric_callable', 'bit_or(12,10)', '14'),
    ('alias_bit_xor', 'numeric_callable', 'bit_xor(12,10)', '6'),
    ('alias_bit_not', 'numeric_callable', 'bit_not(12)', 'not(12)'),
    ('alias_arithmetic_add', 'numeric_callable', 'add(2,3)', '5'),
    ('alias_arithmetic_sub', 'numeric_callable', 'sub(2,3)', '-1'),
    ('alias_arithmetic_mul', 'numeric_callable', 'mul(2,3)', '6'),
    ('alias_arithmetic_div', 'numeric_callable', 'div(2,3)', '2/3'),
    ('alias_arithmetic_neg', 'numeric_callable', 'neg(2)', '-2'),
    ('alias_add_long', 'numeric_callable', 'add_long(2,3)', '5'),
    ('alias_mul_long', 'numeric_callable', 'mul_long(2,3)', '6'),
    ('alias_pow_int', 'numeric_callable', 'pow_int(2,13)', '8192'),
    ('alias_inverse', 'numeric_callable', 'inv(2)', '1/2'),
    ('alias_square', 'numeric_callable', 'sqr(13)', '169'),
    ('alias_pow10', 'numeric_callable', 'pow10(3)', '1000'),
    ('alias_ldexp', 'numeric_callable', 'ldexp(3,5)', '96'),
    ('alias_mul_pow10', 'numeric_callable', 'mul_pow10(3,-2)', '3/100'),
    ('alias_arg', 'numeric_callable', 'arg(1+i)', '@pi/4'),
    ('alias_polylog1', 'numeric_callable', 'polylog1(0)', '0'),
    ('alias_tetragamma', 'numeric_callable', 'tetragamma(1)', 'polygamma(2,1)'),
    ('alias_sincos_sine', 'numeric_callable', 'sincos(0,0)', '0'),
    ('alias_sincos_cosine', 'numeric_callable', 'sincos(0,1)', '1'),
    ('alias_sinhcosh_sinh', 'numeric_callable', 'sinhcosh(0,0)', '0'),
    ('alias_sinhcosh_cosh', 'numeric_callable', 'sinhcosh(0,1)', '1'),
    ('clausen_explicit_order', 'numeric_callable', 'clausen(1,1)', 'Cl(1,1)'),
    ('clausen_legacy_one', 'numeric_callable', 'clausen(1)', 'clausen2(1)'),
    ('powmod_binding', 'numeric_callable', '{x | x=powmod(4,13,497)}', '445'),
    ('bit_length_binding', 'numeric_callable', '{x | x=bit_length(2^1024)}', '1025'),
    ('alias_binding_inverse', 'numeric_callable', '{x | x=inv(2)}', '1/2'),
    ('alias_binding_pow10', 'numeric_callable', '{x | x=pow10(3)}', '1000'),
    ('alias_binding_sinhcosh', 'numeric_callable', '{x | x=sinhcosh(0,1)}', '1'),
    ('clausen_binding_order', 'numeric_callable', '{x | x=clausen(1,1)}', 'Cl(1,1)'),
    ('powmod_finite_sum', 'numeric_callable', '@Z_(k=1)^3 powmod(2,k,7)', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('symbolic_powmod', 'numeric_render', '{powmod(a,b,c) | a=4,b=13,c=497}', 'powmod('),
    ('symbolic_bit_edit', 'numeric_render', '{set_bit(a,b) | a=0,b=701}', 'set_bit('),
    ('symbolic_divmod', 'numeric_render', '{divmod(a,b,c) | a=17,b=5,c=1}', 'divmod('),
    ('symbolic_gcdext', 'numeric_render', '{gcdext(a,b,c) | a=30,b=21,c=2}', 'gcdext('),
    ('symbolic_binding_powmod', 'numeric_render', '{x | x=powmod(4,13,497)}', 'powmod(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.real_sample', 'tests/expression/test_expression.c', 'tests.test_expr_real_sample',
    'expression_cases', 'real_sample')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES
    ('sample_nested_acos_root_power', 'real_sample', '{acos(sin(2-x*y)-10^(-root(x^2+y^2,5))) | x=0.37,y=-0.29}', 'accept'),
    ('sample_positive_variable_power', 'real_sample', '{x^y | x=0.37,y=-0.29}', 'accept'),
    ('sample_positive_integer_power', 'real_sample', '{x^3 | x=0.37}', 'accept'),
    ('sample_positive_fractional_power', 'real_sample', '{x^(1/5) | x=0.37}', 'accept'),
    ('sample_positive_negative_power', 'real_sample', '{x^(-3) | x=0.37}', 'accept'),
    ('sample_positive_fifth_root', 'real_sample', '{root(x,5) | x=0.37}', 'accept'),
    ('sample_positive_square_root', 'real_sample', '{sqrt(x) | x=0.37}', 'accept'),
    ('sample_positive_cube_root', 'real_sample', '{cubrt(x) | x=0.37}', 'accept'),
    ('sample_positive_large_root', 'real_sample', '{root(x,2147483647) | x=0.37}', 'accept'),
    ('sample_negative_integer_power_fallback', 'real_sample', '{x^3 | x=-0.37}', 'fallback'),
    ('sample_negative_principal_root_fallback', 'real_sample', '{root(x,5) | x=-0.37}', 'fallback'),
    ('sample_zero_principal_root_fallback', 'real_sample', '{root(x,5) | x=0}', 'fallback'),
    ('sample_negative_cube_root_fallback', 'real_sample', '{cubrt(x) | x=-0.37}', 'fallback'),
    ('sample_variable_root_order_fallback', 'real_sample', '{root(x,y) | x=0.37,y=5}', 'fallback'),
    ('sample_noninteger_root_order_fallback', 'real_sample', '{root(x,2.5) | x=0.37}', 'fallback'),
    ('sample_unsupported_root_order_fallback', 'real_sample', '{root(x,2147483648) | x=0.37}', 'fallback'),
    ('sample_outside_acos_domain_fallback', 'real_sample', '{acos(1+x) | x=0.37}', 'fallback'),
    ('sample_root_cancellation_fallback', 'real_sample', '{sqrt(x)-sqrt(x+1e-30) | x=1}', 'fallback'),
    ('sample_power_overflow_fallback', 'real_sample', '{10^x | x=400}', 'fallback'),
    ('sample_power_underflow_fallback', 'real_sample', '{10^x | x=-400}', 'fallback');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('exact_addition', 'evaluate', '1/3+1/6', '1/2');

INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('trigonometric_identity', 'evaluate', 'sin(0)^2+cos(0)^2', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression', 'tests/expression/test_expression.c', 'tests.test_expression_database_cases', 'expression_cases', 'evaluate')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_prime_shorthand_cases', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace_prime_shorthand', 'expression_cases', 'test_from_string_laplace_prime_shorthand_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_from_string.c: test_from_string_laplace_prime_shorthand. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_561b746a48f13517', 'test_from_string_laplace_prime_shorthand_cases', '@L{y''''+4y''+5y-50t}', '@L{y''''(t)+4*y''(t)+5*y(t)-50*t}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f9d738f8ac1f9c71', 'test_from_string_laplace_prime_shorthand_cases', '@L{5y+4y''+y''''-50t}', '@L{5*y(t)+4*y''(t)+y''''(t)-50*t}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6349cf811ab3d0ce', 'test_from_string_laplace_prime_shorthand_cases', '@L(y''''+a*y''+b*y,x,p)', '@L(y''''(x)+a*y''(x)+b*y(x),x,p)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f08e164be0389ec3', 'test_from_string_laplace_prime_shorthand_cases', '@L(y''+@L(z'',x,p),t,s)', '@L(y''(t)+@L(z''(x),x,p),t,s)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_58fd07c0c4e77e22', 'test_from_string_laplace_prime_shorthand_cases', 'y+@L{y''+y}', 'y+@L{y''(t)+y(t)}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_88407bb6bd0a2270', 'test_from_string_laplace_prime_shorthand_cases', '@L{y''+y}+@L{y*t}', '@L{y''(t)+y(t)}+@L{y*t}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_function_uses_lowercase_builtin_names_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_function_uses_lowercase_builtin_names', 'expression_cases', 'test_to_string_function_uses_lowercase_builtin_names_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_function_uses_lowercase_builtin_names. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_38ab0e19563d3440', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Li1(x) }', 'return li1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8688c04bfc29ac4b', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Li2(x) }', 'return li2(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8a0d188016724f74', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Ei(x) }', 'return ei(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2b10a19dbcc207ac', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ E1(x) }', 'return e1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_40ba6c2f0ad7a5e8', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_pdf(x) }', 'return normalpdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f3806b70020cea63', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_cdf(x) }', 'return normalcdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a4478a4ebb0c37f2', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ normal_logpdf(x) }', 'return normallogpdf(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_de7346d5f7acee30', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_lower(a, x) }', 'return gammainclower(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2068cbbc69f78f65', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_upper(a, x) }', 'return gammaincupper(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2519facc25db032d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_P(a, x) }', 'return gammaincp(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_05e0f8e93c1c9002', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ gammainc_Q(a, x) }', 'return gammaincq(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2b6db995106e6eb4', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ is_prime(n) }', 'return isprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b00b436980d3690b', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ next_prime(n) }', 'return nextprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_53b743a27c19b5cb', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ prev_prime(n) }', 'return prevprime(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6b027871c72b98e', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ AND(a, b) }', 'return and(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2072076407ce127a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ OR(a, b) }', 'return or(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_06aa96995b412dc6', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ XOR(a, b) }', 'return xor(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_c5878f1926491b44', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ NOT(a) }', 'return not(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ae5d81ba7c1bd92a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ SHL(a, b) }', 'return shl(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_196bb95d68666078', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ SHR(a, b) }', 'return shr(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e59f3806830cc134', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselJ(n, x) }', 'return besselj(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dcec89f7b82b6bfd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselY(n, x) }', 'return bessely(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_597a802907b56437', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselK(n, x) }', 'return besselk(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_68c2a39d23dce7cd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ BesselI(n, x) }', 'return besseli(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_63be85e777c932ce', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ bessel_I(n, x) }', 'return besseli(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_fb16fa830d81068d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ besseli(n, x) }', 'return besseli(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dc094e68f032d2a5', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ StruveH(n, x) }', 'return struveh(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_8f7d671c94fb4584', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struve_H(n, x) }', 'return struveh(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6c862ab491091a65', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struveh(n, x) }', 'return struveh(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_be86657e82588c59', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ StruveL(n, x) }', 'return struvel(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_7bb6f7b390a07568', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struve_L(n, x) }', 'return struvel(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_49af4a2058eb6519', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ struvel(n, x) }', 'return struvel(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_be3027136363ae4a', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ harmonic_poly(n, x) }', 'return harmonicpoly(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5da5ce73765ed956', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ legendre_chi(s, x) }', 'return legendrechi(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_791f224d13ef44c7', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Cl2(x) }', 'return clausen2(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5f55276621be44cf', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Cl(n, x) }', 'return cl(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_866fc5d1549ff17c', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ LommelS(mu, nu, x) }', 'return lommels(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ee04f4be47578edd', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ LerchPhi(z, s, a) }', 'return lerchphi(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b011db8ab98c325e', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ qdigamma(q, z) }', 'return qdigamma(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_4a2b596e3b6a0d3d', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W(x) }', 'return w(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_da4989c5c3123c7f', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W0(x) }', 'return w0(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_736acd8aa9275507', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ W-1(x) }', 'return wm1(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f33d0d5e5bb9fc81', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ Wn(n, x) }', 'return wn(');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_9ca4f291410444e8', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ appell_F1(a, b1, b2, c, x, y) }', 'return appellf1(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('case_1d01b335bc0d20d5', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ lauricella_F(2, a, b1, b2, c, x, y) }', 'return lauricellaf(') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_db2f3559f8284cc0', 'test_to_string_function_uses_lowercase_builtin_names_cases', '{ hypergeometric_pFq(1, 1, a, b, x) }', 'return hypergeometricpfq(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_exact_rational_powers_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_exact_rational_powers', 'expression_cases', 'test_to_string_exact_rational_powers_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_exact_rational_powers. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9ee1785baa57e94b', 'test_to_string_exact_rational_powers_cases', '4^(3/2)', '8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9568ab84cd706d6c', 'test_to_string_exact_rational_powers_cases', '9^(3/2)', '27');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f1e4ec666181bd62', 'test_to_string_exact_rational_powers_cases', '36^(3/2)', '216');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3569ee3cd88d5c79', 'test_to_string_exact_rational_powers_cases', '9^(-3/2)', '1/27');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_bb230502d8394220', 'test_to_string_exact_rational_powers_cases', '(9/4)^(3/2)', '27/8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_680a73b835913657', 'test_to_string_exact_rational_powers_cases', '8^(2/3)', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a2c09251f5cf811a', 'test_to_string_exact_rational_powers_cases', '625^(3/4)', '125');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6798733feb9d266a', 'test_to_string_exact_rational_powers_cases', '(169/144)^(3/2)', '2197/1728');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d487135081b7333b', 'test_to_string_exact_rational_powers_cases', '2/5*(4*9^(3/2)+4^(3/2))', '232/5');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_indexed_principal_roots_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_indexed_principal_roots', 'expression_cases', 'test_to_string_indexed_principal_roots_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_indexed_principal_roots. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_11c85ba7bfc178f3', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x) | x=2}', '³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d8a6137b87f9c1b7', 'test_to_string_indexed_principal_roots_cases', '{root(x,4) | x=2}', '⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_7596dc7aa7bc4750', 'test_to_string_indexed_principal_roots_cases', '{root(x,12) | x=2}', '¹²√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9601067f63f23cab', 'test_to_string_indexed_principal_roots_cases', '{²√(x) | x=2}', '√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_778854900b7ee3af', 'test_to_string_indexed_principal_roots_cases', '{root(x,2147483647) | x=2}', '²¹⁴⁷⁴⁸³⁶⁴⁷√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9ae0f61816919cca', 'test_to_string_indexed_principal_roots_cases', '{³√(⁴√(x)) | x=2}', '³√(⁴√x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3f7485b871d25c07', 'test_to_string_indexed_principal_roots_cases', '{root(x,n) | x=2; n=4}', 'root(x, n)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_12a856cf69c1e5a7', 'test_to_string_indexed_principal_roots_cases', '{2*cubrt(x) | x=2}', '2·³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_57e01464a62575fb', 'test_to_string_indexed_principal_roots_cases', '{2*root(x,4) | x=2}', '2·⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0176a6999476c923', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x+1) | x=2}', '³√(x + 1)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_185519a361780104', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x^2) | x=-2}', '³√(x²)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_40b73aa342826a63', 'test_to_string_indexed_principal_roots_cases', '{³√x+1 | x=2}', '³√x + 1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_12248668e1407dc4', 'test_to_string_indexed_principal_roots_cases', '{³√x*y | x=2; y=3}', 'y·³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_51ae9caffd1cf271', 'test_to_string_indexed_principal_roots_cases', '{cubrt([long name]) | [long name]=2}', '³√([long name])');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_19e801b074628419', 'test_to_string_indexed_principal_roots_cases', '{cubrt(@alpha) | @alpha=2}', '³√α');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3df828c80dbe6f63', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x_1) | x_1=2}', '³√(x₁)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_493898b430f98d2e', 'test_to_string_indexed_principal_roots_cases', '{x*cubrt(2) | x=3}', '³√(2)·x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_110db59658fd9fce', 'test_to_string_indexed_principal_roots_cases', '{x*root(2,4) | x=3}', '⁴√(2)·x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_1f3fc9d95c9119af', 'test_to_string_indexed_principal_roots_cases', '{cubrt(x)^2 | x=2}', '³√(x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_aabaea26f5cdd40c', 'test_to_string_indexed_principal_roots_cases', '{1/root(x,12) | x=2}', '¹²√(x)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_9908d0812f6d0541', 'test_to_string_indexed_principal_roots_cases', '{³√(x) | x=-8}', '³√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a1c8c13e4da1a2ec', 'test_to_string_indexed_principal_roots_cases', '{⁴√(x) | x=-16}', '⁴√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e6bdd6dba307f2f3', 'test_to_string_indexed_principal_roots_cases', '{¹²√(x) | x=3+4i}', '¹²√x');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b7ef147a9af6feea', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√π}', '³√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d93025d630c408c4', 'test_to_string_indexed_principal_roots_cases', '{x | x=⁴√π}', '⁴√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a022397754447b75', 'test_to_string_indexed_principal_roots_cases', '{x | x=√π}', '√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_38d038f8cf8c73e2', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√π*2}', '³√π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3e896b6fa2e6f53d', 'test_to_string_indexed_principal_roots_cases', '{x | x=(⁴√π)^2}', '⁴√');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_13f7693660c784d7', 'test_to_string_indexed_principal_roots_cases', '{x | x=(³√π)^2}', '³√(π)²');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e5001a7f8d166954', 'test_to_string_indexed_principal_roots_cases', '{x | x=³√(2)}', '³√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_29c35527c089214a', 'test_to_string_indexed_principal_roots_cases', '{x | x=⁴√(2)}', '⁴√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0d0cd1ab9e0ab7dc', 'test_to_string_indexed_principal_roots_cases', '{x | x=2*³√(2)}', '·³√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_6f0a9bc6cdd9eaee', 'test_to_string_indexed_principal_roots_cases', '{x | x=2*¹²√(2)}', '·¹²√(2)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d3c50d3d523d3807', 'test_to_string_indexed_principal_roots_cases', '{x^(1/3) | x=-8}', '^⅓');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_complex_projection_TeX_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_complex_projection_TeX', 'expression_cases', 'test_to_string_complex_projection_TeX_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_complex_projection_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_49f6d920532ccfdf', 'test_to_string_complex_projection_TeX_cases', 'Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_47916602a946385f', 'test_to_string_complex_projection_TeX_cases', 'realpart(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_0238206a690af8ba', 'test_to_string_complex_projection_TeX_cases', 'real_part(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_175e420fcd9ee260', 'test_to_string_complex_projection_TeX_cases', 'ℜ(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_66e64dd2c44f6db6', 'test_to_string_complex_projection_TeX_cases', 'Im(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_2f4a512b378d6d31', 'test_to_string_complex_projection_TeX_cases', 'imagpart(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_93edd597041c00fc', 'test_to_string_complex_projection_TeX_cases', 'imag_part(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_f632b7e61d7d22c1', 'test_to_string_complex_projection_TeX_cases', 'ℑ(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_46b3f557656dd27d', 'test_to_string_complex_projection_TeX_cases', 'Re(x+y)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_193aa283bffb5e44', 'test_to_string_complex_projection_TeX_cases', 'Im(x+y)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6814a47ad2e49b3', 'test_to_string_complex_projection_TeX_cases', 'Re(z)^2', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3d0c2572bafd03fe', 'test_to_string_complex_projection_TeX_cases', 'Im(z)^2', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a3390875dd55dca9', 'test_to_string_complex_projection_TeX_cases', '8*Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ca938f9b4208396c', 'test_to_string_complex_projection_TeX_cases', '8*Im(z)', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_947dd4cf4f9d1a65', 'test_to_string_complex_projection_TeX_cases', '8*Re(z)^2', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5b8ee97b506c2a0c', 'test_to_string_complex_projection_TeX_cases', '8*Im(z)^2', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b80b9c47ae7d6475', 'test_to_string_complex_projection_TeX_cases', 'Re(z)*x', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3c0a6372ba227784', 'test_to_string_complex_projection_TeX_cases', 'Im(z)*x', 'Im');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e26b761befdd6a1c', 'test_to_string_complex_projection_TeX_cases', '(1/2)*Re(z)', 'Re');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_ae7ea7e0443cc4a9', 'test_to_string_complex_projection_TeX_cases', '(1/2)*Im(z)', 'Im');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_integrals_outside_fractions_TeX_cases', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_integrals_outside_fractions_TeX', 'expression_cases', 'test_to_string_integrals_outside_fractions_TeX_cases')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_integrals_outside_fractions_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_74cb15ff239a2878', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)/exp(x)', 'e^{-x}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8fe176945abe4b35', 'test_to_string_integrals_outside_fractions_TeX_cases', '-integral(exp(cosh(t)),t,0,x)/exp(x)', '-e^{-x}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_e3f93138b96a265f', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)/c', '\frac{1}{c}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_825879a5d304c356', 'test_to_string_integrals_outside_fractions_TeX_cases', '(1+integral(exp(cosh(t)),t,0,x))/c', '\frac{1}{c}\,');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_d501756fe399b039', 'test_to_string_integrals_outside_fractions_TeX_cases', '1/integral(exp(cosh(t)),t,0,x)', '\left(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_dd5d64982e8a051f', 'test_to_string_integrals_outside_fractions_TeX_cases', '1/exp(integral(exp(cosh(t)),t,0,x))', 'e^{-');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a176a43b48fd2d63', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)^(-2)', '\left(');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_7811b04c2979fda3', 'test_to_string_integrals_outside_fractions_TeX_cases', 'integral(exp(cosh(t)),t,0,x)^(-1/2)', '\left(');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_indexed_principal_roots_invalid', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_indexed_principal_roots', 'expression_cases', 'test_to_string_indexed_principal_roots_invalid')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_indexed_principal_roots. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_03a3a063f760a8fd', 'test_to_string_indexed_principal_roots_invalid', '⁰√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_262c685820f34215', 'test_to_string_indexed_principal_roots_invalid', '¹√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_5b0dc18c5e658907', 'test_to_string_indexed_principal_roots_invalid', '⁻³√(2)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_8fb9a71860a75c5a', 'test_to_string_indexed_principal_roots_invalid', '³√', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b64276ea9aaabd2d', 'test_to_string_indexed_principal_roots_invalid', '³√()', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_b6425bea9aaa8f4c', 'test_to_string_indexed_principal_roots_invalid', '³√(2', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_48b2bae6eb589b20', 'test_to_string_indexed_principal_roots_invalid', '²¹⁴⁷⁴⁸³⁶⁴⁸√(2)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_to_string_compact_product_quotient_TeX_sources', 'tests/expression/test_expr_to_string.c', 'tests.test_expr_t_to_string.test_to_string_all.test_to_string_compact_product_quotient_TeX', 'expression_cases', 'test_to_string_compact_product_quotient_TeX_sources')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

-- Extracted from tests/expression/test_expr_to_string.c: test_to_string_compact_product_quotient_TeX. Existing checks are retained in C.
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_3b5a24a32a1074f7', 'test_to_string_compact_product_quotient_TeX_sources', 'a*(x+1)/(x^2+1)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_a9c3b3d0b151ef65', 'test_to_string_compact_product_quotient_TeX_sources', 'b*(t-2)/(t^2+3)', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_97a5cdeb7f1c68c7', 'test_to_string_compact_product_quotient_TeX_sources', '(x+1)/(2*(x^2+1))', '');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('case_95ff7451793c5299', 'test_to_string_compact_product_quotient_TeX_sources', 'c*(2*c*x+1)/(2*c*x*(c*x+1)+1)', '');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_inverse_rational_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_inverse_rational', 'expression_cases', 'test_from_string_inverse_rational_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('inverse of Equation-mode rational output', 'test_from_string_inverse_rational_check_parse_num_1', '{@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)} | t=0}', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('expanded rational denominator', 'test_from_string_inverse_rational_check_parse_num_1', '{@Linv{(5*s^3+15*s^2+50)/(s^4+4*s^3+5*s^2)} | t=0}', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('repeated quadratic inverse', 'test_from_string_inverse_rational_check_parse_num_1', '{2*@Linv{1/(s^2+1)^2}/(sin(t)-t*cos(t)) | t=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('distinct quadratic inverse', 'test_from_string_inverse_rational_check_parse_num_1', '{3*@Linv{1/((s^2+1)*(s^2+4))}/(sin(t)-sin(2*t)/2) | t=1}', '1');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero hyperbolic tangent', 'test_from_string_laplace_check_parse_num_1', '{@L(tanh(c*t)) | c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero tangent without target', 'test_from_string_laplace_check_parse_num_1', '{@L(tan(c*t)) | c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero tangent no half-plane restriction', 'test_from_string_laplace_check_parse_num_1', '{@L(tan(c*t)) | s=-2; c=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace logarithm', 'test_from_string_laplace_check_parse_num_1', '{@Linv{-(ln(s)+γ)/s} | t=1}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace logarithmic coefficient', 'test_from_string_laplace_check_parse_num_1', '{@Linv{-(ln(s)+γ)/s}/ln(2) | t=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace braced logarithm', 'test_from_string_laplace_check_parse_num_1', '{@L{ln(t)}/@eulermascheroni | s=1}', '-1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace braced inverse', 'test_from_string_laplace_check_parse_num_1', '{@Linv{1/s^2} | t=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace pole', 'test_from_string_laplace_check_parse_num_1', '{@Linv(1/s) | t=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace repeated pole', 'test_from_string_laplace_check_parse_num_1', '{InverseLaplace(1/s^3) | t=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace Unicode', 'test_from_string_laplace_check_parse_num_1', '{ℒ⁻¹(s/(s^2+1)) | t=0}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Inverse Laplace shifted cosine', 'test_from_string_laplace_check_parse_num_1', '{@Linv((s-a)/((s-a)^2+1)) | t=0; a=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace polynomial', 'test_from_string_laplace_check_parse_num_1', '{@L(t^2+3*t+2) | s=2}', '2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace Bessel zero', 'test_from_string_laplace_check_parse_num_1', '{@L(J_0(t))*sqrt(5) | s=2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Bessel ASCII alias', 'test_from_string_laplace_check_parse_num_1', 'J0(0)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Bessel subscript alias', 'test_from_string_laplace_check_parse_num_1', 'J₀(0)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic power', 'test_from_string_laplace_check_parse_num_1', '{@L(t^n,t) | s=2; n=3}', '3/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace fractional power', 'test_from_string_laplace_check_parse_num_2', '{@L(t^(-1/2)) | s=1}', 'sqrt(pi)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace square root', 'test_from_string_laplace_check_parse_num_2', '{2*@L(sqrt(t))/sqrt(pi) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace cube root', 'test_from_string_laplace_check_parse_num_2', '{@L(cubrt(t))/gamma(4/3) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace fourth root', 'test_from_string_laplace_check_parse_num_2', '{@L(root(t,4))/gamma(5/4) | s=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace reciprocal root', 'test_from_string_laplace_check_parse_num_2', '{@L(1/sqrt(t)) | s=1}', 'sqrt(pi)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Readable transform domain', 'test_from_string_laplace_check_parse_num_2', '{120/s^6 where (Re(s)>0) | s=2}', '15/8');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Binding domain condition', 'test_from_string_laplace_check_parse_num_2', '{120/s^6 | s=2; Re(s)>0}', '15/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Binding condition rendering', 'test_from_string_laplace_check_parse_expr_3', '{120/s^6 where (Re(s)>0) | s=2}', '{ 120/s⁶ | s = 2; Re(s) > 0 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Multiple domain conditions', 'test_from_string_laplace_check_parse_num_4', '{gamma(n+1)/s^(n+1) where (Re(s) > 0; Re(n) > -1) | s=2; n=5}', '15/8');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_num_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_num_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace exponential', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(-t)) | s=2}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic exponential notation', 'test_from_string_laplace_check_parse_num_5', '{@L(e^(a*t)) | s=2; a=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace negative rate half-plane', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(a*t)) | s=-1; a=-2}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace complex rate', 'test_from_string_laplace_check_parse_num_5', '{@L(exp(a*t)) | s=2+i; a=1+i}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Real-part bound round trip', 'test_from_string_laplace_check_parse_num_5', '{1/(s-a) | s=2+i; a=1+i; Re(s)>Re(a)}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace sine', 'test_from_string_laplace_check_parse_num_5', '{ℒ(sin(t)) | s=2}', '1/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace inferred time with Greek frequency', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(@omega*t)) | s=2; @omega=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace inferred time with free parameter', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(x*t)) | s=2; x=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace explicit source overrides time', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(x*t),x) | s=2; t=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace complementary error functions', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(t))+@L(erfc(t)) | s=2}', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace opposite error function rates', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(t))+@L(erf(-t)) | s=2}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace zero error function rate', 'test_from_string_laplace_check_parse_num_5', '{@L(erf(at)) | s=2; a=0}', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted symbolic sine', 'test_from_string_laplace_check_parse_num_5', '{@L(tsin(at)) | s=2; a=1}', '4/25');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(tcos(at)) | s=2; a=1}', '3/25');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace squared-time cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(t^2*cos(at),t) | s=2; a=1}', '4/125');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace time-weighted exponential', 'test_from_string_laplace_check_parse_num_5', '{@L(t*exp(at)) | s=2; a=1}', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace hyperbolic sine', 'test_from_string_laplace_check_parse_num_5', '{@L(sinh(at)) | s=2; a=1}', '1/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace hyperbolic cosine', 'test_from_string_laplace_check_parse_num_5', '{@L(cosh(at)) | s=2; a=1}', '2/3');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace imaginary hyperbolic rate', 'test_from_string_laplace_check_parse_num_5', '{@L(cosh(at)) | s=2; a=i}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace bound sine power', 'test_from_string_laplace_check_parse_num_5', '{@L(sin^n(t)) | s=1; n=5}', '3/13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace even sine power', 'test_from_string_laplace_check_parse_num_5', '{ℒ(sin(t)^2) | s=1}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace even cosine power', 'test_from_string_laplace_check_parse_num_5', '{Laplace(cos(t)^4,t,p) | p=1}', '41/85');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace odd cosine power', 'test_from_string_laplace_check_parse_num_5', '{@L(cos(t)^3) | s=1}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic cosine sum', 'test_from_string_laplace_check_parse_num_5', '{@L(cos(t)^n,t) | s=1, n=3}', '2/5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace symbolic sine sum', 'test_from_string_laplace_check_parse_num_5', '{@L(sin(t)^n,t) | s=1, n=5}', '3/13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Integer domain satisfied', 'test_from_string_laplace_check_parse_num_5', '{1/s | s=2; n=5; nonnegative_integer(n)}', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace negative frequency', 'test_from_string_laplace_check_parse_num_5', '{@L(sin(-2*t)^3) | s=1}', '-48/185');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace explicit target', 'test_from_string_laplace_check_parse_num_5', '{Laplace(t,t,p) | p=2}', '1/4');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_expr_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_expr_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace canonical call', 'test_from_string_laplace_check_parse_expr_6', '@L(f(t),t,p)', '{ ℒ(f(t), t, p) | p = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_laplace_check_parse_TeX_7', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_laplace', 'expression_cases', 'test_from_string_laplace_check_parse_TeX_7')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Laplace convergence', 'test_from_string_laplace_check_parse_TeX_7', '@L(t)', '\left\{ \frac{1}{s^{2}}\quad (\mathfrak{Re}(s)>0) \;\middle|\; s = ? \right\}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('w followed by call parentheses selects principal Lambert W', 'test_from_string_function_hash_check_parse_expr_1', 'w(x)', '{ W(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('w0 followed by call parentheses selects Lambert W zero branch', 'test_from_string_function_hash_check_parse_expr_1', 'w0(x)', '{ W₀(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('wm1 followed by call parentheses selects Lambert W minus-one branch', 'test_from_string_function_hash_check_parse_expr_1', 'wm1(x)', '{ W₋₁(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('wn followed by call parentheses selects a named Lambert W branch', 'test_from_string_function_hash_check_parse_expr_1', 'wn(n,x)', '{ Wₙ(n, x) | x = ?; n = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_2', 'zeta(3)', '1.202056903159594285399738161511449990764986292340498881792271555');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetap is registered in the perfect hash', 'test_from_string_function_hash_check_parse_expr_3', 'zetap(x)', '{ ζ''(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetap bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_3', '{ zetap(x) | x = 3 }', '{ ζ''(x) | x = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta-prime symbol selects the Riemann derivative', 'test_from_string_function_hash_check_parse_expr_3', 'ζ''(x)', '{ ζ''(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta-prime symbol selects the Hurwitz derivative', 'test_from_string_function_hash_check_parse_expr_3', 'ζ''(s, a)', '{ ζ''(s, a) | s = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('the zeta symbol is a perfect-hash alias', 'test_from_string_function_hash_check_parse_expr_3', 'ζ(x)', '{ ζ(x) | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('the zeta symbol evaluates through its alias', 'test_from_string_function_hash_check_parse_num_4', 'ζ(3)', '1.202056903159594285399738161511449990764986292340498881792271555');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('two-argument zeta selects Hurwitz zeta', 'test_from_string_function_hash_check_parse_num_4', 'zeta(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('two-argument zeta symbol selects Hurwitz zeta', 'test_from_string_function_hash_check_parse_num_4', 'ζ(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zetah is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'zetah(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta2 is a perfect-hash alias', 'test_from_string_function_hash_check_parse_num_4', 'zeta2(2.5, 101)', '0.000661687499453171542062211501479711744239330816315948606222019329');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zatahp is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'zatahp(2.5, 101)', '-0.003491619656530338106744558404347153037971242923601833749838331612');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('qdigamma is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'qdigamma(1/2,1)', '-0.4205290343560457797847369304069241');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma symbol is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_4', 'ψq(1/2,1)', '-0.4205290343560457797847369304069241');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('qdigamma bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_5', '{ qdigamma(q,z) | q=1/2; z=1 }', '{ ψq(q, z) | q = ¹⁄₂; z = 1 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma symbol bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_5', '{ ψq(q,z) | q=1/2; z=1 }', '{ ψq(q, z) | q = ¹⁄₂; z = 1 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('zeta2p is a perfect-hash alias', 'test_from_string_function_hash_check_parse_num_6', 'zeta2p(2.5, 101)', '-0.003491619656530338106744558404347153037971242923601833749838331612');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_7', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_7')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('infinite product is a built-in function', 'test_from_string_function_hash_check_parse_expr_7', 'product(k,1,inf,1-1/k^2)', 'Π_(k=1)^∞ (1 - 1/k²)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_8', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_8')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('harmonic_poly is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_8', 'harmonic_poly(4, 1/2)', '131/192');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_expr_9', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_expr_9')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('harmonic_poly bindings use the binding perfect hash', 'test_from_string_function_hash_check_parse_expr_9', '{ harmonic_poly(n,x) | x=1/2; n=4 }', '{ Hn(n, x) | x = ¹⁄₂; n = 4 }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_num_10', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_num_10')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Lerch phi evaluates through its polylogarithm reduction', 'test_from_string_function_hash_check_parse_num_10', 'lerch_phi(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('LerchPhi is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_10', 'LerchPhi(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('capital phi is registered in the perfect hash', 'test_from_string_function_hash_check_parse_num_10', 'Φ(0.5,2,1)', '1.164481052930025011805312640319360217488396949612252850868694096');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_function_hash_check_parse_TeX_11', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_function_hash', 'expression_cases', 'test_from_string_function_hash_check_parse_TeX_11')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Lerch phi renders as capital phi', 'test_from_string_function_hash_check_parse_TeX_11', 'lerch_phi(1/2,2,1)', '\Phi\left(\frac{1}{2}, 2, 1\right)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('q-digamma renders its base as a subscript', 'test_from_string_function_hash_check_parse_TeX_11', 'qdigamma(1/2,1)', '\psi_{\frac{1}{2}}\left(1\right)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conj alias parses', 'test_from_string_conjugation_check_parse_expr_1', 'conj(1 + 2i)', 'conj(1 + 2i)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conjugate alias canonicalises', 'test_from_string_conjugation_check_parse_expr_1', 'conjugate(1 + 2i)', 'conj(1 + 2i)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('postfix conjugate parses', 'test_from_string_conjugation_check_parse_expr_1', '(1 + 2i)^*', 'conj(1 + 2i)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conj alias evaluates', 'test_from_string_conjugation_check_parse_num_2', 'conj(1 + 2i)', '1 - 2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('conjugate alias evaluates', 'test_from_string_conjugation_check_parse_num_2', 'conjugate(1 + 2i)', '1 - 2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('postfix conjugate evaluates', 'test_from_string_conjugation_check_parse_num_2', '(1 + 2i)^*', '1 - 2i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_conjugation_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_conjugation', 'expression_cases', 'test_from_string_conjugation_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic modulus bars parse', 'test_from_string_conjugation_check_parse_expr_3', '|z|', '{ |z| | z = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('whitespace separates implicitly multiplied factors', 'test_from_string_arithmetic_check_parse_num_1', '44 i', '44i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('power binds before following division', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^2/6 | x = NAN }', '{ x²/6 | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unit fractional exponent shorthand remains an exponent', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^1/3 | x = NAN }', '{ x^⅓ | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('parenthesised rational remains an exponent', 'test_from_string_arithmetic_check_parse_expr_2', '{ x^(2/6) | x = NAN }', '{ x^⅓ | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_num_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_num_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('decimal binding stays exact through cancellation-sensitive cube', 'test_from_string_arithmetic_check_parse_num_3', '{ (5x)^3 | x = 128064.000000000120974 }', '262537412640768744007706072064702817249456224221302405921303/1000000000000000000000000000000000000000000');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_arithmetic_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_arithmetic', 'expression_cases', 'test_from_string_arithmetic_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('parenthesised exponent can itself be powered', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^(ix)² | x = NAN; a = NAN }', '{ a^(2ix) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('ASCII parenthesised exponent can itself be powered', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^(ix)^2 | x = NAN; a = NAN }', '{ a^(2ix) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised chained powers are left associative', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^x^2 | x = NAN; a = NAN }', '{ a^(2x) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('explicit exponent power stays inside exponent', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ a^((ix)^2) | x = NAN; a = NAN }', '{ a^(-x²) | x = ?; a = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('power of symbolic power folds integer exponent', 'test_from_string_arithmetic_check_parse_simplified_expr_4', '{ (a^(-x))² | x = NAN; a = NAN }', '{ a^(-2x) | x = ?; a = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_TeX_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_TeX_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('cubrt uses an indexed radical in TeX', 'test_from_string_functions_check_parse_TeX_1', 'cubrt(x)', '\left\{ \sqrt[3]{x} \;\middle|\; x = ? \right\}');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('root uses its order as the TeX radical index', 'test_from_string_functions_check_parse_TeX_1', 'root(x, n)', '\left\{ \sqrt[n]{x} \;\middle|\; x = ?; n = ? \right\}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_num_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_num_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('10! lowers to gamma(11)', 'test_from_string_functions_check_parse_num_2', '{ 10! }', '3628800');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('x! lowers to gamma(x+1)', 'test_from_string_functions_check_parse_num_2', '{ x! | x = 5 }', '120');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('exp(pi*i/2) = i', 'test_from_string_functions_check_parse_num_2', '{ exp(@pi*i/2) }', 'i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('1/pi stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ 1/pi }', '1/π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pi/2 stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ pi/2 }', 'π/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('i^i stays symbolic', 'test_from_string_functions_check_parse_expr_3', '{ i^i }', 'i^i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('i^(1+i) parses', 'test_from_string_functions_check_parse_expr_3', '{ i^(1+i) }', 'i^(1 + i)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W0 identity keeps unbound argument', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W_0(x)*exp(W_0(x)) }', '{ x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W0 identity resolves bound pi', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W_0(x)*exp(W_0(x)) | x = pi }', 'π');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W-1 identity resolves bound e', 'test_from_string_functions_check_parse_simplified_expr_4', '{ W-1(x)*exp(W-1(x)) | x = e }', 'e');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W identity recognises e power', 'test_from_string_functions_check_parse_expr_5', '{ W(2e^2) }', '2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_functions_check_parse_expr_6', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_functions', 'expression_cases', 'test_from_string_functions_check_parse_expr_6')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('plain Greek aliases use the perfect hash', 'test_from_string_functions_check_parse_expr_6', '{ cos(theta) + i*sin(theta) }', '{ cos(θ) + i·sin(θ) | θ = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('trigamma at positive infinity is zero', 'test_from_string_special_functions_check_parse_simplified_expr_1', 'trigamma(inf)', '0');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('polygamma expression uses standard symbol', 'test_from_string_special_functions_check_parse_expr_2', '{ polygamma(2, x) | x = ? }', '{ ψ⁽²⁾(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic polygamma order uses the base psi symbol', 'test_from_string_special_functions_check_parse_expr_2', '{ polygamma(n, x) | n = ?; x = ? }', '{ ψ(n, x) | n = ?; x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('base psi symbol round-trips symbolic polygamma', 'test_from_string_special_functions_check_parse_expr_2', '{ ψ(n, x) | n = ?; x = ? }', '{ ψ(n, x) | n = ?; x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W(x) infers x binding', 'test_from_string_special_functions_check_parse_expr_3', '{ W(x) }', '{ W(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W_0(x) infers x binding', 'test_from_string_special_functions_check_parse_expr_3', '{ W_0(x) }', '{ W₀(x) | x = ? }');
INSERT INTO expression_cases(name, operation, input, expected) VALUES ('lambert_wn parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ lambert_Wn(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }') ON CONFLICT(name) DO UPDATE SET input=excluded.input, expected=excluded.expected;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Wn parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ Wn(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('W_n parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ W_n(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Wₙ parses branch-aware Lambert W', 'test_from_string_special_functions_check_parse_expr_3', '{ Wₙ(2, x) | x = ? }', '{ Wₙ(2, x) | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_special_functions_check_parse_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_special_functions', 'expression_cases', 'test_from_string_special_functions_check_parse_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('cdf alias keeps user spelling', 'test_from_string_special_functions_check_parse_expr_4', '{ cdf(1.96) - cdf(-1.96) }', 'cdf(1.96) - cdf(-1.96)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pdf and logpdf aliases keep user spelling', 'test_from_string_special_functions_check_parse_expr_4', '{ pdf(0) + logpdf(0) }', 'pdf(0) + logpdf(0)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_exact_value_functions_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_exact_value_functions', 'expression_cases', 'test_from_string_exact_value_functions_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factorial(10) = 10!', 'test_from_string_exact_value_functions_check_parse_num_1', '{ factorial(n) | n = 10 }', '3628800');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('fibonacci(50)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ fibonacci(n) | n = 50 }', '12586269025');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('partition(5)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ partition(n) | n = 5 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('isqrt(200)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ isqrt(n) | n = 200 }', '14');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('gcd(84, 30)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ gcd(a, b) | a = 84, b = 30 }', '6');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('lcm(-21, 6)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ lcm(a, b) | a = -21, b = 6 }', '42');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('mod(29, 5)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ mod(a, b) | a = 29, b = 5 }', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('modinv(3, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ modinv(a, b) | a = 3, b = 11 }', '4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('is_prime(97)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ is_prime(n) | n = 97 }', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('is_prime(221)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ is_prime(n) | n = 221 }', '0');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('next_prime(14)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ next_prime(n) | n = 14 }', '17');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('prev_prime(14)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ prev_prime(n) | n = 14 }', '13');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('AND(13, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ AND(a, b) | a = 13, b = 11 }', '9');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('OR(13, 11)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ OR(a, b) | a = 13, b = 11 }', '15');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('XOR(13, 8)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ XOR(a, b) | a = 13, b = 8 }', '5');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('NOT(8)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ NOT(a) | a = 8 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('SHL(7, 3)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ SHL(a, n) | a = 7, n = 3 }', '56');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('SHR(56, 3)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ SHR(a, n) | a = 56, n = 3 }', '7');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factors(360)', 'test_from_string_exact_value_functions_check_parse_num_1', '{ a₀³·a₁²·a₂ | ; a₀ = 2, a₁ = 3, a₂ = 5 }', '360');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_prime_factor_products', 'tests/expression/test_expr_from_string.c',
    'tests.test_expr_t_from_string.test_from_string_exact_value_functions', 'expression_cases',
    'test_prime_factor_products')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_360', 'test_prime_factor_products', 'factors(360)', '{ a₀³·a₁²·a₂ | ; a₀ = 2, a₁ = 3, a₂ = 5 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_benchmark_1215_bits', 'test_prime_factor_products', 'factors(547721190191546363398678620411634487909744841314598615206873278092099143398360125227595479954148677571112600596738260011818279723370813377533578833565803706508131569062299976033041302189833215742992653063887339218309467131548996199881113600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)', '{ a₀⁵¹²·a₁²⁵⁶·a₂¹²⁸ | ; a₀ = 2, a₁ = 3, a₂ = 5 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_power_2048_bits', 'test_prime_factor_products', 'factors(2^2048)', '{ a₀²⁰⁴⁸ | ; a₀ = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_bound_power_2048_bits', 'test_prime_factor_products', '{ factors(n) | n = 2^2048 }', '{ a₀²⁰⁴⁸ | ; a₀ = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_identity', 'test_prime_factor_products', 'factors(1)', '1');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_zero_formal', 'test_prime_factor_products', 'factors(0)', 'factors(0)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_negative_formal', 'test_prime_factor_products', 'factors(-8)', 'factors(-8)');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('factor_product_fraction_formal', 'test_prime_factor_products', 'factors(3/2)', 'factors(³⁄₂)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_num_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_num_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure const rational', 'test_from_string_number_literals_check_parse_num_1', '{ [half] = 1/2 }', '1/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('rational atom expression', 'test_from_string_number_literals_check_parse_num_1', '{ 1/2 + 1/4 }', '3/4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unicode rational atom expression', 'test_from_string_number_literals_check_parse_num_1', '{ ½ + ¼ }', '3/4');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom', 'test_from_string_number_literals_check_parse_num_1', '{ ³⁵⁵⁄₁₁₃ }', '355/113');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('rational atom expression simplifies display', 'test_from_string_number_literals_check_parse_expr_2', '{ 231/2310 }', '⅒');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom simplifies display', 'test_from_string_number_literals_check_parse_expr_2', '{ ²³¹⁄₂₃₁₀ }', '⅒');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised radical constant round-trips', 'test_from_string_number_literals_check_parse_expr_2', '{ √3/2 }', '√(3)/2');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unparenthesised radical supports implicit multiplication', 'test_from_string_number_literals_check_parse_expr_2', '{ 4√3/2 }', '2·√(3)');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_TeX_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_TeX_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('stacked unicode rational atom renders TeX', 'test_from_string_number_literals_check_parse_TeX_3', '{ ²³¹⁄₂₃₁₀ }', '\frac{1}{10}');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('NAN is a numeric placeholder, not a variable name', 'test_from_string_number_literals_check_parse_expr_4', '{ NAN }', 'NAN');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('lowercase nan is a numeric placeholder', 'test_from_string_number_literals_check_parse_expr_4', '{ nan }', 'NAN');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('repeated unary signs simplify', 'test_from_string_number_literals_check_parse_expr_4', '{ --2x | x = ? }', '{ 2x | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_number_literals_check_parse_num_5', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_number_literals', 'expression_cases', 'test_from_string_number_literals_check_parse_num_5')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure imaginary coefficient atom', 'test_from_string_number_literals_check_parse_num_5', '{ 3/2i }', '3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure const rational complex', 'test_from_string_number_literals_check_parse_num_5', '{ [z] = 1/2 - 3/2i }', '1/2 - 3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding rational complex', 'test_from_string_number_literals_check_parse_num_5', '{ z | z = 1/2 - 3/2i }', '1/2 - 3/2i');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding parenthesized imag coeff', 'test_from_string_number_literals_check_parse_num_5', '{ z | z = 1/2 + (2)i }', '1/2 + 2i');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_deriv_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_deriv', 'expression_cases', 'test_from_string_deriv_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dx(...) parses as expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dx(exp(x^2)) | x = ? }', '{ 2x·exp(x²) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dx(...) infers only its derivative variable', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dx(exp(x^2)) }', '{ 2x·exp(x²) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dxx(...) parses as repeated expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dxx(x^3) | x = ? }', '{ 6x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Dxy(...) parses as mixed expression derivative', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ Dxy(x^2*y^3) | x = ?, y = ? }', '{ 6xy² | x = ?, y = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz first derivative uses the native derivative parser', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d/dx(x^3) }', '{ 3x² | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz second derivative uses matching orders', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d^2/dx^2(x^3) }', '{ 6x | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('Leibniz mixed derivative matches Dxy', 'test_from_string_deriv_check_parse_simplified_expr_1', '{ d^2/dxy(x^2*y^3) | x = ?, y = ? }', '{ 6xy² | x = ?, y = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_deriv_check_parse_simplified_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_deriv', 'expression_cases', 'test_from_string_deriv_check_parse_simplified_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('D[x^5](...) parses as compact high-order derivative', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ D[x^5](x^7) }', '{ 2520x² | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('D[x^2y^3](...) preserves mixed derivative order', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ D[x^2y^3](x^3*y^4) }', '{ 144xy | x = ?, y = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('canonical superscript derivative input round trips', 'test_from_string_deriv_check_parse_simplified_expr_2', '{ Dx²(x^3) }', '{ 6x | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic pi/2', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ e^(sin(x)) | x = pi/2 }', '{ e^sin(x) | x = π/2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic 3/2*pi', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ e^(sin(x)) | x = 3/2*pi }', '{ e^sin(x) | x = ³⁄₂π }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves symbolic pi^2/2', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_1', '{ x | x = (pi^2)/2 }', 'π²/2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_2', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_2')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value round-trips Unicode powers', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_2', '{ x | x = (pi²)/2 }', 'π²/2');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('pure numeric expression preserves full mathematical tree', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ phi - 1/2(1+sqrt(5)) }', 'φ - ½·(1 + √(5))');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value preserves full mathematical tree', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ x | x = 1/2(1+sqrt(5)) }', '¹⁄₂·(1 + √(5))');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value round-trips pretty multiply', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ -x + phi | x = ½·(1 + √(5)) }', '{ -x + φ | x = ½·(1 + √(5)) }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value simplifies numeric magnitude and preserves symbolic functions', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ x | x = abs(-3)+floor(pi)+ceil(phi) }', '3 + ⌊π⌋ + ⌈φ⌉');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('binding value accepts unknown marker', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ sinh(x) | x = ? }', '{ sinh(x) | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('user-bound e remains symbolic', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ E - M - e·sin(E) | ; M = pi/1.234, e=0.0167 }', '{ E - M - e·sin(E) | E = ?; M = π/1.234, e = 0.0167 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('constant binding simplifies Lambert e power', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ polygamma(9, x*y*c) | x = 7, y = 1; c = W(2e^2) }', '{ ψ⁽⁹⁾(cxy) | x = 7, y = 1; c = 2 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('simplified constants keep bindings', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ ax + yb + zc - 8 | a = NAN, b = NAN, c = NAN; x=1, y = 2, z = 3 }', '{ ax + by + cz - 8 | a = ?, b = ?, c = ?; x = 1, y = 2, z = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('constant bindings accept semicolon separators', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1; y = 2, z = 3 }', '{ ax + by + cz - 8 | a = ?, b = ?, c = ?; x = 1, y = 2, z = 3 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('const-only binding preserves leading semicolon', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√H) | ;H = 163 }', '{ exp(π·√H) | ; H = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('const-only binding accepts trailing digit subscript', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√(H8)) | ; H8 = 163 }', '{ exp(π·√(H₈)) | ; H₈ = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('minus after factor remains subtraction', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(π·√(H8))-(5x)^3 | H8 = 163, x = NAN }', '{ exp(π·√(H₈)) - (5x)³ | H₈ = 163, x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('absolute-value bars omit inner spaces', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ exp(pi*sqrt(x)) - | exp(pi*sqrt(x)) | | x = NAN }', '{ exp(π·√x) - |exp(π·√x)| | x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('symbolic pi quotient cancels powers', 'test_from_string_bindings_with_constant_expression_value_check_parse_expr_3', '{ pi/pi^2 }', 'π/π²');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_bindings_with_constant_expression_value', 'expression_cases', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('generated derivative with NaN bindings simplifies explicitly', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ -y²z²·sin(xyz)·exp(sin(xyz)) + y²z²·cos²(xyz)·exp(sin(xyz)) | y = NAN, z = NAN, x = NAN }', '{ y²z²·exp(sin(xyz))·(cos²(xyz) - sin(xyz)) | y = ?, z = ?, x = ? }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('reparsed symbolic pi derivative simplifies explicitly', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (-2π·exp(π·√x) + 2·π²·√x·exp(π·√x))/(2·√x)/(2·√x)² | x = 163 }', '{ ¼π·exp(π·√x)·(π·√x - 1)/x^³⁄₂ | x = 163 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('nested rational derivative numerator canonicalizes', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (2*(x^2 + 3*x + 5)*(2*(x + 1)*(2*x + 3) - 9*x - (-x - 1)*(2*x + 3) - 3*x^2 - 15) - 6*(2*x + 3)*((-x - 1)*(x^2 + 3*x + 5) - (2*x + 3)*(3*x - (x + 1)*(2*x + 3) + x^2 + 5)))/(x^2 + 3*x + 5)^4 | x = 1 }', '{ 6·(12x² + 44x - x⁴ - 4x³ + 23)/(x² + 3x + 5)⁴ | x = 1 }');
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('higher rational derivative numerator canonicalizes', 'test_from_string_bindings_with_constant_expression_value_check_parse_simplified_expr_4', '{ (24*(x^2 + 3*x + 5)*(6*x - x^3 - 3*x^2 + 11) - 24*(2*x + 3)*(12*x^2 + 44*x - x^4 - 4*x^3 + 23))/(x^2 + 3*x + 5)^5 | x = ? }', '{ 24·(x⁵ + 5x⁴ - 20x³ - 110x² - 115x - 14)/(x² + 3x + 5)⁵ | x = ? }');

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'expression.test_from_string_unevaluated_integral_check_parse_simplified_expr_1', 'tests/expression/test_expr_from_string.c', 'tests.test_expr_t_from_string.test_from_string_unevaluated_integral', 'expression_cases', 'test_from_string_unevaluated_integral_check_parse_simplified_expr_1')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO expression_cases(name, operation, input, expected) VALUES ('unevaluated integral explicit bounds simplify structurally', 'test_from_string_unevaluated_integral_check_parse_simplified_expr_1', '{ ∫_1^3 1/t·dt }', '∫^3_1 1/t·dt');
