#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

/* The absolute-value form is valid away from zeros; a plain real logarithm requires a positive argument. */
static const expr_t *log_trigonometric_argument(const expr_t *expr, bool *cosine)
{
    if (!expr || (!expr_is_op(expr, &ops_log) && !expr_is_op(expr, &ops_log10)))
        return NULL;
    const expr_t *trig = expr->a;

    if (expr_is_op(trig, &ops_abs))
        trig = trig->a;
    *cosine = expr_is_op(trig, &ops_cos);
    return (*cosine || expr_is_op(trig, &ops_sin)) && !expr_integrate_contains_imaginary_unit(trig->a)
               ? trig->a : NULL;
}

static expr_t *log_trigonometric_clausen_phase(const expr_t *argument, bool cosine)
{
    expr_t *twice = expr_mul_long(argument, 2);

    if (cosine)
        return expr_add_simplify_owned(expr_new_named_const(NUM_PI, "@pi"), twice);
    return twice;
}

/* Expand real logarithmic identities for verification without changing the preferred display form. */
static expr_t *expand_real_logarithmic_identities(const expr_t *expr)
{
    if (!expr)
        return NULL;
    bool cosine = false;
    const expr_t *argument = log_trigonometric_argument(expr, &cosine);

    if (argument) {
        expr_t *phase = log_trigonometric_clausen_phase(argument, cosine);
        expr_t *first = phase ? expr_clausen(1u, phase) : NULL;
        expr_t *two = expr_new_const(NUM_TWO);
        expr_t *log_two = two ? expr_log(two) : NULL;
        expr_t *negative = first ? expr_neg(first) : NULL;
        expr_t *out = negative && log_two ? simplify_owned(expr_sub(negative, log_two)) : NULL;

        if (out && expr_is_op(expr, &ops_log10)) {
            expr_t *scaled = expr_div_num(out, &NUM_LN10);

            expr_free(out);
            out = simplify_owned(scaled);
        }
        expr_free(negative);
        expr_free(log_two);
        expr_free(first);
        expr_free(two);
        expr_free(phase);
        return out;
    }
    if ((expr_is_op(expr, &ops_log) || expr_is_op(expr, &ops_log10)) &&
        expr_is_op(expr->a, &ops_cosh) && expr->a->a &&
        !expr_integrate_contains_imaginary_unit(expr->a->a)) {
        const expr_t *u = expr->a->a;
        expr_t *exponent = expr_mul_long(u, -2);
        expr_t *exponential = exponent ? expr_exp(exponent) : NULL;
        expr_t *sum = exponential ? expr_add_simplify_owned(exponential, expr_const_one()) : NULL;
        expr_t *log_sum = sum ? expr_log(sum) : NULL;
        expr_t *two = expr_new_const(NUM_TWO);
        expr_t *log_two = two ? expr_log(two) : NULL;
        expr_t *difference = log_sum && log_two ? expr_sub(log_sum, log_two) : NULL;
        expr_t *out = difference ? expr_add_simplify_owned(difference, expr_clone(u)) : NULL;

        if (out && expr_is_op(expr, &ops_log10)) {
            expr_t *scaled = expr_div_num(out, &NUM_LN10);

            expr_free(out);
            out = simplify_owned(scaled);
        }
        expr_free(log_two);
        expr_free(two);
        expr_free(log_sum);
        expr_free(sum);
        expr_free(exponent);
        return out;
    }
    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub) ||
        expr_is_op(expr, &ops_mul) || expr_is_op(expr, &ops_div)) {
        expr_t *left = expand_real_logarithmic_identities(expr->a);
        expr_t *right = expand_real_logarithmic_identities(expr->b);
        expr_t *out = left && right ? expr->ops->apply_binary(left, right) : NULL;

        expr_free(right);
        expr_free(left);
        return simplify_owned(out);
    }
    if (expr_is_op(expr, &ops_neg))
        return expr_negate_owned(expand_real_logarithmic_identities(expr->a));
    return expr_clone(expr);
}

