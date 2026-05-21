#include "test_dval.h"

#include <math.h>

static char *format_number_at_own_precision(const number_t value)
{
    char fmt[32];
    char *out;
    size_t significant_digits = num_get_prec_digits(value);
    int needed;
    size_t precision;

    if (num_is_exact(value) || significant_digits == 0u)
        return num_to_string(value);

    precision = significant_digits > 0u ? significant_digits - 1u : 0u;
    snprintf(fmt, sizeof(fmt), "%%.%zuN", precision);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, fmt, value) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

static char *format_number_for_test_output(const number_t value)
{
    char *text = format_number_at_own_precision(value);

    return text ? text : num_to_string(value);
}

static char *format_error_for_test_output(const number_t value)
{
    int needed = num_sprintf(NULL, 0u, "%.6N", value);
    char *out;

    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, "%.6N", value) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

static number_t oracle_error_magnitude(const number_t got,
                                       const number_t expected)
{
    number_t promoted_got = num_clone(got);
    number_t diff;
    number_t error;

    if (num_get_prec_bits(expected) > 0u &&
        num_set_prec_bits(&promoted_got, num_get_prec_bits(expected)) != 0) {
        test_mark_failure(__FILE__, __LINE__,
                          "num_set_prec_bits(promoted_got) failed");
        num_destroy(&promoted_got);
        return num_create_from_double(NAN);
    }
    diff = num_sub(promoted_got, expected);
    num_destroy(&promoted_got);
    if (num_is_real(diff)) {
        error = num_abs(diff);
        num_destroy(&diff);
        return error;
    }
    {
        number_t real = num_real_part(diff);
        number_t imag = num_imag_part(diff);
        number_t mag = num_hypot(real, imag);

        num_destroy(&imag);
        num_destroy(&real);
        num_destroy(&diff);
        return mag;
    }
}

static int number_close_for_qfloat_precision(const number_t got,
                                             const number_t expected)
{
    number_t error = oracle_error_magnitude(got, expected);
    number_t one = num_create_from_double(1.0);
    number_t tolerance;
    int ok;

    if (num_set_prec_bits(&one, 106u) != 0) {
        test_mark_failure(__FILE__, __LINE__,
                          "num_set_prec_bits(one) failed");
        num_destroy(&one);
        num_destroy(&error);
        return 0;
    }
    tolerance = num_ldexp(one, 4 - 106);
    ok = num_le(error, tolerance);
    num_destroy(&one);
    num_destroy(&tolerance);
    num_destroy(&error);
    return ok;
}

static int number_close_with_tolerance_text(const number_t got,
                                            const number_t expected,
                                            const char *tolerance_text)
{
    number_t error = oracle_error_magnitude(got, expected);
    number_t tolerance = num_create_from_string(tolerance_text);
    int ok = num_le(error, tolerance);

    num_destroy(&tolerance);
    num_destroy(&error);
    return ok;
}

static void print_precision_comparison(const char *label,
                                       const number_t got,
                                       const number_t expected)
{
    char *expected_text;
    char *got_text;
    char *error_text = NULL;
    int show_error = num_is_finite(got) && num_is_finite(expected);
    number_t error;
    int error_live = 0;

    expected_text = format_number_for_test_output(expected);
    got_text = format_number_for_test_output(got);
    if (show_error) {
        error = oracle_error_magnitude(got, expected);
        error_text = format_error_for_test_output(error);
        error_live = 1;
    }

    if (!expected_text) {
        test_mark_failure(__FILE__, __LINE__,
                          "format_number_for_test_output(expected) failed");
        goto cleanup;
    }
    if (!got_text) {
        test_mark_failure(__FILE__, __LINE__,
                          "format_number_for_test_output(got) failed");
        goto cleanup;
    }
    if (show_error && !error_text) {
        error_text = malloc(sizeof("(unavailable)"));
        if (error_text)
            memcpy(error_text, "(unavailable)", sizeof("(unavailable)"));
    }
    if (show_error && !error_text) {
        test_mark_failure(__FILE__, __LINE__,
                          "format_error_for_test_output(error) failed");
        goto cleanup;
    }

    printf("    %s\n", label);
    printf("        expected = %s\n", expected_text);
    printf("        got      = %s\n", got_text);
    if (show_error)
        printf("        error    = %s\n", error_text);
    printf("        precision: %zu bits, %zu significant digits\n",
           num_get_prec_bits(got), num_get_prec_digits(got));

cleanup:
    free(error_text);
    if (error_live)
        num_destroy(&error);
    free(got_text);
    free(expected_text);
}

typedef dval_t *(*dv_unary_builder_t)(const dval_t *dv);
typedef dval_t *(*dv_binary_builder_t)(const dval_t *a, const dval_t *b);
typedef number_t (*num_unary_builder_t)(const number_t value);
typedef number_t (*num_binary_builder_t)(const number_t a, const number_t b);

typedef struct {
    const char *name;
    const char *input;
    dv_unary_builder_t dv_fn;
    num_unary_builder_t num_fn;
    const char *deriv_tol_override;
} unary_eval_case_t;

typedef struct {
    const char *name;
    const char *lhs;
    const char *rhs;
    dv_binary_builder_t dv_fn;
    num_binary_builder_t num_fn;
} binary_eval_case_t;

#define UCASE(name_, input_, dv_fn_, num_fn_) \
    { name_, input_, dv_fn_, num_fn_, NULL }

#define UCASE_TOL(name_, input_, dv_fn_, num_fn_, tol_) \
    { name_, input_, dv_fn_, num_fn_, tol_ }

#define GAMMAINV_INPUT_TEXT \
    "1.329340388179137020473625612505858887098162092091790346160355842389683463443274136031212992553908499062170117718211927999677114649293316951893820282202090301346528273989828842137443879771713119671699071534450972100130979"

static dval_t *dv_pow3_builder(const dval_t *x)
{
    return dv_pow_d(x, 3.0);
}

static number_t num_pow3_builder(const number_t x)
{
    number_t three = num_create_from_qfloat(qf_from_double(3.0));
    number_t out = num_pow(x, three);

    num_destroy(&three);
    return out;
}

static qfloat_t qf_from_number_text(const char *text)
{
    number_t value = num_create_from_string(text);
    qfloat_t out = num_to_qfloat(value);

    num_destroy(&value);
    return out;
}

static number_t num_from_qtext(const char *text)
{
    return num_create_from_qfloat(qf_from_number_text(text));
}

