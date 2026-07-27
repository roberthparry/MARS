#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

typedef expr_t *(*expr_binary_build_fn)(const expr_t *left, const expr_t *right);
typedef expr_t *(*squared_unary_raw_fn)(const expr_t *u);
typedef expr_t *(*cubed_unary_raw_fn)(const expr_t *u);

typedef struct squared_unary_rule {
    expr_op_kind_t op_kind;
    squared_unary_raw_fn build_raw;
    long divisor_factor;
} squared_unary_rule_t;

typedef struct cubed_unary_rule {
    cubed_unary_raw_fn build_raw;
} cubed_unary_rule_t;

enum {
    squared_unary_rule_kind_first = EXPR_PATTERN_UNARY_SIN,
    squared_unary_rule_kind_last = EXPR_PATTERN_UNARY_COTH,
    squared_unary_rule_kind_count =
        squared_unary_rule_kind_last - squared_unary_rule_kind_first + 1,
    squared_unary_op_kind_first = EXPR_KIND_SIN,
    squared_unary_op_kind_last = EXPR_KIND_COTH,
    squared_unary_op_kind_count =
        squared_unary_op_kind_last - squared_unary_op_kind_first + 1,
    cubed_unary_rule_kind_first = EXPR_PATTERN_UNARY_SIN,
    cubed_unary_rule_kind_last = EXPR_PATTERN_UNARY_COT,
    cubed_unary_rule_kind_count =
        cubed_unary_rule_kind_last - cubed_unary_rule_kind_first + 1,
    cubed_unary_op_kind_first = EXPR_KIND_SIN,
    cubed_unary_op_kind_last = EXPR_KIND_COT,
    cubed_unary_op_kind_count =
        cubed_unary_op_kind_last - cubed_unary_op_kind_first + 1
};

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

static expr_t *build_cot_squared_raw(const expr_t *u)
{
    expr_t *cot_u = build_neg_unary_raw(u, expr_cot);
    expr_t *raw = (cot_u && u) ? expr_sub(cot_u, u) : NULL;

    expr_free(cot_u);
    return raw;
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

static expr_t *build_cot_cubed_raw(const expr_t *u)
{
    expr_t *cot_u = expr_cot(u);
    expr_t *cot_sq = cot_u ? expr_pow(cot_u, &NUM_TWO) : NULL;
    expr_t *half_cot_sq = cot_sq ? expr_mul_num(cot_sq, &NUM_HALF) : NULL;
    expr_t *neg_half_cot_sq = half_cot_sq ? expr_neg(half_cot_sq) : NULL;
    expr_t *sin_u = expr_sin(u);
    expr_t *log_sin = sin_u ? expr_log(sin_u) : NULL;
    expr_t *raw = (neg_half_cot_sq && log_sin)
                      ? expr_sub(neg_half_cot_sq, log_sin)
                      : NULL;

    expr_free(log_sin);
    expr_free(sin_u);
    expr_free(neg_half_cot_sq);
    expr_free(half_cot_sq);
    expr_free(cot_sq);
    expr_free(cot_u);
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

static const squared_unary_rule_t squared_unary_rules[] = {
    [EXPR_PATTERN_UNARY_SIN - squared_unary_rule_kind_first] =
        { EXPR_KIND_SIN, build_sin_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COS - squared_unary_rule_kind_first] =
        { EXPR_KIND_COS, build_cos_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_TAN - squared_unary_rule_kind_first] =
        { EXPR_KIND_TAN, build_tan_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SEC - squared_unary_rule_kind_first] =
        { EXPR_KIND_SEC, build_sec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COSEC - squared_unary_rule_kind_first] =
        { EXPR_KIND_COSEC, build_cosec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COT - squared_unary_rule_kind_first] =
        { EXPR_KIND_COT, build_cot_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SINH - squared_unary_rule_kind_first] =
        { EXPR_KIND_SINH, build_sinh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COSH - squared_unary_rule_kind_first] =
        { EXPR_KIND_COSH, build_cosh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COSECH - squared_unary_rule_kind_first] =
        { EXPR_KIND_COSECH, build_cosech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_TANH - squared_unary_rule_kind_first] =
        { EXPR_KIND_TANH, build_tanh_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SECH - squared_unary_rule_kind_first] =
        { EXPR_KIND_SECH, build_sech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COTH - squared_unary_rule_kind_first] =
        { EXPR_KIND_COTH, build_coth_squared_raw, 1 }
};

static const cubed_unary_rule_t cubed_unary_rules[] = {
    [EXPR_PATTERN_UNARY_SIN - cubed_unary_rule_kind_first] =
        { build_sin_cubed_raw },
    [EXPR_PATTERN_UNARY_COS - cubed_unary_rule_kind_first] =
        { build_cos_cubed_raw },
    [EXPR_PATTERN_UNARY_TAN - cubed_unary_rule_kind_first] =
        { build_tan_cubed_raw },
    [EXPR_PATTERN_UNARY_SEC - cubed_unary_rule_kind_first] =
        { build_sec_cubed_raw },
    [EXPR_PATTERN_UNARY_COSEC - cubed_unary_rule_kind_first] =
        { build_cosec_cubed_raw },
    [EXPR_PATTERN_UNARY_COT - cubed_unary_rule_kind_first] =
        { build_cot_cubed_raw }
};

_Static_assert(sizeof(squared_unary_rules) / sizeof(squared_unary_rules[0]) ==
                   squared_unary_rule_kind_count,
               "squared_unary_rules must follow the unary square kind range");
_Static_assert(sizeof(cubed_unary_rules) / sizeof(cubed_unary_rules[0]) ==
                   cubed_unary_rule_kind_count,
               "cubed_unary_rules must follow the unary cube kind range");

static const squared_unary_rule_t *find_squared_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const squared_unary_rule_t *rule;

    if ((unsigned)kind < (unsigned)squared_unary_rule_kind_first ||
        (unsigned)kind > (unsigned)squared_unary_rule_kind_last) {
        return NULL;
    }
    rule = &squared_unary_rules[kind - squared_unary_rule_kind_first];
    return rule->build_raw ? rule : NULL;
}

static const cubed_unary_rule_t *find_cubed_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const cubed_unary_rule_t *rule;

    if ((unsigned)kind < (unsigned)cubed_unary_rule_kind_first ||
        (unsigned)kind > (unsigned)cubed_unary_rule_kind_last) {
        return NULL;
    }
    rule = &cubed_unary_rules[kind - cubed_unary_rule_kind_first];
    return rule->build_raw ? rule : NULL;
}

