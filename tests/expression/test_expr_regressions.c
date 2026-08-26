#include "test_expr.h"

#include <math.h>

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static string_t *format_number_at_own_precision(const number_t value)
{
    char fmt[32];
    char *out;
    string_t *text;
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
    text = string_new_with(out);
    free(out);
    return text;
}

static string_t *format_number_for_test_output(const number_t value)
{
    string_t *text = format_number_at_own_precision(value);

    return text ? text : num_to_string(value);
}

static const char *formatted_number_cstr(const string_t *text)
{
    return text ? string_c_str(text) : "(unavailable)";
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

static number_t oracle_error_magnitude(const number_t got, const number_t expected)
{
    number_t promoted_got = num_clone(got);
    number_t diff;
    number_t error;

    if (num_get_prec_bits(expected) > 0u && num_set_prec_bits(&promoted_got, num_get_prec_bits(expected)) != 0) {
        test_mark_failure(__FILE__, __LINE__, "num_set_prec_bits(promoted_got) failed");
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

static int number_close_for_qfloat_precision(const number_t got, const number_t expected)
{
    number_t error = oracle_error_magnitude(got, expected);
    number_t one = num_create_from_double(1.0);
    number_t tolerance;
    int ok;

    if (num_set_prec_bits(&one, 106u) != 0) {
        test_mark_failure(__FILE__, __LINE__, "num_set_prec_bits(one) failed");
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

static int number_close_with_tolerance_text(const number_t got, const number_t expected, const char *tolerance_text)
{
    number_t error = oracle_error_magnitude(got, expected);
    number_t tolerance = num_create_from_string(tolerance_text);
    int ok = num_le(error, tolerance);

    num_destroy(&tolerance);
    num_destroy(&error);
    return ok;
}

static void print_precision_comparison(const char *label, const number_t got, const number_t expected)
{
    string_t *expected_text;
    string_t *got_text;
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
        test_mark_failure(__FILE__, __LINE__, "format_number_for_test_output(expected) failed");
        goto cleanup;
    }
    if (!got_text) {
        test_mark_failure(__FILE__, __LINE__, "format_number_for_test_output(got) failed");
        goto cleanup;
    }
    if (show_error && !error_text) {
        error_text = malloc(sizeof("(unavailable)"));
        if (error_text)
            memcpy(error_text, "(unavailable)", sizeof("(unavailable)"));
    }
    if (show_error && !error_text) {
        test_mark_failure(__FILE__, __LINE__, "format_error_for_test_output(error) failed");
        goto cleanup;
    }

    printf("    %s\n", label);
    printf("        expected = %s\n", formatted_number_cstr(expected_text));
    printf("        got      = %s\n", formatted_number_cstr(got_text));
    if (show_error)
        printf("        error    = %s\n", error_text);
    printf("        precision: %zu bits, %zu significant digits\n", num_get_prec_bits(got), num_get_prec_digits(got));

cleanup:
    free(error_text);
    if (error_live)
        num_destroy(&error);
    string_free(got_text);
    string_free(expected_text);
}

typedef expr_t *(*expr_unary_builder_t)(const expr_t *dv);
typedef expr_t *(*expr_binary_builder_t)(const expr_t *a, const expr_t *b);
typedef number_t (*num_unary_builder_t)(const number_t value);
typedef number_t (*num_binary_builder_t)(const number_t a, const number_t b);

typedef struct {
    const char *name;
    const char *input;
    expr_unary_builder_t expr_fn;
    num_unary_builder_t num_fn;
    const char *deriv_tol_override;
} unary_eval_case_t;

typedef struct {
    const char *name;
    const char *lhs;
    const char *rhs;
    expr_binary_builder_t expr_fn;
    num_binary_builder_t num_fn;
} binary_eval_case_t;

#define UCASE(name_, input_, expr_fn_, num_fn_) {name_, input_, expr_fn_, num_fn_, NULL}

#define UCASE_TOL(name_, input_, expr_fn_, num_fn_, tol_) {name_, input_, expr_fn_, num_fn_, tol_}

#define GAMMAINV_INPUT_TEXT                                                                                            \
    "1."                                                                                                               \
    "3293403881791370204736256125058588870981620920917903461603558423896834634432741360312129925539084990621701177182" \
    "11927999677114649293316951893820282202090301346528273989828842137443879771713119671699071534450972100130979"

static expr_t *expr_pow3_builder(const expr_t *x)
{
    return expr_pow_d(x, 3.0);
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

static number_t num_from_text_bits(const char *text, size_t precision_bits)
{
    number_t out = num_create_from_string(text);

    if (num_set_prec_bits(&out, precision_bits) != 0) {
        num_destroy(&out);
        test_mark_failure(__FILE__, __LINE__, "num_create_from_string/num_set_prec_bits failed");
        return num_create_from_double(NAN);
    }
    return out;
}

static size_t high_precision_compare_bits(size_t value_bits)
{
    if (value_bits <= 128u)
        return value_bits;
    return value_bits / 2u + 64u;
}

static void assert_same_to_bits(const number_t got, const number_t expected, size_t compare_bits, const char *label)
{
    number_t got_cmp = num_clone(got);
    number_t expected_cmp = num_clone(expected);

    ASSERT_EQ_INT(num_set_prec_bits(&got_cmp, compare_bits), 0);
    ASSERT_EQ_INT(num_set_prec_bits(&expected_cmp, compare_bits), 0);
    if (!num_eq(got_cmp, expected_cmp)) {
        string_t *got_text = format_number_for_test_output(got);
        string_t *expected_text = format_number_for_test_output(expected);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    compare precision = %zu bits\n", compare_bits);
        printf("    expected          = %s\n", formatted_number_cstr(expected_text));
        printf("    got               = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }
    num_destroy(&expected_cmp);
    num_destroy(&got_cmp);
}

static void check_unary_eval_case(const unary_eval_case_t *tc)
{
    number_t input = num_from_qtext(tc->input);
    expr_t *x = expr_new_var(input);
    expr_t *expr = tc->expr_fn(x);
    number_t got = expr_eval(expr);
    number_t expected = tc->num_fn(input);
    string_t *got_text;
    string_t *expected_text;

    if (!num_eq(got, expected)) {
        got_text = format_number_for_test_output(got);
        expected_text = format_number_for_test_output(expected);
        printf(C_BOLD C_RED "FAIL" C_RESET " numeric function sweep: %s\n", tc->name);
        printf("    input    = %s\n", tc->input);
        printf("    expected = %s\n", formatted_number_cstr(expected_text));
        printf("    got      = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(expr);
    expr_free(x);
    num_destroy(&input);
}

static void check_binary_eval_case(const binary_eval_case_t *tc)
{
    number_t lhs = num_from_qtext(tc->lhs);
    number_t rhs = num_from_qtext(tc->rhs);
    expr_t *a = expr_new_var(lhs);
    expr_t *b = expr_new_var(rhs);
    expr_t *expr = tc->expr_fn(a, b);
    number_t got = expr_eval(expr);
    number_t expected = tc->num_fn(lhs, rhs);
    string_t *got_text;
    string_t *expected_text;

    if (!num_eq(got, expected)) {
        got_text = format_number_for_test_output(got);
        expected_text = format_number_for_test_output(expected);
        printf(C_BOLD C_RED "FAIL" C_RESET " numeric function sweep: %s\n", tc->name);
        printf("    lhs      = %s\n", tc->lhs);
        printf("    rhs      = %s\n", tc->rhs);
        printf("    expected = %s\n", formatted_number_cstr(expected_text));
        printf("    got      = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(expr);
    expr_free(b);
    expr_free(a);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void test_removable_trig_quotient_at_zero_evaluates_to_limit(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ sin(5*x)/sin(9*x) | x = 0 }", &bindings);
    number_t value = expr ? expr_eval(expr) : num_clone(NUM_NAN);
    number_t five = num_create_from_long(5);
    number_t nine = num_create_from_long(9);
    number_t expected = num_div(five, nine);

    ASSERT_TRUE(num_eq(value, expected));

    num_destroy(&expected);
    num_destroy(&nine);
    num_destroy(&five);
    num_destroy(&value);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void check_unary_derivative_case(const unary_eval_case_t *tc)
{
    qfloat_t input_q = qf_from_number_text(tc->input);
    number_t input = num_create_from_qfloat(input_q);
    expr_t *x = expr_new_var(input);
    expr_t *expr = tc->expr_fn(x);
    expr_t *deriv = expr_create_deriv(expr, x);
    number_t value;
    number_t grad;
    number_t deriv_value = expr_eval(deriv);
    const expr_t *vars[1] = {x};
    char label[128];

    ASSERT_EQ_INT(expr_eval_derivatives(expr, 1u, vars, &value, &grad), 0);
    snprintf(label, sizeof(label), "numeric derivative sweep: %s", tc->name);
    if (!(num_eq(deriv_value, grad) ||
          (tc->deriv_tol_override ? number_close_with_tolerance_text(deriv_value, grad, tc->deriv_tol_override)
                                  : number_close_for_qfloat_precision(deriv_value, grad)))) {
        string_t *got_text = format_number_for_test_output(deriv_value);
        string_t *expected_text = format_number_for_test_output(grad);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", formatted_number_cstr(expected_text));
        printf("    got      = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }

    num_destroy(&grad);
    num_destroy(&value);
    num_destroy(&deriv_value);
    expr_free(deriv);
    expr_free(expr);
    expr_free(x);
    num_destroy(&input);
}

static void check_binary_derivative_case(const binary_eval_case_t *tc)
{
    qfloat_t lhs_q = qf_from_number_text(tc->lhs);
    qfloat_t rhs_q = qf_from_number_text(tc->rhs);
    number_t lhs = num_create_from_qfloat(lhs_q);
    number_t rhs = num_create_from_qfloat(rhs_q);
    expr_t *x = expr_new_var(lhs);
    expr_t *y = expr_new_var(rhs);
    expr_t *expr = tc->expr_fn(x, y);
    expr_t *deriv_x = expr_create_deriv(expr, x);
    expr_t *deriv_y = expr_create_deriv(expr, y);
    number_t value;
    number_t grads[2];
    number_t got_dx = expr_eval(deriv_x);
    number_t got_dy = expr_eval(deriv_y);
    const expr_t *vars[2] = {x, y};
    char label[128];

    ASSERT_EQ_INT(expr_eval_derivatives(expr, 2u, vars, &value, grads), 0);
    snprintf(label, sizeof(label), "numeric derivative sweep d/dx: %s", tc->name);
    if (!(num_eq(got_dx, grads[0]) || number_close_for_qfloat_precision(got_dx, grads[0]))) {
        string_t *got_text = format_number_for_test_output(got_dx);
        string_t *expected_text = format_number_for_test_output(grads[0]);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", formatted_number_cstr(expected_text));
        printf("    got      = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }
    snprintf(label, sizeof(label), "numeric derivative sweep d/dy: %s", tc->name);
    if (!(num_eq(got_dy, grads[1]) || number_close_for_qfloat_precision(got_dy, grads[1]))) {
        string_t *got_text = format_number_for_test_output(got_dy);
        string_t *expected_text = format_number_for_test_output(grads[1]);

        printf(C_BOLD C_RED "FAIL" C_RESET " %s\n", label);
        printf("    expected = %s\n", formatted_number_cstr(expected_text));
        printf("    got      = %s\n", formatted_number_cstr(got_text));
        string_free(expected_text);
        string_free(got_text);
        TEST_FAIL();
    }

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    num_destroy(&got_dy);
    num_destroy(&got_dx);
    expr_free(deriv_y);
    expr_free(deriv_x);
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_unary_value_case(const unary_eval_case_t *tc, size_t value_bits, size_t oracle_bits)
{
    number_t input = num_from_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_text_bits(tc->input, oracle_bits);
    expr_t *x = expr_new_var(input);
    expr_t *oracle_x = expr_new_var(oracle_input);
    expr_t *expr = tc->expr_fn(x);
    expr_t *oracle_expr = tc->expr_fn(oracle_x);
    number_t got = expr_eval(expr);
    number_t expected = expr_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_x);
    expr_free(x);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_binary_value_case(const binary_eval_case_t *tc, size_t value_bits, size_t oracle_bits)
{
    number_t lhs = num_from_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_text_bits(tc->rhs, oracle_bits);
    expr_t *a = expr_new_var(lhs);
    expr_t *b = expr_new_var(rhs);
    expr_t *oracle_a = expr_new_var(oracle_lhs);
    expr_t *oracle_b = expr_new_var(oracle_rhs);
    expr_t *expr = tc->expr_fn(a, b);
    expr_t *oracle_expr = tc->expr_fn(oracle_a, oracle_b);
    number_t got = expr_eval(expr);
    number_t expected = expr_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_b);
    expr_free(oracle_a);
    expr_free(b);
    expr_free(a);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_unary_derivative_case(const unary_eval_case_t *tc, size_t value_bits,
                                                       size_t oracle_bits)
{
    number_t input = num_from_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_text_bits(tc->input, oracle_bits);
    expr_t *x = expr_new_var(input);
    expr_t *oracle_x = expr_new_var(oracle_input);
    expr_t *expr = tc->expr_fn(x);
    expr_t *oracle_expr = tc->expr_fn(oracle_x);
    expr_t *deriv = expr_create_deriv(expr, x);
    expr_t *oracle_deriv = expr_create_deriv(oracle_expr, oracle_x);
    number_t got = expr_eval(deriv);
    number_t expected = expr_eval(oracle_deriv);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(oracle_deriv);
    expr_free(deriv);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_x);
    expr_free(x);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_binary_derivative_case(const binary_eval_case_t *tc, size_t value_bits,
                                                        size_t oracle_bits)
{
    number_t lhs = num_from_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_text_bits(tc->rhs, oracle_bits);
    expr_t *x = expr_new_var(lhs);
    expr_t *y = expr_new_var(rhs);
    expr_t *oracle_x = expr_new_var(oracle_lhs);
    expr_t *oracle_y = expr_new_var(oracle_rhs);
    expr_t *expr = tc->expr_fn(x, y);
    expr_t *oracle_expr = tc->expr_fn(oracle_x, oracle_y);
    expr_t *deriv_x = expr_create_deriv(expr, x);
    expr_t *deriv_y = expr_create_deriv(expr, y);
    expr_t *oracle_deriv_x = expr_create_deriv(oracle_expr, oracle_x);
    expr_t *oracle_deriv_y = expr_create_deriv(oracle_expr, oracle_y);
    number_t got_dx = expr_eval(deriv_x);
    number_t got_dy = expr_eval(deriv_y);
    number_t expected_dx = expr_eval(oracle_deriv_x);
    number_t expected_dy = expr_eval(oracle_deriv_y);

    assert_same_to_bits(got_dx, expected_dx, high_precision_compare_bits(value_bits), tc->name);
    assert_same_to_bits(got_dy, expected_dy, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected_dy);
    num_destroy(&expected_dx);
    num_destroy(&got_dy);
    num_destroy(&got_dx);
    expr_free(oracle_deriv_y);
    expr_free(oracle_deriv_x);
    expr_free(deriv_y);
    expr_free(deriv_x);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_y);
    expr_free(oracle_x);
    expr_free(y);
    expr_free(x);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void check_high_precision_complex_unary_value_case(const unary_eval_case_t *tc, size_t value_bits,
                                                          size_t oracle_bits)
{
    number_t input = num_from_text_bits(tc->input, value_bits);
    number_t oracle_input = num_from_text_bits(tc->input, oracle_bits);
    expr_t *z = expr_new_var(input);
    expr_t *oracle_z = expr_new_var(oracle_input);
    expr_t *expr = tc->expr_fn(z);
    expr_t *oracle_expr = tc->expr_fn(oracle_z);
    number_t got = expr_eval(expr);
    number_t expected = expr_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_z);
    expr_free(z);
    num_destroy(&oracle_input);
    num_destroy(&input);
}

static void check_high_precision_complex_binary_value_case(const binary_eval_case_t *tc, size_t value_bits,
                                                           size_t oracle_bits)
{
    number_t lhs = num_from_text_bits(tc->lhs, value_bits);
    number_t rhs = num_from_text_bits(tc->rhs, value_bits);
    number_t oracle_lhs = num_from_text_bits(tc->lhs, oracle_bits);
    number_t oracle_rhs = num_from_text_bits(tc->rhs, oracle_bits);
    expr_t *a = expr_new_var(lhs);
    expr_t *b = expr_new_var(rhs);
    expr_t *oracle_a = expr_new_var(oracle_lhs);
    expr_t *oracle_b = expr_new_var(oracle_rhs);
    expr_t *expr = tc->expr_fn(a, b);
    expr_t *oracle_expr = tc->expr_fn(oracle_a, oracle_b);
    number_t got = expr_eval(expr);
    number_t expected = expr_eval(oracle_expr);

    assert_same_to_bits(got, expected, high_precision_compare_bits(value_bits), tc->name);

    num_destroy(&expected);
    num_destroy(&got);
    expr_free(oracle_expr);
    expr_free(expr);
    expr_free(oracle_b);
    expr_free(oracle_a);
    expr_free(b);
    expr_free(a);
    num_destroy(&oracle_rhs);
    num_destroy(&oracle_lhs);
    num_destroy(&rhs);
    num_destroy(&lhs);
}

static void test_cmp_qfloat_precision(void)
{
    expr_t *a = test_expr_new_const_qf(qf_from_string("1.00000000000000000001"));
    expr_t *b = test_expr_new_const_qf(qf_from_string("1.00000000000000000002"));
    int cmp = expr_cmp(a, b);

    if (cmp < 0) {
        printf(C_BOLD C_GREEN "PASS" C_RESET " expr_cmp respects qfloat precision\n");
    } else {
        printf(C_BOLD C_RED "FAIL" C_RESET " expr_cmp lost qfloat precision %s:%d:1 (got %d, expected < 0)\n", __FILE__,
               __LINE__, cmp);
        TEST_FAIL();
    }

    expr_free(b);
    expr_free(a);
}

static void test_new_const_num_preserves_mpfr_precision(void)
{
    number_t n = num_from_text_bits("1.25", 512u);
    expr_t *dv;
    number_t got;

    dv = expr_new_const(n);
    got = expr_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 512);

    num_destroy(&got);
    expr_free(dv);
    num_destroy(&n);
}

static void test_inexact_known_constant_uses_short_text(void)
{
    number_t n = num_const_prec_digits(NUM_SQRT1ONPI, 80u);
    expr_t *expr = expr_new_const(n);
    char *expr_text = expr ? expr_to_string(expr, style_UNBOUND) : NULL;

    ASSERT_TRUE(expr_text != NULL);
    ASSERT_TRUE(strcmp(expr_text, "1/√π") == 0);

    free(expr_text);
    expr_free(expr);
    num_destroy(&n);
}

static void test_set_val_num_preserves_mpfr_precision(void)
{
    number_t n = num_from_text_bits("1.25", 640u);
    expr_t *dv = test_expr_new_var_d(0.0);
    number_t got;

    expr_set_val(dv, n);
    got = expr_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 640);

    num_destroy(&got);
    expr_free(dv);
    num_destroy(&n);
}

static void test_new_const_num_preserves_complex_precision(void)
{
    number_t n = num_from_text_bits("1 + 2i", 384u);
    expr_t *dv;
    number_t got;

    dv = expr_new_const(n);
    got = expr_eval(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_TRUE(!num_is_real(got));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 384);

    num_destroy(&got);
    expr_free(dv);
    num_destroy(&n);
}

static void test_set_val_num_preserves_complex_precision(void)
{
    number_t n = num_from_text_bits("1 + 2i", 448u);
    expr_t *dv = test_expr_new_var_d(0.0);
    number_t got;

    expr_set_val(dv, n);
    got = expr_get_val(dv);

    ASSERT_TRUE(num_eq(got, n));
    ASSERT_TRUE(!num_is_real(got));
    ASSERT_EQ_INT((int)num_get_prec_bits(got), 448);

    num_destroy(&got);
    expr_free(dv);
    num_destroy(&n);
}

static void test_eval_expression_preserves_mpfr_precision(void)
{
    number_t n = num_from_text_bits("1.25", 512u);
    expr_t *x;
    expr_t *sum;
    expr_t *root;
    number_t got_sum;
    number_t got_root;
    number_t check_sum;
    number_t check_root;
    number_t oracle_n;
    number_t expect_sum;
    number_t expect_root;
    size_t oracle_bits;

    x = expr_new_var(n);
    sum = expr_add(x, EXPR_ONE);
    root = expr_sqrt(x);
    got_sum = expr_eval(sum);
    got_root = expr_eval(root);
    check_sum = num_add(n, NUM_ONE);
    check_root = num_sqrt(n);
    oracle_bits = num_get_prec_bits(n) + 384u;
    oracle_n = num_clone(n);
    ASSERT_EQ_INT(num_set_prec_bits(&oracle_n, oracle_bits), 0);
    expect_sum = num_add(oracle_n, NUM_ONE);
    expect_root = num_sqrt(oracle_n);

    {
        string_t *input_text = format_number_for_test_output(n);

        ASSERT_NOT_NULL(input_text);
        printf(C_BOLD C_GREEN "PASS" C_RESET " high-precision mpfr expr evaluation\n");
        printf("    input    = %s\n", formatted_number_cstr(input_text));
        printf("    input precision: %zu bits, %zu significant digits\n", num_get_prec_bits(n), num_get_prec_digits(n));
        print_precision_comparison("x + 1", got_sum, expect_sum);
        print_precision_comparison("sqrt(x)", got_root, expect_root);
        printf("\n");
        string_free(input_text);
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
    expr_free(root);
    expr_free(sum);
    expr_free(x);
    num_destroy(&n);
}

static void test_eval_expression_preserves_complex_precision(void)
{
    number_t n = num_from_text_bits("1 + 2i", 384u);
    expr_t *z;
    expr_t *sum;
    expr_t *exp_z;
    number_t got_sum;
    number_t got_exp;
    number_t check_sum;
    number_t check_exp;
    number_t oracle_n;
    number_t expect_sum;
    number_t expect_exp;
    size_t oracle_bits;

    z = expr_new_var(n);
    sum = expr_add(z, EXPR_ONE);
    exp_z = expr_exp(z);
    got_sum = expr_eval(sum);
    got_exp = expr_eval(exp_z);
    check_sum = num_add(n, NUM_ONE);
    check_exp = num_exp(n);
    oracle_bits = num_get_prec_bits(n) + 384u;
    oracle_n = num_clone(n);
    ASSERT_EQ_INT(num_set_prec_bits(&oracle_n, oracle_bits), 0);
    expect_sum = num_add(oracle_n, NUM_ONE);
    expect_exp = num_exp(oracle_n);

    {
        string_t *input_text = format_number_for_test_output(n);

        ASSERT_NOT_NULL(input_text);
        printf(C_BOLD C_GREEN "PASS" C_RESET " high-precision complex expr evaluation\n");
        printf("    input    = %s\n", formatted_number_cstr(input_text));
        printf("    input precision: %zu bits, %zu significant digits\n", num_get_prec_bits(n), num_get_prec_digits(n));
        print_precision_comparison("z + 1", got_sum, expect_sum);
        print_precision_comparison("exp(z)", got_exp, expect_exp);
        printf("\n");
        string_free(input_text);
    }

    ASSERT_TRUE(num_eq(got_sum, check_sum));
    ASSERT_TRUE(num_eq(got_exp, check_exp));
    ASSERT_TRUE(!num_is_real(got_sum));
    ASSERT_TRUE(!num_is_real(got_exp));
    ASSERT_EQ_INT((int)num_get_prec_bits(got_sum), 0);
    ASSERT_EQ_INT((int)num_get_prec_bits(got_exp), 384);

    num_destroy(&check_exp);
    num_destroy(&check_sum);
    num_destroy(&expect_exp);
    num_destroy(&expect_sum);
    num_destroy(&oracle_n);
    num_destroy(&got_exp);
    num_destroy(&got_sum);
    expr_free(exp_z);
    expr_free(sum);
    expr_free(z);
    num_destroy(&n);
}

static void test_new_const_num_preserves_qfloat_precision(void)
{
    qfloat_t q = qf_from_string("1.00000000000000000001");
    number_t n = num_create_from_qfloat(q);
    expr_t *dv = expr_new_const(n);
    qfloat_t got = expr_eval_qf(dv);

    check_q_at(__FILE__, __LINE__, 1, "expr_new_const preserves qfloat precision", got, q);

    expr_free(dv);
    num_destroy(&n);
}

static void test_set_val_num_preserves_qfloat_precision(void)
{
    qfloat_t q = qf_from_string("1.00000000000000000001");
    number_t n = num_create_from_qfloat(q);
    expr_t *dv = test_expr_new_var_d(0.0);

    expr_set_val(dv, n);
    check_q_at(__FILE__, __LINE__, 1, "expr_set_val preserves qfloat precision", expr_eval_qf(dv), q);

    expr_free(dv);
    num_destroy(&n);
}

static void test_default_constants_preserve_builtin_precision(void)
{
    expr_t *pi = expr_from_string("{ pi }", NULL);
    expr_t *e = expr_from_string("{ e }", NULL);
    expr_t *phi = expr_from_string("{ @phi }", NULL);
    number_t phi_value = NUM_ZERO;

    ASSERT_NOT_NULL(pi);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(phi);

    check_q_at(__FILE__, __LINE__, 1, "expr pi uses qfloat precision", expr_eval_qf(pi), QF_PI);
    check_q_at(__FILE__, __LINE__, 1, "expr e uses qfloat precision", expr_eval_qf(e), QF_E);
    phi_value = expr_eval(phi);
    ASSERT_EQ_INT((int)num_get_prec_bits(phi_value), (int)num_get_default_prec_bits());
    check_q_at(__FILE__, __LINE__, 1, "expr phi preserves builtin value", num_to_qfloat(phi_value), QF_PHI);

    num_destroy(&phi_value);
    expr_free(phi);
    expr_free(e);
    expr_free(pi);
}

static void test_expr_ln10_singleton(void)
{
    number_t got = expr_eval(EXPR_LN10);
    number_t expected = num_const(NUM_LN10);
    char *text = expr_to_string(EXPR_LN10, style_EXPRESSION);

    ASSERT_TRUE(num_eq(got, expected));
    ASSERT_TRUE(text && strstr(text, "ln10") != NULL);

    free(text);
    num_destroy(&expected);
    num_destroy(&got);
}

static void test_get_val_updates_after_set(void)
{
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *f = expr_add_d(x, 2.0);

    check_q_at(__FILE__, __LINE__, 1, "expr_get_val initial", expr_get_val_qf(f), qf_from_double(3.0));
    test_expr_set_val_d(x, 5.0);
    check_q_at(__FILE__, __LINE__, 1, "expr_get_val after set", expr_get_val_qf(f), qf_from_double(7.0));

    expr_free(f);
    expr_free(x);
}

static void test_new_const_num(void)
{
    number_t half = num_create_from_string("1/2");
    expr_t *c_half = expr_new_const(half);
    number_t got_half = expr_get_val(c_half);
    number_t eval_half = expr_eval(c_half);

    ASSERT_TRUE(num_eq(got_half, half));
    ASSERT_TRUE(num_eq(eval_half, half));

    num_destroy(&got_half);
    num_destroy(&eval_half);
    expr_free(c_half);
    num_destroy(&half);
}

static void test_new_const_num_rational_complex(void)
{
    number_t z = num_create_from_string("1/2 - 3/2i");
    expr_t *c_z = expr_new_const(z);
    number_t got_z = expr_get_val(c_z);
    number_t eval_z = expr_eval(c_z);

    ASSERT_TRUE(num_eq(got_z, z));
    ASSERT_TRUE(num_eq(eval_z, z));

    num_destroy(&eval_z);
    num_destroy(&got_z);
    expr_free(c_z);
    num_destroy(&z);
}

static void test_new_var_num_and_set_val_num(void)
{
    number_t one_plus_i = num_create_from_string("1 + i");
    number_t minus_i = num_create_from_string("-i");
    number_t quarter = num_create_from_string("1/4");
    number_t rational_complex = num_create_from_string("1/2 - 3/2i");
    expr_t *v = expr_new_var(one_plus_i);
    number_t got_var = expr_eval(v);

    ASSERT_TRUE(num_eq(got_var, one_plus_i));

    expr_set_val(v, minus_i);
    num_destroy(&got_var);
    got_var = expr_get_val(v);
    ASSERT_TRUE(num_eq(got_var, minus_i));

    expr_set_val(v, quarter);
    num_destroy(&got_var);
    got_var = expr_eval(v);
    ASSERT_TRUE(num_eq(got_var, quarter));

    expr_set_val(v, rational_complex);
    num_destroy(&got_var);
    got_var = expr_get_val(v);
    ASSERT_TRUE(num_eq(got_var, rational_complex));

    num_destroy(&got_var);
    expr_free(v);
    num_destroy(&rational_complex);
    num_destroy(&quarter);
    num_destroy(&minus_i);
    num_destroy(&one_plus_i);
}

static void test_named_number_constructors(void)
{
    number_t two = num_create_from_string("2");
    number_t z_value = num_create_from_string("2 + 3i");
    expr_t *named_const = expr_new_named_const(two, "two");
    expr_t *named_var = expr_new_named_var(z_value, "z");
    number_t got_const = expr_get_val(named_const);
    number_t got_var = expr_get_val(named_var);
    char *const_text = expr_to_string(named_const, style_EXPRESSION);
    char *var_text = expr_to_string(named_var, style_EXPRESSION);

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
    expr_free(named_var);
    expr_free(named_const);
    num_destroy(&z_value);
    num_destroy(&two);
}

static void test_eval_num_on_expression(void)
{
    number_t half = num_create_from_string("1/2");
    number_t two = num_create_from_string("2");
    number_t expected = num_create_from_string("5/2");
    expr_t *x = expr_new_var(half);
    expr_t *c = expr_new_const(two);
    expr_t *sum = expr_add(x, c);
    number_t got = expr_eval(sum);

    ASSERT_TRUE(num_eq(got, expected));

    num_destroy(&got);
    expr_free(sum);
    expr_free(c);
    expr_free(x);
    num_destroy(&expected);
    num_destroy(&two);
    num_destroy(&half);
}

static void test_eval_num_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        {"sin", "0.5", expr_sin, num_sin, NULL},
        {"cos", "0.5", expr_cos, num_cos, NULL},
        {"tan", "0.5", expr_tan, num_tan, NULL},
        {"sinh", "0.5", expr_sinh, num_sinh, NULL},
        {"cosh", "0.5", expr_cosh, num_cosh, NULL},
        {"tanh", "0.5", expr_tanh, num_tanh, NULL},
        {"asin", "0.25", expr_asin, num_asin, NULL},
        {"acos", "0.25", expr_acos, num_acos, NULL},
        {"atan", "0.25", expr_atan, num_atan, NULL},
        {"asinh", "0.25", expr_asinh, num_asinh, NULL},
        {"acosh", "1.25", expr_acosh, num_acosh, NULL},
        {"atanh", "0.25", expr_atanh, num_atanh, NULL},
        {"exp", "1.5", expr_exp, num_exp, NULL},
        {"log", "1.5", expr_log, num_log, NULL},
        {"log10", "1.5", expr_log10, num_log10, NULL},
        {"sqrt", "2.0", expr_sqrt, num_sqrt, NULL},
        {"pow_d", "2.0", expr_pow3_builder, num_pow3_builder, NULL},
        {"abs", "-3.0", expr_abs, num_abs, NULL},
        {"erf", "0.8", expr_erf, num_erf, NULL},
        {"erfc", "1.2", expr_erfc, num_erfc, NULL},
        {"erfinv", "0.5", expr_erfinv, num_erfinv, NULL},
        {"erfcinv", "0.4", expr_erfcinv, num_erfcinv, NULL},
        {"gamma", "2.5", expr_gamma, num_gamma, NULL},
        {"gammainv", GAMMAINV_INPUT_TEXT, expr_gammainv, num_gammainv, NULL},
        {"lgamma", "2.5", expr_lgamma, num_lgamma, NULL},
        {"digamma", "2.5", expr_digamma, num_digamma, NULL},
        {"trigamma", "2.5", expr_trigamma, num_trigamma, NULL},
        {"W₀", "0.2", expr_lambert_w0, num_lambert_w0, NULL},
        UCASE_TOL("W₋₁", "-0.1", expr_lambert_wm1, num_lambert_wm1, "1e-30"),
        {"normal_pdf", "1.0", expr_normal_pdf, num_normal_pdf, NULL},
        {"normal_cdf", "1.0", expr_normal_cdf, num_normal_cdf, NULL},
        {"normal_logpdf", "1.0", expr_normal_logpdf, num_normal_logpdf, NULL},
        {"ei", "1.0", expr_Ei, num_Ei, NULL},
        {"e1", "1.0", expr_E1, num_E1, NULL}};
    static const binary_eval_case_t binary_cases[] = {{"atan2", "2.0", "3.0", expr_atan2, num_atan2},
                                                      {"pow", "2.0", "3.0", expr_pow_xp, num_pow},
                                                      {"hypot", "3.0", "4.0", expr_hypot, num_hypot},
                                                      {"beta", "2.5", "1.5", expr_beta, num_beta},
                                                      {"logbeta", "2.5", "1.5", expr_logbeta, num_logbeta}};
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_unary_eval_case(&unary_cases[i]);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_binary_eval_case(&binary_cases[i]);
}

static void test_eval_num_function_derivatives(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", expr_sin, num_sin),
        UCASE("cos", "0.5", expr_cos, num_cos),
        UCASE("tan", "0.5", expr_tan, num_tan),
        UCASE("sinh", "0.5", expr_sinh, num_sinh),
        UCASE("cosh", "0.5", expr_cosh, num_cosh),
        UCASE("tanh", "0.5", expr_tanh, num_tanh),
        UCASE("asin", "0.25", expr_asin, num_asin),
        UCASE("acos", "0.25", expr_acos, num_acos),
        UCASE("atan", "0.25", expr_atan, num_atan),
        UCASE("asinh", "0.25", expr_asinh, num_asinh),
        UCASE("acosh", "1.25", expr_acosh, num_acosh),
        UCASE("atanh", "0.25", expr_atanh, num_atanh),
        UCASE("exp", "1.5", expr_exp, num_exp),
        UCASE("log", "1.5", expr_log, num_log),
        UCASE("log10", "1.5", expr_log10, num_log10),
        UCASE("sqrt", "2.0", expr_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", expr_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", expr_abs, num_abs),
        UCASE("erf", "0.8", expr_erf, num_erf),
        UCASE("erfc", "1.2", expr_erfc, num_erfc),
        UCASE("erfinv", "0.5", expr_erfinv, num_erfinv),
        UCASE("erfcinv", "0.4", expr_erfcinv, num_erfcinv),
        UCASE("gamma", "2.5", expr_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, expr_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", expr_lgamma, num_lgamma),
        UCASE("digamma", "2.5", expr_digamma, num_digamma),
        UCASE("trigamma", "2.5", expr_trigamma, num_trigamma),
        UCASE("W₀", "0.2", expr_lambert_w0, num_lambert_w0),
        UCASE_TOL("W₋₁", "-0.1", expr_lambert_wm1, num_lambert_wm1, "1e-30"),
        UCASE("normal_pdf", "1.0", expr_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", expr_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", expr_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", expr_Ei, num_Ei),
        UCASE("e1", "1.0", expr_E1, num_E1)};
    static const binary_eval_case_t binary_cases[] = {{"atan2", "2.0", "3.0", expr_atan2, num_atan2},
                                                      {"pow", "2.0", "3.0", expr_pow_xp, num_pow},
                                                      {"hypot", "3.0", "4.0", expr_hypot, num_hypot},
                                                      {"beta", "2.5", "1.5", expr_beta, num_beta},
                                                      {"logbeta", "2.5", "1.5", expr_logbeta, num_logbeta}};
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_unary_derivative_case(&unary_cases[i]);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_binary_derivative_case(&binary_cases[i]);
}

static void test_high_precision_mpfr_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", expr_sin, num_sin),
        UCASE("cos", "0.5", expr_cos, num_cos),
        UCASE("sinh", "0.5", expr_sinh, num_sinh),
        UCASE("cosh", "0.5", expr_cosh, num_cosh),
        UCASE("tanh", "0.5", expr_tanh, num_tanh),
        UCASE("asin", "0.25", expr_asin, num_asin),
        UCASE("acos", "0.25", expr_acos, num_acos),
        UCASE("atan", "0.25", expr_atan, num_atan),
        UCASE("asinh", "0.25", expr_asinh, num_asinh),
        UCASE("acosh", "1.25", expr_acosh, num_acosh),
        UCASE("atanh", "0.25", expr_atanh, num_atanh),
        UCASE("log", "1.5", expr_log, num_log),
        UCASE("log10", "1.5", expr_log10, num_log10),
        UCASE("sqrt", "2.0", expr_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", expr_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", expr_abs, num_abs),
        UCASE("erf", "0.8", expr_erf, num_erf),
        UCASE("erfc", "1.2", expr_erfc, num_erfc),
        UCASE("erfinv", "0.5", expr_erfinv, num_erfinv),
        UCASE("erfcinv", "0.4", expr_erfcinv, num_erfcinv),
        UCASE("gamma", "2.5", expr_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, expr_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", expr_lgamma, num_lgamma),
        UCASE("digamma", "2.5", expr_digamma, num_digamma),
        UCASE("trigamma", "2.5", expr_trigamma, num_trigamma),
        UCASE("W₀", "0.2", expr_lambert_w0, num_lambert_w0),
        UCASE("W₋₁", "-0.1", expr_lambert_wm1, num_lambert_wm1),
        UCASE("normal_pdf", "1.0", expr_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", expr_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", expr_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", expr_Ei, num_Ei),
        UCASE("e1", "1.0", expr_E1, num_E1)};
    static const binary_eval_case_t binary_cases[] = {{"atan2", "2.0", "3.0", expr_atan2, num_atan2},
                                                      {"pow", "2.0", "3.0", expr_pow_xp, num_pow},
                                                      {"hypot", "3.0", "4.0", expr_hypot, num_hypot},
                                                      {"beta", "2.5", "1.5", expr_beta, num_beta},
                                                      {"logbeta", "2.5", "1.5", expr_logbeta, num_logbeta}};
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_high_precision_unary_value_case(&unary_cases[i], 512u, 896u);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_high_precision_binary_value_case(&binary_cases[i], 512u, 896u);
}

static void test_high_precision_mpfr_function_derivatives(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "0.5", expr_sin, num_sin),
        UCASE("cos", "0.5", expr_cos, num_cos),
        UCASE("sinh", "0.5", expr_sinh, num_sinh),
        UCASE("cosh", "0.5", expr_cosh, num_cosh),
        UCASE("tanh", "0.5", expr_tanh, num_tanh),
        UCASE("asin", "0.25", expr_asin, num_asin),
        UCASE("acos", "0.25", expr_acos, num_acos),
        UCASE("atan", "0.25", expr_atan, num_atan),
        UCASE("asinh", "0.25", expr_asinh, num_asinh),
        UCASE("acosh", "1.25", expr_acosh, num_acosh),
        UCASE("atanh", "0.25", expr_atanh, num_atanh),
        UCASE("log", "1.5", expr_log, num_log),
        UCASE("log10", "1.5", expr_log10, num_log10),
        UCASE("sqrt", "2.0", expr_sqrt, num_sqrt),
        UCASE("pow_d", "2.0", expr_pow3_builder, num_pow3_builder),
        UCASE("abs", "-3.0", expr_abs, num_abs),
        UCASE("gamma", "2.5", expr_gamma, num_gamma),
        UCASE("gammainv", GAMMAINV_INPUT_TEXT, expr_gammainv, num_gammainv),
        UCASE("lgamma", "2.5", expr_lgamma, num_lgamma),
        UCASE("digamma", "2.5", expr_digamma, num_digamma),
        UCASE("trigamma", "2.5", expr_trigamma, num_trigamma),
        UCASE("W₀", "0.2", expr_lambert_w0, num_lambert_w0),
        UCASE("W₋₁", "-0.1", expr_lambert_wm1, num_lambert_wm1),
        UCASE("normal_pdf", "1.0", expr_normal_pdf, num_normal_pdf),
        UCASE("normal_cdf", "1.0", expr_normal_cdf, num_normal_cdf),
        UCASE("normal_logpdf", "1.0", expr_normal_logpdf, num_normal_logpdf),
        UCASE("ei", "1.0", expr_Ei, num_Ei),
        UCASE("e1", "1.0", expr_E1, num_E1)};
    static const binary_eval_case_t binary_cases[] = {{"atan2", "2.0", "3.0", expr_atan2, num_atan2},
                                                      {"pow", "2.0", "3.0", expr_pow_xp, num_pow},
                                                      {"hypot", "3.0", "4.0", expr_hypot, num_hypot},
                                                      {"beta", "2.5", "1.5", expr_beta, num_beta},
                                                      {"logbeta", "2.5", "1.5", expr_logbeta, num_logbeta}};
    size_t i;

    for (i = 0; i < sizeof(unary_cases) / sizeof(unary_cases[0]); ++i)
        check_high_precision_unary_derivative_case(&unary_cases[i], 512u, 896u);
    for (i = 0; i < sizeof(binary_cases) / sizeof(binary_cases[0]); ++i)
        check_high_precision_binary_derivative_case(&binary_cases[i], 512u, 896u);
}

static void test_high_precision_complex_function_values(void)
{
    static const unary_eval_case_t unary_cases[] = {
        UCASE("sin", "1 + 2i", expr_sin, num_sin),       UCASE("cos", "1 + 2i", expr_cos, num_cos),
        UCASE("tan", "1 + 2i", expr_tan, num_tan),       UCASE("sinh", "1 + 2i", expr_sinh, num_sinh),
        UCASE("cosh", "1 + 2i", expr_cosh, num_cosh),    UCASE("tanh", "1 + 2i", expr_tanh, num_tanh),
        UCASE("exp", "1 + 2i", expr_exp, num_exp),       UCASE("log", "1 + 2i", expr_log, num_log),
        UCASE("log10", "1 + 2i", expr_log10, num_log10), UCASE("sqrt", "1 + 2i", expr_sqrt, num_sqrt)};
    static const binary_eval_case_t binary_cases[] = {{"pow", "1 + 2i", "2 - i", expr_pow_xp, num_pow}};
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
    expr_t *named_const = expr_new_named_const(old_value, "c");
    number_t got_value;

    expr_set_val(named_const, new_value);
    got_value = expr_eval(named_const);

    ASSERT_TRUE(num_eq(got_value, new_value));

    num_destroy(&got_value);
    expr_free(named_const);
    num_destroy(&new_value);
    num_destroy(&old_value);
}

static void test_to_string_unbound_omits_binding_wrapper(void)
{
    expr_t *expr = expr_from_string("{ sin(x) + c | x = 2; c = 3 }", NULL);
    char *full_text;
    char *unbound_text;

    ASSERT_NOT_NULL(expr);

    full_text = expr_to_string(expr, style_EXPRESSION);
    unbound_text = expr_to_string(expr, style_UNBOUND);

    TEST_ASSERT_STR_EQ(full_text, "{ sin(x) + c | x = 2; c = 3 }");
    TEST_ASSERT_STR_EQ(unbound_text, "sin(x) + c");

    free(unbound_text);
    free(full_text);
    expr_free(expr);
}

typedef expr_t *(*test_unary_expr_fn)(const expr_t *dv);

static void check_direct_inverse_simplifies(const char *label, test_unary_expr_fn outer, test_unary_expr_fn inner)
{
    expr_t *x = test_expr_new_named_var_d(0.25, "x");
    expr_t *inner_x = inner(x);
    expr_t *outer_inner_x = outer(inner_x);
    expr_t *simp = expr_simplify(outer_inner_x);
    char *text = expr_to_string(simp, style_EXPRESSION);
    const char *expect = "{ x | x = 0.25 }";

    if (str_eq(text, expect))
        to_string_pass(label, text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, label, text ? text : "(null)", expect);

    free(text);
    expr_free(simp);
    expr_free(outer_inner_x);
    expr_free(inner_x);
    expr_free(x);
}

static void check_simplified_expression_string(const char *label, const char *input, const char *expect)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *simp = expr ? expr_simplify(expr) : NULL;
    char *text = simp ? expr_to_string(simp, style_EXPRESSION) : NULL;

    if (str_eq(text, expect))
        to_string_pass(label, text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, label, text ? text : "(null)", expect);

    free(text);
    expr_free(simp);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_simplify_exact_rational_square_roots(void)
{
    number_t four = num_create_from_long(4L);
    number_t nine = num_create_from_long(9L);
    number_t denominator = num_create_from_long(4L);
    number_t nine_quarters = num_div(nine, denominator);
    expr_t *four_expr = expr_new_const(four);
    expr_t *nine_quarters_expr = expr_new_const(nine_quarters);
    expr_t *sqrt_four = four_expr ? expr_sqrt(four_expr) : NULL;
    expr_t *sqrt_nine_quarters = nine_quarters_expr ? expr_sqrt(nine_quarters_expr) : NULL;
    expr_t *simplified_four = sqrt_four ? expr_simplify(sqrt_four) : NULL;
    expr_t *simplified_nine_quarters = sqrt_nine_quarters ? expr_simplify(sqrt_nine_quarters) : NULL;
    char *four_text = simplified_four ? expr_to_string(simplified_four, style_UNBOUND) : NULL;
    char *nine_quarters_text =
        simplified_nine_quarters ? expr_to_string(simplified_nine_quarters, style_UNBOUND) : NULL;

    if (str_eq(four_text, "2"))
        to_string_pass("generated sqrt(4) simplifies exactly", four_text, "2");
    else
        to_string_fail(__FILE__, __LINE__, 1, "generated sqrt(4) simplifies exactly", four_text ? four_text : "(null)",
                       "2");

    if (str_eq(nine_quarters_text, "³⁄₂"))
        to_string_pass("generated sqrt(9/4) simplifies exactly", nine_quarters_text, "³⁄₂");
    else
        to_string_fail(__FILE__, __LINE__, 1, "generated sqrt(9/4) simplifies exactly",
                       nine_quarters_text ? nine_quarters_text : "(null)", "³⁄₂");

    free(nine_quarters_text);
    free(four_text);
    expr_free(simplified_nine_quarters);
    expr_free(simplified_four);
    expr_free(sqrt_nine_quarters);
    expr_free(sqrt_four);
    expr_free(nine_quarters_expr);
    expr_free(four_expr);
    num_destroy(&nine_quarters);
    num_destroy(&denominator);
    num_destroy(&nine);
    num_destroy(&four);
}

static void test_simplify_inverse_unary_pairs(void)
{
    expr_t *x = test_expr_new_named_var_d(3.0, "x");
    expr_t *log_x = expr_log(x);
    expr_t *exp_log_x = expr_exp(log_x);
    expr_t *exp_x = expr_exp(x);
    expr_t *log_exp_x = expr_log(exp_x);
    expr_t *ten = expr_new_const(NUM_TEN);
    expr_t *log10_x = expr_log10(x);
    expr_t *ten_pow_log10_x = expr_pow_xp(ten, log10_x);
    expr_t *ten_pow_x = expr_pow_xp(ten, x);
    expr_t *log10_ten_pow_x = expr_log10(ten_pow_x);
    expr_t *exp_log_simp = expr_simplify(exp_log_x);
    expr_t *log_exp_simp = expr_simplify(log_exp_x);
    expr_t *ten_pow_log10_simp = expr_simplify(ten_pow_log10_x);
    expr_t *log10_ten_pow_simp = expr_simplify(log10_ten_pow_x);
    char *exp_log_s = expr_to_string(exp_log_simp, style_EXPRESSION);
    char *log_exp_s = expr_to_string(log_exp_simp, style_EXPRESSION);
    char *ten_pow_log10_s = expr_to_string(ten_pow_log10_simp, style_EXPRESSION);
    char *log10_ten_pow_s = expr_to_string(log10_ten_pow_simp, style_EXPRESSION);
    const char *expect = "{ x | x = 3 }";

    check_q_at(__FILE__, __LINE__, 1, "exp(log(x)) eval", expr_eval_qf(exp_log_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log(exp(x)) eval", expr_eval_qf(log_exp_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "10^log10(x) eval", expr_eval_qf(ten_pow_log10_x), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "log10(10^x) eval", expr_eval_qf(log10_ten_pow_x), qf_from_double(3.0));

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

    check_direct_inverse_simplifies("sin(asin(x)) simplification (EXPR)", expr_sin, expr_asin);
    check_direct_inverse_simplifies("cos(acos(x)) simplification (EXPR)", expr_cos, expr_acos);
    check_direct_inverse_simplifies("tan(atan(x)) simplification (EXPR)", expr_tan, expr_atan);
    check_direct_inverse_simplifies("sinh(asinh(x)) simplification (EXPR)", expr_sinh, expr_asinh);
    check_direct_inverse_simplifies("cosh(acosh(x)) simplification (EXPR)", expr_cosh, expr_acosh);
    check_direct_inverse_simplifies("tanh(atanh(x)) simplification (EXPR)", expr_tanh, expr_atanh);
    check_direct_inverse_simplifies("erf(erfinv(x)) simplification (EXPR)", expr_erf, expr_erfinv);
    check_direct_inverse_simplifies("erfc(erfcinv(x)) simplification (EXPR)", expr_erfc, expr_erfcinv);
    check_direct_inverse_simplifies("gamma(gammainv(x)) simplification (EXPR)", expr_gamma, expr_gammainv);

    free(log10_ten_pow_s);
    free(ten_pow_log10_s);
    free(log_exp_s);
    free(exp_log_s);
    expr_free(log10_ten_pow_simp);
    expr_free(ten_pow_log10_simp);
    expr_free(log_exp_simp);
    expr_free(exp_log_simp);
    expr_free(log10_ten_pow_x);
    expr_free(ten_pow_x);
    expr_free(ten_pow_log10_x);
    expr_free(log10_x);
    expr_free(ten);
    expr_free(log_exp_x);
    expr_free(exp_x);
    expr_free(exp_log_x);
    expr_free(log_x);
    expr_free(x);
}

static void test_simplify_lambert_exp_to_quotient(void)
{
    expr_t *zero;
    expr_t *w_zero;
    expr_t *exp_w_zero;
    expr_t *simp_zero;
    char *zero_text;

    check_simplified_expression_string("exp(W(x)) simplification (EXPR)", "{ exp(W(x)) | x = NAN }",
                                       "{ x/W(x) | x = NAN }");
    check_simplified_expression_string("exp(W(0)) keeps removable singularity folded (binding)", "{ exp(W(0)) }", "1");

    zero = expr_new_const(NUM_ZERO);
    w_zero = expr_lambert_w(zero);
    exp_w_zero = expr_exp(w_zero);
    simp_zero = expr_simplify(exp_w_zero);
    zero_text = simp_zero ? expr_to_string(simp_zero, style_EXPRESSION) : NULL;

    if (str_eq(zero_text, "1"))
        to_string_pass("exp(W(0)) keeps removable singularity folded (EXPR)", zero_text, "1");
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(W(0)) keeps removable singularity folded (EXPR)",
                       zero_text ? zero_text : "(null)", "1");

    free(zero_text);
    expr_free(simp_zero);
    expr_free(exp_w_zero);
    expr_free(w_zero);
    expr_free(zero);
}

static void test_simplify_exp_quarter_turns(void)
{
    expr_t *pi = test_expr_new_named_const_qf(QF_PI, "@pi");
    expr_t *i = test_expr_new_named_const_qc(QC_I, "i");
    expr_t *pi_i = expr_mul(pi, i);
    expr_t *half = test_expr_new_const_d(2.0);
    expr_t *pi_i_over_2 = expr_div(pi_i, half);
    expr_t *exp_pi_i_over_2 = expr_exp(pi_i_over_2);
    expr_t *simp = expr_simplify(exp_pi_i_over_2);
    char *expr_s = expr_to_string(simp, style_EXPRESSION);
    const char *expect = "i";

    if (str_eq(expr_s, expect))
        to_string_pass("exp(pi*i/2) simplification (EXPR)", expr_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "exp(pi*i/2) simplification (EXPR)", expr_s, expect);

    free(expr_s);
    expr_free(simp);
    expr_free(exp_pi_i_over_2);
    expr_free(pi_i_over_2);
    expr_free(half);
    expr_free(pi_i);
    expr_free(i);
    expr_free(pi);
}

static void test_simplify_two_exp_minus_one_to_two_over_e(void)
{
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *minus_one = test_expr_new_const_d(-1.0);
    expr_t *exp_minus_one = expr_exp(minus_one);
    expr_t *product = expr_mul(two, exp_minus_one);
    expr_t *simp = expr_simplify(product);
    char *expr_s = expr_to_string(simp, style_UNBOUND);
    const char *expect = "2/e";

    if (str_eq(expr_s, expect))
        to_string_pass("2*exp(-1) simplification (UNBOUND)", expr_s, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "2*exp(-1) simplification (UNBOUND)", expr_s, expect);

    free(expr_s);
    expr_free(simp);
    expr_free(product);
    expr_free(exp_minus_one);
    expr_free(minus_one);
    expr_free(two);
}

static void test_simplify_trig_and_hyperbolic_identities(void)
{
    static const struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {
            "{ sin(x)^2 + cos(x)^2 | x = NAN }",
            "1",
            "sin^2(x)+cos^2(x) simplifies to 1",
        },
        {
            "{ cos(x)^2 - sin(x)^2 | x = NAN }",
            "{ cos(2x) | x = NAN }",
            "cos^2(x)-sin^2(x) simplifies to cos(2x)",
        },
        {
            "{ -sin(x)^2 + cos(x)^2 | x = NAN }",
            "{ cos(2x) | x = NAN }",
            "-sin^2(x)+cos^2(x) simplifies to cos(2x)",
        },
        {
            "{ sin(x)/cos(x) | x = NAN }",
            "{ tan(x) | x = NAN }",
            "sin(x)/cos(x) simplifies to tan(x)",
        },
        {
            "{ (tan(x) + tan(y))/(1 - tan(x)*tan(y)) | x = ?, y = ? }",
            "{ tan(x + y) | x = NAN, y = NAN }",
            "tangent addition quotient simplifies to tan(x+y)",
        },
        {
            "{ (tan(x) - tan(y))/(1 + tan(y)*tan(x)) | x = ?, y = ? }",
            "{ tan(x - y) | x = NAN, y = NAN }",
            "tangent subtraction quotient simplifies to tan(x-y)",
        },
        {
            "{ (x^2)^3 | x = NAN }",
            "{ x⁶ | x = NAN }",
            "(x^2)^3 simplifies to x^6",
        },
        {
            "{ exp(x)^3 | x = NAN }",
            "{ exp(3x) | x = NAN }",
            "exp(x)^3 simplifies to exp(3x)",
        },
        {
            "{ e^x | x = NAN }",
            "{ exp(x) | x = NAN }",
            "e^x simplifies to exp(x)",
        },
        {
            "{ (e^x)^2 | x = NAN }",
            "{ exp(2x) | x = NAN }",
            "(e^x)^2 simplifies to exp(2x)",
        },
        {
            "{ e^x^2 | x = NAN }",
            "{ exp(2x) | x = NAN }",
            "e^x^2 simplifies left-associatively to exp(2x)",
        },
        {
            "{ e^(x^2) | x = NAN }",
            "{ exp(x²) | x = NAN }",
            "e^(x^2) keeps the power inside the exponent",
        },
        {
            "{ exp(x)*exp(y) | x = NAN, y = NAN }",
            "{ exp(x + y) | x = NAN, y = NAN }",
            "exp(x)*exp(y) simplifies to exp(x+y)",
        },
        {
            "{ 1/cos(x) | x = NAN }",
            "{ sec(x) | x = NAN }",
            "1/cos(x) simplifies to sec(x)",
        },
        {
            "{ 1/sin(x) | x = NAN }",
            "{ cosec(x) | x = NAN }",
            "1/sin(x) simplifies to cosec(x)",
        },
        {
            "{ 1/tan(x) | x = NAN }",
            "{ cot(x) | x = NAN }",
            "1/tan(x) simplifies to cot(x)",
        },
        {
            "{ 1/tan(x)^3 | x = NAN }",
            "{ cot³(x) | x = NAN }",
            "1/tan(x)^3 simplifies to cot^3(x)",
        },
        {
            "{ 1/sec(x) | x = NAN }",
            "{ cos(x) | x = NAN }",
            "1/sec(x) simplifies to cos(x)",
        },
        {
            "{ 1/cosec(x) | x = NAN }",
            "{ sin(x) | x = NAN }",
            "1/cosec(x) simplifies to sin(x)",
        },
        {
            "{ 1/cot(x) | x = NAN }",
            "{ tan(x) | x = NAN }",
            "1/cot(x) simplifies to tan(x)",
        },
        {
            "{ atan(tan(x)) | x = NAN }",
            "{ x - π·⌊½·(2x/π + 1)⌋ | x = NAN }",
            "atan(tan(x)) simplifies to saw-tooth principal value",
        },
        {
            "{ tan(x - pi*floor(1/2*(2*x/pi + 1))) | x = NAN }",
            "{ tan(x) | x = NAN }",
            "tan of atan(tan) saw-tooth simplifies by periodicity",
        },
        {
            "{ tan(x - floor(pi*(1.5+3.2i))) | x = NAN }",
            "{ tan(x - 4 + 10i) | x = NAN }",
            "floor of complex const folds inside tan argument",
        },
        {
            "{ tan(atan(tan(x))) | x = NAN }",
            "{ tan(x) | x = NAN }",
            "tan(atan(tan(x))) simplifies to tan(x)",
        },
        {
            "{ cos(x)*tan(x) | x = NAN }",
            "{ sin(x) | x = NAN }",
            "cos(x)*tan(x) simplifies to sin(x)",
        },
        {
            "{ tan(x)*cos(x) | x = NAN }",
            "{ sin(x) | x = NAN }",
            "tan(x)*cos(x) simplifies to sin(x)",
        },
        {
            "{ sin(x)*cos(x) | x = NAN }",
            "{ ½·sin(2x) | x = NAN }",
            "sin(x)*cos(x) simplifies to half sin double angle",
        },
        {
            "{ cos(x)*sin(x) | x = NAN }",
            "{ ½·sin(2x) | x = NAN }",
            "cos(x)*sin(x) simplifies to half sin double angle",
        },
        {
            "{ cos(x)+i*sin(x) | x = NAN }",
            "{ exp(ix) | x = NAN }",
            "Euler identity simplifies to exponential form",
        },
        {
            "{ ln(cos(x)+i*sin(x)) | x = NAN }",
            "{ ix | x = NAN }",
            "logarithm of an Euler identity simplifies",
        },
        {
            "{ (cos(x)+i*sin(x))^2 | x = NAN }",
            "{ exp(2ix) | x = NAN }",
            "Euler-form square simplifies to an exponential",
        },
        {
            "sqrt(2^2)",
            "2",
            "square root of a positive numeric square simplifies",
        },
        {
            "{ (sqrt(2)/2*sqrt(sqrt(x^2+y^2)+x)+i*sqrt(2)/2*y*sqrt(sqrt(x^2+y^2)-x)/abs(y))^2 | "
            "x = NAN, y = NAN }",
            "{ x + iy | x = NAN, y = NAN }",
            "square of the Cartesian principal complex root simplifies to its argument",
        },
        {
            "{ cosh(x)^2 - sinh(x)^2 | x = NAN }",
            "1",
            "cosh^2(x)-sinh^2(x) simplifies to 1",
        },
        {
            "{ sinh(x)^2 + cosh(x)^2 | x = NAN }",
            "{ cosh(2x) | x = NAN }",
            "sinh^2(x)+cosh^2(x) simplifies to cosh(2x)",
        },
        {
            "{ cosh(x)^2 + sinh(x)^2 | x = NAN }",
            "{ cosh(2x) | x = NAN }",
            "cosh^2(x)+sinh^2(x) simplifies to cosh(2x)",
        },
        {
            "{ sinh(x)/cosh(x) | x = NAN }",
            "{ tanh(x) | x = NAN }",
            "sinh(x)/cosh(x) simplifies to tanh(x)",
        },
        {
            "{ 1/cosh(x) | x = NAN }",
            "{ sech(x) | x = NAN }",
            "1/cosh(x) simplifies to sech(x)",
        },
        {
            "{ 1/sinh(x) | x = NAN }",
            "{ cosech(x) | x = NAN }",
            "1/sinh(x) simplifies to cosech(x)",
        },
        {
            "{ 1/tanh(x) | x = NAN }",
            "{ coth(x) | x = NAN }",
            "1/tanh(x) simplifies to coth(x)",
        },
        {
            "{ 1/sech(x) | x = NAN }",
            "{ cosh(x) | x = NAN }",
            "1/sech(x) simplifies to cosh(x)",
        },
        {
            "{ 1/cosech(x) | x = NAN }",
            "{ sinh(x) | x = NAN }",
            "1/cosech(x) simplifies to sinh(x)",
        },
        {
            "{ 1/coth(x) | x = NAN }",
            "{ tanh(x) | x = NAN }",
            "1/coth(x) simplifies to tanh(x)",
        },
        {
            "{ cosh(x)*tanh(x) | x = NAN }",
            "{ sinh(x) | x = NAN }",
            "cosh(x)*tanh(x) simplifies to sinh(x)",
        },
        {
            "{ tanh(x)*cosh(x) | x = NAN }",
            "{ sinh(x) | x = NAN }",
            "tanh(x)*cosh(x) simplifies to sinh(x)",
        },
        {
            "{ sinh(x)*cosh(x) | x = NAN }",
            "{ ½·sinh(2x) | x = NAN }",
            "sinh(x)*cosh(x) simplifies to half sinh double angle",
        },
        {
            "{ cosh(x)*sinh(x) | x = NAN }",
            "{ ½·sinh(2x) | x = NAN }",
            "cosh(x)*sinh(x) simplifies to half sinh double angle",
        },
        {
            "{ sinh(i*x) | x = NAN }",
            "{ i·sin(x) | x = NAN }",
            "sinh(i*x) simplifies to i*sin(x)",
        },
        {
            "{ cosh(i*x) | x = NAN }",
            "{ cos(x) | x = NAN }",
            "cosh(i*x) simplifies to cos(x)",
        },
        {
            "{ cos(i*x) | x = NAN }",
            "{ cosh(x) | x = NAN }",
            "cos(i*x) simplifies to cosh(x)",
        },
        {
            "{ sin(i*x) | x = NAN }",
            "{ i·sinh(x) | x = NAN }",
            "sin(i*x) simplifies to i*sinh(x)",
        },
        {
            "{ -i*sin(i*x) | x = ? }",
            "{ sinh(x) | x = NAN }",
            "-i*sin(i*x) simplifies to sinh(x)",
        },
        {
            "{ i^3*sinh(x) | x = ? }",
            "{ -i·sinh(x) | x = NAN }",
            "i^3*sinh(x) simplifies to -i*sinh(x)",
        },
        {
            "{ (i)^2*sinh(x) | x = ? }",
            "{ -sinh(x) | x = NAN }",
            "(i)^2*sinh(x) simplifies to -sinh(x)",
        },
        {
            "{ ii*sin(x) | x = NAN }",
            "{ -sin(x) | x = NAN }",
            "i*i*sin(x) simplifies to -sin(x)",
        },
        {
            "{ (3 + 4i)*sinh((1 + 2i)*(1-i)*x) | x = ? }",
            "{ (3 + 4i)·sinh((3 + i)x) | x = NAN }",
            "numeric complex factors fold before multiplying variables",
        },
        {
            "{ (5+i)/(5-i) }",
            "¹²⁄₁₃ + ⁵⁄₁₃i",
            "exact complex division preserves rational parts",
        },
        {
            "{ a₀a₁a₂a₃a₄x | x = ?; a₀ = 1024, a₁ = 27, a₂ = 389017, a₃ = 241, a₄ = 1103863 }",
            "{ a₀a₁a₂a₃a₄x | x = NAN; a₀ = 1024, a₁ = 27, a₂ = 389017, a₃ = 241, a₄ = 1103863 }",
            "bound constants stay before variables in products",
        },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i)
        check_simplified_expression_string(cases[i].label, cases[i].input, cases[i].expect);
}

static void test_to_string_imaginary_unit_omits_one(void)
{
    expr_t *i = expr_new_const(NUM_I);
    expr_t *neg_i = expr_new_const(NUM_NEG_I);
    char *i_text = expr_to_string(i, style_EXPRESSION);
    char *neg_i_text = expr_to_string(neg_i, style_EXPRESSION);

    if (str_eq(i_text, "i"))
        to_string_pass("imaginary unit omits coefficient one", i_text, "i");
    else
        to_string_fail(__FILE__, __LINE__, 1, "imaginary unit omits coefficient one", i_text ? i_text : "(null)", "i");

    if (str_eq(neg_i_text, "-i"))
        to_string_pass("negative imaginary unit omits coefficient one", neg_i_text, "-i");
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative imaginary unit omits coefficient one",
                       neg_i_text ? neg_i_text : "(null)", "-i");

    free(neg_i_text);
    free(i_text);
    expr_free(neg_i);
    expr_free(i);
}

static void test_complex_coefficient_stays_grouped(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ (2+3i)*x | x = 5 }", &bindings);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

    if (str_eq(text, "{ (2 + 3i)x | x = 5 }"))
        to_string_pass("complex coefficient remains grouped", text, "{ (2 + 3i)x | x = 5 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "complex coefficient remains grouped", text ? text : "(null)",
                       "{ (2 + 3i)x | x = 5 }");

    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_pure_imaginary_addend_stays_ungrouped(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ 1/13*exp(1/13*(x + 5i)) | x = ? }", &bindings);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect = "{ ¹⁄₁₃·exp(¹⁄₁₃·(x + 5i)) | x = NAN }";

    if (str_eq(text, expect))
        to_string_pass("pure imaginary addend stays ungrouped", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "pure imaginary addend stays ungrouped", text ? text : "(null)", expect);

    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_preserved_complex_function_addend_stays_ungrouped(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ -c*exp(c) + (W(-2)) | c = -2 }", &bindings);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *tex = expr ? expr_to_string(expr, style_LATEX) : NULL;
    const char *expect = "{ -c·exp(c) + W(-2) | c = -2 }";
    const char *expect_TeX = "\\left\\{ -c\\,e^{c} + W(-2) \\;\\middle|\\; c = -2 \\right\\}";

    if (str_eq(text, expect))
        to_string_pass("preserved complex function addend stays ungrouped", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "preserved complex function addend stays ungrouped",
                       text ? text : "(null)", expect);

    if (str_eq(tex, expect_TeX))
        to_string_pass("preserved complex function addend TeX stays ungrouped", tex, expect_TeX);
    else
        to_string_fail(__FILE__, __LINE__, 1, "preserved complex function addend TeX stays ungrouped",
                       tex ? tex : "(null)", expect_TeX);

    free(tex);
    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_updated_decimal_binding_stays_decimal(void)
{
    const char *input = "{ (i)^2*sinh(x) | x = "
                        "-0.881373587019543025232609324979792309028160328261635410753295608653377184222026 }";
    const char *expect = "{ -sinh(x) | x = "
                         "-0.881373587019543025232609324979792309028160328261635410753295608653377184222026 }";
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(input, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    number_t value = x ? expr_get_val(x) : num_clone(NUM_NAN);
    expr_t *simp;
    char *text;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x);

    expr_set_val(x, value);
    num_destroy(&value);

    simp = expr_simplify(expr);
    text = simp ? expr_to_string(simp, style_EXPRESSION) : NULL;

    if (str_eq(text, expect))
        to_string_pass("updated exact decimal binding stays decimal", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "updated exact decimal binding stays decimal", text ? text : "(null)",
                       expect);

    free(text);
    expr_free(simp);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_negative_decimal_function_argument_stays_decimal(void)
{
    const char *input = "{ normal_cdf(1.96) - normal_cdf(-1.96) }";
    const char *expect = "normal_cdf(1.96) - normal_cdf(-1.96)";
    expr_t *expr = expr_from_string(input, NULL);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *tex = expr ? expr_to_string(expr, style_LATEX) : NULL;
    int text_ok = str_eq(text, expect);
    int TeX_ok = tex && strstr(tex, "-1.96") != NULL && strstr(tex, "\\frac{49}{25}") == NULL;

    if (text_ok)
        to_string_pass("negative decimal function argument stays decimal", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative decimal function argument stays decimal",
                       text ? text : "(null)", expect);

    if (TeX_ok)
        to_string_pass("negative decimal function argument TeX stays decimal", tex, "TeX contains -1.96");
    else {
        printf(C_RED "  FAIL: negative decimal function argument TeX stays decimal\n" C_RESET);
        printf("    tex = %s\n", tex ? tex : "(null)");
        TEST_FAIL();
    }

    free(tex);
    free(text);
    expr_free(expr);
}

static void test_exact_decimal_literal_stays_decimal_in_expression_render(void)
{
    const char *input = "{ exp(sin(1.57079632679489661923132169163975144209858471948544343596133822168661754865486"
                        " - 8.98275566258534851033990592436991679214844400177870060641283930271949038958935E-441i)) }";
    expr_t *expr = expr_from_string(input, NULL);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *func = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
    int text_ok = text && strstr(text, "1.5707963267948966") != NULL && strstr(text, "/5000000000000000") == NULL;
    int func_ok = func && strstr(func, "1.5707963267948966") != NULL && strstr(func, "/5000000000000000") == NULL;

    if (text_ok)
        to_string_pass("exact decimal literal expression render stays decimal", text,
                       "contains original decimal literal");
    else
        to_string_fail(__FILE__, __LINE__, 1, "exact decimal literal expression render stays decimal",
                       text ? text : "(null)", "contains original decimal literal without giant fraction");

    if (func_ok)
        to_string_pass("exact decimal literal function render stays decimal", func,
                       "contains original decimal literal");
    else
        to_string_fail(__FILE__, __LINE__, 1, "exact decimal literal function render stays decimal",
                       func ? func : "(null)", "contains original decimal literal without giant fraction");

    free(func);
    free(text);
    expr_free(expr);
}

static void test_to_string_does_not_simplify_plain_expressions(void)
{
    expr_t *x = test_expr_new_named_var_d(3.0, "x");
    expr_t *xx = expr_mul(x, x);
    expr_t *dx = expr_create_deriv(xx, x);
    char *expr_text = expr_to_string(xx, style_EXPRESSION);
    char *deriv_text = dx ? expr_to_string(dx, style_EXPRESSION) : NULL;
    const char *expr_expect = "{ xx | x = 3 }";
    const char *deriv_expect = "{ 2x | x = 3 }";

    if (str_eq(expr_text, expr_expect))
        to_string_pass("plain to_string preserves x*x", expr_text, expr_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "plain to_string preserves x*x", expr_text ? expr_text : "(null)",
                       expr_expect);

    if (deriv_text && str_eq(deriv_text, deriv_expect))
        to_string_pass("derivative creation still simplifies (x*x)'", deriv_text, deriv_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "derivative creation still simplifies (x*x)'",
                       deriv_text ? deriv_text : "(null)", deriv_expect);

    free(deriv_text);
    free(expr_text);
    expr_free(dx);
    expr_free(xx);
    expr_free(x);
}

static void test_atan_quotient_derivative_simplifies_to_quartic(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ atan(x/(1-x^2)) + C | x = pi/2 }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *derivative = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *text = derivative ? expr_to_string(derivative, style_EXPRESSION) : NULL;
    const char *expect = "{ (x² + 1)/(x⁴ - x² + 1) | x = π/2 }";

    if (text && str_eq(text, expect))
        to_string_pass("atan quotient derivative simplifies to quartic", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "atan quotient derivative simplifies to quartic", text ? text : "(null)",
                       expect);

    free(text);
    expr_free(derivative);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_polynomial_quotient_derivative_collects_numerator(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ (x^2+1)/(x^4-x^2+1) | x=pi/2 }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *derivative = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *text = derivative ? expr_to_string(derivative, style_EXPRESSION) : NULL;
    const char *expect = "{ 2x·(2 - x⁴ - 2x²)/(x⁴ - x² + 1)² | x = π/2 }";

    if (text && str_eq(text, expect))
        to_string_pass("polynomial quotient derivative collects numerator", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "polynomial quotient derivative collects numerator",
                       text ? text : "(null)", expect);

    free(text);
    expr_free(derivative);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_compound_antiderivative_derivative_cancels_rational_terms(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ -1/4*(ln(x^4-x^2+1)-4*x*atan(x/(1-x^2))"
                                    "+2*sqrt(3)*atan((2*x^2-1)/sqrt(3))) | x=pi/2 }",
                                    &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *derivative = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *text = derivative ? expr_to_string(derivative, style_EXPRESSION) : NULL;
    const char *expect = "{ atan(x/(1 - x²)) | x = π/2 }";

    if (text && str_eq(text, expect))
        to_string_pass("compound antiderivative derivative cancels rational terms", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "compound antiderivative derivative cancels rational terms",
                       text ? text : "(null)", expect);

    free(text);
    expr_free(derivative);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_simplify_reuses_clean_nodes_and_dirty_mutations(void)
{
    expr_t *x = test_expr_new_named_var_d(3.0, "x");
    expr_t *xx = expr_mul(x, x);
    expr_t *first = expr_simplify(xx);
    expr_t *second = first ? expr_simplify(first) : NULL;
    char *first_text = first ? expr_to_string(first, style_EXPRESSION) : NULL;
    char *second_text = second ? expr_to_string(second, style_EXPRESSION) : NULL;

    if (first && second && first == second && str_eq(first_text, "{ x² | x = 3 }"))
        to_string_pass("expr_simplify reuses already simplified clean node", second_text, "{ x² | x = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "expr_simplify reuses already simplified clean node",
                       second_text ? second_text : "(null)", "{ x² | x = 3 }");

    {
        number_t five = num_from_qtext("5");
        expr_set_val(x, five);
        num_destroy(&five);
    }

    expr_t *third = first ? expr_simplify(first) : NULL;
    char *third_text = third ? expr_to_string(third, style_EXPRESSION) : NULL;

    if (third && third != first && str_eq(third_text, "{ x² | x = 5 }"))
        to_string_pass("expr_set_val dirties simplified ancestors lazily", third_text, "{ x² | x = 5 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "expr_set_val dirties simplified ancestors lazily",
                       third_text ? third_text : "(null)", "{ x² | x = 5 }");

    free(third_text);
    free(second_text);
    free(first_text);
    expr_free(third);
    expr_free(second);
    expr_free(first);
    expr_free(xx);
    expr_free(x);
}

static void test_gamma_successor_product_simplifies(void)
{
    expr_t *recurrence = expr_from_string("{ x*gamma(x) }", NULL);
    expr_t *not_recurrence = expr_from_string("{ (x + 1)*gamma(x) }", NULL);
    expr_t *recurrence_simp = recurrence ? expr_simplify(recurrence) : NULL;
    expr_t *not_recurrence_simp = not_recurrence ? expr_simplify(not_recurrence) : NULL;
    char *recurrence_text = recurrence_simp ? expr_to_string(recurrence_simp, style_EXPRESSION) : NULL;
    char *not_recurrence_text = not_recurrence_simp ? expr_to_string(not_recurrence_simp, style_EXPRESSION) : NULL;
    const char *recurrence_expect = "{ Γ(x + 1) | x = NAN }";
    const char *not_recurrence_expect = "{ (x + 1)·Γ(x) | x = NAN }";

    if (str_eq(recurrence_text, recurrence_expect))
        to_string_pass("x*gamma(x) simplifies by gamma recurrence", recurrence_text, recurrence_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "x*gamma(x) simplifies by gamma recurrence",
                       recurrence_text ? recurrence_text : "(null)", recurrence_expect);

    if (str_eq(not_recurrence_text, not_recurrence_expect))
        to_string_pass("(x+1)*gamma(x) is not the gamma recurrence", not_recurrence_text, not_recurrence_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "(x+1)*gamma(x) is not the gamma recurrence",
                       not_recurrence_text ? not_recurrence_text : "(null)", not_recurrence_expect);

    free(not_recurrence_text);
    free(recurrence_text);
    expr_free(not_recurrence_simp);
    expr_free(recurrence_simp);
    expr_free(not_recurrence);
    expr_free(recurrence);
}

static void test_lgamma_successor_sum_simplifies(void)
{
    expr_t *recurrence = expr_from_string("{ ln(x) + lgamma(x) }", NULL);
    expr_t *not_recurrence = expr_from_string("{ ln(x + 1) + lgamma(x) }", NULL);
    expr_t *recurrence_simp = recurrence ? expr_simplify(recurrence) : NULL;
    expr_t *not_recurrence_simp = not_recurrence ? expr_simplify(not_recurrence) : NULL;
    char *recurrence_text = recurrence_simp ? expr_to_string(recurrence_simp, style_EXPRESSION) : NULL;
    char *not_recurrence_text = not_recurrence_simp ? expr_to_string(not_recurrence_simp, style_EXPRESSION) : NULL;
    const char *recurrence_expect = "{ lnΓ(x + 1) | x = NAN }";
    const char *not_recurrence_expect = "{ ln(x + 1) + lnΓ(x) | x = NAN }";

    if (str_eq(recurrence_text, recurrence_expect))
        to_string_pass("ln(x)+lgamma(x) simplifies by log-gamma recurrence", recurrence_text, recurrence_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "ln(x)+lgamma(x) simplifies by log-gamma recurrence",
                       recurrence_text ? recurrence_text : "(null)", recurrence_expect);

    if (str_eq(not_recurrence_text, not_recurrence_expect))
        to_string_pass("ln(x+1)+lgamma(x) is not the log-gamma recurrence", not_recurrence_text, not_recurrence_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "ln(x+1)+lgamma(x) is not the log-gamma recurrence",
                       not_recurrence_text ? not_recurrence_text : "(null)", not_recurrence_expect);

    free(not_recurrence_text);
    free(recurrence_text);
    expr_free(not_recurrence_simp);
    expr_free(recurrence_simp);
    expr_free(not_recurrence);
    expr_free(recurrence);
}

static void test_log_constant_difference_simplifies_to_quotient(void)
{
    expr_t *expr = expr_from_string("{ ln(6) - ln(3) }", NULL);
    expr_t *simp = expr ? expr_simplify(expr) : NULL;
    char *text = simp ? expr_to_string(simp, style_EXPRESSION) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(simp);
    TEST_ASSERT_STR_EQ(text, "ln(2)");

    free(text);
    expr_free(simp);
    expr_free(expr);
}

static void test_symbolic_negative_pi_derivative_stays_symbolic(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ -3*pi*sqrt(x) | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ -³⁄₂π/√(x) | x = NAN }";

    if (str_eq(deriv_text, expect))
        to_string_pass("negative symbolic pi derivative stays symbolic", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative symbolic pi derivative stays symbolic",
                       deriv_text ? deriv_text : "(null)", expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_pow_derivative_preserves_literal_base_log(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ 10^x | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ ln(10)·10^x | x = NAN }";

    if (str_eq(deriv_text, expect))
        to_string_pass("10^x derivative preserves ln(10)", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "10^x derivative preserves ln(10)", deriv_text ? deriv_text : "(null)",
                       expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_complex_power_derivative_keeps_base_log(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ a^(ix) | x = NAN; a = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strstr(deriv_text, "ln(a)") && strstr(deriv_text, "a^(ix)") && !strstr(deriv_text, "AN²"))
        to_string_pass("a^(ix) derivative keeps ln(a)", deriv_text, deriv_text);
    else
        to_string_fail(__FILE__, __LINE__, 1, "a^(ix) derivative keeps ln(a)", deriv_text ? deriv_text : "(null)",
                       "{ i·ln(a)·a^(ix) | x = NAN; a = NAN }");

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_power_derivative_uses_n_minus_one_form(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^n | x = NAN, n = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ n·x^(n - 1) | n = NAN, x = NAN }";

    if (str_eq(deriv_text, expect))
        to_string_pass("x^n derivative simplifies to n*x^(n-1)", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "x^n derivative simplifies to n*x^(n-1)",
                       deriv_text ? deriv_text : "(null)", expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_named_half_exponent_round_trips_as_symbolic_power(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *round_bindings = NULL;
    expr_t *expr = expr_from_string("{ (x + a)^n | x = NAN; a = 2, n = 1/2 }", &bindings);
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    expr_t *round = expr_text ? expr_from_string(expr_text, &round_bindings) : NULL;
    expr_t *x = round_bindings ? expr_bindings_get(round_bindings, "x") : NULL;
    expr_t *deriv = (round && x) ? expr_create_deriv(round, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expr_expect = "{ (x + a)^n | x = NAN; a = 2, n = ¹⁄₂ }";
    const char *deriv_expect = "{ n·(x + a)^(n - 1) | x = NAN; n = ¹⁄₂, a = 2 }";

    if (str_eq(expr_text, expr_expect))
        to_string_pass("named half exponent round-trips as symbolic power", expr_text, expr_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "named half exponent round-trips as symbolic power",
                       expr_text ? expr_text : "(null)", expr_expect);

    if (str_eq(deriv_text, deriv_expect))
        to_string_pass("named half exponent derivative keeps n", deriv_text, deriv_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "named half exponent derivative keeps n",
                       deriv_text ? deriv_text : "(null)", deriv_expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(round_bindings);
    expr_free(round);
    free(expr_text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_function_power_matches_parenthesized_power(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ sin^n(x) }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expr_expect = "{ sin(x)^n | x = NAN; n = NAN }";
    const char *deriv_expect = "{ n·cos(x)·sin(x)^(n - 1) | x = NAN; n = NAN }";

    if (str_eq(expr_text, expr_expect))
        to_string_pass("sin^n(x) parses as sin(x)^n", expr_text, expr_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin^n(x) parses as sin(x)^n", expr_text ? expr_text : "(null)",
                       expr_expect);

    if (str_eq(deriv_text, deriv_expect))
        to_string_pass("sin^n(x) derivative matches sin(x)^n", deriv_text, deriv_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin^n(x) derivative matches sin(x)^n",
                       deriv_text ? deriv_text : "(null)", deriv_expect);

    free(deriv_text);
    free(expr_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_inverse_power_function_notation_uses_supported_inverses_only(void)
{
    expr_t *asin_expr = expr_from_string("{ sin^-1(x) }", NULL);
    expr_t *log_expr = expr_from_string("{ log^-1(x) }", NULL);
    expr_t *sqrt_expr = expr_from_string("{ sqrt^-1(x) }", NULL);
    expr_t *sqrt_power_expr = expr_from_string("{ sqrt(x)^-1 }", NULL);
    char *asin_text = asin_expr ? expr_to_string(asin_expr, style_EXPRESSION) : NULL;
    char *sqrt_power_text = sqrt_power_expr ? expr_to_string(sqrt_power_expr, style_EXPRESSION) : NULL;

    if (str_eq(asin_text, "{ asin(x) | x = NAN }"))
        to_string_pass("sin^-1(x) parses as asin(x)", asin_text, "{ asin(x) | x = NAN }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin^-1(x) parses as asin(x)", asin_text ? asin_text : "(null)",
                       "{ asin(x) | x = NAN }");

    if (log_expr == NULL)
        to_string_pass("log^-1(x) is rejected", "(null)", "(null)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "log^-1(x) is rejected", "(non-null)", "(null)");

    if (sqrt_expr == NULL)
        to_string_pass("sqrt^-1(x) is rejected", "(null)", "(null)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sqrt^-1(x) is rejected", "(non-null)", "(null)");

    if (str_eq(sqrt_power_text, "{ 1/√(x) | x = NAN }"))
        to_string_pass("sqrt(x)^-1 remains valid", sqrt_power_text, "{ 1/√(x) | x = NAN }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sqrt(x)^-1 remains valid", sqrt_power_text ? sqrt_power_text : "(null)",
                       "{ 1/√(x) | x = NAN }");

    free(sqrt_power_text);
    free(asin_text);
    expr_free(sqrt_power_expr);
    expr_free(sqrt_expr);
    expr_free(log_expr);
    expr_free(asin_expr);
}

static void test_bound_euler_symbol_survives_derivative_simplify(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ a^(ix) | x = pi/4; a = e }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

    if (deriv_text && strstr(deriv_text, "a^(ix)") && strstr(deriv_text, "x = π/4") && strstr(deriv_text, "a = e") &&
        !strstr(deriv_text, "exp(ix)") && (strstr(deriv_text, "ln(a)") || strstr(deriv_text, "{ i·a^(ix) |")))
        to_string_pass("a=e binding survives derivative simplify", deriv_text, deriv_text);
    else
        to_string_fail(__FILE__, __LINE__, 1, "a=e binding survives derivative simplify",
                       deriv_text ? deriv_text : "(null)", "{ i·ln(a)·a^(ix) | x = π/4; a = e }");

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_log_of_imaginary_product_derivative_cancels_i(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ log(ix) | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ 1/(x·ln(10)) | x = NAN }";

    if (str_eq(deriv_text, expect))
        to_string_pass("log(ix) derivative cancels imaginary unit", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(ix) derivative cancels imaginary unit",
                       deriv_text ? deriv_text : "(null)", expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_negative_quotient_derivative_has_single_sign(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(
        "{ -exp(tan(x))*(tan^2(x)+1)/(exp^2(tan(x))*(tan^2(x)+1)^2+y^2) | x = pi/2, y = pi/4 }", &bindings);
    expr_t *y = bindings ? expr_bindings_get(bindings, "y") : NULL;
    expr_t *deriv = (expr && y) ? expr_create_deriv(expr, y) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ 2y·exp(tan(x))·(tan²(x) + 1)/((tan²(x) + 1)²·exp(2·tan(x)) + y²)² | y = π/4, x = π/2 }";

    if (str_eq(deriv_text, expect))
        to_string_pass("negative quotient derivative has a single sign", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative quotient derivative has a single sign",
                       deriv_text ? deriv_text : "(null)", expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_ln10_product_expression_round_trips(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ 1/(x·ln(10)) | x = NAN }", &bindings);
    char *text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect = "{ 1/(x·ln(10)) | x = NAN }";

    if (str_eq(text, expect))
        to_string_pass("ln(10) product expression round-trips", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "ln(10) product expression round-trips", text ? text : "(null)", expect);

    free(text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_lambert_inverse_argument_derivative_simplifies(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *simplify_bindings = NULL;
    expr_t *simplify_expr = expr_from_string("{ W₀(x*exp(x)) | x = 5 }", &simplify_bindings);
    expr_t *simplified_expr = simplify_expr ? expr_simplify(simplify_expr) : NULL;
    expr_t *expr = expr_from_string("{ W₀(x*exp(x)) | x = 5 }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *simplify_text = simplified_expr ? expr_to_string(simplified_expr, style_EXPRESSION) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect_simplify = "5";
    const char *expect = "1";

    if (str_eq(simplify_text, expect_simplify))
        to_string_pass("W0(x*exp(x)) resolves principal branch for x=5", simplify_text, expect_simplify);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W0(x*exp(x)) resolves principal branch for x=5",
                       simplify_text ? simplify_text : "(null)", expect_simplify);

    if (str_eq(deriv_text, expect))
        to_string_pass("W0(x*exp(x)) derivative simplifies", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W0(x*exp(x)) derivative simplifies", deriv_text ? deriv_text : "(null)",
                       expect);

    free(deriv_text);
    free(simplify_text);
    expr_free(deriv);
    expr_free(simplified_expr);
    expr_bindings_free(simplify_bindings);
    expr_bindings_free(bindings);
    expr_free(simplify_expr);
    expr_free(expr);
}

static void test_lambert_inverse_branch_selection(void)
{
    expr_bindings_t *bindings_w0 = NULL;
    expr_bindings_t *bindings_wm1 = NULL;
    expr_bindings_t *bindings_w = NULL;
    expr_bindings_t *bindings_productlog = NULL;
    expr_t *w0 = expr_from_string("{ W₀(x*exp(x)) | x = -2 }", &bindings_w0);
    expr_t *wm1 = expr_from_string("{ W-1(x*exp(x)) | x = -2 }", &bindings_wm1);
    expr_t *w = expr_from_string("{ W(x*exp(x)) | x = -2 }", &bindings_w);
    expr_t *productlog = expr_from_string("{ productlog(x*exp(x)) | x = -2 }", &bindings_productlog);
    expr_t *wm1_simplified = wm1 ? expr_simplify(wm1) : NULL;
    expr_t *w_simplified = w ? expr_simplify(w) : NULL;
    expr_t *productlog_simplified = productlog ? expr_simplify(productlog) : NULL;
    expr_t *w_branch = expr_from_string("{ W(-1/e) }", NULL);
    expr_t *productlog_branch = expr_from_string("{ productlog(-1/e) }", NULL);
    expr_t *w0_branch = expr_from_string("{ W₀(-1/e) }", NULL);
    expr_t *wm1_branch = expr_from_string("{ W-1(-1/e) }", NULL);
    expr_t *w_outside_real_domain = expr_from_string("{ W(-2) }", NULL);
    number_t w_branch_value = w_branch ? expr_eval(w_branch) : NUM_NAN;
    number_t productlog_branch_value = productlog_branch ? expr_eval(productlog_branch) : NUM_NAN;
    number_t w0_branch_value = w0_branch ? expr_eval(w0_branch) : NUM_NAN;
    number_t wm1_branch_value = wm1_branch ? expr_eval(wm1_branch) : NUM_NAN;
    number_t w_outside_value = w_outside_real_domain ? expr_eval(w_outside_real_domain) : NUM_NAN;
    number_t w_outside_exp = num_exp(w_outside_value);
    number_t w_outside_check = num_mul(w_outside_value, w_outside_exp);
    number_t neg_two = num_create_from_string("-2");
    char *w0_text = w0 ? expr_to_string(w0, style_EXPRESSION) : NULL;
    char *wm1_text = wm1_simplified ? expr_to_string(wm1_simplified, style_EXPRESSION) : NULL;
    char *w_text = w_simplified ? expr_to_string(w_simplified, style_EXPRESSION) : NULL;
    char *productlog_text = productlog_simplified ? expr_to_string(productlog_simplified, style_EXPRESSION) : NULL;
    string_t *w_branch_text = num_to_string(w_branch_value);
    string_t *productlog_branch_text = num_to_string(productlog_branch_value);
    string_t *w0_branch_text = num_to_string(w0_branch_value);
    string_t *wm1_branch_text = num_to_string(wm1_branch_value);
    const char *expect_w0 = "{ W₀(x·exp(x)) | x = -2 }";
    const char *expect_wm1 = "-2";
    const char *expect_w = "-2";
    const char *expect_branch = "-1";

    if (str_eq(w0_text, expect_w0))
        to_string_pass("W0(x*exp(x)) keeps principal branch for x=-2", w0_text, expect_w0);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W0(x*exp(x)) keeps principal branch for x=-2",
                       w0_text ? w0_text : "(null)", expect_w0);

    if (str_eq(wm1_text, expect_wm1))
        to_string_pass("W-1(x*exp(x)) resolves lower branch for x=-2", wm1_text, expect_wm1);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W-1(x*exp(x)) resolves lower branch for x=-2",
                       wm1_text ? wm1_text : "(null)", expect_wm1);

    if (str_eq(w_text, expect_w))
        to_string_pass("W(x*exp(x)) chooses lower branch for x=-2", w_text, expect_w);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W(x*exp(x)) chooses lower branch for x=-2", w_text ? w_text : "(null)",
                       expect_w);

    if (str_eq(productlog_text, expect_w))
        to_string_pass("productlog(x*exp(x)) chooses lower branch for x=-2", productlog_text, expect_w);
    else
        to_string_fail(__FILE__, __LINE__, 1, "productlog(x*exp(x)) chooses lower branch for x=-2",
                       productlog_text ? productlog_text : "(null)", expect_w);

    if (str_eq(formatted_number_cstr(w_branch_text), expect_branch))
        to_string_pass("W(-1/e) resolves branch point exactly", formatted_number_cstr(w_branch_text), expect_branch);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W(-1/e) resolves branch point exactly",
                       formatted_number_cstr(w_branch_text), expect_branch);

    if (str_eq(formatted_number_cstr(productlog_branch_text), expect_branch))
        to_string_pass("productlog(-1/e) resolves branch point exactly", formatted_number_cstr(productlog_branch_text),
                       expect_branch);
    else
        to_string_fail(__FILE__, __LINE__, 1, "productlog(-1/e) resolves branch point exactly",
                       formatted_number_cstr(productlog_branch_text), expect_branch);

    if (str_eq(formatted_number_cstr(w0_branch_text), expect_branch))
        to_string_pass("W0(-1/e) resolves branch point exactly", formatted_number_cstr(w0_branch_text), expect_branch);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W0(-1/e) resolves branch point exactly",
                       formatted_number_cstr(w0_branch_text), expect_branch);

    if (str_eq(formatted_number_cstr(wm1_branch_text), expect_branch))
        to_string_pass("W-1(-1/e) resolves branch point exactly", formatted_number_cstr(wm1_branch_text),
                       expect_branch);
    else
        to_string_fail(__FILE__, __LINE__, 1, "W-1(-1/e) resolves branch point exactly",
                       formatted_number_cstr(wm1_branch_text), expect_branch);

    ASSERT_TRUE(!num_is_real(w_outside_value));
    ASSERT_TRUE(number_close_with_tolerance_text(w_outside_check, neg_two, "1e-25"));

    num_destroy(&neg_two);
    num_destroy(&w_outside_check);
    num_destroy(&w_outside_exp);
    num_destroy(&w_outside_value);
    string_free(wm1_branch_text);
    string_free(w0_branch_text);
    string_free(productlog_branch_text);
    string_free(w_branch_text);
    num_destroy(&wm1_branch_value);
    num_destroy(&w0_branch_value);
    num_destroy(&productlog_branch_value);
    num_destroy(&w_branch_value);
    free(productlog_text);
    free(w_text);
    free(wm1_text);
    free(w0_text);
    expr_bindings_free(bindings_productlog);
    expr_bindings_free(bindings_w);
    expr_bindings_free(bindings_wm1);
    expr_bindings_free(bindings_w0);
    expr_free(wm1_branch);
    expr_free(w0_branch);
    expr_free(productlog_branch);
    expr_free(w_branch);
    expr_free(w_outside_real_domain);
    expr_free(productlog_simplified);
    expr_free(w_simplified);
    expr_free(wm1_simplified);
    expr_free(productlog);
    expr_free(w);
    expr_free(wm1);
    expr_free(w0);
}

static void test_productlog_small_complex_inverse_uses_principal_branch(void)
{
    expr_t *expr = expr_from_string("{ productlog(1/13i*exp(1/13i)) }", NULL);
    number_t value = expr ? expr_eval(expr) : NUM_NAN;
    number_t expected = num_create_from_string("1/13i");
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect_text = "1/13i";

    if (str_eq(expr_text, expect_text))
        to_string_pass("productlog complex principal branch simplifies", expr_text, expect_text);
    else
        to_string_fail(__FILE__, __LINE__, 1, "productlog complex principal branch simplifies",
                       expr_text ? expr_text : "(null)", expect_text);

    ASSERT_TRUE(number_close_with_tolerance_text(value, expected, "1e-30"));

    free(expr_text);
    num_destroy(&expected);
    num_destroy(&value);
    expr_free(expr);
}

static void test_factorial_postfix_lowers_to_differentiable_gamma(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x! | x = 5 }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    number_t value = expr ? expr_eval(expr) : NUM_NAN;
    number_t deriv_value = deriv ? expr_eval(deriv) : NUM_NAN;
    number_t six = num_create_from_long(6);
    number_t expected_value = num_create_from_long(120);
    number_t gamma_six = num_gamma(six);
    number_t digamma_six = num_digamma(six);
    number_t expected_deriv = num_mul(gamma_six, digamma_six);

    ASSERT_TRUE(num_eq(value, expected_value));
    ASSERT_TRUE(number_close_with_tolerance_text(deriv_value, expected_deriv, "1e-30"));

    num_destroy(&expected_deriv);
    num_destroy(&digamma_six);
    num_destroy(&gamma_six);
    num_destroy(&expected_value);
    num_destroy(&six);
    num_destroy(&deriv_value);
    num_destroy(&value);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_repeated_preserved_log_factor_combines_as_power(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ ln(10)*10^x | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expect = "{ ln(10)²·10^x | x = NAN }";

    if (str_eq(deriv_text, expect))
        to_string_pass("repeated preserved ln(10) factors combine as ln(10)^2", deriv_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "repeated preserved ln(10) factors combine as ln(10)^2",
                       deriv_text ? deriv_text : "(null)", expect);

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_preserved_log_power_chain_combines_as_power(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ ln(10)^2*ln(10)*10^x | x = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    const char *expr_expect = "{ ln(10)³·10^x | x = NAN }";
    const char *deriv_expect = "{ ln(10)⁴·10^x | x = NAN }";

    if (str_eq(expr_text, expr_expect))
        to_string_pass("preserved ln(10)^2*ln(10) combines as ln(10)^3", expr_text, expr_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "preserved ln(10)^2*ln(10) combines as ln(10)^3",
                       expr_text ? expr_text : "(null)", expr_expect);

    if (str_eq(deriv_text, deriv_expect))
        to_string_pass("derivative preserves combined ln(10)^4 factor", deriv_text, deriv_expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "derivative preserves combined ln(10)^4 factor",
                       deriv_text ? deriv_text : "(null)", deriv_expect);

    free(deriv_text);
    free(expr_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_unary_constants_preserve_user_literals_in_derivatives(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {"{ sin(1)*x | x = NAN }", "sin(1)", "sin(1)*x derivative preserves sin(1)"},
        {"{ sqrt(2)*x | x = NAN }", "√(2)", "sqrt(2)*x derivative preserves sqrt(2)"},
        {"{ exp(1)*x | x = NAN }", "exp(1)", "exp(1)*x derivative preserves exp(1)"},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
        expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
        char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;

        if (str_eq(deriv_text, cases[i].expect))
            to_string_pass(cases[i].label, deriv_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, deriv_text ? deriv_text : "(null)", cases[i].expect);

        free(deriv_text);
        expr_free(deriv);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_preserved_reciprocal_constant_derivative_round_trips(void)
{
    size_t old_precision = num_get_default_prec_digits();
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *x = NULL;
    expr_t *deriv = NULL;
    char *deriv_text = NULL;
    char *deriv_TeX = NULL;
    int derivative_is_parse_safe;
    int TeX_keeps_symbolic_pi;
    const char *deriv_expect = "{ -2x·exp(-x²)/√(π) | x = NAN }";

    num_set_default_prec_digits(100u);
    expr = expr_from_string("{ 1/sqrt(pi)*exp(-x^2) | x = NAN }", &bindings);
    x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    deriv_TeX = deriv ? expr_to_string(deriv, style_LATEX) : NULL;
    derivative_is_parse_safe = deriv_text && strcmp(deriv_text, deriv_expect) == 0 && !strstr(deriv_text, "-21/√(π)");
    TeX_keeps_symbolic_pi = deriv_TeX && strstr(deriv_TeX, "\\sqrt{\\pi}") && strstr(deriv_TeX, "\\frac{") &&
                            !strstr(deriv_TeX, "1.128379");

    if (derivative_is_parse_safe && TeX_keeps_symbolic_pi) {
        to_string_pass("preserved reciprocal derivative round-trips safely", deriv_text,
                       "symbolic reciprocal coefficient");
    } else {
        printf(C_RED "  FAIL: preserved reciprocal derivative round-trips safely\n" C_RESET);
        printf("    derivative = %s\n", deriv_text ? deriv_text : "(null)");
        printf("    tex        = %s\n", deriv_TeX ? deriv_TeX : "(null)");
        printf("    expected   = %s, with TeX keeping sqrt(pi) as a fraction\n", deriv_expect);
        num_set_default_prec_digits(old_precision);
        TEST_FAIL();
    }

    free(deriv_TeX);
    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
    num_set_default_prec_digits(old_precision);
}

static void test_binary_constants_preserve_user_literals_in_derivatives(void)
{
    struct {
        const char *input;
        const char *expr_expect;
        const char *deriv_expect;
        const char *label;
        bool simplify_expr;
    } cases[] = {
        {"{ atan2(1,2)*x | x = NAN }", "{ atan2(1, 2)x | x = NAN }", "atan2(1, 2)", "atan2(1,2)*x preserves atan2(1,2)",
         false},
        {"{ hypot(3,4)*x | x = NAN }", "{ hypot(3, 4)x | x = NAN }", "hypot(3, 4)", "hypot(3,4)*x preserves hypot(3,4)",
         false},
        {"{ beta(2,3)*x | x = NAN }", "{ beta(2, 3)x | x = NAN }", "beta(2, 3)", "beta(2,3)*x preserves beta(2,3)",
         false},
        {"{ logbeta(2,3)*x | x = NAN }", "{ -ln(12)x | x = NAN }", "-ln(12)",
         "logbeta(2,3)*x rewrites exactly to -ln(12)", true},
        {"{ logbeta(1,1)*x | x = NAN }", "0", "0", "logbeta(1,1)*x rewrites exactly to zero", true},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
        expr_t *expr_for_text = cases[i].simplify_expr && expr ? expr_simplify(expr) : NULL;
        expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
        char *expr_text = expr ? expr_to_string(expr_for_text ? expr_for_text : expr, style_EXPRESSION) : NULL;
        char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
        char deriv_label[128];

        if (str_eq(expr_text, cases[i].expr_expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expr_expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)",
                           cases[i].expr_expect);

        snprintf(deriv_label, sizeof(deriv_label), "%s derivative", cases[i].label);
        if (str_eq(deriv_text, cases[i].deriv_expect))
            to_string_pass(deriv_label, deriv_text, cases[i].deriv_expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, deriv_label, deriv_text ? deriv_text : "(null)",
                           cases[i].deriv_expect);

        free(deriv_text);
        free(expr_text);
        expr_free(deriv);
        expr_free(expr_for_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_symbolic_negative_pi_quotient_stays_symbolic(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ (1/2)*(-3*pi)/sqrt(x) | x = NAN }", &bindings);
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect = "{ -³⁄₂π/√(x) | x = NAN }";

    if (str_eq(expr_text, expect))
        to_string_pass("negative symbolic pi quotient stays symbolic", expr_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "negative symbolic pi quotient stays symbolic",
                       expr_text ? expr_text : "(null)", expect);

    free(expr_text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_sqrt_quotient_combines_positive_real_denominator(void)
{
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *sqrt_pi = expr_sqrt(pi);
    expr_t *sqrt_two = expr_sqrt(two);
    expr_t *quotient = expr_div(sqrt_pi, sqrt_two);
    expr_t *simp = expr_simplify(quotient);
    char *text = simp ? expr_to_string(simp, style_UNBOUND) : NULL;
    const char *expect = "√(π/2)";

    if (str_eq(text, expect))
        to_string_pass("sqrt quotient combines positive real denominator", text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "sqrt quotient combines positive real denominator",
                       text ? text : "(null)", expect);

    free(text);
    expr_free(simp);
    expr_free(quotient);
    expr_free(sqrt_two);
    expr_free(sqrt_pi);
    expr_free(two);
    expr_free(pi);
}

static void test_real_scalar_over_square_root_combines_into_one_root(void)
{
    number_t three_value = num_create_from_long(3L);
    number_t thirty_three_value = num_create_from_long(33L);
    expr_t *three = expr_new_const(three_value);
    expr_t *thirty_three = expr_new_const(thirty_three_value);
    expr_t *scaled_three = expr_new_const(three_value);
    expr_t *scaled_thirty_three = expr_new_const(thirty_three_value);
    expr_t *x = expr_new_var(NUM_NAN);
    expr_t *scalar_root = expr_sqrt(thirty_three);
    expr_t *scaled_root = expr_sqrt(scaled_thirty_three);
    expr_t *scaled_num;
    expr_t *raw_scalar;
    expr_t *raw_scaled;
    expr_t *scalar;
    expr_t *scaled;
    char *scalar_text;
    char *scaled_text;

    expr_set_name(x, "x");
    scaled_num = expr_mul(scaled_three, x);
    raw_scalar = expr_div(three, scalar_root);
    raw_scaled = expr_div(scaled_num, scaled_root);
    scalar = raw_scalar ? expr_simplify(raw_scalar) : NULL;
    scaled = raw_scaled ? expr_simplify(raw_scaled) : NULL;
    scalar_text = scalar ? expr_to_string(scalar, style_UNBOUND) : NULL;
    scaled_text = scaled ? expr_to_string(scaled, style_UNBOUND) : NULL;

    if (str_eq(scalar_text, "√(³⁄₁₁)"))
        to_string_pass("real scalar over square root combines into one root", scalar_text, "√(³⁄₁₁)");
    else
        to_string_fail(__FILE__, __LINE__, 1, "real scalar over square root combines into one root",
                       scalar_text ? scalar_text : "(null)", "√(³⁄₁₁)");

    if (str_eq(scaled_text, "√(³⁄₁₁)·x"))
        to_string_pass("leading real scalar over square root combines into one root", scaled_text, "√(³⁄₁₁)·x");
    else
        to_string_fail(__FILE__, __LINE__, 1, "leading real scalar over square root combines into one root",
                       scaled_text ? scaled_text : "(null)", "√(³⁄₁₁)·x");

    free(scaled_text);
    free(scalar_text);
    expr_free(scaled);
    expr_free(scalar);
    expr_free(raw_scaled);
    expr_free(raw_scalar);
    expr_free(scaled_num);
    expr_free(scaled_root);
    expr_free(scalar_root);
    expr_free(x);
    expr_free(scaled_thirty_three);
    expr_free(scaled_three);
    expr_free(thirty_three);
    expr_free(three);
    num_destroy(&thirty_three_value);
    num_destroy(&three_value);
}

static void test_nested_symbolic_pi_derivative_has_no_decimalized_coefficients(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ (1/16)*pi*exp(pi*sqrt(x))*(15*pi*sqrt(x) + "
                                    "x*(pi*(-3*pi*sqrt(x) + pi^2*x)/sqrt(x) + 2*pi^2) - "
                                    "5*pi^2*x - 15)/x^(7/2) | x = pi/6 }",
                                    &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *deriv = (expr && x) ? expr_create_deriv(expr, x) : NULL;
    char *deriv_text = deriv ? expr_to_string(deriv, style_EXPRESSION) : NULL;
    int has_decimalized_pi =
        deriv_text && (strstr(deriv_text, "4.712") || strstr(deriv_text, "9.424") || strstr(deriv_text, "3.084"));

    if (deriv_text && !has_decimalized_pi)
        to_string_pass("nested symbolic pi derivative keeps coefficients symbolic", deriv_text,
                       "(no decimalized pi coefficients)");
    else {
        printf(C_RED "  FAIL: nested symbolic pi derivative keeps coefficients symbolic\n" C_RESET);
        printf("    got      = %s\n", deriv_text ? deriv_text : "(null)");
        printf("    expected = no 4.712..., 9.424..., or 3.084... coefficients\n");
        TEST_FAIL();
    }

    free(deriv_text);
    expr_free(deriv);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_binding_exact_unary_numeric_expression_simplifies(void)
{
    static const struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {
            "{ x | x = ln(e)*pi*60/180 }",
            "⅓π",
            "binding ln(e)*pi*60/180 simplifies exactly",
        },
        {
            "{ x | x = log(10^12) }",
            "12",
            "binding log(10^12) simplifies exactly",
        },
        {
            "{ x | x = log(1/1000) }",
            "-3",
            "binding log(1/1000) simplifies exactly",
        },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_binding_exact_trig_numeric_expression_simplifies(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x | x = sin(pi/6)*pi/180*30 }", &bindings);
    char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
    const char *expect = "¹⁄₁₂π";

    if (str_eq(expr_text, expect))
        to_string_pass("binding sin(pi/6)*pi/180*30 simplifies exactly", expr_text, expect);
    else
        to_string_fail(__FILE__, __LINE__, 1, "binding sin(pi/6)*pi/180*30 simplifies exactly",
                       expr_text ? expr_text : "(null)", expect);

    free(expr_text);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_pi_ratio_addsub_uses_number_fraction_arithmetic(void)
{
    static const struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {
            "{ π - ⅔π }",
            "⅓π",
            "pi minus two thirds pi simplifies via numeric fraction arithmetic",
        },
        {
            "{ pi - 4*pi/6 }",
            "⅓π",
            "pi minus four pi sixths simplifies via numeric fraction arithmetic",
        },
        {
            "{ -4*(pi/6) }",
            "-⅔π",
            "scaled pi sixth remains symbolic",
        },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_t *expr = expr_from_string(cases[i].input, NULL);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_free(expr);
    }
}

static void test_binding_direct_inverse_numeric_expression_simplifies(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {"{ x | x = sin(asin(12)) }", "12", "binding sin(asin(12))"},
        {"{ x | x = cos(acos(12)) }", "12", "binding cos(acos(12))"},
        {"{ x | x = tan(atan(12)) }", "12", "binding tan(atan(12))"},
        {"{ x | x = sinh(asinh(12)) }", "12", "binding sinh(asinh(12))"},
        {"{ x | x = cosh(acosh(12)) }", "12", "binding cosh(acosh(12))"},
        {"{ x | x = tanh(atanh(12)) }", "12", "binding tanh(atanh(12))"},
        {"{ x | x = exp(ln(12)) }", "12", "binding exp(ln(12))"},
        {"{ x | x = ln(exp(12)) }", "12", "binding ln(exp(12))"},
        {"{ x | x = erf(erfinv(12)) }", "12", "binding erf(erfinv(12))"},
        {"{ x | x = erfc(erfcinv(12)) }", "12", "binding erfc(erfcinv(12))"},
        {"{ x | x = gamma(gammainv(12)) }", "12", "binding gamma(gammainv(12))"},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_binding_principal_inverse_numeric_expression_simplifies(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {"{ x | x = atan(tan(pi/5)) }", "π/5", "binding atan(tan(pi/5))"},
        {"{ x | x = asin(sin(pi/5)) }", "π/5", "binding asin(sin(pi/5))"},
        {"{ x | x = acos(cos(pi/5)) }", "π/5", "binding acos(cos(pi/5))"},
        {"{ x | x = atan(tan(3*pi/5)) }", "atan(tan(⅗π))", "binding atan(tan(3*pi/5)) is not principal"},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_binding_lambert_inverse_numeric_expression_simplifies(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {
            "{ x | x = W₀(pi/5)*exp(W₀(pi/5)) }",
            "π/5",
            "binding W0(a)*exp(W0(a))",
        },
        {
            "{ x | x = exp(W(pi/5))*W(pi/5) }",
            "π/5",
            "binding exp(W(a))*W(a)",
        },
        {
            "{ x | x = W-1(-1/10)*exp(W-1(-1/10)) }",
            "-⅒",
            "binding W-1(a)*exp(W-1(a))",
        },
        {
            "{ x | x = W₀(5*exp(5)) }",
            "5",
            "binding W0(a*exp(a))",
        },
        {
            "{ x | x = W(exp(pi/5)*pi/5) }",
            "π/5",
            "binding W(exp(a)*a)",
        },
        {
            "{ x | x = W(1/13i*exp(1/13i)) }",
            "1/13i",
            "binding W complex principal branch inverse",
        },
        {
            "{ x | x = W-1(-2*exp(-2)) }",
            "-2",
            "binding W-1(a*exp(a))",
        },
        {
            "{ x | x = W₀(-2*exp(-2)) }",
            "W₀(-2·exp(-2))",
            "binding W0(a*exp(a)) outside principal branch",
        },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_binding_successor_and_trig_shape_simplifies(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {
            "{ x | x = pi/5*gamma(pi/5) }",
            "Γ(π/5 + 1)",
            "binding gamma successor simplifies",
        },
        {
            "{ x | x = pi*gamma(pi/5)/5 }",
            "Γ(π/5 + 1)",
            "binding scaled gamma quotient successor simplifies",
        },
        {
            "{ x | x = ln(pi/5) + lgamma(pi/5) }",
            "lnΓ(π/5 + 1)",
            "binding log-gamma successor simplifies",
        },
        {
            "{ x | x = sin(pi/5)^2 + cos(pi/5)^2 }",
            "1",
            "binding sin^2+cos^2 simplifies",
        },
        {
            "{ x | x = cos(pi/5)^2 - sin(pi/5)^2 }",
            "cos(⅖π)",
            "binding cos^2-sin^2 simplifies",
        },
        {
            "{ x | x = sinh(pi/5)^2 + cosh(pi/5)^2 }",
            "cosh(⅖π)",
            "binding sinh^2+cosh^2 simplifies",
        },
        {
            "{ x | x = cosh(pi/5)^2 - sinh(pi/5)^2 }",
            "1",
            "binding cosh^2-sinh^2 simplifies",
        },
        {
            "{ x | x = sin(pi/5)*cos(pi/5) }",
            "½·sin(⅖π)",
            "binding sin*cos simplifies",
        },
        {
            "{ x | x = cos(pi/5)*tan(pi/5) }",
            "sin(π/5)",
            "binding cos*tan simplifies",
        },
        {
            "{ x | x = (pi^2)^3 }",
            "π⁶",
            "binding (pi^2)^3 simplifies to pi^6",
        },
        {
            "{ x | x = (e^pi)^2 }",
            "exp(2π)",
            "binding (e^pi)^2 simplifies to exp(2pi)",
        },
        {
            "{ x | x = exp(pi/5)^3 }",
            "exp(⅗π)",
            "binding exp(pi/5)^3 simplifies to exp(3pi/5)",
        },
        {
            "{ x | x = exp(2)*exp(3) }",
            "exp(5)",
            "binding exp(2)*exp(3) simplifies to exp(5)",
        },
        {
            "{ x | x = e^3 }",
            "exp(3)",
            "binding e^3 simplifies to exp(3)",
        },
        {
            "{ x | x = 1/cos(2) }",
            "sec(2)",
            "binding 1/cos simplifies to sec",
        },
        {
            "{ x | x = 1/sin(2) }",
            "cosec(2)",
            "binding 1/sin simplifies to cosec",
        },
        {
            "{ x | x = 1/tan(2) }",
            "cot(2)",
            "binding 1/tan simplifies to cot",
        },
        {
            "{ x | x = 1/sec(2) }",
            "cos(2)",
            "binding 1/sec simplifies to cos",
        },
        {
            "{ x | x = 1/cosec(2) }",
            "sin(2)",
            "binding 1/cosec simplifies to sin",
        },
        {
            "{ x | x = 1/cot(2) }",
            "tan(2)",
            "binding 1/cot simplifies to tan",
        },
        {
            "{ x | x = (cos(pi/5)+i*sin(pi/5))^2 }",
            "exp(2·i·π/5)",
            "binding Euler-form square simplifies to an exponential",
        },
        {
            "{ x | x = sinh(pi/5)*cosh(pi/5) }",
            "½·sinh(⅖π)",
            "binding sinh*cosh simplifies",
        },
        {
            "{ x | x = cosh(pi/5)*tanh(pi/5) }",
            "sinh(π/5)",
            "binding cosh*tanh simplifies",
        },
        {
            "{ x | x = 1/cosh(2) }",
            "sech(2)",
            "binding 1/cosh simplifies to sech",
        },
        {
            "{ x | x = 1/sinh(2) }",
            "cosech(2)",
            "binding 1/sinh simplifies to cosech",
        },
        {
            "{ x | x = 1/tanh(2) }",
            "coth(2)",
            "binding 1/tanh simplifies to coth",
        },
        {
            "{ x | x = 1/sech(2) }",
            "cosh(2)",
            "binding 1/sech simplifies to cosh",
        },
        {
            "{ x | x = 1/cosech(2) }",
            "sinh(2)",
            "binding 1/cosech simplifies to sinh",
        },
        {
            "{ x | x = 1/coth(2) }",
            "tanh(2)",
            "binding 1/coth simplifies to tanh",
        },
        {
            "{ x | x = sinh(i*pi/5) }",
            "i·sin(π/5)",
            "binding sinh(i*pi/5) simplifies",
        },
        {
            "{ x | x = cosh(i*pi/5) }",
            "cos(π/5)",
            "binding cosh(i*pi/5) simplifies",
        },
        {
            "{ x | x = cos(i*pi/5) }",
            "cosh(π/5)",
            "binding cos(i*pi/5) simplifies",
        },
        {
            "{ x | x = sin(i*pi/5) }",
            "i·sinh(π/5)",
            "binding sin(i*pi/5) simplifies",
        },
        {
            "{ x | x = -i*sin(i*pi/5) }",
            "sinh(π/5)",
            "binding -i*sin(i*pi/5) simplifies",
        },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (expr_text && str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_binding_exact_core_trig_values_simplify(void)
{
    struct {
        const char *input;
        const char *expect;
        const char *label;
    } cases[] = {
        {"{ x | x = sin(0) }", "0", "sin(0)"},
        {"{ x | x = sin(pi/6) }", "½", "sin(pi/6)"},
        {"{ x | x = sin(⅙π) }", "½", "sin(⅙π)"},
        {"{ x | x = sin(pi/4) }", "√(2)/2", "sin(pi/4)"},
        {"{ x | x = sin(pi/3) }", "√(3)/2", "sin(pi/3)"},
        {"{ x | x = sin(pi/2) }", "1", "sin(pi/2)"},
        {"{ x | x = sin(pi) }", "0", "sin(pi)"},
        {"{ x | x = cos(0) }", "1", "cos(0)"},
        {"{ x | x = cos(pi/6) }", "√(3)/2", "cos(pi/6)"},
        {"{ x | x = cos(pi/4) }", "√(2)/2", "cos(pi/4)"},
        {"{ x | x = cos(pi/3) }", "½", "cos(pi/3)"},
        {"{ x | x = cos(pi/2) }", "0", "cos(pi/2)"},
        {"{ x | x = cos(pi) }", "-1", "cos(pi)"},
        {"{ x | x = tan(0) }", "0", "tan(0)"},
        {"{ x | x = tan(pi/6) }", "√(3)/3", "tan(pi/6)"},
        {"{ x | x = tan(pi/4) }", "1", "tan(pi/4)"},
        {"{ x | x = tan(pi/3) }", "√(3)", "tan(pi/3)"},
        {"{ x | x = tan(pi/2) }", "∞", "tan(pi/2)"},
        {"{ x | x = tan(-pi/2) }", "-∞", "tan(-pi/2)"},
        {"{ x | x = tan(pi) }", "0", "tan(pi)"},
        {"{ x | x = sec(0) }", "1", "sec(0)"},
        {"{ x | x = sec(pi/6) }", "2·√(3)/3", "sec(pi/6)"},
        {"{ x | x = sec(pi/4) }", "√(2)", "sec(pi/4)"},
        {"{ x | x = sec(pi/3) }", "2", "sec(pi/3)"},
        {"{ x | x = cosec(pi/6) }", "2", "cosec(pi/6)"},
        {"{ x | x = cosec(pi/4) }", "√(2)", "cosec(pi/4)"},
        {"{ x | x = cosec(pi/3) }", "2·√(3)/3", "cosec(pi/3)"},
        {"{ x | x = cosec(pi/2) }", "1", "cosec(pi/2)"},
        {"{ x | x = cot(pi/6) }", "√(3)", "cot(pi/6)"},
        {"{ x | x = cot(pi/4) }", "1", "cot(pi/4)"},
        {"{ x | x = cot(pi/3) }", "√(3)/3", "cot(pi/3)"},
        {"{ x | x = cot(pi/2) }", "0", "cot(pi/2)"},
        {"{ x | x = asin(0) }", "0", "asin(0)"},
        {"{ x | x = asin(1/2) }", "π/6", "asin(1/2)"},
        {"{ x | x = asin(1/sqrt(2)) }", "π/4", "asin(1/sqrt(2))"},
        {"{ x | x = asin(sqrt(3)/2) }", "π/3", "asin(sqrt(3)/2)"},
        {"{ x | x = asin(1) }", "π/2", "asin(1)"},
        {"{ x | x = asin(-1/2) }", "-(π/6)", "asin(-1/2)"},
        {"{ x | x = asin(-1/sqrt(2)) }", "-(π/4)", "asin(-1/sqrt(2))"},
        {"{ x | x = acos(1) }", "0", "acos(1)"},
        {"{ x | x = acos(sqrt(3)/2) }", "π/6", "acos(sqrt(3)/2)"},
        {"{ x | x = acos(1/sqrt(2)) }", "π/4", "acos(1/sqrt(2))"},
        {"{ x | x = acos(1/2) }", "π/3", "acos(1/2)"},
        {"{ x | x = acos(0) }", "π/2", "acos(0)"},
        {"{ x | x = acos(-1) }", "π", "acos(-1)"},
        {"{ x | x = atan(0) }", "0", "atan(0)"},
        {"{ x | x = atan(sqrt(3)/3) }", "π/6", "atan(sqrt(3)/3)"},
        {"{ x | x = atan(1) }", "π/4", "atan(1)"},
        {"{ x | x = atan(sqrt(3)) }", "π/3", "atan(sqrt(3))"},
        {"{ x | x = atan(-1) }", "-(π/4)", "atan(-1)"},
        {"{ x | x = asec(1) }", "0", "asec(1)"},
        {"{ x | x = asec(2/sqrt(3)) }", "π/6", "asec(2/sqrt(3))"},
        {"{ x | x = asec(sqrt(2)) }", "π/4", "asec(sqrt(2))"},
        {"{ x | x = asec(2) }", "π/3", "asec(2)"},
        {"{ x | x = acosec(-1) }", "-(π/2)", "acosec(-1)"},
        {"{ x | x = acosec(1) }", "π/2", "acosec(1)"},
        {"{ x | x = acosec(2/sqrt(3)) }", "π/3", "acosec(2/sqrt(3))"},
        {"{ x | x = acosec(sqrt(2)) }", "π/4", "acosec(sqrt(2))"},
        {"{ x | x = acosec(2) }", "π/6", "acosec(2)"},
        {"{ x | x = acot(sqrt(3)) }", "π/6", "acot(sqrt(3))"},
        {"{ x | x = acot(1) }", "π/4", "acot(1)"},
        {"{ x | x = acot(1/sqrt(3)) }", "π/3", "acot(1/sqrt(3))"},
        {"{ x | x = acot(0) }", "π/2", "acot(0)"},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;

        if (str_eq(expr_text, cases[i].expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)", cases[i].expect);

        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_tan_poles_display_as_infinity(void)
{
    struct {
        const char *input;
        const char *expr_expect;
        const char *TeX_expect;
        int inf_sign;
        const char *label;
    } cases[] = {{"{ tan(x) | x = π/2 }", "{ tan(x) | x = π/2 }",
                  "\\left\\{ \\tan(x) \\;\\middle|\\; x = \\frac{\\pi}{2} \\right\\}", 1,
                  "tan(pi/2) evaluates to infinity"},
                 {"{ tan(x) | x = -π/2 }", "{ tan(x) | x = -π/2 }",
                  "\\left\\{ \\tan(x) \\;\\middle|\\; x = \\frac{-\\pi}{2} \\right\\}", -1,
                  "tan(-pi/2) evaluates to negative infinity"}};

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
        char *TeX_text = expr ? expr_to_string(expr, style_LATEX) : NULL;
        number_t value = expr ? expr_eval(expr) : num_clone(NUM_NAN);

        if (num_is_inf(value) && num_get_sign(value) == cases[i].inf_sign)
            printf(C_BOLD C_GREEN "PASS" C_RESET " %s\n\n", cases[i].label);
        else {
            printf(C_BOLD C_RED "FAIL" C_RESET " %s: value was not expected infinity\n\n", cases[i].label);
            TEST_FAIL();
        }

        if (str_eq(expr_text, cases[i].expr_expect))
            to_string_pass(cases[i].label, expr_text, cases[i].expr_expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, expr_text ? expr_text : "(null)",
                           cases[i].expr_expect);

        if (str_eq(TeX_text, cases[i].TeX_expect))
            to_string_pass(cases[i].label, TeX_text, cases[i].TeX_expect);
        else
            to_string_fail(__FILE__, __LINE__, 1, cases[i].label, TeX_text ? TeX_text : "(null)", cases[i].TeX_expect);

        num_destroy(&value);
        free(TeX_text);
        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void expect_sqrt_negative_text(const char *label, const char *field, const char *got, const char *expected)
{
    char full_label[160];

    snprintf(full_label, sizeof(full_label), "%s %s", label, field);
    if (str_eq(got, expected))
        to_string_pass(full_label, got, expected);
    else
        to_string_fail(__FILE__, __LINE__, 1, full_label, got ? got : "(null)", expected);
}

static void test_sqrt_negative_exact_evaluates_to_i(void)
{
    const struct {
        const char *label;
        const char *input;
        const char *value;
        const char *expression;
        const char *function;
        const char *tex;
    } cases[] = {{"sqrt(-1)", "{ sqrt(-1) }", "i", "√(-1)",
                  "expression expr() {\n"
                  "    return sqrt(-1);\n"
                  "}\n\n"
                  "output(expr());",
                  "\\sqrt{-1}"},
                 {"sqrt(-4)", "{ sqrt(-4) }", "2i", "√(-4)",
                  "expression expr() {\n"
                  "    return sqrt(-4);\n"
                  "}\n\n"
                  "output(expr());",
                  "\\sqrt{-4}"},
                 {"sqrt(x) with x = -1", "{ sqrt(x) | x = -1 }", "i", "{ √(x) | x = -1 }",
                  "expression expr(x) {\n"
                  "    return sqrt(x);\n"
                  "}\n\n"
                  "x = -1\n"
                  "output(expr(x));",
                  "\\left\\{ \\sqrt{x} \\;\\middle|\\; x = -1 \\right\\}"}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expr = expr_from_string(cases[i].input, &bindings);
        char *expr_text = expr ? expr_to_string(expr, style_EXPRESSION) : NULL;
        char *function_text = expr ? expr_to_string(expr, style_FUNCTION) : NULL;
        char *TeX_text = expr ? expr_to_string(expr, style_LATEX) : NULL;
        number_t value = expr ? expr_eval(expr) : num_clone(NUM_NAN);
        string_t *value_text = num_to_string(value);

        expect_sqrt_negative_text(cases[i].label, "value", formatted_number_cstr(value_text), cases[i].value);
        expect_sqrt_negative_text(cases[i].label, "expression", expr_text, cases[i].expression);
        expect_sqrt_negative_text(cases[i].label, "function", function_text, cases[i].function);
        expect_sqrt_negative_text(cases[i].label, "TeX", TeX_text, cases[i].tex);

        string_free(value_text);
        num_destroy(&value);
        free(TeX_text);
        free(function_text);
        free(expr_text);
        expr_bindings_free(bindings);
        expr_free(expr);
    }
}

static void test_symbolic_complex_square_root_beautifies_to_cartesian_form(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(x + iy)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text,
                       "√(½·(√(x² + y²) + x)) + i·y/|y|·√(½·(√(x² + y²) - x))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_complex_square_root_reciprocal_beautifies_to_cartesian_form(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(x + iy)^(-1/2)", &bindings);
    expr_t *beautified = expr ? expr_beautify_symbolic_complex_square_root_reciprocal_for_display(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text,
                       "1/√(x² + y²)·(√(½·(√(x² + y²) + x)) - "
                       "i·y/|y|·√(½·(√(x² + y²) - x)))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_imaginary_symbolic_complex_square_root_reciprocal_rotates_cartesian_parts(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("i*(x + iy)^(-1/2)", &bindings);
    expr_t *reciprocal = expr ? expr_beautify_symbolic_complex_square_root_reciprocal_for_display(expr) : NULL;
    expr_t *beautified = reciprocal ? expr_beautify_imaginary_cartesian_product_for_display(reciprocal) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text,
                       "1/√(x² + y²)·(y/|y|·√(½·(√(x² + y²) - x)) + "
                       "i·√(½·(√(x² + y²) + x)))");

    free(text);
    expr_free(beautified);
    expr_free(reciprocal);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_exact_complex_square_root_beautifies_to_cartesian_surds(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(2 + 3i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "√(½·(√(13) + 2)) + i·√(½·(√(13) - 2))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_unit_complex_square_root_keeps_conjugate_surds_together(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(1 + i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "√(½·(√(2) + 1)) + i·√(½·(√(2) - 1))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_unit_complex_cube_root_beautifies_to_cartesian_surds(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("cubrt(1 + i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "1/(2·cubrt(2))·(√(3) + 1 + i·(√(3) - 1))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_conjugate_unit_complex_cube_root_keeps_cartesian_surd_symmetry(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("cubrt(1 - i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "1/(2·cubrt(2))·(√(3) + 1 - i·(√(3) - 1))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_root_order_three_uses_the_principal_cube_root_beautification(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("root(1 + i, 3)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "1/(2·cubrt(2))·(√(3) + 1 + i·(√(3) - 1))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_unit_complex_fourth_root_beautifies_to_cartesian_surds(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("root(1 + i, 4)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text,
                       "1/√(2)·(√(root(2, 4) + √(1/2·(√(2) + 1))) + "
                       "i·√(root(2, 4) - √(1/2·(√(2) + 1))))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_conjugate_unit_complex_fourth_root_keeps_cartesian_surd_symmetry(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("root(1 - i, 4)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text,
                       "1/√(2)·(√(root(2, 4) + √(1/2·(√(2) + 1))) - "
                       "i·√(root(2, 4) - √(1/2·(√(2) + 1))))");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_exact_complex_square_root_reduces_perfect_square_components(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(3 + 4i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "2 + i");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_pure_imaginary_square_root_reduces_to_cartesian_components(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(2i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "1 + i");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_larger_complex_square_root_reduces_perfect_square_components(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("sqrt(5 + 12i)", &bindings);
    expr_t *beautified = expr ? expr_beautify_presimplified(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "3 + 2i");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_exact_complex_integer_power_folds_to_cartesian_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(1 + i)^2", &bindings);
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(simplified);
    TEST_ASSERT_STR_EQ(text, "2i");

    free(text);
    expr_free(simplified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_exact_complex_integer_power_with_negative_real_part_folds_to_cartesian_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(-3 + 4i)^2", &bindings);
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(simplified);
    TEST_ASSERT_STR_EQ(text, "-7 - 24i");

    free(text);
    expr_free(simplified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_explicit_complex_cube_root_preserves_family_and_evaluates_principal_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(-2 + 2i)^(1/3)", &bindings);
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_UNBOUND) : NULL;
    number_t value = expr ? expr_eval(expr) : (number_t){0};
    number_t expected = num_add(NUM_ONE, NUM_I);
    number_t seed = (number_t){0};
    long order = 0L;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(simplified);
    TEST_ASSERT_STR_EQ(text, "(-2 + 2i)^⅓");
    ASSERT_TRUE(num_eq(value, expected));
    ASSERT_TRUE(expr_exact_complex_root_seed(expr, &seed, &order));
    ASSERT_EQ_INT(order, 3L);
    ASSERT_TRUE(num_eq(seed, expected));

    num_destroy(&seed);
    num_destroy(&expected);
    num_destroy(&value);
    free(text);
    expr_free(simplified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_sqrt_is_principal_but_explicit_half_power_preserves_both_roots(void)
{
    expr_bindings_t *sqrt_bindings = NULL;
    expr_bindings_t *power_bindings = NULL;
    expr_t *sqrt_expr = expr_from_string("sqrt(3 + 4i)", &sqrt_bindings);
    expr_t *power_expr = expr_from_string("(3 + 4i)^(1/2)", &power_bindings);
    expr_t *sqrt_simplified = sqrt_expr ? expr_simplify(sqrt_expr) : NULL;
    expr_t *power_simplified = power_expr ? expr_simplify(power_expr) : NULL;
    char *sqrt_text = sqrt_simplified ? expr_to_string(sqrt_simplified, style_UNBOUND) : NULL;
    char *power_text = power_simplified ? expr_to_string(power_simplified, style_UNBOUND) : NULL;
    number_t seed = (number_t){0};
    number_t expected = num_add(NUM_TWO, NUM_I);
    long order = 0L;

    ASSERT_NOT_NULL(sqrt_expr);
    ASSERT_NOT_NULL(power_expr);
    ASSERT_NOT_NULL(sqrt_simplified);
    ASSERT_NOT_NULL(power_simplified);
    TEST_ASSERT_STR_EQ(sqrt_text, "2 + i");
    TEST_ASSERT_STR_EQ(power_text, "(3 + 4i)^½");
    ASSERT_FALSE(expr_explicit_root_order(sqrt_expr, &order));
    ASSERT_TRUE(expr_explicit_root_order(power_expr, &order));
    ASSERT_EQ_INT(order, 2L);
    ASSERT_TRUE(expr_exact_complex_root_seed(power_expr, &seed, &order));
    ASSERT_TRUE(num_eq(seed, expected));

    num_destroy(&expected);
    num_destroy(&seed);
    free(power_text);
    free(sqrt_text);
    expr_free(power_simplified);
    expr_free(sqrt_simplified);
    expr_bindings_free(power_bindings);
    expr_bindings_free(sqrt_bindings);
    expr_free(power_expr);
    expr_free(sqrt_expr);
}

static void test_exact_complex_fifth_root_family_finds_cartesian_seed(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(-4 + 4i)^(1/5)", &bindings);
    number_t seed = (number_t){0};
    number_t expected = num_sub(NUM_ONE, NUM_I);
    long order = 0L;

    ASSERT_NOT_NULL(expr);
    ASSERT_TRUE(expr_exact_complex_root_seed(expr, &seed, &order));
    ASSERT_EQ_INT(order, 5L);
    ASSERT_TRUE(num_eq(seed, expected));

    num_destroy(&expected);
    num_destroy(&seed);
    expr_bindings_free(bindings);
    expr_free(expr);
}

/* README example: a named complex sixth root is one exact Cartesian principal value. */
static void test_named_sixth_root_uses_exact_cartesian_principal_value(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("root(117 + 44i, 6)", &bindings);
    expr_t *simplified = expr ? expr_simplify(expr) : NULL;
    expr_t *beautified = simplified ? expr_beautify_presimplified(simplified) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;
    number_t seed = (number_t){0};
    number_t expected_seed = num_add(NUM_ONE, num_mul(NUM_TWO, NUM_I));
    long order = 0L;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(simplified);
    ASSERT_TRUE(expr_exact_complex_root_seed(expr, &seed, &order));
    ASSERT_EQ_INT(order, 6L);
    ASSERT_TRUE(num_eq(seed, expected_seed));
    ASSERT_NOT_NULL(beautified);
    ASSERT_NOT_NULL(text);
    ASSERT_NULL(strstr(text, "root("));
    ASSERT_NOT_NULL(strstr(text, "√(3)"));
    ASSERT_NOT_NULL(strstr(text, "i"));

    num_destroy(&expected_seed);
    num_destroy(&seed);
    free(text);
    expr_free(beautified);
    expr_free(simplified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_symbolic_complex_square_expands_to_cartesian_form(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("(a + bi)^2", &bindings);
    expr_t *beautified = expr ? expr_beautify(expr) : NULL;
    char *text = beautified ? expr_to_string(beautified, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(beautified);
    TEST_ASSERT_STR_EQ(text, "a² - b² + 2abi");

    free(text);
    expr_free(beautified);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_cartesian_tanh_has_native_symbolic_integrals(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ tanh(x+iy) | x = NAN, y = NAN }", &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, "x") : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, "y") : NULL;
    expr_t *x_integral = x ? expr_integrate_family(expr, x) : NULL;
    expr_t *y_integral = y ? expr_integrate_family(expr, y) : NULL;
    char *x_text = x_integral ? expr_to_string(x_integral, style_UNBOUND) : NULL;
    char *y_text = y_integral ? expr_to_string(y_integral, style_UNBOUND) : NULL;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(x_integral);
    ASSERT_NOT_NULL(y_integral);
    ASSERT_NOT_NULL(x_text);
    ASSERT_NOT_NULL(y_text);
    ASSERT_NOT_NULL(strstr(x_text, "ln("));
    ASSERT_NOT_NULL(strstr(x_text, "atan2("));
    ASSERT_NOT_NULL(strstr(x_text, "i"));
    ASSERT_NOT_NULL(strstr(y_text, "ln("));
    ASSERT_NOT_NULL(strstr(y_text, "atan2("));
    ASSERT_NOT_NULL(strstr(y_text, "i"));

    free(y_text);
    free(x_text);
    expr_free(y_integral);
    expr_free(x_integral);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_large_target_uses_significant_digit_tolerance(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ exp(π·√(x)) | x = NAN }", &bindings);
    expr_t *x;
    number_t target = num_create_from_string("262537412640768744");
    number_t expected = num_create_from_string("163");
    number_t x_value;
    expr_goal_seek_options_t options = {0};
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    options.precision_digits = 32u;
    options.allow_complex = true;
    options.simplify_result = false;

    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(!result.used_complex);
    ASSERT_TRUE(num_is_real(result.residual));

    x_value = expr_get_val(x);
    ASSERT_TRUE(number_close_with_tolerance_text(x_value, expected, "1e-25"));

    num_destroy(&x_value);
    expr_goal_seek_result_clear(&result);
    num_destroy(&expected);
    num_destroy(&target);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_iterated_symbolic_integration_moves_out_of_lab(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x*y | x = NAN, y = NAN }", &bindings);
    expr_t *x;
    expr_t *y;
    expr_t *vars[2];
    expr_integration_bound_kind_t kinds[2] = {EXPR_INTEGRATION_BOUND_DEFINITE, EXPR_INTEGRATION_BOUND_DEFINITE};
    expr_t *lo[2];
    expr_t *hi[2];
    expr_t *result = NULL;
    expr_t *first_antiderivative = NULL;
    number_t value = num_new();
    number_t expected = num_create_from_string("0.25");
    size_t completed_steps = 0u;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    y = expr_bindings_get(bindings, "y");
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);

    vars[0] = x;
    vars[1] = y;
    lo[0] = test_expr_new_const_d(0.0);
    lo[1] = test_expr_new_const_d(0.0);
    hi[0] = test_expr_new_const_d(1.0);
    hi[1] = test_expr_new_const_d(1.0);
    ASSERT_NOT_NULL(lo[0]);
    ASSERT_NOT_NULL(lo[1]);
    ASSERT_NOT_NULL(hi[0]);
    ASSERT_NOT_NULL(hi[1]);

    result = expr_integrate_iterated(expr, 2u, vars, kinds, lo, hi, 2u, &completed_steps, &first_antiderivative);
    ASSERT_NOT_NULL(result);
    ASSERT_NOT_NULL(first_antiderivative);
    ASSERT_EQ_INT((int)completed_steps, 2);

    num_destroy(&value);
    value = expr_eval(result);
    ASSERT_TRUE(number_close_with_tolerance_text(value, expected, "1e-20"));

    num_destroy(&value);
    num_destroy(&expected);
    expr_free(first_antiderivative);
    expr_free(result);
    expr_free(hi[1]);
    expr_free(hi[0]);
    expr_free(lo[1]);
    expr_free(lo[0]);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void test_iterated_symbolic_best_effort_reduces_remaining_numeric_dims(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ sin(x^2) * y | x = NAN, y = NAN }", &bindings);
    const char *expected_names[1];
    expr_t *expected_symbols[1];
    expr_t *expected = NULL;
    expr_t *x;
    expr_t *y;
    expr_t *vars[2];
    expr_t *remaining_vars[2] = {NULL, NULL};
    expr_integration_bound_kind_t kinds[2] = {EXPR_INTEGRATION_BOUND_DEFINITE, EXPR_INTEGRATION_BOUND_DEFINITE};
    expr_t *lo[2];
    expr_t *hi[2];
    number_t lo_num[2];
    number_t hi_num[2];
    number_t remaining_lo_num[2];
    number_t remaining_hi_num[2];
    expr_t *result = NULL;
    number_t got_value = num_new();
    number_t expected_value = num_new();
    size_t completed_steps = 0u;
    size_t remaining_ndim = 0u;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    y = expr_bindings_get(bindings, "y");
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);

    expected_names[0] = "x";
    expected_symbols[0] = x;
    expected = expr_from_expression_string("sin(x^2) / 2", expected_names, expected_symbols, 1u);
    ASSERT_NOT_NULL(expected);

    vars[0] = x;
    vars[1] = y;
    lo[0] = test_expr_new_const_d(0.0);
    lo[1] = test_expr_new_const_d(0.0);
    hi[0] = test_expr_new_const_d(1.0);
    hi[1] = test_expr_new_const_d(1.0);
    ASSERT_NOT_NULL(lo[0]);
    ASSERT_NOT_NULL(lo[1]);
    ASSERT_NOT_NULL(hi[0]);
    ASSERT_NOT_NULL(hi[1]);

    lo_num[0] = num_create_from_string("0");
    lo_num[1] = num_create_from_string("0");
    hi_num[0] = num_create_from_string("1");
    hi_num[1] = num_create_from_string("1");
    remaining_lo_num[0] = num_new();
    remaining_lo_num[1] = num_new();
    remaining_hi_num[0] = num_new();
    remaining_hi_num[1] = num_new();

    result = expr_integrate_iterated_best_effort(expr, 2u, vars, kinds, lo, hi, &completed_steps, &remaining_ndim,
                                                 remaining_vars, remaining_lo_num, remaining_hi_num, lo_num, hi_num);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_INT((int)completed_steps, 1);
    ASSERT_EQ_INT((int)remaining_ndim, 1);
    ASSERT_TRUE(remaining_vars[0] == x);
    ASSERT_TRUE(num_eq(remaining_lo_num[0], lo_num[0]));
    ASSERT_TRUE(num_eq(remaining_hi_num[0], hi_num[0]));

    test_expr_set_val_d(x, 0.3);
    num_destroy(&got_value);
    got_value = expr_eval(result);
    num_destroy(&expected_value);
    expected_value = expr_eval(expected);
    ASSERT_TRUE(number_close_with_tolerance_text(got_value, expected_value, "1e-20"));

    num_destroy(&expected_value);
    num_destroy(&got_value);
    expr_free(result);
    num_destroy(&remaining_hi_num[1]);
    num_destroy(&remaining_hi_num[0]);
    num_destroy(&remaining_lo_num[1]);
    num_destroy(&remaining_lo_num[0]);
    num_destroy(&hi_num[1]);
    num_destroy(&hi_num[0]);
    num_destroy(&lo_num[1]);
    num_destroy(&lo_num[0]);
    expr_free(hi[1]);
    expr_free(hi[0]);
    expr_free(lo[1]);
    expr_free(lo[0]);
    expr_free(expected);
    expr_free(expr);
    expr_bindings_free(bindings);
}

void test_runtime_regressions(void)
{
    TEST_RUN_SUBTEST(test_cmp_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_mpfr_precision, NULL);
    TEST_RUN_SUBTEST(test_inexact_known_constant_uses_short_text, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_mpfr_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_complex_precision, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_complex_precision, NULL);
    TEST_RUN_SUBTEST(test_eval_expression_preserves_mpfr_precision, NULL);
    TEST_RUN_SUBTEST(test_eval_expression_preserves_complex_precision, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_preserves_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_preserves_qfloat_precision, NULL);
    TEST_RUN_SUBTEST(test_default_constants_preserve_builtin_precision, NULL);
    TEST_RUN_SUBTEST(test_expr_ln10_singleton, NULL);
    TEST_RUN_SUBTEST(test_get_val_updates_after_set, NULL);
    TEST_RUN_SUBTEST(test_new_const_num, NULL);
    TEST_RUN_SUBTEST(test_new_const_num_rational_complex, NULL);
    TEST_RUN_SUBTEST(test_new_var_num_and_set_val_num, NULL);
    TEST_RUN_SUBTEST(test_named_number_constructors, NULL);
    TEST_RUN_SUBTEST(test_eval_num_on_expression, NULL);
    TEST_RUN_SUBTEST(test_eval_num_function_values, NULL);
    TEST_RUN_SUBTEST(test_removable_trig_quotient_at_zero_evaluates_to_limit, NULL);
    TEST_RUN_SUBTEST(test_eval_num_function_derivatives, NULL);
    TEST_RUN_SUBTEST(test_high_precision_mpfr_function_values, NULL);
    TEST_RUN_SUBTEST(test_high_precision_mpfr_function_derivatives, NULL);
    TEST_RUN_SUBTEST(test_high_precision_complex_function_values, NULL);
    TEST_RUN_SUBTEST(test_set_val_num_named_constant, NULL);
    TEST_RUN_SUBTEST(test_to_string_unbound_omits_binding_wrapper, NULL);
    TEST_RUN_SUBTEST(test_to_string_does_not_simplify_plain_expressions, NULL);
    TEST_RUN_SUBTEST(test_atan_quotient_derivative_simplifies_to_quartic, NULL);
    TEST_RUN_SUBTEST(test_polynomial_quotient_derivative_collects_numerator, NULL);
    TEST_RUN_SUBTEST(test_compound_antiderivative_derivative_cancels_rational_terms, NULL);
    TEST_RUN_SUBTEST(test_simplify_reuses_clean_nodes_and_dirty_mutations, NULL);
    TEST_RUN_SUBTEST(test_gamma_successor_product_simplifies, NULL);
    TEST_RUN_SUBTEST(test_lgamma_successor_sum_simplifies, NULL);
    TEST_RUN_SUBTEST(test_log_constant_difference_simplifies_to_quotient, NULL);
    TEST_RUN_SUBTEST(test_simplify_exact_rational_square_roots, NULL);
    TEST_RUN_SUBTEST(test_simplify_inverse_unary_pairs, NULL);
    TEST_RUN_SUBTEST(test_simplify_lambert_exp_to_quotient, NULL);
    TEST_RUN_SUBTEST(test_simplify_exp_quarter_turns, NULL);
    TEST_RUN_SUBTEST(test_simplify_two_exp_minus_one_to_two_over_e, NULL);
    TEST_RUN_SUBTEST(test_simplify_trig_and_hyperbolic_identities, NULL);
    TEST_RUN_SUBTEST(test_to_string_imaginary_unit_omits_one, NULL);
    TEST_RUN_SUBTEST(test_complex_coefficient_stays_grouped, NULL);
    TEST_RUN_SUBTEST(test_pure_imaginary_addend_stays_ungrouped, NULL);
    TEST_RUN_SUBTEST(test_preserved_complex_function_addend_stays_ungrouped, NULL);
    TEST_RUN_SUBTEST(test_updated_decimal_binding_stays_decimal, NULL);
    TEST_RUN_SUBTEST(test_negative_decimal_function_argument_stays_decimal, NULL);
    TEST_RUN_SUBTEST(test_exact_decimal_literal_stays_decimal_in_expression_render, NULL);
    TEST_RUN_SUBTEST(test_symbolic_negative_pi_derivative_stays_symbolic, NULL);
    TEST_RUN_SUBTEST(test_pow_derivative_preserves_literal_base_log, NULL);
    TEST_RUN_SUBTEST(test_symbolic_complex_power_derivative_keeps_base_log, NULL);
    TEST_RUN_SUBTEST(test_bound_euler_symbol_survives_derivative_simplify, NULL);
    TEST_RUN_SUBTEST(test_log_of_imaginary_product_derivative_cancels_i, NULL);
    TEST_RUN_SUBTEST(test_negative_quotient_derivative_has_single_sign, NULL);
    TEST_RUN_SUBTEST(test_ln10_product_expression_round_trips, NULL);
    TEST_RUN_SUBTEST(test_lambert_inverse_argument_derivative_simplifies, NULL);
    TEST_RUN_SUBTEST(test_lambert_inverse_branch_selection, NULL);
    TEST_RUN_SUBTEST(test_productlog_small_complex_inverse_uses_principal_branch, NULL);
    TEST_RUN_SUBTEST(test_factorial_postfix_lowers_to_differentiable_gamma, NULL);
    TEST_RUN_SUBTEST(test_repeated_preserved_log_factor_combines_as_power, NULL);
    TEST_RUN_SUBTEST(test_preserved_log_power_chain_combines_as_power, NULL);
    TEST_RUN_SUBTEST(test_unary_constants_preserve_user_literals_in_derivatives, NULL);
    TEST_RUN_SUBTEST(test_preserved_reciprocal_constant_derivative_round_trips, NULL);
    TEST_RUN_SUBTEST(test_binary_constants_preserve_user_literals_in_derivatives, NULL);
    TEST_RUN_SUBTEST(test_symbolic_negative_pi_quotient_stays_symbolic, NULL);
    TEST_RUN_SUBTEST(test_sqrt_quotient_combines_positive_real_denominator, NULL);
    TEST_RUN_SUBTEST(test_real_scalar_over_square_root_combines_into_one_root, NULL);
    TEST_RUN_SUBTEST(test_symbolic_power_derivative_uses_n_minus_one_form, NULL);
    TEST_RUN_SUBTEST(test_named_half_exponent_round_trips_as_symbolic_power, NULL);
    TEST_RUN_SUBTEST(test_symbolic_function_power_matches_parenthesized_power, NULL);
    TEST_RUN_SUBTEST(test_inverse_power_function_notation_uses_supported_inverses_only, NULL);
    TEST_RUN_SUBTEST(test_nested_symbolic_pi_derivative_has_no_decimalized_coefficients, NULL);
    TEST_RUN_SUBTEST(test_binding_exact_unary_numeric_expression_simplifies, NULL);
    TEST_RUN_SUBTEST(test_binding_exact_trig_numeric_expression_simplifies, NULL);
    TEST_RUN_SUBTEST(test_symbolic_pi_ratio_addsub_uses_number_fraction_arithmetic, NULL);
    TEST_RUN_SUBTEST(test_binding_direct_inverse_numeric_expression_simplifies, NULL);
    TEST_RUN_SUBTEST(test_binding_principal_inverse_numeric_expression_simplifies, NULL);
    TEST_RUN_SUBTEST(test_binding_lambert_inverse_numeric_expression_simplifies, NULL);
    TEST_RUN_SUBTEST(test_binding_successor_and_trig_shape_simplifies, NULL);
    TEST_RUN_SUBTEST(test_binding_exact_core_trig_values_simplify, NULL);
    TEST_RUN_SUBTEST(test_tan_poles_display_as_infinity, NULL);
    TEST_RUN_SUBTEST(test_sqrt_negative_exact_evaluates_to_i, NULL);
    TEST_RUN_SUBTEST(test_symbolic_complex_square_root_beautifies_to_cartesian_form, NULL);
    TEST_RUN_SUBTEST(test_symbolic_complex_square_root_reciprocal_beautifies_to_cartesian_form, NULL);
    TEST_RUN_SUBTEST(test_imaginary_symbolic_complex_square_root_reciprocal_rotates_cartesian_parts, NULL);
    TEST_RUN_SUBTEST(test_exact_complex_square_root_beautifies_to_cartesian_surds, NULL);
    TEST_RUN_SUBTEST(test_unit_complex_square_root_keeps_conjugate_surds_together, NULL);
    TEST_RUN_SUBTEST(test_unit_complex_cube_root_beautifies_to_cartesian_surds, NULL);
    TEST_RUN_SUBTEST(test_conjugate_unit_complex_cube_root_keeps_cartesian_surd_symmetry, NULL);
    TEST_RUN_SUBTEST(test_root_order_three_uses_the_principal_cube_root_beautification, NULL);
    TEST_RUN_SUBTEST(test_unit_complex_fourth_root_beautifies_to_cartesian_surds, NULL);
    TEST_RUN_SUBTEST(test_conjugate_unit_complex_fourth_root_keeps_cartesian_surd_symmetry, NULL);
    TEST_RUN_SUBTEST(test_exact_complex_square_root_reduces_perfect_square_components, NULL);
    TEST_RUN_SUBTEST(test_pure_imaginary_square_root_reduces_to_cartesian_components, NULL);
    TEST_RUN_SUBTEST(test_larger_complex_square_root_reduces_perfect_square_components, NULL);
    TEST_RUN_SUBTEST(test_exact_complex_integer_power_folds_to_cartesian_value, NULL);
    TEST_RUN_SUBTEST(test_exact_complex_integer_power_with_negative_real_part_folds_to_cartesian_value, NULL);
    TEST_RUN_SUBTEST(test_explicit_complex_cube_root_preserves_family_and_evaluates_principal_value, NULL);
    TEST_RUN_SUBTEST(test_sqrt_is_principal_but_explicit_half_power_preserves_both_roots, NULL);
    TEST_RUN_SUBTEST(test_exact_complex_fifth_root_family_finds_cartesian_seed, NULL);
    TEST_RUN_SUBTEST(test_named_sixth_root_uses_exact_cartesian_principal_value, NULL);
    TEST_RUN_SUBTEST(test_symbolic_complex_square_expands_to_cartesian_form, NULL);
    TEST_RUN_SUBTEST(test_cartesian_tanh_has_native_symbolic_integrals, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_large_target_uses_significant_digit_tolerance, NULL);
    TEST_RUN_SUBTEST(test_iterated_symbolic_integration_moves_out_of_lab, NULL);
    TEST_RUN_SUBTEST(test_iterated_symbolic_best_effort_reduces_remaining_numeric_dims, NULL);
}

/* ------------------------------------------------------------------------- */
/* Reverse mode                                                              */
/* ------------------------------------------------------------------------- */
