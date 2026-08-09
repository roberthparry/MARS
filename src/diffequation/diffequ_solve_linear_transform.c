#include <stdbool.h>
#include <stddef.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

#define DE_LINEAR_FORM_LIMIT 32u

typedef struct {
    const expr_t *expression;
    expr_t *x_coefficient;
    expr_t *y_coefficient;
} de_linear_form_t;

typedef struct {
    de_linear_form_t forms[DE_LINEAR_FORM_LIMIT];
    size_t count;
} de_linear_forms_t;

static void de_linear_form_clear(de_linear_form_t *form)
{
    if (!form)
        return;
    expr_free(form->y_coefficient);
    expr_free(form->x_coefficient);
    form->y_coefficient = NULL;
    form->x_coefficient = NULL;
    form->expression = NULL;
}

static void de_linear_forms_clear(de_linear_forms_t *forms)
{
    if (!forms)
        return;
    for (size_t i = 0u; i < forms->count; ++i)
        de_linear_form_clear(&forms->forms[i]);
    forms->count = 0u;
}

static bool de_extract_linear_form(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
                                   de_linear_form_t *form)
{
    expr_t *remainder = NULL;
    expr_t *constant = NULL;
    bool valid;

    form->expression = NULL;
    form->x_coefficient = NULL;
    form->y_coefficient = NULL;
    if (!de_linear_decompose(expr, independent, &form->x_coefficient, &remainder) ||
        !de_linear_decompose(remainder, dependent, &form->y_coefficient, &constant))
        goto fail;

    valid = expr_is_exact_zero(constant) && !de_expr_uses(form->x_coefficient, independent) &&
            !de_expr_uses(form->x_coefficient, dependent) && !de_expr_uses(form->y_coefficient, independent) &&
            !de_expr_uses(form->y_coefficient, dependent) &&
            (!expr_is_exact_zero(form->x_coefficient) || !expr_is_exact_zero(form->y_coefficient));
    expr_free(constant);
    expr_free(remainder);
    if (!valid)
        goto fail_without_remainder;

    form->expression = expr;
    return true;

fail:
    expr_free(constant);
    expr_free(remainder);
fail_without_remainder:
    de_linear_form_clear(form);
    return false;
}

static bool de_linear_forms_contains(const de_linear_forms_t *forms, const expr_t *expression)
{
    for (size_t i = 0u; i < forms->count; ++i) {
        if (expr_struct_eq(forms->forms[i].expression, expression))
            return true;
    }
    return false;
}

static void de_collect_linear_forms(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
                                    de_linear_forms_t *forms)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || forms->count >= DE_LINEAR_FORM_LIMIT || expr_is_formal_derivative(expr))
        return;

    if (!de_linear_forms_contains(forms, expr)) {
        de_linear_form_t form = {0};

        if (de_extract_linear_form(expr, independent, dependent, &form))
            forms->forms[forms->count++] = form;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return;
    de_collect_linear_forms(left, independent, dependent, forms);
    de_collect_linear_forms(right, independent, dependent, forms);
}

static expr_t *de_linear_determinant(const de_linear_form_t *transformed_dependent,
                                     const de_linear_form_t *transformed_independent)
{
    expr_t *first = expr_mul_simplify_owned(expr_clone(transformed_independent->x_coefficient),
                                            expr_clone(transformed_dependent->y_coefficient));
    expr_t *second = expr_mul_simplify_owned(expr_clone(transformed_independent->y_coefficient),
                                             expr_clone(transformed_dependent->x_coefficient));

    return expr_sub_simplify_owned(first, second);
}

static expr_t *de_inverse_x(const de_linear_form_t *transformed_dependent,
                            const de_linear_form_t *transformed_independent, const expr_t *new_independent,
                            const expr_t *new_dependent, const expr_t *determinant)
{
    expr_t *x_coefficient =
        expr_div_simplify_owned(expr_clone(transformed_dependent->y_coefficient), expr_clone(determinant));
    expr_t *y_coefficient = expr_div_simplify_owned(
        de_simplify_unary_owned(expr_clone(transformed_independent->y_coefficient), expr_neg), expr_clone(determinant));
    expr_t *x_term = expr_mul_simplify_owned(x_coefficient, expr_clone(new_independent));
    expr_t *y_term = expr_mul_simplify_owned(y_coefficient, expr_clone(new_dependent));

    return expr_add_simplify_owned(x_term, y_term);
}

static expr_t *de_inverse_y(const de_linear_form_t *transformed_dependent,
                            const de_linear_form_t *transformed_independent, const expr_t *new_independent,
                            const expr_t *new_dependent, const expr_t *determinant)
{
    expr_t *x_coefficient = expr_div_simplify_owned(
        de_simplify_unary_owned(expr_clone(transformed_dependent->x_coefficient), expr_neg), expr_clone(determinant));
    expr_t *y_coefficient =
        expr_div_simplify_owned(expr_clone(transformed_independent->x_coefficient), expr_clone(determinant));
    expr_t *x_term = expr_mul_simplify_owned(x_coefficient, expr_clone(new_independent));
    expr_t *y_term = expr_mul_simplify_owned(y_coefficient, expr_clone(new_dependent));

    return expr_add_simplify_owned(x_term, y_term);
}

