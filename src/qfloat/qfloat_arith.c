#include "qfloat_internal.h"

#include <math.h>

qfloat_t qf_add(qfloat_t a, qfloat_t b) {
    double s, e1, e2;
    qf_two_sum(a.hi, b.hi, &s, &e1);
    e2 = a.lo + b.lo + e1;
    return qf_renorm(s, e2);
}

qfloat_t qf_sub(qfloat_t a, qfloat_t b) {
    double s, e1, e2;
    qf_two_sum(a.hi, -b.hi, &s, &e1);
    e2 = a.lo - b.lo + e1;
    return qf_renorm(s, e2);
}

qfloat_t qf_mul(qfloat_t a, qfloat_t b) {
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
    qf_two_sum(C, c, &hi, &lo);

    return (qfloat_t){ hi, lo };
}

/* Correct double-double division */
qfloat_t qf_div(qfloat_t a, qfloat_t b)
{
    qfloat_t zero = QF_ZERO;

    if (qf_eq(b, zero))
        return QF_NAN;

    double q1 = a.hi / b.hi;
    qfloat_t q1q = qf_from_double(q1);

    qfloat_t qb = qf_mul(q1q, b);
    qfloat_t r  = qf_sub(a, qb);

    double q2 = r.hi / b.hi;
    qfloat_t q2q = qf_from_double(q2);

    return qf_add(q1q, q2q);
}

qfloat_t qf_pow_int(qfloat_t x, int n)
{
    if (n == 0) {
        return QF_ONE;
    }

    if (n < 0) {
        qfloat_t r = qf_pow_int(x, -n);
        return qf_div(QF_ONE, r);
    }

    qfloat_t result = QF_ONE;
    qfloat_t base   = x;

    while (n > 0) {
        if (n & 1)
            result = qf_mul(result, base);
        base = qf_mul(base, base);
        n >>= 1;
    }

    return result;
}

qfloat_t qf_pow(qfloat_t x, qfloat_t y)
{
    /* x == 0 handling */
    if (x.hi == 0.0) {
        double yd = qf_to_double(y);
        if (yd <= 0.0)
            return QF_NAN;
        return QF_ZERO;
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
        qfloat_t ax = x;
        ax.hi = fabs(ax.hi);   /* assuming normalised qfloat_t, this is enough */

        qfloat_t r = qf_pow_int(ax, n);

        /* if n is odd, result is negative */
        if (n & 1) {
            r.hi = -r.hi;
            r.lo = -r.lo;
        }

        return r;
    }

    /* x > 0: general case x^y = exp(y * log(x)) */
    qfloat_t lx = qf_log(x);
    qfloat_t t  = qf_mul(y, lx);
    return qf_exp(t);
}

/* Optional: tiny helper for exact 2^k scaling of a qfloat_t */
qfloat_t qf_ldexp(qfloat_t x, int e)
{
    double s = ldexp(1.0, e);

    qfloat_t r;
    r.hi = x.hi * s;
    r.lo = x.lo * s;
    return r;
}

qfloat_t qf_sqrt(qfloat_t x)
{
    qfloat_t zero = QF_ZERO;

    if (qf_eq(x, zero))
        return zero;

    if (qf_lt(x, zero))
        return QF_NAN;

    double approx = sqrt(x.hi);
    qfloat_t y = qf_from_double(approx);

    /* 3–4 Newton steps: y_{n+1} = 0.5 * (y_n + x / y_n) */
    for (int i = 0; i < 4; ++i) {
        qfloat_t x_over_y = qf_div(x, y);
        qfloat_t sum      = qf_add(y, x_over_y);
        y = qf_mul(QF_HALF, sum);
    }

    return y;
}

qfloat_t qf_add_double(qfloat_t x, double y) {
    double H, h, S, s, e;
	S = y + x.hi;
	e = S - y;
	s = S - e;
	s = (x.hi - e) + (y - s);
	H = S + (s + x.lo);
	h = (s + x.lo) + (S - H);
    qfloat_t r = x;
	r.hi = H + h;
	r.lo = h + (H - r.hi);
    return r;
}

qfloat_t qf_mul_double(qfloat_t x, double a)
{
    double xh, xl, ah, al;
    qf_split_double(x.hi, &xh, &xl);
    qf_split_double(a,    &ah, &al);

    double p = x.hi * a;

    double err = ((xh*ah - p) + xh*al + xl*ah) + xl*al;
    err += x.lo * a;

    qfloat_t r;
    r.hi = p + err;
    r.lo = err - (r.hi - p);
    return r;
}

qfloat_t qf_sqr(qfloat_t x) {
	double hx, tx, C, c;

	C = QF_SPLIT * x.hi;
	hx = C - x.hi;
	hx = C - hx;
	tx = x.hi - hx;
	C = x.hi * x.hi;
	c = ((((hx * hx - C) + 2.0 * hx * tx)) + tx * tx) + 2.0 * x.hi * x.lo;
	hx = C + c;

    qfloat_t r;
    r.hi = hx;
    r.lo = c + (C - hx);

	return r;
}
