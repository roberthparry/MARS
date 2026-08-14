#include <stdlib.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

#define EXPR_GOAL_DEFAULT_DIGITS 64u
#define EXPR_GOAL_DEFAULT_ITERATIONS 120u

static void goal_set_var(expr_t *var, number_t value, size_t digits);
static int goal_eval_residual(expr_t *expr, number_t target, number_t *value_out, number_t *residual_out);

static size_t goal_iterations_for_digits(size_t digits)
{
    size_t scaled = digits > 0u ? digits * 4u : EXPR_GOAL_DEFAULT_ITERATIONS;

    return scaled > EXPR_GOAL_DEFAULT_ITERATIONS ? scaled : EXPR_GOAL_DEFAULT_ITERATIONS;
}

static size_t goal_precision_digits(const expr_goal_seek_options_t *options)
{
    if (options && options->precision_digits > 0u)
        return options->precision_digits;
    return EXPR_GOAL_DEFAULT_DIGITS;
}

static size_t goal_work_digits(size_t digits)
{
    if (digits == 0u)
        return EXPR_GOAL_DEFAULT_DIGITS * 2u;
    return digits < 8u ? digits + 8u : digits * 2u;
}

static size_t goal_max_iterations(const expr_goal_seek_options_t *options)
{
    if (options && options->max_iterations > 0u)
        return options->max_iterations;
    return goal_iterations_for_digits(goal_precision_digits(options));
}

static void goal_result_reset(expr_goal_seek_result_t *result)
{
    if (!result)
        return;
    result->expr = NULL;
    result->bindings = NULL;
    result->value = num_new();
    result->residual = num_new();
    result->iterations = 0u;
    result->used_complex = false;
    result->converged = false;
}

void expr_goal_seek_result_clear(expr_goal_seek_result_t *result)
{
    if (!result)
        return;
    expr_free(result->expr);
    result->expr = NULL;
    result->bindings = NULL;
    num_destroy(&result->value);
    num_destroy(&result->residual);
    result->iterations = 0u;
    result->used_complex = false;
    result->converged = false;
}

static number_t goal_default_tolerance(number_t target, size_t digits)
{
    number_t eps;
    number_t target_mag;
    number_t scale;
    number_t tolerance;

    eps = num_pow10(-(int)(digits > 0u ? digits : 1u));
    target_mag = num_abs(target);
    scale = num_is_finite(target_mag) && num_gt(target_mag, NUM_ONE) ? num_clone(target_mag) : num_clone(NUM_ONE);
    tolerance = num_mul(eps, scale);

    num_destroy(&scale);
    num_destroy(&target_mag);
    num_destroy(&eps);
    return tolerance;
}

static number_t goal_tolerance(number_t target, const expr_goal_seek_options_t *options)
{
    if (options && num_is_finite(options->tolerance) && !num_is_zero(options->tolerance)) {
        return num_abs(options->tolerance);
    }
    return goal_default_tolerance(target, goal_precision_digits(options));
}

static bool goal_residual_close(number_t residual, number_t tolerance)
{
    number_t mag = num_abs(residual);
    bool ok = num_is_finite(mag) && num_le(mag, tolerance);

    num_destroy(&mag);
    return ok;
}

static number_t goal_component_tolerance(size_t digits)
{
    int exponent = 0;

    if (digits > 1u)
        exponent = -(int)(digits - 1u);

    return num_pow10(exponent);
}

static number_t goal_cleanup_tolerance(number_t tolerance, size_t digits)
{
    number_t component_tolerance = goal_component_tolerance(digits);
    number_t out;

    if (num_gt(component_tolerance, tolerance))
        out = num_clone(component_tolerance);
    else
        out = num_clone(tolerance);

    num_destroy(&component_tolerance);
    return out;
}

