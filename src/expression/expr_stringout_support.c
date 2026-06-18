#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_stringout.h"
#include "expression.h"

char *expr_tostring_texify(const char *text);

static int expr_tostring_text_needs_parse_for_tex_local(const char *text)
{
    return text &&
        (strchr(text, '/') != NULL ||
         strchr(text, '^') != NULL ||
         strchr(text, '(') != NULL ||
         strchr(text, ')') != NULL ||
         strstr(text, "√") != NULL ||
         strstr(text, "·") != NULL);
}

static const char *expr_tostring_known_text_tex_local(const char *text)
{
    if (!text)
        return NULL;
    if (strcmp(text, "1/√π") == 0)
        return "\\frac{1}{\\sqrt{\\pi}}";
    if (strcmp(text, "2/√π") == 0)
        return "\\frac{2}{\\sqrt{\\pi}}";
    if (strcmp(text, "-2/√π") == 0)
        return "-\\frac{2}{\\sqrt{\\pi}}";
    if (strcmp(text, "√π") == 0)
        return "\\sqrt{\\pi}";
    if (strcmp(text, "√(2π)") == 0)
        return "\\sqrt{2\\pi}";
    if (strcmp(text, "1/√(2π)") == 0)
        return "\\frac{1}{\\sqrt{2\\pi}}";
    if (strcmp(text, "√(π/2)") == 0)
        return "\\sqrt{\\pi/2}";
    if (strcmp(text, "√2") == 0)
        return "\\sqrt{2}";
    if (strcmp(text, "√3") == 0)
        return "\\sqrt{3}";
    if (strcmp(text, "√(1/2)") == 0)
        return "\\sqrt{1/2}";
    if (strcmp(text, "√2/2") == 0)
        return "\\frac{\\sqrt{2}}{2}";
    if (strcmp(text, "√3/2") == 0)
        return "\\frac{\\sqrt{3}}{2}";
    return NULL;
}

void *expr_tostring_xmalloc(size_t n)
{
    void *p = malloc(n);

    if (!p) {
        fprintf(stderr, "expr_to_string: out of memory\n");
        abort();
    }
    return p;
}

char *expr_tostring_xstrdup(const char *s)
{
    size_t n;
    char *p;

    if (!s)
        return NULL;

    n = strlen(s) + 1;
    p = (char *)expr_tostring_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

char *expr_text_to_tex_local(const char *text)
{
    const char *known_tex;
    expr_t *parsed;
    string_t *wrapped = NULL;
    string_t *tex_text;
    char *tex;

    if (!text)
        return NULL;

    known_tex = expr_tostring_known_text_tex_local(text);
    if (known_tex)
        return expr_tostring_xstrdup(known_tex);

    if (!expr_tostring_text_needs_parse_for_tex_local(text))
        return expr_tostring_texify(text);

    wrapped = string_sprintf("{ %s }", text);
    parsed = wrapped ? expr_from_string(string_c_str(wrapped), NULL) : NULL;
    string_free(wrapped);
    if (!parsed)
        return expr_tostring_texify(text);

    tex_text = expr_to_text(parsed, style_TEX);
    tex = tex_text ? expr_tostring_xstrdup(string_c_str(tex_text)) : NULL;
    string_free(tex_text);
    expr_free(parsed);
    return tex ? tex : expr_tostring_texify(text);
}

void sbuf_init(sbuf_t *b)
{
    b->text = string_new();
    if (!b->text) {
        fprintf(stderr, "expr_to_string: out of memory\n");
        abort();
    }
}

void sbuf_free(sbuf_t *b)
{
    if (!b)
        return;
    string_free(b->text);
    b->text = NULL;
}

void sbuf_reserve(sbuf_t *b, size_t extra)
{
    (void)b;
    (void)extra;
}

void sbuf_putc(sbuf_t *b, char c)
{
    if (!b || !b->text)
        return;
    if (string_append_char(b->text, c) != 0) {
        fprintf(stderr, "expr_to_string: out of memory\n");
        abort();
    }
}

void sbuf_puts(sbuf_t *b, const char *s)
{
    if (!b || !b->text || !s)
        return;

    if (string_append_cstr(b->text, s) != 0) {
        fprintf(stderr, "expr_to_string: out of memory\n");
        abort();
    }
}

void sbuf_put_string(sbuf_t *b, const string_t *s)
{
    if (!b || !b->text || !s)
        return;

    if (string_append_string(b->text, s) != 0) {
        fprintf(stderr, "expr_to_string: out of memory\n");
        abort();
    }
}

const char *sbuf_c_str(const sbuf_t *b)
{
    return (b && b->text) ? string_c_str(b->text) : "";
}

size_t sbuf_len(const sbuf_t *b)
{
    return (b && b->text) ? string_view_length(string_view_all(b->text)) : 0u;
}

string_t *sbuf_to_string(const sbuf_t *b)
{
    return (b && b->text) ? string_clone(b->text) : string_new();
}

char *sbuf_to_c_string(const sbuf_t *b)
{
    return expr_tostring_xstrdup(sbuf_c_str(b));
}

char *sbuf_take_c_string(sbuf_t *b)
{
    char *out = sbuf_to_c_string(b);

    sbuf_free(b);
    return out;
}

int expr_tostring_is_negative_const(const expr_t *f)
{
    if (!expr_is_unnamed_const(f))
        return 0;
    return num_is_real(f->c) && num_get_sign(f->c) < 0;
}

int expr_tostring_is_var_pow_d(const expr_t *f)
{
    return expr_is_pow_d_expr(f) && expr_is_var(f->a);
}

int expr_tostring_is_unicode_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return 1;
    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;
    return 0;
}

