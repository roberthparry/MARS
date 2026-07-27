#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

typedef expr_t *(*expr_integrate_binary_rule_fn)(const expr_t *expr,
                                                 const expr_t *wrt);

typedef enum expr_integrate_mul_rule_kind {
    EXPR_INTEGRATE_MUL_RULE_END,
    EXPR_INTEGRATE_MUL_RULE_DIRECT,
    EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE,
    EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE,
    EXPR_INTEGRATE_MUL_RULE_KIND_COUNT
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

typedef expr_t *(*expr_integrate_mul_rule_dispatch_fn)(
    const expr_integrate_mul_rule_t *rule,
    const expr_t *expr,
    const expr_t *wrt);

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
static expr_t *integrate_div_rule_by_numerator_distribution(const expr_t *expr,
                                                            const expr_t *wrt);
static expr_t *integrate_div_quotient_derivative(const expr_t *expr,
                                                 const expr_t *wrt);
static expr_t *integrate_div_quotient_power_polynomial_derivative(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *denominator,
    unsigned int candidate_power);
static bool integrate_collect_numeric_poly_local(const expr_t *expr,
                                                 const expr_t *wrt,
                                                 size_t max_degree,
                                                 number_t *out);
static bool integrate_poly_scale_local(const number_t *src,
                                       number_t scale,
                                       number_t *out,
                                       size_t count);
static number_t *integrate_number_array_alloc_local(size_t count);
static void integrate_number_array_free_local(number_t *values, size_t count);
static number_t *integrate_number_matrix_alloc_local(size_t rows,
                                                     size_t cols);
static inline number_t *integrate_matrix_cell_local(number_t *matrix,
                                                    size_t cols,
                                                    size_t row,
                                                    size_t col);
static expr_t *quotient_power_build_flat_polynomial_expr(const expr_t *var,
                                                         const number_t *coeffs,
                                                         size_t count);
static expr_t *integrate_div_by_exp_denominator(const expr_t *expr,
                                                const expr_t *wrt);
static expr_t *integrate_div_sin_integer_multiple_quotient(const expr_t *expr,
                                                           const expr_t *wrt);
static expr_t *integrate_div_logarithmic_derivative(const expr_t *expr,
                                                    const expr_t *wrt);
static expr_t *integrate_div_wrt_denominator(const expr_t *expr,
                                             const expr_t *wrt);
static expr_t *integrate_div_constant_over_power_denominator(const expr_t *expr,
                                                             const expr_t *wrt);
static expr_t *integrate_div_cubic_over_quadratic_square(const expr_t *expr,
                                                         const expr_t *wrt);
static expr_t *integrate_div_poly_over_quadratic_square(const expr_t *expr,
                                                        const expr_t *wrt);
static expr_t *integrate_div_inverse_affine_square(const expr_t *expr,
                                                   const expr_t *wrt);
static expr_t *integrate_div_inverse_affine_square_root(const expr_t *expr,
                                                        const expr_t *wrt);
static expr_t *integrate_div_inverse_one_plus_unit_circle_root(const expr_t *expr,
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
static bool match_square_of_expr(const expr_t *expr, const expr_t **base_out);
static bool match_positive_integer_power_of_expr(const expr_t *expr,
                                                 const expr_t **base_out,
                                                 unsigned int *exponent_out);

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
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_sec_squared_log_tan_cot },
    { .kind = EXPR_INTEGRATE_MUL_RULE_DIRECT, .direct = integrate_sec_double_angle_log_tan_cot },
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
    integrate_div_quotient_derivative,
    integrate_div_rule_by_numerator_distribution,
    integrate_div_by_exp_denominator,
    integrate_div_sin_integer_multiple_quotient,
    integrate_inverse_sqrt_sin_cos_sin3_cos,
    integrate_inverse_quartic_appell_f1,
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
    integrate_div_cubic_over_quadratic_square,
    integrate_div_poly_over_quadratic_square,
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
    integrate_div_inverse_one_plus_unit_circle_root,
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
    {
        .rules             = integrate_div_constant_denominator_rules,
        .required_features = EXPR_INTEGRATE_DIV_FEATURE_DEN_INDEPENDENT
    },
    { .rules = integrate_div_initial_rules },
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

static expr_t *integrate_sum_terms_rule(const expr_t *expr,
                                        const expr_t *wrt,
                                        bool subtract)
{
    expr_t *left;
    expr_t *right;
    expr_t *whole;
    expr_t *out;

    left = expr_integrate_dispatch(expr->a, wrt);
    right = expr_integrate_dispatch(expr->b, wrt);

    if (!left || !right) {
        whole = integrate_exact_substitution_product(expr, wrt);
        if (whole) {
            expr_free(right);
            expr_free(left);
            return whole;
        }
        if (!left)
            left = expr_integral(expr->a, wrt);
        if (!right)
            right = expr_integral(expr->b, wrt);
    }

    if (!left || !right) {
        expr_free(right);
        expr_free(left);
        return NULL;
    }

    out = subtract ? expr_sub(left, right) : expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(out);
}

expr_t *integrate_add_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *sum;

    sum = integrate_sum_terms_rule(expr, wrt, false);
    if (sum)
        return sum;

    return integrate_exact_substitution_product(expr, wrt);
}

