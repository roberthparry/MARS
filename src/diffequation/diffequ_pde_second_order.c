#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* Index xx, xy, yx, yy directly, with optional separate slots for the first derivatives x and y. */
static bool de_pde_second_order_derivatives(const expr_t *expr, const expr_t *x, const expr_t *y,
                                           const expr_t **dependent, const expr_t **derivatives,
                                           const expr_t **first_derivatives)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *subject = expr_formal_derivative_dependent(expr);
        size_t index = 0u;

        size_t order = expr_formal_derivative_order(expr);
        if ((order != 2u && !(order == 1u && first_derivatives)) || !subject ||
            (*dependent && !expr_struct_eq(*dependent, subject)))
            return false;
        for (size_t i = 0u; i < order; ++i) {
            const expr_t *wrt = expr_formal_derivative_wrt_at(expr, i);

            index *= 2u;
            if (expr_struct_eq(wrt, y))
                ++index;
            else if (!expr_struct_eq(wrt, x))
                return false;
        }
        const expr_t **slots = order == 1u ? first_derivatives : derivatives;
        if (slots[index] && !expr_struct_eq(slots[index], expr))
            return false;
        slots[index] = expr;
        *dependent = subject;
        return true;
    }
    return !expr_child_exprs(expr, &left, &right) ||
           (de_pde_second_order_derivatives(left, x, y, dependent, derivatives, first_derivatives) &&
            de_pde_second_order_derivatives(right, x, y, dependent, derivatives, first_derivatives));
}

static bool de_pde_second_order_coefficients(const expr_t *residual, const expr_t **derivatives,
                                            number_t *coefficients, expr_t **forcing_out)
{
    static const size_t coefficient_index[4] = {0u, 1u, 1u, 2u};
    expr_t *remaining = expr_clone(residual);
    bool valid = remaining != NULL;

    for (size_t i = 0u; valid && i < 4u; ++i) {
        expr_t *coefficient = NULL;
        expr_t *next = NULL;

        if (!derivatives[i])
            continue;
        number_t value = num_new();
        valid = de_linear_decompose(remaining, derivatives[i], &coefficient, &next) &&
                expr_match_const_value(coefficient, &value) && num_is_real(value) && num_is_finite(value);
        if (valid) {
            size_t slot = coefficient_index[i];
            number_t sum = num_add(coefficients[slot], value);

            num_destroy(&coefficients[slot]);
            coefficients[slot] = sum;
        }
        num_destroy(&value);
        expr_free(coefficient);
        expr_free(remaining);
        remaining = next;
    }
    *forcing_out = valid && remaining ? expr_negate_owned(expr_clone(remaining)) : NULL;
    valid = valid && *forcing_out;
    expr_free(remaining);
    return valid;
}

/* Reuse the equation solver's exact quadratic roots, including repeated and complex roots. */
static bool de_pde_second_order_roots(number_t a, number_t b, number_t c, equation_solutions_t *roots)
{
    expr_t *root = expr_new_named_var(NUM_NAN, "m");
    expr_t *square = root ? expr_mul(root, root) : NULL;
    expr_t *quadratic = expr_mul_simplify_owned(expr_new_const(a), square);
    expr_t *linear = expr_mul_simplify_owned(expr_new_const(b), expr_clone(root));
    expr_t *sum = expr_add_simplify_owned(quadratic, linear);
    expr_t *polynomial = expr_add_simplify_owned(sum, expr_new_const(c));
    expr_t *zero = expr_const_zero();

    /* Keep the root ordering unchanged under an overall negative scale. */
    if (num_sign(a) < 0)
        polynomial = expr_negate_owned(polynomial);
    equation_t *characteristic = polynomial && zero ? equ_new(polynomial, zero) : NULL;
    bool solved = characteristic && equ_solve_for_into(characteristic, root, roots) == 0;

    equ_free(characteristic);
    expr_free(zero);
    expr_free(polynomial);
    expr_free(root);
    return solved;
}

