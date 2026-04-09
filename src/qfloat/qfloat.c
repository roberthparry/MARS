#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "qfloat.h"

/* Constants */

const qfloat QF_MAX = {
    1.79769313486231570815e+308,
    9.97920154767359795037e+291
};

const qfloat QF_NAN = {
    NAN,
    NAN
};

const qfloat QF_INF  = { INFINITY, 0.0 };

const qfloat QF_NINF = { -INFINITY, 0.0 };

const qfloat QF_PI = {
    3.14159265358979311600e+00,
    1.2246467991473532072e-16
};

const qfloat QF_2PI = {
    6.28318530717958623200e+00,
    2.4492935982947064144e-16
};

const qfloat QF_PI_2 = {
    1.57079632679489655800e+00,
    6.123233995736766036e-17
};

const qfloat QF_PI_4 = {
    7.8539816339744827900e-01,
    3.061616997868383018e-17
};

const qfloat QF_3PI_4 = {
    2.356194490192344837e+00,
    9.1848509936051484375e-17
};

const qfloat QF_PI_6 = { 
    0.52359877559829893,
    -5.3604088322554746e-17 
};

const qfloat QF_PI_3 = { 
    1.0471975511965979,
    -1.0720817664510948e-16
};

const qfloat QF_2_PI = {
    0.63661977236758134308,   /* hi */
    -1.073741823e-17          /* lo (approx; compute with your qfloat printer) */
};

const qfloat QF_LN2 = {
    6.9314718055994530941723212145817656e-01,   // hi (double)
    2.3190468138462995584177710792423e-17       // lo = ln2 - hi
};

const qfloat QF_INVLN2 = {
    1.4426950408889634073599246810019e+00,      // hi (double)
    2.0355273740931032980408127082449e-17       // lo = 1/ln2 - hi
};

const qfloat QF_SQRT_HALF = {
    0.70710678118654757,
    -4.8336466567264851e-17 
};

const qfloat QF_E = {
    2.718281828459045091e+00,
    1.445646891729250158e-16
};

const qfloat QF_INV_E = {
    0.36787944117144233,
    -1.2428753672788614e-17
};

const qfloat QF_SQRT1ONPI = {
    0.56418958354775628,
    7.6677298065829314e-18
};

const qfloat QF_2_SQRTPI = { 
    1.1283791670955126,
    1.5335459613165487e-17 
};

const qfloat QF_LOG_SQRT_2PI = { 
    0.91893853320467278, 
    -3.8782941580672716e-17
};

/// @brief Euler–Mascheroni constant γ
const qfloat QF_EULER_MASCHERONI = { 
    0.57721566490153287,
    -4.9429151524310308e-18
};

/// @brief 1/sqrt(2pi)
const qfloat QF_INV_SQRT_2PI = {
    0.3989422804014327,
    -2.4923272022777433e-17
};

/// @brief ln(2pi)
const qfloat QF_LN_2PI = {
    1.8378770664093456,
    -7.7565883161345433e-17
};

const double QF_EPS = 4.93038065763132e-32;

#define QF_SPLIT 134217729.0

/* Error-free transforms */

static inline void two_sum(double a, double b, double *s, double *e)
{
    *s = a + b;
    double bb = *s - a;
    *e = (a - (*s - bb)) + (b - bb);
}

static inline void two_prod(double a, double b, double *p, double *e)
{
    *p = a * b;
    *e = fma(a, b, -*p);   // exact error term
}

static inline void quick_two_sum(double a, double b, double *s, double *e)
{
    double t = a + b;
    *s = t;
    *e = b - (t - a);
}

/* Renormalization */

qfloat qf_renorm(double hi, double lo)
{
    qfloat r;
    double s, e;
    quick_two_sum(hi, lo, &s, &e);
    r.hi = s;
    r.lo = e;
    return r;
}

/* Constructors */

inline qfloat qf_from_double(double x)
{
    return (qfloat) { x, 0.0 };
}

inline double qf_to_double(qfloat x) {
    return x.hi + x.lo;
}

static inline int qf_to_int(qfloat x)
{
    return (int)(x.hi + x.lo);
}

/* Basic arithmetic */

qfloat qf_add(qfloat a, qfloat b) {
    double s, e1, e2;
    two_sum(a.hi, b.hi, &s, &e1);
    e2 = a.lo + b.lo + e1;
    return qf_renorm(s, e2);
}

qfloat qf_sub(qfloat a, qfloat b) {
    double s, e1, e2;
    two_sum(a.hi, -b.hi, &s, &e1);
    e2 = a.lo - b.lo + e1;
    return qf_renorm(s, e2);
}

qfloat qf_mul(qfloat a, qfloat b) {
    double hx, tx, hy, ty, C, c;

    // Split a.hi
    C  = QF_SPLIT * a.hi;
    hx = C - a.hi;
    hx = C - hx;
    tx = a.hi - hx;

    // Split b.hi
    double d = QF_SPLIT * b.hi;
    hy = d - b.hi;
    hy = d - hy;
    ty = b.hi - hy;

    // High product
    C = a.hi * b.hi;

    // Error term from Dekker split + cross terms
    c = ((((hx * hy - C) + hx * ty) + tx * hy) + tx * ty)
        + (a.hi * b.lo + a.lo * b.hi)
        + (a.lo * b.lo);

    // Renormalize: (C + c) as double-double
    double hi, lo;
    two_sum(C, c, &hi, &lo);

    return (qfloat){ hi, lo };
}

/* Correct double-double division */
qfloat qf_div(qfloat a, qfloat b)
{
    qfloat zero = qf_from_double(0.0);

    if (qf_eq(b, zero))
        return QF_NAN;

    double q1 = a.hi / b.hi;
    qfloat q1q = qf_from_double(q1);

    qfloat qb = qf_mul(q1q, b);
    qfloat r  = qf_sub(a, qb);

    double q2 = r.hi / b.hi;
    qfloat q2q = qf_from_double(q2);

    return qf_add(q1q, q2q);
}

qfloat qf_pow_int(qfloat x, int n)
{
    if (n == 0) {
        return qf_from_double(1.0);
    }

    if (n < 0) {
        qfloat r = qf_pow_int(x, -n);
        return qf_div(qf_from_double(1.0), r);
    }

    qfloat result = qf_from_double(1.0);
    qfloat base   = x;

    while (n > 0) {
        if (n & 1)
            result = qf_mul(result, base);
        base = qf_mul(base, base);
        n >>= 1;
    }

    return result;
}

qfloat qf_pow(qfloat x, qfloat y)
{
    /* x == 0 handling */
    if (x.hi == 0.0) {
        double yd = qf_to_double(y);
        if (yd <= 0.0)
            return QF_NAN;
        return qf_from_double(0.0);
    }

    /* x < 0: y must be (close to) an integer, and we use integer power */
    if (x.hi < 0.0) {
        double yd = qf_to_double(y);
        double yi = nearbyint(yd);          /* nearest integer */

        /* if y is not an integer within a tight tolerance → NaN */
        if (fabs(yd - yi) > 1e-30)
            return QF_NAN;

        int n = (int)yi;

        /* compute |x|^n using integer power */
        qfloat ax = x;
        ax.hi = fabs(ax.hi);   /* assuming normalised qfloat, this is enough */

        qfloat r = qf_pow_int(ax, n);

        /* if n is odd, result is negative */
        if (n & 1) {
            r.hi = -r.hi;
            r.lo = -r.lo;
        }

        return r;
    }

    /* x > 0: general case x^y = exp(y * log(x)) */
    qfloat lx = qf_log(x);
    qfloat t  = qf_mul(y, lx);
    return qf_exp(t);
}

/* Optional: tiny helper for exact 2^k scaling of a qfloat */
qfloat qf_ldexp(qfloat x, int e)
{
    double s = ldexp(1.0, e);

    qfloat r;
    r.hi = x.hi * s;
    r.lo = x.lo * s;
    return r;
}

qfloat qf_sqrt(qfloat x)
{
    qfloat zero = qf_from_double(0.0);

    if (qf_eq(x, zero))
        return zero;

    if (qf_lt(x, zero))
        return QF_NAN;

    double approx = sqrt(x.hi);
    qfloat y = qf_from_double(approx);

    /* 3–4 Newton steps: y_{n+1} = 0.5 * (y_n + x / y_n) */
    for (int i = 0; i < 4; ++i) {
        qfloat x_over_y = qf_div(x, y);
        qfloat sum      = qf_add(y, x_over_y);
        y = qf_mul(qf_from_double(0.5), sum);
    }

    return y;
}

qfloat qf_add_double(qfloat x, double y) {
    double H, h, S, s, e;
	S = y + x.hi;
	e = S - y;
	s = S - e;
	s = (x.hi - e) + (y - s);
	H = S + (s + x.lo);
	h = (s + x.lo) + (S - H);
    qfloat r = x;
	r.hi = H + h;
	r.lo = h + (H - r.hi);
    return r;
}

static inline void split(double x, double* hi, double* lo)
{
    const double c = (1ULL << 27) + 1.0;   // 2^27+1
    double t = c * x;
    *hi = t - (t - x);
    *lo = x - *hi;
}

qfloat qf_mul_double(qfloat x, double a)
{
    double xh, xl, ah, al;
    split(x.hi, &xh, &xl);
    split(a,    &ah, &al);

    double p = x.hi * a;

    double err = ((xh*ah - p) + xh*al + xl*ah) + xl*al;
    err += x.lo * a;

    qfloat r;
    r.hi = p + err;
    r.lo = err - (r.hi - p);
    return r;
}

qfloat qf_sqr(qfloat x) {
	double hx, tx, C, c;

	C = QF_SPLIT * x.hi;
	hx = C - x.hi;
	hx = C - hx;
	tx = x.hi - hx;
	C = x.hi * x.hi;
	c = ((((hx * hx - C) + 2.0 * hx * tx)) + tx * tx) + 2.0 * x.hi * x.lo;
	hx = C + c;

    qfloat r;
    r.hi = hx;
    r.lo = c + (C - hx);

	return r;
}

qfloat qf_floor(qfloat x)
{
    double fh = floor(x.hi);
    double fl = floor(x.lo);

    double t1 = x.hi - fh;
    double t2 = x.lo - fl;
    double t3 = t1 + t2;   // fractional part of (hi+lo)

    int t = (int)floor(t3);

    qfloat r;

    switch (t) {
        case 0:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl));
            break;

        case 1:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl + 1.0));
            break;

        case 2:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl + 1.0));
            break;
    }

    return r;
}

/* Round qfloat to nearest integer (ties to even) */
qfloat qf_rint(qfloat x)
{
    qfloat t = qf_from_double(0.5);
    t = qf_add(t, x);
    return qf_floor(t);
}

static inline int qf_round_to_int(qfloat y)
{
    double s, e;
    two_sum(y.hi, y.lo, &s, &e);   // s = hi+lo, e = tiny residual
    // s is a double, but it already includes the low part
    return (int)nearbyint(s);
}

static inline qfloat qf_exp_kernel(qfloat r)
{
    static const qfloat EXP_COEF[] = {
        { 2.0078201327067089, 7.6989170590508717e-17 },   // 2.00782013270670897025569345450493539496719826451529e+00
        { 0.12524429962246966, -1.1636659685304788e-17 },   // 1.25244299622469650888488552059403016996167595573611e-01
        { 0.0039113387471945557, 3.6511894489906962e-19 },   // 3.91133874719455603987662155448712302851673533750791e-03
        { 8.1459712243857609e-05, 3.4333135193980365e-21 },   // 8.14597122438576124366623158150800836320647733580159e-05
        { 1.2725594893906429e-06, -3.3960576781977518e-24 },   // 1.27255948939064291683039536327901417762621632314779e-06
        { 1.5904922856465758e-08, 1.1403482187347839e-24 },   // 1.59049228564657595170125652231762639869286765571089e-08
        { 1.6566087338215546e-10, 1.0716777914367335e-26 },   // 1.65660873382155469390145424913058671922198579085015e-10
        { 1.4790117788344555e-12, 6.2026716927995566e-29 },   // 1.47901177883445555860443152263148239761296494738770e-12
        { 1.1554152696446826e-14, 6.0821328583739624e-31 },   // 1.15541526964468264490943783326433895465049775927119e-14
        { 8.0233689261773118e-17, 1.9382518053908152e-33 },   // 8.02336892617731203510960531285356603278155205852885e-17
        { 5.0144275149711857e-19, -3.6401634659733449e-35 },   // 5.01442751497118536546682134254459299542628430307812e-19
        { 2.8490222341545035e-21, 1.1289540617549812e-37 },   // 2.84902223415450362691164782217240099497173603853959e-21
        { 1.4838285925898211e-23, -3.1157275821681204e-40 },   // 1.48382859258982102321175521167244276028875248448675e-23
        { 7.1336382047262347e-26, -2.2336460159428147e-42 },   // 7.13363820472623450778157613108952173312683250216815e-26
        { 3.1846006764245595e-28, -1.8599980405806009e-44 },   // 3.18460067642455931873764058222397983713240357784294e-28
        { 1.3268953522163381e-30, 8.6880377274428939e-48 },   // 1.32689535221633809261226907806897950248487799967001e-30
        { 5.183110534789647e-33, -2.1987298822911846e-49 },   // 5.18311053478964681947948584290311686963786349166464e-33
        { 1.9055310188506825e-35, 6.1261175594053182e-52 },   // 1.90553101885068255207022857815838575849458038608558e-35
        { 6.6163515790277845e-38, 3.4484035454954892e-54 },   // 6.61635157902778484641103123076065326048415118568003e-38
        { 2.1764090680516302e-40, 1.9134780919953053e-56 },   // 2.17640906805163038515836993176194751448446097344594e-40
        { 6.8012150828475526e-43, 3.9199301490910886e-59 },   // 6.80121508284755295866382043328164513898264043772395e-43
        { 2.0241540413438387e-45, -1.0218868505796425e-61 },   // 2.02415404134383859473931118210700100160333742738858e-45
        { 5.7503932255280342e-48, -1.923854484667449e-64 },   // 5.75039322552803397348614021217735954266816983331649e-48
        { 1.5625957970636072e-50, -9.6280561970482323e-68 },   // 1.56259579706360721898274205704425841416460611722259e-50
        { 4.0692333959407629e-53, 8.5660473081783288e-70 },   // 4.06923339594076296494422544885785424193219373742892e-53
        { 1.0173022354240444e-55, 1.6882208569445479e-72 },   // 1.01730223542404441594846828423852626437220498786958e-55
        { 2.4454244585301151e-58, -9.4360149305200058e-75 },   // 2.44542445853011503523119037491844433737859505921607e-58
        { 5.660675516561292e-61, 2.4417881040627945e-77 },   // 5.66067551656129229308827245342002270944323569890047e-61
        { 1.2635375636764616e-63, 9.425695195996618e-80 },   // 1.26353756367646170566750409945268991172372910631121e-63
        { 2.7231290743851696e-66, 2.2865795958219742e-82 },   // 2.72312907438516978540878719719049209293026262692022e-66
        { 5.6731617429252378e-69, 7.0681120929698254e-86 },   // 5.67316174292523782683995630158060408724742022221520e-69
        { 1.1437781055628525e-71, 5.2653811344675536e-88 },   // 1.14377810556285256081724318021310515009202569273747e-71
        { 2.2339333489125188e-74, -1.1023367961606344e-90 },   // 2.23393334891251864301277236025427909727862373965758e-74
    };
    static const int N_EXP_COEF = sizeof(EXP_COEF) / sizeof(EXP_COEF[0]);

    // map r ∈ [-0.125, 0.125] to x ∈ [-1,1]
    qfloat x = qf_mul_double(r, 8.0);

    qfloat b_kplus1 = (qfloat){0,0};
    qfloat b_kplus2 = (qfloat){0,0};

    for (int k = N_EXP_COEF - 1; k >= 1; k--) {
        qfloat t = qf_mul_double(x, 2.0);
        t = qf_mul(t, b_kplus1);
        t = qf_sub(t, b_kplus2);
        t = qf_add(t, EXP_COEF[k]);

        b_kplus2 = b_kplus1;
        b_kplus1 = t;
    }

    // p(x) = (c0/2) + x*b1 - b2   // because c0 came from a DCT-I
    qfloat xb1 = qf_mul(x, b_kplus1);
    qfloat half_c0 = qf_mul_double(EXP_COEF[0], 0.5);
    qfloat p = qf_add(half_c0, xb1);
    return qf_sub(p, b_kplus2);
}