static number_t num_from_mfloat_text_bits(const char *text, size_t precision_bits)
{
    mfloat_t *value = mf_create_string(text);
    number_t out;

    if (!value) {
        test_mark_failure(__FILE__, __LINE__, "mf_create_string failed");
        return num_create_from_double(NAN);
    }
    out = num_create_from_mfloat_with_prec_bits(value, precision_bits);
    mf_free(value);
    return out;
}

static number_t num_from_mcomplex_text_bits(const char *text, size_t precision_bits)
{
    mcomplex_t *value = mc_create_string(text);
    number_t out;

    if (!value) {
        test_mark_failure(__FILE__, __LINE__, "mc_create_string failed");
        return num_create_from_double(NAN);
    }
    out = num_create_from_mcomplex_with_prec_bits(value, precision_bits);
    mc_free(value);
    return out;
}

static size_t high_precision_compare_bits(size_t value_bits)
{
    if (value_bits <= 128u)
        return value_bits;
    return value_bits / 2u + 64u;
}

static void assert_same_to_bits(const number_t got,
                                const number_t expected,
                                size_t compare_bits,
                                const char *label)
{
    number_t got_cmp = num_clone(got);
    number_t expected_cmp = num_clone(expected);

    ASSERT_EQ_INT(num_set_prec_bits(&got_cmp, compare_bits), 0);
    ASSERT_EQ_INT(num_set_prec_bits(&expected_cmp, compare_bits), 0);
    if (!num_eq(got_cmp, expected_cmp)) {
        char *got_text = format_number_for_test_output(got);
        char *expected_text = format_number_for_test_output(expected);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    compare precision = %zu bits\n", compare_bits);
        printf("    expected          = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got               = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }
    num_destroy(&expected_cmp);
    num_destroy(&got_cmp);
}

static void check_unary_eval_case(const unary_eval_case_t *tc)
{
    number_t input = num_from_qtext(tc->input);
    dval_t *x = dv_new_var(input);
    dval_t *expr = tc->dv_fn(x);
    number_t got = dv_eval(expr);
    number_t expected = tc->num_fn(input);
    char *got_text;
    char *expected_text;

    if (!num_eq(got, expected)) {
        got_text = format_number_for_test_output(got);
        expected_text = format_number_for_test_output(expected);
        printf(C_BOLD C_RED "FAIL" C_RESET " numeric function sweep: %s\n", tc->name);
        printf("    input    = %s\n", tc->input);
        printf("    expected = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got      = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(expr);
    dv_free(x);
    num_destroy(&input);
}

static void check_binary_eval_case(const binary_eval_case_t *tc)
{
    number_t lhs = num_from_qtext(tc->lhs);
    number_t rhs = num_from_qtext(tc->rhs);
    dval_t *a = dv_new_var(lhs);
    dval_t *b = dv_new_var(rhs);
    dval_t *expr = tc->dv_fn(a, b);
    number_t got = dv_eval(expr);
    number_t expected = tc->num_fn(lhs, rhs);
    char *got_text;
    char *expected_text;

    if (!num_eq(got, expected)) {
        got_text = format_number_for_test_output(got);
        expected_text = format_number_for_test_output(expected);
        printf(C_BOLD C_RED "FAIL" C_RESET " numeric function sweep: %s\n", tc->name);
        printf("    lhs      = %s\n", tc->lhs);
        printf("    rhs      = %s\n", tc->rhs);
        printf("    expected = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got      = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(expr);
    dv_free(b);
    dv_free(a);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_unary_derivative_case(const unary_eval_case_t *tc)
{
    qfloat_t input_q = qf_from_number_text(tc->input);
    number_t input = num_create_from_qfloat(input_q);
    dval_t *x = dv_new_var(input);
    dval_t *expr = tc->dv_fn(x);
    dval_t *deriv = dv_create_deriv(expr, x);
    number_t value;
    number_t grad;
    number_t deriv_value = dv_eval(deriv);
    const dval_t *vars[1] = { x };
    char label[128];

    ASSERT_EQ_INT(dv_eval_derivatives(expr, 1u, vars, &value, &grad), 0);
    snprintf(label, sizeof(label), "numeric derivative sweep: %s", tc->name);
    if (!(num_eq(deriv_value, grad) ||
          (tc->deriv_tol_override
               ? number_close_with_tolerance_text(deriv_value, grad, tc->deriv_tol_override)
               : number_close_for_qfloat_precision(deriv_value, grad)))) {
        char *got_text = format_number_for_test_output(deriv_value);
        char *expected_text = format_number_for_test_output(grad);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got      = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }

    num_destroy(&grad);
    num_destroy(&value);
    num_destroy(&deriv_value);
    dv_free(deriv);
    dv_free(expr);
    dv_free(x);
    num_destroy(&input);
}

static void check_binary_derivative_case(const binary_eval_case_t *tc)
{
    qfloat_t lhs_q = qf_from_number_text(tc->lhs);
    qfloat_t rhs_q = qf_from_number_text(tc->rhs);
    number_t lhs = num_create_from_qfloat(lhs_q);
    number_t rhs = num_create_from_qfloat(rhs_q);
    dval_t *x = dv_new_var(lhs);
    dval_t *y = dv_new_var(rhs);
    dval_t *expr = tc->dv_fn(x, y);
    dval_t *deriv_x = dv_create_deriv(expr, x);
    dval_t *deriv_y = dv_create_deriv(expr, y);
    number_t value;
    number_t grads[2];
    number_t got_dx = dv_eval(deriv_x);
    number_t got_dy = dv_eval(deriv_y);
    const dval_t *vars[2] = { x, y };
    char label[128];

    ASSERT_EQ_INT(dv_eval_derivatives(expr, 2u, vars, &value, grads), 0);
    snprintf(label, sizeof(label), "numeric derivative sweep d/dx: %s", tc->name);
    if (!(num_eq(got_dx, grads[0]) ||
          number_close_for_qfloat_precision(got_dx, grads[0]))) {
        char *got_text = format_number_for_test_output(got_dx);
        char *expected_text = format_number_for_test_output(grads[0]);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got      = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }
    snprintf(label, sizeof(label), "numeric derivative sweep d/dy: %s", tc->name);
    if (!(num_eq(got_dy, grads[1]) ||
          number_close_for_qfloat_precision(got_dy, grads[1]))) {
        char *got_text = format_number_for_test_output(got_dy);
        char *expected_text = format_number_for_test_output(grads[1]);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", expected_text ? expected_text : "(unavailable)");
        printf("    got      = %s\n", got_text ? got_text : "(unavailable)");
        free(expected_text);
        free(got_text);
        TEST_FAIL();
    }

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    num_destroy(&got_dy);
    num_destroy(&got_dx);
    dv_free(deriv_y);
    dv_free(deriv_x);
    dv_free(expr);
    dv_free(y);
    dv_free(x);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_unary_value_case(const unary_eval_case_t *tc,
                                                  size_t value_bits,
                                                  size_t oracle_bits)
{
    number_t input = num_from_mfloat_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_mfloat_text_bits(tc->input, oracle_bits);
    dval_t *x = dv_new_var(input);
    dval_t *oracle_x = dv_new_var(oracle_input);
    dval_t *expr = tc->dv_fn(x);
    dval_t *oracle_expr = tc->dv_fn(oracle_x);
    number_t got = dv_eval(expr);
    number_t expected = dv_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_x);
    dv_free(x);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_binary_value_case(const binary_eval_case_t *tc,
                                                   size_t value_bits,
                                                   size_t oracle_bits)
{
    number_t lhs = num_from_mfloat_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_mfloat_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_mfloat_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_mfloat_text_bits(tc->rhs, oracle_bits);
    dval_t *a = dv_new_var(lhs);
    dval_t *b = dv_new_var(rhs);
    dval_t *oracle_a = dv_new_var(oracle_lhs);
    dval_t *oracle_b = dv_new_var(oracle_rhs);
    dval_t *expr = tc->dv_fn(a, b);
    dval_t *oracle_expr = tc->dv_fn(oracle_a, oracle_b);
    number_t got = dv_eval(expr);
    number_t expected = dv_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_b);
    dv_free(oracle_a);
    dv_free(b);
    dv_free(a);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_unary_derivative_case(const unary_eval_case_t *tc,
                                                       size_t value_bits,
                                                       size_t oracle_bits)
{
    number_t input = num_from_mfloat_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_mfloat_text_bits(tc->input, oracle_bits);
    dval_t *x = dv_new_var(input);
    dval_t *oracle_x = dv_new_var(oracle_input);
    dval_t *expr = tc->dv_fn(x);
    dval_t *oracle_expr = tc->dv_fn(oracle_x);
    dval_t *deriv = dv_create_deriv(expr, x);
    dval_t *oracle_deriv = dv_create_deriv(oracle_expr, oracle_x);
    number_t got = dv_eval(deriv);
    number_t expected = dv_eval(oracle_deriv);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(oracle_deriv);
    dv_free(deriv);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_x);
    dv_free(x);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_binary_derivative_case(const binary_eval_case_t *tc,
                                                        size_t value_bits,
                                                        size_t oracle_bits)
{
    number_t lhs = num_from_mfloat_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_mfloat_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_mfloat_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_mfloat_text_bits(tc->rhs, oracle_bits);
    dval_t *x = dv_new_var(lhs);
    dval_t *y = dv_new_var(rhs);
    dval_t *oracle_x = dv_new_var(oracle_lhs);
    dval_t *oracle_y = dv_new_var(oracle_rhs);
    dval_t *expr = tc->dv_fn(x, y);
    dval_t *oracle_expr = tc->dv_fn(oracle_x, oracle_y);
    dval_t *deriv_x = dv_create_deriv(expr, x);
    dval_t *deriv_y = dv_create_deriv(expr, y);
    dval_t *oracle_deriv_x = dv_create_deriv(oracle_expr, oracle_x);
    dval_t *oracle_deriv_y = dv_create_deriv(oracle_expr, oracle_y);
    number_t got_dx = dv_eval(deriv_x);
    number_t got_dy = dv_eval(deriv_y);
    number_t expected_dx = dv_eval(oracle_deriv_x);
    number_t expected_dy = dv_eval(oracle_deriv_y);

    assert_same_to_bits(got_dx, expected_dx, high_precision_compare_bits(value_bits), tc->name);
    assert_same_to_bits(got_dy, expected_dy, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected_dy);
    num_destroy(&expected_dx);
    num_destroy(&got_dy);
    num_destroy(&got_dx);
    dv_free(oracle_deriv_y);
    dv_free(oracle_deriv_x);
    dv_free(deriv_y);
    dv_free(deriv_x);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_y);
    dv_free(oracle_x);
    dv_free(y);
    dv_free(x);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_complex_unary_value_case(const unary_eval_case_t *tc,
                                                          size_t value_bits,
                                                          size_t oracle_bits)
{
    number_t input = num_from_mcomplex_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_mcomplex_text_bits(tc->input, oracle_bits);
    dval_t *z = dv_new_var(input);
    dval_t *oracle_z = dv_new_var(oracle_input);
    dval_t *expr = tc->dv_fn(z);
    dval_t *oracle_expr = tc->dv_fn(oracle_z);
    number_t got = dv_eval(expr);
    number_t expected = dv_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_z);
    dv_free(z);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_complex_binary_value_case(const binary_eval_case_t *tc,
                                                           size_t value_bits,
                                                           size_t oracle_bits)
{
    number_t lhs = num_from_mcomplex_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_mcomplex_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_mcomplex_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_mcomplex_text_bits(tc->rhs, oracle_bits);
    dval_t *a = dv_new_var(lhs);
    dval_t *b = dv_new_var(rhs);
    dval_t *oracle_a = dv_new_var(oracle_lhs);
    dval_t *oracle_b = dv_new_var(oracle_rhs);
    dval_t *expr = tc->dv_fn(a, b);
    dval_t *oracle_expr = tc->dv_fn(oracle_a, oracle_b);
    number_t got = dv_eval(expr);
    number_t expected = dv_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    dv_free(oracle_expr);
    dv_free(expr);
    dv_free(oracle_b);
    dv_free(oracle_a);
    dv_free(b);
    dv_free(a);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void test_cmp_qfloat_precision(void)
{
    dval_t *a = test_dv_new_const_qf(qf_from_string("1.00000000000000000001"));
    dval_t *b = test_dv_new_const_qf(qf_from_string("1.00000000000000000002"));
    int cmp = dv_cmp(a, b);

    if (cmp < 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " dv_cmp respects qfloat precision\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " dv_cmp lost qfloat precision %s:%d:1 (got %d, expected < 0)\n",
               __FILE__, __LINE__, cmp);
        TEST_FAIL();
    }

    dv_free(b);
    dv_free(a);
}

static void test_new_const_num_preserves_mfloat_precision(void)
{
    mfloat_t *base = mf_create_string("1.25");
    number_t n;
    dval_t *dv;
    number_t got;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mfloat_with_prec_bits(base, 512u);
    dv = dv_new_const(n);
    got = dv_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 512);

    num_destroy(&got);
    dv_free(dv);
    num_destroy(&n);
    mf_free(base);
}

static void test_set_val_num_preserves_mfloat_precision(void)
{
    mfloat_t *base = mf_create_string("1.25");
    number_t n;
    dval_t *dv = test_dv_new_var_d(0.0);
    number_t got;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mfloat_with_prec_bits(base, 640u);
    dv_set_val(dv, n);
    got = dv_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 640);

    num_destroy(&got);
    dv_free(dv);
    num_destroy(&n);
    mf_free(base);
}

static void test_new_const_num_preserves_mcomplex_precision(void)
{
    mcomplex_t *base = mc_create_string("1 + 2i");
    number_t n;
    dval_t *dv;
    number_t got;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mcomplex_with_prec_bits(base, 384u);
    dv = dv_new_const(n);
    got = dv_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_TRUE(!num_is_real(got));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 384);

    num_destroy(&got);
    dv_free(dv);
    num_destroy(&n);
    mc_free(base);
}

static void test_set_val_num_preserves_mcomplex_precision(void)
{
    mcomplex_t *base = mc_create_string("1 + 2i");
    number_t n;
    dval_t *dv = test_dv_new_var_d(0.0);
    number_t got;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mcomplex_with_prec_bits(base, 448u);
    dv_set_val(dv, n);
    got = dv_get_val(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_TRUE(!num_is_real(got));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 448);

    num_destroy(&got);
    dv_free(dv);
    num_destroy(&n);
    mc_free(base);
}

static void test_eval_expression_preserves_mfloat_precision(void)
{
    mfloat_t *base = mf_create_string("1.25");
    number_t n;
    dval_t *x;
    dval_t *sum;
    dval_t *root;
    number_t got_sum;
    number_t got_root;
    number_t check_sum;
    number_t check_root;
    number_t oracle_n;
    number_t expect_sum;
    number_t expect_root;
    size_t oracle_bits;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mfloat_with_prec_bits(base, 512u);
    x = dv_new_var(n);
    sum = dv_add(x, DV_ONE);
    root = dv_sqrt(x);
    got_sum = dv_eval(sum);
    got_root = dv_eval(root);
    check_sum = num_add(n, NUM_ONE);
    check_root = num_sqrt(n);
    oracle_bits = num_get_prec_bits(n) + 384u;
    oracle_n = num_clone(n);
    ASSERT_EQ_INT(num_set_prec_bits(&oracle_n, oracle_bits), 0);
    expect_sum = num_add(oracle_n, NUM_ONE);
    expect_root = num_sqrt(oracle_n);

    {
        char *input_text = format_number_for_test_output(n);

        ASSERT_NOT_NULL(input_text);
        printf(C_BOLD C_GREEN "PASS" C_RESET " high-precision mfloat dval evaluation\n");
        printf("    input    = %s\n", input_text);
        printf("    input precision: %zu bits, %zu significant digits\n",
               num_get_prec_bits(n), num_get_prec_digits(n));
        print_precision_comparison("x + 1", got_sum, expect_sum);
        print_precision_comparison("sqrt(x)", got_root, expect_root);
        printf("\n");
        free(input_text);
    }

    ASSERT_TRUE(num_eq(got_sum, check_sum));
    ASSERT_TRUE(num_eq(got_root, check_root));
    ASSERT_EQ_INT((int)num_get_prec_bits(got_sum), 512);
    ASSERT_EQ_INT((int)num_get_prec_bits(got_root), 512);

    num_destroy(&check_root);
    num_destroy(&check_sum);
    num_destroy(&expect_root);
    num_destroy(&expect_sum);
    num_destroy(&oracle_n);
    num_destroy(&got_root);
    num_destroy(&got_sum);
    dv_free(root);
    dv_free(sum);
    dv_free(x);
    num_destroy(&n);
    mf_free(base);
}

static void test_eval_expression_preserves_mcomplex_precision(void)
{
    mcomplex_t *base = mc_create_string("1 + 2i");
    number_t n;
    dval_t *z;
    dval_t *sum;
    dval_t *exp_z;
    number_t got_sum;
    number_t got_exp;
    number_t check_sum;
    number_t check_exp;
    number_t oracle_n;
    number_t expect_sum;
    number_t expect_exp;
    size_t oracle_bits;

    ASSERT_NOT_NULL(base);

    n = num_create_from_mcomplex_with_prec_bits(base, 384u);
    z = dv_new_var(n);
    sum = dv_add(z, DV_ONE);
    exp_z = dv_exp(z);
    got_sum = dv_eval(sum);
    got_exp = dv_eval(exp_z);
    check_sum = num_add(n, NUM_ONE);
    check_exp = num_exp(n);
    oracle_bits = num_get_prec_bits(n) + 384u;
    oracle_n = num_clone(n);
    ASSERT_EQ_INT(num_set_prec_bits(&oracle_n, oracle_bits), 0);
    expect_sum = num_add(oracle_n, NUM_ONE);
    expect_exp = num_exp(oracle_n);

    {
        char *input_text = format_number_for_test_output(n);

        ASSERT_NOT_NULL(input_text);
        printf(C_BOLD C_GREEN "PASS" C_RESET " high-precision mcomplex dval evaluation\n");
        printf("    input    = %s\n", input_text);
        printf("    input precision: %zu bits, %zu significant digits\n",
               num_get_prec_bits(n), num_get_prec_digits(n));
        print_precision_comparison("z + 1", got_sum, expect_sum);
        print_precision_comparison("exp(z)", got_exp, expect_exp);
        printf("\n");
        free(input_text);
    }

    ASSERT_TRUE(num_eq(got_sum, check_sum));
    ASSERT_TRUE(num_eq(got_exp, check_exp));
    ASSERT_TRUE(!num_is_real(got_sum));
    ASSERT_TRUE(!num_is_real(got_exp));
    ASSERT_EQ_INT((int)num_get_prec_bits(got_sum), 384);
    ASSERT_EQ_INT((int)num_get_prec_bits(got_exp), 384);

    num_destroy(&check_exp);
    num_destroy(&check_sum);
    num_destroy(&expect_exp);
    num_destroy(&expect_sum);
    num_destroy(&oracle_n);
    num_destroy(&got_exp);
    num_destroy(&got_sum);
    dv_free(exp_z);
    dv_free(sum);
    dv_free(z);
    num_destroy(&n);
    mc_free(base);
}

static void test_new_const_num_preserves_qfloat_precision(void)
{
    qfloat_t q = qf_from_string("1.00000000000000000001");
    number_t n = num_create_from_qfloat(q);
    dval_t *dv = dv_new_const(n);
    qfloat_t got = dv_eval_qf(dv);

    check_q_at(__FILE__, __LINE__, 1, "dv_new_const preserves qfloat precision", got, q);

    dv_free(dv);
    num_destroy(&n);
}

static void test_set_val_num_preserves_qfloat_precision(void)
{
    qfloat_t q = qf_from_string("1.00000000000000000001");
    number_t n = num_create_from_qfloat(q);
    dval_t *dv = test_dv_new_var_d(0.0);

    dv_set_val(dv, n);
    check_q_at(__FILE__, __LINE__, 1, "dv_set_val preserves qfloat precision",
               dv_eval_qf(dv), q);

    dv_free(dv);
    num_destroy(&n);
}

static void test_default_constants_preserve_builtin_precision(void)
{
    dval_t *pi = dval_from_string("{ pi }", NULL);
    dval_t *e = dval_from_string("{ e }", NULL);
    dval_t *phi = dval_from_string("{ @phi }", NULL);
    number_t phi_value = NUM_ZERO;

    ASSERT_NOT_NULL(pi);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(phi);

    check_q_at(__FILE__, __LINE__, 1, "dval pi uses qfloat precision", dv_eval_qf(pi), QF_PI);
    check_q_at(__FILE__, __LINE__, 1, "dval e uses qfloat precision", dv_eval_qf(e), QF_E);
    phi_value = dv_eval(phi);
    ASSERT_EQ_INT((int)num_get_prec_bits(phi_value), (int)num_get_default_prec_bits());
    check_q_at(__FILE__, __LINE__, 1, "dval phi preserves builtin value",
               num_to_qfloat(phi_value), QF_PHI);

    num_destroy(&phi_value);
    dv_free(phi);
    dv_free(e);
    dv_free(pi);
}

static void test_dv_ln10_singleton(void)
{
    number_t got = dv_eval(DV_LN10);
    number_t expected = num_const(NUM_LN10);
    char *text = dv_to_string(DV_LN10, style_EXPRESSION);

    ASSERT_TRUE(num_eq(got, expected));
    ASSERT_TRUE(text && strstr(text, "ln10") != NULL);

    free(text);
    num_destroy(&expected);
    num_destroy(&got);
}

static void test_get_val_updates_after_set(void)
{
    dval_t *x = test_dv_new_var_d(1.0);
    dval_t *f = dv_add_d(x, 2.0);

    check_q_at(__FILE__, __LINE__, 1, "dv_get_val initial", dv_get_val_qf(f), qf_from_double(3.0));
    test_dv_set_val_d(x, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "dv_get_val after set", dv_get_val_qf(f), qf_from_double(7.0));

    dv_free(f);
    dv_free(x);
}

static void test_new_const_num(void)
{
    number_t half = num_create_from_string("1/2");
    dval_t *c_half = dv_new_const(half);
    number_t got_half = dv_get_val(c_half);
    number_t eval_half = dv_eval(c_half);

    ASSERT_TRUE(num_eq(got_half, half));
    ASSERT_TRUE(num_eq(eval_half, half));

    num_destroy(&got_half);
    num_destroy(&eval_half);
    dv_free(c_half);
    num_destroy(&half);
}

static void test_new_const_num_rational_complex(void)
{
    number_t z = num_create_from_string("1/2 - 3/2i");
    dval_t *c_z = dv_new_const(z);
    number_t got_z = dv_get_val(c_z);
    number_t eval_z = dv_eval(c_z);

    ASSERT_TRUE(num_eq(got_z, z));
    ASSERT_TRUE(num_eq(eval_z, z));

    num_destroy(&eval_z);
    num_destroy(&got_z);
    dv_free(c_z);
    num_destroy(&z);
}

static void test_new_var_num_and_set_val_num(void)
{
    number_t one_plus_i = num_create_from_string("1 + i");
    number_t minus_i = num_create_from_string("-i");
    number_t quarter = num_create_from_string("1/4");
    number_t rational_complex = num_create_from_string("1/2 - 3/2i");
    dval_t *v = dv_new_var(one_plus_i);
    number_t got_var = dv_eval(v);

    ASSERT_TRUE(num_eq(got_var, one_plus_i));

    dv_set_val(v, minus_i);
    num_destroy(&got_var);
    got_var = dv_get_val(v);
    ASSERT_TRUE(num_eq(got_var, minus_i));

    dv_set_val(v, quarter);
    num_destroy(&got_var);
    got_var = dv_eval(v);
    ASSERT_TRUE(num_eq(got_var, quarter));

    dv_set_val(v, rational_complex);
    num_destroy(&got_var);
    got_var = dv_get_val(v);
    ASSERT_TRUE(num_eq(got_var, rational_complex));

    num_destroy(&got_var);
    dv_free(v);
    num_destroy(&rational_complex);
    num_destroy(&quarter);
    num_destroy(&minus_i);
    num_destroy(&one_plus_i);
}

static void test_named_number_constructors(void)
{
    number_t two = num_create_from_string("2");
    number_t z_value = num_create_from_string("2 + 3i");
    dval_t *named_const = dv_new_named_const(two, "two");
    dval_t *named_var = dv_new_named_var(z_value, "z");
    number_t got_const = dv_get_val(named_const);
    number_t got_var = dv_get_val(named_var);
    char *const_text = dv_to_string(named_const, style_EXPRESSION);
    char *var_text = dv_to_string(named_var, style_EXPRESSION);

    ASSERT_NOT_NULL(const_text);
    ASSERT_NOT_NULL(var_text);
    ASSERT_TRUE(strstr(const_text, "two") != NULL);
    ASSERT_TRUE(strstr(var_text, "z") != NULL);
    ASSERT_TRUE(num_eq(got_const, two));
    ASSERT_TRUE(num_eq(got_var, z_value));

    free(var_text);
    free(const_text);
    num_destroy(&got_var);
    num_destroy(&got_const);
    dv_free(named_var);
    dv_free(named_const);
    num_destroy(&z_value);
    num_destroy(&two);
}

static void test_eval_num_on_expression(void)
{
    number_t half = num_create_from_string("1/2");
    number_t two = num_create_from_string("2");
    number_t expected = num_create_from_string("5/2");
    dval_t *x = dv_new_var(half);
    dval_t *c = dv_new_const(two);
    dval_t *sum = dv_add(x, c);
    number_t got = dv_eval(sum);

    ASSERT_TRUE(num_eq(got, expected));

    num_destroy(&got);
    dv_free(sum);
    dv_free(c);
    dv_free(x);
    num_destroy(&expected);
    num_destroy(&two);
    num_destroy(&half);
}

static void test_eval_num_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        { "sin", "0.5", dv_sin, num_sin, NULL },
        { "cos", "0.5", dv_cos, num_cos, NULL },
        { "tan", "0.5", dv_tan, num_tan, NULL },
        { "sinh", "0.5", dv_sinh, num_sinh, NULL },
        { "cosh", "0.5", dv_cosh, num_cosh, NULL },
        { "tanh", "0.5", dv_tanh, num_tanh, NULL },
        { "asin", "0.25", dv_asin, num_asin, NULL },
        { "acos", "0.25", dv_acos, num_acos, NULL },
        { "atan", "0.25", dv_atan, num_atan, NULL },
        { "asinh", "0.25", dv_asinh, num_asinh, NULL },
        { "acosh", "1.25", dv_acosh, num_acosh, NULL },
        { "atanh", "0.25", dv_atanh, num_atanh, NULL },
        { "exp", "1.5", dv_exp, num_exp, NULL },
        { "log", "1.5", dv_log, num_log, NULL },
        { "log10", "1.5", dv_log10, num_log10, NULL },
        { "sqrt", "2.0", dv_sqrt, num_sqrt, NULL },
        { "pow_d", "2.0", dv_pow3_builder, num_pow3_builder, NULL },
        { "abs", "-3.0", dv_abs, num_abs, NULL },
        { "erf", "0.8", dv_erf, num_erf, NULL },
        { "erfc", "1.2", dv_erfc, num_erfc, NULL },
        { "erfinv", "0.5", dv_erfinv, num_erfinv, NULL },
        { "erfcinv", "0.4", dv_erfcinv, num_erfcinv, NULL },
        { "gamma", "2.5", dv_gamma, num_gamma, NULL },
        { "gammainv", GAMMAINV_INPUT_TEXT, dv_gammainv, num_gammainv, NULL },
        { "lgamma", "2.5", dv_lgamma, num_lgamma, NULL },
        { "digamma", "2.5", dv_digamma, num_digamma, NULL },
        { "trigamma", "2.5", dv_trigamma, num_trigamma, NULL },
        { "W₀", "0.2", dv_lambert_w0, num_lambert_w0, NULL },
        UCASE_TOL("W₋₁", "-0.1", dv_lambert_wm1, num_lambert_wm1, "1e-30"),
        { "normal_pdf", "1.0", dv_normal_pdf, num_normal_pdf, NULL },
        { "normal_cdf", "1.0", dv_normal_cdf, num_normal_cdf, NULL },
        { "normal_logpdf", "1.0", dv_normal_logpdf, num_normal_logpdf, NULL },
        { "ei", "1.0", dv_ei, num_ei, NULL },
        { "e1", "1.0", dv_e1, num_e1, NULL }
    };
    static const binary_eval_case_t binary_cases[] = {
        { "atan2", "2.0", "3.0", dv_atan2, num_atan2 },
        { "pow", "2.0", "3.0", dv_pow_dv, num_pow },
        { "hypot", "3.0", "4.0", dv_hypot, num_hypot },
        { "beta", "2.5", "1.5", dv_beta, num_beta },
        { "logbeta", "2.5", "1.5", dv_logbeta, num_logbeta }
    };
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_unary_eval_case(&unary_cases[i]);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_binary_eval_case(&binary_cases[i]);
}

static void test_eval_num_function_derivatives(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", dv_sin, num_sin),
        UCASE("cos", "0.5", dv_cos, num_cos),
        UCASE("tan", "0.5", dv_tan, num_tan),
        UCASE("sinh", "0.5", dv_sinh, num_sinh),
        UCASE("cosh", "0.5", dv_cosh, num_cosh),
        UCASE("tanh", "0.5", dv_tanh, num_tanh),
        UCASE("asin", "0.25", dv_asin, num_asin),
        UCASE("acos", "0.25", dv_acos, num_acos),
        UCASE("atan", "0.25", dv_atan, num_atan),
        UCASE("asinh", "0.25", dv_asinh, num_asinh),
        UCASE("acosh", "1.25", dv_acosh, num_acosh),
        UCASE("atanh", "0.25", dv_atanh, num_atanh),
        UCASE("exp", "1.5", dv_exp, num_exp),
        UCASE("log", "1.5", dv_log, num_log),
        UCASE("log10", "1.5", dv_log10, num_log10),
        UCASE("sqrt", "2.0", dv_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", dv_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", dv_abs, num_abs),
        UCASE("erf", "0.8", dv_erf, num_erf),
        UCASE("erfc", "1.2", dv_erfc, num_erfc),
        UCASE("erfinv", "0.5", dv_erfinv, num_erfinv),
        UCASE("erfcinv", "0.4", dv_erfcinv, num_erfcinv),
        UCASE("gamma", "2.5", dv_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, dv_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", dv_lgamma, num_lgamma),
        UCASE("digamma", "2.5", dv_digamma, num_digamma),
        UCASE("trigamma", "2.5", dv_trigamma, num_trigamma),
        UCASE("W₀", "0.2", dv_lambert_w0, num_lambert_w0),
        UCASE_TOL("W₋₁", "-0.1", dv_lambert_wm1, num_lambert_wm1, "1e-30"),
        UCASE("normal_pdf", "1.0", dv_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", dv_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", dv_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", dv_ei, num_ei),
        UCASE("e1", "1.0", dv_e1, num_e1)
    };
    static const binary_eval_case_t binary_cases[] = {
        { "atan2", "2.0", "3.0", dv_atan2, num_atan2 },
        { "pow", "2.0", "3.0", dv_pow_dv, num_pow },
        { "hypot", "3.0", "4.0", dv_hypot, num_hypot },
        { "beta", "2.5", "1.5", dv_beta, num_beta },
        { "logbeta", "2.5", "1.5", dv_logbeta, num_logbeta }
    };
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_unary_derivative_case(&unary_cases[i]);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_binary_derivative_case(&binary_cases[i]);
}

static void test_high_precision_mfloat_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", dv_sin, num_sin),
        UCASE("cos", "0.5", dv_cos, num_cos),
        UCASE("sinh", "0.5", dv_sinh, num_sinh),
        UCASE("cosh", "0.5", dv_cosh, num_cosh),
        UCASE("tanh", "0.5", dv_tanh, num_tanh),
        UCASE("asin", "0.25", dv_asin, num_asin),
        UCASE("acos", "0.25", dv_acos, num_acos),
        UCASE("atan", "0.25", dv_atan, num_atan),
        UCASE("asinh", "0.25", dv_asinh, num_asinh),
        UCASE("acosh", "1.25", dv_acosh, num_acosh),
        UCASE("atanh", "0.25", dv_atanh, num_atanh),
        UCASE("log", "1.5", dv_log, num_log),
        UCASE("log10", "1.5", dv_log10, num_log10),
        UCASE("sqrt", "2.0", dv_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", dv_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", dv_abs, num_abs),
        UCASE("erf", "0.8", dv_erf, num_erf),
        UCASE("erfc", "1.2", dv_erfc, num_erfc),
        UCASE("erfinv", "0.5", dv_erfinv, num_erfinv),
        UCASE("erfcinv", "0.4", dv_erfcinv, num_erfcinv),
        UCASE("gamma", "2.5", dv_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, dv_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", dv_lgamma, num_lgamma),
        UCASE("digamma", "2.5", dv_digamma, num_digamma),
        UCASE("trigamma", "2.5", dv_trigamma, num_trigamma),
        UCASE("W₀", "0.2", dv_lambert_w0, num_lambert_w0),
        UCASE("W₋₁", "-0.1", dv_lambert_wm1, num_lambert_wm1),
        UCASE("normal_pdf", "1.0", dv_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", dv_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", dv_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", dv_ei, num_ei),
        UCASE("e1", "1.0", dv_e1, num_e1)
    };
    static const binary_eval_case_t binary_cases[] = {
        { "atan2", "2.0", "3.0", dv_atan2, num_atan2 },
        { "pow", "2.0", "3.0", dv_pow_dv, num_pow },
        { "hypot", "3.0", "4.0", dv_hypot, num_hypot },
        { "beta", "2.5", "1.5", dv_beta, num_beta },
        { "logbeta", "2.5", "1.5", dv_logbeta, num_logbeta }
    };
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_high_precision_unary_value_case(&unary_cases[i], 512u, 896u);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_high_precision_binary_value_case(&binary_cases[i], 512u, 896u);
}

static void test_high_precision_mfloat_function_derivatives(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", dv_sin, num_sin),
        UCASE("cos", "0.5", dv_cos, num_cos),
        UCASE("sinh", "0.5", dv_sinh, num_sinh),
        UCASE("cosh", "0.5", dv_cosh, num_cosh),
        UCASE("tanh", "0.5", dv_tanh, num_tanh),
        UCASE("asin", "0.25", dv_asin, num_asin),
        UCASE("acos", "0.25", dv_acos, num_acos),
        UCASE("atan", "0.25", dv_atan, num_atan),
        UCASE("asinh", "0.25", dv_asinh, num_asinh),
        UCASE("acosh", "1.25", dv_acosh, num_acosh),
        UCASE("atanh", "0.25", dv_atanh, num_atanh),
        UCASE("log", "1.5", dv_log, num_log),
        UCASE("log10", "1.5", dv_log10, num_log10),
        UCASE("sqrt", "2.0", dv_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", dv_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", dv_abs, num_abs),
        UCASE("gamma", "2.5", dv_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, dv_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", dv_lgamma, num_lgamma),
        UCASE("digamma", "2.5", dv_digamma, num_digamma),
        UCASE("trigamma", "2.5", dv_trigamma, num_trigamma),
        UCASE("W₀", "0.2", dv_lambert_w0, num_lambert_w0),
        UCASE("W₋₁", "-0.1", dv_lambert_wm1, num_lambert_wm1),
        UCASE("normal_pdf", "1.0", dv_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", dv_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", dv_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", dv_ei, num_ei),
        UCASE("e1", "1.0", dv_e1, num_e1)
    };
    static const binary_eval_case_t binary_cases[] = {
        { "atan2", "2.0", "3.0", dv_atan2, num_atan2 },
        { "pow", "2.0", "3.0", dv_pow_dv, num_pow },
        { "hypot", "3.0", "4.0", dv_hypot, num_hypot },
        { "beta", "2.5", "1.5", dv_beta, num_beta },
        { "logbeta", "2.5", "1.5", dv_logbeta, num_logbeta }
    };
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_high_precision_unary_derivative_case(&unary_cases[i], 512u, 896u);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_high_precision_binary_derivative_case(&binary_cases[i], 512u, 896u);
}

static void test_high_precision_mcomplex_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "1 + 2i", dv_sin, num_sin),
        UCASE("cos", "1 + 2i", dv_cos, num_cos),
        UCASE("tan", "1 + 2i", dv_tan, num_tan),
        UCASE("sinh", "1 + 2i", dv_sinh, num_sinh),
        UCASE("cosh", "1 + 2i", dv_cosh, num_cosh),
        UCASE("tanh", "1 + 2i", dv_tanh, num_tanh),
        UCASE("exp", "1 + 2i", dv_exp, num_exp),
        UCASE("log", "1 + 2i", dv_log, num_log),
        UCASE("log10", "1 + 2i", dv_log10, num_log10),
        UCASE("sqrt", "1 + 2i", dv_sqrt, num_sqrt)
    };
    static const binary_eval_case_t binary_cases[] = {
        { "pow", "1 + 2i", "2 - i", dv_pow_dv, num_pow }
    };
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_high_precision_complex_unary_value_case(&unary_cases[i], 384u, 768u);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_high_precision_complex_binary_value_case(&binary_cases[i], 384u, 768u);
}

static void test_set_val_num_named_constant(void)
{
    number_t old_value = num_create_from_string("2");
    number_t new_value = num_create_from_string("1/2 - 3/2i");
    dval_t *named_const = dv_new_named_const(old_value, "c");
    number_t got_value;

    dv_set_val(named_const, new_value);
    got_value = dv_eval(named_const);

    ASSERT_TRUE(num_eq(got_value, new_value));

    num_destroy(&got_value);
    dv_free(named_const);
    num_destroy(&new_value);
    num_destroy(&old_value);
}

static void test_simplify_inverse_unary_pairs(void)
{
    dval_t *x = test_dv_new_named_var_d(3.0, "x");
    dval_t *log_x = dv_log(x);
    dval_t *exp_log_x = dv_exp(log_x);
    dval_t *exp_x = dv_exp(x);
    dval_t *log_exp_x = dv_log(exp_x);
    dval_t *ten = dv_new_const(NUM_TEN);
    dval_t *log10_x = dv_log10(x);
    dval_t *ten_pow_log10_x = dv_pow_dv(ten, log10_x);
    dval_t *ten_pow_x = dv_pow_dv(ten, x);
    dval_t *log10_ten_pow_x = dv_log10(ten_pow_x);
    dval_t *exp_log_simp = dv_simplify(exp_log_x);
    dval_t *log_exp_simp = dv_simplify(log_exp_x);
    dval_t *ten_pow_log10_simp = dv_simplify(ten_pow_log10_x);
    dval_t *log10_ten_pow_simp = dv_simplify(log10_ten_pow_x);
    char *exp_log_s = dv_to_string(exp_log_simp, style_EXPRESSION);
    char *log_exp_s = dv_to_string(log_exp_simp, style_EXPRESSION);
    char *ten_pow_log10_s = dv_to_string(ten_pow_log10_simp, style_EXPRESSION);
    char *log10_ten_pow_s = dv_to_string(log10_ten_pow_simp, style_EXPRESSION);
    const char *expect = "{ x | x = 3 }";

    check_q_at(__FILE__, __LINE__, 1, "exp(log(x)) eval", dv_eval_qf(exp_log_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log(exp(x)) eval", dv_eval_qf(log_exp_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "10^log10(x) eval", dv_eval_qf(ten_pow_log10_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log10(10^x) eval", dv_eval_qf(log10_ten_pow_x), qf_from_double(3.0));

    if (str_eq(exp_log_s, expect))
        to_string_pass("exp(log(x)) simplification (EXPR)", exp_log_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(log(x)) simplification (EXPR)", exp_log_s, expect);

    if (str_eq(log_exp_s, expect))
        to_string_pass("log(exp(x)) simplification (EXPR)", log_exp_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(exp(x)) simplification (EXPR)", log_exp_s, expect);

    if (str_eq(ten_pow_log10_s, expect))
        to_string_pass("10^log10(x) simplification (EXPR)", ten_pow_log10_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "10^log10(x) simplification (EXPR)", ten_pow_log10_s, expect);

    if (str_eq(log10_ten_pow_s, expect))
        to_string_pass("log10(10^x) simplification (EXPR)", log10_ten_pow_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log10(10^x) simplification (EXPR)", log10_ten_pow_s, expect);

    free(log10_ten_pow_s);
    free(ten_pow_log10_s);
    free(log_exp_s);
    free(exp_log_s);
    dv_free(log10_ten_pow_simp);
    dv_free(ten_pow_log10_simp);
    dv_free(log_exp_simp);
    dv_free(exp_log_simp);
    dv_free(log10_ten_pow_x);
    dv_free(ten_pow_x);
    dv_free(ten_pow_log10_x);
    dv_free(log10_x);
    dv_free(ten);
    dv_free(log_exp_x);
    dv_free(exp_x);
    dv_free(exp_log_x);
    dv_free(log_x);
    dv_free(x);
}

static void test_simplify_exp_quarter_turns(void)
{
    dval_t *pi = test_dv_new_named_const_qf(QF_PI, "@pi");
    dval_t *i = test_dv_new_named_const_qc(QC_I, "i");
    dval_t *pi_i = dv_mul(pi, i);
    dval_t *half = test_dv_new_const_d(2.0);
    dval_t *pi_i_over_2 = dv_div(pi_i, half);
    dval_t *exp_pi_i_over_2 = dv_exp(pi_i_over_2);
    dval_t *simp = dv_simplify(exp_pi_i_over_2);
    char *expr_s = dv_to_string(simp, style_EXPRESSION);
    const char *expect = "i";

    if (str_eq(expr_s, expect))
        to_string_pass("exp(pi*i/2) simplification (EXPR)", expr_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(pi*i/2) simplification (EXPR)", expr_s, expect);

    free(expr_s);
    dv_free(simp);
    dv_free(exp_pi_i_over_2);
    dv_free(pi_i_over_2);
    dv_free(half);
    dv_free(pi_i);
    dv_free(i);
    dv_free(pi);
}

static void test_to_string_does_not_simplify_plain_expressions(void)
{
    dval_t *x = test_dv_new_named_var_d(3.0, "x");
    dval_t *xx = dv_mul(x, x);
    dval_t *dx = dv_create_deriv(xx, x);
    char *expr_text = dv_to_string(xx, style_EXPRESSION);
    char *deriv_text = dx ? dv_to_string(dx, style_EXPRESSION) : NULL;
    const char *expr_expect = "{ xx | x = 3 }";
    const char *deriv_expect = "{ 2x | x = 3 }";

    if (str_eq(expr_text, expr_expect))
        to_string_pass("plain to_string preserves x*x", expr_text, expr_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1,
                       "plain to_string preserves x*x",
                       expr_text ? expr_text : "(null)", expr_expect);

    if (deriv_text && str_eq(deriv_text, deriv_expect))
        to_string_pass("derivative creation still simplifies (x*x)'", deriv_text, deriv_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1,
                       "derivative creation still simplifies (x*x)'",
                       deriv_text ? deriv_text : "(null)", deriv_expect);

    free(deriv_text);
    free(expr_text);
    dv_free(dx);
    dv_free(xx);
    dv_free(x);
}

void test_runtime_regressions(void)
{
    TEST_RUN_SUBTEST(test_cmp_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_mfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_mfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_mcomplex_precision, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_mcomplex_precision, NULL);
    TEST_RUN_SUBTEST(test_eval_expression_preserves_mfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_eval_expression_preserves_mcomplex_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_default_constants_preserve_builtin_precision, NULL);
    TEST_RUN_SUBTEST(test_dv_ln10_singleton, NULL);
    TEST_RUN_SUBTEST(test_get_val_updates_after_set, NULL);
    TEST_RUN_SUBTEST(test_new_const_num, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_rational_complex, NULL);
    TEST_RUN_SUBTEST(test_new_var_num_and_set_val_num, NULL);
    TEST_RUN_SUBTEST(test_named_number_constructors, NULL);
    TEST_RUN_SUBTEST(test_eval_num_on_expression, NULL);
    TEST_RUN_SUBTEST(test_eval_num_function_values, NULL);
    TEST_RUN_SUBTEST(test_eval_num_function_derivatives, NULL);
    TEST_RUN_SUBTEST(test_high_precision_mfloat_function_values, NULL);
    TEST_RUN_SUBTEST(test_high_precision_mfloat_function_derivatives, NULL);
    TEST_RUN_SUBTEST(test_high_precision_mcomplex_function_values, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_named_constant, NULL);
    TEST_RUN_SUBTEST(test_to_string_does_not_simplify_plain_expressions, NULL);
    TEST_RUN_SUBTEST(test_simplify_inverse_unary_pairs, NULL);
    TEST_RUN_SUBTEST(test_simplify_exp_quarter_turns, NULL);
}

/* ------------------------------------------------------------------------- */
/* Reverse mode                                                              */
/* ------------------------------------------------------------------------- */
