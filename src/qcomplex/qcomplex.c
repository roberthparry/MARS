/**
 * @file qcomplex.c
 * @brief Double-double complex arithmetic and mathematical functions using qfloat_t precision.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "qcomplex.h"

/* ============================================================
   Constants
   ============================================================ */
   
const qcomplex_t QC_ZERO = {
    .re = { .hi = 0.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_ONE = {
    .re = { .hi = 1.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

/* Helpers: construct purely real qcomplex_t from a double literal or qfloat_t. */
static inline qcomplex_t qcr(double x)    { return qc_make(qf_from_double(x),  qf_from_double(0.0)); }
static inline qcomplex_t qcrf(qfloat_t x) { return qc_make(x, qf_from_double(0.0)); }

static qcomplex_t qc_pow_int(qcomplex_t z, int n)
{
    qcomplex_t result = QC_ONE;
    qcomplex_t base = z;

    if (n == 0)
        return QC_ONE;

    if (n < 0)
        return qc_div(QC_ONE, qc_pow_int(z, -n));

    while (n > 0) {
        if (n & 1)
            result = qc_mul(result, base);
        base = qc_mul(base, base);
        n >>= 1;
    }

    return result;
}

// Basic arithmetic
qcomplex_t qc_add(qcomplex_t a, qcomplex_t b) {
    return qc_make(qf_add(a.re, b.re), qf_add(a.im, b.im));
}
qcomplex_t qc_sub(qcomplex_t a, qcomplex_t b) {
    return qc_make(qf_sub(a.re, b.re), qf_sub(a.im, b.im));
}
qcomplex_t qc_neg(qcomplex_t a) {
    return qc_make(qf_neg(a.re), qf_neg(a.im));
}
qcomplex_t qc_conj(qcomplex_t a) {
    return qc_make(a.re, qf_neg(a.im));
}
qcomplex_t qc_mul(qcomplex_t a, qcomplex_t b) {
    qfloat_t re = qf_sub(qf_mul(a.re, b.re), qf_mul(a.im, b.im));
    qfloat_t im = qf_add(qf_mul(a.re, b.im), qf_mul(a.im, b.re));
    return qc_make(re, im);
}
qcomplex_t qc_div(qcomplex_t a, qcomplex_t b) {
    qfloat_t denom = qf_add(qf_mul(b.re, b.re), qf_mul(b.im, b.im));
    qfloat_t re = qf_div(qf_add(qf_mul(a.re, b.re), qf_mul(a.im, b.im)), denom);
    qfloat_t im = qf_div(qf_sub(qf_mul(a.im, b.re), qf_mul(a.re, b.im)), denom);
    return qc_make(re, im);
}

// Magnitude and argument
qfloat_t qc_abs(qcomplex_t z) {
    return qf_hypot(z.re, z.im);
}
qfloat_t qc_arg(qcomplex_t z) {
    return qf_atan2(z.im, z.re);
}

qcomplex_t qc_from_polar(qfloat_t r, qfloat_t theta) {
    return qc_make(qf_mul(r, qf_cos(theta)), qf_mul(r, qf_sin(theta)));
}
void qc_to_polar(qcomplex_t z, qfloat_t *r, qfloat_t *theta) {
    *r     = qc_abs(z);
    *theta = qc_arg(z);
}

// Elementary functions
qcomplex_t qc_exp(qcomplex_t z) {
    qfloat_t exp_re = qf_exp(z.re);
    return qc_make(qf_mul(exp_re, qf_cos(z.im)), qf_mul(exp_re, qf_sin(z.im)));
}
qcomplex_t qc_log(qcomplex_t z) {
    return qc_make(qf_log(qc_abs(z)), qc_arg(z));
}
qcomplex_t qc_pow(qcomplex_t a, qcomplex_t b) {
    if (qc_eq(b, QC_ZERO))
        return QC_ONE;

    if (qc_eq(a, QC_ONE))
        return QC_ONE;

    if (qc_eq(b, QC_ONE))
        return a;

    if (qf_eq(a.im, QF_ZERO) && qf_eq(b.im, QF_ZERO))
        return qcrf(qf_pow(a.re, b.re));

    if (qf_eq(b.im, QF_ZERO)) {
        double yd = qf_to_double(b.re);
        double yi = nearbyint(yd);

        if (fabs(yd - yi) <= 1e-30)
            return qc_pow_int(a, (int)yi);
    }

    return qc_exp(qc_mul(b, qc_log(a)));
}
qcomplex_t qc_sqrt(qcomplex_t z) {
    qfloat_t r          = qc_abs(z);
    qfloat_t sqrt_r     = qf_sqrt(r);
    qfloat_t half_theta = qf_ldexp(qc_arg(z), -1);
    return qc_make(qf_mul(sqrt_r, qf_cos(half_theta)),
                   qf_mul(sqrt_r, qf_sin(half_theta)));
}

// Trigonometric
qcomplex_t qc_sin(qcomplex_t z) {
    // sin(z) = sin(x)cosh(y) + i cos(x)sinh(y)
    return qc_make(qf_mul(qf_sin(z.re), qf_cosh(z.im)),
                   qf_mul(qf_cos(z.re), qf_sinh(z.im)));
}
qcomplex_t qc_cos(qcomplex_t z) {
    // cos(z) = cos(x)cosh(y) - i sin(x)sinh(y)
    return qc_make(qf_mul(qf_cos(z.re), qf_cosh(z.im)),
                   qf_neg(qf_mul(qf_sin(z.re), qf_sinh(z.im))));
}
qcomplex_t qc_tan(qcomplex_t z) {
    return qc_div(qc_sin(z), qc_cos(z));
}
qcomplex_t qc_asin(qcomplex_t z) {
    // asin(z) = -i log(iz + sqrt(1-z^2))
    // -i*(a+bi) = b - ai, so re=ln.im, im=-ln.re
    qcomplex_t iz        = qc_make(qf_neg(z.im), z.re);
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(qcr(1.0), qc_mul(z, z)));
    qcomplex_t ln        = qc_log(qc_add(iz, sqrt_term));
    return qc_make(ln.im, qf_neg(ln.re));
}
qcomplex_t qc_acos(qcomplex_t z) {
    // acos(z) = -i log(z + i sqrt(1-z^2))
    // -i*(a+bi) = b - ai, so re=ln.im, im=-ln.re
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(qcr(1.0), qc_mul(z, z)));
    qcomplex_t iz        = qc_make(qf_neg(sqrt_term.im), sqrt_term.re);
    qcomplex_t ln        = qc_log(qc_add(z, iz));
    return qc_make(ln.im, qf_neg(ln.re));
}
qcomplex_t qc_atan(qcomplex_t z) {
    // atan(z) = (i/2) * [log(1 - iz) - log(1 + iz)]
    // (i/2)*(a+bi) = -b/2 + (a/2)i, so re=-diff.im/2, im=diff.re/2
    qcomplex_t iz   = qc_make(qf_neg(z.im), z.re);
    qcomplex_t diff = qc_sub(qc_log(qc_sub(qcr(1.0), iz)),
                             qc_log(qc_add(qcr(1.0), iz)));
    return qc_make(qf_mul_double(qf_neg(diff.im), 0.5),
                   qf_mul_double(diff.re, 0.5));
}
qcomplex_t qc_atan2(qcomplex_t y, qcomplex_t x) {
    if (qf_eq(y.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_atan2(y.re, x.re));
    return qc_atan(qc_div(y, x));
}

// Hyperbolic
qcomplex_t qc_sinh(qcomplex_t z) {
    // sinh(z) = sinh(x)cos(y) + i cosh(x)sin(y)
    return qc_make(qf_mul(qf_sinh(z.re), qf_cos(z.im)),
                   qf_mul(qf_cosh(z.re), qf_sin(z.im)));
}
qcomplex_t qc_cosh(qcomplex_t z) {
    // cosh(z) = cosh(x)cos(y) + i sinh(x)sin(y)
    return qc_make(qf_mul(qf_cosh(z.re), qf_cos(z.im)),
                   qf_mul(qf_sinh(z.re), qf_sin(z.im)));
}
qcomplex_t qc_tanh(qcomplex_t z) {
    return qc_div(qc_sinh(z), qc_cosh(z));
}
qcomplex_t qc_asinh(qcomplex_t z) {
    // asinh(z) = log(z + sqrt(z^2 + 1))
    return qc_log(qc_add(z, qc_sqrt(qc_add(qc_mul(z, z), qcr(1.0)))));
}
qcomplex_t qc_acosh(qcomplex_t z) {
    // acosh(z) = log(z + sqrt(z+1) * sqrt(z-1))
    return qc_log(qc_add(z, qc_mul(qc_sqrt(qc_add(z, qcr(1.0))),
                                   qc_sqrt(qc_sub(z, qcr(1.0))))));
}
qcomplex_t qc_atanh(qcomplex_t z) {
    // atanh(z) = 0.5 * log((1+z)/(1-z))
    return qc_ldexp(qc_log(qc_div(qc_add(qcr(1.0), z),
                                  qc_sub(qcr(1.0), z))), -1);
}

// Utility
qcomplex_t qc_ldexp(qcomplex_t z, int k) {
    return qc_make(qf_ldexp(z.re, k), qf_ldexp(z.im, k));
}
qcomplex_t qc_floor(qcomplex_t z) {
    return qc_make(qf_floor(z.re), qf_floor(z.im));
}
qcomplex_t qc_hypot(qcomplex_t x, qcomplex_t y) {
    // Not standard for complex; defined as sqrt(|x|^2 + |y|^2)
    return qcrf(qf_hypot(qc_abs(x), qc_abs(y)));
}

// Comparison
bool qc_eq(qcomplex_t a, qcomplex_t b) {
    return qf_eq(a.re, b.re) && qf_eq(a.im, b.im);
}
bool qc_isnan(qcomplex_t z) {
    return qf_isnan(z.re) || qf_isnan(z.im);
}
bool qc_isinf(qcomplex_t z) {
    return qf_isinf(z.re) || qf_isinf(z.im);
}
bool qc_isposinf(qcomplex_t z) {
    return qf_isposinf(z.re) || qf_isposinf(z.im);
}
bool qc_isneginf(qcomplex_t z) {
    return qf_isneginf(z.re) || qf_isneginf(z.im);
}

// Printing
void qc_to_string(qcomplex_t z, char *out, size_t out_size) {
    if (qf_cmp(z.im, (qfloat_t){0.0, 0.0}) < 0) {
        qf_sprintf(out, out_size, "%q - %qi", z.re, qf_neg(z.im));
    } else {
        qf_sprintf(out, out_size, "%q + %qi", z.re, z.im);
    }
}

static qcomplex_t qc_parse_fail(const char *msg, const char *input)
{
    fprintf(stderr, "qc_from_string: %s: \"%s\"\n", msg, input);
    return qc_make(QF_NAN, QF_NAN);
}

qcomplex_t qc_from_string(const char *s)
{
    const char *s_original = s;

    /* Remove whitespace */
    char buf[256];
    size_t n = 0;
    while (*s && n < sizeof(buf)-1) {
        if (!isspace((unsigned char)*s))
            buf[n++] = *s;
        s++;
    }
    buf[n] = '\0';

    if (n == 0)
        return qc_parse_fail("empty input", s_original);

    /* ------------------------------------------------------------
       (a,b) tuple
       ------------------------------------------------------------ */
    if (buf[0] == '(') {
        char *comma = strchr(buf, ',');
        char *end   = strrchr(buf, ')');
        if (!(comma && end && comma > buf && comma < end))
            return qc_parse_fail("invalid (a,b) form", s_original);

        *comma = '\0';
        *end   = '\0';

        qfloat_t re = qf_from_string(buf+1);
        qfloat_t im = qf_from_string(comma+1);
        if (qf_isnan(re) || qf_isnan(im))
            return qc_parse_fail("invalid numbers in (a,b)", s_original);

        return qc_make(re, im);
    }

    /* ------------------------------------------------------------
       r*exp(...)
       ------------------------------------------------------------ */
    char *exp_ptr = strstr(buf, "exp(");
    if (exp_ptr) {
        char *star = strchr(buf, '*');
        if (!(star && star < exp_ptr))
            return qc_parse_fail("invalid r*exp(...) form", s_original);

        *star = '\0';
        qfloat_t r = qf_from_string(buf);
        if (qf_isnan(r))
            return qc_parse_fail("invalid r in r*exp(...)", s_original);

        char *open  = strchr(star+1, '(');
        char *close = strrchr(star+1, ')');
        if (!(open && close && close > open))
            return qc_parse_fail("invalid exp(...) contents", s_original);

        *close = '\0';
        const char *inside = open + 1;
        size_t L = strlen(inside);

        /* --------------------------------------------------------
           FIRST: exp(a+bi) or exp(a-bi)
           -------------------------------------------------------- */
        {
            int signpos = -1;
            for (int i = 1; inside[i]; i++) {
                if (inside[i] == '+' || inside[i] == '-') {
                    signpos = i;
                    break;
                }
            }

            if (signpos > 0) {
                char left[128], right[128];
                memcpy(left, inside, signpos);
                left[signpos] = '\0';
                strcpy(right, inside + signpos);

                size_t RL = strlen(right);
                if (RL < 2 || right[RL-1] != 'i')
                    return qc_parse_fail("invalid imaginary part in exp(a+bi)", s_original);

                right[RL-1] = '\0';

                qfloat_t a = qf_from_string(left);
                qfloat_t b = qf_from_string(right);
                if (qf_isnan(a) || qf_isnan(b))
                    return qc_parse_fail("invalid numbers in exp(a+bi)", s_original);

                /* Correct exp(a+bi) */
                qfloat_t ea = qf_exp(a);
                qfloat_t cb = qf_cos(b);
                qfloat_t sb = qf_sin(b);

                qcomplex_t e = qc_make(qf_mul(ea, cb),
                                       qf_mul(ea, sb));

                return qc_mul(qc_make(r, qf_from_double(0.0)), e);
            }
        }

        /* --------------------------------------------------------
           SECOND: exp(theta i)
           -------------------------------------------------------- */
        if (L > 1 && inside[L-1] == 'i') {
            char tmp[256];
            memcpy(tmp, inside, L-1);
            tmp[L-1] = '\0';

            qfloat_t theta = qf_from_string(tmp);
            if (qf_isnan(theta))
                return qc_parse_fail("invalid angle in exp(theta i)", s_original);

            qfloat_t re = qf_mul(r, qf_cos(theta));
            qfloat_t im = qf_mul(r, qf_sin(theta));
            return qc_make(re, im);
        }

        return qc_parse_fail("invalid exp(...) form", s_original);
    }

    /* ------------------------------------------------------------
       a ± bi
       ------------------------------------------------------------ */
    int split = -1;
    for (int i = (int)n - 1; i > 0; i--) {
        if (buf[i] == '+' || buf[i] == '-') {
            split = i;
            break;
        }
    }

    if (split > 0) {
        char left[256], right[256];
        memcpy(left, buf, split);
        left[split] = '\0';
        strcpy(right, buf + split);

        size_t RL = strlen(right);
        if (RL > 0 && (right[RL-1] == 'i' || right[RL-1] == 'j')) {
            right[RL-1] = '\0';

            if (strcmp(right, "+") == 0) strcpy(right, "+1");
            if (strcmp(right, "-") == 0) strcpy(right, "-1");

            qfloat_t re = qf_from_string(left);
            qfloat_t im = qf_from_string(right);
            if (qf_isnan(re) || qf_isnan(im))
                return qc_parse_fail("invalid numbers in a±bi", s_original);

            return qc_make(re, im);
        }

        if (split > 0 && (left[split - 1] == 'i' || left[split - 1] == 'j')) {
            left[split - 1] = '\0';

            if (strcmp(left, "") == 0)  strcpy(left, "1");
            if (strcmp(left, "+") == 0) strcpy(left, "1");
            if (strcmp(left, "-") == 0) strcpy(left, "-1");

            qfloat_t im = qf_from_string(left);
            qfloat_t re = qf_from_string(right);
            if (qf_isnan(re) || qf_isnan(im))
                return qc_parse_fail("invalid numbers in bi±a", s_original);

            return qc_make(re, im);
        }
    }

    /* ------------------------------------------------------------
       Pure imaginary
       ------------------------------------------------------------ */
    size_t L2 = strlen(buf);
    if (L2 > 0 && (buf[L2-1] == 'i' || buf[L2-1] == 'j')) {
        char tmp[256];
        memcpy(tmp, buf, L2-1);
        tmp[L2-1] = '\0';

        if (strcmp(tmp, "") == 0)  strcpy(tmp, "1");
        if (strcmp(tmp, "+") == 0) strcpy(tmp, "1");
        if (strcmp(tmp, "-") == 0) strcpy(tmp, "-1");

        qfloat_t im = qf_from_string(tmp);
        if (qf_isnan(im))
            return qc_parse_fail("invalid imaginary number", s_original);

        return qc_make(qf_from_double(0.0), im);
    }

    /* ------------------------------------------------------------
       Pure real
       ------------------------------------------------------------ */
    qfloat_t re = qf_from_string(buf);
    if (qf_isnan(re))
        return qc_parse_fail("invalid real number", s_original);

    return qc_make(re, qf_from_double(0.0));
}

/* ------------------------------------------------------------------ */
/*  qc_vsprintf / qc_sprintf / qc_printf                               */
/* ------------------------------------------------------------------ */

static inline void qc_put_char(char **dst, size_t *remaining, size_t *count, char c) {
    if (*remaining > 1 && *dst) {
        **dst = c;
        (*dst)++;
        (*remaining)--;
    }
    (*count)++;
}

static inline void qc_put_str(char **dst, size_t *remaining, size_t *count, const char *s) {
    while (*s) {
        qc_put_char(dst, remaining, count, *s++);
    }
}

static void qc_pad_string(char *out, size_t out_size,
                           const char *s, int width,
                           int flag_minus, int flag_zero)
{
    size_t len = strlen(s);
    int pad = (width > (int)len) ? (width - (int)len) : 0;
    size_t pos = 0;
    if (flag_minus) {
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
        for (int i = 0; i < pad && pos + 1 < out_size; i++) out[pos++] = ' ';
    } else if (flag_zero) {
        for (int j = 0; j < pad && pos + 1 < out_size; j++) out[pos++] = '0';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
    } else {
        for (int i = 0; i < pad && pos + 1 < out_size; i++) out[pos++] = ' ';
        for (size_t i = 0; s[i] && pos + 1 < out_size; i++) out[pos++] = s[i];
    }
    out[pos] = '\0';
}

/* Ensure a qfloat string has exactly <precision> digits after the decimal.
   Works for both fixed (%q) and scientific (%Q) formats. */
static void qc_fix_precision(char *s, int precision)
{
    if (precision < 0) return;

    char *exp = strchr(s, 'e');
    if (!exp) exp = strchr(s, 'E');

    char saved_exp[64] = {0};
    if (exp) {
        strcpy(saved_exp, exp);
        *exp = '\0';
    }

    char *dot = strchr(s, '.');
    if (!dot) {
        /* No decimal point → add one */
        size_t len = strlen(s);
        s[len] = '.';
        s[len+1] = '\0';
        dot = s + len;
    }

    char *p = dot + 1;
    int count = 0;

    while (*p && count < precision) {
        p++;
        count++;
    }

    /* Truncate or pad */
    *p = '\0';

    while (count < precision) {
        strcat(s, "0");
        count++;
    }

    /* Restore exponent */
    if (saved_exp[0]) {
        strcat(s, saved_exp);
    }
}

int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
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
            qc_put_char(&dst, &remaining, &count, *p++);
            continue;
        }

        p++; /* skip '%' */

        int flag_plus  = 0;
        int flag_space = 0;
        int flag_minus = 0;
        int flag_zero  = 0;
        int flag_hash  = 0;

        while (1) {
            if      (*p == '+') { flag_plus  = 1; p++; }
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

        /* ============================================================
           %z / %Z  (complex number formatting)
           ============================================================ */
        if (*p == 'z' || *p == 'Z') {
            char spec = *p++;
            qcomplex_t z = va_arg(ap_local, qcomplex_t);

            char re_buf[256], im_buf[256];
            qfloat_t im_abs = qf_signbit(z.im) ? qf_neg(z.im) : z.im;

            /* Build qfloat format: %q or %Q */
            char cfmt[16];
            snprintf(cfmt, sizeof(cfmt), "%%%c", (spec == 'Z') ? 'Q' : 'q');

            /* Format real and imaginary parts */
            qf_sprintf(re_buf, sizeof(re_buf), cfmt, z.re);
            qf_sprintf(im_buf, sizeof(im_buf), cfmt, im_abs);

            /* Enforce precision manually */
            if (precision >= 0) {
                qc_fix_precision(re_buf, precision);
                qc_fix_precision(im_buf, precision);
            }

            /* Lowercase exponent for %Z */
            if (spec == 'Z') {
                for (char *s = re_buf; *s; s++) if (*s == 'E') *s = 'e';
                for (char *s = im_buf; *s; s++) if (*s == 'E') *s = 'e';
            }

            const char *sep = qf_signbit(z.im) ? " - " : " + ";

            char assembled[600];
            snprintf(assembled, sizeof(assembled), "%s%s%si", re_buf, sep, im_buf);

            char padded[600];
            qc_pad_string(padded, sizeof(padded),
                          assembled, width, flag_minus, flag_zero);

            qc_put_str(&dst, &remaining, &count, padded);
            continue;
        }

        /* ============================================================
           %q / %Q (qfloat formatting)
           ============================================================ */
        else if (*p == 'q' || *p == 'Q') {
            char spec = *p++;
            qfloat_t x = va_arg(ap_local, qfloat_t);

            char cfmt[32];
            char *f = cfmt;
            *f++ = '%';
            if (flag_plus)  *f++ = '+';
            if (flag_space) *f++ = ' ';
            if (flag_minus) *f++ = '-';
            if (flag_zero)  *f++ = '0';
            if (flag_hash)  *f++ = '#';
            if (width > 0)  f += sprintf(f, "%d", width);
            if (precision >= 0) { *f++ = '.'; f += sprintf(f, "%d", precision); }
            *f++ = spec;
            *f = '\0';

            char tmp[512];
            qf_sprintf(tmp, sizeof(tmp), cfmt, x);
            qc_put_str(&dst, &remaining, &count, tmp);
            continue;
        }

        /* ============================================================
           Standard C specifiers
           ============================================================ */
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
            if (width > 0)  f += sprintf(f, "%d", width);
            if (precision >= 0) { *f++ = '.'; f += sprintf(f, "%d", precision); }
            *f++ = *p;
            *f = '\0';

            switch (*p) {
                case 'd': case 'i': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'u': case 'o': case 'x': case 'X': {
                    unsigned int v = va_arg(ap_local, unsigned int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'f': case 'e': case 'E': case 'g': case 'G': {
                    double v = va_arg(ap_local, double);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'c': {
                    int v = va_arg(ap_local, int);
                    snprintf(tmp, sizeof(tmp), fmtbuf, (char)v);
                } break;
                case 's': {
                    const char *v = va_arg(ap_local, const char *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case 'p': {
                    void *v = va_arg(ap_local, void *);
                    snprintf(tmp, sizeof(tmp), fmtbuf, v);
                } break;
                case '%': {
                    tmp[0] = '%'; tmp[1] = '\0';
                } break;
                default:
                    snprintf(tmp, sizeof(tmp), "<?>");
                    break;
            }
            p++;
            qc_put_str(&dst, &remaining, &count, tmp);
            continue;
        }
    }

    if (dst && remaining > 0)
        *dst = '\0';

    va_end(ap_local);
    return (int)count;
}

int qc_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = qc_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int qc_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int needed = qc_vsprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return needed;

    char *buf = malloc((size_t)needed + 1);
    if (!buf) return -1;

    va_start(ap, fmt);
    qc_vsprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    fwrite(buf, 1, needed, stdout);
    free(buf);
    return needed;
}

/* ------------------------------------------------------------------ */

// ------------------------------------------------------------

// Faddeeva w(z) via Weideman's rational approximation (N=32 for ~quad precision)
// ------------------------------------------------------------

static qcomplex_t qc_faddeeva(qcomplex_t z)
{
    const int N = 32;
    qfloat_t L   = qf_sqrt(qf_from_double((double)N));
    qfloat_t two = qf_from_double(2.0);

    qcomplex_t sum = qcr(0.0);

    for (int k = 1; k <= N; k++) {
        qfloat_t ak = qf_div(
            qf_mul(qf_from_double((double)(2*k - 1)), QF_PI),
            qf_mul(two, L));
        qfloat_t angle = qf_div(
            qf_mul(qf_from_double((double)(2*k - 1)), QF_PI),
            qf_mul(two, qf_from_double((double)N)));
        qfloat_t ck = qf_div(qf_add(qf_from_double(1.0), qf_cos(angle)), L);

        qcomplex_t denom = qc_make(z.re, qf_sub(z.im, ak));
        sum = qc_add(sum, qc_div(qcrf(ck), denom));
    }

    // inside = 1 + (2i / sqrt(pi)) * sum
    qcomplex_t two_i_sum = qc_mul(qc_make(qf_from_double(0.0), qf_from_double(2.0)), sum);
    qcomplex_t inside    = qc_add(qcr(1.0), qc_div(two_i_sum, qcrf(QF_SQRT_PI)));

    return qc_mul(qc_exp(qc_neg(qc_mul(z, z))), inside);
}

qcomplex_t qc_erf(qcomplex_t z) {
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_erf(z.re));
    /* Faddeeva requires Im(iz) = Re(z) >= 0; use antisymmetry erf(-z) = -erf(z) otherwise */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_neg(qc_erf(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    qcomplex_t w  = qc_faddeeva(iz);
    qcomplex_t e  = qc_exp(qc_neg(qc_mul(z, z)));
    return qc_sub(qcr(1.0), qc_mul(e, w));
}

qcomplex_t qc_erfc(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_erfc(z.re));
    /* Use erfc(-z) = 2 - erfc(z) to keep Re(z) >= 0 for Faddeeva */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_sub(qcr(2.0), qc_erfc(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    qcomplex_t w  = qc_faddeeva(iz);
    qcomplex_t e  = qc_exp(qc_neg(qc_mul(z, z)));
    return qc_mul(e, w);
}

qcomplex_t qc_erfinv(qcomplex_t z)
{
    // Newton iteration: solve erf(w) = z.  Initial guess good for small |z|.
    qcomplex_t w = qc_mul(z, qcrf(qf_div(QF_SQRT_PI, qf_from_double(2.0))));

    for (int i = 0; i < 10; i++) {
        qcomplex_t f  = qc_sub(qc_erf(w), z);
        qcomplex_t fp = qc_mul(qcrf(qf_div(qf_from_double(2.0), QF_SQRT_PI)),
                               qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

qcomplex_t qc_erfcinv(qcomplex_t z)
{
    // Newton iteration: solve erfc(w) = z.  Initial guess via erfinv(1-z).
    qcomplex_t w = qc_erfinv(qc_sub(qcr(1.0), z));

    for (int i = 0; i < 10; i++) {
        qcomplex_t f  = qc_sub(qc_erfc(w), z);
        qcomplex_t fp = qc_mul(qcrf(qf_div(qf_neg(qf_from_double(2.0)), QF_SQRT_PI)),
                               qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

// Lanczos coefficients (g=7, N=9)
static qfloat_t c[] = {
    { 1.00000000000000000e+00, -6.99999999999999971e-34 },
    { 6.76520368121885099e+02, -2.77349972750868088e-16 },
    { -1.25913921672240281e+03, -6.33356109449210117e-14 },
    { 7.71323428777653135e+02, -5.57539348441983815e-14 },
    { -1.76615029162140587e+02, -1.20612308688560953e-14 },
    { 1.25073432786869052e+01, -3.93560260890580655e-16 },
    { -1.38571095265720118e-01, 9.21762572455474946e-19 },
    { 9.98436957801957158e-06, -7.23679346146658305e-22 },
    { 1.50563273514931162e-07, -5.92527880744794851e-24 },
};

static qcomplex_t lanczos_sum(qcomplex_t z_minus_one)
{
    qcomplex_t sum = qcrf(c[0]);
    for (int i = 1; i < 9; i++)
        sum = qc_add(sum, qc_div(qcrf(c[i]),
                                 qc_add(z_minus_one, qcr((double)i))));
    return sum;
}

// ------------------------------------------------------------

// Complex Gamma
// ------------------------------------------------------------

qcomplex_t qc_gamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_gamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: Γ(z) = π / (sin(πz) Γ(1-z))
        qcomplex_t sin_pi_z  = qc_sin(qc_mul(z, qcrf(QF_PI)));
        qcomplex_t gamma_1mz = qc_gamma(qc_sub(qcr(1.0), z));
        return qc_div(qcrf(QF_PI), qc_mul(sin_pi_z, gamma_1mz));
    }

    qcomplex_t z_minus_one = qc_sub(z, qcr(1.0));
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qcr(7.5));  // g + 0.5, g = 7

    qfloat_t sqrt_2pi = qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI));
    return qc_mul(qc_mul(qcrf(sqrt_2pi), qc_pow(t, qc_sub(z, qcr(0.5)))),
                  qc_mul(qc_exp(qc_neg(t)), sum));
}

qcomplex_t qc_lgamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_lgamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: lgamma(z) = log(π) - log(sin(πz)) - lgamma(1-z)
        qcomplex_t log_sin_piz = qc_log(qc_sin(qc_mul(z, qcrf(QF_PI))));
        qcomplex_t lg_1mz      = qc_lgamma(qc_sub(qcr(1.0), z));
        return qc_sub(qc_sub(qcrf(qf_log(QF_PI)), log_sin_piz), lg_1mz);
    }

    qcomplex_t z_minus_one = qc_sub(z, qcr(1.0));
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qcr(7.5));

    qfloat_t sqrt_2pi = qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI));
    return qc_add(
        qc_add(qcrf(qf_log(sqrt_2pi)), qc_mul(qc_sub(z, qcr(0.5)), qc_log(t))),
        qc_add(qc_neg(t), qc_log(sum)));
}

qcomplex_t qc_digamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_digamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ(z) = ψ(1-z) - π cot(πz)
        qcomplex_t pi_z = qc_mul(z, qcrf(QF_PI));
        qcomplex_t cotpz = qc_div(qc_cos(pi_z), qc_sin(pi_z));
        return qc_sub(qc_digamma(qc_sub(qcr(1.0), z)), qc_mul(qcrf(QF_PI), cotpz));
    }

    // Shift upward until |z| >= 10
    qcomplex_t psi = qcr(0.0);
    qcomplex_t zz  = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        psi = qc_sub(psi, qc_div(qcr(1.0), zz));
        zz  = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ(z) ≈ log(z) - 1/(2z) - Σ B_{2k}/(2k z^{2k})
    qcomplex_t invz   = qc_div(qcr(1.0), zz);
    qcomplex_t result = qc_sub(qc_log(zz), qc_mul(qcr(0.5), invz));

    qfloat_t B2  = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4  = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6  = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8  = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B10 = qf_div(qf_from_double( 5.0), qf_from_double(66.0));

    qcomplex_t z2  = qc_mul(invz, invz);
    qcomplex_t z4  = qc_mul(z2, z2);
    qcomplex_t z6  = qc_mul(z4, z2);
    qcomplex_t z8  = qc_mul(z6, z2);
    qcomplex_t z10 = qc_mul(z8, z2);

    result = qc_sub(result, qc_mul(qcrf(B2),  qc_mul(qcr(0.5),      z2)));
    result = qc_sub(result, qc_mul(qcrf(B4),  qc_mul(qcr(0.25),     z4)));
    result = qc_sub(result, qc_mul(qcrf(B6),  qc_mul(qcr(1.0/6.0),  z6)));
    result = qc_sub(result, qc_mul(qcrf(B8),  qc_mul(qcr(0.125),    z8)));
    result = qc_sub(result, qc_mul(qcrf(B10), qc_mul(qcr(0.1),      z10)));

    return qc_add(result, psi);
}