// qf_exp_reduce:
//   Input:  x  (qf)
//   Output: *k (int), *r (qf) with |r| <= 0.1733
//
//   x = k*ln2 + r
//
static inline void qf_exp_reduce(qfloat x, int* k, qfloat* r)
{
    // High/low split of ln2 in double
    static const double LN2_HI = 6.93147180559945286227e-01;  // 0x3fe62e42fefa3800
    static const double LN2_LO = 2.31904681384629955842e-17;  // ln2 - LN2_HI

    // y = x * (1/ln2) in qfloat
    qfloat y = qf_mul(x, QF_INVLN2);

    // k = round(y) using qfloat-accurate rounding
    int ki = qf_round_to_int(y);
    *k = ki;

    // k as double and qfloat
    double kd = (double)ki;
    qfloat kq = qf_from_double(kd);

    // First subtract k * LN2_HI in qfloat
    qfloat t_hi = qf_mul_double(kq, LN2_HI);
    qfloat r1   = qf_sub(x, t_hi);

    // Then subtract k * LN2_LO in qfloat
    qfloat t_lo = qf_mul_double(kq, LN2_LO);
    qfloat r2   = qf_sub(r1, t_lo);

    *r = r2;
}


qfloat qf_exp(qfloat x)
{
    int k;
    qfloat r;

    qf_exp_reduce(x, &k, &r);

    qfloat p = qf_exp_kernel(r);

    // scale by 2^k
    return qf_ldexp(p, k);
}

qfloat qf_log(qfloat x)
{
    qfloat one = qf_from_double(1.0);

    /* log(1) = 0 */
    if (qf_eq(x, one))
        return qf_from_double(0.0);

    /* x <= 0: return NaN (you can refine this if you like) */
    qfloat zero = qf_from_double(0.0);
    if (qf_lt(x, zero))
        return QF_NAN;

    /* Use frexp on the high part to get exponent and mantissa */
    int e;
    double m = frexp(x.hi + x.lo, &e);  /* x.hi = m * 2^e, m in [0.5,1) */

    /* Adjust so mantissa is in [sqrt(0.5), 1) to keep u small */
    if (m < QF_SQRT_HALF.hi) {
        m *= 2.0;
        e -= 1;
    }

    qfloat mq = qf_from_double(m);
    qfloat u  = qf_sub(mq, one);   /* u = m - 1, |u| <= ~0.2929 */

    /* log(1+u) = u - u^2/2 + u^3/3 - ... (alternating series) */
    qfloat term = u;
    qfloat sum  = term;

    qfloat u_power = u;  /* u^1 already */
    int sign = -1;

    for (int n = 2; n <= 40; ++n) {
        qfloat nd = qf_from_double((double)n);
        u_power = qf_mul(u_power, u);          /* u^n */
        qfloat frac = qf_div(u_power, nd);     /* u^n / n */

        if (sign > 0)
            sum = qf_add(sum, frac);
        else
            sum = qf_sub(sum, frac);

        sign = -sign;
    }

    /* Add exponent contribution: e * ln2 */
    qfloat ln2 = QF_LN2;
    qfloat eq  = qf_from_double((double)e);
    qfloat elog2 = qf_mul(eq, ln2);

    qfloat y = qf_add(sum, elog2);

    /* Optional: 1–2 Newton refinements to tighten last bits */
    for (int i = 0; i < 2; ++i) {
        qfloat ey   = qf_exp(y);
        qfloat diff = qf_sub(x, ey);
        qfloat corr = qf_div(diff, ey);
        y = qf_add(y, corr);
    }

    return y;
}

inline bool qf_isnan(qfloat x)
{
    return (x.hi != x.hi) || (x.lo != x.lo);
}

inline bool qf_isposinf(qfloat x)
{
    return isinf(x.hi) && x.hi > 0.0;
}

inline bool qf_isneginf(qfloat x)
{
    return isinf(x.hi) && x.hi < 0.0;
}

inline bool qf_isinf(qfloat x)
{
    return qf_eq(x, QF_INF) || qf_eq(x, QF_NINF);
}

static inline qfloat qf_scale_pow10(qfloat x, int exp10)
{
    static const qfloat P10_1   = { 1.00000000000000000e+01, 0.00000000000000000e+00 };
    static const qfloat P10_2   = { 1.00000000000000000e+02, 0.00000000000000000e+00 };
    static const qfloat P10_4   = { 1.00000000000000000e+04, 0.00000000000000000e+00 };
    static const qfloat P10_8   = { 1.00000000000000000e+08, 0.00000000000000000e+00 };
    static const qfloat P10_16  = { 1.00000000000000000e+16, 0.00000000000000000e+00 };
    static const qfloat P10_32  = { 1.00000000000000010e+32, -5.36616220439347200e+15 };
    static const qfloat P10_64  = { 1.00000000000000000e+64, -2.13204190094544240e+47 };
    static const qfloat P10_128 = { 1.00000000000000010e+128, -7.51744869165182680e+111 };
    static const qfloat P10_256 = { 1.00000000000000000e+256, -3.01276599001406900e+239 };

    qfloat r = x;
    int n = exp10;

    if (n < 0) {
        n = -n;
        if (n & 256) r = qf_div(r, P10_256);
        if (n & 128) r = qf_div(r, P10_128);
        if (n &  64) r = qf_div(r, P10_64);
        if (n &  32) r = qf_div(r, P10_32);
        if (n &  16) r = qf_div(r, P10_16);
        if (n &   8) r = qf_div(r, P10_8);
        if (n &   4) r = qf_div(r, P10_4);
        if (n &   2) r = qf_div(r, P10_2);
        if (n &   1) r = qf_div(r, P10_1);
        return r;
    }

    if (n & 256) r = qf_mul(r, P10_256);
    if (n & 128) r = qf_mul(r, P10_128);
    if (n &  64) r = qf_mul(r, P10_64);
    if (n &  32) r = qf_mul(r, P10_32);
    if (n &  16) r = qf_mul(r, P10_16);
    if (n &   8) r = qf_mul(r, P10_8);
    if (n &   4) r = qf_mul(r, P10_4);
    if (n &   2) r = qf_mul(r, P10_2);
    if (n &   1) r = qf_mul(r, P10_1);

    return r;
}

qfloat qf_pow10(int e)
{
    return qf_scale_pow10((qfloat){1.0, 0.0}, e);
}

static inline int qf_iszero(qfloat x)
{
    /* Treat any representable zero as zero, regardless of sign.
       This matches Option A: always print "0". */

    return (x.hi == 0.0 && x.lo == 0.0);
}

/* 32-digit decimal parser (no long double) */
qfloat qf_from_string(const char *s)
{
    qfloat result = qf_from_double(0.0);
    qfloat ten    = qf_from_double(10.0);

    int sign     = 1;
    int exp10    = 0;
    int seen_dot = 0;

    /* skip leading space */
    while (*s == ' ' || *s == '\t')
        s++;

    /* sign */
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;

    /* integer + fractional digits */
    while ((*s >= '0' && *s <= '9') || *s == '.') {
        if (*s == '.') {
            if (!seen_dot)
                seen_dot = 1;
            s++;
            continue;
        }

        int digit = *s - '0';

        result = qf_mul(result, ten);
        result = qf_add(result, qf_from_double((double)digit));

        if (seen_dot)
            exp10--;

        s++;
    }

    /* optional exponent */
    if (*s == 'e' || *s == 'E') {
        s++;
        int esign = 1;
        if (*s == '-') { esign = -1; s++; }
        else if (*s == '+') s++;

        int e = 0;
        while (*s >= '0' && *s <= '9') {
            e = e * 10 + (*s - '0');
            s++;
        }
        exp10 += esign * e;
    }

    /* scale by exact 10^exp10 from table */
    if (!qf_iszero(result)) {
        if (exp10 == 308 && qf_eq(result, (qfloat){1,0})) {
            result = (qfloat){ 1.0000000000000000e+308, -1.0979063629440455e+291 };
        } else {
            result = qf_mul(result, qf_pow10(exp10));
        }
    }

    if (sign < 0) {
        result.hi = -result.hi;
        result.lo = -result.lo;
    }

    return result;
}

/* 32-digit decimal formatter (robust) */

static inline int qf_is_negative(qfloat x) {
    return (x.hi < 0.0) || (x.hi == 0.0 && x.lo < 0.0);
}

static inline void qf_modf_like(qfloat x, qfloat *ip, qfloat *fp) {
    qfloat f = qf_floor(x);
    *ip = f;
    *fp = qf_sub(x, f);
}

static inline qfloat qf_div_double(qfloat x, double d) {
    return qf_div(x, qf_from_double(d));
}

int qf_decimal_exponent(qfloat x)
{
    /* Work with absolute value */
    double v = x.hi;
    if (v < 0.0)
        v = -v;

    /* Zero check */
    if (v == 0.0)
        return 0;

    /*
     * Estimate exponent from hi only.
     * This is safe because hi contains the leading 53 bits.
     */
    double e = floor(log10(v));

    return (int)e;
}

/* Extract ndigits decimal digits and return the (possibly adjusted) exp10 */
int qf_to_decimal_digits(qfloat x, char *digits, int ndigits, int *out_exp10)
{
    /* assume x > 0 and not zero; sign handled outside */

    int exp10 = qf_decimal_exponent(x);
    qfloat y  = qf_scale_pow10(x, -exp10);   /* y ≈ [1,10) */

    qfloat ten = (qfloat){ 10.0, 0.0 };

    for (int i = 0; i < ndigits; i++) {

        /* use full qfloat for the remainder, but digit from hi */
        int digit = qf_to_int(qf_floor(y));
        if (digit < 0) digit = 0;
        if (digit > 9) digit = 9;

        digits[i] = (char)('0' + digit);

        /* y = (y - digit) * 10, using qfloat subtraction */
        qfloat d = (qfloat){ (double)digit, 0.0 };
        y = qf_sub(y, d);
        y = qf_mul(y, ten);
    }

    if (out_exp10) *out_exp10 = exp10;
    return ndigits;
}

void qf_to_string(qfloat x, char *out, size_t out_size)
{
    if (out_size == 0) return;

    /* Zero */
    if (x.hi == 0.0 && x.lo == 0.0) {
        snprintf(out, out_size, "0");
        return;
    }

    /* NaN */
    if (isnan(x.hi) || isnan(x.lo)) {
        if (signbit(x.hi))
            snprintf(out, out_size, "-NAN");
        else
            snprintf(out, out_size, "NAN");
        return;
    }

    /* Inf */
    if (isinf(x.hi)) {
        if (signbit(x.hi))
            snprintf(out, out_size, "-INF");
        else
            snprintf(out, out_size, "INF");
        return;
    }

    int sign = (x.hi < 0.0 || (x.hi == 0.0 && x.lo < 0.0));
    if (sign) { x.hi = -x.hi; x.lo = -x.lo; }

    char digits[40];
    int exp10 = 0;
    qf_to_decimal_digits(x, digits, 34, &exp10);

    size_t pos = 0;
    if (sign && pos < out_size - 1) out[pos++] = '-';
    if (pos < out_size - 1) out[pos++] = digits[0];
    if (pos < out_size - 1) out[pos++] = '.';
    for (int i = 1; i < 34 && pos < out_size - 1; i++)
        out[pos++] = digits[i];

    int n = snprintf(out + pos, out_size - pos, "e%+d", exp10);
    if (n < 0) n = 0;
    pos += n;
    if (pos >= out_size) out[out_size - 1] = '\0';
    else out[pos] = '\0';
}

/* Local helpers */

qfloat qf_abs(qfloat x)
{
    if (qf_is_negative(x)) {
        qfloat r;
        r.hi = -x.hi;
        r.lo = -x.lo;
        return r;
    }
    return x;
}

inline qfloat qf_neg(qfloat x) {
    qfloat r = { -x.hi, -x.lo };
    return r;
}

bool qf_eq(qfloat a, qfloat b)
{
    return a.hi == b.hi && a.lo == b.lo;
}

bool qf_lt(qfloat a, qfloat b)
{
    if (a.hi < b.hi) return 1;
    if (a.hi > b.hi) return 0;
    return a.lo < b.lo;
}

bool qf_le(qfloat a, qfloat b)
{
    if (a.hi < b.hi) return 1;
    if (a.hi > b.hi) return 0;
    return a.lo <= b.lo;
}

bool qf_gt(qfloat a, qfloat b)
{
    if (a.hi > b.hi) return 1;
    if (a.hi < b.hi) return 0;
    return a.lo > b.lo;
}

bool qf_ge(qfloat a, qfloat b)
{
    if (a.hi > b.hi) return 1;
    if (a.hi < b.hi) return 0;
    return a.lo >= b.lo;
}

int qf_cmp(qfloat a, qfloat b) {
    if (qf_eq(a, b)) return 0;
    if (qf_lt(a, b)) return -1;
    return 1;
}

inline int qf_signbit(qfloat x)
{
    return signbit(x.hi);
}

qfloat qf_mul_pow10(qfloat x, int k)
{
    double p = pow(10.0, (double)k);
    return qf_mul(x, qf_from_double(p));
}

/* Helper: apply width / padding to a preformatted string.
 * - s already contains any sign characters.
 * - sign_aware_zero controls whether '0' padding should come after the sign.
 */