/* Recognise a numeric coefficient times a prescribed Euler weight, without accepting coordinate dependence. */
static bool de_pde_euler_coefficient(const expr_t *coefficient, const expr_t *weight, number_t *value)
{
    expr_t *ratio = expr_div_simplify_owned(expr_clone(coefficient), expr_clone(weight));
    bool valid = ratio && expr_match_const_value(ratio, value) && num_is_real(*value) && num_is_finite(*value);
    expr_free(ratio);
    return valid;
}

static bool de_pde_euler_steps(const diffequ_t *de, diffequ_solve_result_t *result, const number_t *parameters)
{
    /* Coordinates are bound problem symbols; render fresh unbound names, not their unset values. */
    expr_t *x_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(de->independent_vars[0]));
    expr_t *y_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(de->independent_vars[1]));
    char *x = x_symbol ? expr_to_TeX_body(x_symbol) : NULL;
    char *y = y_symbol ? expr_to_TeX_body(y_symbol) : NULL;
    expr_free(y_symbol);
    expr_free(x_symbol);
    char *values[3] = {NULL, NULL, NULL};
    char *values_TeX[3] = {NULL, NULL, NULL};
    bool valid = x && y;
    for (size_t i = 0u; i < 3u; ++i) {
        expr_t *value = expr_new_const(parameters[i]);
        values[i] = value ? expr_to_string(value, style_UNBOUND) : NULL;
        values_TeX[i] = value ? expr_to_TeX_body(value) : NULL;
        valid = valid && values[i] && values_TeX[i];
        expr_free(value);
    }
    string_t *steps = valid ? string_sprintf(
        "Radial Euler equation with A=%s, B=%s, C=%s.\n"
        "In generic coordinates, E=x*partial_x+y*partial_y differentiates along radial scaling.\n"
        "x*x*z_xx+2*x*y*z_xy+y*y*z_yy=(E*E-E)z.\n"
        "On the positive first-coordinate chart, set r=ln(x), eta=y/x; then E=partial_r.\n"
        "The equation becomes A*z_rr+(B-A)*z_r+C*z=0 at each fixed eta.\n"
        "Solve A*m*(m-1)+B*m+C=0 using the native equation solver.\n"
        "Distinct roots give x^m1*F(eta)+x^m2*G(eta); a repeated root gives x^m*(F(eta)+ln(x)*G(eta)).\n"
        "F and G are arbitrary twice-differentiable functions (complex-valued if needed).\n"
        "This local chart requires the first coordinate > 0; no boundary data are imposed.\n",
        values[0], values[1], values[2]) : NULL;
    string_t *TeX = valid ? string_sprintf(
        "\\begin{aligned}&\\text{Radial Euler equation: }A=%s,\\ B=%s,\\ C=%s"
        "\\\\&E=%s\\partial_{%s}+%s\\partial_{%s}\\quad\\text{(differentiation along radial scaling)}"
        "\\\\&\\text{The second-order radial operator is }E^2-E."
        "\\\\&r=\\ln(%s),\\quad\\eta=\\frac{%s}{%s},\\quad E=\\partial_r"
        "\\\\&A z_{rr}+(B-A)z_r+Cz=0\\quad\\text{at fixed }\\eta"
        "\\\\&A m(m-1)+Bm+C=0\\quad\\text{determines the powers.}"
        "\\\\&\\text{A repeated root introduces a logarithmic second solution.}"
        "\\\\&\\text{The two coefficients are arbitrary functions of }\\eta\\text{.}"
        "\\\\&\\text{Local chart: }%s>0\\text{; no boundary data imposed.}\\end{aligned}",
        values_TeX[0], values_TeX[1], values_TeX[2], x, x, y, y, x, y, x, x) : NULL;
    valid = steps && TeX && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(TeX)) == 0;
    string_free(TeX);
    string_free(steps);
    for (size_t i = 0u; i < 3u; ++i) {
        free(values_TeX[i]);
        free(values[i]);
    }
    free(y);
    free(x);
    return valid;
}

