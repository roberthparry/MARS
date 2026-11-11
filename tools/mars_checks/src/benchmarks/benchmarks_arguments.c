/**
 * @file benchmarks_arguments.c
 * @brief Shared operand spelling for mathematical workers and saved benchmark reports.
 *
 * The catalogue stores real literals and a mask identifying operands with an imaginary component. Workers and
 * reports use the same imaginary literal. Historical reporting uses the saved literals and mask, rather than
 * today's catalogue. Integer orders remain real; no mathematical expression is evaluated while formatting.
 * Markdown input cells wrap long literals without truncation or changes to stored operands.
 */
#include <string.h>

#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

const char benchmark_imaginary_literal[] = "0.125";

/* Spell one worker operand without changing its authored real component. */
string_t *benchmark_argument_literal(const char *real, bool complex_value, bool imaginary)
{
    if (!real) return NULL;
    return complex_value ? string_sprintf("%s+%si", real, imaginary ? benchmark_imaginary_literal : "0") :
                           string_new_with(real);
}

/* Reconstruct the actual operand values represented by one stored benchmark case. */
string_t *benchmark_arguments_text(const char *arguments, const char *backend, unsigned complex_mask)
{
    if (!arguments || !backend) return NULL;
    if ((!strcmp(backend, "qcomplex") || !strcmp(backend, "mpc")) && complex_mask) {
        string_t *source = string_new_with(arguments), *out = string_new();
        size_t count = 0;
        string_t **parts = source ? string_split(source, ",", &count) : NULL;
        bool ok = out && parts && count > 0 && count <= 6 && (complex_mask >> count) == 0;
        for (size_t i = 0; ok && i < count; ++i) {
            string_trim(parts[i]);
            bool imaginary = (complex_mask & (1u << i)) != 0;
            string_t *value = benchmark_argument_literal(string_c_str(parts[i]), imaginary, imaginary);
            if (i) ok = string_append_cstr(out, ", ") >= 0;
            ok = value && ok && string_append_cstr(out, string_c_str(value)) >= 0;
            string_free(value);
        }
        string_split_free(parts, count);
        string_free(source);
        if (!ok) { string_free(out); return NULL; }
        return out;
    }
    return string_new_with(arguments);
}

/* Bound long Markdown input cells while preserving every literal character. */
string_t *benchmark_arguments_markdown(const string_t *inputs, bool integer_layout)
{
    string_cursor_t *cursor = inputs ? string_cursor_new(inputs) : NULL;
    string_t *out = cursor ? string_new() : NULL;
    size_t width = integer_layout ? 80 : 40;
    bool wrapped = inputs && (string_length(inputs) > width || (integer_layout && string_find(inputs, ",") >= 0));
    bool ok = cursor && out;
    if (ok && wrapped)
        ok = string_append_format(out, "<span style=\"display:inline-block;width:%zuch;max-width:100%;"
                                      "white-space:normal;overflow-wrap:anywhere\">", width) >= 0;
    size_t column = 0;
    while (ok && !string_cursor_done(cursor)) {
        unsigned value = rune_value(string_cursor_peek(cursor));
        if (integer_layout && value == ',') {
            ok = string_append_cstr(out, ",") >= 0;
            string_cursor_next(cursor);
            while (ok && !string_cursor_done(cursor) && rune_value(string_cursor_peek(cursor)) == ' ') {
                ok = string_append_cstr(out, " ") >= 0;
                string_cursor_next(cursor);
            }
            ok = ok && string_append_cstr(out, "<br/>") >= 0;
            column = 0;
            continue;
        }
        if (column == width) {
            ok = string_append_cstr(out, "<br/>") >= 0;
            column = 0;
        }
        string_pos_t start = string_cursor_position(cursor);
        string_cursor_next(cursor);
        const char *escaped = value == '&' ? "&amp;" : value == '<' ? "&lt;" :
                              value == '>' ? "&gt;" : value == '|' ? "&#124;" : NULL;
        if (escaped) ok = ok && string_append_cstr(out, escaped) >= 0;
        else {
            if (value < 128 && strchr("\\*_`[]", (int)value))
                ok = ok && string_append_cstr(out, "\\") >= 0;
            ok = ok && !string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor);
        }
        ++column;
    }
    if (ok && wrapped) ok = string_append_cstr(out, "</span>") >= 0;
    string_cursor_free(cursor);
    if (!ok) { string_free(out); return NULL; }
    return out;
}
