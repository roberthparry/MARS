/**
 * @file format_excel_decimal.c
 * @brief Render compiled decimal and scientific placeholders through token-kind dispatch.
 *
 * Uses immutable grouping, required precision and exponent metadata resolved by the compiler. Per-call state
 * contains only coefficient digits, rounding and output positions; rendering does not classify source spellings.
 */
#include <stdint.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

void fmt_excel_round(char *digits, size_t *length, int *point, int keep)
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

bool fmt_excel_append_unsigned(string_t *out, uint64_t value, size_t minimum)
{
    char digits[32];
    size_t first = sizeof(digits);
    do {
        digits[--first] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (sizeof(digits) - first < minimum && first)
        digits[--first] = '0';
    return string_append_bytes(out, digits + first, sizeof(digits) - first) == 0;
}

static bool fx_group_digit(string_t *out, char digit, int right, bool grouping)
{
    return string_append_char(out, digit) == 0 &&
           (!grouping || right <= 0 || right % 3 != 0 || string_append_char(out, ',') == 0);
}

typedef struct {
    string_t *out;
    const fmt_excel_t *plan;
    const fmt_excel_section_t *section;
    fmt_excel_fill_t *fill;
    const char *digits;
    size_t length;
    int point, exponent, last_fraction;
    unsigned integer_slot, fractional_slot;
    bool zero;
} fx_decimal_state_t;

typedef bool (*fx_decimal_fn)(fx_decimal_state_t *, const fmt_excel_token_t *);

static bool fx_decimal_integer(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    const fmt_excel_section_t *section = state->section;
    int index = state->point - (int)section->whole + (int)state->integer_slot;
    if (state->integer_slot++ == 0u && state->point > (int)section->whole && !state->zero) {
        for (int j = 0; j < state->point - (int)section->whole; ++j) {
            if (!fx_group_digit(state->out, fmt_excel_digit_at(state->digits, state->length, j),
                                state->point - j - 1, section->grouping))
                return false;
        }
    }
    if (index >= 0 && index < state->point && !state->zero)
        return fx_group_digit(state->out, fmt_excel_digit_at(state->digits, state->length, index),
                               state->point - index - 1, section->grouping);
    if (token->flags & FX_REQUIRED)
        return fx_group_digit(state->out, '0', (int)section->whole - (int)state->integer_slot, section->grouping);
    return !(token->flags & FX_RESERVED) || string_append_char(state->out, ' ') == 0;
}

static bool fx_decimal_fraction(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    unsigned slot = state->fractional_slot++;
    if ((int)slot <= state->last_fraction || (token->flags & FX_REQUIRED))
        return string_append_char(state->out, fmt_excel_digit_at(state->digits, state->length, state->point + (int)slot)) == 0;
    return !(token->flags & FX_RESERVED) || string_append_char(state->out, ' ') == 0;
}

static bool fx_decimal_point(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    (void)token;
    return string_append_char(state->out, '.') == 0;
}

static bool fx_decimal_exponent(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    if (string_append_char(state->out, state->plan->code[token->start]) != 0)
        return false;
    if ((state->exponent < 0 || (token->flags & FX_POSITIVE_EXPONENT)) &&
        string_append_char(state->out, state->exponent < 0 ? '-' : '+') != 0)
        return false;
    uint64_t magnitude = (uint64_t)(state->exponent < 0 ? -state->exponent : state->exponent);
    return fmt_excel_append_unsigned(state->out, magnitude, state->section->exponent_digits);
}

static bool fx_decimal_ignore(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    (void)state;
    (void)token;
    return true;
}

static bool fx_decimal_literal(fx_decimal_state_t *state, const fmt_excel_token_t *token)
{
    return fmt_excel_literal(state->out, state->plan, token, state->fill);
}

static int fx_last_fraction(const fmt_excel_section_t *section, const char *digits, size_t length, int point)
{
    int count = (int)length - point;
    if (count > (int)section->fractional)
        count = (int)section->fractional;
    while (count > (int)section->minimum_fraction && fmt_excel_digit_at(digits, length, point + count - 1) == '0')
        --count;
    if (count < (int)section->minimum_fraction)
        count = (int)section->minimum_fraction;
    return count - 1;
}

bool fmt_excel_fixed(string_t *out, const fmt_excel_t *plan, const fmt_excel_section_t *section,
                      char *digits, size_t length, int point, fmt_excel_fill_t *fill)
{
    static const fx_decimal_fn render[FX_KIND_COUNT] = {
        [FX_DOT]            = fx_decimal_point,
        [FX_COMMA]          = fx_decimal_ignore,
        [FX_EXPONENT]       = fx_decimal_exponent,
        [FX_TEXT]           = fx_decimal_ignore,
        [FX_GENERAL]        = fx_decimal_ignore,
        [FX_INTEGER_DIGIT]  = fx_decimal_integer,
        [FX_FRACTION_DIGIT] = fx_decimal_fraction,
    };
    bool scientific = section->exponent != SIZE_MAX;
    int exponent = 0;
    point += section->shift;
    if (scientific && digits[0] != '0') {
        unsigned whole = section->whole ? section->whole : 1u;
        exponent = point - 1;
        int remainder = exponent % (int)whole;
        if (remainder < 0)
            remainder += (int)whole;
        exponent -= remainder;
        point -= exponent;
    }
    fmt_excel_round(digits, &length, &point, point + (int)section->fractional);
    if (scientific && point > (int)section->whole) {
        exponent += (int)section->whole;
        point -= (int)section->whole;
    }
    fx_decimal_state_t state = {.out = out, .plan = plan, .section = section, .fill = fill,
        .digits = digits, .length = length, .point = point, .exponent = exponent, .zero = digits[0] == '0',
        .last_fraction = fx_last_fraction(section, digits, length, point)};
    for (size_t i = section->first; i < section->first + section->count;) {
        const fmt_excel_token_t *token = &plan->tokens[i];
        fx_decimal_fn consume = render[token->kind];
        if (!(consume ? consume : fx_decimal_literal)(&state, token))
            return false;
        i = token->next;
    }
    return true;
}
