#include <stdbool.h>

#include "expr_integrate_internal.h"
#include "internal/number_internal.h"

static number_t pow_small_number(number_t base, size_t exponent)
{
    number_t out = num_clone(NUM_ONE);

    for (size_t i = 0; i < exponent; ++i) {
        number_t next = num_mul(out, base);

        num_destroy(&out);
        out = next;
    }
    return out;
}

bool expr_integrate_rewrite_poly_deg4_to_affine_basis(number_t *poly,
                                                      number_t from_constant,
                                                      number_t from_coeff,
                                                      number_t to_constant,
                                                      number_t to_coeff)
{
    static const long binomial[5][5] = {
        { 1, 0, 0, 0, 0 },
        { 1, 1, 0, 0, 0 },
        { 1, 2, 1, 0, 0 },
        { 1, 3, 3, 1, 0 },
        { 1, 4, 6, 4, 1 }
    };
    number_t alpha;
    number_t scaled_to_constant;
    number_t beta;
    number_t rewritten[5];

    if (num_eq(from_constant, to_constant) && num_eq(from_coeff, to_coeff))
        return true;
    if (num_is_zero(from_constant) && num_is_zero(from_coeff))
        return true;
    if (num_is_zero(to_coeff))
        return false;

    alpha = num_div(from_coeff, to_coeff);
    scaled_to_constant = num_mul(alpha, to_constant);
    beta = num_sub(from_constant, scaled_to_constant);
    number_array_zero_local(rewritten, 5);

    for (size_t i = 0; i < 5u; ++i) {
        if (num_is_zero(poly[i]))
            continue;

        for (size_t j = 0; j <= i; ++j) {
            number_t alpha_power = pow_small_number(alpha, j);
            number_t beta_power = pow_small_number(beta, i - j);
            number_t power_product = num_mul(alpha_power, beta_power);
            number_t choose = num_create_from_long(binomial[i][j]);
            number_t scale = num_mul(power_product, choose);
            number_t term = num_mul(poly[i], scale);
            number_t next = num_add(rewritten[j], term);

            num_destroy(&rewritten[j]);
            rewritten[j] = next;
            num_destroy(&term);
            num_destroy(&scale);
            num_destroy(&choose);
            num_destroy(&power_product);
            num_destroy(&beta_power);
            num_destroy(&alpha_power);
        }
    }

    for (size_t i = 0; i < 5u; ++i) {
        num_destroy(&poly[i]);
        poly[i] = rewritten[i];
    }

    num_destroy(&beta);
    num_destroy(&scaled_to_constant);
    num_destroy(&alpha);
    return true;
}

static bool integrate_long_perfect_square_root(long value, long *root_out)
{
    long lo = 0L;
    long hi = value;

    if (value < 0L || !root_out)
        return false;

    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2L;

        if (mid != 0L && mid > value / mid) {
            hi = mid - 1L;
            continue;
        }

        long square = mid * mid;

        if (square == value) {
            *root_out = mid;
            return true;
        }
        if (square < value) {
            lo = mid + 1L;
        } else {
            hi = mid - 1L;
        }
    }

    return false;
}

static bool integrate_exact_small_rational_sqrt(number_t value, number_t *root_out)
{
    long numerator;
    long denominator;
    long root_numerator;
    long root_denominator;
    number_t numerator_expr;
    number_t denominator_expr;

    if (!root_out ||
        !num_get_small_rational(value, &numerator, &denominator) ||
        numerator < 0L ||
        denominator <= 0L ||
        !integrate_long_perfect_square_root(numerator, &root_numerator) ||
        !integrate_long_perfect_square_root(denominator, &root_denominator) ||
        root_denominator == 0L) {
        return false;
    }

    numerator_expr = num_create_from_long(root_numerator);
    denominator_expr = num_create_from_long(root_denominator);
    num_destroy(root_out);
    *root_out = num_div(numerator_expr, denominator_expr);
    num_destroy(&denominator_expr);
    num_destroy(&numerator_expr);
    return true;
}

static bool match_affine_power_factor(const expr_t *expr,
                                      const expr_t *wrt,
                                      number_t *constant_out,
                                      number_t *coeff_out,
                                      number_t *exponent_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !constant_out || !coeff_out || !exponent_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT && expr->a) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(NUM_HALF);
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(expr->c);
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent)) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(exponent);
        }
    } else {
        ok = match_nonconstant_affine_linear_expr(expr, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(NUM_ONE);
        }
    }

    num_destroy(&exponent);
    return ok;
}

