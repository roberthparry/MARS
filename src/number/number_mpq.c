#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include "ustring.h"

typedef struct {
    number_const_id_t id;
    long numerator;
    unsigned long denominator;
} number_mpq_const_source_t;

typedef struct {
    unsigned long numerator;
    unsigned long denominator;
    const char *glyph;
} number_mpq_fraction_glyph_t;

static const number_mpq_const_source_t number_mpq_const_sources[] = {
    { NUMBER_CONST_HALF, 1L, 2u },
    { NUMBER_CONST_ONE_AND_HALF, 3L, 2u },
    { NUMBER_CONST_ONE_THIRD, 1L, 3u },
    { NUMBER_CONST_QUARTER, 1L, 4u },
    { NUMBER_CONST_ONE_SIXTH, 1L, 6u },
    { NUMBER_CONST_ONE_EIGHTH, 1L, 8u },
    { NUMBER_CONST_ONE_TENTH, 1L, 10u }
};

static const number_mpq_fraction_glyph_t number_mpq_fraction_glyphs[] = {
    { 1u, 2u, "½"  },
    { 1u, 3u, "⅓"  },
    { 2u, 3u, "⅔"  },
    { 1u, 4u, "¼"  },
    { 3u, 4u, "¾"  },
    { 1u, 5u, "⅕"  },
    { 2u, 5u, "⅖"  },
    { 3u, 5u, "⅗"  },
    { 4u, 5u, "⅘"  },
    { 1u, 6u, "⅙"  },
    { 5u, 6u, "⅚"  },
    { 1u, 7u, "⅐"  },
    { 1u, 8u, "⅛"  },
    { 3u, 8u, "⅜"  },
    { 5u, 8u, "⅝"  },
    { 7u, 8u, "⅞"  },
    { 1u, 9u, "⅑"  },
    { 1u, 10u, "⅒" },
};

static const char *const number_mpq_superscript_digits[] = {
    "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹",
};

static const char *const number_mpq_subscript_digits[] = {
    "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉",
};

static int number_mpq_init(number_mpq_t *value)
{
    if (!value || value->initialised)
        return value ? 0 : -1;
    mpq_init(value->value);
    value->initialised = true;
    return 0;
}

static bool number_mpq_const_fraction(number_const_id_t id,
                                      long *numerator_out,
                                      unsigned long *denominator_out)
{
    for (size_t i = 0u;
         i < sizeof(number_mpq_const_sources) / sizeof(number_mpq_const_sources[0]);
         ++i) {
        if (number_mpq_const_sources[i].id == id) {
            if (numerator_out)
                *numerator_out = number_mpq_const_sources[i].numerator;
            if (denominator_out)
                *denominator_out = number_mpq_const_sources[i].denominator;
            return true;
        }
    }
    return false;
}

static const char *number_mpq_fraction_glyph_lookup(mpz_srcptr numerator,
                                                    mpz_srcptr denominator)
{
    unsigned long n;
    unsigned long d;

    if (!mpz_fits_ulong_p(numerator) || !mpz_fits_ulong_p(denominator))
        return NULL;
    n = mpz_get_ui(numerator);
    d = mpz_get_ui(denominator);
    for (size_t i = 0u;
         i < sizeof(number_mpq_fraction_glyphs) / sizeof(number_mpq_fraction_glyphs[0]);
         ++i) {
        if (number_mpq_fraction_glyphs[i].numerator == n &&
            number_mpq_fraction_glyphs[i].denominator == d)
            return number_mpq_fraction_glyphs[i].glyph;
    }
    return NULL;
}

static bool number_mpq_append_digit_table_cstr(string_t *out,
                                               const char *digits,
                                               const char *const table[])
{
    if (!out || !digits || !table)
        return false;

    for (size_t i = 0u; digits[i] != '\0'; ++i) {
        unsigned char digit = (unsigned char)digits[i];

        if (digit < '0' || digit > '9' ||
            string_append_cstr(out, table[(unsigned int)(digit - '0')]) != 0)
            return false;
    }
    return true;
}

static string_t *number_mpq_decode_digit_sequence(string_cursor_t *cursor,
                                                  const char *const table[])
{
    string_t *digits = string_new();
    bool found = false;

    if (!cursor || !digits) {
        string_free(digits);
        return NULL;
    }

    for (;;) {
        bool matched = false;

        for (int digit = 0; digit < 10; ++digit) {
            if (string_cursor_consume(cursor, table[digit])) {
                if (string_append_char(digits, (char)('0' + digit)) != 0) {
                    string_free(digits);
                    return NULL;
                }
                found = true;
                matched = true;
                break;
            }
        }

        if (!matched)
            break;
    }

    if (!found) {
        string_free(digits);
        return NULL;
    }

    return digits;
}

