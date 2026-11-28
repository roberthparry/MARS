/**
 * @file test_expr_lambert.c
 * @brief Database-backed Lambert-sum numerical, calculus, notation and matrix regressions.
 *
 * Finite rational/logarithmic sums and the independently interchanged infinite double series supply
 * references. Tests check selected precision, all numerical entry points, matrix functional calculus,
 * repeated chain rules, affine/exponential primitives and self-defining native TeX. README output runs last.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#include "computation-t.h"
#include "expression-t.h"
#include "json-t.h"
#include "matrix-t.h"
#include "number-t.h"
#include "qcomplex-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expression/expr_internal.h"
#include "test_database.h"
#include "test_expr.h"

static number_t lambert_text(const string_t *text)
{
    expr_t *expression = text ? expr_from_text(text, NULL) : NULL;
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    expr_free(expression);
    return value;
}

static bool lambert_close(number_t actual, number_t expected, size_t bits)
{
    NUM_SCOPE(scope);
    number_t difference = num_sub(actual, expected), error = num_abs(difference);
    number_t size = num_add(NUM_ONE, num_abs(expected)), tolerance = num_ldexp(size, 24 - (int)bits);
    return num_is_finite(actual) && num_is_finite(expected) && num_le(error, tolerance);
}

/* Interchange the two absolutely convergent sums; the reference uses a different index and rational kernel. */
static number_t lambert_dual(const number_t *a, size_t bits)
{
    NUM_SCOPE(scope);
    number_t base = num_is_real(a[1]) ? num_as_inexact_real_prec(a[1], bits + 96) : num_as_complex_prec(a[1], bits + 96);
    number_t amplitude = num_is_real(a[0]) ? num_as_inexact_real_prec(a[0], bits + 96) : num_as_complex_prec(a[0], bits + 96);
    number_t q_power = num_clone(base), a_power = num_clone(amplitude), sum = num_clone(NUM_ZERO);
    long weight = (long)num_to_double(a[3]), order = (long)num_to_double(a[4]);
    for (size_t m = 1; m <= 2 * bits + 64; ++m) {
        NUM_SCOPE(iteration);
        number_t one_minus = num_sub(NUM_ONE, q_power), inner = num_clone(q_power);
        if (weight == 2) {
            number_t extra = num_add(NUM_ONE, q_power), product = num_mul(inner, extra);
            num_destroy(&inner); inner = product;
        }
        number_t denominator = num_pow_int(one_minus, (int)weight + 1), rational = num_div(inner, denominator);
        number_t m_value = num_create_from_long((long)m), m_weight = num_pow_int(m_value, (int)-order);
        number_t term = num_mul(num_mul(a_power, m_weight), rational), next = num_add(sum, term);
        num_destroy(&sum); sum = num_scope_detach(next);
        number_t next_q = num_mul(q_power, base), next_a = num_mul(a_power, amplitude);
        num_destroy(&q_power); num_destroy(&a_power);
        q_power = num_scope_detach(next_q); a_power = num_scope_detach(next_a);
    }
    num_destroy(&q_power); num_destroy(&a_power);
    return num_scope_detach(sum);
}

