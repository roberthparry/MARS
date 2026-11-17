-- Precision regressions for previously failing or timed-out benchmark functions.

-- Full-width defining-series references exercise product trees and both precision crossovers.
WITH precisions(digits) AS (VALUES(80),(300),(900),(1234)),
operations(name) AS (VALUES('bessel_i_reference'),('bessel_j_reference'),
    ('struve_h_reference'),('struve_l_reference')),
arguments(name,nu,z) AS (VALUES
    ('real','2','0.7'),
    ('zero_order','0','0.75+0.125i'),
    ('larger_order','5','1.25+0.125i'),
    ('complex_argument','2','0.7+0.125i'),
    ('complex_order','0.5+0.125i','1.25-0.125i'),
    ('negative_bank','0.5','-1.25+0i'),
    ('negative_order','-0.5','0.75+0.125i'),
    ('outside_radius','2','7+0.125i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'cylinder_tree_' || operations.name || '_' || arguments.name || '_' || digits,
       operations.name, nu || ';' || z, '0', digits
FROM precisions CROSS JOIN operations CROSS JOIN arguments;

WITH precisions(digits) AS (VALUES(80),(300),(900),(1234)), derivative(value) AS (VALUES('0'),('1')),
arguments(name,mu,nu,z) AS (VALUES
    ('real','0.5','0.7','1.2'),
    ('complex','0.5+0.125i','0.7+0.125i','1.2+0.125i'),
    ('conjugate','0.5-0.125i','0.7-0.125i','1.2-0.125i'),
    ('negative_bank','0.5','0.7','-1.2+0i'),
    ('vanishing_leading_derivative','-1','0.7+0.125i','0.75+0.125i'),
    ('negative_lower','-4.125','0.75','0.7+0.125i'),
    ('outside_radius','0.5+0.125i','0.7','7+0.125i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'cylinder_tree_lommel_' || name || '_' || value || '_' || digits, 'lommel_reference',
       mu || ';' || nu || ';' || z || ';' || value, '0', digits
FROM precisions CROSS JOIN derivative CROSS JOIN arguments;

INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision) VALUES
    ('cylinder_tree_alias_real','two_lower_alias','1.25;2.5;-0.5;1','0',1234),
    ('cylinder_tree_alias_complex','two_lower_alias','1.25+0.125i;2.5-0.125i;0.7+0.125i;0.5+0.125i','0',1234),
    ('cylinder_tree_alias_zero_weight','two_lower_alias','1.25;2.5;-0.5;0','0',1234),
    ('cylinder_tree_reject_lower','two_lower_reject','-0.5;1.5;0.25;1','0',1234),
    ('cylinder_tree_reject_radius','two_lower_reject','1;2;8.125;1','0',1234),
    ('cylinder_tree_reject_weight','two_lower_reject','1;2;0.25;33','0',1234);

WITH families(value) AS (VALUES(2),(3),(4)), precisions(digits) AS (VALUES(80),(300),(1234)),
arguments(name,x,y) AS (VALUES('real','0.0625','0.03125'),
    ('complex','0.0625+0.015625i','0.03125-0.015625i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'appell_family_reduction_' || value || '_' || name || '_' || digits,
       'appell_family_reduction', value || ';' || x || ';' || y, '0', digits
FROM families CROSS JOIN precisions CROSS JOIN arguments;

WITH families(value) AS (VALUES(2),(3),(4)),
arguments(name,x,y) AS (VALUES('real','0.0625','0.03125'),
    ('complex','0.0625+0.015625i','0.03125-0.015625i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'appell_family_reduction_native_' || value || '_' || name,
       'appell_family_reduction_native', value || ';' || x || ';' || y, '0', 32
FROM families CROSS JOIN arguments;

WITH families(value) AS (VALUES(2),(3),(4)), precisions(digits) AS (VALUES(80),(300),(1234)),
arguments(name, x, y) AS (VALUES
    ('real','0.0625','0.03125'),
    ('complex','0.0625+0.015625i','0.03125-0.015625i'),
    ('conjugate','0.0625-0.015625i','0.03125+0.015625i'),
    ('negative','-0.125','0.0625'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'appell_family_' || value || '_' || name || '_' || digits, 'appell_family',
       value || ';' || x || ';' || y, '0', digits FROM families CROSS JOIN precisions CROSS JOIN arguments;

WITH families(value) AS (VALUES(2),(3),(4)),
arguments(name,x,y) AS (VALUES('real','0.0625','0.03125'),('complex','0.0625+0.015625i','0.03125-0.015625i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'appell_family_native_' || value || '_' || name, 'appell_family_native',
       value || ';' || x || ';' || y, '0', 32 FROM families CROSS JOIN arguments;

WITH families(value) AS (VALUES(2),(3),(4)), precisions(digits) AS (VALUES(80),(300))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'appell_family_terminate_' || value || '_' || digits, 'appell_family_terminate',
       value || ';2;3', '0', digits FROM families CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision) VALUES
    ('appell_family_outside_f2','appell_family_reject','2;0.75;0.75','0',80),
    ('appell_family_outside_f3','appell_family_reject','3;1.125;0.125','0',80),
    ('appell_family_outside_f4','appell_family_reject','4;0.3;0.3','0',80),
    ('appell_family_opposing_f3','appell_family','3;0.125;-0.125','0',300);

WITH precisions(digits) AS (VALUES(300),(1234)), powers(value) AS (VALUES(1),(2),(3),(4)),
cases(name, input, stop) AS (VALUES
    ('real', '0.5', '96'),
    ('complex', '0.75+0.125i', '96'),
    ('conjugate', '0.75-0.125i', '96'),
    ('near_pole', '0.00000095367431640625+0.00000095367431640625i', '96'),
    ('imaginary_dominant', '0.125+32i', '96'),
    ('long', '2.25+0.125i', '714'),
    ('last_block', '90.25+0.125i', '96'),
    ('empty', '100+0.125i', '96'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT 'polygamma_blocks_' || name || '_' || powers.value || '_' || digits,
       'polygamma_blocks', input || ';' || powers.value || ';' || stop, '0', digits
FROM cases CROSS JOIN powers CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('polygamma_blocks_low_precision', 'polygamma_blocks_reject', '0.5;2;96', '0', 80),
    ('polygamma_blocks_negative', 'polygamma_blocks_reject', '-0.5;2;96', '0', 300),
    ('polygamma_blocks_imaginary', 'polygamma_blocks_reject', '0.5i;2;96', '0', 300),
    ('polygamma_blocks_zero', 'polygamma_blocks_reject', '0;2;96', '0', 300),
    ('polygamma_blocks_zero_power', 'polygamma_blocks_reject', '0.5;0;96', '0', 300),
    ('polygamma_blocks_high_power', 'polygamma_blocks_reject', '0.5;5;96', '0', 300),
    ('polygamma_blocks_zero_limit', 'polygamma_blocks_reject', '0.5;2;0', '0', 300),
    ('polygamma_blocks_high_limit', 'polygamma_blocks_reject', '0.5;2;65537', '0', 300);

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, input) AS (VALUES
    ('real', '0.5;1;1;2.5;0.25;0.3125'),
    ('complex', '0.5+0.125i;1;1;2.5+0.125i;0.2+0.125i;0.3+0.125i'),
    ('conjugate', '0.5-0.125i;1;1;2.5-0.125i;0.2-0.125i;0.3-0.125i'),
    ('alternating', '0.5;1;1;2.5;-0.3125;0.25'),
    ('close', '0.5+0.125i;1;1;2.5+0.125i;0.25+0.125i;0.2509765625+0.125i'),
    ('eight', '0.5+0.125i;1;1;8.5+0.125i;0.25+0.125i;0.3125+0.0625i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT 'appell_precision_distinct_' || name || '_' || digits, 'appell_partial_fraction', input, '0', digits
FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('appell_precision_equal_real', 'appell_equal', '0.5;0.75;1.25;2.5;0.25;0.25'),
    ('appell_precision_equal_complex', 'appell_equal',
        '0.5+0.125i;0.75+0.125i;1.25-0.125i;2.5+0.125i;0.25+0.125i;0.25+0.125i'),
    ('appell_precision_equal_conjugate', 'appell_equal',
        '0.5-0.125i;0.75-0.125i;1.25+0.125i;2.5-0.125i;0.25-0.125i;0.25-0.125i'),
    ('appell_precision_equal_alternating', 'appell_equal', '0.5;0.75;1.25;2.5;-0.3125;-0.3125'),
    ('appell_precision_equal_negative_complex', 'appell_equal',
        '0.5+0.125i;0.75+0.125i;1.25-0.125i;2.5+0.125i;-0.25+0.125i;-0.25+0.125i'),
    ('appell_precision_equal_cancellation', 'appell_equal',
        '0.5+0.125i;-3+0.125i;3.5-0.125i;2.5+0.125i;0.25+0.125i;0.25+0.125i'),
    ('appell_precision_equal_zero', 'appell_equal', '0.5;0.75;-0.75;2.5;0.25;0.25'),
    ('appell_precision_equal_eight', 'appell_equal', '0.5;0.75;1.25;8.5;0.25;0.25'),
    ('appell_precision_radius_limit', 'appell_reduced', '0.5;2.5;0;2.5;0.4375;0.125'),
    ('appell_precision_radius_above', 'appell_reduced', '0.5;2.5;0;2.5;0.453125;0.125'),
    ('appell_precision_parameter_limit', 'appell_reduced', '1;8;0;8;0.3125;0.125'),
    ('appell_precision_contraction_fallback', 'appell_reduced', '0.5;2.5;0;2.5;0.4;0.4'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(name, digits) AS (VALUES('maximum',2465),('above',2490)),
cases(name, input) AS (VALUES
    ('real', '0.5;0.75;1.25;2.5;0.25;0.25'),
    ('complex', '0.5+0.125i;0.75+0.125i;1.25-0.125i;2.5+0.125i;0.25+0.125i;0.25+0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT 'appell_precision_' || cases.name || '_' || precisions.name, 'appell_equal', input, '0', digits
FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('clausen_blocks_real', 'clausen_reference', '0.75'),
    ('clausen_blocks_real_lower_boundary', 'clausen_reference', '0.125'),
    ('clausen_blocks_real_upper_boundary', 'clausen_reference', '0.875'),
    ('clausen_blocks_real_negative', 'clausen_reference', '-0.75'),
    ('clausen_blocks_complex', 'clausen_reference', '0.75+0.125i'),
    ('clausen_blocks_conjugate', 'clausen_reference', '0.75-0.125i'),
    ('clausen_blocks_negative', 'clausen_reference', '-0.75+0.125i'),
    ('clausen_blocks_lower_boundary', 'clausen_reference', '0.125+0.5i'),
    ('clausen_blocks_upper_boundary', 'clausen_reference', '0.875+0.5i'),
    ('clausen_blocks_fallback', 'clausen_reference', '1+0.125i'),
    ('appell_rising_real', 'appell_reduced', '0.5;2.5;0;2.5;0.25;0.375'),
    ('appell_rising_complex', 'appell_reduced', '0.5+0.125i;2.5+0.125i;0;2.5+0.125i;0.25+0.125i;0.375'),
    ('appell_rising_eight', 'appell_reduced', '0.5+0.125i;8.5+0.125i;0;8.5+0.125i;0.25+0.125i;0.375'),
    ('appell_rising_noncompact', 'appell_reduced', '0.7+0.1i;2.7+0.1i;0;2.7+0.1i;0.25+0.125i;0.375'),
    ('appell_rising_negative', 'appell_reduced', '-0.5+0.125i;2.5+0.125i;0;2.5+0.125i;0.25+0.125i;0.375'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, input) AS (VALUES
    ('gamma_split_real', '2.5;1.5'),
    ('gamma_split_complex', '2.5+0.125i;1.5+0.25i'),
    ('gamma_split_conjugate', '2.5-0.125i;1.5-0.25i'),
    ('gamma_split_alternating', '2.5;-1.5+0.25i'),
    ('gamma_split_order_minimum', '0.125+0.125i;1.5+0.25i'),
    ('gamma_split_order_below', '0.0625+0.125i;1.5+0.25i'),
    ('gamma_split_order_maximum', '16+0.125i;7.5+0.125i'),
    ('gamma_split_order_above', '16.125+0.125i;7.5+0.125i'),
    ('gamma_split_imaginary_maximum', '2.5+1i;1.5+0.25i'),
    ('gamma_split_imaginary_above', '2.5+1.125i;1.5+0.25i'),
    ('gamma_split_argument_maximum', '2.5;8'),
    ('gamma_split_argument_above', '2.5;8.125'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'gamma_lower_reference', input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, input) AS (VALUES
    ('polylog_blocks_real', '2;0.75'),
    ('polylog_blocks_negative', '2;-0.75'),
    ('polylog_blocks_complex', '2;0.5+0.125i'),
    ('polylog_blocks_conjugate', '2;-0.5-0.125i'),
    ('polylog_blocks_small', '2;0.0009765625+0.000244140625i'),
    ('polylog_blocks_near_boundary', '2;0.875+0.125i'),
    ('polylog_blocks_fallback', '2;0.875+0.25i'),
    ('polylog_blocks_order_three', '3;0.5+0.125i'),
    ('polylog_blocks_real_order_three', '3;0.75'),
    ('polylog_blocks_order_seven', '7;-0.5+0.125i'),
    ('polylog_blocks_order_limit', '32;0.5+0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'polylog_reference', input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(300),(1234)), orders(value) AS (VALUES(3),(64),(65))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT 'polygamma_half_coefficient_' || orders.value || '_' || digits,
       'polygamma_half', orders.value, '0', digits FROM orders CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('trigamma_reflection_centre', 'trigamma_reflection', '0.5+0.125i'),
    ('trigamma_reflection_shifted', 'trigamma_reflection', '0.75+0.125i'),
    ('tetragamma_reflection_centre', 'tetragamma_reflection', '0.5+0.125i'),
    ('tetragamma_reflection_shifted', 'tetragamma_reflection', '0.75+0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(300),(1234)),
orders(name, value) AS (VALUES('negative_eight','-8'),('negative_four','-4'),('zero','0'),('two','2'),('eight','8')),
offsets(name, value) AS (VALUES('real','0.75'),('complex','0.75+0.125i'),('conjugate','0.75-0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT 'zatahp_integer_reference_' || orders.name || '_' || offsets.name || '_' || digits,
       'zatahp_reference', orders.value || ';' || offsets.value, '0', digits
FROM orders CROSS JOIN offsets CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('erfinv_reflected', 'erfinv', '-0.875'),
    ('erfinv_conjugate', 'erfinv', '0.5-0.125i'),
    ('erfinv_imaginary', 'erfinv', '0.125i'),
    ('erfinv_near_endpoint', 'erfinv', '0.999999999999999999999999999999'),
    ('erfinv_near_negative_endpoint', 'erfinv', '-0.999999999999999999999999999999'),
    ('erfinv_near_complex_endpoint', 'erfinv', '0.999999999999999999999999999999+1e-31i'),
    ('erfcinv_reflected', 'erfcinv', '1.875'),
    ('erfcinv_conjugate', 'erfcinv', '0.5-0.125i'),
    ('erfcinv_near_two', 'erfcinv', '1.999999999999999999999999999999'),
    ('erfcinv_near_two_complex', 'erfcinv', '1.999999999999999999999999999999-1e-31i'),
    ('erfcinv_complex_tail', 'erfcinv', '1e-40+1.25e-41i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, input, digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
cases(name, input) AS (VALUES
    ('lerch_integer_real', '0.3125;-15;0.375'),
    ('lerch_integer_complex', '0.3125+0.125i;-15;0.375+0.125i'),
    ('lerch_integer_conjugate', '0.3125-0.125i;-15;0.375-0.125i'),
    ('lerch_integer_alternating', '-0.5+0.125i;-8;0.375+0.125i'),
    ('lerch_integer_upper_offset', '0.3125+0.125i;-15;-15.75+0.125i'),
    ('lerch_integer_lower_offset', '0.3125-0.125i;-15;-15.75-0.125i'),
    ('lerch_integer_zero_order', '0.3125+0.125i;0;0.375+0.125i'),
    ('lerch_integer_linear', '0.3125+0.125i;-1;0.375+0.125i'),
    ('lerch_integer_wide_input', '0.3+0.1i;-15;0.7+0.2i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'lerch_integer_reference', input, '0', digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('lerch_integer_maximum_order', 'lerch_integer_reference', '0.3125+0.125i;-64;0.375+0.125i', '0', 300),
    ('lerch_integer_maximum_radius', 'lerch_integer_reference', '0.9375;-64;0.375', '0', 80),
    ('lerch_integer_long_binary_denominator', 'lerch_integer_reference', '0.3125+5.42101086242752217003726400434970855712890625e-20i;-15;0.375', '0', 300),
    ('lerch_integer_general_small_component', 'lerch_integer_reference', '0.3125+8.2718061255302767487140869206996285356581211090087890625e-25i;-15;0.375', '0', 300),
    ('lerch_integer_exact_300', 'lerch_integer_exact', '15', '0', 300),
    ('lerch_integer_exact_1234', 'lerch_integer_exact', '15', '0', 1234),
    ('lerch_integer_reject_degree', 'lerch_integer_reject', '0.3125;0.375;65', '0', 300),
    ('lerch_integer_reject_radius', 'lerch_integer_reject', '0.96875;0.375;15', '0', 300),
    ('lerch_integer_reject_offset', 'lerch_integer_reject', '0.3125;17;15', '0', 300),
    ('lerch_integer_reject_zero', 'lerch_integer_reject', '0;0;1', '0', 300),
    ('lerch_integer_reject_cancellation', 'lerch_integer_reject', '-0.5;1/3;1', '0', 300),
    ('lerch_integer_fallback_radius', 'lerch_reference', '0.96875;-3;0.375', '0', 80),
    ('lerch_integer_fallback_order', 'lerch_reference', '0.3125;-65;0.375', '0', 300);

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('lerch_integer_fallback_complex_short', 'lerch_reference', '0.3125+0.125i;-65;0.375+0.125i', '0', 34),
    ('lerch_integer_fallback_complex_radius', 'lerch_reference', '0.96875+0.00390625i;-3;0.375+0.125i', '0', 80),
    ('lerch_integer_fallback_offset', 'lerch_reference', '0.3125;-15;17', '0', 300),
    ('lerch_integer_fallback_complex_offset', 'lerch_reference', '0.3125+0.125i;-15;17+0.125i', '0', 300);

WITH precisions(digits) AS (VALUES(310),(1234)),
cases(name, input) AS (VALUES
    ('zeta_tail_real', '2.25;576.375'),
    ('zeta_tail_complex', '2.25+0.125i;576.375+0.125i'),
    ('zeta_tail_conjugate', '2.25-0.125i;576.375-0.125i'),
    ('zeta_tail_integer', '2;576.375'),
    ('zeta_tail_zero', '0;576.375'),
    ('zeta_tail_negative_order', '-0.25+0.125i;576.375+0.125i'),
    ('zeta_tail_large_order', '15+1i;576.375+0.125i'),
    ('zeta_tail_imaginary_order', '2+10i;576.375+0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'zeta_tail_reference', input, '0', digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('zeta_tail_maximum_precision', 'zeta_tail_reference', '2.25+0.125i;1200.375+0.125i', '0', 2465),
    ('zeta_tail_reject_precision', 'zeta_tail_reject', '2.25;576.375', '0', 80),
    ('zeta_tail_reject_order', 'zeta_tail_reject', '17;576.375', '0', 310),
    ('zeta_tail_reject_endpoint', 'zeta_tail_reject', '2.25;32.375', '0', 310),
    ('zeta_tail_reject_sector', 'zeta_tail_reject', '2.25;576.375+256i', '0', 310),
    ('zeta_tail_reject_state', 'zeta_tail_reject', '-8;576.375', '0', 310),
    ('zeta_tail_reject_pole', 'zeta_tail_reject', '1;576.375', '0', 310),
    ('zeta_tail_budget_exhausted', 'zeta_tail_budget', '2.25+0.125i;576.375+0.125i', '0', 1234),
    ('zeta_blocks_late_zero', 'zeta_blocks', '-15;1.375;159', '0', 1234),
    ('zeta_blocks_early_zero', 'zeta_blocks', '-1;1.375;159', '0', 1234),
    ('lerch_blocks_large_order_1234', 'lerch_blocks_reference', '0.3125+0.125i;15+0.25i;0.375+0.125i', '0', 1234),
    ('lerch_blocks_negative_order_1234', 'lerch_blocks_reference', '0.3125+0.125i;-15+0.25i;0.375+0.125i', '0', 1234),
    ('lerch_blocks_minimum_precision', 'lerch_blocks_reference', '-0.625+0.125i;0.75+0.125i;0.75-0.125i', '0', 155),
    ('lerch_blocks_maximum_precision', 'lerch_blocks_reference', '0.3125+0.125i;2+0.125i;1.375+0.125i', '0', 2465),
    ('lerch_blocks_negative_maximum_precision', 'lerch_blocks_reference', '0.3125-0.125i;-15-0.25i;0.375-0.125i', '0', 2465);

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('lerch_real_negative_integer_1234', 'lerch_reference', '0.3125;-15;0.375', '0', 1234);

WITH precisions(digits) AS (VALUES(310),(1234)),
cases(name, input) AS (VALUES
    ('zeta_blocks_real', '2.25;1.375;159'),
    ('zeta_blocks_count', '2.25;1.375;160'),
    ('zeta_blocks_complex', '2.25+0.125i;1.375+0.125i;159'),
    ('zeta_blocks_conjugate', '2.25-0.125i;1.375-0.125i;159'),
    ('zeta_blocks_negative_integer', '-2;1.375;159'),
    ('zeta_blocks_negative_order', '-2.25+0.125i;1.375+0.125i;159'),
    ('zeta_blocks_zero_order', '0;1.375;159'),
    ('zeta_blocks_large_order', '15+4i;0.25+0.125i;159'),
    ('zeta_blocks_imaginary_offset', '2.25+0.125i;0.25+15i;159'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'zeta_blocks', input, '0', digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('zeta_blocks_minimum_count', 'zeta_blocks', '2.25+0.125i;1.375+0.125i;64', '0', 310),
    ('zeta_blocks_maximum_count', 'zeta_blocks', '2.25+0.125i;1.375+0.125i;4096', '0', 310),
    ('zeta_blocks_reject_precision', 'zeta_blocks_reject', '2.25;1.375;128', '0', 64),
    ('zeta_blocks_reject_offset', 'zeta_blocks_reject', '2.25;0.125;128', '0', 310),
    ('zeta_blocks_reject_order', 'zeta_blocks_reject', '17;1.375;128', '0', 310),
    ('zeta_blocks_reject_short', 'zeta_blocks_reject', '2.25;1.375;32', '0', 310),
    ('zeta_blocks_reject_long', 'zeta_blocks_reject', '2.25;1.375;4097', '0', 310);

WITH precisions(digits) AS (VALUES(64),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('paired_zeta_real', 'zeta_pair', '2.25'),
    ('paired_zeta_complex', 'zeta_pair', '2.25+0.125i'),
    ('paired_zeta_negative', 'zeta_pair', '-2.25'),
    ('paired_zeta_near_pole', 'zeta_pair', '1.0009765625'),
    ('gamma_log_turns_upper', 'lgamma_turns', '0.75+8.125i'),
    ('gamma_log_turns_lower', 'lgamma_turns', '-2.75-8.125i'),
    ('lerch_real_integer', 'lerch_reference', '0.5;2;0.75'),
    ('lerch_real_fractional', 'lerch_reference', '0.5;1.25;0.75'),
    ('lerch_real_alternating', 'lerch_reference', '-0.5;1.25;0.75'),
    ('lerch_real_negative_order', 'lerch_reference', '-0.5;-3.5;0.75'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, '0', digits FROM cases CROSS JOIN precisions;

WITH precisions(digits) AS (VALUES(64),(300)),
cases(name, input) AS (VALUES
    ('lerch_reference_complex', '0.5+0.125i;1.25+0.125i;0.75+0.125i'),
    ('lerch_reference_conjugate', '0.5-0.125i;1.25-0.125i;0.75-0.125i'),
    ('lerch_reference_negative_order', '0.5+0.125i;-3.5+0.125i;0.75+0.125i'),
    ('lerch_reference_alternating', '-0.5+0.125i;1.25+0.125i;0.75+0.125i'),
    ('lerch_reference_lower_cut', '0.5+0.125i;1.25+0.125i;-0.75-0.125i'),
    ('lerch_reference_precision_retry', '0.5+0.125i;-100.5+0.125i;0.75+0.125i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'lerch_reference', input, '0', digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('lerch_reference_complex_1234', 'lerch_reference', '0.5+0.125i;1.25+0.125i;0.75+0.125i', '0', 1234);

WITH precisions(digits) AS (VALUES(64),(300),(1234)),
cases(name, input) AS (VALUES
    ('gamma_inverse_conjugate', '9.5-0.125i'),
    ('gamma_inverse_shifted', '4+0.5i'))
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'gamma_inverse_complex', input, '0', digits FROM cases CROSS JOIN precisions;
-- Each precision is decimal significant digits; reference calculations and checks remain in C.
CREATE TABLE IF NOT EXISTS number_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS number_cases_operation ON number_cases(operation, name);

-- Independent full-width references cover both hypergeometric kernels and their ordinary-series fallbacks.
WITH precisions(digits) AS (VALUES(80),(180),(300),(1234)), cases(name,operation,input) AS (VALUES
    ('exponential_real','pfq_0f0_reference','0.7'),
    ('exponential_complex','pfq_0f0_reference','0.7+0.125i'),
    ('exponential_outside_reduction','pfq_0f0_reference','8.125'),
    ('binomial_real','pfq_1f0_reference','0.7;0.3'),
    ('binomial_complex','pfq_1f0_reference','0.7+0.125i;0.3-0.125i'),
    ('binomial_negative_argument','pfq_1f0_reference','1.25+0.125i;-0.625'),
    ('kummer_real','pfq_1f1_reference','0.7;2.5;0.3'),
    ('kummer_complex','pfq_1f1_reference','0.7+0.125i;2.5+0.125i;0.3+0.125i'),
    ('kummer_real_lower','pfq_1f1_reference','0.7+0.125i;2.5;0.3+0.125i'),
    ('kummer_conjugate','pfq_1f1_reference','0.7-0.125i;2.5-0.125i;0.3-0.125i'),
    ('kummer_negative_argument','pfq_1f1_reference','0.7+0.125i;2.5+0.125i;-3+0.125i'),
    ('kummer_boundary','pfq_1f1_reference','0.125+1i;16+1i;8'),
    ('kummer_cancellation','pfq_1f1_reference','16+1i;0.125-1i;-8'),
    ('kummer_outside_argument','pfq_1f1_reference','0.7+0.125i;2.5+0.125i;8.125'),
    ('kummer_negative_upper','pfq_1f1_reference','-0.25+0.125i;2.5;0.3'),
    ('kummer_negative_lower','pfq_1f1_reference','0.7;-0.25+0.125i;0.3'),
    ('kummer_zero_upper','pfq_1f1_reference','0;2.5;0.3'),
    ('kummer_zero_argument','pfq_1f1_reference','0.7;2.5;0'),
    ('gauss_real','pfq_2f1_reference','0.7;1.25;2.5;0.3'),
    ('gauss_complex','pfq_2f1_reference','0.7+0.125i;1.25-0.125i;2.5+0.125i;0.3+0.125i'),
    ('gauss_split_boundary','pfq_2f1_reference','0.7;1.25;2.5;0.625'),
    ('gauss_outside_split','pfq_2f1_reference','0.7;1.25;2.5;0.75'),
    ('bessel_entire','pfq_0f1_reference','1.25+0.125i;0.7-0.125i'),
    ('balanced_entire','pfq_2f2_reference','0.7+0.125i;1.25;2.5-0.125i;3.5;0.3+0.125i'),
    ('cubic_balanced','pfq_3f2_reference','0.7;1.25+0.125i;1.5;2.5;3.5-0.125i;0.3'),
    ('lower_zero_pole','pfq_pole','0.7;0;0.3'),
    ('lower_negative_pole','pfq_pole','0.7;-2;0.3'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'pfq_split_' || name || '_' || digits, operation, input, '0', digits
FROM precisions CROSS JOIN cases;

INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision) VALUES
    ('pfq_split_upper_work_limit','pfq_1f1_reference','0.7+0.125i;2.5+0.125i;0.3+0.125i','0',5000);

-- The split uses a=1/2 and the squared argument; test its working-size and |z|^2=8 boundaries independently.
WITH precisions(digits) AS (VALUES(150),(180),(300),(1234)), operations(value) AS (VALUES('erf'),('erfc')),
arguments(name,input) AS (VALUES
    ('tiny','0.0009765625+0.0009765625i'),
    ('reflected','-0.75-0.125i'),
    ('conjugate','0.75-0.125i'),
    ('imaginary','2.75i'),
    ('boundary','2+2i'),
    ('outside','2+2.125i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'error_split_' || value || '_' || name || '_' || digits, value, input, '0', digits
FROM precisions CROSS JOIN operations CROSS JOIN arguments;

WITH precisions(digits) AS (VALUES(80),(300),(1234)),
operations(value) AS (VALUES('erfinv_reference'),('erfcinv_reference')),
arguments(name,input) AS (VALUES('moderate','0.5+0.125i'),('reflected','-0.75+0.125i'),
    ('endpoint','0.875+0.03125i'),('conjugate','0.5-0.125i'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'error_split_' || value || '_' || name || '_' || digits, value, input, '0', digits
FROM precisions CROSS JOIN operations CROSS JOIN arguments;

WITH precisions(digits) AS (VALUES(80),(300),(1234)), arguments(name,input) AS (VALUES
    ('real_left','0.3;0.7+0.125i'),
    ('real_right','0.7+0.125i;0.3'),
    ('both_real','0.3;-0.7'),
    ('zero_scalar','0;0.7-0.125i'),
    ('tiny_component','0.7+1e-200i;-0.3'))
INSERT OR IGNORE INTO number_cases(name,operation,input,expected,precision)
SELECT 'guarded_scalar_' || name || '_' || digits, 'guarded_scalar_product', input, '0', digits
FROM precisions CROSS JOIN arguments;

WITH precisions(digits) AS (VALUES(34),(80),(300),(1234)),
cases(name, operation, input, expected) AS (VALUES
    ('erf_complex', 'erf', '0.5+0.125i', '0'),
    ('erf_reflected', 'erf', '-0.5+0.125i', '0'),
    ('erf_imaginary', 'erf', '2i', '0'),
    ('erf_cancellation', 'erf', '4+0.125i', '0'),
    ('erfc_complex', 'erfc', '0.5+0.125i', '0'),
    ('erfinv_real', 'erfinv', '0.5', '0.5'),
    ('erfinv_complex', 'erfinv', '0.5+0.125i', '0.5+0.125i'),
    ('erfcinv_real', 'erfcinv', '0.5', '0.5'),
    ('erfcinv_tiny', 'erfcinv', '1e-100', '1e-100'),
    ('erfcinv_complex', 'erfcinv', '0.5+0.125i', '0.5+0.125i'),
    ('bessel_complex', 'bessel_half', '0.75+0.125i', '0'),
    ('bessel_negative_bank', 'bessel_half', '-0.75+0.125i', '0'),
    ('bessel_k_wronskian_zero', 'bessel_k', '0;0.75+0.125i', '0'),
    ('bessel_k_wronskian_integer', 'bessel_k', '3;0.75', '0'),
    ('bessel_k_wronskian_upper', 'bessel_k', '2;-0.75+0.125i', '0'),
    ('bessel_k_wronskian_lower', 'bessel_k', '2;-0.75-0.125i', '0'),
    ('bessel_k_wronskian_large', 'bessel_k', '2;15+0.125i', '0'),
    ('chi_complex', 'chi', '2;0.75+0.125i', '0'),
    ('chi_real', 'chi', '2;0.75', '0'),
    ('chi_real_negative', 'chi', '5;-0.75', '0'),
    ('chi_negative', 'chi', '5;-0.5+0.125i', '0'),
    ('chi_near_cutoff', 'chi', '2;0.94+0.02i', '0'),
    ('chi_continuation', 'chi', '2;0.97+0.01i', '0'),
    ('chi_large_order', 'chi_limit', '10000;0.5', '0.5'),
    ('dilog_diagonal', 'dilog_diagonal', '0.5+0.5i', '0'),
    ('polylog_trilog_half', 'polylog_half', '3', '0'),
    ('polylog_large_order', 'polylog_limit', '10000;0.5', '0.5'),
    ('gammainv_positive', 'gammainv', '9.5', '0'),
    ('gammainv_lower_branch', 'gammainv', '0.9', '0'),
    ('gammainv_near_minimum', 'gammainv', '0.885603194411', '0'),
    ('gammainv_exact', 'gammainv', '1', '0'),
    ('gammainv_large', 'gammainv', '1e1000', '0'),
    ('lommel_identity', 'lommel', '0.75+0.125i', '0'),
    ('lommel_general', 'lommel_recurrence', '0.5+0.125i;0.75+0.125i;1.25+0.125i', '0'),
    ('appell_product_real', 'appell_product', '0.5;0.75;1.25;0.5;0.25;0.375', '0'),
    ('appell_product_complex', 'appell_product',
        '0.5+0.125i;0.75+0.125i;1.25+0.125i;0.5+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_log_real', 'appell_log', '1;1;1;2;0.25;0.375', '0'),
    ('appell_log_complex', 'appell_log', '1;1;1;2;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_product_full_real', 'appell_product', '0.7;0.75;1.25;0.7;-0.25;0.375', '0'),
    ('appell_product_full_complex', 'appell_product',
        '0.7+0.1i;0.75+0.125i;1.25+0.125i;0.7+0.1i;-0.25+0.125i;0.375-0.125i', '0'),
    ('appell_product_negative_complex', 'appell_product',
        '-0.5+0.125i;0.75+0.125i;1.25+0.125i;-0.5+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_product_large_dyadic', 'appell_product', '65536.125;0.75;1.25;65536.125;0.25;0.375', '0'),
    ('appell_product_wide_components', 'appell_product',
        '1e20+0.125i;0.75+0.125i;1.25+0.125i;1e20+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_log_near_edge', 'appell_log', '1;1;1;2;0.75;0.9375', '0'),
    ('appell_log_cancellation', 'appell_log', '1;1;1;2;-0.5+0.125i;0.5-0.125i', '0'),
    ('appell_linear_real', 'appell_linear', '-1;0.75;1.25;-2.5;0.25;0.375', '0'),
    ('appell_linear_complex', 'appell_linear',
        '-1;0.75+0.125i;1.25+0.125i;-2.5+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_quadratic_real', 'appell_quadratic', '-2;0.75;1.25;-2.5;0.25;0.375', '0'),
    ('appell_quadratic_complex', 'appell_quadratic',
        '-2;0.75+0.125i;1.25+0.125i;-2.5+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_pole', 'appell_nan', '1;0.75;1.25;-2;0.25;0.375', '0'),
    ('appell_domain_real', 'appell_nan', '0.5;0.75;1.25;0.5;1;0.375', '0'),
    ('appell_domain_complex', 'appell_nan', '0.5;0.75;1.25;0.5;0.9+0.5i;0.375', '0'),
    ('normal_cdf_complex', 'normal_cdf', '0.75+0.125i', '0'),
    ('normal_pdf_complex', 'normal_pdf', '0.75+0.125i', '0'),
    ('normal_logpdf_complex', 'normal_logpdf', '0.75+0.125i', '0'),
    ('normal_tail_real', 'normal_tail', '-12', '1'),
    ('normal_tail_complex', 'normal_tail', '-12+0.125i', '1'),
    ('digamma_shift', 'digamma', '2.25+0.125i', '0'),
    ('trigamma_shift', 'trigamma', '2.25+0.125i', '0'),
    ('tetragamma_shift', 'tetragamma', '2.25+0.125i', '0'),
    ('polygamma_shift', 'polygamma', '2.25+0.125i', '0'),
    ('polygamma_exact', 'polygamma_value', '1', '0'),
    ('polygamma_half_first', 'polygamma_half', '1', '0'),
    ('polygamma_half_second', 'polygamma_half', '2', '0'),
    ('polygamma_half_tenth', 'polygamma_half', '10', '0'),
    ('clausen_catalan', 'clausen_catalan', '0.5', '0'),
    ('clausen_odd', 'clausen_odd', '3', '0'),
    ('zeta_even', 'zeta', '2', '0'),
    ('zeta_mpfr_small', 'zeta_reference', '0.75', '0'),
    ('zeta_mpfr_regular', 'zeta_reference', '2.25', '0'),
    ('zeta_mpfr_near_pole_left', 'zeta_reference', '0.999999999068677425384521484375', '0'),
    ('zeta_mpfr_near_pole_right', 'zeta_reference', '1.000000000931322574615478515625', '0'),
    ('zeta_mpfr_upper', 'zeta_reference', '63.5', '0'),
    ('zeta_mpfr_fallback', 'zeta_reference', '64.5', '0'),
    ('zeta_mpfr_negative', 'zeta_reference', '-2.25', '0'),
    ('zeta_functional_positive', 'zeta_reflection', '0.75+2i', '0'),
    ('zeta_functional_negative', 'zeta_reflection', '-2.25+4i', '0'),
    ('zetap_functional_positive', 'zetap_reflection', '0.75+2i', '0'),
    ('zetap_functional_negative', 'zetap_reflection', '-2.25+4i', '0'),
    ('zetap_zero', 'zetap', '0', '0'),
    ('zetah_shift_real', 'zetah', '2.25;0.75', '0'),
    ('zetah_shift_complex', 'zetah', '2.25+0.125i;0.75+0.125i', '0'),
    ('zatahp_shift_real', 'zatahp', '2.25;0.75', '0'),
    ('zatahp_shift_complex', 'zatahp', '2.25+0.125i;0.75+0.125i', '0'),
    ('zatahp_at_zero', 'zatahp_zero', '0.75', '0'),
    ('zatahp_negative_integer', 'zatahp', '-4;0.75+0.125i', '0'),
    ('zatahp_later_termination', 'zatahp', '-8;0.75+0.125i', '0'),
    ('zatahp_high_imaginary', 'zatahp', '0.75+64i;0.75+0.125i', '0'),
    ('zetah_zero_order', 'zetah_value', '0;0.75+0.125i', '-0.25-0.125i'),
    ('zetah_negative_order', 'zetah_negative_one', '0.75+0.125i', '0'),
    ('zetah_trivial_zero', 'zetah_value', '-8;1', '0'),
    ('zetah_later_termination', 'zetah', '-8;0.75+0.125i', '0'),
    ('zetah_negative_offset_upper', 'zetah', '2.25+0.125i;-2.75+0.125i', '0'),
    ('zetah_negative_offset_lower', 'zetah', '2.25+0.125i;-2.75-0.125i', '0')
)
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, expected, digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('zeta_mpfr_above_cache_range', 'zeta_reference', '2.25', '0', 2500),
    ('qc_erf_complex', 'qc_erf', '0.5+0.125i', '0', 34),
    ('qc_erfcinv_complex', 'qc_erfcinv', '0.5+0.125i', '0.5+0.125i', 34),
    ('qc_bessel_complex', 'qc_bessel_half', '0.75+0.125i', '0', 34),
    ('qc_lommel_complex', 'qc_lommel', '0.75+0.125i', '0', 34),
    ('qf_normal_tail', 'qf_normal_tail', '-12', '1', 34);

WITH precisions(digits) AS (VALUES (64), (300), (1234)),
cases(name, input) AS (VALUES
    ('zetah_reference_unit', '2.25;1'),
    ('zetah_reference_half', '0.75;0.5'),
    ('zetah_reference_near_pole', '1.000000000931322574615478515625;0.5'),
    ('zetah_reference_negative', '-2.25;0.5')
)
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, 'zetah_reference', input, '0', digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('double_zeta_positive', 'double_zeta', '2.25', '0', 19),
    ('double_zeta_negative', 'double_zeta', '-2.25', '0', 19),
    ('double_zeta_near_pole', 'double_zeta', '1.125', '0', 18),
    ('double_zetap_positive', 'double_zetap', '2.25', '0', 19),
    ('double_zetap_negative', 'double_zetap', '-2.25', '0', 19),
    ('double_zetah', 'double_zetah', '2.25;0.75', '0', 19),
    ('double_zatahp', 'double_zatahp', '2.25;0.75', '0', 19),
    ('double_clausen_small', 'double_clausen', '0.75', '0', 19),
    ('double_clausen_reflected', 'double_clausen', '2.5', '0', 19);

WITH precisions(digits) AS (VALUES(34),(80),(300),(1234)),
cases(name, operation, input) AS (VALUES
    ('reciprocal_asin_upper', 'acosec', '2.5+0.125i'),
    ('reciprocal_asin_lower', 'acosec', '-0.5-0.125i'),
    ('reciprocal_asin_imaginary', 'acosec', '0.125i'),
    ('reciprocal_asin_small_component', 'acosec', '0.5+1e-80i'),
    ('reciprocal_acos_upper', 'asec', '2.5+0.125i'),
    ('reciprocal_acos_lower', 'asec', '-0.5-0.125i'),
    ('reciprocal_acos_imaginary', 'asec', '-0.125i'),
    ('reciprocal_acos_small_component', 'asec', '0.5-1e-80i'),
    ('gamma_native_recurrence', 'gamma_recurrence', '0.75+0.125i'),
    ('gamma_native_reflection', 'gamma_reflection', '-2.75+0.125i'),
    ('gamma_native_turns', 'gamma_recurrence', '0.75+8.125i'),
    ('gamma_native_inverse', 'gamma_inverse_complex', '9.5+0.125i'),
    ('gamma_native_inverse_lower', 'gamma_inverse_complex', '0.75-0.125i'),
    ('gamma_native_lower', 'gamma_lower_one', '0.75+0.125i'),
    ('gamma_native_upper', 'gamma_upper_one', '0.75+0.125i'),
    ('gamma_native_upper_large', 'gamma_upper_one', '30+0.125i'),
    ('gamma_native_regularised', 'gamma_regularised', '2.5+0.125i;0.75+0.125i'),
    ('gamma_native_recurrence_upper', 'gamma_upper_recurrence', '2.5+0.125i;0.75+0.125i'),
    ('gamma_fast_recurrence_upper', 'gamma_upper_recurrence', '2.3;1.4'),
    ('gamma_fast_regularised', 'gamma_regularised', '2.3;1.4'),
    ('hypergeometric_native_real', 'hypergeometric_exp', '0.75'),
    ('hypergeometric_exact_argument', 'hypergeometric_exact', '1'),
    ('hypergeometric_native_complex', 'hypergeometric_exp', '0.75+0.125i'),
    ('hypergeometric_native_binomial', 'hypergeometric_binomial', '1.25+0.125i;0.5+0.125i'),
    ('hypergeometric_native_polynomial', 'hypergeometric_terminating', '1.25+0.125i;2+0.125i'),
    ('hypergeometric_native_polynomial_zero', 'hypergeometric_terminating', '1.25+0.125i;1'),
    ('lerch_native_log', 'lerch_log', '0.5+0.125i'),
    ('lerch_exact_argument', 'lerch_exact', '2'),
    ('lerch_native_real', 'lerch_shift', '0.75;2;1.25'),
    ('lerch_native_complex', 'lerch_shift', '0.5+0.125i;1.25+0.125i;0.75+0.125i')
)
INSERT OR REPLACE INTO number_cases(name, operation, input, expected, precision)
SELECT cases.name || '_' || precisions.digits, operation, input, '0', precisions.digits
FROM cases CROSS JOIN precisions;

INSERT OR REPLACE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('inverse_erf_double_small', 'double_erfinv', '0.00001', '0', 20),
    ('inverse_erf_double', 'double_erfinv', '0.5', '0', 20),
    ('inverse_erf_double_negative', 'double_erfinv', '-0.875', '0', 20),
    ('inverse_erfc_double', 'double_erfcinv', '0.5', '0', 20),
    ('inverse_erfc_double_reflected', 'double_erfcinv', '1.875', '0', 20);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'number.benchmark_regressions', 'tests/number/test_number_special_regressions.c',
    'tests.run_number_benchmark_regressions', 'number_cases', NULL)
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;

CREATE TABLE IF NOT EXISTS number_conversion_cases (
    name TEXT PRIMARY KEY, operation TEXT NOT NULL, input TEXT NOT NULL, expected TEXT NOT NULL,
    precision INTEGER NOT NULL DEFAULT 32 CHECK (precision BETWEEN 1 AND 10000),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
) WITHOUT ROWID;

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'number.real_complex_conversion', 'tests/number/test_number_backend_parity.c',
    'tests.run_number_real_complex_conversion_tests', 'number_conversion_cases', 'real_complex_conversion')
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
INSERT OR IGNORE INTO number_conversion_cases(name, operation, input, expected) VALUES
    ('complex_real_conversion_zero', 'real_complex_conversion', '0', '0'),
    ('complex_real_conversion_positive', 'real_complex_conversion', '0.125', '0.125'),
    ('complex_real_conversion_negative', 'real_complex_conversion', '-3.5', '-3.5'),
    ('complex_real_conversion_nonreal', 'real_complex_conversion', '0.125+0.25i', 'NAN'),
    ('complex_real_conversion_imaginary', 'real_complex_conversion', 'i', 'NAN');

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('lerch_blocks_complex_300', 'lerch_blocks_reference', '0.3125+0.125i;2+0.125i;1.375+0.125i', '0', 300),
    ('lerch_blocks_complex_1234', 'lerch_blocks_reference', '0.3125+0.125i;2+0.125i;1.375+0.125i', '0', 1234),
    ('lerch_blocks_conjugate_300', 'lerch_blocks_reference', '0.3125-0.125i;2-0.125i;1.375-0.125i', '0', 300),
    ('lerch_blocks_alternating_1234', 'lerch_blocks_reference', '-0.625+0.125i;0.75+0.125i;0.75-0.125i', '0', 1234),
    ('lerch_blocks_negative_order_300', 'lerch_blocks_reference', '0.5+0.125i;-3.5+0.125i;0.75+0.125i', '0', 300),
    ('lerch_blocks_imaginary_300', 'lerch_blocks_reference', '0.5i;1.25+0.125i;0.75-0.125i', '0', 300),
    ('lerch_blocks_upper_offset_300', 'lerch_blocks_reference', '0.5+0.125i;1.5+0.125i;-15.75+0.125i', '0', 300),
    ('lerch_blocks_lower_offset_300', 'lerch_blocks_reference', '0.5+0.125i;1.5+0.125i;-15.75-0.125i', '0', 300),
    ('lerch_blocks_near_unit_300', 'lerch_blocks_reference', '0.875+0.03125i;2.25+0.125i;1.125+0.125i', '0', 300),
    ('lerch_blocks_reject_precision', 'lerch_blocks_reject', '0.5;1.5;0.75', '0', 64),
    ('lerch_blocks_reject_radius', 'lerch_blocks_reject', '0.9375;1.5;0.75', '0', 300),
    ('lerch_blocks_reject_order', 'lerch_blocks_reject', '0.5;-100.5+0.125i;0.75', '0', 300),
    ('lerch_blocks_reject_offset', 'lerch_blocks_reject', '0.5;1.5;32.25+0.125i', '0', 300);
