#include <stdlib.h>
#include "integrator.h"
#include "dval.h"

/* -------------------------------------------------------------------
 * G7K15 nodes and weights — initialised once from decimal strings
 * ------------------------------------------------------------------- */

#define GK_NODES    8   /* t[0]=0 plus 7 positive nodes */
#define GK_GAUSS    4   /* Gauss nodes at t[0], t[2], t[4], t[6] */

/* Kronrod abscissae (non-negative) */
static qfloat_t t_node[GK_NODES] =  {
    { 0.0, 0.0 },
    { 0.20778495500789848, -1.3226987786329531e-17 },
    { 0.40584515137739718, -1.7249275447547112e-17 },
    { 0.58608723546769115, -1.7466970805984842e-17 },
    { 0.74153118559939446, 2.9725137627742975e-17 },
    { 0.8648644233597691, -2.3887783499182572e-17 },
    { 0.94910791234275849, 3.825796587866567e-17 },
    { 0.99145537112081261, 2.7322067495382961e-17 }
};

/* Kronrod weights   */
static qfloat_t wk[GK_NODES] = {
    { 0.20948214108472782, 9.3212527821842532e-18 },
    { 0.20443294007529889, 6.7404018028659684e-18 },
    { 0.19035057806478542, -9.6165132809012221e-18 },
    { 0.16900472663926791, -7.5664329098580932e-18 },
    { 0.14065325971552592, -2.4841647879689914e-19 },
    { 0.10479001032225019, -3.906585979588142e-18 },
    { 0.063092092629978558, -4.536585383520607e-18 },
    { 0.022935322010529224, 5.9571805172231468e-19 }
};

/* Gauss weights     */
static qfloat_t wg[GK_GAUSS] = {
    { 0.4179591836734694, -1.54978071192573e-17 },
    { 0.38183005050511892, 2.1862747923778412e-17 },
    { 0.27970539148927664, 2.3267180221717125e-17 },
    { 0.1294849661688697, -9.6254489702844082e-18 }
};

/* -------------------------------------------------------------------
 * Turán T15/T4 nodes and weights — f(x) + f''(x) rule, degree 31
 *
 * Nodes are zeros of the Jacobi polynomial P_15^(2,2)(x) on [-1,1],
 * giving {0, ±t₁, …, ±t₇} — 8 distinct positions.
 * T15 uses all 8; T4 (nested error estimator) uses positions 0,2,4,6.
 * ------------------------------------------------------------------- */

#define TN_NODES 8
#define TN_T4    4

/* Turán abscissae (non-negative), index 0 = center */
static qfloat_t tn_node[TN_NODES] = {
    { 0.0, 0.0 },
    { 0.17965850484814358, 1.3958910912084355e-18 },
    { 0.35354321747256057, -3.6372867902880103e-19 },
    { 0.5160661648573682,  4.448641811325818e-18  },
    { 0.6620052748493138,  -2.173877298219847e-18 },
    { 0.7866736157817852,  -2.6946458818551736e-18},
    { 0.886075647017153,   1.958874324657169e-18  },
    { 0.9570723569250652,  -8.371559074122715e-18 },
};

/* T15 f weights */
static qfloat_t tn_wa[TN_NODES] = {
    { 0.2743471442314423,   -9.597309529032129e-18  },
    { 0.26061308666823146,  -1.9318722156942648e-17 },
    { 0.22403666663809482,  -7.53884046574816e-18   },
    { 0.17095764700910138,   1.2954883701222613e-17 },
    { 0.11407915793378574,  -6.631709279358162e-18  },
    { 0.06239265193878386,  -1.62957585319424e-19   },
    { 0.025821326601317043,  1.0939038630617943e-18 },
    { 0.004925891094964569,  1.1596802392475038e-19 },
};

/* T15 f'' weights */
static qfloat_t tn_wd[TN_NODES] = {
    { 0.023372041866646102,  1.0145434992926499e-18 },
    { 0.021534197971480148,  7.027768502196576e-19  },
    { 0.016752328089724506, -3.5449117057541755e-19 },
    { 0.010796969670018685, -3.566007487570268e-19  },
    { 0.005552200442152688, -6.136336278523143e-20  },
    { 0.0021148215762825822,-1.09684524159214e-19   },
    { 0.0005107015380345585,-9.17580575234709e-21   },
    { 5.0683572995622733e-05, 3.0865404590769377e-21},
};