static number_t goal_clean_negligible_complex_parts(number_t value, number_t component_tolerance, size_t digits)
{
    number_t real;
    number_t imag;
    number_t real_mag;
    number_t imag_mag;
    number_t imag_unit_delta;
    number_t imag_unit_gap;
    bool real_zero;
    bool imag_zero;
    bool imag_unit;
    number_t out;

    if (num_is_real(value))
        return num_clone(value);

    real = num_real_part(value);
    imag = num_imag_part(value);
    real_mag = num_abs(real);
    imag_mag = num_abs(imag);
    imag_unit_delta = num_sub(imag_mag, NUM_ONE);
    imag_unit_gap = num_abs(imag_unit_delta);
    real_zero = num_le(real_mag, component_tolerance);
    imag_zero = num_le(imag_mag, component_tolerance);
    imag_unit = num_le(imag_unit_gap, component_tolerance);
    num_destroy(&real_mag);
    num_destroy(&imag_mag);
    num_destroy(&imag_unit_delta);
    num_destroy(&imag_unit_gap);

    if (imag_zero) {
        out = real_zero ? num_clone(NUM_ZERO) : num_clone(real);
    } else {
        number_t clean_imag =
            imag_unit ? (num_lt(imag, NUM_ZERO) ? num_clone(NUM_NEG_ONE) : num_clone(NUM_ONE)) : num_clone(imag);
        number_t imag_part = num_mul(NUM_I, clean_imag);

        out = real_zero ? num_clone(imag_part) : num_add(real, imag_part);
        num_destroy(&imag_part);
        num_destroy(&clean_imag);
    }

    if (digits > 0u)
        num_set_prec_digits(&out, digits);
    num_destroy(&imag);
    num_destroy(&real);
    return out;
}

static bool goal_candidate_residual_close(expr_t *expr, expr_t *var, number_t candidate, number_t target,
                                          number_t tolerance, size_t digits)
{
    number_t value;
    number_t residual;
    bool ok = false;

    goal_set_var(var, candidate, digits);
    if (goal_eval_residual(expr, target, &value, &residual) == 0) {
        ok = goal_residual_close(residual, tolerance);
        num_destroy(&residual);
        num_destroy(&value);
    }
    return ok;
}

static number_t goal_simplify_complex_solution(expr_t *expr, expr_t *var, number_t value, number_t target,
                                               number_t tolerance, size_t digits)
{
    number_t component_tolerance = goal_cleanup_tolerance(tolerance, digits);
    number_t clean = goal_clean_negligible_complex_parts(value, component_tolerance, digits);

    num_destroy(&component_tolerance);
    if (!num_is_real(clean)) {
        number_t real = num_real_part(clean);
        number_t imag = num_imag_part(clean);

        if (!num_is_zero(imag) && goal_candidate_residual_close(expr, var, real, target, tolerance, digits)) {
            num_destroy(&clean);
            num_destroy(&imag);
            return real;
        }

        if (!num_is_zero(real)) {
            number_t imag_part = num_mul(NUM_I, imag);

            if (goal_candidate_residual_close(expr, var, imag_part, target, tolerance, digits)) {
                num_destroy(&clean);
                num_destroy(&real);
                num_destroy(&imag);
                return imag_part;
            }
            num_destroy(&imag_part);
        }

        num_destroy(&imag);
        num_destroy(&real);
    }
    return clean;
}

static bool goal_residual_real(number_t residual)
{
    return num_is_real(residual) && num_is_finite(residual);
}

static number_t goal_work_value(number_t value, size_t digits)
{
    number_t rounded;
    size_t bits;

    if (digits == 0u)
        return num_clone(value);

    bits = (size_t)((double)digits * 3.3219280948873623 + 1.0);
    rounded = num_is_real(value) ? num_as_inexact_real_prec(value, bits) : num_as_complex_prec(value, bits);

    if (!num_is_finite(rounded) && num_is_finite(value)) {
        num_destroy(&rounded);
        rounded = num_clone(value);
    }
    num_set_prec_digits(&rounded, digits);
    return rounded;
}

static number_t goal_start_value(expr_t *var, size_t digits)
{
    number_t value = expr_get_val(var);
    number_t rounded;

    if (!num_is_finite(value) || num_is_nan(value)) {
        num_destroy(&value);
        value = num_create_from_long(1L);
    }
    rounded = goal_work_value(value, digits);
    num_destroy(&value);
    return rounded;
}

static void goal_set_var(expr_t *var, number_t value, size_t digits)
{
    number_t copy = goal_work_value(value, digits);

    expr_set_val(var, copy);
    num_destroy(&copy);
}

