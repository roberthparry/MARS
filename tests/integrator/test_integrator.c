/* test_integrator.c — tests for the adaptive integrators */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "test_harness.h"

#include "integrator.h"
#include "expression.h"
#include "internal/number_internal.h"
#include "ustring.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static bool test_integrator_suite_setup(void);
static bool test_assert_integrator_number_close_tol(number_t actual,
                                                    number_t expected,
                                                    const char *tol_text,
                                                    const char *file,
                                                    int line);
static int test_num_printf_compat(const char *fmt, ...);
static void test_begin_integral_display(number_t result, number_t err);
static void test_end_integral_display(void);
static void test_print_number_line(const char *label, number_t value);
static size_t test_precision_expectation_digits(number_t result, number_t err, bool *exact_out);
static void test_print_precision_expectation(number_t result, number_t err);
static void test_print_integral_status(int status, size_t intervals, number_t result, number_t err);
static number_t test_number_for_display(number_t value);
static char *test_format_number(const number_t value, size_t significant_digits);
static int test_emit_number(const number_t value, size_t significant_digits);
static int test_emit_q_line(const char *fmt, number_t value, size_t significant_digits);
static void test_clear_pending_integral_display(void);
static number_t test_num_from_double(double value);
static number_t test_num_mul_double(const number_t number, double value);
static number_t test_num_make_complex(const number_t real, const number_t imag);
TEST_SUITE_SETUP(test_integrator_suite_setup);

static int test_display_sig_digits_override = -1;
static const size_t test_exact_display_sig_digits = 18u;
static const char *test_pending_result_fmt = NULL;
static const char *test_pending_expected_fmt = NULL;
static number_t test_pending_result;
static number_t test_pending_expected;
static bool test_has_pending_result = false;
static bool test_has_pending_expected = false;

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

#define TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(actual_value, expected_value) \
    do { \
        if (!test_assert_integrator_number_close_tol((actual_value), \
                                                     (expected_value), \
                                                     "1e-15", \
                                                     __FILE__, \
                                                     __LINE__)) \
            return; \
    } while (0)

#define TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(actual_value, expected_value, tol_value) \
    do { \
        if (!test_assert_integrator_number_close_tol((actual_value), \
                                                     (expected_value), \
                                                     (tol_value), \
                                                     __FILE__, \
                                                     __LINE__)) \
            return; \
    } while (0)

static bool test_integrator_suite_setup(void)
{
    return true;
}

static bool test_assert_integrator_number_close_tol(number_t actual,
                                                    number_t expected,
                                                    const char *tol_text,
                                                    const char *file,
                                                    int line)
{
    number_t diff = num_sub(actual, expected);
    number_t abs_diff = num_abs(diff);
    number_t tol = num_create_from_string(tol_text);
    string_t *abs_text = NULL;
    string_t *tol_display = NULL;
    bool ok = num_le(abs_diff, tol);

    if (!ok) {
        abs_text = num_to_string(abs_diff);
        tol_display = num_to_string(tol);
        test_set_failure_detailf("expected |actual - expected| <= tolerance, got %s > %s",
                                 abs_text ? string_c_str(abs_text) : "(null)",
                                 tol_display ? string_c_str(tol_display) : "(null)");
        test_mark_failure(file, line, "number tolerance check failed");
    }

    string_free(tol_display);
    string_free(abs_text);
    num_destroy(&tol);
    num_destroy(&abs_diff);
    num_destroy(&diff);
    return ok;
}

static int test_num_printf_compat(const char *fmt, ...)
{
    va_list ap;
    int written = 0;
    bool is_expected_line = strstr(fmt, "expected = ") != NULL;
    bool is_error_line = strstr(fmt, "err      = ") != NULL
                      || strstr(fmt, "error estimate") != NULL;
    bool is_result_line = strstr(fmt, "result   = ") != NULL
                       || (strstr(fmt, "≈ %q") != NULL && !is_error_line);

    if (is_expected_line || is_error_line || is_result_line) {
        va_start(ap, fmt);
        {
            number_t value = va_arg(ap, number_t);
            size_t err_digits = 4u;

            va_end(ap);
            if (is_result_line) {
                if (test_has_pending_result)
                    test_clear_pending_integral_display();
                test_pending_result_fmt = fmt;
                test_pending_result = num_clone(value);
                test_has_pending_result = true;
                return 0;
            }
            if (is_expected_line) {
                if (test_has_pending_expected) {
                    num_destroy(&test_pending_expected);
                    test_has_pending_expected = false;
                }
                test_pending_expected_fmt = fmt;
                test_pending_expected = num_clone(value);
                test_has_pending_expected = true;
                return 0;
            }

            if (test_has_pending_result) {
                test_begin_integral_display(test_pending_result, value);
                written += test_emit_q_line(test_pending_result_fmt,
                                            test_pending_result,
                                            test_display_sig_digits_override > 0
                                                ? (size_t)test_display_sig_digits_override
                                                : 1u);
                if (test_has_pending_expected) {
                    written += test_emit_q_line(test_pending_expected_fmt,
                                                test_pending_expected,
                                                test_display_sig_digits_override > 0
                                                    ? (size_t)test_display_sig_digits_override
                                                    : 1u);
                }
                written += test_emit_q_line(fmt, value, num_is_zero(value) ? 1u : err_digits);
                test_end_integral_display();
                test_clear_pending_integral_display();
                return written;
            }

            return test_emit_q_line(fmt, value, num_is_zero(value) ? 1u : err_digits);
        }
    }

    va_start(ap, fmt);
    for (size_t pos = 0u; fmt[pos] != '\0'; ) {
        if (fmt[pos] == '%' && fmt[pos + 1u] == '%') {
            fputc('%', stdout);
            written += 1;
            pos += 2u;
            continue;
        }

        if (fmt[pos] == '%' && fmt[pos + 1u] == 'q') {
            number_t value = va_arg(ap, number_t);
            number_t display = test_number_for_display(value);
            int rc = num_printf("%N", display);

            if (rc >= 0)
                written += rc;
            num_destroy(&display);
            pos += 2u;
            continue;
        }

        fputc(fmt[pos], stdout);
        written += 1;
        pos += 1u;
    }
    va_end(ap);
    return written;
}

static void test_print_number_line(const char *label, number_t value)
{
    number_t display = test_number_for_display(value);

    printf("  %-8s = ", label);
    num_printf("%N\n", display);
    num_destroy(&display);
}

static int test_emit_number(const number_t value, size_t significant_digits)
{
    char *text = test_format_number(value, significant_digits);
    int rc;

    if (!text)
        return 0;
    rc = printf("%s", text);
    free(text);
    return rc < 0 ? 0 : rc;
}

static int test_emit_q_line(const char *fmt, number_t value, size_t significant_digits)
{
    const char *marker = strstr(fmt, "%q");
    int written = 0;

    if (!marker)
        return printf("%s", fmt);

    written += printf("%.*s", (int)(marker - fmt), fmt);
    written += test_emit_number(value, significant_digits);
    written += printf("%s", marker + 2);
    return written;
}

static void test_begin_integral_display(number_t result, number_t err)
{
    bool exact = false;
    size_t digits = test_precision_expectation_digits(result, err, &exact);

    if (exact)
        digits = test_exact_display_sig_digits < num_get_default_prec_digits()
            ? test_exact_display_sig_digits
            : num_get_default_prec_digits();
    if (digits == 0u)
        digits = 1u;
    test_display_sig_digits_override = (int)digits;
}

static void test_end_integral_display(void)
{
    test_display_sig_digits_override = -1;
}

static void test_clear_pending_integral_display(void)
{
    if (test_has_pending_result) {
        num_destroy(&test_pending_result);
        test_has_pending_result = false;
    }
    if (test_has_pending_expected) {
        num_destroy(&test_pending_expected);
        test_has_pending_expected = false;
    }
    test_pending_result_fmt = NULL;
    test_pending_expected_fmt = NULL;
}

static int test_parse_size_digits(const string_t *text, size_t *out)
{
    const char *digits = text ? string_c_str(text) : NULL;
    size_t i = 0u;
    size_t value = 0u;

    if (!digits || !out || digits[0] == '\0')
        return 0;

    for (; digits[i] != '\0'; ++i) {
        unsigned int digit;

        if (digits[i] < '0' || digits[i] > '9')
            return 0;
        digit = (unsigned int)(digits[i] - '0');
        if (value > (((size_t)-1) - digit) / 10u)
            return 0;
        value = value * 10u + digit;
    }

    *out = value;
    return 1;
}