/* T4 nested f weights (nodes at T15 positions 0,2,4,6) */
static qfloat_t tn4_wa[TN_T4] = {
    { 0.3873750227417537,  -1.8653873125071e-17   },
    { 0.3573488892017908,   1.0918523408671061e-17 },
    { 0.2847343352610255,   1.9803445948553018e-17 },
    { 0.16422926416630682,  6.360542820940334e-18  },
};

/* T4 nested f'' weights (nodes at T15 positions 0,2,4,6) */
static qfloat_t tn4_wd[TN_T4] = {
    { 0.010056786351651225, -5.960144669967088e-19 },
    { 0.007964473972150962, -1.821340375196974e-19 },
    { 0.0037567107666183625,-7.371952593800035e-20 },
    { 0.0007208517799672414, 4.7764084118324135e-20},
};

/* -------------------------------------------------------------------
 * Single-interval G7K15 evaluation
 * ------------------------------------------------------------------- */

static void gk15_eval(integrand_fn f, void *ctx,
                      qfloat_t a, qfloat_t b,
                      qfloat_t *k15_out, qfloat_t *g7_out) {
    qfloat_t c = qf_mul_double(qf_add(a, b), 0.5);
    qfloat_t h = qf_mul_double(qf_sub(b, a), 0.5);

    /* Evaluate f at center */
    qfloat_t f0 = f(c, ctx);

    /* Evaluate f at the 7 symmetric pairs and accumulate K15 */
    qfloat_t fpos[7], fneg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t hi = qf_mul(h, t_node[i + 1]);
        fpos[i] = f(qf_add(c, hi), ctx);
        fneg[i] = f(qf_sub(c, hi), ctx);
    }

    /* K15: center + 7 symmetric pairs */
    qfloat_t k15 = qf_mul(wk[0], f0);
    for (int i = 0; i < 7; i++)
        k15 = qf_add(k15, qf_mul(wk[i + 1], qf_add(fpos[i], fneg[i])));

    /* G7: center (t[0]) + pairs at t[2], t[4], t[6]
       In fpos/fneg those correspond to indices 1, 3, 5 respectively  */
    qfloat_t g7 = qf_mul(wg[0], f0);
    g7 = qf_add(g7, qf_mul(wg[1], qf_add(fpos[1], fneg[1])));
    g7 = qf_add(g7, qf_mul(wg[2], qf_add(fpos[3], fneg[3])));
    g7 = qf_add(g7, qf_mul(wg[3], qf_add(fpos[5], fneg[5])));

    *k15_out = qf_mul(h, k15);
    *g7_out  = qf_mul(h, g7);
}

/* -------------------------------------------------------------------
 * integrator_t
 * ------------------------------------------------------------------- */

struct integrator_t {
    qfloat_t abs_tol;
    qfloat_t rel_tol;
    size_t max_intervals;
    size_t last_intervals;
};

integrator_t *integrator_create(void) {
    integrator_t *ig = malloc(sizeof(integrator_t));
    if (!ig) return NULL;
    ig->abs_tol       = qf_from_string("1e-27");
    ig->rel_tol       = qf_from_string("1e-27");
    ig->max_intervals = 500;
    ig->last_intervals = 0;
    return ig;
}

void integrator_destroy(integrator_t *ig) {
    free(ig);
}

void integrator_set_tol(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol) {
    if (!ig) return;
    ig->abs_tol = abs_tol;
    ig->rel_tol = rel_tol;
}

void integrator_set_max_intervals(integrator_t *ig, size_t max_intervals) {
    if (!ig) return;
    ig->max_intervals = max_intervals;
}

size_t integrator_last_intervals(const integrator_t *ig) {
    return ig ? ig->last_intervals : 0;
}

/* -------------------------------------------------------------------
 * Adaptive integration
 * ------------------------------------------------------------------- */

typedef struct {
    qfloat_t a, b;
    qfloat_t result;
    qfloat_t error;
} subinterval_t;

