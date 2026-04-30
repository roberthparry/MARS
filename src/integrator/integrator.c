#include <stdlib.h>
#include <stdint.h>
#include "integrator.h"
#include "dval.h"
#include "internal/dval_expr_match.h"
#include "internal/dval_pattern.h"
#include "qcomplex.h"

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

typedef struct _integrator_t {
    qfloat_t abs_tol;
    qfloat_t rel_tol;
    size_t max_intervals;
    size_t last_intervals;
} integrator_t;

integrator_t *ig_new(void) {
    integrator_t *ig = malloc(sizeof(integrator_t));
    if (!ig) return NULL;
    ig->abs_tol       = qf_from_string("1e-27");
    ig->rel_tol       = qf_from_string("1e-27");
    ig->max_intervals = 500;
    ig->last_intervals = 0;
    return ig;
}

void ig_free(integrator_t *ig) {
    free(ig);
}

void ig_set_tolerance(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol) {
    if (!ig) return;
    ig->abs_tol = abs_tol;
    ig->rel_tol = rel_tol;
}

void ig_set_interval_count_max(integrator_t *ig, size_t max_intervals) {
    if (!ig) return;
    ig->max_intervals = max_intervals;
}

size_t ig_get_interval_count_used(const integrator_t *ig) {
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

int ig_integral(integrator_t *ig, integrand_fn f, void *ctx,
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
    /* When expr and d2_expr are the same object (d²f/dx² == f), skip the
     * second evaluation at each quadrature point. */
    int same = (expr == d2_expr);

    qfloat_t c  = qf_mul_double(qf_add(a, b), 0.5);
    qfloat_t h  = qf_mul_double(qf_sub(b, a), 0.5);
    qfloat_t h2 = qf_mul(h, h);

    /* Center node */
    dv_set_val_qf(x_var, c);
    qfloat_t f0  = dv_eval_qf(expr);
    qfloat_t d20 = same ? f0 : dv_eval_qf(d2_expr);

    /* Seven symmetric pairs */
    qfloat_t fpos[7], fneg[7], d2pos[7], d2neg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t hi = qf_mul(h, tn_node[i + 1]);

        dv_set_val_qf(x_var, qf_add(c, hi));
        fpos[i]  = dv_eval_qf(expr);
        d2pos[i] = same ? fpos[i] : dv_eval_qf(d2_expr);

        dv_set_val_qf(x_var, qf_sub(c, hi));
        fneg[i]  = dv_eval_qf(expr);
        d2neg[i] = same ? fneg[i] : dv_eval_qf(d2_expr);
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

/* -------------------------------------------------------------------
 * Single-interval 2D Turán T15/T4
 *
 * Outer T15/T4 over [ay,by] in y; inner T15 over [ax,bx] in x.
 * At each outer y-node:
 *   F(y)   = ∫f(x,y)dx         via gturan_eval_dv(expr,   x_var, d2x_expr,    ...)
 *   F''(y) = ∫∂²f/∂y²(x,y)dx  via gturan_eval_dv(d2y_expr, x_var, d2x_d2y_expr, ...)
 * ------------------------------------------------------------------- */

static void gturan_eval_dv_2d(
    dval_t *expr,      dval_t *x_var, dval_t *d2x_expr,
    dval_t *d2y_expr,  dval_t *d2x_d2y_expr,
    dval_t *y_var,
    qfloat_t ax, qfloat_t bx,
    qfloat_t ay, qfloat_t by,
    qfloat_t *t15_out, qfloat_t *t4_out)
{
    qfloat_t cy  = qf_mul_double(qf_add(ay, by), 0.5);
    qfloat_t hy  = qf_mul_double(qf_sub(by, ay), 0.5);
    qfloat_t hy2 = qf_mul(hy, hy);
    qfloat_t dummy;

    dv_set_val_qf(y_var, cy);
    qfloat_t F0, Fpp0;
    gturan_eval_dv(expr,    x_var, d2x_expr,     ax, bx, &F0,   &dummy);
    gturan_eval_dv(d2y_expr, x_var, d2x_d2y_expr, ax, bx, &Fpp0, &dummy);

    qfloat_t Fpos[7], Fneg[7], Fpppos[7], Fppneg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t hi = qf_mul(hy, tn_node[i + 1]);

        dv_set_val_qf(y_var, qf_add(cy, hi));
        gturan_eval_dv(expr,    x_var, d2x_expr,     ax, bx, &Fpos[i],   &dummy);
        gturan_eval_dv(d2y_expr, x_var, d2x_d2y_expr, ax, bx, &Fpppos[i], &dummy);

        dv_set_val_qf(y_var, qf_sub(cy, hi));
        gturan_eval_dv(expr,    x_var, d2x_expr,     ax, bx, &Fneg[i],   &dummy);
        gturan_eval_dv(d2y_expr, x_var, d2x_d2y_expr, ax, bx, &Fppneg[i], &dummy);
    }

    qfloat_t t15 = qf_add(qf_mul(tn_wa[0], F0),
                           qf_mul(tn_wd[0], qf_mul(hy2, Fpp0)));
    for (int i = 0; i < 7; i++) {
        t15 = qf_add(t15, qf_mul(tn_wa[i+1], qf_add(Fpos[i], Fneg[i])));
        t15 = qf_add(t15, qf_mul(tn_wd[i+1], qf_mul(hy2, qf_add(Fpppos[i], Fppneg[i]))));
    }

    qfloat_t t4 = qf_add(qf_mul(tn4_wa[0], F0),
                          qf_mul(tn4_wd[0], qf_mul(hy2, Fpp0)));
    t4 = qf_add(t4, qf_mul(tn4_wa[1], qf_add(Fpos[1], Fneg[1])));
    t4 = qf_add(t4, qf_mul(tn4_wd[1], qf_mul(hy2, qf_add(Fpppos[1], Fppneg[1]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[2], qf_add(Fpos[3], Fneg[3])));
    t4 = qf_add(t4, qf_mul(tn4_wd[2], qf_mul(hy2, qf_add(Fpppos[3], Fppneg[3]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[3], qf_add(Fpos[5], Fneg[5])));
    t4 = qf_add(t4, qf_mul(tn4_wd[3], qf_mul(hy2, qf_add(Fpppos[5], Fppneg[5]))));

    *t15_out = qf_mul(hy, t15);
    *t4_out  = qf_mul(hy, t4);
}

/* Returns only T15 from a 2D Turán evaluation (used as inner call in 3D). */
static qfloat_t eval_2d_t15(
    dval_t *expr,      dval_t *x_var, dval_t *d2x_expr,
    dval_t *d2y_expr,  dval_t *d2x_d2y_expr,
    dval_t *y_var,
    qfloat_t ax, qfloat_t bx, qfloat_t ay, qfloat_t by)
{
    qfloat_t t15, t4;
    gturan_eval_dv_2d(expr, x_var, d2x_expr, d2y_expr, d2x_d2y_expr, y_var,
                       ax, bx, ay, by, &t15, &t4);
    return t15;
}

/* -------------------------------------------------------------------
 * Single-interval 3D Turán T15/T4
 *
 * Outer T15/T4 over [az,bz] in z; inner 2D T15 over [ax,bx]×[ay,by].
 * At each outer z-node:
 *   F(z)   = ∬f dxdy           via eval_2d_t15(expr,    ...)
 *   F''(z) = ∬∂²f/∂z² dxdy    via eval_2d_t15(d2z_expr, ...)
 *
 * Derivative arguments required:
 *   d2x_expr         = ∂²f/∂x²
 *   d2y_expr         = ∂²f/∂y²
 *   d2x_d2y_expr     = ∂⁴f/∂x²∂y²
 *   d2z_expr         = ∂²f/∂z²
 *   d2x_d2z_expr     = ∂⁴f/∂x²∂z²
 *   d2y_d2z_expr     = ∂⁴f/∂y²∂z²
 *   d2x_d2y_d2z_expr = ∂⁶f/∂x²∂y²∂z²
 * ------------------------------------------------------------------- */

static void gturan_eval_dv_3d(
    dval_t *expr,              dval_t *x_var, dval_t *d2x_expr,
    dval_t *d2y_expr,          dval_t *d2x_d2y_expr,
    dval_t *y_var,             qfloat_t ax, qfloat_t bx,
                               qfloat_t ay, qfloat_t by,
    dval_t *d2z_expr,          dval_t *d2x_d2z_expr,
    dval_t *d2y_d2z_expr,      dval_t *d2x_d2y_d2z_expr,
    dval_t *z_var,             qfloat_t az, qfloat_t bz,
    qfloat_t *t15_out, qfloat_t *t4_out)
{
    qfloat_t cz  = qf_mul_double(qf_add(az, bz), 0.5);
    qfloat_t hz  = qf_mul_double(qf_sub(bz, az), 0.5);
    qfloat_t hz2 = qf_mul(hz, hz);

    dv_set_val_qf(z_var, cz);
    qfloat_t F0   = eval_2d_t15(expr,    x_var, d2x_expr,     d2y_expr,     d2x_d2y_expr,
                                 y_var, ax, bx, ay, by);
    qfloat_t Fpp0 = eval_2d_t15(d2z_expr, x_var, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                                 y_var, ax, bx, ay, by);

    qfloat_t Fpos[7], Fneg[7], Fpppos[7], Fppneg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t hi = qf_mul(hz, tn_node[i + 1]);

        dv_set_val_qf(z_var, qf_add(cz, hi));
        Fpos[i]   = eval_2d_t15(expr,    x_var, d2x_expr,     d2y_expr,     d2x_d2y_expr,
                                 y_var, ax, bx, ay, by);
        Fpppos[i] = eval_2d_t15(d2z_expr, x_var, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                                 y_var, ax, bx, ay, by);

        dv_set_val_qf(z_var, qf_sub(cz, hi));
        Fneg[i]   = eval_2d_t15(expr,    x_var, d2x_expr,     d2y_expr,     d2x_d2y_expr,
                                 y_var, ax, bx, ay, by);
        Fppneg[i] = eval_2d_t15(d2z_expr, x_var, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                                 y_var, ax, bx, ay, by);
    }

    qfloat_t t15 = qf_add(qf_mul(tn_wa[0], F0),
                           qf_mul(tn_wd[0], qf_mul(hz2, Fpp0)));
    for (int i = 0; i < 7; i++) {
        t15 = qf_add(t15, qf_mul(tn_wa[i+1], qf_add(Fpos[i], Fneg[i])));
        t15 = qf_add(t15, qf_mul(tn_wd[i+1], qf_mul(hz2, qf_add(Fpppos[i], Fppneg[i]))));
    }

    qfloat_t t4 = qf_add(qf_mul(tn4_wa[0], F0),
                          qf_mul(tn4_wd[0], qf_mul(hz2, Fpp0)));
    t4 = qf_add(t4, qf_mul(tn4_wa[1], qf_add(Fpos[1], Fneg[1])));
    t4 = qf_add(t4, qf_mul(tn4_wd[1], qf_mul(hz2, qf_add(Fpppos[1], Fppneg[1]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[2], qf_add(Fpos[3], Fneg[3])));
    t4 = qf_add(t4, qf_mul(tn4_wd[2], qf_mul(hz2, qf_add(Fpppos[3], Fppneg[3]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[3], qf_add(Fpos[5], Fneg[5])));
    t4 = qf_add(t4, qf_mul(tn4_wd[3], qf_mul(hz2, qf_add(Fpppos[5], Fppneg[5]))));

    *t15_out = qf_mul(hz, t15);
    *t4_out  = qf_mul(hz, t4);
}

int ig_single_integral(integrator_t *ig, dval_t *expr, dval_t *x_var,
                       qfloat_t a, qfloat_t b,
                       qfloat_t *result, qfloat_t *error_est)
{
    if (!ig || !expr || !x_var || !result) return -1;

    dval_t *d2_expr = dv_create_2nd_deriv(expr, x_var, x_var);
    if (!d2_expr) return -1;

    /* If d²f/dx² == f (e.g. exp(x)), pass expr for both args so gturan_eval_dv
     * can skip the redundant second evaluation at each quadrature point. */
    static const double tp[2] = { 0.31415, 0.71828 };
    int d2_same = 1;
    for (int t = 0; t < 2 && d2_same; t++) {
        dv_set_val_qf(x_var, qf_from_double(tp[t]));
        if (!qf_eq(dv_eval_qf(expr), dv_eval_qf(d2_expr)))
            d2_same = 0;
    }
    dval_t *d2_use = d2_same ? expr : d2_expr;

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) { dv_free(d2_expr); return -1; }

    qfloat_t t15, t4;
    gturan_eval_dv(expr, x_var, d2_use, a, b, &t15, &t4);

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

        gturan_eval_dv(expr, x_var, d2_use, intervals[worst].a, mid, &t15, &t4);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = t15;  left.error = qf_abs(qf_sub(t15, t4));

        gturan_eval_dv(expr, x_var, d2_use, mid, intervals[worst].b, &t15, &t4);
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

int ig_double_integral(integrator_t *ig, dval_t *expr,
                       dval_t *x_var, qfloat_t ax, qfloat_t bx,
                       dval_t *y_var, qfloat_t ay, qfloat_t by,
                       qfloat_t *result, qfloat_t *error_est)
{
    if (!ig || !expr || !x_var || !y_var || !result) return -1;

    dval_t *d2x_expr     = dv_create_2nd_deriv(expr,    x_var, x_var);
    dval_t *d2y_expr     = dv_create_2nd_deriv(expr,    y_var, y_var);
    dval_t *d2x_d2y_expr = dv_create_2nd_deriv(d2y_expr, x_var, x_var);
    if (!d2x_expr || !d2y_expr || !d2x_d2y_expr) {
        dv_free(d2x_expr); dv_free(d2y_expr); dv_free(d2x_d2y_expr);
        return -1;
    }

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) {
        dv_free(d2x_expr); dv_free(d2y_expr); dv_free(d2x_d2y_expr);
        return -1;
    }

    qfloat_t t15, t4;
    gturan_eval_dv_2d(expr, x_var, d2x_expr, d2y_expr, d2x_d2y_expr,
                       y_var, ax, bx, ay, by, &t15, &t4);

    intervals[0].a      = ay;
    intervals[0].b      = by;
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

        gturan_eval_dv_2d(expr, x_var, d2x_expr, d2y_expr, d2x_d2y_expr,
                           y_var, ax, bx, intervals[worst].a, mid, &t15, &t4);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = t15;  left.error = qf_abs(qf_sub(t15, t4));

        gturan_eval_dv_2d(expr, x_var, d2x_expr, d2y_expr, d2x_d2y_expr,
                           y_var, ax, bx, mid, intervals[worst].b, &t15, &t4);
        right.a = mid;  right.b = intervals[worst].b;
        right.result = t15;  right.error = qf_abs(qf_sub(t15, t4));

        total     = qf_add(qf_sub(total,     intervals[worst].result),
                           qf_add(left.result, right.result));
        total_err = qf_add(qf_sub(total_err, intervals[worst].error),
                           qf_add(left.error,  right.error));

        if (count >= capacity) {
            capacity *= 2;
            subinterval_t *tmp = realloc(intervals, capacity * sizeof(subinterval_t));
            if (!tmp) {
                free(intervals);
                dv_free(d2x_expr); dv_free(d2y_expr); dv_free(d2x_d2y_expr);
                return -1;
            }
            intervals = tmp;
        }

        intervals[worst]   = left;
        intervals[count++] = right;
    }

    ig->last_intervals = count;
    *result = total;
    if (error_est) *error_est = qf_abs(total_err);

    free(intervals);
    dv_free(d2x_expr);
    dv_free(d2y_expr);
    dv_free(d2x_d2y_expr);
    return status;
}

int ig_triple_integral(integrator_t *ig, dval_t *expr,
                       dval_t *x_var, qfloat_t ax, qfloat_t bx,
                       dval_t *y_var, qfloat_t ay, qfloat_t by,
                       dval_t *z_var, qfloat_t az, qfloat_t bz,
                       qfloat_t *result, qfloat_t *error_est)
{
    if (!ig || !expr || !x_var || !y_var || !z_var || !result) return -1;

    dval_t *d2x_expr         = dv_create_2nd_deriv(expr,         x_var, x_var);
    dval_t *d2y_expr         = dv_create_2nd_deriv(expr,         y_var, y_var);
    dval_t *d2z_expr         = dv_create_2nd_deriv(expr,         z_var, z_var);
    dval_t *d2x_d2y_expr     = dv_create_2nd_deriv(d2y_expr,     x_var, x_var);
    dval_t *d2x_d2z_expr     = dv_create_2nd_deriv(d2z_expr,     x_var, x_var);
    dval_t *d2y_d2z_expr     = dv_create_2nd_deriv(d2z_expr,     y_var, y_var);
    dval_t *d2x_d2y_d2z_expr = dv_create_2nd_deriv(d2y_d2z_expr, x_var, x_var);

    if (!d2x_expr || !d2y_expr || !d2z_expr || !d2x_d2y_expr ||
        !d2x_d2z_expr || !d2y_d2z_expr || !d2x_d2y_d2z_expr) {
        dv_free(d2x_expr);     dv_free(d2y_expr);         dv_free(d2z_expr);
        dv_free(d2x_d2y_expr); dv_free(d2x_d2z_expr);
        dv_free(d2y_d2z_expr); dv_free(d2x_d2y_d2z_expr);
        return -1;
    }

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) {
        dv_free(d2x_expr);     dv_free(d2y_expr);         dv_free(d2z_expr);
        dv_free(d2x_d2y_expr); dv_free(d2x_d2z_expr);
        dv_free(d2y_d2z_expr); dv_free(d2x_d2y_d2z_expr);
        return -1;
    }

    qfloat_t t15, t4;
    gturan_eval_dv_3d(expr, x_var, d2x_expr,
                       d2y_expr, d2x_d2y_expr,
                       y_var, ax, bx, ay, by,
                       d2z_expr, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                       z_var, az, bz, &t15, &t4);

    intervals[0].a      = az;
    intervals[0].b      = bz;
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

        gturan_eval_dv_3d(expr, x_var, d2x_expr,
                           d2y_expr, d2x_d2y_expr,
                           y_var, ax, bx, ay, by,
                           d2z_expr, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                           z_var, intervals[worst].a, mid, &t15, &t4);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = t15;  left.error = qf_abs(qf_sub(t15, t4));

        gturan_eval_dv_3d(expr, x_var, d2x_expr,
                           d2y_expr, d2x_d2y_expr,
                           y_var, ax, bx, ay, by,
                           d2z_expr, d2x_d2z_expr, d2y_d2z_expr, d2x_d2y_d2z_expr,
                           z_var, mid, intervals[worst].b, &t15, &t4);
        right.a = mid;  right.b = intervals[worst].b;
        right.result = t15;  right.error = qf_abs(qf_sub(t15, t4));

        total     = qf_add(qf_sub(total,     intervals[worst].result),
                           qf_add(left.result, right.result));
        total_err = qf_add(qf_sub(total_err, intervals[worst].error),
                           qf_add(left.error,  right.error));

        if (count >= capacity) {
            capacity *= 2;
            subinterval_t *tmp = realloc(intervals, capacity * sizeof(subinterval_t));
            if (!tmp) {
                free(intervals);
                dv_free(d2x_expr);     dv_free(d2y_expr);         dv_free(d2z_expr);
                dv_free(d2x_d2y_expr); dv_free(d2x_d2z_expr);
                dv_free(d2y_d2z_expr); dv_free(d2x_d2y_d2z_expr);
                return -1;
            }
            intervals = tmp;
        }

        intervals[worst]   = left;
        intervals[count++] = right;
    }

    ig->last_intervals = count;
    *result = total;
    if (error_est) *error_est = qf_abs(total_err);

    free(intervals);
    dv_free(d2x_expr);     dv_free(d2y_expr);         dv_free(d2z_expr);
    dv_free(d2x_d2y_expr); dv_free(d2x_d2z_expr);
    dv_free(d2y_d2z_expr); dv_free(d2x_d2y_d2z_expr);
    return status;
}

/* -------------------------------------------------------------------
 * N-dimensional Turán T15/T4 via recursive evaluation
 *
 * deriv_exprs[mask] holds the expression with d²/dxᵢ² applied for each
 * bit i set in mask.  eval_nd_t15 integrates one dimension and recurses
 * inward; eval_nd_turan additionally accumulates the T4 error estimate.
 * ------------------------------------------------------------------- */

typedef struct {
    dval_t    **deriv_exprs; /* 2^ndim entries indexed by derivative bitmask */
    dval_t    **vars;        /* vars[0] = innermost variable                  */
    const qfloat_t *lo;
    const qfloat_t *hi;
    int        all_same;    /* all deriv_exprs[mask] are the same function   */
} multi_ctx_t;

static qfloat_t eval_nd_t15(const multi_ctx_t *ctx, int dim, size_t dmask,
                              qfloat_t a, qfloat_t b)
{
    if (dim == 0) {
        qfloat_t t15, t4;
        dval_t *e = ctx->deriv_exprs[dmask];
        gturan_eval_dv(e, ctx->vars[0],
                       ctx->all_same ? e : ctx->deriv_exprs[dmask | 1],
                       a, b, &t15, &t4);
        return t15;
    }
    qfloat_t c   = qf_mul_double(qf_add(a, b), 0.5);
    qfloat_t h   = qf_mul_double(qf_sub(b, a), 0.5);
    qfloat_t h2  = qf_mul(h, h);
    size_t   bit = (size_t)1 << dim;

    dv_set_val_qf(ctx->vars[dim], c);
    qfloat_t F0   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
    qfloat_t Fpp0 = ctx->all_same ? F0
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);

    qfloat_t Fpos[7], Fneg[7], Fpppos[7], Fppneg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t ht = qf_mul(h, tn_node[i + 1]);
        dv_set_val_qf(ctx->vars[dim], qf_add(c, ht));
        Fpos[i]   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
        Fpppos[i] = ctx->all_same ? Fpos[i]
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);
        dv_set_val_qf(ctx->vars[dim], qf_sub(c, ht));
        Fneg[i]   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
        Fppneg[i] = ctx->all_same ? Fneg[i]
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);
    }

    qfloat_t t15 = qf_add(qf_mul(tn_wa[0], F0), qf_mul(tn_wd[0], qf_mul(h2, Fpp0)));
    for (int i = 0; i < 7; i++) {
        t15 = qf_add(t15, qf_mul(tn_wa[i+1], qf_add(Fpos[i], Fneg[i])));
        t15 = qf_add(t15, qf_mul(tn_wd[i+1], qf_mul(h2, qf_add(Fpppos[i], Fppneg[i]))));
    }
    return qf_mul(h, t15);
}