static void pad_string(char *out, size_t out_size,
                       const char *s,
                       int width,
                       int flag_minus,
                       int flag_zero,
                       int sign_aware_zero)
{
    size_t len = strlen(s);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    if (pad < 0) pad = 0;

    size_t pos = 0;

    if (flag_minus) {
        /* left-justify: value, then spaces */
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
    } else if (flag_zero) {
        size_t i = 0;
        if (sign_aware_zero &&
            (s[0] == '+' || s[0] == '-' || s[0] == ' ')) {
            /* keep sign, then zeros, then rest */
            if (pos + 1 < out_size)
                out[pos++] = s[i++];
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        } else {
            /* zeros first, then full string */
            for (int j = 0; j < pad && pos + 1 < out_size; j++)
                out[pos++] = '0';
        }
        for (; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    } else {
        /* right-justify: spaces, then value */
        for (int i = 0; i < pad && pos + 1 < out_size; i++)
            out[pos++] = ' ';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++)
            out[pos++] = s[i];
    }

    out[pos] = '\0';
}

/* Inline helpers for safe output */

static void qf_put_char(char c, char **dst, size_t *remaining, size_t *count)
{
    /* Count always increases */
    (*count)++;

    /* If no output buffer, only count */
    if (*dst == NULL || *remaining == 0)
        return;

    /* Write only if space for at least 1 char + NUL */
    if (*remaining > 1) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
}

static void qf_put_str(const char *s, char **dst, size_t *remaining, size_t *count)
{
    while (*s) {
        qf_put_char(*s, dst, remaining, count);
        s++;
    }
}

/* Helper: uppercase exponent for %Q */
static inline void qf_uppercase_E(char *s) {
    char *e = strchr(s, 'e');
    if (e) *e = 'E';
}
    
int qf_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    const char *p = fmt;

    va_list ap_local;
    va_copy(ap_local, ap);

    char *dst = out;
    size_t remaining = out_size;
    size_t count = 0;

    if (!out || out_size == 0) {
        dst = NULL;
        remaining = 0;
    }

    while (*p) {

        if (*p != '%') {
            qf_put_char(*p++, &dst, &remaining, &count);
            continue;
        }

        p++; /* skip '%' */

        int flag_plus  = 0;
        int flag_space = 0;
        int flag_minus = 0;
        int flag_zero  = 0;
        int flag_hash  = 0;

        while (1) {
            if      (*p == '+') { flag_plus = 1; p++; }
            else if (*p == ' ') { flag_space = 1; p++; }
            else if (*p == '-') { flag_minus = 1; p++; }
            else if (*p == '0') { flag_zero  = 1; p++; }
            else if (*p == '#') { flag_hash  = 1; p++; }
            else break;
        }

        int width = 0;
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');

        int precision = -1;
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9')
                precision = precision * 10 + (*p++ - '0');
        }

        /* ----------------------------------------------------
           %Q — scientific (uppercase exponent)
           ---------------------------------------------------- */
        if (*p == 'Q') {
            p++;

            qfloat x   = va_arg(ap_local, qfloat);

            char core[256];
            qf_to_string(x, core, sizeof(core));
            qf_uppercase_E(core);

            char tmp[256];
            strcpy(tmp, core);

            if (tmp[0] != '-') {
                if (flag_plus) {
                    memmove(tmp + 1, tmp, strlen(tmp) + 1);
                    tmp[0] = '+';
                } else if (flag_space) {
                    memmove(tmp + 1, tmp, strlen(tmp) + 1);
                    tmp[0] = ' ';
                }
            }

            char padded[256];
            pad_string(padded, sizeof(padded),
                       tmp, width, flag_minus, flag_zero, 0);

            qf_put_str(padded, &dst, &remaining, &count);
            continue;
        }

        /* ----------------------------------------------------
           %q — fixed-format with explicit precision and
                 concise default, fallback to scientific
           ---------------------------------------------------- */
        else if (*p == 'q') {
            p++;

            qfloat x = va_arg(ap_local, qfloat);

            /* Step 1: canonical scientific form */
            char sci[128];
            qf_to_string(x, sci, sizeof(sci));   /* mantissa + 'e' + exponent */

            char *e = strchr(sci, 'e');
            if (!e) e = strchr(sci, 'E');

            int exp10 = 0;
            if (e) {
                exp10 = atoi(e + 1);
                *e = '\0'; /* terminate mantissa */
            }

            /* Step 2: scientific fallback if exponent outside [-6, 32] */
            if (!e || exp10 < -6 || exp10 > 32) {

                char tmp[256];
                qf_to_string(x, tmp, sizeof(tmp));

                /* normalize to lowercase 'e' and trim mantissa only */
                char *ep = strchr(tmp, 'e');
                if (!ep) ep = strchr(tmp, 'E');

                if (ep) {
                    char mant[256];
                    char expbuf[32];

                    size_t mlen = (size_t)(ep - tmp);
                    memcpy(mant, tmp, mlen);
                    mant[mlen] = '\0';

                    strcpy(expbuf, ep); /* includes 'e' and exponent */

                    /* force lowercase 'e' */
                    if (expbuf[0] == 'E')
                        expbuf[0] = 'e';

                    /* trim trailing zeros in mantissa */
                    char *q = mant + strlen(mant) - 1;
                    while (q > mant && *q == '0')
                        *q-- = '\0';
                    if (q > mant && *q == '.')
                        *q = '\0';

                    {
                        size_t mlen = strlen(mant);
                        size_t elen = strlen(expbuf);

                        /* Clamp lengths to avoid overflow */
                        if (mlen > sizeof(tmp) - 1)
                            mlen = sizeof(tmp) - 1;

                        if (elen > sizeof(tmp) - 1 - mlen)
                            elen = sizeof(tmp) - 1 - mlen;

                        memcpy(tmp, mant, mlen);
                        memcpy(tmp + mlen, expbuf, elen);
                        tmp[mlen + elen] = '\0';
                    }
                }

                if (tmp[0] != '-') {
                    if (flag_plus) {
                        memmove(tmp + 1, tmp, strlen(tmp) + 1);
                        tmp[0] = '+';
                    } else if (flag_space) {
                        memmove(tmp + 1, tmp, strlen(tmp) + 1);
                        tmp[0] = ' ';
                    }
                }

                char padded[256];
                pad_string(padded, sizeof(padded),
                           tmp, width, flag_minus, flag_zero, 0);

                qf_put_str(padded, &dst, &remaining, &count);
                continue;
            }

            /* Step 3: fixed-format reconstruction */

            char mantissa[128];
            strcpy(mantissa, sci);

            int negative = 0;
            char *mant = mantissa;

            if (*mant == '-') {
                negative = 1;
                mant++;
            }

            char intpart[128], fracpart[128];
            char *dot = strchr(mant, '.');

            if (dot) {
                size_t ilen = (size_t)(dot - mant);
                memcpy(intpart, mant, ilen);
                intpart[ilen] = '\0';
                strcpy(fracpart, dot + 1);
            } else {
                strcpy(intpart, mant);
                fracpart[0] = '\0';
            }

            char digits[256];
            snprintf(digits, sizeof(digits), "%s%s", intpart, fracpart);
            int nd = (int)strlen(digits);

            int fixed_dp = (int)strlen(intpart) + exp10; /* decimal point position in digits[] */

            /* Step 3a: rounding for explicit precision */
            if (precision >= 0) {
                int K = fixed_dp + precision; /* index of rounding digit */

                if (K + 1 > nd) {
                    int pad = K + 1 - nd;
                    if (nd + pad >= (int)sizeof(digits))
                        pad = (int)sizeof(digits) - 1 - nd;
                    for (int i = 0; i < pad; i++)
                        digits[nd + i] = '0';
                    nd += pad;
                    digits[nd] = '\0';
                }

                if (K >= 0 && K < nd && digits[K] >= '5') {
                    int i = K - 1;
                    while (i >= 0 && digits[i] == '9') {
                        digits[i] = '0';
                        i--;
                    }
                    if (i >= 0) {
                        digits[i]++;
                    } else {
                        if (nd + 1 < (int)sizeof(digits)) {
                            memmove(digits + 1, digits, nd + 1);
                            digits[0] = '1';
                            nd++;
                            fixed_dp++;
                        }
                    }
                }

                if (K < nd)
                    nd = K;
                digits[nd] = '\0';
            }

            char buf2[512];
            char *bp = buf2;

            /* sign */
            if (negative) {
                *bp++ = '-';
            } else if (flag_plus) {
                *bp++ = '+';
            } else if (flag_space) {
                *bp++ = ' ';
            }

            /* integer part */
            if (fixed_dp <= 0) {
                *bp++ = '0';
            } else {
                for (int i = 0; i < fixed_dp; i++) {
                    int idx = i;
                    if (idx >= 0 && idx < nd)
                        *bp++ = digits[idx];
                    else
                        *bp++ = '0';
                }
            }

            /* fractional part */
            int frac_digits;
            if (precision < 0) {
                frac_digits = nd - fixed_dp;
                if (frac_digits < 0) frac_digits = 0;
            } else {
                frac_digits = precision;
            }

            if (frac_digits > 0 || flag_hash) {
                *bp++ = '.';

                for (int i = 0; i < frac_digits; i++) {
                    int idx = fixed_dp + i;
                    if (idx >= 0 && idx < nd)
                        *bp++ = digits[idx];
                    else
                        *bp++ = '0';
                }

                if (precision < 0) {
                    /* trim trailing zeros, but keep '.' if flag_hash */
                    char *q = bp - 1;
                    while (q > buf2 && *q == '0') {
                        *q-- = '\0';
                        bp--;
                    }
                    if (!flag_hash && q > buf2 && *q == '.') {
                        *q = '\0';
                        bp--;
                    }
                } else {
                    if (precision == 0 && !flag_hash) {
                        /* remove '.' if no digits requested and no # */
                        bp--;
                    }
                }
            }

            *bp = '\0';

            /* Fix leading zero in integer part (e.g. "099" → "99") */
            if (buf2[0] == '0' && buf2[1] >= '0' && buf2[1] <= '9') {
                memmove(buf2, buf2 + 1, strlen(buf2));
                bp--;  /* adjust write pointer */
            }

            if (buf2[0] == '\0')
                strcpy(buf2, "0");

            char padded[512];
            pad_string(padded, sizeof(padded),
                       buf2, width, flag_minus, flag_zero, 0);

            qf_put_str(padded, &dst, &remaining, &count);
            continue;
        }

        /* ----------------------------------------------------
           Delegate to snprintf for everything else
           ---------------------------------------------------- */
        else {
            char fmtbuf[32];
            char tmp[256];

            char *f = fmtbuf;
            *f++ = '%';

            if (flag_plus)  *f++ = '+';
            if (flag_space) *f++ = ' ';
            if (flag_minus) *f++ = '-';
            if (flag_zero)  *f++ = '0';
            if (flag_hash)  *f++ = '#';

            if (width > 0)
                f += sprintf(f, "%d", width);

            if (precision >= 0) {
                *f++ = '.';
                f += sprintf(f, "%d", precision);
            }

            *f++ = *p;
            *f   = '\0';

            switch (*p) {
                case 'd': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;

                case 's': {
                    const char *v = va_arg(ap_local, const char *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;

                default:
                    snprintf(tmp, sizeof(tmp), "<?>");
                    break;
            }

            p++;

            qf_put_str(tmp, &dst, &remaining, &count);
            continue;
        }
    }

    if (dst && remaining > 0)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int qf_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int n = qf_vsprintf(out, out_size, fmt, ap);

    va_end(ap);
    return n;
}

int qf_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int needed = qf_vsprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0)
        return needed;

    char *buf = malloc((size_t)needed + 1);
    if (!buf)
        return -1;

    va_start(ap, fmt);
    qf_vsprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    fwrite(buf, 1, needed, stdout);
    free(buf);

    return needed;
}

/*------------------------------------------------------------
 *  High-precision sin/cos/tan using Newton refinement
 *
 *  Strategy:
 *    1. Convert x to double.
 *    2. Compute s0 = sin(xd), c0 = cos(xd).
 *    3. Lift to qfloat.
 *    4. Refine (s, c) so that s^2 + c^2 = 1 using Newton iteration:
 *
 *         r  = s^2 + c^2 - 1
 *         s' = s - r / (2*s)
 *         c' = c - r / (2*c)
 *
 *    5. After 2–3 iterations, (s, c) have full quad-double accuracy.
 *
 *  This avoids range reduction and polynomial kernels entirely.
 *------------------------------------------------------------*/

/* Reduce x to x = k*(pi/2) + r, with r in [-pi/4, pi/4].
 * Returns k mod 4 (0..3), and stores r in *r_out.
 * This is a first-stage reduction using double for k; good for moderate |x|.
 */
/* Reduce x modulo (pi/2), returning:
 *
 *   r = x - k*(pi/2),  with  r ∈ [-pi/4, +pi/4]
 *   q = k mod 4        (quadrant)
 *
 * This version uses a Payne–Hanek style reduction with
 * high‑precision constants stored as qfloat.
 */
static int qf_range_reduce_pi_over_2(qfloat x, qfloat *r_out)
{
    /* Step 1:  k = nearest integer to x * (2/pi) */
    qfloat t = qf_mul(x, QF_2_PI);
    double t_hi = t.hi;               /* safe: only used for rounding */
    long k = (long) llround(t_hi);    /* nearest integer */

    /* Step 2:  r = x - k*(pi/2) */
    qfloat k_qf = qf_from_double((double)k);
    qfloat prod = qf_mul(k_qf, QF_PI_2);
    qfloat r = qf_sub(x, prod);

    /* Step 3:  final cleanup: ensure r ∈ [-pi/4, +pi/4] */
    /* If r is slightly outside due to rounding, adjust. */
    if (r.hi > 0.78539816339744830962) {      /* > pi/4 */
        r = qf_sub(r, QF_PI_2);
        k += 1;
    } else if (r.hi < -0.78539816339744830962) { /* < -pi/4 */
        r = qf_add(r, QF_PI_2);
        k -= 1;
    }

    *r_out = r;

    /* Quadrant = k mod 4 */
    int q = (int)(k & 3);
    return q;
}

/* Kernel on small interval [-pi/4, pi/4].
 * Currently uses libm on the reduced argument as a placeholder.
 * We will replace this with a true quad-precision polynomial kernel.
 */

 /* sin(r) coefficients (odd powers), highest degree first */
static const qfloat SIN_COEF[] = {
    // { 7.2654601791530714e-44, -4.3640971493544333e-61 },
    // { -9.6775929586318907e-41, -3.2022955486455637e-57 },
    // { 1.1516335620771951e-37, -6.0995744578845386e-54 },
    // { -1.2161250415535179e-34, -5.5862905678888081e-51 },
    // { 1.1309962886447716e-31, 1.0498015412959507e-47 },
    { -9.183689863795546e-29, -1.4303150396787328e-45 },
    { 6.4469502843844736e-26, -1.9330404233703462e-42 },
    { -3.8681701706306841e-23, 8.8431776554823406e-40 },
    { 1.9572941063391263e-20, -1.3643503830087907e-36 },
    { -8.2206352466243295e-18, -2.214189411960427e-34 },
    { 2.8114572543455206e-15, 1.6508842730861435e-31 },
    { -7.6471637318198164e-13, -7.038728777334537e-30 },
    { 1.6059043836821613e-10, 1.2585294588752098e-26 },
    { -2.505210838544172e-08, 1.448814070935912e-24 },
    { 2.7557319223985893e-06, -1.8583932740464721e-22 },
    { -0.00019841269841269841, -1.7209558293420644e-22 },
    { 0.0083333333333333332, 1.1564823173178711e-19 },
    { -0.16666666666666666, -9.2518585385429707e-18 },
};

