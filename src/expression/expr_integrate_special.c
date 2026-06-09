#include <stdbool.h>

#include "expr_integrate_internal.h"
#include "internal/number_internal.h"

typedef expr_t *(*expr_binary_build_fn)(const expr_t *left, const expr_t *right);
typedef expr_t *(*squared_unary_raw_fn)(const expr_t *u);
typedef expr_t *(*cubed_unary_raw_fn)(const expr_t *u);

typedef struct squared_unary_rule {
    squared_unary_raw_fn build_raw;
    long divisor_factor;
} squared_unary_rule_t;

typedef struct cubed_unary_rule {
    cubed_unary_raw_fn build_raw;
} cubed_unary_rule_t;

static expr_t *build_double_angle_squared_raw(const expr_t *u,
                                              expr_apply_unary_fn oscillation_fn,
                                              bool scaled_first,
                                              expr_binary_build_fn combine)
{
    expr_t *two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
    expr_t *oscillation = (two_u && oscillation_fn) ? oscillation_fn(two_u) : NULL;
    expr_t *scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
    expr_t *raw = NULL;

    if (scaled_u && oscillation)
        raw = scaled_first ? combine(scaled_u, oscillation) : combine(oscillation, scaled_u);
    expr_free(scaled_u);
    expr_free(oscillation);
    expr_free(two_u);
    return raw;
}

static expr_t *build_sin_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sin, true, expr_sub);
}

static expr_t *build_cos_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sin, true, expr_add);
}

static expr_t *build_sinh_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sinh, false, expr_sub);
}

static expr_t *build_cosh_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sinh, false, expr_add);
}

static expr_t *build_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    return (u && unary_fn) ? unary_fn(u) : NULL;
}

static expr_t *build_unary_minus_u_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = (term && u) ? expr_sub(term, u) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_u_minus_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = (u && term) ? expr_sub(u, term) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_neg_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = term ? expr_neg(term) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_tan_squared_raw(const expr_t *u)
{
    return build_unary_minus_u_raw(u, expr_tan);
}

static expr_t *build_sec_squared_raw(const expr_t *u)
{
    return build_unary_raw(u, expr_tan);
}

static expr_t *build_cosec_squared_raw(const expr_t *u)
{
    return build_neg_unary_raw(u, expr_cot);
}

static expr_t *build_sech_squared_raw(const expr_t *u)
{
    return build_unary_raw(u, expr_tanh);
}

static expr_t *build_cosech_squared_raw(const expr_t *u)
{
    return build_neg_unary_raw(u, expr_coth);
}

static expr_t *build_tanh_squared_raw(const expr_t *u)
{
    return build_u_minus_unary_raw(u, expr_tanh);
}

static expr_t *build_coth_squared_raw(const expr_t *u)
{
    return build_u_minus_unary_raw(u, expr_coth);
}

static expr_t *build_sin_cubed_raw(const expr_t *u)
{
    number_t three = num_create_from_long(3);
    expr_t *cos_u = expr_cos(u);
    expr_t *cos_cubed = cos_u ? expr_pow(cos_u, &three) : NULL;
    expr_t *third = cos_cubed ? expr_mul_num(cos_cubed, &NUM_ONE_THIRD) : NULL;
    expr_t *raw = (third && cos_u) ? expr_sub(third, cos_u) : NULL;

    num_destroy(&three);
    expr_free(third);
    expr_free(cos_cubed);
    expr_free(cos_u);
    return raw;
}

static expr_t *build_cos_cubed_raw(const expr_t *u)
{
    number_t three = num_create_from_long(3);
    expr_t *sin_u = expr_sin(u);
    expr_t *sin_cubed = sin_u ? expr_pow(sin_u, &three) : NULL;
    expr_t *third = sin_cubed ? expr_mul_num(sin_cubed, &NUM_ONE_THIRD) : NULL;
    expr_t *raw = (sin_u && third) ? expr_sub(sin_u, third) : NULL;

    num_destroy(&three);
    expr_free(third);
    expr_free(sin_cubed);
    expr_free(sin_u);
    return raw;
}

static expr_t *build_tan_cubed_raw(const expr_t *u)
{
    expr_t *tan_u = expr_tan(u);
    expr_t *tan_sq = tan_u ? expr_pow(tan_u, &NUM_TWO) : NULL;
    expr_t *half_tan_sq = tan_sq ? expr_mul_num(tan_sq, &NUM_HALF) : NULL;
    expr_t *cos_u = expr_cos(u);
    expr_t *log_cos = cos_u ? expr_log(cos_u) : NULL;
    expr_t *raw = (half_tan_sq && log_cos) ? expr_add(half_tan_sq, log_cos) : NULL;

    expr_free(log_cos);
    expr_free(cos_u);
    expr_free(half_tan_sq);
    expr_free(tan_sq);
    expr_free(tan_u);
    return raw;
}

static expr_t *build_sec_cubed_raw(const expr_t *u)
{
    expr_t *sec_u = expr_sec(u);
    expr_t *tan_u = expr_tan(u);
    expr_t *product = (sec_u && tan_u) ? expr_mul(sec_u, tan_u) : NULL;
    expr_t *sum = (sec_u && tan_u) ? expr_add(sec_u, tan_u) : NULL;
    expr_t *log_sum = sum ? expr_log(sum) : NULL;
    expr_t *raw_sum = (product && log_sum) ? expr_add(product, log_sum) : NULL;
    expr_t *raw = raw_sum ? expr_mul_num(raw_sum, &NUM_HALF) : NULL;

    expr_free(raw_sum);
    expr_free(log_sum);
    expr_free(sum);
    expr_free(product);
    expr_free(tan_u);
    expr_free(sec_u);
    return raw;
}

