#include "integrator_internal.h"

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