/* Reduce A(E²-E)z+B Ez+Cz=0 to an Euler ODE at each fixed ray. */
diffequ_solve_result_t *de_pde_solve_radial_euler(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count != 0u)
        return NULL;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    const expr_t *dependent = NULL;
    const expr_t *derivatives[6] = {NULL};
    if (!de_pde_second_order_derivatives(residual, x, y, &dependent, derivatives, derivatives + 4u) || !dependent)
        return NULL;
    expr_t *weights[6] = {expr_mul(x, x), expr_mul(x, y), expr_mul(x, y), expr_mul(y, y),
                         expr_clone(x), expr_clone(y)};
    number_t coefficients[6];
    for (size_t i = 0u; i < 6u; ++i)
        coefficients[i] = num_clone(NUM_ZERO);
    number_t parameters[3] = {num_new(), num_new(), num_new()};
    number_t middle = num_new(), mixed = num_new(), twice = num_new();
    expr_t *remaining = expr_clone(residual), *constant = NULL, *forcing = NULL;
    expr_t *eta = NULL, *right = NULL;
    equation_t *solution = NULL;
    equation_solutions_t roots = {0};
    diffequ_solve_result_t *result = NULL;
    bool valid = remaining != NULL;
    for (size_t i = 0u; valid && i < 6u; ++i) {
        if (!derivatives[i])
            continue;
        expr_t *coefficient = NULL, *next = NULL;
        valid = weights[i] && de_linear_decompose(remaining, derivatives[i], &coefficient, &next) &&
                de_pde_euler_coefficient(coefficient, weights[i], &coefficients[i]);
        expr_free(coefficient);
        expr_free(remaining);
        remaining = next;
    }
    mixed = num_add(coefficients[1], coefficients[2]);
    twice = num_mul_long(coefficients[0], 2L);
    if (!valid || num_is_zero(coefficients[0]) || !num_eq(coefficients[0], coefficients[3]) ||
        !num_eq(mixed, twice) || !num_eq(coefficients[4], coefficients[5]) ||
        !de_linear_decompose(remaining, dependent, &constant, &forcing) || !expr_is_exact_zero(forcing) ||
        !expr_match_const_value(constant, &parameters[2]) || !num_is_real(parameters[2]) ||
        !num_is_finite(parameters[2]))
        goto cleanup;
    parameters[0] = num_clone(coefficients[0]);
    parameters[1] = num_clone(coefficients[4]);
    middle = num_sub(parameters[1], parameters[0]);
    if (!de_pde_second_order_roots(parameters[0], middle, parameters[2], &roots))
        goto cleanup;
    size_t count = equ_solutions_count(&roots);
    if (count != 1u && count != 2u)
        goto cleanup;
    eta = expr_div_simplify_owned(expr_clone(y), expr_clone(x));
    for (size_t i = 0u; i < 2u; ++i) {
        const expr_t *root = equ_rhs(equ_solutions_at(&roots, count == 1u ? 0u : 1u - i));
        expr_t *power = root ? expr_pow_xp(x, root) : NULL;
        expr_t *amplitude = eta ? expr_new_arbitrary_function(i == 0u ? "F" : "G", eta) : NULL;
        expr_t *term = expr_mul_simplify_owned(power, amplitude);
        if (!term)
            goto cleanup;
        if (i == 1u && count == 1u)
            term = expr_mul_simplify_owned(term, expr_ln(x));
        right = i == 0u ? term : expr_add_simplify_owned(right, term);
        if (!right)
            goto cleanup;
    }
    solution = de_pde_solution_equation(dependent, right);
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                            "solved using radial Euler coordinates on the positive first-coordinate chart")
                      : NULL;
    if (!result || de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;
    if (include_steps && !de_pde_euler_steps(de, result, parameters)) {
        de_solve_result_free(result);
        result = NULL;
    }
cleanup:
    equ_free(solution);
    equ_solutions_clear(&roots);
    expr_free(right);
    expr_free(eta);
    expr_free(forcing);
    expr_free(constant);
    expr_free(remaining);
    num_destroy(&twice);
    num_destroy(&mixed);
    num_destroy(&middle);
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&parameters[i]);
    for (size_t i = 0u; i < 6u; ++i) {
        num_destroy(&coefficients[i]);
        expr_free(weights[i]);
    }
    return result;
}