static expr_t *build_cosec_cubed_raw(const expr_t *u)
{
    expr_t *cosec_u = expr_cosec(u);
    expr_t *cot_u = expr_cot(u);
    expr_t *product = (cosec_u && cot_u) ? expr_mul(cosec_u, cot_u) : NULL;
    expr_t *sum = (cosec_u && cot_u) ? expr_add(cosec_u, cot_u) : NULL;
    expr_t *log_sum = sum ? expr_log(sum) : NULL;
    expr_t *raw_sum = (product && log_sum) ? expr_add(product, log_sum) : NULL;
    expr_t *half = raw_sum ? expr_mul_num(raw_sum, &NUM_HALF) : NULL;
    expr_t *raw = half ? expr_neg(half) : NULL;

    expr_free(half);
    expr_free(raw_sum);
    expr_free(log_sum);
    expr_free(sum);
    expr_free(product);
    expr_free(cot_u);
    expr_free(cosec_u);
    return raw;
}

static const squared_unary_rule_t squared_unary_rules[EXPR_PATTERN_UNARY_COUNT] = {
    [EXPR_PATTERN_UNARY_SIN] = { build_sin_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COS] = { build_cos_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_SINH] = { build_sinh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COSH] = { build_cosh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_TAN] = { build_tan_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SEC] = { build_sec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COSEC] = { build_cosec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SECH] = { build_sech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COSECH] = { build_cosech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_TANH] = { build_tanh_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COTH] = { build_coth_squared_raw, 1 }
};

static const cubed_unary_rule_t cubed_unary_rules[EXPR_PATTERN_UNARY_COUNT] = {
    [EXPR_PATTERN_UNARY_SIN] = { build_sin_cubed_raw },
    [EXPR_PATTERN_UNARY_COS] = { build_cos_cubed_raw },
    [EXPR_PATTERN_UNARY_TAN] = { build_tan_cubed_raw },
    [EXPR_PATTERN_UNARY_SEC] = { build_sec_cubed_raw },
    [EXPR_PATTERN_UNARY_COSEC] = { build_cosec_cubed_raw }
};

static const squared_unary_rule_t *find_squared_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const squared_unary_rule_t *rule;

    if ((unsigned)kind >= (unsigned)EXPR_PATTERN_UNARY_COUNT)
        return NULL;
    rule = &squared_unary_rules[kind];
    return rule->build_raw ? rule : NULL;
}

static const cubed_unary_rule_t *find_cubed_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const cubed_unary_rule_t *rule;

    if ((unsigned)kind >= (unsigned)EXPR_PATTERN_UNARY_COUNT)
        return NULL;
    rule = &cubed_unary_rules[kind];
    return rule->build_raw ? rule : NULL;
}