static void eval_nd_turan(const multi_ctx_t *ctx, int dim, size_t dmask,
                            qfloat_t a, qfloat_t b,
                            qfloat_t *t15_out, qfloat_t *t4_out)
{
    if (dim == 0) {
        dval_t *e = ctx->deriv_exprs[dmask];
        gturan_eval_dv(e, ctx->vars[0],
                       ctx->all_same ? e : ctx->deriv_exprs[dmask | 1],
                       a, b, t15_out, t4_out);
        return;
    }
    qfloat_t c   = qf_mul_double(qf_add(a, b), 0.5);
    qfloat_t h   = qf_mul_double(qf_sub(b, a), 0.5);
    qfloat_t h2  = qf_mul(h, h);
    size_t   bit = (size_t)1 << dim;

    dv_set_val_qf(ctx->vars[dim], c);
    qfloat_t F0   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
    qfloat_t Fpp0 = ctx->all_same ? F0
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);

    qfloat_t Fpos[7], Fneg[7], Fpppos[7], Fppneg[7];
    for (int i = 0; i < 7; i++) {
        qfloat_t ht = qf_mul(h, tn_node[i + 1]);
        dv_set_val_qf(ctx->vars[dim], qf_add(c, ht));
        Fpos[i]   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
        Fpppos[i] = ctx->all_same ? Fpos[i]
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);
        dv_set_val_qf(ctx->vars[dim], qf_sub(c, ht));
        Fneg[i]   = eval_nd_t15(ctx, dim-1, dmask,       ctx->lo[dim-1], ctx->hi[dim-1]);
        Fppneg[i] = ctx->all_same ? Fneg[i]
                  : eval_nd_t15(ctx, dim-1, dmask | bit, ctx->lo[dim-1], ctx->hi[dim-1]);
    }

    qfloat_t t15 = qf_add(qf_mul(tn_wa[0], F0), qf_mul(tn_wd[0], qf_mul(h2, Fpp0)));
    for (int i = 0; i < 7; i++) {
        t15 = qf_add(t15, qf_mul(tn_wa[i+1], qf_add(Fpos[i], Fneg[i])));
        t15 = qf_add(t15, qf_mul(tn_wd[i+1], qf_mul(h2, qf_add(Fpppos[i], Fppneg[i]))));
    }
    qfloat_t t4 = qf_add(qf_mul(tn4_wa[0], F0), qf_mul(tn4_wd[0], qf_mul(h2, Fpp0)));
    t4 = qf_add(t4, qf_mul(tn4_wa[1], qf_add(Fpos[1], Fneg[1])));
    t4 = qf_add(t4, qf_mul(tn4_wd[1], qf_mul(h2, qf_add(Fpppos[1], Fppneg[1]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[2], qf_add(Fpos[3], Fneg[3])));
    t4 = qf_add(t4, qf_mul(tn4_wd[2], qf_mul(h2, qf_add(Fpppos[3], Fppneg[3]))));
    t4 = qf_add(t4, qf_mul(tn4_wa[3], qf_add(Fpos[5], Fneg[5])));
    t4 = qf_add(t4, qf_mul(tn4_wd[3], qf_mul(h2, qf_add(Fpppos[5], Fppneg[5]))));

    *t15_out = qf_mul(h, t15);
    *t4_out  = qf_mul(h, t4);
}

static qfloat_t integral_exp_affine_box(size_t ndim, const qfloat_t *coeffs,
                                        qfloat_t constant,
                                        const qfloat_t *lo, const qfloat_t *hi,
                                        const bool *active)
{
    qfloat_t total = qf_exp(constant);

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t factor;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return QF_NAN;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO)) {
            factor = qf_sub(hi[i], lo[i]);
        } else {
            qfloat_t ahi = qf_mul(coeffs[i], hi[i]);
            qfloat_t alo = qf_mul(coeffs[i], lo[i]);
            factor = qf_div(qf_sub(qf_exp(ahi), qf_exp(alo)), coeffs[i]);
        }
        total = qf_mul(total, factor);
    }
    return total;
}

