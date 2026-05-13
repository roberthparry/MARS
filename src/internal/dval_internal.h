#ifndef DVAL_SHARED_INTERNAL_H
#define DVAL_SHARED_INTERNAL_H

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

static inline dval_t *dv_num_const_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_const_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_const_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_const_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_const_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_named_const(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_var_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_var_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_var_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline dval_t *dv_num_named_var_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *dv = dv_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static inline void dv_num_set_d(dval_t *dv, double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline void dv_num_set_qf(dval_t *dv, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    dv_set_val(dv, n);
    num_destroy(&n);
}

static inline void dv_num_set_qc(dval_t *dv, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);

    dv_set_val(dv, n);
    num_destroy(&n);
}

int dv_get_default_constant_num(const char *name, number_t *value_out);

#ifndef dv_sub_d
static inline dval_t *dv_sub_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_sub_num(dv, &n);

    num_destroy(&n);
    return out;
}
#endif

#ifndef dv_d_sub
static inline dval_t *dv_d_sub(double x, const dval_t *dv)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_num_sub(&n, dv);

    num_destroy(&n);
    return out;
}
#endif

#ifndef dv_mul_d
static inline dval_t *dv_mul_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}
#endif

#ifndef dv_d_div
static inline dval_t *dv_d_div(double x, const dval_t *dv)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_num_div(&n, dv);

    num_destroy(&n);
    return out;
}
#endif

#ifndef dv_pow_qc
static inline dval_t *dv_pow_qc(const dval_t *dv, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    dval_t *out = dv_pow(dv, &n);

    num_destroy(&n);
    return out;
}
#endif

static inline double dv_num_eval_d(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    double out = num_to_double(n);

    num_destroy(&n);
    return out;
}

static inline qfloat_t dv_num_eval_qf(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

static inline qcomplex_t dv_num_eval_qc(const dval_t *dv)
{
    number_t n = dv_eval(dv);
    number_t re_n = num_real_part(n);
    number_t im_n = num_imag_part(n);
    qcomplex_t out = qc_make(num_to_qfloat(re_n), num_to_qfloat(im_n));

    num_destroy(&im_n);
    num_destroy(&re_n);
    num_destroy(&n);
    return out;
}

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
