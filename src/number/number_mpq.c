#include <stdlib.h>

#include "number_internal.h"

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

static size_t number_mpq_digit_table_len(const char *digits,
                                         const char *const table[])
{
    size_t len = 0u;

    for (; *digits; ++digits)
        len += strlen(table[(unsigned int)(*digits - '0')]);
    return len;
}

static char *number_mpq_append_digit_table(char *out,
                                           const char *digits,
                                           const char *const table[])
{
    const char *encoded;
    size_t len;

    for (; *digits; ++digits) {
        encoded = table[(unsigned int)(*digits - '0')];
        len = strlen(encoded);
        memcpy(out, encoded, len);
        out += len;
    }
    return out;
}

static int number_mpq_match_encoded_digit(const char **text,
                                          const char *const table[])
{
    size_t len;

    for (int digit = 0; digit < 10; ++digit) {
        len = strlen(table[digit]);
        if (strncmp(*text, table[digit], len) == 0) {
            *text += len;
            return digit;
        }
    }
    return -1;
}

static char *number_mpq_decode_digit_sequence(const char **text,
                                              const char *const table[])
{
    const char *p = *text;
    size_t cap = strlen(p) + 1u;
    size_t len = 0u;
    char *digits = malloc(cap);
    int digit;

    if (!digits)
        return NULL;
    while ((digit = number_mpq_match_encoded_digit(&p, table)) >= 0)
        digits[len++] = (char)('0' + digit);
    if (len == 0u) {
        free(digits);
        return NULL;
    }
    digits[len] = '\0';
    *text = p;
    return digits;
}

static int number_mpq_parse_glyph_fraction(mpq_t value,
                                           const char *text,
                                           bool negative)
{
    for (size_t i = 0u;
         i < sizeof(number_mpq_fraction_glyphs) / sizeof(number_mpq_fraction_glyphs[0]);
         ++i) {
        if (strcmp(text, number_mpq_fraction_glyphs[i].glyph) == 0) {
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
                                             const char *text,
                                             bool negative)
{
    static const char fraction_slash[] = "⁄";
    char *num_text;
    char *den_text;
    const char *p = text;
    int rc = -1;

    num_text = number_mpq_decode_digit_sequence(&p, number_mpq_superscript_digits);
    if (!num_text)
        return -1;
    if (strncmp(p, fraction_slash, strlen(fraction_slash)) != 0)
        goto cleanup_num;
    p += strlen(fraction_slash);
    den_text = number_mpq_decode_digit_sequence(&p, number_mpq_subscript_digits);
    if (!den_text)
        goto cleanup_num;
    if (*p != '\0')
        goto cleanup_den;
    if (mpz_set_str(mpq_numref(value), num_text, 10) != 0 ||
        mpz_set_str(mpq_denref(value), den_text, 10) != 0 ||
        mpz_sgn(mpq_denref(value)) == 0)
        goto cleanup_den;
    if (negative)
        mpz_neg(mpq_numref(value), mpq_numref(value));
    mpq_canonicalize(value);
    rc = 0;

cleanup_den:
    free(den_text);
cleanup_num:
    free(num_text);
    return rc;
}

static int number_mpq_set_unicode_fraction(mpq_t value, const char *text)
{
    bool negative = false;

    if (!text || *text == '\0')
        return -1;
    if (*text == '-') {
        negative = true;
        ++text;
    }
    if (*text == '\0')
        return -1;
    if (number_mpq_parse_glyph_fraction(value, text, negative) == 0)
        return 0;
    return number_mpq_parse_stacked_fraction(value, text, negative);
}

static char *number_mpq_format_glyph_fraction(bool negative, const char *glyph)
{
    size_t glyph_len = strlen(glyph);
    char *text = malloc((negative ? 1u : 0u) + glyph_len + 1u);

    if (!text)
        return NULL;
    if (negative) {
        text[0] = '-';
        memcpy(text + 1, glyph, glyph_len + 1u);
    } else {
        memcpy(text, glyph, glyph_len + 1u);
    }
    return text;
}

static char *number_mpq_format_stacked_fraction(bool negative,
                                                mpz_srcptr numerator,
                                                mpz_srcptr denominator)
{
    static const char fraction_slash[] = "⁄";
    char *num_text;
    char *den_text;
    char *text;
    char *out;
    size_t len;

    num_text = mpz_get_str(NULL, 10, numerator);
    if (!num_text)
        return NULL;
    den_text = mpz_get_str(NULL, 10, denominator);
    if (!den_text) {
        free(num_text);
        return NULL;
    }

    len = (negative ? 1u : 0u) +
        number_mpq_digit_table_len(num_text, number_mpq_superscript_digits) +
        strlen(fraction_slash) +
        number_mpq_digit_table_len(den_text, number_mpq_subscript_digits);

    text = malloc(len + 1u);
    if (!text) {
        free(num_text);
        free(den_text);
        return NULL;
    }

    out = text;
    if (negative)
        *out++ = '-';
    out = number_mpq_append_digit_table(out, num_text, number_mpq_superscript_digits);
    memcpy(out, fraction_slash, strlen(fraction_slash));
    out += strlen(fraction_slash);
    out = number_mpq_append_digit_table(out, den_text, number_mpq_subscript_digits);
    *out = '\0';

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

number_mpq_t *number_mpq_from_string(const char *text)
{
    number_mpq_t *out = number_mpq_new();
    int rc;

    if (!text || !out)
        return NULL;
    rc = (strchr(text, '/') != NULL) ? mpq_set_str(out->value, text, 10)
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

char *number_mpq_to_string(const number_mpq_t *value)
{
    const char *glyph;
    char *text;
    bool negative;
    mpz_t numerator;

    if (number_mpq_ensure(value) != 0)
        return NULL;
    if (mpz_cmp_ui(mpq_denref(value->value), 1u) == 0)
        return mpz_get_str(NULL, 10, mpq_numref(value->value));

    negative = mpq_sgn(value->value) < 0;
    mpz_init(numerator);
    mpz_abs(numerator, mpq_numref(value->value));
    glyph = number_mpq_fraction_glyph_lookup(numerator, mpq_denref(value->value));
    text = glyph ? number_mpq_format_glyph_fraction(negative, glyph)
                 : number_mpq_format_stacked_fraction(
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
