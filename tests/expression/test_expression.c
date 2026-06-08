#include "test_expr.h"

static bool test_expr_suite_setup(void);
static void test_expr_post_summary(void);
static int expr_number_exact_equal(const void *actual,
                                   const void *expected,
                                   void *ctx);
static int expr_number_close_equal(const void *actual,
                                   const void *expected,
                                   void *ctx);
static int expr_number_format(const void *value,
                              string_t *out,
                              void *ctx);
static number_t expr_error_magnitude(number_t got, number_t expected);

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
TEST_SUITE_SETUP(test_expr_suite_setup);
TEST_POST_SUMMARY(test_expr_post_summary);

#pragma GCC diagnostic ignored "-Wunused-function"

static bool test_expr_suite_setup(void)
{
    test_register_validity_checker("expr-number-exact",
                                   expr_validity_contract_number_exact());
    test_register_validity_checker("expr-number-close",
                                   expr_validity_contract_number_close());
    return TEST_REQUIRE_VALIDITY_CHECKER("expr-number-exact") &&
           TEST_REQUIRE_VALIDITY_CHECKER("expr-number-close");
}

static int expr_number_exact_equal(const void *actual,
                                   const void *expected,
                                   void *ctx)
{
    (void)ctx;
    return num_eq(*(const number_t *)actual, *(const number_t *)expected);
}