/* A distinct Stirling-number polynomial and independently powered coordinates check the rational kernels. */
static number_t lambert_rational_reference(const number_t *a, size_t bits)
{
    NUM_SCOPE(scope);
    num_set_default_prec_bits(bits + 192);
    unsigned degree = (unsigned)-num_to_double(a[4]);
    size_t count = (size_t)num_to_double(a[5]);
    number_t stirling[34];
    for (unsigned j = 0; j <= degree + 1; ++j)
        stirling[j] = num_clone(j == 0 ? NUM_ONE : NUM_ZERO);
    for (unsigned n = 1; n <= degree + 1; ++n) {
        for (unsigned j = n; j > 0; --j) {
            number_t product = num_mul_long(stirling[j], j), next = num_add(product, stirling[j - 1]);
            num_destroy(&product); num_destroy(&stirling[j]); stirling[j] = next;
        }
        num_destroy(&stirling[0]); stirling[0] = num_clone(NUM_ZERO);
    }
    number_t factorial = num_clone(NUM_ONE);
    for (unsigned j = 1; j <= degree + 1; ++j) {
        number_t next = num_mul(stirling[j], factorial);
        num_destroy(&stirling[j]); stirling[j] = next;
        next = num_mul_long(factorial, j);
        num_destroy(&factorial); factorial = next;
    }
    number_t sum = num_clone(NUM_ZERO);
    for (size_t n = 0; n < count; ++n) {
        NUM_SCOPE(iteration);
        number_t coordinate = num_add_long(a[2], (long)n), w = num_mul(a[0], num_pow(a[1], coordinate));
        number_t u = num_div(w, num_sub(NUM_ONE, w)), polynomial = num_clone(stirling[degree + 1]);
        for (unsigned j = degree; j > 0; --j) {
            number_t next = num_add(num_mul(polynomial, u), stirling[j]);
            num_destroy(&polynomial); polynomial = next;
        }
        number_t term = num_mul(num_pow_int(coordinate, (int)num_to_double(a[3])), num_mul(u, polynomial));
        number_t next = num_add(sum, term);
        num_destroy(&sum); sum = num_scope_detach(next);
    }
    num_set_default_prec_bits(bits);
    return num_scope_detach(sum);
}

static qcomplex_t lambert_fixed(number_t value)
{
    NUM_SCOPE(scope);
    return qc_make(num_to_qfloat(num_real_part(value)), num_to_qfloat(num_imag_part(value)));
}

static void lambert_value_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits(), bits = (size_t)row->precision;
    num_set_default_prec_bits(bits);
    NUM_SCOPE(scope);
    json_t *input = json_from_text(row->input);
    number_t a[6];
    for (unsigned j = 0; j < 6; ++j) a[j] = lambert_text(json_string_value(json_array_get(input, j)));
    number_t expected = strcmp(string_c_str(row->expected), "dual") == 0 ? lambert_dual(a, bits) :
        strcmp(string_c_str(row->expected), "rational") == 0 ? lambert_rational_reference(a, bits) : lambert_text(row->expected);
    number_t actual = num_lambert_S(a[0], a[1], a[2], a[3], a[4], a[5]);
    TEST_ASSERT_TRUE(num_get_default_prec_bits() == bits, "Lambert evaluation preserves the caller's precision");
    if (!lambert_close(actual, expected, bits))
        fmt_print("Lambert value {:s}: actual {:.17g}, expected {:.17g}\n",
            FMT(row->name),
            FMT(num_to_double(actual)),
            FMT(num_to_double(expected)));
    TEST_ASSERT_TRUE(lambert_close(actual, expected, bits),
                     "Lambert values retain the selected precision against independent sums");
    qcomplex_t fixed[6];
    bool real = true;
    for (unsigned j = 0; j < 6; ++j) { fixed[j] = lambert_fixed(a[j]); real = real && num_is_real(a[j]); }
    qcomplex_t complex_value = qc_lambert_S(fixed[0], fixed[1], fixed[2], fixed[3], fixed[4], fixed[5]);
    number_t complex_number = num_create_from_qcomplex(complex_value);
    TEST_ASSERT_TRUE(lambert_close(complex_number, expected, 106), "native complex Lambert values agree without a number bridge");
    if (real) {
        qfloat_t value = qf_lambert_S(fixed[0].re, fixed[1].re, fixed[2].re, fixed[3].re, fixed[4].re, fixed[5].re);
        number_t real_number = num_create_from_qfloat(value);
        TEST_ASSERT_TRUE(num_is_real(expected) ? lambert_close(real_number, expected, 106) : qf_isnan(value),
                         "native real Lambert values agree or reject a non-real principal branch");
    }
    matrix_t *coordinate = mat_create(1, 1, &a[2]);
    matrix_t *matrix = mat_lambert_S(coordinate, a[0], a[1], a[3], a[4], a[5]);
    number_t matrix_value = matrix ? mat_get_num(matrix, 0, 0) : NUM_NAN;
    TEST_ASSERT_TRUE(lambert_close(matrix_value, expected, bits), "matrix spectral evaluation agrees with the scalar convention");
    mat_free(matrix); mat_free(coordinate);
    json_free(input);
    num_set_default_prec_bits(saved);
}

