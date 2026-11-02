/**
 * @file diffequ_riccati_bessel.c
 * @brief Real one-constant Bessel notation for Riccati equations with positive even monomial potentials.
 *
 * Cancels the auxiliary solution's common scale before presenting the first-order solution. Absolute values
 * and signs preserve its real continuation to negative coordinates. Removable origin values are shown
 * separately; the independently owned analytic evaluation graph remains regular there.
 * All presentation is supplied as mathematical nodes, with TeX syntax owned by the TeX module.
 */
#include <limits.h>
#include <stdlib.h>

#include "TeX-t.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

struct _de_riccati_bessel_t {
    expr_t *x, *sign, *root, *argument, *speed;
    expr_t *negative, *positive, *upper, *lower;
    expr_t *first, *second, *first_origin, *second_origin_slope, *scale;
};

static expr_t *power(const expr_t *base, long exponent)
{
    expr_t *order = expr_const_long(exponent);
    expr_t *result = order ? expr_pow_xp(base, order) : NULL;
    expr_free(order);
    return result;
}

static expr_t *at(const expr_t *expression, const expr_t *x, const expr_t *point)
{
    return expr_simplify_owned(expr_substitute(expression, x, point));
}

static bool finite_real(const expr_t *expression, bool nonzero)
{
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    bool valid = num_is_real(value) && num_is_finite(value) && (!nonzero || !num_is_zero(value));
    num_destroy(&value);
    return valid;
}

/* The owning fields form a fixed set; each expression is released exactly once. */
void de_riccati_bessel_free(de_riccati_bessel_t *bessel)
{
    if (!bessel)
        return;
    expr_free(bessel->scale);
    expr_free(bessel->second_origin_slope); expr_free(bessel->first_origin);
    expr_free(bessel->second); expr_free(bessel->first);
    expr_free(bessel->lower); expr_free(bessel->upper);
    expr_free(bessel->positive); expr_free(bessel->negative);
    expr_free(bessel->speed); expr_free(bessel->argument);
    expr_free(bessel->root); expr_free(bessel->sign); expr_free(bessel->x);
    free(bessel);
}

