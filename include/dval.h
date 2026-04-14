#ifndef DVAL_H
#define DVAL_H

#include "qfloat.h"

/**
 * @file dval.h
 * @brief Lazy, vtable-driven, reference-counted differentiable value type.
 *
 * Ownership rules:
 *   • Any function whose name begins with dv_new_* or dv_create_* returns
 *     an owning handle. The caller must call dv_free() exactly once.
 *
 *   • All arithmetic and math functions (dv_add, dv_mul, dv_sin, …) also
 *     return owning handles. The caller must dv_free() them.
 *
 *   • dv_get_deriv() returns a *borrowed* view. The caller must NOT free it.
 *
 * Internally, each dval_t is a node in a reference-counted DAG.
 */

typedef struct _dval_t dval_t;

/* ------------------------------------------------------------------------- */
/* Constructors — constants                                                  */
/* ------------------------------------------------------------------------- */

dval_t *dv_new_const_d(double x);
dval_t *dv_new_const(qfloat x);
dval_t *dv_new_named_const(qfloat x, const char *name);
dval_t *dv_new_named_const_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Constructors — variables                                                  */
/* ------------------------------------------------------------------------- */

dval_t *dv_new_var_d(double x);
dval_t *dv_new_var(qfloat x);
dval_t *dv_new_named_var(qfloat x, const char *name);
dval_t *dv_new_named_var_d(double x, const char *name);

/* ------------------------------------------------------------------------- */
/* Mutators                                                                  */
/* ------------------------------------------------------------------------- */

void dv_set_val(dval_t *dv, qfloat value);
void dv_set_val_d(dval_t *dv, double value);
void dv_set_name(dval_t *dv, const char *name);

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

double dv_get_val_d(const dval_t *dv);
qfloat dv_get_val(const dval_t *dv);

/**
 * @brief Get the cached first derivative node (borrowed).
 *
 * The returned pointer is owned by @p dv and must NOT be freed by the caller.
 * If the derivative has not yet been built, it is created lazily.
 */
const dval_t *dv_get_deriv(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Evaluation                                                                */
/* ------------------------------------------------------------------------- */

qfloat dv_eval(const dval_t *dv);
double dv_eval_d(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(dval_t *dv);
dval_t *dv_create_2nd_deriv(dval_t *dv);
dval_t *dv_create_3rd_deriv(dval_t *dv);
dval_t *dv_create_nth_deriv(unsigned int n, dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Arithmetic (graph-building, owning)                                       */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(dval_t *dv);
dval_t *dv_add(dval_t *dv1, dval_t *dv2);
dval_t *dv_sub(dval_t *dv1, dval_t *dv2);
dval_t *dv_mul(dval_t *dv1, dval_t *dv2);
dval_t *dv_div(dval_t *dv1, dval_t *dv2);

dval_t *dv_add_d(dval_t *dv, double d);
dval_t *dv_sub_d(dval_t *dv, double d);
dval_t *dv_d_sub(double d, dval_t *dv);
dval_t *dv_mul_d(dval_t *dv, double d);
dval_t *dv_div_d(dval_t *dv, double d);
dval_t *dv_d_div(double d, dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Comparison                                                                */
/* ------------------------------------------------------------------------- */

int dv_cmp(const dval_t *dv1, const dval_t *dv2);
int dv_compare(const dval_t *dv1, const dval_t *dv2);

/* ------------------------------------------------------------------------- */
/* Elementary functions (owning)                                             */
/* ------------------------------------------------------------------------- */

dval_t *dv_sin(dval_t *dv);
dval_t *dv_cos(dval_t *dv);
dval_t *dv_tan(dval_t *dv);
dval_t *dv_sinh(dval_t *dv);
dval_t *dv_cosh(dval_t *dv);
dval_t *dv_tanh(dval_t *dv);
dval_t *dv_asin(dval_t *dv);
dval_t *dv_acos(dval_t *dv);
dval_t *dv_atan(dval_t *dv);
dval_t *dv_atan2(dval_t *dv1, dval_t *dv2);
dval_t *dv_asinh(dval_t *dv);
dval_t *dv_acosh(dval_t *dv);
dval_t *dv_atanh(dval_t *dv);
dval_t *dv_exp(dval_t *dv);
dval_t *dv_log(dval_t *dv);
dval_t *dv_sqrt(dval_t *dv);
dval_t *dv_pow_d(dval_t *dv, double d);
dval_t *dv_pow(dval_t *dv1, dval_t *dv2);

/* ------------------------------------------------------------------------- */
/* Special functions (owning)                                                */
/* ------------------------------------------------------------------------- */

dval_t *dv_abs(dval_t *dv);
dval_t *dv_hypot(dval_t *dv1, dval_t *dv2);
dval_t *dv_erf(dval_t *dv);
dval_t *dv_erfc(dval_t *dv);
dval_t *dv_erfinv(dval_t *dv);
dval_t *dv_erfcinv(dval_t *dv);
dval_t *dv_gamma(dval_t *dv);
dval_t *dv_lgamma(dval_t *dv);
dval_t *dv_digamma(dval_t *dv);
dval_t *dv_trigamma(dval_t *dv);
dval_t *dv_lambert_w0(dval_t *dv);
dval_t *dv_lambert_wm1(dval_t *dv);
dval_t *dv_beta(dval_t *dv1, dval_t *dv2);
dval_t *dv_logbeta(dval_t *dv1, dval_t *dv2);
dval_t *dv_normal_pdf(dval_t *dv);
dval_t *dv_normal_cdf(dval_t *dv);
dval_t *dv_normal_logpdf(dval_t *dv);
dval_t *dv_ei(dval_t *dv);
dval_t *dv_e1(dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Decrement the reference count and free if it reaches zero.
 *
 * Must be called exactly once for every owning handle returned by:
 *   - dv_new_*
 *   - dv_create_*
 *   - dv_add, dv_mul, dv_sin, etc.
 */
void dv_free(dval_t *dv);

/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

typedef enum {
    style_FUNCTION,
    style_EXPRESSION
} style_t;

char *dv_to_string(const dval_t *dv, style_t style);

/* @brief Print the string representation of @p dv to stdout.
 *
 * note: this uses dv_to_string with style_EXPRESSION
 */
void dv_print(const dval_t *dv);

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Construct a dval_t from an expression-style string.
 *
 * Accepts strings in the format produced by dv_to_string(f, style_EXPRESSION):
 *
 *   { expr | x₀ = val, ...; [name] = val, ... }
 *
 * or for a pure named constant:
 *
 *   { name = val }
 *
 * Variables appear before the ';' in the binding section; named constants
 * appear after it.
 *
 * The following ASCII alternatives are accepted in addition to the canonical
 * Unicode forms:
 *
 *   @p _N       subscript digit (x_0 ≡ x₀); interchangeable with U+2080–U+2089
 *               within the same string
 *   @p *        explicit multiplication (c * sin(x) ≡ c·sin(x)); spaces around
 *               @p * are permitted
 *   @p ^N       integer exponent on a function name (sin^2 ≡ sin²) or on a
 *               sub-expression ((x+1)^2 ≡ (x+1)²)
 *
 * Bracketed names (@p [my var], @p [2pi], …) are supported for identifiers
 * that do not fit the single-letter-plus-subscript rule.
 *
 * Returns an owning handle on success, or NULL on error (details written to
 * stderr).  The caller must call dv_free() on the returned pointer exactly
 * once.
 */
dval_t *dval_from_string(const char *s);

#endif /* DVAL_H */
