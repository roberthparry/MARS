#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* At most four coordinates: identify pure second derivatives without relying on coordinate names. */
static bool de_wave_derivatives(const expr_t *expr, size_t count, expr_t *const *coordinates, const expr_t **dependent,
                                 const expr_t **derivatives)
{
    const expr_t *left = NULL, *right = NULL;
    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *subject = expr_formal_derivative_dependent(expr);
        const expr_t *wrt = expr_formal_derivative_wrt_at(expr, 0u);
        if (expr_formal_derivative_order(expr) != 2u || !subject ||
            !expr_struct_eq(wrt, expr_formal_derivative_wrt_at(expr, 1u)) ||
            (*dependent && !expr_struct_eq(*dependent, subject)))
            return false;
        for (size_t i = 0u; i < count; ++i) {
            if (!expr_struct_eq(wrt, coordinates[i]))
                continue;
            if (derivatives[i] && !expr_struct_eq(derivatives[i], expr))
                return false;
            derivatives[i] = expr;
            *dependent = subject;
            return true;
        }
        return false;
    }
    return !expr_child_exprs(expr, &left, &right) ||
           (de_wave_derivatives(left, count, coordinates, dependent, derivatives) &&
            de_wave_derivatives(right, count, coordinates, dependent, derivatives));
}

/* Look up only parameter leaves used by this coefficient; keep the symbolic solution itself unbound. */
static void de_wave_bind_known_parameters(const expr_t *expr, const diffequ_t *de, expr_t **copy, bool *known)
{
    const expr_t *left = NULL, *right = NULL;
    if (!expr || !*copy)
        return;
    if (expr_child_exprs(expr, &left, &right)) {
        de_wave_bind_known_parameters(left, de, copy, known);
        de_wave_bind_known_parameters(right, de, copy, known);
        return;
    }
    const char *name = expr_symbol_name(expr);
    const expr_t *binding = name ? de_constant(de, name) : NULL;
    number_t value = expr_eval(binding ? binding : expr);
    if (name && num_is_nan(value)) {
        *known = false;
    } else if (binding) {
        expr_t *constant = expr_new_const(value);
        expr_t *next = constant ? expr_substitute(*copy, expr, constant) : NULL;
        expr_free(constant);
        expr_free(*copy);
        *copy = next;
    }
    num_destroy(&value);
}

static bool de_wave_numeric_value(const expr_t *expr, const diffequ_t *de, number_t *value)
{
    expr_t *copy = expr_clone(expr);
    bool known = true;
    de_wave_bind_known_parameters(expr, de, &copy, &known);
    num_destroy(value);
    *value = copy ? expr_eval(copy) : num_clone(NUM_NAN);
    expr_free(copy);
    return known;
}

static bool de_wave_constant_coefficient(const expr_t *expr, const diffequ_t *de, const expr_t *dependent)
{
    if (!expr || expr_is_exact_zero(expr) || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(expr, de->independent_vars[i]))
            return false;
    number_t value = num_new();
    bool known = de_wave_numeric_value(expr, de, &value);
    bool valid = !known || (num_is_finite(value) && num_is_real(value) && !num_is_zero(value));
    num_destroy(&value);
    return valid;
}

static bool de_wave_has_name(const expr_t *expr, const char *name)
{
    const char *symbol = expr ? expr_symbol_name(expr) : NULL;
    const expr_t *left = NULL, *right = NULL;
    return (symbol && strcmp(symbol, name) == 0) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_wave_has_name(left, name) || de_wave_has_name(right, name)));
}

/* Clear a rational time coefficient before division, avoiding nested reciprocals in the derived speed. */
static expr_t *de_wave_squared_speed(const expr_t *spatial, const expr_t *temporal)
{
    expr_t *opposite = expr_negate_owned(expr_clone(temporal));
    const expr_t *numerator = NULL, *denominator = NULL;
    expr_t *ratio = expr_match_div_expr(opposite, &numerator, &denominator)
                        ? expr_div_simplify_owned(expr_mul(spatial, denominator), expr_clone(numerator))
                        : expr_div_simplify_owned(expr_clone(spatial), expr_clone(opposite));
    expr_free(opposite);
    return ratio;
}

