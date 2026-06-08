#include "expr_parse_text.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "expr_fromstring.h"

typedef struct {
    const char *text;
    size_t      len;
} expr_parse_literal_t;

#define EXPR_PARSE_LITERAL(text) { (text), sizeof(text) - 1u }

bool expr_parse_cursor_at_identifier_boundary(const string_cursor_t *cursor,
                                              string_pos_t pos)
{
    rune_t rune;

    if (!cursor)
        return true;

    rune = string_cursor_peek_at(cursor, pos);
    return !rune_is_alpha_numeric(rune) && !rune_is_equal(rune, '_');
}

bool expr_parse_cursor_consume_char(string_cursor_t *cursor, char ch)
{
    if (!cursor || !rune_is_equal(string_cursor_peek(cursor), ch))
        return false;

    return string_cursor_next(cursor) == 0;
}

bool expr_parse_cursor_peek_value_at(const string_cursor_t *cursor,
                                     string_pos_t pos,
                                     uint32_t *out,
                                     size_t *width_out)
{
    string_cursor_t *scan;
    rune_t rune;
    uint32_t value;

    if (!cursor)
        return false;

    rune = string_cursor_peek_at(cursor, pos);
    value = rune_value(rune);
    if (value == 0u)
        return false;

    if (out)
        *out = value;
    if (!width_out)
        return true;

    scan = string_cursor_clone(cursor);
    if (!scan)
        return false;
    if (string_cursor_seek(scan, pos) != 0 ||
        string_cursor_next(scan) != 0) {
        string_cursor_free(scan);
        return false;
    }
    *width_out = string_cursor_position(scan) - pos;
    string_cursor_free(scan);
    return true;
}

bool expr_parse_cursor_peek_value(const string_cursor_t *cursor,
                                  uint32_t *out,
                                  size_t *width_out)
{
    return cursor
        ? expr_parse_cursor_peek_value_at(cursor,
                                          string_cursor_position(cursor),
                                          out,
                                          width_out)
        : false;
}

bool expr_parse_view_peek_ascii(string_view_t view,
                                string_pos_t pos,
                                unsigned char *out)
{
    return string_view_peek_ascii(view, pos, out);
}

bool expr_parse_view_peek_value(string_view_t view,
                                string_pos_t pos,
                                uint32_t *out,
                                size_t *width_out)
{
    string_pos_t next = 0u;

    if (!string_view_peek_rune_value(view, pos, out, &next))
        return false;
    if (width_out)
        *width_out = next - pos;
    return true;
}

bool expr_parse_is_superscript_digit(uint32_t value)
{
    return value == 0x00B2 || value == 0x00B3 || value == 0x00B9 ||
           value == 0x2070 || (value >= 0x2074 && value <= 0x2079);
}

int expr_parse_superscript_digit_value(uint32_t value)
{
    if (value == 0x00B9)
        return 1;
    if (value == 0x00B2)
        return 2;
    if (value == 0x00B3)
        return 3;
    if (value == 0x2070)
        return 0;
    if (value >= 0x2074 && value <= 0x2079)
        return (int)(value - 0x2070);
    return -1;
}

bool expr_parse_is_subscript_digit(uint32_t value)
{
    return value >= 0x2080 && value <= 0x2089;
}

bool expr_parse_is_fraction_glyph(uint32_t value)
{
    return value == 0x00BC || value == 0x00BD || value == 0x00BE ||
           (value >= 0x2150 && value <= 0x215E);
}

