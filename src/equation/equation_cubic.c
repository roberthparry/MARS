#include <stdbool.h>
#include <stddef.h>

#include "equation_internal.h"
#include "number.h"

static void equation_init_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
}

static void equation_destroy_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
}

static number_t equation_div_long(number_t value, long divisor)
{
    number_t denom = num_create_from_long(divisor);
    number_t quotient = num_div(value, denom);

    num_destroy(&denom);
    return quotient;
}

static number_t equation_cubic_shifted_root(number_t term, number_t shift)
{
    return num_sub(term, shift);
}

static number_t equation_cubic_sum_root(number_t first,
                                        number_t second,
                                        number_t shift)
{
    number_t sum = num_add(first, second);
    number_t root = equation_cubic_shifted_root(sum, shift);

    num_destroy(&sum);
    return root;
}

static number_t equation_cubic_real_cuberoot(number_t value)
{
    number_t third = num_create_from_frac(1L, 3L);
    number_t root;

    if (num_is_real(value) && num_lt(value, NUM_ZERO)) {
        number_t positive = num_neg(value);
        number_t positive_root = num_pow(positive, third);

        root = num_neg(positive_root);
        num_destroy(&positive_root);
        num_destroy(&positive);
    } else {
        root = num_pow(value, third);
    }

    num_destroy(&third);
    return root;
}

static number_t equation_cubic_omega(bool conjugate)
{
    number_t real = num_create_from_frac(-1L, 2L);
    number_t imag = num_mul(NUM_I, NUM_SQRT3_OVER_TWO);
    number_t omega = conjugate ? num_sub(real, imag) : num_add(real, imag);

    num_destroy(&imag);
    num_destroy(&real);
    return omega;
}

static int equation_append_distinct_root(const expr_t *wrt,
                                         number_t root,
                                         number_t *seen,
                                         size_t *seen_count,
                                         equation_solutions_t *solutions)
{
    for (size_t i = 0u; i < *seen_count; ++i) {
        if (num_eq(root, seen[i]))
            return 0;
    }

    if (equation_append_solution_value(wrt, root, solutions) != 0)
        return -1;

    seen[*seen_count] = num_clone(root);
    ++*seen_count;
    return 0;
}

static void equation_cubic_reduce(const number_t *coeffs,
                                  number_t *p_out,
                                  number_t *q_out,
                                  number_t *shift_out)
{
    number_t a = coeffs[3];
    number_t b = coeffs[2];
    number_t c = coeffs[1];
    number_t d = coeffs[0];
    number_t norm_b = num_div(b, a);
    number_t norm_c = num_div(c, a);
    number_t norm_d = num_div(d, a);
    number_t norm_b_sq = num_mul(norm_b, norm_b);
    number_t norm_b_cu = num_mul(norm_b_sq, norm_b);
    number_t norm_b_sq_third = equation_div_long(norm_b_sq, 3L);
    number_t p = num_sub(norm_c, norm_b_sq_third);
    number_t two_norm_b_cu = num_mul_long(norm_b_cu, 2L);
    number_t first_q_term = equation_div_long(two_norm_b_cu, 27L);
    number_t norm_b_norm_c = num_mul(norm_b, norm_c);
    number_t second_q_term = equation_div_long(norm_b_norm_c, 3L);
    number_t first_q_sum = num_sub(first_q_term, second_q_term);
    number_t q = num_add(first_q_sum, norm_d);
    number_t shift = equation_div_long(norm_b, 3L);

    num_destroy(p_out);
    *p_out = p;
    num_destroy(q_out);
    *q_out = q;
    num_destroy(shift_out);
    *shift_out = shift;

    num_destroy(&first_q_sum);
    num_destroy(&second_q_term);
    num_destroy(&norm_b_norm_c);
    num_destroy(&first_q_term);
    num_destroy(&two_norm_b_cu);
    num_destroy(&norm_b_sq_third);
    num_destroy(&norm_b_cu);
    num_destroy(&norm_b_sq);
    num_destroy(&norm_d);
    num_destroy(&norm_c);
    num_destroy(&norm_b);
}

static number_t equation_cubic_discriminant(number_t p, number_t q)
{
    number_t q_half = equation_div_long(q, 2L);
    number_t p_third = equation_div_long(p, 3L);
    number_t q_half_sq = num_mul(q_half, q_half);
    number_t p_third_cu = num_pow_int(p_third, 3);
    number_t discriminant = num_add(q_half_sq, p_third_cu);

    num_destroy(&p_third_cu);
    num_destroy(&q_half_sq);
    num_destroy(&p_third);
    num_destroy(&q_half);
    return discriminant;
}