expr_t *integrate_squared_unary_affine(const expr_t *expr,
                                      const expr_t *wrt,
                                      expr_pattern_unary_affine_kind_t kind)
{
    const squared_unary_rule_t *rule = find_squared_unary_rule(kind);
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u;
    expr_t *raw = NULL;

    if (!rule ||
        !expr || !expr->a || !num_eq(expr->c, NUM_TWO) ||
        !match_affine_unary_data(expr->a, wrt, kind, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    raw = u ? rule->build_raw(u) : NULL;
    expr_free(u);
    num_destroy(&constant);
    if (rule->divisor_factor == 1)
        return div_number_owned_consuming(raw, &coeff);
    raw = expr_integrate_div_number_owned_by_long_product(raw, rule->divisor_factor, coeff);
    num_destroy(&coeff);
    return raw;
}

expr_t *integrate_cubed_unary_affine(const expr_t *expr,
                                    const expr_t *wrt,
                                    expr_pattern_unary_affine_kind_t kind)
{
    const cubed_unary_rule_t *rule = find_cubed_unary_rule(kind);
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u;
    expr_t *raw = NULL;

    if (!rule ||
        !expr || !expr->a ||
        !match_affine_unary_data(expr->a, wrt, kind, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    raw = u ? rule->build_raw(u) : NULL;
    expr_free(u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

static bool match_affine_unary_power_data(const expr_t *expr,
                                          const expr_t *wrt,
                                          expr_pattern_unary_affine_kind_t kind,
                                          number_t exponent,
                                          number_t *constant_out,
                                          number_t *coeff_out)
{
    number_t matched_exponent = num_new();
    const expr_t *base = NULL;
    bool ok = false;

    if (!expr)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        base = expr->a;
        ok = num_eq(expr->c, exponent);
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &matched_exponent)) {
        base = expr->a;
        ok = num_eq(matched_exponent, exponent);
    }

    if (ok)
        ok = match_affine_unary_data(base, wrt, kind, constant_out, coeff_out);

cleanup:
    num_destroy(&matched_exponent);
    return ok;
}

static bool match_affine_unary_any_power_data(const expr_t *expr,
                                              const expr_t *wrt,
                                              expr_pattern_unary_affine_kind_t kind,
                                              expr_t **exponent_out,
                                              number_t *constant_out,
                                              number_t *coeff_out)
{
    const expr_t *base = NULL;
    expr_t *exponent = NULL;
    bool ok = false;

    if (!expr || !wrt || !exponent_out)
        return false;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        base = expr->a;
        exponent = expr_new_const(expr->c);
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b && !depends_on_wrt(expr->b, wrt)) {
        base = expr->a;
        exponent = expr_integrate_retain_expr(expr->b);
    }

    if (!exponent || expr_const_is_zero(exponent))
        goto cleanup;

    ok = match_affine_unary_data(base, wrt, kind, constant_out, coeff_out);
    if (ok) {
        *exponent_out = exponent;
        exponent = NULL;
    }

cleanup:
    expr_free(exponent);
    return ok;
}

static expr_t *integrate_affine_unary_power_times_unary_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t power_kind,
    expr_pattern_unary_affine_kind_t trigger_kind,
    expr_apply_unary_fn power_fn,
    bool negate)
{
    number_t power_constant = num_new();
    number_t power_coeff = num_new();
    number_t trigger_constant = num_new();
    number_t trigger_coeff = num_new();
    expr_t *exponent = NULL;
    expr_t *u = NULL;
    expr_t *base = NULL;
    expr_t *power = NULL;
    expr_t *numerator = NULL;
    expr_t *coeff_expr = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool matched = false;

    if (!expr || !expr->a || !expr->b || !power_fn)
        goto cleanup;

    if (match_affine_unary_any_power_data(expr->a, wrt, power_kind, &exponent,
                                          &power_constant, &power_coeff) &&
        match_affine_unary_data(expr->b, wrt, trigger_kind,
                                &trigger_constant, &trigger_coeff) &&
        affine_linear_match_eq(power_constant, power_coeff,
                               trigger_constant, trigger_coeff)) {
        matched = true;
    } else {
        expr_free(exponent);
        exponent = NULL;
        if (match_affine_unary_data(expr->a, wrt, trigger_kind,
                                    &trigger_constant, &trigger_coeff) &&
            match_affine_unary_any_power_data(expr->b, wrt, power_kind, &exponent,
                                              &power_constant, &power_coeff) &&
            affine_linear_match_eq(power_constant, power_coeff,
                                   trigger_constant, trigger_coeff)) {
            matched = true;
        }
    }

    if (!matched || !exponent)
        goto cleanup;

    u = build_affine_from_match(wrt, power_constant, power_coeff);
    base = u ? power_fn(u) : NULL;
    power = (base && exponent) ? expr_pow_xp(base, exponent) : NULL;
    numerator = (negate && power) ? expr_neg(power) : power;
    coeff_expr = expr_new_const(power_coeff);
    denom = (exponent && coeff_expr) ? expr_mul(exponent, coeff_expr) : NULL;
    quotient = (numerator && denom) ? expr_div(numerator, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(coeff_expr);
    if (negate)
        expr_free(numerator);
    expr_free(power);
    expr_free(base);
    expr_free(u);
    expr_free(exponent);
    num_destroy(&trigger_coeff);
    num_destroy(&trigger_constant);
    num_destroy(&power_coeff);
    num_destroy(&power_constant);
    return out;
}

static bool match_exp_and_unary_affine_product(const expr_t *expr,
                                               const expr_t *wrt,
                                               expr_pattern_unary_affine_kind_t kind,
                                               number_t *exp_constant,
                                               number_t *exp_coeff,
                                               number_t *unary_constant,
                                               number_t *unary_coeff)
{
    if (!expr || !expr->a || !expr->b)
        return false;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP,
                                exp_constant, exp_coeff) &&
        match_affine_unary_data(expr->b, wrt, kind,
                                unary_constant, unary_coeff))
        return true;

    return match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_EXP,
                                   exp_constant, exp_coeff) &&
           match_affine_unary_data(expr->a, wrt, kind,
                                   unary_constant, unary_coeff);
}

static expr_t *integrate_exp_times_trig_affine_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind)
{
    number_t exp_constant = num_new();
    number_t exp_coeff = num_new();
    number_t trig_constant = num_new();
    number_t trig_coeff = num_new();
    expr_t *exp_u = NULL;
    expr_t *trig_v = NULL;
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *sum = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    if (!match_exp_and_unary_affine_product(expr, wrt, kind,
                                            &exp_constant, &exp_coeff,
                                            &trig_constant, &trig_coeff))
        goto cleanup;

    exp_u = build_affine_from_match(wrt, exp_constant, exp_coeff);
    trig_v = build_affine_from_match(wrt, trig_constant, trig_coeff);
    if (!exp_u || !trig_v)
        goto cleanup;

    {
        expr_t *exp_arg = exp_u;

        exp_u = exp_arg ? expr_exp(exp_arg) : NULL;
        expr_free(exp_arg);
        exp_u = simplify_owned(exp_u);
    }
    if (kind == EXPR_PATTERN_UNARY_SIN) {
        sin_v = expr_sin(trig_v);
        cos_v = expr_cos(trig_v);
        left = sin_v ? expr_mul_num(sin_v, &exp_coeff) : NULL;
        right = cos_v ? expr_mul_num(cos_v, &trig_coeff) : NULL;
        sum = (left && right) ? expr_sub(left, right) : NULL;
    } else if (kind == EXPR_PATTERN_UNARY_COS) {
        cos_v = expr_cos(trig_v);
        sin_v = expr_sin(trig_v);
        left = cos_v ? expr_mul_num(cos_v, &exp_coeff) : NULL;
        right = sin_v ? expr_mul_num(sin_v, &trig_coeff) : NULL;
        sum = (left && right) ? expr_add(left, right) : NULL;
    } else {
        goto cleanup;
    }

    product = (exp_u && sum) ? expr_mul(exp_u, sum) : NULL;
    if (product) {
        number_t exp_coeff_sq = num_mul(exp_coeff, exp_coeff);
        number_t trig_coeff_sq = num_mul(trig_coeff, trig_coeff);
        number_t denom = num_add(exp_coeff_sq, trig_coeff_sq);

        out = div_number_owned(product, denom);
        product = NULL;
        num_destroy(&denom);
        num_destroy(&trig_coeff_sq);
        num_destroy(&exp_coeff_sq);
    }

cleanup:
    expr_free(product);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(cos_v);
    expr_free(sin_v);
    expr_free(trig_v);
    expr_free(exp_u);
    num_destroy(&trig_coeff);
    num_destroy(&trig_constant);
    num_destroy(&exp_coeff);
    num_destroy(&exp_constant);
    return out;
}