static bool antiderivative_terms_equal(const expr_t *actual, const expr_t *expected)
{
    expr_t *difference = actual && expected ? simplify_owned(expr_sub(actual, expected)) : NULL;
    bool equal = difference && expr_is_exact_zero(difference);

    if (!equal && difference) {
        expr_t *expanded = simplify_owned(expr_display_expanded(difference));

        equal = expanded && expr_is_exact_zero(expanded);
        if (!equal && expanded && expr_is_addsub(expanded)) {
            expr_t *opposite = expr_is_op(expanded, &ops_sub) ? expr_clone(expanded->b) : expr_neg(expanded->b);

            equal = opposite && expr_equal_exact_local(expanded->a, opposite);
            expr_free(opposite);
        }
        expr_free(expanded);
    }
    expr_free(difference);
    return equal;
}

/* Verify native antiderivatives without requiring callers to know special-function identities. */
bool expr_verify_antiderivative_real_internal(const expr_t *primitive, const expr_t *integrand, const expr_t *wrt)
{
    if (!primitive || !integrand || !wrt)
        return false;
    expr_t *derivative = expr_create_deriv(primitive, wrt);
    bool verified = derivative && antiderivative_terms_equal(derivative, integrand);

    if (!verified && derivative) {
        expr_t *actual = expand_real_logarithmic_identities(derivative);
        expr_t *expected = expand_real_logarithmic_identities(integrand);

        verified = actual && expected && antiderivative_terms_equal(actual, expected);
        expr_free(expected);
        expr_free(actual);
    }
    expr_free(derivative);
    return verified;
}

expr_t *integrate_log_over_proportional_affine(const expr_t *expr, const expr_t *wrt)
{
    number_t log_constant = num_new();
    number_t log_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        !match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_LOG, &log_constant, &log_coeff) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &denom_constant, &denom_coeff) || num_is_zero(log_coeff) ||
        num_is_zero(denom_coeff)) {
        num_destroy(&denom_coeff);
        num_destroy(&denom_constant);
        num_destroy(&log_coeff);
        num_destroy(&log_constant);
        return NULL;
    }

    number_t scale = num_div(denom_coeff, log_coeff);
    number_t scaled_log_constant = num_mul(scale, log_constant);
    if (num_eq(denom_constant, scaled_log_constant)) {
        expr_t *log_term = expr_log(expr->a->a);
        expr_t *log_sq = log_term ? expr_mul(log_term, log_term) : NULL;
        number_t denom = num_mul(denom_coeff, NUM_TWO);

        out = div_number_owned_consuming(log_sq, &denom);
        expr_free(log_term);
    }

    num_destroy(&scaled_log_constant);
    num_destroy(&scale);
    num_destroy(&denom_coeff);
    num_destroy(&denom_constant);
    num_destroy(&log_coeff);
    num_destroy(&log_constant);
    return out;
}

/* For real u, integrate ln(cosh(u)) using Li2(-exp(-2u)); the additive constant is arbitrary. */
static expr_t *integrate_log_cosh_affine(const expr_t *expr, const expr_t *wrt)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    expr_t *out = NULL;

    if (!expr || (!expr_is_op(expr, &ops_log) && !expr_is_op(expr, &ops_log10)) ||
        !expr_is_op(expr->a, &ops_cosh) || !expr->a->a ||
        !match_symbolic_affine_constant_and_coeff(expr->a->a, wrt, &constant, &coeff) ||
        expr_const_is_zero(coeff) || expr_integrate_contains_imaginary_unit(expr->a->a))
        goto cleanup;

    const expr_t *u = expr->a->a;
    expr_t *square = expr_mul(u, u);
    expr_t *half_square = square ? expr_div_long(square, 2) : NULL;
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *log_two = two ? expr_log(two) : NULL;
    expr_t *linear = log_two ? expr_mul(u, log_two) : NULL;
    expr_t *polynomial = half_square && linear ? expr_sub(half_square, linear) : NULL;
    expr_t *exponent = expr_mul_long(u, -2);
    expr_t *exponential = exponent ? expr_exp(exponent) : NULL;
    expr_t *argument = exponential ? expr_neg(exponential) : NULL;
    expr_t *dilog = argument ? expr_dilog(argument) : NULL;
    expr_t *half_dilog = dilog ? expr_div_long(dilog, 2) : NULL;
    expr_t *sum = polynomial && half_dilog ? expr_add(polynomial, half_dilog) : NULL;

    out = sum ? simplify_owned(expr_div(sum, coeff)) : NULL;
    if (out && expr_is_op(expr, &ops_log10)) {
        expr_t *scaled = expr_div_num(out, &NUM_LN10);

        expr_free(out);
        out = simplify_owned(scaled);
    }
    expr_free(sum);
    expr_free(half_dilog);
    expr_free(dilog);
    expr_free(argument);
    expr_free(exponential);
    expr_free(exponent);
    expr_free(polynomial);
    expr_free(linear);
    expr_free(log_two);
    expr_free(two);
    expr_free(half_square);
    expr_free(square);

