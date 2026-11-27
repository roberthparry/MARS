/**
 * @file format_excel_number.c
 * @brief Decimal-coefficient rendering for compiled Excel numeric and text sections.
 *
 * Keeps exact integer and native real coefficients in decimal storage rather than narrowing them to double.
 * Implements section selection, scaling, decimal and scientific placeholders, fractions and terminal colours.
 * C floating-point values are decomposed directly into their binary digits before decimal conversion.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

static string_t *fx_failure(fmt_error_t *error, fmt_code_t code, const char *message)
{
    fmt_error_set(error, code, SIZE_MAX, SIZE_MAX, message);
    return NULL;
}

bool fmt_excel_literal(string_t *out, const fmt_excel_t *plan, const fmt_excel_token_t *token, fmt_excel_fill_t *fill)
{
    if (token->kind == FX_SPACE)
        return string_append_char(out, ' ') == 0;
    if (token->kind == FX_FILL) {
        fill->token = token;
        fill->offset = string_byte_length(out);
        return true;
    }
    return string_append_bytes(out, plan->code + token->start, token->length) == 0;
}

string_t *fmt_excel_finish(string_t *text, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                           const fmt_options_t *options, fmt_excel_fill_t fill)
{
    if (text && fill.token && options->width > string_length(text)) {
        size_t count = options->width - string_length(text);
        string_t *out = string_new();
        const char *bytes = string_c_str(text);
        bool ok = out && string_append_bytes(out, bytes, fill.offset) == 0;
        for (size_t i = 0u; ok && i < count; ++i)
            ok = string_append_bytes(out, plan->code + fill.token->start, fill.token->length) == 0;
        if (ok)
            ok = string_append_bytes(out, bytes + fill.offset, string_byte_length(text) - fill.offset) == 0;
        string_free(text);
        text = out;
        if (!ok) {
            string_free(out);
            return NULL;
        }
    }
    if (!text || !*section->colour)
        return text;
    string_t *out = string_new_with(section->colour);
    if (out && (string_append_string(out, text) != 0 || string_append_cstr(out, "\033[0m") != 0)) {
        string_free(out);
        out = NULL;
    }
    string_free(text);
    return out;
}

static bool fx_zero(const char *digits)
{
    return strspn(digits, "0") == strlen(digits);
}

static int fx_compare(const char *a, int ap, bool an, const char *b, int bp, bool bn)
{
    bool az = fx_zero(a), bz = fx_zero(b);
    if (az && bz)
        return 0;
    an = an && !az;
    bn = bn && !bz;
    if (an != bn)
        return an ? -1 : 1;
    int comparison;
    if (az || bz)
        comparison = az ? -1 : 1;
    else if (ap != bp)
        comparison = ap < bp ? -1 : 1;
    else {
        size_t al = strlen(a), bl = strlen(b), count = al > bl ? al : bl;
        comparison = 0;
        for (size_t i = 0u; i < count; ++i) {
            char ac = i < al ? a[i] : '0', bc = i < bl ? b[i] : '0';
            if (ac != bc) {
                comparison = ac < bc ? -1 : 1;
                break;
            }
        }
    }
    return an ? -comparison : comparison;
}

static bool fx_condition_matches(const fmt_excel_section_t *section, const char *digits, int point, bool negative)
{
    int comparison = fx_compare(digits, point, negative, section->threshold, section->threshold_point,
                                section->threshold_negative);
    static const unsigned mask[7] = {0u, 1u, 3u, 2u, 5u, 6u, 4u};
    return (mask[section->condition] & (1u << (comparison + 1))) != 0u;
}

static const fmt_excel_section_t *fx_select(const fmt_excel_t *plan, const char *digits, int point,
                                           bool negative, bool *automatic_sign)
{
    bool zero = fx_zero(digits);
    bool conditions = plan->section[0].condition || (plan->sections > 1u && plan->section[1].condition);
    *automatic_sign = false;
    if (conditions) {
        unsigned count = plan->sections > 3u ? 3u : plan->sections;
        for (unsigned i = 0u; i < count; ++i) {
            const fmt_excel_section_t *section = &plan->section[i];
            if (section->condition ? fx_condition_matches(section, digits, point, negative) : true) {
                *automatic_sign = negative && !zero && (i == 0u || plan->sections == 1u);
                return section;
            }
        }
        return NULL;
    }
    unsigned index = zero && plan->sections >= 3u ? 2u : negative && !zero && plan->sections >= 2u ? 1u : 0u;
    *automatic_sign = negative && index == 0u && (plan->sections == 1u || !zero);
    return &plan->section[index];
}

static char fx_digit(const char *digits, size_t length, int index)
{
    return index >= 0 && (size_t)index < length ? digits[index] : '0';
}

static void fx_round(char *digits, size_t *length, int *point, int keep)
{
    if (keep >= 0 && (size_t)keep >= *length)
        return;
    bool increment = keep >= 0 && digits[keep] >= '5';
    if (keep <= 0) {
        digits[0] = increment ? '1' : '0';
        digits[1] = '\0';
        *length = 1u;
        *point = increment ? *point + 1 : 1;
        return;
    }
    *length = (size_t)keep;
    digits[*length] = '\0';
    if (!increment)
        return;
    int at = keep - 1;
    while (at >= 0 && digits[at] == '9')
        digits[at--] = '0';
    if (at >= 0)
        ++digits[at];
    else {
        digits[0] = '1';
        ++*point;
    }
}

static bool fx_append_unsigned(string_t *out, uint64_t value, unsigned minimum)
{
    char digits[32];
    unsigned count = 0u;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (count < minimum && count < sizeof(digits))
        digits[count++] = '0';
    while (count) {
        if (string_append_char(out, digits[--count]) != 0)
            return false;
    }
    return true;
}

static bool fx_group_digit(string_t *out, char digit, int right, bool grouping)
{
    return string_append_char(out, digit) == 0 &&
           (!grouping || right <= 0 || right % 3 != 0 || string_append_char(out, ',') == 0);
}

static bool fx_fixed(string_t *out, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                     char *digits, size_t length, int point, fmt_excel_fill_t *fill)
{
    bool scientific = section->exponent != SIZE_MAX;
    int exponent = 0;
    point += section->shift;
    if (scientific && !fx_zero(digits)) {
        unsigned whole = section->whole ? section->whole : 1u;
        exponent = point - 1;
        int remainder = exponent % (int)whole;
        if (remainder < 0)
            remainder += (int)whole;
        exponent -= remainder;
        point -= exponent;
    }
    fx_round(digits, &length, &point, point + (int)section->fractional);
    if (scientific && point > (int)section->whole) {
        exponent += (int)section->whole;
        point -= (int)section->whole;
    }
    bool zero = fx_zero(digits);
    size_t end = section->first + section->count;
    size_t integer_end = section->decimal != SIZE_MAX ? section->decimal :
                         scientific ? section->exponent : end;
    size_t integer_last = SIZE_MAX;
    bool grouping = false;
    for (size_t i = section->first; i < integer_end; ++i) {
        if (plan->tokens[i].kind == FX_DIGIT)
            integer_last = i;
    }
    if (integer_last != SIZE_MAX) {
        for (size_t i = section->first; i < integer_last; ++i)
            grouping = grouping || plan->tokens[i].kind == FX_COMMA;
    }
    int last_fraction = -1;
    unsigned slot = 0u;
    if (section->decimal != SIZE_MAX) {
        for (size_t i = section->decimal + 1u; i < (scientific ? section->exponent : end); ++i) {
            const fmt_excel_token_t *token = &plan->tokens[i];
            if (token->kind == FX_DIGIT) {
                if (plan->code[token->start] == '0' || fx_digit(digits, length, point + (int)slot) != '0')
                    last_fraction = (int)slot;
                ++slot;
            }
        }
    }
    unsigned integer_slot = 0u, fractional_slot = 0u;
    bool after_decimal = false, exponent_done = false;
    for (size_t i = section->first; i < end; ++i) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        char placeholder = plan->code[token->start];
        if (token->kind == FX_DIGIT && !exponent_done) {
            if (!after_decimal) {
                int index = point - (int)section->whole + (int)integer_slot;
                if (integer_slot++ == 0u && point > (int)section->whole && !zero) {
                    for (int j = 0; j < point - (int)section->whole; ++j) {
                        if (!fx_group_digit(out, fx_digit(digits, length, j), point - j - 1, grouping))
                            return false;
                    }
                }
                if (index >= 0 && index < point && !zero) {
                    if (!fx_group_digit(out, fx_digit(digits, length, index), point - index - 1, grouping))
                        return false;
                } else if (placeholder == '0') {
                    if (!fx_group_digit(out, '0', (int)section->whole - (int)integer_slot, grouping))
                        return false;
                } else if (placeholder == '?' && string_append_char(out, ' ') != 0) {
                    return false;
                }
            } else {
                char digit = fx_digit(digits, length, point + (int)fractional_slot);
                if ((int)fractional_slot <= last_fraction || placeholder == '0') {
                    if (string_append_char(out, digit) != 0)
                        return false;
                } else if (placeholder == '?' && string_append_char(out, ' ') != 0) {
                    return false;
                }
                ++fractional_slot;
            }
        } else if (token->kind == FX_DOT) {
            after_decimal = true;
            if (string_append_char(out, '.') != 0)
                return false;
        } else if (token->kind == FX_EXPONENT) {
            if (string_append_char(out, placeholder) != 0 ||
                ((exponent < 0 || plan->code[token->start + 1u] == '+') &&
                 string_append_char(out, exponent < 0 ? '-' : '+') != 0))
                return false;
            unsigned minimum = 0u;
            size_t next = i + 1u;
            while (next < end && plan->tokens[next].kind == FX_DIGIT) {
                if (plan->code[plan->tokens[next].start] == '0')
                    ++minimum;
                ++next;
            }
            if (!fx_append_unsigned(out, (uint64_t)(exponent < 0 ? -exponent : exponent), minimum))
                return false;
            i = next - 1u;
            exponent_done = true;
        } else if (token->kind != FX_COMMA && token->kind != FX_TEXT && token->kind != FX_GENERAL) {
            if (!fmt_excel_literal(out, plan, token, fill))
                return false;
        }
    }
    return true;
}

static long double fx_approximate(const char *digits, int point)
{
    long double value = 0.0L;
    size_t count = strlen(digits);
    if (count > LDBL_DECIMAL_DIG + 2u)
        count = LDBL_DECIMAL_DIG + 2u;
    for (size_t i = 0u; i < count; ++i)
        value = value * 10.0L + (long double)(digits[i] - '0');
    return value * powl(10.0L, (long double)point - (long double)count);
}

static void fx_best_fraction(long double value, uint64_t limit, uint64_t *numerator, uint64_t *denominator)
{
    uint64_t p0 = 0u, q0 = 1u, p1 = 1u, q1 = 0u;
    long double current = value;
    /* A convergent doubles its denominator at least every two iterations: 128 covers a 64-bit bound. */
    for (unsigned i = 0u; i < 128u; ++i) {
        long double whole = floorl(current);
        if (whole > (long double)UINT64_MAX)
            break;
        uint64_t a = (uint64_t)whole;
        if ((q1 && a > (limit - q0) / q1) || (p1 && a > (UINT64_MAX - p0) / p1))
            break;
        uint64_t p2 = p0 + a * p1, q2 = q0 + a * q1;
        p0 = p1;
        q0 = q1;
        p1 = p2;
        q1 = q2;
        long double fraction = current - whole;
        if (fraction <= LDBL_EPSILON)
            break;
        current = 1.0L / fraction;
    }
    if (!q1) {
        *numerator = 0u;
        *denominator = 1u;
        return;
    }
    uint64_t k = (limit - q0) / q1;
    if (!p1 || k <= (UINT64_MAX - p0) / p1) {
        uint64_t p = p0 + k * p1, q = q0 + k * q1;
        if (q && fabsl((long double)p / (long double)q - value) <
                 fabsl((long double)p1 / (long double)q1 - value)) {
            p1 = p;
            q1 = q;
        }
    }
    *numerator = p1;
    *denominator = q1;
}