static expr_t *de_pde_second_order_family(const expr_t *x, const expr_t *y, number_t a, number_t b, number_t c)
{
    expr_t *arguments[2] = {NULL, NULL};
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *right = NULL;
    bool repeated = false;

    if (num_is_zero(a) && num_is_zero(c)) {
        if (num_is_zero(b))
            return NULL;
        arguments[0] = expr_clone(x);
        arguments[1] = expr_clone(y);
    } else {
        if (num_is_zero(a)) {
            const expr_t *coordinate = x;
            number_t coefficient = a;

            x = y;
            y = coordinate;
            a = c;
            c = coefficient;
        }
        equation_solutions_t roots = {0};
        size_t count = de_pde_second_order_roots(a, b, c, &roots) ? equ_solutions_count(&roots) : 0u;

        repeated = count == 1u;
        if (count == 1u || count == 2u) {
            for (size_t i = 0u; i < 2u; ++i) {
                bool opposite_roots = count == 2u && num_is_zero(b);
                size_t index = repeated || opposite_roots ? 0u : count - 1u - i;
                const expr_t *root = equ_rhs(equ_solutions_at(&roots, index));
                expr_t *scaled_x = root ? expr_mul_simplify_owned(expr_clone(root), expr_clone(x)) : NULL;

                if (opposite_roots) {
                    /* Preserve the shared slope explicitly as y-s*x and y+s*x. */
                    arguments[i] = scaled_x ? (i == 0u ? expr_sub(y, scaled_x) : expr_add(y, scaled_x)) : NULL;
                    expr_free(scaled_x);
                } else {
                    arguments[i] = scaled_x ? expr_add_simplify_owned(scaled_x, expr_clone(y)) : NULL;
                }
            }
        }
        equ_solutions_clear(&roots);
    }

    if (arguments[0] && arguments[1]) {
        first = expr_new_arbitrary_function("F", arguments[0]);
        second = expr_new_arbitrary_function("G", arguments[1]);
        if (repeated)
            second = expr_mul_simplify_owned(expr_clone(x), second);
        right = expr_add_simplify_owned(first, second);
    }
    expr_free(arguments[1]);
    expr_free(arguments[0]);
    return right;
}

static expr_t *de_pde_second_order_apply(const expr_t *candidate, const expr_t *x, const expr_t *y,
                                        const number_t *coefficients)
{
    static const size_t derivative_pairs[3][2] = {{0u, 0u}, {0u, 1u}, {1u, 1u}};
    const expr_t *coordinates[2] = {x, y};
    expr_t *first[2] = {expr_create_deriv(candidate, x), expr_create_deriv(candidate, y)};
    expr_t *applied = expr_const_zero();

    for (size_t i = 0u; applied && i < 3u; ++i) {
        if (num_is_zero(coefficients[i]))
            continue;
        expr_t *second = expr_create_deriv(first[derivative_pairs[i][0]], coordinates[derivative_pairs[i][1]]);
        expr_t *term = second ? expr_mul_simplify_owned(expr_new_const(coefficients[i]), second) : NULL;

        applied = expr_add_simplify_owned(applied, term);
    }
    expr_free(first[1]);
    expr_free(first[0]);
    return applied;
}

static bool de_pde_second_order_has_name(const expr_t *expr, const char *name)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const char *symbol = expr ? expr_symbol_name(expr) : NULL;

    if (symbol && strcmp(symbol, name) == 0)
        return true;
    return expr_child_exprs(expr, &left, &right) &&
           (de_pde_second_order_has_name(left, name) || de_pde_second_order_has_name(right, name));
}

static bool de_pde_second_order_has_integral(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    return expr_match_integral_expr(expr, NULL, NULL) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_pde_second_order_has_integral(left) || de_pde_second_order_has_integral(right)));
}

