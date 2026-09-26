#include "test_expr.h"
#include "matrix.h"
#include "qcomplex.h"

static void test_modified_bessel_k_numeric_layers(void)
{
    NUM_SCOPE(scope);
    const double expected = 0.42102443824070833334;
    ASSERT_TRUE(fabs(qf_to_double(qf_bessel_k(QF_ZERO, QF_ONE))-expected) < 1e-15);
    qcomplex_t complex_value = qc_bessel_k(QC_ZERO, QC_ONE);
    ASSERT_TRUE(fabs(qf_to_double(qc_real(complex_value))-expected) < 1e-15);
    ASSERT_TRUE(qf_eq(qc_imag(complex_value), QF_ZERO));
    ASSERT_TRUE(fabs(num_to_double(num_bessel_k(NUM_ZERO, NUM_ONE))-expected) < 1e-15);
    ASSERT_TRUE(qf_isnan(qf_bessel_k(QF_ZERO, QF_ZERO)));
    ASSERT_TRUE(num_is_nan(num_bessel_k(NUM_ZERO, NUM_ZERO)));
    number_t diagonal[] = {NUM_ONE, NUM_TWO};
    matrix_t *a = mat_create_diagonal(2u, diagonal), *result = mat_bessel_k(a, &NUM_ZERO);
    ASSERT_NOT_NULL(result);
    if (result) {
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(result, 0, 0))-expected) < 1e-15);
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(result, 1, 1))-0.11389387274953343565) < 1e-15);
        ASSERT_TRUE(num_is_zero(mat_get_num(result, 0, 1)));
    }
    mat_free(result);
    mat_free(a);
    a = mat_from_string("(3/2, 1/2; 1/2, 3/2)");
    result = mat_bessel_k(a, &NUM_ZERO);
    ASSERT_NOT_NULL(result);
    if (result) {
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(result, 0, 0))-(expected+0.11389387274953343565)/2) < 1e-15);
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(result, 0, 1))-(0.11389387274953343565-expected)/2) < 1e-15);
    }
    mat_free(result);
    mat_free(a);
}

static void test_signal_numeric_layers(void)
{
    NUM_SCOPE(scope);
    const double points[] = {-2, -1, -0.5, 0, 0.5, 1, 2};
    const double steps[] = {0, 0, 0, 0.5, 1, 1, 1};
    const double rectangles[] = {0, 0, 0.5, 1, 0.5, 0, 0};
    const double triangles[] = {0, 0, 0.5, 1, 0.5, 0, 0};
    const double circles[] = {0, 0.5, 1, 1, 1, 0.5, 0};
    for (size_t n = 0u; n < sizeof(points) / sizeof(points[0]); ++n) {
        qfloat_t x = qf_from_double(points[n]);
        qcomplex_t z = qc_make(x, QF_ZERO);
        number_t value = num_create_from_double(points[n]);
        ASSERT_TRUE(qf_to_double(qf_step(x)) == steps[n]);
        ASSERT_TRUE(qf_to_double(qf_rect(x)) == rectangles[n]);
        ASSERT_TRUE(qf_to_double(qf_tri(x)) == triangles[n]);
        ASSERT_TRUE(qf_to_double(qf_circ(x)) == circles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_step(z))) == steps[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_rect(z))) == rectangles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_tri(z))) == triangles[n]);
        ASSERT_TRUE(qf_to_double(qc_real(qc_circ(z))) == circles[n]);
        ASSERT_TRUE(num_to_double(num_step(value)) == steps[n]);
        ASSERT_TRUE(num_to_double(num_rect(value)) == rectangles[n]);
        ASSERT_TRUE(num_to_double(num_tri(value)) == triangles[n]);
        ASSERT_TRUE(num_to_double(num_circ(value)) == circles[n]);
    }
    ASSERT_TRUE(qf_eq(qf_sinc(QF_ZERO), QF_ONE));
    ASSERT_TRUE(qc_eq(qc_sinc(qc_make(QF_ZERO, QF_ZERO)), qc_make(QF_ONE, QF_ZERO)));
    ASSERT_TRUE(num_eq(num_sinc(NUM_ZERO), NUM_ONE));
    ASSERT_TRUE(num_is_nan(num_step(NUM_I)));
    ASSERT_TRUE(qc_isnan(qc_rect(qc_make(QF_ONE, QF_ONE))));
    ASSERT_TRUE(qf_isnan(qf_tri(QF_NAN)));
    ASSERT_TRUE(num_is_zero(num_tri(NUM_INF)));
    ASSERT_TRUE(num_is_zero(num_rect(NUM_INF)));
}