cleanup:
    expr_free(coeff);
    expr_free(constant);
    return out;
}

static expr_t *integrate_log_trigonometric_affine(const expr_t *expr, const expr_t *wrt)
{
    bool cosine = false;
    const expr_t *argument = log_trigonometric_argument(expr, &cosine);
    expr_t *constant = NULL;
    expr_t *coefficient = NULL;
    expr_t *out = NULL;

    if (!argument || !match_symbolic_affine_constant_and_coeff(argument, wrt, &constant, &coefficient) ||
        expr_const_is_zero(coefficient))
        goto cleanup;
    expr_t *phase = log_trigonometric_clausen_phase(argument, cosine);
    expr_t *clausen = phase ? expr_clausen2(phase) : NULL;
    expr_t *half_clausen = clausen ? expr_div_long(clausen, -2) : NULL;
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *log_two = two ? expr_log(two) : NULL;
    expr_t *linear = log_two ? expr_mul(argument, log_two) : NULL;
    expr_t *numerator = half_clausen && linear ? expr_sub(half_clausen, linear) : NULL;

    out = numerator ? simplify_owned(expr_div(numerator, coefficient)) : NULL;
    if (out && expr_is_op(expr, &ops_log10)) {
        expr_t *scaled = expr_div_num(out, &NUM_LN10);

        expr_free(out);
        out = simplify_owned(scaled);
    }
    expr_free(numerator);
    expr_free(linear);
    expr_free(log_two);
    expr_free(two);
    expr_free(half_clausen);
    expr_free(clausen);
    expr_free(phase);

cleanup:
    expr_free(coefficient);
    expr_free(constant);
    return out;
}

expr_t *integrate_log_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *x_log_x;
    expr_t *raw;

    number_t constant = num_new();
    number_t coeff = num_new();

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_LOG, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        raw = integrate_log_trigonometric_affine(expr, wrt);
        if (raw)
            return raw;
        raw = integrate_log_cosh_affine(expr, wrt);
        if (raw)
            return raw;
        raw = integrate_log_of_symbolic_affine(expr, wrt);
        if (raw)
            return raw;
        raw = integrate_log_of_symbolic_quadratic(expr, wrt);
        if (raw)
            return raw;
        return integrate_poly_times_rational_unary_by_parts(expr, wrt);
    }

    x_log_x = expr_mul(expr->a, expr);
    raw = x_log_x ? expr_sub(x_log_x, expr->a) : NULL;
    expr_free(x_log_x);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

