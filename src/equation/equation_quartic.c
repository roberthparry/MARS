/**
 * @file equation_quartic.c
 * @brief Quartic root solving and factor reductions.
 *
 * Combines exact-factor attempts, numerical evaluation and distinct-root assembly for fourth-degree polynomials.
 * The companion quartic surd unit constructs exact radical expressions where supported.
 *
 * This belongs to the equation.h implementation. Public callers work with equations and solution collections;
 * internal matching or numerical attempts are not independent application APIs.
 */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#include "number.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/*
 * Newton iteration finds
 * one root.  A non-real root of a real-coefficient quartic contributes its
 * conjugate, allowing real quadratic deflation before the quadratic solver
 * finishes. Rational quadratic factors are certified against the original
 * coefficients before their roots are retained as exact expressions.
 * Other exact non-degenerate real quartics retain Ferrari radicals. Remaining
 * cases use numerical deflation and the cubic solver.
 */
enum {
    EQU_QUARTIC_DEGREE = 4u,
    EQU_QUARTIC_COEFF_COUNT = EQU_QUARTIC_DEGREE + 1u,
    EQU_QUARTIC_NEWTON_SEEDS = 37u,
    EQU_QUARTIC_NEWTON_ITERATIONS = 1024u
};

static void equ_quartic_init_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
}

static void equ_quartic_destroy_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
}

static void equ_quartic_eval(const number_t *coeffs, number_t z, number_t *value_out, number_t *derivative_out)
{
    /* Simultaneous Horner evaluation of p(z) and p'(z). */
    number_t value = num_clone(coeffs[EQU_QUARTIC_DEGREE]);
    number_t derivative = num_new();

    for (size_t degree = EQU_QUARTIC_DEGREE; degree-- > 0u;) {
        number_t next_derivative = num_mul(derivative, z);
        number_t derivative_sum = num_add(next_derivative, value);
        number_t next_value = num_mul(value, z);
        number_t value_sum = num_add(next_value, coeffs[degree]);

        num_destroy(&next_value);
        num_destroy(&value);
        value = value_sum;
        num_destroy(&next_derivative);
        num_destroy(&derivative);
        derivative = derivative_sum;
    }

    if (value_out) {
        num_destroy(value_out);
        *value_out = value;
    } else {
        num_destroy(&value);
    }
    if (derivative_out) {
        num_destroy(derivative_out);
        *derivative_out = derivative;
    } else {
        num_destroy(&derivative);
    }
}

static bool equ_quartic_magnitude_le(number_t value, number_t tolerance)
{
    number_t magnitude = num_abs(value);
    bool close = num_is_finite(magnitude) && num_le(magnitude, tolerance);

    num_destroy(&magnitude);
    return close;
}

static number_t equ_quartic_newton_tolerance(void)
{
    size_t digits = num_get_default_prec_digits();
    int exponent;

    if (digits < 24u)
        digits = 24u;
    exponent = digits > 8u ? -(int)(digits - 8u) : -16;
    return num_pow10(exponent);
}

static number_t equ_quartic_distinct_tolerance(number_t newton_tolerance)
{
    number_t square_root = num_sqrt(newton_tolerance);
    number_t tolerance = num_mul_long(square_root, 100L);

    num_destroy(&square_root);
    return tolerance;
}

static double equ_quartic_root_bound(const number_t *coeffs)
{
    /* Cauchy's 1 + max |a_i/a_n| bound contains every polynomial root. */
    number_t max_ratio = num_new();
    double bound;

    for (size_t i = 0u; i < EQU_QUARTIC_DEGREE; ++i) {
        number_t ratio = num_div(coeffs[i], coeffs[EQU_QUARTIC_DEGREE]);
        number_t magnitude = num_abs(ratio);

        if (num_gt(magnitude, max_ratio)) {
            num_destroy(&max_ratio);
            max_ratio = num_clone(magnitude);
        }
        num_destroy(&magnitude);
        num_destroy(&ratio);
    }

    bound = 1.0 + num_to_double(max_ratio);
    num_destroy(&max_ratio);
    return isfinite(bound) && bound > 0.0 ? bound : 1.0;
}