expr_t *integrate_sub_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *diff;

    diff = integrate_sum_terms_rule(expr, wrt, true);
    if (diff)
        return diff;

    diff = integrate_exact_substitution_product(expr, wrt);
    return diff;
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
    if (expr_is_div(inner) && inner->a && inner->b) {
        expr_t *scale_expr = expr_new_const(scale);
        expr_t *scaled_numerator = (scale_expr && inner->a)
                                       ? expr_mul(scale_expr, inner->a)
                                       : NULL;

        out = (scaled_numerator && inner->b)
                  ? expr_div(scaled_numerator, inner->b)
                  : NULL;
        expr_free(scaled_numerator);
        expr_free(scale_expr);
        expr_free(inner);
        num_destroy(&scale);
        return out;
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

static expr_t *integrate_mul_rule_direct_dispatch(
    const expr_integrate_mul_rule_t *rule,
    const expr_t *expr,
    const expr_t *wrt)
{
    return rule->direct ? rule->direct(expr, wrt) : NULL;
}

static expr_t *integrate_mul_rule_poly_unary_affine_dispatch(
    const expr_integrate_mul_rule_t *rule,
    const expr_t *expr,
    const expr_t *wrt)
{
    return integrate_poly_times_unary_affine_kind(expr, wrt,
                                                  rule->unary_kind);
}

static expr_t *integrate_mul_rule_linear_inverse_affine_dispatch(
    const expr_integrate_mul_rule_t *rule,
    const expr_t *expr,
    const expr_t *wrt)
{
    return integrate_linear_poly_times_inverse_affine(expr, wrt,
                                                      rule->unary_kind);
}

static const expr_integrate_mul_rule_dispatch_fn
integrate_mul_rule_candidate_dispatch[EXPR_INTEGRATE_MUL_RULE_KIND_COUNT] = {
    [EXPR_INTEGRATE_MUL_RULE_DIRECT] =
        integrate_mul_rule_direct_dispatch,
    [EXPR_INTEGRATE_MUL_RULE_POLY_UNARY_AFFINE] =
        integrate_mul_rule_poly_unary_affine_dispatch,
    [EXPR_INTEGRATE_MUL_RULE_LINEAR_INVERSE_AFFINE] =
        integrate_mul_rule_linear_inverse_affine_dispatch
};

static expr_t *integrate_mul_rule_candidate(const expr_integrate_mul_rule_t *rule,
                                            const expr_t *expr,
                                            const expr_t *wrt)
{
    expr_integrate_mul_rule_dispatch_fn dispatch;

    if (!rule || (unsigned)rule->kind >= EXPR_INTEGRATE_MUL_RULE_KIND_COUNT)
        return NULL;

    dispatch = integrate_mul_rule_candidate_dispatch[rule->kind];
    return dispatch ? dispatch(rule, expr, wrt) : NULL;
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

    left_depends = depends_on_wrt(expr->a, wrt);
    right_depends = depends_on_wrt(expr->b, wrt);
    if (left_depends != right_depends) {
        if (!left_depends) {
            inner = expr_integrate_dispatch(expr->b, wrt);
            product = inner ? expr_mul(expr->a, inner) : NULL;
        } else {
            inner = expr_integrate_dispatch(expr->a, wrt);
            product = inner ? expr_mul(inner, expr->b) : NULL;
        }
        expr_free(inner);
        if (product)
            return simplify_owned(product);
    }

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

static expr_t *integrate_div_rule_by_numerator_distribution(const expr_t *expr,
                                                            const expr_t *wrt)
{
    expr_t *left_div = NULL;
    expr_t *right_div = NULL;
    expr_t *left_anti = NULL;
    expr_t *right_anti = NULL;
    expr_t *combined = NULL;

    if (!expr || !expr->a || !expr->a->ops || !expr->b ||
        (expr->a->ops->kind != EXPR_KIND_ADD && expr->a->ops->kind != EXPR_KIND_SUB)) {
        return NULL;
    }

    left_div = expr_div(expr->a->a, expr->b);
    right_div = expr_div(expr->a->b, expr->b);
    left_anti = left_div ? expr_integrate_dispatch(left_div, wrt) : NULL;
    right_anti = right_div ? expr_integrate_dispatch(right_div, wrt) : NULL;

    if (left_anti && right_anti)
        combined = expr->a->ops->kind == EXPR_KIND_SUB
            ? expr_sub(left_anti, right_anti)
            : expr_add(left_anti, right_anti);

    expr_free(right_anti);
    expr_free(left_anti);
    expr_free(right_div);
    expr_free(left_div);
    return simplify_owned(combined);
}

static expr_t *integrate_div_by_exp_denominator(const expr_t *expr,
                                                const expr_t *wrt)
{
    expr_t *neg_arg = NULL;
    expr_t *reciprocal = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b || !expr_is_exp_expr(expr->b))
        return NULL;

    neg_arg = expr_neg(expr->b->a);
    reciprocal = neg_arg ? expr_exp(neg_arg) : NULL;
    product = reciprocal ? expr_mul(expr->a, reciprocal) : NULL;
    expr_free(reciprocal);
    expr_free(neg_arg);

    if (!product || expr_equal_exact_local(product, expr)) {
        expr_free(product);
        return NULL;
    }

    out = expr_integrate_dispatch(product, wrt);
    expr_free(product);
    return out;
}

static bool integrate_small_positive_integer_coeff(const expr_t *expr,
                                                   long *out)
{
    number_t value = num_new();
    long numerator = 0;
    long denominator = 0;
    bool ok = false;

    if (!expr || !out ||
        !expr_match_const_value(expr, &value) ||
        !num_is_real(value) ||
        !num_is_integer(value) ||
        !num_get_small_rational(value, &numerator, &denominator) ||
        denominator != 1 ||
        numerator <= 0) {
        goto cleanup;
    }

    *out = numerator;
    ok = true;

cleanup:
    num_destroy(&value);
    return ok;
}

static expr_t *integrate_build_pi_rational(long numerator, long denominator)
{
    number_t ratio = num_create_from_frac(numerator, denominator);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *scaled = pi ? expr_mul_num(pi, &ratio) : NULL;
    expr_t *out = simplify_owned(scaled);

    expr_free(pi);
    num_destroy(&ratio);
    return out;
}

static expr_t *integrate_sin_quotient_paired_log_term(const expr_t *wrt,
                                                      long k,
                                                      long denominator)
{
    expr_t *angle = NULL;
    expr_t *left_arg = NULL;
    expr_t *right_arg = NULL;
    expr_t *left_sin = NULL;
    expr_t *right_sin = NULL;
    expr_t *left_abs = NULL;
    expr_t *right_abs = NULL;
    expr_t *left_log = NULL;
    expr_t *right_log = NULL;
    expr_t *out = NULL;

    angle = integrate_build_pi_rational(k, denominator);
    left_arg = angle ? expr_sub(wrt, angle) : NULL;
    right_arg = angle ? expr_add(wrt, angle) : NULL;
    left_sin = left_arg ? expr_sin(left_arg) : NULL;
    right_sin = right_arg ? expr_sin(right_arg) : NULL;
    left_abs = left_sin ? expr_abs(left_sin) : NULL;
    right_abs = right_sin ? expr_abs(right_sin) : NULL;
    left_log = left_abs ? expr_log(left_abs) : NULL;
    right_log = right_abs ? expr_log(right_abs) : NULL;
    out = (left_log && right_log) ? expr_sub(left_log, right_log) : NULL;

    expr_free(right_log);
    expr_free(left_log);
    expr_free(right_abs);
    expr_free(left_abs);
    expr_free(right_sin);
    expr_free(left_sin);
    expr_free(right_arg);
    expr_free(left_arg);
    expr_free(angle);
    return out;
}

static expr_t *integrate_sin_quotient_residue_coeff(long numerator,
                                                    long denominator,
                                                    long k)
{
    number_t scale = num_create_from_frac((k & 1L) ? -1L : 1L, denominator);
    expr_t *angle = NULL;
    expr_t *sin_angle = NULL;
    expr_t *scaled = NULL;
    expr_t *out = NULL;

    angle = integrate_build_pi_rational(numerator * k, denominator);
    sin_angle = angle ? expr_sin(angle) : NULL;
    scaled = sin_angle ? expr_mul_num(sin_angle, &scale) : NULL;
    out = simplify_owned(scaled);

    expr_free(sin_angle);
    expr_free(angle);
    num_destroy(&scale);
    return out;
}

static expr_t *integrate_build_odd_sin_integer_multiple_quotient(long numerator,
                                                                 long denominator,
                                                                 const expr_t *wrt)
{
    expr_t *sum = NULL;

    for (long k = 1; k <= (denominator - 1L) / 2L; ++k) {
        expr_t *coeff = integrate_sin_quotient_residue_coeff(numerator,
                                                             denominator,
                                                             k);
        expr_t *log_term = integrate_sin_quotient_paired_log_term(wrt,
                                                                  k,
                                                                  denominator);
        expr_t *term = (coeff && log_term) ? expr_mul(coeff, log_term) : NULL;

        if (!term) {
            expr_free(log_term);
            expr_free(coeff);
            expr_free(sum);
            return NULL;
        }

        if (sum) {
            expr_t *next = expr_add(sum, term);

            expr_free(sum);
            expr_free(term);
            term = NULL;
            sum = next;
        } else {
            sum = term;
            term = NULL;
        }

        expr_free(term);
        expr_free(log_term);
        expr_free(coeff);
    }

    return sum;
}

static expr_t *integrate_div_sin_integer_multiple_quotient(const expr_t *expr,
                                                           const expr_t *wrt)
{
    enum { MAX_DENOMINATOR = 65 };
    expr_t *numerator_coeff = NULL;
    expr_t *denominator_coeff = NULL;
    long numerator = 0;
    long denominator = 0;
    bool numerator_is_sin = false;
    bool denominator_is_sin = false;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b || !wrt ||
        !match_trig_proportional_wrt_coeff(expr->a, wrt, &numerator_is_sin,
                                           &numerator_coeff) ||
        !match_trig_proportional_wrt_coeff(expr->b, wrt, &denominator_is_sin,
                                           &denominator_coeff) ||
        !numerator_is_sin ||
        !denominator_is_sin ||
        !integrate_small_positive_integer_coeff(numerator_coeff, &numerator) ||
        !integrate_small_positive_integer_coeff(denominator_coeff, &denominator) ||
        denominator <= 1 ||
        denominator > MAX_DENOMINATOR ||
        (denominator % 2L) == 0L ||
        numerator == denominator) {
        goto cleanup;
    }

    out = integrate_build_odd_sin_integer_multiple_quotient(numerator,
                                                           denominator,
                                                           wrt);

cleanup:
    expr_free(denominator_coeff);
    expr_free(numerator_coeff);
    return out;
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

static bool integrate_poly_expr_equivalent_local(const expr_t *left,
                                                 const expr_t *right,
                                                 const expr_t *wrt)
{
    enum { MAX_EQUIV_DEGREE = 2 };
    number_t *left_poly = NULL;
    number_t *right_poly = NULL;
    size_t count = MAX_EQUIV_DEGREE + 1u;
    bool ok = false;

    left_poly = integrate_number_array_alloc_local(count);
    right_poly = integrate_number_array_alloc_local(count);
    if (!left_poly || !right_poly)
        goto cleanup;

    if (integrate_collect_numeric_poly_local(left, wrt, MAX_EQUIV_DEGREE,
                                             left_poly) &&
        integrate_collect_numeric_poly_local(right, wrt, MAX_EQUIV_DEGREE,
                                             right_poly)) {
        ok = true;
        for (size_t i = 0u; i < count; ++i)
            ok = ok && num_eq(left_poly[i], right_poly[i]);
    }

cleanup:
    integrate_number_array_free_local(right_poly, count);
    integrate_number_array_free_local(left_poly, count);
    return ok;
}

static bool integrate_product_with_factor_local(const expr_t *expr,
                                                const expr_t *factor,
                                                const expr_t *wrt,
                                                const expr_t **other_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !factor || !wrt || !other_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_struct_eq(left, factor) ||
        integrate_poly_expr_equivalent_local(left, factor, wrt)) {
        *other_out = right;
        return true;
    }
    if (expr_struct_eq(right, factor) ||
        integrate_poly_expr_equivalent_local(right, factor, wrt)) {
        *other_out = left;
        return true;
    }
    return false;
}