/* Integrate a base-ten logarithm with an affine argument. */
expr_t *integrate_log10_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *symbolic_constant = NULL;
    expr_t *symbolic_coeff = NULL;
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_log10_u;
    expr_t *u_over_ln10;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_LOG10, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        raw = integrate_log_trigonometric_affine(expr, wrt);
        if (raw)
            return raw;
        raw = integrate_log_cosh_affine(expr, wrt);
        if (raw)
            return raw;
        if (!expr_integrate_contains_imaginary_unit(expr) || !expr || !expr->a ||
            !match_symbolic_affine_constant_and_coeff(expr->a, wrt, &symbolic_constant, &symbolic_coeff) ||
            expr_const_is_zero(symbolic_coeff)) {
            expr_free(symbolic_coeff);
            expr_free(symbolic_constant);
            return NULL;
        }
        u_log10_u = expr_mul(expr->a, expr);
        u_over_ln10 = expr->a ? expr_div_num(expr->a, &NUM_LN10) : NULL;
        raw = u_log10_u && u_over_ln10 ? expr_sub(u_log10_u, u_over_ln10) : NULL;
        {
            expr_t *out = raw && symbolic_coeff ? expr_div(raw, symbolic_coeff) : NULL;

            out = simplify_owned(out);
            expr_free(raw);
            expr_free(u_over_ln10);
            expr_free(u_log10_u);
            expr_free(symbolic_coeff);
            expr_free(symbolic_constant);
            return out;
        }
    }

    u_log10_u = expr_mul(expr->a, expr);
    u_over_ln10 = expr->a ? expr_div_num(expr->a, &NUM_LN10) : NULL;
    raw = (u_log10_u && u_over_ln10) ? expr_sub(u_log10_u, u_over_ln10) : NULL;

    expr_free(u_over_ln10);
    expr_free(u_log10_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

static bool match_wrt_times_log_expr(const expr_t *expr, const expr_t *wrt, const expr_t **log_expr_out)
{
    if (!expr || !wrt || !log_expr_out || !expr->a || !expr->b)
        return false;

    if (is_wrt_symbolic_affine_leaf(expr->a, wrt) && expr_is_op(expr->b, &ops_log) && expr->b->a) {
        *log_expr_out = expr->b;
        return true;
    }
    if (is_wrt_symbolic_affine_leaf(expr->b, wrt) && expr_is_op(expr->a, &ops_log) && expr->a->a) {
        *log_expr_out = expr->a;
        return true;
    }
    return false;
}

expr_t *integrate_log_of_symbolic_affine(const expr_t *expr, const expr_t *wrt)
{
    expr_t *constant_term = NULL;
    expr_t *coeff = NULL;
    expr_t *log_base = NULL;
    expr_t *base_log = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr_is_op(expr, &ops_log) || !expr->a ||
        !match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant_term, &coeff) || expr_is_exact_zero(coeff))
        goto cleanup;

    log_base = expr_log(expr->a);
    base_log = log_base ? expr_mul(expr->a, log_base) : NULL;
    raw = base_log ? expr_sub(base_log, expr->a) : NULL;
    out = (raw && coeff) ? expr_div(raw, coeff) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(raw);
    expr_free(base_log);
    expr_free(log_base);
    expr_free(coeff);
    expr_free(constant_term);
    return out;
}

expr_t *integrate_log_over_symbolic_proportional_affine(const expr_t *expr, const expr_t *wrt)
{
    expr_t *log_constant = NULL;
    expr_t *log_coeff = NULL;
    expr_t *denom_constant = NULL;
    expr_t *denom_coeff = NULL;
    expr_t *log_term = NULL;
    expr_t *log_sq = NULL;
    expr_t *denom = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr->b || !expr_is_op(expr->a, &ops_log) || !expr->a->a ||
        !match_symbolic_affine_constant_and_coeff(expr->a->a, wrt, &log_constant, &log_coeff) ||
        !match_symbolic_affine_constant_and_coeff(expr->b, wrt, &denom_constant, &denom_coeff) ||
        !expr_is_exact_zero(log_constant) || !expr_is_exact_zero(denom_constant) || expr_is_exact_zero(denom_coeff))
        goto cleanup;

    log_term = expr_log(expr->a->a);
    log_sq = log_term ? expr_mul(log_term, log_term) : NULL;
    denom = denom_coeff ? expr_mul_num(denom_coeff, &NUM_TWO) : NULL;
    out = (log_sq && denom) ? expr_div(log_sq, denom) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(denom);
    expr_free(log_sq);
    expr_free(log_term);
    expr_free(denom_coeff);
    expr_free(denom_constant);
    expr_free(log_coeff);
    expr_free(log_constant);
    return out;
}

