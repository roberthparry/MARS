#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "equation_internal.h"
#include "number.h"

/*
 * Numeric polynomials above degree five are solved in constant stack space.
 * Each loop pass finds one real or complex root with Newton iteration.  A real
 * root removes one linear factor.  For real coefficients, a non-real root also
 * supplies its conjugate, so one real quadratic factor removes both at once.
 * The established quartic/cubic path finishes the final four roots.
 */
enum {
    EQU_POLY_NEWTON_ITERATIONS = 512u,
    EQU_POLY_MIN_NEWTON_SEEDS = 37u
};

static void equ_general_destroy_numbers(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
}

static number_t *equ_general_new_numbers(size_t count)
{
    number_t *values;

    if (count == 0u || count > SIZE_MAX / sizeof(*values))
        return NULL;
    values = malloc(count * sizeof(*values));
    if (!values)
        return NULL;
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
    return values;
}

static void equ_general_eval(const number_t *coeffs,
                             size_t degree,
                             number_t z,
                             number_t *value_out,
                             number_t *derivative_out)
{
    number_t value = num_clone(coeffs[degree]);
    number_t derivative = num_new();

    for (size_t i = degree; i-- > 0u;) {
        number_t derivative_product = num_mul(derivative, z);
        number_t derivative_sum = num_add(derivative_product, value);
        number_t value_product = num_mul(value, z);
        number_t value_sum = num_add(value_product, coeffs[i]);

        num_destroy(&value_product);
        num_destroy(&value);
        value = value_sum;
        num_destroy(&derivative_product);
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

static bool equ_general_magnitude_le(number_t value, number_t tolerance)
{
    number_t magnitude = num_abs(value);
    bool close = num_is_finite(magnitude) && num_le(magnitude, tolerance);

    num_destroy(&magnitude);
    return close;
}

static number_t equ_general_newton_tolerance(void)
{
    size_t digits = num_get_default_prec_digits();
    int exponent;

    if (digits < 24u)
        digits = 24u;
    exponent = digits > 8u ? -(int)(digits - 8u) : -16;
    return num_pow10(exponent);
}

static number_t equ_general_distinct_tolerance(number_t newton_tolerance)
{
    number_t square_root = num_sqrt(newton_tolerance);
    number_t tolerance = num_mul_long(square_root, 100L);

    num_destroy(&square_root);
    return tolerance;
}

static double equ_general_root_bound(const number_t *coeffs, size_t degree)
{
    number_t max_ratio = num_new();
    double bound;

    for (size_t i = 0u; i < degree; ++i) {
        number_t ratio = num_div(coeffs[i], coeffs[degree]);
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

static size_t equ_general_seed_count(size_t degree)
{
    size_t count;

    if (degree > (SIZE_MAX - 5u) / 8u)
        return SIZE_MAX;
    count = degree * 8u + 5u;
    return count > EQU_POLY_MIN_NEWTON_SEEDS
        ? count
        : EQU_POLY_MIN_NEWTON_SEEDS;
}

static number_t equ_general_seed(size_t index,
                                 size_t seed_count,
                                 double bound)
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
        const size_t circle_count = seed_count - 5u;
        const size_t circle_index = index - 5u;
        const double angle =
            2.0 * acos(-1.0) * ((double)circle_index + 0.5) /
            (double)circle_count;
        const double ring =
            0.25 + 0.75 * (double)(circle_index % 4u) / 3.0;
        number_t real = num_create_from_double(bound * ring * cos(angle));
        number_t imag = num_create_from_double(bound * ring * sin(angle));
        number_t imag_term = num_mul(NUM_I, imag);
        number_t seed = num_add(real, imag_term);

        num_destroy(&imag_term);
        num_destroy(&imag);
        num_destroy(&real);
        return seed;
    }
}

static bool equ_general_newton_from(const number_t *coeffs,
                                    size_t degree,
                                    number_t start,
                                    number_t tolerance,
                                    number_t *root_out)
{
    number_t z = num_clone(start);
    number_t value = num_new();
    number_t derivative = num_new();
    bool converged = false;

    for (size_t iteration = 0u;
         iteration < EQU_POLY_NEWTON_ITERATIONS;
         ++iteration) {
        number_t step;
        number_t next;

        equ_general_eval(coeffs, degree, z, &value, &derivative);
        if (equ_general_magnitude_le(value, tolerance)) {
            converged = true;
            break;
        }
        if (!num_is_finite(derivative) ||
            equ_general_magnitude_le(derivative, tolerance))
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
        equ_general_eval(coeffs, degree, z, &value, NULL);
        converged = equ_general_magnitude_le(value, tolerance);
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

bool equ_polynomial_coefficients_real(const number_t *coeffs,
                                      size_t degree)
{
    if (!coeffs)
        return false;
    for (size_t i = 0u; i <= degree; ++i) {
        if (!num_is_real(coeffs[i]))
            return false;
    }
    return true;
}

bool equ_polynomial_root_effectively_real(number_t root,
                                          number_t tolerance)
{
    number_t imaginary = num_imag_part(root);
    number_t magnitude = num_abs(imaginary);
    bool effectively_real =
        num_is_finite(magnitude) && num_le(magnitude, tolerance);

    num_destroy(&magnitude);
    num_destroy(&imaginary);
    return effectively_real;
}

static bool equ_general_find_real_root(const number_t *coeffs,
                                       size_t degree,
                                       number_t tolerance,
                                       number_t *root_out)
{
    const double bound = equ_general_root_bound(coeffs, degree);
    number_t left = num_create_from_double(-bound);
    number_t right = num_create_from_double(bound);
    number_t value_left = num_new();
    number_t value_right = num_new();
    number_t z = num_new();
    number_t value = num_new();
    number_t derivative = num_new();
    bool found = false;

    equ_general_eval(coeffs, degree, left, &value_left, NULL);
    equ_general_eval(coeffs, degree, right, &value_right, NULL);
    if (equ_general_magnitude_le(value_left, tolerance)) {
        num_destroy(root_out);
        *root_out = num_clone(left);
        found = true;
        goto cleanup;
    }
    if (equ_general_magnitude_le(value_right, tolerance)) {
        num_destroy(root_out);
        *root_out = num_clone(right);
        found = true;
        goto cleanup;
    }
    if (!num_is_real(value_left) || !num_is_real(value_right) ||
        num_sign(value_left) == num_sign(value_right))
        goto cleanup;

    for (size_t iteration = 0u;
         iteration < EQU_POLY_NEWTON_ITERATIONS;
         ++iteration) {
        number_t candidate = num_new();
        bool use_newton = false;

        equ_general_eval(coeffs, degree, z, &value, &derivative);
        if (equ_general_magnitude_le(value, tolerance)) {
            num_destroy(root_out);
            *root_out = num_clone(z);
            num_destroy(&candidate);
            found = true;
            break;
        }

        if (num_sign(value) == num_sign(value_left)) {
            num_destroy(&left);
            left = num_clone(z);
            num_destroy(&value_left);
            value_left = num_clone(value);
        } else {
            num_destroy(&right);
            right = num_clone(z);
            num_destroy(&value_right);
            value_right = num_clone(value);
        }

        if (num_is_real(derivative) &&
            !equ_general_magnitude_le(derivative, tolerance)) {
            number_t step = num_div(value, derivative);
            number_t next = num_sub(z, step);

            if (num_is_real(next) && num_gt(next, left) && num_lt(next, right)) {
                num_destroy(&candidate);
                candidate = num_clone(next);
                use_newton = true;
            }
            num_destroy(&next);
            num_destroy(&step);
        }

        if (!use_newton) {
            number_t sum = num_add(left, right);
            number_t two = num_create_from_long(2L);

            num_destroy(&candidate);
            candidate = num_div(sum, two);
            num_destroy(&two);
            num_destroy(&sum);
        }
        num_destroy(&z);
        z = candidate;
    }

cleanup:
    num_destroy(&derivative);
    num_destroy(&value);
    num_destroy(&z);
    num_destroy(&value_right);
    num_destroy(&value_left);
    num_destroy(&right);
    num_destroy(&left);
    return found;
}

static bool equ_general_find_complex_root(const number_t *coeffs,
                                          size_t degree,
                                          number_t tolerance,
                                          number_t *root_out)
{
    const double bound = equ_general_root_bound(coeffs, degree);
    const size_t seed_count = equ_general_seed_count(degree);

    if (seed_count == SIZE_MAX)
        return false;
    for (size_t i = 0u; i < seed_count; ++i) {
        number_t seed = equ_general_seed(i, seed_count, bound);
        bool found = equ_general_newton_from(
            coeffs, degree, seed, tolerance, root_out);

        num_destroy(&seed);
        if (found)
            return true;
    }
    return false;
}

static bool equ_general_find_root(const number_t *coeffs,
                                  size_t degree,
                                  number_t tolerance,
                                  number_t *root_out)
{
    if ((degree & 1u) != 0u &&
        equ_polynomial_coefficients_real(coeffs, degree) &&
        equ_general_find_real_root(
            coeffs, degree, tolerance, root_out))
        return true;

    return equ_general_find_complex_root(
        coeffs, degree, tolerance, root_out);
}

void equ_polynomial_deflate_conjugate_pair(const number_t *coeffs,
                                           size_t degree,
                                           number_t root,
                                           number_t *deflated)
{
    number_t real = num_real_part(root);
    number_t imaginary = num_imag_part(root);
    number_t linear = num_mul_long(real, -2L);
    number_t real_squared = num_mul(real, real);
    number_t imaginary_squared = num_mul(imaginary, imaginary);
    number_t constant = num_add(real_squared, imaginary_squared);
    number_t linear_product;
    number_t constant_product;
    number_t product_sum;
    number_t next;

    if (!coeffs || !deflated || degree < 2u)
        goto cleanup;

    num_destroy(&deflated[degree - 2u]);
    deflated[degree - 2u] = num_clone(coeffs[degree]);
    if (degree == 2u)
        goto cleanup;

    linear_product = num_mul(linear, deflated[degree - 2u]);
    next = num_sub(coeffs[degree - 1u], linear_product);
    num_destroy(&linear_product);
    num_destroy(&deflated[degree - 3u]);
    deflated[degree - 3u] = next;

    for (size_t i = degree - 2u; i > 1u; --i) {
        linear_product = num_mul(linear, deflated[i - 1u]);
        constant_product = num_mul(constant, deflated[i]);
        product_sum = num_add(linear_product, constant_product);
        next = num_sub(coeffs[i], product_sum);

        num_destroy(&product_sum);
        num_destroy(&constant_product);
        num_destroy(&linear_product);
        num_destroy(&deflated[i - 2u]);
        deflated[i - 2u] = next;
    }

cleanup:
    num_destroy(&constant);
    num_destroy(&imaginary_squared);
    num_destroy(&real_squared);
    num_destroy(&linear);
    num_destroy(&imaginary);
    num_destroy(&real);
}

static void equ_general_synthetic_divide(const number_t *coeffs,
                                         size_t degree,
                                         number_t root,
                                         number_t *deflated)
{
    num_destroy(&deflated[degree - 1u]);
    deflated[degree - 1u] = num_clone(coeffs[degree]);

    for (size_t i = degree - 1u; i > 0u; --i) {
        number_t product = num_mul(root, deflated[i]);
        number_t next = num_add(coeffs[i], product);

        num_destroy(&deflated[i - 1u]);
        deflated[i - 1u] = next;
        num_destroy(&product);
    }
}

static number_t equ_general_snap_gaussian_integer(const number_t *coeffs,
                                                  size_t degree,
                                                  number_t root)
{
    number_t real = num_real_part(root);
    number_t imag = num_imag_part(root);
    double real_d = num_to_double(real);
    double imag_d = num_to_double(imag);
    bool bounded = isfinite(real_d) && isfinite(imag_d) &&
                   fabs(real_d) < 1000000.0 && fabs(imag_d) < 1000000.0;
    long real_i = bounded ? lround(real_d) : 0L;
    long imag_i = bounded ? lround(imag_d) : 0L;
    number_t candidate = num_new();
    number_t value = num_new();

    if (bounded && fabs(real_d - (double)real_i) < 1e-6 &&
        fabs(imag_d - (double)imag_i) < 1e-6) {
        number_t real_part = num_create_from_long(real_i);
        number_t imag_part = num_create_from_long(imag_i);
        number_t imag_term = num_mul(NUM_I, imag_part);

        num_destroy(&candidate);
        candidate = imag_i == 0L ? num_clone(real_part)
                                 : num_add(real_part, imag_term);
        equ_general_eval(coeffs, degree, candidate, &value, NULL);
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

static bool equ_general_roots_close(number_t left,
                                    number_t right,
                                    number_t tolerance)
{
    number_t difference = num_sub(left, right);
    bool close = equ_general_magnitude_le(difference, tolerance);

    num_destroy(&difference);
    return close;
}

static int equ_general_append_distinct(const number_t *original_coeffs,
                                       size_t original_degree,
                                       const expr_t *wrt,
                                       number_t candidate,
                                       number_t newton_tolerance,
                                       number_t distinct_tolerance,
                                       number_t *seen,
                                       size_t *seen_count,
                                       equation_solutions_t *solutions)
{
    number_t polished = num_new();
    number_t clean;

    if (!equ_general_newton_from(
            original_coeffs, original_degree, candidate,
            newton_tolerance, &polished)) {
        num_destroy(&polished);
        polished = num_clone(candidate);
    }
    clean = equ_general_snap_gaussian_integer(
        original_coeffs, original_degree, polished);

    for (size_t i = 0u; i < *seen_count; ++i) {
        if (equ_general_roots_close(clean, seen[i], distinct_tolerance)) {
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
    num_destroy(&seen[*seen_count]);
    seen[*seen_count] = num_clone(clean);
    ++*seen_count;
    num_destroy(&clean);
    num_destroy(&polished);
    return 0;
}

int equ_try_solve_general_polynomial(const equation_t *equation,
                                     const expr_t *wrt,
                                     equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t *original_coeffs = NULL;
    number_t *active_coeffs = NULL;
    number_t *deflated = NULL;
    number_t *roots = NULL;
    number_t *seen = NULL;
    size_t original_degree = 0u;
    size_t active_degree = 0u;
    size_t root_count = 0u;
    size_t seen_count = 0u;
    size_t deflated_count = 0u;
    number_t root = num_new();
    number_t newton_tolerance = equ_general_newton_tolerance();
    number_t distinct_tolerance =
        equ_general_distinct_tolerance(newton_tolerance);
    equation_solutions_t quartic_solutions = {0};
    int rc = -1;

    if (!residual ||
        !equ_match_polynomial_alloc(
            residual, wrt, &original_coeffs, &original_degree) ||
        original_degree <= 5u) {
        rc = 1;
        goto cleanup;
    }

    active_coeffs = equ_general_new_numbers(original_degree + 1u);
    roots = equ_general_new_numbers(original_degree);
    seen = equ_general_new_numbers(original_degree);
    if (!active_coeffs || !roots || !seen)
        goto cleanup;
    for (size_t i = 0u; i <= original_degree; ++i) {
        num_destroy(&active_coeffs[i]);
        active_coeffs[i] = num_clone(original_coeffs[i]);
    }

    /*
     * Iterative deflation is the constant-stack equivalent of tail recursion:
     * the active coefficient buffer becomes the next recursive state.
     */
    active_degree = original_degree;
    while (active_degree > 4u) {
        number_t snapped;
        bool active_coefficients_real =
            equ_polynomial_coefficients_real(active_coeffs, active_degree);

        if (!equ_general_find_root(
                active_coeffs, active_degree, newton_tolerance, &root)) {
            rc = 1;
            goto cleanup;
        }
        snapped = equ_general_snap_gaussian_integer(
            active_coeffs, active_degree, root);
        num_destroy(&root);
        root = snapped;

        if (active_coefficients_real &&
            equ_polynomial_root_effectively_real(
                root, newton_tolerance)) {
            number_t real_root = num_real_part(root);

            num_destroy(&root);
            root = real_root;
        }

        num_destroy(&roots[root_count]);
        roots[root_count++] = num_clone(root);

        if (active_coefficients_real && !num_is_real(root)) {
            number_t conjugate = num_conj(root);

            num_destroy(&roots[root_count]);
            roots[root_count++] = num_clone(conjugate);
            deflated_count = active_degree - 1u;
            deflated = equ_general_new_numbers(deflated_count);
            if (deflated)
                equ_polynomial_deflate_conjugate_pair(
                    active_coeffs, active_degree, root, deflated);
            num_destroy(&conjugate);
        } else {
            deflated_count = active_degree;
            deflated = equ_general_new_numbers(deflated_count);
            if (deflated)
                equ_general_synthetic_divide(
                    active_coeffs, active_degree, root, deflated);
        }
        if (!deflated)
            goto cleanup;
        equ_general_destroy_numbers(active_coeffs, active_degree + 1u);
        free(active_coeffs);
        active_coeffs = deflated;
        deflated = NULL;
        active_degree =
            active_coefficients_real && !num_is_real(root)
            ? active_degree - 2u
            : active_degree - 1u;
    }

    if (equ_solve_quartic_coefficients(
            active_coeffs, wrt, &quartic_solutions) != 0) {
        rc = 1;
        goto cleanup;
    }
    for (size_t i = 0u; i < quartic_solutions.count; ++i) {
        number_t value =
            expr_eval(equ_rhs(quartic_solutions.solutions[i]));

        num_destroy(&roots[root_count]);
        roots[root_count++] = value;
    }

    for (size_t i = 0u; i < root_count; ++i) {
        if (equ_general_append_distinct(
                original_coeffs, original_degree, wrt, roots[i],
                newton_tolerance, distinct_tolerance, seen, &seen_count,
                solutions) != 0)
            goto cleanup;
    }
    rc = seen_count > 0u ? 0 : 1;

cleanup:
    if (rc < 0 && solutions)
        equ_solutions_clear(solutions);
    equ_solutions_clear(&quartic_solutions);
    equ_general_destroy_numbers(seen, original_degree);
    free(seen);
    equ_general_destroy_numbers(roots, original_degree);
    free(roots);
    equ_general_destroy_numbers(deflated, deflated_count);
    free(deflated);
    if (active_coeffs) {
        equ_general_destroy_numbers(active_coeffs, active_degree + 1u);
        free(active_coeffs);
    }
    if (original_coeffs) {
        equ_general_destroy_numbers(
            original_coeffs, original_degree + 1u);
        free(original_coeffs);
    }
    num_destroy(&distinct_tolerance);
    num_destroy(&newton_tolerance);
    num_destroy(&root);
    expr_free(residual);
    return rc;
}