static number_t expr_error_magnitude(number_t got, number_t expected)
{
    number_t promoted_got = num_clone(got);
    number_t diff;
    number_t error;

    if (num_get_prec_bits(expected) > 0u &&
        num_set_prec_bits(&promoted_got, num_get_prec_bits(expected)) != 0) {
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

static int expr_number_close_equal(const void *actual,
                                   const void *expected,
                                   void *ctx)
{
    number_t got = *(const number_t *)actual;
    number_t want = *(const number_t *)expected;
    number_t error;
    number_t one;
    number_t tolerance;
    int ok;

    (void)ctx;
    if (num_eq(got, want))
        return 1;

    error = expr_error_magnitude(got, want);
    one = num_create_from_double(1.0);
    if (num_set_prec_bits(&one, 106u) != 0) {
        num_destroy(&one);
        num_destroy(&error);
        return 0;
    }
    tolerance = num_ldexp(one, 4 - 106);
    ok = num_le(error, tolerance);
    num_destroy(&tolerance);
    num_destroy(&one);
    num_destroy(&error);
    return ok;
}

static int expr_number_format(const void *value,
                              string_t *out,
                              void *ctx)
{
    string_t *text;

    (void)ctx;
    if (!out)
        return -1;
    text = num_to_string(*(const number_t *)value);
    if (!text)
        return string_append_cstr(out, "<num_to_string failed>");
    if (string_append_string(out, text) != 0) {
        string_free(text);
        return -1;
    }
    string_free(text);
    return 0;
}

const test_validity_contract_t *expr_validity_contract_number_exact(void)
{
    static const test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("expr-number-exact",
                               expr_number_exact_equal,
                               expr_number_format,
                               NULL);

    return &contract;
}

const test_validity_contract_t *expr_validity_contract_number_close(void)
{
    static const test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("expr-number-close",
                               expr_number_close_equal,
                               expr_number_format,
                               NULL);

    return &contract;
}

/* ------------------------------------------------------------------------- */
/* Compact qfloat_t comparison (kept exactly as-is, but using harness colours) */
/* ------------------------------------------------------------------------- */

void check_q_at(const char *file, int line, int col,
                const char *label, qfloat_t got, qfloat_t expect)
{
    qfloat_t diff = qf_sub(got, expect);
    double abs_err = fabs(qf_to_double(diff));
    double exp_d   = fabs(qf_to_double(expect));

    double rel_err = (exp_d > 0)? abs_err / exp_d : abs_err;

    const double ABS_TOL = 1e-15;
    const double REL_TOL = 1e-15;

    if (abs_err < ABS_TOL || rel_err < REL_TOL) {
        printf("%s%sPASS%s %-32s  got=", C_BOLD, C_GREEN, C_RESET, label);
        qf_printf("%.34q", got);
        printf("\n");
        return;
    }

    printf("%s%sFAIL%s %s: %s:%d:%d: got=", C_BOLD, C_RED, C_RESET, label, file, line, col);
    TEST_FAIL();

    qf_printf("%.34q", got);

    printf(" expect=");
    qf_printf("%.34q", expect);

    printf(" diff=");
    qf_printf("%.34q", diff);

    printf("\n");

}

void print_expr_of(const expr_t *f)
{
    char *s = expr_to_string(f, style_EXPRESSION);
    printf(C_YELLOW "     = %s\n" C_RESET, s);
    free(s);
}

static string_t *format_num_at_own_precision(const number_t value)
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

static void print_num_line(const char *label, const number_t value)
{
    string_t *text = format_num_at_own_precision(value);

    if (!text)
        text = num_to_string(value);
    if (text)
        string_printf("%-8s = %S\n", label, text);
    else
        string_printf("%-8s = (unavailable)\n", label);
    string_free(text);
}

static expr_t *make_readme_f(expr_t *x)
{
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    number_t seven = num_create_from_long(7);
    expr_t *sinx   = expr_sin(x);
    expr_t *exp_sx = expr_exp(sinx);
    expr_t *x2     = expr_pow(x, &two);
    expr_t *term2  = expr_mul_num(x2, &three);
    expr_t *f0     = expr_add(exp_sx, term2);
    expr_t *f      = expr_sub_num(f0, &seven);

    expr_free(sinx);
    expr_free(exp_sx);
    expr_free(x2);
    expr_free(term2);
    expr_free(f0);
    num_destroy(&two);
    num_destroy(&seven);
    num_destroy(&three);

    return f;
}

static int run_readme_example(void)
{
    const size_t old_prec_bits = num_get_default_prec_bits();
    number_t x0;
    expr_t *x;
    expr_t *f;
    expr_t *df_dx;
    const expr_t *d2f_dx;
    number_t f_val;
    number_t d1_val;
    number_t d2_val;

    if (num_set_default_prec_bits(384u) != 0)
        return 1;
    x0 = num_create_from_string("1.25");
    x = expr_new_named_var(x0, "x");
    num_destroy(&x0);
    if (!x) {
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    f = make_readme_f(x);
    if (!f) {
        expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    df_dx = expr_create_deriv(f, x);
    if (!df_dx) {
        expr_free(f);
        expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    d2f_dx = expr_get_deriv(df_dx, x);
    if (!d2f_dx) {
        expr_free(df_dx);
        expr_free(f);
        expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    f_val  = expr_eval(f);
    d1_val = expr_eval(df_dx);
    d2_val = expr_eval(d2f_dx);

    printf("f(x)    = "); expr_print(f);
    printf("f'(x)   = "); expr_print(df_dx);
    printf("f''(x)  = "); expr_print(d2f_dx);

    printf("\nAt x = 1.25 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(f_val));
    print_num_line("f(x)", f_val);
    print_num_line("f'(x)", d1_val);
    print_num_line("f''(x)", d2_val);

    num_destroy(&d2_val);
    num_destroy(&d1_val);
    num_destroy(&f_val);
    expr_free(df_dx);
    expr_free(f);
    expr_free(x);
    num_set_default_prec_bits(old_prec_bits);
    return 0;
}

static int run_readme_from_string_example(void)
{
    const size_t old_prec_bits = num_get_default_prec_bits();
    expr_bindings_t *bindings = NULL;
    expr_t *x;
    expr_t *f;
    expr_t *df_dx;
    const expr_t *d2f_dx;
    number_t f_val;
    number_t d1_val;
    number_t d2_val;

    if (num_set_default_prec_bits(384u) != 0)
        return 1;
    f = expr_from_string("{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }",
                         &bindings);
    if (!f) {
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }
    x = expr_bindings_get(bindings, "x");
    if (!x) {
        expr_free(f);
        expr_bindings_free(bindings);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    df_dx = expr_create_deriv(f, x);
    if (!df_dx) {
        expr_free(f);
        expr_bindings_free(bindings);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    d2f_dx = expr_get_deriv(df_dx, x);
    if (!d2f_dx) {
        expr_free(df_dx);
        expr_free(f);
        expr_bindings_free(bindings);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    f_val  = expr_eval(f);
    d1_val = expr_eval(df_dx);
    d2_val = expr_eval(d2f_dx);

    printf("f(x)    = "); expr_print(f);
    printf("f'(x)   = "); expr_print(df_dx);
    printf("f''(x)  = "); expr_print(d2f_dx);

    printf("\nAt x = 1.25 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(f_val));
    print_num_line("f(x)", f_val);
    print_num_line("f'(x)", d1_val);
    print_num_line("f''(x)", d2_val);

    num_destroy(&d2_val);
    num_destroy(&d1_val);
    num_destroy(&f_val);
    expr_free(df_dx);
    expr_free(f);
    expr_bindings_free(bindings);
    num_set_default_prec_bits(old_prec_bits);
    return 0;
}

static int run_readme_partial_example(void)
{
    const size_t old_prec_bits = num_get_default_prec_bits();
    number_t x0;
    number_t y0;
    expr_t *x;
    expr_t *y;
    expr_t *x2;
    expr_t *xy;
    expr_t *y2;
    expr_t *t0;
    expr_t *f;
    expr_t *df_dx;
    expr_t *df_dy;
    expr_t *d2f_dxdy;
    const expr_t *p;
    number_t value;

    if (num_set_default_prec_bits(384u) != 0)
        return 1;
    x0 = num_create_from_string("1");
    y0 = num_create_from_string("2");
    x = expr_new_named_var(x0, "x");
    y = expr_new_named_var(y0, "y");
    num_destroy(&y0);
    num_destroy(&x0);
    x2 = expr_pow_d(x, 2.0);
    xy = expr_mul(x, y);
    y2 = expr_pow_d(y, 2.0);
    t0 = expr_add(x2, xy);
    f = expr_add(t0, y2);

    if (!x || !y || !x2 || !xy || !y2 || !t0 || !f) {
        expr_free(f); expr_free(t0); expr_free(y2); expr_free(xy); expr_free(x2);
        expr_free(y); expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    df_dx    = expr_create_deriv(f, x);
    df_dy    = expr_create_deriv(f, y);
    d2f_dxdy = expr_create_2nd_deriv(f, x, y);
    if (!df_dx || !df_dy || !d2f_dxdy) {
        expr_free(d2f_dxdy);
        expr_free(df_dy);
        expr_free(df_dx);
        expr_free(f); expr_free(t0); expr_free(y2); expr_free(xy); expr_free(x2);
        expr_free(y); expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    value = expr_eval(f);
    printf("At x=1, y=2 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(value));
    print_num_line("f", value);
    num_destroy(&value);
    value = expr_eval(df_dx);
    print_num_line("∂f/∂x", value);
    num_destroy(&value);
    value = expr_eval(df_dy);
    print_num_line("∂f/∂y", value);
    num_destroy(&value);
    value = expr_eval(d2f_dxdy);
    print_num_line("∂²f/∂x∂y", value);
    num_destroy(&value);

    p = expr_get_deriv(f, x);
    if (!p) {
        expr_free(d2f_dxdy);
        expr_free(df_dy);
        expr_free(df_dx);
        expr_free(f); expr_free(t0); expr_free(y2); expr_free(xy); expr_free(x2);
        expr_free(y); expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    x0 = num_create_from_string("3");
    expr_set_val(x, x0);
    num_destroy(&x0);
    printf("\nAfter x=3:\n");
    value = expr_eval(df_dx);
    print_num_line("∂f/∂x", value);
    num_destroy(&value);
    value = expr_eval(df_dy);
    print_num_line("∂f/∂y", value);
    num_destroy(&value);

    expr_free(d2f_dxdy);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f); expr_free(t0); expr_free(y2); expr_free(xy); expr_free(x2);
    expr_free(y); expr_free(x);
    num_set_default_prec_bits(old_prec_bits);
    return 0;
}

static int run_readme_eval_derivatives_example(void)
{
    const size_t old_prec_bits = num_get_default_prec_bits();
    number_t x0;
    number_t y0;
    expr_t *x;
    expr_t *y;
    expr_t *xy;
    expr_t *sin_xy;
    expr_t *exp_xy;
    expr_t *log_y;
    expr_t *x_log_y;
    expr_t *f;
    const expr_t *vars[2];
    number_t value;
    number_t grads[2];

    if (num_set_default_prec_bits(384u) != 0)
        return 1;

    x0 = num_create_from_string("1");
    y0 = num_create_from_string("2");
    x = expr_new_named_var(x0, "x");
    y = expr_new_named_var(y0, "y");
    num_destroy(&y0);
    num_destroy(&x0);
    xy = expr_mul(x, y);
    sin_xy = expr_sin(xy);
    exp_xy = expr_exp(sin_xy);
    log_y = expr_log(y);
    x_log_y = expr_mul(x, log_y);
    f = expr_add(exp_xy, x_log_y);

    if (!x || !y || !xy || !sin_xy || !exp_xy || !log_y || !x_log_y || !f) {
        expr_free(f);
        expr_free(x_log_y);
        expr_free(log_y);
        expr_free(exp_xy);
        expr_free(sin_xy);
        expr_free(xy);
        expr_free(y);
        expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    vars[0] = x;
    vars[1] = y;
    if (expr_eval_derivatives(f, 2u, vars, &value, grads) != 0) {
        expr_free(f);
        expr_free(x_log_y);
        expr_free(log_y);
        expr_free(exp_xy);
        expr_free(sin_xy);
        expr_free(xy);
        expr_free(y);
        expr_free(x);
        num_set_default_prec_bits(old_prec_bits);
        return 1;
    }

    printf("Evaluating derivatives at x=1, y=2 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(value));
    print_num_line("f", value);
    print_num_line("∂f/∂x", grads[0]);
    print_num_line("∂f/∂y", grads[1]);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(f);
    expr_free(x_log_y);
    expr_free(log_y);
    expr_free(exp_xy);
    expr_free(sin_xy);
    expr_free(xy);
    expr_free(y);
    expr_free(x);
    num_set_default_prec_bits(old_prec_bits);
    return 0;
}

static int run_README_md_example(void)
{
    if (run_readme_example() != 0)
        return 1;
    putchar('\n');
    if (run_readme_from_string_example() != 0)
        return 1;
    putchar('\n');
    if (run_readme_partial_example() != 0)
        return 1;
    putchar('\n');
    if (run_readme_eval_derivatives_example() != 0)
        return 1;
    return 0;
}

static void test_expr_post_summary(void)
{
    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    if (run_README_md_example() != 0)
        fprintf(stderr, C_BOLD C_RED
                "README examples failed after summary output.\n"
                C_RESET);
    else
        printf(C_YELLOW "\nREADME examples complete.\n" C_RESET);
}

/* ------------------------------------------------------------------------- */
/* Arithmetic tests                                                          */
/* ------------------------------------------------------------------------- */

int tests_main(void)
{
    /* ---------------- Arithmetic ---------------- */
    TEST_SECTION("Arithmetic");
    TEST_RUN_CASE(test_arithmetic, NULL);

    /* ---------------- _d variants ---------------- */
    TEST_SECTION("_d variants");
    TEST_RUN_CASE(test_d_variants, NULL);

    /* ---------------- Mathematical functions ----- */
    TEST_SECTION("Mathematical functions");
    TEST_RUN_CASE(test_maths_functions, NULL);

    /* ---------------- First derivatives ---------- */
    TEST_SECTION("First derivatives");
    TEST_RUN_CASE(test_first_derivatives, NULL);

    /* ---------------- Second derivatives --------- */
    TEST_SECTION("Second derivatives");
    TEST_RUN_CASE(test_second_derivatives, NULL);

    TEST_SECTION("expr_t to_string Tests");
    TEST_RUN_CASE(test_expr_t_to_string, NULL);

    TEST_SECTION("expr_t from_string Tests");
    TEST_RUN_CASE(test_expr_t_from_string, NULL);

    TEST_SECTION("Goal seek");
    TEST_RUN_CASE(test_expr_t_goal_seek, NULL);

    TEST_SECTION("Partial derivatives");
    TEST_RUN_CASE(test_partial_derivatives, NULL);

    TEST_SECTION("expr_pattern helpers");
    TEST_RUN_CASE(test_expr_pattern_helpers, NULL);

    TEST_SECTION("Symbolic integration");
    TEST_RUN_CASE(test_symbolic_integration, NULL);

    TEST_SECTION("Runtime regressions");
    TEST_RUN_CASE(test_runtime_regressions, NULL);

    TEST_SECTION("Reverse mode");
    TEST_RUN_CASE(test_reverse_mode, NULL);

    return TESTS_EXIT_CODE();
}
