#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>

#include "expr_bindings.h"
#include "expr_internal.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"
#include "expression.h"
#include "internal/number_internal.h"
#include "ustring.h"

static string_t *expr_trim_decimal_display_artifacts_text_local(
    const string_t *text);

static char *expr_take_text_object_as_c_string_local(string_t *text_obj)
{
    char *text = text_obj
        ? expr_tostring_xstrdup(string_c_str(text_obj))
        : NULL;

    string_free(text_obj);
    return text;
}

static char *expr_take_clean_decimal_text_object_as_c_string_local(
    string_t *text_obj)
{
    string_t *cleaned;

    if (!text_obj)
        return NULL;

    cleaned = expr_trim_decimal_display_artifacts_text_local(text_obj);
    if (cleaned) {
        string_free(text_obj);
        text_obj = cleaned;
    }

    return expr_take_text_object_as_c_string_local(text_obj);
}

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
    char *digits_raw = mpz_get_str(NULL, 10, scaled);
    string_t *digits;
    string_t *mag = NULL;
    string_t *out = NULL;
    bool negative;
    size_t len;

    if (!digits_raw)
        return NULL;

    digits = string_new_with(digits_raw);
    free(digits_raw);
    if (!digits)
        return NULL;

    len = string_length(digits);
    negative = len > 0u && rune_is_equal(string_at(digits, 0u), '-');
    mag = negative
        ? string_substring(digits, 1u, len - 1u)
        : string_clone(digits);
    string_free(digits);
    if (!mag)
        return NULL;

    len = string_length(mag);
    out = string_new();
    if (!out)
        goto fail;

    if (negative && string_append_char(out, '-') != 0)
        goto fail;

    if (scale == 0u || len > scale) {
        if (scale == 0u) {
            if (string_append_string(out, mag) != 0)
                goto fail;
        } else {
            size_t int_len = len - scale;
            string_t *intpart = string_substring(mag, 0u, int_len);
            string_t *fracpart = string_substring(mag, int_len, scale);

            if (!intpart || !fracpart ||
                string_append_string(out, intpart) != 0 ||
                string_append_char(out, '.') != 0 ||
                string_append_string(out, fracpart) != 0) {
                string_free(intpart);
                string_free(fracpart);
                goto fail;
            }
            string_free(intpart);
            string_free(fracpart);
        }
    } else {
        size_t zero_count = scale - len;

        if (string_append_cstr(out, "0.") != 0)
            goto fail;
        for (size_t i = 0u; i < zero_count; ++i) {
            if (string_append_char(out, '0') != 0)
                goto fail;
        }
        if (string_append_string(out, mag) != 0)
            goto fail;
    }

    string_free(mag);
    return expr_take_clean_decimal_text_object_as_c_string_local(out);

fail:
    string_free(out);
    string_free(mag);
    return NULL;
}

static int expr_unicode_digit_value_local(rune_t rune, bool *subscript_out)
{
    uint32_t value = rune_value(rune);

    if (value >= 0x2080u && value <= 0x2089u) {
        *subscript_out = true;
        return (int)(value - 0x2080u);
    }
    if (value == 0x2070u) {
        *subscript_out = false;
        return 0;
    }
    if (value == 0x00B9u) {
        *subscript_out = false;
        return 1;
    }
    if (value == 0x00B2u) {
        *subscript_out = false;
        return 2;
    }
    if (value == 0x00B3u) {
        *subscript_out = false;
        return 3;
    }
    if (value >= 0x2074u && value <= 0x2079u) {
        *subscript_out = false;
        return (int)(value - 0x2070u);
    }
    return -1;
}

