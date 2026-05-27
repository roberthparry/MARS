#ifndef INTEGRATOR_INTERNAL_H
#define INTEGRATOR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "expression.h"
#include "integrator.h"
#include "internal/expr_internal.h"

static inline expr_t *ig_expr_new_const_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *dv = expr_new_const(n);

    num_destroy(&n);
    return dv;
}

static inline expr_t *ig_expr_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *dv = expr_new_var(n);

    num_destroy(&n);
    return dv;
}

static inline void ig_expr_set_val_qf(expr_t *dv, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    expr_set_val(dv, n);
    num_destroy(&n);
}

static inline qfloat_t ig_expr_eval_qf(const expr_t *dv)
{
    number_t n = expr_eval(dv);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}
struct _integrator_t {
    qfloat_t abs_tol;
    qfloat_t rel_tol;
    size_t max_intervals;
    size_t last_intervals;
};

typedef struct {
    qfloat_t a, b;
    qfloat_t result;
    qfloat_t error;
} subinterval_t;

#define TN_NODES 8
#define TN_T4    4
#define TN_SYMMETRIC_PAIRS (TN_NODES - 1)

extern qfloat_t tn_node[TN_NODES];
extern qfloat_t tn_wa[TN_NODES];
extern qfloat_t tn_wd[TN_NODES];
extern qfloat_t tn4_wa[TN_T4];
extern qfloat_t tn4_wd[TN_T4];

void gturan_eval_expr(expr_t *expr, expr_t *x_var, expr_t *d2_expr,
                    qfloat_t a, qfloat_t b,
                    qfloat_t *t15_out, qfloat_t *t4_out);

int try_integral_multi_special_affine(integrator_t *ig, expr_t *expr,
                                      size_t ndim, expr_t *const *vars,
                                      const qfloat_t *lo, const qfloat_t *hi,
                                      qfloat_t *result, qfloat_t *error_est);

#endif