static int goal_eval_residual(expr_t *expr, number_t target, number_t *value_out, number_t *residual_out)
{
    number_t value;
    number_t residual;

    if (!expr || !value_out || !residual_out)
        return -1;

    value = expr_eval(expr);
    residual = num_sub(value, target);
    *value_out = value;
    *residual_out = residual;
    return 0;
}

static int goal_eval_derivative_residual(expr_t *expr, expr_t *var, number_t target, number_t *value_out,
                                         number_t *residual_out, number_t *derivative_out)
{
    const expr_t *vars[1];
    number_t value = (number_t){0};
    number_t deriv = (number_t){0};

    vars[0] = var;
    if (expr_eval_derivatives(expr, 1u, vars, &value, &deriv) != 0) {
        num_destroy(&deriv);
        num_destroy(&value);
        return -1;
    }

    *value_out = value;
    *residual_out = num_sub(value, target);
    *derivative_out = deriv;
    return 0;
}

static int goal_finish(expr_t *expr, expr_bindings_t *bindings, number_t target, number_t tolerance,
                       const expr_goal_seek_options_t *options, bool used_complex, size_t iterations,
                       expr_goal_seek_result_t *result)
{
    number_t value;
    number_t residual;

    if (!result || goal_eval_residual(expr, target, &value, &residual) != 0)
        return -1;
    if (!goal_residual_close(residual, tolerance)) {
        num_destroy(&value);
        num_destroy(&residual);
        return -1;
    }

    result->expr = options && options->simplify_result ? expr_simplify(expr) : NULL;
    if (!result->expr) {
        expr_retain(expr);
        result->expr = expr;
    }
    result->bindings = bindings;
    num_destroy(&result->value);
    num_destroy(&result->residual);
    result->value = value;
    result->residual = residual;
    result->iterations = iterations;
    result->used_complex = used_complex;
    result->converged = true;
    return 0;
}

static int goal_collect_variables(expr_bindings_t *bindings, expr_t ***vars_out)
{
    expr_t **vars;
    size_t count = 0u;

    if (!bindings || !vars_out)
        return -1;

    for (size_t i = 0u; i < bindings->count; ++i) {
        if (!bindings->entries[i].is_constant)
            count++;
    }
    if (count == 0u)
        return 0;

    vars = calloc(count, sizeof(*vars));
    if (!vars)
        return -1;

    count = 0u;
    for (size_t i = 0u; i < bindings->count; ++i) {
        if (!bindings->entries[i].is_constant)
            vars[count++] = bindings->entries[i].expr;
    }

    *vars_out = vars;
    return (int)count;
}

