/**
 * @file qfloat_format_decimal.c
 * @brief Exact native decimal presentation of the two binary64 qfloat components.
 *
 * Converts the finite binary components to a common decimal integer using bounded base-10 limbs. Fixed,
 * scientific and general output then round that exact value to nearest with ties to even. This is used by
 * typed formatting and does not change the established q/Q printf conventions or depend on MPFR/MPC.
 */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "string-t.h"
#include "qfloat-t.h"
#define MARS_SHARED_QFLOAT_FORMAT_INTERNAL_ACCESS
#include "internal/qfloat_format_internal.h"

/* Two finite binary64 components require at most 1384 decimal coefficient digits, including a final carry. */
enum { QF_DECIMAL_LIMBS = 180, QF_DECIMAL_DIGITS = QF_DECIMAL_LIMBS * 9 };
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021,
               "native decimal formatting requires binary64 double");

typedef struct {
    uint32_t limb[QF_DECIMAL_LIMBS];
    size_t count;
} qf_decimal_integer_t;

static void qf_decimal_multiply(qf_decimal_integer_t *integer, unsigned factor)
{
    uint64_t carry = 0;
    for (size_t i = 0; i < integer->count; ++i) {
        uint64_t product = (uint64_t)integer->limb[i] * factor + carry;
        integer->limb[i] = (uint32_t)(product % UINT64_C(1000000000));
        carry = product / UINT64_C(1000000000);
    }
    if (carry)
        integer->limb[integer->count++] = (uint32_t)carry;
}

static uint64_t qf_decimal_component(double value, int *exponent)
{
    if (value == 0.0) {
        *exponent = 0;
        return 0;
    }
    double fraction = frexp(fabs(value), exponent);
    uint64_t mantissa = (uint64_t)ldexp(fraction, 53);
    *exponent -= 53;
    while ((mantissa & 1u) == 0u) {
        mantissa >>= 1;
        ++*exponent;
    }
    return mantissa;
}

static qf_decimal_integer_t qf_decimal_scaled(uint64_t mantissa, int exponent, int scale)
{
    qf_decimal_integer_t result = {.count = 1u};
    result.limb[0] = (uint32_t)(mantissa % UINT64_C(1000000000));
    if (mantissa >= UINT64_C(1000000000)) {
        result.limb[1] = (uint32_t)(mantissa / UINT64_C(1000000000));
        result.count = 2u;
    }
    if (mantissa) {
        for (int i = 0; i < scale; ++i)
            qf_decimal_multiply(&result, 5u);
        for (int i = 0; i < exponent + scale; ++i)
            qf_decimal_multiply(&result, 2u);
    }
    return result;
}

static int qf_decimal_compare(const qf_decimal_integer_t *a, const qf_decimal_integer_t *b)
{
    if (a->count != b->count)
        return a->count > b->count ? 1 : -1;
    for (size_t i = a->count; i > 0u; --i) {
        if (a->limb[i - 1u] != b->limb[i - 1u])
            return a->limb[i - 1u] > b->limb[i - 1u] ? 1 : -1;
    }
    return 0;
}

static void qf_decimal_combine(qf_decimal_integer_t *a, const qf_decimal_integer_t *b, bool subtract)
{
    size_t count = a->count > b->count ? a->count : b->count;
    int64_t carry = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t digit = (int64_t)a->limb[i] + carry;
        digit += subtract ? -(int64_t)b->limb[i] : (int64_t)b->limb[i];
        carry = 0;
        if (digit < 0) {
            digit += INT64_C(1000000000);
            carry = -1;
        } else if (digit >= INT64_C(1000000000)) {
            digit -= INT64_C(1000000000);
            carry = 1;
        }
        a->limb[i] = (uint32_t)digit;
    }
    a->count = count;
    if (carry > 0)
        a->limb[a->count++] = (uint32_t)carry;
    while (a->count > 1u && a->limb[a->count - 1u] == 0u)
        --a->count;
}

/* Supply an exact decimal coefficient to module-owned formatting adapters. */
int qfi_decimal_components(qfloat_t value, char *digits, int *point, bool *negative)
{
    int high_exponent, low_exponent;
    uint64_t high = qf_decimal_component(value.hi, &high_exponent);
    uint64_t low = qf_decimal_component(value.lo, &low_exponent);
    int minimum = high && low ? (high_exponent < low_exponent ? high_exponent : low_exponent)
                              : high ? high_exponent : low_exponent;
    int scale = minimum < 0 ? -minimum : 0;
    qf_decimal_integer_t a = qf_decimal_scaled(high, high_exponent, scale);
    qf_decimal_integer_t b = qf_decimal_scaled(low, low_exponent, scale);
    bool high_negative = signbit(value.hi) != 0;
    bool low_negative = signbit(value.lo) != 0;
    *negative = high_negative;
    if (high_negative == low_negative) {
        qf_decimal_combine(&a, &b, false);
    } else if (qf_decimal_compare(&a, &b) >= 0) {
        qf_decimal_combine(&a, &b, true);
    } else {
        qf_decimal_combine(&b, &a, true);
        a = b;
        *negative = low_negative;
    }
    if (a.count == 1u && a.limb[0] == 0u) {
        digits[0] = '0';
        digits[1] = '\0';
        *point = 1;
        *negative = high == 0u && low == 0u && qf_signbit(value);
        return 1;
    }
    int length = 0;
    for (size_t i = a.count; i > 0u; --i) {
        uint32_t limb = a.limb[i - 1u];
        char chunk[9];
        for (int j = 8; j >= 0; --j) {
            chunk[j] = (char)('0' + limb % 10u);
            limb /= 10u;
        }
        int start = 0;
        if (i == a.count) {
            while (start < 8 && chunk[start] == '0')
                ++start;
        }
        memcpy(digits + length, chunk + start, (size_t)(9 - start));
        length += 9 - start;
    }
    *point = length - scale;
    while (length > 1 && scale > 0 && digits[length - 1] == '0') {
        --length;
        --scale;
    }
    digits[length] = '\0';
    return length;
}

