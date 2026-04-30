#ifndef DVAL_PATTERN_H
#define DVAL_PATTERN_H

#include "dval_helpers.h"

/**
 * @file dval_pattern.h
 * @brief Affine-family pattern matchers for dval expression DAGs.
 *
 * These helpers build on the lighter-weight utilities in `dval_helpers.h` and
 * expose the higher-level affine and polynomial matcher surface used by the
 * integrator.
 */

typedef enum {
    DV_PATTERN_UNARY_EXP,
    DV_PATTERN_UNARY_SIN,
    DV_PATTERN_UNARY_COS,
    DV_PATTERN_UNARY_SINH,
    DV_PATTERN_UNARY_COSH
} dv_pattern_unary_affine_kind_t;

/* ------------------------------------------------------------------------- */
/* Canonical affine-family matchers                                           */
/* ------------------------------------------------------------------------- */

/**
 * @brief Detect whether @p expr is unary_kind(affine(vars)).
 *
 * Supported unary kinds are the `DV_PATTERN_UNARY_*` constants above.
 *
 * Recognised affine forms are built from constants and the nominated
 * variables using addition, subtraction, negation, multiplication by
 * constants, and division by constants.
 */
bool dv_match_unary_affine_kind(const dval_t *expr,
                                dv_pattern_unary_affine_kind_t unary_kind,
                                size_t nvars,
                                dval_t *const *vars,
                                qfloat_t *constant_out,
                                qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is a degree <= 4 polynomial in affine(vars).
 *
 * The matched polynomial is returned in ascending-power order:
 * `poly_coeffs_out[0] + poly_coeffs_out[1] a + ... + poly_coeffs_out[4] a^4`,
 * where `a = constant_out + sum coeffs_out[i] vars[i]`.
 *
 * Constant-only polynomials are accepted. In that case, the affine outputs are
 * written as zero because no affine basis is required.
 */
bool dv_match_affine_poly_deg4(const dval_t *expr,
                               size_t nvars,
                               dval_t *const *vars,
                               qfloat_t *poly_coeffs_out,
                               qfloat_t *constant_out,
                               qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is P(affine(vars)) * unary_kind(affine(vars)),
 * where P is a polynomial of degree at most 4.
 *
 * Supported unary kinds are the `DV_PATTERN_UNARY_*` constants above.
 */
bool dv_match_affine_poly_deg4_times_unary_affine_kind(const dval_t *expr,
                                                       dv_pattern_unary_affine_kind_t unary_kind,
                                                       size_t nvars,
                                                       dval_t *const *vars,
                                                       qfloat_t *poly_coeffs_out,
                                                       qfloat_t *constant_out,
                                                       qfloat_t *coeffs_out);

#endif