int expr_tostring_is_simple_name(const char *name)
{
    string_t *text;
    string_cursor_t *cursor;
    unsigned int c;
    int ok = 0;

    if (!name || !*name)
        return 0;

    text = string_new_with(name);
    cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor)
        goto done;

    c = rune_value(string_cursor_peek(cursor));
    if (!expr_tostring_is_unicode_letter(c))
        goto done;

    if (string_cursor_next(cursor) != 0)
        goto done;

    while (!string_cursor_done(cursor)) {
        unsigned int sc = rune_value(string_cursor_peek(cursor));

        if (sc < 0x2080 || sc > 0x2089)
            goto done;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    string_free(text);
    return ok;
}

void emit_name(sbuf_t *b, const char *name)
{
    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }

    if (expr_tostring_is_simple_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
}

int expr_tostring_is_safe_func_name(const char *name)
{
    string_t *text;
    string_cursor_t *cursor;
    unsigned int c;
    int ok = 0;

    if (!name || !*name)
        return 0;

    text = string_new_with(name);
    cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor)
        goto done;

    c = rune_value(string_cursor_peek(cursor));
    if (!expr_tostring_is_unicode_letter(c))
        goto done;

    if (string_cursor_next(cursor) != 0)
        goto done;

    while (!string_cursor_done(cursor)) {
        unsigned int sc = rune_value(string_cursor_peek(cursor));

        if (!expr_tostring_is_unicode_letter(sc) &&
            !(sc >= '0' && sc <= '9') &&
            !(sc >= 0x2080 && sc <= 0x2089))
            goto done;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    string_free(text);
    return ok;
}

void emit_name_func(sbuf_t *b, const char *name)
{
    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }

    if (expr_tostring_is_simple_name(name)) {
        sbuf_puts(b, name);
    } else {
        sbuf_putc(b, '[');
        sbuf_puts(b, name);
        sbuf_putc(b, ']');
    }
}

static int expr_tostring_is_superscript_cp(unsigned int c)
{
    return (c >= 0x2070 && c <= 0x2079) || c == 0x00B9 || c == 0x00B2 || c == 0x00B3 ||
           c == 0x207A || c == 0x207B || c == 0x207C || c == 0x207D || c == 0x207E;
}

static int expr_tostring_is_subscript_cp(unsigned int c)
{
    return (c >= 0x2080 && c <= 0x2089) ||
           c == 0x208A || c == 0x208B || c == 0x208C || c == 0x208D || c == 0x208E;
}

typedef struct {
    unsigned int codepoint;
    const char *text;
} expr_tostring_tex_map_t;

typedef struct {
    unsigned int codepoint;
    char ascii;
} expr_tostring_ascii_map_t;

#define EXPR_TEX_HASH_GREEK_SIZE       19u
#define EXPR_TEX_HASH_FRACTION_SIZE    21u
#define EXPR_TEX_HASH_SYMBOL_SIZE      9u
#define EXPR_ASCII_HASH_SUPERSCRIPT_SIZE 28u
#define EXPR_ASCII_SUBSCRIPT_BASE      0x2080u

static const expr_tostring_tex_map_t expr_tostring_greek_tex_table[EXPR_TEX_HASH_GREEK_SIZE] = {
    [0]  = {0x03A6, "\\Phi"},
    [3]  = {0x03A0, "\\Pi"},
    [4]  = {0x03A3, "\\Sigma"},
    [5]  = {0x03C4, "\\tau"},
    [6]  = {0x03BB, "\\lambda"},
    [7]  = {0x03C6, "\\phi"},
    [9]  = {0x03B8, "\\theta"},
    [10] = {0x03C0, "\\pi"},
    [11] = {0x03C3, "\\sigma"},
    [12] = {0x03BC, "\\mu"},
    [13] = {0x03D5, "\\phi"},
    [14] = {0x03B1, "\\alpha"},
    [15] = {0x03A9, "\\Omega"},
    [16] = {0x03B3, "\\gamma"},
    [17] = {0x03B2, "\\beta"},
    [18] = {0x03B4, "\\delta"}
};

