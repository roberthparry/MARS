#include <float.h>
#include <math.h>
#include <stdio.h>

#include "mfloat.h"
#include "qfloat.h"

typedef struct {
    const char *label;
    double start;
    double end;
    int samples;
    qfloat_t (*qf_fn)(qfloat_t);
    int (*mf_fn)(mfloat_t *);
} unary_range_case_t;

static int qfloat_is_normalized(qfloat_t value)
{
    double hi = value.hi;
    double lo = value.lo;
    double abs_hi, ulp;
    int exp2;

    if (isnan(hi) || isnan(lo) || isinf(hi))
        return 1;
    if (hi == 0.0)
        return lo == 0.0;

    abs_hi = fabs(hi);
    if (abs_hi < DBL_MIN) {
        ulp = ldexp(1.0, -1074);
    } else {
        frexp(abs_hi, &exp2);
        ulp = ldexp(1.0, exp2 - 53);
    }

    return fabs(lo) <= 0.5 * ulp;
}

static mfloat_t *ref_pi(void) { return mf_pi(); }
static mfloat_t *ref_2pi(void)
{
    mfloat_t *x = mf_pi();
    mf_mul_long(x, 2);
    return x;
}
static mfloat_t *ref_pi_2(void)
{
    mfloat_t *x = mf_pi();
    mfloat_t *two = mf_create_long(2);
    mf_div(x, two);
    mf_free(two);
    return x;
}
static mfloat_t *ref_pi_4(void)
{
    mfloat_t *x = mf_pi();
    mfloat_t *four = mf_create_long(4);
    mf_div(x, four);
    mf_free(four);
    return x;
}
static mfloat_t *ref_3pi_4(void)
{
    mfloat_t *x = mf_pi();
    mf_mul_long(x, 3);
    mfloat_t *four = mf_create_long(4);
    mf_div(x, four);
    mf_free(four);
    return x;
}
static mfloat_t *ref_pi_6(void)
{
    mfloat_t *x = mf_pi();
    mfloat_t *six = mf_create_long(6);
    mf_div(x, six);
    mf_free(six);
    return x;
}
static mfloat_t *ref_pi_3(void)
{
    mfloat_t *x = mf_pi();
    mfloat_t *three = mf_create_long(3);
    mf_div(x, three);
    mf_free(three);
    return x;
}
static mfloat_t *ref_2_pi(void)
{
    mfloat_t *pi = mf_pi();
    mfloat_t *x = mf_create_long(2);
    mf_div(x, pi);
    mf_free(pi);
    return x;
}
static mfloat_t *ref_e(void) { return mf_e(); }
static mfloat_t *ref_inv_e(void)
{
    mfloat_t *e = mf_e();
    mfloat_t *x = mf_create_long(1);
    mf_div(x, e);
    mf_free(e);
    return x;
}
static mfloat_t *ref_ln2(void)
{
    mfloat_t *x = mf_create_long(2);
    mf_log(x);
    return x;
}
static mfloat_t *ref_invln2(void)
{
    mfloat_t *ln2 = ref_ln2();
    mfloat_t *x = mf_create_long(1);
    mf_div(x, ln2);
    mf_free(ln2);
    return x;
}
static mfloat_t *ref_sqrt_half(void)
{
    mfloat_t *x = mf_clone(MF_HALF);
    mf_sqrt(x);
    return x;
}
static mfloat_t *ref_sqrt_pi(void)
{
    return mf_clone(MF_SQRT_PI);
}
static mfloat_t *ref_sqrt1onpi(void)
{
    mfloat_t *pi = mf_pi();
    mfloat_t *x = mf_create_long(1);
    mf_div(x, pi);
    mf_free(pi);
    mf_sqrt(x);
    return x;
}
static mfloat_t *ref_2_sqrtpi(void)
{
    mfloat_t *x = mf_create_long(2);
    mfloat_t *root_pi = mf_clone(MF_SQRT_PI);
    mf_div(x, root_pi);
    mf_free(root_pi);
    return x;
}
static mfloat_t *ref_inv_sqrt_2pi(void)
{
    mfloat_t *x = mf_create_long(2);
    mfloat_t *pi = mf_pi();
    mfloat_t *one = mf_create_long(1);
    mf_mul(x, pi);
    mf_free(pi);
    mf_sqrt(x);
    mf_div(one, x);
    mf_free(x);
    return one;
}
static mfloat_t *ref_log_sqrt_2pi(void)
{
    mfloat_t *x = mf_clone(MF_SQRT2);
    mfloat_t *root_pi = mf_clone(MF_SQRT_PI);
    mf_mul(x, root_pi);
    mf_free(root_pi);
    mf_log(x);
    return x;
}
static mfloat_t *ref_ln_2pi(void)
{
    mfloat_t *x = mf_create_long(2);
    mfloat_t *pi = mf_pi();
    mf_mul(x, pi);
    mf_free(pi);
    mf_log(x);
    return x;
}
static mfloat_t *ref_euler_mascheroni(void)
{
    return mf_euler_mascheroni();
}

