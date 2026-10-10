/**
 * @file lab_evaluate_syntax.c
 * @brief Native lexical spans and escaped display markup for Function cards and matrix section headings.
 *
 * Reads exact Unicode text through string_t cursors. Classifies comments, quoted
 * strings, numbers, identifiers, array variables and call parentheses without
 * parsing or evaluating mathematics. A bounded token pass collects array names
 * in a keyed JSON object; a second pass emits lossless spans for DOM rendering.
 * All storage is local to a call. Incomplete source remains displayable.
 * Matrix headings are recognised only as complete trimmed lines; surrounding
 * whitespace and all non-heading output remain exact source text.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ustring.h"
#include "lab_evaluate_internal.h"
#include "lab_presentation_internal.h"
#include "lab_syntax_internal.h"

enum { syntax_plain, syntax_identifier, syntax_keyword, syntax_number, syntax_constant,
       syntax_comment, syntax_variable, syntax_function, syntax_bracket, syntax_array };
enum { lab_syntax_span_limit = 1024 };

/* Produce final escaped markup from the already classified, bounded native spans. */
static bool lab_syntax_html(json_t *result, const json_t *spans)
{
    string_t *html = string_new();
    bool ok = html != NULL;
    for (size_t i = 0; ok && i < json_array_size(spans); ++i) {
        const json_t *span = json_array_get(spans, i);
        const char *kind = lab_eval_text(span, "kind");
        bool heading = !strcmp(kind, "heading");
        string_t *text = string_new_with(lab_eval_text(span, heading ? "display" : "text"));
        if (*kind) {
            if (heading) ok = !string_append_cstr(html, "<span class=\"matrix-section-heading\">");
            else ok = string_append_format(html, "<span class=\"function-token-%s%s\">", kind,
                                           !strcmp(kind, "array") ? " function-token-variable" : "") >= 0;
        }
        ok = ok && text && lab_pres_html_text(html, text);
        if (ok && *kind) ok = !string_append_cstr(html, "</span>");
        string_free(text);
    }
    if (ok && json_array_size(spans)) ok = lab_eval_set(result, "html", string_c_str(html));
    string_free(html);
    return ok;
}

static bool lab_heading_span(json_t *spans, const string_t *source, size_t begin, size_t end,
                             const char *display)
{
    if (begin == end) return true;
    string_view_t view = string_view(source, begin, end - begin);
    string_t *text = string_from_view(&view);
    json_t *span = json_new_object();
    bool ok = text && span && lab_eval_set(span, "text", string_c_str(text)) &&
              lab_eval_set(span, "kind", display ? "heading" : "");
    if (ok && display) ok = lab_eval_set(span, "display", display);
    string_free(text);
    if (!ok) {
        json_free(span);
        return false;
    }
    return lab_pres_append(spans, span);
}

/* Classify headings natively; never reinterpret numerical or symbolic result lines. */
json_t *lab_pres_matrix_headings(const string_t *source)
{
    if (!source || strlen(string_c_str(source)) != string_byte_length(source)) return NULL;
    size_t bytes = string_byte_length(source), plain = 0u;
    json_t *result = json_new_object(), *spans = json_new_array();
    string_cursor_t *cursor = string_cursor_new(source);
    bool ok = result && spans && cursor, bounded = bytes <= 65536u;
    while (ok && bounded && !string_cursor_done(cursor)) {
        size_t begin = string_cursor_position(cursor);
        while (!string_cursor_done(cursor) && !rune_is_equal(string_cursor_peek(cursor), '\n'))
            string_cursor_next(cursor);
        size_t end = string_cursor_position(cursor);
        string_view_t view = string_view_trim(string_view(source, begin, end - begin));
        size_t length = string_view_length(view);
        /* Two fixed labels, dispatched by length; no open-ended keyword scan. */
        if (length == 11u || length == 12u) {
            string_t *word = string_from_view(&view);
            if (!word) { ok = false; break; }
            string_to_lower(word);
            const char *label = length == 11u ? "eigenvalues" : "eigenvectors";
            if (string_view_equals_literal(string_view_all(word), label)) {
                string_cursor_seek(cursor, begin);
                string_cursor_skip_spaces(cursor);
                size_t heading = string_cursor_position(cursor);
                size_t required = json_array_size(spans) + (plain < heading ? 1u : 0u) + 1u +
                                  (heading + length < bytes ? 1u : 0u);
                if (required > lab_syntax_span_limit) {
                    bounded = false;
                } else {
                    ok = lab_heading_span(spans, source, plain, heading, NULL) &&
                         lab_heading_span(spans, source, heading, heading + length, label);
                    plain = heading + length;
                }
                string_cursor_seek(cursor, end);
            }
            string_free(word);
        }
        if (!string_cursor_done(cursor)) string_cursor_next(cursor);
    }
    if (ok && bounded) ok = lab_heading_span(spans, source, plain, bytes, NULL);
    if (!bounded) {
        json_free(spans);
        spans = json_new_array();
        ok = ok && spans;
    }
    ok = ok && lab_eval_set(result, "source", string_c_str(source));
    ok = ok && lab_syntax_html(result, spans);
    if (!lab_eval_put(result, "spans", spans)) ok = false;
    string_cursor_free(cursor);
    if (!ok) { json_free(result); return NULL; }
    return result;
}