static qcomplex_t integral_exp_i_affine_box(size_t ndim, const qfloat_t *coeffs,
                                            qfloat_t constant,
                                            const qfloat_t *lo, const qfloat_t *hi,
                                            const bool *active)
{
    qcomplex_t total = qc_exp(qc_make(QF_ZERO, constant));

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t factor;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return qc_make(QF_NAN, QF_NAN);
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO)) {
            factor = qc_make(qf_sub(hi[i], lo[i]), QF_ZERO);
        } else {
            qcomplex_t num = qc_sub(qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i]))),
                                    qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i]))));
            factor = qc_div(num, qc_make(QF_ZERO, coeffs[i]));
        }
        total = qc_mul(total, factor);
    }
    return total;
}

static bool find_single_active_dim(size_t ndim, const bool *active, size_t *dim_out)
{
    size_t found = ndim;

    if (!dim_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        if (active && !active[i])
            continue;
        if (found != ndim)
            return false;
        found = i;
    }

    if (found == ndim)
        return false;

    *dim_out = found;
    return true;
}

typedef enum {
    AFFINE_POLY_SPECIAL_EXP,
    AFFINE_POLY_SPECIAL_SIN,
    AFFINE_POLY_SPECIAL_COS,
    AFFINE_POLY_SPECIAL_SINH,
    AFFINE_POLY_SPECIAL_COSH
} affine_poly_special_kind_t;

static qfloat_t eval_box_affine_poly_times_exp_common(size_t ndim, const qfloat_t *coeffs,
                                                      qfloat_t constant,
                                                      const qfloat_t *lo, const qfloat_t *hi,
                                                      const bool *active,
                                                      const qfloat_t poly[5]);

static qcomplex_t eval_box_affine_poly_times_exp_i_common(size_t ndim, const qfloat_t *coeffs,
                                                          qfloat_t constant,
                                                          const qfloat_t *lo, const qfloat_t *hi,
                                                          const bool *active,
                                                          const qfloat_t poly[5]);

static qfloat_t eval_poly_deg4_from_real_moments(const qfloat_t poly[5],
                                                 qfloat_t mean,
                                                 qfloat_t variance,
                                                 qfloat_t third_central,
                                                 qfloat_t sum_fourth_central,
                                                 qfloat_t sum_var_sq)
{
    qfloat_t value = poly[0];

    if (!qf_eq(poly[1], QF_ZERO))
        value = qf_add(value, qf_mul(poly[1], mean));
    if (!qf_eq(poly[2], QF_ZERO)) {
        qfloat_t raw2 = qf_add(qf_mul(mean, mean), variance);
        value = qf_add(value, qf_mul(poly[2], raw2));
    }
    if (!qf_eq(poly[3], QF_ZERO)) {
        qfloat_t raw3 = qf_add(qf_mul(qf_mul(mean, mean), mean),
                               qf_add(qf_mul_double(qf_mul(mean, variance), 3.0),
                                      third_central));
        value = qf_add(value, qf_mul(poly[3], raw3));
    }
    if (!qf_eq(poly[4], QF_ZERO)) {
        qfloat_t raw4 = qf_add(qf_mul(qf_mul(qf_mul(mean, mean), mean), mean),
                               qf_add(qf_mul_double(qf_mul(qf_mul(mean, mean), variance), 6.0),
                                      qf_add(qf_mul_double(qf_mul(mean, third_central), 4.0),
                                             qf_add(qf_mul_double(qf_mul(variance, variance), 3.0),
                                                    qf_sub(sum_fourth_central,
                                                           qf_mul_double(sum_var_sq, 3.0))))));
        value = qf_add(value, qf_mul(poly[4], raw4));
    }

    return value;
}

