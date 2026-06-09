#include "expr_integrate_internal.h"

typedef expr_t *(*inverse_affine_term_builder_fn)(const expr_t *u,
                                                  expr_t *inverse_u,
                                                  const expr_t *u_sq,
                                                  number_t poly_coeff);

typedef struct {
    expr_pattern_unary_affine_kind_t kind;
    expr_apply_unary_fn antiderivative_fn;
    inverse_affine_term_builder_fn build_base_term;
    inverse_affine_term_builder_fn build_linear_term;
    inverse_affine_term_builder_fn build_quadratic_term;
} inverse_affine_rule_t;

static expr_t *combine_binary_owned(expr_t *left, expr_t *right, bool is_add)
{
    expr_t *out = NULL;

    if (left && right)
        out = is_add ? expr_add(left, right) : expr_sub(left, right);
    expr_free(right);
    expr_free(left);
    return out;
}

static expr_t *scale_owned_by_ratio(expr_t *expr, number_t numer, long denom)
{
    number_t denom_num;
    number_t scale;

    if (!expr)
        return NULL;
    denom_num = num_create_from_long(denom);
    scale = num_div(numer, denom_num);
    expr = mul_number_owned(expr, scale);
    num_destroy(&scale);
    num_destroy(&denom_num);
    return expr;
}

static expr_t *build_u_times_inverse(const expr_t *u, expr_t *inverse_u)
{
    return (u && inverse_u) ? expr_mul(u, inverse_u) : NULL;
}

static expr_t *build_sqrt_one_minus_u_sq(const expr_t *u_sq)
{
    expr_t *u_sq_minus_one = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    expr_t *negated = u_sq_minus_one ? expr_neg(u_sq_minus_one) : NULL;
    expr_t *root = negated ? expr_sqrt(negated) : NULL;

    expr_free(negated);
    expr_free(u_sq_minus_one);
    return root;
}

static expr_t *build_sqrt_u_sq_minus_one(const expr_t *u_sq)
{
    expr_t *u_sq_minus_one = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    expr_t *root = u_sq_minus_one ? expr_sqrt(u_sq_minus_one) : NULL;

    expr_free(u_sq_minus_one);
    return root;
}

static expr_t *build_sqrt_one_plus_u_sq(const expr_t *u_sq)
{
    expr_t *one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    expr_t *root = one_plus_u_sq ? expr_sqrt(one_plus_u_sq) : NULL;

    expr_free(one_plus_u_sq);
    return root;
}

static expr_t *build_half_log_one_plus_u_sq(const expr_t *u_sq)
{
    expr_t *one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    expr_t *log_term = one_plus_u_sq ? expr_log(one_plus_u_sq) : NULL;
    expr_t *half_log = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;

    expr_free(log_term);
    expr_free(one_plus_u_sq);
    return half_log;
}

static expr_t *build_half_log_one_minus_u_sq(const expr_t *u_sq)
{
    expr_t *u_sq_minus_one = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    expr_t *negated = u_sq_minus_one ? expr_neg(u_sq_minus_one) : NULL;
    expr_t *log_term = negated ? expr_log(negated) : NULL;
    expr_t *half_log = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;

    expr_free(log_term);
    expr_free(negated);
    expr_free(u_sq_minus_one);
    return half_log;
}

static expr_t *build_exp_neg_u_sq_over_sqrt_pi(const expr_t *u_sq)
{
    expr_t *neg_u_sq = u_sq ? expr_neg(u_sq) : NULL;
    expr_t *exp_term = neg_u_sq ? expr_exp(neg_u_sq) : NULL;
    expr_t *sqrt_pi = expr_new_const(NUM_SQRT_PI);
    expr_t *correction = (exp_term && sqrt_pi) ? expr_div(exp_term, sqrt_pi) : NULL;

    expr_free(sqrt_pi);
    expr_free(exp_term);
    expr_free(neg_u_sq);
    return correction;
}

static expr_t *build_base_with_correction(const expr_t *u,
                                          expr_t *inverse_u,
                                          number_t poly_coeff,
                                          expr_t *correction,
                                          bool is_add)
{
    expr_t *u_inverse = build_u_times_inverse(u, inverse_u);
    expr_t *term = combine_binary_owned(u_inverse, correction, is_add);

    return term ? mul_number_owned(term, poly_coeff) : NULL;
}

