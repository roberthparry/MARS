#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

/* Keep the documented Taylor normal-form bound independent of the shared collector's capacity. */
static bool de_series_polynomial(const de_lie_t *normal, const expr_t *expr, size_t coordinate)
{
    if (!expr)
        return false;
    if (coordinate == 3u) {
        number_t value = num_new();
        bool valid = !expr_match_const_value(expr, &value) || num_is_finite(value);
        num_destroy(&value);
        return valid;
    }
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    bool valid = equ_collect_symbolic_polynomial_alloc(expr, de_lie_coordinate(normal, coordinate),
                                                        &coefficients, &degree);
    valid = valid && degree <= 15u;
    for (size_t i = 0u; valid && i <= degree; ++i)
        valid = de_series_polynomial(normal, coefficients[i], coordinate + 1u);
    if (coefficients) {
        for (size_t i = 0u; i <= degree; ++i)
            expr_free(coefficients[i]);
    }
    free(coefficients);
    return valid;
}

static bool de_series_name_used(const expr_t *expr, const char *name)
{
    const char *symbol = expr_symbol_name(expr);
    const expr_t *left = NULL, *right = NULL;
    return (symbol && strcmp(symbol, name) == 0) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_series_name_used(left, name) || de_series_name_used(right, name)));
}

static expr_t *de_series_constant(const diffequ_t *de, size_t *suffix)
{
    for (;;) {
        char name[64];
        snprintf(name, sizeof(name), "C_%zu", ++*suffix);
        expr_t *candidate = expr_new_named_const(NUM_NAN, name);
        if (!candidate)
            return NULL;
        const char *canonical = expr_symbol_name(candidate);
        bool used = de_series_name_used(equ_lhs(de->equation), canonical) ||
                    de_series_name_used(equ_rhs(de->equation), canonical);
        /* There are at most two accepted initial conditions. */
        for (size_t i = 0u; !used && i < de->condition_count; ++i)
            used = de_series_name_used(equ_rhs(de->conditions[i]), canonical) ||
                   de_series_name_used(de->condition_points[i][0], canonical);
        if (!used)
            return candidate;
        expr_free(candidate);
    }
}

static void de_series_clear(expr_t **coefficients, size_t degree)
{
    if (coefficients) {
        for (size_t i = 0u; i <= degree; ++i)
            expr_free(coefficients[i]);
    }
    free(coefficients);
}

/* Collect in the initial-data symbols, so recurrence trees do not grow between orders. */
static expr_t *de_series_collect(const expr_t *expr, expr_t *const symbols[3], size_t coordinate)
{
    if (coordinate == 3u)
        return expr_simplify(expr);
    if (!symbols[coordinate])
        return de_series_collect(expr, symbols, coordinate + 1u);
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    if (!equ_collect_symbolic_polynomial_alloc(expr, symbols[coordinate], &coefficients, &degree))
        return expr_simplify(expr);
    expr_t *out = expr_const_zero();
    expr_t *power = expr_const_one();
    for (size_t n = 0u; out && power && n <= degree; ++n) {
        expr_t *coefficient = de_series_collect(coefficients[n], symbols, coordinate + 1u);
        expr_t *term = coefficient ? expr_mul_simplify_owned(coefficient, expr_clone(power)) : NULL;
        expr_t *next = !term ? NULL : expr_is_exact_zero(term) ? expr_clone(out) :
                       expr_is_exact_zero(out) ? expr_clone(term) : expr_add(out, term);
        expr_free(term);
        expr_free(out);
        out = next;
        power = expr_mul_simplify_owned(power, expr_clone(symbols[coordinate]));
    }
    if (!power) {
        expr_free(out);
        out = NULL;
    }
    expr_free(power);
    de_series_clear(coefficients, degree);
    return out;
}

static expr_t *de_series_collect_initial_data(expr_t *owned, expr_t *const initial[3])
{
    expr_t *symbols[3] = {NULL, NULL, NULL};
    for (size_t i = 0u; owned && i < 3u; ++i) {
        const char *name = expr_symbol_name(initial[i]);
        if (!name || (!expr_is_variable(initial[i]) && !expr_is_named_const(initial[i])))
            continue;
        symbols[i] = expr_new_named_var(NUM_NAN, name);
        expr_t *next = symbols[i] ? expr_substitute(owned, symbols[i], symbols[i]) : NULL;
        expr_free(owned);
        owned = next;
    }
    expr_t *out = owned ? de_series_collect(owned, symbols, 0u) : NULL;
    expr_free(owned);
    for (size_t i = 0u; i < 3u; ++i) {
        if (out && symbols[i]) {
            expr_t *next = expr_substitute(out, symbols[i], initial[i]);
            expr_free(out);
            out = next;
        }
        expr_free(symbols[i]);
    }
    return out;
}