static string_t *expr_ascii_rational_part_local(const string_t *text,
                                                string_pos_t start,
                                                string_pos_t end,
                                                bool want_subscript)
{
    string_cursor_t *cursor;
    string_t *out;

    if (!text || end < start)
        return NULL;

    cursor = string_cursor_new(text);
    out = string_new();
    if (!cursor || !out) {
        string_cursor_free(cursor);
        string_free(out);
        return NULL;
    }

    if (string_cursor_seek(cursor, start) != 0)
        goto fail;

    while (string_cursor_position(cursor) < end) {
        string_pos_t before = string_cursor_position(cursor);
        rune_t rune = string_cursor_peek(cursor);
        unsigned char ascii = 0u;
        bool is_subscript = false;
        int digit;

        if (string_cursor_peek_ascii(cursor, &ascii) &&
            ascii >= '0' && ascii <= '9') {
            if (want_subscript)
                goto fail;
            if (string_append_char(out, (char)ascii) != 0 ||
                string_cursor_next(cursor) != 0 ||
                string_cursor_position(cursor) > end)
                goto fail;
            continue;
        }

        digit = expr_unicode_digit_value_local(rune, &is_subscript);
        if (digit < 0 || is_subscript != want_subscript)
            goto fail;
        if (string_append_char(out, (char)('0' + digit)) != 0 ||
            string_cursor_next(cursor) != 0 ||
            string_cursor_position(cursor) > end ||
            string_cursor_position(cursor) == before)
            goto fail;
    }

    string_cursor_free(cursor);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
}

static bool expr_split_rational_text_local(const string_t *text,
                                           string_t **numer_out,
                                           string_t **denom_out)
{
    string_cursor_t *cursor;
    string_pos_t numer_start = 0u;
    string_pos_t slash_pos = 0u;
    string_pos_t slash_end = 0u;
    string_pos_t end;
    bool negative = false;
    bool unicode_slash = false;
    bool found_slash = false;
    string_t *numer;

    if (!text || !numer_out || !denom_out)
        return false;

    *numer_out = NULL;
    *denom_out = NULL;
    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = true;
        if (string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            return false;
        }
        numer_start = string_cursor_position(cursor);
    }

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);

        if (rune_is_equal(rune, '/') || rune_value(rune) == 0x2044u) {
            slash_pos = string_cursor_position(cursor);
            unicode_slash = rune_value(rune) == 0x2044u;
            if (string_cursor_next(cursor) != 0) {
                string_cursor_free(cursor);
                return false;
            }
            slash_end = string_cursor_position(cursor);
            found_slash = true;
            break;
        }
        if (string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            return false;
        }
    }

    end = string_cursor_end_position(cursor);
    string_cursor_free(cursor);
    if (!found_slash)
        return false;

    numer = expr_ascii_rational_part_local(text, numer_start, slash_pos, false);
    *denom_out = expr_ascii_rational_part_local(text,
                                                slash_end,
                                                end,
                                                unicode_slash);
    if (!numer || !*denom_out) {
        string_free(numer);
        string_free(*denom_out);
        *denom_out = NULL;
        return false;
    }

    if (negative) {
        *numer_out = string_new_with("-");
        if (!*numer_out ||
            string_append_string(*numer_out, numer) != 0) {
            string_free(*numer_out);
            *numer_out = NULL;
        }
        string_free(numer);
        if (!*numer_out) {
            string_free(*denom_out);
            *denom_out = NULL;
            return false;
        }
    } else {
        *numer_out = numer;
    }

    return true;
}

static char *expr_decimal_string_from_rational_text_local(const string_t *text)
{
    string_t *numer_text = NULL;
    string_t *denom_text = NULL;
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
    if (mpz_set_str(den, string_c_str(denom_text), 10) != 0 ||
        mpz_set_str(scaled, string_c_str(numer_text), 10) != 0 ||
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
    string_free(denom_text);
    string_free(numer_text);
    return out;
}

char *expr_number_to_string_local(number_t value)
{
    char *text;
    string_t *text_obj;
    size_t bits;
    size_t digits;
    char fmt[32];

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
        text_obj = num_to_string(value);
        if (num_is_exact(value)) {
            char *decimal = expr_decimal_string_from_rational_text_local(text_obj);

            if (decimal) {
                string_free(text_obj);
                num_destroy(&value);
                return decimal;
            }
        }
        text = expr_take_clean_decimal_text_object_as_c_string_local(text_obj);
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
        text = expr_take_clean_decimal_text_object_as_c_string_local(
            num_to_string(value));
        num_destroy(&value);
        return text;
    }

    snprintf(fmt, sizeof(fmt), "%%.%dn", (int)digits);
    text_obj = num_sprintf_text(fmt, value);
    text = expr_take_clean_decimal_text_object_as_c_string_local(
        text_obj ? text_obj : num_to_string(value));

    num_destroy(&value);
    return text;
}