int integrator_eval(integrator_t *ig, integrand_fn f, void *ctx,
                    qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est) {
    if (!ig || !f || !result) return -1;

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) return -1;

    /* Evaluate the initial interval */
    qfloat_t k15, g7;
    gk15_eval(f, ctx, a, b, &k15, &g7);

    intervals[0].a      = a;
    intervals[0].b      = b;
    intervals[0].result = k15;
    intervals[0].error  = qf_abs(qf_sub(k15, g7));

    size_t count      = 1;
    qfloat_t total      = k15;
    qfloat_t total_err  = intervals[0].error;

    int status = 0;

    while (1) {
        /* Convergence check */
        qfloat_t thresh = ig->abs_tol;
        qfloat_t rel    = qf_mul(ig->rel_tol, qf_abs(total));
        if (qf_gt(rel, thresh)) thresh = rel;
        if (qf_le(qf_abs(total_err), thresh)) break;

        if (count >= ig->max_intervals) { status = 1; break; }

        /* Find the subinterval with the largest error */
        size_t worst = 0;
        for (size_t i = 1; i < count; i++) {
            if (qf_gt(intervals[i].error, intervals[worst].error))
                worst = i;
        }

        /* Bisect the worst interval */
        qfloat_t mid = qf_mul_double(qf_add(intervals[worst].a,
                                           intervals[worst].b), 0.5);

        subinterval_t left, right;

        gk15_eval(f, ctx, intervals[worst].a, mid, &k15, &g7);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = k15;  left.error = qf_abs(qf_sub(k15, g7));

        gk15_eval(f, ctx, mid, intervals[worst].b, &k15, &g7);
        right.a = mid;  right.b = intervals[worst].b;
        right.result = k15;  right.error = qf_abs(qf_sub(k15, g7));

        /* Update running totals incrementally */
        total     = qf_add(qf_sub(total,     intervals[worst].result),
                           qf_add(left.result, right.result));
        total_err = qf_add(qf_sub(total_err, intervals[worst].error),
                           qf_add(left.error,  right.error));

        /* Grow storage if needed */
        if (count >= capacity) {
            capacity *= 2;
            subinterval_t *tmp = realloc(intervals,
                                         capacity * sizeof(subinterval_t));
            if (!tmp) { free(intervals); return -1; }
            intervals = tmp;
        }

        intervals[worst] = left;
        intervals[count++] = right;
    }

    ig->last_intervals = count;
    *result = total;
    if (error_est) *error_est = qf_abs(total_err);

    free(intervals);
    return status;
}

/* -------------------------------------------------------------------
 * Single-interval Turán T15/T4 evaluation via dval_t AD
 *
 * T15 uses f and f'' at all 8 node positions (degree 31).
 * T4 reuses the evaluations at positions 0,2,4,6 (degree 15).
 * The change-of-variable g(t)=f(c+h*t) gives g''(t) = h²·f''(c+h*t).
 * ------------------------------------------------------------------- */

