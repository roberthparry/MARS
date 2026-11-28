/**
 * @file format_excel_lex.c
 * @brief Tokenise numeric and date masks using directly indexed byte and directive handlers.
 *
 * Each leading byte selects its grammar handler. Repetition widths, ordinal styles, name/period case and exponent
 * signs are resolved into tokens once; named colours use a checked perfect-hash lookup rather than a name scan.
 */
#include <ctype.h>
#include <stdint.h>
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

static const unsigned fx_kind_context[FX_KIND_COUNT] = {
    [FX_YEAR]           = FX_DATE_CONTEXT,
    [FX_MONTH]          = FX_DATE_CONTEXT,
    [FX_DAY]            = FX_DATE_CONTEXT,
    [FX_DAY_ORDINAL]    = FX_DATE_CONTEXT,
    [FX_HOUR]           = FX_DATE_CONTEXT | FX_HOUR_CONTEXT,
    [FX_MINUTE]         = FX_DATE_CONTEXT,
    [FX_SECOND]         = FX_DATE_CONTEXT | FX_SECOND_CONTEXT,
    [FX_AMPM]           = FX_DATE_CONTEXT,
    [FX_ELAPSED_HOUR]   = FX_DATE_CONTEXT | FX_HOUR_CONTEXT,
    [FX_ELAPSED_MINUTE] = FX_DATE_CONTEXT,
    [FX_ELAPSED_SECOND] = FX_DATE_CONTEXT | FX_SECOND_CONTEXT,
    [FX_LONG_DATE]      = FX_DATE_CONTEXT,
    [FX_LONG_TIME]      = FX_DATE_CONTEXT,
};

static size_t fx_character_length(const char *text)
{
    unsigned char ch = (unsigned char)*text;
    return ch < 0x80u ? 1u : ch < 0xe0u ? 2u : ch < 0xf0u ? 3u : 4u;
}

