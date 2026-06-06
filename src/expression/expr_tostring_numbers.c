#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>

#include "expr_bindings.h"
#include "expr_internal.h"
#include "expr_tostring.h"
#include "expr_tostring_internal.h"
#include "expression.h"
#include "internal/number_internal.h"

static void expr_trim_decimal_display_artifacts_local(char *text);

static bool expr_mpz_factor_out_ulong(mpz_t value, unsigned long factor, size_t *count)
{
    if (!count || factor == 0u)
        return false;

    while (true) {
        if (mpz_cmp_ui(value, 1u) == 0)
            return true;
        if (!mpz_divisible_ui_p(value, factor))
            return true;
        mpz_divexact_ui(value, value, factor);
        ++*count;
    }
}

static void expr_mpz_mul_small_power(mpz_t value, unsigned long factor, size_t exponent)
{
    for (size_t i = 0u; i < exponent; ++i)
        mpz_mul_ui(value, value, factor);
}

static char *expr_decimal_from_scaled_integer(mpz_t scaled, size_t scale)
{
    char *digits = mpz_get_str(NULL, 10, scaled);
    char *out;
    const char *mag;
    bool negative;
    size_t len;
    size_t out_len;
    size_t pos = 0u;

    if (!digits)
        return NULL;

    negative = digits[0] == '-';
    mag = negative ? digits + 1 : digits;
    len = strlen(mag);

    if (scale == 0u || len > scale) {
        out_len = (negative ? 1u : 0u) + len + (scale ? 1u : 0u) + 1u;
        out = expr_tostring_xmalloc(out_len);
        if (negative)
            out[pos++] = '-';
        if (scale == 0u) {
            memcpy(out + pos, mag, len + 1u);
        } else {
            size_t int_len = len - scale;

            memcpy(out + pos, mag, int_len);
            pos += int_len;
            out[pos++] = '.';
            memcpy(out + pos, mag + int_len, scale + 1u);
        }
    } else {
        size_t zero_count = scale - len;

        out_len = (negative ? 1u : 0u) + 2u + zero_count + len + 1u;
        out = expr_tostring_xmalloc(out_len);
        if (negative)
            out[pos++] = '-';
        out[pos++] = '0';
        out[pos++] = '.';
        memset(out + pos, '0', zero_count);
        pos += zero_count;
        memcpy(out + pos, mag, len + 1u);
    }

    free(digits);
    expr_trim_decimal_display_artifacts_local(out);
    return out;
}

static int expr_unicode_digit_value_local(const char *text,
                                        const char **next_out,
                                        bool *subscript_out)
{
    static const char *const sup[] = {
        "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"
    };
    static const char *const sub[] = {
        "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"
    };

    for (int i = 0; i < 10; ++i) {
        size_t len = strlen(sup[i]);

        if (strncmp(text, sup[i], len) == 0) {
            *next_out = text + len;
            *subscript_out = false;
            return i;
        }
        len = strlen(sub[i]);
        if (strncmp(text, sub[i], len) == 0) {
            *next_out = text + len;
            *subscript_out = true;
            return i;
        }
    }

    return -1;
}

static char *expr_ascii_rational_part_local(const char *start,
                                          const char *end,
                                          bool want_subscript)
{
    char *out;
    size_t pos = 0u;

    out = expr_tostring_xmalloc((size_t)(end - start) + 1u);
    for (const char *p = start; p < end;) {
        const char *next = p;
        bool is_subscript = false;
        int digit;

        if (isdigit((unsigned char)*p)) {
            if (want_subscript)
                goto fail;
            out[pos++] = *p++;
            continue;
        }

        digit = expr_unicode_digit_value_local(p, &next, &is_subscript);
        if (digit < 0 || is_subscript != want_subscript)
            goto fail;
        out[pos++] = (char)('0' + digit);
        p = next;
    }

    out[pos] = '\0';
    return out;

fail:
    free(out);
    return NULL;
}

static bool expr_split_rational_text_local(const char *text,
                                         char **numer_out,
                                         char **denom_out)
{
    const char *slash;
    const char *numer_start;
    bool negative = false;
    char *numer;

    if (!text)
        return false;

    slash = strstr(text, "⁄");
    if (!slash)
        slash = strchr(text, '/');
    if (!slash)
        return false;

    numer_start = text;
    if (*numer_start == '-') {
        negative = true;
        ++numer_start;
    }

    numer = expr_ascii_rational_part_local(numer_start, slash, false);
    *denom_out = expr_ascii_rational_part_local(slash + (slash[0] == '/' ? 1u : strlen("⁄")),
                                              text + strlen(text),
                                              slash[0] != '/');
    if (!numer || !*denom_out) {
        free(numer);
        free(*denom_out);
        *denom_out = NULL;
        return false;
    }

    if (negative) {
        size_t len = strlen(numer);
        *numer_out = expr_tostring_xmalloc(len + 2u);
        (*numer_out)[0] = '-';
        memcpy(*numer_out + 1, numer, len + 1u);
        free(numer);
    } else {
        *numer_out = numer;
    }

    return true;
}