static number_t equ_quartic_seed(size_t index, double bound)
{
    if (index == 0u)
        return num_clone(NUM_ZERO);
    if (index == 1u)
        return num_clone(NUM_ONE);
    if (index == 2u)
        return num_clone(NUM_NEG_ONE);
    if (index == 3u)
        return num_clone(NUM_I);
    if (index == 4u)
        return num_neg(NUM_I);

    {
        const size_t circle_count = EQU_QUARTIC_NEWTON_SEEDS - 5u;
        const size_t circle_index = index - 5u;
        const double angle = 2.0 * acos(-1.0) * ((double)circle_index + 0.5) / (double)circle_count;
        number_t real = num_create_from_double(bound * cos(angle));
        number_t imag = num_create_from_double(bound * sin(angle));
        number_t imag_term = num_mul(NUM_I, imag);
        number_t seed = num_add(real, imag_term);

        num_destroy(&imag_term);
        num_destroy(&imag);
        num_destroy(&real);
        return seed;
    }
}

static bool equ_quartic_newton_from(const number_t *coeffs, number_t start, number_t tolerance, number_t *root_out)
{
    number_t z = num_clone(start);
    number_t value = num_new();
    number_t derivative = num_new();
    bool converged = false;

    for (size_t iteration = 0u; iteration < EQU_QUARTIC_NEWTON_ITERATIONS; ++iteration) {
        number_t step;
        number_t next;

        equ_quartic_eval(coeffs, z, &value, &derivative);
        if (equ_quartic_magnitude_le(value, tolerance)) {
            converged = true;
            break;
        }
        if (!num_is_finite(derivative) || equ_quartic_magnitude_le(derivative, tolerance))
            break;

        step = num_div(value, derivative);
        next = num_sub(z, step);
        num_destroy(&step);
        if (!num_is_finite(next)) {
            num_destroy(&next);
            break;
        }
        num_destroy(&z);
        z = next;
    }

    if (!converged) {
        equ_quartic_eval(coeffs, z, &value, NULL);
        converged = equ_quartic_magnitude_le(value, tolerance);
    }
    if (converged) {
        num_destroy(root_out);
        *root_out = num_clone(z);
    }

    num_destroy(&derivative);
    num_destroy(&value);
    num_destroy(&z);
    return converged;
}

static bool equ_quartic_find_root(const number_t *coeffs, number_t tolerance, number_t *root_out)
{
    const double bound = equ_quartic_root_bound(coeffs);

    for (size_t i = 0u; i < EQU_QUARTIC_NEWTON_SEEDS; ++i) {
        number_t seed = equ_quartic_seed(i, bound);
        bool found = equ_quartic_newton_from(coeffs, seed, tolerance, root_out);

        num_destroy(&seed);
        if (found)
            return true;
    }
    return false;
}

static void equ_quartic_synthetic_divide(const number_t *coeffs, number_t root, number_t *cubic)
{
    /* Descending synthetic division by (x - root). */
    num_destroy(&cubic[3]);
    cubic[3] = num_clone(coeffs[4]);

    for (size_t degree = 3u; degree > 0u; --degree) {
        number_t product = num_mul(root, cubic[degree]);
        number_t next = num_add(coeffs[degree], product);

        num_destroy(&cubic[degree - 1u]);
        cubic[degree - 1u] = next;
        num_destroy(&product);
    }
}

static number_t equ_quartic_polish_root(const number_t *coeffs, number_t root, number_t tolerance)
{
    number_t polished = num_new();

    if (equ_quartic_newton_from(coeffs, root, tolerance, &polished))
        return polished;
    num_destroy(&polished);
    return num_clone(root);
}

