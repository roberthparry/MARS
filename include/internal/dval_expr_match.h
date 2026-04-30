#ifndef DVAL_EXPR_MATCH_H
#define DVAL_EXPR_MATCH_H

#include <stdbool.h>
#include <stddef.h>

#include "dval.h"

/**
 * @file dval_expr_match.h
 * @brief Internal structural matchers for dval expression DAGs.
 *
 * These helpers support the symbolic matcher/integrator layers and are not
 * part of the public dval API surface.
 */

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

#endif
