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

#include "TeX-t.h"
#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

const char benchmark_imaginary_literal[] = "0.125";

/* Recognise the two exact profiles without inspecting their mathematical operands. */
bool benchmark_backend_exact(const char *backend)
{
    return backend && (!strcmp(backend, "exact-integer") || !strcmp(backend, "exact-fraction"));
}

/* Keep saved backend identifiers stable while displaying the requested descriptive names. */
const char *benchmark_backend_title(const char *backend)
{
    if (!strcmp(backend, "exact-integer")) return "number / unrestricted integer";
    if (!strcmp(backend, "exact-fraction")) return "number / unrestricted fraction";
    return backend;
}

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

/* Recognise an authored integer/integer literal without evaluating or reducing either component. */
static size_t benchmark_fraction_separator(const string_t *input)
{
    const char *text = string_c_str(input), *slash = strchr(text, '/');
    if (!slash || slash == text || !slash[1]) return 0;
    bool digit = false, denominator_nonzero = false;
    for (const char *p = text; *p; ++p) {
        if ((*p == '-' || *p == '+') && (p == text || p == slash + 1)) continue;
        if (p == slash) {
            if (!digit) return 0;
            digit = false;
        } else {
            if (*p < '0' || *p > '9') return 0;
            digit = true;
            if (p > slash && *p != '0') denominator_nonzero = true;
        }
    }
    return digit && denominator_nonzero ? (size_t)(slash - text) : 0;
}

static bool benchmark_fraction_digits(string_t *out, const char *text, size_t length, bool lower)
{
    static const char *const digits[2][12] = {
        {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹", "⁻", "⁺"},
        {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉", "₋", "₊"}
    };
    /* Every digit must be emitted; direct indexing avoids searching the character mapping. */
    for (size_t i = 0; i < length; ++i) {
        unsigned index = text[i] == '-' ? 10 : text[i] == '+' ? 11 : (unsigned)(text[i] - '0');
        if (string_append_cstr(out, digits[lower][index]) < 0) return false;
    }
    return true;
}

/* Literal digit rows retain every authored sign and leading zero; TeX owns their mathematical layout. */
static bool benchmark_fraction_component(TeX_builder_t *builder, const char *text, size_t length)
{
    size_t rows = (length + 79) / 80;
    for (size_t offset = 0; offset < length; offset += 80) {
        size_t count = length - offset < 80 ? length - offset : 80;
        string_t *digits = string_new();
        bool ok = digits && string_append_bytes(digits, text + offset, count) >= 0 && TeX_text(builder, digits);
        string_free(digits);
        if (!ok || (rows > 1 && (!TeX_integer(builder, 1) || !TeX_list(builder)))) return false;
    }
    return rows == 1 || (TeX_integer(builder, (long)rows) && TeX_list(builder) && TeX_align(builder));
}

static string_t *benchmark_fraction_TeX(const char *text, size_t slash)
{
    TeX_builder_t *builder = TeX_builder_new();
    bool ok = builder && benchmark_fraction_component(builder, text, slash) &&
              benchmark_fraction_component(builder, text + slash + 1, strlen(text + slash + 1)) && TeX_frac(builder);
    TeX_t *fraction = ok ? TeX_builder_finish(builder) : NULL;
    string_t *source = fraction ? TeX_render(fraction) : NULL;
    TeX_free(fraction);
    TeX_builder_free(builder);
    return source;
}

static string_t *benchmark_fraction_arguments(const string_t *inputs, bool markdown, bool *changed)
{
    size_t count = 0, separators[6] = {0};
    string_t **parts = string_split(inputs, ",", &count);
    string_t *out = NULL;
    *changed = false;
    bool ok = parts && count && count <= 6;
    for (size_t i = 0; ok && i < count; ++i) {
        string_trim(parts[i]);
        separators[i] = benchmark_fraction_separator(parts[i]);
        *changed = *changed || separators[i] != 0;
    }
    if (!ok || !*changed) {
        string_split_free(parts, count);
        return ok ? string_clone(inputs) : NULL;
    }
    out = string_new();
    ok = out != NULL;
    for (size_t i = 0; ok && i < count; ++i) {
        if (i) ok = string_append_cstr(out, markdown ? "<br/><br/>" : ", ") >= 0;
        if (markdown && count > 1)
            ok = ok && string_append_format(out, "**Input %zu**<br/>", i + 1) >= 0;
        size_t slash = separators[i];
        const char *text = string_c_str(parts[i]);
        if (slash && markdown) {
            string_t *source = benchmark_fraction_TeX(text, slash);
            ok = ok && source && string_append_format(out, "$%s$", string_c_str(source)) >= 0;
            string_free(source);
        } else if (slash) {
            ok = ok && benchmark_fraction_digits(out, text, slash, false) && string_append_cstr(out, "⁄") >= 0 &&
                 benchmark_fraction_digits(out, text + slash + 1, strlen(text + slash + 1), true);
        } else {
            string_t *literal = markdown ? benchmark_arguments_markdown(parts[i], false) : string_clone(parts[i]);
            ok = ok && literal && string_append_cstr(out, string_c_str(literal)) >= 0;
            string_free(literal);
        }
    }
    string_split_free(parts, count);
    if (!ok) { string_free(out); return NULL; }
    return out;
}

/* Keep machine operands separate from Unicode presentation stored alongside them. */
string_t *benchmark_arguments_display(const char *arguments, const char *backend, unsigned complex_mask)
{
    string_t *literal = benchmark_arguments_text(arguments, backend, complex_mask);
    if (!literal || strcmp(backend, "exact-fraction")) return literal;
    bool changed;
    string_t *display = benchmark_fraction_arguments(literal, false, &changed);
    string_free(literal);
    return display;
}

/* Bound long Markdown input cells while preserving every literal character. */
string_t *benchmark_arguments_markdown(const string_t *inputs, bool exact_layout)
{
    if (inputs && exact_layout) {
        bool changed;
        string_t *fractions = benchmark_fraction_arguments(inputs, true, &changed);
        if (!fractions || changed) return fractions;
        string_free(fractions);
    }
    string_cursor_t *cursor = inputs ? string_cursor_new(inputs) : NULL;
    string_t *out = cursor ? string_new() : NULL;
    size_t width = exact_layout ? 80 : 40;
    bool wrapped = inputs && (string_length(inputs) > width || (exact_layout && string_find(inputs, ",") >= 0));
    bool ok = cursor && out;
    if (ok && wrapped)
        ok = string_append_format(out, "<span style=\"display:inline-block;width:%zuch;max-width:100%;"
                                      "white-space:normal;overflow-wrap:anywhere\">", width) >= 0;
    size_t column = 0;
    while (ok && !string_cursor_done(cursor)) {
        unsigned value = rune_value(string_cursor_peek(cursor));
        if (exact_layout && value == ',') {
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