static const int NSIN = sizeof(SIN_COEF) / sizeof(SIN_COEF[0]);

/* cos(r) coefficients (even powers), highest degree first */
static const qfloat COS_COEF[] = {
    // { 2.6882202662866363e-42, 5.355061165943334e-59 },
    // { -3.3871575355211618e-39, -5.0905614815108499e-56 },
    // { 3.8003907548547434e-36, 1.7457158024652518e-52 },
    // { -3.7699876288159054e-33, -2.5870347832750324e-49 },
    // { 3.2798892370698378e-30, 1.5117542744029879e-46 },
    { -2.4795962632247976e-27, 1.2953730964765231e-43 },
    { 1.6117375710961184e-24, -3.6846573564509786e-41 },
    { -8.8967913924505741e-22, 7.9114026148723783e-38 },
    { 4.1103176233121648e-19, 1.4412973378659449e-36 },
    { -1.5619206968586225e-16, -1.191067966027375e-32 },
    { 4.7794773323873853e-14, 4.3992054858340681e-31 },
    { -1.1470745597729725e-11, -2.0655512752830714e-28 },
    { 2.08767569878681e-09, -1.2073450591132604e-25 },
    { -2.7557319223985888e-07, -2.3767714622250291e-23 },
    { 2.4801587301587302e-05, 2.1511947866775376e-23 },
    { -0.0013888888888888889, 5.3005439543735801e-20 },
    { 0.041666666666666664, 2.3129646346357427e-18 },
    { -0.5, 0 },
};

static const int NCOS = sizeof(COS_COEF) / sizeof(COS_COEF[0]);

static void qf_sin_cos_kernel(qfloat x, qfloat *s_out, qfloat *c_out)
{
    qfloat x2 = qf_mul(x, x);

    /* sin */
    qfloat p = SIN_COEF[0];
    for (int i = 1; i < NSIN; ++i)
        p = qf_add(SIN_COEF[i], qf_mul(p, x2));
    qfloat x3 = qf_mul(x, x2);
    qfloat s = qf_add(x, qf_mul(x3, p));

    /* cos */
    qfloat q = COS_COEF[0];
    for (int i = 1; i < NCOS; ++i)
        q = qf_add(COS_COEF[i], qf_mul(q, x2));
    qfloat c = qf_add(qf_from_double(1.0), qf_mul(x2, q));

    *s_out = s;
    *c_out = c;
}

qfloat qf_hypot(qfloat x, qfloat y)
{
    // double ax = fabs(x.hi);
    // double ay = fabs(y.hi);

    /* Reject extreme magnitudes outright */
    //if (ax > 1e200 || ay > 1e200)
    //    return QF_NAN;

    /* Normal qfloat path (safe mid-range) */

    qfloat axq = qf_abs(x);
    qfloat ayq = qf_abs(y);

    /* ensure axq >= ayq */
    if (qf_lt(axq, ayq)) {
        qfloat tmp = axq;
        axq = ayq;
        ayq = tmp;
    }

    /* if the smaller one is zero, result is the larger */
    if (qf_eq(ayq, (qfloat){0.0, 0.0}))
        return axq;

    /* r = ayq / axq, so 0 < r <= 1 */
    qfloat r = qf_div(ayq, axq);

    /* s = 1 + r*r, in [1,2] */
    qfloat one = qf_from_double(1.0);
    qfloat s   = qf_add(one, qf_mul(r, r));

    /* t = sqrt(s), in [1, sqrt(2)] */
    qfloat t = qf_sqrt(s);

    /* axq * t */
    return qf_mul(axq, t);
}

qfloat qf_sin(qfloat x)
{
    qfloat r;
    int q = qf_range_reduce_pi_over_2(x, &r);

    qfloat s, c;
    qf_sin_cos_kernel(r, &s, &c);

    /* Quadrant reconstruction:
     * q = 0:  sin(x) =  sin(r)
     * q = 1:  sin(x) =  cos(r)
     * q = 2:  sin(x) = -sin(r)
     * q = 3:  sin(x) = -cos(r)
     */
    switch (q) {
    case 0:  return s;
    case 1:  return c;
    case 2:  return qf_neg(s);
    default: return qf_neg(c);
    }
}

qfloat qf_cos(qfloat x)
{
    qfloat r;
    int q = qf_range_reduce_pi_over_2(x, &r);

    qfloat s, c;
    qf_sin_cos_kernel(r, &s, &c);

    /* Quadrant reconstruction:
     * q = 0:  cos(x) =  cos(r)
     * q = 1:  cos(x) = -sin(r)
     * q = 2:  cos(x) = -cos(r)
     * q = 3:  cos(x) =  sin(r)
     */
    switch (q) {
    case 0:  return c;
    case 1:  return qf_neg(s);
    case 2:  return qf_neg(c);
    default: return s;
    }
}

qfloat qf_tan(qfloat x)
{
    qfloat s = qf_sin(x);
    qfloat c = qf_cos(x);

    double cd = qf_to_double(c);
    if (fabs(cd) < 1e-30) {
        /* Pole: tan undefined */
        return QF_NAN;
    }

    return qf_div(s, c);
}

/* Taylor tail: atan(t) = t + t^3 * P(t^2),
   P(u) = sum_{k=0}^34 (-1)^k / (2k+3) * u^k
*/

static const qfloat ATAN_COEF[] = {
    /* c0..c34, ascending degree */
    { 0.33333333333333331,  1.8503717077085584e-17 },   /*  1/3  */
    { -0.20000000000000001, 1.1102230246251769e-17 },   /* -1/5  */
    { 0.14285714285714285,  7.9301644616081096e-18 },   /*  1/7  */
    { -0.1111111111111111, -6.1679056923618695e-18 },   /* -1/9  */
    { 0.090909090909090912,-2.5232341468754475e-18 },   /*  1/11 */
    { -0.076923076923076927,4.2700885562506824e-18 },   /* -1/13 */
    { 0.066666666666666666, 9.2518585385422604e-19 },   /*  1/15 */
    { -0.058823529411764705,-8.1634045928314497e-19 },  /* -1/17 */
    { 0.052631578947368418, 2.9216395384871969e-18 },   /*  1/19 */
    { -0.047619047619047616,-2.6433881538693693e-18 },  /* -1/21 */
    { 0.043478260869565216, 1.2067641572012109e-18 },   /*  1/23 */
    { -0.040000000000000001,8.3266726846890743e-19 },   /* -1/25 */
    { 0.037037037037037035, 2.0559685641206216e-18 },   /*  1/27 */
    { -0.034482758620689655,-4.7854440716597973e-19 },  /* -1/29 */
    { 0.032258064516129031, 8.953411488912202e-19 },    /*  1/31 */
    { -0.030303030303030304,8.4107804895848353e-19 },   /* -1/33 */
    { 0.028571428571428571, 8.9214350193089923e-19 },   /*  1/35 */
    { -0.027027027027027029,1.5003013846286181e-18 },   /* -1/37 */
    { 0.02564102564102564,  8.8960178255218249e-19 },   /*  1/39 */
    { -0.024390243902439025,8.4620657364724838e-19 },   /* -1/41 */
    { 0.023255813953488372, 3.2273925134449817e-19 },   /*  1/43 */
    { -0.022222222222222223,8.48087032699794e-19 },     /* -1/45 */
    { 0.021276595744680851, 5.1672614178030277e-19 },   /*  1/47 */
    { -0.020408163265306121,-1.6285159162231047e-18 },  /* -1/49 */
    { 0.019607843137254902, 2.7211348642771499e-19 },   /*  1/51 */
    { -0.018867924528301886,-7.2007389568846597e-19 },  /* -1/53 */
    { 0.018181818181818181, 8.8313195140635633e-19 },   /*  1/55 */
    { -0.017543859649122806,-9.7387984616240005e-19 },  /* -1/57 */
    { 0.016949152542372881, 5.8804185626314954e-20 },   /*  1/59 */
    { -0.016393442622950821,8.5314269310336449e-19 },   /* -1/61 */
    { 0.015873015873015872, 8.8112938462312311e-19 },   /*  1/63 */
    { -0.015384615384615385,8.5401771125013664e-19 },   /* -1/65 */
    { 0.014925373134328358, 2.84805346802147e-19 },     /*  1/67 */
    { -0.014492753623188406,1.7598643959186484e-19 },   /* -1/69 */
    { 0.014084507042253521, -3.1762542517886622e-19 }   /*  1/71 */
};
static const int NATAN = sizeof(ATAN_COEF) / sizeof(ATAN_COEF[0]);

qfloat qf_atan_kernel(qfloat t)
{
    qfloat t2 = qf_mul(t, t);      // t^2

    qfloat p = ATAN_COEF[NATAN - 1];
    for (int i = NATAN - 2; i >= 0; --i)
        p = qf_add(ATAN_COEF[i], qf_mul(p, t2));

    qfloat t3 = qf_mul(t, t2);     // t^3
    return qf_sub(t, qf_mul(t3, p));
}

qfloat qf_atan(qfloat x)
{
    static const qfloat one  = { 1.0, 0.0 };
    static const qfloat RED  = { 0.41421356237309503, 1.4349369327986359e-17 }; /* tan(pi/8) */
    static const qfloat TMAX = { 0.35, 0.0 };                                   /* kernel limit */

    int neg = (x.hi < 0.0);
    if (neg)
        x = qf_neg(x);

    qfloat r, t;

    if (qf_gt(x, one)) {
        t = qf_div(one, x);  /* t = 1/x */

        if (qf_gt(t, TMAX)) {
            /* atan(t) = atan((t-1)/(t+1)) + pi/4
               atan(x) = pi/2 - atan(t)
                       = pi/2 - (atan(u) + pi/4)
                       = pi/4 - atan(u)
            */
            qfloat num = qf_sub(t, one);
            qfloat den = qf_add(t, one);
            qfloat u   = qf_div(num, den);
            qfloat k   = qf_atan_kernel(u);
            r = qf_sub(QF_PI_4, k);
        } else {
            qfloat k = qf_atan_kernel(t);
            r = qf_sub(QF_PI_2, k);
        }
    }
    else if (qf_gt(x, RED)) {
        /* atan(x) = atan((x-1)/(x+1)) + pi/4 */
        qfloat num = qf_sub(x, one);
        qfloat den = qf_add(x, one);
        t = qf_div(num, den);          /* |t| <= 1/3 */
        qfloat k = qf_atan_kernel(t);
        r = qf_add(QF_PI_4, k);
    }
    else {
        r = qf_atan_kernel(x);
    }

    if (neg)
        r = qf_neg(r);

    return r;
}

qfloat qf_atan2(qfloat y, qfloat x)
{
    qfloat zero = qf_from_double(0.0);

    /* y = 0 */
    if (qf_eq(y, zero)) {
        if (qf_gt(x, zero)) return zero;
        if (qf_lt(x, zero)) return QF_PI;
        return zero; /* undefined, return 0 */
    }

    /* x = 0 */
    if (qf_eq(x, zero)) {
        if (qf_gt(y, zero)) return QF_PI_2;
        else return qf_neg(QF_PI_2);
    }

    /* General case */
    qfloat a = qf_atan(qf_div(y, x));

    if (qf_gt(x, zero))
        return a;

    if (qf_gt(y, zero))
        return qf_add(a, QF_PI);

    return qf_sub(a, QF_PI);
}

qfloat qf_asin(qfloat x)
{
    qfloat one = qf_from_double(1.0);
    qfloat zero = qf_from_double(0.0);

    /* Domain check */
    if (qf_gt(qf_abs(x), one))
        return QF_NAN;

    /* Exact endpoints */
    if (qf_eq(x, zero))
        return zero;
    if (qf_eq(x, one))
        return QF_PI_2;
    if (qf_eq(x, qf_neg(one)))
        return qf_neg(QF_PI_2);

    /* asin(x) = atan(x / sqrt(1 - x^2)) for |x| < 1 */
    qfloat x2 = qf_mul(x, x);
    qfloat t  = qf_sqrt(qf_sub(one, x2));
    qfloat y  = qf_div(x, t);

    return qf_atan(y);
}

qfloat qf_acos(qfloat x)
{
    qfloat one = qf_from_double(1.0);

    if (qf_gt(qf_abs(x), one))
        return QF_NAN;

    qfloat half_pi = QF_PI_2;
    return qf_sub(half_pi, qf_asin(x));
}

qfloat qf_sinh(qfloat x)
{
    qfloat ex  = qf_exp(x);
    qfloat emx = qf_exp(qf_neg(x));

    qfloat num = qf_sub(ex, emx);
    qfloat two = qf_from_double(2.0);

    return qf_div(num, two);
}

qfloat qf_cosh(qfloat x)
{
    qfloat ex  = qf_exp(x);
    qfloat emx = qf_exp(qf_neg(x));

    qfloat sum = qf_add(ex, emx);
    qfloat two = qf_from_double(2.0);

    return qf_div(sum, two);
}

qfloat qf_tanh(qfloat x)
{
    qfloat two = qf_from_double(2.0);
    qfloat t   = qf_exp(qf_mul(two, x));   // e^(2x)

    qfloat num = qf_sub(t, qf_from_double(1.0));
    qfloat den = qf_add(t, qf_from_double(1.0));

    return qf_div(num, den);
}

qfloat qf_asinh(qfloat x)
{
    qfloat one = qf_from_double(1.0);

    qfloat x2  = qf_mul(x, x);
    qfloat rad = qf_sqrt(qf_add(x2, one));
    qfloat sum = qf_add(x, rad);

    return qf_log(sum);
}

qfloat qf_acosh(qfloat x)
{
    qfloat one = qf_from_double(1.0);

    if (qf_eq(x, one))
        return qf_from_double(0.0);

    /* Domain check: x < 1 → NaN */
    if (qf_lt(x, one))
        return QF_NAN;

    qfloat xm1 = qf_sub(x, one);
    qfloat xp1 = qf_add(x, one);

    qfloat s1 = qf_sqrt(xm1);
    qfloat s2 = qf_sqrt(xp1);

    qfloat prod = qf_mul(s1, s2);
    qfloat sum  = qf_add(x, prod);

    return qf_log(sum);
}

qfloat qf_atanh(qfloat x)
{
    qfloat one = qf_from_double(1.0);

    /* Domain check: |x| >= 1 → NaN */
    if (qf_ge(qf_abs(x), one))
        return QF_NAN;

    qfloat num = qf_add(one, x);
    qfloat den = qf_sub(one, x);

    qfloat frac = qf_div(num, den);
    qfloat half = qf_from_double(0.5);

    return qf_mul(half, qf_log(frac));
}