static expr_t *build_integrated_poly_affine_power(const number_t *poly,
                                                  number_t power_constant,
                                                  number_t power_coeff,
                                                  number_t exponent,
                                                  const expr_t *wrt)
{
    expr_t *u = build_affine_from_match(wrt, power_constant, power_coeff);
    expr_t *sum = NULL;

    if (!u)
        return NULL;

    for (size_t i = 0; i < 5u; ++i) {
        number_t degree = num_create_from_long((long)i);
        number_t integrand_exponent = num_add(exponent, degree);
        number_t next_exponent = num_add(integrand_exponent, NUM_ONE);
        expr_t *term = NULL;

        if (!num_is_zero(poly[i])) {
            if (num_eq(next_exponent, NUM_ZERO)) {
                expr_t *log_u = expr_log(u);

                term = log_u ? mul_number_owned(log_u, poly[i]) : NULL;
                term = div_number_owned(term, power_coeff);
            } else {
                expr_t *u_power = expr_pow(u, &next_exponent);
                number_t denom = num_mul(power_coeff, next_exponent);

                term = u_power ? mul_number_owned(u_power, poly[i]) : NULL;
                term = div_number_owned_consuming(term, &denom);
            }
        }

        if (term) {
            if (sum) {
                expr_t *next = expr_add(sum, term);

                expr_free(sum);
                expr_free(term);
                sum = next;
            } else {
                sum = term;
            }
        }

        num_destroy(&next_exponent);
        num_destroy(&integrand_exponent);
        num_destroy(&degree);
    }

    expr_free(u);
    return simplify_owned(sum);
}

