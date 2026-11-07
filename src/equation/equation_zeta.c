/**
 * @file equation_zeta.c
 * @brief Equation solving helpers for zeta-related roots.
 *
 * Recognises and processes supported zeta-equation cases within the equation solver. Keep these specialised
 * numerical and structural rules separate from general polynomial factorisation. Zero-target searches use
 * Lambert W starting estimates, staged complex Newton refinement and a bounded height-grid fallback.
 *
 * This belongs to the equation_t implementation. Public callers work with equations and solution collections;
 * internal matching or numerical attempts are not independent application APIs.
 */

#include <stdbool.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

enum {
    ZETA_ZERO_HEIGHT_LIMIT = 80,
    ZETA_VALUE_HEIGHT_LIMIT = 50,
    ZETA_ZERO_ROOT_LIMIT = 40
};

static const expr_t *equ_find_zeta_term(const expr_t *expr, const expr_t *prototype, expr_t *wrt, size_t depth)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *variables[] = {wrt};
    size_t index = 0u;
    bool subtract = false;

    if (!expr || depth > 32u)
        return NULL;
    if (!prototype) {
        expr_t *order = NULL;
        expr_t *closed = expr_infinite_power_sum_closed_form(expr, &order);
        bool match = closed && expr_match_var_expr(order, 1u, variables, &index);

        expr_free(order);
        expr_free(closed);
        if (match)
            return expr;
    }
    if (expr_match_same_reapplicable_unary_function(expr, prototype, &left) &&
        expr_match_var_expr(left, 1u, variables, &index))
        return expr;
    if (expr_match_add_sub_expr(expr, &left, &right, &subtract) || expr_match_mul_expr(expr, &left, &right) ||
        expr_match_div_expr(expr, &left, &right)) {
        const expr_t *found = equ_find_zeta_term(left, prototype, wrt, depth + 1u);

        return found ? found : equ_find_zeta_term(right, prototype, wrt, depth + 1u);
    }
    if (expr_match_neg_expr(expr, &left))
        return equ_find_zeta_term(left, prototype, wrt, depth + 1u);
    return NULL;
}

/* Recognise a real affine equation in zeta(wrt), not an arbitrary expression containing zeta. */
static bool equ_is_real_zeta_equation(const expr_t *residual, expr_t *wrt, bool *zero_target)
{
    expr_t *zeta = expr_zeta(wrt);
    const expr_t *term = zeta ? equ_find_zeta_term(residual, zeta, wrt, 0u) : NULL;
    expr_t *placeholder = expr_new_var(NUM_NAN);
    expr_t *affine = term && placeholder ? expr_substitute(residual, term, placeholder) : NULL;
    number_t constant = NUM_NAN;
    number_t coefficient = NUM_NAN;
    expr_t *variables[] = {wrt};
    bool uses_wrt = true;
    bool matched = affine && expr_collect_var_usage(affine, 1u, variables, &uses_wrt) && !uses_wrt &&
                   equ_match_affine_linear_expr(affine, placeholder, true, &constant, &coefficient) &&
                   num_is_finite(constant) && num_is_real(constant) &&
                   num_is_finite(coefficient) && num_is_real(coefficient);

    *zero_target = matched && num_is_zero(constant);
    num_destroy(&coefficient);
    num_destroy(&constant);
    expr_free(affine);
    expr_free(placeholder);
    expr_free(zeta);
    return matched;
}

static bool equ_zeta_root_in_bounds(number_t root, bool zero_target)
{
    number_t real = num_real_part(root);
    number_t imag = num_imag_part(root);
    number_t magnitude = num_abs(imag);
    number_t right = num_create_from_long(4L);
    number_t top = num_create_from_long(zero_target ? ZETA_ZERO_HEIGHT_LIMIT : ZETA_VALUE_HEIGHT_LIMIT);
    bool inside = num_is_finite(root) && num_le(magnitude, top) &&
                  (zero_target ? (num_gt(real, NUM_ZERO) && num_lt(real, NUM_ONE))
                               : (num_gt(real, NUM_ONE) && num_le(real, right)));

    num_destroy(&top);
    num_destroy(&right);
    num_destroy(&magnitude);
    num_destroy(&imag);
    num_destroy(&real);
    return inside;
}

