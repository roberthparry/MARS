/**
 * @file format_excel_compile.c
 * @brief Resolve tokenised numeric and date masks into immutable rendering plans.
 *
 * Token-kind handlers collect numeric section properties. Calendar resolution fixes Excel's month/minute context,
 * fuses fractional-second runs and caches precision; renderers subsequently dispatch tokens without parsing masks.
 */
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_internal.h"
#define MARS_FORMAT_INTERNAL_ACCESS
#include "format_excel_internal.h"

static bool fx_error(fmt_error_t *error, const char *message)
{
    return fmt_error_set(error, FMT_ERROR_SYNTAX, SIZE_MAX, SIZE_MAX, message);
}

static bool fx_resolve_date(fmt_excel_t *plan, fmt_excel_section_t *section, fmt_error_t *error)
{
    unsigned previous = 0u;
    size_t end = section->first + section->count;
    for (size_t i = section->first; i < end; ++i) {
        fmt_excel_token_t *token = &plan->tokens[i];
        section->has_seconds |= token->kind == FX_SECOND || token->kind == FX_ELAPSED_SECOND ||
                                token->kind == FX_LONG_TIME;
        if (plan->excel_compatibility && token->kind == FX_MONTH && token->length <= 2u) {
            size_t next = i + 1u;
            while (next < end && !(plan->tokens[next].flags & FX_DATE_CONTEXT))
                ++next;
            bool next_second = next < end && (plan->tokens[next].flags & FX_SECOND_CONTEXT);
            if ((previous & FX_HOUR_CONTEXT) || next_second)
                token->kind = FX_MINUTE;
        }
        if ((token->flags & FX_DATE_CONTEXT) && token->kind != FX_AMPM)
            previous = token->flags;
        if (token->kind == FX_DIGIT && !(token->flags & FX_REQUIRED))
            return fx_error(error, "date fractional seconds require zero placeholders");
        if (token->kind == FX_DOT) {
            size_t next = i + 1u;
            while (next < end && plan->tokens[next].kind == FX_DIGIT)
                ++next;
            token->kind = FX_SUBSECOND;
            token->length = next - i - 1u;
            token->next = next;
            if (token->length > section->second_precision)
                section->second_precision = (unsigned)token->length;
        }
    }
    return true;
}

typedef struct {
    fmt_excel_t *plan;
    fmt_excel_section_t *section;
    fmt_error_t *error;
    bool after_decimal;
} fx_resolver_t;

typedef bool (*fx_resolve_fn)(fx_resolver_t *, fmt_excel_token_t *);

static bool fx_resolve_decimal(fx_resolver_t *state, fmt_excel_token_t *token)
{
    if (state->section->decimal != SIZE_MAX)
        return fx_error(state->error, "multiple decimal points in one numeric section");
    state->section->decimal = (size_t)(token - state->plan->tokens);
    state->after_decimal = true;
    return true;
}

static bool fx_resolve_digit(fx_resolver_t *state, fmt_excel_token_t *token)
{
    fmt_excel_section_t *section = state->section;
    size_t index = (size_t)(token - state->plan->tokens);
    if (section->first_digit == SIZE_MAX)
        section->first_digit = index;
    section->last_digit = index;
    if (section->exponent != SIZE_MAX || section->slash != SIZE_MAX)
        return true;
    if (state->after_decimal) {
        ++section->fractional;
        if (token->flags & FX_REQUIRED)
            section->minimum_fraction = section->fractional;
    } else {
        ++section->whole;
    }
    return true;
}

static bool fx_resolve_exponent(fx_resolver_t *state, fmt_excel_token_t *token)
{
    fmt_excel_section_t *section = state->section;
    if (section->exponent != SIZE_MAX || section->slash != SIZE_MAX)
        return fx_error(state->error, "multiple or mixed fraction and scientific fields");
    section->exponent = (size_t)(token - state->plan->tokens);
    size_t next = token->next, end = section->first + section->count;
    while (next < end && state->plan->tokens[next].kind == FX_DIGIT) {
        section->exponent_digits += (state->plan->tokens[next].flags & FX_REQUIRED) != 0u;
        ++next;
    }
    token->next = next;
    return true;
}

static bool fx_resolve_slash(fx_resolver_t *state, fmt_excel_token_t *token)
{
    fmt_excel_section_t *section = state->section;
    if (section->slash != SIZE_MAX || section->exponent != SIZE_MAX || section->decimal != SIZE_MAX)
        return fx_error(state->error, "invalid fraction field");
    section->slash = (size_t)(token - state->plan->tokens);
    return true;
}

static bool fx_resolve_percent(fx_resolver_t *state, fmt_excel_token_t *token)
{
    (void)token;
    state->section->shift += 2;
    return true;
}