/* Four display/binder names are chosen once; suffix searches occur only when the input occupies a preferred name. */
static expr_t *de_wave_fresh_symbol(const diffequ_t *de, const expr_t *residual, const char *preferred)
{
    char name[64];
    snprintf(name, sizeof(name), "%s", preferred);
    for (size_t suffix = 1u;; ++suffix) {
        bool occupied = de_constant(de, name) != NULL || de_wave_has_name(residual, name);
        for (size_t i = 0u; i < de->independent_count; ++i)
            occupied = occupied || de_wave_has_name(de->independent_vars[i], name);
        for (size_t i = 0u; i < de->condition_count; ++i) {
            occupied = occupied || de_wave_has_name(equ_rhs(de->conditions[i]), name);
            for (size_t j = 0u; j < de->condition_point_counts[i]; ++j)
                occupied = occupied || de_wave_has_name(de->condition_points[i][j], name);
        }
        if (!occupied)
            break;
        snprintf(name, sizeof(name), "%s_%zu", preferred, suffix);
    }
    return expr_new_named_var(NUM_NAN, name);
}

/* Build a genuine spherical mean from the existing definite-integral nodes, not an unevaluated function label. */
static expr_t *de_wave_mean(const char *name, expr_t *const *shifted, const expr_t *theta, const expr_t *phi)
{
    expr_t *function = expr_new_arbitrary_function_n(name, 3u, shifted);
    expr_t *integrand = expr_mul_simplify_owned(function, expr_sin(theta));
    expr_t *zero = expr_const_zero(), *pi = expr_new_const(NUM_PI);
    expr_t *two_pi = expr_mul_simplify_owned(expr_const_long(2L), expr_clone(pi));
    expr_t *four_pi = expr_mul_simplify_owned(expr_const_long(4L), expr_clone(pi));
    expr_t *inner = integrand ? expr_integral_with_bounds_internal(integrand, zero, two_pi, phi) : NULL;
    expr_t *outer = inner ? expr_integral_with_bounds_internal(inner, zero, pi, theta) : NULL;
    expr_t *mean = expr_div(outer, four_pi);
    expr_free(outer);
    expr_free(inner);
    expr_free(four_pi);
    expr_free(two_pi);
    expr_free(pi);
    expr_free(zero);
    expr_free(integrand);
    return mean;
}

/* A bound coordinate must render as its name rather than its unset numeric value. */
static char *de_wave_coordinate_TeX(const expr_t *coordinate)
{
    expr_t *symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(coordinate));
    char *TeX = symbol ? expr_to_TeX_body(symbol) : NULL;
    expr_free(symbol);
    return TeX;
}

static const expr_t *de_wave_square_base(const expr_t *squared)
{
    const expr_t *base = NULL, *exponent = NULL;
    expr_t *two = expr_const_long(2L);
    bool square = expr_match_pow_expr(squared, &base, &exponent) && two && expr_struct_eq(exponent, two);
    expr_free(two);
    number_t power = num_new();
    square = square || (expr_match_pow_const(squared, &base, &power) && num_eq(power, NUM_TWO));
    num_destroy(&power);
    const expr_t *left = NULL, *right = NULL;
    if (!square && expr_match_mul_expr(squared, &left, &right) && expr_struct_eq(left, right)) {
        base = left;
        square = true;
    }
    return square ? base : NULL;
}

/* Under the wave family's positive-real squared-speed assumption, sqrt(b^2) is |b|, with real non-zero b. */
static bool de_wave_speed_notation(const expr_t *squared, const expr_t *speed, char **TeX, char **text,
                                    string_t **condition_TeX, string_t **condition_text)
{
    const expr_t *base = de_wave_square_base(squared);
    bool square = base != NULL;
    expr_t *display = square ? expr_abs(base) : expr_clone(speed);
    *TeX = display ? expr_to_TeX_body_wrapped(display, SIZE_MAX) : NULL;
    *text = display ? expr_to_string(display, style_UNBOUND) : NULL;
    expr_free(display);
    if (square) {
        char *base_TeX = expr_to_TeX_body_wrapped(base, SIZE_MAX);
        char *base_text = expr_to_string(base, style_UNBOUND);
        if (base_TeX && base_text) {
            *condition_TeX = string_sprintf(
                ",\\quad %s\\in\\mathbb R,\\quad %s\\ne0", base_TeX, base_TeX);
            *condition_text = string_sprintf("; %s is real and non-zero (either sign)", base_text);
        }
        free(base_text);
        free(base_TeX);
    } else {
        *condition_TeX = string_sprintf(",\\quad\\text{real, non-zero wave speed}");
        *condition_text = string_sprintf(" (real, non-zero wave speed)");
    }
    return *TeX && *text && *condition_TeX && *condition_text;
}

