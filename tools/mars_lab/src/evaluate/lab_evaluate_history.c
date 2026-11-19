/**
 * @file lab_evaluate_history.c
 * @brief Native source-preserving formula previews for the worksheet history list.
 *
 * Uses the owning expression and equation parsers to typeset authored operations without calculating their results.
 * Exact bindings and conditions come from the existing editor metadata adapter. Only disposable display strings
 * abbreviate numeric tokens; saved worksheets remain unchanged. Optional SVG uses the restricted TeX service,
 * with a small preview size limit and a literal fallback when parsing or rendering is unavailable.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_LAB_EVALUATE_INTERNAL_ACCESS
#include "lab_evaluate_internal.h"
#define MARS_LAB_PRESENTATION_INTERNAL_ACCESS
#include "lab_presentation_internal.h"

static string_t *history_compact(const string_t *source, bool TeX)
{
    const char *bytes = string_c_str(source);
    size_t length = string_byte_length(source), position = 0, copied = 0;
    string_t *out = string_new();
    bool ok = out != NULL;
    while (ok && position < length) {
        unsigned char byte = (unsigned char)bytes[position];
        if (!isdigit(byte)) { ++position; continue; }
        size_t start = position;
        while (position < length && (isdigit((unsigned char)bytes[position]) ||
            (bytes[position] == '.' && position + 1 < length && isdigit((unsigned char)bytes[position + 1])))) ++position;
        bool identifier = start && (isalnum((unsigned char)bytes[start - 1]) || bytes[start - 1] == '_');
        if (identifier || position - start <= 20) continue;
        ok = !string_append_bytes(out, bytes + copied, start + 18 - copied) &&
             !string_append_cstr(out, TeX ? "\\ldots{}" : "...");
        copied = position;
    }
    ok = ok && !string_append_bytes(out, bytes + copied, length - copied);
    if (!ok) { string_free(out); return NULL; }
    return out;
}

/* Extract a complete factorisation argument without invoking its mathematical constructor. */
static string_t *history_factor_argument(const char *source)
{
    string_t *text = string_new_with(source);
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor) { string_free(text); return NULL; }
    string_cursor_skip_spaces(cursor);
    unsigned wrappers = 0, depth = 0;
    while (string_cursor_consume(cursor, "(")) { ++wrappers; string_cursor_skip_spaces(cursor); }
    bool matched = string_cursor_consume(cursor, "factors") || string_cursor_consume(cursor, "factor");
    string_cursor_skip_spaces(cursor);
    matched = matched && string_cursor_consume(cursor, "(");
    if (matched) depth = 1;
    string_pos_t start = string_cursor_position(cursor), end = start;
    bool quoted = false, escaped = false;
    unsigned brackets = 0;
    while (depth && !string_cursor_done(cursor)) {
        unsigned char byte = 0;
        if (string_cursor_peek_ascii(cursor, &byte)) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (byte == '\\') escaped = true;
                else if (byte == '"') quoted = false;
            } else if (byte == '[') ++brackets;
            else if (byte == ']' && brackets) --brackets;
            else if (!brackets && byte == '"') quoted = true;
            else if (!brackets && byte == '(') ++depth;
            else if (!brackets && byte == ')') {
                if (--depth == 0) end = string_cursor_position(cursor);
            }
        }
        string_cursor_next(cursor);
    }
    string_cursor_skip_spaces(cursor);
    while (wrappers && string_cursor_consume(cursor, ")")) { --wrappers; string_cursor_skip_spaces(cursor); }
    matched = matched && !depth && !wrappers && string_cursor_done(cursor);
    string_t *argument = matched ? string_cursor_slice_between(start, end, cursor) : NULL;
    if (argument) string_trim(argument);
    string_cursor_free(cursor);
    string_free(text);
    return argument;
}

static TeX_t *history_operand(void *context, const string_t *text)
{
    (void)context;
    expr_t *expression = expr_from_notation_text_internal(text, true);
    char *body = expression ? expr_to_TeX_operation_body(expression) : NULL;
    TeX_builder_t *builder = body ? TeX_builder_new() : NULL;
    TeX_t *result = builder && TeX_raw(builder, body) ? TeX_builder_finish(builder) : NULL;
    TeX_builder_free(builder);
    free(body);
    expr_free(expression);
    return result;
}

static string_t *history_formula(const char *source)
{
    string_t *text = string_new_with(source);
    bool recognised = false;
    TeX_t *notation = text ? equ_notation_input(text, history_operand, NULL, &recognised) : NULL;
    if (text && !recognised) notation = history_operand(NULL, text);
    if (text && !notation) notation = TeX_from_expression(text);
    string_t *result = notation ? string_new_with(TeX_source(notation)) : NULL;
    TeX_free(notation);
    string_free(text);
    return result;
}