static expr_t *build_symbolic_quadratic_reciprocal_integral(const expr_t *wrt, const expr_t *quad_coeff,
                                                            const expr_t *linear_coeff, const expr_t *constant_coeff)
{
    expr_t *a_c = NULL;
    expr_t *four_ac = NULL;
    expr_t *linear_sq = NULL;
    expr_t *delta = NULL;
    expr_t *sqrt_delta = NULL;
    expr_t *a_x = NULL;
    expr_t *two_a_x = NULL;
    expr_t *linear = NULL;
    expr_t *arg = NULL;
    expr_t *atan_arg = NULL;
    expr_t *two_atan = NULL;
    expr_t *out = NULL;
    number_t four = num_create_from_long(4);

    if (!wrt || !quad_coeff || !linear_coeff || !constant_coeff)
        goto cleanup;

    a_c = expr_mul(quad_coeff, constant_coeff);
    four_ac = a_c ? expr_mul_num(a_c, &four) : NULL;
    linear_sq = expr_mul(linear_coeff, linear_coeff);
    delta = (four_ac && linear_sq) ? expr_sub(four_ac, linear_sq) : NULL;
    delta = simplify_owned(delta);
    sqrt_delta = delta ? expr_sqrt(delta) : NULL;
    a_x = expr_mul(quad_coeff, wrt);
    two_a_x = a_x ? expr_mul_num(a_x, &NUM_TWO) : NULL;
    linear = two_a_x ? expr_add(two_a_x, linear_coeff) : NULL;
    arg = (linear && sqrt_delta) ? expr_div(linear, sqrt_delta) : NULL;
    atan_arg = arg ? expr_atan(arg) : NULL;
    two_atan = atan_arg ? expr_mul_num(atan_arg, &NUM_TWO) : NULL;
    out = (two_atan && sqrt_delta) ? expr_div(two_atan, sqrt_delta) : NULL;
    out = simplify_owned(out);

cleanup:
    num_destroy(&four);
    expr_free(two_atan);
    expr_free(atan_arg);
    expr_free(arg);
    expr_free(linear);
    expr_free(two_a_x);
    expr_free(a_x);
    expr_free(sqrt_delta);
    expr_free(delta);
    expr_free(linear_sq);
    expr_free(four_ac);
    expr_free(a_c);
    return out;
}

static expr_t *build_linear_over_symbolic_quadratic_integral(const expr_t *quadratic, const expr_t *wrt,
                                                             const expr_t *quad_coeff, const expr_t *linear_coeff,
                                                             const expr_t *constant_coeff, const expr_t *numer_linear,
                                                             const expr_t *numer_constant)
{
    expr_t *two_a = NULL;
    expr_t *alpha = NULL;
    expr_t *log_q = NULL;
    expr_t *log_part = NULL;
    expr_t *alpha_b = NULL;
    expr_t *remainder = NULL;
    expr_t *inverse_integral = NULL;
    expr_t *remainder_part = NULL;
    expr_t *out = NULL;

    if (!quadratic || !wrt || !quad_coeff || !linear_coeff || !constant_coeff || !numer_linear || !numer_constant)
        goto cleanup;

    two_a = expr_mul_num(quad_coeff, &NUM_TWO);
    alpha = two_a ? expr_div(numer_linear, two_a) : NULL;
    alpha = simplify_owned(alpha);
    if (alpha && !expr_is_exact_zero(alpha)) {
        log_q = expr_log(quadratic);
        log_part = log_q ? expr_mul(alpha, log_q) : NULL;
    }

    alpha_b = (alpha && linear_coeff) ? expr_mul(alpha, linear_coeff) : NULL;
    remainder = (numer_constant && alpha_b) ? expr_sub(numer_constant, alpha_b) : NULL;
    remainder = simplify_owned(remainder);
    if (remainder && !expr_is_exact_zero(remainder)) {
        inverse_integral = build_symbolic_quadratic_reciprocal_integral(wrt, quad_coeff, linear_coeff, constant_coeff);
        remainder_part = inverse_integral ? expr_mul(remainder, inverse_integral) : NULL;
    }

    out = simplify_owned(expr_add_owned(log_part, remainder_part));
    log_part = NULL;
    remainder_part = NULL;

cleanup:
    expr_free(remainder_part);
    expr_free(inverse_integral);
    expr_free(remainder);
    expr_free(alpha_b);
    expr_free(log_part);
    expr_free(log_q);
    expr_free(alpha);
    expr_free(two_a);
    return out;
}

