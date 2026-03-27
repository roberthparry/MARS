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

void dv_set_val(dval_t *x, qfloat value);
void dv_set_val_d(dval_t *x, double value);
void dv_set_name(dval_t *x, const char *name);

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

double dv_get_val_d(const dval_t *f);
qfloat dv_get_val(const dval_t *f);

/**
 * @brief Get the cached first derivative node (borrowed).
 *
 * The returned pointer is owned by @p f and must NOT be freed by the caller.
 * If the derivative has not yet been built, it is created lazily.
 */
const dval_t *dv_get_deriv(const dval_t *f);

/* ------------------------------------------------------------------------- */
/* Evaluation                                                                */
/* ------------------------------------------------------------------------- */

qfloat dv_eval(const dval_t *f);
double dv_eval_d(const dval_t *f);

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(dval_t *f);
dval_t *dv_create_2nd_deriv(dval_t *f);
dval_t *dv_create_3rd_deriv(dval_t *f);
dval_t *dv_create_nth_deriv(unsigned int n, dval_t *f);

/* ------------------------------------------------------------------------- */
/* Arithmetic (graph-building, owning)                                       */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(dval_t *f);
dval_t *dv_add(dval_t *f, dval_t *g);
dval_t *dv_sub(dval_t *f, dval_t *g);
dval_t *dv_mul(dval_t *f, dval_t *g);
dval_t *dv_div(dval_t *f, dval_t *g);

dval_t *dv_add_d(dval_t *f, double d);
dval_t *dv_sub_d(dval_t *f, double d);
dval_t *dv_d_sub(double d, dval_t *f);
dval_t *dv_mul_d(dval_t *f, double d);
dval_t *dv_div_d(dval_t *f, double d);
dval_t *dv_d_div(double d, dval_t *f);

/* ------------------------------------------------------------------------- */
/* Comparison                                                                */
/* ------------------------------------------------------------------------- */

int dv_cmp(const dval_t *f, const dval_t *g);
int dv_compare(const dval_t *f, const dval_t *g);

/* ------------------------------------------------------------------------- */
/* Elementary functions (owning)                                             */
/* ------------------------------------------------------------------------- */

dval_t *dv_sin(dval_t *f);
dval_t *dv_cos(dval_t *f);
dval_t *dv_tan(dval_t *f);
dval_t *dv_sinh(dval_t *f);
dval_t *dv_cosh(dval_t *f);
dval_t *dv_tanh(dval_t *f);
dval_t *dv_asin(dval_t *f);
dval_t *dv_acos(dval_t *f);
dval_t *dv_atan(dval_t *f);
dval_t *dv_atan2(dval_t *f, dval_t *g);
dval_t *dv_asinh(dval_t *f);
dval_t *dv_acosh(dval_t *f);
dval_t *dv_atanh(dval_t *f);
dval_t *dv_exp(dval_t *f);
dval_t *dv_log(dval_t *f);
dval_t *dv_sqrt(dval_t *f);
dval_t *dv_pow_d(dval_t *f, double d);
dval_t *dv_pow(dval_t *f, dval_t *g);

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
void dv_free(dval_t *f);

/* ------------------------------------------------------------------------- */
/* String conversion                                                         */
/* ------------------------------------------------------------------------- */

typedef enum {
    style_FUNCTION,
    style_EXPRESSION
} style_t;

char *dv_to_string(const dval_t *f, style_t style);

/* @brief Print the string representation of @p f to stdout.
 *
 * note: this uses dv_to_string with style_EXPRESSION
 */
void dv_print(const dval_t *f);

#endif /* DVAL_H */
