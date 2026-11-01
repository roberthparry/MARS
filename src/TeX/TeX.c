/**
 * @file TeX.c
 * @brief Application-independent composition and escaping of mathematical TeX.
 *
 * Stores eagerly composed immutable strings behind the public TeX_t interface.
 * Structured stack objects retain independently owned children. Nesting is bounded
 * to keep copying and destruction safe without sharing mutable ownership.
 * Rendering does not simplify mathematics or impose application presentation policy.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

struct _TeX_t {
    string_t *source;
    TeX_node_kind_t kind;
    bool has_integer;
    long integer;
    TeX_object_kind_t object_kind;
    TeX_t **items;
    size_t count;
    unsigned depth;
    TeX_matrix_style_t matrix_style;
    unsigned row_spacing;
};

static TeX_t *TeX_adopt(string_t *source)
{
    if (!source)
        return NULL;
    TeX_t *notation = calloc(1, sizeof *notation);
    if (!notation) {
        string_free(source);
        return NULL;
    }
    notation->source = source;
    notation->kind = TeX_NODE_COUNT;
    notation->has_integer = false;
    notation->integer = 0;
    return notation;
}

/* Retain presentation structure without exposing mathematical implementation objects. */
TeX_t *TeX_classify(TeX_t *notation, TeX_node_kind_t kind)
{
    if (notation)
        notation->kind = kind;
    return notation;
}

/* Inspect the presentation kind without parsing generated TeX source. */
TeX_node_kind_t TeX_kind(const TeX_t *notation)
{
    return notation ? notation->kind : TeX_NODE_COUNT;
}

/* Preserve structure when copying a finished or stack-owned value. */
TeX_t *TeX_copy(const TeX_t *notation)
{
    TeX_t *copy = notation ? TeX_classify(TeX_compose_raw(TeX_source(notation)), notation->kind) : NULL;
    if (copy) {
        copy->has_integer = notation->has_integer;
        copy->integer = notation->integer;
        copy->object_kind = notation->object_kind;
        copy->depth = notation->depth;
        copy->matrix_style = notation->matrix_style;
        copy->row_spacing = notation->row_spacing;
        if (notation->count) {
            copy->items = calloc(notation->count, sizeof *copy->items);
            if (!copy->items) {
                TeX_free(copy);
                return NULL;
            }
            for (size_t i = 0; i < notation->count; ++i) {
                copy->items[i] = TeX_copy(notation->items[i]);
                if (!copy->items[i]) {
                    TeX_free(copy);
                    return NULL;
                }
                ++copy->count;
            }
        }
    }
    return copy;
}

/* Keep literal integer controls separate from rendered mathematical source. */
TeX_t *TeX_compose_integer(long value)
{
    TeX_t *notation = TeX_adopt(string_sprintf("%ld", value));
    if (notation) {
        notation->kind = value < 0 ? TeX_NODE_NEGATIVE : TeX_NODE_NUMBER;
        notation->has_integer = true;
        notation->integer = value;
    }
    return notation;
}

/* Inspect an explicitly supplied integer without evaluating or parsing notation. */
bool TeX_integer_value(const TeX_t *notation, long *value)
{
    if (!notation || !notation->has_integer || !value)
        return false;
    *value = notation->integer;
    return true;
}

/* Inspect structural operands directly, without interpreting their printed form. */
TeX_object_kind_t TeX_object_kind(const TeX_t *notation)
{
    return notation ? notation->object_kind : TeX_OBJECT_FORMULA;
}

/* Borrow the directly indexed members until their immutable owner is released. */
const TeX_t *const *TeX_object_items(const TeX_t *notation, size_t *count)
{
    *count = notation ? notation->count : 0;
    return notation ? (const TeX_t *const *)notation->items : NULL;
}

/* Copy a validated collection into a new, bounded-depth immutable object. */
TeX_t *TeX_object_new(TeX_object_kind_t kind, const TeX_t *const *items, size_t count)
{
    if ((count && !items) || count > SIZE_MAX / sizeof(TeX_t *)) {
        errno = count > SIZE_MAX / sizeof(TeX_t *) ? EOVERFLOW : EINVAL;
        return NULL;
    }
    unsigned depth = 1;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i] || items[i]->depth >= 128) {
            errno = items[i] ? EOVERFLOW : EINVAL;
            return NULL;
        }
        if (items[i]->depth >= depth)
            depth = items[i]->depth + 1;
    }
    string_t *source = string_new_with(kind == TeX_OBJECT_LIST ? "\\left\\{" : "");
    if (!source)
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        if ((i && string_append_cstr(source, ", ")) ||
            string_append_cstr(source, TeX_source(items[i]))) {
            string_free(source);
            return NULL;
        }
    }
    if (kind == TeX_OBJECT_LIST && string_append_cstr(source, "\\right\\}")) {
        string_free(source);
        return NULL;
    }
    TeX_t *result = TeX_adopt(source);
    if (!result)
        return NULL;
    result->object_kind = kind;
    result->depth = depth;
    result->items = count ? calloc(count, sizeof *result->items) : NULL;
    if (count && !result->items) {
        TeX_free(result);
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        result->items[i] = TeX_copy(items[i]);
        if (!result->items[i]) {
            TeX_free(result);
            return NULL;
        }
        ++result->count;
    }
    return result;
}