static bool de_wave_notation(equation_t *solution, diffequ_solve_result_t *result, expr_t *const *space,
                              const expr_t *time, const expr_t *speed_squared, const expr_t *speed,
                              const expr_t *f, const expr_t *g,
                              const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    expr_t *speed_symbol = de_wave_fresh_symbol(de, residual, "c");
    char *names[8] = {NULL};
    const expr_t *symbols[8] = {space[0], space[1], space[2], time, f, g, speed_symbol, equ_lhs(solution)};
    bool valid = speed_symbol != NULL;
    for (size_t i = 0u; valid && i < 8u; ++i) {
        names[i] = de_wave_coordinate_TeX(symbols[i]);
        valid = names[i] != NULL;
    }
    char *speed_TeX = NULL, *speed_text = NULL;
    string_t *condition_TeX = NULL, *condition_text = NULL;
    valid = de_wave_speed_notation(speed_squared, speed, &speed_TeX, &speed_text,
                                   &condition_TeX, &condition_text) && valid;
    const char *tn = expr_symbol_name(time), *fn = expr_symbol_name(f), *gn = expr_symbol_name(g);
    const char *cn = expr_symbol_name(speed_symbol);
    valid = valid && speed_TeX && speed_text;
    string_t *notation = valid ? string_sprintf(
        "\\begin{aligned}[t]\n"
        "&\\frac{\\partial}{\\partial %s}\\!\\left[%s\\,\\mathcal M_{%s\\,%s}%s(\\mathbf r)\\right]"
        "+%s\\,\\mathcal M_{%s\\,%s}%s(\\mathbf r)"
        "\\\\[0.6em]&\\mathbf r=(%s,%s,%s),\\quad %s=%s%S"
        "\\\\[0.4em]&\\mathcal M_a:\\text{ spherical average at radius }|a|"
        "\\\\&%s,%s:\\text{ arbitrary smooth spatial functions.}"
        "\n\\end{aligned}",
        names[3], names[3], names[6], names[3], names[4], names[3], names[6], names[3], names[5],
        names[0], names[1], names[2], names[6], speed_TeX, condition_TeX, names[4], names[5]) : NULL;
    string_t *unbound = valid ? string_sprintf(
        "d/d%s[%s*M_(%s*%s) %s(r)] + %s*M_(%s*%s) %s(r)\n"
        "r = (%s, %s, %s); %s = %s%S.\n"
        "M_a H(r): average of H over the sphere centred at r, radius |a|.\n"
        "%s, %s: arbitrary smooth spatial functions.",
        tn, tn, cn, tn, fn, tn, cn, tn, gn,
        expr_symbol_name(space[0]), expr_symbol_name(space[1]), expr_symbol_name(space[2]),
        cn, speed_text, condition_text, fn, gn) : NULL;
    valid = notation && unbound && equ_set_display_TeX(solution, NULL, notation) == 0 &&
            equ_set_display_unbound(solution, NULL, unbound) == 0;
    if (valid && include_steps) {
        string_t *plain = string_sprintf(
            "Kirchhoff's formula for the three-dimensional constant-speed wave equation.\n"
            "Time coordinate: %s; spatial coordinates: (%s, %s, %s).\n"
            "M_a H(r) = (1/(4*pi))*∫_{|omega|=1} H(r+a*omega) dOmega.\n"
            "This is the spherical average of H, centred at r with radius |a|; M_0 H = H.\n"
            "%s and %s are arbitrary initial field and initial time derivative; no initial data imposed.\n"
            "Require finite non-zero constant coefficients and a real positive squared speed.\n"
            "This is the general smooth whole-space family, with no boundary conditions imposed.\n"
            "It is an exact integral representation, not a finite elementary closed form.\n",
            tn, expr_symbol_name(space[0]), expr_symbol_name(space[1]), expr_symbol_name(space[2]), fn, gn);
        string_t *TeX = string_sprintf(
            "\\begin{aligned}&\\text{Kirchhoff's formula (three spatial dimensions)}"
            "\\\\[0.6em]&\\mathcal M_a H(\\mathbf r)=\\frac{1}{4\\pi}"
            "\\int_{|\\boldsymbol\\omega|=1}H(\\mathbf r+a\\boldsymbol\\omega)\\,d\\Omega"
            "\\\\[0.5em]&\\text{Average over a sphere centred at }\\mathbf r\\text{, radius }|a|."
            "\\\\&\\mathcal M_0 H=H"
            "\\\\[0.5em]&%s(\\mathbf r,0)=%s(\\mathbf r),\\quad "
            "\\frac{\\partial %s}{\\partial %s}(\\mathbf r,0)=%s(\\mathbf r)"
            "\\\\&\\text{Both functions are arbitrary; no initial data imposed.}"
            "\\\\[0.5em]&\\text{Assumes finite non-zero constant coefficients and real }%s>0."
            "\\\\&\\text{Whole space; no boundary conditions. Exact integral representation.}\\end{aligned}",
            names[7], names[4], names[7], names[3], names[5], names[6]);
        valid = plain && TeX && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                de_solve_result_set_steps_TeX(result, string_c_str(TeX)) == 0;
        string_free(TeX);
        string_free(plain);
    }
    string_free(unbound);
    string_free(notation);
    string_free(condition_text);
    string_free(condition_TeX);
    free(speed_text);
    free(speed_TeX);
    for (size_t i = 0u; i < 8u; ++i)
        free(names[i]);
    expr_free(speed_symbol);
    return valid;
}