static void gturan_eval_dv(dval_t *expr, dval_t *x_var, dval_t *d2_expr,
                            qfloat_t a, qfloat_t b,
                            qfloat_t *t15_out, qfloat_t *t4_out)
{
    qfloat_t c  = qf_mul_double(qf_add(a, b), 0.5);
    qfloat_t h  = qf_mul_double(qf_sub(b, a), 0.5);
    qfloat_t h2 = qf_mul(h, h);

    /* Center node */
    dv_invalidate(expr);
    dv_invalidate(d2_expr);
    dv_set_val(x_var, c);
    qfloat_t f0  = dv_eval(expr);
    qfloat_t d20 = dv_eval(d2_expr);

    /* Seven symmetric pairs */
    qfloat_t fpos[7], fneg[7], d2pos[7], d2neg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t hi = qf_mul(h, tn_node[i + 1]);

        dv_invalidate(expr);
        dv_invalidate(d2_expr);
        dv_set_val(x_var, qf_add(c, hi));
        fpos[i]  = dv_eval(expr);
        d2pos[i] = dv_eval(d2_expr);

        dv_invalidate(expr);
        dv_invalidate(d2_expr);
        dv_set_val(x_var, qf_sub(c, hi));
        fneg[i]  = dv_eval(expr);
        d2neg[i] = dv_eval(d2_expr);
    }

    /* T15 accumulation (all 8 positions) */
    qfloat_t t15 = qf_add(qf_mul(tn_wa[0], f0),
                           qf_mul(tn_wd[0], qf_mul(h2, d20)));
    for (int i = 0; i < 7; i++) {
        t15 = qf_add(t15, qf_mul(tn_wa[i + 1], qf_add(fpos[i], fneg[i])));
        t15 = qf_add(t15, qf_mul(tn_wd[i + 1],
                                  qf_mul(h2, qf_add(d2pos[i], d2neg[i]))));
    }

    /* T4 accumulation: T15 positions 0,2,4,6 map to fpos/fneg[1,3,5] */
    qfloat_t t4 = qf_add(qf_mul(tn4_wa[0], f0),
                          qf_mul(tn4_wd[0], qf_mul(h2, d20)));
    t4 = qf_add(t4, qf_mul(tn4_wa[1], qf_add(fpos[1], fneg[1])));
    t4 = qf_add(t4, qf_mul(tn4_wd[1], qf_mul(h2, qf_add(d2pos[1], d2neg[1]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[2], qf_add(fpos[3], fneg[3])));
    t4 = qf_add(t4, qf_mul(tn4_wd[2], qf_mul(h2, qf_add(d2pos[3], d2neg[3]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[3], qf_add(fpos[5], fneg[5])));
    t4 = qf_add(t4, qf_mul(tn4_wd[3], qf_mul(h2, qf_add(d2pos[5], d2neg[5]))));

    *t15_out = qf_mul(h, t15);
    *t4_out  = qf_mul(h, t4);
}

int integrator_eval_dv(integrator_t *ig, dval_t *expr, dval_t *x_var,
                        qfloat_t a, qfloat_t b,
                        qfloat_t *result, qfloat_t *error_est)
{
    if (!ig || !expr || !x_var || !result) return -1;

    dval_t *d2_expr = dv_create_2nd_deriv(expr);
    if (!d2_expr) return -1;

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) { dv_free(d2_expr); return -1; }

    qfloat_t t15, t4;
    gturan_eval_dv(expr, x_var, d2_expr, a, b, &t15, &t4);

    intervals[0].a      = a;
    intervals[0].b      = b;
    intervals[0].result = t15;
    intervals[0].error  = qf_abs(qf_sub(t15, t4));

    size_t    count     = 1;
    qfloat_t  total     = t15;
    qfloat_t  total_err = intervals[0].error;

    int status = 0;

    while (1) {
        qfloat_t thresh = ig->abs_tol;
        qfloat_t rel    = qf_mul(ig->rel_tol, qf_abs(total));
        if (qf_gt(rel, thresh)) thresh = rel;
        if (qf_le(qf_abs(total_err), thresh)) break;

        if (count >= ig->max_intervals) { status = 1; break; }

        size_t worst = 0;
        for (size_t i = 1; i < count; i++) {
            if (qf_gt(intervals[i].error, intervals[worst].error))
                worst = i;
        }

        qfloat_t mid = qf_mul_double(qf_add(intervals[worst].a,
                                             intervals[worst].b), 0.5);

        subinterval_t left, right;

        gturan_eval_dv(expr, x_var, d2_expr, intervals[worst].a, mid, &t15, &t4);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = t15;  left.error = qf_abs(qf_sub(t15, t4));

        gturan_eval_dv(expr, x_var, d2_expr, mid, intervals[worst].b, &t15, &t4);
        right.a = mid;  right.b = intervals[worst].b;
        right.result = t15;  right.error = qf_abs(qf_sub(t15, t4));

        total     = qf_add(qf_sub(total,     intervals[worst].result),
                           qf_add(left.result, right.result));
        total_err = qf_add(qf_sub(total_err, intervals[worst].error),
                           qf_add(left.error,  right.error));

        if (count >= capacity) {
            capacity *= 2;
            subinterval_t *tmp = realloc(intervals,
                                         capacity * sizeof(subinterval_t));
            if (!tmp) { free(intervals); dv_free(d2_expr); return -1; }
            intervals = tmp;
        }

        intervals[worst]   = left;
        intervals[count++] = right;
    }

    ig->last_intervals = count;
    *result = total;
    if (error_est) *error_est = qf_abs(total_err);

    free(intervals);
    dv_free(d2_expr);
    return status;
}