/* Reserve the original coordinates and all bound names before introducing a new integration dummy. */
static expr_t *de_pde_second_order_integrate_phase(const expr_t *integrand, const expr_t *parameter,
                                                  const expr_t *phase, const expr_t *forcing)
{
    static const char *const names[] = {"t", "u", "v", "w", "r", "q", "p", "s"};
    expr_t *integral = expr_integrate(integrand, parameter);
    bool verified = integral && expr_verify_antiderivative_real_internal(integral, integrand, parameter);
    if (verified && !de_pde_second_order_has_integral(integral))
        return integral;
    expr_free(integral);

    expr_t *seed = NULL;
    const char *name = NULL;

    /* This small, fixed preference list only selects a readable dummy; the allocator supplies a fallback. */
    for (size_t i = 0u; !name && i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!de_pde_second_order_has_name(integrand, names[i]) &&
            !de_pde_second_order_has_name(phase, names[i]) && !de_pde_second_order_has_name(forcing, names[i]) &&
            !de_pde_second_order_has_name(parameter, names[i]))
            name = names[i];
    }
    if (!name) {
        seed = expr_new_integration_constant(integrand, phase, forcing);
        name = seed ? expr_symbol_name(seed) : NULL;
    }
    expr_t *dummy = name && !de_pde_second_order_has_name(integrand, name) &&
                           !de_pde_second_order_has_name(phase, name) && !de_pde_second_order_has_name(forcing, name)
                       ? expr_new_named_var(NUM_ZERO, name) : NULL;
    expr_t *display_integrand = dummy ? expr_substitute(integrand, parameter, dummy) : NULL;

    integral = display_integrand ? expr_integral_with_dummy_internal(display_integrand, parameter, dummy) : NULL;
    expr_free(display_integrand);
    expr_free(dummy);
    expr_free(seed);
    return integral;
}

