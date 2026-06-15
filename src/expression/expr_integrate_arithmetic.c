#include <stdbool.h>
#include <stdint.h>

#include "expr_integrate_internal.h"

typedef expr_t *(*expr_integrate_binary_rule_fn)(const expr_t *expr,
                                                 const expr_t *wrt);

typedef enum expr_integrate_mul_rule_kind {
    EXPR_INTEGRATE_MUL_RULE_END,
    EXPR_INTEGRATE_MUL_RULE_DIRECT,
    EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
    EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE
} expr_integrate_mul_rule_kind_t;

typedef enum expr_integrate_mul_rule_feature {
    EXPR_INTEGRATE_MUL_FEATURE_EXP = 1u << 0,
    EXPR_INTEGRATE_MUL_FEATURE_TRIG = 1u << 1,
    EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC = 1u << 2,
    EXPR_INTEGRATE_MUL_FEATURE_LOG = 1u << 3,
    EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY = 1u << 4,
    EXPR_INTEGRATE_MUL_FEATURE_NORMAL_LOGPDF = 1u << 5,
    EXPR_INTEGRATE_MUL_FEATURE_EXP_TRIG =
        EXPR_INTEGRATE_MUL_FEATURE_EXP | EXPR_INTEGRATE_MUL_FEATURE_TRIG,
    EXPR_INTEGRATE_MUL_FEATURE_EXP_HYPERBOLIC =
        EXPR_INTEGRATE_MUL_FEATURE_EXP | EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC,
    EXPR_INTEGRATE_MUL_FEATURE_TRIG_HYPERBOLIC =
        EXPR_INTEGRATE_MUL_FEATURE_TRIG | EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC,
    EXPR_INTEGRATE_MUL_FEATURE_SPECIAL_PRODUCT =
        EXPR_INTEGRATE_MUL_FEATURE_EXP | EXPR_INTEGRATE_MUL_FEATURE_TRIG |
        EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC
} expr_integrate_mul_rule_feature_t;

typedef enum expr_integrate_div_rule_feature {
    EXPR_INTEGRATE_DIV_FEATURE_NUM_INDEPENDENT = 1u << 0,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_INDEPENDENT = 1u << 1,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_WRT = 1u << 2,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER = 1u << 3,
    EXPR_INTEGRATE_DIV_FEATURE_NUM_LOG = 1u << 4,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT = 1u << 5,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB = 1u << 6,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER_OR_SQRT =
        EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER | EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT,
    EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB_OR_SQRT =
        EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB | EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT
} expr_integrate_div_rule_feature_t;

typedef struct expr_integrate_mul_rule {
    expr_integrate_mul_rule_kind_t kind;
    expr_integrate_binary_rule_fn direct;
    expr_pattern_unary_affine_kind_t unary_kind;
} expr_integrate_mul_rule_t;

typedef struct expr_integrate_mul_rule_stage {
    const expr_integrate_mul_rule_t *rules;
    unsigned int required_features;
    unsigned int any_features;
} expr_integrate_mul_rule_stage_t;

typedef struct expr_integrate_div_rule_stage {
    const expr_integrate_binary_rule_fn *rules;
    unsigned int required_features;
    unsigned int any_features;
} expr_integrate_div_rule_stage_t;

typedef struct expr_integrate_mul_rule_feature_entry {
    expr_op_kind_t kind;
    unsigned int features;
} expr_integrate_mul_rule_feature_entry_t;

typedef struct expr_integrate_div_rule_feature_entry {
    expr_op_kind_t kind;
    unsigned int numerator_features;
    unsigned int denominator_features;
} expr_integrate_div_rule_feature_entry_t;

enum {
    EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN = EXPR_KIND_SIN,
    EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX = EXPR_KIND_E1
};

enum {
    EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN = EXPR_KIND_ADD,
    EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX = EXPR_KIND_SQRT
};