qcomplex_t qc_trigamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_trigamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ₁(z) = π² csc²(πz) - ψ₁(1-z)
        qcomplex_t pi_z  = qc_mul(z, qcrf(QF_PI));
        qcomplex_t csc   = qc_div(qcr(1.0), qc_sin(pi_z));
        qcomplex_t term  = qc_mul(qcrf(qf_mul(QF_PI, QF_PI)), qc_mul(csc, csc));
        return qc_sub(term, qc_trigamma(qc_sub(qcr(1.0), z)));
    }

    // Recurrence upward until |z| >= 10
    qcomplex_t accum = qcr(0.0);
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        qcomplex_t invz = qc_div(qcr(1.0), zz);
        accum = qc_add(accum, qc_mul(invz, invz));
        zz    = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ₁(z) ≈ 1/z + 1/(2z²) + Σ B_{2k}/z^{2k+1}
    qcomplex_t invz  = qc_div(qcr(1.0), zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz, qc_mul(qcr(0.5), invz2));

    qfloat_t B2 = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6 = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));

    qcomplex_t z3 = qc_mul(invz2, invz);
    qcomplex_t z5 = qc_mul(z3, invz2);
    qcomplex_t z7 = qc_mul(z5, invz2);
    qcomplex_t z9 = qc_mul(z7, invz2);

    result = qc_add(result, qc_mul(qcrf(B2), z3));
    result = qc_add(result, qc_mul(qcrf(B4), z5));
    result = qc_add(result, qc_mul(qcrf(B6), z7));
    result = qc_add(result, qc_mul(qcrf(B8), z9));

    return qc_add(result, accum);
}

