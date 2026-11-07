/**
 * @file expr_stringout_numbers.c
 * @brief Numeric atoms for symbolic presentation.
 *
 * Formats constants and evaluated numeric values and removes display artefacts where appropriate. It keeps numeric
 * spelling consistent across renderers without changing the underlying value.
 *
 * This is part of the expr_t implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "string-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"


static bool expr_number_scientific_local = false;
static int expr_number_precision_local = -1;

static string_t *expr_trim_decimal_display_artifacts_text_local(const string_t *text);

bool expr_set_number_scientific_local(bool scientific)
{
    bool old = expr_number_scientific_local;

    expr_number_scientific_local = scientific;
    return old;
}

int expr_set_number_precision_local(int precision)
{
    int old = expr_number_precision_local;

    expr_number_precision_local = precision;
    return old;
}

/* Semantic notation and native formatting share the same explicit display-rounding policy. */
string_t *expr_number_format_text_local(number_t value)
{
    string_t *decimal = expr_number_preferred_decimal_local(value);
    if (decimal)
        return decimal;
    if (expr_number_precision_local >= 0)
        return num_sprintf_text(expr_number_scientific_local ? "%.*N" : "%.*n", expr_number_precision_local, value);
    return expr_number_scientific_local ? num_sprintf_text("%N", value) : num_to_string(value);
}

static char *expr_take_text_object_as_c_string_local(string_t *text_obj)
{
    char *text = text_obj ? expr_tostring_xstrdup(string_c_str(text_obj)) : NULL;

    string_free(text_obj);
    return text;
}

static char *expr_take_clean_decimal_text_object_as_c_string_local(string_t *text_obj)
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

static string_t *expr_ascii_rational_part_local(const string_t *text, string_pos_t start, string_pos_t end,
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

        if (string_cursor_peek_ascii(cursor, &ascii) && ascii >= '0' && ascii <= '9') {
            if (want_subscript)
                goto fail;
            if (string_append_char(out, (char)ascii) != 0 || string_cursor_next(cursor) != 0 ||
                string_cursor_position(cursor) > end)
                goto fail;
            continue;
        }

        digit = expr_unicode_digit_value_local(rune, &is_subscript);
        if (digit < 0 || is_subscript != want_subscript)
            goto fail;
        if (string_append_char(out, (char)('0' + digit)) != 0 || string_cursor_next(cursor) != 0 ||
            string_cursor_position(cursor) > end || string_cursor_position(cursor) == before)
            goto fail;
    }

    string_cursor_free(cursor);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
}