static number_t equ_quartic_snap_gaussian_integer(const number_t *coeffs, number_t root)
{
    number_t real = num_real_part(root);
    number_t imag = num_imag_part(root);
    double real_d = num_to_double(real);
    double imag_d = num_to_double(imag);
    bool bounded = isfinite(real_d) && isfinite(imag_d) && fabs(real_d) < 1000000.0 && fabs(imag_d) < 1000000.0;
    long real_i = bounded ? lround(real_d) : 0L;
    long imag_i = bounded ? lround(imag_d) : 0L;
    number_t candidate = num_new();
    number_t value = num_new();
    bool in_range = bounded && fabs(real_d - (double)real_i) < 1e-6 && fabs(imag_d - (double)imag_i) < 1e-6;

    if (in_range) {
        number_t real_part = num_create_from_long(real_i);
        number_t imag_part = num_create_from_long(imag_i);
        number_t imag_term = num_mul(NUM_I, imag_part);

        num_destroy(&candidate);
        candidate = imag_i == 0L ? num_clone(real_part) : num_add(real_part, imag_term);
        equ_quartic_eval(coeffs, candidate, &value, NULL);
        num_destroy(&imag_term);
        num_destroy(&imag_part);
        num_destroy(&real_part);
        if (num_is_zero(value)) {
            num_destroy(&value);
            num_destroy(&imag);
            num_destroy(&real);
            return candidate;
        }
    }

    num_destroy(&value);
    num_destroy(&candidate);
    num_destroy(&imag);
    num_destroy(&real);
    return num_clone(root);
}

static bool equ_quartic_roots_close(number_t left, number_t right, number_t tolerance)
{
    number_t difference = num_sub(left, right);
    bool close = equ_quartic_magnitude_le(difference, tolerance);

    num_destroy(&difference);
    return close;
}


static bool equ_quartic_certify_rational_root(const number_t *coeffs, number_t root)
{
    if (!num_is_real(root))
        return false;
    number_t candidate = equ_polynomial_rational_candidate(root);
    number_t residual = num_new();

    equ_quartic_eval(coeffs, candidate, &residual, NULL);
    bool exact = num_is_exact(residual) && num_is_zero(residual);
    num_destroy(&residual);
    num_destroy(&candidate);
    return exact;
}

static bool equ_quartic_certify_factors(const number_t *coeffs, const number_t *factor, const number_t *quotient)
{
    for (size_t degree = 0u; degree <= EQU_QUARTIC_DEGREE; ++degree) {
        number_t sum = num_clone(NUM_ZERO);

        for (size_t i = 0u; i <= 2u; ++i) {
            if (i > degree || degree - i > 2u)
                continue;
            number_t product = num_mul(factor[i], quotient[degree - i]);
            number_t next = num_add(sum, product);

            num_destroy(&sum);
            num_destroy(&product);
            sum = next;
        }
        number_t difference = num_sub(sum, coeffs[degree]);
        bool exact = num_is_exact(difference) && num_is_zero(difference);

        num_destroy(&difference);
        num_destroy(&sum);
        if (!exact)
            return false;
    }
    return true;
}