static size_t test_precision_expectation_digits(number_t result, number_t err, bool *exact_out)
{
    number_t abs_err = num_new();
    size_t digits = 0u;

    if (exact_out)
        *exact_out = false;
    if (!num_is_real(err) || !num_is_finite(err))
        return 0u;

    abs_err = num_abs(err);
    if (num_is_zero(abs_err)) {
        if (exact_out)
            *exact_out = true;
        num_destroy(&abs_err);
        return num_get_default_prec_digits();
    }

    {
        number_t abs_result = num_new();
        number_t scale = num_new();
        number_t metric = num_new();
        number_t neg_log10 = num_new();
        number_t digits_num = num_new();
        string_t *digits_text = NULL;

        abs_result = num_abs(result);
        scale = num_is_zero(abs_result) ? num_clone(NUM_ONE) : abs_result;
        metric = num_div(abs_err, scale);

        if (num_is_real(metric) && num_is_finite(metric) && num_lt(metric, NUM_ONE)) {
            neg_log10 = num_neg(num_log10(metric));
            digits_num = num_floor(neg_log10);
            digits_text = num_to_string(digits_num);
            (void)test_parse_size_digits(digits_text, &digits);
            if (digits > num_get_default_prec_digits())
                digits = num_get_default_prec_digits();
        }

        string_free(digits_text);
        num_destroy(&digits_num);
        num_destroy(&neg_log10);
        num_destroy(&metric);
        num_destroy(&scale);
        num_destroy(&abs_result);
    }

    num_destroy(&abs_err);
    return digits;
}

static void test_print_precision_expectation(number_t result, number_t err)
{
    bool exact = false;
    size_t digits = test_precision_expectation_digits(result, err, &exact);

    if (!num_is_real(err) || !num_is_finite(err)) {
        printf("  precision expectation: unavailable\n");
        return;
    }

    if (exact) {
        printf("  precision expectation: exact on current engine path\n");
        return;
    }

    if (digits == 0u)
        printf("  precision expectation: fewer than 1 significant digit guaranteed\n");
    else
        printf("  precision expectation: about %zu significant digits\n", digits);
}

static void test_print_integral_status(int status, size_t intervals, number_t result, number_t err)
{
    printf("  status = %d  intervals = %zu\n", status, intervals);
    test_print_precision_expectation(result, err);
}

static number_t test_number_for_display(number_t value)
{
    size_t precision_bits;

    if (!num_is_finite(value))
        return num_clone(value);
    precision_bits = num_get_effective_prec_bits(value);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    return num_as_inexact_real_prec(value, precision_bits);
}

static char *test_format_number(const number_t value, size_t significant_digits)
{
    char spec[32];
    number_t display = test_number_for_display(value);
    size_t precision = significant_digits > 0u ? significant_digits - 1u : 0u;
    int needed;
    char *text;
    char *exp_marker;
    char *trim;

    snprintf(spec, sizeof(spec), "%%.%zuN", precision);
    needed = num_sprintf(NULL, 0u, spec, display);
    if (needed < 0) {
        num_destroy(&display);
        return NULL;
    }

    text = malloc((size_t)needed + 1u);
    if (!text) {
        num_destroy(&display);
        return NULL;
    }
    if (num_sprintf(text, (size_t)needed + 1u, spec, display) < 0) {
        free(text);
        num_destroy(&display);
        return NULL;
    }
    num_destroy(&display);

    exp_marker = strchr(text, 'E');
    if (!exp_marker)
        exp_marker = strchr(text, 'e');
    if (!exp_marker)
        return text;

    trim = exp_marker - 1;
    while (trim > text && *trim == '0' && strchr(text, '.') && trim[-1] != 'E' && trim[-1] != 'e') {
        memmove(trim, trim + 1, strlen(trim + 1) + 1u);
        exp_marker -= 1;
        trim -= 1;
    }
    if (trim >= text && *trim == '.')
        memmove(trim, trim + 1, strlen(trim + 1) + 1u);

    return text;
}

static number_t test_num_from_double(double value)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%.17g", value);
    return num_create_from_string(buf);
}

static number_t test_num_mul_double(const number_t number, double value)
{
    number_t factor = test_num_from_double(value);
    number_t out = num_mul(number, factor);

    num_destroy(&factor);
    return out;
}

static number_t test_num_make_complex(const number_t real, const number_t imag)
{
    number_t imag_term = num_mul(NUM_I, imag);
    number_t out = num_add(real, imag_term);

    num_destroy(&imag_term);
    return out;
}

static expr_t *test_expr_new_const_d(double x)
{
    number_t n = test_num_from_double(x);
    expr_t *dv = expr_new_const(n);

    num_destroy(&n);
    return dv;
}

static expr_t *test_expr_new_var_num(number_t x)
{
    number_t n = num_clone(x);
    expr_t *dv = expr_new_var(n);

    num_destroy(&n);
    return dv;
}