/* Recognise A*Delta(u)+B*u_tt=0 and construct its general whole-space Kirchhoff family. */
diffequ_solve_result_t *de_pde_solve_wave(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 4u || de->condition_count != 0u)
        return NULL;
    const expr_t *dependent = NULL, *derivatives[4] = {NULL};
    if (!de_wave_derivatives(residual, 4u, de->independent_vars, &dependent, derivatives) || !dependent)
        return NULL;
    expr_t *coefficients[4] = {NULL}, *remaining = expr_clone(residual);
    expr_t *speed_squared = NULL, *speed = NULL, *theta = NULL, *phi = NULL, *f = NULL, *g = NULL;
    expr_t *shifted[3] = {NULL}, *space[3] = {NULL};
    expr_t *mean_f = NULL, *mean_g = NULL, *weighted_f = NULL, *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    size_t time_index = 4u;
    bool valid = remaining != NULL;
    for (size_t i = 0u; valid && i < 4u; ++i) {
        expr_t *next = NULL;
        valid = derivatives[i] && de_linear_decompose(remaining, derivatives[i], &coefficients[i], &next) &&
                de_wave_constant_coefficient(coefficients[i], de, dependent);
        expr_free(remaining);
        remaining = next;
    }
    if (!valid || !de_pde_is_symbolically_zero(remaining))
        goto cleanup;
    /* At most four candidates, each comparing the other three coefficients. */
    for (size_t candidate = 0u; candidate < 4u; ++candidate) {
        size_t first = candidate == 0u ? 1u : 0u;
        bool equal = true;
        for (size_t i = 0u; i < 4u; ++i)
            if (i != candidate && !de_pde_same_symbolic_form(coefficients[first], coefficients[i]))
                equal = false;
        if (!equal)
            continue;
        expr_t *q = de_wave_squared_speed(coefficients[first], coefficients[candidate]);
        number_t value = num_new();
        bool known = q && de_wave_numeric_value(q, de, &value);
        bool positive = q && (!known || (num_is_real(value) && num_is_finite(value) && num_sign(value) > 0));
        num_destroy(&value);
        if (positive) {
            time_index = candidate;
            speed_squared = expr_display_expanded(q);
            expr_free(q);
            break;
        }
        expr_free(q);
    }
    if (time_index == 4u)
        goto cleanup;
    speed = expr_simplify_owned(expr_sqrt(speed_squared));
    theta = de_wave_fresh_symbol(de, residual, "θ");
    phi = de_wave_fresh_symbol(de, residual, "φ");
    f = de_wave_fresh_symbol(de, residual, "F");
    g = de_wave_fresh_symbol(de, residual, "G");
    if (!speed || !theta || !phi || !f || !g)
        goto cleanup;
    expr_t *time = de->independent_vars[time_index];
    expr_t *radius = expr_mul_simplify_owned(expr_clone(time), expr_clone(speed));
    expr_t *directions[3] = {expr_mul_simplify_owned(expr_sin(theta), expr_cos(phi)),
                             expr_mul_simplify_owned(expr_sin(theta), expr_sin(phi)), expr_cos(theta)};
    for (size_t i = 0u, j = 0u; i < 4u; ++i) {
        if (i == time_index)
            continue;
        space[j] = de->independent_vars[i];
        shifted[j] = expr_add_simplify_owned(expr_clone(space[j]), expr_mul(radius, directions[j]));
        ++j;
    }
    expr_free(radius);
    for (size_t i = 0u; i < 3u; ++i)
        expr_free(directions[i]);
    if (!shifted[0] || !shifted[1] || !shifted[2])
        goto cleanup;
    mean_f = de_wave_mean(expr_symbol_name(f), shifted, theta, phi);
    mean_g = de_wave_mean(expr_symbol_name(g), shifted, theta, phi);
    weighted_f = mean_f ? expr_mul(time, mean_f) : NULL;
    expr_t *derivative = weighted_f ? expr_create_deriv(weighted_f, time) : NULL;
    expr_t *weighted_g = mean_g ? expr_mul(time, mean_g) : NULL;
    right = derivative && weighted_g ? expr_add(derivative, weighted_g) : NULL;
    expr_free(weighted_g);
    expr_free(derivative);
    solution = right ? de_pde_solution_equation(dependent, right) : NULL;
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_KIRCHHOFF,
        "general whole-space Kirchhoff integral; arbitrary smooth spatial functions; positive constant wave speed") : NULL;
    if (!result || !de_wave_notation(solution, result, space, time, speed_squared, speed, f, g, de, residual,
                                    include_steps) ||
        de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;
cleanup:
    equ_free(solution);
    expr_free(right);
    expr_free(weighted_f);
    expr_free(mean_g);
    expr_free(mean_f);
    for (size_t i = 0u; i < 3u; ++i)
        expr_free(shifted[i]);
    expr_free(g);
    expr_free(f);
    expr_free(phi);
    expr_free(theta);
    expr_free(speed);
    expr_free(speed_squared);
    expr_free(remaining);
    for (size_t i = 0u; i < 4u; ++i)
        expr_free(coefficients[i]);
    return result;
}