static int equation_append_cubic_zero_discriminant_roots(const expr_t *wrt,
                                                         number_t p,
                                                         number_t q,
                                                         number_t shift,
                                                         number_t *seen,
                                                         size_t *seen_count,
                                                         equation_solutions_t *solutions)
{
    number_t root;
    int rc = 0;

    if (num_is_zero(p)) {
        number_t zero_term = num_new();

        root = equation_cubic_shifted_root(zero_term, shift);
        rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);
        num_destroy(&root);
        num_destroy(&zero_term);
        return rc;
    }

    {
        number_t three_q = num_mul_long(q, 3L);
        number_t first_term = num_div(three_q, p);
        number_t neg_three_q = num_neg(three_q);
        number_t two_p = num_mul_long(p, 2L);
        number_t second_term = num_div(neg_three_q, two_p);

        root = equation_cubic_shifted_root(first_term, shift);
        rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);
        num_destroy(&root);

        if (rc == 0) {
            root = equation_cubic_shifted_root(second_term, shift);
            rc = equation_append_distinct_root(wrt, root, seen, seen_count,
                                               solutions);
            num_destroy(&root);
        }

        num_destroy(&second_term);
        num_destroy(&two_p);
        num_destroy(&neg_three_q);
        num_destroy(&first_term);
        num_destroy(&three_q);
    }
    return rc;
}

static int equation_append_cubic_trig_roots(const expr_t *wrt,
                                            number_t p,
                                            number_t q,
                                            number_t shift,
                                            number_t *seen,
                                            size_t *seen_count,
                                            equation_solutions_t *solutions)
{
    number_t neg_p = num_neg(p);
    number_t neg_p_third = equation_div_long(neg_p, 3L);
    number_t radius_base = num_sqrt(neg_p_third);
    number_t radius = num_mul_long(radius_base, 2L);
    number_t three_q = num_mul_long(q, 3L);
    number_t two_p = num_mul_long(p, 2L);
    number_t first_factor = num_div(three_q, two_p);
    number_t neg_three = num_create_from_long(-3L);
    number_t second_factor_arg = num_div(neg_three, p);
    number_t second_factor = num_sqrt(second_factor_arg);
    number_t acos_arg = num_mul(first_factor, second_factor);
    number_t theta_raw = num_acos(acos_arg);
    number_t theta = equation_div_long(theta_raw, 3L);
    number_t two_pi = num_mul_long(NUM_PI, 2L);
    number_t turn = equation_div_long(two_pi, 3L);
    int rc = 0;

    for (size_t i = 0u; i < 3u && rc == 0; ++i) {
        number_t offset = num_mul_long(turn, (long)i);
        number_t angle = num_sub(theta, offset);
        number_t cos_angle = num_cos(angle);
        number_t term = num_mul(radius, cos_angle);
        number_t root = equation_cubic_shifted_root(term, shift);

        rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);

        num_destroy(&root);
        num_destroy(&term);
        num_destroy(&cos_angle);
        num_destroy(&angle);
        num_destroy(&offset);
    }

    num_destroy(&turn);
    num_destroy(&two_pi);
    num_destroy(&theta);
    num_destroy(&theta_raw);
    num_destroy(&acos_arg);
    num_destroy(&second_factor);
    num_destroy(&second_factor_arg);
    num_destroy(&neg_three);
    num_destroy(&first_factor);
    num_destroy(&two_p);
    num_destroy(&three_q);
    num_destroy(&radius);
    num_destroy(&radius_base);
    num_destroy(&neg_p_third);
    num_destroy(&neg_p);
    return rc;
}