/* Keep structural data and serialised notation together without exposing their representation. */
TeX_t *TeX_object_rendered(TeX_object_kind_t kind, const TeX_t *const *items, size_t count, TeX_t *rendered)
{
    TeX_t *result = rendered ? TeX_object_new(kind, items, count) : NULL;
    if (result) {
        string_free(result->source);
        result->source = rendered->source;
        result->kind = rendered->kind;
        rendered->source = NULL;
    }
    TeX_free(rendered);
    return result;
}

/* Retain an index label as data while supplying its visible variance annotation. */
TeX_t *TeX_object_index(const TeX_t *label, bool upper)
{
    const TeX_t *const *members = NULL;
    size_t count;
    if (TeX_object_kind(label) == TeX_OBJECT_UPPER || TeX_object_kind(label) == TeX_OBJECT_LOWER) {
        members = TeX_object_items(label, &count);
        label = members[0];
    }
    TeX_t *result = TeX_object_new(upper ? TeX_OBJECT_UPPER : TeX_OBJECT_LOWER, &label, 1);
    string_t *source = result ? string_sprintf(upper ? "{}^{%s}" : "{}_{%s}", TeX_source(label)) : NULL;
    if (!source) {
        TeX_free(result);
        return NULL;
    }
    string_free(result->source);
    result->source = source;
    return result;
}

/* Infer rectangular dimensions from row objects and retain rows for later formatting. */
TeX_t *TeX_object_matrix(const TeX_t *rows, TeX_matrix_style_t style, unsigned spacing)
{
    size_t row_count, columns = 0;
    const TeX_t *const *row = TeX_object_items(rows, &row_count);
    if (TeX_object_kind(rows) != TeX_OBJECT_LIST || !row_count) {
        errno = EINVAL;
        return NULL;
    }
    for (size_t r = 0; r < row_count; ++r) {
        size_t n;
        TeX_object_items(row[r], &n);
        if (TeX_object_kind(row[r]) != TeX_OBJECT_LIST || !n || (r && n != columns)) {
            errno = EINVAL;
            return NULL;
        }
        columns = n;
    }
    if (row_count > SIZE_MAX / columns || row_count * columns > SIZE_MAX / sizeof(TeX_t *)) {
        errno = EOVERFLOW;
        return NULL;
    }
    const TeX_t **cells = malloc(row_count * columns * sizeof *cells);
    if (!cells)
        return NULL;
    for (size_t r = 0; r < row_count; ++r) {
        size_t n;
        const TeX_t *const *values = TeX_object_items(row[r], &n);
        memcpy(cells + r * columns, values, columns * sizeof *cells);
    }
    TeX_t *rendered = TeX_compose_matrix(cells, row_count, columns, style, spacing);
    free(cells);
    TeX_t *result = rendered ? TeX_object_new(TeX_OBJECT_MATRIX, &rows, 1) : NULL;
    if (result) {
        string_free(result->source);
        result->source = rendered->source;
        rendered->source = NULL;
        result->kind = TeX_NODE_MATRIX;
        result->matrix_style = style;
        result->row_spacing = spacing;
    }
    TeX_free(rendered);
    return result;
}

/* Reformat a retained matrix without parsing or altering any of its cells. */
TeX_t *TeX_object_matrix_format(const TeX_t *matrix, TeX_matrix_style_t style, unsigned spacing,
                              bool change_style)
{
    if (TeX_object_kind(matrix) != TeX_OBJECT_MATRIX) {
        errno = EINVAL;
        return NULL;
    }
    return TeX_object_matrix(matrix->items[0], change_style ? style : matrix->matrix_style,
                             change_style ? matrix->row_spacing : spacing);
}

static TeX_t *TeX_invalid(void)
{
    errno = EINVAL;
    return NULL;
}