static bool de_wave_has_integral(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    return expr_match_integral_expr(expr, NULL, NULL) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_wave_has_integral(left) || de_wave_has_integral(right)));
}

/* Collect polynomial powers before integration, including powers of affine characteristic endpoints. */
static expr_t *de_wave_collect(const expr_t *expr, const expr_t *coordinate)
{
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    if (!equ_collect_symbolic_polynomial_alloc(expr, coordinate, &coefficients, &degree))
        return expr_clone(expr);
    expr_t *result = expr_const_zero(), *power = expr_const_one();
    for (size_t n = 0u; result && power && n <= degree; ++n) {
        result = expr_add_simplify_owned(result, expr_mul_simplify_owned(expr_clone(coefficients[n]), expr_clone(power)));
        power = expr_mul_simplify_owned(power, expr_clone(coordinate));
    }
    for (size_t n = 0u; n <= degree; ++n)
        expr_free(coefficients[n]);
    free(coefficients);
    expr_free(power);
    return result;
}

/* Use a verified elementary primitive when available; otherwise preserve the actual integration bounds. */
static expr_t *de_wave_integral(const expr_t *integrand, const expr_t *dummy,
                                const expr_t *lower, const expr_t *upper)
{
    if (!integrand || !dummy || !lower || !upper)
        return NULL;
    if (expr_is_exact_zero(integrand))
        return expr_const_zero();
    expr_t *collected = de_wave_collect(integrand, dummy);
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    bool polynomial = equ_collect_symbolic_polynomial_alloc(integrand, dummy, &coefficients, &degree);
    expr_t *primitive = NULL;
    if (polynomial) {
        primitive = expr_const_zero();
        expr_t *power = expr_clone(dummy);
        for (size_t n = 0u; primitive && power && n <= degree; ++n) {
            expr_t *coefficient = expr_div_simplify_owned(expr_clone(coefficients[n]), expr_const_long((long)n + 1L));
            primitive = expr_add_simplify_owned(primitive, expr_mul_simplify_owned(coefficient, expr_clone(power)));
            power = expr_mul_simplify_owned(power, expr_clone(dummy));
        }
        expr_free(power);
        for (size_t n = 0u; n <= degree; ++n)
            expr_free(coefficients[n]);
        free(coefficients);
    } else if (!de_wave_has_integral(integrand)) {
        primitive = expr_integrate(collected, dummy);
    }
    expr_t *result = NULL;
    if (primitive && !de_wave_has_integral(primitive) &&
        (polynomial || expr_verify_antiderivative_real_internal(primitive, collected, dummy))) {
        result = expr_sub_simplify_owned(expr_substitute(primitive, dummy, upper),
                                         expr_substitute(primitive, dummy, lower));
        expr_t *expanded = result ? expr_expand_products_internal(result) : NULL;
        expr_free(result);
        result = expr_simplify_owned(expanded);
    } else {
        result = expr_integral_with_bounds_internal(integrand, lower, upper, dummy);
    }
    expr_free(primitive);
    expr_free(collected);
    return result;
}