qcomplex_t qc_tetragamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_tetragamma(z.re));

    if (qf_lt(z.re, qf_from_double(0.5))) {
        // Reflection: ψ₂(z) = ψ₂(1-z) + 2π³ csc²(πz) cot(πz)
        qcomplex_t pi_z   = qc_mul(z, qcrf(QF_PI));
        qcomplex_t sin_pz = qc_sin(pi_z);
        qcomplex_t csc    = qc_div(qcr(1.0), sin_pz);
        qcomplex_t csc2   = qc_mul(csc, csc);
        qcomplex_t cot_pz = qc_div(qc_cos(pi_z), sin_pz);
        qfloat_t pi3 = qf_mul(qf_mul(QF_PI, QF_PI), QF_PI);
        qcomplex_t term = qc_mul(qcrf(qf_mul_double(pi3, 2.0)),
                                 qc_mul(csc2, cot_pz));
        return qc_add(qc_tetragamma(qc_sub(qcr(1.0), z)), term);
    }

    // Recurrence upward until |z| >= 10
    qcomplex_t accum = qcr(0.0);
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        qcomplex_t invz  = qc_div(qcr(1.0), zz);
        qcomplex_t invz3 = qc_mul(invz, qc_mul(invz, invz));
        accum = qc_add(accum, qc_mul(qcr(2.0), invz3));
        zz    = qc_add(zz, qcr(1.0));
    }

    // Asymptotic: ψ₂(z) ≈ 1/z² + 1/z³ + Σ B_{2k}/z^{2k+2}
    qcomplex_t invz  = qc_div(qcr(1.0), zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz2, qc_mul(invz2, invz));

    qfloat_t B2 = qf_div(qf_from_double( 1.0), qf_from_double( 6.0));
    qfloat_t B4 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));
    qfloat_t B6 = qf_div(qf_from_double( 1.0), qf_from_double(42.0));
    qfloat_t B8 = qf_div(qf_from_double(-1.0), qf_from_double(30.0));

    qcomplex_t z4  = qc_mul(invz2, invz2);
    qcomplex_t z6  = qc_mul(z4, invz2);
    qcomplex_t z8  = qc_mul(z6, invz2);
    qcomplex_t z10 = qc_mul(z8, invz2);

    result = qc_add(result, qc_mul(qcrf(B2), z4));
    result = qc_add(result, qc_mul(qcrf(B4), z6));
    result = qc_add(result, qc_mul(qcrf(B6), z8));
    result = qc_add(result, qc_mul(qcrf(B8), z10));

    return qc_add(result, accum);
}