static expr_t *integrate_scaled_rule(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_inverse_symbolic_square_sum(const expr_t *expr,
                                                     const expr_t *wrt);
static expr_t *integrate_mul_rule_by_distribution(const expr_t *expr,
                                                  const expr_t *wrt);
static bool integrate_rule_kind_bit(expr_op_kind_t kind,
                                    expr_op_kind_t min_kind,
                                    expr_op_kind_t max_kind,
                                    uint64_t *bit_out);
static uint64_t integrate_rule_kind_range_mask(expr_op_kind_t first_kind,
                                               expr_op_kind_t last_kind,
                                               expr_op_kind_t min_kind,
                                               expr_op_kind_t max_kind);
static size_t integrate_rule_kind_index(uint64_t recognized_kind_mask,
                                        uint64_t kind_bit);
static uint64_t integrate_mul_rule_recognized_kind_mask(void);
static uint64_t integrate_div_rule_recognized_kind_mask(void);
static unsigned int integrate_mul_rule_direct_features(const expr_t *expr);
static unsigned int integrate_mul_rule_expr_features(const expr_t *expr);
static expr_t *integrate_mul_rule_dispatch(unsigned int features,
                                           const expr_t *expr,
                                           const expr_t *wrt);
static bool integrate_mul_rule_stage_matches(const expr_integrate_mul_rule_stage_t *stage,
                                             unsigned int features);
static expr_t *integrate_mul_rule_list(const expr_integrate_mul_rule_t *rules,
                                       const expr_t *expr,
                                       const expr_t *wrt);
static expr_t *integrate_mul_rule_candidate(const expr_integrate_mul_rule_t *rule,
                                            const expr_t *expr,
                                            const expr_t *wrt);
static expr_t *integrate_div_constant_denominator(const expr_t *expr,
                                                  const expr_t *wrt);
static expr_t *integrate_div_logarithmic_derivative(const expr_t *expr,
                                                    const expr_t *wrt);
static expr_t *integrate_div_wrt_denominator(const expr_t *expr,
                                             const expr_t *wrt);
static expr_t *integrate_div_constant_over_power_denominator(const expr_t *expr,
                                                             const expr_t *wrt);
static expr_t *integrate_div_inverse_affine_square(const expr_t *expr,
                                                   const expr_t *wrt);
static expr_t *integrate_div_inverse_affine_square_root(const expr_t *expr,
                                                        const expr_t *wrt);
static expr_t *integrate_div_constant_over_affine(const expr_t *expr,
                                                  const expr_t *wrt);
static expr_t *integrate_div_affine_over_affine(const expr_t *expr,
                                                const expr_t *wrt);
static unsigned int integrate_div_rule_kind_features(const expr_t *expr,
                                                     bool numerator);
static unsigned int integrate_div_rule_features(const expr_t *expr,
                                                const expr_t *wrt);
static expr_t *integrate_div_rule_dispatch(unsigned int features,
                                           const expr_t *expr,
                                           const expr_t *wrt);
static bool integrate_div_rule_stage_matches(const expr_integrate_div_rule_stage_t *stage,
                                             unsigned int features);
static expr_t *integrate_div_rule_list(const expr_integrate_binary_rule_fn *rules,
                                       const expr_t *expr,
                                       const expr_t *wrt);

static const expr_integrate_mul_rule_t integrate_mul_always_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_scaled_rule },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_poly_times_affine_power },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_linear_poly_times_centered_quadratic_root },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_monomial_times_affine_power },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_square_family_times_root },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_general_quadratic_times_root },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_exp_power_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_polynomial_times_polynomial_exp },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_integer_power_times_exp },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_trig_power_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_integer_power_times_trig },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_exp_gamma_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_power_times_exp_gamma },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_exp_trig_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_exp_times_trig },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_exp_hyperbolic_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_exp_times_hyperbolic },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_hyperbolic_product_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_hyperbolic_product },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_trig_hyperbolic_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_symbolic_trig_times_hyperbolic },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_wrt_exp_trig_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_wrt_exp_times_trig_exact },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_exp_tanh_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_exp_tanh_exact },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_poly_exp_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_EXP },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_poly_trig_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_SIN },
    { .kind = EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_COS },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_poly_hyperbolic_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_SINH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_COSH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_log_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_poly_times_log_affine },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_wrt_times_log_symbolic_affine },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_wrt_times_log_symbolic_quadratic },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_by_parts_primary_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ATAN },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ASIN },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOS },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ASEC },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOSEC },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOT },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ASINH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOSH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ATANH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ASECH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOSECH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ACOTH },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ERF },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_ERFC },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_NORMAL_PDF },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_NORMAL_CDF },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_normal_logpdf_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_linear_poly_times_normal_logpdf_affine },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_by_parts_expint_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_EI },
    { .kind = EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
      .unary_kind = EXPR_PATTERN_UNARY_E1 },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_t integrate_mul_special_product_rules[] = {
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_same_affine_special_product },
    { .kind = EXPR_INTEGRATE_MUL_RULE_END }
};

static const expr_integrate_mul_rule_stage_t integrate_mul_rule_stages[] = {
    { .rules = integrate_mul_always_rules },
    {
        .rules             = integrate_mul_exp_power_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP
    },
    {
        .rules             = integrate_mul_trig_power_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_TRIG
    },
    {
        .rules             = integrate_mul_exp_gamma_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP
    },
    {
        .rules             = integrate_mul_exp_trig_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP_TRIG
    },
    {
        .rules             = integrate_mul_exp_hyperbolic_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP_HYPERBOLIC
    },
    {
        .rules             = integrate_mul_hyperbolic_product_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC
    },
    {
        .rules             = integrate_mul_trig_hyperbolic_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_TRIG_HYPERBOLIC
    },
    {
        .rules             = integrate_mul_wrt_exp_trig_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP_TRIG
    },
    {
        .rules             = integrate_mul_exp_tanh_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP_HYPERBOLIC
    },
    {
        .rules             = integrate_mul_poly_exp_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_EXP
    },
    {
        .rules             = integrate_mul_poly_trig_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_TRIG
    },
    {
        .rules             = integrate_mul_poly_hyperbolic_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC
    },
    {
        .rules             = integrate_mul_log_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_LOG
    },
    {
        .rules             = integrate_mul_by_parts_primary_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY
    },
    {
        .rules             = integrate_mul_normal_logpdf_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_NORMAL_LOGPDF
    },
    {
        .rules             = integrate_mul_by_parts_expint_rules,
        .required_features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY
    },
    {
        .rules        = integrate_mul_special_product_rules,
        .any_features = EXPR_INTEGRATE_MUL_FEATURE_SPECIAL_PRODUCT
    },
    { .rules = NULL }
};