static expr_t *de_transformed_derivative(const expr_t *transformed_rhs, const de_linear_form_t *transformed_dependent,
                                         const de_linear_form_t *transformed_independent)
{
    const expr_t *rhs_numerator = NULL;
    const expr_t *rhs_denominator = NULL;
    expr_t *dependent_base = NULL;
    expr_t *dependent_scaled = NULL;
    expr_t *numerator = NULL;
    expr_t *independent_base = NULL;
    expr_t *independent_scaled = NULL;
    expr_t *denominator = NULL;
    expr_t *result = NULL;

    if (!expr_match_div_expr(transformed_rhs, &rhs_numerator, &rhs_denominator)) {
        rhs_numerator = transformed_rhs;
        rhs_denominator = NULL;
    }

    dependent_base = rhs_denominator ? expr_mul_simplify_owned(expr_clone(transformed_dependent->x_coefficient),
                                                               expr_clone(rhs_denominator))
                                     : expr_clone(transformed_dependent->x_coefficient);
    dependent_scaled =
        expr_mul_simplify_owned(expr_clone(transformed_dependent->y_coefficient), expr_clone(rhs_numerator));
    numerator = expr_add_simplify_owned(dependent_base, dependent_scaled);
    dependent_base = NULL;
    dependent_scaled = NULL;

    independent_base = rhs_denominator ? expr_mul_simplify_owned(expr_clone(transformed_independent->x_coefficient),
                                                                 expr_clone(rhs_denominator))
                                       : expr_clone(transformed_independent->x_coefficient);
    independent_scaled =
        expr_mul_simplify_owned(expr_clone(transformed_independent->y_coefficient), expr_clone(rhs_numerator));
    denominator = expr_add_simplify_owned(independent_base, independent_scaled);
    independent_base = NULL;
    independent_scaled = NULL;
    result = numerator && denominator ? expr_div_simplify_owned(numerator, denominator) : NULL;
    if (numerator && denominator) {
        numerator = NULL;
        denominator = NULL;
    }

    expr_free(denominator);
    expr_free(independent_scaled);
    expr_free(independent_base);
    expr_free(numerator);
    expr_free(dependent_scaled);
    expr_free(dependent_base);
    return result;
}

static expr_t *de_linear_form_at_condition(const de_linear_form_t *form, const expr_t *point, const expr_t *value)
{
    expr_t *x_term = expr_mul_simplify_owned(expr_clone(form->x_coefficient), expr_clone(point));
    expr_t *y_term = expr_mul_simplify_owned(expr_clone(form->y_coefficient), expr_clone(value));

    return expr_add_simplify_owned(x_term, y_term);
}

static expr_t *de_transformed_constant(const diffequ_t *de, const expr_t *dependent,
                                       const de_linear_form_t *transformed_dependent,
                                       const de_linear_form_t *transformed_independent, const expr_t *new_dependent,
                                       const expr_t *new_independent, const expr_t *dependent_integral,
                                       const expr_t *independent_integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *dependent_value = NULL;
    expr_t *independent_value = NULL;
    expr_t *dependent_at_condition = NULL;
    expr_t *independent_at_condition = NULL;
    expr_t *constant = NULL;

    if (!de_find_initial_condition(de, dependent, &point, &value))
        return de_arbitrary_constant();

    dependent_value = de_linear_form_at_condition(transformed_dependent, point, value);
    independent_value = de_linear_form_at_condition(transformed_independent, point, value);
    dependent_at_condition =
        dependent_value ? expr_substitute(dependent_integral, new_dependent, dependent_value) : NULL;
    independent_at_condition =
        independent_value ? expr_substitute(independent_integral, new_independent, independent_value) : NULL;
    dependent_at_condition = expr_simplify_owned(dependent_at_condition);
    independent_at_condition = expr_simplify_owned(independent_at_condition);
    constant = dependent_at_condition && independent_at_condition
                   ? expr_sub_simplify_owned(dependent_at_condition, independent_at_condition)
                   : NULL;
    if (dependent_at_condition && independent_at_condition) {
        dependent_at_condition = NULL;
        independent_at_condition = NULL;
    }

    expr_free(independent_at_condition);
    expr_free(dependent_at_condition);
    expr_free(independent_value);
    expr_free(dependent_value);
    return constant;
}