static expr_t *build_base_asin_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_one_minus_u_sq(u_sq), true);
}

static expr_t *build_base_acos_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_one_minus_u_sq(u_sq), false);
}

static expr_t *build_base_atan_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_half_log_one_plus_u_sq(u_sq), false);
}

static expr_t *build_base_asec_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_u_sq_minus_one(u_sq), false);
}

static expr_t *build_base_acosec_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_u_sq_minus_one(u_sq), true);
}

static expr_t *build_base_acot_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_half_log_one_plus_u_sq(u_sq), true);
}

static expr_t *build_base_asinh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_one_plus_u_sq(u_sq), false);
}

static expr_t *build_base_acosh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_u_sq_minus_one(u_sq), false);
}

static expr_t *build_base_atanh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_half_log_one_minus_u_sq(u_sq), true);
}

static expr_t *build_base_asech_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_one_minus_u_sq(u_sq), true);
}

static expr_t *build_base_acosech_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_sqrt_one_plus_u_sq(u_sq), true);
}

static expr_t *build_base_acoth_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_half_log_one_minus_u_sq(u_sq), true);
}

static expr_t *build_base_erf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_exp_neg_u_sq_over_sqrt_pi(u_sq), true);
}

static expr_t *build_base_erfc_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_base_with_correction(u, inverse_u, poly_coeff, build_exp_neg_u_sq_over_sqrt_pi(u_sq), false);
}

static expr_t *build_base_normal_pdf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    (void)u;
    (void)u_sq;
    return inverse_u ? mul_number_owned(expr_integrate_retain_expr(inverse_u), poly_coeff) : NULL;
}

static expr_t *build_base_normal_cdf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *u_cdf = build_u_times_inverse(u, inverse_u);
    expr_t *phi = u ? expr_normal_pdf(u) : NULL;
    expr_t *term = combine_binary_owned(u_cdf, phi, true);

    (void)u_sq;
    return term ? mul_number_owned(term, poly_coeff) : NULL;
}

static expr_t *build_base_ei_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *exp_u = u ? expr_exp(u) : NULL;
    expr_t *term = build_base_with_correction(u, inverse_u, poly_coeff, exp_u, false);

    (void)u_sq;
    return term;
}

static expr_t *build_base_e1_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *neg_u = u ? expr_neg(u) : NULL;
    expr_t *exp_neg_u = neg_u ? expr_exp(neg_u) : NULL;
    expr_t *term = build_base_with_correction(u, inverse_u, poly_coeff, exp_neg_u, false);

    expr_free(neg_u);
    (void)u_sq;
    return term;
}

static expr_t *build_linear_quarter_asin_acos_term(const expr_t *u,
                                                   expr_t *inverse_u,
                                                   const expr_t *u_sq,
                                                   number_t poly_coeff,
                                                   bool is_add)
{
    expr_t *twice_u_sq = u_sq ? expr_mul_num(u_sq, &NUM_TWO) : NULL;
    expr_t *two_u_sq_minus_one = twice_u_sq ? expr_sub_num(twice_u_sq, &NUM_ONE) : NULL;
    expr_t *inverse_part = (two_u_sq_minus_one && inverse_u) ? expr_mul(two_u_sq_minus_one, inverse_u) : NULL;
    expr_t *root = build_sqrt_one_minus_u_sq(u_sq);
    expr_t *root_part = (u && root) ? expr_mul(u, root) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, root_part, is_add);

    expr_free(root);
    expr_free(twice_u_sq);
    expr_free(two_u_sq_minus_one);
    return scale_owned_by_ratio(term, poly_coeff, 4);
}

static expr_t *build_linear_half_shifted_inverse_term(const expr_t *u,
                                                      expr_t *inverse_u,
                                                      const expr_t *u_sq,
                                                      number_t poly_coeff,
                                                      number_t constant,
                                                      bool add_u)
{
    expr_t *shifted = u_sq ? expr_add_num(u_sq, &constant) : NULL;
    expr_t *inverse_part = (shifted && inverse_u) ? expr_mul(shifted, inverse_u) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, u ? expr_integrate_retain_expr(u) : NULL, add_u);

    expr_free(shifted);
    return scale_owned_by_ratio(term, poly_coeff, 2);
}