static qcomplex_t eval_poly_deg4_from_complex_moments(const qfloat_t poly[5],
                                                      qcomplex_t mean,
                                                      qcomplex_t variance,
                                                      qcomplex_t third_central,
                                                      qcomplex_t sum_fourth_central,
                                                      qcomplex_t sum_var_sq)
{
    qcomplex_t value = qc_make(poly[0], QF_ZERO);

    if (!qf_eq(poly[1], QF_ZERO))
        value = qc_add(value, qc_mul(qc_make(poly[1], QF_ZERO), mean));
    if (!qf_eq(poly[2], QF_ZERO)) {
        qcomplex_t raw2 = qc_add(qc_mul(mean, mean), variance);
        value = qc_add(value, qc_mul(qc_make(poly[2], QF_ZERO), raw2));
    }
    if (!qf_eq(poly[3], QF_ZERO)) {
        qcomplex_t raw3 = qc_add(qc_mul(qc_mul(mean, mean), mean),
                                 qc_add(qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                               qc_mul(mean, variance)),
                                        third_central));
        value = qc_add(value, qc_mul(qc_make(poly[3], QF_ZERO), raw3));
    }
    if (!qf_eq(poly[4], QF_ZERO)) {
        qcomplex_t raw4 = qc_add(qc_mul(qc_mul(qc_mul(mean, mean), mean), mean),
                                 qc_add(qc_mul(qc_make(qf_from_double(6.0), QF_ZERO),
                                               qc_mul(qc_mul(mean, mean), variance)),
                                        qc_add(qc_mul(qc_make(qf_from_double(4.0), QF_ZERO),
                                                      qc_mul(mean, third_central)),
                                               qc_add(qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                                             qc_mul(variance, variance)),
                                                      qc_sub(sum_fourth_central,
                                                             qc_mul(qc_make(qf_from_double(3.0), QF_ZERO),
                                                                    sum_var_sq))))));
        value = qc_add(value, qc_mul(qc_make(poly[4], QF_ZERO), raw4));
    }

    return value;
}

static qfloat_t eval_box_affine_unary_special(affine_poly_special_kind_t kind,
                                              size_t ndim,
                                              const qfloat_t *coeffs,
                                              qfloat_t constant,
                                              const qfloat_t *lo, const qfloat_t *hi,
                                              const bool *active)
{
    size_t dim;

    switch (kind) {
    case AFFINE_POLY_SPECIAL_EXP:
        return integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active);
    case AFFINE_POLY_SPECIAL_SINH:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_sinh(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_cosh(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_cosh(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        } else {
            qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
            qfloat_t total;

            if (!neg_coeffs)
                return QF_NAN;
            for (size_t i = 0; i < ndim; ++i)
                neg_coeffs[i] = qf_neg(coeffs[i]);
            total = qf_mul_double(qf_sub(integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active),
                                         integral_exp_affine_box(ndim, neg_coeffs, qf_neg(constant), lo, hi, active)),
                                  0.5);
            free(neg_coeffs);
            return total;
        }
    case AFFINE_POLY_SPECIAL_COSH:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_cosh(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_sinh(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_sinh(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        } else {
            qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
            qfloat_t total;

            if (!neg_coeffs)
                return QF_NAN;
            for (size_t i = 0; i < ndim; ++i)
                neg_coeffs[i] = qf_neg(coeffs[i]);
            total = qf_mul_double(qf_add(integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active),
                                         integral_exp_affine_box(ndim, neg_coeffs, qf_neg(constant), lo, hi, active)),
                                  0.5);
            free(neg_coeffs);
            return total;
        }
    case AFFINE_POLY_SPECIAL_SIN:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_sin(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_cos(qf_add(qf_mul(coeffs[dim], lo[dim]), constant)),
                                 qf_cos(qf_add(qf_mul(coeffs[dim], hi[dim]), constant))),
                          coeffs[dim]);
        }
        return integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active).im;
    case AFFINE_POLY_SPECIAL_COS:
        if (find_single_active_dim(ndim, active, &dim)) {
            if (qf_eq(coeffs[dim], QF_ZERO))
                return qf_mul(qf_cos(constant), qf_sub(hi[dim], lo[dim]));
            return qf_div(qf_sub(qf_sin(qf_add(qf_mul(coeffs[dim], hi[dim]), constant)),
                                 qf_sin(qf_add(qf_mul(coeffs[dim], lo[dim]), constant))),
                          coeffs[dim]);
        }
        return integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active).re;
    }

    return QF_NAN;
}

static bool exp_weighted_affine_stats(size_t ndim, const qfloat_t *coeffs,
                                      qfloat_t constant,
                                      const qfloat_t *lo, const qfloat_t *hi,
                                      const bool *active,
                                      qfloat_t *mean_out,
                                      qfloat_t *variance_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;

    if (!mean_out || !variance_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t first_term;
        qfloat_t second_term;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);

        mean = qf_add(mean, qf_sub(first_term, QF_ONE));
        variance = qf_add(variance,
                          qf_add(second_term,
                                 qf_sub(QF_ONE, qf_mul(first_term, first_term))));
    }

    *mean_out = mean;
    *variance_out = variance;
    return true;
}

static bool exp_weighted_affine_third_moment(size_t ndim, const qfloat_t *coeffs,
                                             qfloat_t constant,
                                             const qfloat_t *lo, const qfloat_t *hi,
                                             const bool *active,
                                             qfloat_t *mean_out,
                                             qfloat_t *variance_out,
                                             qfloat_t *third_central_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t third_central = QF_ZERO;

    if (!mean_out || !variance_out || !third_central_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t third_num;
        qfloat_t first_term;
        qfloat_t second_term;
        qfloat_t third_term;
        qfloat_t raw1;
        qfloat_t raw2;
        qfloat_t raw3;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        third_num = qf_sub(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), qf_exp(ahi)),
                           qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);
        third_term = qf_div(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), third_num), den);
        raw1 = qf_sub(first_term, QF_ONE);
        raw2 = qf_add(second_term, qf_sub(qf_mul_double(first_term, -2.0), qf_from_double(-2.0)));
        raw3 = qf_add(third_term,
                      qf_add(qf_mul_double(second_term, -3.0),
                             qf_add(qf_mul_double(first_term, 6.0),
                                    qf_from_double(-6.0))));

        mean = qf_add(mean, raw1);
        variance = qf_add(variance, qf_sub(raw2, qf_mul(raw1, raw1)));
        third_central = qf_add(third_central,
                               qf_add(raw3,
                                      qf_add(qf_mul_double(qf_mul(raw2, raw1), -3.0),
                                             qf_mul_double(qf_mul(qf_mul(raw1, raw1), raw1), 2.0))));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    return true;
}

static bool exp_i_weighted_affine_stats(size_t ndim, const qfloat_t *coeffs,
                                        qfloat_t constant,
                                        const qfloat_t *lo, const qfloat_t *hi,
                                        const bool *active,
                                        qcomplex_t *mean_out,
                                        qcomplex_t *variance_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;

    if (!mean_out || !variance_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);

        mean = qc_add(mean, qc_add(first_term, qc_make(QF_ZERO, QF_ONE)));
        variance = qc_add(variance,
                          qc_sub(second_term,
                                 qc_add(qc_mul(first_term, first_term),
                                        qc_make(QF_ONE, QF_ZERO))));
    }

    *mean_out = mean;
    *variance_out = variance;
    return true;
}

static bool exp_i_weighted_affine_third_moment(size_t ndim, const qfloat_t *coeffs,
                                               qfloat_t constant,
                                               const qfloat_t *lo, const qfloat_t *hi,
                                               const bool *active,
                                               qcomplex_t *mean_out,
                                               qcomplex_t *variance_out,
                                               qcomplex_t *third_central_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;
    qcomplex_t third_central = QC_ZERO;

    if (!mean_out || !variance_out || !third_central_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t third_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;
        qcomplex_t third_term;
        qcomplex_t raw1;
        qcomplex_t raw2;
        qcomplex_t raw3;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        third_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), QF_ZERO), exp_hi),
                                    qc_mul(qc_make(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), QF_ZERO), exp_lo)),
                             den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);
        third_term = qc_mul(qc_make(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), QF_ZERO), third_ratio);
        raw1 = qc_add(first_term, qc_make(QF_ZERO, QF_ONE));
        raw2 = qc_add(second_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(2.0)), first_term),
                             qc_make(qf_from_double(-2.0), QF_ZERO)));
        raw3 = qc_add(third_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(3.0)), second_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-6.0), QF_ZERO), first_term),
                                    qc_make(QF_ZERO, qf_from_double(-6.0)))));

        mean = qc_add(mean, raw1);
        variance = qc_add(variance, qc_sub(raw2, qc_mul(raw1, raw1)));
        third_central = qc_add(third_central,
                               qc_add(raw3,
                                      qc_add(qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO),
                                                    qc_mul(raw2, raw1)),
                                             qc_mul(qc_make(qf_from_double(2.0), QF_ZERO),
                                                    qc_mul(qc_mul(raw1, raw1), raw1)))));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    return true;
}