static const expr_tostring_tex_map_t expr_tostring_vulgar_fraction_tex_table[EXPR_TEX_HASH_FRACTION_SIZE] = {
    [0]  = {0x00BD, "\\frac{1}{2}"},
    [1]  = {0x00BE, "\\frac{3}{4}"},
    [2]  = {0x2150, "\\frac{1}{7}"},
    [3]  = {0x2151, "\\frac{1}{9}"},
    [4]  = {0x2152, "\\frac{1}{10}"},
    [5]  = {0x2153, "\\frac{1}{3}"},
    [6]  = {0x2154, "\\frac{2}{3}"},
    [7]  = {0x2155, "\\frac{1}{5}"},
    [8]  = {0x2156, "\\frac{2}{5}"},
    [9]  = {0x2157, "\\frac{3}{5}"},
    [10] = {0x2158, "\\frac{4}{5}"},
    [11] = {0x2159, "\\frac{1}{6}"},
    [12] = {0x215A, "\\frac{5}{6}"},
    [13] = {0x215B, "\\frac{1}{8}"},
    [14] = {0x215C, "\\frac{3}{8}"},
    [15] = {0x215D, "\\frac{5}{8}"},
    [16] = {0x215E, "\\frac{7}{8}"},
    [20] = {0x00BC, "\\frac{1}{4}"}
};

static const expr_tostring_tex_map_t expr_tostring_symbol_tex_table[EXPR_TEX_HASH_SYMBOL_SIZE] = {
    [0]  = {0x2260, "\\neq"},
    [1]  = {0x2202, "\\partial"},
    [2]  = {0x221E, "\\infty"},
    [3]  = {0x00B7, " \\cdot "},
    [4]  = {0x2264, "\\leq"},
    [5]  = {0x2265, "\\geq"},
    [7]  = {0x221A, "\\sqrt{}"},
    [8]  = {0x00D7, " \\times "}
};

static const expr_tostring_ascii_map_t
expr_tostring_superscript_ascii_table[EXPR_ASCII_HASH_SUPERSCRIPT_SIZE] = {
    [0]  = {0x207C, '='},
    [1]  = {0x207D, '('},
    [2]  = {0x207E, ')'},
    [10] = {0x00B2, '2'},
    [11] = {0x00B3, '3'},
    [16] = {0x2070, '0'},
    [17] = {0x00B9, '1'},
    [20] = {0x2074, '4'},
    [21] = {0x2075, '5'},
    [22] = {0x2076, '6'},
    [23] = {0x2077, '7'},
    [24] = {0x2078, '8'},
    [25] = {0x2079, '9'},
    [26] = {0x207A, '+'},
    [27] = {0x207B, '-'}
};

static const char expr_tostring_subscript_ascii_table[] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    '+', '-', '=', '(', ')'
};

static size_t expr_tostring_greek_hash(unsigned int cp)
{
    return (cp ^ (cp >> 2) ^ 37u) % EXPR_TEX_HASH_GREEK_SIZE;
}

static size_t expr_tostring_fraction_hash(unsigned int cp)
{
    return cp % EXPR_TEX_HASH_FRACTION_SIZE;
}

static size_t expr_tostring_symbol_hash(unsigned int cp)
{
    return (cp ^ (cp >> 12)) % EXPR_TEX_HASH_SYMBOL_SIZE;
}

static size_t expr_tostring_superscript_ascii_hash(unsigned int cp)
{
    return cp % EXPR_ASCII_HASH_SUPERSCRIPT_SIZE;
}

static const char *expr_tostring_tex_lookup(const expr_tostring_tex_map_t *table,
                                          size_t idx,
                                          unsigned int cp)
{
    return table[idx].codepoint == cp ? table[idx].text : NULL;
}

static char expr_tostring_ascii_lookup(const expr_tostring_ascii_map_t *table,
                                     size_t idx,
                                     unsigned int cp)
{
    return table[idx].codepoint == cp ? table[idx].ascii : '\0';
}

static char expr_tostring_superscript_ascii(unsigned int c)
{
    return expr_tostring_ascii_lookup(
        expr_tostring_superscript_ascii_table,
        expr_tostring_superscript_ascii_hash(c),
        c);
}

static char expr_tostring_subscript_ascii(unsigned int c)
{
    unsigned int idx;

    if (c < EXPR_ASCII_SUBSCRIPT_BASE)
        return '\0';

    idx = c - EXPR_ASCII_SUBSCRIPT_BASE;
    if (idx >= sizeof(expr_tostring_subscript_ascii_table))
        return '\0';

    return expr_tostring_subscript_ascii_table[idx];
}

