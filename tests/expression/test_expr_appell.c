/**
 * @file test_expr_appell.c
 * @brief Database-backed Appell parsing, evaluation, calculus and rendering regressions.
 *
 * SQL supplies public spellings, parameter lists, bindings and independent expected values. C retains the
 * checking logic for symbolic derivatives, affine antiderivatives, reverse-mode evaluation and style round trips.
 * These are ordinary expression tests; documented runnable examples execute later in the suite.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "number-t.h"
#include "string-t.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_expr.h"

typedef struct {
    const test_database_case_t *row;
    expr_t *expression, *expected, *variable, *actual, *primitive;
    expr_bindings_t *bindings;
    number_t value, reference;
    size_t precision;
} appell_check_state_t;

typedef struct {
    const char *name;
    bool (*check)(appell_check_state_t *state);
    const char *variable;
} appell_operation_t;

static bool appell_close(number_t value, number_t expected, size_t precision)
{
    number_t error = num_abs(num_sub(value, expected));
    number_t scale = num_abs(expected);
    if (num_is_zero(scale)) scale = NUM_ONE;
    return num_is_finite(error) && num_lt(error, num_ldexp(scale, -(int)precision + 24));
}

static bool appell_check_value(appell_check_state_t *state)
{
    state->value = expr_eval(state->expression);
    return true;
}

static bool appell_check_derivative(appell_check_state_t *state)
{
    state->actual = state->variable ? expr_create_deriv(state->expression, state->variable) : NULL;
    state->value = state->actual ? expr_eval(state->actual) : NUM_NAN;
    return state->actual != NULL;
}

static bool appell_check_primitive(appell_check_state_t *state)
{
    state->primitive = state->variable ? expr_integrate(state->expression, state->variable) : NULL;
    state->actual = state->primitive ? expr_create_deriv(state->primitive, state->variable) : NULL;
    state->value = state->actual ? expr_eval(state->actual) : NUM_NAN;
    return state->primitive && !expr_contains_integral_operation(state->primitive) && state->actual;
}

static bool appell_check_reverse(appell_check_state_t *state)
{
    const expr_t *variables[] = {state->variable};
    return state->variable && expr_eval_derivatives(state->expression, 1, variables, NULL, &state->value) == 0;
}

static bool appell_check_roundtrip(appell_check_state_t *state)
{
    string_t *native = expr_to_text(state->expression, style_EXPRESSION);
    string_t *function = expr_to_function_body_text(state->expression);
    string_t *notation = expr_to_text(state->expression, style_LATEX);
    expr_t *native_again = native ? expr_from_text(native, NULL) : NULL;
    expr_bindings_t *function_bindings = NULL;
    expr_t *function_again = function ? expr_from_function_body_text(function, &function_bindings) : NULL;
    if (function_bindings) {
        expr_t *x = expr_bindings_get(function_bindings, "x"), *y = expr_bindings_get(function_bindings, "y");
        if (x && state->bindings) expr_set_val(x, expr_eval(expr_bindings_get(state->bindings, "x")));
        if (y && state->bindings) expr_set_val(y, expr_eval(expr_bindings_get(state->bindings, "y")));
    }
    state->value = native_again ? expr_eval(native_again) : NUM_NAN;
    number_t other = function_again ? expr_eval(function_again) : NUM_NAN;
    bool ok = native_again && function_again && notation && strstr(string_c_str(notation), "F") &&
        (strstr(string_c_str(notation), "_{2}") || strstr(string_c_str(notation), "_{3}") ||
         strstr(string_c_str(notation), "_{4}")) && appell_close(other, state->reference, state->precision);
    if (!ok) fprintf(stderr, "%s: native %s; function %s; TeX %s\n", string_c_str(state->row->name),
        native ? string_c_str(native) : "missing", function ? string_c_str(function) : "missing",
        notation ? string_c_str(notation) : "missing");
    expr_free(function_again); expr_free(native_again); expr_bindings_free(function_bindings);
    string_free(notation); string_free(function); string_free(native);
    return ok;
}

static bool appell_check_reject(appell_check_state_t *state)
{
    state->value = state->reference = NUM_ZERO;
    return state->expression == NULL;
}

static const appell_operation_t appell_operations[] = {
    {"appell_dx", appell_check_derivative, "x"},
    {"appell_dy", appell_check_derivative, "y"},
    {"appell_parse_reject", appell_check_reject, "x"},
    {"appell_primitive", appell_check_primitive, "x"},
    {"appell_reverse", appell_check_reverse, "x"},
    {"appell_roundtrip", appell_check_roundtrip, "x"},
    {"appell_value", appell_check_value, "x"}
};

static int appell_operation_compare(const void *key, const void *entry)
{
    return strcmp(key, ((const appell_operation_t *)entry)->name);
}

static void appell_expression_row(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_digits(row->precision);
    NUM_SCOPE(scope);
    const appell_operation_t *operation = bsearch(string_c_str(row->operation), appell_operations,
        sizeof appell_operations / sizeof *appell_operations, sizeof *appell_operations, appell_operation_compare);
    appell_check_state_t state = {.row = row, .precision = num_get_default_prec_bits(),
                                  .value = NUM_NAN, .reference = NUM_NAN};
    state.expression = expr_from_text(row->input, &state.bindings);
    state.expected = operation && operation->check == appell_check_primitive ? expr_clone(state.expression) :
        operation && operation->check != appell_check_reject ? expr_from_text(row->expected, NULL) : NULL;
    state.variable = state.bindings && operation ? expr_bindings_get(state.bindings, operation->variable) : NULL;
    state.reference = state.expected ? expr_eval(state.expected) : NUM_NAN;
    bool ok = operation && (operation->check == appell_check_reject || (state.expression && state.expected)) &&
        operation->check(&state) && appell_close(state.value, state.reference, state.precision);
    if (!ok) {
        string_t *primitive = state.primitive ? expr_to_text(state.primitive, style_EXPRESSION) : NULL;
        string_t *derivative = state.actual ? expr_to_text(state.actual, style_EXPRESSION) : NULL;
        if (primitive) fprintf(stderr, "primitive: %s\n", string_c_str(primitive));
        if (derivative) fprintf(stderr, "derivative: %s\n", string_c_str(derivative));
        string_free(derivative); string_free(primitive);
        string_t *got = num_to_string(state.value), *want = num_to_string(state.reference);
        fprintf(stderr, "%s: got %s; expected %s\n", string_c_str(row->name),
            got ? string_c_str(got) : "missing", want ? string_c_str(want) : "missing");
        string_free(want); string_free(got);
    }
    expr_free(state.primitive); expr_free(state.actual); expr_free(state.expected); expr_free(state.expression);
    expr_bindings_free(state.bindings);
    num_set_default_prec_bits(saved);
    TEST_ASSERT_TRUE(ok, "Appell syntax, calculus, gradients and styles agree with independent references");
}

/* Run all authored Appell modes through their database selections without changing existing cases. */
void test_expr_appell(void)
{
    for (size_t j = 0; j < sizeof appell_operations / sizeof *appell_operations; ++j) {
        string_t *module = string_sprintf("expression.%s", appell_operations[j].name);
        test_run_database_cases(__FILE__, __LINE__, string_c_str(module), NULL, appell_expression_row);
        string_free(module);
    }
}