/* Long integer calls retain a bounded run of actual TeX digits for responsive glyph clipping. */
static string_t *history_factor_formula(const string_t *argument, bool *fit)
{
    const char *bytes = string_c_str(argument), *digits = bytes;
    if (*digits == '+' || *digits == '-') ++digits;
    size_t length = string_byte_length(argument);
    *fit = length > 20 && *digits && digits[strspn(digits, "0123456789")] == '\0';
    if (*fit) {
        size_t prefix = length < 1024 ? length : 1024;
        return string_sprintf("\\operatorname{factor}(%.*s%s)", (int)prefix, bytes,
                              prefix < length ? "\\ldots{}" : "");
    }
    string_t *body = history_formula(bytes);
    string_t *result = body ? string_sprintf("\\operatorname{factor}(%s)", string_c_str(body)) : NULL;
    string_free(body);
    return result;
}

static bool history_details(string_t *out, const json_t *editor)
{
    const json_t *bindings = lab_eval_get(editor, "bindings"), *conditions = lab_eval_get(editor, "conditions");
    bool ok = true;
    for (size_t i = 0; ok && i < json_array_size(bindings); ++i) {
        const json_t *binding = json_array_get(bindings, i);
        string_t *name = history_formula(lab_eval_text(binding, "name"));
        const char *value = lab_eval_text(binding, "value");
        string_t *notation = !*value || !strcmp(value, "?") ? string_new_with("?") : history_formula(value);
        ok = name && notation && (!i || !string_append_cstr(out, ",\\;")) &&
             !string_append_string(out, name) && !string_append_char(out, '=') && !string_append_string(out, notation);
        string_free(name);
        string_free(notation);
    }
    for (size_t i = 0; ok && i < json_array_size(conditions); ++i) {
        const string_t *condition = json_string_value(json_array_get(conditions, i));
        string_t *notation = condition ? history_formula(string_c_str(condition)) : NULL;
        ok = notation && (!string_byte_length(out) || !string_append_cstr(out, ";\\;")) &&
             !string_append_string(out, notation);
        string_free(notation);
    }
    return ok;
}

/* Build presentation-only metadata; authored arithmetic, bindings and saved values remain independent. */
json_t *lab_pres_history_metadata(const string_t *source)
{
    json_t *result = json_new_object(), *editor = lab_pres_editor_metadata(source);
    string_t *argument = editor ? history_factor_argument(lab_eval_text(editor, "body")) : NULL;
    string_t *short_argument = argument ? history_compact(argument, false) : NULL;
    string_t *display = short_argument ? string_sprintf("factor(%s)", string_c_str(short_argument)) :
                                       history_compact(source, false);
    bool fit = false;
    string_t *body = argument ? history_factor_formula(argument, &fit) :
                    editor ? history_formula(lab_eval_text(editor, "body")) : NULL;
    string_t *details = string_new(), *TeX = NULL;
    if (body && details && (argument || history_details(details, editor))) {
        TeX = string_byte_length(details) ? string_sprintf("\\begin{aligned}[t]&%s\\\\[.3em]&\\textstyle{%s}\\end{aligned}",
            string_c_str(body), string_c_str(details)) : string_clone(body);
    }
    string_t *compact = TeX ? fit ? string_clone(TeX) : history_compact(TeX, true) : NULL;
    bool ok = result && display && lab_eval_set(result, "source", string_c_str(source)) &&
              lab_eval_set(result, "display", string_c_str(display));
    if (ok && fit) ok = lab_eval_set(result, "tail_TeX", "\\ldots)");
    if (ok && compact && string_byte_length(compact) <= 4096) ok = lab_eval_set(result, "TeX", string_c_str(compact));
    json_free(editor);
    string_free(argument);
    string_free(short_argument);
    string_free(display);
    string_free(body);
    string_free(details);
    string_free(TeX);
    string_free(compact);
    if (!ok) { json_free(result); return NULL; }
    return result;
}

/* Render native notation and any overflow ending, retaining readable text when SVG is unavailable. */
json_t *lab_eval_history_preview(const string_t *source, unsigned *status)
{
    json_t *result = lab_pres_history_metadata(source);
    if (!result) { if (status) *status = 500; return NULL; }
    lab_eval_put(result, "ok", json_new_bool(true));
    static const char *const fields[][2] = {
        {"TeX", "svg"},
        {"tail_TeX", "tail_svg"}
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(*fields); ++i) {
        const string_t *TeX = json_string_value(lab_eval_get(result, fields[i][0]));
        if (!TeX) continue;
        diagnostic_t *error = NULL;
        string_t *svg = lab_eval_render(TeX, &error);
        if (svg && string_byte_length(svg) <= 262144) lab_eval_set(result, fields[i][1], string_c_str(svg));
        diagnostic_free(error);
        string_free(svg);
    }
    if (status) *status = 200;
    return result;
}