static void compare_against_reference(qfloat_t got,
                                      const mfloat_t *reference,
                                      double *abs_err_out,
                                      double *rel_err_out)
{
    mfloat_t *got_m = mf_create_qfloat(got);
    mfloat_t *diff = mf_clone(reference);
    mfloat_t *ref_abs = mf_clone(reference);

    mf_sub(diff, got_m);
    mf_abs(diff);
    *abs_err_out = mf_to_double(diff);

    mf_abs(ref_abs);
    if (mf_is_zero(ref_abs)) {
        *rel_err_out = *abs_err_out;
    } else {
        mf_div(diff, ref_abs);
        mf_abs(diff);
        *rel_err_out = mf_to_double(diff);
    }

    mf_free(got_m);
    mf_free(diff);
    mf_free(ref_abs);
}

static void audit_public_constants(void)
{
    qfloat_t ref_q[20];
    const char *names[20];
    qfloat_t values[20];
    mfloat_t *ref_m[20];
    size_t exact_count = 0u;
    size_t case_count = 20u;

    ref_m[0] = ref_pi();
    ref_q[0] = mf_to_qfloat(ref_m[0]);
    names[0] = "QF_PI";
    values[0] = QF_PI;

    names[1] = "QF_2PI";
    values[1] = QF_2PI;
    ref_q[1] = qf_mul(QF_TWO, ref_q[0]);
    ref_m[1] = mf_create_qfloat(ref_q[1]);

    names[2] = "QF_PI_2";
    values[2] = QF_PI_2;
    ref_q[2] = qf_mul(QF_HALF, ref_q[0]);
    ref_m[2] = mf_create_qfloat(ref_q[2]);

    names[3] = "QF_PI_4";
    values[3] = QF_PI_4;
    ref_q[3] = qf_mul(QF_HALF, ref_q[2]);
    ref_m[3] = mf_create_qfloat(ref_q[3]);

    names[4] = "QF_3PI_4";
    values[4] = QF_3PI_4;
    ref_q[4] = qf_mul(qf_from_double(3.0), ref_q[3]);
    ref_m[4] = mf_create_qfloat(ref_q[4]);

    names[5] = "QF_PI_6";
    values[5] = QF_PI_6;
    ref_q[5] = qf_div(ref_q[0], qf_from_double(6.0));
    ref_m[5] = mf_create_qfloat(ref_q[5]);

    names[6] = "QF_PI_3";
    values[6] = QF_PI_3;
    ref_q[6] = qf_div(ref_q[0], qf_from_double(3.0));
    ref_m[6] = mf_create_qfloat(ref_q[6]);

    names[7] = "QF_2_PI";
    values[7] = QF_2_PI;
    ref_q[7] = qf_div(QF_TWO, ref_q[0]);
    ref_m[7] = mf_create_qfloat(ref_q[7]);

    ref_m[8] = ref_e();
    ref_q[8] = mf_to_qfloat(ref_m[8]);
    names[8] = "QF_E";
    values[8] = QF_E;

    names[9] = "QF_INV_E";
    values[9] = QF_INV_E;
    ref_q[9] = qf_div(QF_ONE, ref_q[8]);
    ref_m[9] = mf_create_qfloat(ref_q[9]);

    ref_m[10] = ref_ln2();
    ref_q[10] = mf_to_qfloat(ref_m[10]);
    names[10] = "QF_LN2";
    values[10] = QF_LN2;

    names[11] = "QF_INVLN2";
    values[11] = QF_INVLN2;
    ref_q[11] = qf_div(QF_ONE, ref_q[10]);
    ref_m[11] = mf_create_qfloat(ref_q[11]);

    {
        mfloat_t *sqrt2 = mf_clone(MF_SQRT2);
        ref_q[12] = qf_mul(QF_HALF, mf_to_qfloat(sqrt2));
        mf_free(sqrt2);
    }
    names[12] = "QF_SQRT_HALF";
    values[12] = QF_SQRT_HALF;
    ref_m[12] = mf_create_qfloat(ref_q[12]);

    ref_m[13] = ref_sqrt_pi();
    ref_q[13] = mf_to_qfloat(ref_m[13]);
    names[13] = "QF_SQRT_PI";
    values[13] = QF_SQRT_PI;

    names[14] = "QF_SQRT1ONPI";
    values[14] = QF_SQRT1ONPI;
    ref_q[14] = qf_div(QF_ONE, ref_q[13]);
    ref_m[14] = mf_create_qfloat(ref_q[14]);

    names[15] = "QF_2_SQRTPI";
    values[15] = QF_2_SQRTPI;
    ref_q[15] = qf_mul(QF_TWO, ref_q[14]);
    ref_m[15] = mf_create_qfloat(ref_q[15]);

    names[16] = "QF_INV_SQRT_2PI";
    values[16] = QF_INV_SQRT_2PI;
    ref_q[16] = qf_mul(ref_q[12], ref_q[14]);
    ref_m[16] = mf_create_qfloat(ref_q[16]);

    ref_m[17] = ref_log_sqrt_2pi();
    ref_q[17] = mf_to_qfloat(ref_m[17]);
    names[17] = "QF_LOG_SQRT_2PI";
    values[17] = QF_LOG_SQRT_2PI;

    ref_m[18] = ref_ln_2pi();
    ref_q[18] = mf_to_qfloat(ref_m[18]);
    names[18] = "QF_LN_2PI";
    values[18] = QF_LN_2PI;

    ref_m[19] = ref_euler_mascheroni();
    ref_q[19] = mf_to_qfloat(ref_m[19]);
    names[19] = "QF_EULER_MASCHERONI";
    values[19] = QF_EULER_MASCHERONI;

    printf("Public constants:\n");
    for (size_t i = 0; i < case_count; i++) {
        double abs_err, rel_err;
        int exact = qf_eq(values[i], ref_q[i]);
        int normalized = qfloat_is_normalized(values[i]);

        compare_against_reference(values[i], ref_m[i], &abs_err, &rel_err);

        if (exact)
            exact_count++;

        printf("  %-18s normalized=%s exact=%s abs_err=%.3e rel_err=%.3e\n",
               names[i],
               normalized ? "yes" : "NO",
               exact ? "yes" : "NO",
               abs_err,
               rel_err);
        if (!exact) {
            printf("    suggested = { %.17g, %.17g }\n",
                   ref_q[i].hi,
                   ref_q[i].lo);
        }

        mf_free(ref_m[i]);
    }
    printf("  exact matches: %zu/%zu\n\n",
           exact_count,
           case_count);
}