static expr_t *test_expr_add_d(const expr_t *dv, double x)
{
    number_t n = test_num_from_double(x);
    expr_t *out = expr_add_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_sub_d(const expr_t *dv, double x)
{
    number_t n = test_num_from_double(x);
    expr_t *out = expr_sub_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_mul_d(const expr_t *dv, double x)
{
    number_t n = test_num_from_double(x);
    expr_t *out = expr_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_pow_d(const expr_t *dv, double x)
{
    number_t n = test_num_from_double(x);
    expr_t *out = expr_pow(dv, &n);

    num_destroy(&n);
    return out;
}

#define expr_add_d test_expr_add_d
#define expr_sub_d test_expr_sub_d
#define expr_mul_d test_expr_mul_d
#define expr_pow_d test_expr_pow_d

/* -----------------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------------- */

void test_create_and_destroy(void) {
    integrator_t *ig = intg_new();
    ASSERT_TRUE(ig);
    intg_free(ig);
    intg_free(NULL);  /* must not crash */
}

void test_polynomial(void) {
    /* ∫₀¹ x² dx = 1/3 — degree-2 polynomial; Turán is exact to full number_t precision */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_mul(x, x);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t expected = num_create_from_string("0.33333333333333333333333333333333333333");
    printf("  ∫₀¹ x² dx\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_single_integral_num_high_precision_log(void) {
    size_t old_bits = num_get_default_prec_bits();
    integrator_t *ig = intg_new();
    expr_t *x = NULL;
    expr_t *expr = NULL;
    number_t one = num_new();
    number_t two = num_new();
    number_t result = num_new();
    number_t err = num_new();
    number_t log_two = num_new();
    number_t expected = num_new();
    int s = -1;

    ASSERT_TRUE(ig);
    if (num_set_default_prec_digits(40u) != 0) {
        test_mark_failure(__FILE__, __LINE__, "could not set high precision for test");
        goto cleanup;
    }

    one = num_create_from_long(1);
    two = num_create_from_long(2);
    x = expr_new_named_var(num_clone(one), "x");
    expr = expr_log(x);
    if (!x || !expr) {
        test_mark_failure(__FILE__, __LINE__, "could not build log(x) expression");
        goto cleanup;
    }

    intg_set_interval_count_max(ig, 5000u);
    s = intg_integral(ig, expr, x, one, two, &result, &err);
    if (s != 0 && s != 1) {
        test_set_failure_detailf("intg_integral returned %d", s);
        test_mark_failure(__FILE__, __LINE__, "multiprecision integral did not converge");
        goto cleanup;
    }

    log_two = num_log(two);
    expected = num_sub(num_mul_long(log_two, 2L), NUM_ONE);

    printf("  ∫₁² log(x) dx  [multiprecision]\n");
    test_print_number_line("result", result);
    test_print_number_line("expected", expected);
    test_print_number_line("err", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);

    if (!test_assert_integrator_number_close_tol(result, expected,
                                                 "1e-27", __FILE__, __LINE__))
        goto cleanup;

cleanup:
    num_destroy(&expected);
    num_destroy(&log_two);
    num_destroy(&err);
    num_destroy(&result);
    num_destroy(&two);
    num_destroy(&one);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
    ASSERT_EQ_INT(num_set_default_prec_bits(old_bits), 0);
}

void test_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_sin(x);
    intg_set_tolerance(ig, num_create_from_string("1e-21"), num_create_from_string("1e-21"));
    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(0.0), NUM_PI,
                        &result, &err);
    number_t expected = test_num_from_double(2.0);
    printf("  ∫₀^π sin(x) dx\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    intg_set_tolerance(ig, num_create_from_string("1e-21"), num_create_from_string("1e-21"));
    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(0.0), test_num_from_double(1.0),
                        &result, &err);
    number_t expected = num_sub(NUM_E, test_num_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *x2 = expr_mul(x, x);
    expr_t *denom = expr_add(one, x2);
    expr_t *expr = expr_div(one, denom);
    intg_set_tolerance(ig, num_create_from_string("1e-21"), num_create_from_string("1e-21"));
    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(-1.0), test_num_from_double(1.0),
                        &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q  (π/2)\n", NUM_PI_2);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, NUM_PI_2, "1e-20");
    expr_free(expr);
    expr_free(denom);
    expr_free(x2);
    expr_free(one);
    expr_free(x);
    intg_free(ig);
}

void test_log(void) {
    /* ∫₁^e ln(x) dx = 1 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(1.0));
    expr_t *expr = expr_log(x);
    intg_set_tolerance(ig, num_create_from_string("1e-21"), num_create_from_string("1e-21"));
    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(1.0), NUM_E,
                        &result, &err);
    number_t expected = test_num_from_double(1.0);
    printf("  ∫₁^e ln(x) dx\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_constant(void) {
    /* ∫₀^5 1 dx = 5 — constant integrand; the exact special path is exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = test_expr_new_const_d(1.0);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(0.0), test_num_from_double(5.0),
                               &result, &err);
    number_t expected = test_num_from_double(5.0);
    printf("  ∫₀^5 1 dx  (rectangle — exact special path)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_linear(void) {
    /* ∫₀^5 x dx = 12.5 — linear integrand; the exact special path is exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));

    number_t result, err;
    int s = intg_integral(ig, x, x,
                               test_num_from_double(0.0), test_num_from_double(5.0),
                               &result, &err);
    number_t expected = num_create_from_string("12.5");
    printf("  ∫₀^5 x dx  (triangle — exact special path)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(x);
    intg_free(ig);
}

void test_set_tol(void) {
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_sin(x);
    number_t loose = num_create_from_string("1e-10");
    intg_set_tolerance(ig, loose, loose);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(0.0), NUM_PI,
                        &result, &err);
    printf("  ∫₀^π sin(x) dx  (tolerance 1e-10)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  err      = %q  (limit 1e-8)\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    /* Error estimate should be at or below the loose tolerance */
    ASSERT_TRUE(num_le(err, num_create_from_string("1e-8")));
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_max_intervals(void) {
    /* Force early termination by allowing only 1 subinterval */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_sin(x);
    intg_set_interval_count_max(ig, 1);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(0.0), NUM_PI,
                        &result, &err);
    size_t n = intg_get_interval_count_used(ig);
    printf("  ∫₀^π sin(x) dx  (max_intervals = 1)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, n, result, err);
    /* Should stop early — status 1 is acceptable for a highly oscillatory
       integrand restricted to a single subinterval */
    ASSERT_TRUE(s == 0 || s == 1);
    ASSERT_TRUE(n <= 1);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_last_intervals(void) {
    /* A smooth integrand over a moderate range should converge in a handful
       of intervals; verify the counter is updated. */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    number_t result, err;
    int s = intg_integral(ig, expr, x,
                        test_num_from_double(0.0), test_num_from_double(1.0),
                        &result, &err);
    size_t n = intg_get_interval_count_used(ig);
    printf("  ∫₀¹ exp(x) dx  (interval counter)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, n, result, err);
    ASSERT_TRUE(n >= 1);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_null_safety(void) {
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    number_t result;
    /* NULL expr */
    int s = intg_integral(ig, NULL, x,
                        test_num_from_double(0.0), test_num_from_double(1.0),
                        &result, NULL);
    ASSERT_TRUE(s == -1);
    /* NULL result */
    s = intg_integral(ig, expr, x,
                    test_num_from_double(0.0), test_num_from_double(1.0),
                    NULL, NULL);
    ASSERT_TRUE(s == -1);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_reversed_limits(void) {
    /* ∫₁⁰ x² dx = -1/3 — reversed limits; Turán handles sign and is polynomially exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_mul(x, x);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(1.0), test_num_from_double(0.0),
                               &result, &err);
    number_t expected = num_create_from_string("-0.33333333333333333333333333333333333333");
    printf("  ∫₁⁰ x² dx  (reversed limits)\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * intg_integral tests (Turán T15/T4 rule)
 * --------------------------------------------------------------------- */

void test_expr_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_sin(x);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(0.0), NUM_PI,
                               &result, &err);
    number_t expected = test_num_from_double(2.0);
    printf("  ∫₀^π sin(x) dx  [Turán T15/T4]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);

    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_expr_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t expected = num_sub(NUM_E, test_num_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx  [Turán T15/T4]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-20");

    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_expr_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *one  = test_expr_new_const_d(1.0);
    expr_t *x2   = expr_mul(x, x);
    expr_t *denom = expr_add(one, x2);
    expr_t *expr = expr_div(one, denom);

    number_t result, err;
    int s = intg_integral(ig, expr, x,
                               test_num_from_double(-1.0), test_num_from_double(1.0),
                               &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx  [Turán T15/T4]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q  (π/2)\n", NUM_PI_2);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, NUM_PI_2, "1e-20");

    expr_free(expr);
    expr_free(denom);
    expr_free(x2);
    expr_free(one);
    expr_free(x);
    intg_free(ig);
}

void test_expr_null_safety(void) {
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    number_t result;

    /* NULL integrator */
    int s = intg_integral(NULL, expr, x,
                               test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL expr */
    s = intg_integral(ig, NULL, x,
                           test_num_from_double(0.0), test_num_from_double(1.0),
                           &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL result */
    s = intg_integral(ig, expr, x,
                           test_num_from_double(0.0), test_num_from_double(1.0),
                           NULL, NULL);
    ASSERT_TRUE(s == -1);

    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * intg_double_integral tests
 * --------------------------------------------------------------------- */

void test_double_polynomial(void) {
    /* ∫₀¹∫₀¹ x·y dx dy = 1/4 — polynomial; the exact special path is exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_mul(x, y);

    number_t result, err;
    int s = intg_double_integral(ig, expr,
                               x, test_num_from_double(0.0), test_num_from_double(1.0),
                               y, test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t expected = test_num_from_double(0.25);
    printf("  ∫₀¹∫₀¹ x·y dx dy\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-20");
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_double_exp(void) {
    /* ∫₀¹∫₀¹ exp(x+y) dx dy = (e−1)² */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *sum  = expr_add(x, y);           // store intermediate
    expr_t *expr = expr_exp(sum);

    number_t result, err;
    int s = intg_double_integral(ig, expr,
                               x, test_num_from_double(0.0), test_num_from_double(1.0),
                               y, test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t em1      = num_sub(NUM_E, test_num_from_double(1.0));
    number_t expected = num_mul(em1, em1);
    printf("  ∫₀¹∫₀¹ exp(x+y) dx dy  [(e−1)²]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-20");
    expr_free(expr);
    expr_free(sum);   // free intermediate
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_double_nonunit_bounds(void) {
    /* ∫₀²∫₀³ x·y dx dy = 9 — polynomial with non-unit bounds */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_mul(x, y);

    number_t result, err;
    int s = intg_double_integral(ig, expr,
                               x, test_num_from_double(0.0), test_num_from_double(2.0),
                               y, test_num_from_double(0.0), test_num_from_double(3.0),
                               &result, &err);
    number_t expected = test_num_from_double(9.0);
    printf("  ∫₀²∫₀³ x·y dx dy\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-23");
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_double_null_safety(void) {
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_mul(x, y);
    number_t result;
    number_t z = test_num_from_double(0.0), o = test_num_from_double(1.0);

    ASSERT_TRUE(intg_double_integral(NULL, expr, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(intg_double_integral(ig, NULL, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(intg_double_integral(ig, expr, x, z, o, y, z, o, NULL, NULL) == -1);
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * intg_triple_integral tests
 * --------------------------------------------------------------------- */

void test_triple_polynomial(void) {
    /* ∫₀¹∫₀¹∫₀¹ x·y·z dx dy dz = 1/8 — polynomial exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_mul(x, y);           // store intermediate
    expr_t *expr = expr_mul(xy, z);

    number_t result, err;
    int s = intg_triple_integral(ig, expr,
                               x, test_num_from_double(0.0), test_num_from_double(1.0),
                               y, test_num_from_double(0.0), test_num_from_double(1.0),
                               z, test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t expected = test_num_from_double(0.125);
    printf("  ∫₀¹∫₀¹∫₀¹ x·y·z dx dy dz\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-23");
    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_triple_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹ exp(x+y+z) dx dy dz = (e−1)³ */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *xyz  = expr_add(xy, z);          // store intermediate
    expr_t *expr = expr_exp(xyz);

    number_t result, err;
    int s = intg_triple_integral(ig, expr,
                               x, test_num_from_double(0.0), test_num_from_double(1.0),
                               y, test_num_from_double(0.0), test_num_from_double(1.0),
                               z, test_num_from_double(0.0), test_num_from_double(1.0),
                               &result, &err);
    number_t em1      = num_sub(NUM_E, test_num_from_double(1.0));
    number_t expected = num_mul(num_mul(em1, em1), em1);
    printf("  ∫₀¹∫₀¹∫₀¹ exp(x+y+z) dx dy dz  [(e−1)³]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    expr_free(expr);
    expr_free(xyz);   // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_triple_null_safety(void) {
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_mul(x, y);           // store intermediate
    expr_t *expr = expr_mul(xy, z);
    number_t result;
    number_t lo = test_num_from_double(0.0), hi = test_num_from_double(1.0);

    ASSERT_TRUE(intg_triple_integral(NULL, expr, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(intg_triple_integral(ig, NULL, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(intg_triple_integral(ig, expr, x, lo, hi, y, lo, hi, z, lo, hi, NULL, NULL) == -1);

    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * intg_integral_multi tests (N-dimensional Turán T15/T4)
 * --------------------------------------------------------------------- */

void test_multi_2d(void) {
    /* ∫₀¹ ∫₀¹ (x+y) dx dy = 1 — linear; expect exact symbolic evaluation */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_add(x, y);

    expr_t *vars[2] = { x, y };
    number_t lo[2]  = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2]  = { test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);
    number_t expected = test_num_from_double(1.0);
    printf("  ∫₀¹∫₀¹ (x+y) dx dy  [double integral]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d(void) {
    /* ∫₀¹ ∫₀¹ ∫₀¹ (x+y+z) dx dy dz = 1.5 — linear; expect exact symbolic evaluation */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *expr = expr_add(xy, z);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3]  = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3]  = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);
    number_t expected = num_create_from_string("1.5");
    printf("  ∫₀¹∫₀¹∫₀¹ (x+y+z) dx dy dz  [triple integral]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_null_safety(void) {
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    expr_t *vars[1] = { x };
    number_t lo[1] = { test_num_from_double(0.0) };
    number_t hi[1] = { test_num_from_double(1.0) };
    number_t result;

    ASSERT_TRUE(intg_integral_multi(NULL, expr, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(intg_integral_multi(ig, NULL, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(intg_integral_multi(ig, expr, 0, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(intg_integral_multi(ig, expr, 1, vars, lo, hi, NULL, NULL) == -1);

    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_multi_nd1(void) {
    /* ndim=1 degenerates to intg_integral: ∫₀¹ exp(x) dx = e−1 */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *expr = expr_exp(x);
    expr_t *vars[1] = { x };
    number_t lo[1]  = { test_num_from_double(0.0) };
    number_t hi[1]  = { test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 1, vars, lo, hi, &result, &err);
    number_t expected = num_sub(NUM_E, test_num_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx  [multi ndim=1]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    expr_free(expr);
    expr_free(x);
    intg_free(ig);
}

void test_multi_4d(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ (x+y+z+w) dx dy dz dw = 2.0 — linear polynomial in 4D, exact */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *w    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *zw   = expr_add(z, w);           // store intermediate
    expr_t *expr = expr_add(xy, zw);

    expr_t *vars[4] = { x, y, z, w };
    number_t lo[4]  = { test_num_from_double(0.0), test_num_from_double(0.0),
                        test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[4]  = { test_num_from_double(1.0), test_num_from_double(1.0),
                        test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);
    number_t expected = test_num_from_double(2.0);
    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ (x+y+z+w) dx dy dz dw  [quadruple integral]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    expr_free(expr);
    expr_free(zw);    // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_4d_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ exp(x+y+z+w) dx dy dz dw = (e−1)⁴ */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *w    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *zw   = expr_add(z, w);           // store intermediate
    expr_t *sum  = expr_add(xy, zw);         // store intermediate
    expr_t *expr = expr_exp(sum);

    expr_t *vars[4] = { x, y, z, w };
    number_t lo[4]  = { test_num_from_double(0.0), test_num_from_double(0.0),
                        test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[4]  = { test_num_from_double(1.0), test_num_from_double(1.0),
                        test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);
    number_t em1      = num_sub(NUM_E, test_num_from_double(1.0));
    number_t em1sq    = num_mul(em1, em1);
    number_t expected = num_mul(em1sq, em1sq);
    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ exp(x+y+z+w) dx dy dz dw  [(e - 1)⁴]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    expr_free(expr);
    expr_free(sum);   // free intermediate
    expr_free(zw);    // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_4d_exp_affine(void) {
    /* ∫ exp(2x - y + 0.5z + 3w + 1) dV = e * Π_i ∫ exp(a_i t) dt on [0,1]^4 */
    integrator_t *ig = intg_new();
    expr_t *x    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *w    = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *neg_y = expr_neg(y);
    expr_t *half_z = expr_mul_d(z, 0.5);
    expr_t *three_w = expr_mul_d(w, 3.0);
    expr_t *sum_xy = expr_add(two_x, neg_y);
    expr_t *sum_xyz = expr_add(sum_xy, half_z);
    expr_t *sum_xyzw = expr_add(sum_xyz, three_w);
    expr_t *affine = expr_add_d(sum_xyzw, 1.0);
    expr_t *expr = expr_exp(affine);

    expr_t *vars[4] = { x, y, z, w };
    number_t lo[4]  = { test_num_from_double(0.0), test_num_from_double(0.0),
                        test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[4]  = { test_num_from_double(1.0), test_num_from_double(1.0),
                        test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);

    number_t ex = test_num_mul_double(num_sub(num_exp(test_num_from_double(2.0)), NUM_ONE), 0.5);
    number_t ey = num_sub(NUM_ONE, num_div(NUM_ONE, NUM_E));
    number_t ez = test_num_mul_double(num_sub(num_exp(test_num_from_double(0.5)), NUM_ONE), 2.0);
    number_t ew = num_div(num_sub(num_exp(test_num_from_double(3.0)), NUM_ONE),
                         test_num_from_double(3.0));
    number_t expected = num_mul(NUM_E, num_mul(num_mul(ex, ey), num_mul(ez, ew)));

    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ exp(2x-y+0.5z+3w+1) dx dy dz dw  [affine exp]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyzw);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(three_w);
    expr_free(half_z);
    expr_free(neg_y);
    expr_free(two_x);
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_sinh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sinh(x - 2y + 0.5z + 1) dV */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *neg2y = expr_mul_d(y, -2.0);
    expr_t *halfz = expr_mul_d(z, 0.5);
    expr_t *sum_xy = expr_add(x, neg2y);
    expr_t *sum_xyz = expr_add(sum_xy, halfz);
    expr_t *affine = expr_add_d(sum_xyz, 1.0);
    expr_t *expr = expr_sinh(affine);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t ix_p = num_sub(NUM_E, NUM_ONE);
    number_t iy_p = test_num_mul_double(num_sub(NUM_ONE, num_exp(test_num_from_double(-2.0))), 0.5);
    number_t iz_p = test_num_mul_double(num_sub(num_exp(test_num_from_double(0.5)), NUM_ONE), 2.0);
    number_t i_pos = num_mul(NUM_E, num_mul(ix_p, num_mul(iy_p, iz_p)));

    number_t ix_n = num_sub(NUM_ONE, num_div(NUM_ONE, NUM_E));
    number_t iy_n = test_num_mul_double(num_sub(num_exp(test_num_from_double(2.0)), NUM_ONE), 0.5);
    number_t iz_n = test_num_mul_double(num_sub(NUM_ONE, num_exp(test_num_from_double(-0.5))), 2.0);
    number_t i_neg = num_mul(num_div(NUM_ONE, NUM_E), num_mul(ix_n, num_mul(iy_n, iz_n)));
    number_t expected = test_num_mul_double(num_sub(i_pos, i_neg), 0.5);

    printf("  ∫₀¹∫₀¹∫₀¹ sinh(x-2y+0.5z+1) dx dy dz  [affine sinh]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(halfz);
    expr_free(neg2y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_cosh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cosh(1.5x + y - z + 0.25) dV */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *onept5x = expr_mul_d(x, 1.5);
    expr_t *negz = expr_neg(z);
    expr_t *sum_xy = expr_add(onept5x, y);
    expr_t *sum_xyz = expr_add(sum_xy, negz);
    expr_t *affine = expr_add_d(sum_xyz, 0.25);
    expr_t *expr = expr_cosh(affine);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t ix_p = num_div(num_sub(num_exp(test_num_from_double(1.5)), NUM_ONE),
                           test_num_from_double(1.5));
    number_t iy_p = num_sub(NUM_E, NUM_ONE);
    number_t iz_p = num_sub(NUM_ONE, num_div(NUM_ONE, NUM_E));
    number_t i_pos = num_mul(num_exp(test_num_from_double(0.25)), num_mul(ix_p, num_mul(iy_p, iz_p)));

    number_t ix_n = num_div(num_sub(NUM_ONE, num_exp(test_num_from_double(-1.5))),
                           test_num_from_double(1.5));
    number_t iy_n = num_sub(NUM_ONE, num_div(NUM_ONE, NUM_E));
    number_t iz_n = num_sub(NUM_E, NUM_ONE);
    number_t i_neg = num_mul(num_exp(test_num_from_double(-0.25)), num_mul(ix_n, num_mul(iy_n, iz_n)));
    number_t expected = test_num_mul_double(num_add(i_pos, i_neg), 0.5);

    printf("  ∫₀¹∫₀¹∫₀¹ cosh(1.5x+y-z+0.25) dx dy dz  [affine cosh]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(negz);
    expr_free(onept5x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_sin_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin] */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *neg_z = expr_neg(z);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *sum_xyz = expr_add(sum_xy, neg_z);
    expr_t *affine = expr_add_d(sum_xyz, 0.3);
    expr_t *expr = expr_sin(affine);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t expected_z = num_mul(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(0.3))),
                                   num_mul(num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(1.0))), NUM_ONE),
                                                 test_num_make_complex(NUM_ZERO, test_num_from_double(1.0))),
                                          num_mul(num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(2.0))), NUM_ONE),
                                                        test_num_make_complex(NUM_ZERO, test_num_from_double(2.0))),
                                                 num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(-1.0))), NUM_ONE),
                                                        test_num_make_complex(NUM_ZERO, test_num_from_double(-1.0))))));
    number_t expected = num_imag_part(expected_z);

    printf("  ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(neg_z);
    expr_free(two_y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_cos_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos] */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *half_x = expr_mul_d(x, 0.5);
    expr_t *neg_y = expr_neg(y);
    expr_t *onept5_z = expr_mul_d(z, 1.5);
    expr_t *sum_xy = expr_add(half_x, neg_y);
    expr_t *sum_xyz = expr_add(sum_xy, onept5_z);
    expr_t *affine = expr_sub_d(sum_xyz, 0.2);
    expr_t *expr = expr_cos(affine);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t expected_z = num_mul(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(-0.2))),
                                   num_mul(num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(0.5))), NUM_ONE),
                                                 test_num_make_complex(NUM_ZERO, test_num_from_double(0.5))),
                                          num_mul(num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(-1.0))), NUM_ONE),
                                                        test_num_make_complex(NUM_ZERO, test_num_from_double(-1.0))),
                                                 num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(1.5))), NUM_ONE),
                                                        test_num_make_complex(NUM_ZERO, test_num_from_double(1.5))))));
    number_t expected = num_real_part(expected_z);

    printf("  ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(onept5_z);
    expr_free(neg_y);
    expr_free(half_x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_scaled_sum_specials(void) {
    /* 2*exp(x+y) - 3*cosh(z+0.5) + 4 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *xy = expr_add(x, y);
    expr_t *exp_xy = expr_exp(xy);
    expr_t *term1 = expr_mul_d(exp_xy, 2.0);
    expr_t *zshift = expr_add_d(z, 0.5);
    expr_t *cosh_z = expr_cosh(zshift);
    expr_t *term2 = expr_mul_d(cosh_z, 3.0);
    expr_t *partial = expr_sub(term1, term2);
    expr_t *expr = expr_add_d(partial, 4.0);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t em1 = num_sub(NUM_E, NUM_ONE);
    number_t term1_expected = test_num_mul_double(num_mul(em1, em1), 2.0);
    number_t term2_expected = test_num_mul_double(num_sub(num_sinh(test_num_from_double(1.5)),
                                                   num_sinh(test_num_from_double(0.5))), 3.0);
    number_t expected = num_add(num_sub(term1_expected, term2_expected), test_num_from_double(4.0));

    printf("  ∫ (2exp(x+y)-3cosh(z+0.5)+4) dV  [scaled sum specials]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(partial);
    expr_free(term2);
    expr_free(cosh_z);
    expr_free(zshift);
    expr_free(term1);
    expr_free(exp_xy);
    expr_free(xy);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_sum_of_specials(void) {
    /* sin(x+0.2) + cos(2y-0.1) + exp(x-y) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *sin_arg = expr_add_d(x, 0.2);
    expr_t *sin_term = expr_sin(sin_arg);
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *cos_arg = expr_sub_d(two_y, 0.1);
    expr_t *cos_term = expr_cos(cos_arg);
    expr_t *exp_arg = expr_sub(x, y);
    expr_t *exp_term = expr_exp(exp_arg);
    expr_t *sum1 = expr_add(sin_term, cos_term);
    expr_t *expr = expr_add(sum1, exp_term);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    number_t sin_expected_z = num_mul(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(0.2))),
                                       num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(1.0))), NUM_ONE),
                                              test_num_make_complex(NUM_ZERO, test_num_from_double(1.0))));
    number_t sin_expected = num_imag_part(sin_expected_z);
    number_t cos_expected_z = num_mul(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(-0.1))),
                                       num_div(num_sub(num_exp(test_num_make_complex(NUM_ZERO, test_num_from_double(2.0))), NUM_ONE),
                                              test_num_make_complex(NUM_ZERO, test_num_from_double(2.0))));
    number_t cos_expected = num_real_part(cos_expected_z);
    number_t exp_expected = num_mul(num_sub(NUM_E, NUM_ONE),
                                   num_sub(NUM_ONE, num_div(NUM_ONE, NUM_E)));
    number_t expected = num_add(num_add(sin_expected, cos_expected), exp_expected);

    printf("  ∫∫ [sin(x+0.2)+cos(2y-0.1)+exp(x-y)] dA  [sum specials]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");

    expr_free(expr);
    expr_free(sum1);
    expr_free(exp_term);
    expr_free(exp_arg);
    expr_free(cos_term);
    expr_free(cos_arg);
    expr_free(two_y);
    expr_free(sin_term);
    expr_free(sin_arg);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_separable_product(void) {
    /* exp(x) * cos(2y-0.1) * sinh(z+0.2) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *exp_x = expr_exp(x);
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *cos_arg = expr_sub_d(two_y, 0.1);
    expr_t *cos_y = expr_cos(cos_arg);
    expr_t *sinh_arg = expr_add_d(z, 0.2);
    expr_t *sinh_z = expr_sinh(sinh_arg);
    expr_t *prod_xy = expr_mul(exp_x, cos_y);
    expr_t *expr = expr_mul(prod_xy, sinh_z);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t exp1_minus_1 = num_sub(num_exp(test_num_from_double(1.0)), NUM_ONE);
    number_t cos_part = test_num_mul_double(num_sub(num_sin(test_num_from_double(1.9)),
                                             num_sin(test_num_from_double(-0.1))), 0.5);
    number_t sinh_part = num_sub(num_cosh(test_num_from_double(1.2)),
                                num_cosh(test_num_from_double(0.2)));
    number_t left_part = num_mul(exp1_minus_1, cos_part);
    number_t expected = num_mul(left_part, sinh_part);

    printf("  ∫ exp(x)cos(2y-0.1)sinh(z+0.2) dV  [separable product]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);

    expr_free(expr);
    expr_free(prod_xy);
    expr_free(sinh_z);
    expr_free(sinh_arg);
    expr_free(cos_y);
    expr_free(cos_arg);
    expr_free(two_y);
    expr_free(exp_x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_3d_regrouped_separable_product(void) {
    /* (x*cos(y)) * (x*exp(z)) -> x^2 * cos(y) * exp(z) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *z = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *cos_y = expr_cos(y);
    expr_t *exp_z = expr_exp(z);
    expr_t *left = expr_mul(x, cos_y);
    expr_t *right = expr_mul(x, exp_z);
    expr_t *expr = expr_mul(left, right);

    expr_t *vars[3] = { x, y, z };
    number_t lo[3] = { test_num_from_double(0.0), test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[3] = { test_num_from_double(1.0), test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    number_t expected = num_mul(num_div(test_num_from_double(1.0), test_num_from_double(3.0)),
                               num_mul(num_sin(test_num_from_double(1.0)),
                                      num_sub(num_exp(test_num_from_double(1.0)), NUM_ONE)));

    printf("  ∫ (x*cos(y))*(x*exp(z)) dV  [regrouped separable product]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);

    expr_free(expr);
    expr_free(right);
    expr_free(left);
    expr_free(exp_z);
    expr_free(cos_y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_sum_of_separable_products(void) {
    /* exp(x)cos(y) + sinh(x+0.1)exp(y) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *exp_x = expr_exp(x);
    expr_t *cos_y = expr_cos(y);
    expr_t *term1 = expr_mul(exp_x, cos_y);
    expr_t *x_shift = expr_add_d(x, 0.1);
    expr_t *sinh_x = expr_sinh(x_shift);
    expr_t *exp_y = expr_exp(y);
    expr_t *term2 = expr_mul(sinh_x, exp_y);
    expr_t *expr = expr_add(term1, term2);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };

    number_t result, err;
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    number_t exp1_minus_1 = num_sub(num_exp(test_num_from_double(1.0)), NUM_ONE);
    number_t term1_expected = num_mul(exp1_minus_1, num_sin(test_num_from_double(1.0)));
    number_t term2_expected = num_mul(num_sub(num_cosh(test_num_from_double(1.1)),
                                            num_cosh(test_num_from_double(0.1))),
                                     exp1_minus_1);
    number_t expected = num_add(term1_expected, term2_expected);

    printf("  ∫∫ [exp(x)cos(y)+sinh(x+0.1)exp(y)] dA  [sum separable products]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE(result, expected);

    expr_free(expr);
    expr_free(term2);
    expr_free(exp_y);
    expr_free(sinh_x);
    expr_free(x_shift);
    expr_free(term1);
    expr_free(cos_y);
    expr_free(exp_x);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_square(void) {
    /* (x + 2y + 3)^2 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *expr = expr_mul(affine, affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_div(test_num_from_double(62.0), test_num_from_double(3.0));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2 dA  [affine square]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_cube(void) {
    /* (x + 2y + 3)^3 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *expr = expr_mul(square, affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_div(test_num_from_double(387.0), test_num_from_double(4.0));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3 dA  [affine cube]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_quartic(void) {
    /* (x + 2y + 3)^4 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *lhs = expr_mul(affine, affine);
    expr_t *rhs = expr_mul(affine, affine);
    expr_t *expr = expr_mul(lhs, rhs);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_div(test_num_from_double(6916.0), test_num_from_double(15.0));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4 dA  [affine quartic]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(rhs);
    expr_free(lhs);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_poly_deg4(void) {
    /* 3(x + 2y + 3)^4 - 2(x + 2y + 3)^2 + (x + 2y + 3) + 7 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *scaled_quartic = expr_mul_d(quartic, 3.0);
    expr_t *scaled_square = expr_mul_d(square, 2.0);
    expr_t *poly_core = expr_sub(scaled_quartic, scaled_square);
    expr_t *seven = test_expr_new_const_d(7.0);
    expr_t *affine_plus_seven = expr_add(affine, seven);
    expr_t *poly = expr_add(poly_core, affine_plus_seven);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_div(test_num_from_double(40601.0), test_num_from_double(30.0));
    int s = intg_integral_multi(ig, poly, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ [3a^4-2a^2+a+7] dA  [affine poly]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(poly);
    expr_free(affine_plus_seven);
    expr_free(seven);
    expr_free(poly_core);
    expr_free(scaled_square);
    expr_free(scaled_quartic);
    expr_free(square);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_times_exp_affine(void) {
    /* (x + 2y + 3) * exp(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t expected = num_add(num_sub(test_num_mul_double(e6, 2.0), e4),
                               num_sub(test_num_mul_double(e3, 0.5), test_num_mul_double(e5, 1.5)));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)exp(x+2y+3) dA  [affine*exp(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_square_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^2 * exp(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(square, exp_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t expected = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 18.0), test_num_mul_double(e5, 11.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 6.0))),
        0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2exp(x+2y+3) dA  [affine^2*exp(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_times_sin_affine(void) {
    /* (x + 2y + 3) * sin(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(affine, sin_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t f3 = num_sub(num_neg(num_mul(u3, num_sin(u3))), test_num_mul_double(num_cos(u3), 2.0));
    number_t f4 = num_sub(num_neg(num_mul(u4, num_sin(u4))), test_num_mul_double(num_cos(u4), 2.0));
    number_t f5 = num_sub(num_neg(num_mul(u5, num_sin(u5))), test_num_mul_double(num_cos(u5), 2.0));
    number_t f6 = num_sub(num_neg(num_mul(u6, num_sin(u6))), test_num_mul_double(num_cos(u6), 2.0));
    number_t expected = test_num_mul_double(num_sub(num_sub(f6, f4), num_sub(f5, f3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)sin(x+2y+3) dA  [affine*sin(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_square_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^2 * sin(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(square, sin_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t f3 = num_add(num_neg(num_mul(num_mul(u3, u3), num_sin(u3))),
                         num_add(test_num_mul_double(num_mul(u3, num_cos(u3)), -4.0),
                                test_num_mul_double(num_sin(u3), 6.0)));
    number_t f4 = num_add(num_neg(num_mul(num_mul(u4, u4), num_sin(u4))),
                         num_add(test_num_mul_double(num_mul(u4, num_cos(u4)), -4.0),
                                test_num_mul_double(num_sin(u4), 6.0)));
    number_t f5 = num_add(num_neg(num_mul(num_mul(u5, u5), num_sin(u5))),
                         num_add(test_num_mul_double(num_mul(u5, num_cos(u5)), -4.0),
                                test_num_mul_double(num_sin(u5), 6.0)));
    number_t f6 = num_add(num_neg(num_mul(num_mul(u6, u6), num_sin(u6))),
                         num_add(test_num_mul_double(num_mul(u6, num_cos(u6)), -4.0),
                                test_num_mul_double(num_sin(u6), 6.0)));
    number_t expected = test_num_mul_double(num_sub(num_sub(f6, f4), num_sub(f5, f3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2sin(x+2y+3) dA  [affine^2*sin(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_times_cos_affine(void) {
    /* (x + 2y + 3) * cos(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t g3 = num_add(num_neg(num_mul(u3, num_cos(u3))), test_num_mul_double(num_sin(u3), 2.0));
    number_t g4 = num_add(num_neg(num_mul(u4, num_cos(u4))), test_num_mul_double(num_sin(u4), 2.0));
    number_t g5 = num_add(num_neg(num_mul(u5, num_cos(u5))), test_num_mul_double(num_sin(u5), 2.0));
    number_t g6 = num_add(num_neg(num_mul(u6, num_cos(u6))), test_num_mul_double(num_sin(u6), 2.0));
    number_t expected = test_num_mul_double(num_sub(num_sub(g6, g4), num_sub(g5, g3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)cos(x+2y+3) dA  [affine*cos(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_square_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^2 * cos(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, square);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t g3 = num_add(num_neg(num_mul(num_mul(u3, u3), num_cos(u3))),
                         num_add(test_num_mul_double(num_mul(u3, num_sin(u3)), 4.0),
                                test_num_mul_double(num_cos(u3), 6.0)));
    number_t g4 = num_add(num_neg(num_mul(num_mul(u4, u4), num_cos(u4))),
                         num_add(test_num_mul_double(num_mul(u4, num_sin(u4)), 4.0),
                                test_num_mul_double(num_cos(u4), 6.0)));
    number_t g5 = num_add(num_neg(num_mul(num_mul(u5, u5), num_cos(u5))),
                         num_add(test_num_mul_double(num_mul(u5, num_sin(u5)), 4.0),
                                test_num_mul_double(num_cos(u5), 6.0)));
    number_t g6 = num_add(num_neg(num_mul(num_mul(u6, u6), num_cos(u6))),
                         num_add(test_num_mul_double(num_mul(u6, num_sin(u6)), 4.0),
                                test_num_mul_double(num_cos(u6), 6.0)));
    number_t expected = test_num_mul_double(num_sub(num_sub(g6, g4), num_sub(g5, g3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2cos(x+2y+3) dA  [affine^2*cos(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_times_sinh_affine(void) {
    /* (x + 2y + 3) * sinh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(affine, sinh_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = num_add(num_sub(test_num_mul_double(e6, 2.0), e4),
                          num_sub(test_num_mul_double(e3, 0.5), test_num_mul_double(e5, 1.5)));
    number_t minus = num_add(num_sub(test_num_mul_double(em6, 4.0), test_num_mul_double(em4, 3.0)),
                            num_sub(test_num_mul_double(em3, 2.5), test_num_mul_double(em5, 3.5)));
    number_t expected = test_num_mul_double(num_sub(pos, minus), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)sinh(x+2y+3) dA  [affine*sinh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_square_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^2 * sinh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(square, sinh_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 18.0), test_num_mul_double(e5, 11.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 6.0))),
        0.5);
    number_t neg = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(em6, 66.0), test_num_mul_double(em5, 51.0)),
               num_sub(test_num_mul_double(em3, 27.0), test_num_mul_double(em4, 38.0))),
        0.5);
    number_t expected = test_num_mul_double(num_sub(pos, neg), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2sinh(x+2y+3) dA  [affine^2*sinh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_times_cosh_affine(void) {
    /* (x + 2y + 3) * cosh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = num_add(num_sub(test_num_mul_double(e6, 2.0), e4),
                          num_sub(test_num_mul_double(e3, 0.5), test_num_mul_double(e5, 1.5)));
    number_t minus = num_add(num_sub(test_num_mul_double(em6, 4.0), test_num_mul_double(em4, 3.0)),
                            num_sub(test_num_mul_double(em3, 2.5), test_num_mul_double(em5, 3.5)));
    number_t expected = test_num_mul_double(num_add(pos, minus), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)cosh(x+2y+3) dA  [affine*cosh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_square_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^2 * cosh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, square);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 18.0), test_num_mul_double(e5, 11.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 6.0))),
        0.5);
    number_t neg = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(em6, 66.0), test_num_mul_double(em5, 51.0)),
               num_sub(test_num_mul_double(em3, 27.0), test_num_mul_double(em4, 38.0))),
        0.5);
    number_t expected = test_num_mul_double(num_add(pos, neg), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2cosh(x+2y+3) dA  [affine^2*cosh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_cube_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^3 * exp(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(cube, exp_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t expected = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 84.0), test_num_mul_double(e5, 41.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 16.0))),
        0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3exp(x+2y+3) dA  [affine^3*exp(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_cube_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^3 * sin(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube_square = expr_mul(affine, affine);
    expr_t *cube = expr_mul(cube_square, affine);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(cube, sin_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t f3 = num_add(num_mul(num_add(num_neg(num_mul(u3, num_mul(u3, u3))),
                                       test_num_mul_double(u3, 18.0)), num_sin(u3)),
                         num_mul(num_add(test_num_mul_double(num_mul(u3, u3), -6.0),
                                       test_num_from_double(24.0)), num_cos(u3)));
    number_t f4 = num_add(num_mul(num_add(num_neg(num_mul(u4, num_mul(u4, u4))),
                                       test_num_mul_double(u4, 18.0)), num_sin(u4)),
                         num_mul(num_add(test_num_mul_double(num_mul(u4, u4), -6.0),
                                       test_num_from_double(24.0)), num_cos(u4)));
    number_t f5 = num_add(num_mul(num_add(num_neg(num_mul(u5, num_mul(u5, u5))),
                                       test_num_mul_double(u5, 18.0)), num_sin(u5)),
                         num_mul(num_add(test_num_mul_double(num_mul(u5, u5), -6.0),
                                       test_num_from_double(24.0)), num_cos(u5)));
    number_t f6 = num_add(num_mul(num_add(num_neg(num_mul(u6, num_mul(u6, u6))),
                                       test_num_mul_double(u6, 18.0)), num_sin(u6)),
                         num_mul(num_add(test_num_mul_double(num_mul(u6, u6), -6.0),
                                       test_num_from_double(24.0)), num_cos(u6)));
    number_t expected = test_num_mul_double(num_sub(num_sub(f6, f4), num_sub(f5, f3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3sin(x+2y+3) dA  [affine^3*sin(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(cube_square);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_cube_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^3 * cos(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, cube);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t u3 = test_num_from_double(3.0);
    number_t u4 = test_num_from_double(4.0);
    number_t u5 = test_num_from_double(5.0);
    number_t u6 = test_num_from_double(6.0);
    number_t g3 = num_add(num_mul(num_add(num_neg(num_mul(u3, num_mul(u3, u3))),
                                       test_num_mul_double(u3, 18.0)), num_cos(u3)),
                         num_mul(num_add(test_num_mul_double(num_mul(u3, u3), 6.0),
                                       test_num_from_double(-24.0)), num_sin(u3)));
    number_t g4 = num_add(num_mul(num_add(num_neg(num_mul(u4, num_mul(u4, u4))),
                                       test_num_mul_double(u4, 18.0)), num_cos(u4)),
                         num_mul(num_add(test_num_mul_double(num_mul(u4, u4), 6.0),
                                       test_num_from_double(-24.0)), num_sin(u4)));
    number_t g5 = num_add(num_mul(num_add(num_neg(num_mul(u5, num_mul(u5, u5))),
                                       test_num_mul_double(u5, 18.0)), num_cos(u5)),
                         num_mul(num_add(test_num_mul_double(num_mul(u5, u5), 6.0),
                                       test_num_from_double(-24.0)), num_sin(u5)));
    number_t g6 = num_add(num_mul(num_add(num_neg(num_mul(u6, num_mul(u6, u6))),
                                       test_num_mul_double(u6, 18.0)), num_cos(u6)),
                         num_mul(num_add(test_num_mul_double(num_mul(u6, u6), 6.0),
                                       test_num_from_double(-24.0)), num_sin(u6)));
    number_t expected = test_num_mul_double(num_sub(num_sub(g6, g4), num_sub(g5, g3)), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3cos(x+2y+3) dA  [affine^3*cos(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_cube_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^3 * sinh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(cube, sinh_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 84.0), test_num_mul_double(e5, 41.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 16.0))),
        0.5);
    number_t neg = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(em6, 564.0), test_num_mul_double(em5, 389.0)),
               num_sub(test_num_mul_double(em3, 159.0), test_num_mul_double(em4, 256.0))),
        0.5);
    number_t expected = test_num_mul_double(num_sub(pos, neg), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3sinh(x+2y+3) dA  [affine^3*sinh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_cube_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^3 * cosh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube_square = expr_mul(affine, affine);
    expr_t *cube = expr_mul(cube_square, affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, cube);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t e3 = num_exp(test_num_from_double(3.0));
    number_t e4 = num_exp(test_num_from_double(4.0));
    number_t e5 = num_exp(test_num_from_double(5.0));
    number_t e6 = num_exp(test_num_from_double(6.0));
    number_t em3 = num_exp(test_num_from_double(-3.0));
    number_t em4 = num_exp(test_num_from_double(-4.0));
    number_t em5 = num_exp(test_num_from_double(-5.0));
    number_t em6 = num_exp(test_num_from_double(-6.0));
    number_t pos = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(e6, 84.0), test_num_mul_double(e5, 41.0)),
               num_sub(test_num_mul_double(e3, 3.0), test_num_mul_double(e4, 16.0))),
        0.5);
    number_t neg = test_num_mul_double(
        num_add(num_sub(test_num_mul_double(em6, 564.0), test_num_mul_double(em5, 389.0)),
               num_sub(test_num_mul_double(em3, 159.0), test_num_mul_double(em4, 256.0))),
        0.5);
    number_t expected = test_num_mul_double(num_add(pos, neg), 0.5);
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3cosh(x+2y+3) dA  [affine^3*cosh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(cube_square);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_quartic_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^4 * exp(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(quartic, exp_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected =
        num_create_from_string("68737.53818332082704696161172519864941330607887958296538908503087525293084452735250637289");
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4exp(x+2y+3) dA  [affine^4*exp(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_quartic_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^4 * sin(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic_lhs = expr_mul(affine, affine);
    expr_t *quartic_rhs = expr_mul(affine, affine);
    expr_t *quartic = expr_mul(quartic_lhs, quartic_rhs);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(quartic, sin_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected =
        num_create_from_string("-381.33814729825575506728041524097607853401008090198");
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4sin(x+2y+3) dA  [affine^4*sin(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(quartic_rhs);
    expr_free(quartic_lhs);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_quartic_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^4 * cos(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, quartic);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected =
        num_create_from_string("56.617810832398686377797715265898455798291870430519");
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4cos(x+2y+3) dA  [affine^4*cos(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_quartic_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^4 * sinh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(quartic, sinh_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected =
        num_create_from_string("34366.578859352871623151816024873924373424902707813");
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4sinh(x+2y+3) dA  [affine^4*sinh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_quartic_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^4 * cosh(x + 2y + 3) */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic_lhs = expr_mul(affine, affine);
    expr_t *quartic_rhs = expr_mul(affine, affine);
    expr_t *quartic = expr_mul(quartic_lhs, quartic_rhs);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, quartic);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected =
        num_create_from_string("34370.95932396795542380979570032394");
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4cosh(x+2y+3) dA  [affine^4*cosh(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(quartic_rhs);
    expr_free(quartic_lhs);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_poly_times_exp_affine_combination(void) {
    /* (3a^4 - 2a^2 + a) * exp(a), a = x + 2y + 3 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *affine_term = expr_mul(affine, exp_affine);
    expr_t *square_term = expr_mul(square, exp_affine);
    expr_t *quartic_term = expr_mul(quartic, exp_affine);
    expr_t *scaled_quartic = expr_mul_d(quartic_term, 3.0);
    expr_t *scaled_square = expr_mul_d(square_term, -2.0);
    expr_t *sum = expr_add(scaled_quartic, scaled_square);
    expr_t *expr = expr_add(sum, affine_term);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_add(
        num_add(test_num_mul_double(num_create_from_string("68737.53818332082704696161172519695"), 3.0),
               test_num_mul_double(num_create_from_string("2680.920621655793569036410945540741"), -2.0)),
        num_create_from_string("539.6824667600549348774549946503721"));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (3a^4-2a^2+a)exp(a) dA  [affine poly * exp(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-23");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sum);
    expr_free(scaled_square);
    expr_free(scaled_quartic);
    expr_free(quartic_term);
    expr_free(square_term);
    expr_free(affine_term);
    expr_free(exp_affine);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

void test_multi_2d_affine_poly_times_sin_affine_combination(void) {
    /* (2a^4 + a^2 - 3a) * sin(a), a = x + 2y + 3 */
    integrator_t *ig = intg_new();
    expr_t *x = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *y = test_expr_new_var_num(test_num_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *affine_term = expr_mul(affine, sin_affine);
    expr_t *square_term = expr_mul(square, sin_affine);
    expr_t *quartic_term = expr_mul(quartic, sin_affine);
    expr_t *scaled_quartic = expr_mul_d(quartic_term, 2.0);
    expr_t *scaled_affine = expr_mul_d(affine_term, -3.0);
    expr_t *sum = expr_add(scaled_quartic, square_term);
    expr_t *expr = expr_add(sum, scaled_affine);

    expr_t *vars[2] = { x, y };
    number_t lo[2] = { test_num_from_double(0.0), test_num_from_double(0.0) };
    number_t hi[2] = { test_num_from_double(1.0), test_num_from_double(1.0) };
    number_t result, err;
    number_t expected = num_add(
        num_add(test_num_mul_double(num_create_from_string("-381.3381472982557550672804152409705"), 2.0),
               num_create_from_string("-16.88885619742372162769129239240872")),
        test_num_mul_double(num_create_from_string("-3.624508420217032103141223583565993"), -3.0));
    int s = intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (2a^4+a^2-3a)sin(a) dA  [affine poly * sin(affine)]\n");
    test_num_printf_compat("  result   = %q\n", result);
    test_num_printf_compat("  expected = %q\n", expected);
    test_num_printf_compat("  err      = %q\n", err);
    test_print_integral_status(s, intg_get_interval_count_used(ig), result, err);
    ASSERT_TRUE(s == 0 || s == 1);
    TEST_ASSERT_INTEGRATOR_NUMBER_CLOSE_TOL(result, expected, "1e-27");
    ASSERT_TRUE(intg_get_interval_count_used(ig) >= 1);

    expr_free(expr);
    expr_free(sum);
    expr_free(scaled_affine);
    expr_free(scaled_quartic);
    expr_free(quartic_term);
    expr_free(square_term);
    expr_free(affine_term);
    expr_free(sin_affine);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * README examples
 * --------------------------------------------------------------------- */

static void example_integrator(void) {
    /* ∫₋₃³ exp(-x²) dx ≈ √π * erf(3) */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(-3);
    number_t hi = num_create_from_long(3);
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *x2 = expr_mul(x, x);
    expr_t *negx2 = expr_neg(x2);
    expr_t *expr = expr_exp(negx2);
    number_t result = num_new();
    number_t err = num_new();

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₋₃³ exp(-x²) dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);
    printf("  subintervals used: %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(negx2);
    expr_free(x2);
    expr_free(x);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
}

static void example_expression_backed_integration(void) {
    /* ∫₀¹ exp(x) dx = e - 1, at default 1e-27 tolerance */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    expr_t *x = expr_new_var(x0);
    expr_t *expr = expr_exp(x);
    number_t result = num_new();
    number_t err = num_new();

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₀¹ exp(x) dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);
    printf("  subintervals used: %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(x);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
}

static void example_symbolic_fast_path(void) {
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t y0 = num_create_from_long(0);
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    expr_t *x = expr_new_var(x0);
    expr_t *y = expr_new_var(y0);
    expr_t *two_y = expr_mul_num(y, &two);
    expr_t *sum = expr_add(x, two_y);
    expr_t *affine = expr_add_num(sum, &three);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);
    expr_t *vars[2] = { x, y };
    number_t lo[2] = { num_create_from_long(0), num_create_from_long(0) };
    number_t hi[2] = { num_create_from_long(1), num_create_from_long(1) };
    number_t result = num_new();
    number_t err = num_new();

    intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    num_printf("result = %.64N\n", result);
    printf("intervals = %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    num_destroy(&hi[1]);
    num_destroy(&hi[0]);
    num_destroy(&lo[1]);
    num_destroy(&lo[0]);
    num_destroy(&three);
    num_destroy(&two);
    num_destroy(&y0);
    num_destroy(&x0);
    expr_free(expr);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(sum);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
}

static void example_ctx(void) {
    /* ∫₀¹ x^2.5 dx = 1 / 3.5 */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    number_t exponent = num_create_from_string("2.5");
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *expr = expr_pow(x, &exponent);
    number_t result = num_new();
    number_t err = num_new();

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₀¹ x^2.5 dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(x);
    num_destroy(&exponent);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int tests_main(void) {
    TEST_SECTION("Lifecycle Tests");
    TEST_RUN_IN_GROUP(test_create_and_destroy, tests, NULL);
    TEST_RUN_IN_GROUP(test_null_safety, tests, NULL);

    TEST_SECTION("Integral Value Tests");
    TEST_RUN_IN_GROUP(test_polynomial, tests, NULL);
    TEST_RUN_IN_GROUP(test_sin, tests, NULL);
    TEST_RUN_IN_GROUP(test_exp, tests, NULL);
    TEST_RUN_IN_GROUP(test_arctan, tests, NULL);
    TEST_RUN_IN_GROUP(test_log, tests, NULL);
    TEST_RUN_IN_GROUP(test_constant, tests, NULL);
    TEST_RUN_IN_GROUP(test_linear, tests, NULL);
    TEST_RUN_IN_GROUP(test_reversed_limits, tests, NULL);

    TEST_SECTION("Configuration Tests");
    TEST_RUN_IN_GROUP(test_set_tol, tests, NULL);
    TEST_RUN_IN_GROUP(test_max_intervals, tests, NULL);
    TEST_RUN_IN_GROUP(test_last_intervals, tests, NULL);

    TEST_SECTION("Turan T15/T4 expr_t Tests");
    TEST_RUN_IN_GROUP(test_expr_sin, tests, NULL);
    TEST_RUN_IN_GROUP(test_expr_exp, tests, NULL);
    TEST_RUN_IN_GROUP(test_expr_arctan, tests, NULL);
    TEST_RUN_IN_GROUP(test_single_integral_num_high_precision_log, tests, NULL);
    TEST_RUN_IN_GROUP(test_expr_null_safety, tests, NULL);

    TEST_SECTION("intg_double_integral Tests");
    TEST_RUN_IN_GROUP(test_double_polynomial, tests, NULL);
    TEST_RUN_IN_GROUP(test_double_exp, tests, NULL);
    TEST_RUN_IN_GROUP(test_double_nonunit_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_double_null_safety, tests, NULL);

    TEST_SECTION("intg_triple_integral Tests");
    TEST_RUN_IN_GROUP(test_triple_polynomial, tests, NULL);
    TEST_RUN_IN_GROUP(test_triple_exp, tests, NULL);
    TEST_RUN_IN_GROUP(test_triple_null_safety, tests, NULL);

    TEST_SECTION("N-dimensional Turan T15/T4 Tests");
    TEST_RUN_IN_GROUP(test_multi_2d, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_null_safety, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_nd1, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_4d, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_4d_exp, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_4d_exp_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_sinh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_cosh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_sin_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_cos_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_scaled_sum_specials, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_sum_of_specials, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_separable_product, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_3d_regrouped_separable_product, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_sum_of_separable_products, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_square, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_cube, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_quartic, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_poly_deg4, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_times_exp_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_square_affine_times_exp_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_cube_affine_times_exp_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_quartic_affine_times_exp_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_poly_times_exp_affine_combination, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_times_sin_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_square_affine_times_sin_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_cube_affine_times_sin_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_quartic_affine_times_sin_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_poly_times_sin_affine_combination, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_times_cos_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_square_affine_times_cos_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_cube_affine_times_cos_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_quartic_affine_times_cos_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_times_sinh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_square_affine_times_sinh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_cube_affine_times_sinh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_quartic_affine_times_sinh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_affine_times_cosh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_square_affine_times_cosh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_cube_affine_times_cosh_affine, tests, NULL);
    TEST_RUN_IN_GROUP(test_multi_2d_quartic_affine_times_cosh_affine, tests, NULL);

    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_integrator, readme_examples,
                                  "integrator,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_expression_backed_integration, readme_examples,
                                  "integrator,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_symbolic_fast_path, readme_examples,
                                  "integrator,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_ctx, readme_examples,
                                  "integrator,readme,output");

    return TESTS_EXIT_CODE();
}