static expr_t *build_linear_half_root_inverse_term(const expr_t *u,
                                                   expr_t *inverse_u,
                                                   const expr_t *u_sq,
                                                   number_t poly_coeff,
                                                   expr_t *root,
                                                   bool add_root,
                                                   bool subtract_one_from_u_sq)
{
    expr_t *leading = subtract_one_from_u_sq ? expr_sub_num(u_sq, &NUM_ONE) : expr_integrate_retain_expr(u_sq);
    expr_t *inverse_part = (leading && inverse_u) ? expr_mul(leading, inverse_u) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, root, add_root);

    expr_free(leading);
    (void)u;
    return scale_owned_by_ratio(term, poly_coeff, 2);
}

static expr_t *build_linear_asinh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *twice_u_sq = u_sq ? expr_mul_num(u_sq, &NUM_TWO) : NULL;
    expr_t *two_u_sq_plus_one = twice_u_sq ? expr_add_num(twice_u_sq, &NUM_ONE) : NULL;
    expr_t *inverse_part = (two_u_sq_plus_one && inverse_u) ? expr_mul(two_u_sq_plus_one, inverse_u) : NULL;
    expr_t *root = build_sqrt_one_plus_u_sq(u_sq);
    expr_t *root_part = (u && root) ? expr_mul(u, root) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, root_part, false);

    expr_free(root);
    expr_free(twice_u_sq);
    expr_free(two_u_sq_plus_one);
    return scale_owned_by_ratio(term, poly_coeff, 4);
}

static expr_t *build_linear_acosh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *twice_u_sq = u_sq ? expr_mul_num(u_sq, &NUM_TWO) : NULL;
    expr_t *two_u_sq_minus_one = twice_u_sq ? expr_sub_num(twice_u_sq, &NUM_ONE) : NULL;
    expr_t *inverse_part = (two_u_sq_minus_one && inverse_u) ? expr_mul(two_u_sq_minus_one, inverse_u) : NULL;
    expr_t *root = build_sqrt_u_sq_minus_one(u_sq);
    expr_t *root_part = (u && root) ? expr_mul(u, root) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, root_part, false);

    expr_free(root);
    expr_free(twice_u_sq);
    expr_free(two_u_sq_minus_one);
    return scale_owned_by_ratio(term, poly_coeff, 4);
}

static expr_t *build_linear_erf_erfc_term(const expr_t *u,
                                          expr_t *inverse_u,
                                          const expr_t *u_sq,
                                          number_t poly_coeff,
                                          bool is_add)
{
    expr_t *twice_u_sq = u_sq ? expr_mul_num(u_sq, &NUM_TWO) : NULL;
    expr_t *two_u_sq_minus_one = twice_u_sq ? expr_sub_num(twice_u_sq, &NUM_ONE) : NULL;
    expr_t *inverse_part = (two_u_sq_minus_one && inverse_u) ? expr_mul(two_u_sq_minus_one, inverse_u) : NULL;
    expr_t *neg_u_sq = u_sq ? expr_neg(u_sq) : NULL;
    expr_t *exp_term = neg_u_sq ? expr_exp(neg_u_sq) : NULL;
    expr_t *u_exp_term = (u && exp_term) ? expr_mul(u, exp_term) : NULL;
    expr_t *twice_u_exp_term = u_exp_term ? expr_mul_num(u_exp_term, &NUM_TWO) : NULL;
    expr_t *sqrt_pi = expr_new_const(NUM_SQRT_PI);
    expr_t *correction = (twice_u_exp_term && sqrt_pi) ? expr_div(twice_u_exp_term, sqrt_pi) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, correction, is_add);

    expr_free(sqrt_pi);
    expr_free(twice_u_exp_term);
    expr_free(u_exp_term);
    expr_free(exp_term);
    expr_free(neg_u_sq);
    expr_free(two_u_sq_minus_one);
    expr_free(twice_u_sq);
    return scale_owned_by_ratio(term, poly_coeff, 4);
}

