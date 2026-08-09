#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

static bool match_secant_positive_integer_power(const expr_t *expr, const expr_t **argument_out,
                                                unsigned int *power_out)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    bool matched = false;

    if (!expr || !argument_out || !power_out)
        goto cleanup;
    if (expr_is_op(expr, &ops_sec) && expr->a) {
        *argument_out = expr->a;
        *power_out = 1u;
        matched = true;
        goto cleanup;
    }
    if (expr_match_pow_const(expr, &base, &exponent) && base && expr_is_op(base, &ops_sec) && base->a &&
        expr_integrate_number_matches_uint_at_most(exponent, 64u, power_out) && *power_out > 0u) {
        *argument_out = base->a;
        matched = true;
    }

cleanup:
    num_destroy(&exponent);
    return matched;
}

static bool match_sine_or_cosine_times_secant_power(const expr_t *expr, const expr_t **trig_argument_out,
                                                    bool *is_sine_out, unsigned int *secant_power_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *trig_argument = NULL;
    const expr_t *secant_argument = NULL;
    bool is_sine;

    if (!expr || !trig_argument_out || !is_sine_out || !secant_power_out || !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_is_op(left, &ops_sin) && left->a) {
        trig_argument = left->a;
        is_sine = true;
    } else if (expr_is_op(left, &ops_cos) && left->a) {
        trig_argument = left->a;
        is_sine = false;
    } else {
        trig_argument = NULL;
    }
    if (trig_argument && match_secant_positive_integer_power(right, &secant_argument, secant_power_out) &&
        expr_struct_eq(trig_argument, secant_argument)) {
        *trig_argument_out = trig_argument;
        *is_sine_out = is_sine;
        return true;
    }

    if (expr_is_op(right, &ops_sin) && right->a) {
        trig_argument = right->a;
        is_sine = true;
    } else if (expr_is_op(right, &ops_cos) && right->a) {
        trig_argument = right->a;
        is_sine = false;
    } else {
        return false;
    }
    if (!match_secant_positive_integer_power(left, &secant_argument, secant_power_out) ||
        !expr_struct_eq(trig_argument, secant_argument))
        return false;

    *trig_argument_out = trig_argument;
    *is_sine_out = is_sine;
    return true;
}

expr_t *integrate_sine_cosine_times_secant_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *argument = NULL;
    expr_t *affine_constant = NULL;
    expr_t *affine_coefficient = NULL;
    expr_t *secant = NULL;
    expr_t *reduced_power = NULL;
    expr_t *denominator = NULL;
    expr_t *integral = NULL;
    unsigned int power = 0u;
    bool is_sine = false;

    if (!match_sine_or_cosine_times_secant_power(expr, &argument, &is_sine, &power) ||
        !match_symbolic_affine_constant_and_coeff(argument, wrt, &affine_constant, &affine_coefficient) ||
        expr_const_is_zero(affine_coefficient))
        goto cleanup;

    secant = expr_sec(argument);
    reduced_power = power == 1u ? expr_const_one() : expr_integrate_build_unsigned_expr_power(secant, power - 1u);
    if (!reduced_power)
        goto cleanup;

    if (!is_sine) {
        integral = expr_integrate_dispatch(reduced_power, wrt);
        goto cleanup;
    }
    if (power == 1u)
        goto cleanup;

    denominator = expr_mul_long(affine_coefficient, (long)(power - 1u));
    integral = denominator ? expr_div(reduced_power, denominator) : NULL;
    integral = simplify_owned(integral);

cleanup:
    expr_free(denominator);
    expr_free(reduced_power);
    expr_free(secant);
    expr_free(affine_coefficient);
    expr_free(affine_constant);
    return integral;
}

bool match_trig_proportional_wrt_coeff(const expr_t *expr, const expr_t *wrt, bool *is_sin_out, expr_t **coeff_out)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !is_sin_out || !coeff_out || !expr->a)
        return false;

    if (expr_is_op(expr, &ops_sin)) {
        *is_sin_out = true;
    } else if (expr_is_op(expr, &ops_cos)) {
        *is_sin_out = false;
    } else {
        return false;
    }

    if (!match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant, &coeff))
        goto cleanup;
    if (!expr_const_is_zero(constant) || expr_const_is_zero(coeff))
        goto cleanup;

    *coeff_out = expr_clone(coeff);
    ok = *coeff_out != NULL;

