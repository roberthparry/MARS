-- Precision regressions for previously failing or timed-out benchmark functions.

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