static bool fx_fraction_digits(string_t *out, uint64_t value, const fmt_excel_t *plan, size_t first, size_t end)
{
    char digits[32];
    unsigned count = 0u;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    size_t slots = end - first;
    for (size_t i = count; i < slots; ++i) {
        char placeholder = plan->code[plan->tokens[first + slots - i - 1u].start];
        if (placeholder != '#' && string_append_char(out, placeholder == '?' ? ' ' : '0') != 0)
            return false;
    }
    while (count) {
        if (string_append_char(out, digits[--count]) != 0)
            return false;
    }
    return true;
}

static bool fx_fraction(string_t *out, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                        const char *digits, int point, fmt_error_t *error, fmt_excel_fill_t *fill)
{
    size_t slash = section->slash, end = section->first + section->count;
    size_t first = slash, last = slash + 1u;
    while (first > section->first && plan->tokens[first - 1u].kind == FX_DIGIT)
        --first;
    uint64_t denominator = 0u;
    unsigned denominator_slots = 0u;
    bool fixed = false;
    size_t denominator_end = slash + 1u;
    while (denominator_end < end) {
        const fmt_excel_token_t *token = &plan->tokens[denominator_end];
        char ch = plan->code[token->start];
        if (token->kind == FX_DIGIT || (token->kind == FX_LITERAL && ch >= '1' && ch <= '9')) {
            fixed = fixed || (ch >= '1' && ch <= '9');
            ++denominator_end;
        } else {
            break;
        }
    }
    while (last < end) {
        const fmt_excel_token_t *token = &plan->tokens[last];
        char ch = plan->code[token->start];
        if (token->kind == FX_DIGIT && !fixed) {
            ++denominator_slots;
        } else if ((token->kind == FX_LITERAL || token->kind == FX_DIGIT) && ch >= '0' && ch <= '9') {
            if (denominator > UINT64_C(100000000))
                return fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "fixed fraction denominator exceeds 1e9");
            denominator = denominator * 10u + (unsigned)(ch - '0');
        } else {
            break;
        }
        ++last;
    }
    if (first == slash || last == slash + 1u || denominator_slots > 9u)
        return fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX,
                             "fractions require numerator and one through nine denominator places");
    long double value = fx_approximate(digits, point + section->shift);
    if (!isfinite(value) || value > (long double)UINT64_MAX / 1000000000.0L)
        return fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "fraction integer part exceeds the supported range");
    bool mixed = false;
    for (size_t i = section->first; i < first; ++i)
        mixed = mixed || plan->tokens[i].kind == FX_DIGIT;
    uint64_t whole = mixed ? (uint64_t)floorl(value) : 0u;
    long double fraction = value - (long double)whole;
    uint64_t numerator;
    if (denominator) {
        numerator = (uint64_t)floorl(fraction * (long double)denominator + 0.5L);
    } else {
        uint64_t limit = 1u;
        for (unsigned i = 0u; i < denominator_slots; ++i)
            limit *= 10u;
        fx_best_fraction(fraction, limit - 1u, &numerator, &denominator);
    }
    if (mixed && numerator >= denominator) {
        whole += numerator / denominator;
        numerator %= denominator;
    }
    for (size_t i = section->first; i < end; ++i) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        if (i == first) {
            bool mandatory = false;
            for (size_t j = first; j < slash; ++j)
                mandatory = mandatory || plan->code[plan->tokens[j].start] == '0';
            if (!numerator && !mandatory) {
                for (size_t j = first; j < last; ++j) {
                    const fmt_excel_token_t *part = &plan->tokens[j];
                    if ((part->kind != FX_DIGIT || plan->code[part->start] != '#') &&
                        string_append_char(out, ' ') != 0)
                        return false;
                }
                i = last - 1u;
                continue;
            }
            if (!fx_fraction_digits(out, numerator, plan, first, slash) || string_append_char(out, '/') != 0)
                return false;
            if (denominator_slots) {
                if (!fx_fraction_digits(out, denominator, plan, slash + 1u, last))
                    return false;
            } else if (!fx_append_unsigned(out, denominator, 1u)) {
                return false;
            }
            i = last - 1u;
        } else if (token->kind == FX_DIGIT) {
            size_t next = i + 1u;
            while (next < first && plan->tokens[next].kind == FX_DIGIT)
                ++next;
            if ((whole || plan->code[token->start] == '0') && !fx_fraction_digits(out, whole, plan, i, next))
                return false;
            i = next - 1u;
        } else if (!fmt_excel_literal(out, plan, token, fill)) {
            return false;
        }
    }
    return true;
}