static void audit_unary_range(const unary_range_case_t *test)
{
    double max_abs = 0.0;
    double max_rel = 0.0;
    double at_abs = test->start;
    double at_rel = test->start;

    for (int i = 0; i < test->samples; i++) {
        double t = test->samples == 1 ? 0.0 : (double)i / (double)(test->samples - 1);
        double x = test->start + (test->end - test->start) * t;
        qfloat_t qx = qf_from_double(x);
        qfloat_t qy = test->qf_fn(qx);
        mfloat_t *mx = mf_create_double(x);
        double abs_err, rel_err;

        test->mf_fn(mx);
        compare_against_reference(qy, mx, &abs_err, &rel_err);

        if (abs_err > max_abs) {
            max_abs = abs_err;
            at_abs = x;
        }
        if (rel_err > max_rel) {
            max_rel = rel_err;
            at_rel = x;
        }

        mf_free(mx);
    }

    printf("  %-26s max_abs=%.3e @ %.17g   max_rel=%.3e @ %.17g\n",
           test->label, max_abs, at_abs, max_rel, at_rel);
}

static void audit_function_ranges(void)
{
    static const unary_range_case_t cases[] = {
        { "qf_exp kernel range", -0.125, 0.125, 41, qf_exp, mf_exp },
        { "qf_sin kernel range", -0.7853981633974483, 0.7853981633974483, 41, qf_sin, mf_sin },
        { "qf_cos kernel range", -0.7853981633974483, 0.7853981633974483, 41, qf_cos, mf_cos },
        { "qf_atan kernel range", -0.35, 0.35, 41, qf_atan, mf_atan },
        { "qf_gamma core regime", 0.5, 2.0, 41, qf_gamma, mf_gamma },
        { "qf_lgamma core regime", 1.0, 2.0, 41, qf_lgamma, mf_lgamma },
        { "qf_digamma core 1..2", 1.0, 2.0, 41, qf_digamma, mf_digamma },
        { "qf_digamma core 8..20", 8.0, 20.0, 41, qf_digamma, mf_digamma },
    };

    printf("Function-level audits for fitted/analytic tables:\n");
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        audit_unary_range(&cases[i]);
    printf("\n");
}

int main(void)
{
    mf_set_default_precision(256u);

    audit_public_constants();
    if (0)
        audit_function_ranges();
    return 0;
}