cleanup:
    expr_free(coeff);
    expr_free(constant);
    return ok;
}

static bool match_power_trig_product(const expr_t *expr, const expr_t *wrt, const expr_t **power_out,
                                     const expr_t **trig_out, bool *is_sin_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !power_out || !trig_out || !is_sin_out || !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_integrate_match_wrt_power_factor_exponent(left, wrt, &exponent) &&
        match_trig_proportional_wrt_coeff(right, wrt, is_sin_out, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = left;
        *trig_out = right;
        return true;
    }
    expr_free(coeff);
    expr_free(exponent);
    coeff = NULL;
    exponent = NULL;

    if (expr_integrate_match_wrt_power_factor_exponent(right, wrt, &exponent) &&
        match_trig_proportional_wrt_coeff(left, wrt, is_sin_out, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = right;
        *trig_out = left;
        return true;
    }

    expr_free(coeff);
    expr_free(exponent);
    return false;
}

static expr_t *divide_expr_by_expr_power_owned(expr_t *numer, const expr_t *base, unsigned int power)
{
    expr_t *denom = NULL;
    expr_t *out = NULL;

    if (!numer || !base || power == 0u) {
        expr_free(numer);
        return NULL;
    }

    denom = expr_integrate_build_unsigned_expr_power(base, power);
    out = denom ? expr_div(numer, denom) : NULL;
    expr_free(denom);
    expr_free(numer);
    return out;
}

static expr_t *build_symbolic_quadratic_power_trig_integral(bool integrand_is_sin, const expr_t *trig_expr,
                                                            const expr_t *coeff, const expr_t *wrt)
{
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *x_sq = NULL;
    expr_t *term1 = NULL;
    expr_t *x_trig = NULL;
    expr_t *term2 = NULL;
    expr_t *tail_trig = NULL;
    expr_t *term3 = NULL;
    expr_t *sum = NULL;

    if (!trig_expr || !trig_expr->a || !coeff || !wrt)
        return NULL;

    sin_v = expr_sin(trig_expr->a);
    cos_v = expr_cos(trig_expr->a);
    x_sq = expr_integrate_build_unsigned_expr_power(wrt, 2u);
    x_trig = (x_sq && (integrand_is_sin ? cos_v : sin_v)) ? expr_mul(x_sq, integrand_is_sin ? cos_v : sin_v) : NULL;
    term1 = divide_expr_by_expr_power_owned(x_trig, coeff, 1u);
    x_trig = NULL;
    if (integrand_is_sin)
        term1 = expr_negate_owned(term1);

    x_trig = (wrt && (integrand_is_sin ? sin_v : cos_v)) ? expr_mul(wrt, integrand_is_sin ? sin_v : cos_v) : NULL;
    if (x_trig) {
        term2 = expr_mul_num(x_trig, &NUM_TWO);
        expr_free(x_trig);
        x_trig = NULL;
    }
    term2 = divide_expr_by_expr_power_owned(term2, coeff, 2u);

    tail_trig = integrand_is_sin ? expr_retain_expr(cos_v) : expr_retain_expr(sin_v);
    if (tail_trig) {
        term3 = expr_mul_num(tail_trig, &NUM_TWO);
        expr_free(tail_trig);
        tail_trig = NULL;
    }
    term3 = divide_expr_by_expr_power_owned(term3, coeff, 3u);
    if (!integrand_is_sin)
        term3 = expr_negate_owned(term3);

    sum = expr_add_owned(term1, term2);
    term1 = NULL;
    term2 = NULL;
    sum = expr_add_owned(sum, term3);
    term3 = NULL;

    expr_free(term3);
    expr_free(tail_trig);
    expr_free(term2);
    expr_free(x_trig);
    expr_free(term1);
    expr_free(x_sq);
    expr_free(cos_v);
    expr_free(sin_v);
    return simplify_owned(sum);
}

static expr_t *build_symbolic_linear_power_trig_integral(bool integrand_is_sin, const expr_t *trig_expr,
                                                         const expr_t *coeff, const expr_t *wrt)
{
    expr_t *first_trig = NULL;
    expr_t *x_trig = NULL;
    expr_t *first = NULL;
    expr_t *second_trig = NULL;
    expr_t *second = NULL;
    expr_t *sum = NULL;

    if (!trig_expr || !trig_expr->a || !coeff || !wrt)
        return NULL;

    first_trig = integrand_is_sin ? expr_cos(trig_expr->a) : expr_sin(trig_expr->a);
    x_trig = (wrt && first_trig) ? expr_mul(wrt, first_trig) : NULL;
    first = divide_expr_by_expr_power_owned(x_trig, coeff, 1u);
    x_trig = NULL;
    if (integrand_is_sin)
        first = expr_negate_owned(first);

    second_trig = integrand_is_sin ? expr_sin(trig_expr->a) : expr_cos(trig_expr->a);
    second = divide_expr_by_expr_power_owned(second_trig ? expr_retain_expr(second_trig) : NULL, coeff, 2u);

    sum = expr_add_owned(first, second);
    first = NULL;
    second = NULL;

    expr_free(second);
    expr_free(second_trig);
    expr_free(first);
    expr_free(x_trig);
    expr_free(first_trig);
    return simplify_owned(sum);
}

static expr_t *build_symbolic_integer_power_trig_integral(unsigned int degree, bool integrand_is_sin,
                                                          const expr_t *trig_expr, const expr_t *coeff,
                                                          const expr_t *wrt)
{
    expr_t *x_power = NULL;
    expr_t *trig_part = NULL;
    expr_t *product = NULL;
    expr_t *tmp_coeff = NULL;
    expr_t *term1 = NULL;
    expr_t *inner = NULL;
    expr_t *scaled_inner = NULL;
    expr_t *term2 = NULL;
    expr_t *sum = NULL;

    if (!trig_expr || !trig_expr->a || !coeff || !wrt)
        return NULL;

    if (degree == 1u) {
        expr_t *linear = build_symbolic_linear_power_trig_integral(integrand_is_sin, trig_expr, coeff, wrt);

        if (linear)
            return linear;
    }

    if (degree == 2u) {
        expr_t *quadratic = build_symbolic_quadratic_power_trig_integral(integrand_is_sin, trig_expr, coeff, wrt);

        if (quadratic)
            return quadratic;
    }

    x_power = expr_integrate_build_unsigned_expr_power(wrt, degree);
    trig_part = integrand_is_sin ? expr_cos(trig_expr->a) : expr_sin(trig_expr->a);
    product = (x_power && trig_part) ? expr_mul(x_power, trig_part) : NULL;
    if (product && coeff->ops == &ops_const) {
        term1 = div_number_owned(expr_retain_expr(product), coeff->c);
    } else {
        tmp_coeff = expr_clone(coeff);
        term1 = (product && tmp_coeff) ? expr_div(product, tmp_coeff) : NULL;
        expr_free(tmp_coeff);
        tmp_coeff = NULL;
    }
    if (integrand_is_sin)
        term1 = expr_negate_owned(term1);
    term1 = simplify_owned(term1);

    if (degree == 0u) {
        expr_free(product);
        expr_free(trig_part);
        expr_free(x_power);
        return term1;
    }

    inner = build_symbolic_integer_power_trig_integral(degree - 1u, !integrand_is_sin, trig_expr, coeff, wrt);
    if (inner) {
        number_t scale = num_create_from_long((long)degree);

        scaled_inner = expr_mul_num(inner, &scale);
        num_destroy(&scale);
    }
    if (scaled_inner && coeff->ops == &ops_const) {
        term2 = div_number_owned(expr_retain_expr(scaled_inner), coeff->c);
    } else {
        tmp_coeff = expr_clone(coeff);
        term2 = (scaled_inner && tmp_coeff) ? expr_div(scaled_inner, tmp_coeff) : NULL;
        expr_free(tmp_coeff);
        tmp_coeff = NULL;
    }
    if (!integrand_is_sin)
        term2 = expr_negate_owned(term2);
    term2 = simplify_owned(term2);
    sum = expr_add_owned(term1, term2);
    term1 = NULL;
    term2 = NULL;

    expr_free(tmp_coeff);
    expr_free(term2);
    expr_free(scaled_inner);
    expr_free(inner);
    expr_free(term1);
    expr_free(product);
    expr_free(trig_part);
    expr_free(x_power);
    return simplify_owned(sum);
}

expr_t *integrate_symbolic_integer_power_times_trig(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *trig_expr = NULL;
    expr_t *exponent_expr = NULL;
    expr_t *coeff = NULL;
    number_t exponent = num_new();
    expr_t *out = NULL;
    unsigned int degree = 0u;
    bool is_sin = false;

    if (!match_power_trig_product(expr, wrt, &power_expr, &trig_expr, &is_sin))
        goto cleanup;
    if (!expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent_expr) ||
        !expr_match_const_value(exponent_expr, &exponent) ||
        !expr_integrate_number_matches_uint_at_most(exponent, 4u, &degree) ||
        !match_trig_proportional_wrt_coeff(trig_expr, wrt, &is_sin, &coeff))
        goto cleanup;

    out = build_symbolic_integer_power_trig_integral(degree, is_sin, trig_expr, coeff, wrt);

cleanup:
    num_destroy(&exponent);
    expr_free(coeff);
    expr_free(exponent_expr);
    return out;
}