qcomplex_t qc_gammainv(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_gammainv(z.re));

    qcomplex_t w = z;
    for (int i = 0; i < 20; i++) {
        qcomplex_t gw    = qc_gamma(w);
        qcomplex_t delta = qc_div(qc_sub(gw, z), qc_mul(gw, qc_digamma(w)));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_beta(a.re, b.re));

    return qc_div(qc_mul(qc_gamma(a), qc_gamma(b)), qc_gamma(qc_add(a, b)));
}

qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_logbeta(a.re, b.re));

    return qc_sub(qc_add(qc_lgamma(a), qc_lgamma(b)), qc_lgamma(qc_add(a, b)));
}

qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_binomial(a.re, b.re));

    // C(a,b) = Γ(a+1) / (Γ(b+1) Γ(a-b+1))
    qcomplex_t a1   = qc_add(a, qcr(1.0));
    qcomplex_t b1   = qc_add(b, qcr(1.0));
    qcomplex_t amb1 = qc_add(qc_sub(a, b), qcr(1.0));
    return qc_div(qc_gamma(a1), qc_mul(qc_gamma(b1), qc_gamma(amb1)));
}

qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_beta_pdf(x.re, a.re, b.re));

    // f(x; a, b) = x^(a-1) * (1-x)^(b-1) / B(a,b)
    qcomplex_t one_minus_x = qc_sub(qcr(1.0), x);
    qcomplex_t num = qc_mul(qc_pow(x,           qc_sub(a, qcr(1.0))),
                            qc_pow(one_minus_x, qc_sub(b, qcr(1.0))));
    return qc_div(num, qc_beta(a, b));
}

qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qcrf(qf_logbeta_pdf(x.re, a.re, b.re));

    // log f(x; a,b) = (a-1)log(x) + (b-1)log(1-x) - log B(a,b)
    return qc_sub(qc_add(qc_mul(qc_sub(a, qcr(1.0)), qc_log(x)),
                         qc_mul(qc_sub(b, qcr(1.0)), qc_log(qc_sub(qcr(1.0), x)))),
                  qc_logbeta(a, b));
}

qcomplex_t qc_normal_pdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_pdf(z.re));

    // φ(z) = exp(-z²/2) / sqrt(2π)
    qfloat_t sqrt_2pi = qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI));
    return qc_div(qc_exp(qc_mul(qcr(-0.5), qc_mul(z, z))), qcrf(sqrt_2pi));
}

qcomplex_t qc_normal_cdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_cdf(z.re));

    // Φ(z) = 0.5 * (1 + erf(z / sqrt(2)))
    qfloat_t sqrt2 = qf_sqrt(qf_from_double(2.0));
    return qc_mul(qcr(0.5), qc_add(qcr(1.0), qc_erf(qc_div(z, qcrf(sqrt2)))));
}

qcomplex_t qc_normal_logpdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_normal_logpdf(z.re));

    // log φ(z) = -z²/2 - log(2π)/2
    qfloat_t log_2pi = qf_log(qf_mul(qf_from_double(2.0), QF_PI));
    return qc_sub(qc_mul(qcr(-0.5), qc_mul(z, z)),
                  qcrf(qf_mul_double(log_2pi, 0.5)));
}