static de_attempt_t de_try_linear_transformation(const diffequ_t *de, const expr_t *independent,
                                                 const expr_t *dependent, const expr_t *derivative_right,
                                                 const de_linear_form_t *transformed_dependent,
                                                 const de_linear_form_t *transformed_independent,
                                                 equation_t **solution_out)
{
    expr_t *determinant = NULL;
    expr_t *new_independent = NULL;
    expr_t *new_dependent = NULL;
    expr_t *x_replacement = NULL;
    expr_t *y_replacement = NULL;
    expr_t *after_x = NULL;
    expr_t *transformed_rhs_raw = NULL;
    expr_t *transformed_rhs = NULL;
    expr_t *new_derivative = NULL;
    expr_t *independent_factor = NULL;
    expr_t *dependent_factor = NULL;
    expr_t *reciprocal = NULL;
    expr_t *dependent_integral = NULL;
    expr_t *independent_integral = NULL;
    expr_t *left = NULL;
    expr_t *right_base = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    determinant = de_linear_determinant(transformed_dependent, transformed_independent);
    if (!determinant || expr_is_exact_zero(determinant))
        goto cleanup;

    new_independent = expr_new_named_var(NUM_NAN, "__de_X");
    new_dependent = expr_new_named_var(NUM_NAN, "__de_Y");
    x_replacement =
        de_inverse_x(transformed_dependent, transformed_independent, new_independent, new_dependent, determinant);
    y_replacement =
        de_inverse_y(transformed_dependent, transformed_independent, new_independent, new_dependent, determinant);
    after_x = x_replacement ? expr_substitute(derivative_right, independent, x_replacement) : NULL;
    transformed_rhs_raw = after_x && y_replacement ? expr_substitute(after_x, dependent, y_replacement) : NULL;
    transformed_rhs = transformed_rhs_raw ? expr_simplify(transformed_rhs_raw) : NULL;
    if (!transformed_rhs || de_expr_uses(transformed_rhs, independent) || de_expr_uses(transformed_rhs, dependent))
        goto cleanup;

    new_derivative = de_transformed_derivative(transformed_rhs, transformed_dependent, transformed_independent);
    if (new_derivative) {
        expr_t *display_derivative = expr_display_simplified(new_derivative);

        if (display_derivative) {
            expr_free(new_derivative);
            new_derivative = display_derivative;
        }
    }
    if (!new_derivative ||
        !de_split_separable(new_derivative, new_independent, new_dependent, &independent_factor, &dependent_factor))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    reciprocal = expr_div_simplify_owned(expr_const_one(), dependent_factor);
    dependent_factor = NULL;
    dependent_integral = reciprocal ? de_integrate_or_formal(reciprocal, new_dependent) : NULL;
    independent_integral = independent_factor ? de_integrate_or_formal(independent_factor, new_independent) : NULL;
    expr_free(independent_factor);
    independent_factor = NULL;
    left = dependent_integral ? expr_substitute(dependent_integral, new_dependent, transformed_dependent->expression)
                              : NULL;
    right_base = independent_integral
                     ? expr_substitute(independent_integral, new_independent, transformed_independent->expression)
                     : NULL;
    constant = dependent_integral && independent_integral
                   ? de_transformed_constant(de, dependent, transformed_dependent, transformed_independent,
                                             new_dependent, new_independent, dependent_integral, independent_integral)
                   : NULL;
    right = right_base && constant ? expr_add_simplify_owned(right_base, constant) : NULL;
    if (right_base && constant) {
        right_base = NULL;
        constant = NULL;
    }
    if (!left || !right)
        goto cleanup;

    *solution_out = equ_new(left, right);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(right);
    expr_free(constant);
    expr_free(right_base);
    expr_free(left);
    expr_free(independent_integral);
    expr_free(dependent_integral);
    expr_free(reciprocal);
    expr_free(dependent_factor);
    expr_free(independent_factor);
    expr_free(new_derivative);
    expr_free(transformed_rhs);
    expr_free(transformed_rhs_raw);
    expr_free(after_x);
    expr_free(y_replacement);
    expr_free(x_replacement);
    expr_free(new_dependent);
    expr_free(new_independent);
    expr_free(determinant);
    return attempt;
}

de_attempt_t de_attempt_linear_transformation(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                              const expr_t *derivative_right, equation_t **solution_out)
{
    de_linear_forms_t forms = {0};
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    de_collect_linear_forms(derivative_right, independent, dependent, &forms);
    for (size_t i = 0u; i < forms.count && attempt == DE_ATTEMPT_NOT_MATCHED; ++i) {
        for (size_t j = 0u; j < forms.count && attempt == DE_ATTEMPT_NOT_MATCHED; ++j) {
            if (i == j)
                continue;
            attempt = de_try_linear_transformation(de, independent, dependent, derivative_right, &forms.forms[i],
                                                   &forms.forms[j], solution_out);
        }
    }

    de_linear_forms_clear(&forms);
    return attempt;
}