static bool integrate_extract_common_scaled_sub_terms(const expr_t *expr,
                                                     const expr_t **positive_out,
                                                     const expr_t **negative_out,
                                                     number_t *scale_out)
{
    const expr_t *base = expr;
    const expr_t *positive = NULL;
    const expr_t *negative = NULL;
    const expr_t *positive_base = NULL;
    const expr_t *negative_base = NULL;
    number_t scale = num_new();
    number_t positive_scale = num_new();
    number_t negative_scale = num_new();
    number_t add_negative_scale = num_new();
    bool is_sub = false;
    bool ok = false;

    if (!expr || !positive_out || !negative_out || !scale_out)
        goto cleanup;

    num_destroy(&scale);
    scale = num_clone(NUM_ONE);
    if (expr_match_scaled_expr(expr, &scale, &base) && base && base != expr) {
        if (!expr_match_add_sub_expr(base, &positive, &negative, &is_sub))
            goto cleanup;
        if (!is_sub && negative &&
            negative->ops && negative->ops->kind == EXPR_KIND_NEG &&
            negative->a) {
            negative = negative->a;
            is_sub = true;
        }
        if (!is_sub &&
            expr_match_scaled_expr(negative, &add_negative_scale,
                                   &negative_base) &&
            negative_base && negative_base != negative &&
            num_eq(add_negative_scale, NUM_NEG_ONE)) {
            negative = negative_base;
            is_sub = true;
        }
        if (!is_sub)
            goto cleanup;
        *positive_out = positive;
        *negative_out = negative;
        num_destroy(scale_out);
        *scale_out = num_clone(scale);
        ok = true;
        goto cleanup;
    }

    if (!expr_match_add_sub_expr(expr, &positive, &negative, &is_sub))
        goto cleanup;
    if (!is_sub && negative &&
        negative->ops && negative->ops->kind == EXPR_KIND_NEG &&
        negative->a) {
        negative = negative->a;
        is_sub = true;
    }
    if (!is_sub &&
        expr_match_scaled_expr(negative, &add_negative_scale,
                               &negative_base) &&
        negative_base && negative_base != negative &&
        num_eq(add_negative_scale, NUM_NEG_ONE)) {
        negative = negative_base;
        is_sub = true;
    }
    if (!is_sub)
        goto cleanup;

    if (expr_match_scaled_expr(positive, &positive_scale, &positive_base) &&
        positive_base && positive_base != positive &&
        expr_match_scaled_expr(negative, &negative_scale, &negative_base) &&
        negative_base && negative_base != negative &&
        num_eq(positive_scale, negative_scale)) {
        *positive_out = positive_base;
        *negative_out = negative_base;
        num_destroy(scale_out);
        *scale_out = num_clone(positive_scale);
        ok = true;
        goto cleanup;
    }

    *positive_out = positive;
    *negative_out = negative;
    num_destroy(scale_out);
    *scale_out = num_clone(NUM_ONE);
    ok = true;

cleanup:
    num_destroy(&negative_scale);
    num_destroy(&positive_scale);
    num_destroy(&add_negative_scale);
    num_destroy(&scale);
    return ok;
}

static expr_t *integrate_div_scaled_quadratic_power_derivative_form(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *denominator,
    unsigned int candidate_power)
{
    const expr_t *positive = NULL;
    const expr_t *negative = NULL;
    const expr_t *a_expr = NULL;
    const expr_t *b_expr = NULL;
    expr_t *numerator_view = NULL;
    expr_t *denominator_deriv = NULL;
    expr_t *anti_numerator = NULL;
    expr_t *anti_denominator = NULL;
    expr_t *out = NULL;
    number_t scale = num_new();
    number_t candidate_power_number = num_new();
    number_t anti_scale = num_new();
    number_t *a_poly = NULL;
    number_t *b_poly = NULL;
    number_t *b_deriv = NULL;
    number_t *scaled_a = NULL;
    number_t *anti_poly = NULL;
    size_t poly_count = (size_t)candidate_power + 1u;
    size_t a_max_degree = candidate_power > 0u
                              ? (size_t)candidate_power - 1u
                              : 0u;
    size_t b_max_degree = (size_t)candidate_power;
    number_t power_number = num_new();
    bool ok = false;

    if (!expr || !expr->a || !wrt || !denominator || candidate_power == 0u)
        goto cleanup_uninit;
    if (candidate_power > 64u)
        goto cleanup_uninit;

    a_poly = integrate_number_array_alloc_local(poly_count);
    b_poly = integrate_number_array_alloc_local(poly_count);
    b_deriv = integrate_number_array_alloc_local(poly_count);
    scaled_a = integrate_number_array_alloc_local(poly_count);
    anti_poly = integrate_number_array_alloc_local(poly_count);
    if (!a_poly || !b_poly || !b_deriv || !scaled_a || !anti_poly)
        goto cleanup;

    denominator_deriv = expr_create_deriv(denominator, wrt);
    denominator_deriv = simplify_owned(denominator_deriv);
    if (!denominator_deriv)
        goto cleanup;

    if (!integrate_extract_common_scaled_sub_terms(expr->a, &positive,
                                                  &negative, &scale)) {
        numerator_view = simplify_owned(expr_clone(expr->a));
        if (!integrate_extract_common_scaled_sub_terms(numerator_view,
                                                       &positive,
                                                       &negative, &scale))
        {
            goto cleanup;
        }
    }
    if (!integrate_product_with_factor_local(positive, denominator, wrt,
                                             &a_expr)) {
        goto cleanup;
    }
    if (!integrate_product_with_factor_local(negative, denominator_deriv, wrt,
                                             &b_expr)) {
        goto cleanup;
    }
    if (!integrate_collect_numeric_poly_local(a_expr, wrt, a_max_degree,
                                              a_poly)) {
        goto cleanup;
    }
    if (!integrate_collect_numeric_poly_local(b_expr, wrt, b_max_degree,
                                              b_poly)) {
        goto cleanup;
    }

    for (size_t i = 1u; i < poly_count; ++i) {
        number_t factor = num_create_from_long((long)i);

        num_destroy(&b_deriv[i - 1u]);
        b_deriv[i - 1u] = num_mul(factor, b_poly[i]);
        num_destroy(&factor);
    }

    num_destroy(&candidate_power_number);
    candidate_power_number = num_create_from_long((long)candidate_power);
    if (!integrate_poly_scale_local(a_poly, candidate_power_number, scaled_a,
                                    poly_count))
        goto cleanup;

    ok = true;
    for (size_t i = 0u; i < poly_count; ++i)
        ok = ok && num_eq(b_deriv[i], scaled_a[i]);
    if (!ok) {
        goto cleanup;
    }

    num_destroy(&anti_scale);
    anti_scale = num_div(scale, candidate_power_number);
    if (!integrate_poly_scale_local(b_poly, anti_scale, anti_poly,
                                    poly_count))
        goto cleanup;

    anti_numerator = quotient_power_build_flat_polynomial_expr(wrt, anti_poly,
                                                               poly_count);
    if (candidate_power == 1u) {
        expr_retain((expr_t *)denominator);
        anti_denominator = (expr_t *)denominator;
    } else {
        num_destroy(&power_number);
        power_number = num_create_from_long((long)candidate_power);
        anti_denominator = expr_pow(denominator, &power_number);
    }
    out = (anti_numerator && anti_denominator)
              ? expr_div(anti_numerator, anti_denominator)
              : NULL;

cleanup:
    expr_free(anti_denominator);
    expr_free(anti_numerator);
    expr_free(denominator_deriv);
    expr_free(numerator_view);
    integrate_number_array_free_local(anti_poly, poly_count);
    integrate_number_array_free_local(scaled_a, poly_count);
    integrate_number_array_free_local(b_deriv, poly_count);
    integrate_number_array_free_local(b_poly, poly_count);
    integrate_number_array_free_local(a_poly, poly_count);
cleanup_uninit:
    num_destroy(&power_number);
    num_destroy(&anti_scale);
    num_destroy(&candidate_power_number);
    num_destroy(&scale);
    return out;
}