static char *expr_decimal_string_from_rational_text_local(const char *text)
{
    char *numer_text = NULL;
    char *denom_text = NULL;
    mpz_t den;
    mpz_t scaled;
    bool mpz_ready = false;
    size_t twos = 0u;
    size_t fives = 0u;
    size_t scale;
    char *out = NULL;

    if (!expr_split_rational_text_local(text, &numer_text, &denom_text))
        return NULL;

    mpz_init(den);
    mpz_init(scaled);
    mpz_ready = true;
    if (mpz_set_str(den, denom_text, 10) != 0 ||
        mpz_set_str(scaled, numer_text, 10) != 0 ||
        mpz_sgn(den) == 0)
        goto done;
    if (mpz_sgn(den) < 0) {
        mpz_neg(den, den);
        mpz_neg(scaled, scaled);
    }

    if (!expr_mpz_factor_out_ulong(den, 2u, &twos) ||
        !expr_mpz_factor_out_ulong(den, 5u, &fives) ||
        mpz_cmp_ui(den, 1u) != 0)
        goto done;

    scale = twos > fives ? twos : fives;
    if (scale < 12u)
        goto done;

    if (fives > twos) {
        expr_mpz_mul_small_power(scaled, 2u, fives - twos);
    } else if (twos > fives) {
        expr_mpz_mul_small_power(scaled, 5u, twos - fives);
    }

    out = expr_decimal_from_scaled_integer(scaled, scale);

done:
    if (mpz_ready) {
        mpz_clear(scaled);
        mpz_clear(den);
    }
    free(denom_text);
    free(numer_text);
    return out;
}

char *expr_number_to_string_local(number_t value)
{
    char *text;
    size_t bits;
    size_t digits;
    char fmt[32];
    int needed;

    if (num_is_inf(value)) {
        if (num_get_sign(value) < 0) {
            num_destroy(&value);
            return expr_tostring_xstrdup("-∞");
        }
        num_destroy(&value);
        return expr_tostring_xstrdup("∞");
    }
    if (num_eq(value, NUM_I)) {
        num_destroy(&value);
        return expr_tostring_xstrdup("i");
    }
    if (num_eq(value, NUM_NEG_I)) {
        num_destroy(&value);
        return expr_tostring_xstrdup("-i");
    }

    if ((!num_is_inexact_real_backend(value) && !num_is_complex_backend(value)) ||
        num_is_exact(value) || !num_is_finite(value)) {
        text = num_to_string(value);
        if (num_is_exact(value)) {
            char *decimal = expr_decimal_string_from_rational_text_local(text);

            if (decimal) {
                free(text);
                num_destroy(&value);
                return decimal;
            }
        }
        expr_trim_decimal_display_artifacts_local(text);
        num_destroy(&value);
        return text;
    }

    bits = num_get_prec_bits(value);
    if (bits == 0u)
        bits = num_get_effective_prec_bits(value);
    digits = bits == 0u ? 0u : (size_t)((double)bits * 0.3010299956639812);
    if (digits == 0u)
        digits = num_get_default_prec_digits();
    if (digits == 0u || digits > (size_t)INT_MAX) {
        text = num_to_string(value);
        expr_trim_decimal_display_artifacts_local(text);
        num_destroy(&value);
        return text;
    }

    snprintf(fmt, sizeof(fmt), "%%.%dn", (int)digits);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0) {
        text = num_to_string(value);
    } else {
        text = malloc((size_t)needed + 1u);
        if (text)
            num_sprintf(text, (size_t)needed + 1u, fmt, value);
    }

    expr_trim_decimal_display_artifacts_local(text);
    num_destroy(&value);
    return text;
}

static void expr_trim_decimal_display_artifacts_local(char *text)
{
    char *p;

    if (!text)
        return;

    p = text;
    while ((p = strchr(p, '.')) != NULL) {
        char *frac = p + 1;
        char *end = frac;
        char *q;
        char *zero_start = NULL;
        size_t zero_run = 0u;
        bool seen_nonzero = false;

        while (isdigit((unsigned char)*end))
            ++end;
        if (end == frac) {
            ++p;
            continue;
        }

        for (q = frac; q < end; ++q) {
            if (*q == '0') {
                if (seen_nonzero) {
                    if (!zero_start)
                        zero_start = q;
                    ++zero_run;
                }
                if (zero_start && zero_run >= 24u) {
                    memmove(zero_start, end, strlen(end) + 1u);
                    p = zero_start;
                    break;
                }
            } else {
                seen_nonzero = true;
                zero_start = NULL;
                zero_run = 0u;
            }
        }
        if (q != end)
            continue;

        while (end > frac && end[-1] == '0')
            --end;
        if (end == frac) {
            memmove(p, q, strlen(q) + 1u);
            continue;
        }
        if (*end == '\0') {
            *end = '\0';
            p = end;
        } else {
            memmove(end, q, strlen(q) + 1u);
            p = end;
        }
    }
}

char *expr_const_to_string_local(const expr_t *dv)
{
    return dv ? expr_number_to_string_local(num_clone(dv->c)) : NULL;
}

char *expr_eval_to_string_local(const expr_t *dv)
{
    return expr_number_to_string_local(expr_eval(dv));
}

bool expr_is_immortal_default_const_local(const expr_t *dv)
{
    const char *canon;
    number_t builtin;
    bool match;
    bool precise_match = false;

    if (!dv || !expr_is_const(dv) || !dv->name || !*dv->name)
        return false;

    canon = expr_default_constant_canonical_name(dv->name);
    if (!canon)
        return false;
    if (strcmp(canon, "@tau") == 0)
        return false;
    if (!expr_get_default_constant_num(canon, &builtin))
        return false;

    match = num_eq(dv->c, builtin);
    if (!match || num_get_prec_bits(dv->c) != num_get_prec_bits(builtin)) {
        number_t builtin_at_prec = num_const_prec(builtin,
                                                  num_get_prec_bits(dv->c));

        precise_match = num_eq(dv->c, builtin_at_prec);
        num_destroy(&builtin_at_prec);
    }
    num_destroy(&builtin);
    return match || precise_match;
}