static int goal_solve_real_one(expr_t *expr, expr_t *var, number_t target, number_t tolerance, size_t digits,
                               size_t max_iterations, size_t *iterations_out)
{
    number_t x0 = goal_start_value(var, digits);
    number_t value = (number_t){0};
    number_t f0 = (number_t){0};
    number_t step = num_create_from_long(1L);
    number_t lo = (number_t){0};
    number_t hi = (number_t){0};
    number_t flo = (number_t){0};
    number_t fhi = (number_t){0};
    bool bracketed = false;
    size_t iterations = 0u;
    int rc = -1;

    goal_set_var(var, x0, digits);
    if (goal_eval_residual(expr, target, &value, &f0) != 0 || !goal_residual_real(f0))
        goto cleanup;
    num_destroy(&value);
    value = (number_t){0};

    if (goal_residual_close(f0, tolerance)) {
        rc = 0;
        goto cleanup;
    }

    for (size_t i = 0u; i < max_iterations; ++i) {
        number_t left = num_sub(x0, step);
        number_t right = num_add(x0, step);
        number_t fleft = (number_t){0};
        number_t fright = (number_t){0};
        short s0 = num_get_sign(f0);
        short sl = 0;
        short sr = 0;

        iterations++;
        goal_set_var(var, left, digits);
        if (goal_eval_residual(expr, target, &value, &fleft) == 0) {
            num_destroy(&value);
            value = (number_t){0};
            if (goal_residual_real(fleft)) {
                if (goal_residual_close(fleft, tolerance)) {
                    num_destroy(&x0);
                    x0 = num_clone(left);
                    num_destroy(&fleft);
                    num_destroy(&fright);
                    num_destroy(&left);
                    num_destroy(&right);
                    rc = 0;
                    goto cleanup;
                }
                sl = num_get_sign(fleft);
            }
        }

        goal_set_var(var, right, digits);
        if (goal_eval_residual(expr, target, &value, &fright) == 0) {
            num_destroy(&value);
            value = (number_t){0};
            if (goal_residual_real(fright)) {
                if (goal_residual_close(fright, tolerance)) {
                    num_destroy(&x0);
                    x0 = num_clone(right);
                    num_destroy(&fleft);
                    num_destroy(&fright);
                    num_destroy(&left);
                    num_destroy(&right);
                    rc = 0;
                    goto cleanup;
                }
                sr = num_get_sign(fright);
            }
        }

        if (sl != 0 && s0 != 0 && sl != s0) {
            num_destroy(&lo);
            num_destroy(&hi);
            num_destroy(&flo);
            num_destroy(&fhi);
            lo = num_clone(left);
            hi = num_clone(x0);
            flo = num_clone(fleft);
            fhi = num_clone(f0);
            bracketed = true;
        } else if (sr != 0 && s0 != 0 && sr != s0) {
            num_destroy(&lo);
            num_destroy(&hi);
            num_destroy(&flo);
            num_destroy(&fhi);
            lo = num_clone(x0);
            hi = num_clone(right);
            flo = num_clone(f0);
            fhi = num_clone(fright);
            bracketed = true;
        }

        num_destroy(&fleft);
        num_destroy(&fright);
        num_destroy(&left);
        num_destroy(&right);

        if (bracketed)
            break;
        {
            number_t next_step = num_mul_long(step, 2L);
            num_destroy(&step);
            step = next_step;
        }
    }

    if (!bracketed)
        goto cleanup;

    for (size_t i = 0u; i < max_iterations * 4u; ++i) {
        number_t sum = num_add(lo, hi);
        number_t mid = num_div(sum, NUM_TWO);
        number_t fmid = (number_t){0};

        iterations++;
        num_destroy(&sum);
        goal_set_var(var, mid, digits);
        if (goal_eval_residual(expr, target, &value, &fmid) != 0 || !goal_residual_real(fmid)) {
            num_destroy(&value);
            value = (number_t){0};
            num_destroy(&fmid);
            num_destroy(&mid);
            goto cleanup;
        }
        num_destroy(&value);
        value = (number_t){0};

        if (goal_residual_close(fmid, tolerance)) {
            num_destroy(&x0);
            x0 = num_clone(mid);
            num_destroy(&mid);
            num_destroy(&fmid);
            rc = 0;
            goto cleanup;
        }

        if (num_get_sign(flo) != 0 && num_get_sign(fmid) != 0 && num_get_sign(flo) != num_get_sign(fmid)) {
            num_destroy(&hi);
            num_destroy(&fhi);
            hi = mid;
            fhi = fmid;
        } else {
            num_destroy(&lo);
            num_destroy(&flo);
            lo = mid;
            flo = fmid;
        }
    }

    {
        number_t sum = num_add(lo, hi);
        number_t mid = num_div(sum, NUM_TWO);

        num_destroy(&sum);
        num_destroy(&x0);
        x0 = mid;
    }
    rc = 0;

cleanup:
    goal_set_var(var, x0, digits);
    if (iterations_out)
        *iterations_out = iterations;
    num_destroy(&value);
    num_destroy(&fhi);
    num_destroy(&flo);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&step);
    num_destroy(&f0);
    num_destroy(&x0);
    return rc;
}