static void test_lambert_values(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_value", NULL, lambert_value_row);
}

static void lambert_real_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits((size_t)row->precision);
    NUM_SCOPE(scope);
    json_t *expected = json_from_text(row->expected);
    number_t actual = lambert_text(row->input);
    number_t reference = lambert_text(json_string_value(json_array_get(expected, 0)));
    bool real = false;
    bool ok = json_bool_value(json_array_get(expected, 1), &real);
    bool imaginary = false;
    bool check_imaginary = json_bool_value(json_array_get(expected, 2), &imaginary);
    number_t real_part = num_real_part(actual);
    TEST_ASSERT_TRUE(ok && num_is_real(actual) == real && (!check_imaginary || num_is_zero(real_part) == imaginary) &&
                     lambert_close(actual, reference, (size_t)row->precision),
                     "proved unit-circle parity is exact while general complex inputs retain both components");
    json_free(expected);
    num_set_default_prec_bits(saved);
}

static void test_lambert_real(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_real", NULL, lambert_real_row);
}

static void quotient_recognition_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits((size_t)row->precision);
    NUM_SCOPE(scope);
    json_t *expected = json_from_text(row->expected);
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_text(row->input, &bindings);
    const char *name = string_c_str(json_string_value(json_array_get(expected, 0)));
    expr_t *axis = expr_bindings_get(bindings, name);
    bool recognised = false;
    bool ok = source && axis && json_bool_value(json_array_get(expected, 2), &recognised);
    expr_t *closed = expr_finite_quotient_derivative_closed_form(source, NULL);
    ok = ok && (closed != NULL) == recognised;
    if (ok && recognised) {
        expr_t *shared = expr_finite_progression_closed_form(source);
        const expr_t *axes[] = {axis};
        expr_t *sample = expr_finite_progression_sample_form(source, axes, 1);
        char *function = shared ? expr_to_string(shared, style_FUNCTION) : NULL;
        char *TeX = expr_finite_progression_identity_TeX(source);
        ok = shared && expr_is_differentiable(shared) && sample && function && strstr(function, "lambert_S(") &&
             TeX && strstr(TeX, "\\sum") && strstr(TeX, " = ") && strstr(TeX, "\\Lambda") && strstr(TeX, ":=");
        number_t point = lambert_text(json_string_value(json_array_get(expected, 1)));
        for (unsigned j = 0; ok && j < 2; ++j) {
            expr_set_val(axis, point);
            ok = lambert_close(expr_eval(closed), expr_eval(source), (size_t)row->precision) &&
                 lambert_close(expr_eval(sample), expr_eval(source), (size_t)row->precision);
            point = num_mul(point, num_create_from_frac(3, 2));
        }
        free(TeX); free(function);
        expr_free(sample); expr_free(shared);
    }
    if (!ok)
        fmt_print("Expanded quotient recognition {:s}: expected {:s}, got {:s}\n",
            FMT(row->name),
            FMT(recognised ? "a linked Lambert identity" : "no match"),
            FMT(closed ? "a match" : "no match"));
    expr_free(closed); expr_free(source);
    expr_bindings_free(bindings); json_free(expected);
    num_set_default_prec_bits(saved);
    TEST_ASSERT_TRUE(ok, "expanded quotient derivative polynomials share evaluable identities, notation and sampling");
}