static int equ_quartic_try_exact_factors(const number_t *coeffs, const expr_t *wrt, number_t root,
                                         equation_solutions_t *solutions)
{
    if (num_is_real(root))
        return 1;
    for (size_t i = 0u; i < EQU_QUARTIC_COEFF_COUNT; ++i)
        if (!num_is_exact(coeffs[i]) || !num_is_real(coeffs[i]))
            return 1;

    number_t real = num_real_part(root);
    number_t imaginary = num_imag_part(root);
    number_t linear = num_mul_long(real, -2L);
    number_t real_squared = num_mul(real, real);
    number_t imaginary_squared = num_mul(imaginary, imaginary);
    number_t constant = num_add(real_squared, imaginary_squared);
    number_t factor[3] = {equ_polynomial_rational_candidate(constant), equ_polynomial_rational_candidate(linear),
                          num_clone(NUM_ONE)};
    number_t quotient[3] = {num_new(), num_new(), num_clone(coeffs[4])};
    equation_solutions_t exact_solutions = {0};
    int rc = 1;

    if (!num_is_finite(factor[0]) || !num_is_finite(factor[1]))
        goto cleanup;
    number_t product = num_mul(factor[1], quotient[2]);
    num_destroy(&quotient[1]);
    quotient[1] = num_sub(coeffs[3], product);
    num_destroy(&product);
    product = num_mul(factor[1], quotient[1]);
    number_t other_product = num_mul(factor[0], quotient[2]);
    number_t sum = num_add(product, other_product);
    num_destroy(&quotient[0]);
    quotient[0] = num_sub(coeffs[2], sum);
    num_destroy(&sum);
    num_destroy(&other_product);
    num_destroy(&product);
    if (!equ_quartic_certify_factors(coeffs, factor, quotient))
        goto cleanup;
    /* Remove the verified leading scale before constructing quadratic radicals. */
    for (size_t i = 0u; i < 2u; ++i) {
        number_t monic = num_div(quotient[i], quotient[2]);
        num_destroy(&quotient[i]);
        quotient[i] = monic;
    }
    num_destroy(&quotient[2]);
    quotient[2] = num_clone(NUM_ONE);
    rc = -1;
    if (equ_solve_quadratic_coefficients(factor, wrt, &exact_solutions) != 0 ||
        equ_solve_quadratic_coefficients(quotient, wrt, &exact_solutions) != 0)
        goto cleanup;
    for (size_t i = 0u; i < exact_solutions.count; ++i) {
        expr_t *rhs = expr_clone(equ_rhs(exact_solutions.solutions[i]));
        bool duplicate = false;

        if (!rhs)
            goto cleanup;
        /* At most four roots; exact structural comparison retains distinct close roots. */
        for (size_t j = 0u; j < solutions->count; ++j)
            duplicate |= expr_simplify_same_factor(rhs, equ_rhs(solutions->solutions[j]));
        int append_rc = duplicate ? 0 : equ_append_solution_expr(wrt, rhs, solutions);
        expr_free(rhs);
        if (append_rc != 0)
            goto cleanup;
    }
    rc = 0;

cleanup:
    equ_solutions_clear(&exact_solutions);
    equ_quartic_destroy_numbers(quotient, 3u);
    equ_quartic_destroy_numbers(factor, 3u);
    num_destroy(&constant);
    num_destroy(&imaginary_squared);
    num_destroy(&real_squared);
    num_destroy(&linear);
    num_destroy(&imaginary);
    num_destroy(&real);
    return rc;
}

static int equ_quartic_append_distinct(const number_t *coeffs, const expr_t *wrt, number_t candidate,
                                       number_t newton_tolerance, number_t distinct_tolerance, number_t *seen,
                                       size_t *seen_count, equation_solutions_t *solutions)
{
    number_t polished = equ_quartic_polish_root(coeffs, candidate, newton_tolerance);
    number_t clean = equ_quartic_snap_gaussian_integer(coeffs, polished);

    for (size_t i = 0u; i < *seen_count; ++i) {
        if (equ_quartic_roots_close(clean, seen[i], distinct_tolerance)) {
            num_destroy(&clean);
            num_destroy(&polished);
            return 0;
        }
    }

    if (equ_append_solution_value(wrt, clean, solutions) != 0) {
        num_destroy(&clean);
        num_destroy(&polished);
        return -1;
    }
    seen[*seen_count] = num_clone(clean);
    ++*seen_count;
    num_destroy(&clean);
    num_destroy(&polished);
    return 0;
}

