#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

bool de_pde_find_first_derivatives(const expr_t *expr, const expr_t *x, const expr_t *y, const expr_t **dependent_out,
                                   const expr_t **dx_out, const expr_t **dy_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent;
        const expr_t *wrt;
        const expr_t **slot;

        if (expr_formal_derivative_order(expr) != 1u)
            return false;
        dependent = expr_formal_derivative_dependent(expr);
        wrt = expr_formal_derivative_wrt_at(expr, 0u);
        if (!dependent || !wrt)
            return false;
        if (expr_struct_eq(wrt, x))
            slot = dx_out;
        else if (expr_struct_eq(wrt, y))
            slot = dy_out;
        else
            return false;
        if ((*dependent_out && !expr_struct_eq(*dependent_out, dependent)) || (*slot && !expr_struct_eq(*slot, expr)))
            return false;
        *dependent_out = dependent;
        *slot = expr;
        return true;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_pde_find_first_derivatives(left, x, y, dependent_out, dx_out, dy_out) &&
           de_pde_find_first_derivatives(right, x, y, dependent_out, dx_out, dy_out);
}

bool de_pde_find_first_derivatives_n(const expr_t *expr, size_t independent_count, expr_t *const *independents,
                                     const expr_t **dependent_out, const expr_t **derivatives_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent;
        const expr_t *wrt;
        size_t index = independent_count;

        if (expr_formal_derivative_order(expr) != 1u)
            return false;
        dependent = expr_formal_derivative_dependent(expr);
        wrt = expr_formal_derivative_wrt_at(expr, 0u);
        if (!dependent || !wrt)
            return false;
        for (size_t i = 0u; i < independent_count; ++i) {
            if (expr_struct_eq(wrt, independents[i])) {
                index = i;
                break;
            }
        }
        if (index == independent_count || (*dependent_out && !expr_struct_eq(*dependent_out, dependent)) ||
            (derivatives_out[index] && !expr_struct_eq(derivatives_out[index], expr)))
            return false;
        *dependent_out = dependent;
        derivatives_out[index] = expr;
        return true;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_pde_find_first_derivatives_n(left, independent_count, independents, dependent_out, derivatives_out) &&
           de_pde_find_first_derivatives_n(right, independent_count, independents, dependent_out, derivatives_out);
}

const expr_t *de_pde_find_named_coordinate(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
                                           const char *name)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const char *symbol_name;
    const expr_t *found;

    if (!expr)
        return NULL;
    symbol_name = expr_symbol_name(expr);
    if (expr != independent && expr != dependent && symbol_name && strcmp(symbol_name, name) == 0)
        return expr;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    found = de_pde_find_named_coordinate(left, independent, dependent, name);
    return found ? found : de_pde_find_named_coordinate(right, independent, dependent, name);
}

bool de_pde_same_symbolic_form(const expr_t *left, const expr_t *right)
{
    string_t *left_text;
    string_t *right_text;
    bool same;

    if (expr_struct_eq(left, right))
        return true;
    left_text = expr_to_text(left, style_EXPRESSION);
    right_text = expr_to_text(right, style_EXPRESSION);
    same = left_text && right_text && strcmp(string_c_str(left_text), string_c_str(right_text)) == 0;
    string_free(right_text);
    string_free(left_text);
    return same;
}

equation_t *de_pde_solution_equation(const expr_t *dependent, const expr_t *right)
{
    const char *name = expr_symbol_name(dependent);
    expr_t *left = name ? expr_new_named_var(NUM_NAN, name) : expr_clone(dependent);
    equation_t *solution = left && right ? equ_new(left, right) : NULL;

    expr_free(left);
    return solution;
}