static void test_quotient_recognition(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.quotient_recognition", NULL, quotient_recognition_row);
}

static void lambert_calculus_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits(), bits = (size_t)row->precision;
    num_set_default_prec_bits(bits);
    NUM_SCOPE(scope);
    json_t *expected = json_from_text(row->expected);
    expr_bindings_t *bindings = NULL, *reference_bindings = NULL;
    expr_t *source = expr_from_text(row->input, &bindings);
    expr_t *reference = expr_from_text(json_string_value(json_array_get(expected, 1)), &reference_bindings);
    TEST_ASSERT_TRUE(expr_is_differentiable(source), "Lambert calculus rules are available to native front-ends");
    expr_t *x = expr_bindings_get(bindings, "x"), *rx = expr_bindings_get(reference_bindings, "x");
    number_t order_value = NUM_NAN;
    json_number_value(json_array_get(expected, 2), &order_value);
    long order = (long)num_to_double(order_value);
    expr_t *actual = order < 0 ? expr_integrate(source, x) : source;
    if (order >= 0) expr_retain(actual);
    if (order < 0) {
        expr_t *check = actual ? expr_create_deriv(actual, x) : NULL;
        expr_free(actual); actual = check;
    } else {
        for (long j = 0; j < order; ++j) {
            expr_t *next = actual ? expr_create_deriv(actual, x) : NULL;
            expr_free(actual); actual = next;
            /* Differentiate the reference summand explicitly, retaining its finite bounds. */
            expr_t *term = reference ? expr_create_deriv(reference->a, rx) : NULL;
            expr_t *reference_next = term ? expr_new_summation(term, reference->b) : NULL;
            expr_free(term); expr_free(reference); reference = reference_next;
        }
    }
    number_t point = lambert_text(json_string_value(json_array_get(expected, 0)));
    if (x) expr_set_val(x, point);
    if (rx) expr_set_val(rx, point);
    number_t value = actual ? expr_eval(actual) : num_clone(NUM_NAN), want = reference ? expr_eval(reference) : num_clone(NUM_NAN);
    if (!lambert_close(value, want, bits)) {
        char *text = actual ? expr_to_string(actual, style_UNBOUND) : NULL;
        fmt_print("Lambert calculus {:s}: {:.17g} versus {:.17g}\n{:s}\n",
            FMT(row->name),
            FMT(num_to_double(value)),
            FMT(num_to_double(want)),
            FMT(text));
        free(text);
    }
    TEST_ASSERT_TRUE(lambert_close(value, want, bits), "analytic chain rules and primitives agree with direct finite sums");
    char *TeX = actual ? expr_to_TeX_body(actual) : NULL;
    TEST_ASSERT_TRUE(TeX && strstr(TeX, "\\Lambda") && strstr(TeX, ":=") && strstr(TeX, "\\operatorname{Li}"),
                     "Lambert notation always accompanies its MARS definition");
    free(TeX); expr_free(actual); expr_free(reference); expr_free(source);
    expr_bindings_free(reference_bindings); expr_bindings_free(bindings); json_free(expected);
    num_set_default_prec_bits(saved);
}

static void test_lambert_calculus(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_calculus", NULL, lambert_calculus_row);
}

