/**
 * @file TeX.c
 * @brief Application-independent composition and escaping of mathematical TeX.
 *
 * Stores eagerly composed immutable strings behind the public TeX_t interface.
 * Composition copies children, avoiding shared ownership and recursive destruction.
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
};

static TeX_t *TeX_adopt(string_t *source)
{
    if (!source)
        return NULL;
    TeX_t *notation = malloc(sizeof *notation);
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
    TeX_t *copy = notation ? TeX_classify(TeX_raw(TeX_source(notation)), notation->kind) : NULL;
    if (copy) {
        copy->has_integer = notation->has_integer;
        copy->integer = notation->integer;
    }
    return copy;
}

/* Keep literal integer controls separate from rendered mathematical source. */
TeX_t *TeX_integer(long value)
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

static TeX_t *TeX_invalid(void)
{
    errno = EINVAL;
    return NULL;
}

/* Copy syntax supplied by another trusted mathematical renderer. */
TeX_t *TeX_raw(const char *source)
{
    return source ? TeX_adopt(string_new_with(source)) : TeX_invalid();
}

/* Borrow a string cursor and escape metacharacters using direct ASCII indexing. */
TeX_t *TeX_text(const string_t *text)
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
    static const char *const left[] = {"(", "[", "|", "\\lfloor", "\\lceil"};
    static const char *const right[] = {")", "]", "|", "\\rfloor", "\\rceil"};
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
    /* Each slot must be emitted; lookup of its variance is constant-time. */
    for (size_t i = 0; i < count; ++i) {
        if (!indices[i].value || (unsigned)indices[i].position >= sizeof positions / sizeof *positions) {
            string_free(out);
            return TeX_invalid();
        }
        if (string_append_format(out, "%s%s{%s}", i ? "{}" : "", positions[indices[i].position],
                                 TeX_source(indices[i].value)) < 0) {
            string_free(out);
            return NULL;
        }
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

/* Destroy a value without traversing any borrowed children. */
void TeX_free(TeX_t *notation)
{
    if (notation) {
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
