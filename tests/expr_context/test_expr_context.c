/**
 * @file test_expr_context.c
 * @brief Symbol policy, ownership and binding-aware expression context regressions.
 *
 * Exercises independent context revisions, contradictory updates, variable identity,
 * assumptions and numerical evidence. The README example runs last and uses only
 * public APIs, retaining symbolic formulae while changing a parameter binding.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_context-t.h"
#include "equation-t.h"
#include "matrix-t.h"
#include "diffequation-t.h"
#include "test_harness.h"
#include "test_context_solver.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void context_require(bool success)
{
    if (!success)
        abort();
}

static void context_expect(const expr_context_t *context, const expr_t *source, const char *expected)
{
    expr_t *display = expr_context_simplify(context, source);
    char *text = display ? expr_to_string(display, style_UNBOUND) : NULL;
    TEST_ASSERT_STR_EQ(text, expected);
    free(text);
    expr_free(display);
}

static void test_context_binding_snapshots(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{x+sqrt(s^2)/s | x=1; s=15}", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x"), *s = expr_bindings_get(bindings, "s");
    expr_context_t *context = expr_context_from_expression(source, x);
    expr_context_t *copy = expr_context_clone(context);
    context_require(source && context && copy);
    expr_t *snapshot = expr_context_simplify(context, source);
    number_t negative = num_create_from_long(-15);
    uint64_t revision = expr_context_revision(context);
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, negative));
    TEST_ASSERT_TRUE(expr_context_revision(context) == revision + 1, "update advances revision");
    TEST_ASSERT_TRUE(expr_context_revision(copy) == revision, "cloned revision remains independent");
    context_expect(context, source, "x - 1");
    context_expect(copy, source, "x + 1");
    char *old = expr_to_string(snapshot, style_UNBOUND);
    TEST_ASSERT_STR_EQ(old, "x + 1");
    number_t value = expr_context_evaluate(context, source);
    TEST_ASSERT_TRUE(num_eq(value, NUM_ZERO), "evaluate original algebra with changed parameter");
    num_destroy(&value);
    value = expr_eval(s);
    number_t positive = num_create_from_long(15);
    TEST_ASSERT_TRUE(num_eq(value, positive), "context does not mutate source bindings");
    num_destroy(&value);
    expr_set_val(s, negative);
    context_expect(copy, source, "x + 1");
    expr_context_free(context);
    expr_free(source);
    expr_bindings_free(bindings);
    number_t result = expr_context_evaluate(copy, snapshot);
    TEST_ASSERT_TRUE(num_eq(result, NUM_TWO), "context and snapshot outlive their source");
    num_destroy(&result);
    expr_context_free(copy);
    expr_free(snapshot);
    num_destroy(&positive);
    num_destroy(&negative);
    free(old);
}

static void test_context_free_and_assumed(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{sqrt(x^2) | x=15}", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    expr_context_t *context = expr_context_from_expression(source, NULL);
    context_require(context != NULL);
    context_expect(context, source, "√(x²)");
    context_require(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_REAL, NUM_NAN));
    context_expect(context, source, "√(x²)");
    context_require(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_POSITIVE, NUM_NAN));
    context_expect(context, source, "x");
    number_t value = expr_context_evaluate(context, source);
    TEST_ASSERT_TRUE(num_is_nan(value), "an assumption is not a numerical binding");
    num_destroy(&value);
    expr_t *ratio = expr_div(source, x);
    value = expr_context_evaluate(context, ratio);
    TEST_ASSERT_TRUE(num_eq(value, NUM_ONE), "proved binding-independent value needs no numerical sample");
    num_destroy(&value);
    expr_free(ratio);
    context_require(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_NEGATIVE, NUM_NAN));
    context_expect(context, source, "-x");
    uint64_t revision = expr_context_revision(context);
    TEST_ASSERT_TRUE(expr_context_remove(context, x), "remove existing symbol");
    TEST_ASSERT_TRUE(expr_context_revision(context) == revision + 1, "removal advances revision");
    TEST_ASSERT_TRUE(!expr_context_remove(context, x), "absent removal leaves revision alone");
    context_expect(context, source, "√(x²)");
    expr_context_free(context);
    expr_free(source);
    expr_bindings_free(bindings);
}

static void test_context_conditions(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{x+2 where (Re(1/(2*s^2))>0) | x=1; s=?}", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x"), *s = expr_bindings_get(bindings, "s");
    expr_context_t *context = expr_context_from_expression(source, x);
    context_require(context != NULL);
    expr_t *unknown = expr_context_simplify(context, source);
    char *text = expr_to_string(unknown, style_FUNCTION);
    TEST_ASSERT_TRUE(text && strstr(text, "realpart("), "unknown condition is retained");
    free(text);
    expr_free(unknown);
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER,
                                    EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO, NUM_NAN));
    context_expect(context, source, "x + 2");
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, NUM_ZERO));
    context_expect(context, source, "NAN");
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, NUM_I));
    context_expect(context, source, "NAN");
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, NUM_ONE));
    context_expect(context, source, "x + 2");
    context_require(expr_context_set(context, s, EXPR_CONTEXT_FREE, 0, NUM_ONE));
    unknown = expr_context_simplify(context, source);
    text = expr_to_string(unknown, style_FUNCTION);
    TEST_ASSERT_TRUE(text && strstr(text, "realpart("), "free sample does not prove a condition");
    free(text);
    expr_free(unknown);
    expr_context_free(context);
    expr_free(source);
    expr_bindings_free(bindings);
}

static void test_context_invalid_updates(void)
{
    expr_context_t *context = expr_context_new();
    expr_t *x = expr_new_named_var(NUM_NAN, "x");
    context_require(context && x);
    context_require(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_POSITIVE, NUM_ONE));
    uint64_t revision = expr_context_revision(context);
    TEST_ASSERT_TRUE(!expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_POSITIVE, NUM_ZERO) &&
                     errno == EDOM, "contradictory value rejected");
    TEST_ASSERT_TRUE(!expr_context_set(context, x, EXPR_CONTEXT_FREE,
                                      EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE, NUM_NAN) && errno == EDOM,
                     "contradictory assumptions rejected");
    TEST_ASSERT_TRUE(!expr_context_set(context, x, (expr_context_role_t)99, 0, NUM_NAN) && errno == EINVAL,
                     "unknown role rejected");
    TEST_ASSERT_TRUE(!expr_context_set(context, x, EXPR_CONTEXT_FREE, 128, NUM_NAN), "unknown assumption rejected");
    TEST_ASSERT_TRUE(!expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_REAL, NUM_I) && errno == EDOM,
                     "complex binding contradicts real assumption");
    TEST_ASSERT_TRUE(!expr_context_set(context, x, EXPR_CONTEXT_FREE, 0, NUM_INF) && errno == EINVAL,
                     "non-finite binding rejected");
    TEST_ASSERT_TRUE(!expr_context_set(context, EXPR_ONE, EXPR_CONTEXT_PARAMETER, 0, NUM_TWO),
                     "cannot rebind literal constants");
    TEST_ASSERT_TRUE(expr_context_revision(context) == revision, "failed updates are atomic");
    unsigned assumptions = 0;
    number_t value;
    expr_context_role_t role;
    context_require(expr_context_get(context, x, &role, &assumptions, &value));
    TEST_ASSERT_TRUE(role == EXPR_CONTEXT_FREE && num_eq(value, NUM_ONE) &&
                     assumptions == (EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO | EXPR_CONTEXT_POSITIVE),
                     "normalised assumptions and original value retained");
    num_destroy(&value);
    TEST_ASSERT_TRUE(!expr_context_from_expression(NULL, NULL) && !expr_context_clone(NULL), "NULL constructors");
    TEST_ASSERT_TRUE(!expr_context_from_expression(x, EXPR_ONE), "active symbol must be a variable");
    TEST_ASSERT_TRUE(!expr_context_simplify(NULL, x) && !expr_context_simplify(context, NULL), "NULL operations");
    value = expr_context_evaluate(NULL, x);
    TEST_ASSERT_TRUE(num_is_nan(value), "invalid evaluation returns owned NAN");
    num_destroy(&value);
    expr_context_free(NULL);
    expr_context_free(context);
    expr_free(x);
}

static void test_context_symbol_identity(void)
{
    expr_context_t *context = expr_context_new();
    expr_t *first = expr_new_named_var(NUM_NAN, "s");
    expr_t *second = expr_new_named_var(NUM_NAN, "s");
    expr_t *constant = expr_new_named_const(NUM_NAN, "s");
    expr_t *clone = expr_clone(first);
    context_require(context && first && second && constant && clone);
    context_require(expr_context_set(context, first, EXPR_CONTEXT_PARAMETER, 0, NUM_ONE));
    context_require(expr_context_set(context, second, EXPR_CONTEXT_PARAMETER, 0, NUM_TWO));
    context_require(expr_context_set(context, constant, EXPR_CONTEXT_PARAMETER, 0, NUM_ZERO));
    number_t a = expr_context_evaluate(context, clone);
    number_t b = expr_context_evaluate(context, second);
    number_t c = expr_context_evaluate(context, constant);
    TEST_ASSERT_TRUE(num_eq(a, NUM_ONE) && num_eq(b, NUM_TWO) && num_eq(c, NUM_ZERO),
                     "same-named variables stay distinct; clones retain identity; constants have a separate key");
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&c);
    expr_free(clone);
    expr_free(constant);
    expr_free(second);
    expr_free(first);
    expr_context_free(context);
}

static void test_context_scopes_and_conflicts(void)
{
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    expr_t *s = expr_new_named_const(NUM_ONE, "s");
    expr_t *other = expr_new_named_const(NUM_TWO, "s");
    expr_t *conflict = expr_add(s, other);
    expr_context_t *bad = expr_context_from_expression(conflict, x);
    TEST_ASSERT_TRUE(!bad && errno == EDOM, "conflicting same-named constants do not silently overwrite");
    expr_context_free(bad);
    expr_t *body = expr_mul(s, x);
    expr_t *integral = expr_integral(body, x);
    expr_context_t *context = expr_context_from_expression(integral, x);
    context_require(context != NULL);
    TEST_ASSERT_TRUE(!expr_context_get(context, s, NULL, NULL, NULL), "capture does not enter a binder");
    context_require(expr_context_set(context, x, EXPR_CONTEXT_PARAMETER, EXPR_CONTEXT_NEGATIVE, NUM_NAN));
    expr_t *snapshot = expr_context_simplify(context, integral);
    char *before = expr_to_string(integral, style_FUNCTION);
    char *after = expr_to_string(snapshot, style_FUNCTION);
    TEST_ASSERT_STR_EQ(after, before);
    free(after);
    free(before);
    expr_free(snapshot);
    expr_context_free(context);
    expr_free(integral);
    expr_free(body);
    expr_free(conflict);
    expr_free(other);
    expr_free(s);
    expr_free(x);
}

static void test_context_many_symbols(void)
{
    expr_context_t *context = expr_context_new();
    expr_t *symbols[128];
    for (size_t i = 0; i < sizeof symbols / sizeof *symbols; ++i) {
        number_t value = num_create_from_long((long)i);
        symbols[i] = expr_new_named_var(NUM_NAN, "x");
        context_require(expr_context_set(context, symbols[i], EXPR_CONTEXT_PARAMETER, 0, value));
        num_destroy(&value);
    }
    expr_context_t *copy = expr_context_clone(context);
    context_require(copy != NULL);
    expr_context_free(context);
    bool correct = true;
    for (size_t i = 0; i < sizeof symbols / sizeof *symbols; ++i) {
        number_t value;
        number_t expected = num_create_from_long((long)i);
        context_require(expr_context_get(copy, symbols[i], NULL, NULL, &value));
        correct = num_eq(value, expected) && correct;
        num_destroy(&value);
        num_destroy(&expected);
        expr_free(symbols[i]);
    }
    TEST_ASSERT_TRUE(correct, "hash-indexed clones retain all distinct variable identities");
    expr_context_free(copy);
}

/* README example: the source remains symbolic; a changed parameter changes its justified display. */
static void example_expr_context_binding(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{x+sqrt(s^2)/s | x=1; s=15}", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    expr_t *s = expr_bindings_get(bindings, "s");
    expr_context_t *context = expr_context_from_expression(source, x);
    context_require(context != NULL);
    expr_t *display = expr_context_simplify(context, source);
    char *text = expr_to_string(display, style_UNBOUND);
    printf("Positive binding: %s\n", text);
    TEST_ASSERT_STR_EQ(text, "x + 1");
    free(text);
    expr_free(display);

    number_t negative = num_create_from_long(-15);
    context_require(expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, negative));
    num_destroy(&negative);
    display = expr_context_simplify(context, source);
    text = expr_to_string(display, style_UNBOUND);
    printf("Negative binding: %s\n", text);
    TEST_ASSERT_STR_EQ(text, "x - 1");
    free(text);
    expr_free(display);
    expr_context_free(context);
    expr_free(source);
    expr_bindings_free(bindings);
}