static expr_t *build_linear_normal_pdf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *phi = u ? expr_normal_pdf(u) : NULL;
    expr_t *term = phi ? expr_neg(phi) : NULL;
    expr_t *out = term ? mul_number_owned(term, poly_coeff) : NULL;

    (void)inverse_u;
    (void)u_sq;
    expr_free(phi);
    return out;
}

static expr_t *build_linear_erf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_erf_erfc_term(u, inverse_u, u_sq, poly_coeff, true);
}

static expr_t *build_linear_erfc_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_erf_erfc_term(u, inverse_u, u_sq, poly_coeff, false);
}

static expr_t *build_linear_normal_cdf_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *u_sq_minus_one = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    expr_t *inverse_part = (u_sq_minus_one && inverse_u) ? expr_mul(u_sq_minus_one, inverse_u) : NULL;
    expr_t *phi = u ? expr_normal_pdf(u) : NULL;
    expr_t *u_phi = (u && phi) ? expr_mul(u, phi) : NULL;
    expr_t *term = combine_binary_owned(inverse_part, u_phi, true);

    expr_free(u_sq_minus_one);
    expr_free(phi);
    return scale_owned_by_ratio(term, poly_coeff, 2);
}

static expr_t *build_linear_ei_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *u_sq_inverse = (u_sq && inverse_u) ? expr_mul(u_sq, inverse_u) : NULL;
    expr_t *exp_u = u ? expr_exp(u) : NULL;
    expr_t *u_exp_u = (u && exp_u) ? expr_mul(u, exp_u) : NULL;
    expr_t *correction = combine_binary_owned(u_exp_u, exp_u ? expr_integrate_retain_expr(exp_u) : NULL, false);
    expr_t *term = combine_binary_owned(u_sq_inverse, correction, false);

    expr_free(exp_u);
    return scale_owned_by_ratio(term, poly_coeff, 2);
}

static expr_t *build_linear_e1_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    expr_t *u_sq_inverse = (u_sq && inverse_u) ? expr_mul(u_sq, inverse_u) : NULL;
    expr_t *neg_u = u ? expr_neg(u) : NULL;
    expr_t *exp_neg_u = neg_u ? expr_exp(neg_u) : NULL;
    expr_t *u_plus_one = u ? expr_add_num(u, &NUM_ONE) : NULL;
    expr_t *correction = (u_plus_one && exp_neg_u) ? expr_mul(u_plus_one, exp_neg_u) : NULL;
    expr_t *term = combine_binary_owned(u_sq_inverse, correction, false);

    expr_free(u_plus_one);
    expr_free(exp_neg_u);
    expr_free(neg_u);
    return scale_owned_by_ratio(term, poly_coeff, 2);
}

static expr_t *build_quadratic_atan_acot_term(const expr_t *u,
                                              expr_t *inverse_u,
                                              const expr_t *u_sq,
                                              number_t poly_coeff,
                                              bool is_atan)
{
    expr_t *u_cu = (u && u_sq) ? expr_mul(u_sq, u) : NULL;
    expr_t *u_cu_inverse = (u_cu && inverse_u) ? expr_mul(u_cu, inverse_u) : NULL;
    expr_t *one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    expr_t *log_term = one_plus_u_sq ? expr_log(one_plus_u_sq) : NULL;
    expr_t *first = u_cu_inverse ? expr_mul_num(u_cu_inverse, &NUM_ONE_THIRD) : NULL;
    expr_t *second = u_sq ? expr_mul_num(u_sq, &NUM_ONE_SIXTH) : NULL;
    expr_t *third = log_term ? expr_mul_num(log_term, &NUM_ONE_SIXTH) : NULL;
    expr_t *partial = combine_binary_owned(first, second, !is_atan);
    expr_t *term = combine_binary_owned(partial, third, is_atan);

    expr_free(log_term);
    expr_free(one_plus_u_sq);
    expr_free(u_cu_inverse);
    expr_free(u_cu);
    return term ? mul_number_owned(term, poly_coeff) : NULL;
}

static expr_t *build_linear_atan_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_shifted_inverse_term(u, inverse_u, u_sq, poly_coeff, NUM_ONE, false);
}

