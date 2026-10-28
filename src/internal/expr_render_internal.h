/**
 * @file expr_render_internal.h
 * @brief Controlled access to expression graphs for the native TeX rendering backend.
 *
 * The TeX module inspects native expression nodes, binding values and shared
 * formatting state through this facade. Mathematical evaluation and transformation
 * remain supplied by expression. Ordinary clients use expression-t.h and TeX-t.h.
 */
#ifndef MARS_SHARED_EXPR_RENDER_INTERNAL_H
#define MARS_SHARED_EXPR_RENDER_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_RENDER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_RENDER_INTERNAL_ACCESS."
#endif

#define MARS_EXPR_INTERNAL_ACCESS
#include "expression/expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expression/expr_stringout_internal.h"

/** @brief Render an exact binding value; the returned string is owned by the caller. */
char *expr_binding_expr_to_TeX(const expr_binding_expr_t *binding);

/** @brief Report whether a binding factor requires an explicit multiplication separator. */
bool expr_binding_expr_needs_explicit_mul_separator(const expr_binding_expr_t *binding);

/** @brief Shared native rendering helper; arguments are borrowed unless named as output. */
int expr_polygamma_order(const expr_t *f, long *order);

/** @brief Shared native rendering helper; arguments are borrowed unless named as output. */
int expr_polylog_order(const expr_t *f, long *order);

/** @brief Shared native rendering helper; arguments are borrowed unless named as output. */
int expr_legendre_chi_order(const expr_t *f, long *order);

/** @brief Shared native rendering helper; arguments are borrowed unless named as output. */
bool expr_stringout_tree_has_symbol_name(const expr_t *expr, const char *name);

/** @brief Shared native rendering helper; arguments are borrowed unless named as output. */
bool expr_kind_has_analytic_cartesian_series(expr_op_kind_t kind);

#endif