/* Copy syntax supplied by another trusted mathematical renderer. */
TeX_t *TeX_compose_raw(const char *source)
{
    return source ? TeX_adopt(string_new_with(source)) : TeX_invalid();
}

/* Borrow a string cursor and escape metacharacters using direct ASCII indexing. */
TeX_t *TeX_compose_text(const string_t *text)
{
    static const char *const escapes[128] = {
        ['#']  = "\\#",
        ['$']  = "\\$",
        ['%']  = "\\%",
        ['&']  = "\\&",
        ['\\'] = "\\textbackslash{}",
        ['^']  = "\\textasciicircum{}",
        ['_']  = "\\_",
        ['{']  = "\\{",
        ['}']  = "\\}",
        ['~']  = "\\textasciitilde{}"
    };
    if (!text)
        return TeX_invalid();
    string_t *out = string_new_with("\\text{");
    if (!out)
        return NULL;
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        goto fail;
    string_pos_t start = string_cursor_position(cursor);
    /* Visit each character once, copying ordinary text in contiguous spans. */
    while (!string_cursor_done(cursor)) {
        unsigned char ascii;
        const char *escaped = NULL;
        if (string_cursor_peek_ascii(cursor, &ascii)) {
            if (!ascii) {
                errno = EINVAL;
                goto fail;
            }
            escaped = escapes[ascii];
        }
        if (escaped && (string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor) ||
                        string_append_cstr(out, escaped)))
            goto fail;
        if (string_cursor_next(cursor))
            goto fail;
        if (escaped)
            start = string_cursor_position(cursor);
    }
    if (string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor) ||
        string_append_cstr(out, "}"))
        goto fail;
    string_cursor_free(cursor);
    return TeX_adopt(out);
fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
}

/* Construct ordinary algebraic fraction notation. */
TeX_t *TeX_compose_fraction(const TeX_t *numerator, const TeX_t *denominator)
{
    if (!numerator || !denominator)
        return TeX_invalid();
    return TeX_adopt(string_sprintf("\\frac{%s}{%s}", TeX_source(numerator), TeX_source(denominator)));
}

/* Construct square or indexed radical notation. */
TeX_t *TeX_compose_root(const TeX_t *radicand, const TeX_t *index)
{
    if (!radicand)
        return TeX_invalid();
    return TeX_adopt(index ? string_sprintf("\\sqrt[%s]{%s}", TeX_source(index), TeX_source(radicand))
                           : string_sprintf("\\sqrt{%s}", TeX_source(radicand)));
}

/* Protect the complete base before attaching scripts. */
TeX_t *TeX_compose_scripts(const TeX_t *base, const TeX_t *lower, const TeX_t *upper)
{
    if (!base)
        return TeX_invalid();
    string_t *out = string_sprintf("{%s}", TeX_source(base));
    if (!out)
        return NULL;
    if ((lower && string_append_format(out, "_{%s}", TeX_source(lower)) < 0) ||
        (upper && string_append_format(out, "^{%s}", TeX_source(upper)) < 0)) {
        string_free(out);
        return NULL;
    }
    return TeX_adopt(out);
}

/* Copy each fragment exactly once in caller-selected order. */
TeX_t *TeX_compose_sequence(const TeX_t *const *parts, size_t count)
{
    if (count && !parts)
        return TeX_invalid();
    string_t *out = string_new();
    if (!out)
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        if (!parts[i]) {
            string_free(out);
            return TeX_invalid();
        }
        if (string_append_string(out, parts[i]->source)) {
            string_free(out);
            return NULL;
        }
    }
    return TeX_adopt(out);
}

/* Join equation sides without interpreting their mathematics or introducing line breaks. */
TeX_t *TeX_compose_equation(const TeX_t *left, const TeX_t *right, TeX_equation_style_t style)
{
    if (!left || !right || (unsigned)style > TeX_EQUATION_ALIGNED)
        return TeX_invalid();
    const char *lhs = TeX_source(left), *rhs = TeX_source(right);
    if (style == TeX_EQUATION_INLINE)
        return TeX_adopt(string_sprintf("%s = %s", lhs, rhs));
    static const char prefix[] = "\\begin{aligned}[t]\n";
    static const char suffix[] = "\n\\end{aligned}";
    size_t length = strlen(rhs);
    if (length >= sizeof prefix + sizeof suffix - 2u &&
        strncmp(rhs, prefix, sizeof prefix - 1u) == 0 &&
        strcmp(rhs + length - (sizeof suffix - 1u), suffix) == 0) {
        const char *body = rhs + sizeof prefix - 1u;
        const char *end = rhs + length - (sizeof suffix - 1u);
        if (*body == '&')
            ++body;
        while (end > body && isspace((unsigned char)end[-1]))
            --end;
        if ((size_t)(end - body) > INT_MAX)
            return TeX_invalid();
        return TeX_adopt(string_sprintf("%s%s ={}& %.*s%s", prefix, lhs, (int)(end - body), body, suffix));
    }
    return TeX_adopt(string_sprintf("%s%s &= %s%s", prefix, lhs, rhs, suffix));
}