static expr_t *integrate_div_quotient_derivative(const expr_t *expr,
                                                 const expr_t *wrt)
{
    const expr_t *denominator = NULL;
    const expr_t *positive_term = NULL;
    const expr_t *negative_term = NULL;
    const expr_t *negative_base = NULL;
    const expr_t *scaled_numerator_base = NULL;
    bool is_subtraction = false;
    bool is_factored_power_derivative = false;
    unsigned int denominator_power = 0u;
    unsigned int candidate_power = 0u;
    expr_t *denominator_deriv = NULL;
    expr_t *scaled_denominator_deriv = NULL;
    expr_t *numerator = NULL;
    expr_t *numerator_deriv = NULL;
    expr_t *expected_positive = NULL;
    expr_t *scaled_positive_term = NULL;
    expr_t *candidate_denominator = NULL;
    expr_t *candidate = NULL;
    expr_t *out = NULL;
    number_t candidate_power_number = num_new();
    number_t numerator_scale = num_new();

    if (!expr || !expr->a || !expr->b ||
        !match_positive_integer_power_of_expr(expr->b,
                                              &denominator,
                                              &denominator_power) ||
        denominator_power < 2u ||
        !denominator || !depends_on_wrt(denominator, wrt)) {
        goto cleanup;
    }
    candidate_power = denominator_power - 1u;

    out = integrate_div_scaled_quadratic_power_derivative_form(
        expr, wrt, denominator, candidate_power);
    if (out)
        goto cleanup;

    out = integrate_div_quotient_power_polynomial_derivative(
        expr, wrt, denominator, candidate_power);
    if (out)
        goto cleanup;

    if (!expr_match_add_sub_expr(expr->a, &positive_term, &negative_term,
                                 &is_subtraction)) {
        number_t power_scale = num_create_from_long((long)candidate_power);

        if (expr_match_scaled_expr(expr->a, &numerator_scale,
                                   &scaled_numerator_base) &&
            scaled_numerator_base &&
            scaled_numerator_base != expr->a &&
            num_eq(numerator_scale, power_scale) &&
            expr_match_add_sub_expr(scaled_numerator_base,
                                    &positive_term,
                                    &negative_term,
                                    &is_subtraction)) {
            is_factored_power_derivative = true;
        } else {
            num_destroy(&power_scale);
            goto cleanup;
        }
        num_destroy(&power_scale);
    }
    if (!is_subtraction) {
        if (negative_term->ops &&
            negative_term->ops->kind == EXPR_KIND_NEG && negative_term->a) {
            negative_term = negative_term->a;
            is_subtraction = true;
        } else if (positive_term->ops &&
                   positive_term->ops->kind == EXPR_KIND_NEG &&
                   positive_term->a) {
            const expr_t *negated_left = positive_term->a;

            positive_term = negative_term;
            negative_term = negated_left;
            is_subtraction = true;
        }
    }
    if (negative_term &&
        negative_term->ops &&
        negative_term->ops->kind == EXPR_KIND_NEG &&
        negative_term->a) {
        negative_term = negative_term->a;
    }
    if (!is_subtraction || !positive_term || !negative_term)
        goto cleanup;

    denominator_deriv = expr_create_deriv(denominator, wrt);
    denominator_deriv = simplify_owned(denominator_deriv);
    if (!denominator_deriv || expr_is_exact_zero(denominator_deriv))
        goto cleanup;

    if (is_factored_power_derivative) {
        scaled_denominator_deriv = expr_clone(denominator_deriv);
    } else if (candidate_power > 1u) {
        number_t scale = num_create_from_long((long)candidate_power);

        scaled_denominator_deriv = mul_number_owned(expr_clone(denominator_deriv),
                                                    scale);
        scaled_denominator_deriv = simplify_owned(scaled_denominator_deriv);
        num_destroy(&scale);
    } else {
        scaled_denominator_deriv = expr_clone(denominator_deriv);
    }
    if (!scaled_denominator_deriv)
        goto cleanup;

    if (candidate_power == 1u &&
        match_square_of_expr(negative_term, &negative_base) &&
        negative_base &&
        integrate_expr_equivalent_by_zero_difference(negative_base,
                                                     denominator_deriv)) {
        expr_retain(negative_base);
        numerator = (expr_t *)negative_base;
    } else {
        numerator = expr_simplify_extract_exact_factor_quotient(
            negative_term, scaled_denominator_deriv);
        numerator = simplify_owned(numerator);
        if (!numerator) {
            expr_t *trial = expr_div(negative_term, scaled_denominator_deriv);
            expr_t *trial_product = NULL;

            trial = simplify_owned(trial);
            trial_product = trial
                                ? expr_mul(trial, scaled_denominator_deriv)
                                : NULL;
            trial_product = simplify_owned(trial_product);
            if (trial && trial_product &&
                integrate_expr_equivalent_by_zero_difference(trial_product,
                                                             negative_term)) {
                numerator = trial;
                trial = NULL;
            }
            expr_free(trial_product);
            expr_free(trial);
        }
    }
    if (!numerator)
        goto cleanup;

    numerator_deriv = expr_create_deriv(numerator, wrt);
    numerator_deriv = simplify_owned(numerator_deriv);
    expected_positive = numerator_deriv
                            ? expr_mul(numerator_deriv, denominator)
                            : NULL;
    expected_positive = simplify_owned(expected_positive);
    if (is_factored_power_derivative) {
        scaled_positive_term = mul_number_owned(expr_clone(positive_term),
                                                numerator_scale);
    }
    if (!expected_positive ||
        !integrate_expr_equivalent_by_zero_difference(
            is_factored_power_derivative ? scaled_positive_term : positive_term,
                                                       expected_positive)) {
        goto cleanup;
    }

    if (candidate_power == 1u) {
        expr_retain((expr_t *)denominator);
        candidate_denominator = (expr_t *)denominator;
    } else {
        num_destroy(&candidate_power_number);
        candidate_power_number = num_create_from_long((long)candidate_power);
        candidate_denominator = expr_pow(denominator, &candidate_power_number);
        candidate_denominator = simplify_owned(candidate_denominator);
    }
    if (!candidate_denominator)
        goto cleanup;

    candidate = expr_div(numerator, candidate_denominator);
    candidate = simplify_owned(candidate);
    if (candidate) {
        out = candidate;
        candidate = NULL;
    }

cleanup:
    if (!out && denominator && candidate_power > 0u)
        out = integrate_div_quotient_power_polynomial_derivative(
            expr, wrt, denominator, candidate_power);
    expr_free(candidate);
    expr_free(candidate_denominator);
    expr_free(scaled_positive_term);
    expr_free(expected_positive);
    expr_free(numerator_deriv);
    expr_free(numerator);
    expr_free(scaled_denominator_deriv);
    expr_free(denominator_deriv);
    num_destroy(&numerator_scale);
    num_destroy(&candidate_power_number);
    return out;
}

static void quotient_power_add_number(number_t *target, number_t value)
{
    number_t next = num_add(*target, value);

    num_destroy(target);
    *target = next;
}

static void quotient_power_swap_numbers(number_t *left, number_t *right)
{
    number_t tmp = *left;

    *left = *right;
    *right = tmp;
}

static number_t *integrate_number_array_alloc_local(size_t count)
{
    number_t *values = calloc(count, sizeof(*values));

    if (!values)
        return NULL;
    number_array_zero_local(values, count);
    return values;
}

static void integrate_number_array_free_local(number_t *values, size_t count)
{
    if (!values)
        return;
    number_array_clear_local(values, count);
    free(values);
}

static number_t *integrate_number_matrix_alloc_local(size_t rows,
                                                     size_t cols)
{
    return integrate_number_array_alloc_local(rows * cols);
}

static inline number_t *integrate_matrix_cell_local(number_t *matrix,
                                                    size_t cols,
                                                    size_t row,
                                                    size_t col)
{
    return &matrix[row * cols + col];
}

static bool quotient_power_number_is_effectively_zero(number_t value)
{
    if (num_is_zero(value))
        return true;
    if (!num_is_finite(value))
        return false;
    return fabs(num_to_double(value)) <= 1.0e-24;
}