typedef struct {
    string_t *text;
    unsigned kind;
    bool word, space;
} lab_syntax_token_t;

static bool lab_syntax_ascii(string_cursor_t *cursor, char ch)
{
    return rune_is_equal(string_cursor_peek(cursor), ch);
}

static bool lab_syntax_digit(string_cursor_t *cursor)
{
    unsigned char ch = 0;
    return string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9';
}

static bool lab_syntax_word(rune_t rune, bool first)
{
    return rune_is_equal(rune, '_') || (rune_is_alpha_numeric(rune) && (!first || !rune_is_digit(rune)));
}

static bool lab_syntax_keyword(const char *text)
{
    /* Nine fixed language words: dispatch by byte length bounds comparisons. */
    size_t length = strlen(text);
    if (length == 1u) return !strcmp(text, "i");
    if (length == 2u) return !strcmp(text, "if");
    if (length == 4u) return !strcmp(text, "else");
    if (length == 5u) return !strcmp(text, "array") || !strcmp(text, "const");
    if (length == 6u) return !strcmp(text, "matrix") || !strcmp(text, "return");
    if (length == 8u) return !strcmp(text, "equation");
    return length == 10u && !strcmp(text, "expression");
}

static bool lab_syntax_named_constant(const string_t *text)
{
    const char *name = string_c_str(text);
    /* Seven immutable spellings; no user-controlled symbol-table scan. */
    size_t length = string_byte_length(text);
    if (length == 3u) return !strcmp(name, "@pi");
    if (length == 4u)
        return !strcmp(name, "@phi") || !strcmp(name, "@tau") || !strcmp(name, "@inf") || !strcmp(name, "@nan");
    if (length == 6u) return !strcmp(name, "@gamma");
    return length == 16u && !strcmp(name, "@eulermascheroni");
}

static void lab_syntax_quoted(string_cursor_t *cursor, char quote)
{
    string_cursor_next(cursor);
    bool escaped = false;
    while (!string_cursor_done(cursor)) {
        bool closing = lab_syntax_ascii(cursor, quote) && !escaped;
        bool slash = lab_syntax_ascii(cursor, '\\');
        string_cursor_next(cursor);
        if (closing) break;
        escaped = slash && !escaped;
    }
}

static void lab_syntax_number(string_cursor_t *cursor)
{
    while (lab_syntax_digit(cursor)) string_cursor_next(cursor);
    if (lab_syntax_ascii(cursor, '.')) {
        size_t point = string_cursor_position(cursor);
        string_cursor_next(cursor);
        if (!lab_syntax_digit(cursor)) string_cursor_seek(cursor, point);
        else while (lab_syntax_digit(cursor)) string_cursor_next(cursor);
    }
    if (lab_syntax_ascii(cursor, 'e') || lab_syntax_ascii(cursor, 'E')) {
        size_t exponent = string_cursor_position(cursor);
        string_cursor_next(cursor);
        if (lab_syntax_ascii(cursor, '+') || lab_syntax_ascii(cursor, '-')) string_cursor_next(cursor);
        if (!lab_syntax_digit(cursor)) string_cursor_seek(cursor, exponent);
        else while (lab_syntax_digit(cursor)) string_cursor_next(cursor);
    }
}