size_t expr_parse_scan_unicode_fraction_len(string_view_t view,
                                            string_pos_t pos)
{
    string_pos_t p = pos;
    uint32_t value;
    size_t len;
    int digits = 0;

    if (!expr_parse_view_peek_value(view, p, &value, &len))
        return 0u;

    if (expr_parse_is_fraction_glyph(value))
        return len;

    while (expr_parse_view_peek_value(view, p, &value, &len) &&
           expr_parse_is_superscript_digit(value)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    if (!expr_parse_view_peek_value(view, p, &value, &len) || value != 0x2044)
        return 0u;
    p += len;

    digits = 0;
    while (expr_parse_view_peek_value(view, p, &value, &len) &&
           expr_parse_is_subscript_digit(value)) {
        p += len;
        ++digits;
    }
    if (digits == 0)
        return 0u;

    return p - pos;
}

static bool special_number_boundary(string_view_t view, string_pos_t pos)
{
    unsigned char c;

    if (!expr_parse_view_peek_ascii(view, pos, &c))
        return true;
    return !isalnum(c) && c != '_';
}

bool expr_parse_view_starts_with_text(string_view_t view,
                                      const char *text,
                                      bool case_insensitive)
{
    string_t *literal = string_new_with(text);
    bool matches = literal &&
                   string_view_starts_with(view, literal, case_insensitive);

    string_free(literal);
    return matches;
}

size_t expr_parse_scan_special_number_len(string_view_t view,
                                          string_pos_t pos,
                                          bool include_unicode_infinity,
                                          bool require_identifier_boundary)
{
    static const expr_parse_literal_t specials[] = {
        EXPR_PARSE_LITERAL("infinity"),
        EXPR_PARSE_LITERAL("nan"),
        EXPR_PARSE_LITERAL("inf")
    };
    string_view_t remaining = string_view_slice(view, pos,
        string_view_length(view) > pos ? string_view_length(view) - pos : 0u);
    uint32_t value = 0u;
    size_t value_len = 0u;

    if (include_unicode_infinity &&
        expr_parse_view_peek_value(view, pos, &value, &value_len) &&
        value == 0x221Eu)
        return value_len;

    for (size_t i = 0u; i < sizeof(specials) / sizeof(specials[0]); ++i) {
        if (expr_parse_view_starts_with_text(remaining,
                                             specials[i].text,
                                             true) &&
            (!require_identifier_boundary ||
             special_number_boundary(view, pos + specials[i].len)))
            return specials[i].len;
    }

    return 0u;
}

size_t expr_parse_scan_decimal_len(string_view_t view, string_pos_t pos)
{
    string_pos_t i = pos;
    size_t len = string_view_length(view);
    unsigned char c;
    bool have_digit = false;
    bool have_dot = false;

    if (expr_parse_view_peek_ascii(view, i, &c) && (c == '-' || c == '+'))
        i++;

    while (expr_parse_view_peek_ascii(view, i, &c) && isdigit(c)) {
        have_digit = true;
        i++;
    }

    if (expr_parse_view_peek_ascii(view, i, &c) && c == '.') {
        have_dot = true;
        i++;
        while (expr_parse_view_peek_ascii(view, i, &c) && isdigit(c)) {
            have_digit = true;
            i++;
        }
    }

    if (!have_digit && !have_dot)
        return 0u;

    if (expr_parse_view_peek_ascii(view, i, &c) && (c == 'e' || c == 'E')) {
        string_pos_t exp_start = i;
        bool exp_digit = false;

        i++;
        if (expr_parse_view_peek_ascii(view, i, &c) && (c == '+' || c == '-'))
            i++;
        while (expr_parse_view_peek_ascii(view, i, &c) && isdigit(c)) {
            exp_digit = true;
            i++;
        }
        if (!exp_digit)
            i = exp_start;
    }

    return i <= len ? i - pos : 0u;
}

size_t expr_parse_scan_number_atom_len(string_view_t view,
                                       string_pos_t pos,
                                       bool include_special_numbers)
{
    size_t len = expr_parse_scan_decimal_len(view, pos);
    string_pos_t p;
    size_t tail;
    unsigned char c;

    if (len == 0u) {
        if (include_special_numbers) {
            len = expr_parse_scan_special_number_len(view, pos, true, false);
            if (len > 0u)
                return len;
        }

        len = expr_parse_scan_unicode_fraction_len(view, pos);
        if (len == 0u)
            return 0u;
        p = pos + len;
        if (expr_parse_view_peek_ascii(view, p, &c) && (c == 'i' || c == 'I'))
            p++;
        return p - pos;
    }

    p = pos + len;
    if (expr_parse_view_peek_ascii(view, p, &c) && c == '/') {
        tail = expr_parse_scan_decimal_len(view, p + 1u);
        if (tail == 0u)
            return len;
        p += 1u + tail;
    }

    if (expr_parse_view_peek_ascii(view, p, &c) && (c == 'i' || c == 'I'))
        p++;

    return p - pos;
}

static string_t *cursor_read_literal_name(string_cursor_t *cursor,
                                          expr_parse_literal_t name)
{
    string_t *result = string_new_with(name.text);

    if (result)
        string_cursor_skip(cursor, name.len);
    return result;
}

static int append_subscript_digit(string_t *out, unsigned char digit)
{
    static const char *const subscripts[10] = {
        "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"
    };

    if (digit < '0' || digit > '9')
        return -1;
    return string_append_cstr(out, subscripts[digit - '0']);
}

static string_t *read_bracketed_name(string_cursor_t *cursor)
{
    string_cursor_t *scan;
    string_pos_t start_pos;
    string_pos_t end_pos;
    string_t *name;
    unsigned char b;

    scan = string_cursor_clone(cursor);
    if (!scan)
        return NULL;

    if (!expr_parse_cursor_consume_char(scan, '[')) {
        string_cursor_free(scan);
        return NULL;
    }

    start_pos = string_cursor_position(scan);
    while (!string_cursor_done(scan)) {
        if (string_cursor_peek_ascii(scan, &b) && b == ']')
            break;
        if (string_cursor_next(scan) != 0) {
            string_cursor_free(scan);
            return NULL;
        }
    }
    if (!string_cursor_peek_ascii(scan, &b) || b != ']') {
        string_cursor_free(scan);
        return NULL;
    }

    end_pos = string_cursor_position(scan);
    name = string_new();
    if (!name) {
        string_cursor_free(scan);
        return NULL;
    }
    if (string_cursor_append_slice_between(name,
                                           start_pos,
                                           end_pos,
                                           cursor) != 0) {
        string_free(name);
        string_cursor_free(scan);
        return NULL;
    }

    string_cursor_skip(scan, 1u);
    string_cursor_seek(cursor, string_cursor_position(scan));
    string_cursor_free(scan);
    return name;
}

static bool cursor_alias_is_accepted(const string_cursor_t *cursor,
                                     string_pos_t alias_pos)
{
    static const expr_parse_literal_t accepted[] = {
        EXPR_PARSE_LITERAL("pi"),
        EXPR_PARSE_LITERAL("phi"),
        EXPR_PARSE_LITERAL("gamma"),
        EXPR_PARSE_LITERAL("tau")
    };

    for (size_t i = 0u; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        string_pos_t suffix_pos = alias_pos + accepted[i].len;
        unsigned char suffix = 0u;
        uint32_t suffix_value = 0;
        size_t suffix_len = 0u;
        bool suffix_is_subscript;

        if (!string_cursor_match_at(cursor, alias_pos, accepted[i].text))
            continue;

        suffix_is_subscript =
            expr_parse_cursor_peek_value_at(cursor,
                                            suffix_pos,
                                            &suffix_value,
                                            &suffix_len) &&
            expr_parse_is_subscript_digit(suffix_value);

        if (!string_cursor_peek_ascii_at(cursor, suffix_pos, &suffix) ||
            suffix == '_' ||
            isdigit(suffix) ||
            suffix_is_subscript ||
            (!isalnum(suffix) && suffix != '_'))
            return true;
    }

    return false;
}

static string_t *read_simple_name(string_cursor_t *cursor,
                                  bool allow_plain_letters_after_first)
{
    static const expr_parse_literal_t builtin_names[] = {
        EXPR_PARSE_LITERAL("pi"),
        EXPR_PARSE_LITERAL("phi"),
        EXPR_PARSE_LITERAL("gamma")
    };
    string_cursor_t *scan;
    string_t *out;
    uint32_t value = 0;
    size_t width = 0u;
    unsigned char b = 0u;
    bool allow_alias = false;

    scan = string_cursor_clone(cursor);
    if (!scan)
        return NULL;

    for (size_t i = 0u; i < sizeof(builtin_names) / sizeof(builtin_names[0]); ++i) {
        if (string_cursor_match_at(
                scan, string_cursor_position(scan), builtin_names[i].text) &&
            expr_parse_cursor_at_identifier_boundary(
                scan, string_cursor_position(scan) + builtin_names[i].len)) {
            string_cursor_free(scan);
            return cursor_read_literal_name(cursor, builtin_names[i]);
        }
    }

    out = string_new();
    if (!out) {
        string_cursor_free(scan);
        return NULL;
    }

    if (string_cursor_peek_ascii(scan, &b) && b == '@') {
        if (!cursor_alias_is_accepted(scan, string_cursor_position(scan) + 1u)) {
            string_free(out);
            string_cursor_free(scan);
            return NULL;
        }
        if (string_append_char(out, '@') != 0) {
            string_free(out);
            string_cursor_free(scan);
            return NULL;
        }
        string_cursor_skip(scan, 1u);
        allow_alias = true;
    }

    if (!expr_parse_cursor_peek_value(scan, &value, &width) ||
        !fs_is_letter(value)) {
        string_free(out);
        string_cursor_free(scan);
        return NULL;
    }

    if (allow_alias) {
        while (string_cursor_peek_ascii(scan, &b) && isalpha(b)) {
            if (string_append_char(out, (char)b) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, 1u);
        }
    } else {
        string_pos_t start_pos = string_cursor_position(scan);

        string_cursor_skip(scan, width);
        if (string_cursor_append_slice_between(
                out, start_pos, string_cursor_position(scan), cursor) != 0) {
            string_free(out);
            string_cursor_free(scan);
            return NULL;
        }
    }

    for (;;) {
        string_pos_t start_pos = string_cursor_position(scan);
        uint32_t next_value = 0;
        size_t next_width = 0u;
        unsigned char next = 0u;

        if (expr_parse_cursor_peek_value(scan, &next_value, &next_width) &&
            expr_parse_is_subscript_digit(next_value)) {
            string_cursor_skip(scan, next_width);
            if (string_cursor_append_slice_between(
                    out, start_pos, string_cursor_position(scan), cursor) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            continue;
        }

        if (allow_plain_letters_after_first &&
            expr_parse_cursor_peek_value(scan, &next_value, &next_width) &&
            fs_is_letter(next_value)) {
            if (allow_alias && next_value >= 128u) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, next_width);
            if (string_cursor_append_slice_between(
                    out, start_pos, string_cursor_position(scan), cursor) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            continue;
        }

        if (allow_alias &&
            string_cursor_peek_ascii(scan, &b) &&
            isdigit(b)) {
            if (string_append_char(out, (char)b) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, 1u);
            continue;
        }

        if (!allow_alias &&
            string_cursor_peek_ascii(scan, &b) &&
            isdigit(b)) {
            if (append_subscript_digit(out, b) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, 1u);
            continue;
        }

        if (allow_alias &&
            string_cursor_peek_ascii(scan, &b) &&
            b == '_' &&
            string_cursor_peek_ascii_at(scan,
                                        string_cursor_position(scan) + 1u,
                                        &next) &&
            isdigit(next)) {
            if (string_append_char(out, '_') != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, 1u);
            while (string_cursor_peek_ascii(scan, &b) && isdigit(b)) {
                if (string_append_char(out, (char)b) != 0) {
                    string_free(out);
                    string_cursor_free(scan);
                    return NULL;
                }
                string_cursor_skip(scan, 1u);
            }
            continue;
        }

        if (!allow_alias &&
            string_cursor_peek_ascii(scan, &b) &&
            b == '_' &&
            string_cursor_peek_ascii_at(scan,
                                        string_cursor_position(scan) + 1u,
                                        &next) &&
            isdigit(next)) {
            if (append_subscript_digit(out, next) != 0) {
                string_free(out);
                string_cursor_free(scan);
                return NULL;
            }
            string_cursor_skip(scan, 2u);
            continue;
        }

        break;
    }

    string_cursor_seek(cursor, string_cursor_position(scan));
    string_cursor_free(scan);
    return out;
}

string_t *expr_parse_read_name(string_cursor_t *cursor,
                               bool allow_plain_letters_after_first)
{
    static const expr_parse_literal_t special_names[] = {
        EXPR_PARSE_LITERAL("@pi"),
        EXPR_PARSE_LITERAL("@phi"),
        EXPR_PARSE_LITERAL("@gamma"),
        EXPR_PARSE_LITERAL("@tau"),
        EXPR_PARSE_LITERAL("pi")
    };
    unsigned char b;

    if (!cursor)
        return NULL;

    if (string_cursor_peek_ascii(cursor, &b) && b == '[')
        return read_bracketed_name(cursor);

    for (size_t i = 0u; i < sizeof(special_names) / sizeof(special_names[0]); ++i) {
        if (string_cursor_match_at(
                cursor, string_cursor_position(cursor), special_names[i].text) &&
            expr_parse_cursor_at_identifier_boundary(
                cursor,
                string_cursor_position(cursor) + special_names[i].len))
            return cursor_read_literal_name(cursor, special_names[i]);
    }

    return read_simple_name(cursor, allow_plain_letters_after_first);
}

int expr_parse_read_superscript_int(string_cursor_t *cursor)
{
    string_cursor_t *scan;
    uint32_t value;
    int digit;
    int result = -1;

    scan = string_cursor_clone(cursor);
    if (!scan)
        return -1;

    while (expr_parse_cursor_peek_value(scan, &value, NULL) &&
           expr_parse_is_superscript_digit(value)) {
        digit = expr_parse_superscript_digit_value(value);
        if (digit < 0)
            break;
        result = (result < 0) ? digit : result * 10 + digit;
        (void)string_cursor_next(scan);
    }

    if (result >= 0)
        string_cursor_seek(cursor, string_cursor_position(scan));
    string_cursor_free(scan);
    return result;
}