static void test_context_equation_results(void)
{
    equation_t *source = equ_from_string("{x=sqrt(s^2) | x=?; s=15}");
    context_require(source != NULL);
    equation_t *display = equ_simplify_with_context(source, NULL);
    char *text = display ? expr_to_string(equ_rhs(display), style_UNBOUND) : NULL;
    TEST_ASSERT_STR_EQ(text, "s");
    free(text);
    equ_free(display);
    expr_context_t *context = expr_context_new();
    context_require(expr_context_set(context, equ_binding(source, "s"), EXPR_CONTEXT_PARAMETER,
                                     EXPR_CONTEXT_NEGATIVE, NUM_NAN));
    equation_solutions_t *solutions = equ_derive_solutions_with_context(source, context);
    context_require(solutions && equ_solutions_count(solutions) == 1);
    text = expr_to_string(equ_rhs(equ_solutions_at(solutions, 0)), style_UNBOUND);
    TEST_ASSERT_STR_EQ(text, "-s");
    free(text);
    equ_solutions_free(solutions);
    number_t original = expr_eval(equ_binding(source, "s"));
    number_t fifteen = num_create_from_long(15);
    TEST_ASSERT_TRUE(num_eq(original, fifteen), "solving does not alter source bindings");
    num_destroy(&fifteen);
    num_destroy(&original);
    expr_context_free(context);
    equ_free(source);
    source = equ_from_string("{x=sqrt(s^2) | x=?; s=?}");
    display = equ_simplify_with_context(source, NULL);
    text = expr_to_string(equ_rhs(display), style_UNBOUND);
    TEST_ASSERT_STR_EQ(text, "√(s²)");
    free(text);
    equ_free(display);
    equ_free(source);
    TEST_ASSERT_TRUE(equ_simplify_with_context(NULL, NULL) == NULL, "invalid equation rejected");
}