static const char *expr_tostring_greek_tex(unsigned int cp)
{
    return expr_tostring_tex_lookup(expr_tostring_greek_tex_table,
                                  expr_tostring_greek_hash(cp),
                                  cp);
}

static const char *expr_tostring_vulgar_fraction_tex(unsigned int cp)
{
    return expr_tostring_tex_lookup(expr_tostring_vulgar_fraction_tex_table,
                                  expr_tostring_fraction_hash(cp),
                                  cp);
}

static const char *expr_tostring_symbol_tex(unsigned int cp)
{
    return expr_tostring_tex_lookup(expr_tostring_symbol_tex_table,
                                  expr_tostring_symbol_hash(cp),
                                  cp);
}

static int expr_tostring_is_ascii_letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static void expr_tostring_collect_superscript(string_cursor_t *cursor,
                                              sbuf_t *tmp)
{
    while (!string_cursor_done(cursor)) {
        unsigned int cp = rune_value(string_cursor_peek(cursor));
        char mapped;

        if (!expr_tostring_is_superscript_cp(cp))
            break;

        mapped = expr_tostring_superscript_ascii(cp);
        if (mapped == '\0')
            break;

        sbuf_putc(tmp, mapped);
        if (string_cursor_next(cursor) != 0)
            break;
    }
}

static void expr_tostring_collect_subscript(string_cursor_t *cursor,
                                            sbuf_t *tmp)
{
    while (!string_cursor_done(cursor)) {
        unsigned int cp = rune_value(string_cursor_peek(cursor));
        char mapped;

        if (!expr_tostring_is_subscript_cp(cp))
            break;

        mapped = expr_tostring_subscript_ascii(cp);
        if (mapped == '\0')
            break;

        sbuf_putc(tmp, mapped);
        if (string_cursor_next(cursor) != 0)
            break;
    }
}

char *expr_tostring_texify(const char *text)
{
    sbuf_t out;
    string_t *source;
    string_cursor_t *cursor;

    sbuf_init(&out);

    source = string_new_with(text ? text : "");
    cursor = source ? string_cursor_new(source) : NULL;
    if (!cursor) {
        string_free(source);
        return sbuf_take_c_string(&out);
    }

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned int cp = rune_value(rune);
        const char *mapped;
        sbuf_t seq;

        mapped = expr_tostring_vulgar_fraction_tex(cp);
        if (mapped) {
            sbuf_puts(&out, mapped);
            string_cursor_next(cursor);
            continue;
        }

        if (expr_tostring_is_superscript_cp(cp)) {
            sbuf_t num;
            sbuf_t den;

            sbuf_init(&num);
            expr_tostring_collect_superscript(cursor, &num);
            if (rune_value(string_cursor_peek(cursor)) == 0x2044) {
                string_cursor_next(cursor);
                sbuf_init(&den);
                expr_tostring_collect_subscript(cursor, &den);
                if (sbuf_len(&den) > 0u) {
                    sbuf_puts(&out, "\\frac{");
                    sbuf_puts(&out, sbuf_c_str(&num));
                    sbuf_puts(&out, "}{");
                    sbuf_puts(&out, sbuf_c_str(&den));
                    sbuf_putc(&out, '}');
                    sbuf_free(&num);
                    sbuf_free(&den);
                    continue;
                }
                sbuf_free(&den);
            }

            sbuf_puts(&out, "^{");
            sbuf_puts(&out, sbuf_c_str(&num));
            sbuf_putc(&out, '}');
            sbuf_free(&num);
            continue;
        }

        if (expr_tostring_is_subscript_cp(cp)) {
            sbuf_init(&seq);
            expr_tostring_collect_subscript(cursor, &seq);
            sbuf_puts(&out, "_{");
            sbuf_puts(&out, sbuf_c_str(&seq));
            sbuf_putc(&out, '}');
            sbuf_free(&seq);
            continue;
        }

        mapped = expr_tostring_greek_tex(cp);
        if (mapped) {
            unsigned char next_ascii;

            sbuf_puts(&out, mapped);
            string_cursor_next(cursor);
            if (string_cursor_peek_ascii(cursor, &next_ascii) &&
                expr_tostring_is_ascii_letter((char)next_ascii))
                sbuf_puts(&out, "{}");
            continue;
        }

        mapped = expr_tostring_symbol_tex(cp);
        if (mapped) {
            sbuf_puts(&out, mapped);
            string_cursor_next(cursor);
            continue;
        }

        {
            string_t *rune_text = rune_to_string(rune);

            sbuf_puts(&out, string_c_str(rune_text));
            string_free(rune_text);
        }
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    string_free(source);
    return sbuf_take_c_string(&out);
}