static void qf_decimal_round(char *digits, int *length, int *point, int keep)
{
    if (keep >= *length)
        return;
    if (keep < 0) {
        digits[0] = '0';
        *length = 1;
        *point = 1;
        return;
    }
    bool after_half = false;
    for (int i = keep + 1; i < *length; ++i)
        after_half = after_half || digits[i] != '0';
    bool increment = digits[keep] > '5' ||
                     (digits[keep] == '5' && (after_half || (keep > 0 && (digits[keep - 1] - '0') % 2 != 0)));
    if (keep == 0) {
        digits[0] = increment ? '1' : '0';
        *length = 1;
        *point = increment ? *point + 1 : 1;
        return;
    }
    *length = keep;
    if (!increment)
        return;
    int i = keep - 1;
    while (i >= 0 && digits[i] == '9')
        digits[i--] = '0';
    if (i >= 0) {
        ++digits[i];
    } else {
        digits[0] = '1';
        ++*point;
    }
}

static char qf_decimal_at(const char *digits, int length, int index)
{
    return index >= 0 && index < length ? digits[index] : '0';
}

static bool qf_decimal_exponent(string_t *out, char marker, int exponent)
{
    char digits[16];
    size_t length = 0;
    unsigned magnitude = (unsigned)(exponent < 0 ? -exponent : exponent);
    do {
        digits[length++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude);
    if (length < 2u)
        digits[length++] = '0';
    if (string_append_char(out, marker) != 0 || string_append_char(out, exponent < 0 ? '-' : '+') != 0)
        return false;
    while (length) {
        if (string_append_char(out, digits[--length]) != 0)
            return false;
    }
    return true;
}

/* Render the exact sum of the native components with checked, decimal presentation options. */
string_t *qfi_format_decimal_text(qfloat_t value, const qfi_format_options_t *options)
{
    if (!options || options->precision > 10000 || !options->conversion ||
        !strchr("fFeEgG", options->conversion))
        return NULL;
    char conversion = options->conversion;
    bool upper = conversion >= 'A' && conversion <= 'Z';
    bool general = conversion == 'g' || conversion == 'G';
    bool scientific = conversion == 'e' || conversion == 'E';
    int precision = options->precision < 0 ? 6 : options->precision;
    bool negative = qf_signbit(value);
    string_t *out = string_new();
    if (!out)
        return NULL;
    bool high_infinite = isinf(value.hi) != 0;
    bool low_infinite = isinf(value.lo) != 0;
    bool is_nan = isnan(value.hi) || isnan(value.lo) ||
                  (high_infinite && low_infinite && !!signbit(value.hi) != !!signbit(value.lo));
    if (is_nan || high_infinite || low_infinite) {
        negative = !isfinite(value.hi) ? signbit(value.hi) != 0 : signbit(value.lo) != 0;
        if ((negative && string_append_char(out, '-') != 0) ||
            (!negative && options->flag_plus && string_append_char(out, '+') != 0) ||
            (!negative && !options->flag_plus && options->flag_space && string_append_char(out, ' ') != 0) ||
            string_append_cstr(out, is_nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf")) != 0)
            goto fail;
        return out;
    }
    char digits[QF_DECIMAL_DIGITS + 1];
    int point;
    int length = qfi_decimal_components(value, digits, &point, &negative);
    if (general && precision == 0)
        precision = 1;
    qf_decimal_round(digits, &length, &point, general ? precision : scientific ? precision + 1 : point + precision);
    if (general) {
        scientific = point - 1 < -4 || point - 1 >= precision;
        precision = scientific ? precision - 1 : precision > point ? precision - point : 0;
    }
    int fractional = precision;
    if (general && !options->flag_hash) {
        int first = scientific ? 1 : point;
        if (fractional > length - first)
            fractional = length > first ? length - first : 0;
        while (fractional > 0 && qf_decimal_at(digits, length, first + fractional - 1) == '0')
            --fractional;
    }
    if ((negative && string_append_char(out, '-') != 0) ||
        (!negative && options->flag_plus && string_append_char(out, '+') != 0) ||
        (!negative && !options->flag_plus && options->flag_space && string_append_char(out, ' ') != 0))
        goto fail;
    int whole = scientific ? 1 : point > 0 ? point : 1;
    for (int i = 0; i < whole; ++i) {
        char digit = !scientific && point <= 0 ? '0' : qf_decimal_at(digits, length, i);
        if (string_append_char(out, digit) != 0)
            goto fail;
    }
    if ((fractional || options->flag_hash) && string_append_char(out, '.') != 0)
        goto fail;
    for (int i = 0; i < fractional; ++i) {
        if (string_append_char(out, qf_decimal_at(digits, length, (scientific ? 1 : point) + i)) != 0)
            goto fail;
    }
    if (scientific && !qf_decimal_exponent(out, upper ? 'E' : 'e', point - 1))
        goto fail;
    return out;

fail:
    string_free(out);
    return NULL;
}