/* Reduce f(a*x+b*y+c) to an ODE, including characteristic and repeated-characteristic phases. */
static expr_t *de_pde_second_order_phase_candidate(const expr_t *phase, const expr_t *forcing,
                                                   const expr_t *x, const expr_t *y, const number_t *coefficients)
{
    expr_t *a_expr = NULL;
    expr_t *after_x = NULL;
    expr_t *b_expr = NULL;
    expr_t *offset = NULL;
    expr_t *parameter = NULL;
    expr_t *reduced = NULL;
    expr_t *primitive = NULL;
    expr_t *multiplier = NULL;
    expr_t *particular = NULL;
    number_t a = num_new();
    number_t b = num_new();
    number_t divisor = num_new();
    size_t integrations = 2u;

    if (!de_linear_decompose(phase, x, &a_expr, &after_x) ||
        !de_linear_decompose(after_x, y, &b_expr, &offset) ||
        !expr_match_const_value(a_expr, &a) || !expr_match_const_value(b_expr, &b) ||
        !num_is_real(a) || !num_is_real(b) || !num_is_finite(a) || !num_is_finite(b) ||
        (num_is_zero(a) && num_is_zero(b)) || de_expr_uses(offset, x) || de_expr_uses(offset, y))
        goto cleanup;

    /* Reuse the collision-free name allocator for the temporary integration coordinate. */
    expr_t *name_seed = expr_new_integration_constant(forcing, phase, NULL);
    const char *parameter_name = name_seed ? expr_symbol_name(name_seed) : NULL;

    parameter = parameter_name && !de_pde_second_order_has_name(forcing, parameter_name) &&
                                  !de_pde_second_order_has_name(phase, parameter_name)
                    ? expr_new_named_var(NUM_NAN, parameter_name) : NULL;
    expr_free(name_seed);
    reduced = parameter ? expr_substitute(forcing, phase, parameter) : NULL;
    if (reduced && (de_expr_uses(reduced, x) || de_expr_uses(reduced, y))) {
        bool use_x = !num_is_zero(a);
        expr_t *numerator = expr_sub_simplify_owned(expr_clone(parameter), expr_clone(use_x ? after_x : offset));
        expr_t *coordinate = expr_div_simplify_owned(numerator, expr_clone(use_x ? a_expr : b_expr));
        expr_t *substituted = coordinate ? expr_substitute(forcing, use_x ? x : y, coordinate) : NULL;

        expr_free(reduced);
        reduced = substituted ? expr_simplify_owned(substituted) : NULL;
        expr_free(coordinate);
    }
    if (!reduced || de_expr_uses(reduced, x) || de_expr_uses(reduced, y))
        goto cleanup;

    number_t aa = num_sqr(a);
    number_t ab = num_mul(a, b);
    number_t bb = num_sqr(b);
    number_t axx = num_mul(coefficients[0], aa);
    number_t bxy = num_mul(coefficients[1], ab);
    number_t cyy = num_mul(coefficients[2], bb);
    number_t sum = num_add(axx, bxy);

    num_destroy(&divisor);
    divisor = num_add(sum, cyy);
    num_destroy(&sum);
    num_destroy(&cyy);
    num_destroy(&bxy);
    num_destroy(&axx);
    num_destroy(&bb);
    num_destroy(&ab);
    num_destroy(&aa);
    multiplier = expr_const_one();

    if (num_is_zero(divisor)) {
        const expr_t *coordinates[2] = {x, y};
        number_t slopes[2] = {a, b};

        integrations = 1u;
        for (size_t i = 0u; i < 2u && num_is_zero(divisor); ++i) {
            number_t diagonal = num_mul(coefficients[2u * i], slopes[i]);
            number_t twice_diagonal = num_add(diagonal, diagonal);
            number_t mixed = num_mul(coefficients[1], slopes[1u - i]);

            num_destroy(&divisor);
            divisor = num_add(twice_diagonal, mixed);
            num_destroy(&mixed);
            num_destroy(&twice_diagonal);
            num_destroy(&diagonal);
            if (!num_is_zero(divisor)) {
                expr_free(multiplier);
                multiplier = expr_clone(coordinates[i]);
            }
        }
        if (num_is_zero(divisor)) {
            size_t axis = num_is_zero(coefficients[0]) ? 1u : 0u;

            integrations = 0u;
            num_destroy(&divisor);
            divisor = num_add(coefficients[2u * axis], coefficients[2u * axis]);
            expr_free(multiplier);
            multiplier = expr_mul(coordinates[axis], coordinates[axis]);
        }
    }
    if (num_is_zero(divisor) || !num_is_finite(divisor) || !multiplier)
        goto cleanup;
    primitive = expr_div_simplify_owned(expr_clone(reduced), expr_new_const(divisor));
    for (size_t i = 0u; primitive && i < integrations; ++i) {
        expr_t *next = de_pde_second_order_integrate_phase(primitive, parameter, phase, forcing);

        expr_free(primitive);
        primitive = next;
    }
    if (primitive) {
        expr_t *composed = expr_substitute(primitive, parameter, phase);

        /* Each antiderivative is verified above; the affine chain rule supplies the PDE identity. */
        particular = composed ? expr_mul_simplify_owned(expr_clone(multiplier), composed) : NULL;
    }

cleanup:
    expr_free(multiplier);
    expr_free(primitive);
    expr_free(reduced);
    expr_free(parameter);
    expr_free(offset);
    expr_free(b_expr);
    expr_free(after_x);
    expr_free(a_expr);
    num_destroy(&divisor);
    num_destroy(&b);
    num_destroy(&a);
    return particular;
}

/* Walk the forcing tree once to find an affine phase shared by all coordinate-dependent factors. */
static expr_t *de_pde_second_order_phase_particular(const expr_t *node, const expr_t *forcing,
                                                     const expr_t *x, const expr_t *y, const number_t *coefficients)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *particular = de_pde_second_order_phase_candidate(node, forcing, x, y, coefficients);

    if (!particular && expr_child_exprs(node, &left, &right)) {
        if (left)
            particular = de_pde_second_order_phase_particular(left, forcing, x, y, coefficients);
        if (!particular && right)
            particular = de_pde_second_order_phase_particular(right, forcing, x, y, coefficients);
    }
    return particular;
}