static bool fx_general(string_t *out, const char *digits, int point)
{
    size_t length = strlen(digits);
    while (length > 1u && digits[length - 1u] == '0')
        --length;
    if (point > 11 || point < -8) {
        if (string_append_char(out, digits[0]) != 0)
            return false;
        if (length > 1u && (string_append_char(out, '.') != 0 || string_append_bytes(out, digits + 1u, length - 1u) != 0))
            return false;
        return string_append_cstr(out, point <= 0 ? "E-" : "E+") == 0 &&
               fx_append_unsigned(out, (uint64_t)(point <= 0 ? 1 - point : point - 1), 2u);
    }
    if (point <= 0) {
        if (string_append_cstr(out, "0.") != 0)
            return false;
        for (int i = 0; i < -point; ++i) {
            if (string_append_char(out, '0') != 0)
                return false;
        }
    }
    int end = point > (int)length ? point : (int)length;
    for (int i = 0; i < end; ++i) {
        if (i > 0 && i == point && string_append_char(out, '.') != 0)
            return false;
        if (string_append_char(out, fx_digit(digits, length, i)) != 0)
            return false;
    }
    return true;
}

/* Render a native or exact decimal coefficient without a numeric string round trip. */
string_t *fmt_excel_decimal(const fmt_options_t *options, const char *digits, int point, bool negative, fmt_error_t *error)
{
    if (!options || !options->excel || !digits || !*digits || strspn(digits, "0123456789") != strlen(digits))
        return fx_failure(error, FMT_ERROR_ARGUMENT, "Excel rendering requires an unsigned decimal coefficient");
    if (point < -100000 || point > 100000 || strlen(digits) > 100000u)
        return fx_failure(error, FMT_ERROR_RANGE, "Excel decimal coefficient exceeds the supported range");
    while (digits[0] == '0' && digits[1]) {
        ++digits;
        --point;
    }
    bool automatic_sign;
    const fmt_excel_t *plan = options->excel;
    const fmt_excel_section_t *section = fx_select(plan, digits, point, negative, &automatic_sign);
    if (!section)
        return string_new_with("########");
    if (section->date) {
        long double serial = fx_approximate(digits, point);
        return fmt_excel_calendar(options, section, negative ? -serial : serial, error);
    }
    fmt_excel_fill_t fill = {0};
    string_t *out = string_new();
    char *rounded = strdup(digits);
    if (!out || !rounded)
        goto memory;
    if (automatic_sign && section->first_digit != SIZE_MAX && string_append_char(out, '-') != 0)
        goto memory;
    bool ok;
    if (section->slash != SIZE_MAX) {
        ok = fx_fraction(out, plan, section, digits, point, error, &fill);
    } else if (section->general) {
        if (automatic_sign && section->first_digit == SIZE_MAX && string_append_char(out, '-') != 0)
            goto memory;
        size_t length = strlen(rounded);
        if (options->precision > 0)
            fx_round(rounded, &length, &point, options->precision);
        ok = true;
        for (size_t i = section->first; ok && i < section->first + section->count; ++i) {
            const fmt_excel_token_t *token = &plan->tokens[i];
            ok = token->kind == FX_GENERAL ? fx_general(out, rounded, point + section->shift) :
                                            fmt_excel_literal(out, plan, token, &fill);
        }
    } else {
        ok = fx_fixed(out, plan, section, rounded, strlen(rounded), point, &fill);
    }
    free(rounded);
    if (!ok) {
        string_free(out);
        if (!error || error->code == FMT_OK)
            return fx_failure(error, FMT_ERROR_MEMORY, "cannot render Excel numeric section");
        return NULL;
    }
    return fmt_excel_finish(out, plan, section, options, fill);

memory:
    string_free(out);
    free(rounded);
    return fx_failure(error, FMT_ERROR_MEMORY, "cannot allocate Excel numeric output");
}