/* Two Cauchy data on one time slice; their free spatial argument identifies the slice, not declaration order. */
static bool de_wave_initial_data(const diffequ_t *de, const expr_t *dependent, size_t *time_index,
                                  const expr_t **initial_time, const expr_t **displacement, const expr_t **velocity)
{
    size_t velocity_index = 2u;
    for (size_t i = 0u; i < 2u; ++i) {
        const expr_t *lhs = equ_lhs(de->conditions[i]);
        if (expr_struct_eq(lhs, dependent)) {
            if (*displacement)
                return false;
            *displacement = equ_rhs(de->conditions[i]);
        } else if (expr_is_formal_derivative(lhs) && expr_formal_derivative_order(lhs) == 1u &&
                   expr_struct_eq(expr_formal_derivative_dependent(lhs), dependent)) {
            if (*velocity)
                return false;
            *velocity = equ_rhs(de->conditions[i]);
            velocity_index = i;
        } else {
            return false;
        }
    }
    if (!*displacement || !*velocity || velocity_index == 2u)
        return false;
    const expr_t *wrt = expr_formal_derivative_wrt_at(equ_lhs(de->conditions[velocity_index]), 0u);
    if (expr_struct_eq(wrt, de->independent_vars[0]))
        *time_index = 0u;
    else if (expr_struct_eq(wrt, de->independent_vars[1]))
        *time_index = 1u;
    else
        return false;
    const expr_t *space = de->independent_vars[1u - *time_index];
    const expr_t *time = de->independent_vars[*time_index];
    size_t spatial_argument = 2u;
    for (size_t i = 0u; i < 2u; ++i) {
        if (de->condition_point_counts[i] != 2u)
            return false;
        size_t index = expr_struct_eq(de->condition_points[i][0], space) ? 0u : 1u;
        if (!expr_struct_eq(de->condition_points[i][index], space) ||
            (spatial_argument != 2u && spatial_argument != index))
            return false;
        spatial_argument = index;
        const expr_t *point = de->condition_points[i][1u - index];
        if (de_expr_uses(point, space) || de_expr_uses(point, time) || de_expr_uses(point, dependent) ||
            (*initial_time && !de_pde_same_symbolic_form(point, *initial_time)))
            return false;
        *initial_time = point;
        const expr_t *value = equ_rhs(de->conditions[i]);
        if (de_expr_uses(value, time) || de_expr_uses(value, dependent))
            return false;
    }
    number_t point_value = num_new();
    bool known = de_wave_numeric_value(*initial_time, de, &point_value);
    bool valid = !known || (num_is_real(point_value) && num_is_finite(point_value));
    num_destroy(&point_value);
    return valid;
}

static bool de_wave_ivp_steps(diffequ_solve_result_t *result, const expr_t *space, const expr_t *time,
                               const expr_t *initial_time, const expr_t *speed_squared, const expr_t *speed)
{
    char *x = de_wave_coordinate_TeX(space), *t = de_wave_coordinate_TeX(time);
    char *t0 = expr_to_TeX_body_wrapped(initial_time, SIZE_MAX);
    char *speed_TeX = NULL, *speed_text = NULL;
    string_t *condition_TeX = NULL, *condition_text = NULL;
    bool valid = de_wave_speed_notation(speed_squared, speed, &speed_TeX, &speed_text,
                                         &condition_TeX, &condition_text) && x && t && t0;
    string_t *plain = valid ? string_sprintf(
        "d'Alembert's formula plus Duhamel's principle (one spatial dimension).\n"
        "Space: %s; time: %s. Wave speed: %s%S.\n"
        "The two supplied data are the displacement and time derivative on the same initial slice.\n"
        "Translate the displacement in both directions and average.\n"
        "Integrate the initial velocity between the two characteristic endpoints, with coefficient 1/(2*speed).\n"
        "Integrate the normalised forcing over the past characteristic triangle with the same coefficient.\n"
        "The forcing contribution and its first time derivative vanish on the initial slice.\n"
        "Use verified elementary primitives where available; otherwise retain exact definite integrals.\n"
        "Assumes sufficiently smooth data, finite constant coefficients, real positive squared speed,\n"
        "and a real initial time. Whole spatial line; no boundary conditions imposed.",
        expr_symbol_name(space), expr_symbol_name(time), speed_text, condition_text) : NULL;
    string_t *TeX = valid ? string_sprintf(
        "\\begin{aligned}&\\text{d'Alembert + Duhamel (one spatial dimension)}"
        "\\\\[0.5em]&\\text{Space: }%s,\\quad\\text{time: }%s,\\quad\\text{initial time: }%s"
        "\\\\&\\text{Wave speed: }%s%S"
        "\\\\[0.5em]&\\text{Average the two translated initial displacements.}"
        "\\\\&\\text{Add the initial-velocity integral between the characteristic endpoints.}"
        "\\\\&\\text{Add the forcing integral over the past characteristic triangle.}"
        "\\\\&\\text{Each integral has coefficient }1/(2\\,\\text{wave speed})."
        "\\\\[0.5em]&\\text{The forcing term and its first time derivative initially vanish.}"
        "\\\\&\\text{Verified elementary primitives, or exact definite integrals.}"
        "\\\\&\\text{Assumes smooth data, real initial time and finite constant coefficients.}"
        "\\\\&\\text{Whole spatial line; no boundary conditions imposed.}\\end{aligned}",
        x, t, t0, speed_TeX, condition_TeX) : NULL;
    valid = plain && TeX && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(TeX)) == 0;
    string_free(TeX);
    string_free(plain);
    string_free(condition_text);
    string_free(condition_TeX);
    free(speed_text);
    free(speed_TeX);
    free(t0);
    free(t);
    free(x);
    return valid;
}