static expr_t *integrate_exp_times_hyperbolic_affine_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind)
{
    number_t exp_constant = num_new();
    number_t exp_coeff = num_new();
    number_t hyper_constant = num_new();
    number_t hyper_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *u_plus_v = NULL;
    expr_t *u_minus_v = NULL;
    expr_t *exp_sum = NULL;
    expr_t *exp_diff = NULL;
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *combined = NULL;
    expr_t *out = NULL;

    if (!match_exp_and_unary_affine_product(expr, wrt, kind,
                                            &exp_constant, &exp_coeff,
                                            &hyper_constant, &hyper_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(exp_coeff, hyper_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(exp_coeff, hyper_coeff);
    if (num_eq(sum_coeff, NUM_ZERO) || num_eq(diff_coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, exp_constant, exp_coeff);
    v = build_affine_from_match(wrt, hyper_constant, hyper_coeff);
    u_plus_v = (u && v) ? expr_add(u, v) : NULL;
    u_minus_v = (u && v) ? expr_sub(u, v) : NULL;
    exp_sum = u_plus_v ? expr_exp(u_plus_v) : NULL;
    exp_diff = u_minus_v ? expr_exp(u_minus_v) : NULL;
    first = exp_sum ? div_number_owned(exp_sum, sum_coeff) : NULL;
    exp_sum = NULL;
    second = exp_diff ? div_number_owned(exp_diff, diff_coeff) : NULL;
    exp_diff = NULL;

    if (kind == EXPR_PATTERN_UNARY_SINH) {
        combined = (first && second) ? expr_sub(first, second) : NULL;
    } else if (kind == EXPR_PATTERN_UNARY_COSH) {
        combined = (first && second) ? expr_add(first, second) : NULL;
    }
    out = combined ? mul_number_owned(combined, NUM_HALF) : NULL;
    combined = NULL;

cleanup:
    expr_free(combined);
    expr_free(second);
    expr_free(first);
    expr_free(exp_diff);
    expr_free(exp_sum);
    expr_free(u_minus_v);
    expr_free(u_plus_v);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&hyper_coeff);
    num_destroy(&hyper_constant);
    num_destroy(&exp_coeff);
    num_destroy(&exp_constant);
    return out;
}

static bool match_sinh_cosh_affine_product(const expr_t *expr,
                                           const expr_t *wrt,
                                           number_t *sinh_constant,
                                           number_t *sinh_coeff,
                                           number_t *cosh_constant,
                                           number_t *cosh_coeff)
{
    if (!expr || !expr->a || !expr->b)
        return false;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH,
                                sinh_constant, sinh_coeff) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH,
                                cosh_constant, cosh_coeff))
        return true;

    return match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH,
                                   sinh_constant, sinh_coeff) &&
           match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSH,
                                   cosh_constant, cosh_coeff);
}

static bool match_affine_unary_pair_product(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_pattern_unary_affine_kind_t first_kind,
                                            expr_pattern_unary_affine_kind_t second_kind,
                                            number_t *first_constant,
                                            number_t *first_coeff,
                                            number_t *second_constant,
                                            number_t *second_coeff)
{
    if (!expr || !expr->a || !expr->b)
        return false;

    if (match_affine_unary_data(expr->a, wrt, first_kind, first_constant, first_coeff) &&
        match_affine_unary_data(expr->b, wrt, second_kind, second_constant, second_coeff))
        return true;

    return match_affine_unary_data(expr->b, wrt, first_kind, first_constant, first_coeff) &&
           match_affine_unary_data(expr->a, wrt, second_kind, second_constant, second_coeff);
}

static expr_t *integrate_unary_affine_argument(const expr_t *arg,
                                               const number_t *coeff,
                                               const expr_t *wrt,
                                               expr_apply_unary_fn integrand_fn,
                                               expr_apply_unary_fn antiderivative_fn,
                                               bool negate_antiderivative)
{
    expr_t *primitive = NULL;
    expr_t *scaled = NULL;

    if (!arg || !coeff || !wrt || !integrand_fn || !antiderivative_fn)
        return NULL;

    if (num_eq(*coeff, NUM_ZERO)) {
        expr_t *constant = integrand_fn(arg);

        scaled = constant ? expr_mul(constant, wrt) : NULL;
        expr_free(constant);
        return scaled;
    }

    primitive = antiderivative_fn(arg);
    if (negate_antiderivative) {
        expr_t *negated = primitive ? expr_neg(primitive) : NULL;

        expr_free(primitive);
        primitive = negated;
    }
    return div_number_owned(primitive, *coeff);
}

static expr_t *integrate_cos_affine_argument(const expr_t *arg,
                                             const number_t *coeff,
                                             const expr_t *wrt)
{
    return integrate_unary_affine_argument(arg, coeff, wrt, expr_cos, expr_sin, false);
}