static bool match_exp_trig_product(const expr_t *expr, const expr_t *wrt, const expr_t **exp_out,
                                   const expr_t **trig_out, bool *is_sin_out, expr_t **exp_coeff_out,
                                   expr_t **trig_coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *trig_coeff = NULL;

    if (!expr || !wrt || !exp_out || !trig_out || !is_sin_out || !exp_coeff_out || !trig_coeff_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (match_exp_proportional_wrt_coeff(left, wrt, &exp_coeff) &&
        match_trig_proportional_wrt_coeff(right, wrt, is_sin_out, &trig_coeff)) {
        *exp_out = left;
        *trig_out = right;
        *exp_coeff_out = exp_coeff;
        *trig_coeff_out = trig_coeff;
        return true;
    }
    expr_free(exp_coeff);
    expr_free(trig_coeff);
    exp_coeff = NULL;
    trig_coeff = NULL;

    if (match_exp_proportional_wrt_coeff(right, wrt, &exp_coeff) &&
        match_trig_proportional_wrt_coeff(left, wrt, is_sin_out, &trig_coeff)) {
        *exp_out = right;
        *trig_out = left;
        *exp_coeff_out = exp_coeff;
        *trig_coeff_out = trig_coeff;
        return true;
    }

    expr_free(exp_coeff);
    expr_free(trig_coeff);
    return false;
}

expr_t *integrate_symbolic_exp_times_trig(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *exp_expr = NULL;
    const expr_t *trig_expr = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *trig_coeff = NULL;
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *sum = NULL;
    expr_t *product = NULL;
    expr_t *exp_coeff_sq = NULL;
    expr_t *trig_coeff_sq = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool is_sin = false;

    if (!match_exp_trig_product(expr, wrt, &exp_expr, &trig_expr, &is_sin, &exp_coeff, &trig_coeff))
        goto cleanup;

    sin_v = trig_expr && trig_expr->a ? expr_sin(trig_expr->a) : NULL;
    cos_v = trig_expr && trig_expr->a ? expr_cos(trig_expr->a) : NULL;
    if (is_sin) {
        left = (exp_coeff && sin_v) ? expr_mul(exp_coeff, sin_v) : NULL;
        right = (trig_coeff && cos_v) ? expr_mul(trig_coeff, cos_v) : NULL;
        sum = (left && right) ? expr_sub(left, right) : NULL;
    } else {
        left = (trig_coeff && sin_v) ? expr_mul(trig_coeff, sin_v) : NULL;
        right = (exp_coeff && cos_v) ? expr_mul(exp_coeff, cos_v) : NULL;
        sum = (left && right) ? expr_add(left, right) : NULL;
    }

    product = (exp_expr && sum) ? expr_mul(exp_expr, sum) : NULL;
    exp_coeff_sq = exp_coeff ? expr_pow(exp_coeff, &NUM_TWO) : NULL;
    trig_coeff_sq = trig_coeff ? expr_pow(trig_coeff, &NUM_TWO) : NULL;
    denom = (exp_coeff_sq && trig_coeff_sq) ? expr_add(exp_coeff_sq, trig_coeff_sq) : NULL;
    quotient = (product && denom) ? expr_div(product, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(trig_coeff_sq);
    expr_free(exp_coeff_sq);
    expr_free(product);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(cos_v);
    expr_free(sin_v);
    expr_free(trig_coeff);
    expr_free(exp_coeff);
    return out;
}

typedef struct {
    bool has_wrt;
    bool has_exp;
    bool has_trig;
    bool trig_is_sin;
} exact_wrt_exp_trig_match_t;

static bool match_wrt_exp_trig_exact_rec(const expr_t *expr, const expr_t *wrt, exact_wrt_exp_trig_match_t *match)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt || !match)
        return false;

    if (expr_match_mul_expr(expr, &left, &right))
        return match_wrt_exp_trig_exact_rec(left, wrt, match) && match_wrt_exp_trig_exact_rec(right, wrt, match);

    if (is_wrt(expr, wrt)) {
        if (match->has_wrt)
            return false;
        match->has_wrt = true;
        return true;
    }

    if (expr_is_op(expr, &ops_exp) && expr->a && is_wrt(expr->a, wrt)) {
        if (match->has_exp)
            return false;
        match->has_exp = true;
        return true;
    }

    if ((expr_is_op(expr, &ops_sin) || expr_is_op(expr, &ops_cos)) && expr->a && is_wrt(expr->a, wrt)) {
        if (match->has_trig)
            return false;
        match->has_trig = true;
        match->trig_is_sin = expr_is_op(expr, &ops_sin);
        return true;
    }

    return false;
}