expr_t *integrate_wrt_times_log_symbolic_affine(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *log_expr = NULL;
    expr_t *constant_term = NULL;
    expr_t *coeff = NULL;
    expr_t *log_u = NULL;
    expr_t *x_sq = NULL;
    expr_t *x_sq_log = NULL;
    expr_t *first = NULL;
    expr_t *x_sq_for_quarter = NULL;
    expr_t *second = NULL;
    expr_t *two_coeff = NULL;
    expr_t *constant_over_two_coeff = NULL;
    expr_t *third = NULL;
    expr_t *constant_sq = NULL;
    expr_t *coeff_sq = NULL;
    expr_t *two_coeff_sq = NULL;
    expr_t *constant_sq_over_two_coeff_sq = NULL;
    expr_t *scaled_log = NULL;
    expr_t *fourth = NULL;
    expr_t *sum = NULL;
    number_t quarter = num_mul(NUM_HALF, NUM_HALF);
    number_t neg_quarter = num_neg(quarter);

    if (!match_wrt_times_log_expr(expr, wrt, &log_expr) ||
        !match_symbolic_affine_constant_and_coeff(log_expr->a, wrt, &constant_term, &coeff) ||
        expr_is_exact_zero(coeff))
        goto cleanup;

    log_u = expr_log(log_expr->a);
    x_sq = expr_mul(wrt, wrt);
    x_sq_log = (x_sq && log_u) ? expr_mul(x_sq, log_u) : NULL;
    first = x_sq_log ? mul_number_owned(x_sq_log, NUM_HALF) : NULL;
    x_sq_log = NULL;

    x_sq_for_quarter = expr_mul(wrt, wrt);
    second = x_sq_for_quarter ? mul_number_owned(x_sq_for_quarter, neg_quarter) : NULL;
    x_sq_for_quarter = NULL;

    two_coeff = coeff ? expr_mul_num(coeff, &NUM_TWO) : NULL;
    constant_over_two_coeff = (constant_term && two_coeff) ? expr_div(constant_term, two_coeff) : NULL;
    third = constant_over_two_coeff ? expr_mul(wrt, constant_over_two_coeff) : NULL;

    constant_sq = expr_mul(constant_term, constant_term);
    coeff_sq = expr_mul(coeff, coeff);
    two_coeff_sq = coeff_sq ? expr_mul_num(coeff_sq, &NUM_TWO) : NULL;
    constant_sq_over_two_coeff_sq = (constant_sq && two_coeff_sq) ? expr_div(constant_sq, two_coeff_sq) : NULL;
    scaled_log = (constant_sq_over_two_coeff_sq && log_u) ? expr_mul(constant_sq_over_two_coeff_sq, log_u) : NULL;
    fourth = expr_negate_owned(scaled_log);
    scaled_log = NULL;

    sum = expr_add_owned(first, second);
    first = NULL;
    second = NULL;
    sum = expr_add_owned(sum, third);
    third = NULL;
    sum = expr_add_owned(sum, fourth);
    fourth = NULL;
    sum = simplify_owned(sum);

cleanup:
    num_destroy(&neg_quarter);
    num_destroy(&quarter);
    expr_free(fourth);
    expr_free(scaled_log);
    expr_free(constant_sq_over_two_coeff_sq);
    expr_free(two_coeff_sq);
    expr_free(coeff_sq);
    expr_free(constant_sq);
    expr_free(third);
    expr_free(constant_over_two_coeff);
    expr_free(two_coeff);
    expr_free(second);
    expr_free(x_sq_for_quarter);
    expr_free(first);
    expr_free(x_sq_log);
    expr_free(x_sq);
    expr_free(log_u);
    expr_free(coeff);
    expr_free(constant_term);
    return sum;
}