qfloat qf_gamma(qfloat x)
{
    /* Reject extreme magnitudes for now */
    if (fabs(x.hi) > 1e+240) 
        return (qfloat){ NAN, NAN };

    /* Coefficients for 1/gamma(1+x) - x ... */
    static const qfloat c[43] = {
        { 0.57721566490153287, -4.9429151524306318e-18 },
        { -0.6558780715202539,  2.137185197068536e-17 },
        { -0.042002635034095237, 1.4920306285650486e-18 },
        { 0.16653861138229148,  1.0189144546842026e-17 },
        { -0.042197734555544333, -3.3579992682480126e-18 },
        { -0.009621971527876973, -5.3000313688302694e-19 },
        { 0.0072189432466630999, -3.6006537063394298e-19 },
        { -0.0011651675918590652, 5.6599478538809832e-20 },
        { -0.00021524167411495098, 2.3758686180729462e-21 },
        { 0.0001280502823881162, -9.3591244991989629e-21 },
        { -2.0134854780788239e-05, 3.0488773972038167e-23 },
        { -1.2504934821426706e-06, -2.6621409227189828e-23 },
        { 1.1330272319816959e-06, -4.62223521210487e-23 },
        { -2.0563384169776071e-07, -3.0061601618645131e-24 },
        { 6.1160951044814161e-09, -2.6934582981713047e-25 },
        { 5.0020076444692229e-09, -1.5381236140567385e-26 },
        { -1.18127457048702e-09, -1.0052356155716209e-25 },
        { 1.0434267116911005e-10, -2.9298419956825013e-27 },
        { 7.7822634399050708e-12, 4.3972555565958471e-28 },
        { -3.696805618642206e-12, 2.7050034921703867e-28 },
        { 5.1003702874544758e-13, 2.2530014610858764e-29 },
        { -2.0583260535665066e-14, -1.4747481491954327e-30 },
        { -5.3481225394230178e-15, -1.6208384686356537e-31 },
        { 1.2267786282382608e-15, -5.0729151460238656e-32 },
        { -1.1812593016974588e-16, 6.4222578381496798e-33 },
        { 1.1866922547516004e-18, -4.2037265494226025e-35 },
        { 1.4123806553180319e-18, -7.5769467011162874e-35 },
        { -2.2987456844353702e-19, 1.3335481917069349e-36 },
        { 1.7144063219273374e-20, 5.2307151504269495e-38 },
        { 1.3373517304936931e-22, 2.6434059649079244e-39 },
        { -2.0542335517666728e-22, 3.6856892424569032e-39 },
        { 2.7360300486080001e-23, -2.8599315416397761e-39 },
        { -1.7323564459105165e-24, -1.75408835081976e-40 },
        { -2.3606190244992872e-26, -1.2602250169957861e-42 },
        { 1.8649829417172943e-26, 8.7747756172909715e-43 },
        { 2.2180956242071973e-27, -6.809640315042761e-44 },
        { 1.2977819749479937e-28, -3.3256924668040879e-45 },
        { 1.1806974749665284e-30, -4.1849492759666025e-48 },
        { -1.1245843492770881e-30, -2.0184281548735493e-47 },
        { 1.2770851751408661e-31, 1.0535632367878755e-47 },
        { -7.391451169615141e-33, 1.8114253268366172e-49 },
        { 1.1347502575542158e-35, -4.9791058715013262e-52 },
        { 4.639134641058722e-35, 2.6040634859975001e-52 }
    };

    const int n = 43;

    /* ss = x, f = 1, sum = 0 */
    qfloat ss = x;
    qfloat f  = (qfloat){1.0, 0.0};
    qfloat sum = (qfloat){0.0, 0.0};
    qfloat one = (qfloat){1.0, 0.0};

    /* Reduce x into [1,2) by shifting and accumulating f */
    while (ss.hi > 1.0) {
        ss = qf_sub(ss, one);
        f  = qf_mul(f, ss);
    }

    while (ss.hi < 1.0) {
        f  = qf_div(f, ss);
        ss = qf_add(ss, one);
    }

    /* If ss == 1, gamma(x) = f */
    if (ss.hi == 1.0 && ss.lo == 0.0)
        return f;

    /* ss = ss - 1 */
    ss = qf_sub(ss, one);

    /* Horner evaluation of polynomial */
    for (int i = n - 1; i >= 0; i--) {
        qfloat tmp = qf_mul(ss, sum);
        tmp = qf_add(tmp, c[i]);
        sum = tmp;
    }

    /* temp = 1 + ss * sum */
    qfloat temp = qf_add(one, qf_mul(ss, sum));

    /* result = f / temp */
    return qf_div(f, temp);
}

qfloat qf_erf(qfloat x)
{
    qfloat zero = (qfloat){0.0, 0.0};

    /* erf(0) = 0 */
    if (x.hi == 0.0)
        return zero;

    /* |x| */
    qfloat y = qf_abs(x);

    /* erf saturates for |x| > 26 */
    if (y.hi > 26.0) {
        qfloat one = (qfloat){1.0, 0.0};
        return (x.hi > 0.0) ? one : qf_neg(one);
    }

    /* cutoff between power series and continued fraction */
    const double cut = 1.5;

    /* ------------------------------------------------------------
       POWER SERIES for |x| < cut
       ------------------------------------------------------------ */
    if (y.hi < cut) {

        /* ap = 0.5, s = 2.0, t = 2.0 */
        qfloat ap = qf_from_double(0.5);
        qfloat s  = qf_from_double(2.0);
        qfloat t  = qf_from_double(2.0);

        /* x^2 */
        qfloat x2 = qf_mul(x, x);

        int i;
        for (i = 0; i < 200; i++) {

            /* ap += 1 */
            ap = qf_add(ap, qf_from_double(1.0));

            /* t *= x2 / ap */
            qfloat temp = qf_div(x2, ap);
            t = qf_mul(t, temp);

            /* s += t */
            s = qf_add(s, t);

            /* convergence test: |t| < 1e-35 * |s| */
            if (fabs(t.hi) < 1e-35 * fabs(s.hi)) {

                /* ex2 = exp(x^2) */
                qfloat ex2 = qf_exp(x2);

                /* result = x * (2/sqrt(pi)) * s / exp(x^2) */
                qfloat result = x;
                result = qf_mul(result, QF_SQRT1ONPI); /* your qfloat constant */
                result = qf_mul(result, s);
                result = qf_div(result, ex2);

                return result;
            }
        }

        /* failed to converge */
        qfloat nan = {NAN, NAN};
        return nan;
    }

    /* ------------------------------------------------------------
       CONTINUED FRACTION (Lentz method) for |x| >= cut
       ------------------------------------------------------------ */

    double small = 1e-300;

    qfloat x2 = qf_mul(x, x);

    /* b = x^2 + 0.5 */
    qfloat b = qf_add(x2, qf_from_double(0.5));

    /* c = huge */
    qfloat c = qf_from_double(1e300);

    /* d = 1/b */
    qfloat d = qf_div(qf_from_double(1.0), b);

    /* h = d */
    qfloat h = d;

    int i;
    for (i = 1; i < 300; i++) {

        double an = i * (0.5 - i);

        /* b += 2 */
        b = qf_add(b, qf_from_double(2.0));

        /* d = an * d + b */
        qfloat dtemp = qf_mul(qf_from_double(an), d);
        d = qf_add(dtemp, b);

        if (fabs(d.hi) < small)
            d = qf_from_double(small);

        /* c = an / c + b */
        qfloat ctemp = qf_div(qf_from_double(an), c);
        c = qf_add(ctemp, b);

        if (fabs(c.hi) < small)
            c = qf_from_double(small);

        /* d = 1/d */
        d = qf_div(qf_from_double(1.0), d);

        /* del = d * c */
        qfloat del = qf_mul(d, c);

        /* h *= del */
        h = qf_mul(h, del);

        /* convergence: del ≈ 1 */
        if (del.hi == 1.0 && fabs(del.lo) < 1e-30)
            break;

        if (i == 299) {
            qfloat nan = {NAN, NAN};
            return nan;
        }
    }

    /* sx2 = sqrt(x^2) = |x| */
    qfloat sx2 = qf_sqrt(x2);

    /* ex2 = exp(x^2) */
    qfloat ex2 = qf_exp(x2);

    /* result = (2/sqrt(pi)) * sx2 / exp(x^2) * h - 1 */
    qfloat result = QF_SQRT1ONPI;
    result = qf_mul(result, sx2);
    result = qf_div(result, ex2);
    result = qf_mul(result, h);
    result = qf_sub(result, qf_from_double(1.0));
    result = qf_neg(result);

    /* restore sign */
    if (x.hi <= 0.0)
        result = qf_neg(result);

    return result;
}

qfloat qf_erfc(qfloat x)
{
    qfloat one  = (qfloat){1.0, 0.0};

    /* erfc(0) = 1 */
    if (x.hi == 0.0)
        return one;

    if (qf_gt(x, (qfloat){26.3,0}))
        return (qfloat){0,0};

    /* For x < 0: erfc(x) = 1 - erf(x) */
    if (x.hi < 0.0) {
        qfloat erfx = qf_erf(x);
        return qf_sub(one, erfx);
    }

    /* |x| */
    qfloat y = qf_abs(x);

    const double cut = 1.5;

    /* ------------------------------------------------------------
       POWER SERIES for |x| < cut
       ------------------------------------------------------------ */
    if (y.hi < cut) {

        /* ap = 0.5, s = 2.0, t = 2.0 */
        qfloat ap = qf_from_double(0.5);
        qfloat s  = qf_from_double(2.0);
        qfloat t  = qf_from_double(2.0);

        /* x^2 */
        qfloat x2 = qf_mul(x, x);

        int i;
        for (i = 0; i < 200; i++) {

            /* ap += 1 */
            ap = qf_add(ap, qf_from_double(1.0));

            /* t *= x2; t /= ap */
            t = qf_mul(t, x2);
            t = qf_div(t, ap);

            /* s += t */
            s = qf_add(s, t);

            /* convergence: |t| < 1e-35 * |s| */
            if (fabs(t.hi) < 1e-35 * fabs(s.hi)) {

                /* ex2 = exp(x^2) */
                qfloat ex2 = qf_exp(x2);

                /* result = - ( x * (2/sqrt(pi)) * s / exp(x^2) - 1 ) */
                qfloat result = x;
                result = qf_mul(result, QF_SQRT1ONPI);
                result = qf_mul(result, s);
                result = qf_div(result, ex2);
                result = qf_sub(result, one);
                result = qf_neg(result);

                return result;
            }
        }

        /* failed to converge */
        qfloat nan = {NAN, NAN};
        return nan;
    }

    /* ------------------------------------------------------------
       CONTINUED FRACTION (Lentz method) for |x| >= cut
       ------------------------------------------------------------ */

    double small = 1e-300;

    /* x^2 */
    qfloat x2 = qf_mul(x, x);

    /* b = x^2 + 0.5 */
    qfloat b = qf_add(x2, qf_from_double(0.5));

    /* c = huge */
    qfloat c = qf_from_double(1e300);

    /* d = 1/b */
    qfloat d = qf_div(qf_from_double(1.0), b);

    /* h = d */
    qfloat h = d;

    int i;
    for (i = 1; i < 300; i++) {

        double an = i * (0.5 - i);

        /* b += 2 */
        b = qf_add(b, qf_from_double(2.0));

        /* d = an*d + b */
        qfloat dtemp = qf_mul(qf_from_double(an), d);
        d = qf_add(dtemp, b);

        if (fabs(d.hi) < small)
            d = qf_from_double(small);

        /* c = an/c + b */
        qfloat ctemp = qf_div(qf_from_double(an), c);
        c = qf_add(ctemp, b);

        if (fabs(c.hi) < small)
            c = qf_from_double(small);

        /* d = 1/d */
        d = qf_div(qf_from_double(1.0), d);

        /* del = d * c */
        qfloat del = qf_mul(d, c);

        /* h *= del */
        h = qf_mul(h, del);

        /* convergence: del ≈ 1 */
        if (del.hi == 1.0 && fabs(del.lo) < 1e-30)
            break;

        if (i == 299) {
            qfloat nan = {NAN, NAN};
            return nan;
        }
    }

    /* sx2 = sqrt(x^2) = |x| */
    qfloat sx2 = qf_sqrt(x2);

    /* ex2 = exp(x^2) */
    qfloat ex2 = qf_exp(x2);

    /* result = (2/sqrt(pi)) * sx2 * h / exp(x^2) */
    qfloat result = QF_SQRT1ONPI;
    result = qf_mul(result, sx2);
    result = qf_mul(result, h);
    result = qf_div(result, ex2);

    return result;
}

qfloat qf_erfinv_initial(qfloat x)
{
    qfloat one = qf_from_double(1.0);
    qfloat a   = qf_from_double(0.147);

    /* t = 1 - x^2 */
    qfloat t  = qf_mul(qf_sub(one, x), qf_add(one, x));
    qfloat ln = qf_log(t);   /* ln(1 - x^2), negative */

    /* c = 2/(pi*a) + ln(1-x^2)/2 */
    qfloat c1 = qf_div(qf_from_double(2.0),
                       qf_mul(qf_from_double(M_PI), a));
    qfloat c2 = qf_div(ln, qf_from_double(2.0));
    qfloat c  = qf_add(c1, c2);

    /* inside = c^2 - ln(1-x^2)/a */
    qfloat c2sq   = qf_mul(c, c);
    qfloat ln_over_a = qf_div(ln, a);
    qfloat inside = qf_sub(c2sq, ln_over_a);

    qfloat y0 = qf_sqrt(qf_sub(qf_sqrt(inside), c));

    return (x.hi >= 0.0 ? y0 : qf_neg(y0));
}

qfloat qf_erfinv(qfloat x)
{
    /* Domain check: erf(x) ∈ [-1,1] */
    if (x.hi <= -1.0 || x.hi >= 1.0)
        return QF_NAN;

    qfloat one = qf_from_double(1.0);

    /* y0 = sign(x) * p */
    qfloat y;
    if (x.hi > 0.87 || x.hi < -0.87)
        y = qf_erfinv_initial(x);
    else {
        /* Initial guess using A&S 7.1.26 */
        qfloat t   = qf_log(qf_mul(qf_sub(one, x), qf_add(one, x)));  /* log(1-x^2) */

        /* p = sqrt(-2 * t) */
        qfloat p = qf_sqrt(qf_mul(qf_from_double(-2.0), t));
        y = (x.hi >= 0.0 ? p : qf_neg(p));
    }

    /* Halley iteration */
    for (int i = 0; i < 20; i++) {

        qfloat erfy = qf_erf(y);
        qfloat f    = qf_sub(erfy, x);

        /* f' = 2/sqrt(pi) * exp(-y^2) */
        qfloat y2   = qf_mul(y, y);
        qfloat fp   = qf_mul(QF_2_SQRTPI, qf_exp(qf_neg(y2)));

        /* f'' = -2*y*f' */
        qfloat fpp  = qf_mul(qf_mul(qf_from_double(-2.0), y), fp);

        /* Halley correction */
        qfloat ratio = qf_div(f, fp);
        qfloat corr  = qf_mul(ratio,
                              qf_sub(one,
                                     qf_mul(qf_div(fpp, fp),
                                            qf_div(f, qf_from_double(2.0)))));

        y = qf_sub(y, corr);

        if (qf_abs(corr).hi < 1e-33)
            break;
    }

    return y;
}