static bool lab_syntax_scan(string_cursor_t *cursor, lab_syntax_token_t *token)
{
    size_t start = string_cursor_position(cursor);
    unsigned char ch = 0;
    bool ascii = string_cursor_peek_ascii(cursor, &ch);
    bool named_candidate = ascii && ch == '@';
    token->kind = syntax_plain;
    if (ascii && isspace(ch)) {
        token->space = true;
        do { string_cursor_next(cursor); }
        while (string_cursor_peek_ascii(cursor, &ch) && isspace(ch));
    } else if (ascii && ch == '`') {
        string_cursor_next(cursor);
        if (lab_syntax_ascii(cursor, '`')) {
            while (!string_cursor_done(cursor) && !lab_syntax_ascii(cursor, '\n')) string_cursor_next(cursor);
        } else {
            string_cursor_seek(cursor, start);
            lab_syntax_quoted(cursor, '`');
        }
        token->kind = syntax_comment;
    } else if (ascii && (ch == '\'' || ch == '"')) {
        lab_syntax_quoted(cursor, (char)ch);
    } else if (ascii && ch == '$') {
        string_cursor_next(cursor);
        token->kind = syntax_keyword;
        if (lab_syntax_ascii(cursor, '[')) {
            size_t depth = 0u;
            do {
                if (lab_syntax_ascii(cursor, '[')) ++depth;
                if (lab_syntax_ascii(cursor, ']')) --depth;
                string_cursor_next(cursor);
            } while (!string_cursor_done(cursor) && depth);
            token->kind = syntax_variable;
        }
    } else if (lab_syntax_digit(cursor) || (ascii && ch == '.')) {
        if (ch == '.') {
            string_cursor_next(cursor);
            if (!lab_syntax_digit(cursor)) goto finished;
            string_cursor_seek(cursor, start);
        }
        lab_syntax_number(cursor);
        token->kind = syntax_number;
    } else if (lab_syntax_word(string_cursor_peek(cursor), true) || (ascii && ch == '@')) {
        token->word = ch != '@';
        string_cursor_next(cursor);
        while (lab_syntax_word(string_cursor_peek(cursor), false)) string_cursor_next(cursor);
        token->kind = token->word ? syntax_identifier : syntax_plain;
    } else {
        if (ascii && (ch == '[' || ch == ']')) token->kind = syntax_keyword;
        string_cursor_next(cursor);
    }
finished:
    token->text = string_cursor_slice_between(start, string_cursor_position(cursor), cursor);
    if (!token->text) return false;
    if (token->word && lab_syntax_keyword(string_c_str(token->text))) token->kind = syntax_keyword;
    if (lab_syntax_named_constant(token->text)) token->kind = syntax_constant;
    else if (named_candidate) {
        string_free(token->text);
        string_cursor_seek(cursor, start + 1u);
        token->text = string_new_with("@");
        if (!token->text) return false;
    }
    return true;
}

static bool lab_syntax_type(const char *word)
{
    return !strcmp(word, "expression") || !strcmp(word, "equation") || !strcmp(word, "matrix");
}

