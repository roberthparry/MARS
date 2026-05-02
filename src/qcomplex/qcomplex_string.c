#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcomplex_internal.h"
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

static int qc_has_imag_suffix(char c)
{
    return c == 'i' || c == 'j';
}

static void qc_normalize_unit_imag(char *text)
{
    if (strcmp(text, "") == 0 || strcmp(text, "+") == 0)
        strcpy(text, "1");
    else if (strcmp(text, "-") == 0)
        strcpy(text, "-1");
}

static int qc_parse_real_imag_parts(const char *re_text,
                                    const char *im_text,
                                    qcomplex_t *out)
{
    qfloat_t re = qf_from_string(re_text);
    qfloat_t im = qf_from_string(im_text);

    if (qf_isnan(re) || qf_isnan(im))
        return -1;

    *out = qc_make(re, im);
    return 0;
}

static int qc_find_signed_split(const char *text)
{
    for (int i = (int)strlen(text) - 1; i > 0; --i) {
        if (text[i] == '+' || text[i] == '-')
            return i;
    }
    return -1;
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

        qcomplex_t z;
        if (qc_parse_real_imag_parts(buf + 1, comma + 1, &z) != 0)
            return qc_parse_fail("invalid numbers in (a,b)", s_original);

        return z;
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
                qcomplex_t e;

                memcpy(left, inside, signpos);
                left[signpos] = '\0';
                strcpy(right, inside + signpos);

                size_t RL = strlen(right);
                if (RL < 2 || !qc_has_imag_suffix(right[RL - 1]))
                    return qc_parse_fail("invalid imaginary part in exp(a+bi)", s_original);

                right[RL - 1] = '\0';
                if (qc_parse_real_imag_parts(left, right, &e) != 0)
                    return qc_parse_fail("invalid numbers in exp(a+bi)", s_original);

                return qc_mul(qcrf(r), qc_exp(e));
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
    int split = qc_find_signed_split(buf);

    if (split > 0) {
        char left[256], right[256];
        qcomplex_t z;

        memcpy(left, buf, split);
        left[split] = '\0';
        strcpy(right, buf + split);

        size_t RL = strlen(right);
        if (RL > 0 && qc_has_imag_suffix(right[RL - 1])) {
            right[RL - 1] = '\0';
            qc_normalize_unit_imag(right);

            if (qc_parse_real_imag_parts(left, right, &z) != 0)
                return qc_parse_fail("invalid numbers in a±bi", s_original);

            return z;
        }

        if (split > 0 && qc_has_imag_suffix(left[split - 1])) {
            left[split - 1] = '\0';
            qc_normalize_unit_imag(left);

            if (qc_parse_real_imag_parts(right, left, &z) != 0)
                return qc_parse_fail("invalid numbers in bi±a", s_original);

            return z;
        }
    }

    /* ------------------------------------------------------------
       Pure imaginary
       ------------------------------------------------------------ */
    size_t L2 = strlen(buf);
    if (L2 > 0 && qc_has_imag_suffix(buf[L2 - 1])) {
        char tmp[256];
        qcomplex_t z;

        memcpy(tmp, buf, L2 - 1);
        tmp[L2 - 1] = '\0';
        qc_normalize_unit_imag(tmp);
        if (qc_parse_real_imag_parts("0", tmp, &z) != 0)
            return qc_parse_fail("invalid imaginary number", s_original);

        return z;
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