static int number_mpq_parse_glyph_fraction(mpq_t value,
                                           string_view_t text,
                                           bool negative)
{
    for (size_t i = 0u;
         i < sizeof(number_mpq_fraction_glyphs) / sizeof(number_mpq_fraction_glyphs[0]);
         ++i) {
        if (string_view_equals_literal(text, number_mpq_fraction_glyphs[i].glyph)) {
            mpq_set_ui(value,
                       number_mpq_fraction_glyphs[i].numerator,
                       number_mpq_fraction_glyphs[i].denominator);
            if (negative)
                mpz_neg(mpq_numref(value), mpq_numref(value));
            mpq_canonicalize(value);
            return 0;
        }
    }
    return -1;
}

static int number_mpq_parse_stacked_fraction(mpq_t value,
                                             string_cursor_t *cursor,
                                             bool negative)
{
    string_t *num_text;
    string_t *den_text;
    int rc = -1;

    if (!cursor)
        return -1;

    num_text = number_mpq_decode_digit_sequence(cursor, number_mpq_superscript_digits);
    if (!num_text)
        return -1;
    if (!string_cursor_consume(cursor, "⁄"))
        goto cleanup_num;

    den_text = number_mpq_decode_digit_sequence(cursor, number_mpq_subscript_digits);
    if (!den_text)
        goto cleanup_num;
    if (!string_cursor_done(cursor))
        goto cleanup_den;

    if (mpz_set_str(mpq_numref(value), string_c_str(num_text), 10) != 0 ||
        mpz_set_str(mpq_denref(value), string_c_str(den_text), 10) != 0 ||
        mpz_sgn(mpq_denref(value)) == 0)
        goto cleanup_den;

    if (negative)
        mpz_neg(mpq_numref(value), mpq_numref(value));
    mpq_canonicalize(value);
    rc = 0;

cleanup_den:
    string_free(den_text);
cleanup_num:
    string_free(num_text);
    return rc;
}

static int number_mpq_set_unicode_fraction(mpq_t value, const string_t *text)
{
    string_cursor_t *cursor;
    string_pos_t start;
    string_view_t unsigned_text;
    bool negative = false;
    int rc;

    if (!text || string_length(text) == 0u)
        return -1;

    cursor = string_cursor_new(text);
    if (!cursor)
        return -1;

    if (string_cursor_consume(cursor, "-"))
        negative = true;

    if (string_cursor_done(cursor)) {
        string_cursor_free(cursor);
        return -1;
    }

    start = string_cursor_position(cursor);
    unsigned_text = string_cursor_view_between(start,
                                               string_cursor_end_position(cursor),
                                               cursor);
    if (number_mpq_parse_glyph_fraction(value, unsigned_text, negative) == 0) {
        string_cursor_free(cursor);
        return 0;
    }

    (void)string_cursor_seek(cursor, start);
    rc = number_mpq_parse_stacked_fraction(value, cursor, negative);
    string_cursor_free(cursor);
    return rc;
}

static string_t *number_mpq_format_glyph_fraction_text(bool negative, const char *glyph)
{
    string_t *text = string_new();

    if (!text)
        return NULL;
    if ((negative && string_append_char(text, '-') != 0) ||
        string_append_cstr(text, glyph) != 0) {
        string_free(text);
        return NULL;
    }
    return text;
}

static string_t *number_mpq_text_from_mpz(mpz_srcptr value)
{
    char *raw = mpz_get_str(NULL, 10, value);
    string_t *text;

    if (!raw)
        return NULL;
    text = string_new_with(raw);
    free(raw);
    return text;
}

static string_t *number_mpq_format_stacked_fraction_text(bool negative,
                                                         mpz_srcptr numerator,
                                                         mpz_srcptr denominator)
{
    static const char fraction_slash[] = "⁄";
    char *num_text;
    char *den_text;
    string_t *text;

    num_text = mpz_get_str(NULL, 10, numerator);
    if (!num_text)
        return NULL;
    den_text = mpz_get_str(NULL, 10, denominator);
    if (!den_text) {
        free(num_text);
        return NULL;
    }

    text = string_new();
    if (!text) {
        free(num_text);
        free(den_text);
        return NULL;
    }

    if ((negative && string_append_char(text, '-') != 0) ||
        !number_mpq_append_digit_table_cstr(text, num_text, number_mpq_superscript_digits) ||
        string_append_cstr(text, fraction_slash) != 0 ||
        !number_mpq_append_digit_table_cstr(text, den_text, number_mpq_subscript_digits)) {
        string_free(text);
        text = NULL;
    }

    free(num_text);
    free(den_text);
    return text;
}

number_mpq_t *number_mpq_from_const_id(number_const_id_t id)
{
    number_mpq_t *out;
    long ignored_numerator;
    unsigned long ignored_denominator;

    if (!number_mpq_const_fraction(id, &ignored_numerator, &ignored_denominator))
        return NULL;
    out = malloc(sizeof(*out));
    if (!out)
        return NULL;
    out->constant_id = id;
    out->immortal = false;
    out->initialised = false;
    return out;
}

number_mpq_t *number_mpq_new(void)
{
    number_mpq_t *out = malloc(sizeof(*out));

    if (!out)
        return NULL;
    out->constant_id = NUMBER_CONST_COUNT;
    out->immortal = false;
    out->initialised = false;
    if (number_mpq_init(out) != 0) {
        free(out);
        return NULL;
    }
    return out;
}