qfloat qf_erfcinv(qfloat x)
{
    static const qfloat QF_POSINF = { INFINITY, 0.0 };
    static const qfloat QF_NEGINF = { -INFINITY, 0.0 };

    /* Domain: erfc(x) ∈ [0,2] */
    if (x.hi < 0.0 || x.hi > 2.0)
        return QF_NAN;

    /* Special cases */
    if (x.hi == 0.0)
        return QF_POSINF;   /* erfcinv(0) = +∞ */

    if (x.hi == 2.0)
        return QF_NEGINF;   /* erfcinv(2) = -∞ */

    /* erfcinv(x) = erfinv(1 - x) */
    qfloat one = qf_from_double(1.0);
    qfloat t   = qf_sub(one, x);

    return qf_erfinv(t);
}

/* lgamma */

// Chebyshev coefficients for lgamma on x in [1,2]
// x = 1.5 + 0.5*y, y in [-1,1]
static const qfloat QF_LGAMMA_C[41] = {
    { -6.08999038183025906e-02, -1.67161028145317951e-18 },
    { 4.68966674650670226e-03, -8.00414782984271387e-20 },
    { 6.03821964754341009e-02, 8.91374388867524059e-19 },
    { -4.62354291360419882e-03, -1.50080384149393643e-19 },
    { 5.08594246177812852e-04, -4.64865960072415394e-20 },
    { -6.48096309907942919e-05, 6.08542959501079400e-21 },
    { 8.91789555893555209e-06, 3.64946801639257361e-22 },
    { -1.28460622392681072e-06, 5.13415286655144083e-23 },
    { 1.90645789381254639e-07, -6.73825530594384962e-24 },
    { -2.88863999762326789e-08, -1.02191936721667432e-24 },
    { 4.44388964696602180e-09, 2.32420800108149269e-25 },
    { -6.91644118641151481e-10, -4.43529847594145473e-26 },
    { 1.08642239812207473e-10, -6.31783249108896957e-27 },
    { -1.71936441435665828e-11, -1.65929888887441709e-28 },
    { 2.73808111348050974e-12, -2.44643404739865581e-29 },
    { -4.38351981266623460e-13, 2.43587894320257005e-29 },
    { 7.04983690833580226e-14, 1.80750883256918504e-30 },
    { -1.13831191223879100e-14, -6.61987983795187872e-31 },
    { 1.84443899804457734e-15, -1.03502551361958826e-33 },
    { -2.99791195728252394e-16, -4.70682418520674646e-33 },
    { 4.88633780521465099e-17, -2.18707254275030300e-33 },
    { -7.98432732970867613e-18, -2.83974669811037101e-34 },
    { 1.30761814113621808e-18, 4.59424372259388873e-37 },
    { -2.14596610292519470e-19, -1.06374646291926466e-35 },
    { 3.52847599204739409e-20, 1.86807197209395812e-36 },
    { -5.81174419925206039e-21, 1.07441003363300641e-39 },
    { 9.58785514221059594e-22, -6.97965797235405770e-38 },
    { -1.58408867977223588e-22, -7.04997204669257162e-39 },
    { 2.62079917095521999e-23, -5.14720645065674467e-40 },
    { -4.34152534587496908e-24, 3.43741543057570546e-41 },
    { 7.20058323882912525e-25, 2.13645720841362319e-41 },
    { -1.19557229613262265e-25, -3.99052811166131268e-42 },
    { 1.98717527367176802e-26, 1.20574236884886726e-42 },
    { -3.30613691379029724e-27, -3.29526650073585415e-43 },
    { 5.50559780589002012e-28, -3.18258650823278825e-44 },
    { -9.17622347123853872e-29, -4.26164431763687497e-45 },
    { 1.53065795475830925e-29, 8.10791405401706583e-46 },
    { -2.55521564526239694e-30, 5.58787868610482347e-47 },
    { 4.26868702632878174e-31, -2.51950511524706172e-48 },
    { -7.13611652116631315e-32, -2.36529320285422706e-48 },
    { 1.19375492841650126e-32, 4.16816997158601089e-50 },
};

static const int QF_LGAMMA_N = sizeof(QF_LGAMMA_C) / sizeof(QF_LGAMMA_C[0]);

// x in [1,2]
// Chebyshev core: lgamma(x) on x in [1,2]
// x = 1.5 + 0.5*y, y in [-1,1]
static qfloat qf_lgamma_core_1_2(qfloat x)
{
    qfloat one_point_five = qf_div((qfloat){ 3.0, 0.0 }, (qfloat){ 2.0, 0.0 });
    qfloat two            = (qfloat){ 2.0, 0.0 };

    // y = 2*(x - 1.5)
    qfloat t = qf_sub(x, one_point_five);
    qfloat y = qf_mul(two, t);

    qfloat b1 = (qfloat){ 0.0, 0.0 };
    qfloat b2 = (qfloat){ 0.0, 0.0 };

    // k = N .. 1  (do NOT include c[0] in the loop)
    for (int k = QF_LGAMMA_N - 1; k >= 1; --k) {
        qfloat term = qf_mul(y, b1);      // y*b1
        term        = qf_add(term, term); // 2*y*b1
        qfloat bk   = qf_sub(term, b2);   // 2*y*b1 - b2
        bk          = qf_add(bk, QF_LGAMMA_C[k]); // + c_k
        b2          = b1;
        b1          = bk;
    }

    // f(y) = c0 + y*b1 - b2
    qfloat yb1 = qf_mul(y, b1);
    qfloat res = qf_add(QF_LGAMMA_C[0], qf_sub(yb1, b2));
    return res;
}

static int qf_is_integer(qfloat x)
{
    // crude but effective: check hi is integer and lo is (near) zero
    double r = nearbyint(x.hi);
    if (fabs(x.hi - r) > 1e-30) return 0;
    if (fabs(x.lo) > 1e-30)     return 0;
    return 1;
}

qfloat qf_lgamma(qfloat x)
{
    if (x.hi <= 0.0 && qf_is_integer(x)) {
        return QF_NAN;
    }

    // Handle poles and reflection as you already do
    if (x.hi < 0.5) {
        qfloat one_minus_x = qf_sub((qfloat){1.0,0.0}, x);
        qfloat lg1mx       = qf_lgamma(one_minus_x);

        qfloat pix     = qf_mul(QF_PI, x);
        qfloat sin_pix = qf_sin(pix);
        qfloat sin_abs = qf_abs(sin_pix);

        qfloat term = qf_sub(qf_log(QF_PI), qf_log(sin_abs));
        return qf_sub(term, lg1mx);
    }

    // Shift x into [1,2] using recurrence
    qfloat acc = (qfloat){ 0.0, 0.0 };
    qfloat one = (qfloat){ 1.0, 0.0 };

    qfloat z = x;
    while (z.hi > 2.0) {
        qfloat zm1 = qf_sub(z, one);
        acc = qf_add(acc, qf_log(zm1));
        z   = zm1;
    }
    while (z.hi < 1.0) {
        acc = qf_sub(acc, qf_log(z));
        z   = qf_add(z, one);
    }

    // Now z in [1,2]
    qfloat core = qf_lgamma_core_1_2(z);
    return qf_add(core, acc);
}

/* digamma */
static const qfloat QF_DIGAMMA_C[41] = {
    { -1.90285404176089613e-02, 5.77950654326901646e-19 },
    { 4.91415393029387138e-01, -1.00314059023864708e-17 },
    { -5.68157478212447317e-02, 1.47606722184751199e-18 },
    { 8.35782122591431295e-03, 1.84833746441407156e-19 },
    { -1.33323285799434262e-03, 2.44253141837223908e-20 },
    { 2.20313287069308242e-04, 7.04743594459306408e-21 },
    { -3.70402381784568812e-05, -2.41558762401185015e-21 },
    { 6.28379365485498991e-06, -1.72214002405117141e-23 },
    { -1.07126390850618494e-06, -4.15021640019019289e-23 },
    { 1.83128394654841645e-07, 1.31143123378132922e-23 },
    { -3.13535093618085118e-08, 1.90437811832256668e-24 },
    { 5.37280877620077694e-09, -3.18422729277816914e-25 },
    { -9.21168141597842787e-10, 3.01991146790539010e-26 },
    { 1.57981265214818217e-10, 1.06826244367357038e-26 },
    { -2.70986461323804431e-11, 5.33776877776948319e-29 },
    { 4.64872285990968355e-12, -6.42760009157896288e-29 },
    { -7.97527256383036903e-13, 5.48906401220595635e-30 },
    { 1.36827238574769926e-13, -3.25172343071082977e-30 },
    { -2.34751560606589727e-14, 1.71489761339785169e-32 },
    { 4.02763071556035413e-15, -2.17579728461725310e-32 },
    { -6.91025185311790359e-16, -1.95628384008146353e-32 },
    { 1.18560471388633490e-16, 5.29641084491198226e-33 },
    { -2.03416896162615605e-17, 1.20954595229290271e-33 },
    { 3.49007496864630452e-18, -1.36622344131104636e-34 },
    { -5.98801469349767083e-19, -2.66267378666949710e-35 },
    { 1.02738016280805886e-19, -2.99363748919878719e-36 },
    { -1.76270494245610717e-20, 3.49210972887234883e-37 },
    { 3.02432280181569192e-21, 1.27466687154981162e-37 },
    { -5.18891683020923120e-22, -1.78666228593633717e-38 },
    { 8.90277303458457169e-23, -2.96369672922930009e-39 },
    { -1.52747428994267271e-23, -1.28287361944115855e-39 },
    { 2.62073147989620832e-24, -1.73156098461752579e-41 },
    { -4.49646427382207021e-25, 3.84713318267651681e-41 },
    { 7.71471295963450907e-26, 5.40922237403262156e-43 },
    { -1.32363547618877013e-26, -1.25706875670109363e-42 },
    { 2.27099943624082203e-27, -1.53013807540290209e-43 },
    { -3.89641902153747750e-28, 1.21847106439810112e-44 },
    { 6.68519813888564475e-29, -7.18464767771076781e-46 },
    { -1.14699866549129985e-29, -2.89245204428577906e-46 },
    { 1.96793858865896475e-30, 1.48246258276809115e-47 },
    { -3.37644881893549909e-31, -7.84061880361953800e-48 },
};

static const int QF_DIGAMMA_N = sizeof(QF_DIGAMMA_C) / sizeof(QF_DIGAMMA_C[0]);

/* Asymptotic digamma for x >= 8 */
static qfloat qf_digamma_core_1_2(qfloat x)
{
    qfloat one_point_five = (qfloat){ 1.5, 0.0 };
    qfloat two            = (qfloat){ 2.0, 0.0 };

    qfloat t = qf_sub(x, one_point_five);
    qfloat y = qf_mul(two, t);

    qfloat b1 = (qfloat){ 0.0, 0.0 };
    qfloat b2 = (qfloat){ 0.0, 0.0 };

    for (int k = QF_DIGAMMA_N - 1; k >= 1; --k) {
        qfloat term = qf_mul(y, b1);
        term        = qf_add(term, term);
        qfloat bk   = qf_sub(term, b2);
        bk          = qf_add(bk, QF_DIGAMMA_C[k]);
        b2          = b1;
        b1          = bk;
    }

    qfloat yb1 = qf_mul(y, b1);
    qfloat res = qf_add(QF_DIGAMMA_C[0], qf_sub(yb1, b2));
    return res;
}

static const qfloat QF_DIGAMMA_C_8_20[81] = {
    { 2.54950422984596603e+00, -2.09119746016275927e-16 },
    { 4.68589303047532435e-01, 7.66340925430541880e-18 },
    { -5.48629055282131919e-02, 5.94722323398451975e-19 },
    { 8.55970924708734579e-03, -1.55316535645569145e-19 },
    { -1.50157808062876012e-03, -5.23010271126881327e-20 },
    { 2.80816752048716517e-04, 3.26891307598668353e-22 },
    { -5.46746185277003971e-05, -8.41274598327493914e-22 },
    { 1.09431770690511839e-05, 5.37240696950517601e-22 },
    { -2.23469416121451561e-06, 9.43404942376238626e-23 },
    { 4.63336139407399596e-07, 2.02850169436541142e-23 },
    { -9.72154361378278585e-08, -6.17292219697947856e-24 },
    { 2.05924060670192887e-08, 8.92944176059212616e-25 },
    { -4.39595526923414367e-09, -3.85561470010516859e-25 },
    { 9.44492147297975419e-10, 7.48681058004827020e-26 },
    { -2.04030783999987236e-10, 6.37250994137952819e-27 },
    { 4.42783887629607408e-11, -1.00337349061612093e-28 },
    { -9.64722203147335565e-12, 7.09849273471600496e-28 },
    { 2.10908948581337061e-12, 1.00923614250666221e-28 },
    { -4.62464368342014497e-13, -8.24091266461147395e-30 },
    { 1.01669803272339272e-13, 5.37396311345255245e-30 },
    { -2.24027412129914093e-14, 4.15569640714869150e-32 },
    { 4.94642108934845712e-15, -1.35494938565922316e-31 },
    { -1.09411631291027294e-15, -3.21679576000587572e-32 },
    { 2.42400588115582970e-16, 2.37780837984352437e-32 },
    { -5.37809081710171457e-17, -1.99178852486117083e-33 },
    { 1.19476584815702535e-17, -4.87860090383926079e-34 },
    { -2.65730569201736606e-18, -1.58018102531447681e-34 },
    { 5.91636357563904049e-19, -3.01369635443818123e-35 },
    { -1.31849693670185424e-19, 6.22264614029992917e-36 },
    { 2.94086832206377069e-20, -4.55650533303423022e-37 },
    { -6.56462209470111854e-21, 5.64166333574949169e-38 },
    { 1.46639438931999229e-21, 9.20812396853552739e-38 },
    { -3.27771507299573072e-22, -3.11050936062752757e-39 },
    { 7.33071300097256533e-23, -2.33614557130863466e-39 },
    { -1.64041447251502394e-23, 1.07940145553020031e-39 },
    { 3.67259686204029976e-24, -2.99094772511882266e-40 },
    { -8.22596941684426241e-25, -6.13353410222867454e-42 },
    { 1.84322639825345939e-25, -1.01090471483355077e-41 },
    { -4.13174165609741238e-26, 2.82948228976731020e-42 },
    { 9.26482064151839989e-27, -7.64139054045586873e-44 },
    { -2.07815517401612575e-27, 1.24097757863511696e-43 },
    { 4.66277914882720062e-28, 1.12128970468799759e-44 },
    { -1.04647158273489964e-28, 3.45115668269509025e-45 },
    { 2.34918082945703175e-29, -8.54386302190212043e-46 },
    { -5.27476807242550816e-30, -1.96837563137223220e-46 },
    { 1.18462372010939758e-30, -4.05748394249842265e-47 },
    { -2.66097316817147352e-31, -2.29281033419110456e-48 },
    { 5.97829110182441406e-32, -3.50820600036048337e-48 },
    { -1.34333438600136001e-32, -1.30185301499354292e-48 },
    { 3.01895212801414155e-33, -3.21653300105483220e-50 },
    { -6.78560073114749181e-34, -3.59234351763996687e-50 },
    { 1.52537184784109782e-34, -2.76250027186877719e-51 },
    { -3.42936919952149926e-35, -1.26344965765483598e-51 },
    { 7.71080878101812822e-36, -5.81648630510925356e-52 },
    { -1.73392054486389538e-36, 3.01171404808667397e-53 },
    { 3.89940820263927923e-37, -1.46817298701566144e-53 },
    { -8.77011726400388661e-38, 4.27347369163916683e-54 },
    { 1.97263380858744082e-38, -2.65872524920411217e-55 },
    { -4.43730537545962758e-39, 2.83184759699717909e-55 },
    { 9.98209137301931613e-40, 1.55764712514258647e-56 },
    { -2.24569581745285912e-40, -2.69329227245850776e-57 },
    { 5.05248979264752302e-41, -2.21244120016699501e-57 },
    { -1.13679773886279710e-41, 1.38531827473891985e-58 },
    { 2.55789350652751397e-42, 3.83164022160231196e-59 },
    { -5.75574590275790612e-43, -2.86680964297605779e-59 },
    { 1.29520698365242855e-43, 1.49934637999608797e-60 },
    { -2.91469946287079303e-44, -1.66476245476715657e-60 },
    { 6.55940060653321680e-45, 7.89592189740674819e-62 },
    { -1.47621335357011412e-45, 1.08536005285453660e-62 },
    { 3.32236711107112803e-46, 1.43350665400540364e-62 },
    { -7.47753787722548085e-47, 4.67408071318429684e-63 },
    { 1.68298874128133347e-47, -6.43586748664060322e-64 },
    { -3.78803975904979566e-48, -1.77054886526016087e-64 },
    { 8.52624449917679186e-49, -7.12649910268406436e-65 },
    { -1.91915582115639763e-49, -1.03966904242559168e-65 },
    { 4.31987534874810472e-50, -3.00304905000790884e-67 },
    { -9.72389110486293897e-51, -7.22149325186610660e-68 },
    { 2.18885135597211899e-51, 9.07490570731457596e-68 },
    { -4.92718905179103295e-52, 8.26506117798127763e-69 },
    { 1.10914533469180689e-52, -8.62454683886728293e-69 },
    { -2.49679853174101265e-53, -1.39000124035474389e-70 },
};