expr_t *integrate_wrt_times_log_symbolic_quadratic(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *log_expr = NULL;
    expr_t *quad_coeff = NULL;
    expr_t *linear_coeff = NULL;
    expr_t *constant_coeff = NULL;
    expr_t *log_q = NULL;
    expr_t *x_sq = NULL;
    expr_t *x_sq_log = NULL;
    expr_t *first = NULL;
    expr_t *x_sq_for_half = NULL;
    expr_t *second = NULL;
    expr_t *two_a = NULL;
    expr_t *linear_over_two_a = NULL;
    expr_t *third = NULL;
    expr_t *linear_sq = NULL;
    expr_t *linear_sq_over_a = NULL;
    expr_t *two_c = NULL;
    expr_t *numer_linear = NULL;
    expr_t *linear_c = NULL;
    expr_t *numer_constant = NULL;
    expr_t *remainder_integral = NULL;
    expr_t *fourth = NULL;
    expr_t *sum = NULL;
    number_t neg_half = num_neg(NUM_HALF);

    if (!match_wrt_times_log_expr(expr, wrt, &log_expr) ||
        !match_symbolic_quadratic_coeffs(log_expr->a, wrt, &quad_coeff, &linear_coeff, &constant_coeff))
        goto cleanup;

    log_q = expr_log(log_expr->a);
    x_sq = expr_mul(wrt, wrt);
    x_sq_log = (x_sq && log_q) ? expr_mul(x_sq, log_q) : NULL;
    first = x_sq_log ? mul_number_owned(x_sq_log, NUM_HALF) : NULL;
    x_sq_log = NULL;

    x_sq_for_half = expr_mul(wrt, wrt);
    second = x_sq_for_half ? mul_number_owned(x_sq_for_half, neg_half) : NULL;
    x_sq_for_half = NULL;

    two_a = quad_coeff ? expr_mul_num(quad_coeff, &NUM_TWO) : NULL;
    linear_over_two_a = (linear_coeff && two_a) ? expr_div(linear_coeff, two_a) : NULL;
    third = linear_over_two_a ? expr_mul(wrt, linear_over_two_a) : NULL;

    linear_sq = expr_mul(linear_coeff, linear_coeff);
    linear_sq_over_a = (linear_sq && quad_coeff) ? expr_div(linear_sq, quad_coeff) : NULL;
    two_c = constant_coeff ? expr_mul_num(constant_coeff, &NUM_TWO) : NULL;
    numer_linear = (linear_sq_over_a && two_c) ? expr_sub(linear_sq_over_a, two_c) : NULL;
    numer_linear = simplify_owned(numer_linear);
    linear_c = expr_mul(linear_coeff, constant_coeff);
    numer_constant = (linear_c && quad_coeff) ? expr_div(linear_c, quad_coeff) : NULL;
    numer_constant = simplify_owned(numer_constant);
    remainder_integral = build_linear_over_symbolic_quadratic_integral(log_expr->a, wrt, quad_coeff, linear_coeff,
                                                                       constant_coeff, numer_linear, numer_constant);
    fourth = remainder_integral ? mul_number_owned(remainder_integral, neg_half) : NULL;
    remainder_integral = NULL;

    sum = expr_add_owned(first, second);
    first = NULL;
    second = NULL;
    sum = expr_add_owned(sum, third);
    third = NULL;
    sum = expr_add_owned(sum, fourth);
    fourth = NULL;
    sum = simplify_owned(sum);

cleanup:
    num_destroy(&neg_half);
    expr_free(fourth);
    expr_free(remainder_integral);
    expr_free(numer_constant);
    expr_free(linear_c);
    expr_free(numer_linear);
    expr_free(two_c);
    expr_free(linear_sq_over_a);
    expr_free(linear_sq);
    expr_free(third);
    expr_free(linear_over_two_a);
    expr_free(two_a);
    expr_free(second);
    expr_free(x_sq_for_half);
    expr_free(first);
    expr_free(x_sq_log);
    expr_free(x_sq);
    expr_free(log_q);
    expr_free(constant_coeff);
    expr_free(linear_coeff);
    expr_free(quad_coeff);
    return sum;
}