/* Produce lexical metadata without mutating or validating the programme. */
json_t *lab_pres_function_syntax(const string_t *source)
{
    if (!source) return NULL;
    size_t bytes = string_byte_length(source), count = 0u;
    if (strlen(string_c_str(source)) != bytes) return NULL;
    bool bounded = bytes <= 65536u;
    size_t capacity = bytes < lab_syntax_span_limit ? bytes : lab_syntax_span_limit;
    lab_syntax_token_t *tokens = bounded ? calloc(capacity + 1u, sizeof(*tokens)) : NULL;
    bool *brackets = bounded ? calloc(capacity + 1u, sizeof(*brackets)) : NULL;
    string_cursor_t *cursor = bounded ? string_cursor_new(source) : NULL;
    json_t *result = json_new_object(), *spans = json_new_array(), *arrays = json_new_object();
    bool ok = result && spans && arrays && (!bounded || (tokens && brackets && cursor));
    while (ok && bounded && count < lab_syntax_span_limit && !string_cursor_done(cursor)) {
        ok = lab_syntax_scan(cursor, &tokens[count]);
        ++count;
    }
    bool highlight = bounded && ok && string_cursor_done(cursor);
    size_t candidate = 0u, previous = count, depth = 0u;
    bool collecting_array = false, is_function = false, line_start = true, declaration = false;
    unsigned declaration_stage = 0u;
    for (size_t i = 0u; ok && highlight && i < count; ++i) {
        lab_syntax_token_t *token = &tokens[i];
        const char *text = string_c_str(token->text);
        if (token->space) {
            if (string_find(token->text, "\n") >= 0) line_start = true;
            continue;
        }
        if (token->kind == syntax_comment) continue;
        if (line_start) {
            declaration_stage = !strcmp(text, "array") ? 1u : lab_syntax_type(text) ? 2u : 0u;
            declaration = declaration_stage != 0u;
        } else if (declaration) {
            if (declaration_stage == 1u && lab_syntax_type(text)) declaration_stage = 2u;
            else if (declaration_stage == 2u && !strcmp(text, "$")) { /* Optional generated-name sigil. */ }
            else if (declaration_stage == 2u && token->word) declaration_stage = 3u;
            else if (declaration_stage == 3u && !strcmp(text, "(")) is_function = true;
            else declaration = false;
        }
        line_start = false;
        if (!strcmp(text, "array")) {
            collecting_array = true;
            candidate = count;
        } else if (collecting_array && token->word && token->kind != syntax_keyword) {
            candidate = i;
        } else if (collecting_array && (!strcmp(text, "=") || !strcmp(text, ",") || !strcmp(text, ")"))) {
            if (candidate < count) {
                json_t *value = json_new_bool(true);
                ok = value && json_object_set(arrays, tokens[candidate].text, value);
                json_free(value);
            }
            collecting_array = false;
        } else if (collecting_array && (!strcmp(text, "(") || !strcmp(text, ";") || !strcmp(text, "{"))) {
            collecting_array = false;
        }
        if (!strcmp(text, "(")) {
            bool call = previous < count && tokens[previous].word;
            brackets[depth++] = call;
            if (call) token->kind = syntax_bracket;
        } else if (!strcmp(text, ")") && depth && brackets[--depth]) {
            token->kind = syntax_bracket;
        }
        previous = i;
    }
    size_t following = count;
    for (size_t i = count; ok && highlight && i > 0u;) {
        lab_syntax_token_t *token = &tokens[--i];
        if (token->space || token->kind == syntax_comment) continue;
        if (token->kind == syntax_identifier) {
            const char *name = string_c_str(token->text);
            bool call = following < count && !strcmp(string_c_str(tokens[following].text), "(");
            bool constant = !strcmp(name, "NAN") || !strcmp(name, "e") || !strcmp(name, "pi") || !strcmp(name, "π");
            token->kind = call ? syntax_function : constant ? syntax_plain :
                          json_object_get(arrays, token->text) ? syntax_array : syntax_variable;
        }
        following = i;
    }
    static const char *const kinds[] = {
        "", "", "keyword", "number", "constant", "comment", "variable", "function", "bracket", "array"
    };
    for (size_t i = 0u; ok && highlight && i < count; ++i) {
        json_t *span = json_new_object();
        ok = span && lab_eval_set(span, "text", string_c_str(tokens[i].text)) &&
             lab_eval_set(span, "kind", kinds[tokens[i].kind]);
        if (!lab_pres_append(spans, span)) ok = false;
    }
    ok = ok && lab_eval_set(result, "source", string_c_str(source)) &&
         lab_eval_put(result, "is_function", json_new_bool(is_function));
    ok = ok && lab_syntax_html(result, spans);
    if (!lab_eval_put(result, "spans", spans)) ok = false;
    for (size_t i = 0u; i < count; ++i) string_free(tokens[i].text);
    free(tokens);
    free(brackets);
    string_cursor_free(cursor);
    json_free(arrays);
    if (!ok) {
        json_free(result);
        return NULL;
    }
    return result;
}