static expr_t *build_linear_asin_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_quarter_asin_acos_term(u, inverse_u, u_sq, poly_coeff, true);
}

static expr_t *build_linear_acos_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_quarter_asin_acos_term(u, inverse_u, u_sq, poly_coeff, false);
}

static expr_t *build_linear_acot_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_shifted_inverse_term(u, inverse_u, u_sq, poly_coeff, NUM_ONE, true);
}

static expr_t *build_linear_atanh_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_shifted_inverse_term(u, inverse_u, u_sq, poly_coeff, NUM_NEG_ONE, true);
}

static expr_t *build_linear_asec_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_root_inverse_term(u, inverse_u, u_sq, poly_coeff, build_sqrt_u_sq_minus_one(u_sq), false, false);
}

static expr_t *build_linear_acosec_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_root_inverse_term(u, inverse_u, u_sq, poly_coeff, build_sqrt_u_sq_minus_one(u_sq), true, false);
}

static expr_t *build_linear_asech_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_root_inverse_term(u, inverse_u, u_sq, poly_coeff, build_sqrt_one_minus_u_sq(u_sq), false, false);
}

static expr_t *build_linear_acosech_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_root_inverse_term(u, inverse_u, u_sq, poly_coeff, build_sqrt_one_plus_u_sq(u_sq), true, false);
}

static expr_t *build_linear_acoth_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_linear_half_shifted_inverse_term(u, inverse_u, u_sq, poly_coeff, NUM_NEG_ONE, true);
}

static expr_t *build_quadratic_atan_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_quadratic_atan_acot_term(u, inverse_u, u_sq, poly_coeff, true);
}

static expr_t *build_quadratic_acot_term(const expr_t *u, expr_t *inverse_u, const expr_t *u_sq, number_t poly_coeff)
{
    return build_quadratic_atan_acot_term(u, inverse_u, u_sq, poly_coeff, false);
}

#define INVERSE_AFFINE_RULE_FIRST EXPR_PATTERN_UNARY_ASIN
#define INVERSE_AFFINE_RULE_LAST EXPR_PATTERN_UNARY_E1
#define INVERSE_AFFINE_RULE_COUNT (INVERSE_AFFINE_RULE_LAST - INVERSE_AFFINE_RULE_FIRST + 1)
#define INVERSE_AFFINE_RULE_INDEX(kind) ((kind) - INVERSE_AFFINE_RULE_FIRST)

static const inverse_affine_rule_t inverse_affine_rules[INVERSE_AFFINE_RULE_COUNT] = {
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ASIN)] =
        { EXPR_PATTERN_UNARY_ASIN,        expr_asin,        build_base_asin_term,        build_linear_asin_term,              NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOS)] =
        { EXPR_PATTERN_UNARY_ACOS,        expr_acos,        build_base_acos_term,        build_linear_acos_term,              NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ATAN)] =
        { EXPR_PATTERN_UNARY_ATAN,        expr_atan,        build_base_atan_term,        build_linear_atan_term,              build_quadratic_atan_term },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ASEC)] =
        { EXPR_PATTERN_UNARY_ASEC,        expr_asec,        build_base_asec_term,        build_linear_asec_term,              NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOSEC)] =
        { EXPR_PATTERN_UNARY_ACOSEC,      expr_acosec,      build_base_acosec_term,      build_linear_acosec_term,            NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOT)] =
        { EXPR_PATTERN_UNARY_ACOT,        expr_acot,        build_base_acot_term,        build_linear_acot_term,              build_quadratic_acot_term },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ASINH)] =
        { EXPR_PATTERN_UNARY_ASINH,       expr_asinh,       build_base_asinh_term,       build_linear_asinh_term,             NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOSH)] =
        { EXPR_PATTERN_UNARY_ACOSH,       expr_acosh,       build_base_acosh_term,       build_linear_acosh_term,             NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ATANH)] =
        { EXPR_PATTERN_UNARY_ATANH,       expr_atanh,       build_base_atanh_term,       build_linear_atanh_term,             NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ASECH)] =
        { EXPR_PATTERN_UNARY_ASECH,       expr_asech,       build_base_asech_term,       build_linear_asech_term,             NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOSECH)] =
        { EXPR_PATTERN_UNARY_ACOSECH,     expr_acosech,     build_base_acosech_term,     build_linear_acosech_term,           NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ACOTH)] =
        { EXPR_PATTERN_UNARY_ACOTH,       expr_acoth,       build_base_acoth_term,       build_linear_acoth_term,             NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ERF)] =
        { EXPR_PATTERN_UNARY_ERF,         expr_erf,         build_base_erf_term,         build_linear_erf_term,               NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_ERFC)] =
        { EXPR_PATTERN_UNARY_ERFC,        expr_erfc,        build_base_erfc_term,        build_linear_erfc_term,              NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_NORMAL_PDF)] =
        { EXPR_PATTERN_UNARY_NORMAL_PDF,  expr_normal_cdf,  build_base_normal_pdf_term,  build_linear_normal_pdf_term,         NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_NORMAL_CDF)] =
        { EXPR_PATTERN_UNARY_NORMAL_CDF,  expr_normal_cdf,  build_base_normal_cdf_term,  build_linear_normal_cdf_term,         NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_EI)] =
        { EXPR_PATTERN_UNARY_EI,          expr_ei,          build_base_ei_term,          build_linear_ei_term,                NULL },
    [INVERSE_AFFINE_RULE_INDEX(EXPR_PATTERN_UNARY_E1)] =
        { EXPR_PATTERN_UNARY_E1,          expr_e1,          build_base_e1_term,          build_linear_e1_term,                NULL }
};

