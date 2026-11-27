/**
 * @file format_excel_compile.c
 * @brief Compile Excel custom number and datetime codes into immutable sections and tokens.
 *
 * Resolves conditions, colours, scaling, quoted literals and date-field context once per compiled template.
 * Calendar names and separators follow documented MARS conventions independently of the host locale.
 */
#include <ctype.h>
#include <limits.h>
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

static unsigned char fx_lower(unsigned char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (unsigned char)(ch + ('a' - 'A')) : ch;
}

static int fx_ascii_compare(const char *a, const char *b, size_t length)
{
    for (size_t i = 0u; i < length; ++i) {
        unsigned char left = fx_lower((unsigned char)a[i]), right = fx_lower((unsigned char)b[i]);
        if (left != right)
            return (int)left - (int)right;
        if (!left)
            return 0;
    }
    return 0;
}

static bool fx_native_options(const char *code)
{
    const char *at = code;
    bool aligned = false;
    if (*at == '[' || *at == '"' || *at == '\\')
        return false;
    if (at[0] && at[1] && strchr("<>=^", at[1])) {
        if ((unsigned char)at[0] < 32u || (unsigned char)at[0] > 126u)
            return false;
        at += 2u;
        aligned = true;
    } else if (*at && strchr("<>=^", *at)) {
        ++at;
        aligned = true;
    }
    if (*at == '+' || *at == '-' || *at == ' ')
        ++at;
    if (*at == '#')
        ++at;
    if (*at == '0')
        ++at;
    while (*at >= '0' && *at <= '9')
        ++at;
    if (*at == '.') {
        ++at;
        if (*at < '0' || *at > '9')
            return false;
        while (*at >= '0' && *at <= '9')
            ++at;
    }
    const char *word = at;
    while ((*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') || *at == '_')
        ++at;
    if (*at)
        return false;
    return (word != code && at != word) || (aligned && at == word) || (code[0] == '.' && at == word);
}


bool fmt_excel_detect(const char *code)
{
    if (!code || !*code)
        return false;
    if ((!fx_ascii_compare(code, "General", 7u) && strlen(code) == 7u) || !fx_ascii_compare(code, "excel:", 6u))
        return true;
    if (fx_native_options(code))
        return false;
    for (size_t i = 0u; code[i]; ++i) {
        if ((unsigned char)code[i] >= 128u &&
            (strchr("ymdhsYMDHS", code[0]) || !strpbrk(code, "<>=^")))
            return true;
    }
    if (strpbrk(code, ";[\"\\%?@/,:()$"))
        return true;
    if (strchr(code, '#') && !strpbrk(code, "bxXodcsnpqQfFeEgG"))
        return true;
    if (code[0] == '*' && code[1] && !strchr("<>=^", code[1]))
        return true;
    size_t n = strlen(code);
    if (strspn(code, "0#?.,Ee+- ") == n && strpbrk(code, "0#?") &&
        !(code[0] == '.' && isalpha((unsigned char)code[n - 1u])))
        return true;
    if (strspn(code, "0#?.,% +-()") == n && strpbrk(code, "0#?"))
        return true;
    return strspn(code, "ymdhsoYMDHSO0 -:./^") == n && n > 1u;
}

static size_t fx_character_length(const char *text)
{
    unsigned char ch = (unsigned char)*text;
    return ch < 0x80u ? 1u : ch < 0xe0u ? 2u : ch < 0xf0u ? 3u : 4u;
}

static void fx_token(fmt_excel_t *plan, fmt_excel_kind_t kind, size_t start, size_t length)
{
    plan->tokens[plan->count++] = (fmt_excel_token_t){kind, start, length};
}

static bool fx_condition(fmt_excel_section_t *section, const char *text, size_t length, fmt_error_t *error)
{
    if (section->condition)
        return fx_error(error, "only one condition is permitted in a section");
    size_t at = 1u;
    section->condition = *text == '<' ? 1 : *text == '=' ? 3 : 6;
    if (at < length && text[at] == '=') {
        section->condition = *text == '<' ? 2 : 5;
        ++at;
    } else if (*text == '<' && at < length && text[at] == '>') {
        section->condition = 4;
        ++at;
    }
    section->threshold_negative = at < length && text[at] == '-';
    if (at < length && (text[at] == '-' || text[at] == '+'))
        ++at;
    char *digits = malloc(length + 1u);
    if (!digits)
        return fmt_error_set(error, FMT_ERROR_MEMORY, SIZE_MAX, SIZE_MAX, "cannot allocate condition coefficient");
    size_t count = 0u;
    int point = -1;
    while (at < length && (isdigit((unsigned char)text[at]) || text[at] == '.')) {
        if (text[at] == '.') {
            if (point >= 0) {
                free(digits);
                return fx_error(error, "invalid condition threshold");
            }
            point = (int)count;
        } else {
            digits[count++] = text[at];
        }
        ++at;
    }
    if (point < 0)
        point = (int)count;
    if (at < length && (text[at] == 'e' || text[at] == 'E')) {
        ++at;
        bool negative = at < length && text[at] == '-';
        if (at < length && (text[at] == '-' || text[at] == '+'))
            ++at;
        size_t first = at;
        int exponent = 0;
        while (at < length && isdigit((unsigned char)text[at])) {
            exponent = exponent * 10 + text[at++] - '0';
            if (exponent > 10000) {
                free(digits);
                return fx_error(error, "condition exponent exceeds the supported range");
            }
        }
        if (at == first) {
            free(digits);
            return fx_error(error, "condition exponent requires digits");
        }
        point += negative ? -exponent : exponent;
    }
    if (!count || at != length) {
        free(digits);
        return fx_error(error, "invalid condition threshold");
    }
    size_t first = 0u;
    while (first + 1u < count && digits[first] == '0')
        ++first;
    memmove(digits, digits + first, count - first);
    count -= first;
    digits[count] = '\0';
    section->threshold = digits;
    section->threshold_point = point - (int)first;
    return true;
}

static bool fx_colour(fmt_excel_section_t *section, const char *text, size_t length)
{
    static const char *const names[8] = {"Black", "Blue", "Cyan", "Green", "Magenta", "Red", "White", "Yellow"};
    static const unsigned rgb[56] = {
        0x000000, 0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff,
        0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080, 0xc0c0c0, 0x808080,
        0x9999ff, 0x993366, 0xffffcc, 0xccffff, 0x660066, 0xff8080, 0x0066cc, 0xccccff,
        0x000080, 0xff00ff, 0xffff00, 0x00ffff, 0x800080, 0x800000, 0x008080, 0x0000ff,
        0x00ccff, 0xccffff, 0xccffcc, 0xffff99, 0x99ccff, 0xff99cc, 0xcc99ff, 0xffcc99,
        0x3366ff, 0x33cccc, 0x99cc00, 0xffcc00, 0xff9900, 0xff6600, 0x666699, 0x969696,
        0x003366, 0x339966, 0x003300, 0x333300, 0x993300, 0x993366, 0x333399, 0x333333,
    };
    static const unsigned ansi[8] = {30u, 34u, 36u, 32u, 35u, 31u, 37u, 33u};
    /* Eight fixed names form a bounded vocabulary; colour numbers use direct palette indexing. */
    for (size_t i = 0; i < 8u; ++i) {
        if (strlen(names[i]) == length && !fx_ascii_compare(text, names[i], length)) {
            snprintf(section->colour, sizeof(section->colour), "\033[%um", ansi[i]);
            return true;
        }
    }
    if (length > 5u && !fx_ascii_compare(text, "Color", 5u)) {
        size_t at = 5u;
        while (at < length && text[at] == ' ')
            ++at;
        unsigned number = 0;
        for (; at < length; ++at) {
            if (!isdigit((unsigned char)text[at]) || number > 56u)
                return false;
            number = number * 10u + (unsigned)(text[at] - '0');
        }
        if (!number || number > 56u)
            return false;
        unsigned colour = rgb[number - 1u];
        snprintf(section->colour, sizeof(section->colour), "\033[38;2;%u;%u;%um",
                 colour >> 16, (colour >> 8) & 255u, colour & 255u);
        return true;
    }
    return false;
}

static bool fx_bracket(fmt_excel_t *plan, fmt_excel_section_t *section, size_t *at, fmt_error_t *error)
{
    size_t first = ++*at;
    while (plan->code[*at] && plan->code[*at] != ']')
        ++*at;
    if (plan->code[*at] != ']')
        return fx_error(error, "unclosed Excel bracket directive");
    size_t length = *at - first;
    const char *text = plan->code + first;
    ++*at;
    if (!length)
        return fx_error(error, "empty Excel bracket directive");
    if (strchr("<=>", *text)) {
        if (plan->sections > 2u)
            return fx_error(error, "conditions are permitted only in the first two sections");
        return fx_condition(section, text, length, error);
    }
    if (fx_colour(section, text, length))
        return true;
    unsigned ch = (unsigned char)fx_lower((unsigned char)*text);
    if ((ch == 'h' || ch == 'm' || ch == 's') && length <= 2u && (length == 1u || fx_lower((unsigned char)text[1]) == (int)ch)) {
        fmt_excel_kind_t kind = ch == 'h' ? FX_ELAPSED_HOUR : ch == 'm' ? FX_ELAPSED_MINUTE : FX_ELAPSED_SECOND;
        fx_token(plan, kind, first, length);
        section->date = true;
        return true;
    }
    if (*text == '$') {
        size_t symbol = first + 1u;
        size_t end = symbol;
        while (end < first + length && plan->code[end] != '-')
            ++end;
        if (end > symbol)
            fx_token(plan, FX_LITERAL, symbol, end - symbol);
        if (end < first + length && first + length - end == 5u) {
            const char *locale = plan->code + end + 1u;
            if (!fx_ascii_compare(locale, "F800", 4u) || !fx_ascii_compare(locale, "F400", 4u)) {
                fx_token(plan, fx_lower((unsigned char)locale[1]) == '8' ? FX_LONG_DATE : FX_LONG_TIME, first, 0u);
                section->date = true;
            }
        }
        return true;
    }
    return fx_error(error, "unsupported Excel bracket directive under MARS conventions");
}

static bool fx_resolve(fmt_excel_t *plan, fmt_excel_section_t *section, fmt_error_t *error)
{
    size_t end = section->first + section->count;
    if (section->date) {
        fmt_excel_kind_t previous = FX_LITERAL;
        for (size_t i = section->first; i < end; ++i) {
            fmt_excel_token_t *token = &plan->tokens[i];
            if (plan->excel_compatibility && token->kind == FX_MONTH && token->length <= 2u) {
                size_t next = i + 1u;
                while (next < end && plan->tokens[next].kind < FX_YEAR)
                    ++next;
                if (previous == FX_HOUR || previous == FX_ELAPSED_HOUR ||
                    (next < end && (plan->tokens[next].kind == FX_SECOND ||
                                    plan->tokens[next].kind == FX_ELAPSED_SECOND)))
                    token->kind = FX_MINUTE;
            }
            if (token->kind >= FX_YEAR && token->kind != FX_AMPM)
                previous = token->kind;
            if (token->kind == FX_DIGIT && plan->code[token->start] != '0')
                return fx_error(error, "date fractional seconds require zero placeholders");
        }
        return true;
    }
    bool after_decimal = false;
    for (size_t i = section->first; i < end; ++i) {
        fmt_excel_token_t *token = &plan->tokens[i];
        if (token->kind == FX_DOT) {
            if (section->decimal != SIZE_MAX)
                return fx_error(error, "multiple decimal points in one numeric section");
            section->decimal = i;
            after_decimal = true;
        } else if (token->kind == FX_DIGIT) {
            if (section->first_digit == SIZE_MAX)
                section->first_digit = i;
            section->last_digit = i;
            if (section->exponent == SIZE_MAX && section->slash == SIZE_MAX) {
                if (after_decimal)
                    ++section->fractional;
                else
                    ++section->whole;
            }
        } else if (token->kind == FX_EXPONENT) {
            if (section->exponent != SIZE_MAX || section->slash != SIZE_MAX)
                return fx_error(error, "multiple or mixed fraction and scientific fields");
            section->exponent = i;
        } else if (token->kind == FX_SLASH) {
            if (section->slash != SIZE_MAX || section->exponent != SIZE_MAX || section->decimal != SIZE_MAX)
                return fx_error(error, "invalid fraction field");
            section->slash = i;
        } else if (token->kind == FX_PERCENT) {
            section->shift += 2;
        }
    }
    if (section->fractional > 10000u)
        return fx_error(error, "Excel precision exceeds 10000 places");
    if (section->exponent != SIZE_MAX &&
        (section->exponent == section->first || section->exponent + 1u >= end ||
         plan->tokens[section->exponent + 1u].kind != FX_DIGIT))
        return fx_error(error, "scientific notation requires mantissa and exponent placeholders");
    if (section->last_digit != SIZE_MAX && section->slash == SIZE_MAX) {
        size_t tail = section->last_digit + 1u;
        /* Scaling commas can follow the integer placeholders before the decimal point. */
        if (section->decimal != SIZE_MAX) {
            tail = section->decimal;
            while (tail > section->first && plan->tokens[tail - 1u].kind == FX_COMMA)
                --tail;
            for (size_t i = tail; i < section->decimal; ++i)
                section->shift -= 3;
        }
        for (size_t i = section->last_digit + 1u; i < end && plan->tokens[i].kind == FX_COMMA; ++i)
            section->shift -= 3;
    }
    return true;
}

fmt_excel_t *fmt_excel_compile(const char *code, fmt_error_t *error)
{
    bool excel_compatibility = !fx_ascii_compare(code, "excel:", 6u);
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
        while (at < length && code[at] != ';') {
            size_t first = at;
            char ch = code[at++];
            if (ch == '"') {
                first = at;
                while (at < length && code[at] != '"')
                    ++at;
                if (at == length) {
                    fx_error(error, "unclosed Excel quoted literal");
                    goto fail;
                }
                fx_token(plan, FX_LITERAL, first, at++ - first);
            } else if (ch == '\\' || ch == '_' || ch == '*') {
                if (at == length) {
                    fx_error(error, "Excel escape or spacing code requires a character");
                    goto fail;
                }
                size_t count = fx_character_length(code + at);
                fx_token(plan, ch == '_' ? FX_SPACE : ch == '*' ? FX_FILL : FX_LITERAL, at, count);
                at += count;
            } else if (ch == '[') {
                --at;
                if (!fx_bracket(plan, section, &at, error))
                    goto fail;
            } else if (!fx_ascii_compare(code + first, "General", 7u)) {
                fx_token(plan, FX_GENERAL, first, 7u);
                section->general = true;
                at = first + 7u;
            } else if (!fx_ascii_compare(code + first, "AM/PM", 5u) || !fx_ascii_compare(code + first, "A/P", 3u)) {
                size_t count = fx_lower((unsigned char)code[first + 1u]) == 'm' ? 5u : 3u;
                fx_token(plan, FX_AMPM, first, count);
                at = first + count;
                section->ampm = section->date = true;
            } else if ((ch == 'E' || ch == 'e') && (code[at] == '+' || code[at] == '-') &&
                       (code[at + 1u] == '0' || code[at + 1u] == '#' || code[at + 1u] == '?')) {
                fx_token(plan, FX_EXPONENT, first, 2u);
                ++at;
            } else if (strchr("ymdhsYMDHS", ch)) {
                while (at < length && fx_lower((unsigned char)code[at]) == fx_lower((unsigned char)ch))
                    ++at;
                unsigned key = (unsigned char)fx_lower((unsigned char)ch);
                static const fmt_excel_kind_t dates[128] = {
                    ['d'] = FX_DAY,
                    ['h'] = FX_HOUR,
                    ['m'] = FX_MONTH,
                    ['s'] = FX_SECOND,
                    ['y'] = FX_YEAR,
                };
                size_t count = at - first;
                size_t maximum = key == 'm' ? (excel_compatibility || ch == 'M' ? 5u : 2u) :
                                 key == 'd' || key == 'y' ? 4u : 2u;
                if (count > maximum) {
                    fx_error(error, "invalid repeated date field");
                    goto fail;
                }
                fmt_excel_kind_t kind = dates[key];
                if (!excel_compatibility && ch == 'm')
                    kind = FX_MINUTE;
                if (!excel_compatibility && key == 'd' && count <= 2u && fx_lower((unsigned char)code[at]) == 'o') {
                    kind = FX_DAY_ORDINAL;
                    ++at;
                    if (code[at] == '^')
                        ++at;
                }
                fx_token(plan, kind, first, count);
                section->date = true;
            } else {
                static const fmt_excel_kind_t kinds[128] = {
                    ['#'] = FX_DIGIT,
                    ['%'] = FX_PERCENT,
                    [','] = FX_COMMA,
                    ['.'] = FX_DOT,
                    ['/'] = FX_SLASH,
                    ['0'] = FX_DIGIT,
                    ['?'] = FX_DIGIT,
                    ['@'] = FX_TEXT,
                };
                unsigned key = (unsigned char)ch;
                if ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z')) {
                    fx_error(error, "literal letters in Excel codes must be quoted or escaped");
                    goto fail;
                }
                size_t count = fx_character_length(code + first);
                fx_token(plan, key < 128u ? kinds[key] : FX_LITERAL, first, count);
                at = first + count;
            }
        }
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