static void test_signal_spectral_matrices(void)
{
    NUM_SCOPE(scope);
    number_t diagonal[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *matrix = mat_create_diagonal(3u, diagonal);
    matrix_t *step = mat_step(matrix);
    matrix_t *rect = mat_rect(matrix);
    matrix_t *tri = mat_tri(matrix);
    matrix_t *circ = mat_circ(matrix);
    matrix_t *sinc = mat_sinc(matrix);
    ASSERT_TRUE(step && rect && tri && circ && sinc);
    if (step && rect && tri && circ && sinc) {
        ASSERT_TRUE(num_eq(mat_get_num(step, 0, 0), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(step, 1, 1), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(step, 2, 2), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(rect, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(tri, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(circ, 0, 0), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(sinc, 1, 1), NUM_ONE));
        ASSERT_TRUE(num_eq(mat_get_num(step, 0, 1), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(sinc, 0, 1), NUM_ZERO));
    }
    mat_free(sinc);
    mat_free(circ);
    mat_free(tri);
    mat_free(rect);
    mat_free(step);
    mat_free(matrix);
    number_t nilpotent[] = {NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ONE,
                           NUM_ZERO, NUM_ZERO, NUM_ZERO};
    matrix_t *jordan = mat_create(3, 3, nilpotent);
    matrix_t *entire = mat_sinc(jordan);
    ASSERT_TRUE(entire != NULL);
    if (entire) {
        ASSERT_TRUE(num_eq(mat_get_num(entire, 0, 0), NUM_ONE));
        ASSERT_TRUE(num_is_zero(mat_get_num(entire, 0, 1)));
        ASSERT_TRUE(fabs(num_to_double(mat_get_num(entire, 0, 2)) + M_PI*M_PI/6.0) < 1e-12);
    }
    mat_free(entire);
    mat_free(jordan);
}

static void test_signal_calculus_and_finite_sums(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ZERO, "x");
    expr_t *f = expr_sinc(x);
    expr_t *derivative = expr_create_deriv(f, x);
    expr_t *second = expr_create_2nd_deriv(f, x, x);
    ASSERT_TRUE(derivative && second);
    if (derivative && second) {
        ASSERT_TRUE(num_is_zero(expr_eval(derivative)));
        ASSERT_TRUE(fabs(num_to_double(expr_eval(second)) + M_PI * M_PI / 3.0) < 1e-12);
    }
    expr_t *primitive = expr_integrate(f, x);
    ASSERT_TRUE(primitive != NULL);
    expr_t *back = primitive ? expr_create_deriv(primitive, x) : NULL;
    ASSERT_TRUE(back != NULL);
    if (back)
        ASSERT_TRUE(fabs(num_to_double(expr_eval(back)) - 1.0) < 1e-12);
    expr_t *window = expr_rect(x);
    expr_t *lower = expr_new_const(NUM_NEG_ONE);
    expr_t *upper = expr_new_const(NUM_ONE);
    expr_t *sum = expr_new_finite_summation_range(window, x, lower, upper);
    ASSERT_TRUE(sum && num_eq(expr_eval(sum), NUM_ONE));
    expr_free(sum);
    expr_free(upper);
    expr_free(lower);
    expr_free(window);
    expr_free(back);
    expr_free(primitive);
    expr_free(second);
    expr_free(derivative);
    expr_free(f);
    expr_free(x);
}

static void test_finite_part_distribution(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *absolute = expr_abs(x);
    expr_t *reciprocal = expr_div(one, absolute);
    expr_t *distribution = expr_finite_part(reciprocal);
    expr_free(reciprocal);
    expr_free(absolute);
    ASSERT_TRUE(distribution != NULL);
    if (distribution) {
        ASSERT_TRUE(num_eq(expr_eval(distribution), NUM_ONE));
        const style_t styles[] = {style_EXPRESSION, style_FUNCTION, style_LATEX};
        for (size_t n = 0u; n < sizeof(styles) / sizeof(styles[0]); ++n) {
            char *text = expr_to_string(distribution, styles[n]);
            ASSERT_TRUE(text && strstr(text, "finite part"));
            free(text);
        }
        expr_t *derivative = expr_create_deriv(distribution, x);
        expr_t *primitive = expr_integrate(distribution, x);
        expr_t *formal_primitive = expr_integral(distribution, x);
        expr_t *sum = expr_new_finite_summation_range(distribution, x, one, one);
        ASSERT_TRUE(derivative && num_eq(expr_eval(derivative), NUM_NEG_ONE));
        ASSERT_TRUE(primitive == NULL); /* No ordinary primitive is claimed for a distribution. */
        ASSERT_TRUE(formal_primitive != NULL);
        ASSERT_TRUE(sum && num_eq(expr_eval(sum), NUM_ONE));
        expr_free(sum);
        expr_free(primitive);
        expr_free(formal_primitive);
        expr_free(derivative);
    }
    ASSERT_TRUE(expr_finite_part(NULL) == NULL);
    expr_free(distribution);
    expr_free(one);
    expr_free(x);
}

static void test_distribution_regular_rebinding(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ONE, "x"), *one = expr_new_const(NUM_ONE);
    expr_t *reciprocal = expr_div(one, x), *delta = expr_delta(x);
    expr_t *principal = expr_principal_value(reciprocal), *finite = expr_finite_part(reciprocal);
    expr_t *delta_derivative = expr_create_deriv(delta, x), *finite_derivative = expr_create_deriv(finite, x);
    ASSERT_NOT_NULL(delta_derivative);
    ASSERT_NOT_NULL(finite_derivative);
    const number_t points[] = {NUM_ONE, NUM_ZERO, NUM_NEG_ONE, NUM_TWO, NUM_ZERO};
    for (size_t index = 0u; index < sizeof(points) / sizeof(points[0]); ++index) {
        expr_set_val(x, points[index]);
        if (num_is_zero(points[index])) {
            ASSERT_TRUE(num_is_nan(expr_eval(delta)));
            ASSERT_TRUE(num_is_nan(expr_eval(principal)));
            ASSERT_TRUE(num_is_nan(expr_eval(finite)));
            ASSERT_TRUE(num_is_nan(expr_eval(delta_derivative)));
            ASSERT_TRUE(num_is_nan(expr_eval(finite_derivative)));
        } else {
            number_t expected = num_div(NUM_ONE, points[index]);
            ASSERT_TRUE(num_is_zero(expr_eval(delta)));
            ASSERT_TRUE(num_eq(expr_eval(principal), expected));
            ASSERT_TRUE(num_eq(expr_eval(finite), expected));
            ASSERT_TRUE(num_is_zero(expr_eval(delta_derivative)));
            ASSERT_TRUE(num_eq(expr_eval(finite_derivative), num_neg(num_mul(expected, expected))));
        }
        char *body = expr_to_function_body(delta);
        ASSERT_TRUE(body && strstr(body, "delta(x)"));
        free(body);
    }
    expr_free(finite_derivative);
    expr_free(delta_derivative);
    expr_free(finite);
    expr_free(principal);
    expr_free(delta);
    expr_free(reciprocal);
    expr_free(one);
    expr_free(x);
}

static void test_hyperbolic_fourier_domains_and_beta_symbol(void)
{
    NUM_SCOPE(scope);
    const char *constants[] = {"@F{sinh(t)^0}", "{@F{sinh(a*t+b)^n} | ω=?; n=0}",
                               "{@F{sinh(a*t+b)^n} | ω=?; a=0; b=1; n=-1/2}"};
    for (size_t index = 0u; index < sizeof(constants) / sizeof(constants[0]); ++index) {
        expr_t *parsed = expr_from_string(constants[index], NULL);
        expr_t *simplified = parsed ? expr_simplify(parsed) : NULL;
        char *text = simplified ? expr_to_string(simplified, style_EXPRESSION) : NULL;
        ASSERT_TRUE(text && strstr(text, "δ("));
        free(text);
        expr_free(simplified);
        expr_free(parsed);
    }
    const char *aliases[] = {"beta(x,y)", "B(x,y)", "Β(x,y)"};
    for (size_t index = 0u; index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        expr_t *parsed = expr_from_string(aliases[index], NULL);
        char *text = parsed ? expr_to_string(parsed, style_LATEX) : NULL;
        ASSERT_TRUE(text && strstr(text, "\\mathrm{B}"));
        free(text);
        expr_free(parsed);
    }
    expr_t *power = expr_from_string("{@F{sinh(t)^(-1/2)} | ω=0}", NULL);
    ASSERT_TRUE(power != NULL);
    if (power) {
        number_t value = expr_eval(power);
        number_t real = num_real_part(value), imaginary = num_imag_part(value);
        double expected = tgamma(0.25)*tgamma(0.5)/(sqrt(2.0)*tgamma(0.75));
        ASSERT_TRUE(fabs(num_to_double(real)-expected) < 1e-12);
        ASSERT_TRUE(fabs(num_to_double(imaginary)+expected) < 1e-12);
    }
    expr_free(power);
}

static void test_inverse_hyperbolic_beta_spectra(void)
{
    const char *source = "@Finv{2^(-(n+1))*exp(i*b*ω/a)/abs(a)*"
                         "(exp(i*@pi*n)*B(-(n+i*ω/a)/2,n+1)+B((i*ω/a-n)/2,n+1))}";
    expr_t *parsed = expr_from_string(source, NULL);
    expr_t *simplified = parsed ? expr_simplify(parsed) : NULL;
    char *text = simplified ? expr_to_string(simplified, style_EXPRESSION) : NULL;
    ASSERT_TRUE(text && strstr(text, "sinh(at + b)^n"));
    ASSERT_TRUE(text && !strstr(text, "ℱ"));
    ASSERT_TRUE(text && strstr(text, "Re(n + 1) > 0") && strstr(text, "Re(-n) > 0"));
    expr_t *reparsed = text ? expr_from_string(text, NULL) : NULL;
    ASSERT_TRUE(reparsed != NULL);
    expr_free(reparsed);
    free(text);
    expr_free(simplified);
    expr_free(parsed);
}

static void test_distribution_function_qualifier_round_trips(void)
{
    const char *sources[] = {
        "PV(1/x)", "finite_part(1/abs(x))", "PV(1/x)-1/x", "PV(1/x)+PV(1/x)",
        "PV(1/x)+finite_part(1/x)", "PV(finite_part(1/x))", "finite_part(PV(1/x))",
        "PV(PV(1/x))", "PV(1/x)^2", "1/PV(1/x)", "sin(PV(1/x))", "PV(1)",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        expr_t *original = expr_from_string(sources[i], NULL);
        char *function = original ? expr_to_string(original, style_FUNCTION) : NULL;
        char *body = original ? expr_to_function_body(original) : NULL;
        ASSERT_NOT_NULL(body);
        ASSERT_TRUE(function && !strstr(function, "principal_value(") && !strstr(function, "finite_part("));
        ASSERT_TRUE(body && !strstr(body, "principal_value(") && !strstr(body, "finite_part("));
        expr_t *restored = body ? expr_from_function_body(body, NULL) : NULL;
        ASSERT_TRUE(restored != NULL);
        expr_t *expected_tree = original ? expr_beautify(original) : NULL;
        expr_t *actual_tree = restored ? expr_beautify(restored) : NULL;
        char *expected = expected_tree ? expr_to_string(expected_tree, style_UNBOUND) : NULL;
        char *actual = actual_tree ? expr_to_string(actual_tree, style_UNBOUND) : NULL;
        ASSERT_NOT_NULL(expected);
        ASSERT_NOT_NULL(actual);
        TEST_ASSERT_STR_EQ(actual, expected);
        expr_free(actual_tree);
        expr_free(expected_tree);
        free(actual);
        free(expected);
        expr_free(restored);
        free(body);
        free(function);
        expr_free(original);
    }
}

static void test_mathematical_nonzero_domains(void)
{
    const char *sources[] = {
        "tan(x) where (x ∈ ℝ; cos(x) ≠ 0)", "cot(x) where (x ∈ ℝ; sin(x) != 0)",
        "1/x where (x!=0)", "1/(x-1) where (x!=1)", "1/x where (Re(abs(x))>0)",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        expr_t *original = expr_from_string(sources[i], NULL);
        ASSERT_NOT_NULL(original);
        char *body = original ? expr_to_function_body(original) : NULL;
        char *function = original ? expr_to_string(original, style_FUNCTION) : NULL;
        ASSERT_TRUE(body && strstr(body, " != 0"));
        ASSERT_TRUE(function && strstr(function, "if (") && strstr(function, " != 0"));
        ASSERT_TRUE(function && !strstr(function, "principal value"));
        expr_t *restored = body ? expr_from_function_body(body, NULL) : NULL;
        ASSERT_NOT_NULL(restored);
        char *expected = original ? expr_to_string(original, style_UNBOUND) : NULL;
        char *actual = restored ? expr_to_string(restored, style_UNBOUND) : NULL;
        TEST_ASSERT_STR_EQ(actual, expected);
        free(actual);
        free(expected);
        free(function);
        free(body);
        expr_free(restored);
        expr_free(original);
    }
}

static void test_odd_hyperbolic_fourier_copy_and_rendering(void)
{
    const char *sources[] = {
        "@F{tanh(x)}", "-i*@pi*x", "-2*i*@pi*x", "(1-i)*x", "x*(-i)", "(-i*x)^2",
        "InverseFourier({-i*@pi*csch(@pi*k/2) | ; k=?; k ∈ ℝ; k ≠ 0},k,x)",
        "@F{coth(x)}", "@Finv{-i*@pi*coth(@pi*k/2)}",
        "@F{atan(x)}", "@Finv{-i*@pi*exp(-abs(k))/k}",
        "@F{asinh(x)}", "@Finv{-2*i*K0(abs(k))/k}", "K_n(x)",
        "@F{atanh(x)}", "@F{asin(x)}", "@F{acos(x)}", "@F{acosh(x)}",
        "@Finv{i*@pi^2*delta(k)+2*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)}",
        "@F{gamma(1+i*x)}", "@Finv{2*@pi*exp(k-exp(k))}",
        "@Finv{2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*Dk(step(k)*ln(abs(k))))}",
        "@pi^2*i", "@pi^2*cos(x)", "2*x", "1.25*x",
        "@F{gamma(x)}", "@Finv{2*@pi*exp(k*Re(x)-exp(k))}",
        "Fourier(gamma(x),Im(x),k)",
    };
    for (size_t index = 0u; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        expr_t *parsed = expr_from_string(sources[index], NULL);
        expr_t *result = parsed ? expr_beautify(parsed) : NULL;
        char *text = result ? expr_to_string(result, style_EXPRESSION) : NULL;
        char *TeX = result ? expr_to_string(result, style_LATEX) : NULL;
        char *body = result ? expr_to_function_body(result) : NULL;
        ASSERT_NOT_NULL(text);
        ASSERT_NOT_NULL(TeX);
        expr_t *copy = text ? expr_from_string(text, NULL) : NULL;
        ASSERT_NOT_NULL(copy);
        expr_t *function_copy = NULL;
        if (index == 0u || index >= 6u) {
            function_copy = body ? expr_from_function_body(body, NULL) : NULL;
            if (!function_copy)
                fprintf(stderr, "Function round-trip failed for %s:\n%s\n", sources[index], body ? body : "(null)");
            ASSERT_NOT_NULL(function_copy);
        }
        if (index < 3u) {
            ASSERT_TRUE(text && !strstr(text, "(-i)") && !strstr(text, "(-2i)"));
            ASSERT_TRUE(TeX && !strstr(TeX, "\\left(-i\\right)") && !strstr(TeX, "\\left(-2i\\right)"));
        }
        if (index == 0u)
            ASSERT_TRUE(body && strstr(body, "k != 0"));
        if (index == 6u)
            ASSERT_TRUE(text && strstr(text, "tanh(x)") && !strstr(text, "k"));
        if (index == 7u)
            ASSERT_TRUE(body && strstr(body, "coth(") && strstr(body, "k != 0") && !strstr(body, "fourier("));
        if (index == 8u)
            ASSERT_TRUE(body && strstr(body, "coth(x)") && strstr(body, "x != 0") && !strstr(body, "fourier("));
        if (index == 9u)
            ASSERT_TRUE(body && strstr(body, "exp(") && strstr(body, "k != 0") && !strstr(body, "fourier("));
        if (index == 10u)
            ASSERT_TRUE(body && strstr(body, "atan(x)") && !strstr(body, "x != 0") && !strstr(body, "fourier("));
        if (index >= 11u) {
            ASSERT_TRUE(body && !strstr(body, "fourier("));
            ASSERT_TRUE(text && !strstr(text, ": principal value") && !strstr(text, ": finite part"));
        }
        expr_free(function_copy);
        expr_free(copy);
        free(body);
        free(TeX);
        free(text);
        expr_free(result);
        expr_free(parsed);
    }
}

static void test_analytic_evaluation_functional(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    expr_t *i = expr_new_const(NUM_I);
    expr_t *argument = expr_add(x, i);
    expr_t *functional = expr_analytic_delta(argument);
    expr_t *derivative = expr_create_deriv(functional, x);
    expr_t *primitive = expr_integrate(functional, x);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *sum = expr_new_finite_summation_range(functional, x, one, one);
    ASSERT_TRUE(functional && num_is_nan(expr_eval(functional)));
    ASSERT_TRUE(derivative && num_is_nan(expr_eval(derivative)));
    ASSERT_TRUE(sum && num_is_nan(expr_eval(sum)));
    ASSERT_TRUE(primitive == NULL); /* No ordinary step-function primitive for complex evaluation. */
    char *body = expr_to_string(functional, style_FUNCTION);
    char *TeX = expr_to_string(functional, style_LATEX);
    ASSERT_TRUE(body && strstr(body, "analytic_delta("));
    ASSERT_TRUE(TeX && strstr(TeX, "\\delta(") && !strstr(TeX, "\\delta_"));
    free(TeX);
    free(body);
    expr_free(sum);
    expr_free(one);
    expr_free(primitive);
    expr_free(derivative);
    expr_free(functional);
    expr_free(argument);
    expr_free(i);
    expr_free(x);
}

static void test_imaginary_additive_parentheses(void)
{
    NUM_SCOPE(scope);
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    const number_t coefficients[] = {NUM_I, NUM_NEG_I, num_mul(NUM_TWO, NUM_I), num_mul(NUM_TWO, NUM_NEG_I)};
    const char *expected[] = {"x + i", "x - i", "x + 2i", "x - 2i"};
    for (size_t index = 0u; index < sizeof(coefficients) / sizeof(coefficients[0]); ++index) {
        expr_t *coefficient = expr_new_const(coefficients[index]);
        expr_t *sum = expr_add(x, coefficient);
        expr_t *difference = expr_sub(x, coefficient);
        const expr_t *expressions[] = {sum, difference};
        for (size_t operation = 0u; operation < 2u; ++operation) {
            const expr_t *expression = expressions[operation];
            char *text = expr_to_string(expression, style_UNBOUND);
            char *TeX = expr_to_string(expression, style_LATEX);
            char *body = expr_to_function_body(expression);
            TEST_ASSERT_STR_EQ(text, expected[index ^ operation]);
            ASSERT_TRUE(TeX && !strstr(TeX, "\\left("));
            expr_t *copy = body ? expr_from_function_body(body, NULL) : NULL;
            ASSERT_NOT_NULL(copy);
            char *copied = copy ? expr_to_string(copy, style_UNBOUND) : NULL;
            TEST_ASSERT_STR_EQ(copied, text);
            free(copied);
            expr_free(copy);
            free(body);
            free(TeX);
            free(text);
        }
        expr_free(difference);
        expr_free(sum);
        expr_free(coefficient);
    }
    expr_free(x);
}

void test_fourier_and_signal_functions(void)
{
    TEST_RUN_SUBTEST(test_imaginary_additive_parentheses, NULL);
    TEST_RUN_SUBTEST(test_analytic_evaluation_functional, NULL);
    TEST_RUN_SUBTEST(test_modified_bessel_k_numeric_layers, NULL);
    TEST_RUN_SUBTEST(test_signal_numeric_layers, NULL);
    TEST_RUN_SUBTEST(test_signal_spectral_matrices, NULL);
    TEST_RUN_SUBTEST(test_signal_calculus_and_finite_sums, NULL);
    TEST_RUN_SUBTEST(test_finite_part_distribution, NULL);
    TEST_RUN_SUBTEST(test_distribution_regular_rebinding, NULL);
    TEST_RUN_SUBTEST(test_distribution_function_qualifier_round_trips, NULL);
    TEST_RUN_SUBTEST(test_mathematical_nonzero_domains, NULL);
    TEST_RUN_SUBTEST(test_odd_hyperbolic_fourier_copy_and_rendering, NULL);
    TEST_RUN_SUBTEST(test_hyperbolic_fourier_domains_and_beta_symbol, NULL);
    TEST_RUN_SUBTEST(test_inverse_hyperbolic_beta_spectra, NULL);
}

/* README examples from the qfloat, qcomplex, number and matrix module guides. */
void example_signal_readme_examples(void)
{
    NUM_SCOPE(scope);
    double half = qf_to_double(qf_step(QF_ZERO));
    ASSERT_TRUE(half == 0.5);
    printf("step(0) = %.1f\n", half);

    qcomplex_t value = qc_sinc(qc_make(QF_ZERO, QF_ZERO));
    ASSERT_TRUE(qc_eq(value, qc_make(QF_ONE, QF_ZERO)));
    printf("sinc(0) = %.0f + %.0fi\n", qf_to_double(qc_real(value)), qf_to_double(qc_imag(value)));

    number_t edge = num_rect(NUM_HALF);
    ASSERT_TRUE(num_eq(edge, NUM_HALF));
    printf("rect(1/2) = %.1f\n", num_to_double(edge));

    number_t diagonal[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *a = mat_create_diagonal(3u, diagonal);
    matrix_t *b = mat_step(a);
    ASSERT_TRUE(b != NULL);
    if (b) {
        ASSERT_TRUE(num_eq(mat_get_num(b, 0, 0), NUM_ZERO));
        ASSERT_TRUE(num_eq(mat_get_num(b, 1, 1), NUM_HALF));
        ASSERT_TRUE(num_eq(mat_get_num(b, 2, 2), NUM_ONE));
        printf("step(diag(-1, 0, 1)) = diag(%.1f, %.1f, %.1f)\n",
               num_to_double(mat_get_num(b, 0, 0)), num_to_double(mat_get_num(b, 1, 1)),
               num_to_double(mat_get_num(b, 2, 2)));
    }
    mat_free(b);
    mat_free(a);
}