static bool quotient_power_solve_linear_system(size_t rows,
                                               size_t cols,
                                               number_t *matrix,
                                               number_t *solution)
{
    size_t matrix_cols = cols + 1u;
    size_t *pivot_rows = calloc(cols, sizeof(*pivot_rows));
    bool *has_pivot = calloc(cols, sizeof(*has_pivot));
    size_t pivot_row = 0u;
    bool ok = false;

    if (!matrix || !solution || !pivot_rows || !has_pivot)
        goto cleanup;

    for (size_t col = 0u; col < cols; ++col) {
        pivot_rows[col] = 0u;
        has_pivot[col] = false;
    }

    for (size_t col = 0u; col < cols && pivot_row < rows; ++col) {
        size_t selected = rows;

        for (size_t row = pivot_row; row < rows; ++row) {
            if (!quotient_power_number_is_effectively_zero(
                    *integrate_matrix_cell_local(matrix, matrix_cols,
                                                 row, col))) {
                selected = row;
                break;
            }
        }
        if (selected == rows)
            continue;

        if (selected != pivot_row) {
            for (size_t c = col; c <= cols; ++c)
                quotient_power_swap_numbers(
                    integrate_matrix_cell_local(matrix, matrix_cols,
                                                pivot_row, c),
                    integrate_matrix_cell_local(matrix, matrix_cols,
                                                selected, c));
        }

        {
            number_t pivot = num_clone(*integrate_matrix_cell_local(
                matrix, matrix_cols, pivot_row, col));

            for (size_t c = col; c <= cols; ++c) {
                number_t *cell = integrate_matrix_cell_local(
                    matrix, matrix_cols, pivot_row, c);
                number_t normalized = num_div(*cell, pivot);

                num_destroy(cell);
                *cell = normalized;
            }
            num_destroy(&pivot);
        }

        for (size_t row = 0u; row < rows; ++row) {
            if (row == pivot_row ||
                quotient_power_number_is_effectively_zero(
                    *integrate_matrix_cell_local(matrix, matrix_cols,
                                                 row, col)))
                continue;

            {
                number_t factor = num_clone(*integrate_matrix_cell_local(
                    matrix, matrix_cols, row, col));

                for (size_t c = col; c <= cols; ++c) {
                    number_t *cell = integrate_matrix_cell_local(
                        matrix, matrix_cols, row, c);
                    number_t scaled = num_mul(
                        factor,
                        *integrate_matrix_cell_local(matrix, matrix_cols,
                                                     pivot_row, c));
                    number_t reduced = num_sub(*cell, scaled);

                    num_destroy(cell);
                    *cell = reduced;
                    num_destroy(&scaled);
                }
                num_destroy(&factor);
            }
        }

        has_pivot[col] = true;
        pivot_rows[col] = pivot_row;
        ++pivot_row;
    }

    for (size_t row = 0u; row < rows; ++row) {
        bool all_zero = true;

        for (size_t col = 0u; col < cols; ++col) {
            if (!quotient_power_number_is_effectively_zero(
                    *integrate_matrix_cell_local(matrix, matrix_cols,
                                                 row, col))) {
                all_zero = false;
                break;
            }
        }
        if (all_zero &&
            !quotient_power_number_is_effectively_zero(
                *integrate_matrix_cell_local(matrix, matrix_cols,
                                             row, cols)))
            goto cleanup;
    }

    for (size_t col = 0u; col < cols; ++col) {
        num_destroy(&solution[col]);
        solution[col] = has_pivot[col]
                            ? num_clone(*integrate_matrix_cell_local(
                                  matrix, matrix_cols, pivot_rows[col], cols))
                            : num_clone(NUM_ZERO);
    }
    ok = true;

cleanup:
    free(has_pivot);
    free(pivot_rows);
    return ok;
}

static void quotient_power_normalize_near_integer_solution(number_t *solution,
                                                           size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        double value = num_to_double(solution[i]);
        double rounded = round(value);

        if (!isfinite(value) ||
            fabs(value - rounded) > 1.0e-9 ||
            rounded < (double)LONG_MIN ||
            rounded > (double)LONG_MAX) {
            continue;
        }

        {
            number_t exact = num_create_from_long((long)rounded);

            num_destroy(&solution[i]);
            solution[i] = exact;
        }
    }
}

static void integrate_normalize_small_rational_local(number_t *value)
{
    double value_d;

    if (!value || !num_is_real(*value) || !num_is_finite(*value))
        return;

    value_d = num_to_double(*value);
    if (!isfinite(value_d))
        return;

    for (long denominator = 1L; denominator <= 64L; ++denominator) {
        long numerator = lround(value_d * (double)denominator);
        number_t candidate;
        number_t diff;
        number_t abs_diff;
        double error;
        double scale;

        if (numerator < -1024L || numerator > 1024L)
            continue;

        candidate = num_create_from_frac(numerator, denominator);
        diff = num_sub(*value, candidate);
        abs_diff = num_abs(diff);
        error = fabs(num_to_double(abs_diff));
        scale = fmax(1.0, fabs(value_d));
        num_destroy(&abs_diff);
        num_destroy(&diff);

        if (error <= scale * 1.0e-30) {
            num_destroy(value);
            *value = candidate;
            return;
        }
        num_destroy(&candidate);
    }
}

static expr_t *quotient_power_build_flat_polynomial_expr(const expr_t *var,
                                                         const number_t *coeffs,
                                                         size_t count)
{
    expr_t *sum = NULL;

    if (!var || !coeffs)
        return NULL;

    for (size_t i = count; i-- > 0u;) {
        expr_t *base = NULL;
        expr_t *term = NULL;

        if (num_is_zero(coeffs[i]))
            continue;

        if (i == 0u) {
            term = expr_new_const(coeffs[i]);
        } else {
            if (i == 1u) {
                base = expr_retain_expr(var);
            } else {
                number_t exponent = num_create_from_long((long)i);

                base = expr_pow(var, &exponent);
                num_destroy(&exponent);
            }
            term = base ? expr_mul_num(base, &coeffs[i]) : NULL;
            expr_free(base);
        }

        if (!term) {
            expr_free(sum);
            return NULL;
        }
        if (sum) {
            expr_t *next = expr_add(sum, term);

            expr_free(sum);
            expr_free(term);
            sum = next;
        } else {
            sum = term;
        }
    }

    return sum ? sum : expr_new_const(NUM_ZERO);
}

static size_t quotient_power_polynomial_degree(const number_t *coeffs,
                                               size_t count)
{
    size_t degree = count ? count - 1u : 0u;

    while (degree > 0u && num_is_zero(coeffs[degree]))
        --degree;
    return degree;
}