bool expr_split_rational_text_local(const string_t *text, string_t **numer_out, string_t **denom_out)
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
    *denom_out = expr_ascii_rational_part_local(text, slash_end, end, unicode_slash);
    if (!numer || !*denom_out) {
        string_free(numer);
        string_free(*denom_out);
        *denom_out = NULL;
        return false;
    }

    if (negative) {
        *numer_out = string_new_with("-");
        if (!*numer_out || string_append_string(*numer_out, numer) != 0) {
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

/* Format complex components independently so a long rational coefficient cannot bypass the shared policy. */
static char *expr_complex_number_text_local(number_t value)
{
    number_t real = num_real_part(value), imaginary = num_imag_part(value);
    bool real_zero = num_is_zero(real), negative = num_get_sign(imaginary) < 0;
    number_t magnitude = num_abs(imaginary);
    bool unit = num_eq(magnitude, NUM_ONE);
    char *real_text = real_zero ? NULL : expr_number_to_string_local(num_clone(real));
    char *imaginary_text = unit ? NULL : expr_number_to_string_local(num_clone(magnitude));
    string_t *text = NULL;
    if ((real_zero || real_text) && (unit || imaginary_text)) {
        const char *coefficient = unit ? "" : imaginary_text;
        text = real_zero ? string_sprintf("%s%si", negative ? "-" : "", coefficient)
                         : string_sprintf("%s %c %si", real_text, negative ? '-' : '+', coefficient);
    }
    free(real_text);
    free(imaginary_text);
    num_destroy(&real);
    num_destroy(&imaginary);
    num_destroy(&magnitude);
    return expr_take_text_object_as_c_string_local(text);
}

char *expr_number_to_string_local(number_t value)
{
    const char *constant_name;
    char *text;
    string_t *text_obj;
    size_t bits;
    size_t digits;
    bool scientific = expr_number_scientific_local;
    int precision_override = expr_number_precision_local;
    char fmt[32];

    /* A real-valued complex backend is still a real atom. Printing its zero
     * imaginary part would introduce an ungrouped sum into products and powers. */
    if (num_is_complex_backend(value) && num_is_real(value)) {
        number_t real = num_real_part(value);
        num_destroy(&value);
        value = real;
    }
    if (num_is_complex_backend(value) && num_is_finite(value)) {
        text = expr_complex_number_text_local(value);
        num_destroy(&value);
        return text;
    }

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
    constant_name = !num_is_exact(value) ? num_constant_name(value) : NULL;
    if (constant_name) {
        num_destroy(&value);
        return expr_tostring_xstrdup(constant_name);
    }

    if (!scientific && ((!num_is_inexact_real_backend(value) && !num_is_complex_backend(value)) ||
                        num_is_exact(value) || !num_is_finite(value))) {
        text_obj = expr_number_format_text_local(value);
        text = num_is_exact(value) ? expr_take_text_object_as_c_string_local(text_obj)
                                  : expr_take_clean_decimal_text_object_as_c_string_local(text_obj);
        num_destroy(&value);
        return text;
    }

    if (precision_override >= 0) {
        digits = (size_t)precision_override;
    } else {
        bits = num_get_prec_bits(value);
        if (bits == 0u)
            bits = num_get_effective_prec_bits(value);
        digits = bits == 0u ? 0u : (size_t)((double)bits * 0.3010299956639812);
        if (digits == 0u)
            digits = num_get_default_prec_digits();
    }

    if (digits == 0u || digits > (size_t)INT_MAX) {
        text_obj = scientific ? num_sprintf_text("%N", value) : num_to_string(value);
        text = expr_take_clean_decimal_text_object_as_c_string_local(text_obj);
        num_destroy(&value);
        return text;
    }

    snprintf(fmt, sizeof(fmt), scientific ? "%%.%dN" : "%%.%dn", (int)digits);
    text_obj = num_sprintf_text(fmt, value);
    text = expr_take_clean_decimal_text_object_as_c_string_local(text_obj ? text_obj : num_to_string(value));

    num_destroy(&value);
    return text;
}

static bool expr_append_decimal_text_slice_local(string_t *out, const string_cursor_t *cursor, string_pos_t start,
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

static string_t *expr_trim_decimal_display_artifacts_text_local(const string_t *text)
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
                while (string_cursor_peek_ascii(cursor, &ch) && isdigit(ch)) {
                    if (string_cursor_next(cursor) != 0)
                        goto fail;
                }
                break;
            }
        }

        frac_end = string_cursor_position(cursor);
        if (long_zero_run) {
            if (!expr_append_decimal_text_slice_local(out, cursor, segment_start, zero_start))
                goto fail;
            segment_start = frac_end;
            continue;
        }

        if (!seen_nonzero) {
            if (!expr_append_decimal_text_slice_local(out, cursor, segment_start, dot_pos))
                goto fail;
            segment_start = frac_end;
            continue;
        }

        if (last_nonzero_end < frac_end) {
            if (!expr_append_decimal_text_slice_local(out, cursor, segment_start, last_nonzero_end))
                goto fail;
            segment_start = frac_end;
        }
    }

    if (!expr_append_decimal_text_slice_local(out, cursor, segment_start, string_cursor_end_position(cursor)))
        goto fail;

    string_cursor_free(cursor);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
}

char *expr_const_to_string_local(const expr_t *expr)
{
    return expr ? expr_number_to_string_local(num_clone(expr->c)) : NULL;
}

char *expr_eval_to_string_local(const expr_t *expr)
{
    return expr_number_to_string_local(expr_eval(expr));
}

bool expr_is_immortal_default_const_local(const expr_t *expr)
{
    const char *canon;
    number_t builtin;
    bool match;
    bool precise_match = false;

    if (!expr || !expr_is_const(expr) || !expr->name || !*expr->name)
        return false;

    canon = expr_default_constant_canonical_name(expr->name);
    if (!canon)
        return false;
    if (strcmp(canon, "@tau") == 0)
        return false;
    if (!expr_get_default_constant_num(canon, &builtin))
        return false;

    match = num_eq(expr->c, builtin);
    if (!match || num_get_prec_bits(expr->c) != num_get_prec_bits(builtin)) {
        number_t builtin_at_prec = num_const_prec(builtin, num_get_prec_bits(expr->c));

        precise_match = num_eq(expr->c, builtin_at_prec);
        num_destroy(&builtin_at_prec);
    }
    num_destroy(&builtin);
    return match || precise_match;
}