static int equation_append_cubic_cardano_roots(const expr_t *wrt,
                                               number_t p,
                                               number_t q,
                                               number_t shift,
                                               number_t discriminant,
                                               number_t *seen,
                                               size_t *seen_count,
                                               equation_solutions_t *solutions)
{
    number_t q_half = equation_div_long(q, 2L);
    number_t neg_q_half = num_neg(q_half);
    number_t sqrt_discriminant = num_sqrt(discriminant);
    number_t u_arg = num_add(neg_q_half, sqrt_discriminant);
    number_t v_arg = num_sub(neg_q_half, sqrt_discriminant);
    number_t u = equation_cubic_real_cuberoot(u_arg);
    number_t v = equation_cubic_real_cuberoot(v_arg);
    number_t omega = equation_cubic_omega(false);
    number_t omega_conj = equation_cubic_omega(true);
    number_t omega_u;
    number_t omega_v;
    number_t root;
    int rc;

    (void)p;

    root = equation_cubic_sum_root(u, v, shift);
    rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);
    num_destroy(&root);

    if (rc == 0) {
        omega_u = num_mul(omega, u);
        omega_v = num_mul(omega_conj, v);
        root = equation_cubic_sum_root(omega_u, omega_v, shift);
        rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);
        num_destroy(&root);
        num_destroy(&omega_v);
        num_destroy(&omega_u);
    }

    if (rc == 0) {
        omega_u = num_mul(omega_conj, u);
        omega_v = num_mul(omega, v);
        root = equation_cubic_sum_root(omega_u, omega_v, shift);
        rc = equation_append_distinct_root(wrt, root, seen, seen_count, solutions);
        num_destroy(&root);
        num_destroy(&omega_v);
        num_destroy(&omega_u);
    }

    num_destroy(&omega_conj);
    num_destroy(&omega);
    num_destroy(&v);
    num_destroy(&u);
    num_destroy(&v_arg);
    num_destroy(&u_arg);
    num_destroy(&sqrt_discriminant);
    num_destroy(&neg_q_half);
    num_destroy(&q_half);
    return rc;
}

static expr_t *equation_cubic_simplify_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;

    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

static expr_t *equation_cubic_retain_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;

    expr_retain(expr);
    return (expr_t *)expr;
}