static bool equ_zeta_root_seen(const equation_solutions_t *solutions, number_t root, number_t tolerance)
{
    /* This bounded search stores at most 40 roots; compare numbers, not rounded display text. */
    for (size_t i = 0u; i < solutions->count; ++i) {
        number_t existing = expr_eval(equ_rhs(solutions->solutions[i]));
        number_t difference = num_sub(existing, root);
        number_t distance = num_abs(difference);
        bool same = num_le(distance, tolerance);

        num_destroy(&distance);
        num_destroy(&difference);
        num_destroy(&existing);
        if (same)
            return true;
    }
    return false;
}

static bool equ_zeta_residual_verified(expr_t *residual, expr_t *wrt, number_t root, number_t tolerance)
{
    number_t value;
    number_t magnitude;
    bool verified;

    expr_set_val(wrt, root);
    value = expr_eval(residual);
    magnitude = num_abs(value);
    verified = num_is_finite(magnitude) && num_le(magnitude, tolerance);
    num_destroy(&magnitude);
    num_destroy(&value);
    return verified;
}

/* Invert the smooth zero-counting approximation; W0 also gives the first, negative-argument estimate. */
static number_t equ_zeta_zero_seed(long index)
{
    number_t order = num_create_from_frac(8L * index - 11L, 8L);
    number_t argument = num_div(order, NUM_E);
    number_t lambert = num_lambert_w0(argument);
    number_t turn = num_mul(NUM_TWO, NUM_PI);
    number_t numerator = num_mul(turn, order);
    number_t height = num_div(numerator, lambert);
    number_t imaginary = num_mul(height, NUM_I);
    number_t seed = num_add(NUM_HALF, imaginary);

    num_destroy(&imaginary);
    num_destroy(&height);
    num_destroy(&numerator);
    num_destroy(&turn);
    num_destroy(&lambert);
    num_destroy(&argument);
    num_destroy(&order);
    return seed;
}

/* Both seed strategies share refinement, original-equation verification and conjugate checks. */
static int equ_zeta_try_seed(expr_t *residual, expr_t *verification_residual, expr_t *wrt,
                            equation_solutions_t *solutions, number_t seed, bool zero_target, size_t digits,
                            number_t tolerance, number_t duplicate_tolerance, const expr_newton_region_t *region)
{
    expr_goal_seek_options_t options = {
        .precision_digits = 24u, .max_iterations = 32u, .allow_complex = true,
        .simplify_result = false, .tolerance = NUM_NAN
    };
    number_t candidate;
    int rc = 0;

    if (!num_is_finite(seed))
        return 0;
    expr_set_val(wrt, seed);
    if (expr_goal_seek_newton_one(residual, wrt, NUM_ZERO, &options, region) != 0)
        return 0;
    candidate = expr_eval(wrt);
    bool distinct = equ_zeta_root_in_bounds(candidate, zero_target) &&
                    !equ_zeta_root_seen(solutions, candidate, duplicate_tolerance);

    num_destroy(&candidate);
    if (!distinct)
        return 0;
    /* Raise accuracy with quadratic convergence rather than doing every remaining iteration at full precision. */
    for (size_t refinement = 24u; refinement < digits;) {
        refinement = refinement > digits / 2u ? digits : refinement * 2u;
        options.precision_digits = refinement;
        options.max_iterations = refinement > 48u ? refinement : 48u;
        if (expr_goal_seek_newton_one(residual, wrt, NUM_ZERO, &options, region) != 0)
            return 0;
    }
    candidate = expr_eval(wrt);
    if (equ_zeta_root_in_bounds(candidate, zero_target) &&
        equ_zeta_residual_verified(verification_residual, wrt, candidate, tolerance) &&
        !equ_zeta_root_seen(solutions, candidate, duplicate_tolerance)) {
        number_t conjugate = num_conj(candidate);

        rc = equ_append_solution_value(wrt, candidate, solutions);
        if (rc == 0 && !num_is_real(candidate) &&
            !equ_zeta_root_seen(solutions, conjugate, duplicate_tolerance) &&
            equ_zeta_residual_verified(verification_residual, wrt, conjugate, tolerance))
            rc = equ_append_solution_value(wrt, conjugate, solutions);
        num_destroy(&conjugate);
    }
    num_destroy(&candidate);
    return rc;
}

