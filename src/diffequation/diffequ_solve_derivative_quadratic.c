#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_derivative_quadratic_independent_symbol(
    const diffequ_t *de)
{
    const char *text;
    const char *equals;
    const char *end;
    char *name;
    expr_t *symbol;
    size_t length;

    if (!de || !de->independent_text)
        return NULL;
    text = string_c_str(de->independent_text);
    equals = strchr(text, '=');
    end = equals ? equals : text + strlen(text);
    while (end > text && end[-1] == ' ')
        end--;
    while (*text == ' ')
        text++;
    length = (size_t)(end - text);
    if (length == 0u)
        return NULL;

    name = malloc(length + 1u);
    if (!name)
        return NULL;
    memcpy(name, text, length);
    name[length] = '\0';
    symbol = expr_new_named_var(NUM_NAN, name);
    free(name);
    return symbol;
}

static expr_t *de_derivative_quadratic_coefficient(
    const expr_t *coefficient,
    const expr_t *dependent_coefficient)
{
    expr_t *quotient = expr_div_simplify_owned(
        expr_clone(coefficient),
        expr_clone(dependent_coefficient));

    return de_simplify_unary_owned(quotient, expr_neg);
}

static expr_t *de_derivative_quadratic_root(
    const expr_t *quadratic,
    const expr_t *linear,
    const expr_t *constant,
    bool positive)
{
    expr_t *four_a_c = expr_mul_simplify_owned(
        expr_mul_long(quadratic, 4L),
        expr_clone(constant));
    expr_t *discriminant = four_a_c
        ? expr_sub_simplify_owned(
              expr_pow_long(linear, 2L), four_a_c)
        : NULL;
    expr_t *root = discriminant ? expr_sqrt(discriminant) : NULL;
    expr_t *negative_linear =
        de_simplify_unary_owned(expr_clone(linear), expr_neg);
    expr_t *numerator = positive
        ? expr_add_simplify_owned(negative_linear, root)
        : expr_sub_simplify_owned(negative_linear, root);
    expr_t *denominator = expr_mul_long(quadratic, 2L);

    return numerator && denominator
        ? expr_div_simplify_owned(numerator, denominator)
        : NULL;
}

static expr_t *de_derivative_quadratic_implicit_rhs(
    const expr_t *quadratic,
    const expr_t *linear,
    const expr_t *root)
{
    expr_t *linear_root = expr_mul_simplify_owned(
        expr_mul_long(quadratic, 2L), expr_clone(root));
    expr_t *absolute = expr_abs(root);
    expr_t *logarithm = absolute ? expr_log(absolute) : NULL;
    expr_t *log_term = logarithm
        ? expr_mul_simplify_owned(
              expr_clone(linear), logarithm)
        : NULL;
    expr_t *rhs = linear_root && log_term
        ? expr_add_simplify_owned(linear_root, log_term)
        : NULL;

    if (rhs) {
        expr_t *constant = de_arbitrary_constant();
        expr_t *sum = constant ? expr_add(rhs, constant) : NULL;

        expr_free(constant);
        expr_free(rhs);
        return sum;
    }
    return NULL;
}

de_attempt_t de_attempt_derivative_quadratic(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *quadratic = NULL;
    expr_t *dependent_coefficient = NULL;
    expr_t *offset = NULL;
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *d = NULL;
    expr_t *positive_root = NULL;
    expr_t *negative_root = NULL;
    expr_t *positive_rhs = NULL;
    expr_t *negative_rhs = NULL;
    expr_t *independent_symbol = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de || !independent || !dependent || !first_derivative ||
        !residual || !solutions_out || !solution_count_out ||
        de->condition_count != 0u)
        return DE_ATTEMPT_NOT_MATCHED;
    *solution_count_out = 0u;

    if (!equ_match_symbolic_quadratic_expr(
            residual,
            first_derivative,
            &constant,
            &linear,
            &quadratic) ||
        !quadratic ||
        expr_is_exact_zero(quadratic) ||
        de_expr_uses(quadratic, independent) ||
        de_expr_uses(quadratic, dependent) ||
        de_expr_uses(linear, independent) ||
        de_expr_uses(linear, dependent) ||
        !de_linear_decompose(
            constant,
            dependent,
            &dependent_coefficient,
            &offset) ||
        expr_is_exact_zero(dependent_coefficient) ||
        de_expr_uses(dependent_coefficient, independent) ||
        de_expr_uses(dependent_coefficient, dependent) ||
        de_expr_uses(offset, independent) ||
        de_expr_uses(offset, dependent))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    a = de_derivative_quadratic_coefficient(
        quadratic, dependent_coefficient);
    b = de_derivative_quadratic_coefficient(
        linear, dependent_coefficient);
    d = de_derivative_quadratic_coefficient(
        offset, dependent_coefficient);
    if (!a || !b || !d || expr_is_exact_zero(a))
        goto cleanup;

    positive_root = de_derivative_quadratic_root(
        quadratic, linear, constant, true);
    negative_root = de_derivative_quadratic_root(
        quadratic, linear, constant, false);
    positive_rhs = positive_root
        ? de_derivative_quadratic_implicit_rhs(
              a, b, positive_root)
        : NULL;
    negative_rhs = negative_root
        ? de_derivative_quadratic_implicit_rhs(
              a, b, negative_root)
        : NULL;
    if (!positive_rhs || !negative_rhs)
        goto cleanup;

    independent_symbol =
        de_derivative_quadratic_independent_symbol(de);
    if (!independent_symbol)
        goto cleanup;
    solutions_out[0] = equ_new(independent_symbol, positive_rhs);
    solutions_out[1] = equ_new(independent_symbol, negative_rhs);
    solutions_out[2] = equ_new(dependent, d);
    if (!solutions_out[0] ||
        !solutions_out[1] ||
        !solutions_out[2]) {
        for (size_t i = 0u; i < 3u; ++i) {
            equ_free(solutions_out[i]);
            solutions_out[i] = NULL;
        }
        goto cleanup;
    }
    *solution_count_out = 3u;
    attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(independent_symbol);
    expr_free(negative_rhs);
    expr_free(positive_rhs);
    expr_free(negative_root);
    expr_free(positive_root);
    expr_free(d);
    expr_free(b);
    expr_free(a);
    expr_free(offset);
    expr_free(dependent_coefficient);
    expr_free(quadratic);
    expr_free(linear);
    expr_free(constant);
    return attempt;
}