string_t *fmt_excel_text(const fmt_options_t *options, const string_t *value)
{
    const fmt_excel_t *plan = options->excel;
    const fmt_excel_section_t *section = plan->sections == 4u ? &plan->section[3] : &plan->section[0];
    bool text = false;
    for (size_t i = section->first; i < section->first + section->count; ++i)
        text = text || plan->tokens[i].kind == FX_TEXT;
    if (plan->sections < 4u && !text)
        return string_clone(value);
    string_t *out = string_new();
    fmt_excel_fill_t fill = {0};
    for (size_t i = section->first; out && i < section->first + section->count; ++i) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        bool ok = token->kind == FX_TEXT || token->kind == FX_GENERAL ? string_append_string(out, value) == 0 :
                                                                                   fmt_excel_literal(out, plan, token, &fill);
        if (!ok) {
            string_free(out);
            out = NULL;
        }
    }
    return fmt_excel_finish(out, plan, section, options, fill);
}

static bool fx_multiply(uint32_t *limbs, size_t *count, size_t capacity, unsigned multiplier, unsigned add)
{
    uint64_t carry = add;
    for (size_t i = 0u; i < *count; ++i) {
        uint64_t value = (uint64_t)limbs[i] * multiplier + carry;
        limbs[i] = (uint32_t)(value % UINT64_C(1000000000));
        carry = value / UINT64_C(1000000000);
    }
    if (carry) {
        if (*count == capacity)
            return false;
        limbs[(*count)++] = (uint32_t)carry;
    }
    return true;
}