static const expr_integrate_mul_rule_feature_entry_t integrate_mul_rule_feature_table[] = {
    { .kind = EXPR_KIND_SIN,           .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_COS,           .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_TAN,           .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_SEC,           .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_COSEC,         .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_COT,           .features = EXPR_INTEGRATE_MUL_FEATURE_TRIG },
    { .kind = EXPR_KIND_SINH,          .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_COSH,          .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_TANH,          .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_SECH,          .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_COSECH,        .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_COTH,          .features = EXPR_INTEGRATE_MUL_FEATURE_HYPERBOLIC },
    { .kind = EXPR_KIND_ASIN,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOS,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ATAN,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ASEC,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOSEC,        .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOT,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ASINH,         .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOSH,         .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ATANH,         .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ASECH,         .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOSECH,       .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ACOTH,         .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_EXP,           .features = EXPR_INTEGRATE_MUL_FEATURE_EXP },
    { .kind = EXPR_KIND_LOG,           .features = EXPR_INTEGRATE_MUL_FEATURE_LOG },
    { .kind = EXPR_KIND_LOG10,         .features = EXPR_INTEGRATE_MUL_FEATURE_LOG },
    { .kind = EXPR_KIND_ERF,           .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_ERFC,          .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_NORMAL_PDF,    .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_NORMAL_CDF,    .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_NORMAL_LOGPDF, .features = EXPR_INTEGRATE_MUL_FEATURE_NORMAL_LOGPDF },
    { .kind = EXPR_KIND_EI,            .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY },
    { .kind = EXPR_KIND_E1,            .features = EXPR_INTEGRATE_MUL_FEATURE_BY_PARTS_UNARY }
};

static const expr_integrate_div_rule_feature_entry_t integrate_div_rule_feature_table[] = {
    { .kind = EXPR_KIND_ADD,   .denominator_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB },
    { .kind = EXPR_KIND_SUB,   .denominator_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB },
    { .kind = EXPR_KIND_POW,   .denominator_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER },
    { .kind = EXPR_KIND_POW_D, .denominator_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER },
    { .kind = EXPR_KIND_LOG,   .numerator_features   = EXPR_INTEGRATE_DIV_FEATURE_NUM_LOG },
    { .kind = EXPR_KIND_LOG10, .numerator_features   = EXPR_INTEGRATE_DIV_FEATURE_NUM_LOG },
    { .kind = EXPR_KIND_SQRT,  .denominator_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT }
};