static bool exp_weighted_affine_fourth_moment(size_t ndim, const qfloat_t *coeffs,
                                              qfloat_t constant,
                                              const qfloat_t *lo, const qfloat_t *hi,
                                              const bool *active,
                                              qfloat_t *mean_out,
                                              qfloat_t *variance_out,
                                              qfloat_t *third_central_out,
                                              qfloat_t *sum_fourth_central_out,
                                              qfloat_t *sum_var_sq_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t third_central = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;

    if (!mean_out || !variance_out || !third_central_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t ahi;
        qfloat_t alo;
        qfloat_t den;
        qfloat_t first_num;
        qfloat_t second_num;
        qfloat_t third_num;
        qfloat_t fourth_num;
        qfloat_t first_term;
        qfloat_t second_term;
        qfloat_t third_term;
        qfloat_t fourth_term;
        qfloat_t raw1;
        qfloat_t raw2;
        qfloat_t raw3;
        qfloat_t raw4;
        qfloat_t var_i;
        qfloat_t third_i;
        qfloat_t fourth_i;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        ahi = qf_mul(coeffs[i], hi[i]);
        alo = qf_mul(coeffs[i], lo[i]);
        den = qf_sub(qf_exp(ahi), qf_exp(alo));
        first_num = qf_sub(qf_mul(hi[i], qf_exp(ahi)),
                           qf_mul(lo[i], qf_exp(alo)));
        second_num = qf_sub(qf_mul(qf_mul(hi[i], hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(lo[i], lo[i]), qf_exp(alo)));
        third_num = qf_sub(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), qf_exp(ahi)),
                           qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), qf_exp(alo)));
        fourth_num = qf_sub(qf_mul(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), hi[i]), qf_exp(ahi)),
                            qf_mul(qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), lo[i]), qf_exp(alo)));
        first_term = qf_div(qf_mul(coeffs[i], first_num), den);
        second_term = qf_div(qf_mul(qf_mul(coeffs[i], coeffs[i]), second_num), den);
        third_term = qf_div(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), third_num), den);
        fourth_term = qf_div(qf_mul(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), coeffs[i]), fourth_num), den);
        raw1 = qf_sub(first_term, QF_ONE);
        raw2 = qf_add(second_term, qf_sub(qf_mul_double(first_term, -2.0), qf_from_double(-2.0)));
        raw3 = qf_add(third_term,
                      qf_add(qf_mul_double(second_term, -3.0),
                             qf_add(qf_mul_double(first_term, 6.0), qf_from_double(-6.0))));
        raw4 = qf_add(fourth_term,
                      qf_add(qf_mul_double(third_term, -4.0),
                             qf_add(qf_mul_double(second_term, 12.0),
                                    qf_add(qf_mul_double(first_term, -24.0),
                                           qf_from_double(24.0)))));
        var_i = qf_sub(raw2, qf_mul(raw1, raw1));
        third_i = qf_add(raw3,
                         qf_add(qf_mul_double(qf_mul(raw2, raw1), -3.0),
                                qf_mul_double(qf_mul(qf_mul(raw1, raw1), raw1), 2.0)));
        fourth_i = qf_add(raw4,
                          qf_add(qf_mul_double(qf_mul(raw3, raw1), -4.0),
                                 qf_add(qf_mul_double(qf_mul(raw2, qf_mul(raw1, raw1)), 6.0),
                                        qf_mul_double(qf_mul(qf_mul(raw1, raw1),
                                                             qf_mul(raw1, raw1)), -3.0))));

        mean = qf_add(mean, raw1);
        variance = qf_add(variance, var_i);
        third_central = qf_add(third_central, third_i);
        sum_fourth_central = qf_add(sum_fourth_central, fourth_i);
        sum_var_sq = qf_add(sum_var_sq, qf_mul(var_i, var_i));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static bool exp_i_weighted_affine_fourth_moment(size_t ndim, const qfloat_t *coeffs,
                                                qfloat_t constant,
                                                const qfloat_t *lo, const qfloat_t *hi,
                                                const bool *active,
                                                qcomplex_t *mean_out,
                                                qcomplex_t *variance_out,
                                                qcomplex_t *third_central_out,
                                                qcomplex_t *sum_fourth_central_out,
                                                qcomplex_t *sum_var_sq_out)
{
    qcomplex_t mean = qc_make(constant, QF_ZERO);
    qcomplex_t variance = QC_ZERO;
    qcomplex_t third_central = QC_ZERO;
    qcomplex_t sum_fourth_central = QC_ZERO;
    qcomplex_t sum_var_sq = QC_ZERO;

    if (!mean_out || !variance_out || !third_central_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qcomplex_t exp_hi;
        qcomplex_t exp_lo;
        qcomplex_t den;
        qcomplex_t first_ratio;
        qcomplex_t second_ratio;
        qcomplex_t third_ratio;
        qcomplex_t fourth_ratio;
        qcomplex_t first_term;
        qcomplex_t second_term;
        qcomplex_t third_term;
        qcomplex_t fourth_term;
        qcomplex_t raw1;
        qcomplex_t raw2;
        qcomplex_t raw3;
        qcomplex_t raw4;
        qcomplex_t var_i;
        qcomplex_t third_i;
        qcomplex_t fourth_i;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        if (qf_eq(coeffs[i], QF_ZERO))
            continue;

        exp_hi = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], hi[i])));
        exp_lo = qc_exp(qc_make(QF_ZERO, qf_mul(coeffs[i], lo[i])));
        den = qc_sub(exp_hi, exp_lo);
        first_ratio = qc_div(qc_sub(qc_mul(qc_make(hi[i], QF_ZERO), exp_hi),
                                    qc_mul(qc_make(lo[i], QF_ZERO), exp_lo)),
                             den);
        second_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(hi[i], hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(lo[i], lo[i]), QF_ZERO), exp_lo)),
                              den);
        third_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), QF_ZERO), exp_hi),
                                    qc_mul(qc_make(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), QF_ZERO), exp_lo)),
                             den);
        fourth_ratio = qc_div(qc_sub(qc_mul(qc_make(qf_mul(qf_mul(qf_mul(hi[i], hi[i]), hi[i]), hi[i]), QF_ZERO), exp_hi),
                                     qc_mul(qc_make(qf_mul(qf_mul(qf_mul(lo[i], lo[i]), lo[i]), lo[i]), QF_ZERO), exp_lo)),
                              den);
        first_term = qc_mul(qc_make(coeffs[i], QF_ZERO), first_ratio);
        second_term = qc_mul(qc_make(qf_mul(coeffs[i], coeffs[i]), QF_ZERO), second_ratio);
        third_term = qc_mul(qc_make(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), QF_ZERO), third_ratio);
        fourth_term = qc_mul(qc_make(qf_mul(qf_mul(qf_mul(coeffs[i], coeffs[i]), coeffs[i]), coeffs[i]), QF_ZERO), fourth_ratio);
        raw1 = qc_add(first_term, qc_make(QF_ZERO, QF_ONE));
        raw2 = qc_add(second_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(2.0)), first_term),
                             qc_make(qf_from_double(-2.0), QF_ZERO)));
        raw3 = qc_add(third_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(3.0)), second_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-6.0), QF_ZERO), first_term),
                                    qc_make(QF_ZERO, qf_from_double(-6.0)))));
        raw4 = qc_add(fourth_term,
                      qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(4.0)), third_term),
                             qc_add(qc_mul(qc_make(qf_from_double(-12.0), QF_ZERO), second_term),
                                    qc_add(qc_mul(qc_make(QF_ZERO, qf_from_double(-24.0)), first_term),
                                           qc_make(qf_from_double(24.0), QF_ZERO)))));
        var_i = qc_sub(raw2, qc_mul(raw1, raw1));
        third_i = qc_add(raw3,
                         qc_add(qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO), qc_mul(raw2, raw1)),
                                qc_mul(qc_make(qf_from_double(2.0), QF_ZERO), qc_mul(qc_mul(raw1, raw1), raw1))));
        fourth_i = qc_add(raw4,
                          qc_add(qc_mul(qc_make(qf_from_double(-4.0), QF_ZERO), qc_mul(raw3, raw1)),
                                 qc_add(qc_mul(qc_make(qf_from_double(6.0), QF_ZERO),
                                               qc_mul(raw2, qc_mul(raw1, raw1))),
                                        qc_mul(qc_make(qf_from_double(-3.0), QF_ZERO),
                                               qc_mul(qc_mul(raw1, raw1),
                                                      qc_mul(raw1, raw1))))));

        mean = qc_add(mean, raw1);
        variance = qc_add(variance, var_i);
        third_central = qc_add(third_central, third_i);
        sum_fourth_central = qc_add(sum_fourth_central, fourth_i);
        sum_var_sq = qc_add(sum_var_sq, qc_mul(var_i, var_i));
    }

    *mean_out = mean;
    *variance_out = variance;
    *third_central_out = third_central;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static qfloat_t eval_box_affine_poly_times_exp_common(size_t ndim, const qfloat_t *coeffs,
                                                      qfloat_t constant,
                                                      const qfloat_t *lo, const qfloat_t *hi,
                                                      const bool *active,
                                                      const qfloat_t poly[5])
{
    qfloat_t total = integral_exp_affine_box(ndim, coeffs, constant, lo, hi, active);
    qfloat_t mean;
    qfloat_t variance;
    qfloat_t third_central = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;

    if (qf_isnan(total))
        return QF_NAN;
    if (!qf_eq(poly[4], QF_ZERO)) {
        if (!exp_weighted_affine_fourth_moment(ndim, coeffs, constant, lo, hi, active,
                                               &mean, &variance, &third_central,
                                               &sum_fourth_central, &sum_var_sq))
            return QF_NAN;
    } else if (!qf_eq(poly[3], QF_ZERO)) {
        if (!exp_weighted_affine_third_moment(ndim, coeffs, constant, lo, hi, active,
                                              &mean, &variance, &third_central))
            return QF_NAN;
    } else if (!qf_eq(poly[2], QF_ZERO) || !qf_eq(poly[1], QF_ZERO)) {
        if (!exp_weighted_affine_stats(ndim, coeffs, constant, lo, hi, active,
                                       &mean, &variance))
            return QF_NAN;
    }

    return qf_mul(total,
                  eval_poly_deg4_from_real_moments(poly, mean, variance,
                                                   third_central,
                                                   sum_fourth_central,
                                                   sum_var_sq));
}

