#include "qfloat_internal.h"

#include <math.h>
#include <stdio.h>

static inline qfloat_t qf_scale_pow10(qfloat_t x, int exp10)
{
    static const qfloat_t P10_1   = { 1.00000000000000000e+01, 0.00000000000000000e+00 };
    static const qfloat_t P10_2   = { 1.00000000000000000e+02, 0.00000000000000000e+00 };
    static const qfloat_t P10_4   = { 1.00000000000000000e+04, 0.00000000000000000e+00 };
    static const qfloat_t P10_8   = { 1.00000000000000000e+08, 0.00000000000000000e+00 };
    static const qfloat_t P10_16  = { 1.00000000000000000e+16, 0.00000000000000000e+00 };
    static const qfloat_t P10_32  = { 1.00000000000000010e+32, -5.36616220439347200e+15 };
    static const qfloat_t P10_64  = { 1.00000000000000000e+64, -2.13204190094544240e+47 };
    static const qfloat_t P10_128 = { 1.00000000000000010e+128, -7.51744869165182680e+111 };
    static const qfloat_t P10_256 = { 1.00000000000000000e+256, -3.01276599001406900e+239 };

    qfloat_t r = x;
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

qfloat_t qf_pow10(int e)
{
    return qf_scale_pow10(QF_ONE, e);
}

static inline int qf_iszero(qfloat_t x)
{
    /* Treat any representable zero as zero, regardless of sign.
       This matches Option A: always print "0". */

    return (x.hi == 0.0 && x.lo == 0.0);
}

/* 32-digit decimal parser (no long double) */
qfloat_t qf_from_string(const char *s)
{
    qfloat_t result = QF_ZERO;
    qfloat_t ten    = QF_TEN;

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
        if (exp10 == 308 && qf_eq(result, QF_ONE)) {
            result = (qfloat_t){ 1.0000000000000000e+308, -1.0979063629440455e+291 };
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

static inline int qf_is_negative(qfloat_t x) {
    return (x.hi < 0.0) || (x.hi == 0.0 && x.lo < 0.0);
}

static inline void qf_modf_like(qfloat_t x, qfloat_t *ip, qfloat_t *fp) {
    qfloat_t f = qf_floor(x);
    *ip = f;
    *fp = qf_sub(x, f);
}

static inline qfloat_t qf_div_double(qfloat_t x, double d) {
    return qf_div(x, qf_from_double(d));
}

int qf_decimal_exponent(qfloat_t x)
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
int qf_to_decimal_digits(qfloat_t x, char *digits, int ndigits, int *out_exp10)
{
    /* assume x > 0 and not zero; sign handled outside */

    int exp10 = qf_decimal_exponent(x);
    qfloat_t y  = qf_scale_pow10(x, -exp10);   /* y ≈ [1,10) */

    qfloat_t ten = QF_TEN;

    for (int i = 0; i < ndigits; i++) {

        /* use full qfloat_t for the remainder, but digit from hi */
        int digit = qf_to_int(qf_floor(y));
        if (digit < 0) digit = 0;
        if (digit > 9) digit = 9;

        digits[i] = (char)('0' + digit);

        /* y = (y - digit) * 10, using qfloat_t subtraction */
        qfloat_t d = (qfloat_t){ (double)digit, 0.0 };
        y = qf_sub(y, d);
        y = qf_mul(y, ten);
    }

    if (out_exp10) *out_exp10 = exp10;
    return ndigits;
}

void qf_to_string(qfloat_t x, char *out, size_t out_size)
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