/* Try undetermined coefficients, then a single-phase ODE reduction; use superposition for sums. */
static expr_t *de_pde_second_order_particular(const expr_t *forcing, const expr_t *x, const expr_t *y,
                                              const number_t *coefficients)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (expr_is_exact_zero(forcing))
        return expr_const_zero();
    bool is_sum = expr_match_add_sub_expr(forcing, &left, &right, &is_sub);
    expr_t *canonical = is_sum ? NULL : expr_display_expanded(forcing);
    /* Preserve distributed terms for superposition after collecting cancellations and like terms. */
    expr_t *expanded = canonical ? expr_expand_products_internal(canonical) : NULL;

    expr_free(canonical);

    if (is_sum || (expanded && expr_match_add_sub_expr(expanded, &left, &right, &is_sub))) {
        expr_t *first = de_pde_second_order_particular(left, x, y, coefficients);
        expr_t *second = first ? de_pde_second_order_particular(right, x, y, coefficients) : NULL;

        expr_free(expanded);
        return is_sub ? expr_sub_simplify_owned(first, second) : expr_add_simplify_owned(first, second);
    }
    expr_free(expanded);

    /* Fixed basis 1,x,y,xx,xy,yy; index 2 denotes an identity factor. Construct only candidates needed. */
    static const size_t multiplier_factors[6][2] = {{2u, 2u}, {0u, 2u}, {1u, 2u}, {0u, 0u}, {0u, 1u}, {1u, 1u}};
    const expr_t *coordinates[3] = {x, y, NULL};
    expr_t *particular = NULL;

    for (size_t i = 0u; !particular && i < 6u; ++i) {
        const expr_t *first = coordinates[multiplier_factors[i][0]];
        const expr_t *second = coordinates[multiplier_factors[i][1]];
        expr_t *multiplier = first ? (second ? expr_mul(first, second) : expr_clone(first)) : expr_const_one();
        expr_t *candidate = expr_mul_simplify_owned(multiplier, expr_clone(forcing));
        expr_t *applied = candidate ? de_pde_second_order_apply(candidate, x, y, coefficients) : NULL;
        expr_t *ratio = applied ? expr_div_simplify_owned(expr_clone(applied), expr_clone(forcing)) : NULL;
        number_t eigenvalue = num_new();

        if (ratio && expr_match_const_value(ratio, &eigenvalue) && num_is_finite(eigenvalue) &&
            !num_is_zero(eigenvalue)) {
            expr_t *trial = expr_div_simplify_owned(expr_clone(candidate), expr_clone(ratio));
            expr_t *response = trial ? de_pde_second_order_apply(trial, x, y, coefficients) : NULL;
            expr_t *difference = response ? expr_sub_simplify_owned(response, expr_clone(forcing)) : NULL;

            if (difference && de_pde_is_symbolically_zero(difference)) {
                particular = trial;
                trial = NULL;
            }
            expr_free(difference);
            expr_free(trial);
        }
        num_destroy(&eigenvalue);
        expr_free(ratio);
        expr_free(applied);
        expr_free(candidate);
    }
    return particular ? particular : de_pde_second_order_phase_particular(forcing, forcing, x, y, coefficients);
}