static qcomplex_t eval_box_affine_poly_times_exp_i_common(size_t ndim, const qfloat_t *coeffs,
                                                          qfloat_t constant,
                                                          const qfloat_t *lo, const qfloat_t *hi,
                                                          const bool *active,
                                                          const qfloat_t poly[5])
{
    qcomplex_t total = integral_exp_i_affine_box(ndim, coeffs, constant, lo, hi, active);
    qcomplex_t mean;
    qcomplex_t variance;
    qcomplex_t third_central = QC_ZERO;
    qcomplex_t sum_fourth_central = QC_ZERO;
    qcomplex_t sum_var_sq = QC_ZERO;

    if (qc_isnan(total))
        return qc_make(QF_NAN, QF_NAN);
    if (!qf_eq(poly[4], QF_ZERO)) {
        if (!exp_i_weighted_affine_fourth_moment(ndim, coeffs, constant, lo, hi, active,
                                                 &mean, &variance, &third_central,
                                                 &sum_fourth_central, &sum_var_sq))
            return qc_make(QF_NAN, QF_NAN);
    } else if (!qf_eq(poly[3], QF_ZERO)) {
        if (!exp_i_weighted_affine_third_moment(ndim, coeffs, constant, lo, hi, active,
                                                &mean, &variance, &third_central))
            return qc_make(QF_NAN, QF_NAN);
    } else if (!qf_eq(poly[2], QF_ZERO) || !qf_eq(poly[1], QF_ZERO)) {
        if (!exp_i_weighted_affine_stats(ndim, coeffs, constant, lo, hi, active,
                                         &mean, &variance))
            return qc_make(QF_NAN, QF_NAN);
    }

    return qc_mul(total,
                  eval_poly_deg4_from_complex_moments(poly, mean, variance,
                                                      third_central,
                                                      sum_fourth_central,
                                                      sum_var_sq));
}

static void negate_odd_poly_deg4(const qfloat_t in[5], qfloat_t out[5])
{
    out[0] = in[0];
    out[1] = qf_neg(in[1]);
    out[2] = in[2];
    out[3] = qf_neg(in[3]);
    out[4] = in[4];
}

static qfloat_t eval_box_affine_poly_deg4_times_special(affine_poly_special_kind_t kind,
                                                        size_t ndim,
                                                        const qfloat_t *coeffs,
                                                        qfloat_t constant,
                                                        const qfloat_t *lo, const qfloat_t *hi,
                                                        const bool *active,
                                                        const qfloat_t poly[5])
{
    switch (kind) {
    case AFFINE_POLY_SPECIAL_EXP:
        return eval_box_affine_poly_times_exp_common(ndim, coeffs, constant, lo, hi, active, poly);
    case AFFINE_POLY_SPECIAL_SIN:
        return eval_box_affine_poly_times_exp_i_common(ndim, coeffs, constant, lo, hi, active, poly).im;
    case AFFINE_POLY_SPECIAL_COS:
        return eval_box_affine_poly_times_exp_i_common(ndim, coeffs, constant, lo, hi, active, poly).re;
    case AFFINE_POLY_SPECIAL_SINH:
    case AFFINE_POLY_SPECIAL_COSH: {
        qfloat_t *neg_coeffs = malloc(ndim * sizeof(*neg_coeffs));
        qfloat_t neg_poly[5];
        qfloat_t pos;
        qfloat_t neg;

        if (!neg_coeffs)
            return QF_NAN;

        for (size_t i = 0; i < ndim; ++i)
            neg_coeffs[i] = qf_neg(coeffs[i]);
        negate_odd_poly_deg4(poly, neg_poly);

        pos = eval_box_affine_poly_times_exp_common(ndim, coeffs, constant, lo, hi, active, poly);
        neg = eval_box_affine_poly_times_exp_common(ndim, neg_coeffs, qf_neg(constant), lo, hi, active, neg_poly);
        free(neg_coeffs);
        return qf_mul_double(kind == AFFINE_POLY_SPECIAL_SINH ? qf_sub(pos, neg)
                                                              : qf_add(pos, neg),
                             0.5);
    }
    }

    return QF_NAN;
}

static bool uniform_affine_box_stats_deg4(size_t ndim,
                                          const qfloat_t *coeffs,
                                          qfloat_t constant,
                                          const qfloat_t *lo, const qfloat_t *hi,
                                          const bool *active,
                                          qfloat_t *volume_out,
                                          qfloat_t *mean_out,
                                          qfloat_t *variance_out,
                                          qfloat_t *sum_fourth_central_out,
                                          qfloat_t *sum_var_sq_out)
{
    qfloat_t mean = constant;
    qfloat_t variance = QF_ZERO;
    qfloat_t sum_fourth_central = QF_ZERO;
    qfloat_t sum_var_sq = QF_ZERO;
    qfloat_t volume = QF_ONE;
    qfloat_t two = qf_from_double(2.0);
    qfloat_t twelve = qf_from_double(12.0);
    qfloat_t eighty = qf_from_double(80.0);

    if (!volume_out || !mean_out || !variance_out ||
        !sum_fourth_central_out || !sum_var_sq_out)
        return false;

    for (size_t i = 0; i < ndim; ++i) {
        qfloat_t len;

        if (active && !active[i]) {
            if (!qf_eq(coeffs[i], QF_ZERO))
                return false;
            continue;
        }

        len = qf_sub(hi[i], lo[i]);
        volume = qf_mul(volume, len);

        if (!qf_eq(coeffs[i], QF_ZERO)) {
            qfloat_t midpoint = qf_div(qf_add(lo[i], hi[i]), two);
            qfloat_t coeff_sq = qf_mul(coeffs[i], coeffs[i]);
            qfloat_t len_sq = qf_mul(len, len);
            qfloat_t var_i = qf_div(qf_mul(coeff_sq, len_sq), twelve);
            qfloat_t fourth_i = qf_div(qf_mul(qf_mul(coeff_sq, coeff_sq),
                                              qf_mul(len_sq, len_sq)),
                                       eighty);

            mean = qf_add(mean, qf_mul(coeffs[i], midpoint));
            variance = qf_add(variance, var_i);
            sum_var_sq = qf_add(sum_var_sq, qf_mul(var_i, var_i));
            sum_fourth_central = qf_add(sum_fourth_central, fourth_i);
        }
    }

    *volume_out = volume;
    *mean_out = mean;
    *variance_out = variance;
    *sum_fourth_central_out = sum_fourth_central;
    *sum_var_sq_out = sum_var_sq;
    return true;
}

static qfloat_t eval_box_affine_poly_deg4(size_t ndim,
                                          const qfloat_t *coeffs,
                                          qfloat_t constant,
                                          const qfloat_t *lo, const qfloat_t *hi,
                                          const bool *active,
                                          const qfloat_t poly[5])
{
    qfloat_t volume;
    qfloat_t mean;
    qfloat_t variance;
    qfloat_t sum_fourth_central;
    qfloat_t sum_var_sq;

    if (!uniform_affine_box_stats_deg4(ndim, coeffs, constant, lo, hi, active,
                                       &volume, &mean, &variance,
                                       &sum_fourth_central, &sum_var_sq))
        return QF_NAN;

    return qf_mul(volume,
                  eval_poly_deg4_from_real_moments(poly, mean, variance, QF_ZERO,
                                                   sum_fourth_central, sum_var_sq));
}

static const struct {
    dv_pattern_unary_affine_kind_t unary_kind;
    affine_poly_special_kind_t special_kind;
} affine_special_kinds[] = {
    { DV_PATTERN_UNARY_EXP,  AFFINE_POLY_SPECIAL_EXP  },
    { DV_PATTERN_UNARY_SIN,  AFFINE_POLY_SPECIAL_SIN  },
    { DV_PATTERN_UNARY_COS,  AFFINE_POLY_SPECIAL_COS  },
    { DV_PATTERN_UNARY_SINH, AFFINE_POLY_SPECIAL_SINH },
    { DV_PATTERN_UNARY_COSH, AFFINE_POLY_SPECIAL_COSH }
};

typedef enum {
    SYMBOLIC_PLAN_NONE = 0,
    SYMBOLIC_PLAN_CONST,
    SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL,
    SYMBOLIC_PLAN_AFFINE_POLY,
    SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL,
    SYMBOLIC_PLAN_SCALED,
    SYMBOLIC_PLAN_ADD_SUB,
    SYMBOLIC_PLAN_MUL
} symbolic_plan_kind_t;

typedef struct {
    symbolic_plan_kind_t kind;
    qfloat_t scalar;
    qfloat_t constant;
    qfloat_t poly[5];
    affine_poly_special_kind_t special_kind;
    const dval_t *left;
    const dval_t *right;
    const dval_t *base;
    bool is_sub;
} symbolic_plan_t;

typedef struct {
    const dval_t *left;
    const dval_t *right;
    bool *used_left;
    bool *used_right;
} symbolic_binary_product_plan_t;

typedef struct {
    size_t ngroups;
    const dval_t **group_exprs;
    bool *group_owned;
    bool **group_used;
} symbolic_separable_product_plan_t;

static qfloat_t box_volume(size_t ndim, const qfloat_t *lo, const qfloat_t *hi,
                           const bool *active)
{
    qfloat_t v = QF_ONE;
    for (size_t i = 0; i < ndim; ++i)
        if (!active || active[i])
            v = qf_mul(v, qf_sub(hi[i], lo[i]));
    return v;
}

static bool try_eval_special_combination(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const qfloat_t *lo,
                                         const qfloat_t *hi,
                                         const bool *active,
                                         int depth,
                                         qfloat_t *out);

static void symbolic_plan_reset(symbolic_plan_t *plan)
{
    if (!plan)
        return;
    plan->kind = SYMBOLIC_PLAN_NONE;
    plan->left = NULL;
    plan->right = NULL;
    plan->base = NULL;
    plan->is_sub = false;
    plan->scalar = QF_ZERO;
    plan->constant = QF_ZERO;
    plan->special_kind = AFFINE_POLY_SPECIAL_EXP;
    for (size_t i = 0; i < 5; ++i)
        plan->poly[i] = QF_ZERO;
}

static void free_symbolic_binary_product_plan(symbolic_binary_product_plan_t *plan)
{
    if (!plan)
        return;
    free(plan->used_left);
    free(plan->used_right);
    plan->used_left = NULL;
    plan->used_right = NULL;
    plan->left = NULL;
    plan->right = NULL;
}