static void test_context_matrix_results(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *entry = expr_from_string("{sqrt(s^2)/s | s=15}", &bindings);
    matrix_t *source = mat_create_expr(1, 1, &entry);
    expr_context_t *context = expr_context_from_expression(entry, NULL);
    matrix_t *display = mat_simplify_with_context(source, context);
    expr_t *got = NULL;
    mat_get(display, 0, 0, &got);
    char *text = expr_to_string(got, style_UNBOUND);
    TEST_ASSERT_STR_EQ(text, "√(s²)/s");
    free(text);
    mat_free(display);
    context_require(expr_context_set(context, expr_bindings_get(bindings, "s"), EXPR_CONTEXT_FREE,
                                     EXPR_CONTEXT_POSITIVE, NUM_NAN));
    display = mat_simplify_with_context(source, context);
    mat_get(display, 0, 0, &got);
    number_t value = expr_eval(got);
    TEST_ASSERT_TRUE(num_eq(value, NUM_ONE), "explicit assumptions simplify symbolic entries");
    num_destroy(&value);
    mat_free(display);
    context_require(expr_context_set(context, expr_bindings_get(bindings, "s"), EXPR_CONTEXT_FREE,
                                     EXPR_CONTEXT_NEGATIVE, NUM_NAN));
    display = mat_simplify_with_context(source, context);
    mat_get(display, 0, 0, &got);
    value = expr_eval(got);
    number_t minus_one = num_create_from_long(-1);
    TEST_ASSERT_TRUE(num_eq(value, minus_one), "recompute from original after context changes");
    num_destroy(&minus_one);
    num_destroy(&value);
    mat_free(display);
    mat_free(source);
    expr_free(entry);
    expr_bindings_free(bindings);
    expr_context_free(context);
    TEST_ASSERT_TRUE(mat_simplify_with_context(NULL, NULL) == NULL, "invalid matrix rejected");
}