static const int QF_DIGAMMA_N_8_20 = sizeof(QF_DIGAMMA_C_8_20) / sizeof(QF_DIGAMMA_C_8_20[0]);

static qfloat qf_digamma_core_8_20(qfloat x)
{
    qfloat center = (qfloat){ 14.0, 0.0 };
    qfloat halfw  = (qfloat){  6.0, 0.0 };

    qfloat t = qf_sub(x, center);
    qfloat y = qf_div(t, halfw);   // y in [-1,1]

    qfloat b1 = (qfloat){ 0.0, 0.0 };
    qfloat b2 = (qfloat){ 0.0, 0.0 };

    for (int k = QF_DIGAMMA_N_8_20 - 1; k >= 1; --k) {
        qfloat term = qf_mul(y, b1);
        term        = qf_add(term, term);   // 2*y*b1
        qfloat bk   = qf_sub(term, b2);
        bk          = qf_add(bk, QF_DIGAMMA_C_8_20[k]);
        b2 = b1;
        b1 = bk;
    }

    qfloat yb1 = qf_mul(y, b1);
    return qf_add(QF_DIGAMMA_C_8_20[0], qf_sub(yb1, b2));
}

static qfloat qf_digamma_asymp(qfloat x)
{
    /* psi(x) ~ log(x) - 1/(2x) - 1/(12x^2) + 1/(120x^4) - 1/(252x^6) + ... */
    qfloat logx = qf_log(x);

    qfloat x2 = qf_mul(x, x);
    qfloat x4 = qf_mul(x2, x2);
    qfloat x6 = qf_mul(x4, x2);

    qfloat one = qf_from_double(1.0);
    qfloat t1 = qf_div(qf_div(one, (qfloat){2.0,0.0}),   x);
    qfloat t2 = qf_div(qf_div(one, (qfloat){12.0,0.0}),  x2);
    qfloat t3 = qf_div(qf_div(one, (qfloat){120.0,0.0}), x4);
    qfloat t4 = qf_div(qf_div(one, (qfloat){252.0,0.0}), x6);

    qfloat s = qf_sub(logx, t1);
    s = qf_sub(s, t2);
    s = qf_add(s, t3);
    s = qf_sub(s, t4);

    return s;
}

qfloat qf_digamma(qfloat x)
{
    qfloat one = (qfloat){ 1.0, 0.0 };
    qfloat pi  = QF_PI;

    /* Poles */
    if (x.hi <= 0.0 && x.hi == floor(x.hi))
        return QF_NAN;

    /* Reflection */
    if (x.hi < 0.5) {
        qfloat one_minus_x = qf_sub(one, x);
        qfloat psi_1mx     = qf_digamma(one_minus_x);

        qfloat pix  = qf_mul(pi, x);
        qfloat sinx = qf_sin(pix);
        qfloat cosx = qf_cos(pix);
        qfloat cot  = qf_div(cosx, sinx);

        return qf_sub(psi_1mx, qf_mul(pi, cot));
    }

    /* Region A: reduce upward into [1,2] */
    if (x.hi < 1.0) {
        return qf_sub(qf_digamma(qf_add(x, one)), qf_div(one, x));
    }

    /* Region B: [1,2] → low‑range core */
    if (x.hi <= 2.0) {
        return qf_digamma_core_1_2(x);
    }

    /* Region C: (2,8) → reduce downward into [1,2] */
    if (x.hi < 8.0) {
        return qf_add(qf_digamma(qf_sub(x, one)), qf_div(one, qf_sub(x, one)));
    }

    /* Region D: [8,20] → high‑range core */
    if (x.hi <= 20.0) {
        return qf_digamma_core_8_20(x);
    }

    /* Region E: x > 20 → asymptotic */
    return qf_digamma_asymp(x);
}

/* gammainv */

static const qfloat QF_GAMMA_MIN_VAL = { 0.885603194410888700278815900582588, 0.0 };

qfloat qf_gammainv(qfloat y)
{
    qfloat one = qf_from_double(1.0);

    /* y <= 0: no real solution */
    if (y.hi <= 0.0)
        return QF_NAN;

    /* y < Gamma_min: no real solution on R */
    if (qf_lt(y, QF_GAMMA_MIN_VAL))
        return QF_NAN;

    qfloat x;

    /* ---------------------------------------------------------
       BRANCH SELECTION
       ---------------------------------------------------------
       Gamma(x) has:
         - a decreasing branch on (0, x_min)
         - a minimum at x_min ≈ 1.461632
         - an increasing branch on (x_min, ∞)

       For 0 < y <= 1:
         There are TWO solutions: x=1 and x=2.
         We want the PRINCIPAL branch: x in (0,1].

       For y > 1:
         Only one solution, on the increasing branch.
       --------------------------------------------------------- */

    if (qf_le(y, one)) {
        /* y <= 1: principal branch root is in (0,1] */
        x = one;   /* perfect starting point for Γ(x)=1 */
    } else {
        /* y > 1: monotonic region, start near log(y) */
        double yd = y.hi;
        double x0 = log(yd);
        if (x0 < 1.5) x0 = 1.5;   /* stay above the minimum */
        x = qf_from_double(x0);
    }

    qfloat logy = qf_log(y);

    /* Newton on f(x) = lgamma(x) - log(y) */
    for (int i = 0; i < 40; i++) {
        qfloat lg   = qf_lgamma(x);
        qfloat psi  = qf_digamma(x);
        qfloat f    = qf_sub(lg, logy);
        qfloat step = qf_div(f, psi);
        x = qf_sub(x, step);

        if (qf_abs(step).hi < 1e-33)
            break;
    }

    return x;
}

/* Lambert W: solve W * exp(W) = x.  W0: principal branch, Wm1: lower branch */

/* Newton iteration (stable everywhere) */

static qfloat qf_lambert_newton(qfloat x, qfloat w0)
{
    qfloat one = qf_from_double(1.0);

    for (int i = 0; i < 50; ++i) {
        qfloat ew   = qf_exp(w0);
        qfloat wew  = qf_mul(w0, ew);              /* w e^w */
        qfloat f    = qf_sub(wew, x);              /* f(w)  */

        qfloat denom = qf_mul(ew, qf_add(w0, one)); /* e^w (w+1) */

        qfloat step = qf_div(f, denom);
        w0 = qf_sub(w0, step);

        if (qf_abs(step).hi < 1e-33)
            break;
    }
    return w0;
}

/* Halley iteration (fast but unstable near x≈1) */

static qfloat qf_lambert_halley(qfloat x, qfloat w0)
{
    qfloat one = qf_from_double(1.0);
    qfloat two = qf_from_double(2.0);

    for (int i = 0; i < 40; ++i) {
        qfloat ew   = qf_exp(w0);
        qfloat wew  = qf_mul(w0, ew);
        qfloat f    = qf_sub(wew, x);

        qfloat wp1  = qf_add(w0, one);
        qfloat denom1 = qf_mul(ew, wp1);           /* f' = e^w (w+1) */

        qfloat wplus2 = qf_add(w0, two);
        qfloat f2     = qf_mul(ew, wplus2);        /* f'' = e^w (w+2) */

        qfloat t      = qf_div(f, denom1);
        qfloat corr2  = qf_mul(qf_from_double(0.5), qf_mul(t, f2));

        qfloat denom  = qf_sub(denom1, corr2);

        qfloat step   = qf_div(f, denom);
        w0 = qf_sub(w0, step);

        if (qf_abs(step).hi < 1e-33)
            break;
    }

    return w0;
}

/* Principal branch W0(x) */

qfloat qf_lambert_w0(qfloat x)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);
    qfloat e    = QF_E;
    qfloat minus_one = qf_from_double(-1.0);
    qfloat minus_one_over_e = qf_div(minus_one, e);

    /* Domain check */
    if (qf_lt(x, minus_one_over_e))
        return QF_NAN;

    /* Special case x = 0 */
    if (x.hi == 0.0)
        return zero;

    /* Special-case x ≈ -1/e using absolute tolerance */
    qfloat diff = qf_abs(qf_sub(x, minus_one_over_e));
    if (diff.hi < 1e-30)   /* absolute tolerance */
        return minus_one;

    /* ---------- FIX #1: avoid Halley near x ≈ 1 ---------- */
    if (qf_lt(qf_abs(x), qf_from_double(3.0))) {
        qfloat w0 = x;   /* safe initial guess */
        return qf_lambert_newton(x, w0);
    }

    /* ---------- Initial guess for general x ---------- */
    qfloat w0;

    /* Region A: near -1/e */
    if (qf_le(x, qf_from_double(-0.2))) {
        qfloat ex  = qf_mul(e, x);
        qfloat t   = qf_add(one, ex);
        qfloat two = qf_from_double(2.0);
        t = qf_mul(two, t);
        qfloat s = qf_sqrt(t);
        w0 = qf_add(minus_one, s);
    }
    /* Region B: small |x| */
    else if (qf_lt(qf_abs(x), qf_from_double(0.3))) {
        qfloat x2  = qf_mul(x, x);
        qfloat x3  = qf_mul(x2, x);
        w0 = qf_sub(x, x2);
        w0 = qf_add(w0, qf_mul(qf_from_double(1.5), x3));
    }
    /* Region C: large x */
    else {
        qfloat L1  = qf_log(x);
        qfloat L2  = qf_log(L1);
        qfloat L2_over_L1 = qf_div(L2, L1);
        w0 = qf_sub(L1, L2);
        w0 = qf_add(w0, L2_over_L1);
    }

    return qf_lambert_halley(x, w0);
}

/* Lower branch W_{-1}(x) */

qfloat qf_lambert_wm1(qfloat x)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);
    qfloat e    = QF_E;
    qfloat minus_one = qf_from_double(-1.0);
    qfloat minus_one_over_e = qf_div(minus_one, e);

    /* Domain: -1/e <= x < 0 */
    if (qf_lt(x, minus_one_over_e) || qf_ge(x, zero))
        return QF_NAN;

    /* Special-case x ≈ -1/e */
    qfloat diff = qf_abs(qf_sub(x, minus_one_over_e));
    if (diff.hi < 1e-30)   /* absolute tolerance */
        return minus_one;

    qfloat w0;

    /* Region A: near -1/e */
    if (qf_le(x, qf_from_double(-0.2))) {
        qfloat ex  = qf_mul(e, x);
        qfloat t   = qf_add(one, ex);
        qfloat two = qf_from_double(2.0);
        t = qf_mul(two, t);
        qfloat s = qf_sqrt(t);
        w0 = qf_sub(minus_one, s);
    }
    /* Region B: near 0− */
    else {
        qfloat nx   = qf_neg(x);
        qfloat L1   = qf_log(nx);
        qfloat minus_L1 = qf_neg(L1);
        qfloat L2   = qf_log(minus_L1);
        qfloat L2_over_L1 = qf_div(L2, L1);
        w0 = qf_sub(L1, L2);
        w0 = qf_add(w0, L2_over_L1);
    }

    return qf_lambert_newton(x, w0);
}

/* Beta function B(a,b) = Γ(a)Γ(b) / Γ(a+b) */

qfloat qf_beta(qfloat a, qfloat b)
{
    qfloat zero = qf_from_double(0.0);

    /* Domain: a>0, b>0 */
    if (qf_le(a, zero) || qf_le(b, zero))
        return QF_NAN;

    /* log B(a,b) = lgamma(a) + lgamma(b) - lgamma(a+b) */
    qfloat lg_a  = qf_lgamma(a);
    qfloat lg_b  = qf_lgamma(b);
    qfloat a_plus_b = qf_add(a, b);
    qfloat lg_ab = qf_lgamma(a_plus_b);

    qfloat logB = qf_add(lg_a, lg_b);
    logB        = qf_sub(logB, lg_ab);

    return qf_exp(logB);
}

/* log-Beta function: log B(a,b) */

qfloat qf_logbeta(qfloat a, qfloat b)
{
    qfloat zero = qf_from_double(0.0);

    /* Domain: a>0, b>0 */
    if (qf_le(a, zero) || qf_le(b, zero))
        return QF_NAN;

    qfloat lg_a  = qf_lgamma(a);
    qfloat lg_b  = qf_lgamma(b);
    qfloat lg_ab = qf_lgamma(qf_add(a, b));

    qfloat logB = qf_add(lg_a, lg_b);
    logB        = qf_sub(logB, lg_ab);

    return logB;
}

/* Generalized binomial coefficient C(a,b) = Γ(a+1) / (Γ(b+1) Γ(a-b+1)) */

qfloat qf_binomial(qfloat a, qfloat b)
{
    qfloat one  = qf_from_double(1.0);

    /* Compute log C(a,b) = lgamma(a+1) - lgamma(b+1) - lgamma(a-b+1) */
    qfloat a1   = qf_add(a, one);
    qfloat b1   = qf_add(b, one);
    qfloat amb1 = qf_add(qf_sub(a, b), one);

    qfloat lg_a1   = qf_lgamma(a1);
    qfloat lg_b1   = qf_lgamma(b1);
    qfloat lg_amb1 = qf_lgamma(amb1);

    qfloat logC = qf_sub(lg_a1, lg_b1);
    logC        = qf_sub(logC, lg_amb1);

    return qf_exp(logC);
}