static void free_symbolic_separable_product_plan(symbolic_separable_product_plan_t *plan)
{
    if (!plan)
        return;

    if (plan->group_exprs && plan->group_owned) {
        for (size_t i = 0; i < plan->ngroups; ++i) {
            if (plan->group_owned[i] && plan->group_exprs[i])
                dv_free((dval_t *)plan->group_exprs[i]);
        }
    }

    if (plan->group_used) {
        for (size_t i = 0; i < plan->ngroups; ++i)
            free(plan->group_used[i]);
    }

    free(plan->group_exprs);
    free(plan->group_owned);
    free(plan->group_used);

    plan->ngroups = 0;
    plan->group_exprs = NULL;
    plan->group_owned = NULL;
    plan->group_used = NULL;
}

static bool append_product_factor(const dval_t *expr,
                                  const dval_t ***factors_io,
                                  size_t *count_io,
                                  size_t *cap_io)
{
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    const dval_t **grown;
    size_t new_cap;

    if (!expr || !factors_io || !count_io || !cap_io)
        return false;

    if (dv_match_mul_expr(expr, &left, &right))
        return append_product_factor(left, factors_io, count_io, cap_io) &&
               append_product_factor(right, factors_io, count_io, cap_io);

    if (*count_io == *cap_io) {
        new_cap = (*cap_io == 0) ? 4 : (*cap_io * 2);
        grown = realloc((void *)*factors_io, new_cap * sizeof(**factors_io));
        if (!grown)
            return false;
        *factors_io = grown;
        *cap_io = new_cap;
    }

    (*factors_io)[(*count_io)++] = expr;
    return true;
}

static bool usage_masks_overlap(size_t ndim, const bool *lhs, const bool *rhs)
{
    for (size_t i = 0; i < ndim; ++i) {
        if (lhs[i] && rhs[i])
            return true;
    }
    return false;
}

static bool build_direct_affine_plan(const dval_t *expr,
                                     size_t ndim,
                                     dval_t *const *vars,
                                     symbolic_plan_t *plan,
                                     qfloat_t *coeffs_out)
{
    qfloat_t constant;
    qfloat_t poly[5];

    if (!expr || !plan || !coeffs_out)
        return false;

    for (size_t i = 0; i < sizeof(affine_special_kinds) / sizeof(affine_special_kinds[0]); ++i) {
        if (!dv_match_affine_poly_deg4_times_unary_affine_kind(expr,
                                                               affine_special_kinds[i].unary_kind,
                                                               ndim, vars, poly, &constant,
                                                               coeffs_out))
            continue;
        plan->kind = SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL;
        plan->constant = constant;
        plan->special_kind = affine_special_kinds[i].special_kind;
        for (size_t j = 0; j < 5; ++j)
            plan->poly[j] = poly[j];
        return true;
    }

    if (dv_match_affine_poly_deg4(expr, ndim, vars, poly, &constant, coeffs_out)) {
        plan->kind = SYMBOLIC_PLAN_AFFINE_POLY;
        plan->constant = constant;
        for (size_t j = 0; j < 5; ++j)
            plan->poly[j] = poly[j];
        return true;
    }

    for (size_t i = 0; i < sizeof(affine_special_kinds) / sizeof(affine_special_kinds[0]); ++i) {
        if (!dv_match_unary_affine_kind(expr, affine_special_kinds[i].unary_kind,
                                        ndim, vars, &constant, coeffs_out))
            continue;
        plan->kind = SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL;
        plan->constant = constant;
        plan->special_kind = affine_special_kinds[i].special_kind;
        return true;
    }

    return false;
}

static bool eval_direct_affine_plan(const symbolic_plan_t *plan,
                                    size_t ndim,
                                    const qfloat_t *coeffs,
                                    const qfloat_t *lo,
                                    const qfloat_t *hi,
                                    const bool *active,
                                    qfloat_t *out)
{
    if (!plan || !coeffs || !out)
        return false;

    switch (plan->kind) {
    case SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL:
        *out = eval_box_affine_poly_deg4_times_special(plan->special_kind, ndim, coeffs,
                                                       plan->constant, lo, hi, active,
                                                       plan->poly);
        return !qf_isnan(*out);
    case SYMBOLIC_PLAN_AFFINE_POLY:
        *out = eval_box_affine_poly_deg4(ndim, coeffs, plan->constant, lo, hi, active,
                                         plan->poly);
        return !qf_isnan(*out);
    case SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL:
        *out = eval_box_affine_unary_special(plan->special_kind, ndim, coeffs,
                                             plan->constant, lo, hi, active);
        return !qf_isnan(*out);
    default:
        return false;
    }
}

static bool build_symbolic_plan(const dval_t *expr,
                                size_t ndim,
                                dval_t *const *vars,
                                symbolic_plan_t *plan,
                                qfloat_t *coeffs_out)
{
    qfloat_t constant;
    qfloat_t scale;
    const dval_t *base;
    const dval_t *left;
    const dval_t *right;
    bool is_sub;

    if (!expr || !plan)
        return false;

    symbolic_plan_reset(plan);

    if (dv_match_const_value(expr, &constant)) {
        plan->kind = SYMBOLIC_PLAN_CONST;
        plan->scalar = constant;
        return true;
    }

    if (coeffs_out && build_direct_affine_plan(expr, ndim, vars, plan, coeffs_out))
        return true;

    if (dv_match_scaled_expr(expr, &scale, &base)) {
        plan->kind = SYMBOLIC_PLAN_SCALED;
        plan->scalar = scale;
        plan->base = base;
        return true;
    }

    if (dv_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        plan->kind = SYMBOLIC_PLAN_ADD_SUB;
        plan->left = left;
        plan->right = right;
        plan->is_sub = is_sub;
        return true;
    }

    if (dv_match_mul_expr(expr, &left, &right)) {
        plan->kind = SYMBOLIC_PLAN_MUL;
        plan->left = left;
        plan->right = right;
        return true;
    }

    return false;
}

static bool build_binary_product_plan(const dval_t *left,
                                      const dval_t *right,
                                      size_t ndim,
                                      dval_t *const *vars,
                                      const bool *active,
                                      symbolic_binary_product_plan_t *plan)
{
    bool disjoint = true;

    if (!left || !right || !plan)
        return false;

    plan->left = left;
    plan->right = right;
    plan->used_left = malloc(ndim * sizeof(*plan->used_left));
    plan->used_right = malloc(ndim * sizeof(*plan->used_right));
    if ((ndim > 0) && (!plan->used_left || !plan->used_right)) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }
    if (!dv_collect_var_usage(left, ndim, vars, plan->used_left) ||
        !dv_collect_var_usage(right, ndim, vars, plan->used_right)) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }
    for (size_t i = 0; i < ndim; ++i) {
        if (active && !active[i]) {
            plan->used_left[i] = false;
            plan->used_right[i] = false;
        }
        if (plan->used_left[i] && plan->used_right[i]) {
            disjoint = false;
            break;
        }
    }
    if (!disjoint) {
        free_symbolic_binary_product_plan(plan);
        return false;
    }

    return true;
}

static bool eval_binary_product_plan(const symbolic_binary_product_plan_t *plan,
                                     size_t ndim,
                                     dval_t *const *vars,
                                     const qfloat_t *lo,
                                     const qfloat_t *hi,
                                     int depth,
                                     qfloat_t *out)
{
    qfloat_t left_v;
    qfloat_t right_v;

    if (!plan || !plan->left || !plan->right || !plan->used_left || !plan->used_right || !out)
        return false;

    if (!try_eval_special_combination(plan->left, ndim, vars, lo, hi, plan->used_left,
                                      depth + 1, &left_v) ||
        !try_eval_special_combination(plan->right, ndim, vars, lo, hi, plan->used_right,
                                      depth + 1, &right_v))
        return false;

    *out = qf_mul(left_v, right_v);
    return true;
}

static bool build_separable_product_plan(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const bool *active,
                                         symbolic_separable_product_plan_t *plan)
{
    const dval_t **factors = NULL;
    bool **factor_used = NULL;
    size_t *component_ids = NULL;
    size_t factor_count = 0;
    size_t factor_cap = 0;
    size_t component_count = 0;
    bool matched = false;

    if (!expr || !plan)
        return false;

    plan->ngroups = 0;
    plan->group_exprs = NULL;
    plan->group_owned = NULL;
    plan->group_used = NULL;

    if (!append_product_factor(expr, &factors, &factor_count, &factor_cap) || factor_count < 2)
        goto cleanup;

    factor_used = calloc(factor_count, sizeof(*factor_used));
    component_ids = malloc(factor_count * sizeof(*component_ids));
    if (!factor_used || !component_ids)
        goto cleanup;

    for (size_t i = 0; i < factor_count; ++i) {
        factor_used[i] = malloc(ndim * sizeof(*factor_used[i]));
        if ((ndim > 0) && !factor_used[i])
            goto cleanup;
        if (!dv_collect_var_usage(factors[i], ndim, vars, factor_used[i]))
            goto cleanup;
        for (size_t j = 0; j < ndim; ++j) {
            if (active && !active[j])
                factor_used[i][j] = false;
        }
        component_ids[i] = SIZE_MAX;
    }

    for (size_t i = 0; i < factor_count; ++i) {
        if (component_ids[i] != SIZE_MAX)
            continue;
        component_ids[i] = component_count;
        for (size_t changed = 1; changed != 0;) {
            changed = 0;
            for (size_t j = 0; j < factor_count; ++j) {
                if (component_ids[j] == component_count)
                    continue;
                for (size_t k = 0; k < factor_count; ++k) {
                    if (component_ids[k] != component_count)
                        continue;
                    if (usage_masks_overlap(ndim, factor_used[j], factor_used[k])) {
                        component_ids[j] = component_count;
                        changed = 1;
                        break;
                    }
                }
            }
        }
        ++component_count;
    }

    if (component_count < 2)
        goto cleanup;

    plan->group_exprs = calloc(component_count, sizeof(*plan->group_exprs));
    plan->group_owned = calloc(component_count, sizeof(*plan->group_owned));
    plan->group_used = calloc(component_count, sizeof(*plan->group_used));
    if (!plan->group_exprs || !plan->group_owned || !plan->group_used)
        goto cleanup;

    for (size_t c = 0; c < component_count; ++c) {
        dval_t *group_expr = NULL;
        bool owned = false;

        plan->group_used[c] = calloc(ndim, sizeof(*plan->group_used[c]));
        if ((ndim > 0) && !plan->group_used[c])
            goto cleanup;

        for (size_t i = 0; i < factor_count; ++i) {
            dval_t *next_expr;

            if (component_ids[i] != c)
                continue;

            for (size_t j = 0; j < ndim; ++j)
                plan->group_used[c][j] = plan->group_used[c][j] || factor_used[i][j];

            if (!group_expr) {
                group_expr = (dval_t *)factors[i];
                continue;
            }

            next_expr = dv_mul(group_expr, (dval_t *)factors[i]);
            if (!next_expr) {
                if (owned)
                    dv_free(group_expr);
                goto cleanup;
            }
            if (owned)
                dv_free(group_expr);
            group_expr = next_expr;
            owned = true;
        }

        if (!group_expr)
            goto cleanup;

        plan->group_exprs[c] = group_expr;
        plan->group_owned[c] = owned;
    }

    plan->ngroups = component_count;
    matched = true;

cleanup:
    if (!matched)
        free_symbolic_separable_product_plan(plan);
    if (factor_used) {
        for (size_t i = 0; i < factor_count; ++i)
            free(factor_used[i]);
    }
    free(factor_used);
    free(component_ids);
    free((void *)factors);
    return matched;
}