static bool squared_unary_kind_from_op(expr_op_kind_t op_kind,
                                       expr_pattern_unary_affine_kind_t *kind_out)
{
    static const expr_pattern_unary_affine_kind_t kinds[] = {
        [EXPR_KIND_SIN - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_SIN,
        [EXPR_KIND_COS - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COS,
        [EXPR_KIND_TAN - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_TAN,
        [EXPR_KIND_SEC - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_SEC,
        [EXPR_KIND_COSEC - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COSEC,
        [EXPR_KIND_COT - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COT,
        [EXPR_KIND_SINH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_SINH,
        [EXPR_KIND_COSH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COSH,
        [EXPR_KIND_TANH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_TANH,
        [EXPR_KIND_SECH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_SECH,
        [EXPR_KIND_COSECH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COSECH,
        [EXPR_KIND_COTH - squared_unary_op_kind_first] = EXPR_PATTERN_UNARY_COTH
    };

    if (!kind_out ||
        (unsigned)op_kind < (unsigned)squared_unary_op_kind_first ||
        (unsigned)op_kind > (unsigned)squared_unary_op_kind_last) {
        return false;
    }

    *kind_out = kinds[op_kind - squared_unary_op_kind_first];
    return find_squared_unary_rule(*kind_out) != NULL;
}

static bool cubed_unary_kind_from_op(expr_op_kind_t op_kind,
                                     expr_pattern_unary_affine_kind_t *kind_out)
{
    static const expr_pattern_unary_affine_kind_t kinds[] = {
        [EXPR_KIND_SIN - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_SIN,
        [EXPR_KIND_COS - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_COS,
        [EXPR_KIND_TAN - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_TAN,
        [EXPR_KIND_SEC - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_SEC,
        [EXPR_KIND_COSEC - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_COSEC,
        [EXPR_KIND_COT - cubed_unary_op_kind_first] = EXPR_PATTERN_UNARY_COT
    };

    if (!kind_out ||
        (unsigned)op_kind < (unsigned)cubed_unary_op_kind_first ||
        (unsigned)op_kind > (unsigned)cubed_unary_op_kind_last) {
        return false;
    }

    *kind_out = kinds[op_kind - cubed_unary_op_kind_first];
    return find_cubed_unary_rule(*kind_out) != NULL;
}

static bool match_symbolic_squared_unary_base(const expr_t *expr,
                                              expr_pattern_unary_affine_kind_t kind,
                                              const expr_t *wrt,
                                              const expr_t **base_out,
                                              expr_t **coeff_out)
{
    const squared_unary_rule_t *rule = find_squared_unary_rule(kind);
    number_t exponent = num_new();
    const expr_t *base = NULL;
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !base_out || !coeff_out)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        if (num_eq(expr->c, NUM_TWO))
            base = expr->a;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) &&
               num_eq(exponent, NUM_TWO)) {
        base = expr->a;
    }

    if (!base ||
        !base->ops ||
        !base->a ||
        !rule ||
        base->ops->kind != rule->op_kind ||
        !match_symbolic_affine_constant_and_coeff(base->a, wrt, &constant, &coeff)) {
        goto cleanup;
    }

    *base_out = base;
    *coeff_out = coeff;
    coeff = NULL;
    ok = true;

cleanup:
    expr_free(coeff);
    expr_free(constant);
    num_destroy(&exponent);
    return ok;
}

static expr_t *divide_owned_by_symbolic_factor(expr_t *numerator,
                                               expr_t *factor)
{
    expr_t *quotient = (numerator && factor) ? expr_div(numerator, factor) : NULL;

    expr_free(factor);
    expr_free(numerator);
    return simplify_owned(quotient);
}

static expr_t *integrate_symbolic_squared_unary_affine(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind,
    const squared_unary_rule_t *rule)
{
    const expr_t *base = NULL;
    expr_t *coeff = NULL;
    expr_t *u = NULL;
    expr_t *raw = NULL;
    expr_t *denom = NULL;

    if (!rule || !match_symbolic_squared_unary_base(expr, kind, wrt, &base, &coeff))
        return NULL;

    u = expr_clone(base->a);
    raw = u ? rule->build_raw(u) : NULL;
    if (rule->divisor_factor == 1) {
        denom = coeff;
        coeff = NULL;
    } else {
        expr_t *factor = expr_const_long(rule->divisor_factor);

        denom = (factor && coeff) ? expr_mul(factor, coeff) : NULL;
        expr_free(factor);
        expr_free(coeff);
        coeff = NULL;
    }

    expr_free(u);
    return divide_owned_by_symbolic_factor(raw, denom);
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
        return integrate_symbolic_squared_unary_affine(expr, wrt, kind, rule);
    }

    u = build_affine_from_match(wrt, constant, coeff);
    raw = u ? rule->build_raw(u) : NULL;
    expr_free(u);
    num_destroy(&constant);
    if (rule->divisor_factor == 1)
        return div_number_owned_consuming(raw, &coeff);
    raw = div_number_owned_by_long_product(raw, rule->divisor_factor, coeff);
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

static bool expr_affine_parts_match_double(const expr_t *single_constant,
                                           const expr_t *single_coeff,
                                           const expr_t *double_constant,
                                           const expr_t *double_coeff)
{
    expr_t *twice_constant = single_constant ? expr_mul_num(single_constant, &NUM_TWO) : NULL;
    expr_t *twice_coeff = single_coeff ? expr_mul_num(single_coeff, &NUM_TWO) : NULL;
    bool ok = twice_constant && twice_coeff &&
              expr_equal_exact_local(twice_constant, double_constant) &&
              expr_equal_exact_local(twice_coeff, double_coeff);

    expr_free(twice_coeff);
    expr_free(twice_constant);
    return ok;
}

static bool match_tan_plus_cot_same_arg(const expr_t *expr,
                                        const expr_t **arg_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *tan_expr = NULL;
    const expr_t *cot_expr = NULL;

    if (!expr || !arg_out || !expr_is_op(expr, &ops_add) || !expr->a || !expr->b)
        return false;
    left = expr->a;
    right = expr->b;

    if (expr_is_op(left, &ops_tan) && expr_is_op(right, &ops_cot)) {
        tan_expr = left;
        cot_expr = right;
    } else if (expr_is_op(left, &ops_cot) && expr_is_op(right, &ops_tan)) {
        tan_expr = right;
        cot_expr = left;
    } else {
        return false;
    }

    if (!tan_expr->a || !cot_expr->a ||
        !expr_equal_exact_local(tan_expr->a, cot_expr->a)) {
        return false;
    }

    *arg_out = tan_expr->a;
    return true;
}

static bool match_sec_log_tan_cot_product(const expr_t *expr,
                                          const expr_t **sec_expr_out,
                                          const expr_t **log_expr_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !sec_expr_out || !log_expr_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_is_op(left, &ops_sec) && expr_is_op(right, &ops_log)) {
        *sec_expr_out = left;
        *log_expr_out = right;
        return true;
    }

    if (expr_is_op(right, &ops_sec) && expr_is_op(left, &ops_log)) {
        *sec_expr_out = right;
        *log_expr_out = left;
        return true;
    }

    return false;
}

static bool match_sec_squared_log_tan_cot_product(const expr_t *expr,
                                                  const expr_t **sec_expr_out,
                                                  const expr_t **log_expr_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *sec_sq = NULL;
    const expr_t *log_expr = NULL;
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !sec_expr_out || !log_expr_out ||
        !expr_match_mul_expr(expr, &left, &right)) {
        goto cleanup;
    }

    if (expr_is_op(left, &ops_log)) {
        log_expr = left;
        sec_sq = right;
    } else if (expr_is_op(right, &ops_log)) {
        log_expr = right;
        sec_sq = left;
    } else {
        goto cleanup;
    }

    if (sec_sq && sec_sq->ops && sec_sq->ops->kind == EXPR_KIND_POW_D &&
        sec_sq->a && expr_is_op(sec_sq->a, &ops_sec) &&
        num_eq(sec_sq->c, NUM_TWO)) {
        *sec_expr_out = sec_sq->a;
        *log_expr_out = log_expr;
        ok = true;
        goto cleanup;
    }

    if (sec_sq && sec_sq->ops && sec_sq->ops->kind == EXPR_KIND_POW &&
        sec_sq->a && sec_sq->b &&
        expr_is_op(sec_sq->a, &ops_sec) &&
        expr_match_const_value(sec_sq->b, &exponent) &&
        num_eq(exponent, NUM_TWO)) {
        *sec_expr_out = sec_sq->a;
        *log_expr_out = log_expr;
        ok = true;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *build_tan_pi_over_four_minus(const expr_t *u)
{
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *pi_over_four = pi ? expr_div_long(pi, 4) : NULL;
    expr_t *arg = (pi_over_four && u) ? expr_sub(pi_over_four, u) : NULL;
    expr_t *out = arg ? expr_tan(arg) : NULL;

    expr_free(arg);
    expr_free(pi_over_four);
    expr_free(pi);
    return out;
}

static expr_t *build_sec_double_angle_log_tan_cot_raw(const expr_t *u)
{
    number_t neg_half = num_neg(NUM_HALF);
    expr_t *y = build_tan_pi_over_four_minus(u);
    expr_t *y_sq = y ? expr_pow(y, &NUM_TWO) : NULL;
    expr_t *chi = y_sq ? expr_legendre_chi(2u, y_sq) : NULL;
    expr_t *chi_part = chi ? mul_number_owned(chi, neg_half) : NULL;
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *log_two = two ? expr_log(two) : NULL;
    expr_t *log_y = y ? expr_log(y) : NULL;
    expr_t *log_product = (log_two && log_y) ? expr_mul(log_two, log_y) : NULL;
    expr_t *log_part = log_product ? mul_number_owned(log_product, NUM_HALF) : NULL;
    expr_t *raw = (chi_part && log_part) ? expr_sub(chi_part, log_part) : NULL;

    chi = NULL;
    log_product = NULL;
    expr_free(log_part);
    expr_free(log_product);
    expr_free(log_y);
    expr_free(log_two);
    expr_free(two);
    expr_free(chi_part);
    expr_free(chi);
    expr_free(y_sq);
    expr_free(y);
    num_destroy(&neg_half);
    return raw;
}

static expr_t *build_sec_squared_log_tan_cot_raw(const expr_t *u)
{
    expr_t *tan_u = u ? expr_tan(u) : NULL;
    expr_t *cot_u = u ? expr_cot(u) : NULL;
    expr_t *sum = (tan_u && cot_u) ? expr_add(tan_u, cot_u) : NULL;
    expr_t *log_sum = sum ? expr_log(sum) : NULL;
    expr_t *product = (tan_u && log_sum) ? expr_mul(tan_u, log_sum) : NULL;
    expr_t *minus_tan = tan_u ? expr_neg(tan_u) : NULL;
    expr_t *two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
    expr_t *first_sum = (product && minus_tan) ? expr_add(product, minus_tan) : NULL;
    expr_t *raw = (first_sum && two_u) ? expr_add(first_sum, two_u) : NULL;

    expr_free(first_sum);
    expr_free(two_u);
    expr_free(minus_tan);
    expr_free(product);
    expr_free(log_sum);
    expr_free(sum);
    expr_free(cot_u);
    expr_free(tan_u);
    return raw;
}

expr_t *integrate_sec_squared_log_tan_cot(const expr_t *expr,
                                          const expr_t *wrt)
{
    const expr_t *sec_expr = NULL;
    const expr_t *log_expr = NULL;
    const expr_t *tan_arg = NULL;
    expr_t *tan_constant = NULL;
    expr_t *tan_coeff = NULL;
    expr_t *sec_constant = NULL;
    expr_t *sec_coeff = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt ||
        !match_sec_squared_log_tan_cot_product(expr, &sec_expr, &log_expr) ||
        !sec_expr->a || !log_expr->a ||
        !match_tan_plus_cot_same_arg(log_expr->a, &tan_arg) ||
        !match_symbolic_affine_constant_and_coeff(tan_arg, wrt, &tan_constant, &tan_coeff) ||
        !match_symbolic_affine_constant_and_coeff(sec_expr->a, wrt, &sec_constant, &sec_coeff) ||
        expr_const_is_zero(tan_coeff) ||
        !expr_equal_exact_local(tan_constant, sec_constant) ||
        !expr_equal_exact_local(tan_coeff, sec_coeff)) {
        goto cleanup;
    }

    raw = build_sec_squared_log_tan_cot_raw(tan_arg);
    out = raw ? expr_div(raw, tan_coeff) : NULL;
    expr_free(raw);
    raw = NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(raw);
    expr_free(sec_coeff);
    expr_free(sec_constant);
    expr_free(tan_coeff);
    expr_free(tan_constant);
    return out;
}

expr_t *integrate_sec_double_angle_log_tan_cot(const expr_t *expr,
                                               const expr_t *wrt)
{
    const expr_t *sec_expr = NULL;
    const expr_t *log_expr = NULL;
    const expr_t *tan_arg = NULL;
    expr_t *tan_constant = NULL;
    expr_t *tan_coeff = NULL;
    expr_t *sec_constant = NULL;
    expr_t *sec_coeff = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt ||
        !match_sec_log_tan_cot_product(expr, &sec_expr, &log_expr) ||
        !sec_expr->a || !log_expr->a ||
        !match_tan_plus_cot_same_arg(log_expr->a, &tan_arg) ||
        !match_symbolic_affine_constant_and_coeff(tan_arg, wrt, &tan_constant, &tan_coeff) ||
        !match_symbolic_affine_constant_and_coeff(sec_expr->a, wrt, &sec_constant, &sec_coeff) ||
        expr_const_is_zero(tan_coeff) ||
        !expr_affine_parts_match_double(tan_constant, tan_coeff, sec_constant, sec_coeff)) {
        goto cleanup;
    }

    raw = build_sec_double_angle_log_tan_cot_raw(tan_arg);
    out = raw ? expr_div(raw, tan_coeff) : NULL;
    expr_free(raw);
    raw = NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(raw);
    expr_free(sec_coeff);
    expr_free(sec_constant);
    expr_free(tan_coeff);
    expr_free(tan_constant);
    return out;
}

static bool match_wrt_trig_coeff(const expr_t *expr,
                                 const expr_t *wrt,
                                 bool want_sin,
                                 long want_coeff)
{
    bool is_sin = false;
    expr_t *coeff_expr = NULL;
    number_t coeff = num_new();
    number_t expected = num_create_from_long(want_coeff);
    bool ok = false;

    if (!match_trig_proportional_wrt_coeff(expr, wrt, &is_sin, &coeff_expr))
        goto cleanup;
    if (is_sin != want_sin || !expr_match_const_value(coeff_expr, &coeff))
        goto cleanup;
    ok = num_eq(coeff, expected);

cleanup:
    expr_free(coeff_expr);
    num_destroy(&expected);
    num_destroy(&coeff);
    return ok;
}

static bool match_sum_of_sin_cos_coeffs(const expr_t *expr,
                                        const expr_t *wrt,
                                        long sin_coeff,
                                        long cos_coeff)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !expr_is_op(expr, &ops_add))
        return false;
    left = expr->a;
    right = expr->b;
    return (match_wrt_trig_coeff(left, wrt, true, sin_coeff) &&
            match_wrt_trig_coeff(right, wrt, false, cos_coeff)) ||
           (match_wrt_trig_coeff(left, wrt, false, cos_coeff) &&
            match_wrt_trig_coeff(right, wrt, true, sin_coeff));
}

static bool match_sqrt_sin_cos_factor(const expr_t *expr,
                                      const expr_t *wrt,
                                      long sin_coeff,
                                      long cos_coeff)
{
    return expr && expr_is_op(expr, &ops_sqrt) &&
           match_sum_of_sin_cos_coeffs(expr->a, wrt, sin_coeff, cos_coeff);
}

static bool match_sqrt_sin_cos_sin3_cos_denominator(const expr_t *expr,
                                                    const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr_match_mul_expr(expr, &left, &right))
        return false;
    return (match_sqrt_sin_cos_factor(left, wrt, 1, 1) &&
            match_sqrt_sin_cos_factor(right, wrt, 3, 1)) ||
           (match_sqrt_sin_cos_factor(left, wrt, 3, 1) &&
            match_sqrt_sin_cos_factor(right, wrt, 1, 1));
}

static expr_t *build_sin_cos_sin3_cos_double_radicand(const expr_t *wrt)
{
    number_t three = num_create_from_long(3);
    expr_t *sin_x = NULL;
    expr_t *cos_x = NULL;
    expr_t *three_x = NULL;
    expr_t *sin_3x = NULL;
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *product = NULL;
    expr_t *doubled = NULL;
    expr_t *out = NULL;

    if (!wrt)
        goto cleanup;

    sin_x = expr_sin(wrt);
    cos_x = expr_cos(wrt);
    three_x = expr_mul_num(wrt, &three);
    sin_3x = three_x ? expr_sin(three_x) : NULL;
    first = (sin_x && cos_x) ? expr_add(sin_x, cos_x) : NULL;
    second = (sin_3x && cos_x) ? expr_add(sin_3x, cos_x) : NULL;
    product = (first && second) ? expr_mul(first, second) : NULL;
    doubled = product ? expr_mul_num(product, &NUM_TWO) : NULL;
    out = simplify_owned(doubled);
    doubled = NULL;

cleanup:
    expr_free(doubled);
    expr_free(product);
    expr_free(second);
    expr_free(first);
    expr_free(sin_3x);
    expr_free(three_x);
    expr_free(cos_x);
    expr_free(sin_x);
    num_destroy(&three);
    return out;
}

static bool match_sqrt_sin_cos_sin3_cos_simplified_denominator(const expr_t *expr,
                                                               const expr_t *wrt)
{
    expr_t *expected = NULL;
    bool ok = false;

    if (!expr || !expr_is_op(expr, &ops_sqrt) || !expr->a)
        return false;

    expected = build_sin_cos_sin3_cos_double_radicand(wrt);
    ok = expected && expr_struct_eq(expr->a, expected);
    expr_free(expected);
    return ok;
}

static bool match_sqrt_sin_cos_sin3_cos_scaled_denominator(const expr_t *expr,
                                                           const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t scale = num_new();
    bool ok = false;

    if (!expr_match_mul_expr(expr, &left, &right))
        goto cleanup;

    if (expr_match_const_value(left, &scale) &&
        num_eq(scale, NUM_SQRT_HALF) &&
        match_sqrt_sin_cos_sin3_cos_simplified_denominator(right, wrt)) {
        ok = true;
        goto cleanup;
    }

    if (expr_match_const_value(right, &scale) &&
        num_eq(scale, NUM_SQRT_HALF) &&
        match_sqrt_sin_cos_sin3_cos_simplified_denominator(left, wrt)) {
        ok = true;
        goto cleanup;
    }

cleanup:
    num_destroy(&scale);
    return ok;
}

static expr_t *build_inverse_sqrt_sin_cos_sin3_cos_raw(const expr_t *wrt)
{
    expr_t *tan_x = NULL;
    expr_t *one = NULL;
    expr_t *tan_minus_one = NULL;
    expr_t *scaled_tan_minus_one = NULL;
    expr_t *tan_sq = NULL;
    expr_t *neg_tan_sq = NULL;
    expr_t *two_tan = NULL;
    expr_t *quad_part = NULL;
    expr_t *quad = NULL;
    expr_t *quad_root = NULL;
    expr_t *sqrt_two = NULL;
    expr_t *denom = NULL;
    expr_t *fraction = NULL;
    expr_t *atan_arg = NULL;
    expr_t *atan_term = NULL;
    expr_t *out = NULL;

    if (!wrt)
        return NULL;

    tan_x = expr_tan(wrt);
    one = expr_new_const(NUM_ONE);
    tan_minus_one = (tan_x && one) ? expr_sub(tan_x, one) : NULL;
    scaled_tan_minus_one = tan_minus_one ? expr_mul_num(tan_minus_one, &NUM_SQRT2) : NULL;
    tan_sq = tan_x ? expr_pow(tan_x, &NUM_TWO) : NULL;
    neg_tan_sq = tan_sq ? expr_neg(tan_sq) : NULL;
    two_tan = tan_x ? expr_mul_num(tan_x, &NUM_TWO) : NULL;
    quad_part = (neg_tan_sq && two_tan) ? expr_add(neg_tan_sq, two_tan) : NULL;
    quad = (quad_part && one) ? expr_add(quad_part, one) : NULL;
    quad_root = quad ? expr_sqrt(quad) : NULL;
    sqrt_two = expr_new_const(NUM_SQRT2);
    denom = (sqrt_two && quad_root) ? expr_add(sqrt_two, quad_root) : NULL;
    fraction = (scaled_tan_minus_one && denom) ? expr_div(scaled_tan_minus_one, denom) : NULL;
    atan_arg = (one && fraction) ? expr_add(one, fraction) : NULL;
    atan_term = atan_arg ? expr_atan(atan_arg) : NULL;
    out = atan_term ? expr_mul_num(atan_term, &NUM_SQRT2) : NULL;

    expr_free(atan_term);
    expr_free(atan_arg);
    expr_free(fraction);
    expr_free(denom);
    expr_free(sqrt_two);
    expr_free(quad_root);
    expr_free(quad);
    expr_free(quad_part);
    expr_free(two_tan);
    expr_free(neg_tan_sq);
    expr_free(tan_sq);
    expr_free(scaled_tan_minus_one);
    expr_free(tan_minus_one);
    expr_free(one);
    expr_free(tan_x);
    return out;
}

expr_t *integrate_inverse_sqrt_sin_cos_sin3_cos(const expr_t *expr,
                                                const expr_t *wrt)
{
    number_t numerator = num_new();
    expr_t *raw = NULL;
    expr_t *out = NULL;
    bool matched = false;

    if (!expr || !wrt || !expr_is_div(expr) ||
        !expr_match_const_value(expr->a, &numerator))
        goto cleanup;

    if (num_eq(numerator, NUM_ONE))
        matched = match_sqrt_sin_cos_sin3_cos_denominator(expr->b, wrt) ||
                  match_sqrt_sin_cos_sin3_cos_scaled_denominator(expr->b, wrt);
    else if (num_eq(numerator, NUM_SQRT2))
        matched = match_sqrt_sin_cos_sin3_cos_simplified_denominator(expr->b, wrt);

    if (!matched)
        goto cleanup;

    raw = build_inverse_sqrt_sin_cos_sin3_cos_raw(wrt);
    out = simplify_owned(raw);
    raw = NULL;

cleanup:
    expr_free(raw);
    num_destroy(&numerator);
    return out;
}

static expr_t *build_scaled_quartic_minus_one(const expr_t *wrt, long scale)
{
    number_t four = num_create_from_long(4);
    number_t scale_num = num_create_from_long(scale);
    expr_t *x4 = NULL;
    expr_t *scaled = NULL;
    expr_t *one = NULL;
    expr_t *diff = NULL;
    expr_t *out = NULL;

    if (!wrt)
        goto cleanup;

    x4 = expr_pow(wrt, &four);
    scaled = (scale == 1) ? (x4 ? expr_simplify(x4) : NULL)
                          : (x4 ? expr_mul_num(x4, &scale_num) : NULL);
    one = expr_new_const(NUM_ONE);
    diff = (scaled && one) ? expr_sub(scaled, one) : NULL;
    out = simplify_owned(diff);
    diff = NULL;

cleanup:
    expr_free(diff);
    expr_free(one);
    expr_free(scaled);
    expr_free(x4);
    num_destroy(&scale_num);
    num_destroy(&four);
    return out;
}

static expr_t *build_inverse_quartic_appell_denominator(const expr_t *wrt)
{
    number_t one = num_create_from_long(1);
    number_t eight = num_create_from_long(8);
    number_t one_eighth = num_div(one, eight);
    expr_t *first = NULL;
    expr_t *second_base = NULL;
    expr_t *second_root = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    first = build_scaled_quartic_minus_one(wrt, 1);
    second_base = build_scaled_quartic_minus_one(wrt, 2);
    second_root = second_base ? expr_pow(second_base, &one_eighth) : NULL;
    product = (first && second_root) ? expr_mul(first, second_root) : NULL;
    out = simplify_owned(product);
    product = NULL;

    expr_free(product);
    expr_free(second_root);
    expr_free(second_base);
    expr_free(first);
    num_destroy(&one_eighth);
    num_destroy(&eight);
    num_destroy(&one);
    return out;
}

static bool match_inverse_quartic_appell_denominator(const expr_t *expr,
                                                     const expr_t *wrt)
{
    expr_t *actual = expr ? expr_simplify(expr) : NULL;
    expr_t *expected = build_inverse_quartic_appell_denominator(wrt);
    bool ok = actual && expected && expr_struct_eq(actual, expected);

    expr_free(expected);
    expr_free(actual);
    return ok;
}

static expr_t *build_inverse_quartic_elementary_raw(const expr_t *wrt)
{
    number_t one = num_create_from_long(1);
    number_t two = num_create_from_long(2);
    number_t four = num_create_from_long(4);
    number_t eight = num_create_from_long(8);
    number_t one_eighth = num_div(one, eight);
    number_t one_fourth = num_div(one, four);
    number_t inv_eight = num_div(one, eight);
    number_t neg_two = num_create_from_long(-2);
    number_t neg_sqrt2 = num_neg(NUM_SQRT2);
    expr_t *base = NULL;
    expr_t *root8 = NULL;
    expr_t *root4 = NULL;
    expr_t *x2 = NULL;
    expr_t *sqrt2 = NULL;
    expr_t *sqrt2_term3 = NULL;
    expr_t *sqrt2_term4 = NULL;
    expr_t *arg1 = NULL;
    expr_t *atan1 = NULL;
    expr_t *term1 = NULL;
    expr_t *arg2 = NULL;
    expr_t *acoth2 = NULL;
    expr_t *term2 = NULL;
    expr_t *num3 = NULL;
    expr_t *sqrt2_x = NULL;
    expr_t *den3 = NULL;
    expr_t *arg3 = NULL;
    expr_t *atan3 = NULL;
    expr_t *term3 = NULL;
    expr_t *den4 = NULL;
    expr_t *arg4 = NULL;
    expr_t *atanh4 = NULL;
    expr_t *sqrt2_atanh4 = NULL;
    expr_t *term4 = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *sum = NULL;
    expr_t *raw = NULL;

    if (!wrt)
        goto cleanup;

    base = build_scaled_quartic_minus_one(wrt, 2);
    root8 = base ? expr_pow(base, &one_eighth) : NULL;
    root4 = base ? expr_pow(base, &one_fourth) : NULL;
    x2 = expr_pow(wrt, &two);
    sqrt2 = expr_new_const(NUM_SQRT2);

    arg1 = (root8 && wrt) ? expr_div(root8, wrt) : NULL;
    atan1 = arg1 ? expr_atan(arg1) : NULL;
    term1 = atan1 ? expr_mul_num(atan1, &two) : NULL;

    arg2 = (wrt && root8) ? expr_div(wrt, root8) : NULL;
    acoth2 = arg2 ? expr_acoth(arg2) : NULL;
    term2 = acoth2 ? expr_mul_num(acoth2, &neg_two) : NULL;

    num3 = (root4 && x2) ? expr_sub(root4, x2) : NULL;
    sqrt2_x = (sqrt2 && wrt) ? expr_mul(sqrt2, wrt) : NULL;
    den3 = (sqrt2_x && root8) ? expr_mul(sqrt2_x, root8) : NULL;
    arg3 = (num3 && den3) ? expr_div(num3, den3) : NULL;
    atan3 = arg3 ? expr_atan(arg3) : NULL;
    sqrt2_term3 = expr_new_const(NUM_SQRT2);
    term3 = (sqrt2_term3 && atan3) ? expr_mul(sqrt2_term3, atan3) : NULL;

    den4 = (root4 && x2) ? expr_add(root4, x2) : NULL;
    arg4 = (den3 && den4) ? expr_div(den3, den4) : NULL;
    atanh4 = arg4 ? expr_atanh(arg4) : NULL;
    sqrt2_term4 = expr_new_const(NUM_SQRT2);
    sqrt2_atanh4 = (sqrt2_term4 && atanh4) ? expr_mul(sqrt2_term4, atanh4) : NULL;
    term4 = sqrt2_atanh4 ? expr_neg(sqrt2_atanh4) : NULL;

    left = (term1 && term2) ? expr_add(term1, term2) : NULL;
    right = (term3 && term4) ? expr_add(term3, term4) : NULL;
    sum = (left && right) ? expr_add(left, right) : NULL;
    raw = sum ? expr_mul_num(sum, &inv_eight) : NULL;

cleanup:
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(term4);
    expr_free(sqrt2_atanh4);
    expr_free(atanh4);
    expr_free(arg4);
    expr_free(den4);
    expr_free(term3);
    expr_free(sqrt2_term4);
    expr_free(sqrt2_term3);
    expr_free(atan3);
    expr_free(arg3);
    expr_free(den3);
    expr_free(sqrt2_x);
    expr_free(num3);
    expr_free(term2);
    expr_free(acoth2);
    expr_free(arg2);
    expr_free(term1);
    expr_free(atan1);
    expr_free(arg1);
    expr_free(sqrt2);
    expr_free(x2);
    expr_free(root4);
    expr_free(root8);
    expr_free(base);
    num_destroy(&neg_sqrt2);
    num_destroy(&neg_two);
    num_destroy(&inv_eight);
    num_destroy(&one_fourth);
    num_destroy(&one_eighth);
    num_destroy(&eight);
    num_destroy(&four);
    num_destroy(&two);
    num_destroy(&one);
    return raw;
}

expr_t *integrate_inverse_quartic_appell_f1(const expr_t *expr,
                                            const expr_t *wrt)
{
    number_t numerator = num_new();
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr_is_div(expr) ||
        !expr_match_const_value(expr->a, &numerator) ||
        !num_eq(numerator, NUM_ONE) ||
        !match_inverse_quartic_appell_denominator(expr->b, wrt))
        goto cleanup;

    raw = build_inverse_quartic_elementary_raw(wrt);
    out = simplify_owned(raw);
    raw = NULL;

cleanup:
    expr_free(raw);
    num_destroy(&numerator);
    return out;
}

expr_t *integrate_matching_squared_unary_affine(const expr_t *expr,
                                                const expr_t *wrt)
{
    expr_pattern_unary_affine_kind_t kind = EXPR_PATTERN_UNARY_COUNT;

    if (!expr || !expr->a || !expr->a->ops ||
        !squared_unary_kind_from_op(expr->a->ops->kind, &kind)) {
        return NULL;
    }

    return integrate_squared_unary_affine(expr, wrt, kind);
}

expr_t *integrate_matching_cubed_unary_affine(const expr_t *expr,
                                              const expr_t *wrt)
{
    expr_pattern_unary_affine_kind_t kind = EXPR_PATTERN_UNARY_COUNT;

    if (!expr || !expr->a || !expr->a->ops ||
        !cubed_unary_kind_from_op(expr->a->ops->kind, &kind)) {
        return NULL;
    }

    return integrate_cubed_unary_affine(expr, wrt, kind);
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
        exponent = expr_retain_expr(expr->b);
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
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_product(out, NUM_TWO, k1);
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
        out = div_number_owned_by_product(out, NUM_TWO, k1);
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
        out = div_number_owned_by_long_product(diff, 4, k1);
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
        out = div_number_owned_by_long_product(sum, 4, k1);
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
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_long_product(sin_cubed, 3, k1);
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
        out = div_number_owned_by_long_product(neg_cos_cubed, 3, k1);
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
        out = div_number_owned_by_product(sec_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_product(neg_cosec_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_long_product(diff, 4, k1);
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
        out = div_number_owned_by_long_product(sum, 4, k1);
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
        out = div_number_owned_by_product(sinh_sq, NUM_TWO, k1);
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
        out = div_number_owned_by_long_product(diff, 4, k1);
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
        out = div_number_owned_by_long_product(sum, 4, k1);
    }

cleanup:
    num_destroy(&k2);
    num_destroy(&c2);
    num_destroy(&k1);
    num_destroy(&c1);
    return out;
}