expr_t *integrate_wrt_exp_times_trig_exact(const expr_t *expr, const expr_t *wrt)
{
    exact_wrt_exp_trig_match_t match = {false, false, false, false};
    expr_t *exp_x = NULL;
    expr_t *sin_x = NULL;
    expr_t *cos_x = NULL;
    expr_t *x_sin = NULL;
    expr_t *x_cos = NULL;
    expr_t *first = NULL;
    expr_t *bracket = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    if (!match_wrt_exp_trig_exact_rec(expr, wrt, &match) || !match.has_wrt || !match.has_exp || !match.has_trig)
        goto cleanup;

    exp_x = expr_exp(wrt);
    sin_x = expr_sin(wrt);
    cos_x = expr_cos(wrt);
    x_sin = (wrt && sin_x) ? expr_mul(wrt, sin_x) : NULL;
    x_cos = (wrt && cos_x) ? expr_mul(wrt, cos_x) : NULL;
    if (match.trig_is_sin) {
        first = (cos_x && x_cos) ? expr_sub(cos_x, x_cos) : NULL;
        bracket = (first && x_sin) ? expr_add(first, x_sin) : NULL;
    } else {
        first = (x_cos && sin_x) ? expr_sub(x_cos, sin_x) : NULL;
        bracket = (first && x_sin) ? expr_add(first, x_sin) : NULL;
    }
    product = (exp_x && bracket) ? expr_mul(exp_x, bracket) : NULL;
    out = product ? mul_number_owned(product, NUM_HALF) : NULL;
    product = NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(product);
    expr_free(bracket);
    expr_free(first);
    expr_free(x_cos);
    expr_free(x_sin);
    expr_free(cos_x);
    expr_free(sin_x);
    expr_free(exp_x);
    return out;
}