/* Match the family structurally, without choosing a coordinate name or one particular polynomial degree. */
de_riccati_bessel_t *de_riccati_bessel_new(const expr_t *potential, const expr_t *x)
{
    number_t *coefficients = NULL;
    size_t degree = 0u;
    de_riccati_bessel_t *b = NULL;
    expr_t *root_k = NULL, *alpha = NULL, *order = NULL, *negative_order = NULL;
    expr_t *absolute = NULL, *alpha_order = NULL, *gamma_minus = NULL, *gamma_plus = NULL;
    if (!equ_match_polynomial_alloc(potential, x, &coefficients, &degree))
        return NULL;
    if (degree % 2u || degree > (size_t)(LONG_MAX - 3L) || !num_is_real(coefficients[degree]) ||
        !num_is_finite(coefficients[degree]) || !num_gt(coefficients[degree], NUM_ZERO))
        goto cleanup;
    for (size_t i = 0u; i < degree; ++i)
        if (!num_is_zero(coefficients[i]))
            goto cleanup;

    b = calloc(1u, sizeof *b);
    if (!b)
        goto cleanup;
    long half = (long)degree / 2L, m = (long)degree + 2L;
    b->x = expr_clone(x);
    b->sign = expr_sgn(x);
    absolute = expr_abs(x);
    b->root = absolute ? expr_sqrt(absolute) : NULL;
    root_k = de_simplify_unary_owned(expr_new_const(coefficients[degree]), expr_sqrt);
    alpha = expr_div_simplify_owned(expr_clone(root_k), expr_const_long(m));
    order = expr_div_simplify_owned(expr_const_one(), expr_const_long(m));
    negative_order = expr_negate_owned(expr_clone(order));
    expr_t *argument_power = power((half + 1L) % 2L ? absolute : x, half + 1L);
    b->argument = expr_mul_simplify_owned(expr_mul_simplify_owned(expr_const_long(2L), expr_clone(alpha)),
                                         argument_power);
    b->speed = expr_mul_simplify_owned(expr_clone(root_k), power(x, half));
    if (!(half % 2L)) {
        expr_t *signed_speed = expr_mul_simplify_owned(expr_clone(b->sign), b->speed);
        b->speed = signed_speed;
    }
    expr_t *upper_order = expr_add_long(negative_order, 1L), *lower_order = expr_add_long(order, -1L);
    b->negative = expr_bessel_j(negative_order, b->argument);
    b->positive = expr_bessel_j(order, b->argument);
    b->upper = expr_bessel_j(upper_order, b->argument);
    b->lower = expr_bessel_j(lower_order, b->argument);
    expr_free(lower_order); expr_free(upper_order);
    b->first = expr_mul(b->root, b->negative);
    b->second = expr_mul_simplify_owned(expr_clone(b->sign), expr_mul(b->root, b->positive));
    alpha_order = expr_pow_xp(alpha, order);
    expr_t *minus_parameter = expr_add_long(negative_order, 1L), *plus_parameter = expr_add_long(order, 1L);
    gamma_minus = expr_gamma(minus_parameter);
    gamma_plus = expr_gamma(plus_parameter);
    expr_free(plus_parameter); expr_free(minus_parameter);
    b->first_origin = expr_div_simplify_owned(expr_const_one(), expr_mul(alpha_order, gamma_minus));
    b->second_origin_slope = expr_div(alpha_order, gamma_plus);
    /* Gamma(1+nu)=nu*Gamma(nu); keep the scalar together before the gamma quotient. */
    expr_t *twice_order = expr_mul_long(order, 2L);
    expr_t *alpha_square_order = expr_pow_xp(alpha, twice_order);
    expr_t *scalar = expr_div_simplify_owned(alpha_square_order, expr_clone(order));
    number_t scalar_value = scalar ? expr_eval(scalar) : num_clone(NUM_NAN);
    expr_t *pretty_scalar = num_is_exact(scalar_value) && num_is_finite(scalar_value)
        ? expr_new_const(scalar_value) : scalar ? expr_beautify(scalar) : NULL;
    num_destroy(&scalar_value);
    expr_t *gamma_order = expr_gamma(order), *pi = expr_new_named_const(NUM_PI, "@pi");
    /* Reflect Gamma(1-nu)/Gamma(nu) to pi/(sin(pi*nu)*Gamma(nu)^2). */
    expr_t *sine = expr_exact_sin_pi_ratio(1L, (unsigned long)m);
    if (!sine)
        sine = de_simplify_unary_owned(expr_mul(pi, order), expr_sin);
    expr_t *reflection = expr_div_simplify_owned(expr_const_one(), sine);
    expr_t *reflected_scalar = expr_mul_simplify_owned(pretty_scalar, expr_mul_simplify_owned(pi, reflection));
    b->scale = expr_div_simplify_owned(reflected_scalar, power(gamma_order, 2L));
    expr_free(gamma_order); expr_free(scalar); expr_free(twice_order);
    if (!b->x || !b->sign || !b->root || !b->argument || !b->speed || !b->negative || !b->positive ||
        !b->upper || !b->lower || !b->first || !b->second || !b->first_origin ||
        !b->second_origin_slope || !b->scale) {
        de_riccati_bessel_free(b);
        b = NULL;
    }

cleanup:
    expr_free(gamma_plus); expr_free(gamma_minus); expr_free(alpha_order);
    expr_free(absolute); expr_free(negative_order); expr_free(order); expr_free(alpha); expr_free(root_k);
    for (size_t i = 0u; i <= degree; ++i)
        num_destroy(&coefficients[i]);
    free(coefficients);
    return b;
}

/* Keep the sole displayed arbitrary constant consistent with the analytic evaluation basis. */
const expr_t *de_riccati_bessel_scale(const de_riccati_bessel_t *bessel)
{
    return bessel ? bessel->scale : NULL;
}

/* Share only expression views; callers never inspect the private owner. */
void de_riccati_bessel_basis(const de_riccati_bessel_t *bessel, const expr_t **first, const expr_t **second)
{
    *first = bessel ? bessel->first : NULL;
    *second = bessel ? bessel->second : NULL;
}

static bool weights(const de_riccati_bessel_t *b, const expr_t *a, const expr_t *shift,
                     const expr_t *constant, const expr_t *point, const expr_t *value,
                     expr_t **first, expr_t **second)
{
    if (!point) {
        *first = constant ? expr_const_one() : expr_const_zero();
        *second = constant ? expr_clone(constant) : expr_const_one();
        return *first && *second;
    }
    expr_t *slope = expr_negate_owned(expr_add_simplify_owned(
        expr_mul_simplify_owned(at(a, b->x, point), expr_clone(value)), at(shift, b->x, point)));
    if (expr_is_exact_zero(point)) {
        *first = expr_clone(b->second_origin_slope);
        *second = expr_mul_simplify_owned(slope, expr_clone(b->first_origin));
        return *first && *second;
    }
    expr_t *first_at = at(b->first, b->x, point), *second_at = at(b->second, b->x, point);
    expr_t *first_slope = expr_negate_owned(expr_mul_simplify_owned(expr_clone(b->speed),
                                                                  expr_mul(b->root, b->upper)));
    expr_t *second_slope = expr_mul_simplify_owned(expr_mul(b->sign, b->speed), expr_mul(b->root, b->lower));
    expr_t *first_slope_at = at(first_slope, b->x, point), *second_slope_at = at(second_slope, b->x, point);
    *first = expr_sub_simplify_owned(second_slope_at, expr_mul(slope, second_at));
    *second = expr_sub_simplify_owned(expr_mul(slope, first_at), first_slope_at);
    expr_free(second_slope); expr_free(first_slope);
    expr_free(second_at); expr_free(first_at); expr_free(slope);
    return *first && *second;
}