static void fx_resolve_grouping(const fmt_excel_t *plan, fmt_excel_section_t *section)
{
    size_t end = section->first + section->count;
    size_t integer_end = section->decimal != SIZE_MAX ? section->decimal :
                         section->exponent != SIZE_MAX ? section->exponent : end;
    bool comma = false;
    for (size_t i = section->first; i < integer_end; ++i) {
        comma |= plan->tokens[i].kind == FX_COMMA;
        if (plan->tokens[i].kind == FX_DIGIT)
            section->grouping |= comma;
    }
    if (section->last_digit == SIZE_MAX || section->slash != SIZE_MAX)
        return;
    if (section->decimal != SIZE_MAX) {
        size_t first = section->decimal;
        while (first > section->first && plan->tokens[first - 1u].kind == FX_COMMA)
            --first;
        section->shift -= (int)(section->decimal - first) * 3;
    }
    for (size_t i = section->last_digit + 1u; i < end && plan->tokens[i].kind == FX_COMMA; ++i)
        section->shift -= 3;
}

static bool fx_resolve(fmt_excel_t *plan, fmt_excel_section_t *section, fmt_error_t *error)
{
    static const fx_resolve_fn resolve[FX_KIND_COUNT] = {
        [FX_DIGIT]    = fx_resolve_digit,
        [FX_DOT]      = fx_resolve_decimal,
        [FX_PERCENT]  = fx_resolve_percent,
        [FX_EXPONENT] = fx_resolve_exponent,
        [FX_SLASH]    = fx_resolve_slash,
    };
    if (section->date)
        return fx_resolve_date(plan, section, error);
    if (section->name_case_set)
        return fx_error(error, "calendar-name case directives require date fields");
    fx_resolver_t state = {plan, section, error, false};
    for (size_t i = section->first; i < section->first + section->count; ++i) {
        fmt_excel_token_t *token = &plan->tokens[i];
        if (resolve[token->kind] && !resolve[token->kind](&state, token))
            return false;
    }
    if (section->fractional > 10000u)
        return fx_error(error, "Excel precision exceeds 10000 places");
    if (section->exponent != SIZE_MAX) {
        size_t index = section->exponent, end = section->first + section->count;
        if (index == section->first || index + 1u >= end || plan->tokens[index + 1u].kind != FX_DIGIT)
            return fx_error(error, "scientific notation requires mantissa and exponent placeholders");
    }
    fx_resolve_grouping(plan, section);
    if (section->slash == SIZE_MAX) {
        for (size_t i = section->first; i < section->first + section->count; ++i) {
            fmt_excel_token_t *token = &plan->tokens[i];
            if (token->kind == FX_DIGIT)
                token->kind = section->exponent != SIZE_MAX && i > section->exponent ? FX_LITERAL :
                              section->decimal != SIZE_MAX && i > section->decimal ? FX_FRACTION_DIGIT :
                                                                                    FX_INTEGER_DIGIT;
        }
    }
    return true;
}

fmt_excel_t *fmt_excel_compile(const char *code, fmt_error_t *error)
{
    bool excel_compatibility = !fmt_excel_ascii_compare(code, "excel:", 6u);
    if (excel_compatibility)
        code += 6u;
    size_t length = strlen(code);
    if (length > 40000u) {
        fmt_error_set(error, FMT_ERROR_RANGE, SIZE_MAX, SIZE_MAX, "Excel custom code is too long");
        return NULL;
    }
    fmt_excel_t *plan = calloc(1u, sizeof(*plan));
    if (!plan)
        goto memory;
    plan->excel_compatibility = excel_compatibility;
    plan->length = length;
    plan->code = strdup(code);
    plan->tokens = calloc(length + 1u, sizeof(*plan->tokens));
    if (!plan->code || !plan->tokens)
        goto memory;
    size_t at = 0u;
    for (unsigned s = 0u; s < 4u; ++s) {
        fmt_excel_section_t *section = &plan->section[s];
        *section = (fmt_excel_section_t){.first = plan->count, .decimal = SIZE_MAX, .exponent = SIZE_MAX,
                                        .slash = SIZE_MAX, .first_digit = SIZE_MAX, .last_digit = SIZE_MAX};
        plan->sections = s + 1u;
        if (!fmt_excel_lex_section(plan, section, &at, error))
            goto fail;
        section->count = plan->count - section->first;
        if (!fx_resolve(plan, section, error))
            goto fail;
        if (at == length)
            return plan;
        if (s == 3u) {
            fx_error(error, "Excel custom codes allow at most four sections");
            goto fail;
        }
        ++at;
    }
    return plan;

memory:
    fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate Excel custom format");
fail:
    fmt_excel_free(plan);
    return NULL;
}

void fmt_excel_free(fmt_excel_t *plan)
{
    if (!plan)
        return;
    for (unsigned i = 0u; i < plan->sections; ++i)
        free(plan->section[i].threshold);
    free(plan->tokens);
    free(plan->code);
    free(plan);
}

/* Return enough decimal guard digits for the requested custom-code fields. */
size_t fmt_excel_precision(const fmt_options_t *options)
{
    unsigned precision = 0u;
    if (options && options->excel) {
        for (unsigned i = 0u; i < options->excel->sections; ++i) {
            const fmt_excel_section_t *section = &options->excel->section[i];
            unsigned digits = section->whole + section->fractional;
            if (digits > precision)
                precision = digits;
        }
    }
    return precision > 10000u ? 10064u : (size_t)precision + 64u;
}