static void test_context_differential_results(void)
{
    diffequ_t *de = de_from_string("{ Dx(y) = sqrt(a^2) | x = ?; a = ?; y(0) = 0 }");
    expr_context_t *context = expr_context_new();
    context_require(de && expr_context_set(context, de_constant(de, "a"), EXPR_CONTEXT_PARAMETER,
                                          EXPR_CONTEXT_POSITIVE, NUM_NAN));
    diffequ_solve_result_t *result = de_solve_with_context(de, DE_SOLVE_OPTION_STEPS, context);
    context_require(result && de_solve_result_count(result) == 1);
    TEST_ASSERT_TRUE(de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED, "context retains solver status");
    char *text = expr_to_string(equ_rhs(de_solve_result_at(result, 0)), style_UNBOUND);
    TEST_ASSERT_TRUE(text && !strstr(text, "√"), "parameter assumption removes the principal square root");
    TEST_ASSERT_TRUE(text && strstr(text, "a") && strstr(text, "x"), "parameter and coordinate remain symbolic");
    TEST_ASSERT_TRUE(de_solve_result_steps(result) != NULL, "derivation is retained");
    free(text);
    de_solve_result_free(result);
    expr_context_free(context);
    de_free(de);
}

static void test_context_consumer_boundaries(void)
{
    equation_t *problem = equ_from_string("{x^2=4 | x=1}");
    expr_context_t *context = expr_context_new();
    context_require(expr_context_set(context, equ_binding(problem, "x"), EXPR_CONTEXT_FREE,
                                     EXPR_CONTEXT_POSITIVE, NUM_ONE));
    equation_solutions_t *roots = equ_derive_solutions_with_context(problem, context);
    TEST_ASSERT_TRUE(equ_solutions_count(roots) == 2, "result simplification never filters solver branches");
    equ_solutions_free(roots);
    expr_context_free(context);
    equ_free(problem);

    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{x+2 where (Re(s)>0) | x=?; s=?}", &bindings);
    expr_t *zero = expr_new_const(NUM_ZERO);
    equation_t *equation = equ_new(zero, source);
    matrix_t *matrix = mat_new_sparse_expr(2, 2);
    mat_set(matrix, 1, 1, &source);
    context = expr_context_from_expression(source, NULL);
    equation_t *display = equ_simplify_with_context(equation, context);
    matrix_t *snapshot = mat_simplify_with_context(matrix, context);
    expr_t *entry = NULL;
    mat_get(snapshot, 1, 1, &entry);
    char *text = expr_to_string(equ_rhs(display), style_FUNCTION);
    TEST_ASSERT_TRUE(text && strstr(text, "realpart"), "equations retain unproved restrictions");
    free(text);
    text = expr_to_string(entry, style_FUNCTION);
    TEST_ASSERT_TRUE(text && strstr(text, "realpart"), "sparse matrix entries retain unproved restrictions");
    free(text);
    mat_free(snapshot);
    equ_free(display);
    context_require(expr_context_set(context, expr_bindings_get(bindings, "s"), EXPR_CONTEXT_FREE,
                                     EXPR_CONTEXT_POSITIVE, NUM_NAN));
    display = equ_simplify_with_context(equation, context);
    text = expr_to_string(equ_rhs(display), style_UNBOUND);
    TEST_ASSERT_STR_EQ(text, "x + 2");
    free(text);
    equ_free(display);
    snapshot = mat_simplify_with_context(matrix, context);
    mat_get(snapshot, 1, 1, &entry);
    text = expr_to_string(entry, style_UNBOUND);
    TEST_ASSERT_STR_EQ(text, "x + 2");
    free(text);
    mat_free(snapshot);
    mat_free(matrix);
    equ_free(equation);
    expr_free(zero);
    expr_free(source);
    expr_bindings_free(bindings);
    expr_context_free(context);

    number_t two = num_clone(NUM_TWO);
    matrix = mat_create(1, 1, &two);
    snapshot = mat_simplify_with_context(matrix, NULL);
    number_t value = mat_get_num(snapshot, 0, 0);
    TEST_ASSERT_TRUE(num_eq(value, two) && mat_typeof(snapshot) == MAT_TYPE_NUMBER,
                     "numeric matrices retain their domain and values");
    num_destroy(&value);
    num_destroy(&two);
    mat_free(snapshot);
    mat_free(matrix);
}