/* Keep the three computed contributions on one line, with coefficients beside rather than enclosing integrals. */
static bool de_wave_ivp_notation(equation_t *solution, const expr_t *displacement,
                                  const expr_t *velocity, const expr_t *source, const expr_t *twice_speed)
{
    expr_t *terms[3] = {expr_clone(displacement), expr_div(velocity, twice_speed), expr_div(source, twice_speed)};
    string_t *TeX = string_new();
    bool valid = TeX != NULL, first = true;
    for (size_t i = 0u; valid && i < 3u; ++i) {
        if (!terms[i]) {
            valid = false;
            break;
        }
        /* Zero data can still be represented as (0+0)/2 or 0/(2*c).
           Test the reduced term, but retain the paired-endpoint notation for non-zero contributions. */
        expr_t *reduced = expr_simplify(terms[i]);
        if (!reduced) {
            valid = false;
            break;
        }
        bool zero = expr_is_exact_zero(reduced);
        expr_free(reduced);
        if (zero)
            continue;
        char *term = expr_to_TeX_body_wrapped(terms[i], SIZE_MAX);
        valid = term && string_append_format(TeX, "%s%s", first ? "" : " + ", term) >= 0;
        free(term);
        first = false;
    }
    if (valid && first)
        valid = string_append_cstr(TeX, "0") >= 0;
    valid = valid && equ_set_display_TeX(solution, NULL, TeX) == 0;
    string_free(TeX);
    for (size_t i = 0u; i < 3u; ++i)
        expr_free(terms[i]);
    return valid;
}