static void lambert_notation_row(const void *data)
{
    const test_database_case_t *row = data;
    NUM_SCOPE(scope);
    expr_t *expression = expr_from_text(row->input, NULL);
    json_t *expected = json_from_text(row->expected);
    number_t limit = NUM_NAN;
    bool ok = expression && expected && json_number_value(json_array_get(expected, 0), &limit);
    char *body = expression ? expr_to_TeX_body(expression) : NULL;
    char *wrapped = ok ? expr_to_TeX_body_wrapped(expression, (size_t)num_to_double(limit)) : NULL;
    const char *line = wrapped ? strstr(wrapped, "\\\\&") : NULL;
    const char *definition = wrapped ? strstr(wrapped, ":=") : NULL;
    ok = ok && body && wrapped && !strcmp(body, wrapped) && line && definition && definition > line &&
         !strstr(definition + 2, ":=");
    for (size_t i = 1; ok && i < json_array_size(expected); ++i) {
        const string_t *fragment = json_string_value(json_array_get(expected, i));
        const char *position = fragment ? strstr(wrapped, string_c_str(fragment)) : NULL;
        ok = position && position < line;
    }
    if (!ok)
        fmt_print("Lambert notation {:s}:\n{:s}\n", FMT(row->name), FMT(wrapped ? wrapped : "(null)"));
    TEST_ASSERT_TRUE(ok, "the complete signed result precedes one separate Lambert definition");
    free(wrapped);
    free(body);
    json_free(expected);
    expr_free(expression);
}

static void test_lambert_notation(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_notation", NULL, lambert_notation_row);
}

static void lambert_integral_limit_row(const void *data)
{
    const test_database_case_t *row = data;
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_text(row->input, &bindings), *x = expr_bindings_get(bindings, "x");
    expr_t *primitive = source && x ? expr_integrate_lambert(source, x) : NULL;
    json_t *expected = json_from_text(row->expected);
    bool supported = false;
    TEST_ASSERT_TRUE(source && x && json_bool_value(expected, &supported) && (primitive != NULL) == supported,
                     "a contiguous primitive must not discard other dependencies");
    json_free(expected);
    expr_free(primitive);
    expr_free(source);
    expr_bindings_free(bindings);
}

static void test_lambert_integral_limits(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_integral_limits", NULL, lambert_integral_limit_row);
}

static void lambert_qpartial_row(const void *data)
{
    const test_database_case_t *row = data;
    NUM_SCOPE(scope);
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits((size_t)row->precision);
    json_t *expected = json_from_text(row->expected);
    expr_bindings_t *bindings = NULL, *reference_bindings = NULL;
    expr_t *source = expr_from_text(row->input, &bindings);
    expr_t *reference = expr_from_text(json_string_value(json_array_get(expected, 1)), &reference_bindings);
    expr_t *x = expr_bindings_get(bindings, "x"), *rx = expr_bindings_get(reference_bindings, "x");
    number_t degree = NUM_NAN;
    json_number_value(json_array_get(expected, 2), &degree);
    expr_t *actual = expr_create_nth_deriv((unsigned)num_to_double(degree), source, x);
    if (rx) {
        expr_t *derivative = expr_create_nth_deriv((unsigned)num_to_double(degree), reference, rx);
        expr_free(reference); reference = derivative;
    }
    number_t point = lambert_text(json_string_value(json_array_get(expected, 0)));
    expr_set_val(x, point);
    if (rx) expr_set_val(rx, point);
    number_t value = expr_eval(actual), want = expr_eval(reference);
    TEST_ASSERT_TRUE(lambert_close(value, want, (size_t)row->precision), "analytic q-digamma partials obey the shift identity");
    expr_free(actual); expr_free(reference); expr_free(source);
    expr_bindings_free(reference_bindings); expr_bindings_free(bindings); json_free(expected);
    num_set_default_prec_bits(saved);
}

static void test_lambert_qpartials(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.lambert_qpartial", NULL, lambert_qpartial_row);
}

static bool lambert_controlled(void *context)
{
    const number_t *a = context;
    number_t value = num_lambert_S(a[0], a[1], a[2], a[3], a[4], a[5]);
    bool finite = num_is_finite(value);
    num_destroy(&value);
    return finite;
}