static int goal_complex_newton_from(expr_t *expr, expr_t *var, number_t target, number_t tolerance, number_t start,
                                    size_t digits, size_t max_iterations, size_t *iterations_out)
{
    number_t x = goal_work_value(start, digits);
    number_t best_x = num_clone(x);
    number_t best_residual = num_clone(NUM_INF);
    number_t best_norm = num_clone(NUM_INF);
    int rc = -1;
    size_t iterations = 0u;

    goal_set_var(var, x, digits);

    for (size_t i = 0u; i < max_iterations; ++i) {
        number_t value = (number_t){0};
        number_t residual = (number_t){0};
        number_t derivative = (number_t){0};
        number_t step;
        number_t trial;
        number_t trial_value = (number_t){0};
        number_t trial_residual = (number_t){0};
        number_t trial_norm;

        iterations++;
        if (goal_eval_derivative_residual(expr, var, target, &value, &residual, &derivative) != 0)
            goto loop_cleanup;
        num_destroy(&value);

        if (goal_residual_close(residual, tolerance)) {
            num_destroy(&best_x);
            num_destroy(&best_residual);
            best_x = num_clone(x);
            best_residual = num_clone(residual);
            num_destroy(&derivative);
            num_destroy(&trial_value);
            num_destroy(&trial_residual);
            num_destroy(&residual);
            rc = 0;
            break;
        }
        if (num_is_zero(derivative)) {
            num_destroy(&derivative);
            num_destroy(&trial_value);
            num_destroy(&trial_residual);
            num_destroy(&residual);
            break;
        }

        step = num_div(residual, derivative);
        trial = num_sub(x, step);
        num_destroy(&step);
        num_destroy(&derivative);

        if (digits > 0u)
            num_set_prec_digits(&trial, digits);
        goal_set_var(var, trial, digits);
        if (goal_eval_residual(expr, target, &trial_value, &trial_residual) != 0) {
            num_destroy(&trial);
            num_destroy(&trial_value);
            num_destroy(&trial_residual);
            num_destroy(&residual);
            break;
        }

        trial_norm = num_abs(trial_residual);
        if (num_lt(trial_norm, best_norm)) {
            num_destroy(&best_x);
            num_destroy(&best_norm);
            num_destroy(&best_residual);
            best_x = num_clone(trial);
            best_norm = num_clone(trial_norm);
            best_residual = num_clone(trial_residual);
        }

        num_destroy(&x);
        x = trial;
        num_destroy(&trial_norm);
        num_destroy(&trial_value);
        num_destroy(&trial_residual);
        num_destroy(&residual);
        continue;

    loop_cleanup:
        num_destroy(&derivative);
        num_destroy(&trial_value);
        num_destroy(&trial_residual);
        num_destroy(&residual);
        break;
    }

    if (rc != 0 && goal_residual_close(best_residual, tolerance))
        rc = 0;

    if (rc == 0) {
        number_t clean_x = goal_simplify_complex_solution(expr, var, best_x, target, tolerance, digits);
        goal_set_var(var, clean_x, digits);
        num_destroy(&clean_x);
    } else {
        goal_set_var(var, start, digits);
    }
    if (iterations_out)
        *iterations_out = iterations;
    num_destroy(&best_norm);
    num_destroy(&best_residual);
    num_destroy(&best_x);
    num_destroy(&x);
    return rc;
}

static size_t goal_complex_probe_iteration_limit(size_t max_iterations)
{
    return max_iterations < 8u ? max_iterations : 8u;
}

static number_t goal_complex_offset_seed(number_t base, number_t imag_offset, size_t digits)
{
    number_t imag = goal_work_value(imag_offset, digits);
    number_t seed = num_add(base, imag);

    num_destroy(&imag);
    return seed;
}

static int goal_solve_complex_one(expr_t *expr, expr_t *var, number_t target, number_t tolerance, size_t digits,
                                  size_t max_iterations, size_t *iterations_out)
{
    number_t base = goal_start_value(var, digits);
    size_t iterations = 0u;
    int rc = -1;

    if (num_is_real(base)) {
        number_t seeds[4];
        size_t seed_count = sizeof(seeds) / sizeof(seeds[0]);
        size_t probe_limit = goal_complex_probe_iteration_limit(max_iterations);

        seeds[0] = goal_complex_offset_seed(base, NUM_NEG_I, digits);
        seeds[1] = goal_complex_offset_seed(base, NUM_I, digits);
        seeds[2] = goal_work_value(NUM_NEG_I, digits);
        seeds[3] = goal_work_value(NUM_I, digits);

        for (size_t i = 0u; i < seed_count; ++i) {
            size_t attempt_iterations = 0u;
            size_t attempt_limit = i < 2u ? probe_limit : max_iterations;

            rc = goal_complex_newton_from(expr, var, target, tolerance, seeds[i], digits, attempt_limit,
                                          &attempt_iterations);
            iterations += attempt_iterations;
            if (rc == 0)
                break;
        }

        for (size_t i = 0u; i < seed_count; ++i)
            num_destroy(&seeds[i]);
    } else {
        rc = goal_complex_newton_from(expr, var, target, tolerance, base, digits, max_iterations, &iterations);
    }

    if (iterations_out)
        *iterations_out = iterations;
    num_destroy(&base);
    return rc;
}

