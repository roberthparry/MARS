#ifndef EXPR_SHARED_INTERNAL_H
#define EXPR_SHARED_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "expression.h"

/**
 * @file expr_internal.h
 * @brief Shared private expr helpers for sibling implementation modules.
 *
 * This header consolidates the structural matchers, parser symbol rules, and
 * general DAG helper utilities and ownership helpers that are shared across
 * expr, matrix, and integrator implementation code.
 */

int expr_get_default_constant_num(const char *name, number_t *value_out);

void expr_retain(const expr_t *dv);

/* ------------------------------------------------------------------------- */
/* General helper utilities                                                  */
/* ------------------------------------------------------------------------- */

bool expr_is_exact_zero(const expr_t *dv);
bool expr_is_named_const(const expr_t *dv);
expr_t *expr_substitute(const expr_t *expr,
                      const expr_t *needle,
                      expr_t *replacement);

/* ------------------------------------------------------------------------- */
/* Structural matchers                                                       */
/* ------------------------------------------------------------------------- */

bool expr_match_const_value(const expr_t *expr, number_t *value_out);

bool expr_match_scaled_expr(const expr_t *expr,
                          number_t *scale_out,
                          const expr_t **base_out);

bool expr_match_add_sub_expr(const expr_t *expr,
                           const expr_t **left_out,
                           const expr_t **right_out,
                           bool *is_sub_out);

bool expr_match_mul_expr(const expr_t *expr,
                       const expr_t **left_out,
                       const expr_t **right_out);

bool expr_collect_var_usage(const expr_t *expr,
                          size_t nvars,
                          expr_t *const *vars,
                          bool *used_out);

/* ------------------------------------------------------------------------- */
/* Affine / polynomial pattern matchers                                      */
/* ------------------------------------------------------------------------- */

typedef enum {
    EXPR_PATTERN_UNARY_EXP,
    EXPR_PATTERN_UNARY_SIN,
    EXPR_PATTERN_UNARY_COS,
    EXPR_PATTERN_UNARY_SINH,
    EXPR_PATTERN_UNARY_COSH
} expr_pattern_unary_affine_kind_t;

bool expr_match_unary_affine_kind(const expr_t *expr,
                                expr_pattern_unary_affine_kind_t unary_kind,
                                size_t nvars,
                                expr_t *const *vars,
                                number_t *constant_out,
                                number_t *coeffs_out);

bool expr_match_affine_poly_deg4(const expr_t *expr,
                               size_t nvars,
                               expr_t *const *vars,
                               number_t *poly_coeffs_out,
                               number_t *constant_out,
                               number_t *coeffs_out);

bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                       expr_pattern_unary_affine_kind_t unary_kind,
                                                       size_t nvars,
                                                       expr_t *const *vars,
                                                       number_t *poly_coeffs_out,
                                                       number_t *constant_out,
                                                       number_t *coeffs_out);

/* ------------------------------------------------------------------------- */
/* Symbol normalisation / default constants                                  */
/* ------------------------------------------------------------------------- */

char *expr_normalise_name(const char *name);
char *expr_normalise_binding_name(const char *name);
int expr_is_default_constant_name(const char *name);
const char *expr_default_constant_canonical_name(const char *name);

/* ------------------------------------------------------------------------- */
/* Shared string rendering helpers                                           */
/* ------------------------------------------------------------------------- */

char *expr_tostring_texify(const char *text);
int expr_to_tex_parts(const expr_t *dv, char **expr_out, char **bindings_out);

#endif /* EXPR_SHARED_INTERNAL_H */