static bool eval_separable_product_plan(const symbolic_separable_product_plan_t *plan,
                                        size_t ndim,
                                        dval_t *const *vars,
                                        const qfloat_t *lo,
                                        const qfloat_t *hi,
                                        int depth,
                                        qfloat_t *out)
{
    qfloat_t total = QF_ONE;
    qfloat_t term;

    if (!plan || !plan->group_exprs || !plan->group_used || plan->ngroups < 2 || !out)
        return false;

    for (size_t i = 0; i < plan->ngroups; ++i) {
        if (!try_eval_special_combination(plan->group_exprs[i], ndim, vars, lo, hi,
                                          plan->group_used[i], depth + 1, &term))
            return false;
        total = qf_mul(total, term);
    }

    *out = total;
    return true;
}

static bool try_eval_special_combination(const dval_t *expr,
                                         size_t ndim,
                                         dval_t *const *vars,
                                         const qfloat_t *lo,
                                         const qfloat_t *hi,
                                         const bool *active,
                                         int depth,
                                         qfloat_t *out)
{
    symbolic_plan_t plan;
    qfloat_t *coeffs;
    qfloat_t left_v;
    qfloat_t right_v;

    if (!expr || !out || depth > 32)
        return false;

    coeffs = malloc(ndim * sizeof(*coeffs));
    if ((ndim > 0) && !coeffs)
        return false;

    if (!build_symbolic_plan(expr, ndim, vars, &plan, coeffs)) {
        free(coeffs);
        return false;
    }

    switch (plan.kind) {
    case SYMBOLIC_PLAN_CONST:
        *out = qf_mul(plan.scalar, box_volume(ndim, lo, hi, active));
        free(coeffs);
        return true;
    case SYMBOLIC_PLAN_AFFINE_POLY_SPECIAL:
    case SYMBOLIC_PLAN_AFFINE_POLY:
    case SYMBOLIC_PLAN_AFFINE_UNARY_SPECIAL: {
        bool ok = eval_direct_affine_plan(&plan, ndim, coeffs, lo, hi, active, out);
        free(coeffs);
        return ok;
    }
    case SYMBOLIC_PLAN_SCALED: {
        bool ok = try_eval_special_combination(plan.base, ndim, vars, lo, hi, active,
                                               depth + 1, &left_v);
        free(coeffs);
        if (!ok)
            return false;
        *out = qf_mul(plan.scalar, left_v);
        return true;
    }
    case SYMBOLIC_PLAN_ADD_SUB: {
        bool ok_left = try_eval_special_combination(plan.left, ndim, vars, lo, hi, active,
                                                    depth + 1, &left_v);
        bool ok_right = ok_left &&
                        try_eval_special_combination(plan.right, ndim, vars, lo, hi, active,
                                                     depth + 1, &right_v);
        free(coeffs);
        if (!ok_right)
            return false;
        *out = plan.is_sub ? qf_sub(left_v, right_v) : qf_add(left_v, right_v);
        return true;
    }
    case SYMBOLIC_PLAN_MUL: {
        symbolic_separable_product_plan_t separable_plan = {0};
        symbolic_binary_product_plan_t binary_plan = {0};
        bool ok = false;

        if (build_separable_product_plan(expr, ndim, vars, active, &separable_plan))
            ok = eval_separable_product_plan(&separable_plan, ndim, vars, lo, hi, depth, out);
        else if (build_binary_product_plan(plan.left, plan.right, ndim, vars, active,
                                           &binary_plan))
            ok = eval_binary_product_plan(&binary_plan, ndim, vars, lo, hi, depth, out);

        free_symbolic_separable_product_plan(&separable_plan);
        free_symbolic_binary_product_plan(&binary_plan);
        free(coeffs);
        return ok;
    }
    case SYMBOLIC_PLAN_NONE:
    default:
        free(coeffs);
        return false;
    }
}

static int try_integral_multi_special_affine(integrator_t *ig, dval_t *expr,
                                             size_t ndim, dval_t *const *vars,
                                             const qfloat_t *lo, const qfloat_t *hi,
                                             qfloat_t *result, qfloat_t *error_est)
{
    qfloat_t total;

    if (!try_eval_special_combination(expr, ndim, vars, lo, hi, NULL, 0, &total))
        return 0;
    if (qf_isnan(total))
        return -1;
    ig->last_intervals = 1;
    *result = total;
    if (error_est)
        *error_est = QF_ZERO;
    return 1;
}

int ig_integral_multi(integrator_t *ig, dval_t *expr,
                      size_t ndim, dval_t * const *vars,
                      const qfloat_t *lo, const qfloat_t *hi,
                      qfloat_t *result, qfloat_t *error_est)
{
    int fast_status;

    if (!ig || !expr || ndim == 0 || !vars || !lo || !hi || !result) return -1;

    fast_status = try_integral_multi_special_affine(ig, expr, ndim, vars, lo, hi,
                                                    result, error_est);
    if (fast_status != 0)
        return fast_status > 0 ? 0 : -1;

    size_t nexprs = (size_t)1 << ndim;
    dval_t **deriv_exprs = malloc(nexprs * sizeof(dval_t *));
    if (!deriv_exprs) return -1;

    deriv_exprs[0] = expr;
    for (size_t mask = 1; mask < nexprs; mask++) {
        int i = __builtin_ctz((unsigned int)mask);
        size_t prev = mask ^ ((size_t)1 << i);
        deriv_exprs[mask] = dv_create_2nd_deriv(deriv_exprs[prev], vars[i], vars[i]);
        if (!deriv_exprs[mask]) {
            for (size_t j = 1; j < mask; j++) dv_free(deriv_exprs[j]);
            free(deriv_exprs);
            return -1;
        }
    }

    /* Detect if all deriv_exprs evaluate to the same function.
     * If so, eval_nd_t15/turan skip redundant recursive calls, and gturan_eval_dv
     * skips redundant evaluations. Uses two test points to distinguish functions. */
    int all_same = (nexprs > 1);
    if (all_same) {
        static const double tp[2] = { 0.31415, 0.71828 };
        for (int t = 0; t < 2 && all_same; t++) {
            for (size_t v = 0; v < ndim; v++)
                dv_set_val_qf(vars[v], qf_from_double(tp[t]));
            qfloat_t ref = dv_eval_qf(deriv_exprs[0]);
            for (size_t mask = 1; mask < nexprs && all_same; mask++) {
                if (!qf_eq(ref, dv_eval_qf(deriv_exprs[mask])))
                    all_same = 0;
            }
        }
    }

    multi_ctx_t ctx = { deriv_exprs, (dval_t **)vars, lo, hi, all_same };

    size_t capacity = 64;
    subinterval_t *intervals = malloc(capacity * sizeof(subinterval_t));
    if (!intervals) {
        for (size_t j = 1; j < nexprs; j++) dv_free(deriv_exprs[j]);
        free(deriv_exprs);
        return -1;
    }

    int outer = (int)ndim - 1;
    qfloat_t t15, t4;
    eval_nd_turan(&ctx, outer, 0, lo[outer], hi[outer], &t15, &t4);

    intervals[0].a      = lo[outer];
    intervals[0].b      = hi[outer];
    intervals[0].result = t15;
    intervals[0].error  = qf_abs(qf_sub(t15, t4));

    size_t   count     = 1;
    qfloat_t total     = t15;
    qfloat_t total_err = intervals[0].error;
    int      status    = 0;

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

        eval_nd_turan(&ctx, outer, 0, intervals[worst].a, mid, &t15, &t4);
        left.a = intervals[worst].a;  left.b = mid;
        left.result = t15;  left.error = qf_abs(qf_sub(t15, t4));

        eval_nd_turan(&ctx, outer, 0, mid, intervals[worst].b, &t15, &t4);
        right.a = mid;  right.b = intervals[worst].b;
        right.result = t15;  right.error = qf_abs(qf_sub(t15, t4));

        total     = qf_add(qf_sub(total,     intervals[worst].result),
                           qf_add(left.result, right.result));
        total_err = qf_add(qf_sub(total_err, intervals[worst].error),
                           qf_add(left.error,  right.error));

        if (count >= capacity) {
            capacity *= 2;
            subinterval_t *tmp = realloc(intervals, capacity * sizeof(subinterval_t));
            if (!tmp) {
                free(intervals);
                for (size_t j = 1; j < nexprs; j++) dv_free(deriv_exprs[j]);
                free(deriv_exprs);
                return -1;
            }
            intervals = tmp;
        }

        intervals[worst]   = left;
        intervals[count++] = right;
    }

    ig->last_intervals = count;
    *result = total;
    if (error_est) *error_est = qf_abs(total_err);

    free(intervals);
    for (size_t j = 1; j < nexprs; j++) dv_free(deriv_exprs[j]);
    free(deriv_exprs);
    return status;
}