static const expr_integrate_binary_rule_fn integrate_div_initial_rules[] = {
    integrate_scaled_rule,
    integrate_div_logarithmic_derivative,
    integrate_poly_times_affine_power,
    integrate_symbolic_monomial_times_affine_power,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_constant_denominator_rules[] = {
    integrate_div_constant_denominator,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_wrt_denominator_rules[] = {
    integrate_div_wrt_denominator,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_power_denominator_rules[] = {
    integrate_div_constant_over_power_denominator,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_log_over_rules[] = {
    integrate_log_over_proportional_affine,
    integrate_log_over_symbolic_proportional_affine,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_square_sum_rules[] = {
    integrate_inverse_symbolic_square_sum,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_root_lead_rules[] = {
    integrate_wrt_over_symbolic_affine_root,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_affine_power_rules[] = {
    integrate_symbolic_monomial_times_affine_power,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_root_family_rules[] = {
    integrate_symbolic_square_family_inverse_root,
    integrate_symbolic_square_family_wrt_over_root,
    integrate_linear_poly_over_centered_quadratic_root,
    integrate_symbolic_general_quadratic_linear_over_root,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_elementary_inverse_rules[] = {
    integrate_div_inverse_affine_square,
    integrate_div_inverse_affine_square_root,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_affine_quotient_rules[] = {
    integrate_div_constant_over_affine,
    integrate_div_affine_over_affine,
    NULL
};

static const expr_integrate_binary_rule_fn integrate_div_rational_rules[] = {
    integrate_poly_over_matching_affine,
    integrate_linear_over_symbolic_quadratic,
    integrate_poly_over_centered_quadratic,
    integrate_polynomial_over_monomial_power,
    integrate_rational_partial_fractions,
    NULL
};

static const expr_integrate_div_rule_stage_t integrate_div_rule_stages[] = {
    { .rules = integrate_div_initial_rules },
    {
        .rules             = integrate_div_constant_denominator_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_INDEPENDENT
    },
    {
        .rules             = integrate_div_wrt_denominator_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_NUM_INDEPENDENT |
                             EXPR_INTEGRATE_DIV_FEATURE_DEN_WRT
    },
    {
        .rules        = integrate_div_power_denominator_rules,
        .any_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER_OR_SQRT
    },
    {
        .rules             = integrate_div_log_over_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_NUM_LOG
    },
    {
        .rules             = integrate_div_square_sum_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB
    },
    {
        .rules             = integrate_div_root_lead_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT
    },
    {
        .rules        = integrate_div_affine_power_rules,
        .any_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_POWER_OR_SQRT
    },
    {
        .rules             = integrate_div_root_family_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_SQRT
    },
    {
        .rules             = integrate_div_elementary_inverse_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_NUM_INDEPENDENT,
        .any_features      = EXPR_INTEGRATE_DIV_FEATURE_DEN_ADD_SUB_OR_SQRT
    },
    { .rules = integrate_div_affine_quotient_rules },
    { .rules = integrate_div_rational_rules },
    { .rules = NULL }
};

expr_t *expr_integrate_as_constant(const expr_t *expr, const expr_t *wrt)
{
    return simplify_owned(expr_mul(expr, wrt));
}

expr_t *integrate_constant_rule(const expr_t *expr, const expr_t *wrt)
{
    if (!depends_on_wrt(expr, wrt))
        return expr_integrate_as_constant(expr, wrt);
    return NULL;
}

expr_t *integrate_var_rule(const expr_t *expr, const expr_t *wrt)
{
    if (is_wrt(expr, wrt))
        return integrate_power_of_wrt(expr, NUM_ONE, wrt);
    return expr_integrate_as_constant(expr, wrt);
}

expr_t *integrate_add_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left;
    expr_t *right;
    expr_t *sum;

    sum = integrate_exact_substitution_product(expr, wrt);
    if (sum)
        return sum;

    left = expr_integrate_dispatch(expr->a, wrt);
    if (!left)
        return NULL;
    right = expr_integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(sum);
}

expr_t *integrate_sub_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left;
    expr_t *right;
    expr_t *diff;

    diff = integrate_exact_substitution_product(expr, wrt);
    if (diff)
        return diff;

    left = expr_integrate_dispatch(expr->a, wrt);
    if (!left)
        return NULL;
    right = expr_integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    diff = expr_sub(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(diff);
}

expr_t *integrate_neg_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *inner = expr_integrate_dispatch(expr->a, wrt);
    expr_t *negated;

    if (!inner)
        return NULL;
    negated = expr_neg(inner);
    expr_free(inner);
    return simplify_owned(negated);
}

static expr_t *integrate_scaled_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t scale = num_new();
    const expr_t *base = NULL;
    expr_t *inner;
    expr_t *out;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr) {
        num_destroy(&scale);
        return NULL;
    }

    inner = expr_integrate_dispatch(base, wrt);
    if (!inner) {
        num_destroy(&scale);
        return NULL;
    }
    out = mul_number_owned(inner, scale);
    num_destroy(&scale);
    return out;
}

static bool integrate_rule_kind_bit(expr_op_kind_t kind,
                                    expr_op_kind_t min_kind,
                                    expr_op_kind_t max_kind,
                                    uint64_t *bit_out)
{
    unsigned int kind_value = (unsigned int)kind;

    if (!bit_out ||
        kind_value < (unsigned int)min_kind ||
        kind_value > (unsigned int)max_kind)
        return false;
    *bit_out = UINT64_C(1) << (kind_value - (unsigned int)min_kind);
    return true;
}

static uint64_t integrate_rule_kind_range_mask(expr_op_kind_t first_kind,
                                               expr_op_kind_t last_kind,
                                               expr_op_kind_t min_kind,
                                               expr_op_kind_t max_kind)
{
    uint64_t first_bit;
    uint64_t last_bit;
    uint64_t mask = 0u;

    if ((unsigned int)last_kind < (unsigned int)first_kind ||
        !integrate_rule_kind_bit(first_kind, min_kind, max_kind, &first_bit) ||
        !integrate_rule_kind_bit(last_kind, min_kind, max_kind, &last_bit))
        return 0u;

    for (uint64_t bit = first_bit; bit <= last_bit; bit <<= 1u) {
        mask |= bit;
        if (bit == last_bit)
            break;
    }
    return mask;
}

static size_t integrate_rule_kind_index(uint64_t recognized_kind_mask,
                                        uint64_t kind_bit)
{
    uint64_t lower_bits = recognized_kind_mask & (kind_bit - UINT64_C(1));
    size_t index = 0u;

    while (lower_bits) {
        lower_bits &= lower_bits - UINT64_C(1);
        ++index;
    }
    return index;
}

static uint64_t integrate_mul_rule_recognized_kind_mask(void)
{
    uint64_t exp_bit = 0u;

    integrate_rule_kind_bit(EXPR_KIND_EXP,
                            (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
                            (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX,
                            &exp_bit);

    return integrate_rule_kind_range_mask(
               EXPR_KIND_SIN, EXPR_KIND_COT,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           integrate_rule_kind_range_mask(
               EXPR_KIND_SINH, EXPR_KIND_COTH,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           integrate_rule_kind_range_mask(
               EXPR_KIND_ASIN, EXPR_KIND_ACOTH,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           exp_bit |
           integrate_rule_kind_range_mask(
               EXPR_KIND_LOG, EXPR_KIND_LOG10,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           integrate_rule_kind_range_mask(
               EXPR_KIND_ERF, EXPR_KIND_ERFC,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           integrate_rule_kind_range_mask(
               EXPR_KIND_NORMAL_PDF, EXPR_KIND_NORMAL_LOGPDF,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX) |
           integrate_rule_kind_range_mask(
               EXPR_KIND_EI, EXPR_KIND_E1,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
               (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX);
}

static uint64_t integrate_div_rule_recognized_kind_mask(void)
{
    uint64_t mask = 0u;
    uint64_t bit;

    if (integrate_rule_kind_bit(EXPR_KIND_ADD,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_SUB,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_POW,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_POW_D,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_LOG,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_LOG10,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;
    if (integrate_rule_kind_bit(EXPR_KIND_SQRT,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                &bit))
        mask |= bit;

    return mask;
}

static unsigned int integrate_mul_rule_direct_features(const expr_t *expr)
{
    uint64_t recognized_kind_mask;
    uint64_t kind_bit;
    size_t index;

    if (!expr || !expr->ops)
        return 0u;
    if (!integrate_rule_kind_bit(expr->ops->kind,
                                 (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MIN,
                                 (expr_op_kind_t)EXPR_INTEGRATE_MUL_FEATURE_KIND_MAX,
                                 &kind_bit))
        return 0u;
    recognized_kind_mask = integrate_mul_rule_recognized_kind_mask();
    if (!(recognized_kind_mask & kind_bit))
        return 0u;

    index = integrate_rule_kind_index(recognized_kind_mask, kind_bit);
    if (index >= sizeof(integrate_mul_rule_feature_table) /
                     sizeof(integrate_mul_rule_feature_table[0]))
        return 0u;
    if (integrate_mul_rule_feature_table[index].kind != expr->ops->kind)
        return 0u;
    return integrate_mul_rule_feature_table[index].features;
}

static unsigned int integrate_mul_rule_expr_features(const expr_t *expr)
{
    unsigned int direct_features;
    unsigned int child_features;

    if (!expr)
        return 0u;

    direct_features = integrate_mul_rule_direct_features(expr);
    if (direct_features)
        return direct_features;

    child_features = integrate_mul_rule_expr_features(expr->a);
    child_features |= integrate_mul_rule_expr_features(expr->b);
    return child_features;
}

static expr_t *integrate_mul_rule_candidate(const expr_integrate_mul_rule_t *rule,
                                            const expr_t *expr,
                                            const expr_t *wrt)
{
    if (!rule)
        return NULL;

    switch (rule->kind) {
        case EXPR_INTEGRATE_MUL_RULE_END:
            return NULL;
        case EXPR_INTEGRATE_MUL_RULE_DIRECT:
            return rule->direct ? rule->direct(expr, wrt) : NULL;
        case EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE:
            return integrate_poly_times_unary_affine_kind(expr, wrt, rule->unary_kind);
        case EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE:
            return integrate_linear_poly_times_inverse_affine(expr, wrt, rule->unary_kind);
    }

    return NULL;
}

static expr_t *integrate_mul_rule_list(const expr_integrate_mul_rule_t *rules,
                                       const expr_t *expr,
                                       const expr_t *wrt)
{
    if (!rules)
        return NULL;

    for (const expr_integrate_mul_rule_t *rule = rules;
         rule->kind != EXPR_INTEGRATE_MUL_RULE_END; ++rule) {
        expr_t *matched = integrate_mul_rule_candidate(rule, expr, wrt);

        if (matched)
            return matched;
    }
    return NULL;
}

static bool integrate_mul_rule_stage_matches(const expr_integrate_mul_rule_stage_t *stage,
                                             unsigned int features)
{
    if (!stage)
        return false;
    if ((features & stage->required_features) != stage->required_features)
        return false;
    if (stage->any_features && !(features & stage->any_features))
        return false;
    return true;
}

static expr_t *integrate_mul_rule_dispatch(unsigned int features,
                                           const expr_t *expr,
                                           const expr_t *wrt)
{
    for (const expr_integrate_mul_rule_stage_t *stage = integrate_mul_rule_stages;
         stage->rules; ++stage) {
        expr_t *matched;

        if (!integrate_mul_rule_stage_matches(stage, features))
            continue;
        matched = integrate_mul_rule_list(stage->rules, expr, wrt);
        if (matched)
            return matched;
    }

    return NULL;
}

expr_t *integrate_mul_rule(const expr_t *expr, const expr_t *wrt)
{
    bool left_depends;
    bool right_depends;
    unsigned int features;
    expr_t *matched;
    expr_t *inner;
    expr_t *product;

    matched = integrate_exact_substitution_product(expr, wrt);
    if (matched)
        return matched;

    features = integrate_mul_rule_expr_features(expr);
    matched = integrate_mul_rule_dispatch(features, expr, wrt);
    if (matched)
        return matched;

    matched = integrate_mul_rule_by_distribution(expr, wrt);
    if (matched)
        return matched;

    left_depends = depends_on_wrt(expr->a, wrt);
    right_depends = depends_on_wrt(expr->b, wrt);
    if (left_depends && right_depends)
        return NULL;

    if (!left_depends) {
        inner = expr_integrate_dispatch(expr->b, wrt);
        product = inner ? expr_mul(expr->a, inner) : NULL;
    } else {
        inner = expr_integrate_dispatch(expr->a, wrt);
        product = inner ? expr_mul(inner, expr->b) : NULL;
    }
    expr_free(inner);
    return simplify_owned(product);
}

static expr_t *integrate_mul_rule_by_distribution(const expr_t *expr,
                                                  const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *expanded = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr_match_mul_expr(expr, &left, &right))
        return NULL;

    if (left && left->ops && left->ops->kind == EXPR_KIND_ADD) {
        expr_t *first = expr_mul((expr_t *)left->a, (expr_t *)right);
        expr_t *second = expr_mul((expr_t *)left->b, (expr_t *)right);
        expanded = (first && second) ? expr_add(first, second) : NULL;
        expr_free(second);
        expr_free(first);
    } else if (left && left->ops && left->ops->kind == EXPR_KIND_SUB) {
        expr_t *first = expr_mul((expr_t *)left->a, (expr_t *)right);
        expr_t *second = expr_mul((expr_t *)left->b, (expr_t *)right);
        expanded = (first && second) ? expr_sub(first, second) : NULL;
        expr_free(second);
        expr_free(first);
    } else if (right && right->ops && right->ops->kind == EXPR_KIND_ADD) {
        expr_t *first = expr_mul((expr_t *)left, (expr_t *)right->a);
        expr_t *second = expr_mul((expr_t *)left, (expr_t *)right->b);
        expanded = (first && second) ? expr_add(first, second) : NULL;
        expr_free(second);
        expr_free(first);
    } else if (right && right->ops && right->ops->kind == EXPR_KIND_SUB) {
        expr_t *first = expr_mul((expr_t *)left, (expr_t *)right->a);
        expr_t *second = expr_mul((expr_t *)left, (expr_t *)right->b);
        expanded = (first && second) ? expr_sub(first, second) : NULL;
        expr_free(second);
        expr_free(first);
    }

    expanded = simplify_owned(expanded);
    if (!expanded ||
        (expanded->ops->kind != EXPR_KIND_ADD && expanded->ops->kind != EXPR_KIND_SUB) ||
        expr_equal_exact_local(expanded, expr)) {
        expr_free(expanded);
        return NULL;
    }
    out = expanded ? expr_integrate_dispatch(expanded, wrt) : NULL;
    expr_free(expanded);
    return out;
}

static expr_t *integrate_div_constant_denominator(const expr_t *expr,
                                                  const expr_t *wrt)
{
    expr_t *inner;
    expr_t *quotient;

    if (!expr || !expr->b || depends_on_wrt(expr->b, wrt))
        return NULL;

    inner = expr_integrate_dispatch(expr->a, wrt);
    quotient = inner ? expr_div(inner, expr->b) : NULL;
    expr_free(inner);
    return simplify_owned(quotient);
}

static bool integrate_expr_equivalent_by_zero_difference(const expr_t *left,
                                                         const expr_t *right)
{
    expr_t *difference = NULL;
    expr_t *simplified = NULL;
    bool equivalent = false;

    if (!left || !right)
        return false;
    if (expr_equal_exact_local(left, right))
        return true;

    difference = expr_sub(left, right);
    simplified = simplify_owned(difference);
    difference = NULL;
    equivalent = simplified && expr_is_exact_zero(simplified);
    expr_free(simplified);
    return equivalent;
}

static bool integrate_match_numeric_scaled_equivalent(const expr_t *expr,
                                                      const expr_t *base,
                                                      number_t *scale_out)
{
    number_t expr_scale = num_new();
    number_t base_scale = num_new();
    const expr_t *expr_base = NULL;
    const expr_t *base_base = NULL;
    bool expr_scaled = false;
    bool base_scaled = false;
    bool matched = false;

    if (!expr || !base || !scale_out)
        goto cleanup;

    if (integrate_expr_equivalent_by_zero_difference(expr, base)) {
        num_destroy(scale_out);
        *scale_out = num_clone(NUM_ONE);
        matched = true;
        goto cleanup;
    }

    expr_scaled = expr_match_scaled_expr(expr, &expr_scale, &expr_base);
    base_scaled = expr_match_scaled_expr(base, &base_scale, &base_base);

    if (expr_scaled && expr_base &&
        integrate_expr_equivalent_by_zero_difference(expr_base, base)) {
        num_destroy(scale_out);
        *scale_out = num_clone(expr_scale);
        matched = true;
        goto cleanup;
    }

    if (base_scaled && base_base && !num_eq(base_scale, NUM_ZERO) &&
        integrate_expr_equivalent_by_zero_difference(expr, base_base)) {
        num_destroy(scale_out);
        *scale_out = num_div(NUM_ONE, base_scale);
        matched = true;
        goto cleanup;
    }

    if (expr_scaled && base_scaled && expr_base && base_base &&
        !num_eq(base_scale, NUM_ZERO) &&
        integrate_expr_equivalent_by_zero_difference(expr_base, base_base)) {
        num_destroy(scale_out);
        *scale_out = num_div(expr_scale, base_scale);
        matched = true;
        goto cleanup;
    }

cleanup:
    num_destroy(&base_scale);
    num_destroy(&expr_scale);
    return matched;
}

static expr_t *integrate_div_logarithmic_derivative(const expr_t *expr,
                                                    const expr_t *wrt)
{
    number_t scale = num_new();
    expr_t *denominator_deriv = NULL;
    expr_t *log_denominator = NULL;
    expr_t *scaled = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b || !depends_on_wrt(expr->b, wrt))
        goto cleanup;

    denominator_deriv = expr_create_deriv(expr->b, wrt);
    denominator_deriv = simplify_owned(denominator_deriv);
    if (!denominator_deriv || expr_is_exact_zero(denominator_deriv))
        goto cleanup;

    if (!integrate_match_numeric_scaled_equivalent(expr->a, denominator_deriv,
                                                   &scale))
        goto cleanup;

    log_denominator = expr_log(expr->b);
    scaled = log_denominator ? mul_number_owned(log_denominator, scale) : NULL;
    log_denominator = NULL;
    out = simplify_owned(scaled);
    scaled = NULL;

cleanup:
    expr_free(scaled);
    expr_free(log_denominator);
    expr_free(denominator_deriv);
    num_destroy(&scale);
    return out;
}

static expr_t *integrate_div_wrt_denominator(const expr_t *expr,
                                             const expr_t *wrt)
{
    expr_t *log_x;
    expr_t *quotient;

    if (!expr || !expr->a || !expr->b ||
        !is_wrt(expr->b, wrt) ||
        depends_on_wrt(expr->a, wrt))
        return NULL;

    log_x = expr_log(wrt);
    quotient = log_x ? expr_mul(expr->a, log_x) : NULL;
    expr_free(log_x);
    return simplify_owned(quotient);
}

static expr_t *integrate_div_constant_over_power_denominator(const expr_t *expr,
                                                             const expr_t *wrt)
{
    if (!expr)
        return NULL;
    return integrate_constant_over_power_denominator(expr->a, expr->b, wrt);
}

static expr_t *integrate_div_inverse_affine_square(const expr_t *expr,
                                                   const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t numer_constant = num_new();
    expr_t *u = NULL;
    expr_t *inverse_term = NULL;
    expr_t *out = NULL;
    bool is_plus_square = false;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !expr_match_const_value(expr->a, &numer_constant) ||
        !num_eq(numer_constant, NUM_ONE) ||
        !match_one_plus_minus_affine_square(expr->b, wrt, &is_plus_square,
                                            &constant, &coeff) ||
        !num_eq(constant, NUM_ZERO) ||
        num_eq(coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, constant, coeff);
    inverse_term = u ? (is_plus_square ? expr_atan(u) : expr_atanh(u)) : NULL;
    out = div_number_owned(inverse_term, coeff);
    inverse_term = NULL;

cleanup:
    expr_free(inverse_term);
    expr_free(u);
    num_destroy(&numer_constant);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_div_inverse_affine_square_root(const expr_t *expr,
                                                        const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t numer_constant = num_new();
    expr_t *u = NULL;
    expr_t *inverse_term = NULL;
    expr_t *out = NULL;
    bool is_plus_square = false;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !expr_match_const_value(expr->a, &numer_constant) ||
        !num_eq(numer_constant, NUM_ONE) ||
        !expr->b->ops || expr->b->ops->kind != EXPR_KIND_SQRT ||
        !match_one_plus_minus_affine_square(expr->b->a, wrt, &is_plus_square,
                                            &constant, &coeff) ||
        !num_eq(constant, NUM_ZERO) ||
        num_eq(coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, constant, coeff);
    inverse_term = u ? (is_plus_square ? expr_asinh(u) : expr_asin(u)) : NULL;
    out = div_number_owned(inverse_term, coeff);
    inverse_term = NULL;

cleanup:
    expr_free(inverse_term);
    expr_free(u);
    num_destroy(&numer_constant);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_div_constant_over_affine(const expr_t *expr,
                                                  const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *log_denom = NULL;
    expr_t *scaled_log = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &constant, &coeff))
        goto cleanup;

    log_denom = expr_log(expr->b);
    scaled_log = log_denom ? expr_mul(expr->a, log_denom) : NULL;
    out = div_number_owned(scaled_log, coeff);
    scaled_log = NULL;

cleanup:
    expr_free(scaled_log);
    expr_free(log_denom);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_div_affine_over_affine(const expr_t *expr,
                                                const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();
    number_t linear_scale = num_new();
    number_t scaled_denom_const = num_new();
    number_t remainder = num_new();
    expr_t *linear_term = NULL;
    expr_t *log_denom = NULL;
    expr_t *log_term = NULL;
    expr_t *sum = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        !match_nonconstant_affine_linear_expr(expr->a, wrt, &numer_constant,
                                             &numer_coeff) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO))
        goto cleanup;

    num_destroy(&linear_scale);
    num_destroy(&scaled_denom_const);
    num_destroy(&remainder);
    linear_scale = num_div(numer_coeff, coeff);
    scaled_denom_const = num_mul(linear_scale, constant);
    remainder = num_sub(numer_constant, scaled_denom_const);
    linear_term = expr_mul_num(wrt, &linear_scale);
    log_denom = expr_log(expr->b);
    log_term = log_denom ? expr_mul_num(log_denom, &remainder) : NULL;
    sum = (linear_term && log_term) ? expr_add(linear_term, log_term)
                                    : (linear_term ? linear_term : log_term);

    if (sum == linear_term)
        linear_term = NULL;
    if (sum == log_term)
        log_term = NULL;
    out = div_number_owned(sum, coeff);
    sum = NULL;

cleanup:
    expr_free(sum);
    expr_free(log_term);
    expr_free(log_denom);
    expr_free(linear_term);
    num_destroy(&remainder);
    num_destroy(&scaled_denom_const);
    num_destroy(&linear_scale);
    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static unsigned int integrate_div_rule_kind_features(const expr_t *expr,
                                                     bool numerator)
{
    uint64_t recognized_kind_mask;
    uint64_t kind_bit;
    size_t index;

    if (!expr || !expr->ops)
        return 0u;
    if (!integrate_rule_kind_bit(expr->ops->kind,
                                 (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MIN,
                                 (expr_op_kind_t)EXPR_INTEGRATE_DIV_FEATURE_KIND_MAX,
                                 &kind_bit))
        return 0u;
    recognized_kind_mask = integrate_div_rule_recognized_kind_mask();
    if (!(recognized_kind_mask & kind_bit))
        return 0u;

    index = integrate_rule_kind_index(recognized_kind_mask, kind_bit);
    if (index >= sizeof(integrate_div_rule_feature_table) /
                     sizeof(integrate_div_rule_feature_table[0]))
        return 0u;
    if (integrate_div_rule_feature_table[index].kind != expr->ops->kind)
        return 0u;
    return numerator ? integrate_div_rule_feature_table[index].numerator_features
                     : integrate_div_rule_feature_table[index].denominator_features;
}

static unsigned int integrate_div_rule_features(const expr_t *expr,
                                                const expr_t *wrt)
{
    unsigned int features = 0u;

    if (!expr)
        return 0u;
    if (expr->a && !depends_on_wrt(expr->a, wrt))
        features |= EXPR_INTEGRATE_DIV_FEATURE_NUM_INDEPENDENT;
    if (expr->b && !depends_on_wrt(expr->b, wrt))
        features |= EXPR_INTEGRATE_DIV_FEATURE_DEN_INDEPENDENT;
    if (expr->b && is_wrt(expr->b, wrt))
        features |= EXPR_INTEGRATE_DIV_FEATURE_DEN_WRT;

    features |= integrate_div_rule_kind_features(expr->a, true);
    features |= integrate_div_rule_kind_features(expr->b, false);
    return features;
}

static expr_t *integrate_div_rule_list(const expr_integrate_binary_rule_fn *rules,
                                       const expr_t *expr,
                                       const expr_t *wrt)
{
    if (!rules)
        return NULL;

    for (size_t i = 0u; rules[i]; ++i) {
        expr_t *matched = rules[i](expr, wrt);

        if (matched)
            return matched;
    }
    return NULL;
}

static bool integrate_div_rule_stage_matches(const expr_integrate_div_rule_stage_t *stage,
                                             unsigned int features)
{
    if (!stage)
        return false;
    if ((features & stage->required_features) != stage->required_features)
        return false;
    if (stage->any_features && !(features & stage->any_features))
        return false;
    return true;
}

static expr_t *integrate_div_rule_dispatch(unsigned int features,
                                           const expr_t *expr,
                                           const expr_t *wrt)
{
    for (const expr_integrate_div_rule_stage_t *stage = integrate_div_rule_stages;
         stage->rules; ++stage) {
        expr_t *matched;

        if (!integrate_div_rule_stage_matches(stage, features))
            continue;
        matched = integrate_div_rule_list(stage->rules, expr, wrt);
        if (matched)
            return matched;
    }
    return NULL;
}

expr_t *integrate_div_rule(const expr_t *expr, const expr_t *wrt)
{
    unsigned int features = integrate_div_rule_features(expr, wrt);
    expr_t *matched = integrate_div_rule_dispatch(features, expr, wrt);

    if (matched)
        return matched;
    return integrate_exact_substitution_product(expr, wrt);
}

static bool match_square_of_expr(const expr_t *expr, const expr_t **base_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !base_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a &&
        num_eq(expr->c, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) &&
               num_eq(exponent, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    }

    num_destroy(&exponent);
    return ok;
}

static bool match_symbolic_square_sum_denominator(const expr_t *expr,
                                                  const expr_t *wrt,
                                                  const expr_t **symbol_base_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *left_base = NULL;
    const expr_t *right_base = NULL;
    bool is_sub = false;

    if (!expr || !wrt || !symbol_base_out ||
        !expr_match_add_sub_expr(expr, &left, &right, &is_sub) ||
        is_sub ||
        !match_square_of_expr(left, &left_base) ||
        !match_square_of_expr(right, &right_base))
        return false;

    if (is_wrt(left_base, wrt) && !depends_on_wrt(right_base, wrt)) {
        *symbol_base_out = right_base;
        return true;
    }
    if (is_wrt(right_base, wrt) && !depends_on_wrt(left_base, wrt)) {
        *symbol_base_out = left_base;
        return true;
    }
    return false;
}

static expr_t *integrate_inverse_symbolic_square_sum(const expr_t *expr,
                                                     const expr_t *wrt)
{
    number_t numer_constant = num_new();
    const expr_t *symbol_base = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !expr_match_const_value(expr->a, &numer_constant) ||
        !num_eq(numer_constant, NUM_ONE) ||
        !match_symbolic_square_sum_denominator(expr->b, wrt, &symbol_base))
        goto cleanup;

    expr_t *arg = expr_div(wrt, symbol_base);
    expr_t *atan_arg = arg ? expr_atan(arg) : NULL;
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *reciprocal = one ? expr_div(one, symbol_base) : NULL;

    out = (reciprocal && atan_arg) ? expr_mul(reciprocal, atan_arg) : NULL;
    out = simplify_owned(out);
    expr_free(reciprocal);
    expr_free(one);
    expr_free(atan_arg);
    expr_free(arg);

cleanup:
    num_destroy(&numer_constant);
    return out;
}