static void goal_set_vars(expr_t **vars, number_t *values, size_t nvars, size_t digits)
{
    for (size_t i = 0u; i < nvars; ++i)
        goal_set_var(vars[i], values[i], digits);
}

static void goal_destroy_numbers(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
}

static int goal_eval_real_gradient(expr_t *expr, expr_t **vars, size_t nvars, number_t target, number_t *residual_out,
                                   number_t *derivs_out)
{
    const expr_t **var_refs = calloc(nvars, sizeof(*var_refs));
    number_t value = (number_t){0};
    number_t residual;
    int rc = -1;

    if (!var_refs)
        return -1;

    for (size_t i = 0u; i < nvars; ++i)
        var_refs[i] = vars[i];

    if (expr_eval_derivatives(expr, nvars, var_refs, &value, derivs_out) != 0)
        goto cleanup;

    residual = num_sub(value, target);
    if (!goal_residual_real(residual)) {
        num_destroy(&residual);
        goal_destroy_numbers(derivs_out, nvars);
        goto cleanup;
    }

    for (size_t i = 0u; i < nvars; ++i) {
        if (!goal_residual_real(derivs_out[i])) {
            num_destroy(&residual);
            goal_destroy_numbers(derivs_out, nvars);
            goto cleanup;
        }
    }

    *residual_out = residual;
    rc = 0;

cleanup:
    num_destroy(&value);
    free(var_refs);
    return rc;
}

static int goal_solve_real_multi(expr_t *expr, expr_t **vars, size_t nvars, number_t target, number_t tolerance,
                                 size_t digits, size_t max_iterations, size_t *iterations_out)
{
    static const char *const alphas[] = {"1", "0.5", "0.25", "0.125", "0.0625", "0.03125"};
    number_t *values = calloc(nvars, sizeof(*values));
    number_t best_residual = num_new();
    number_t best_mag = num_clone(NUM_INF);
    size_t iterations = 0u;
    int rc = -1;

    if (!values)
        goto cleanup;

    for (size_t i = 0u; i < nvars; ++i)
        values[i] = goal_start_value(vars[i], digits);
    goal_set_vars(vars, values, nvars, digits);

    for (size_t iter = 0u; iter < max_iterations; ++iter) {
        number_t *derivs = calloc(nvars, sizeof(*derivs));
        number_t residual = (number_t){0};
        number_t residual_mag;
        number_t norm2 = num_create_from_long(0L);
        number_t neg_residual;
        number_t scale;
        bool improved = false;

        iterations++;
        if (!derivs || goal_eval_real_gradient(expr, vars, nvars, target, &residual, derivs) != 0) {
            free(derivs);
            num_destroy(&residual);
            num_destroy(&norm2);
            break;
        }

        residual_mag = num_abs(residual);
        if (num_lt(residual_mag, best_mag)) {
            num_destroy(&best_mag);
            num_destroy(&best_residual);
            best_mag = num_clone(residual_mag);
            best_residual = num_clone(residual);
        }
        if (num_le(residual_mag, tolerance)) {
            num_destroy(&residual_mag);
            num_destroy(&residual);
            num_destroy(&norm2);
            goal_destroy_numbers(derivs, nvars);
            free(derivs);
            rc = 0;
            break;
        }

        for (size_t i = 0u; i < nvars; ++i) {
            number_t square = num_mul(derivs[i], derivs[i]);
            number_t next = num_add(norm2, square);

            num_destroy(&square);
            num_destroy(&norm2);
            norm2 = next;
        }
        if (num_is_zero(norm2)) {
            num_destroy(&residual_mag);
            num_destroy(&residual);
            num_destroy(&norm2);
            goal_destroy_numbers(derivs, nvars);
            free(derivs);
            break;
        }

        neg_residual = num_neg(residual);
        scale = num_div(neg_residual, norm2);
        num_destroy(&neg_residual);

        for (size_t a = 0u; a < sizeof(alphas) / sizeof(alphas[0]); ++a) {
            number_t alpha = num_create_from_string(alphas[a]);
            number_t scaled = num_mul(scale, alpha);
            number_t *trial = calloc(nvars, sizeof(*trial));
            number_t trial_value = (number_t){0};
            number_t trial_residual = (number_t){0};
            number_t trial_mag;

            num_destroy(&alpha);
            if (!trial) {
                num_destroy(&scaled);
                num_destroy(&trial_value);
                num_destroy(&trial_residual);
                continue;
            }

            for (size_t i = 0u; i < nvars; ++i) {
                number_t delta = num_mul(scaled, derivs[i]);

                trial[i] = num_add(values[i], delta);
                if (digits > 0u)
                    num_set_prec_digits(&trial[i], digits);
                num_destroy(&delta);
            }

            goal_set_vars(vars, trial, nvars, digits);
            if (goal_eval_residual(expr, target, &trial_value, &trial_residual) != 0 ||
                !goal_residual_real(trial_residual)) {
                num_destroy(&trial_value);
                num_destroy(&trial_residual);
                goal_destroy_numbers(trial, nvars);
                free(trial);
                num_destroy(&scaled);
                continue;
            }

            trial_mag = num_abs(trial_residual);
            if (num_lt(trial_mag, residual_mag)) {
                goal_destroy_numbers(values, nvars);
                free(values);
                values = trial;
                improved = true;
                if (num_lt(trial_mag, best_mag)) {
                    num_destroy(&best_mag);
                    num_destroy(&best_residual);
                    best_mag = num_clone(trial_mag);
                    best_residual = num_clone(trial_residual);
                }
                num_destroy(&trial_mag);
                num_destroy(&trial_value);
                num_destroy(&trial_residual);
                num_destroy(&scaled);
                break;
            }

            num_destroy(&trial_mag);
            num_destroy(&trial_value);
            num_destroy(&trial_residual);
            goal_destroy_numbers(trial, nvars);
            free(trial);
            num_destroy(&scaled);
        }

        if (!improved)
            goal_set_vars(vars, values, nvars, digits);

        num_destroy(&scale);
        num_destroy(&residual_mag);
        num_destroy(&residual);
        num_destroy(&norm2);
        goal_destroy_numbers(derivs, nvars);
        free(derivs);

        if (!improved)
            break;
    }

    if (goal_residual_close(best_residual, tolerance))
        rc = 0;

cleanup:
    if (values) {
        goal_set_vars(vars, values, nvars, digits);
        goal_destroy_numbers(values, nvars);
        free(values);
    }
    if (iterations_out)
        *iterations_out = iterations;
    num_destroy(&best_mag);
    num_destroy(&best_residual);
    return rc;
}