/* Solve A*u_tt+B*u_xx=R with both Cauchy data, allowing a translated initial time and exact forcing integrals. */
diffequ_solve_result_t *de_pde_solve_wave_ivp(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count != 2u)
        return NULL;
    const expr_t *dependent = NULL, *derivatives[2] = {NULL};
    const expr_t *initial_time = NULL, *displacement = NULL, *velocity = NULL;
    size_t time_index = 0u;
    if (!de_wave_derivatives(residual, 2u, de->independent_vars, &dependent, derivatives) || !dependent ||
        !de_wave_initial_data(de, dependent, &time_index, &initial_time, &displacement, &velocity))
        return NULL;
    expr_t *coefficients[2] = {NULL}, *remaining = expr_clone(residual);
    expr_t *speed_squared = NULL, *speed = NULL, *forcing = NULL, *xi = NULL, *s = NULL;
    expr_t *offset = NULL, *radius = NULL, *lower = NULL, *upper = NULL, *right = NULL;
    expr_t *g_plus = NULL, *g_minus = NULL, *h_xi = NULL, *velocity_term = NULL, *displacement_term = NULL;
    expr_t *f_xi = NULL, *f_xi_s = NULL, *inner = NULL, *source_term = NULL, *twice_speed = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    bool valid = remaining != NULL;
    for (size_t i = 0u; valid && i < 2u; ++i) {
        expr_t *next = NULL;
        valid = derivatives[i] && de_linear_decompose(remaining, derivatives[i], &coefficients[i], &next) &&
                de_wave_constant_coefficient(coefficients[i], de, dependent);
        expr_free(remaining);
        remaining = next;
    }
    if (!valid || !remaining || de_expr_uses(remaining, dependent))
        goto cleanup_ivp;
    speed_squared = de_wave_squared_speed(coefficients[1u - time_index], coefficients[time_index]);
    number_t value = num_new();
    bool known = speed_squared && de_wave_numeric_value(speed_squared, de, &value);
    valid = speed_squared && (!known || (num_is_real(value) && num_is_finite(value) && num_sign(value) > 0));
    num_destroy(&value);
    if (!valid)
        goto cleanup_ivp;
    /* In one dimension either signed root gives the same answer: both limits and the prefactor reverse. */
    const expr_t *signed_root = de_wave_square_base(speed_squared);
    speed = signed_root ? expr_clone(signed_root) : expr_simplify_owned(expr_sqrt(speed_squared));
    forcing = expr_div_simplify_owned(expr_negate_owned(expr_clone(remaining)), expr_clone(coefficients[time_index]));
    xi = de_wave_fresh_symbol(de, residual, "ξ");
    s = de_wave_fresh_symbol(de, residual, "s");
    const expr_t *space = de->independent_vars[1u - time_index], *time = de->independent_vars[time_index];
    offset = expr_sub_simplify_owned(expr_clone(time), expr_clone(initial_time));
    radius = expr_mul_simplify_owned(expr_clone(speed), expr_clone(offset));
    lower = expr_sub_simplify_owned(expr_clone(space), expr_clone(radius));
    upper = expr_add_simplify_owned(expr_clone(space), expr_clone(radius));
    if (!forcing || !xi || !s || !lower || !upper)
        goto cleanup_ivp;
    g_plus = expr_substitute(displacement, space, upper);
    g_minus = expr_substitute(displacement, space, lower);
    right = expr_div_simplify_owned(expr_add_simplify_owned(expr_clone(g_plus), expr_clone(g_minus)),
                                    expr_const_long(2L));
    h_xi = expr_substitute(velocity, space, xi);
    velocity_term = de_wave_integral(h_xi, xi, lower, upper);
    twice_speed = expr_mul_simplify_owned(expr_const_long(2L), expr_clone(speed));
    f_xi = expr_substitute(forcing, space, xi);
    f_xi_s = f_xi ? expr_substitute(f_xi, time, s) : NULL;
    expr_free(radius);
    radius = expr_mul_simplify_owned(expr_clone(speed), expr_sub_simplify_owned(expr_clone(time), expr_clone(s)));
    expr_free(lower);
    expr_free(upper);
    lower = expr_sub_simplify_owned(expr_clone(space), expr_clone(radius));
    upper = expr_add_simplify_owned(expr_clone(space), expr_clone(radius));
    inner = de_wave_integral(f_xi_s, xi, lower, upper);
    source_term = de_wave_integral(inner, s, initial_time, time);
    if (!right || !velocity_term || !source_term || !twice_speed)
        goto cleanup_ivp;
    number_t half_value = num_create_from_frac(1L, 2L);
    expr_t *half = expr_new_const(half_value), *initial_sum = expr_add(g_plus, g_minus);
    displacement_term = expr_mul(half, initial_sum);
    expr_free(initial_sum);
    expr_free(half);
    num_destroy(&half_value);
    right = expr_add_simplify_owned(right,
        expr_div_simplify_owned(expr_add(velocity_term, source_term), expr_clone(twice_speed)));
    if (right && !de_wave_has_integral(right)) {
        expr_t *expanded = de_wave_collect(right, time);
        expr_free(right);
        right = de_wave_collect(expanded, space);
        expr_free(expanded);
    }
    solution = right ? de_pde_solution_equation(dependent, right) : NULL;
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_DALEMBERT_DUHAMEL,
        "d'Alembert-Duhamel whole-line IVP; smooth data, real initial time and positive constant squared speed") : NULL;
    if (!result || (de_wave_has_integral(right) &&
                    !de_wave_ivp_notation(solution, displacement_term, velocity_term, source_term, twice_speed)) ||
        (include_steps && !de_wave_ivp_steps(result, space, time, initial_time, speed_squared, speed)) ||
        de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup_ivp;
    }
    solution = NULL;
cleanup_ivp:
    equ_free(solution);
    expr_free(twice_speed);
    expr_free(source_term);
    expr_free(inner);
    expr_free(f_xi_s);
    expr_free(f_xi);
    expr_free(velocity_term);
    expr_free(displacement_term);
    expr_free(h_xi);
    expr_free(g_minus);
    expr_free(g_plus);
    expr_free(right);
    expr_free(upper);
    expr_free(lower);
    expr_free(radius);
    expr_free(offset);
    expr_free(s);
    expr_free(xi);
    expr_free(forcing);
    expr_free(speed);
    expr_free(speed_squared);
    expr_free(remaining);
    for (size_t i = 0u; i < 2u; ++i)
        expr_free(coefficients[i]);
    return result;
}