/* Executable graph fixtures check alias dispatch, reverse accumulation and matrix off-diagonal derivatives. */
static void test_lambert_interfaces(void)
{
    NUM_SCOPE(scope);
    const char *aliases[] = {"lambert_S", "lambertS", "LambertS", "Λ"};
    for (unsigned j = 0; j < sizeof aliases / sizeof *aliases; ++j) {
        string_t *input = string_new();
        string_append_format(input, "%s(-1,1,1,1,-1,4)", aliases[j]);
        number_t value = lambert_text(input);
        TEST_ASSERT_TRUE(lambert_close(value, num_create_from_double(-2.5), 106), "perfect-hash Lambert aliases agree");
        string_free(input);
    }
    expr_t *outer = expr_from_string("sum(k,1,4,lambert_S(-1,1,k,0,-1,2))", NULL);
    TEST_ASSERT_TRUE(num_eq(expr_eval(outer), num_create_from_long(-2)), "outer finite sums retain Lambert index bindings");
    expr_free(outer);
    number_t typed = num_lambert_S(num_create_from_double(0.5), num_create_from_double(0.25),
                                   num_create_from_double(1.25), NUM_ONE, NUM_NEG_ONE, NUM_INF);
    expr_t *typed_reference = expr_from_string("lambert_S(0.5,0.25,1.25,1,-1,inf)", NULL);
    TEST_ASSERT_TRUE(lambert_close(typed, expr_eval(typed_reference), 53),
                     "typed native number arguments evaluate the full infinite sum");
    expr_free(typed_reference);
    number_t branched = num_lambert_S(num_create_from_double(4), num_create_from_double(0.5),
                                      num_create_from_double(1), NUM_ONE, NUM_ONE, NUM_ONE);
    TEST_ASSERT_TRUE(lambert_close(branched, num_mul(num_neg(NUM_I), NUM_PI), 53),
                     "real native inputs retain complex values on a positive-order polylogarithm cut");
    number_t pole = num_lambert_S(NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, num_create_from_long(1000));
    TEST_ASSERT_TRUE(num_is_nan(pole), "a constant-base collapse does not turn a polylogarithm pole into a finite value");
    pole = num_lambert_S(NUM_ONE, NUM_I, NUM_ONE, NUM_ONE, NUM_NEG_ONE, num_create_from_long(65));
    TEST_ASSERT_TRUE(num_is_nan(pole), "a reciprocal recurrence retains exact finite-section poles");
    number_t elements[] = {NUM_ONE, NUM_ONE, NUM_ZERO, NUM_ONE};
    matrix_t *jordan = mat_create(2, 2, elements);
    matrix_t *value = mat_lambert_S(jordan, NUM_NEG_ONE, NUM_ONE, NUM_ONE, NUM_NEG_ONE, num_create_from_long(4));
    TEST_ASSERT_TRUE(value && lambert_close(mat_get_num(value, 0, 0), num_create_from_double(-2.5), 106) &&
                     lambert_close(mat_get_num(value, 0, 1), NUM_NEG_ONE, 106),
                     "matrix Lambert uses analytic off-diagonal functional calculus");
    mat_free(value); mat_free(jordan);
    expr_t *a = expr_new_named_var(num_create_from_string("0.5"), "a");
    expr_t *q = expr_new_named_var(num_create_from_string("0.25"), "q");
    expr_t *z = expr_new_named_var(num_create_from_string("1.25"), "z");
    expr_t *r = expr_new_const(NUM_ONE), *s = expr_new_const(NUM_NEG_ONE), *n = expr_new_const(num_create_from_long(8));
    expr_t *function = expr_lambert_S(a, q, z, r, s, n);
    TEST_ASSERT_TRUE(!expr_is_differentiable(function->a) && !expr_is_differentiable(NULL),
                     "bare argument storage and missing expressions are not differentiable scalar operations");
    const expr_t *variables[] = {a, q, z};
    number_t result = NUM_NAN, gradients[3] = {NUM_NAN, NUM_NAN, NUM_NAN};
    TEST_ASSERT_TRUE(expr_eval_derivatives(function, 3, variables, &result, gradients) == 0,
                     "Lambert reverse accumulation supports all continuous arguments");
    for (unsigned j = 0; j < 3; ++j) {
        expr_t *partial = expr_create_deriv(function, variables[j]);
        TEST_ASSERT_TRUE(lambert_close(gradients[j], expr_eval(partial), 106),
                         "Lambert forward and reverse partials agree");
        expr_free(partial); num_destroy(&gradients[j]);
    }
    num_destroy(&result);
    expr_free(function); expr_free(n); expr_free(s); expr_free(r); expr_free(z); expr_free(q); expr_free(a);
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(512);
    number_t base = num_create_from_string("0.250000000000000000000000000000000000000000000001");
    number_t real_base = num_as_inexact_real_prec(base, 512), complex_base = num_as_complex_prec(real_base, 512);
    number_t real_value = num_lambert_S(NUM_HALF, real_base, NUM_ONE, NUM_ONE, NUM_NEG_ONE, NUM_TWO);
    number_t complex_value = num_lambert_S(NUM_HALF, complex_base, NUM_ONE, NUM_ONE, NUM_NEG_ONE, NUM_TWO);
    TEST_ASSERT_TRUE(lambert_close(complex_value, real_value, 512),
                     "a zero imaginary component retains every bit when selecting the real kernel");
    number_t inputs[] = {NUM_HALF, num_create_from_frac(1, 4), NUM_ONE, NUM_ONE, NUM_NEG_ONE, NUM_INF};
    number_t floating[6];
    size_t input_precision[6];
    for (unsigned j = 0; j < 6; ++j) {
        floating[j] = num_as_inexact_real_prec(inputs[j], 512);
        input_precision[j] = num_get_prec_bits(floating[j]);
    }
    for (unsigned repeat = 0; repeat < 8; ++repeat)
        TEST_ASSERT_TRUE(lambert_controlled(floating), "repeated floating Lambert inputs retain a finite result");
    for (unsigned j = 0; j < 6; ++j)
        TEST_ASSERT_TRUE(num_get_prec_bits(floating[j]) == input_precision[j],
                         "Lambert guard precision never widens a borrowed input across calls");
    computation_options_t options = {.work_limit = 18};
    computation_t *control = computation_new(&options);
    TEST_ASSERT_TRUE(control && !computation_run(control, lambert_controlled, inputs) &&
                     computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                     "geometric acceleration retains cancellation inside its outer series");
    computation_free(control);
    number_t recurrent[] = {NUM_NEG_ONE, num_exp(num_mul(NUM_I, num_create_from_frac(1, 500))),
                            NUM_ONE, NUM_ONE, NUM_NEG_ONE, num_create_from_long(1000)};
    control = computation_new(&options);
    TEST_ASSERT_TRUE(control && !computation_run(control, lambert_controlled, recurrent) &&
                     computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                     "the reciprocal recurrence retains per-term cancellation checkpoints");
    computation_free(control);
    TEST_ASSERT_TRUE(num_get_default_prec_bits() == 512 && lambert_controlled(inputs),
                     "a stopped Lambert call leaves the caller's precision and subsequent calls intact");
    num_set_default_prec_bits(saved);
}

/* Run the database-backed numerical and analytic Lambert checks in their ordinary suite group. */
void test_expr_lambert(void)
{
    TEST_RUN_SUBTEST(test_lambert_values, NULL);
    TEST_RUN_SUBTEST(test_lambert_real, NULL);
    TEST_RUN_SUBTEST(test_quotient_recognition, NULL);
    TEST_RUN_SUBTEST(test_lambert_calculus, NULL);
    TEST_RUN_SUBTEST(test_lambert_notation, NULL);
    TEST_RUN_SUBTEST(test_lambert_integral_limits, NULL);
    TEST_RUN_SUBTEST(test_lambert_interfaces, NULL);
    TEST_RUN_SUBTEST(test_lambert_qpartials, NULL);
}