static expr_t *integrate_sin_affine_argument(const expr_t *arg,
                                             const number_t *coeff,
                                             const expr_t *wrt)
{
    return integrate_unary_affine_argument(arg, coeff, wrt, expr_sin, expr_cos, true);
}

static expr_t *integrate_cosh_affine_argument(const expr_t *arg,
                                              const number_t *coeff,
                                              const expr_t *wrt)
{
    return integrate_unary_affine_argument(arg, coeff, wrt, expr_cosh, expr_sinh, false);
}

static expr_t *integrate_sinh_affine_argument(const expr_t *arg,
                                              const number_t *coeff,
                                              const expr_t *wrt)
{
    return integrate_unary_affine_argument(arg, coeff, wrt, expr_sinh, expr_cosh, false);
}

static expr_t *combine_half_sum_difference(expr_t *left, expr_t *right, bool subtract)
{
    expr_t *combined = NULL;
    expr_t *out = NULL;

    if (left && right)
        combined = subtract ? expr_sub(left, right) : expr_add(left, right);
    expr_free(right);
    expr_free(left);
    out = combined ? mul_number_owned(combined, NUM_HALF) : NULL;
    return out;
}

static expr_t *integrate_trig_affine_product(const expr_t *expr,
                                             const expr_t *wrt,
                                             expr_pattern_unary_affine_kind_t first_kind,
                                             expr_pattern_unary_affine_kind_t second_kind)
{
    number_t first_constant = num_new();
    number_t first_coeff = num_new();
    number_t second_constant = num_new();
    number_t second_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *sum_arg = NULL;
    expr_t *diff_arg = NULL;
    expr_t *sum_term = NULL;
    expr_t *diff_term = NULL;
    expr_t *out = NULL;

    if (!match_affine_unary_pair_product(expr, wrt, first_kind, second_kind,
                                         &first_constant, &first_coeff,
                                         &second_constant, &second_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(first_coeff, second_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(first_coeff, second_coeff);

    u = build_affine_from_match(wrt, first_constant, first_coeff);
    v = build_affine_from_match(wrt, second_constant, second_coeff);
    sum_arg = (u && v) ? expr_add(u, v) : NULL;
    diff_arg = (u && v) ? expr_sub(u, v) : NULL;
    if (!sum_arg || !diff_arg)
        goto cleanup;

    if (first_kind == EXPR_PATTERN_UNARY_SIN &&
        second_kind == EXPR_PATTERN_UNARY_SIN) {
        diff_term = integrate_cos_affine_argument(diff_arg, &diff_coeff, wrt);
        sum_term = integrate_cos_affine_argument(sum_arg, &sum_coeff, wrt);
        out = combine_half_sum_difference(diff_term, sum_term, true);
        diff_term = NULL;
        sum_term = NULL;
    } else if (first_kind == EXPR_PATTERN_UNARY_COS &&
               second_kind == EXPR_PATTERN_UNARY_COS) {
        sum_term = integrate_cos_affine_argument(sum_arg, &sum_coeff, wrt);
        diff_term = integrate_cos_affine_argument(diff_arg, &diff_coeff, wrt);
        out = combine_half_sum_difference(sum_term, diff_term, false);
        sum_term = NULL;
        diff_term = NULL;
    } else if (first_kind == EXPR_PATTERN_UNARY_SIN &&
               second_kind == EXPR_PATTERN_UNARY_COS) {
        sum_term = integrate_sin_affine_argument(sum_arg, &sum_coeff, wrt);
        diff_term = integrate_sin_affine_argument(diff_arg, &diff_coeff, wrt);
        out = combine_half_sum_difference(sum_term, diff_term, false);
        sum_term = NULL;
        diff_term = NULL;
    }

cleanup:
    expr_free(diff_term);
    expr_free(sum_term);
    expr_free(diff_arg);
    expr_free(sum_arg);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&second_coeff);
    num_destroy(&second_constant);
    num_destroy(&first_coeff);
    num_destroy(&first_constant);
    return out;
}

static expr_t *integrate_hyperbolic_affine_product(const expr_t *expr,
                                                   const expr_t *wrt,
                                                   expr_pattern_unary_affine_kind_t first_kind,
                                                   expr_pattern_unary_affine_kind_t second_kind)
{
    number_t first_constant = num_new();
    number_t first_coeff = num_new();
    number_t second_constant = num_new();
    number_t second_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *sum_arg = NULL;
    expr_t *diff_arg = NULL;
    expr_t *sum_term = NULL;
    expr_t *diff_term = NULL;
    expr_t *out = NULL;

    if (!match_affine_unary_pair_product(expr, wrt, first_kind, second_kind,
                                         &first_constant, &first_coeff,
                                         &second_constant, &second_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(first_coeff, second_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(first_coeff, second_coeff);

    u = build_affine_from_match(wrt, first_constant, first_coeff);
    v = build_affine_from_match(wrt, second_constant, second_coeff);
    sum_arg = (u && v) ? expr_add(u, v) : NULL;
    diff_arg = (u && v) ? expr_sub(u, v) : NULL;
    if (!sum_arg || !diff_arg)
        goto cleanup;

    if (first_kind == EXPR_PATTERN_UNARY_SINH &&
        second_kind == EXPR_PATTERN_UNARY_SINH) {
        sum_term = integrate_cosh_affine_argument(sum_arg, &sum_coeff, wrt);
        diff_term = integrate_cosh_affine_argument(diff_arg, &diff_coeff, wrt);
        out = combine_half_sum_difference(sum_term, diff_term, true);
        sum_term = NULL;
        diff_term = NULL;
    } else if (first_kind == EXPR_PATTERN_UNARY_COSH &&
               second_kind == EXPR_PATTERN_UNARY_COSH) {
        sum_term = integrate_cosh_affine_argument(sum_arg, &sum_coeff, wrt);
        diff_term = integrate_cosh_affine_argument(diff_arg, &diff_coeff, wrt);
        out = combine_half_sum_difference(sum_term, diff_term, false);
        sum_term = NULL;
        diff_term = NULL;
    } else if (first_kind == EXPR_PATTERN_UNARY_SINH &&
               second_kind == EXPR_PATTERN_UNARY_COSH) {
        sum_term = integrate_sinh_affine_argument(sum_arg, &sum_coeff, wrt);
        diff_term = integrate_sinh_affine_argument(diff_arg, &diff_coeff, wrt);
        out = combine_half_sum_difference(sum_term, diff_term, false);
        sum_term = NULL;
        diff_term = NULL;
    }

cleanup:
    expr_free(diff_term);
    expr_free(sum_term);
    expr_free(diff_arg);
    expr_free(sum_arg);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&second_coeff);
    num_destroy(&second_constant);
    num_destroy(&first_coeff);
    num_destroy(&first_constant);
    return out;
}

static expr_t *integrate_trig_hyperbolic_affine_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t trig_kind,
    expr_pattern_unary_affine_kind_t hyper_kind)
{
    number_t trig_constant = num_new();
    number_t trig_coeff = num_new();
    number_t hyper_constant = num_new();
    number_t hyper_coeff = num_new();
    number_t trig_coeff_sq = num_new();
    number_t hyper_coeff_sq = num_new();
    number_t denom = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *sin_u = NULL;
    expr_t *cos_u = NULL;
    expr_t *sinh_v = NULL;
    expr_t *cosh_v = NULL;
    expr_t *left_factor = NULL;
    expr_t *right_factor = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *combined = NULL;
    expr_t *out = NULL;
    bool subtract = false;

    if (!match_affine_unary_pair_product(expr, wrt, trig_kind, hyper_kind,
                                         &trig_constant, &trig_coeff,
                                         &hyper_constant, &hyper_coeff))
        goto cleanup;

    num_destroy(&trig_coeff_sq);
    trig_coeff_sq = num_mul(trig_coeff, trig_coeff);
    num_destroy(&hyper_coeff_sq);
    hyper_coeff_sq = num_mul(hyper_coeff, hyper_coeff);
    num_destroy(&denom);
    denom = num_add(trig_coeff_sq, hyper_coeff_sq);
    if (num_eq(denom, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, trig_constant, trig_coeff);
    v = build_affine_from_match(wrt, hyper_constant, hyper_coeff);
    sin_u = u ? expr_sin(u) : NULL;
    cos_u = u ? expr_cos(u) : NULL;
    sinh_v = v ? expr_sinh(v) : NULL;
    cosh_v = v ? expr_cosh(v) : NULL;

    if (trig_kind == EXPR_PATTERN_UNARY_COS &&
        hyper_kind == EXPR_PATTERN_UNARY_COSH) {
        left_factor = (sin_u && cosh_v) ? expr_mul(sin_u, cosh_v) : NULL;
        right_factor = (cos_u && sinh_v) ? expr_mul(cos_u, sinh_v) : NULL;
        left = left_factor ? expr_mul_num(left_factor, &trig_coeff) : NULL;
        right = right_factor ? expr_mul_num(right_factor, &hyper_coeff) : NULL;
    } else if (trig_kind == EXPR_PATTERN_UNARY_COS &&
               hyper_kind == EXPR_PATTERN_UNARY_SINH) {
        left_factor = (cos_u && cosh_v) ? expr_mul(cos_u, cosh_v) : NULL;
        right_factor = (sin_u && sinh_v) ? expr_mul(sin_u, sinh_v) : NULL;
        left = left_factor ? expr_mul_num(left_factor, &hyper_coeff) : NULL;
        right = right_factor ? expr_mul_num(right_factor, &trig_coeff) : NULL;
    } else if (trig_kind == EXPR_PATTERN_UNARY_SIN &&
               hyper_kind == EXPR_PATTERN_UNARY_COSH) {
        left_factor = (sin_u && sinh_v) ? expr_mul(sin_u, sinh_v) : NULL;
        right_factor = (cos_u && cosh_v) ? expr_mul(cos_u, cosh_v) : NULL;
        left = left_factor ? expr_mul_num(left_factor, &hyper_coeff) : NULL;
        right = right_factor ? expr_mul_num(right_factor, &trig_coeff) : NULL;
        subtract = true;
    } else if (trig_kind == EXPR_PATTERN_UNARY_SIN &&
               hyper_kind == EXPR_PATTERN_UNARY_SINH) {
        left_factor = (sin_u && cosh_v) ? expr_mul(sin_u, cosh_v) : NULL;
        right_factor = (cos_u && sinh_v) ? expr_mul(cos_u, sinh_v) : NULL;
        left = left_factor ? expr_mul_num(left_factor, &hyper_coeff) : NULL;
        right = right_factor ? expr_mul_num(right_factor, &trig_coeff) : NULL;
        subtract = true;
    }

    combined = (left && right) ? (subtract ? expr_sub(left, right) : expr_add(left, right)) : NULL;
    out = div_number_owned(combined, denom);
    combined = NULL;

cleanup:
    expr_free(combined);
    expr_free(right);
    expr_free(left);
    expr_free(right_factor);
    expr_free(left_factor);
    expr_free(cosh_v);
    expr_free(sinh_v);
    expr_free(cos_u);
    expr_free(sin_u);
    expr_free(v);
    expr_free(u);
    num_destroy(&denom);
    num_destroy(&hyper_coeff_sq);
    num_destroy(&trig_coeff_sq);
    num_destroy(&hyper_coeff);
    num_destroy(&hyper_constant);
    num_destroy(&trig_coeff);
    num_destroy(&trig_constant);
    return out;
}

static expr_t *integrate_sinh_cosh_affine_product(const expr_t *expr,
                                                  const expr_t *wrt)
{
    number_t sinh_constant = num_new();
    number_t sinh_coeff = num_new();
    number_t cosh_constant = num_new();
    number_t cosh_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *u_plus_v = NULL;
    expr_t *u_minus_v = NULL;
    expr_t *cosh_sum = NULL;
    expr_t *cosh_diff = NULL;
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *combined = NULL;
    expr_t *out = NULL;

    if (!match_sinh_cosh_affine_product(expr, wrt,
                                        &sinh_constant, &sinh_coeff,
                                        &cosh_constant, &cosh_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(sinh_coeff, cosh_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(sinh_coeff, cosh_coeff);
    if (num_eq(sum_coeff, NUM_ZERO) || num_eq(diff_coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, sinh_constant, sinh_coeff);
    v = build_affine_from_match(wrt, cosh_constant, cosh_coeff);
    u_plus_v = (u && v) ? expr_add(u, v) : NULL;
    u_minus_v = (u && v) ? expr_sub(u, v) : NULL;
    cosh_sum = u_plus_v ? expr_cosh(u_plus_v) : NULL;
    cosh_diff = u_minus_v ? expr_cosh(u_minus_v) : NULL;
    first = cosh_sum ? div_number_owned(cosh_sum, sum_coeff) : NULL;
    cosh_sum = NULL;
    second = cosh_diff ? div_number_owned(cosh_diff, diff_coeff) : NULL;
    cosh_diff = NULL;
    combined = (first && second) ? expr_add(first, second) : NULL;
    out = combined ? mul_number_owned(combined, NUM_HALF) : NULL;
    combined = NULL;

cleanup:
    expr_free(combined);
    expr_free(second);
    expr_free(first);
    expr_free(cosh_diff);
    expr_free(cosh_sum);
    expr_free(u_minus_v);
    expr_free(u_plus_v);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&cosh_coeff);
    num_destroy(&cosh_constant);
    num_destroy(&sinh_coeff);
    num_destroy(&sinh_constant);
    return out;
}

expr_t *integrate_same_affine_special_product(const expr_t *expr, const expr_t *wrt)
{
    number_t c1 = num_new();
    number_t k1 = num_new();
    number_t c2 = num_new();
    number_t k2 = num_new();
    expr_t *u;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    out = integrate_exp_times_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN);
    if (out)
        goto cleanup;

    out = integrate_exp_times_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COS);
    if (out)
        goto cleanup;

    out = integrate_exp_times_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SINH);
    if (out)
        goto cleanup;

    out = integrate_exp_times_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                        EXPR_PATTERN_UNARY_SIN);
    if (out)
        goto cleanup;

    out = integrate_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COS,
                                        EXPR_PATTERN_UNARY_COS);
    if (out)
        goto cleanup;

    out = integrate_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                        EXPR_PATTERN_UNARY_COS);
    if (out)
        goto cleanup;

    out = integrate_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SINH,
                                              EXPR_PATTERN_UNARY_SINH);
    if (out)
        goto cleanup;

    out = integrate_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COSH,
                                              EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SINH,
                                              EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_trig_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COS,
                                                   EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_trig_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COS,
                                                   EXPR_PATTERN_UNARY_SINH);
    if (out)
        goto cleanup;

    out = integrate_trig_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                                   EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_trig_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                                   EXPR_PATTERN_UNARY_SINH);
    if (out)
        goto cleanup;

    out = integrate_sinh_cosh_affine_product(expr, wrt);
    if (out)
        goto cleanup;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
        affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
        affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *exp_u;
        expr_t *sin_u;
        expr_t *cos_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        exp_u = u ? expr_exp(u) : NULL;
        sin_u = u ? expr_sin(u) : NULL;
        cos_u = u ? expr_cos(u) : NULL;
        diff = (sin_u && cos_u) ? expr_sub(sin_u, cos_u) : NULL;
        out = (exp_u && diff) ? expr_mul(exp_u, diff) : NULL;
        expr_free(diff);
        expr_free(cos_u);
        expr_free(sin_u);
        expr_free(exp_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(out, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *exp_u;
        expr_t *sin_u;
        expr_t *cos_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        exp_u = u ? expr_exp(u) : NULL;
        sin_u = u ? expr_sin(u) : NULL;
        cos_u = u ? expr_cos(u) : NULL;
        sum = (sin_u && cos_u) ? expr_add(sin_u, cos_u) : NULL;
        out = (exp_u && sum) ? expr_mul(exp_u, sum) : NULL;
        expr_free(sum);
        expr_free(cos_u);
        expr_free(sin_u);
        expr_free(exp_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(out, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *exp_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        exp_two_u = two_u ? expr_exp(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (exp_two_u && scaled_u) ? expr_sub(exp_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(exp_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *exp_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        exp_two_u = two_u ? expr_exp(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (exp_two_u && scaled_u) ? expr_add(exp_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(exp_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(sum, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        number_t three = num_create_from_long(3);
        expr_t *sin_u;
        expr_t *sin_cubed;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_cubed = sin_u ? expr_pow(sin_u, &three) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(sin_cubed, 3, k1);
        num_destroy(&three);
    } else if (((match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c2, &k2)) ||
                (match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        number_t three = num_create_from_long(3);
        expr_t *cos_u;
        expr_t *cos_cubed;
        expr_t *neg_cos_cubed;

        u = build_affine_from_match(wrt, c1, k1);
        cos_u = u ? expr_cos(u) : NULL;
        cos_cubed = cos_u ? expr_pow(cos_u, &three) : NULL;
        neg_cos_cubed = cos_cubed ? expr_neg(cos_cubed) : NULL;
        expr_free(cos_cubed);
        expr_free(cos_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(neg_cos_cubed, 3, k1);
        num_destroy(&three);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TAN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        u = build_affine_from_match(wrt, c1, k1);
        out = u ? expr_sec(u) : NULL;
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COT, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosec_u;

        u = build_affine_from_match(wrt, c1, k1);
        cosec_u = u ? expr_cosec(u) : NULL;
        out = cosec_u ? expr_neg(cosec_u) : NULL;
        expr_free(cosec_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TAN, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_TAN, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_SEC, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sec_u;
        expr_t *sec_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sec_u = u ? expr_sec(u) : NULL;
        sec_sq = sec_u ? expr_pow(sec_u, &NUM_TWO) : NULL;
        expr_free(sec_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(sec_sq, NUM_TWO, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COT, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COT, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSEC, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosec_u;
        expr_t *cosec_sq;
        expr_t *neg_cosec_sq;

        u = build_affine_from_match(wrt, c1, k1);
        cosec_u = u ? expr_cosec(u) : NULL;
        cosec_sq = cosec_u ? expr_pow(cosec_u, &NUM_TWO) : NULL;
        neg_cosec_sq = cosec_sq ? expr_neg(cosec_sq) : NULL;
        expr_free(cosec_sq);
        expr_free(cosec_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(neg_cosec_sq, NUM_TWO, k1);
    }

    if (!out)
        out = integrate_affine_unary_power_times_unary_product(
            expr, wrt, EXPR_PATTERN_UNARY_SEC, EXPR_PATTERN_UNARY_TAN,
            expr_sec, false);
    if (!out)
        out = integrate_affine_unary_power_times_unary_product(
            expr, wrt, EXPR_PATTERN_UNARY_COSEC, EXPR_PATTERN_UNARY_COT,
            expr_cosec, true);
    if (out) {
        goto cleanup;
    } else if (((match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSEC, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SEC, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *tan_u;

        u = build_affine_from_match(wrt, c1, k1);
        tan_u = u ? expr_tan(u) : NULL;
        out = tan_u ? expr_log(tan_u) : NULL;
        expr_free(tan_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c2, &k2)) ||
                (match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        number_t four = num_create_from_long(4);
        number_t eight = num_create_from_long(8);
        number_t thirty_two = num_create_from_long(32);
        expr_t *four_u;
        expr_t *sin_four_u;
        expr_t *u_for_linear_term;
        expr_t *u_term;
        expr_t *sin_term;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        four_u = u ? expr_mul_num(u, &four) : NULL;
        sin_four_u = four_u ? expr_sin(four_u) : NULL;
        u_for_linear_term = build_affine_from_match(wrt, c1, k1);
        u_term = u_for_linear_term ? div_number_owned(u_for_linear_term, eight) : NULL;
        sin_term = sin_four_u ? div_number_owned(sin_four_u, thirty_two) : NULL;
        diff = (u_term && sin_term) ? expr_sub(u_term, sin_term) : NULL;
        expr_free(sin_term);
        expr_free(u_term);
        expr_free(four_u);
        expr_free(u);
        out = div_number_owned(diff, k1);
        num_destroy(&thirty_two);
        num_destroy(&eight);
        num_destroy(&four);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sin_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sin_two_u = two_u ? expr_sin(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (scaled_u && sin_two_u) ? expr_sub(scaled_u, sin_two_u) : NULL;
        expr_free(scaled_u);
        expr_free(sin_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sin_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sin_two_u = two_u ? expr_sin(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (scaled_u && sin_two_u) ? expr_add(scaled_u, sin_two_u) : NULL;
        expr_free(scaled_u);
        expr_free(sin_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(sum, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sinh_u;
        expr_t *sinh_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sinh_u = u ? expr_sinh(u) : NULL;
        sinh_sq = sinh_u ? expr_pow(sinh_u, &NUM_TWO) : NULL;
        expr_free(sinh_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_product(sinh_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SECH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TANH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sech_u;

        u = build_affine_from_match(wrt, c1, k1);
        sech_u = u ? expr_sech(u) : NULL;
        out = sech_u ? expr_neg(sech_u) : NULL;
        expr_free(sech_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSECH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COTH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosech_u;

        u = build_affine_from_match(wrt, c1, k1);
        cosech_u = u ? expr_cosech(u) : NULL;
        out = cosech_u ? expr_neg(cosech_u) : NULL;
        expr_free(cosech_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sinh_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sinh_two_u = two_u ? expr_sinh(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (sinh_two_u && scaled_u) ? expr_sub(sinh_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(sinh_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sinh_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sinh_two_u = two_u ? expr_sinh(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (sinh_two_u && scaled_u) ? expr_add(sinh_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(sinh_two_u);
        expr_free(two_u);
        expr_free(u);
        out = expr_integrate_div_number_owned_by_long_product(sum, 4, k1);
    }

cleanup:
    num_destroy(&k2);
    num_destroy(&c2);
    num_destroy(&k1);
    num_destroy(&c1);
    return out;
}