int equ_solve_quartic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions)
{
    number_t cubic[4];
    number_t quadratic[3];
    number_t first_root = num_new();
    number_t conjugate_root = num_new();
    number_t newton_tolerance = equ_quartic_newton_tolerance();
    number_t distinct_tolerance = equ_quartic_distinct_tolerance(newton_tolerance);
    number_t seen[EQU_QUARTIC_DEGREE];
    size_t seen_count = 0u;
    equation_solutions_t reduced_solutions = {0};
    bool use_conjugate_pair = false;
    int rc = -1;

    equ_quartic_init_numbers(cubic, 4u);
    equ_quartic_init_numbers(quadratic, 3u);
    if (!coeffs || !wrt || !solutions || num_is_zero(coeffs[EQU_QUARTIC_DEGREE]))
        goto cleanup;
    if (!equ_quartic_find_root(coeffs, newton_tolerance, &first_root)) {
        rc = 1;
        goto cleanup;
    }
    rc = equ_quartic_try_exact_factors(coeffs, wrt, first_root, solutions);
    if (rc != 1)
        goto cleanup;
    /* Keep the short rational-root route, especially inside repeated-exponential inversions. */
    rc = equ_quartic_certify_rational_root(coeffs, first_root)
        ? 1 : equ_try_quartic_radicals(coeffs, wrt, solutions);
    if (rc != 1)
        goto cleanup;
    rc = -1;

    {
        number_t snapped = equ_quartic_snap_gaussian_integer(coeffs, first_root);

        num_destroy(&first_root);
        first_root = snapped;
    }
    if (equ_polynomial_coefficients_real(coeffs, EQU_QUARTIC_DEGREE) &&
        equ_polynomial_root_effectively_real(first_root, newton_tolerance)) {
        number_t real_root = num_real_part(first_root);

        num_destroy(&first_root);
        first_root = real_root;
    }
    use_conjugate_pair = equ_polynomial_coefficients_real(coeffs, EQU_QUARTIC_DEGREE) && !num_is_real(first_root);

    if (use_conjugate_pair) {
        num_destroy(&conjugate_root);
        conjugate_root = num_conj(first_root);
        equ_polynomial_deflate_conjugate_pair(coeffs, EQU_QUARTIC_DEGREE, first_root, quadratic);
        if (equ_solve_quadratic_coefficients(quadratic, wrt, &reduced_solutions) != 0)
            goto cleanup;
    } else {
        equ_quartic_synthetic_divide(coeffs, first_root, cubic);
        if (equ_solve_cubic_coefficients(cubic, wrt, &reduced_solutions) != 0)
            goto cleanup;
    }
    if (equ_quartic_append_distinct(coeffs, wrt, first_root, newton_tolerance, distinct_tolerance, seen, &seen_count,
                                    solutions) != 0)
        goto cleanup;
    if (use_conjugate_pair && equ_quartic_append_distinct(coeffs, wrt, conjugate_root, newton_tolerance,
                                                          distinct_tolerance, seen, &seen_count, solutions) != 0)
        goto cleanup;

    for (size_t i = 0u; i < reduced_solutions.count; ++i) {
        number_t root = expr_eval(equ_rhs(reduced_solutions.solutions[i]));
        int append_rc = equ_quartic_append_distinct(coeffs, wrt, root, newton_tolerance, distinct_tolerance, seen,
                                                    &seen_count, solutions);

        num_destroy(&root);
        if (append_rc != 0)
            goto cleanup;
    }
    rc = seen_count > 0u ? 0 : 1;

cleanup:
    for (size_t i = 0u; i < seen_count; ++i)
        num_destroy(&seen[i]);
    equ_solutions_clear(&reduced_solutions);
    num_destroy(&distinct_tolerance);
    num_destroy(&newton_tolerance);
    num_destroy(&conjugate_root);
    num_destroy(&first_root);
    equ_quartic_destroy_numbers(quadratic, 3u);
    equ_quartic_destroy_numbers(cubic, 4u);
    return rc;
}

int equ_try_solve_quartic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t coeffs[EQU_QUARTIC_COEFF_COUNT];
    int rc = -1;

    equ_quartic_init_numbers(coeffs, EQU_QUARTIC_COEFF_COUNT);
    if (!residual)
        goto cleanup;
    if (!equ_match_polynomial_expr(residual, wrt, EQU_QUARTIC_DEGREE, coeffs) ||
        num_is_zero(coeffs[EQU_QUARTIC_DEGREE])) {
        rc = 1;
        goto cleanup;
    }

    rc = equ_solve_quartic_coefficients(coeffs, wrt, solutions);

cleanup:
    equ_quartic_destroy_numbers(coeffs, EQU_QUARTIC_COEFF_COUNT);
    expr_free(residual);
    return rc;
}