static bool expr_append_decimal_text_slice_local(string_t *out,
                                                 const string_cursor_t *cursor,
                                                 string_pos_t start,
                                                 string_pos_t end)
{
    string_t *slice;
    bool ok;

    if (!out || !cursor || end < start)
        return false;

    slice = string_cursor_slice_between(start, end, cursor);
    if (!slice)
        return false;

    ok = string_append_string(out, slice) == 0;
    string_free(slice);
    return ok;
}

static string_t *expr_trim_decimal_display_artifacts_text_local(
    const string_t *text)
{
    string_cursor_t *cursor;
    string_t *out;
    string_pos_t segment_start = 0u;

    if (!text)
        return NULL;

    cursor = string_cursor_new(text);
    out = string_new();
    if (!cursor || !out) {
        string_cursor_free(cursor);
        string_free(out);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        unsigned char ch;
        string_pos_t dot_pos;
        string_pos_t frac_end;
        string_pos_t last_nonzero_end = 0u;
        string_pos_t zero_start = 0u;
        size_t zero_run = 0u;
        bool seen_nonzero = false;
        bool long_zero_run = false;

        if (!string_cursor_peek_ascii(cursor, &ch) || ch != '.') {
            if (string_cursor_next(cursor) != 0)
                goto fail;
            continue;
        }

        dot_pos = string_cursor_position(cursor);
        if (string_cursor_next(cursor) != 0)
            goto fail;

        if (!string_cursor_peek_ascii(cursor, &ch) || !isdigit(ch))
            continue;

        while (string_cursor_peek_ascii(cursor, &ch) && isdigit(ch)) {
            string_pos_t digit_pos = string_cursor_position(cursor);

            if (ch == '0') {
                if (seen_nonzero) {
                    if (zero_run == 0u)
                        zero_start = digit_pos;
                    ++zero_run;
                }
            } else {
                seen_nonzero = true;
                last_nonzero_end = 0u;
                zero_start = 0u;
                zero_run = 0u;
            }

            if (string_cursor_next(cursor) != 0)
                goto fail;

            if (ch != '0')
                last_nonzero_end = string_cursor_position(cursor);
            if (zero_run >= 24u) {
                long_zero_run = true;
                while (string_cursor_peek_ascii(cursor, &ch) &&
                       isdigit(ch)) {
                    if (string_cursor_next(cursor) != 0)
                        goto fail;
                }
                break;
            }
        }

        frac_end = string_cursor_position(cursor);
        if (long_zero_run) {
            if (!expr_append_decimal_text_slice_local(out,
                                                      cursor,
                                                      segment_start,
                                                      zero_start))
                goto fail;
            segment_start = frac_end;
            continue;
        }

        if (!seen_nonzero) {
            if (!expr_append_decimal_text_slice_local(out,
                                                      cursor,
                                                      segment_start,
                                                      dot_pos))
                goto fail;
            segment_start = frac_end;
            continue;
        }

        if (last_nonzero_end < frac_end) {
            if (!expr_append_decimal_text_slice_local(out,
                                                      cursor,
                                                      segment_start,
                                                      last_nonzero_end))
                goto fail;
            segment_start = frac_end;
        }
    }

    if (!expr_append_decimal_text_slice_local(out,
                                              cursor,
                                              segment_start,
                                              string_cursor_end_position(cursor)))
        goto fail;

    string_cursor_free(cursor);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
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