int expr_goal_seek(expr_t *expr, expr_bindings_t *bindings, number_t target, const expr_goal_seek_options_t *options,
                   expr_goal_seek_result_t *result)
{
    expr_t **vars = NULL;
    int nvars;
    size_t digits;
    size_t work_digits;
    size_t max_iterations;
    size_t iterations = 0u;
    number_t tolerance;
    int rc = -1;

    if (!expr || !bindings || !result)
        return -1;

    goal_result_reset(result);
    nvars = goal_collect_variables(bindings, &vars);
    if (nvars <= 0)
        goto cleanup_no_tol;

    digits = goal_precision_digits(options);
    work_digits = goal_work_digits(digits);
    max_iterations = goal_max_iterations(options);
    tolerance = goal_tolerance(target, options);

    if (nvars == 1 &&
        goal_solve_real_one(expr, vars[0], target, tolerance, work_digits, max_iterations, &iterations) == 0) {
        rc = goal_finish(expr, bindings, target, tolerance, options, false, iterations, result);
        goto cleanup;
    }

    if (nvars > 1 && goal_solve_real_multi(expr, vars, (size_t)nvars, target, tolerance, work_digits, max_iterations,
                                           &iterations) == 0) {
        rc = goal_finish(expr, bindings, target, tolerance, options, false, iterations, result);
        goto cleanup;
    }

    if (options && options->allow_complex && nvars == 1 &&
        goal_solve_complex_one(expr, vars[0], target, tolerance, work_digits, max_iterations, &iterations) == 0) {
        rc = goal_finish(expr, bindings, target, tolerance, options, true, iterations, result);
    }

cleanup:
    num_destroy(&tolerance);
cleanup_no_tol:
    free(vars);
    if (rc != 0)
        expr_goal_seek_result_clear(result);
    return rc;
}