/* At most nine coefficients: discard higher powers during every convolution. */
static expr_t **de_series_product(expr_t *const *left, expr_t *const *right, size_t degree)
{
    expr_t **out = calloc(degree + 1u, sizeof(*out));
    if (!out)
        return NULL;
    for (size_t n = 0u; n <= degree; ++n) {
        out[n] = expr_const_zero();
        for (size_t k = 0u; out[n] && k <= n; ++k) {
            if (!expr_is_exact_zero(left[k]) && !expr_is_exact_zero(right[n-k]))
                out[n] = expr_add_simplify_owned(out[n], expr_mul(left[k], right[n-k]));
        }
        if (!out[n]) {
            de_series_clear(out, degree);
            return NULL;
        }
    }
    return out;
}

static expr_t **de_series_compose(const de_lie_t *normal, const expr_t *expr, size_t coordinate,
                                   expr_t *const inputs[3][9], size_t degree)
{
    expr_t **out = calloc(degree + 1u, sizeof(*out));
    if (!out)
        return NULL;
    for (size_t i = 0u; i <= degree; ++i) {
        out[i] = coordinate == 3u && i == 0u ? expr_clone(expr) : expr_const_zero();
        if (!out[i]) {
            de_series_clear(out, degree);
            return NULL;
        }
    }
    if (coordinate == 3u)
        return out;
    expr_t **coefficients = NULL;
    size_t highest = 0u;
    if (!equ_collect_symbolic_polynomial_alloc(expr, de_lie_coordinate(normal, coordinate), &coefficients, &highest)) {
        de_series_clear(out, degree);
        return NULL;
    }
    for (size_t k = highest + 1u; out && k-- > 0u;) {
        expr_t **next = de_series_product(out, inputs[coordinate], degree);
        expr_t **term = next ? de_series_compose(normal, coefficients[k], coordinate + 1u, inputs, degree) : NULL;
        de_series_clear(out, degree);
        out = next;
        bool valid = out && term;
        for (size_t i = 0u; valid && i <= degree; ++i) {
            out[i] = expr_add_simplify_owned(out[i], expr_clone(term[i]));
            valid = out[i] != NULL;
        }
        de_series_clear(term, degree);
        if (!valid) {
            de_series_clear(out, degree);
            out = NULL;
        }
    }
    de_series_clear(coefficients, highest);
    return out;
}

static bool de_series_steps(const diffequ_t *de, diffequ_solve_result_t *result)
{
    char *centre = expr_to_string(result->series_centre, style_UNBOUND);
    char *centre_TeX = expr_to_TeX_body(result->series_centre);
    string_t *steps = centre ? string_sprintf(
        "Local Taylor series about %s, through degree %zu.\n"
        "Here x0 is the expansion point and N is the highest retained power.\n"
        "Using generic coordinates, write y'' = f(x,y,p), with p = y' (the slope).\n"
        "Along a solution, dy/dx = p and dp/dx = f.\n"
        "D is total differentiation along that solution: it applies the chain rule, not a new unknown.\n"
        "For any g(x,y,p), Dg = partial_x g + p*partial_y g + f*partial_p g\n"
        "                   = d/dx g(x,y(x),p(x)).\n"
        "The partial derivatives hold the other coordinates fixed.\n"
        "D^k means applying D k times: y'' = f, y''' = Df, y'''' = D^2 f.\n"
        "Taylor coefficients are derivatives at x0 divided by factorials:\n"
        "c0 = y(x0), c1 = y'(x0), cn = y^(n)(x0)/n! = (D^(n-2) f)(x0,c0,c1)/n! for n >= 2.\n"
        "The implementation computes the same coefficients by matching powers in the ODE,\n"
        "using truncated polynomial convolution.\n"
        "Missing initial values remain arbitrary constants.\n"
        "Polynomial f guarantees local convergence for finite initial data; no radius or numerical error bound is supplied.\n"
        "O((x-x0)^(%zu)) denotes omitted higher powers; this is a local truncated series, not a closed form.\n",
        centre, result->series_degree, result->series_degree + 1u) : NULL;
    string_t *steps_TeX = centre_TeX ? string_sprintf(
        "\\begin{aligned}&\\text{Local Taylor series: }x_0=%s,\\quad N=%zu"
        "\\\\&\\text{Expand about }x_0\\text{, retaining powers through degree }N\\text{.}"
        "\\\\[0.5em]&\\text{Using generic coordinates: }y''=f(x,y,p),\\quad p=y'\\text{ (the slope).}"
        "\\\\&\\frac{dy}{dx}=p,\\quad\\frac{dp}{dx}=f\\quad\\text{along a solution.}"
        "\\\\[0.5em]&D\\text{ is total differentiation along that solution (the chain rule).}"
        "\\\\&\\text{For any }g(x,y,p):\\quad Dg=\\partial_x g+p\\partial_y g+f\\partial_p g"
        "\\\\&\\hphantom{\\text{For any }g(x,y,p):\\quad Dg}=\\frac{d}{dx}g(x,y(x),p(x))"
        "\\\\&\\text{Partial derivatives hold the other coordinates fixed.}"
        "\\\\&D^k\\text{ means applying }D\\text{ }k\\text{ times: }y'''=Df,\\quad y^{(4)}=D^2f."
        "\\\\[0.5em]&\\text{Taylor coefficients are derivatives at }x_0\\text{ divided by factorials:}"
        "\\\\&c_0=y(x_0),\\quad c_1=y'(x_0)"
        "\\\\&c_n=\\frac{y^{(n)}(x_0)}{n!}=\\frac{(D^{n-2}f)(x_0,c_0,c_1)}{n!}\\quad(n\\ge2)"
        "\\\\&\\text{Computed by matching powers in the ODE.}"
        "\\\\&y=\\sum_{n=0}^{N}c_n(x-x_0)^n+O((x-x_0)^{N+1})"
        "\\\\&\\text{Missing initial values remain arbitrary constants.}"
        "\\\\&O((x-x_0)^{N+1})\\text{ denotes omitted higher powers, not a closed form.}"
        "\\\\[0.5em]&\\text{Locally convergent for finite initial data.}"
        "\\\\&\\text{Convergence radius and numerical error bound not supplied.}\\end{aligned}",
        centre_TeX, result->series_degree) : NULL;
    /* Preserve the useful diagnostic independently of the series construction. */
    diffequ_solve_result_t *analysis = steps ? de_lie_unsolved_analysis(de, true) : NULL;
    if (analysis && de_solve_result_steps(analysis))
        string_append_format(steps, "\n%s", de_solve_result_steps(analysis));
    bool ok = steps && steps_TeX && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
              de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) == 0;
    de_solve_result_free(analysis);
    string_free(steps_TeX);
    string_free(steps);
    free(centre_TeX);
    free(centre);
    return ok;
}