static qcomplex_t qc_lambert_w_series_guess(qcomplex_t z, int branch)
{
    qcomplex_t ez = qc_mul(qcrf(QF_E), z);
    qcomplex_t p = qc_sqrt(qc_mul(qcr(2.0), qc_add(qcr(1.0), ez)));
    qcomplex_t p2 = qc_mul(p, p);
    qcomplex_t p3 = qc_mul(p2, p);
    qcomplex_t sign_p = (branch == -1) ? qc_neg(p) : p;
    qcomplex_t sign_p3 = (branch == -1) ? qc_neg(p3) : p3;
    qcomplex_t w = qcr(-1.0);

    w = qc_add(w, sign_p);
    w = qc_sub(w, qc_mul(qcr(1.0 / 3.0), p2));
    w = qc_add(w, qc_mul(qcr(11.0 / 72.0), sign_p3));
    return w;
}

static qcomplex_t qc_lambert_w_asymptotic_guess(qcomplex_t z, int branch)
{
    qfloat_t two_pi = qf_mul_double(QF_PI, 2.0);
    qcomplex_t L1 = qc_log(z);
    L1.im = qf_add(L1.im, qf_mul_double(two_pi, (double)branch));

    if (qc_abs(L1).hi == 0.0)
        return L1;

    qcomplex_t L2 = qc_log(L1);
    return qc_add(qc_sub(L1, L2), qc_div(L2, L1));
}

