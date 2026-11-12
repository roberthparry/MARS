/**
 * @file test_expr_numeric.c
 * @brief Database-backed numerical callable coverage and the exact-integer README programme.
 *
 * Checks parsed numerical helpers, precision-replayable bindings, symbolic round trips and semantic TeX output.
 * The documented example runs separately after ordinary expression tests.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "matrix-t.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_expr.h"

static void numeric_row(const void *data)
{
    const test_database_case_t *row = data;
    expr_t *expression = expr_from_text(row->input, NULL);
    expr_t *expected = expr_from_text(row->expected, NULL);
    number_t got = expression ? expr_eval(expression) : NUM_NAN;
    number_t want = expected ? expr_eval(expected) : NUM_NAN;
    bool ok = expression && expected && ((num_is_nan(got) && num_is_nan(want)) || num_eq(got, want));
    if (!ok) {
        string_t *actual = num_to_string(got);
        printf("%s: %s -> %s; wanted %s\n", string_c_str(row->name), string_c_str(row->input),
               actual ? string_c_str(actual) : "unavailable", string_c_str(row->expected));
        string_free(actual);
    }
    num_destroy(&want);
    num_destroy(&got);
    expr_free(expected);
    expr_free(expression);
    TEST_ASSERT_TRUE(ok, "numerical callable evaluates exactly, including invalid-domain NAN outcomes");
}

static void integral_root_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(row->precision);
    expr_t *expression = expr_from_text(row->input, NULL);
    expr_t *expected = expr_from_text(row->expected, NULL);
    number_t got = expression ? expr_eval(expression) : NUM_NAN;
    number_t want = expected ? expr_eval(expected) : NUM_NAN;
    number_t difference = num_sub(got, want), error = num_abs(difference);
    number_t tolerance = num_ldexp(NUM_ONE, -(int)row->precision + 12);
    number_t magnitude = num_abs(want);
    if (!num_is_zero(magnitude)) {
        number_t scaled = num_mul(tolerance, magnitude);
        num_destroy(&tolerance);
        tolerance = scaled;
    }
    bool ok = expression && expected && num_is_real(got) && num_is_finite(got) && num_lt(error, tolerance);
    if (!ok) {
        string_t *actual = num_to_string(got);
        fprintf(stderr, "%s: got %s; expected %s\n", string_c_str(row->name),
                actual ? string_c_str(actual) : "unavailable", string_c_str(row->expected));
        string_free(actual);
    }
    num_destroy(&tolerance);
    num_destroy(&magnitude);
    num_destroy(&error);
    num_destroy(&difference);
    num_destroy(&want);
    num_destroy(&got);
    expr_free(expected);
    expr_free(expression);
    num_set_default_prec_bits(precision);
    TEST_ASSERT_TRUE(ok, "affine-exponential root integrals agree with independent elementary primitives");
}

static void integral_primitive_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(256u);
    expr_bindings_t *bindings = NULL;
    expr_t *integrand = expr_from_text(row->input, &bindings);
    expr_t *variable = expr_bindings_get(bindings, "y");
    expr_t *primitive = variable ? expr_integrate(integrand, variable) : NULL;
    expr_t *derivative = primitive ? expr_create_deriv(primitive, variable) : NULL;
    number_t got = derivative ? expr_eval(derivative) : NUM_NAN;
    number_t want = integrand ? expr_eval(integrand) : NUM_NAN;
    number_t difference = num_sub(got, want), error = num_abs(difference);
    number_t tolerance = num_create_from_string("1e-60");
    bool formal = !strcmp(string_c_str(row->expected), "formal");
    bool ok = formal ? !primitive || expr_contains_integral_operation(primitive) :
              primitive && !expr_contains_integral_operation(primitive) && num_is_finite(got) &&
              num_is_real(got) && num_lt(error, tolerance);
    if (!ok) {
        char *text = primitive ? expr_to_string(primitive, style_EXPRESSION) : NULL;
        fprintf(stderr, "%s: primitive %s\n", string_c_str(row->name), text ? text : "unavailable");
        free(text);
    }
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&difference);
    num_destroy(&want);
    num_destroy(&got);
    expr_free(derivative);
    expr_free(primitive);
    expr_free(integrand);
    expr_bindings_free(bindings);
    num_set_default_prec_bits(precision);
    TEST_ASSERT_TRUE(ok, "elementary exponential-root primitives differentiate back to their authored integrands");
}

static void render_row(const void *data)
{
    const test_database_case_t *row = data;
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_text(row->input, &bindings);
    string_t *native = expression ? expr_to_text(expression, style_EXPRESSION) : NULL;
    string_t *function = expression ? expr_to_text(expression, style_FUNCTION) : NULL;
    string_t *notation = expression ? expr_to_text(expression, style_LATEX) : NULL;
    expr_t *reparsed = native ? expr_from_text(native, NULL) : NULL;
    bool ok = expression && native && function && notation && reparsed &&
              string_find(function, string_c_str(row->expected)) >= 0 &&
              string_byte_length(notation) > 0 && !strstr(string_c_str(notation), "integer_parameters");
    number_t original_value = expression ? expr_eval(expression) : NUM_NAN;
    number_t roundtrip_value = reparsed ? expr_eval(reparsed) : NUM_NAN;
    ok = ok && num_eq(original_value, roundtrip_value);
    if (strcmp(string_c_str(row->name), "symbolic_binding_powmod"))
        ok = ok && !expr_is_differentiable(expression);
    num_destroy(&roundtrip_value);
    num_destroy(&original_value);
    if (!ok)
        printf("%s\nnative: %s\nfunction: %s\nTeX: %s\n", string_c_str(row->name),
               native ? string_c_str(native) : "missing", function ? string_c_str(function) : "missing",
               notation ? string_c_str(notation) : "missing");
    expr_free(reparsed);
    string_free(notation);
    string_free(function);
    string_free(native);
    expr_free(expression);
    expr_bindings_free(bindings);
    TEST_ASSERT_TRUE(ok, "native, Function and TeX output retain complete operands and round-trip correctly");
}

void test_expr_numeric_callables(void)
{
    test_run_database_cases(__FILE__, __LINE__, "expression.numeric_callables", NULL, numeric_row);
    test_run_database_cases(__FILE__, __LINE__, "expression.numeric_rendering", NULL, render_row);
    test_run_database_cases(__FILE__, __LINE__, "expression.integral_exp_root", NULL, integral_root_row);
    test_run_database_cases(__FILE__, __LINE__, "expression.integral_exp_root_primitive", NULL, integral_primitive_row);
    matrix_t *matrix = mat_from_string_expr("(powmod(4,13,497), bit_length(2^1024); set_bit(0,5), gcdext(30,21,0))", NULL);
    matrix_t *values = matrix ? mat_evaluate(matrix) : NULL;
    bool ok = values && mat_get_row_count(values) == 2 && mat_get_col_count(values) == 2;
    const long expected[] = {445, 1025, 32, 3};
    for (size_t i = 0; ok && i < 4; ++i) {
        number_t got = mat_get_num(values, i / 2, i % 2);
        number_t want = num_create_from_long(expected[i]);
        ok = num_eq(got, want);
        num_destroy(&want);
        num_destroy(&got);
    }
    mat_free(values);
    mat_free(matrix);
    TEST_ASSERT_TRUE(ok, "matrix entries use the same exact numerical callable support");
}

/* README example: docs/expression.md, exact integer functions available in MARS Lab. */
void example_expression_integer_callables(void)
{
    const char *sources[] = {"powmod(4,13,497)", "bit_length(2^1024)", "test_bit(set_bit(0,701),701)",
                             "divmod(17,5,0)", "divmod(17,5,1)", "gcdext(30,21,0)"};
    const long expected[] = {445, 1025, 1, 3, 2, 3};
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        expr_t *expression = expr_from_string(sources[i], NULL);
        number_t value = expression ? expr_eval(expression) : NUM_NAN;
        string_t *text = num_to_string(value);
        if (text)
            printf("%s = %s\n", sources[i], string_c_str(text));
        number_t want = num_create_from_long(expected[i]);
        TEST_ASSERT_TRUE(expression && text && num_eq(value, want), "README numerical call produces documented output");
        num_destroy(&want);
        string_free(text);
        num_destroy(&value);
        expr_free(expression);
    }
}
