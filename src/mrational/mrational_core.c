#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "internal/mint_internal.h"
#include "mrational_internal.h"

static void mrational_prepare_constant(const mrational_t *rational)
{
    if (rational && rational->constant_id != MRCONST_NONE)
        mrational_constant_ensure(rational);
}

static void mint_prepare_constant(const mint_t *mint)
{
    if (mint && mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
}

static int mrational_alloc_views(mrational_t *rational)
{
    if (!rational->numerator_view) {
        rational->numerator_view = mi_new();
        if (!rational->numerator_view)
            return -1;
    }

    if (!rational->denominator_view) {
        rational->denominator_view = mi_new();
        if (!rational->denominator_view)
            return -1;
    }

    return 0;
}

static mrational_t *mrational_alloc(void)
{
    mrational_t *rational;

    rational = calloc(1u, sizeof(*rational));
    if (!rational)
        return NULL;

    rational->constant_id = MRCONST_NONE;
    mpq_init(rational->value);
    mpq_set_si(rational->value, 0, 1);
    return rational;
}

static char *mrational_trimmed_copy(const char *text)
{
    const unsigned char *start;
    const unsigned char *end;
    size_t len;
    char *copy;

    if (!text)
        return NULL;

    start = (const unsigned char *)text;
    while (*start && isspace(*start))
        ++start;

    end = start + strlen((const char *)start);
    while (end > start && isspace(end[-1]))
        --end;

    if (*start == '+')
        ++start;

    len = (size_t)(end - start);
    copy = malloc(len + 1u);
    if (!copy)
        return NULL;

    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

typedef struct mrational_fraction_glyph_t {
    unsigned long numerator;
    unsigned long denominator;
    const char *glyph;
} mrational_fraction_glyph_t;

static const mrational_fraction_glyph_t mrational_fraction_glyphs[] = {
    { 1ul, 2ul, "½"  },
    { 1ul, 3ul, "⅓"  },
    { 2ul, 3ul, "⅔"  },
    { 1ul, 4ul, "¼"  },
    { 3ul, 4ul, "¾"  },
    { 1ul, 5ul, "⅕"  },
    { 2ul, 5ul, "⅖"  },
    { 3ul, 5ul, "⅗"  },
    { 4ul, 5ul, "⅘"  },
    { 1ul, 6ul, "⅙"  },
    { 5ul, 6ul, "⅚"  },
    { 1ul, 7ul, "⅐"  },
    { 1ul, 8ul, "⅛"  },
    { 3ul, 8ul, "⅜"  },
    { 5ul, 8ul, "⅝"  },
    { 7ul, 8ul, "⅞"  },
    { 1ul, 9ul, "⅑"  },
    { 1ul, 10ul, "⅒" },
};

static const char *const mrational_superscript_digits[10] = {
    "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹",
};

static const char *const mrational_subscript_digits[10] = {
    "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉",
};

static const char *mrational_fraction_glyph_lookup(
    const mpz_t numerator,
    const mpz_t denominator)
{
    unsigned long n;
    unsigned long d;
    size_t i;

    if (!mpz_fits_ulong_p(numerator) || !mpz_fits_ulong_p(denominator))
        return NULL;

    n = mpz_get_ui(numerator);
    d = mpz_get_ui(denominator);

    for (i = 0u; i < sizeof(mrational_fraction_glyphs) /
                         sizeof(mrational_fraction_glyphs[0]); ++i) {
        if (mrational_fraction_glyphs[i].numerator == n &&
            mrational_fraction_glyphs[i].denominator == d)
            return mrational_fraction_glyphs[i].glyph;
    }

    return NULL;
}

static size_t mrational_digit_table_len(
    const char *digits,
    const char *const table[10])
{
    size_t len = 0u;

    for (; *digits; ++digits)
        len += strlen(table[(unsigned int)(*digits - '0')]);

    return len;
}

static char *mrational_append_digit_table(
    char *out,
    const char *digits,
    const char *const table[10])
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

static char *mrational_format_glyph_fraction(int negative, const char *glyph)
{
    size_t glyph_len;
    char *text;

    glyph_len = strlen(glyph);
    text = malloc((negative ? 1u : 0u) + glyph_len + 1u);
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

static char *mrational_format_stacked_fraction(
    int negative,
    const mpz_t numerator,
    const mpz_t denominator)
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
          mrational_digit_table_len(num_text, mrational_superscript_digits) +
          strlen(fraction_slash) +
          mrational_digit_table_len(den_text, mrational_subscript_digits);

    text = malloc(len + 1u);
    if (!text) {
        free(num_text);
        free(den_text);
        return NULL;
    }

    out = text;
    if (negative)
        *out++ = '-';
    out = mrational_append_digit_table(out, num_text, mrational_superscript_digits);
    memcpy(out, fraction_slash, strlen(fraction_slash));
    out += strlen(fraction_slash);
    out = mrational_append_digit_table(out, den_text, mrational_subscript_digits);
    *out = '\0';

    free(num_text);
    free(den_text);
    return text;
}

static int mrational_match_encoded_digit(
    const char **text,
    const char *const table[10])
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

static char *mrational_decode_digit_sequence(
    const char **text,
    const char *const table[10])
{
    const char *p = *text;
    size_t cap = strlen(p) + 1u;
    size_t len = 0u;
    char *digits = malloc(cap);
    int digit;

    if (!digits)
        return NULL;

    while ((digit = mrational_match_encoded_digit(&p, table)) >= 0)
        digits[len++] = (char)('0' + digit);

    if (len == 0u) {
        free(digits);
        return NULL;
    }

    digits[len] = '\0';
    *text = p;
    return digits;
}

static int mrational_parse_glyph_fraction(mpq_t value, const char *text, int negative)
{
    size_t i;

    for (i = 0u; i < sizeof(mrational_fraction_glyphs) /
                         sizeof(mrational_fraction_glyphs[0]); ++i) {
        if (strcmp(text, mrational_fraction_glyphs[i].glyph) == 0) {
            mpq_set_ui(value,
                       mrational_fraction_glyphs[i].numerator,
                       mrational_fraction_glyphs[i].denominator);
            if (negative)
                mpz_neg(mpq_numref(value), mpq_numref(value));
            mpq_canonicalize(value);
            return 0;
        }
    }

    return -1;
}

static int mrational_parse_stacked_fraction(mpq_t value, const char *text, int negative)
{
    static const char fraction_slash[] = "⁄";
    char *num_text;
    char *den_text;
    const char *p = text;
    int rc = -1;

    num_text = mrational_decode_digit_sequence(&p, mrational_superscript_digits);
    if (!num_text)
        return -1;

    if (strncmp(p, fraction_slash, strlen(fraction_slash)) != 0)
        goto cleanup_num;
    p += strlen(fraction_slash);

    den_text = mrational_decode_digit_sequence(&p, mrational_subscript_digits);
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

static int mrational_parse_unicode_fraction(mpq_t value, const char *text)
{
    int negative = 0;

    if (!text || *text == '\0')
        return -1;

    if (*text == '-') {
        negative = 1;
        ++text;
    }

    if (*text == '\0')
        return -1;

    if (mrational_parse_glyph_fraction(value, text, negative) == 0)
        return 0;

    return mrational_parse_stacked_fraction(value, text, negative);
}

int mrational_prepare_mutable(mrational_t *rational)
{
    if (!rational || rational->constant_id != MRCONST_NONE)
        return -1;
    return 0;
}

int mrational_sync_views(mrational_t *rational)
{
    if (!rational)
        return -1;

    mrational_prepare_constant(rational);
    if (mrational_alloc_views(rational) != 0)
        return -1;

    mpz_set(rational->numerator_view->value, mpq_numref(rational->value));
    mpz_set(rational->denominator_view->value, mpq_denref(rational->value));
    return 0;
}

mrational_t *mr_new(void)
{
    return mrational_alloc();
}

mrational_t *mr_const(const mrational_t *constant)
{
    return mr_clone(constant);
}

mrational_t *mr_create_long(long value)
{
    mrational_t *rational;

    rational = mrational_alloc();
    if (!rational)
        return NULL;

    if (mr_set_long(rational, value) != 0) {
        mr_free(rational);
        return NULL;
    }

    return rational;
}

mrational_t *mr_create_frac_long(long numerator, long denominator)
{
    mrational_t *rational;

    rational = mrational_alloc();
    if (!rational)
        return NULL;

    if (mr_set_frac_long(rational, numerator, denominator) != 0) {
        mr_free(rational);
        return NULL;
    }

    return rational;
}

mrational_t *mr_create_mints(const mint_t *numerator, const mint_t *denominator)
{
    mrational_t *rational;

    rational = mrational_alloc();
    if (!rational)
        return NULL;

    if (mr_set_mints(rational, numerator, denominator) != 0) {
        mr_free(rational);
        return NULL;
    }

    return rational;
}

mrational_t *mr_create_string(const char *text)
{
    mrational_t *rational;

    rational = mrational_alloc();
    if (!rational)
        return NULL;

    if (mr_set_string(rational, text) != 0) {
        mr_free(rational);
        return NULL;
    }

    return rational;
}

mrational_t *mr_clone(const mrational_t *rational)
{
    mrational_t *copy;

    if (!rational)
        return NULL;

    mrational_prepare_constant(rational);
    copy = mrational_alloc();
    if (!copy)
        return NULL;

    mpq_set(copy->value, rational->value);
    return copy;
}

void mr_free(mrational_t *rational)
{
    if (!rational || rational->constant_id != MRCONST_NONE)
        return;

    mi_free(rational->numerator_view);
    mi_free(rational->denominator_view);
    mpq_clear(rational->value);
    free(rational);
}

void mr_clear(mrational_t *rational)
{
    if (mrational_prepare_mutable(rational) != 0)
        return;
    mpq_set_si(rational->value, 0, 1);
}

int mr_set_long(mrational_t *rational, long value)
{
    if (mrational_prepare_mutable(rational) != 0)
        return -1;

    mpq_set_si(rational->value, value, 1);
    return 0;
}

int mr_set_frac_long(mrational_t *rational, long numerator, long denominator)
{
    unsigned long den_mag;

    if (mrational_prepare_mutable(rational) != 0 || denominator == 0)
        return -1;

    den_mag = denominator > 0 ? (unsigned long)denominator
                              : (unsigned long)(-(denominator + 1L)) + 1ul;
    mpq_set_si(rational->value, numerator, den_mag);
    if (denominator < 0)
        mpz_neg(mpq_numref(rational->value), mpq_numref(rational->value));
    mpq_canonicalize(rational->value);
    return 0;
}

int mr_set_mints(mrational_t *rational, const mint_t *numerator, const mint_t *denominator)
{
    if (mrational_prepare_mutable(rational) != 0 || !numerator || !denominator)
        return -1;

    mint_prepare_constant(numerator);
    mint_prepare_constant(denominator);
    if (mpz_sgn(denominator->value) == 0)
        return -1;

    mpz_set(mpq_numref(rational->value), numerator->value);
    mpz_set(mpq_denref(rational->value), denominator->value);
    mpq_canonicalize(rational->value);
    return 0;
}

int mr_set_string(mrational_t *rational, const char *text)
{
    char *copy;
    int rc;

    if (mrational_prepare_mutable(rational) != 0 || !text)
        return -1;

    copy = mrational_trimmed_copy(text);
    if (!copy)
        return -1;

    rc = mpq_set_str(rational->value, copy, 10);
    if (rc != 0)
        rc = mrational_parse_unicode_fraction(rational->value, copy);
    free(copy);
    if (rc != 0)
        return -1;
    if (mpz_sgn(mpq_denref(rational->value)) == 0) {
        mpq_set_si(rational->value, 0, 1);
        return -1;
    }

    mpq_canonicalize(rational->value);
    return 0;
}

char *mr_to_string(const mrational_t *rational)
{
    const char *glyph;
    char *text;
    int negative;
    mpz_t numerator;

    mrational_prepare_constant(rational);
    if (!rational)
        return NULL;

    if (mpz_cmp_ui(mpq_denref(rational->value), 1u) == 0)
        return mpz_get_str(NULL, 10, mpq_numref(rational->value));

    negative = mpq_sgn(rational->value) < 0;
    mpz_init(numerator);
    mpz_abs(numerator, mpq_numref(rational->value));

    glyph = mrational_fraction_glyph_lookup(numerator, mpq_denref(rational->value));
    text = glyph ? mrational_format_glyph_fraction(negative, glyph)
                 : mrational_format_stacked_fraction(
                       negative, numerator, mpq_denref(rational->value));

    mpz_clear(numerator);
    return text;
}

bool mr_is_zero(const mrational_t *rational)
{
    mrational_prepare_constant(rational);
    return rational && mpq_sgn(rational->value) == 0;
}

bool mr_is_integer(const mrational_t *rational)
{
    mrational_prepare_constant(rational);
    return rational && mpz_cmp_ui(mpq_denref(rational->value), 1u) == 0;
}

const mint_t *mr_numerator(const mrational_t *rational)
{
    if (!rational)
        return NULL;
    if (mrational_sync_views((mrational_t *)rational) != 0)
        return NULL;
    return rational->numerator_view;
}

const mint_t *mr_denominator(const mrational_t *rational)
{
    if (!rational)
        return NULL;
    if (mrational_sync_views((mrational_t *)rational) != 0)
        return NULL;
    return rational->denominator_view;
}

int mr_cmp(const mrational_t *a, const mrational_t *b)
{
    if (!a || !b)
        return 0;
    mrational_prepare_constant(a);
    mrational_prepare_constant(b);
    return mpq_cmp(a->value, b->value);
}

bool mr_eq(const mrational_t *a, const mrational_t *b)
{
    return mr_cmp(a, b) == 0;
}

bool mr_lt(const mrational_t *a, const mrational_t *b)
{
    return mr_cmp(a, b) < 0;
}

bool mr_le(const mrational_t *a, const mrational_t *b)
{
    return mr_cmp(a, b) <= 0;
}

bool mr_gt(const mrational_t *a, const mrational_t *b)
{
    return mr_cmp(a, b) > 0;
}

bool mr_ge(const mrational_t *a, const mrational_t *b)
{
    return mr_cmp(a, b) >= 0;
}

int mr_neg(mrational_t *rational)
{
    if (mrational_prepare_mutable(rational) != 0)
        return -1;
    mpq_neg(rational->value, rational->value);
    return 0;
}

int mr_abs(mrational_t *rational)
{
    if (mrational_prepare_mutable(rational) != 0)
        return -1;
    mpq_abs(rational->value, rational->value);
    return 0;
}

int mr_inv(mrational_t *rational)
{
    if (mrational_prepare_mutable(rational) != 0 || mpq_sgn(rational->value) == 0)
        return -1;

    mpq_inv(rational->value, rational->value);
    return 0;
}

int mr_add(mrational_t *rational, const mrational_t *other)
{
    if (mrational_prepare_mutable(rational) != 0 || !other)
        return -1;

    mrational_prepare_constant(other);
    mpq_add(rational->value, rational->value, other->value);
    return 0;
}

int mr_sub(mrational_t *rational, const mrational_t *other)
{
    if (mrational_prepare_mutable(rational) != 0 || !other)
        return -1;

    mrational_prepare_constant(other);
    mpq_sub(rational->value, rational->value, other->value);
    return 0;
}

int mr_mul(mrational_t *rational, const mrational_t *other)
{
    if (mrational_prepare_mutable(rational) != 0 || !other)
        return -1;

    mrational_prepare_constant(other);
    mpq_mul(rational->value, rational->value, other->value);
    return 0;
}

int mr_div(mrational_t *rational, const mrational_t *other)
{
    if (mrational_prepare_mutable(rational) != 0 || !other)
        return -1;

    mrational_prepare_constant(other);
    if (mpq_sgn(other->value) == 0)
        return -1;

    mpq_div(rational->value, rational->value, other->value);
    return 0;
}
