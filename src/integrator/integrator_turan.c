#include "integrator_internal.h"

/* -------------------------------------------------------------------
 * Turán T15/T4 nodes and weights — f(x) + f''(x) rule, degree 31
 *
 * Nodes are zeros of the Jacobi polynomial P_15^(2,2)(x) on [-1,1],
 * giving {0, ±t₁, …, ±t₇} — 8 distinct positions.
 * T15 uses all 8; T4 (nested error estimator) uses positions 0,2,4,6.
 * ------------------------------------------------------------------- */

/* Turán abscissae (non-negative), index 0 = center */
qfloat_t tn_node[TN_NODES] = {
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
qfloat_t tn_wa[TN_NODES] = {
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
qfloat_t tn_wd[TN_NODES] = {
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
qfloat_t tn4_wa[TN_T4] = {
    { 0.3873750227417537,  -1.8653873125071e-17   },
    { 0.3573488892017908,   1.0918523408671061e-17 },
    { 0.2847343352610255,   1.9803445948553018e-17 },
    { 0.16422926416630682,  6.360542820940334e-18  },
};

/* T4 nested f'' weights (nodes at T15 positions 0,2,4,6) */
qfloat_t tn4_wd[TN_T4] = {
    { 0.010056786351651225, -5.960144669967088e-19 },
    { 0.007964473972150962, -1.821340375196974e-19 },
    { 0.0037567107666183625,-7.371952593800035e-20 },
    { 0.0007208517799672414, 4.7764084118324135e-20},
};

/* -------------------------------------------------------------------
 * Single-interval Turán T15/T4 evaluation via dval_t AD
 *
 * T15 uses f and f'' at all 8 node positions (degree 31).
 * T4 reuses the evaluations at positions 0,2,4,6 (degree 15).
 * The change-of-variable g(t)=f(c+h*t) gives g''(t) = h²·f''(c+h*t).
 * ------------------------------------------------------------------- */

void gturan_eval_dv(dval_t *expr, dval_t *x_var, dval_t *d2_expr,
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