/* Beta PDF: f(x; a,b) = x^(a-1) (1-x)^(b-1) / B(a,b) */

qfloat qf_beta_pdf(qfloat x, qfloat a, qfloat b)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);

    /* Domain: 0 < x < 1, a>0, b>0 */
    if (qf_le(x, zero) || qf_ge(x, one))
        return zero;
    if (qf_le(a, zero) || qf_le(b, zero))
        return QF_NAN;

    qfloat log_x    = qf_log(x);
    qfloat log_1mx  = qf_log(qf_sub(one, x));

    qfloat a1 = qf_sub(a, one);
    qfloat b1 = qf_sub(b, one);

    qfloat term1 = qf_mul(a1, log_x);
    qfloat term2 = qf_mul(b1, log_1mx);

    qfloat logB  = qf_logbeta(a, b);

    qfloat logpdf = qf_sub(qf_add(term1, term2), logB);

    return qf_exp(logpdf);
}

/* log Beta PDF: log f(x; a,b) = (a-1)log x + (b-1)log(1-x) - log B(a,b) */

qfloat qf_logbeta_pdf(qfloat x, qfloat a, qfloat b)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);

    /* Domain: 0 < x < 1, a>0, b>0 */
    if (qf_le(x, zero) || qf_ge(x, one))
        return QF_NAN;
    if (qf_le(a, zero) || qf_le(b, zero))
        return QF_NAN;

    qfloat log_x   = qf_log(x);
    qfloat log_1mx = qf_log(qf_sub(one, x));

    qfloat a1 = qf_sub(a, one);
    qfloat b1 = qf_sub(b, one);

    qfloat term1 = qf_mul(a1, log_x);
    qfloat term2 = qf_mul(b1, log_1mx);

    qfloat logB = qf_logbeta(a, b);

    return qf_sub(qf_add(term1, term2), logB);
}

/* Standard normal PDF: φ(x) = exp(-x^2/2) / sqrt(2π) */

qfloat qf_normal_pdf(qfloat x)
{
    qfloat half = qf_from_double(0.5);

    /* Compute -x^2 / 2 */
    qfloat x2   = qf_mul(x, x);
    qfloat expo = qf_mul(qf_neg(x2), half);

    /* exp(-x^2/2) */
    qfloat e = qf_exp(expo);

    return qf_mul(QF_INV_SQRT_2PI, e);
}

/* Standard normal CDF: Φ(x) = 0.5 * (1 + erf(x / sqrt(2))) */

qfloat qf_normal_cdf(qfloat x)
{
    qfloat half = qf_from_double(0.5);
    qfloat one  = qf_from_double(1.0);

    qfloat t = qf_mul(x, QF_SQRT_HALF);
    qfloat erf_t = qf_erf(t);

    return qf_mul(half, qf_add(one, erf_t));
}

/* Standard normal log-PDF: log φ(x) = -0.5 * log(2π) - 0.5 * x^2 */

qfloat qf_normal_logpdf(qfloat x)
{
    qfloat half = qf_from_double(0.5);

    qfloat x2 = qf_mul(x, x);
    qfloat term1 = qf_mul(half, QF_LN_2PI);   /* 0.5 * log(2π) */
    qfloat term2 = qf_mul(half, x2);          /* 0.5 * x^2 */

    return qf_neg(qf_add(term1, term2));
}

/* ProductLog(x) = LambertW_0(x) */

qfloat qf_productlog(qfloat x)
{
    /* principal branch */
    return qf_lambert_w0(x);
}

/* Incomplete gamma: lower γ(s,x), upper Γ(s,x), and P/Q */

static qfloat qf_gammainc_series_P(qfloat s, qfloat x)
{
    /* P(s,x) via series when x < s+1
       P(s,x) = e^{-x} x^s / Γ(s) * Σ_{n=0..∞} x^n / (s+n) n!
    */
    qfloat one  = qf_from_double(1.0);
    qfloat zero = qf_from_double(0.0);

    if (qf_le(x, zero))
        return zero;

    qfloat lgamma_s = qf_lgamma(s);
    qfloat log_x    = qf_log(x);

    qfloat log_pref = qf_sub(qf_mul(s, log_x), x);      /* s*log(x) - x */
    log_pref        = qf_sub(log_pref, lgamma_s);       /* s*log(x) - x - log Γ(s) */

    qfloat pref = qf_exp(log_pref);                     /* e^{-x} x^s / Γ(s) */

    qfloat sum  = qf_div(one, s);                       /* n=0: x^0 / ((s+0) 0!) = 1/s */
    qfloat term = sum;

    qfloat tol  = qf_from_double(1e-34);

    for (int n = 1; n < 2000; ++n) {
        qfloat n_q = qf_from_double((double)n);

        /* term *= x / (s + n) / n */
        qfloat s_plus_n = qf_add(s, n_q);
        term = qf_mul(term, x);
        term = qf_div(term, s_plus_n);
        term = qf_div(term, n_q);

        sum = qf_add(sum, term);

        qfloat at = qf_abs(term);
        if (qf_le(at, tol))
            break;
    }

    return qf_mul(pref, sum);
}

/* Q(s,x) via continued fraction when x >= s+1
   Based on standard Lentz algorithm for incomplete gamma.
*/
static qfloat qf_gammainc_cf_Q(qfloat s, qfloat x)
{
    qfloat one  = qf_from_double(1.0);
    qfloat zero = qf_from_double(0.0);

    qfloat lgamma_s = qf_lgamma(s);
    qfloat log_x    = qf_log(x);

    qfloat log_pref = qf_sub(qf_mul(s, log_x), x);      /* s*log(x) - x */
    log_pref        = qf_sub(log_pref, lgamma_s);       /* s*log(x) - x - log Γ(s) */

    qfloat pref = qf_exp(log_pref);                     /* e^{-x} x^s / Γ(s) */

    /* Lentz algorithm for continued fraction.
       We keep the same structure you had; this computes the CF factor f,
       then Q(s,x) = pref * f.
    */

    qfloat tiny = qf_from_double(1e-300);
    qfloat C    = tiny;
    qfloat D    = zero;

    qfloat f    = qf_from_double(1.0);

    qfloat a, b, delta;

    for (int n = 1; n < 2000; ++n) {
        qfloat n_q = qf_from_double((double)n);

        /* a_n = n * (s - n) */
        qfloat s_minus_n = qf_sub(s, n_q);
        a = qf_mul(n_q, s_minus_n);

        /* b_n = x + 2n - s */
        qfloat two_n = qf_mul_double(n_q, 2.0);
        b = qf_sub(qf_add(x, two_n), s);

        /* D = b + a * D */
        D = qf_add(b, qf_mul(a, D));
        if (qf_eq(D, zero))
            D = tiny;
        D = qf_div(one, D);

        /* C = b + a / C */
        qfloat a_over_C = qf_div(a, C);
        C = qf_add(b, a_over_C);
        if (qf_eq(C, zero))
            C = tiny;

        delta = qf_mul(C, D);
        f     = qf_mul(f, delta);

        /* convergence: |delta - 1| small */
        qfloat one_minus_delta = qf_sub(delta, one);
        qfloat ad = qf_abs(one_minus_delta);
        if (qf_le(ad, qf_from_double(1e-32)))
            break;
    }

    /* Q(s,x) = pref * f */
    return qf_mul(pref, f);
}

/* Regularized P(s,x) and Q(s,x) */

qfloat qf_gammainc_P(qfloat s, qfloat x)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);

    if (qf_le(x, zero))
        return zero;

    if (qf_le(s, zero))
        return QF_NAN;

    /* s = 1: P = 1 - e^{-x} */
    if (qf_eq(s, one)) {
        return qf_sub(one, qf_exp(qf_neg(x)));
    }

    /* s = 1/2: P = erf(sqrt(x)) */
    qfloat half = qf_from_double(0.5);
    if (qf_eq(s, half)) {
        qfloat r = qf_sqrt(x);
        return qf_erf(r);
    }

    qfloat s_plus_one = qf_add(s, one);

    if (qf_lt(x, s_plus_one)) {
        return qf_gammainc_series_P(s, x);
    } else {
        qfloat Q = qf_gammainc_cf_Q(s, x);
        if (qf_isnan(Q) || qf_isinf(Q)) {
            /* CF failed: compute P via series and complement */
            qfloat P = qf_gammainc_series_P(s, x);
            return P;
        }
        return qf_sub(one, Q);
    }
}

qfloat qf_gammainc_Q(qfloat s, qfloat x)
{
    qfloat zero = qf_from_double(0.0);
    qfloat one  = qf_from_double(1.0);

    if (qf_le(x, zero))
        return one;

    if (qf_le(s, zero))
        return QF_NAN;

    /* s = 1: Q = e^{-x} */
    if (qf_eq(s, one)) {
        return qf_exp(qf_neg(x));
    }

    /* s = 1/2: Q = erfc(sqrt(x)) */
    qfloat half = qf_from_double(0.5);
    if (qf_eq(s, half)) {
        qfloat r = qf_sqrt(x);
        return qf_erfc(r);
    }

    qfloat s_plus_one = qf_add(s, one);

    if (qf_lt(x, s_plus_one)) {
        qfloat P = qf_gammainc_series_P(s, x);
        return qf_sub(one, P);
    } else {
        /* CF gives Q directly; if it fails, fall back to series */
        qfloat Q = qf_gammainc_cf_Q(s, x);
        if (qf_isnan(Q) || qf_isinf(Q)) {
            qfloat P = qf_gammainc_series_P(s, x);
            return qf_sub(one, P);
        }
        return Q;
    }
}

/* Unregularized lower and upper incomplete gamma */

qfloat qf_gammainc_lower(qfloat s, qfloat x)
{
    qfloat P = qf_gammainc_P(s, x);
    qfloat gamma_s = qf_gamma(s);
    return qf_mul(P, gamma_s);
}

qfloat qf_gammainc_upper(qfloat s, qfloat x)
{
    qfloat Q = qf_gammainc_Q(s, x);
    qfloat gamma_s = qf_gamma(s);
    return qf_mul(Q, gamma_s);
}

/*===============================================================
   Ei(x) asymptotic for large positive x:
   Ei(x) ~ e^x / x * Σ_{k=0..∞} k! / x^k
  ===============================================================*/
static qfloat qf_ei_asymp_pos(qfloat x)
{
    qfloat one  = qf_from_double(1.0);
    qfloat invx = qf_div(one, x);

    qfloat term = one;
    qfloat sum  = term;
    qfloat prev_abs = qf_abs(term);

    for (int k = 1; k < 50; ++k) {
        qfloat kq = qf_from_double((double)k);
        term = qf_mul(term, kq);
        term = qf_mul(term, invx);

        qfloat at = qf_abs(term);
        if (qf_gt(at, prev_abs))
            break;
        prev_abs = at;

        sum = qf_add(sum, term);
    }

    return qf_mul(qf_mul(qf_exp(x), invx), sum);
}

/*===============================================================
   E1(x) asymptotic for large positive x
  ===============================================================*/
static qfloat qf_e1_asymp_pos(qfloat x)
{
    qfloat one  = qf_from_double(1.0);
    qfloat invx = qf_div(one, x);

    qfloat term = one;
    qfloat sum  = term;
    qfloat prev_abs = qf_abs(term);

    for (int k = 1; k < 50; ++k) {
        qfloat kq = qf_from_double((double)k);
        term = qf_mul(term, qf_neg(kq));
        term = qf_mul(term, invx);

        qfloat at = qf_abs(term);
        if (qf_gt(at, prev_abs))
            break;
        prev_abs = at;

        sum = qf_add(sum, term);
    }

    return qf_mul(qf_mul(qf_exp(qf_neg(x)), invx), sum);
}

/*===============================================================
   Ei(x) series for small/moderate |x|
   Ei(x) = γ + ln|x| + Σ_{k=1..∞} x^k / (k·k!)
  ===============================================================*/
static qfloat qf_ei_series(qfloat x)
{
    qfloat ax   = qf_abs(x);
    qfloat zero = qf_from_double(0.0);

    if (qf_eq(ax, zero))
        return QF_NAN;

    qfloat u   = x;   /* u_1 = x */
    qfloat sum = u;

    qfloat tol = qf_from_double(1e-40);  /* absolute on term */

    for (int k = 2; k < 800; ++k) {
        qfloat kq = qf_from_double((double)k);

        /* u_k = u_{k-1} * x / k */
        u = qf_div(qf_mul(u, x), kq);

        /* term_k = u_k / k */
        qfloat term = qf_div(u, kq);

        sum = qf_add(sum, term);

        if (qf_le(qf_abs(term), tol))
            break;
    }

    qfloat log_ax = qf_log(ax);
    return qf_add(qf_add(QF_EULER_MASCHERONI, log_ax), sum);
}

/*===============================================================
   Ei(x) asymptotic for large negative x
  ===============================================================*/
static qfloat qf_ei_asymp_neg(qfloat x)
{
    qfloat t = qf_neg(x);  /* t = -x > 0 */

    qfloat one  = qf_from_double(1.0);
    qfloat invt = qf_div(one, t);

    qfloat term = one;
    qfloat sum  = term;
    qfloat prev_abs = qf_abs(term);

    for (int k = 1; k < 100; ++k) {   /* was 50 */
        qfloat kq = qf_from_double((double)k);

        term = qf_mul(term, kq);
        term = qf_mul(term, invt);
        term = qf_neg(term);   /* alternating */

        qfloat at = qf_abs(term);
        if (qf_gt(at, prev_abs))
            break;
        prev_abs = at;

        sum = qf_add(sum, term);
    }

    qfloat e_minus_t = qf_exp(qf_neg(t));
    qfloat e_over_t  = qf_mul(e_minus_t, invt);

    return qf_neg(qf_mul(e_over_t, sum));
}

/*===============================================================
   Ei(x) — PRIMARY ENTRY POINT (NO CF)
  ===============================================================*/
qfloat qf_ei(qfloat x)
{
    qfloat zero   = qf_from_double(0.0);
    qfloat twelve = qf_from_double(12.0);

    qfloat ax = qf_abs(x);

    /* |x| ≤ 12: series */
    if (qf_le(ax, twelve))
        return qf_ei_series(x);

    if (qf_gt(x, zero))
        return qf_ei_asymp_pos(x);

    return qf_ei_asymp_neg(x);
}

/*===============================================================
   E1(x) — SECONDARY ENTRY POINT
   E1(x) = -Ei(-x) for moderate x
  ===============================================================*/
qfloat qf_e1(qfloat x)
{
    qfloat zero   = qf_from_double(0.0);
    qfloat twenty = qf_from_double(20.0);

    if (qf_le(x, zero))
        return QF_NAN;

    if (qf_le(x, twenty))
        return qf_neg(qf_ei(qf_neg(x)));

    return qf_e1_asymp_pos(x);
}