static bool de_pde_second_order_steps(const diffequ_t *de, diffequ_solve_result_t *result,
                                      const expr_t *particular)
{
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *problem = de_to_string(de, style_UNBOUND);
    char *problem_TeX = de_to_string(de, style_LATEX);
    string_t *solution_text = equ_to_text(solution, style_UNBOUND);
    char *left_TeX = solution ? expr_to_TeX_body(equ_lhs(solution)) : NULL;
    char *right_TeX = solution ? expr_to_TeX_body(equ_rhs(solution)) : NULL;
    char *particular_text = particular ? expr_to_string(particular, style_UNBOUND) : NULL;
    char *particular_TeX = particular ? expr_to_TeX_body(particular) : NULL;
    string_t *steps = NULL;
    string_t *steps_TeX = NULL;
    bool success = false;

    if (!problem || !problem_TeX || !solution_text || !left_TeX || !right_TeX || !particular_text || !particular_TeX)
        goto cleanup;
    steps = string_sprintf("Factor the homogeneous second-order constant-coefficient operator.\n"
                           "Parsed equation: %s\n"
                           "For A*u_xx + B*u_xy + C*u_yy = 0, solve A*m^2 + B*m + C = 0.\n"
                           "Distinct roots give u_h = F(m1*x+y) + G(m2*x+y).\n"
                           "A repeated root gives u_h = F(m*x+y) + x*G(m*x+y).\n"
                           "Exchange coordinates if A = 0; for u_xy = 0 use F(x) + G(y).\n"
                           "The roots are obtained by the native equation solver.\n"
                           "For forcing f, seek u_p = q*f/k with q in {1,x,y,x*x,x*y,y*y} and L(q*f) = k*f.\n"
                           "Otherwise, for f = f(s), s = a*x+b*y+c, reduce to (A*a*a+B*a*b+C*b*b)*H'' = f.\n"
                           "At resonance use a coordinate multiplier and one integration, or a quadratic multiplier.\n"
                           "Verify antiderivatives and retain exact integral nodes when no closed form is available.\n"
                           "Apply superposition to sums and verify L(u_p) = f symbolically.\n"
                           "Particular solution: u_p = %s\nSolution: %s",
                           problem, particular_text, string_c_str(solution_text));
    steps_TeX = string_sprintf("\\begin{aligned}[t]"
                               "\\text{Equation:}\\quad&%s\\\\"
                               "\\text{Characteristic polynomial:}\\quad&A m^2+B m+C=0\\\\"
                               "\\text{Distinct roots:}\\quad&u_h=F(m_1x+y)+G(m_2x+y)\\\\"
                               "\\text{Repeated root:}\\quad&u_h=F(mx+y)+xG(mx+y)\\\\"
                               "\\text{If }A=0:\\quad&\\text{exchange coordinates;}\\\\"
                               "A=C=0:\\quad&u_h=F(x)+G(y)\\\\"
                               "\\text{Single-phase reduction:}\\quad&s=ax+by+c\\\\"
                               "\\kappa\\ne0:\\quad&\\kappa H''(s)=f(s),\\quad\\kappa=Aa^2+Bab+Cb^2\\\\"
                               "\\text{Particular solution:}\\quad&u_p=%s\\\\"
                               "\\text{Verified:}\\quad&L(u_p)=f\\\\"
                               "\\text{Solution:}\\quad&%s=%s\\end{aligned}",
                               problem_TeX, particular_TeX, left_TeX, right_TeX);
    success = steps && steps_TeX && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
              de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) == 0;

cleanup:
    string_free(steps_TeX);
    string_free(steps);
    free(right_TeX);
    free(left_TeX);
    free(particular_TeX);
    free(particular_text);
    string_free(solution_text);
    free(problem_TeX);
    free(problem);
    return success;
}

/* Combine characteristic families with a verified particular solution for a constant-coefficient PDE. */
diffequ_solve_result_t *de_pde_solve_second_order_constant(const diffequ_t *de, const expr_t *residual,
                                                         bool include_steps)
{
    const expr_t *derivatives[4] = {NULL, NULL, NULL, NULL};
    const expr_t *dependent = NULL;
    number_t coefficients[3] = {num_clone(NUM_ZERO), num_clone(NUM_ZERO), num_clone(NUM_ZERO)};
    expr_t *right = NULL;
    expr_t *forcing = NULL;
    expr_t *particular = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;

    if (!de || !residual || de->independent_count != 2u || de->condition_count != 0u)
        goto cleanup;
    if (!de_pde_second_order_derivatives(residual, de->independent_vars[0], de->independent_vars[1], &dependent,
                                          derivatives, NULL) || !dependent ||
        !de_pde_second_order_coefficients(residual, derivatives, coefficients, &forcing) ||
        de_expr_uses(forcing, dependent))
        goto cleanup;
    particular = de_pde_second_order_particular(forcing, de->independent_vars[0], de->independent_vars[1], coefficients);
    if (!particular)
        goto cleanup;
    right = de_pde_second_order_family(de->independent_vars[0], de->independent_vars[1], coefficients[0],
                                       coefficients[1], coefficients[2]);
    right = right ? expr_add_simplify_owned(right, expr_clone(particular)) : NULL;
    solution = right ? de_pde_solution_equation(dependent, right) : NULL;
    if (!solution)
        goto cleanup;
    result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                                 "solved using second-order characteristic coordinates");
    if (!result || de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;
    if (include_steps && !de_pde_second_order_steps(de, result, particular)) {
        de_solve_result_free(result);
        result = NULL;
    }

cleanup:
    equ_free(solution);
    expr_free(right);
    expr_free(particular);
    expr_free(forcing);
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&coefficients[i]);
    return result;
}