/* Enclose a mathematical body using directly indexed delimiter pairs. */
TeX_t *TeX_compose_fenced(const TeX_t *body, TeX_fence_t fence)
{
    static const char *const left[] = {"(", "[", "|", "\\lfloor", "\\lceil", "\\langle"};
    static const char *const right[] = {")", "]", "|", "\\rfloor", "\\rceil", "\\rangle"};
    if (!body || (unsigned)fence >= sizeof left / sizeof *left)
        return TeX_invalid();
    return TeX_adopt(string_sprintf("\\left%s %s \\right%s", left[fence], TeX_source(body), right[fence]));
}

/* Preserve the caller's tensor slot order, including alternating variance. */
TeX_t *TeX_compose_tensor(const TeX_t *base, const TeX_index_t *indices, size_t count)
{
    static const char *const positions[] = {"_", "^"};
    if (!base || (count && !indices))
        return TeX_invalid();
    string_t *out = string_sprintf("{%s}", TeX_source(base));
    if (!out)
        return NULL;
    /* Emit each label once, grouping adjacent equal-variance slots without reordering mixed slots. */
    for (size_t i = 0; i < count; ++i) {
        if (!indices[i].value || (unsigned)indices[i].position >= sizeof positions / sizeof *positions) {
            string_free(out);
            return TeX_invalid();
        }
        bool same_position = i && indices[i - 1].position == indices[i].position;
        int appended = same_position ? string_append_format(out, " %s", TeX_source(indices[i].value))
            : string_append_format(out, "%s%s{%s", i ? "}{}" : "", positions[indices[i].position],
                                    TeX_source(indices[i].value));
        if (appended < 0) {
            string_free(out);
            return NULL;
        }
    }
    if (count && string_append_cstr(out, "}")) {
        string_free(out);
        return NULL;
    }
    return TeX_adopt(out);
}