static expr_t *equation_cubic_expr_const_long(long value)
{
    number_t number = num_create_from_long(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

static expr_t *equation_cubic_expr_mul_long(const expr_t *expr, long value)
{
    number_t number = num_create_from_long(value);
    expr_t *out = expr ? expr_mul_num(expr, &number) : NULL;

    num_destroy(&number);
    return out;
}

static expr_t *equation_cubic_expr_div_long(const expr_t *expr, long value)
{
    number_t number = num_create_from_long(value);
    expr_t *denom = expr_new_const(number);
    expr_t *out = (expr && denom) ? expr_div(expr, denom) : NULL;

    expr_free(denom);
    num_destroy(&number);
    return out;
}

static expr_t *equation_cubic_expr_pow_long(const expr_t *expr, long exponent)
{
    number_t number = num_create_from_long(exponent);
    expr_t *out = expr ? expr_pow(expr, &number) : NULL;

    num_destroy(&number);
    return out;
}

static expr_t *equation_cubic_expr_cuberoot(const expr_t *expr)
{
    number_t exponent = num_create_from_frac(1L, 3L);
    expr_t *root = expr ? expr_pow(expr, &exponent) : NULL;
    expr_t *out = equation_cubic_simplify_owned(root);

    num_destroy(&exponent);
    return out;
}

static expr_t *equation_cubic_symbolic_p(const expr_t *a,
                                         const expr_t *b,
                                         const expr_t *c)
{
    expr_t *a_sq = equation_cubic_expr_pow_long(a, 2L);
    expr_t *ac = expr_mul(a, c);
    expr_t *three_ac = equation_cubic_expr_mul_long(ac, 3L);
    expr_t *b_sq = equation_cubic_expr_pow_long(b, 2L);
    expr_t *numerator = (three_ac && b_sq) ? expr_sub(three_ac, b_sq) : NULL;
    expr_t *denominator = equation_cubic_expr_mul_long(a_sq, 3L);
    expr_t *quotient = (numerator && denominator)
        ? expr_div(numerator, denominator)
        : NULL;
    expr_t *out = equation_cubic_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(b_sq);
    expr_free(three_ac);
    expr_free(ac);
    expr_free(a_sq);
    return out;
}

static expr_t *equation_cubic_symbolic_q(const expr_t *a,
                                         const expr_t *b,
                                         const expr_t *c,
                                         const expr_t *d)
{
    expr_t *a_sq = equation_cubic_expr_pow_long(a, 2L);
    expr_t *a_cu = equation_cubic_expr_pow_long(a, 3L);
    expr_t *b_cu = equation_cubic_expr_pow_long(b, 3L);
    expr_t *a_sq_d = (a_sq && d) ? expr_mul(a_sq, d) : NULL;
    expr_t *first = equation_cubic_expr_mul_long(a_sq_d, 27L);
    expr_t *ab = expr_mul(a, b);
    expr_t *abc = (ab && c) ? expr_mul(ab, c) : NULL;
    expr_t *second = equation_cubic_expr_mul_long(abc, 9L);
    expr_t *third = equation_cubic_expr_mul_long(b_cu, 2L);
    expr_t *first_diff = (first && second) ? expr_sub(first, second) : NULL;
    expr_t *numerator = (first_diff && third) ? expr_add(first_diff, third) : NULL;
    expr_t *denominator = equation_cubic_expr_mul_long(a_cu, 27L);
    expr_t *quotient = (numerator && denominator)
        ? expr_div(numerator, denominator)
        : NULL;
    expr_t *out = equation_cubic_simplify_owned(quotient);

    expr_free(denominator);
    expr_free(numerator);
    expr_free(first_diff);
    expr_free(third);
    expr_free(second);
    expr_free(abc);
    expr_free(ab);
    expr_free(first);
    expr_free(a_sq_d);
    expr_free(b_cu);
    expr_free(a_cu);
    expr_free(a_sq);
    return out;
}

static expr_t *equation_cubic_symbolic_shift(const expr_t *a,
                                             const expr_t *b)
{
    expr_t *denominator = equation_cubic_expr_mul_long(a, 3L);
    expr_t *quotient = denominator ? expr_div(b, denominator) : NULL;
    expr_t *out = equation_cubic_simplify_owned(quotient);

    expr_free(denominator);
    return out;
}

static expr_t *equation_cubic_symbolic_discriminant(const expr_t *p,
                                                    const expr_t *q)
{
    expr_t *q_half = equation_cubic_expr_div_long(q, 2L);
    expr_t *p_third = equation_cubic_expr_div_long(p, 3L);
    expr_t *q_half_sq = equation_cubic_expr_pow_long(q_half, 2L);
    expr_t *p_third_cu = equation_cubic_expr_pow_long(p_third, 3L);
    expr_t *sum = (q_half_sq && p_third_cu)
        ? expr_add(q_half_sq, p_third_cu)
        : NULL;
    expr_t *out = equation_cubic_simplify_owned(sum);

    expr_free(p_third_cu);
    expr_free(q_half_sq);
    expr_free(p_third);
    expr_free(q_half);
    return out;
}

static bool equation_cubic_symbolic_uv(const expr_t *q,
                                       const expr_t *discriminant,
                                       expr_t **u_out,
                                       expr_t **v_out)
{
    expr_t *q_half = equation_cubic_expr_div_long(q, 2L);
    expr_t *neg_q_half = q_half ? expr_neg(q_half) : NULL;
    expr_t *sqrt_discriminant = discriminant ? expr_sqrt(discriminant) : NULL;
    expr_t *u_arg = (neg_q_half && sqrt_discriminant)
        ? expr_add(neg_q_half, sqrt_discriminant)
        : NULL;
    expr_t *v_arg = (neg_q_half && sqrt_discriminant)
        ? expr_sub(neg_q_half, sqrt_discriminant)
        : NULL;
    bool ok = false;

    *u_out = equation_cubic_expr_cuberoot(u_arg);
    *v_out = equation_cubic_expr_cuberoot(v_arg);
    ok = *u_out && *v_out;

    expr_free(v_arg);
    expr_free(u_arg);
    expr_free(sqrt_discriminant);
    expr_free(neg_q_half);
    expr_free(q_half);
    return ok;
}

static expr_t *equation_cubic_omega_expr(bool conjugate)
{
    expr_t *neg_one = equation_cubic_expr_const_long(-1L);
    expr_t *three = equation_cubic_expr_const_long(3L);
    expr_t *sqrt_three = three ? expr_sqrt(three) : NULL;
    expr_t *imag_unit = expr_new_const(NUM_I);
    expr_t *imag = (imag_unit && sqrt_three)
        ? expr_mul(imag_unit, sqrt_three)
        : NULL;
    expr_t *numerator = (neg_one && imag)
        ? (conjugate ? expr_sub(neg_one, imag) : expr_add(neg_one, imag))
        : NULL;
    expr_t *out = equation_cubic_expr_div_long(numerator, 2L);

    expr_free(numerator);
    expr_free(imag);
    expr_free(imag_unit);
    expr_free(sqrt_three);
    expr_free(three);
    expr_free(neg_one);
    return out;
}

static expr_t *equation_cubic_symbolic_root(const expr_t *u,
                                            const expr_t *v,
                                            const expr_t *shift,
                                            const expr_t *u_factor,
                                            const expr_t *v_factor)
{
    expr_t *u_term = u_factor ? expr_mul(u_factor, u)
                              : equation_cubic_retain_expr(u);
    expr_t *v_term = v_factor ? expr_mul(v_factor, v)
                              : equation_cubic_retain_expr(v);
    expr_t *sum = (u_term && v_term) ? expr_add(u_term, v_term) : NULL;
    expr_t *raw_root = (sum && shift) ? expr_sub(sum, shift) : NULL;
    expr_t *root = equation_cubic_simplify_owned(raw_root);

    expr_free(sum);
    expr_free(v_term);
    expr_free(u_term);
    return root;
}

static int equation_try_solve_symbolic_cubic(const expr_t *residual,
                                             const expr_t *wrt,
                                             equation_solutions_t *solutions)
{
    expr_t *constant = NULL;
    expr_t *linear = NULL;
    expr_t *quadratic = NULL;
    expr_t *cubic = NULL;
    expr_t *p = NULL;
    expr_t *q = NULL;
    expr_t *shift = NULL;
    expr_t *discriminant = NULL;
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *omega = NULL;
    expr_t *omega_conj = NULL;
    expr_t *root = NULL;
    int rc = -1;

    if (!equation_match_symbolic_cubic_expr(residual, wrt, &constant,
                                            &linear, &quadratic, &cubic)) {
        rc = 1;
        goto cleanup;
    }

    p = equation_cubic_symbolic_p(cubic, quadratic, linear);
    q = equation_cubic_symbolic_q(cubic, quadratic, linear, constant);
    shift = equation_cubic_symbolic_shift(cubic, quadratic);
    discriminant = (p && q) ? equation_cubic_symbolic_discriminant(p, q) : NULL;
    if (!p || !q || !shift || !discriminant ||
        !equation_cubic_symbolic_uv(q, discriminant, &u, &v))
        goto cleanup;

    root = equation_cubic_symbolic_root(u, v, shift, NULL, NULL);
    if (!root || equation_append_solution_expr(wrt, root, solutions) != 0)
        goto cleanup;
    expr_free(root);
    root = NULL;

    omega = equation_cubic_omega_expr(false);
    omega_conj = equation_cubic_omega_expr(true);
    root = equation_cubic_symbolic_root(u, v, shift, omega, omega_conj);
    if (!root || equation_append_solution_expr(wrt, root, solutions) != 0) {
        equation_solutions_clear(solutions);
        goto cleanup;
    }
    expr_free(root);
    root = NULL;

    root = equation_cubic_symbolic_root(u, v, shift, omega_conj, omega);
    if (!root || equation_append_solution_expr(wrt, root, solutions) != 0) {
        equation_solutions_clear(solutions);
        goto cleanup;
    }

    rc = 0;

cleanup:
    expr_free(root);
    expr_free(omega_conj);
    expr_free(omega);
    expr_free(v);
    expr_free(u);
    expr_free(discriminant);
    expr_free(shift);
    expr_free(q);
    expr_free(p);
    expr_free(cubic);
    expr_free(quadratic);
    expr_free(linear);
    expr_free(constant);
    return rc;
}

int equation_try_solve_cubic(const equation_t *equation,
                             const expr_t *wrt,
                             equation_solutions_t *solutions)
{
    expr_t *residual = equation_residual(equation);
    number_t coeffs[4];
    number_t p = num_new();
    number_t q = num_new();
    number_t shift = num_new();
    number_t discriminant = num_new();
    number_t seen[3];
    size_t seen_count = 0u;
    bool ok;
    int rc = -1;

    equation_init_numbers(coeffs, 4u);
    if (!residual)
        goto cleanup;

    ok = equation_match_polynomial_expr(residual, wrt, 3u, coeffs) &&
         !num_is_zero(coeffs[3]);
    if (!ok) {
        rc = equation_try_solve_symbolic_cubic(residual, wrt, solutions);
        goto cleanup;
    }

    equation_cubic_reduce(coeffs, &p, &q, &shift);
    num_destroy(&discriminant);
    discriminant = equation_cubic_discriminant(p, q);

    if (num_is_real(discriminant) && num_lt(discriminant, NUM_ZERO)) {
        rc = equation_append_cubic_trig_roots(wrt, p, q, shift, seen,
                                              &seen_count, solutions);
    } else if (num_is_zero(discriminant)) {
        rc = equation_append_cubic_zero_discriminant_roots(wrt, p, q, shift,
                                                           seen, &seen_count,
                                                           solutions);
    } else {
        rc = equation_append_cubic_cardano_roots(wrt, p, q, shift,
                                                 discriminant, seen,
                                                 &seen_count, solutions);
    }

cleanup:
    for (size_t i = 0u; i < seen_count; ++i)
        num_destroy(&seen[i]);
    num_destroy(&discriminant);
    num_destroy(&shift);
    num_destroy(&q);
    num_destroy(&p);
    equation_destroy_numbers(coeffs, 4u);
    expr_free(residual);
    return rc;
}