static qcomplex_t qc_lambert_wm1_complex(qcomplex_t z)
{
    qfloat_t zero_tol = qf_from_double(1e-30);

    if (qc_isnan(z))
        return qc_make(QF_NAN, QF_NAN);

    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_lambert_wm1(z.re));

    if (qf_eq(z.re, QF_ZERO) && qf_eq(z.im, QF_ZERO))
        return qc_make(QF_NINF, QF_NAN);

    qcomplex_t branch_probe = qc_add(qc_mul(qcrf(QF_E), z), qcr(1.0));
    qcomplex_t w = qf_lt(qc_abs(branch_probe), qf_from_double(0.25))
        ? qc_lambert_w_series_guess(z, -1)
        : qc_lambert_w_asymptotic_guess(z, -1);

    for (int i = 0; i < 60; i++) {
        qcomplex_t ew = qc_exp(w);
        qcomplex_t wew = qc_mul(w, ew);
        qcomplex_t f = qc_sub(wew, z);
        qcomplex_t wp1 = qc_add(w, qcr(1.0));
        qcomplex_t denom;

        if (qf_lt(qc_abs(wp1), zero_tol)) {
            denom = qc_mul(ew, qcr(1.0));
        } else {
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, qcr(2.0)), f),
                                            qc_mul(qcr(2.0), wp1));
            denom = qc_sub(qc_mul(ew, wp1), halley_corr);
        }

        qcomplex_t delta = qc_div(f, denom);
        w = qc_sub(w, delta);

        if (qf_lt(qc_abs(delta), zero_tol))
            break;
    }

    return w;
}

