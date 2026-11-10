-- Precision regressions for previously failing or timed-out benchmark functions.
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
    ('lommel_identity', 'lommel', '0.75+0.125i', '0'),
    ('lommel_general', 'lommel_recurrence', '0.5+0.125i;0.75+0.125i;1.25+0.125i', '0'),
    ('appell_product_real', 'appell_product', '0.5;0.75;1.25;0.5;0.25;0.375', '0'),
    ('appell_product_complex', 'appell_product',
        '0.5+0.125i;0.75+0.125i;1.25+0.125i;0.5+0.125i;0.25+0.125i;0.375+0.125i', '0'),
    ('appell_log_real', 'appell_log', '1;1;1;2;0.25;0.375', '0'),
    ('appell_log_complex', 'appell_log', '1;1;1;2;0.25+0.125i;0.375+0.125i', '0'),
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
    ('zeta_even', 'zeta', '2', '0'),
    ('zetap_zero', 'zetap', '0', '0'),
    ('zetah_shift_real', 'zetah', '2.25;0.75', '0'),
    ('zetah_shift_complex', 'zetah', '2.25+0.125i;0.75+0.125i', '0'),
    ('zatahp_shift_real', 'zatahp', '2.25;0.75', '0'),
    ('zatahp_shift_complex', 'zatahp', '2.25+0.125i;0.75+0.125i', '0')
)
INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision)
SELECT name || '_' || digits, operation, input, expected, digits FROM cases CROSS JOIN precisions;

INSERT OR IGNORE INTO number_cases(name, operation, input, expected, precision) VALUES
    ('qc_erf_complex', 'qc_erf', '0.5+0.125i', '0', 34),
    ('qc_erfcinv_complex', 'qc_erfcinv', '0.5+0.125i', '0.5+0.125i', 34),
    ('qc_bessel_complex', 'qc_bessel_half', '0.75+0.125i', '0', 34),
    ('qc_lommel_complex', 'qc_lommel', '0.75+0.125i', '0', 34),
    ('qf_normal_tail', 'qf_normal_tail', '-12', '1', 34);

INSERT INTO test_case_runner(module, source, parent, table_name, operation) VALUES (
    'number.benchmark_regressions', 'tests/number/test_number_special_regressions.c',
    'tests.run_number_benchmark_regressions', 'number_cases', NULL)
ON CONFLICT(module) DO UPDATE SET table_name=excluded.table_name, operation=excluded.operation;