static void test_context_differential_metadata(void)
{
    diffequ_t *de = de_from_string("y'' + 3*y*y' + y^4 = 0; y(0)=0; y'(0)=1");
    diffequ_solve_result_t *original = de_solve(de);
    diffequ_solve_result_t *result = de_solve_with_context(de, DE_SOLVE_OPTION_NONE, NULL);
    context_require(original && result);
    TEST_ASSERT_TRUE(de_solve_result_status(result) == DE_SOLVE_STATUS_SERIES, "series status survives");
    TEST_ASSERT_TRUE(de_solve_result_series_degree(result) == de_solve_result_series_degree(original),
                     "series degree survives");
    string_t *before = equ_to_text(de_solve_result_at(original, 0), style_LATEX);
    string_t *after = equ_to_text(de_solve_result_at(result, 0), style_LATEX);
    TEST_ASSERT_STR_EQ(string_c_str(after), string_c_str(before));
    TEST_ASSERT_TRUE(after && strstr(string_c_str(after), "O"), "specialised remainder notation survives");
    string_free(after);
    string_free(before);
    for (size_t i = 0; i <= de_solve_result_series_degree(result); ++i) {
        number_t a = expr_eval(de_solve_result_series_coefficient(result, i));
        number_t b = expr_eval(de_solve_result_series_coefficient(original, i));
        bool equal = num_eq(a, b);
        num_destroy(&a);
        num_destroy(&b);
        TEST_ASSERT_TRUE(equal, "series metadata agrees with the solution snapshot");
    }
    de_solve_result_free(result);
    de_solve_result_free(original);
    de_free(de);
}

int tests_main(void)
{
    TEST_RUN_CASE(test_context_binding_snapshots, NULL);
    TEST_RUN_CASE(test_context_free_and_assumed, NULL);
    TEST_RUN_CASE(test_context_conditions, NULL);
    TEST_RUN_CASE(test_context_invalid_updates, NULL);
    TEST_RUN_CASE(test_context_symbol_identity, NULL);
    TEST_RUN_CASE(test_context_scopes_and_conflicts, NULL);
    TEST_RUN_CASE(test_context_many_symbols, NULL);
    TEST_RUN_CASE(test_context_equation_results, NULL);
    TEST_RUN_CASE(test_context_matrix_results, NULL);
    TEST_RUN_CASE(test_context_differential_results, NULL);
    TEST_RUN_CASE(test_context_consumer_boundaries, NULL);
    TEST_RUN_CASE(test_context_differential_metadata, NULL);
    test_context_solver_cases();
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_expr_context_binding);
    return TEST_EXIT_CODE();
}