number_mpq_t *number_mpq_from_frac_long(long numerator, long denominator)
{
    number_mpq_t *out;
    unsigned long den_mag;

    if (denominator == 0)
        return NULL;
    out = number_mpq_new();
    if (!out)
        return NULL;
    den_mag = denominator > 0 ? (unsigned long)denominator
                              : (unsigned long)(-(denominator + 1L)) + 1ul;
    mpq_set_si(out->value, numerator, den_mag);
    if (denominator < 0)
        mpz_neg(mpq_numref(out->value), mpq_numref(out->value));
    mpq_canonicalize(out->value);
    return out;
}

number_mpq_t *number_mpq_from_mpq(mpq_srcptr value)
{
    number_mpq_t *out;

    if (!value)
        return NULL;
    out = number_mpq_new();
    if (!out)
        return NULL;
    mpq_set(out->value, value);
    mpq_canonicalize(out->value);
    return out;
}

static bool number_mpq_text_has_ascii(const string_t *text, char needle)
{
    string_cursor_t *cursor;
    unsigned char ch;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &ch) &&
            ch == (unsigned char)needle) {
            string_cursor_free(cursor);
            return true;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return false;
}

number_mpq_t *number_mpq_from_text(const string_t *text)
{
    number_mpq_t *out = number_mpq_new();
    int rc;

    if (!text || !out) {
        number_mpq_free(out);
        return NULL;
    }

    rc = number_mpq_text_has_ascii(text, '/')
        ? mpq_set_str(out->value, string_c_str(text), 10)
        : number_mpq_set_unicode_fraction(out->value, text);
    if (rc != 0 || mpz_sgn(mpq_denref(out->value)) == 0) {
        number_mpq_free(out);
        return NULL;
    }
    mpq_canonicalize(out->value);
    return out;
}

number_mpq_t *number_mpq_clone(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 ? number_mpq_from_mpq(value->value) : NULL;
}

void number_mpq_free(number_mpq_t *value)
{
    if (!value || value->immortal)
        return;
    if (value->initialised)
        mpq_clear(value->value);
    free(value);
}

int number_mpq_ensure(const number_mpq_t *value)
{
    number_mpq_t *mutable_value = (number_mpq_t *)value;
    long numerator;
    unsigned long denominator;

    if (!value)
        return -1;
    if (value->initialised)
        return 0;
    if (!number_mpq_const_fraction(value->constant_id, &numerator, &denominator))
        return -1;
    if (number_mpq_init(mutable_value) != 0)
        return -1;
    mpq_set_si(mutable_value->value, numerator, denominator);
    mpq_canonicalize(mutable_value->value);
    return 0;
}

mpq_srcptr number_mpq_value(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 ? value->value : NULL;
}

string_t *number_mpq_to_text(const number_mpq_t *value)
{
    const char *glyph;
    string_t *text;
    bool negative;
    mpz_t numerator;

    if (number_mpq_ensure(value) != 0)
        return NULL;
    if (mpz_cmp_ui(mpq_denref(value->value), 1u) == 0)
        return number_mpq_text_from_mpz(mpq_numref(value->value));

    negative = mpq_sgn(value->value) < 0;
    mpz_init(numerator);
    mpz_abs(numerator, mpq_numref(value->value));
    glyph = number_mpq_fraction_glyph_lookup(numerator, mpq_denref(value->value));
    text = glyph ? number_mpq_format_glyph_fraction_text(negative, glyph)
                 : number_mpq_format_stacked_fraction_text(
                       negative, numerator, mpq_denref(value->value));
    mpz_clear(numerator);
    return text;
}

bool number_mpq_get_small_fraction(const number_mpq_t *value,
                                   long *numerator,
                                   long *denominator)
{
    if (!numerator || !denominator || number_mpq_ensure(value) != 0 ||
        !mpz_fits_slong_p(mpq_numref(value->value)) ||
        !mpz_fits_slong_p(mpq_denref(value->value)))
        return false;
    *numerator = mpz_get_si(mpq_numref(value->value));
    *denominator = mpz_get_si(mpq_denref(value->value));
    return *denominator != 0L;
}

int number_mpq_cmp(const number_mpq_t *a, const number_mpq_t *b)
{
    if (number_mpq_ensure(a) != 0 || number_mpq_ensure(b) != 0)
        return 0;
    return mpq_cmp(a->value, b->value);
}

bool number_mpq_is_zero(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 && mpq_sgn(value->value) == 0;
}

bool number_mpq_is_one(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 &&
        mpz_cmp_si(mpq_numref(value->value), 1L) == 0 &&
        mpz_cmp_ui(mpq_denref(value->value), 1u) == 0;
}

bool number_mpq_is_integer(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 &&
        mpz_cmp_ui(mpq_denref(value->value), 1u) == 0;
}

bool number_mpq_is_negative(const number_mpq_t *value)
{
    return number_mpq_ensure(value) == 0 && mpq_sgn(value->value) < 0;
}