static void integrate_poly_copy_local(const number_t *src,
                                      number_t *dst,
                                      size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool integrate_poly_add_sub_local(const number_t *left,
                                         const number_t *right,
                                         bool subtract,
                                         number_t *out,
                                         size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        number_t value = subtract ? num_sub(left[i], right[i])
                                  : num_add(left[i], right[i]);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static bool integrate_poly_scale_local(const number_t *src,
                                       number_t scale,
                                       number_t *out,
                                       size_t count)
{
    if (!num_is_real(scale))
        return false;

    for (size_t i = 0u; i < count; ++i) {
        number_t value = num_mul(src[i], scale);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static bool integrate_poly_mul_local(const number_t *left,
                                     const number_t *right,
                                     number_t *out,
                                     size_t max_degree)
{
    size_t count = max_degree + 1u;
    size_t product_count = max_degree * 2u + 1u;
    number_t *tmp = NULL;
    bool ok = true;

    if (max_degree > 64u)
        return false;

    tmp = integrate_number_array_alloc_local(product_count);
    if (!tmp)
        return false;
    for (size_t i = 0u; i < count; ++i) {
        for (size_t j = 0u; j < count; ++j) {
            number_t product = num_mul(left[i], right[j]);
            number_t next = num_add(tmp[i + j], product);

            num_destroy(&tmp[i + j]);
            tmp[i + j] = next;
            num_destroy(&product);
        }
    }

    for (size_t i = count; i < product_count; ++i)
        ok = ok && num_is_zero(tmp[i]);
    if (ok)
        integrate_poly_copy_local(tmp, out, count);
    integrate_number_array_free_local(tmp, product_count);
    return ok;
}

static bool integrate_collect_numeric_poly_local(const expr_t *expr,
                                                const expr_t *wrt,
                                                size_t max_degree,
                                                number_t *out)
{
    size_t count = max_degree + 1u;
    expr_t *vars[1];
    size_t var_index = 0u;
    number_t value = num_new();
    number_t scale = num_new();
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t *left_poly = NULL;
    number_t *right_poly = NULL;
    bool is_sub = false;
    unsigned int exponent = 0u;
    bool ok = false;

    if (!expr || !wrt || !out || max_degree > 64u)
        goto cleanup_value;

    vars[0] = (expr_t *)wrt;
    if (max_degree >= 1u &&
        expr_match_var_expr(expr, 1u, vars, &var_index) &&
        var_index == 0u) {
        number_array_reset_zero_local(out, count);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_value;
    }

    if (expr_match_const_value(expr, &value) && num_is_real(value)) {
        number_array_reset_zero_local(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    left_poly = integrate_number_array_alloc_local(count);
    right_poly = integrate_number_array_alloc_local(count);
    if (!left_poly || !right_poly)
        goto cleanup_value;

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &base)) {
        ok = integrate_collect_numeric_poly_local(base, wrt, max_degree,
                                                  left_poly) &&
             integrate_poly_scale_local(left_poly, NUM_NEG_ONE, out, count);
    } else if (expr_match_scaled_expr(expr, &scale, &base) && base &&
               base != expr) {
        ok = integrate_collect_numeric_poly_local(base, wrt, max_degree,
                                                  left_poly) &&
             integrate_poly_scale_local(left_poly, scale, out, count);
    } else if (match_positive_integer_power_of_expr(expr, &base,
                                                    &exponent) &&
               exponent <= max_degree) {
        number_t *product = integrate_number_array_alloc_local(count);

        if (!product)
            goto cleanup_poly;
        number_array_reset_zero_local(product, count);
        num_destroy(&product[0]);
        product[0] = num_clone(NUM_ONE);
        ok = integrate_collect_numeric_poly_local(base, wrt, max_degree,
                                                  left_poly);
        for (size_t i = 0u; ok && i < exponent; ++i) {
            ok = integrate_poly_mul_local(product, left_poly, right_poly,
                                          max_degree);
            if (ok)
                integrate_poly_copy_local(right_poly, product, count);
        }
        if (ok)
            integrate_poly_copy_local(product, out, count);
        integrate_number_array_free_local(product, count);
    } else if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        ok = integrate_collect_numeric_poly_local(left, wrt, max_degree,
                                                  left_poly) &&
             integrate_collect_numeric_poly_local(right, wrt, max_degree,
                                                  right_poly) &&
             integrate_poly_add_sub_local(left_poly, right_poly, is_sub,
                                          out, count);
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = integrate_collect_numeric_poly_local(left, wrt, max_degree,
                                                  left_poly) &&
             integrate_collect_numeric_poly_local(right, wrt, max_degree,
                                                  right_poly) &&
             integrate_poly_mul_local(left_poly, right_poly, out, max_degree);
    }

cleanup_poly:
    integrate_number_array_free_local(right_poly, count);
    integrate_number_array_free_local(left_poly, count);

cleanup_value:
    num_destroy(&scale);
    num_destroy(&value);
    return ok;
}

static expr_t *integrate_div_quotient_power_polynomial_derivative(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *denominator,
    unsigned int candidate_power)
{
    enum { MAX_QUOTIENT_POWER_DEGREE = 64 };
    expr_t *vars[1];
    number_t *numerator = NULL;
    number_t *denom = NULL;
    number_t *solution = NULL;
    number_t *reconstructed = NULL;
    number_t *matrix = NULL;
    size_t coeff_count = MAX_QUOTIENT_POWER_DEGREE + 1u;
    size_t matrix_cols = 0u;
    number_t numerator_constant = num_new();
    number_t numerator_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    size_t numerator_degree = 0u;
    size_t denom_degree = 0u;
    size_t anti_degree = 0u;
    size_t rows = 0u;
    size_t cols = 0u;
    expr_t *anti_numerator = NULL;
    expr_t *anti_denominator = NULL;
    expr_t *candidate = NULL;
    expr_t *candidate_deriv = NULL;
    expr_t *out = NULL;
    number_t power_number = num_new();
    bool numerator_ok = false;
    bool denom_ok = false;
    bool algebra_ok = false;

    if (!expr || !expr->a || !expr->b || !wrt || !denominator ||
        candidate_power == 0u)
        goto cleanup_uninit;
    if (candidate_power > MAX_QUOTIENT_POWER_DEGREE)
        goto cleanup_uninit;

    vars[0] = (expr_t *)wrt;
    numerator = integrate_number_array_alloc_local(coeff_count);
    denom = integrate_number_array_alloc_local(coeff_count);
    reconstructed = integrate_number_array_alloc_local(coeff_count);
    if (!numerator || !denom || !reconstructed)
        goto cleanup;

    denom_ok = integrate_collect_numeric_poly_local(
        denominator, wrt, MAX_QUOTIENT_POWER_DEGREE, denom);
    if (denom_ok) {
        num_destroy(&denom_constant);
        denom_constant = num_clone(NUM_ZERO);
        num_destroy(&denom_coeff);
        denom_coeff = num_clone(NUM_ONE);
    } else {
        denom_ok = expr_match_affine_poly_deg4(denominator, 1u, vars, denom,
                                               &denom_constant, &denom_coeff);
    }

    if (!(denom_ok &&
          num_eq(denom_constant, NUM_ZERO) &&
          num_eq(denom_coeff, NUM_ONE)))
        goto cleanup;

    denom_degree = quotient_power_polynomial_degree(denom, coeff_count);
    if (denom_degree == 0u || denom_degree > 2u)
        goto cleanup;

    numerator_ok = integrate_collect_numeric_poly_local(
        expr->a, wrt, MAX_QUOTIENT_POWER_DEGREE, numerator);
    if (numerator_ok) {
        num_destroy(&numerator_constant);
        numerator_constant = num_clone(NUM_ZERO);
        num_destroy(&numerator_coeff);
        numerator_coeff = num_clone(NUM_ONE);
    } else {
        numerator_ok = expr_match_affine_poly_deg4(expr->a, 1u, vars,
                                                   numerator,
                                                   &numerator_constant,
                                                   &numerator_coeff);
    }

    if (!(numerator_ok &&
          num_eq(numerator_constant, NUM_ZERO) &&
          num_eq(numerator_coeff, NUM_ONE)))
        goto cleanup;

    numerator_degree = quotient_power_polynomial_degree(numerator,
                                                        coeff_count);

    anti_degree = (numerator_degree + 1u >= denom_degree)
                      ? numerator_degree + 1u - denom_degree
                      : 0u;
    if (anti_degree > MAX_QUOTIENT_POWER_DEGREE)
        goto cleanup;

    rows = numerator_degree > anti_degree + denom_degree - 1u
               ? numerator_degree + 1u
               : anti_degree + denom_degree;
    cols = anti_degree + 1u;
    if (rows > coeff_count || cols > coeff_count)
        goto cleanup;
    matrix_cols = cols + 1u;
    solution = integrate_number_array_alloc_local(cols);
    matrix = integrate_number_matrix_alloc_local(rows, matrix_cols);
    if (!solution || !matrix)
        goto cleanup;

    for (size_t row = 0u; row < rows; ++row) {
        number_t *rhs = integrate_matrix_cell_local(matrix, matrix_cols,
                                                    row, cols);

        num_destroy(rhs);
        *rhs = num_clone(numerator[row]);

        for (size_t ai = 0u; ai < cols; ++ai) {
            if (ai > 0u && row >= ai - 1u) {
                size_t q = row - (ai - 1u);

                if (q <= denom_degree && !num_is_zero(denom[q])) {
                    number_t scale = num_create_from_long((long)ai);
                    number_t term = num_mul(scale, denom[q]);

                    quotient_power_add_number(
                        integrate_matrix_cell_local(matrix, matrix_cols,
                                                    row, ai),
                        term);
                    num_destroy(&term);
                    num_destroy(&scale);
                }
            }

            if (row >= ai) {
                size_t q = row - ai + 1u;

                if (q > 0u && q <= denom_degree && !num_is_zero(denom[q])) {
                    long signed_scale = -((long)candidate_power * (long)q);
                    number_t scale = num_create_from_long(signed_scale);
                    number_t term = num_mul(scale, denom[q]);

                    quotient_power_add_number(
                        integrate_matrix_cell_local(matrix, matrix_cols,
                                                    row, ai),
                        term);
                    num_destroy(&term);
                    num_destroy(&scale);
                }
            }
        }
    }

    if (!quotient_power_solve_linear_system(rows, cols, matrix, solution))
        goto cleanup;
    quotient_power_normalize_near_integer_solution(solution, cols);

    for (size_t ai = 0u; ai < cols; ++ai) {
        for (size_t q = 0u; q <= denom_degree; ++q) {
            if (ai > 0u && q + ai - 1u < coeff_count &&
                !num_is_zero(denom[q])) {
                number_t ai_scale = num_create_from_long((long)ai);
                number_t scaled = num_mul(ai_scale, solution[ai]);
                number_t term = num_mul(scaled, denom[q]);

                quotient_power_add_number(&reconstructed[q + ai - 1u],
                                          term);
                num_destroy(&term);
                num_destroy(&scaled);
                num_destroy(&ai_scale);
            }
            if (q > 0u && q + ai - 1u < coeff_count &&
                !num_is_zero(denom[q])) {
                long signed_scale = -((long)candidate_power * (long)q);
                number_t power_scale = num_create_from_long(signed_scale);
                number_t scaled = num_mul(power_scale, solution[ai]);
                number_t term = num_mul(scaled, denom[q]);

                quotient_power_add_number(&reconstructed[q + ai - 1u],
                                          term);
                num_destroy(&term);
                num_destroy(&scaled);
                num_destroy(&power_scale);
            }
        }
    }
    algebra_ok = true;
    for (size_t i = 0u; i < coeff_count; ++i)
        algebra_ok = algebra_ok && num_eq(reconstructed[i], numerator[i]);

    anti_numerator = quotient_power_build_flat_polynomial_expr(wrt, solution,
                                                               cols);
    if (candidate_power == 1u) {
        expr_retain((expr_t *)denominator);
        anti_denominator = (expr_t *)denominator;
    } else {
        num_destroy(&power_number);
        power_number = num_create_from_long((long)candidate_power);
        anti_denominator = expr_pow(denominator, &power_number);
    }
    candidate = anti_numerator && anti_denominator
                    ? expr_div(anti_numerator, anti_denominator)
                    : NULL;
    if (candidate && algebra_ok) {
        out = candidate;
        candidate = NULL;
        goto cleanup;
    }
    candidate = simplify_owned(candidate);
    candidate_deriv = candidate ? expr_create_deriv(candidate, wrt) : NULL;
    candidate_deriv = simplify_owned(candidate_deriv);
    if (candidate && candidate_deriv &&
        integrate_expr_equivalent_by_zero_difference(candidate_deriv, expr)) {
        out = candidate;
        candidate = NULL;
    }

cleanup:
    integrate_number_array_free_local(matrix, rows * matrix_cols);
    integrate_number_array_free_local(reconstructed, coeff_count);
    integrate_number_array_free_local(solution, cols);
    integrate_number_array_free_local(denom, coeff_count);
    integrate_number_array_free_local(numerator, coeff_count);
cleanup_uninit:
    expr_free(candidate_deriv);
    expr_free(candidate);
    expr_free(anti_denominator);
    expr_free(anti_numerator);
    num_destroy(&power_number);
    num_destroy(&denom_coeff);
    num_destroy(&denom_constant);
    num_destroy(&numerator_coeff);
    num_destroy(&numerator_constant);
    return out;
}

static expr_t *integrate_div_cubic_over_quadratic_square(const expr_t *expr,
                                                         const expr_t *wrt)
{
    const expr_t *denominator = NULL;
    unsigned int denominator_power = 0u;
    number_t numer[5];
    number_t denom[5];
    number_t quotient[2];
    number_t remainder[2];
    number_t leading_product = num_new();
    number_t reduced_quadratic = num_new();
    number_t linear_product = num_new();
    number_t constant_product = num_new();
    number_t reconstructed[4];
    expr_t *quotient_numerator = NULL;
    expr_t *remainder_numerator = NULL;
    expr_t *quotient_fraction = NULL;
    expr_t *remainder_fraction = NULL;
    expr_t *quotient_integral = NULL;
    expr_t *remainder_integral = NULL;
    expr_t *sum = NULL;
    expr_t *out = NULL;
    bool arrays_ready = false;

    if (!expr || !expr->a || !expr->b || !wrt ||
        !match_positive_integer_power_of_expr(expr->b,
                                              &denominator,
                                              &denominator_power) ||
        denominator_power != 2u ||
        !denominator)
        goto cleanup;

    number_array_zero_local(numer, 5u);
    number_array_zero_local(denom, 5u);
    number_array_zero_local(quotient, 2u);
    number_array_zero_local(remainder, 2u);
    number_array_zero_local(reconstructed, 4u);
    arrays_ready = true;

    if (!integrate_collect_numeric_poly_local(expr->a, wrt, 4u, numer) ||
        !integrate_collect_numeric_poly_local(denominator, wrt, 4u, denom) ||
        !num_is_zero(numer[4]) ||
        num_is_zero(numer[3]) ||
        !num_is_zero(denom[3]) ||
        !num_is_zero(denom[4]) ||
        num_is_zero(denom[2]))
        goto cleanup;

    num_destroy(&quotient[1]);
    quotient[1] = num_div(numer[3], denom[2]);
    integrate_normalize_small_rational_local(&quotient[1]);

    num_destroy(&leading_product);
    leading_product = num_mul(quotient[1], denom[1]);
    num_destroy(&reduced_quadratic);
    reduced_quadratic = num_sub(numer[2], leading_product);
    num_destroy(&quotient[0]);
    quotient[0] = num_div(reduced_quadratic, denom[2]);
    integrate_normalize_small_rational_local(&quotient[0]);

    num_destroy(&linear_product);
    linear_product = num_mul(quotient[1], denom[0]);
    num_destroy(&constant_product);
    constant_product = num_mul(quotient[0], denom[1]);
    {
        number_t removed_linear = num_add(linear_product, constant_product);

            num_destroy(&remainder[1]);
            remainder[1] = num_sub(numer[1], removed_linear);
            integrate_normalize_small_rational_local(&remainder[1]);
            num_destroy(&removed_linear);
        }

    num_destroy(&constant_product);
    constant_product = num_mul(quotient[0], denom[0]);
    num_destroy(&remainder[0]);
    remainder[0] = num_sub(numer[0], constant_product);
    integrate_normalize_small_rational_local(&remainder[0]);

    {
        number_t q0d1 = num_mul(quotient[0], denom[1]);
        number_t q1d0 = num_mul(quotient[1], denom[0]);

        num_destroy(&reconstructed[0]);
        reconstructed[0] = num_add(constant_product, remainder[0]);
        num_destroy(&reconstructed[1]);
        reconstructed[1] = num_add(q0d1, q1d0);
        {
            number_t with_remainder = num_add(reconstructed[1], remainder[1]);

            num_destroy(&reconstructed[1]);
            reconstructed[1] = with_remainder;
        }
        num_destroy(&reconstructed[2]);
        reconstructed[2] = num_add(leading_product, reduced_quadratic);
        num_destroy(&reconstructed[3]);
        reconstructed[3] = num_mul(quotient[1], denom[2]);
        num_destroy(&q1d0);
        num_destroy(&q0d1);
    }
    for (size_t i = 0u; i < 4u; ++i) {
        if (!num_eq(reconstructed[i], numer[i]))
            goto cleanup;
    }

    quotient_numerator =
        quotient_power_build_flat_polynomial_expr(wrt, quotient, 2u);
    remainder_numerator =
        quotient_power_build_flat_polynomial_expr(wrt, remainder, 2u);
    quotient_fraction = quotient_numerator
                            ? expr_div(quotient_numerator, denominator)
                            : NULL;
    remainder_fraction = remainder_numerator
                             ? expr_div(remainder_numerator, expr->b)
                             : NULL;
    quotient_integral = quotient_fraction
                            ? expr_integrate_dispatch(quotient_fraction, wrt)
                            : NULL;
    remainder_integral = remainder_fraction
                             ? expr_integrate_dispatch(remainder_fraction, wrt)
                             : NULL;
    if (!quotient_integral || !remainder_integral)
        goto cleanup;

    sum = expr_add(quotient_integral, remainder_integral);
    if (sum) {
        out = sum;
        sum = NULL;
    }

cleanup:
    expr_free(sum);
    expr_free(remainder_integral);
    expr_free(quotient_integral);
    expr_free(remainder_fraction);
    expr_free(quotient_fraction);
    expr_free(remainder_numerator);
    expr_free(quotient_numerator);
    if (arrays_ready) {
        number_array_clear_local(reconstructed, 4u);
        number_array_clear_local(remainder, 2u);
        number_array_clear_local(quotient, 2u);
        number_array_clear_local(denom, 5u);
        number_array_clear_local(numer, 5u);
    }
    num_destroy(&constant_product);
    num_destroy(&linear_product);
    num_destroy(&reduced_quadratic);
    num_destroy(&leading_product);
    return out;
}

static expr_t *integrate_div_poly_over_quadratic_square(const expr_t *expr,
                                                        const expr_t *wrt)
{
    const expr_t *denominator = NULL;
    unsigned int denominator_power = 0u;
    number_t numer[5];
    number_t denom[5];
    number_t linear_coeffs[2];
    number_t four = num_create_from_long(4L);
    number_t ac = num_new();
    number_t four_ac = num_new();
    number_t b_sq = num_new();
    number_t delta = num_new();
    number_t ar = num_new();
    number_t two_ar = num_new();
    number_t bq = num_new();
    number_t cp = num_new();
    number_t two_cp = num_new();
    number_t k_num_left = num_new();
    number_t k_num = num_new();
    number_t k = num_new();
    number_t p_over_a = num_new();
    number_t m = num_new();
    number_t bk = num_new();
    number_t bk_minus_q = num_new();
    number_t two_a = num_new();
    number_t n = num_new();
    number_t reconstructed[3];
    expr_t *linear_numerator = NULL;
    expr_t *derivative_part = NULL;
    expr_t *k_expr = NULL;
    expr_t *k_over_denominator = NULL;
    expr_t *quadratic_part = NULL;
    expr_t *sum = NULL;
    expr_t *out = NULL;
    bool numer_ok = false;
    bool denom_ok = false;
    bool reconstructed_ready = false;

    if (!expr || !expr->a || !expr->b || !wrt ||
        !match_positive_integer_power_of_expr(expr->b,
                                              &denominator,
                                              &denominator_power) ||
        denominator_power != 2u ||
        !denominator) {
        goto cleanup_uninit;
    }

    number_array_zero_local(numer, 5u);
    number_array_zero_local(denom, 5u);
    number_array_zero_local(linear_coeffs, 2u);
    number_array_zero_local(reconstructed, 3u);
    reconstructed_ready = true;

    numer_ok = integrate_collect_numeric_poly_local(expr->a, wrt, 4u, numer);
    denom_ok =
        integrate_collect_numeric_poly_local(denominator, wrt, 4u, denom);

    if (!numer_ok ||
        !denom_ok ||
        !num_is_zero(numer[3]) ||
        !num_is_zero(numer[4]) ||
        !num_is_zero(denom[3]) ||
        !num_is_zero(denom[4]) ||
        num_is_zero(denom[2])) {
        goto cleanup;
    }

    num_destroy(&ac);
    ac = num_mul(denom[2], denom[0]);
    num_destroy(&four_ac);
    four_ac = num_mul(four, ac);
    num_destroy(&b_sq);
    b_sq = num_mul(denom[1], denom[1]);
    num_destroy(&delta);
    delta = num_sub(four_ac, b_sq);
    if (num_is_zero(delta))
        goto cleanup;

    num_destroy(&ar);
    ar = num_mul(denom[2], numer[0]);
    num_destroy(&two_ar);
    two_ar = num_mul(NUM_TWO, ar);
    num_destroy(&bq);
    bq = num_mul(denom[1], numer[1]);
    num_destroy(&cp);
    cp = num_mul(denom[0], numer[2]);
    num_destroy(&two_cp);
    two_cp = num_mul(NUM_TWO, cp);
    num_destroy(&k_num_left);
    k_num_left = num_sub(two_ar, bq);
    num_destroy(&k_num);
    k_num = num_add(k_num_left, two_cp);
    num_destroy(&k);
    k = num_div(k_num, delta);
    integrate_normalize_small_rational_local(&k);

    num_destroy(&p_over_a);
    p_over_a = num_div(numer[2], denom[2]);
    integrate_normalize_small_rational_local(&p_over_a);
    num_destroy(&m);
    m = num_sub(k, p_over_a);
    integrate_normalize_small_rational_local(&m);

    num_destroy(&bk);
    bk = num_mul(denom[1], k);
    num_destroy(&bk_minus_q);
    bk_minus_q = num_sub(bk, numer[1]);
    num_destroy(&two_a);
    two_a = num_mul(NUM_TWO, denom[2]);
    num_destroy(&n);
    n = num_div(bk_minus_q, two_a);
    integrate_normalize_small_rational_local(&n);

    {
        number_t k_minus_m = num_sub(k, m);
        number_t bk_check = num_mul(denom[1], k);
        number_t two_an = num_mul(two_a, n);
        number_t m_plus_k = num_add(m, k);
        number_t c_m_plus_k = num_mul(denom[0], m_plus_k);
        number_t bn = num_mul(denom[1], n);

        num_destroy(&reconstructed[2]);
        reconstructed[2] = num_mul(denom[2], k_minus_m);
        num_destroy(&reconstructed[1]);
        reconstructed[1] = num_sub(bk_check, two_an);
        num_destroy(&reconstructed[0]);
        reconstructed[0] = num_sub(c_m_plus_k, bn);

        num_destroy(&bn);
        num_destroy(&c_m_plus_k);
        num_destroy(&m_plus_k);
        num_destroy(&two_an);
        num_destroy(&bk_check);
        num_destroy(&k_minus_m);
    }
    for (size_t i = 0u; i < 3u; ++i) {
        if (!num_eq(reconstructed[i], numer[i]))
            goto cleanup;
    }

    num_destroy(&linear_coeffs[0]);
    linear_coeffs[0] = num_clone(n);
    num_destroy(&linear_coeffs[1]);
    linear_coeffs[1] = num_clone(m);

    linear_numerator =
        quotient_power_build_flat_polynomial_expr(wrt, linear_coeffs, 2u);
    derivative_part = linear_numerator
                          ? expr_div(linear_numerator, denominator)
                          : NULL;

    if (!num_is_zero(k)) {
        k_expr = expr_new_const(k);
        k_over_denominator = k_expr ? expr_div(k_expr, denominator) : NULL;
        k_over_denominator = simplify_owned(k_over_denominator);
        quadratic_part = k_over_denominator
                             ? expr_integrate_dispatch(k_over_denominator, wrt)
                             : NULL;
        quadratic_part = simplify_owned(quadratic_part);
        if (!quadratic_part)
            goto cleanup;
    }

    if (derivative_part && quadratic_part) {
        sum = expr_add(derivative_part, quadratic_part);
    } else if (derivative_part) {
        sum = derivative_part;
        derivative_part = NULL;
    } else if (quadratic_part) {
        sum = quadratic_part;
        quadratic_part = NULL;
    }
    if (sum) {
        out = sum;
        sum = NULL;
    }

cleanup:
    expr_free(sum);
    expr_free(quadratic_part);
    expr_free(k_over_denominator);
    expr_free(k_expr);
    expr_free(derivative_part);
    expr_free(linear_numerator);
    number_array_clear_local(linear_coeffs, 2u);
    number_array_clear_local(denom, 5u);
    number_array_clear_local(numer, 5u);
cleanup_uninit:
    if (reconstructed_ready)
        number_array_clear_local(reconstructed, 3u);
    num_destroy(&n);
    num_destroy(&two_a);
    num_destroy(&bk_minus_q);
    num_destroy(&bk);
    num_destroy(&m);
    num_destroy(&p_over_a);
    num_destroy(&k);
    num_destroy(&k_num);
    num_destroy(&k_num_left);
    num_destroy(&two_cp);
    num_destroy(&cp);
    num_destroy(&bq);
    num_destroy(&two_ar);
    num_destroy(&ar);
    num_destroy(&delta);
    num_destroy(&b_sq);
    num_destroy(&four_ac);
    num_destroy(&ac);
    num_destroy(&four);
    return out;
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

static bool match_one_plus_unit_circle_root(const expr_t *expr,
                                            const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *root = NULL;
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t one = num_new();
    bool is_sub = false;
    bool is_plus_square = false;
    bool ok = false;

    if (!expr || !wrt ||
        !expr_match_add_sub_expr(expr, &left, &right, &is_sub) ||
        is_sub)
        goto cleanup;

    if (expr_match_const_value(left, &one) && num_eq(one, NUM_ONE)) {
        root = right;
    } else if (expr_match_const_value(right, &one) && num_eq(one, NUM_ONE)) {
        root = left;
    } else {
        goto cleanup;
    }

    ok = expr_is_op(root, &ops_sqrt) &&
         root->a &&
         match_one_plus_minus_affine_square(root->a, wrt, &is_plus_square,
                                            &constant, &coeff) &&
         !is_plus_square &&
         num_eq(constant, NUM_ZERO) &&
         (num_eq(coeff, NUM_ONE) || num_eq(coeff, NUM_NEG_ONE));

cleanup:
    num_destroy(&one);
    num_destroy(&coeff);
    num_destroy(&constant);
    return ok;
}

static expr_t *integrate_div_inverse_one_plus_unit_circle_root(const expr_t *expr,
                                                               const expr_t *wrt)
{
    number_t numer_constant = num_new();
    expr_t *asin_x = NULL;
    expr_t *correction = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !expr_match_const_value(expr->a, &numer_constant) ||
        !num_eq(numer_constant, NUM_ONE) ||
        !match_one_plus_unit_circle_root(expr->b, wrt))
        goto cleanup;

    asin_x = expr_asin(wrt);
    correction = expr_div(wrt, expr->b);
    raw = (asin_x && correction) ? expr_sub(asin_x, correction) : NULL;
    out = simplify_owned(raw);
    raw = NULL;

cleanup:
    expr_free(raw);
    expr_free(correction);
    expr_free(asin_x);
    num_destroy(&numer_constant);
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
    unsigned int exponent = 0u;

    return match_positive_integer_power_of_expr(expr, base_out, &exponent) &&
           exponent == 2u;
}

static bool match_positive_integer_power_of_expr(const expr_t *expr,
                                                 const expr_t **base_out,
                                                 unsigned int *exponent_out)
{
    number_t exponent = num_new();
    long numerator = 0;
    long denominator = 0;
    bool ok = false;

    if (!expr || !base_out || !exponent_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a &&
        num_is_real(expr->c) &&
        num_is_integer(expr->c) &&
        num_get_small_rational(expr->c, &numerator, &denominator) &&
        denominator == 1 &&
        numerator > 0) {
        *base_out = expr->a;
        *exponent_out = (unsigned int)numerator;
        ok = true;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) &&
               num_is_real(exponent) &&
               num_is_integer(exponent) &&
               num_get_small_rational(exponent, &numerator, &denominator) &&
               denominator == 1 &&
               numerator > 0) {
        *base_out = expr->a;
        *exponent_out = (unsigned int)numerator;
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
