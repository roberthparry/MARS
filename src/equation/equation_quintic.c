#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#include "number.h"

/*
 * A real-coefficient quintic always has at least one real root.  Find that
 * root with safeguarded Newton iteration, divide by its linear factor, and
 * delegate the deflated polynomial to the quartic solver.
 */
enum {
    EQU_QUINTIC_DEGREE = 5u,
    EQU_QUINTIC_COEFF_COUNT = EQU_QUINTIC_DEGREE + 1u,
    EQU_QUINTIC_NEWTON_ITERATIONS = 512u
};

static void equ_quintic_init_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
}

static void equ_quintic_destroy_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
}

static void equ_quintic_eval(const number_t *coeffs, number_t z, number_t *value_out, number_t *derivative_out)
{
    number_t value = num_clone(coeffs[EQU_QUINTIC_DEGREE]);
    number_t derivative = num_new();

    for (size_t degree = EQU_QUINTIC_DEGREE; degree-- > 0u;) {
        number_t derivative_product = num_mul(derivative, z);
        number_t derivative_sum = num_add(derivative_product, value);
        number_t value_product = num_mul(value, z);
        number_t value_sum = num_add(value_product, coeffs[degree]);

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

static bool equ_quintic_magnitude_le(number_t value, number_t tolerance)
{
    number_t magnitude = num_abs(value);
    bool close = num_is_finite(magnitude) && num_le(magnitude, tolerance);

    num_destroy(&magnitude);
    return close;
}

static number_t equ_quintic_newton_tolerance(void)
{
    size_t digits = num_get_default_prec_digits();
    int exponent;

    if (digits < 24u)
        digits = 24u;
    exponent = digits > 8u ? -(int)(digits - 8u) : -16;
    return num_pow10(exponent);
}

static number_t equ_quintic_distinct_tolerance(number_t newton_tolerance)
{
    number_t fifth = num_create_from_frac(1L, 5L);
    number_t root = num_pow(newton_tolerance, fifth);
    number_t tolerance = num_mul_long(root, 100L);

    num_destroy(&root);
    num_destroy(&fifth);
    return tolerance;
}

static double equ_quintic_root_bound(const number_t *coeffs)
{
    number_t max_ratio = num_new();
    double bound;

    for (size_t i = 0u; i < EQU_QUINTIC_DEGREE; ++i) {
        number_t ratio = num_div(coeffs[i], coeffs[EQU_QUINTIC_DEGREE]);
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

static bool equ_quintic_newton_from(const number_t *coeffs, number_t start, number_t tolerance, number_t *root_out)
{
    number_t z = num_clone(start);
    number_t value = num_new();
    number_t derivative = num_new();
    bool converged = false;

    for (size_t iteration = 0u; iteration < EQU_QUINTIC_NEWTON_ITERATIONS; ++iteration) {
        number_t step;
        number_t next;

        equ_quintic_eval(coeffs, z, &value, &derivative);
        if (equ_quintic_magnitude_le(value, tolerance)) {
            converged = true;
            break;
        }
        if (!num_is_finite(derivative) || equ_quintic_magnitude_le(derivative, tolerance))
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
        equ_quintic_eval(coeffs, z, &value, NULL);
        converged = equ_quintic_magnitude_le(value, tolerance);
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

static bool equ_quintic_find_real_root(const number_t *coeffs, number_t tolerance, number_t *root_out)
{
    const double bound = equ_quintic_root_bound(coeffs);
    number_t left = num_create_from_double(-bound);
    number_t right = num_create_from_double(bound);
    number_t value_left = num_new();
    number_t value_right = num_new();
    number_t z = num_new();
    number_t value = num_new();
    number_t derivative = num_new();
    bool found = false;

    equ_quintic_eval(coeffs, left, &value_left, NULL);
    equ_quintic_eval(coeffs, right, &value_right, NULL);
    if (equ_quintic_magnitude_le(value_left, tolerance)) {
        num_destroy(root_out);
        *root_out = num_clone(left);
        found = true;
        goto cleanup;
    }
    if (equ_quintic_magnitude_le(value_right, tolerance)) {
        num_destroy(root_out);
        *root_out = num_clone(right);
        found = true;
        goto cleanup;
    }
    if (!num_is_real(value_left) || !num_is_real(value_right) || num_sign(value_left) == num_sign(value_right))
        goto cleanup;

    for (size_t iteration = 0u; iteration < EQU_QUINTIC_NEWTON_ITERATIONS; ++iteration) {
        number_t candidate = num_new();
        bool use_newton = false;

        equ_quintic_eval(coeffs, z, &value, &derivative);
        if (equ_quintic_magnitude_le(value, tolerance)) {
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

        if (num_is_real(derivative) && !equ_quintic_magnitude_le(derivative, tolerance)) {
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

static void equ_quintic_synthetic_divide(const number_t *coeffs, number_t root, number_t *quartic)
{
    num_destroy(&quartic[4]);
    quartic[4] = num_clone(coeffs[5]);

    for (size_t degree = 4u; degree > 0u; --degree) {
        number_t product = num_mul(root, quartic[degree]);
        number_t next = num_add(coeffs[degree], product);

        num_destroy(&quartic[degree - 1u]);
        quartic[degree - 1u] = next;
        num_destroy(&product);
    }
}

static number_t equ_quintic_snap_gaussian_integer(const number_t *coeffs, number_t root)
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

    if (bounded && fabs(real_d - (double)real_i) < 1e-6 && fabs(imag_d - (double)imag_i) < 1e-6) {
        number_t real_part = num_create_from_long(real_i);
        number_t imag_part = num_create_from_long(imag_i);
        number_t imag_term = num_mul(NUM_I, imag_part);

        num_destroy(&candidate);
        candidate = imag_i == 0L ? num_clone(real_part) : num_add(real_part, imag_term);
        equ_quintic_eval(coeffs, candidate, &value, NULL);
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

static bool equ_quintic_roots_close(number_t left, number_t right, number_t tolerance)
{
    number_t difference = num_sub(left, right);
    bool close = equ_quintic_magnitude_le(difference, tolerance);

    num_destroy(&difference);
    return close;
}

static int equ_quintic_append_distinct(const number_t *coeffs, const expr_t *wrt, number_t candidate,
                                       number_t newton_tolerance, number_t distinct_tolerance, number_t *seen,
                                       size_t *seen_count, equation_solutions_t *solutions)
{
    number_t polished = num_new();
    number_t clean;

    if (!equ_quintic_newton_from(coeffs, candidate, newton_tolerance, &polished)) {
        num_destroy(&polished);
        polished = num_clone(candidate);
    }
    clean = equ_quintic_snap_gaussian_integer(coeffs, polished);

    for (size_t i = 0u; i < *seen_count; ++i) {
        if (equ_quintic_roots_close(clean, seen[i], distinct_tolerance)) {
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

int equ_try_solve_quintic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions)
{
    expr_t *residual = equ_residual(equation);
    number_t coeffs[EQU_QUINTIC_COEFF_COUNT];
    number_t quartic[5];
    number_t first_root = num_new();
    number_t newton_tolerance = equ_quintic_newton_tolerance();
    number_t distinct_tolerance = equ_quintic_distinct_tolerance(newton_tolerance);
    number_t seen[EQU_QUINTIC_DEGREE];
    size_t seen_count = 0u;
    equation_solutions_t quartic_solutions = {0};
    int rc = -1;

    equ_quintic_init_numbers(coeffs, EQU_QUINTIC_COEFF_COUNT);
    equ_quintic_init_numbers(quartic, 5u);
    if (!residual)
        goto cleanup;
    if (!equ_match_polynomial_expr(residual, wrt, EQU_QUINTIC_DEGREE, coeffs) ||
        num_is_zero(coeffs[EQU_QUINTIC_DEGREE])) {
        rc = 1;
        goto cleanup;
    }
    if (!equ_quintic_find_real_root(coeffs, newton_tolerance, &first_root)) {
        rc = 1;
        goto cleanup;
    }

    {
        number_t snapped = equ_quintic_snap_gaussian_integer(coeffs, first_root);

        num_destroy(&first_root);
        first_root = snapped;
    }
    equ_quintic_synthetic_divide(coeffs, first_root, quartic);
    if (equ_solve_quartic_coefficients(quartic, wrt, &quartic_solutions) != 0)
        goto cleanup;

    if (equ_quintic_append_distinct(coeffs, wrt, first_root, newton_tolerance, distinct_tolerance, seen, &seen_count,
                                    solutions) != 0)
        goto cleanup;

    for (size_t i = 0u; i < quartic_solutions.count; ++i) {
        number_t root = expr_eval(equ_rhs(quartic_solutions.solutions[i]));
        int append_rc = equ_quintic_append_distinct(coeffs, wrt, root, newton_tolerance, distinct_tolerance, seen,
                                                    &seen_count, solutions);

        num_destroy(&root);
        if (append_rc != 0)
            goto cleanup;
    }
    rc = seen_count > 0u ? 0 : 1;

cleanup:
    for (size_t i = 0u; i < seen_count; ++i)
        num_destroy(&seen[i]);
    equ_solutions_clear(&quartic_solutions);
    num_destroy(&distinct_tolerance);
    num_destroy(&newton_tolerance);
    num_destroy(&first_root);
    equ_quintic_destroy_numbers(quartic, 5u);
    equ_quintic_destroy_numbers(coeffs, EQU_QUINTIC_COEFF_COUNT);
    expr_free(residual);
    return rc;
}
