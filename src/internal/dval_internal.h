#ifndef SRC_INTERNAL_DVAL_INTERNAL_H
#define SRC_INTERNAL_DVAL_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "dval.h"
#include "qcomplex.h"

/**
 * @file dval_internal.h
 * @brief Shared private dval helpers for sibling implementation modules.
 *
 * This header consolidates the structural matchers, parser symbol rules, and
 * general DAG helper utilities and ownership helpers that are shared across
 * dval, matrix, and integrator implementation code.
 */

/* ------------------------------------------------------------------------- */
/* Ownership helpers                                                         */
/* ------------------------------------------------------------------------- */

void dv_retain(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* General helper utilities                                                  */
/* ------------------------------------------------------------------------- */

bool dv_is_exact_zero(const dval_t *dv);
bool dv_is_named_const(const dval_t *dv);
dval_t *dv_substitute(const dval_t *expr,
                      const dval_t *needle,
                      dval_t *replacement);

/* ------------------------------------------------------------------------- */
/* Structural matchers                                                       */
/* ------------------------------------------------------------------------- */

bool dv_match_var_expr(const dval_t *expr,
                       size_t nvars,
                       dval_t *const *vars,
                       size_t *index_out);

bool dv_match_const_value(const dval_t *expr, qfloat_t *value_out);

bool dv_match_scaled_expr(const dval_t *expr,
                          qfloat_t *scale_out,
                          const dval_t **base_out);

bool dv_match_add_sub_expr(const dval_t *expr,
                           const dval_t **left_out,
                           const dval_t **right_out,
                           bool *is_sub_out);

bool dv_match_mul_expr(const dval_t *expr,
                       const dval_t **left_out,
                       const dval_t **right_out);

bool dv_collect_var_usage(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          bool *used_out);

/* ------------------------------------------------------------------------- */
/* Affine / polynomial pattern matchers                                      */
/* ------------------------------------------------------------------------- */

typedef enum {
    DV_PATTERN_UNARY_EXP,
    DV_PATTERN_UNARY_SIN,
    DV_PATTERN_UNARY_COS,
    DV_PATTERN_UNARY_SINH,
    DV_PATTERN_UNARY_COSH
} dv_pattern_unary_affine_kind_t;

bool dv_match_unary_affine_kind(const dval_t *expr,
                                dv_pattern_unary_affine_kind_t unary_kind,
                                size_t nvars,
                                dval_t *const *vars,
                                qfloat_t *constant_out,
                                qfloat_t *coeffs_out);

bool dv_match_affine_poly_deg4(const dval_t *expr,
                               size_t nvars,
                               dval_t *const *vars,
                               qfloat_t *poly_coeffs_out,
                               qfloat_t *constant_out,
                               qfloat_t *coeffs_out);

bool dv_match_affine_poly_deg4_times_unary_affine_kind(const dval_t *expr,
                                                       dv_pattern_unary_affine_kind_t unary_kind,
                                                       size_t nvars,
                                                       dval_t *const *vars,
                                                       qfloat_t *poly_coeffs_out,
                                                       qfloat_t *constant_out,
                                                       qfloat_t *coeffs_out);

/* ------------------------------------------------------------------------- */
/* Symbol normalisation / default constants                                  */
/* ------------------------------------------------------------------------- */

char *dv_normalize_name(const char *name);
char *dv_normalize_binding_name(const char *name);
int dv_is_default_constant_name(const char *name);
int dv_get_default_constant_value(const char *name, qcomplex_t *value_out);
const char *dv_default_constant_canonical_name(const char *name);

#endif