int equ_try_search_zeta_roots(const equation_t *equation, equation_solutions_t *solutions)
{
    expr_bindings_t *bindings = equ_bindings(equation);
    expr_t *wrt = NULL;
    expr_t *residual = NULL;
    expr_t *verification_residual = NULL;
    number_t original = NUM_NAN;
    number_t duplicate_tolerance = NUM_NAN;
    number_t tolerance = NUM_NAN;
    expr_newton_region_t region = {NUM_NAN, NUM_NAN, NUM_NAN};
    size_t digits = num_get_default_prec_digits();
    bool zero_target = false;
    bool series_domain = false;
    int rc = 1;

    for (size_t i = 0u; bindings && i < expr_bindings_count(bindings); ++i) {
        if (expr_bindings_is_constant_at(bindings, i))
            continue;
        if (wrt)
            return 1;
        wrt = expr_bindings_expr_at(bindings, i);
    }
    if (!wrt)
        return 1;
    original = expr_eval(wrt);
    if (!equ_computation_active()) {
        num_destroy(&original);
        return -1;
    }
    residual = equ_residual(equation);
    if (residual) {
        const expr_t *series = equ_find_zeta_term(residual, NULL, wrt, 0u);

        verification_residual = residual;
        expr_retain(verification_residual);
        if (series) {
            expr_t *closed = expr_infinite_power_sum_closed_form(series, NULL);
            expr_t *working = closed ? expr_substitute(residual, series, closed) : NULL;

            expr_free(closed);
            expr_free(residual);
            residual = working;
            series_domain = true;
        }
    }
    if (!residual || !equ_is_real_zeta_equation(residual, wrt, &zero_target))
        goto cleanup;
    if (series_domain && zero_target) {
        solutions->search_kind = EQUATION_SEARCH_ZETA_SERIES_EMPTY;
        solutions->status = EQUATION_SOLVE_NO_SOLUTIONS;
        rc = 0;
        goto cleanup;
    }
    /* An explicitly supplied seed continues to request the existing single-root solver. */
    if (!num_is_nan(original))
        goto cleanup;

    digits = digits > 0u ? digits : 64u;
    tolerance = num_pow10(-(int)digits);
    duplicate_tolerance = num_create_from_string("1e-12");
    region.real_min = num_create_from_long(-2L);
    region.real_max = num_create_from_long(6L);
    long height_limit = zero_target ? ZETA_ZERO_HEIGHT_LIMIT : ZETA_VALUE_HEIGHT_LIMIT;

    region.imaginary_magnitude_max = num_create_from_long(height_limit + 10L);
    solutions->search_kind = zero_target ? EQUATION_SEARCH_ZETA_ZEROS : EQUATION_SEARCH_ZETA_VALUE;
    solutions->status = EQUATION_SOLVE_UNSOLVED;
    rc = 0;
    /* These estimates apply only to zeros, never to a non-zero zeta level or an authored starting binding. */
    for (long index = 1L; zero_target && index <= ZETA_ZERO_ROOT_LIMIT / 2; ++index) {
        if (!equ_computation_step("equation.zeta_seed")) {
            rc = -1;
            break;
        }
        number_t seed = equ_zeta_zero_seed(index);

        rc = equ_zeta_try_seed(residual, verification_residual, wrt, solutions, seed, zero_target, digits,
                               tolerance, duplicate_tolerance, &region);
        num_destroy(&seed);
        if (rc < 0 || solutions->count >= ZETA_ZERO_ROOT_LIMIT)
            break;
    }
    /* Retain the bounded height grid for missed zeros and all non-zero target searches. */
    for (long height = zero_target ? 2L : 0L;
         rc == 0 && (!zero_target || solutions->count < ZETA_ZERO_ROOT_LIMIT) && height <= height_limit;
         height += zero_target ? 1L : 3L) {
        if (!equ_computation_step("equation.zeta_seed")) {
            rc = -1;
            break;
        }
        number_t imaginary_height = num_create_from_long(height);
        number_t imaginary = num_mul(imaginary_height, NUM_I);
        number_t seed = num_add(zero_target ? NUM_HALF : NUM_TWO, imaginary);

        rc = equ_zeta_try_seed(residual, verification_residual, wrt, solutions, seed, zero_target, digits,
                               tolerance, duplicate_tolerance, &region);
        num_destroy(&seed);
        num_destroy(&imaginary);
        num_destroy(&imaginary_height);
    }

cleanup:
    expr_set_val(wrt, original);
    num_destroy(&region.imaginary_magnitude_max);
    num_destroy(&region.real_max);
    num_destroy(&region.real_min);
    num_destroy(&tolerance);
    num_destroy(&duplicate_tolerance);
    num_destroy(&original);
    expr_free(verification_residual);
    expr_free(residual);
    if (!equ_computation_active()) {
        equ_solutions_clear(solutions);
        return -1;
    }
    return rc;
}