/* Decompose the native long double exactly, preserving its complete binary significand and sign bit. */
string_t *fmt_excel_real(const fmt_options_t *options, long double value, fmt_error_t *error)
{
    if (!options || !options->excel)
        return fx_failure(error, FMT_ERROR_ARGUMENT, "Excel rendering requires compiled field options");
    if (!isfinite(value))
        return string_new_with(isnan(value) ? "#NUM!" : signbit(value) ? "-∞" : "∞");
    if (value == 0.0L)
        return fmt_excel_decimal(options, "0", 1, signbit(value) != 0, error);
    int exponent;
    long double fraction = frexpl(fabsl(value), &exponent);
    size_t capacity = (size_t)(LDBL_MANT_DIG + (exponent < 0 ? -exponent : exponent) + 32) / 9u + 4u;
    uint32_t *limbs = calloc(capacity, sizeof(*limbs));
    char *digits = malloc(capacity * 9u + 1u);
    if (!limbs || !digits) {
        free(limbs);
        free(digits);
        return fx_failure(error, FMT_ERROR_MEMORY, "cannot allocate native real coefficient");
    }
    size_t count = 1u;
    for (int i = 0; i < LDBL_MANT_DIG; ++i) {
        fraction *= 2.0L;
        unsigned bit = fraction >= 1.0L;
        fraction -= bit;
        fx_multiply(limbs, &count, capacity, 2u, bit);
    }
    exponent -= LDBL_MANT_DIG;
    int scale = exponent < 0 ? -exponent : 0;
    for (int i = 0; i < (exponent < 0 ? -exponent : exponent); ++i)
        fx_multiply(limbs, &count, capacity, exponent < 0 ? 5u : 2u, 0u);
    size_t length = 0u;
    for (size_t i = count; i > 0u; --i) {
        char chunk[9];
        uint32_t limb = limbs[i - 1u];
        for (int j = 8; j >= 0; --j) {
            chunk[j] = (char)('0' + limb % 10u);
            limb /= 10u;
        }
        size_t first = 0u;
        if (i == count) {
            while (first < 8u && chunk[first] == '0')
                ++first;
        }
        memcpy(digits + length, chunk + first, 9u - first);
        length += 9u - first;
    }
    digits[length] = '\0';
    int point = (int)length - scale;
    while (length > 1u && digits[length - 1u] == '0')
        digits[--length] = '\0';
    fmt_options_t native = *options;
    if (native.precision < 0)
        native.precision = LDBL_DECIMAL_DIG;
    string_t *out = fmt_excel_decimal(&native, digits, point, signbit(value) != 0, error);
    free(digits);
    free(limbs);
    return out;
}