/* Match Taylor coefficients using truncated polynomial arithmetic for the parsed normal form. */
diffequ_solve_result_t *de_solve_series(const diffequ_t *de, size_t degree, unsigned int options)
{
    if (!de || degree < 2u || degree > 8u)
        return de_solve_result_new(DE_SOLVE_STATUS_INVALID, DE_SOLVER_NONE,
                                    "Taylor series requires a problem and a degree from 2 through 8");
    const char *reason = "Taylor fallback requires a polynomial second-order normal form "
                         "with a finite non-zero numeric leading coefficient";
    de_solve_status_t status = DE_SOLVE_STATUS_UNSUPPORTED;
    de_lie_t *normal = de_lie_new(de);
    diffequ_solve_result_t *result = NULL;
    expr_t *residual = NULL, *leading = NULL, *remainder = NULL;
    expr_t *initial[3] = {NULL, NULL, NULL};
    expr_t *polynomial = NULL, *shift = NULL, *power = NULL, *tail = NULL;
    equation_t *solution = NULL;
    if (!normal || de->partial_derivative_input || !de_series_polynomial(normal, de_lie_rhs(normal), 0u))
        goto cleanup;
    const expr_t *dependent = NULL, *first = NULL, *second = NULL;
    size_t highest = 0u;
    residual = equ_residual(de->equation);
    number_t numeric_leading = num_new();
    bool regular = residual && de_find_derivatives(residual, de_lie_coordinate(normal, 0u), &dependent,
                                                    &first, &second, &highest) && second &&
                   de_linear_decompose(residual, second, &leading, &remainder) &&
                   expr_match_const_value(leading, &numeric_leading) && num_is_finite(numeric_leading) &&
                   !num_is_zero(numeric_leading);
    num_destroy(&numeric_leading);
    if (!regular)
        goto cleanup;
    const expr_t *point = NULL, *value = NULL, *slope_point = NULL, *slope = NULL;
    bool has_value = de_find_initial_condition(de, dependent, &point, &value);
    bool has_slope = de_find_derivative_condition(de, dependent, de_lie_coordinate(normal, 0u), 1u,
                                                 &slope_point, &slope);
    reason = "Taylor series accepts only value and slope data at one common point";
    if (de->condition_count != (size_t)has_value + (size_t)has_slope ||
        (has_value && has_slope && !expr_struct_eq(point, slope_point)))
        goto cleanup;
    size_t suffix = 0u;
    initial[0] = has_value ? expr_clone(point) : has_slope ? expr_clone(slope_point) : expr_const_zero();
    initial[1] = has_value ? expr_clone(value) : de_series_constant(de, &suffix);
    initial[2] = has_slope ? expr_clone(slope) : de_series_constant(de, &suffix);
    for (size_t i = 0u; i < 3u; ++i) {
        if (!initial[i])
            goto failed;
        for (size_t j = 0u; j < 3u; ++j) {
            if (de_expr_uses(initial[i], de_lie_coordinate(normal, j)))
                goto cleanup;
        }
        number_t number = num_new();
        bool finite = !expr_match_const_value(initial[i], &number) || num_is_finite(number);
        num_destroy(&number);
        if (!finite)
            goto cleanup;
    }
    result = de_solve_result_new(DE_SOLVE_STATUS_SERIES, DE_SOLVER_TAYLOR_SERIES,
                                 "local convergent Taylor series, truncated with an explicit O remainder; not a closed form");
    if (!result)
        goto cleanup;
    result->series_degree = degree;
    result->series_centre = expr_clone(initial[0]);
    result->series_coefficients = calloc(degree + 1u, sizeof(*result->series_coefficients));
    if (!result->series_centre || !result->series_coefficients)
        goto failed;
    result->series_coefficients[0] = expr_clone(initial[1]);
    result->series_coefficients[1] = expr_clone(initial[2]);
    if (!result->series_coefficients[0] || !result->series_coefficients[1])
        goto failed;
    for (size_t n = 0u; n + 2u <= degree; ++n) {
        expr_t *inputs[3][9] = {{NULL}};
        bool valid = true;
        for (size_t k = 0u; k <= n; ++k) {
            inputs[0][k] = k == 0u ? expr_clone(initial[0]) : k == 1u ? expr_const_one() : expr_const_zero();
            inputs[1][k] = expr_clone(result->series_coefficients[k]);
            inputs[2][k] = expr_mul_long(result->series_coefficients[k+1u], (long)k + 1L);
            valid = valid && inputs[0][k] && inputs[1][k] && inputs[2][k];
        }
        expr_t **forcing = valid ? de_series_compose(normal, de_lie_rhs(normal), 0u, inputs, n) : NULL;
        result->series_coefficients[n+2u] = forcing ? expr_div_long(forcing[n], (long)((n+1u)*(n+2u))) : NULL;
        result->series_coefficients[n+2u] = de_series_collect_initial_data(result->series_coefficients[n+2u], initial);
        de_series_clear(forcing, n);
        for (size_t i = 0u; i < 3u; ++i) {
            for (size_t k = 0u; k <= n; ++k)
                expr_free(inputs[i][k]);
        }
        if (!result->series_coefficients[n+2u])
            goto failed;
    }
    shift = expr_sub_simplify_owned(expr_clone(de_lie_coordinate(normal, 0u)), expr_clone(initial[0]));
    polynomial = expr_clone(result->series_coefficients[0]);
    power = expr_clone(shift);
    for (size_t n = 1u; polynomial && power && n <= degree; ++n) {
        /* Keep each Taylor coefficient beside its power, without a global common denominator. */
        number_t scalar = num_new();
        bool unit = expr_match_const_value(result->series_coefficients[n], &scalar) && num_eq(scalar, NUM_ONE);
        expr_t *term = expr_is_exact_zero(result->series_coefficients[n]) ? expr_const_zero() :
                       unit ? expr_clone(power) : expr_mul(result->series_coefficients[n], power);
        num_destroy(&scalar);
        expr_t *next = !term ? NULL : expr_is_exact_zero(term) ? expr_clone(polynomial) :
                       expr_is_exact_zero(polynomial) ? expr_clone(term) : expr_add(polynomial, term);
        expr_free(term);
        expr_free(polynomial);
        polynomial = next;
        power = expr_mul_simplify_owned(power, expr_clone(shift));
    }
    tail = power ? expr_new_arbitrary_function("O", power) : NULL;
    expr_t *rhs = polynomial && tail ? expr_add(polynomial, tail) : NULL;
    solution = rhs ? equ_new(dependent, rhs) : NULL;
    expr_free(rhs);
    if (!solution || de_solve_result_append(result, solution) != 0)
        goto failed;
    solution = NULL;
    if ((options & DE_SOLVE_OPTION_STEPS) && !de_series_steps(de, result))
        goto failed;
    goto cleanup;
failed:
    de_solve_result_free(result);
    result = NULL;
    status = DE_SOLVE_STATUS_FAILED;
    reason = "failed to construct the local Taylor series";
cleanup:
    equ_free(solution);
    expr_free(tail);
    expr_free(power);
    expr_free(shift);
    expr_free(polynomial);
    for (size_t i = 0u; i < 3u; ++i)
        expr_free(initial[i]);
    expr_free(remainder);
    expr_free(leading);
    expr_free(residual);
    de_lie_free(normal);
    return result ? result : de_solve_result_new(status, DE_SOLVER_NONE, reason);
}