static const inverse_affine_rule_t *find_inverse_affine_rule(expr_pattern_unary_affine_kind_t kind)
{
    const inverse_affine_rule_t *rule;

    if (kind < INVERSE_AFFINE_RULE_FIRST || kind > INVERSE_AFFINE_RULE_LAST)
        return NULL;
    rule = &inverse_affine_rules[INVERSE_AFFINE_RULE_INDEX(kind)];
    return rule->build_base_term ? rule : NULL;
}

expr_t *integrate_linear_poly_times_inverse_affine(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind)
{
    const inverse_affine_rule_t *rule = find_inverse_affine_rule(kind);
    number_t poly[5];
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *vars[1];
    expr_t *u = NULL;
    expr_t *inverse_u = NULL;
    expr_t *u_sq = NULL;
    expr_t *quadratic_term = NULL;
    expr_t *base_term = NULL;
    expr_t *linear_term = NULL;
    expr_t *sum = NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    if (!rule ||
        !expr_match_affine_poly_deg4_times_unary_affine_kind(expr, kind, 1u, vars,
                                                              poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO) ||
        !num_eq(poly[3], NUM_ZERO) ||
        !num_eq(poly[4], NUM_ZERO) ||
        (!num_eq(poly[2], NUM_ZERO) && !rule->build_quadratic_term)) {
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    inverse_u = (u && rule->antiderivative_fn) ? rule->antiderivative_fn(u) : NULL;
    u_sq = u ? expr_pow(u, &NUM_TWO) : NULL;

    if (!num_eq(poly[0], NUM_ZERO) && rule->build_base_term && u && inverse_u)
        base_term = rule->build_base_term(u, inverse_u, u_sq, poly[0]);

    if (!num_eq(poly[1], NUM_ZERO) && rule->build_linear_term && u && inverse_u && u_sq)
        linear_term = rule->build_linear_term(u, inverse_u, u_sq, poly[1]);

    if (!num_eq(poly[2], NUM_ZERO) && rule->build_quadratic_term && u && inverse_u && u_sq)
        quadratic_term = rule->build_quadratic_term(u, inverse_u, u_sq, poly[2]);

    if (base_term && linear_term)
        sum = expr_add(base_term, linear_term);
    else if (base_term)
        sum = base_term, base_term = NULL;
    else if (linear_term)
        sum = linear_term, linear_term = NULL;
    if (sum && quadratic_term) {
        expr_t *next = expr_add(sum, quadratic_term);

        expr_free(sum);
        sum = next;
    } else if (quadratic_term) {
        sum = quadratic_term;
        quadratic_term = NULL;
    }

    expr_free(quadratic_term);
    expr_free(linear_term);
    expr_free(base_term);
    expr_free(u_sq);
    expr_free(inverse_u);
    expr_free(u);
    number_array_clear_local(poly, 5);
    num_destroy(&constant);
    return div_number_owned_consuming(sum, &coeff);
}

expr_t *integrate_linear_poly_times_normal_logpdf_affine(const expr_t *expr,
                                                         const expr_t *wrt)
{
    number_t poly[5];
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t neg_one_sixth = num_neg(NUM_ONE_SIXTH);
    number_t neg_log_sqrt_2pi = num_neg(NUM_LOG_SQRT_2PI);
    number_t neg_half_log_sqrt_2pi = num_div(neg_log_sqrt_2pi, NUM_TWO);
    number_t neg_one_eighth = num_neg(NUM_ONE_EIGHTH);
    expr_t *vars[1];
    expr_t *u = NULL;
    expr_t *u_sq = NULL;
    expr_t *u_cu = NULL;
    expr_t *u_qu = NULL;
    expr_t *base_poly = NULL;
    expr_t *linear_poly = NULL;
    expr_t *base_term = NULL;
    expr_t *linear_term = NULL;
    expr_t *sum = NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    if (!expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_NORMAL_LOGPDF,
                                                              1u, vars, poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO) ||
        !num_eq(poly[2], NUM_ZERO) ||
        !num_eq(poly[3], NUM_ZERO) ||
        !num_eq(poly[4], NUM_ZERO)) {
        number_array_clear_local(poly, 5);
        num_destroy(&neg_one_eighth);
        num_destroy(&neg_half_log_sqrt_2pi);
        num_destroy(&neg_log_sqrt_2pi);
        num_destroy(&neg_one_sixth);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    u_sq = u ? expr_pow(u, &NUM_TWO) : NULL;
    u_cu = (u && u_sq) ? expr_mul(u_sq, u) : NULL;
    u_qu = u_sq ? expr_pow(u_sq, &NUM_TWO) : NULL;

    if (!num_eq(poly[0], NUM_ZERO) && u && u_cu) {
        expr_t *scaled_linear = u ? expr_mul_num(u, &neg_log_sqrt_2pi) : NULL;
        expr_t *scaled_cubic = u_cu ? expr_mul_num(u_cu, &neg_one_sixth) : NULL;

        base_poly = (scaled_linear && scaled_cubic) ? expr_add(scaled_linear, scaled_cubic)
                                                    : NULL;
        if (base_poly) {
            base_term = mul_number_owned(base_poly, poly[0]);
            base_poly = NULL;
        }
        expr_free(scaled_cubic);
        expr_free(scaled_linear);
    }

    if (!num_eq(poly[1], NUM_ZERO) && u_sq && u_qu) {
        expr_t *quadratic_term = expr_mul_num(u_sq, &neg_half_log_sqrt_2pi);
        expr_t *quartic_term = u_qu ? expr_mul_num(u_qu, &neg_one_eighth) : NULL;

        linear_poly = (quadratic_term && quartic_term) ? expr_add(quadratic_term, quartic_term)
                                                       : NULL;
        if (linear_poly) {
            linear_term = mul_number_owned(linear_poly, poly[1]);
            linear_poly = NULL;
        }
        expr_free(quartic_term);
        expr_free(quadratic_term);
    }

    if (base_term && linear_term)
        sum = expr_add(base_term, linear_term);
    else if (base_term)
        sum = base_term, base_term = NULL;
    else if (linear_term)
        sum = linear_term, linear_term = NULL;

    expr_free(linear_term);
    expr_free(base_term);
    expr_free(linear_poly);
    expr_free(base_poly);
    expr_free(u_qu);
    expr_free(u_cu);
    expr_free(u_sq);
    expr_free(u);
    number_array_clear_local(poly, 5);
    num_destroy(&neg_one_eighth);
    num_destroy(&neg_half_log_sqrt_2pi);
    num_destroy(&neg_log_sqrt_2pi);
    num_destroy(&neg_one_sixth);
    num_destroy(&constant);
    return div_number_owned_consuming(sum, &coeff);
}