static const TeX_model_node_t *relation(notation_build_t *build, const expr_t *x, bool zero)
{
    const TeX_model_node_t *children[] = {notation_build_expr(build, x),
        notation_build_literal(build, TeX_NODE_NUMBER, "0")};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
        .relation = zero ? TeX_RELATION_EQUAL : TeX_RELATION_NOT_EQUAL, .child_count = 2u}, children);
}

/* The display is a faithful real extension of the underlying analytic solution, with just one free constant. */
bool de_riccati_bessel_present(const de_riccati_bessel_t *b, equation_t *solution,
                              const expr_t *a, const expr_t *shift, const expr_t *constant,
                              const expr_t *point, const expr_t *value)
{
    expr_t *first = NULL, *second = NULL, *right = NULL, *origin = NULL;
    string_t *left_text = NULL, *right_text = NULL, *formula_text = NULL, *origin_text = NULL;
    notation_build_t *build = NULL;
    bool valid = false;
    if (!b || !solution || !weights(b, a, shift, constant, point, value, &first, &second))
        goto cleanup;
    if (point && finite_real(first, true)) {
        second = expr_div_simplify_owned(second, expr_clone(first));
        expr_free(first);
        first = expr_const_one();
    }
    expr_t *signed_second = expr_mul_simplify_owned(expr_clone(second), expr_clone(b->sign));
    expr_t *numerator = expr_sub_simplify_owned(expr_mul(first, b->upper), expr_mul(signed_second, b->lower));
    expr_t *denominator = expr_add_simplify_owned(expr_mul(first, b->negative), expr_mul(signed_second, b->positive));
    expr_t *quotient = expr_div(numerator, denominator);
    right = expr_div_simplify_owned(
        expr_sub_simplify_owned(expr_mul_simplify_owned(expr_clone(b->speed), quotient), expr_clone(shift)),
        expr_clone(a));
    expr_free(denominator); expr_free(numerator); expr_free(signed_second);
    expr_t *zero = expr_const_zero();
    expr_t *a_zero = at(a, b->x, zero), *shift_zero = at(shift, b->x, zero);
    if (finite_real(a_zero, true) && finite_real(shift_zero, false) && !expr_is_exact_zero(first)) {
        origin = point && expr_is_exact_zero(point) ? expr_clone(value) : expr_div_simplify_owned(
            expr_negate_owned(expr_add_simplify_owned(
                expr_mul_simplify_owned(expr_clone(b->scale), expr_div(second, first)), expr_clone(shift_zero))),
            expr_clone(a_zero));
    }
    expr_free(shift_zero); expr_free(a_zero); expr_free(zero);
    left_text = expr_to_text(equ_lhs(solution), style_UNBOUND);
    formula_text = right ? expr_to_text(right, style_UNBOUND) : NULL;
    origin_text = origin ? expr_to_text(origin, style_UNBOUND) : NULL;
    const char *x_name = expr_symbol_name(b->x);
    right_text = formula_text && left_text && x_name ? (origin_text
        ? string_sprintf("%S; %s ≠ 0; %S(0) = %S", formula_text, x_name, left_text, origin_text)
        : string_sprintf("%S; %s ≠ 0", formula_text, x_name)) : NULL;
    build = right_text ? notation_build_new() : NULL;
    if (build) {
        const TeX_model_node_t *children[] = {notation_build_expr(build, right), relation(build, b->x, false),
            origin ? notation_build_expr(build, origin) : NULL, origin ? relation(build, b->x, true) : NULL};
        const TeX_model_node_t *body = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_CASES, .child_count = origin ? 4u : 2u}, children);
        valid = de_notation_display(solution, left_text, right_text, build, body);
    }

cleanup:
    notation_build_free(build);
    string_free(origin_text); string_free(formula_text); string_free(right_text); string_free(left_text);
    expr_free(origin); expr_free(right); expr_free(second); expr_free(first);
    return valid;
}