qcomplex_t qc_lambert_wm1(qcomplex_t z)
{
    return qc_lambert_wm1_complex(z);
}

qcomplex_t qc_productlog(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_productlog(z.re));

    /* Newton iteration on the principal branch: w e^w = z */
    qcomplex_t w = qc_log(z);
    for (int i = 0; i < 40; i++) {
        qcomplex_t ew    = qc_exp(w);
        qcomplex_t f     = qc_sub(qc_mul(w, ew), z);
        qcomplex_t delta = qc_div(f, qc_mul(ew, qc_add(w, qcr(1.0))));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

static qcomplex_t qc_gammainc_lower_series(qcomplex_t s, qcomplex_t x)
{
    qfloat_t tol = qf_from_double(1e-30);

    qcomplex_t term = qc_div(qcr(1.0), s);
    qcomplex_t sum  = term;

    for (int i = 1; i < 10000; i++) {
        term = qc_mul(term, qc_div(x, qc_add(s, qcr((double)i))));
        sum  = qc_add(sum, term);
        if (qf_lt(qc_abs(term), qf_mul(tol, qc_abs(sum))))
            break;
    }

    return qc_mul(qc_mul(qc_pow(x, s), qc_exp(qc_neg(x))), sum);
}

qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_lower(s.re, x.re));

    return qc_gammainc_lower_series(s, x);
}

qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_upper(s.re, x.re));

    return qc_sub(qc_gamma(s), qc_gammainc_lower_series(s, x));
}

qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_P(s.re, x.re));

    return qc_div(qc_gammainc_lower_series(s, x), qc_gamma(s));
}

qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qcrf(qf_gammainc_Q(s.re, x.re));

    return qc_sub(qcr(1.0), qc_gammainc_P(s, x));
}

qcomplex_t qc_ei(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_ei(z.re));

    // Ei(z) = γ + log(z) + Σ_{k=1}^∞ z^k / (k × k!)
    qfloat_t tol  = qf_from_double(1e-30);
    qcomplex_t sum = qc_add(qcrf(QF_EULER_MASCHERONI), qc_log(z));

    qfloat_t   fact = qf_from_double(1.0);  // fact = k!
    qcomplex_t term = z;                     // term = z^k

    for (int k = 1; k < 10000; k++) {
        qcomplex_t add = qc_div(term, qcrf(qf_mul(qf_from_double((double)k), fact)));
        sum = qc_add(sum, add);
        if (qf_lt(qc_abs(add), qf_mul(tol, qc_abs(sum))))
            break;
        fact = qf_mul(fact, qf_from_double((double)(k + 1)));
        term = qc_mul(term, z);
    }
    return sum;
}

qcomplex_t qc_e1(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qcrf(qf_e1(z.re));

    return qc_neg(qc_ei(qc_neg(z)));
}
