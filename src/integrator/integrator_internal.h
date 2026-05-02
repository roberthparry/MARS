#ifndef INTEGRATOR_INTERNAL_H
#define INTEGRATOR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "integrator.h"
#include "dval.h"
#include "internal/dval_expr_match.h"
#include "internal/dval_pattern.h"
#include "qcomplex.h"

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

extern qfloat_t tn_node[TN_NODES];
extern qfloat_t tn_wa[TN_NODES];
extern qfloat_t tn_wd[TN_NODES];
extern qfloat_t tn4_wa[TN_T4];
extern qfloat_t tn4_wd[TN_T4];

void gturan_eval_dv(dval_t *expr, dval_t *x_var, dval_t *d2_expr,
                    qfloat_t a, qfloat_t b,
                    qfloat_t *t15_out, qfloat_t *t4_out);

int try_integral_multi_special_affine(integrator_t *ig, dval_t *expr,
                                      size_t ndim, dval_t *const *vars,
                                      const qfloat_t *lo, const qfloat_t *hi,
                                      qfloat_t *result, qfloat_t *error_est);

#endif