static void fx_token(fmt_excel_t *plan, fmt_excel_kind_t kind, size_t start, size_t length)
{
    plan->tokens[plan->count] = (fmt_excel_token_t){.kind = kind, .start = start, .length = length,
        .next = plan->count + 1u, .flags = fx_kind_context[kind] | plan->section[plan->sections - 1u].name_case};
    ++plan->count;
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
    static const char words[] = "Green\0Red\0Cyan\0White\0Blue\0Yellow\0Black\0Magenta\0";
    static const struct {
        uint8_t offset;
        uint8_t length;
        uint8_t ansi;
    } names[8] = {
        [0] = { 0u, 5u, 32u},
        [1] = { 6u, 3u, 31u},
        [2] = {10u, 4u, 36u},
        [3] = {15u, 5u, 37u},
        [4] = {21u, 4u, 34u},
        [5] = {26u, 6u, 33u},
        [6] = {33u, 5u, 30u},
        [7] = {39u, 7u, 35u},
    };
    static const unsigned rgb[56] = {
        0x000000, 0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff,
        0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080, 0xc0c0c0, 0x808080,
        0x9999ff, 0x993366, 0xffffcc, 0xccffff, 0x660066, 0xff8080, 0x0066cc, 0xccccff,
        0x000080, 0xff00ff, 0xffff00, 0x00ffff, 0x800080, 0x800000, 0x008080, 0x0000ff,
        0x00ccff, 0xccffff, 0xccffcc, 0xffff99, 0x99ccff, 0xff99cc, 0xcc99ff, 0xffcc99,
        0x3366ff, 0x33cccc, 0x99cc00, 0xffcc00, 0xff9900, 0xff6600, 0x666699, 0x969696,
        0x003366, 0x339966, 0x003300, 0x333300, 0x993300, 0x993366, 0x333399, 0x333333,
    };
    /* Seed 347 yields exactly eight distinct slots for the eight names; exact matching rejects non-members. */
    uint32_t hash = 347u;
    for (size_t i = 0u; i < length; ++i)
        hash = (hash ^ fmt_excel_lower((unsigned char)text[i])) * UINT32_C(16777619);
    size_t index = (hash ^ (hash >> 16)) & 7u;
    if (names[index].length == length && !fmt_excel_ascii_compare(text, words + names[index].offset, length)) {
        snprintf(section->colour, sizeof(section->colour), "\033[%um", names[index].ansi);
        return true;
    }
    if (length > 5u && !fmt_excel_ascii_compare(text, "Color", 5u)) {
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

typedef bool (*fx_directive_fn)(fmt_excel_t *, fmt_excel_section_t *, size_t, size_t, fmt_error_t *);

static bool fx_directive_colour(fmt_excel_t *plan, fmt_excel_section_t *section, size_t first,
                                 size_t length, fmt_error_t *error)
{
    return fx_colour(section, plan->code + first, length) ||
           fx_error(error, "unsupported Excel bracket directive under MARS conventions");
}

static bool fx_directive_condition(fmt_excel_t *plan, fmt_excel_section_t *section, size_t first,
                                    size_t length, fmt_error_t *error)
{
    if (plan->sections > 2u)
        return fx_error(error, "conditions are permitted only in the first two sections");
    return fx_condition(section, plan->code + first, length, error);
}

static bool fx_directive_elapsed(fmt_excel_t *plan, fmt_excel_section_t *section, size_t first,
                                  size_t length, fmt_error_t *error)
{
    const char *text = plan->code + first;
    unsigned ch = fmt_excel_lower((unsigned char)*text);
    if (length > 2u || (length == 2u && fmt_excel_lower((unsigned char)text[1]) != ch))
        return fx_directive_colour(plan, section, first, length, error);
    static const fmt_excel_kind_t elapsed[128] = {
        ['h'] = FX_ELAPSED_HOUR,
        ['m'] = FX_ELAPSED_MINUTE,
        ['s'] = FX_ELAPSED_SECOND,
    };
    fx_token(plan, elapsed[ch], first, length);
    section->date = true;
    return true;
}

static bool fx_directive_currency(fmt_excel_t *plan, fmt_excel_section_t *section, size_t first,
                                   size_t length, fmt_error_t *error)
{
    (void)error;
    size_t symbol = first + 1u, end = symbol;
    while (end < first + length && plan->code[end] != '-')
        ++end;
    if (end > symbol)
        fx_token(plan, FX_LITERAL, symbol, end - symbol);
    if (end == first + length || first + length - end != 5u)
        return true;
    const char *locale = plan->code + end + 1u;
    bool long_date = !fmt_excel_ascii_compare(locale, "F800", 4u);
    bool long_time = !fmt_excel_ascii_compare(locale, "F400", 4u);
    if (long_date || long_time) {
        fx_token(plan, long_date ? FX_LONG_DATE : FX_LONG_TIME, first, 0u);
        section->date = true;
    }
    return true;
}

static bool fx_directive_case(fmt_excel_t *plan, fmt_excel_section_t *section, size_t first,
                               size_t length, fmt_error_t *error)
{
    /* Three directive names occupy three checked slots. The first-byte hash has no empty entries. */
    static const struct { const char *name; unsigned flags; } names[3] = {
        {"upper", FX_UPPER_NAME}, {"lower", FX_LOWER_NAME}, {"title", 0u},
    };
    unsigned key = fmt_excel_lower((unsigned char)plan->code[first]);
    unsigned slot = (key ^ (key >> 3u)) % 3u;
    if (plan->excel_compatibility || length != 5u ||
        fmt_excel_ascii_compare(plan->code + first, names[slot].name, 5u))
        return fx_error(error, "unsupported native calendar-name case directive");
    section->name_case = names[slot].flags;
    section->name_case_set = true;
    return true;
}

static bool fx_bracket(fmt_excel_t *plan, fmt_excel_section_t *section, size_t *at, fmt_error_t *error)
{
    static const uint8_t directive_index[128] = {
        ['$'] = 1u,
        ['<'] = 2u,
        ['='] = 2u,
        ['>'] = 2u,
        ['h'] = 3u,
        ['l'] = 4u,
        ['m'] = 3u,
        ['s'] = 3u,
        ['t'] = 4u,
        ['u'] = 4u,
    };
    static const fx_directive_fn directives[5] = {
        fx_directive_colour, fx_directive_currency, fx_directive_condition, fx_directive_elapsed, fx_directive_case,
    };
    size_t first = ++*at;
    while (*at < plan->length && plan->code[*at] != ']')
        ++*at;
    if (*at == plan->length)
        return fx_error(error, "unclosed Excel bracket directive");
    size_t length = *at - first;
    ++*at;
    if (!length)
        return fx_error(error, "empty Excel bracket directive");
    unsigned key = fmt_excel_lower((unsigned char)plan->code[first]);
    return directives[key < 128u ? directive_index[key] : 0u](plan, section, first, length, error);
}

typedef struct _fx_lex_rule_t fx_lex_rule_t;

typedef struct {
    fmt_excel_t *plan;
    fmt_excel_section_t *section;
    fmt_error_t *error;
    size_t position;
} fx_lexer_t;

struct _fx_lex_rule_t {
    bool (*consume)(fx_lexer_t *, const fx_lex_rule_t *);
    fmt_excel_kind_t native_kind;
    fmt_excel_kind_t excel_kind;
    unsigned native_max;
    unsigned excel_max;
    unsigned flags;
};

static bool fx_lex_literal(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    size_t first = lexer->position;
    unsigned ch = (unsigned char)lexer->plan->code[first];
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
        return fx_error(lexer->error, "literal letters in Excel codes must be quoted or escaped");
    size_t length = fx_character_length(lexer->plan->code + first);
    fx_token(lexer->plan, rule->native_kind, first, length);
    lexer->plan->tokens[lexer->plan->count - 1u].flags |= rule->flags;
    lexer->section->text |= rule->native_kind == FX_TEXT;
    lexer->position += length;
    return true;
}

static bool fx_lex_quoted(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    (void)rule;
    size_t first = ++lexer->position;
    while (lexer->position < lexer->plan->length && lexer->plan->code[lexer->position] != '"')
        ++lexer->position;
    if (lexer->position == lexer->plan->length)
        return fx_error(lexer->error, "unclosed Excel quoted literal");
    fx_token(lexer->plan, FX_LITERAL, first, lexer->position++ - first);
    return true;
}

static bool fx_lex_escape(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    if (++lexer->position == lexer->plan->length)
        return fx_error(lexer->error, "Excel escape or spacing code requires a character");
    size_t length = fx_character_length(lexer->plan->code + lexer->position);
    fx_token(lexer->plan, rule->native_kind, lexer->position, length);
    lexer->position += length;
    return true;
}

static bool fx_lex_bracket(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    (void)rule;
    return fx_bracket(lexer->plan, lexer->section, &lexer->position, lexer->error);
}

static bool fx_lex_general(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    if (lexer->plan->length - lexer->position < 7u ||
        fmt_excel_ascii_compare(lexer->plan->code + lexer->position, "General", 7u))
        return fx_lex_literal(lexer, rule);
    fx_token(lexer->plan, FX_GENERAL, lexer->position, 7u);
    lexer->section->general = true;
    lexer->position += 7u;
    return true;
}

static bool fx_lex_period(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    const char *text = lexer->plan->code + lexer->position;
    size_t remaining = lexer->plan->length - lexer->position;
    size_t count = 0u;
    if (remaining >= 5u && !fmt_excel_ascii_compare(text, "AM/PM", 5u))
        count = 5u;
    else if (remaining >= 3u && !fmt_excel_ascii_compare(text, "A/P", 3u))
        count = 3u;
    if (!count)
        return fx_lex_literal(lexer, rule);
    fx_token(lexer->plan, FX_AMPM, lexer->position, count);
    lexer->plan->tokens[lexer->plan->count - 1u].flags |= rule->flags;
    lexer->position += count;
    lexer->section->ampm = lexer->section->date = true;
    return true;
}

static bool fx_lex_exponent(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    const char *text = lexer->plan->code + lexer->position;
    if (lexer->plan->length - lexer->position < 3u || (text[1] != '+' && text[1] != '-') || !strchr("0#?", text[2]))
        return fx_lex_literal(lexer, rule);
    fx_token(lexer->plan, FX_EXPONENT, lexer->position, 2u);
    if (text[1] == '+')
        lexer->plan->tokens[lexer->plan->count - 1u].flags |= FX_POSITIVE_EXPONENT;
    lexer->position += 2u;
    return true;
}

static bool fx_lex_date(fx_lexer_t *lexer, const fx_lex_rule_t *rule)
{
    fmt_excel_t *plan = lexer->plan;
    size_t first = lexer->position++;
    unsigned ch = fmt_excel_lower((unsigned char)plan->code[first]);
    while (lexer->position < plan->length && fmt_excel_lower((unsigned char)plan->code[lexer->position]) == ch)
        ++lexer->position;
    size_t count = lexer->position - first;
    unsigned maximum = plan->excel_compatibility ? rule->excel_max : rule->native_max;
    if (count > maximum)
        return fx_error(lexer->error, "invalid repeated date field");
    fmt_excel_kind_t kind = plan->excel_compatibility ? rule->excel_kind : rule->native_kind;
    bool raised = false;
    if (!plan->excel_compatibility && kind == FX_DAY && count <= 2u &&
        fmt_excel_lower((unsigned char)plan->code[lexer->position]) == 'o') {
        kind = FX_DAY_ORDINAL;
        ++lexer->position;
        raised = plan->code[lexer->position] == '^';
        lexer->position += raised;
    }
    fx_token(plan, kind, first, count);
    if (raised)
        plan->tokens[plan->count - 1u].flags |= FX_SUPERSCRIPT;
    lexer->section->date = true;
    return true;
}

enum {
    FX_LX_LITERAL,
    FX_LX_QUOTED,
    FX_LX_OPTIONAL,
    FX_LX_PERCENT,
    FX_LX_FILL,
    FX_LX_COMMA,
    FX_LX_DOT,
    FX_LX_SLASH,
    FX_LX_ZERO,
    FX_LX_RESERVED,
    FX_LX_TEXT,
    FX_LX_PERIOD_U,
    FX_LX_DAY,
    FX_LX_EXPONENT,
    FX_LX_GENERAL,
    FX_LX_HOUR,
    FX_LX_MONTH,
    FX_LX_SECOND,
    FX_LX_YEAR,
    FX_LX_BRACKET,
    FX_LX_ESCAPE,
    FX_LX_SPACE,
    FX_LX_PERIOD_L,
    FX_LX_MINUTE,
    FX_LX_COUNT
};

static const fx_lex_rule_t fx_lex_rules[FX_LX_COUNT] = {
    [FX_LX_LITERAL ] = {fx_lex_literal, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_QUOTED  ] = {fx_lex_quoted, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_OPTIONAL] = {fx_lex_literal, FX_DIGIT, FX_DIGIT, 0u, 0u, 0u},
    [FX_LX_PERCENT ] = {fx_lex_literal, FX_PERCENT, FX_PERCENT, 0u, 0u, 0u},
    [FX_LX_FILL    ] = {fx_lex_escape, FX_FILL, FX_FILL, 0u, 0u, 0u},
    [FX_LX_COMMA   ] = {fx_lex_literal, FX_COMMA, FX_COMMA, 0u, 0u, 0u},
    [FX_LX_DOT     ] = {fx_lex_literal, FX_DOT, FX_DOT, 0u, 0u, 0u},
    [FX_LX_SLASH   ] = {fx_lex_literal, FX_SLASH, FX_SLASH, 0u, 0u, 0u},
    [FX_LX_ZERO    ] = {fx_lex_literal, FX_DIGIT, FX_DIGIT, 0u, 0u, FX_REQUIRED},
    [FX_LX_RESERVED] = {fx_lex_literal, FX_DIGIT, FX_DIGIT, 0u, 0u, FX_RESERVED},
    [FX_LX_TEXT    ] = {fx_lex_literal, FX_TEXT, FX_TEXT, 0u, 0u, 0u},
    [FX_LX_PERIOD_U] = {fx_lex_period, FX_AMPM, FX_AMPM, 0u, 0u, FX_UPPER_PERIOD},
    [FX_LX_DAY     ] = {fx_lex_date, FX_DAY, FX_DAY, 4u, 4u, 0u},
    [FX_LX_EXPONENT] = {fx_lex_exponent, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_GENERAL ] = {fx_lex_general, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_HOUR    ] = {fx_lex_date, FX_HOUR, FX_HOUR, 2u, 2u, 0u},
    [FX_LX_MONTH   ] = {fx_lex_date, FX_MONTH, FX_MONTH, 5u, 5u, 0u},
    [FX_LX_SECOND  ] = {fx_lex_date, FX_SECOND, FX_SECOND, 2u, 2u, 0u},
    [FX_LX_YEAR    ] = {fx_lex_date, FX_YEAR, FX_YEAR, 4u, 4u, 0u},
    [FX_LX_BRACKET ] = {fx_lex_bracket, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_ESCAPE  ] = {fx_lex_escape, FX_LITERAL, FX_LITERAL, 0u, 0u, 0u},
    [FX_LX_SPACE   ] = {fx_lex_escape, FX_SPACE, FX_SPACE, 0u, 0u, 0u},
    [FX_LX_PERIOD_L] = {fx_lex_period, FX_AMPM, FX_AMPM, 0u, 0u, 0u},
    [FX_LX_MINUTE  ] = {fx_lex_date, FX_MINUTE, FX_MONTH, 2u, 5u, 0u},
};

static const unsigned char fx_lex_index[128] = {
    ['"' ] = FX_LX_QUOTED,
    ['#' ] = FX_LX_OPTIONAL,
    ['%' ] = FX_LX_PERCENT,
    ['*' ] = FX_LX_FILL,
    [',' ] = FX_LX_COMMA,
    ['.' ] = FX_LX_DOT,
    ['/' ] = FX_LX_SLASH,
    ['0' ] = FX_LX_ZERO,
    ['?' ] = FX_LX_RESERVED,
    ['@' ] = FX_LX_TEXT,
    ['A' ] = FX_LX_PERIOD_U,
    ['D' ] = FX_LX_DAY,
    ['E' ] = FX_LX_EXPONENT,
    ['G' ] = FX_LX_GENERAL,
    ['H' ] = FX_LX_HOUR,
    ['M' ] = FX_LX_MONTH,
    ['S' ] = FX_LX_SECOND,
    ['Y' ] = FX_LX_YEAR,
    ['[' ] = FX_LX_BRACKET,
    ['\\'] = FX_LX_ESCAPE,
    ['_' ] = FX_LX_SPACE,
    ['a' ] = FX_LX_PERIOD_L,
    ['d' ] = FX_LX_DAY,
    ['e' ] = FX_LX_EXPONENT,
    ['g' ] = FX_LX_GENERAL,
    ['h' ] = FX_LX_HOUR,
    ['m' ] = FX_LX_MINUTE,
    ['s' ] = FX_LX_SECOND,
    ['y' ] = FX_LX_YEAR,
};


bool fmt_excel_lex_section(fmt_excel_t *plan, fmt_excel_section_t *section, size_t *position, fmt_error_t *error)
{
    fx_lexer_t lexer = {plan, section, error, *position};
    while (lexer.position < plan->length && plan->code[lexer.position] != ';') {
        unsigned byte = (unsigned char)plan->code[lexer.position];
        unsigned index = byte < 128u ? fx_lex_index[byte] : FX_LX_LITERAL;
        const fx_lex_rule_t *rule = &fx_lex_rules[index];
        if (!rule->consume(&lexer, rule))
            return false;
    }
    *position = lexer.position;
    return true;
}