static string_t *TeX_bounded_body(const char *operator, const TeX_t *body, const TeX_t *lower, const TeX_t *upper)
{
    string_t *out = string_new_with(operator);
    if (!out)
        return NULL;
    if ((lower && string_append_format(out, "_{%s}", TeX_source(lower)) < 0) ||
        (upper && string_append_format(out, "^{%s}", TeX_source(upper)) < 0) ||
        string_append_format(out, " {%s}", TeX_source(body)) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

/* Compose summation notation without evaluating the summand. */
TeX_t *TeX_compose_sum(const TeX_t *body, const TeX_t *lower, const TeX_t *upper)
{
    return body ? TeX_adopt(TeX_bounded_body("\\sum", body, lower, upper)) : TeX_invalid();
}

/* Compose a bounded product using the same annotation layout as summation. */
TeX_t *TeX_compose_product(const TeX_t *body, const TeX_t *lower, const TeX_t *upper)
{
    return body ? TeX_adopt(TeX_bounded_body("\\prod", body, lower, upper)) : TeX_invalid();
}

/* Compose an integral and its differential without performing integration. */
TeX_t *TeX_compose_integral(const TeX_t *body, const TeX_t *variable, const TeX_t *lower, const TeX_t *upper)
{
    if (!body || !variable)
        return TeX_invalid();
    string_t *out = TeX_bounded_body("\\int", body, lower, upper);
    if (!out)
        return NULL;
    if (string_append_format(out, "\\,\\mathrm{d}{%s}", TeX_source(variable)) < 0) {
        string_free(out);
        return NULL;
    }
    return TeX_adopt(out);
}

/* Typeset the derivative's own fraction, leaving surrounding algebra to the caller. */
TeX_t *TeX_compose_derivative(const TeX_t *body, const TeX_t *variable, unsigned order, TeX_derivative_style_t style)
{
    static const char *const symbols[] = {"\\mathrm{d}", "\\partial"};
    if (!body || !variable || !order || (unsigned)style >= sizeof symbols / sizeof *symbols)
        return TeX_invalid();
    const char *symbol = symbols[style];
    if (order == 1)
        return TeX_adopt(string_sprintf("\\frac{%s {%s}}{%s {%s}}", symbol, TeX_source(body),
                                       symbol, TeX_source(variable)));
    return TeX_adopt(string_sprintf("\\frac{%s^{%u} {%s}}{%s {%s}^{%u}}", symbol, order,
                                   TeX_source(body), symbol, TeX_source(variable), order));
}

/* Decorate a complete operand without assigning conjugation or coordinate semantics. */
TeX_t *TeX_compose_bar(const TeX_t *body)
{
    return body ? TeX_adopt(string_sprintf("\\overline{%s}", TeX_source(body))) : TeX_invalid();
}

/* Emit row-major mathematical cells without interpreting their contents. */
TeX_t *TeX_compose_matrix(const TeX_t *const *cells, size_t rows, size_t columns,
                  TeX_matrix_style_t style, unsigned row_spacing)
{
    static const char *const environments[] = {"bmatrix", "pmatrix", "matrix"};
    if (!cells || !rows || !columns || rows > SIZE_MAX / columns ||
        (unsigned)style >= sizeof environments / sizeof *environments)
        return TeX_invalid();
    const char *environment = environments[style];
    string_t *out = string_sprintf("\\begin{%s}", environment);
    if (!out)
        return NULL;
    for (size_t row = 0; row < rows; ++row) {
        if (row && (row_spacing ? string_append_format(out, " \\\\[%upt] ", row_spacing) < 0
                                : string_append_cstr(out, " \\\\ ")))
            goto fail;
        for (size_t column = 0; column < columns; ++column) {
            const TeX_t *cell = cells[row * columns + column];
            if ((column && string_append_cstr(out, " & ")) ||
                (cell && string_append_string(out, cell->source)))
                goto fail;
        }
    }
    if (string_append_format(out, "\\end{%s}", environment) < 0)
        goto fail;
    return TeX_adopt(out);
fail:
    string_free(out);
    return NULL;
}

/* Expose immutable syntax without allocating a second copy. */
const char *TeX_source(const TeX_t *notation)
{
    return notation ? string_c_str(notation->source) : NULL;
}

/* Provide an independent string for consumers requiring ownership. */
string_t *TeX_render(const TeX_t *notation)
{
    return notation ? string_clone(notation->source) : NULL;
}

/* Release independently owned members; construction bounds recursive depth to 128. */
void TeX_free(TeX_t *notation)
{
    if (notation) {
        for (size_t i = 0; i < notation->count; ++i)
            TeX_free(notation->items[i]);
        free(notation->items);
        string_free(notation->source);
        free(notation);
    }
}

/* Compose bound notation without interpreting its binding values. */
TeX_t *TeX_compose_bound(const TeX_t *body, const TeX_t *bindings)
{
    if (!body)
        return TeX_invalid();
    return TeX_adopt(bindings && string_length(bindings->source)
        ? string_sprintf("\\left\\{ %s \\;\\middle|\\; %s \\right\\}", TeX_source(body), TeX_source(bindings))
        : string_sprintf("\\left\\{ %s \\right\\}", TeX_source(body)));
}

/* Keep scalar factors close to matrices and other compound bodies. */
TeX_t *TeX_compose_scaled(const TeX_t *factor, const TeX_t *body)
{
    if (!factor || !body)
        return TeX_invalid();
    return TeX_adopt(string_sprintf("%s\\mkern-5mu %s", TeX_source(factor), TeX_source(body)));
}

/* Stack complex components without changing their values or signs. */
TeX_t *TeX_compose_cartesian_sum(const TeX_t *real, const TeX_t *imaginary)
{
    if (!real || !imaginary)
        return TeX_invalid();
    return TeX_adopt(string_sprintf("\\begin{aligned}&%s \\\\[10pt] &{}+ i\\mkern-2mu %s\\end{aligned}",
                                    TeX_source(real), TeX_source(imaginary)));
}

/* Present a precomputed common centre and displacement as a symmetric pair. */
TeX_t *TeX_compose_symmetric_pair(const TeX_t *centre, const TeX_t *offset,
                          const TeX_t *denominator, bool group_offset)
{
    if (!centre || !offset)
        return TeX_invalid();
    string_t *body = string_sprintf(group_offset ? "%s \\pm \\left(%s\\right)" : "%s \\pm %s",
                                    TeX_source(centre), TeX_source(offset));
    if (!body || !denominator)
        return TeX_adopt(body);
    string_t *out = string_sprintf("\\frac{%S}{%s}", body, TeX_source(denominator));
    string_free(body);
    return TeX_adopt(out);
}