static bool match_poly_and_affine_power_product(const expr_t *poly_expr,
                                                const expr_t *power_expr,
                                                const expr_t *wrt,
                                                number_t *poly,
                                                number_t *power_constant,
                                                number_t *power_coeff,
                                                number_t *exponent)
{
    expr_t *vars[1];
    number_t poly_constant = num_new();
    number_t poly_coeffs[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    poly_coeffs[0] = num_new();
    ok = expr_match_affine_poly_deg4(poly_expr, 1u, vars, poly,
                                     &poly_constant, poly_coeffs) &&
         match_affine_power_factor(power_expr, wrt, power_constant, power_coeff,
                                   exponent) &&
         expr_integrate_rewrite_poly_deg4_to_affine_basis(poly,
                                                          poly_constant,
                                                          poly_coeffs[0],
                                                          *power_constant,
                                                          *power_coeff);

    num_destroy(&poly_coeffs[0]);
    num_destroy(&poly_constant);
    return ok;
}

expr_t *integrate_poly_times_affine_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t poly[5];
    number_t power_constant = num_new();
    number_t power_coeff = num_new();
    number_t exponent = num_new();
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    number_array_zero_local(poly, 5);
    if (expr_match_mul_expr(expr, &left, &right)) {
        if (!match_poly_and_affine_power_product(left, right, wrt, poly,
                                                 &power_constant, &power_coeff,
                                                 &exponent)) {
            number_array_clear_local(poly, 5);
            number_array_zero_local(poly, 5);
            if (!match_poly_and_affine_power_product(right, left, wrt, poly,
                                                     &power_constant, &power_coeff,
                                                     &exponent))
                goto cleanup;
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b) {
        if (!match_poly_and_affine_power_product(expr->a, expr->b, wrt, poly,
                                                 &power_constant, &power_coeff,
                                                 &exponent))
            goto cleanup;
        number_t neg_exponent = num_neg(exponent);

        num_destroy(&exponent);
        exponent = neg_exponent;
    } else {
        goto cleanup;
    }

    out = build_integrated_poly_affine_power(poly, power_constant, power_coeff,
                                             exponent, wrt);

cleanup:
    number_array_clear_local(poly, 5);
    num_destroy(&exponent);
    num_destroy(&power_coeff);
    num_destroy(&power_constant);
    return out;
}

expr_t *integrate_poly_times_unary_affine_kind(const expr_t *expr,
                                               const expr_t *wrt,
                                               expr_pattern_unary_affine_kind_t kind)
{
    number_t poly[5];
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *vars[1];
    expr_t *u = NULL;
    expr_t *out = NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    if (!expr_match_affine_poly_deg4_times_unary_affine_kind(expr, kind, 1u, vars,
                                                              poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO)) {
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    if (!u) {
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    if (kind == EXPR_PATTERN_UNARY_EXP) {
        number_t anti[5];
        expr_t *poly_expr;
        expr_t *exp_u;

        exp_antiderivative_once_local(poly, 5u, anti);
        poly_expr = build_polynomial_expr(u, anti, 5u);
        exp_u = expr_exp(u);
        out = (poly_expr && exp_u) ? expr_mul(poly_expr, exp_u) : NULL;
        expr_free(exp_u);
        expr_free(poly_expr);
        number_array_clear_local(anti, 5);
    } else {
        number_t a_src[5];
        number_t b_src[5];
        number_t a_dst[5];
        number_t b_dst[5];
        expr_t *poly_a;
        expr_t *poly_b;
        expr_t *left;
        expr_t *right;
        expr_t *first = NULL;
        expr_t *second = NULL;
        bool trig = (kind == EXPR_PATTERN_UNARY_SIN || kind == EXPR_PATTERN_UNARY_COS);

        number_array_zero_local(a_src, 5);
        number_array_zero_local(b_src, 5);
        if (kind == EXPR_PATTERN_UNARY_SIN || kind == EXPR_PATTERN_UNARY_SINH) {
            for (size_t i = 0; i < 5u; ++i) {
                num_destroy(&a_src[i]);
                a_src[i] = num_clone(poly[i]);
            }
        } else {
            for (size_t i = 0; i < 5u; ++i) {
                num_destroy(&b_src[i]);
                b_src[i] = num_clone(poly[i]);
            }
        }

        if (trig) {
            trig_antiderivative_once_local(a_src, b_src, 5u, a_dst, b_dst);
            first = expr_sin(u);
            second = expr_cos(u);
        } else {
            hyperbolic_antiderivative_once_local(a_src, b_src, 5u, a_dst, b_dst);
            first = expr_sinh(u);
            second = expr_cosh(u);
        }

        poly_a = build_polynomial_expr(u, a_dst, 5u);
        poly_b = build_polynomial_expr(u, b_dst, 5u);
        left = (poly_a && first) ? expr_mul(poly_a, first) : NULL;
        right = (poly_b && second) ? expr_mul(poly_b, second) : NULL;
        out = (left && right) ? expr_add(left, right) : NULL;

        expr_free(right);
        expr_free(left);
        expr_free(poly_b);
        expr_free(poly_a);
        expr_free(second);
        expr_free(first);
        number_array_clear_local(a_dst, 5);
        number_array_clear_local(b_dst, 5);
        number_array_clear_local(a_src, 5);
        number_array_clear_local(b_src, 5);
    }

    expr_free(u);
    number_array_clear_local(poly, 5);
    num_destroy(&constant);
    return div_number_owned_consuming(out, &coeff);
}

expr_t *integrate_poly_times_log_affine(const expr_t *expr, const expr_t *wrt)
{
    number_t poly[5];
    number_t q[6];
    number_t t[6];
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *vars[1];
    expr_t *u;
    expr_t *q_poly;
    expr_t *log_u;
    expr_t *first_term;
    expr_t *t_poly;
    expr_t *raw;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    number_array_zero_local(q, 6);
    number_array_zero_local(t, 6);
    if (!expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_LOG,
                                                              1u, vars, poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO)) {
        number_array_clear_local(t, 6);
        number_array_clear_local(q, 6);
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    for (size_t i = 0; i < 5u; ++i) {
        number_t denom = num_create_from_long((long)(i + 1u));
        number_t q_coeff = num_div(poly[i], denom);

        num_destroy(&q[i + 1u]);
        q[i + 1u] = q_coeff;
        num_destroy(&denom);
    }
    for (size_t i = 1; i < 6u; ++i) {
        number_t denom = num_create_from_long((long)i);
        number_t t_coeff = num_div(q[i], denom);

        num_destroy(&t[i]);
        t[i] = t_coeff;
        num_destroy(&denom);
    }

    u = build_affine_from_match(wrt, constant, coeff);
    q_poly = u ? build_polynomial_expr(u, q, 6u) : NULL;
    log_u = u ? expr_log(u) : NULL;
    first_term = (q_poly && log_u) ? expr_mul(q_poly, log_u) : NULL;
    t_poly = u ? build_polynomial_expr(u, t, 6u) : NULL;
    raw = (first_term && t_poly) ? expr_sub(first_term, t_poly) : NULL;

    expr_free(t_poly);
    expr_free(first_term);
    expr_free(log_u);
    expr_free(q_poly);
    expr_free(u);
    number_array_clear_local(t, 6);
    number_array_clear_local(q, 6);
    number_array_clear_local(poly, 5);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_poly_over_matching_affine(const expr_t *expr, const expr_t *wrt)
{
    number_t poly[5];
    number_t anti[5];
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *vars[1];
    expr_t *u;
    expr_t *poly_term;
    expr_t *log_u = NULL;
    expr_t *log_term = NULL;
    expr_t *raw = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    number_array_zero_local(anti, 5);
    if (!expr_match_affine_poly_deg4(expr->a, 1u, vars, poly, &numer_constant, &numer_coeff) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &denom_constant, &denom_coeff) ||
        !affine_linear_match_eq(numer_constant, numer_coeff, denom_constant, denom_coeff) ||
        num_eq(denom_coeff, NUM_ZERO)) {
        number_array_clear_local(anti, 5);
        number_array_clear_local(poly, 5);
        num_destroy(&denom_coeff);
        num_destroy(&denom_constant);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        return NULL;
    }

    for (size_t i = 1; i < 5u; ++i) {
        number_t denom = num_create_from_long((long)i);
        number_t coeff_i = num_div(poly[i], denom);

        num_destroy(&anti[i]);
        anti[i] = coeff_i;
        num_destroy(&denom);
    }

    u = build_affine_from_match(wrt, denom_constant, denom_coeff);
    poly_term = u ? build_polynomial_expr(u, anti, 5u) : NULL;
    if (!num_eq(poly[0], NUM_ZERO) && u) {
        log_u = expr_log(u);
        log_term = log_u ? expr_mul_num(log_u, &poly[0]) : NULL;
        expr_free(log_u);
    }
    if (poly_term && log_term) {
        raw = expr_add(poly_term, log_term);
    } else if (poly_term) {
        raw = poly_term;
        poly_term = NULL;
    } else if (log_term) {
        raw = log_term;
        log_term = NULL;
    }

    expr_free(log_term);
    expr_free(poly_term);
    expr_free(u);
    number_array_clear_local(anti, 5);
    number_array_clear_local(poly, 5);
    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    num_destroy(&denom_constant);
    return div_number_owned_consuming(raw, &denom_coeff);
}

expr_t *integrate_poly_over_centered_quadratic(const expr_t *expr, const expr_t *wrt)
{
    number_t numer[5];
    number_t denom[5];
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *vars[1];
    expr_t *poly_part = NULL;
    expr_t *log_part = NULL;
    expr_t *atan_part = NULL;
    expr_t *sum = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(numer, 5);
    number_array_zero_local(denom, 5);
    if (!expr_match_affine_poly_deg4(expr->a, 1u, vars, numer, &numer_constant,
                                     &numer_coeff) ||
        !expr_match_affine_poly_deg4(expr->b, 1u, vars, denom, &denom_constant,
                                     &denom_coeff) ||
        !((num_eq(numer_constant, NUM_ZERO) && num_eq(numer_coeff, NUM_ONE)) ||
          (num_is_zero(numer_constant) && num_is_zero(numer_coeff))) ||
        !num_eq(denom_constant, NUM_ZERO) ||
        !num_eq(denom_coeff, NUM_ONE) ||
        !num_is_zero(denom[1]) ||
        !num_is_zero(denom[3]) ||
        !num_is_zero(denom[4]) ||
        num_is_zero(denom[0]) ||
        num_is_zero(denom[2])) {
        goto cleanup;
    }

    number_t q0 = num_div(numer[2], denom[2]);
    number_t q1 = num_div(numer[3], denom[2]);
    number_t half_q1 = num_mul(q1, NUM_HALF);
    number_t c_q0 = num_mul(denom[0], q0);
    number_t c_q1 = num_mul(denom[0], q1);
    number_t r0 = num_sub(numer[0], c_q0);
    number_t r1 = num_sub(numer[1], c_q1);

    if (!num_is_zero(q0) || !num_is_zero(q1)) {
        expr_t *linear = expr_mul_num(wrt, &q0);
        expr_t *x_sq = expr_pow(wrt, &NUM_TWO);
        expr_t *quadratic = x_sq ? expr_mul_num(x_sq, &half_q1) : NULL;

        if (linear && quadratic) {
            poly_part = expr_add(linear, quadratic);
        } else if (linear) {
            poly_part = linear;
            linear = NULL;
        } else if (quadratic) {
            poly_part = quadratic;
            quadratic = NULL;
        }
        expr_free(quadratic);
        expr_free(x_sq);
        expr_free(linear);
    }

    if (!num_is_zero(r1)) {
        number_t two_k = num_mul(NUM_TWO, denom[2]);
        expr_t *log_denom = expr_log(expr->b);
        expr_t *scaled_log = log_denom ? expr_mul_num(log_denom, &r1) : NULL;

        log_part = div_number_owned_consuming(scaled_log, &two_k);
        expr_free(log_denom);
    }

    if (!num_is_zero(r0)) {
        number_t k_over_c = num_div(denom[2], denom[0]);
        bool use_atanh = num_lt(k_over_c, NUM_ZERO);
        number_t abs_c = num_lt(denom[0], NUM_ZERO) ? num_neg(denom[0])
                                                    : num_clone(denom[0]);
        number_t abs_k = num_lt(denom[2], NUM_ZERO) ? num_neg(denom[2])
                                                    : num_clone(denom[2]);
        number_t scale_base = num_div(abs_c, abs_k);
        number_t numeric_scale = num_new();
        bool exact_numeric_scale = integrate_exact_small_rational_sqrt(scale_base,
                                                                       &numeric_scale);
        number_t denom_coeff = use_atanh
                                 ? (num_lt(denom[0], NUM_ZERO) ? num_neg(abs_k)
                                                               : num_clone(abs_k))
                                 : num_clone(denom[2]);

        expr_t *scale_base_expr = exact_numeric_scale ? NULL : expr_new_const(scale_base);
        expr_t *scale_raw = scale_base_expr ? expr_sqrt(scale_base_expr) : NULL;
        expr_t *scale = exact_numeric_scale ? expr_new_const(numeric_scale)
                                            : simplify_owned(scale_raw);
        expr_t *arg = scale ? expr_div(wrt, scale) : NULL;
        expr_t *inverse_arg = arg ? (use_atanh ? expr_atanh(arg)
                                               : expr_atan(arg))
                                  : NULL;
        expr_t *scaled_inverse = inverse_arg ? expr_mul_num(inverse_arg, &r0) : NULL;
        expr_t *denom_expr = scale ? expr_mul_num(scale, &denom_coeff) : NULL;

        atan_part = (scaled_inverse && denom_expr) ? expr_div(scaled_inverse, denom_expr)
                                                   : NULL;
        atan_part = simplify_owned(atan_part);
        expr_free(denom_expr);
        expr_free(scaled_inverse);
        expr_free(inverse_arg);
        expr_free(arg);
        expr_free(scale);
        expr_free(scale_base_expr);
        num_destroy(&denom_coeff);
        num_destroy(&numeric_scale);
        num_destroy(&scale_base);
        num_destroy(&abs_k);
        num_destroy(&abs_c);
        num_destroy(&k_over_c);
    }

    if (poly_part && log_part) {
        sum = expr_add(poly_part, log_part);
    } else if (poly_part) {
        sum = poly_part;
        poly_part = NULL;
    } else if (log_part) {
        sum = log_part;
        log_part = NULL;
    }
    if (sum && atan_part) {
        expr_t *next = expr_add(sum, atan_part);

        expr_free(sum);
        sum = next;
    } else if (!sum && atan_part) {
        sum = atan_part;
        atan_part = NULL;
    }

    num_destroy(&r1);
    num_destroy(&r0);
    num_destroy(&c_q1);
    num_destroy(&c_q0);
    num_destroy(&half_q1);
    num_destroy(&q1);
    num_destroy(&q0);

cleanup:
    expr_free(atan_part);
    expr_free(log_part);
    expr_free(poly_part);
    number_array_clear_local(denom, 5);
    number_array_clear_local(numer, 5);
    num_destroy(&denom_coeff);
    num_destroy(&denom_constant);
    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    return simplify_owned(sum);
}
