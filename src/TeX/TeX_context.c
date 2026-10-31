/**
 * @file TeX_context.c
 * @brief Render mathematical provider callbacks without inspecting native objects.
 *
 * Each rendering call owns a dictionary of visited semantic nodes. Shared children
 * are rendered once; active entries detect cycles. Provider text is copied before
 * requesting children, so providers may reuse their own temporary text buffers.
 * No expression, matrix, equation or differential-equation implementation is included.
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

struct _TeX_context_t {
    TeX_node_ref_t root;
    void (*release)(const void *);
};

typedef struct {
    TeX_t *notation;
    bool active;
    bool calculus;
    bool projection;
    bool negative_exponential;
    TeX_node_kind_t kind;
} TeX_cached_node_t;

typedef TeX_t *(*TeX_node_renderer_fn)(const TeX_node_view_t *, const TeX_t *const *);

static size_t node_hash(const void *key)
{
    const TeX_node_ref_t *ref = key;
    uintptr_t a = (uintptr_t)ref->provider, b = (uintptr_t)ref->context, c = (uintptr_t)ref->node;
    return (size_t)((a >> 3) ^ (b * (uintptr_t)2654435761u) ^ (c * (uintptr_t)2246822519u));
}

static int node_compare(const void *left, const void *right)
{
    const TeX_node_ref_t *a = left, *b = right;
    if (a->provider != b->provider)
        return (uintptr_t)a->provider < (uintptr_t)b->provider ? -1 : 1;
    if (a->context != b->context)
        return (uintptr_t)a->context < (uintptr_t)b->context ? -1 : 1;
    return a->node == b->node ? 0 : (uintptr_t)a->node < (uintptr_t)b->node ? -1 : 1;
}

static void node_destroy(void *value)
{
    TeX_cached_node_t *entry = value;
    TeX_free(entry->notation);
}

static TeX_t *invalid_node(void)
{
    errno = EINVAL;
    return NULL;
}

static TeX_t *from_owned_source(string_t *source)
{
    TeX_t *result = source ? TeX_raw(string_c_str(source)) : NULL;
    string_free(source);
    return result;
}

static TeX_t *render_number(const TeX_node_view_t *view, const TeX_t *const *children)
{
    (void)children;
    if (!view->text || view->child_count)
        return invalid_node();
    const char *text = string_c_str(view->text), *p = text;
    if (*p == '+' || *p == '-')
        ++p;
    size_t digits = 0;
    while (*p >= '0' && *p <= '9') { ++p; ++digits; }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') { ++p; ++digits; }
    }
    if (!digits)
        return invalid_node();
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-')
            ++p;
        const char *start = p;
        while (*p >= '0' && *p <= '9')
            ++p;
        if (start == p)
            return invalid_node();
    }
    return (size_t)(p - text) == string_byte_length(view->text) ? TeX_raw(text) : invalid_node();
}

static TeX_t *render_text(const TeX_node_view_t *view, const TeX_t *const *children)
{
    (void)children;
    return view->text && !view->child_count ? TeX_text(view->text) : invalid_node();
}

static TeX_t *render_identifier(const TeX_node_view_t *view, const TeX_t *const *children)
{
    (void)children;
    if (!view->text || !string_byte_length(view->text) || view->child_count)
        return invalid_node();
    const unsigned char *name = (const unsigned char *)string_c_str(view->text);
    size_t bytes = string_byte_length(view->text);
    if (bytes == 2u && name[0] >= 0xc2u && name[0] <= 0xdfu && (name[1] & 0xc0u) == 0x80u) {
        const char *symbol = TeX_semantic_greek(((name[0] & 0x1fu) << 6u) | (name[1] & 0x3fu));
        if (symbol)
            return TeX_raw(symbol);
    }
    if (!strcmp((const char *)name, "∞"))
        return TeX_raw("\\infty");
    if (!strcmp((const char *)name, "−∞"))
        return TeX_raw("-\\infty");
    if (!strcmp((const char *)name, "?"))
        return TeX_raw("?");
    TeX_t *literal = TeX_text(view->text);
    if (!literal)
        return NULL;
    const char *escaped = TeX_source(literal);
    size_t length = strlen(escaped);
    if (length < 7u || length - 7u > INT_MAX) {
        TeX_free(literal);
        return invalid_node();
    }
    string_t *escaped_name = string_new();
    if (!escaped_name || string_append_bytes(escaped_name, escaped + 6, length - 7u)) {
        string_free(escaped_name);
        TeX_free(literal);
        return NULL;
    }
    char *converted = TeX_from_unicode_math(string_c_str(escaped_name));
    string_free(escaped_name);
    string_t *source = converted ? string_sprintf("\\mathit{%s}", converted) : NULL;
    free(converted);
    if (string_byte_length(view->text) == 1u) {
        unsigned char c = (unsigned char)*string_c_str(view->text);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            string_free(source);
            source = string_clone(view->text);
        }
    }
    TeX_free(literal);
    return from_owned_source(source);
}

/* Render an identifier independently of a native mathematical representation. */
TeX_t *TeX_identifier(const string_t *name)
{
    const TeX_node_view_t view = {.kind = TeX_NODE_IDENTIFIER, .text = name};
    return render_identifier(&view, NULL);
}

static TeX_t *render_binary(const TeX_node_view_t *view, const TeX_t *const *children)
{
    static const char *const separators[TeX_NODE_COUNT] = {
        [TeX_NODE_SUM]        = " + ",
        [TeX_NODE_DIFFERENCE] = " - ",
        [TeX_NODE_PRODUCT]    = "\\mkern-2mu "
    };
    if (view->child_count != 2u)
        return invalid_node();
    const char *separator = separators[view->kind];
    const char *left = TeX_source(children[0]), *right = TeX_source(children[1]);
    if (view->kind == TeX_NODE_SUM && right[0] == '-') {
        separator = " - ";
        ++right;
    }
    if (view->kind == TeX_NODE_DIFFERENCE && right[0] == '-') {
        separator = " + ";
        ++right;
    }
    size_t length = strlen(left);
    if (view->kind == TeX_NODE_PRODUCT && length && left[length - 1u] >= '0' && left[length - 1u] <= '9' &&
        ((right[0] >= '0' && right[0] <= '9') || strncmp(right, "\\left(-", 7u) == 0))
        separator = " \\times ";
    return from_owned_source(string_sprintf("%s%s%s", left, separator, right));
}

static TeX_t *render_fraction(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? TeX_compose_fraction(children[0], children[1]) : invalid_node();
}

static TeX_t *render_power(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? from_owned_source(string_sprintf("%s^{%s}",
        TeX_source(children[0]), TeX_source(children[1]))) : invalid_node();
}

static TeX_t *render_root(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u || view->child_count == 2u
        ? TeX_compose_root(children[0], view->child_count == 2u ? children[1] : NULL) : invalid_node();
}

static TeX_t *render_negative(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u
        ? from_owned_source(string_sprintf("-%s", TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_equation(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? TeX_compose_equation(children[0], children[1], TeX_EQUATION_INLINE) : invalid_node();
}

static TeX_t *render_assignment(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? from_owned_source(string_sprintf(
        view->kind == TeX_NODE_BINDING_ASSIGNMENT ? "%s = %s" : "%s=%s",
        TeX_source(children[0]), TeX_source(children[1]))) : invalid_node();
}

static TeX_t *render_matrix(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (!view->rows || !view->columns || view->rows > SIZE_MAX / view->columns ||
        view->rows * view->columns != view->child_count)
        return invalid_node();
    return TeX_compose_matrix(children, view->rows, view->columns, TeX_MATRIX_BRACKETS, 10u);
}

static TeX_t *render_function_name(const TeX_node_view_t *view, const TeX_t *const *children)
{
    (void)children;
    if (view->child_count)
        return invalid_node();
    TeX_node_view_t name = {.kind = TeX_NODE_IDENTIFIER, .text = view->text};
    const char *standard = TeX_semantic_function(view->text);
    TeX_t *identifier = standard ? TeX_raw(standard) : render_identifier(&name, NULL);
    if (identifier && !strncmp(TeX_source(identifier), "\\mathit{", 8u)) {
        string_t *operator = string_sprintf("\\operatorname%s", TeX_source(identifier) + 7u);
        TeX_free(identifier);
        identifier = from_owned_source(operator);
    }
    return identifier;
}

static TeX_t *apply_function(const TeX_t *function, const TeX_t *const *arguments, size_t count, bool scalable)
{
    string_t *source = function ? string_sprintf("%s%s", TeX_source(function), scalable ? "\\left(" : "(") : NULL;
    if (!source)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if ((i && string_append_cstr(source, ", ")) || string_append_cstr(source, TeX_source(arguments[i]))) {
            string_free(source);
            return NULL;
        }
    if (string_append_cstr(source, scalable ? "\\right)" : ")")) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

/* Apply an explicitly constructed function, including one with mathematical indices. */
TeX_t *TeX_applied(const TeX_t *function, const TeX_t *const *arguments, size_t count)
{
    return TeX_classify(apply_function(function, arguments, count, true), TeX_NODE_CALL);
}

static TeX_t *render_call(const TeX_node_view_t *view, const TeX_t *const *children)
{
    TeX_node_view_t name = {.kind = TeX_NODE_FUNCTION_NAME, .text = view->text};
    TeX_t *function = render_function_name(&name, NULL);
    if (function && view->text && view->child_count == 2u && !strcmp(string_c_str(view->text), "cl")) {
        TeX_t *result = from_owned_source(string_sprintf("%s_{%s}(%s)", TeX_source(function),
                                                        TeX_source(children[0]), TeX_source(children[1])));
        TeX_free(function);
        return result;
    }
    TeX_t *result = function ? apply_function(function, children, view->child_count,
                                             TeX_semantic_function(view->text) == NULL) : NULL;
    TeX_free(function);
    return result;
}

/* Share established function-name spelling with the explicit RPN construction interface. */
TeX_t *TeX_named_call(const string_t *name, const TeX_t *const *arguments, size_t count)
{
    const char *alias = TeX_symbol_name(name);
    if (alias && !TeX_semantic_function(name)) {
        TeX_t *function = TeX_raw(alias);
        TeX_t *result = function ? TeX_applied(function, arguments, count) : NULL;
        TeX_free(function);
        return result;
    }
    const TeX_node_view_t view = {.kind = TeX_NODE_CALL, .text = name, .child_count = count};
    return TeX_classify(render_call(&view, arguments), TeX_NODE_CALL);
}

static TeX_t *render_apply(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count ? apply_function(children[0], children + 1u, view->child_count - 1u, true) : invalid_node();
}

static TeX_t *render_ordered_derivative(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count < 3u)
        return invalid_node();
    const char *order = TeX_source(children[1]);
    string_t *source = order[0] >= '1' && order[0] <= '3' && !order[1]
        ? string_sprintf("%s%.*s", TeX_source(children[0]), order[0] - '0', "'''")
        : string_sprintf("%s^{(%s)}", TeX_source(children[0]), order);
    TeX_t *function = from_owned_source(source);
    TeX_t *result = function ? apply_function(function, children + 2u, view->child_count - 2u, true) : NULL;
    TeX_free(function);
    return result;
}

static TeX_t *render_function_order(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 3u ? from_owned_source(string_sprintf("%s^{(%s)}(%s)",
        TeX_source(children[0]), TeX_source(children[1]), TeX_source(children[2]))) : invalid_node();
}

static TeX_t *render_derivative(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count < 2u || !view->order ||
        (view->child_count > 2u && view->order != view->child_count - 1u))
        return invalid_node();
    bool variable = view->kind == TeX_NODE_VARIABLE_DERIVATIVE;
    const char *symbol = view->partial ? "\\partial" : variable ? "d" : "\\mathrm{d}";
    string_t *source = string_sprintf("\\frac{%s", symbol);
    if (!source)
        return NULL;
    bool valid = view->order == 1u || string_append_format(source, "^{%u}", view->order) >= 0;
    if (variable)
        valid = valid && string_append_format(source, " %s", TeX_source(children[0])) >= 0;
    valid = valid && string_append_cstr(source, "}{") == 0;
    for (size_t i = view->child_count; valid && i > 1u;) {
        size_t repeated = 1u;
        while (i > repeated + 1u && !strcmp(TeX_source(children[i - 1u]),
                                             TeX_source(children[i - repeated - 1u])))
            ++repeated;
        if (i < view->child_count)
            valid = string_append_cstr(source, "\\,") == 0;
        valid = valid && string_append_format(source, "%s %s", symbol, TeX_source(children[i - 1u])) >= 0;
        size_t multiplicity = view->child_count == 2u ? view->order : repeated;
        if (valid && multiplicity > 1u)
            valid = string_append_format(source, "^{%zu}", multiplicity) >= 0;
        i -= repeated;
    }
    valid = valid && (variable ? string_append_char(source, '}') == 0
        : string_append_format(source, "}\\left[%s\\right]", TeX_source(children[0])) >= 0);
    if (!valid) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

/* Share ordered coordinate-derivative notation with the public construction stack. */
TeX_t *TeX_compose_partial(const TeX_t *const *operands, size_t count)
{
    if (!operands || !count || count > UINT_MAX || count == SIZE_MAX)
        return invalid_node();
    const TeX_node_view_t view = {
        .kind = TeX_NODE_DERIVATIVE, .child_count = count + 1, .order = (unsigned)count, .partial = true
    };
    return render_derivative(&view, operands);
}

static TeX_t *render_subscript(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? from_owned_source(string_sprintf("%s_{%s}",
        TeX_source(children[0]), TeX_source(children[1]))) : invalid_node();
}

static TeX_t *render_hat(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("\\widehat{%s}",
        TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_prime(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 1u || !view->order)
        return invalid_node();
    return from_owned_source(view->order <= 3u
        ? string_sprintf("%s%.*s", TeX_source(children[0]), (int)view->order, "'''")
        : string_sprintf("%s^{(%u)}", TeX_source(children[0]), view->order));
}

/* Share derivative-prime spelling with the explicit operand stack. */
TeX_t *TeX_compose_prime(const TeX_t *body, unsigned order)
{
    if (!body || !order)
        return invalid_node();
    const TeX_node_view_t view = {.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order};
    return render_prime(&view, &body);
}

static TeX_t *render_bold(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("\\boldsymbol{%s}",
        TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_calligraphic(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("\\mathcal{%s}",
        TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_integral_condition(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 3u ? from_owned_source(string_sprintf("\\int_{%s}%s\\,\\mathrm{d}{%s}",
        TeX_source(children[1]), TeX_source(children[0]), TeX_source(children[2]))) : invalid_node();
}

static TeX_t *render_factorial(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("%s!", TeX_source(children[0])))
                                  : invalid_node();
}

static TeX_t *render_real_part(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("\\mathfrak{Re}(%s)",
        TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_conjugate(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("\\overline{%s}",
        TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_projection(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 1u)
        return invalid_node();
    return from_owned_source(string_sprintf("\\mathfrak{%s}\\mathopen{}\\left\\{%s\\right\\}",
        view->kind == TeX_NODE_REAL_PROJECTION ? "Re" : "Im", TeX_source(children[0])));
}

static TeX_t *render_left_subscript(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? from_owned_source(string_sprintf("{}_{%s}%s",
        TeX_source(children[1]), TeX_source(children[0]))) : invalid_node();
}

static TeX_t *render_parameter_groups(const TeX_node_view_t *view, const TeX_t *const *children)
{
    string_t *source = string_new();
    if (!source)
        return NULL;
    for (size_t i = 0u; i < view->child_count; ++i)
        if ((i && string_append_cstr(source, "; ")) ||
            string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    return from_owned_source(source);
}

static TeX_t *render_evaluation(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count < 2u)
        return invalid_node();
    string_t *source = string_sprintf("\\left. %s\\right|_{", TeX_source(children[0]));
    if (!source)
        return NULL;
    for (size_t i = 1u; i < view->child_count; ++i) {
        if ((i > 1u && string_append_cstr(source, ",\\,")) ||
            string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    }
    if (string_append_cstr(source, "}")) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_lines(const TeX_node_view_t *view, const TeX_t *const *children)
{
    string_t *source = string_new_with("\\begin{aligned}");
    if (!source)
        return NULL;
    for (size_t i = 0u; i < view->child_count; ++i)
        if ((i && string_append_cstr(source, "\\\\")) || string_append_cstr(source, "&") ||
            string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    if (string_append_cstr(source, "\\end{aligned}")) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_conditions(const TeX_node_view_t *view, const TeX_t *const *children)
{
    string_t *source = string_new();
    if (!source)
        return NULL;
    for (size_t i = 0u; i < view->child_count; ++i)
        if ((i && string_append_cstr(source, ",\\qquad ")) ||
            string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    return from_owned_source(source);
}

static TeX_t *render_integral(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count < 2u || view->child_count > 4u)
        return invalid_node();
    string_t *source = string_new_with("\\int");
    if (!source)
        return NULL;
    bool valid = (view->child_count != 4u ||
                  string_append_format(source, "_{%s}", TeX_source(children[2])) >= 0) &&
                 (view->child_count == 2u ||
                  string_append_format(source, "^{%s}", TeX_source(children[view->child_count - 1u])) >= 0) &&
                 string_append_format(source, " %s\\, d%s", TeX_source(children[0]), TeX_source(children[1])) >= 0;
    if (!valid) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_sequence(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return TeX_compose_sequence(children, view->child_count);
}

static TeX_t *render_transform(const TeX_node_view_t *view, const TeX_t *const *children)
{
    static const char *const symbols[] = {
        [TeX_TRANSFORM_LAPLACE]         = "\\mathcal{L}",
        [TeX_TRANSFORM_INVERSE_LAPLACE] = "\\mathcal{L}^{-1}",
        [TeX_TRANSFORM_FOURIER]         = "\\mathcal{F}",
        [TeX_TRANSFORM_INVERSE_FOURIER] = "\\mathcal{F}^{-1}"
    };
    if (view->child_count != 3u || (unsigned)view->transform >= sizeof symbols / sizeof *symbols)
        return invalid_node();
    return from_owned_source(string_sprintf("%s_{%s\\to %s}\\{%s\\}", symbols[view->transform],
        TeX_source(children[1]), TeX_source(children[2]), TeX_source(children[0])));
}

/* Share transform presentation with callers that already own their mathematical operands. */
TeX_t *TeX_compose_transform(const TeX_t *const *operands, TeX_transform_kind_t kind)
{
    if (!operands || !operands[0] || !operands[1] || !operands[2])
        return invalid_node();
    const TeX_node_view_t view = {.kind = TeX_NODE_TRANSFORM, .child_count = 3, .transform = kind};
    return render_transform(&view, operands);
}

static TeX_t *render_relation(const TeX_node_view_t *view, const TeX_t *const *children)
{
    static const char *const operators[] = {
        [TeX_RELATION_EQUAL]         = " = ",
        [TeX_RELATION_NOT_EQUAL]     = " \\ne ",
        [TeX_RELATION_LESS]          = " < ",
        [TeX_RELATION_LESS_EQUAL]    = " \\le ",
        [TeX_RELATION_GREATER]       = " > ",
        [TeX_RELATION_GREATER_EQUAL] = " \\ge ",
        [TeX_RELATION_MEMBER]        = "\\in ",
        [TeX_RELATION_DEFINITION]    = ":=",
        [TeX_RELATION_IMPLIES]       = " \\Longrightarrow ",
        [TeX_RELATION_APPROXIMATE]   = " \\approx ",
        [TeX_RELATION_CONGRUENT]     = " \\cong "
    };
    if (view->child_count != 2u || (unsigned)view->relation >= sizeof operators / sizeof *operators)
        return invalid_node();
    return from_owned_source(string_sprintf("%s%s%s", TeX_source(children[0]), operators[view->relation],
        TeX_source(children[1])));
}

/* Share relation symbols with explicit construction without exposing semantic contexts. */
TeX_t *TeX_compose_relation(const TeX_t *left, const TeX_t *right, TeX_relation_t relation)
{
    if (!left || !right)
        return invalid_node();
    const TeX_t *const children[] = {left, right};
    const TeX_node_view_t view = {.kind = TeX_NODE_RELATION, .child_count = 2, .relation = relation};
    return render_relation(&view, children);
}

static TeX_t *render_domain_relation(const TeX_node_view_t *view, const TeX_t *const *children)
{
    static const char *const operators[] = {
        [TeX_RELATION_NOT_EQUAL] = "\\ne ",
        [TeX_RELATION_GREATER]   = ">",
        [TeX_RELATION_MEMBER]    = "\\in"
    };
    if (view->child_count != 2u || (unsigned)view->relation >= sizeof operators / sizeof *operators ||
        !operators[view->relation])
        return invalid_node();
    return from_owned_source(string_sprintf("%s%s%s", TeX_source(children[0]), operators[view->relation],
                                           TeX_source(children[1])));
}

static TeX_t *render_domain(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count < 2u)
        return invalid_node();
    string_t *source = string_sprintf("%s\\quad (", TeX_source(children[0]));
    if (!source)
        return NULL;
    for (size_t i = 1u; i < view->child_count; ++i)
        if (((view->domain_qualification && i + 1u == view->child_count)
                 ? string_append_cstr(source, ";\\;") : i > 1u ? string_append_cstr(source, ",\\;") : 0) ||
            string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    if (string_append_char(source, ')')) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_distribution_condition(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 1u || !view->text)
        return invalid_node();
    TeX_t *caption = TeX_text(view->text);
    TeX_t *result = caption ? from_owned_source(string_sprintf("%s:\\;%s", TeX_source(children[0]),
                                                               TeX_source(caption))) : NULL;
    TeX_free(caption);
    return result;
}

static TeX_t *render_distribution_qualification(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if ((view->child_count != 1u && (view->kind != TeX_NODE_DISTRIBUTION_QUALIFICATION || view->child_count != 2u)) ||
        (view->kind != TeX_NODE_FINITE_PART && !view->text))
        return invalid_node();
    string_t *operand = string_new_with(TeX_source(children[view->child_count - 1u]));
    string_t *qualification = operand ? TeX_qualification(operand, view->text ? string_c_str(view->text) : NULL,
        view->kind == TeX_NODE_FINITE_PART ? TeX_QUALIFICATION_FINITE_PART :
        view->kind == TeX_NODE_DISTRIBUTION_INLINE ? TeX_QUALIFICATION_INLINE : TeX_QUALIFICATION_TRAILING) : NULL;
    if (qualification && view->kind == TeX_NODE_DISTRIBUTION_QUALIFICATION) {
        string_t *combined = string_sprintf("%s%S", TeX_source(children[0]), qualification);
        string_free(qualification);
        qualification = combined;
    }
    string_free(operand);
    return qualification ? from_owned_source(qualification) : NULL;
}

static TeX_t *render_statements(const TeX_node_view_t *view, const TeX_t *const *children)
{
    string_t *source = string_new();
    if (!source)
        return NULL;
    for (size_t i = 0; i < view->child_count; ++i)
        if ((i && string_append_cstr(source, "\\quad ")) || string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    return from_owned_source(source);
}

static TeX_t *render_cases(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (!view->child_count || view->child_count % 2u)
        return invalid_node();
    string_t *source = string_new_with("\\begin{cases}");
    if (!source)
        return NULL;
    for (size_t i = 0; i < view->child_count; i += 2u)
        if ((i && string_append_cstr(source, " \\\\ ")) ||
            string_append_format(source, "%s, & %s", TeX_source(children[i]), TeX_source(children[i + 1u])) < 0) {
            string_free(source);
            return NULL;
        }
    if (string_append_cstr(source, "\\end{cases}")) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_big_operator(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 4u)
        return invalid_node();
    return from_owned_source(string_sprintf("%s_{%s=%s}^{%s}%s",
        view->kind == TeX_NODE_SUMMATION ? "\\sum" : "\\prod", TeX_source(children[1]),
        TeX_source(children[2]), TeX_source(children[3]), TeX_source(children[0])));
}

static TeX_t *render_conditional_product(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 2u && view->child_count != 3u)
        return invalid_node();
    string_t *source = string_sprintf("\\prod_{%s}", TeX_source(children[1]));
    if (!source || (view->child_count == 3u &&
        string_append_format(source, "^{%s}", TeX_source(children[2])) < 0) ||
        string_append_cstr(source, TeX_source(children[0]))) {
        string_free(source);
        return NULL;
    }
    return from_owned_source(source);
}

static TeX_t *render_number_set(const TeX_node_view_t *view, const TeX_t *const *children)
{
    (void)children;
    static const char *const sets[] = {
        [TeX_SET_REALS]               = "\\mathbb{R}",
        [TeX_SET_COMPLEX]             = "\\mathbb{C}",
        [TeX_SET_INTEGERS]            = "\\mathbb{Z}",
        [TeX_SET_NONNEGATIVE_INTEGERS] = "\\mathbb{Z}_{\\ge0}",
        [TeX_SET_NATURALS]            = "\\mathbb{N}",
        [TeX_SET_PRIMES]              = "\\mathbb{P}"
    };
    return !view->child_count && (unsigned)view->number_set < sizeof sets / sizeof *sets
        ? TeX_raw(sets[view->number_set]) : invalid_node();
}

static TeX_t *render_list(const TeX_node_view_t *view, const TeX_t *const *children)
{
    string_t *source = string_new();
    if (!source)
        return NULL;
    for (size_t i = 0; i < view->child_count; ++i)
        if ((i && string_append_cstr(source, ",\\;")) || string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    return from_owned_source(source);
}

static TeX_t *render_fenced(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? TeX_compose_fenced(children[0], view->fence) : invalid_node();
}

static TeX_t *render_bound(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u || view->child_count == 2u
        ? TeX_compose_bound(children[0], view->child_count == 2u ? children[1] : NULL) : invalid_node();
}

static TeX_t *render_bindings(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->rows > view->child_count)
        return invalid_node();
    string_t *source = string_new();
    if (!source)
        return NULL;
    for (size_t i = 0u; i < view->child_count; ++i) {
        const char *separator = i == view->rows ? "; " : i ? ", " : "";
        if (string_append_cstr(source, separator) || string_append_cstr(source, TeX_source(children[i]))) {
            string_free(source);
            return NULL;
        }
    }
    return from_owned_source(source);
}

static TeX_t *render_scaled(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? TeX_compose_scaled(children[0], children[1]) : invalid_node();
}

static TeX_t *render_cartesian(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? TeX_compose_cartesian_sum(children[0], children[1]) : invalid_node();
}

static TeX_t *render_complex_literal(const TeX_node_view_t *view, const TeX_t *const *children)
{
    if (view->child_count != 2u)
        return invalid_node();
    const char *imaginary = TeX_source(children[1]);
    return from_owned_source(string_sprintf("%s %s %si", TeX_source(children[0]),
        imaginary[0] == '-' ? "-" : "+", imaginary[0] == '-' ? imaginary + 1 : imaginary));
}

static TeX_t *render_imaginary_displacement(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("i\\,%s", TeX_source(children[0])))
                                   : invalid_node();
}

static TeX_t *render_antiderivative_definition(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 2u ? from_owned_source(string_sprintf(
        "\\qquad F'(%s)=%s\\quad\\text{(F is the chosen antiderivative)}",
        TeX_source(children[0]), TeX_source(children[1]))) : invalid_node();
}

static TeX_t *render_differential(const TeX_node_view_t *view, const TeX_t *const *children)
{
    return view->child_count == 1u ? from_owned_source(string_sprintf("%s{%s}",
        view->partial ? "\\partial " : "\\mathrm{d}", TeX_source(children[0]))) : invalid_node();
}

static const TeX_node_renderer_fn renderers[TeX_NODE_COUNT] = {
    [TeX_NODE_NUMBER]     = render_number,
    [TeX_NODE_IDENTIFIER] = render_identifier,
    [TeX_NODE_TEXT]       = render_text,
    [TeX_NODE_SUM]        = render_binary,
    [TeX_NODE_DIFFERENCE] = render_binary,
    [TeX_NODE_PRODUCT]    = render_binary,
    [TeX_NODE_FRACTION]   = render_fraction,
    [TeX_NODE_POWER]      = render_power,
    [TeX_NODE_ROOT]       = render_root,
    [TeX_NODE_NEGATIVE]   = render_negative,
    [TeX_NODE_EQUATION]   = render_equation,
    [TeX_NODE_MATRIX]     = render_matrix,
    [TeX_NODE_CALL]       = render_call,
    [TeX_NODE_DERIVATIVE] = render_derivative,
    [TeX_NODE_INTEGRAL]   = render_integral,
    [TeX_NODE_SEQUENCE]   = render_sequence,
    [TeX_NODE_SUBSCRIPT]  = render_subscript,
    [TeX_NODE_EVALUATION] = render_evaluation,
    [TeX_NODE_LINES]      = render_lines,
    [TeX_NODE_TRANSFORM]  = render_transform,
    [TeX_NODE_RELATION]   = render_relation,
    [TeX_NODE_DOMAIN_RELATION] = render_domain_relation,
    [TeX_NODE_DOMAIN]     = render_domain,
    [TeX_NODE_DISTRIBUTION_QUALIFICATION] = render_distribution_qualification,
    [TeX_NODE_DISTRIBUTION_INLINE] = render_distribution_qualification,
    [TeX_NODE_DISTRIBUTION_CONDITION] = render_distribution_condition,
    [TeX_NODE_FINITE_PART] = render_distribution_qualification,
    [TeX_NODE_STATEMENTS] = render_statements,
    [TeX_NODE_CASES]      = render_cases,
    [TeX_NODE_SUMMATION]  = render_big_operator,
    [TeX_NODE_INDEXED_PRODUCT] = render_big_operator,
    [TeX_NODE_CONDITIONAL_PRODUCT] = render_conditional_product,
    [TeX_NODE_NUMBER_SET] = render_number_set,
    [TeX_NODE_LIST]       = render_list,
    [TeX_NODE_FENCED]     = render_fenced,
    [TeX_NODE_BOUND]      = render_bound,
    [TeX_NODE_BINDINGS]   = render_bindings,
    [TeX_NODE_SCALED]     = render_scaled,
    [TeX_NODE_CARTESIAN]  = render_cartesian,
    [TeX_NODE_COMPLEX_LITERAL] = render_complex_literal,
    [TeX_NODE_IMAGINARY_DISPLACEMENT] = render_imaginary_displacement,
    [TeX_NODE_ANTIDERIVATIVE_DEFINITION] = render_antiderivative_definition,
    [TeX_NODE_DIFFERENTIAL] = render_differential,
    [TeX_NODE_FUNCTION_NAME] = render_function_name,
    [TeX_NODE_APPLY]      = render_apply,
    [TeX_NODE_ORDERED_DERIVATIVE] = render_ordered_derivative,
    [TeX_NODE_VARIABLE_DERIVATIVE] = render_derivative,
    [TeX_NODE_CONDITIONS] = render_conditions,
    [TeX_NODE_PARAMETER_GROUPS] = render_parameter_groups,
    [TeX_NODE_LEFT_SUBSCRIPT] = render_left_subscript,
    [TeX_NODE_ASSIGNMENT] = render_assignment,
    [TeX_NODE_BINDING_ASSIGNMENT] = render_assignment,
    [TeX_NODE_HAT]        = render_hat,
    [TeX_NODE_PRIME]      = render_prime,
    [TeX_NODE_BOLD]       = render_bold,
    [TeX_NODE_CALLIGRAPHIC] = render_calligraphic,
    [TeX_NODE_INTEGRAL_CONDITION] = render_integral_condition,
    [TeX_NODE_FACTORIAL] = render_factorial,
    [TeX_NODE_REAL_PART] = render_real_part,
    [TeX_NODE_CONJUGATE] = render_conjugate,
    [TeX_NODE_REAL_PROJECTION] = render_projection,
    [TeX_NODE_IMAG_PROJECTION] = render_projection,
    [TeX_NODE_FUNCTION_ORDER] = render_function_order
};

/* Share structural delimiter rules with the explicit construction interface. */
bool TeX_needs_fence(TeX_node_kind_t parent, TeX_node_kind_t child, size_t index)
{
    bool statement = child == TeX_NODE_EQUATION || child == TeX_NODE_RELATION;
    bool additive = child == TeX_NODE_SUM || child == TeX_NODE_DIFFERENCE || statement ||
                    child == TeX_NODE_COMPLEX_LITERAL;
    bool aggregate = child == TeX_NODE_SUMMATION || child == TeX_NODE_INDEXED_PRODUCT ||
                     child == TeX_NODE_CONDITIONAL_PRODUCT;
    return (statement && (parent == TeX_NODE_SUM || parent == TeX_NODE_DIFFERENCE)) ||
           (parent == TeX_NODE_PRODUCT && (additive || aggregate)) ||
           ((parent == TeX_NODE_SUMMATION || parent == TeX_NODE_INDEXED_PRODUCT ||
             parent == TeX_NODE_CONDITIONAL_PRODUCT) && index == 0u && additive) ||
           (parent == TeX_NODE_DIFFERENCE && index == 1u && additive) ||
           (parent == TeX_NODE_NEGATIVE && additive) ||
           (parent == TeX_NODE_PRIME && child == TeX_NODE_PRIME) ||
           ((parent == TeX_NODE_FACTORIAL || parent == TeX_NODE_PRIME) &&
            (additive || child == TeX_NODE_PRODUCT || child == TeX_NODE_NEGATIVE)) ||
           (parent == TeX_NODE_POWER && index == 0u &&
            (additive || child == TeX_NODE_PRODUCT || child == TeX_NODE_FRACTION ||
             child == TeX_NODE_NEGATIVE || child == TeX_NODE_POWER ||
             child == TeX_NODE_FACTORIAL || child == TeX_NODE_INTEGRAL ||
             child == TeX_NODE_DERIVATIVE || child == TeX_NODE_VARIABLE_DERIVATIVE ||
             child == TeX_NODE_TRANSFORM)) ||
           (parent == TeX_NODE_SUBSCRIPT && index == 0u &&
            (additive || child == TeX_NODE_PRODUCT || child == TeX_NODE_NEGATIVE || child == TeX_NODE_SUBSCRIPT));
}

static TeX_t *render_calculus_quotient(const TeX_t *const *children)
{
    TeX_t *denominator = TeX_compose_fenced(children[1], TeX_FENCE_PARENTHESES);
    TeX_t *exponent = TeX_raw("-1");
    TeX_t *reciprocal = denominator && exponent ? TeX_compose_scripts(denominator, NULL, exponent) : NULL;
    TeX_t *numerator = TeX_compose_fenced(children[0], TeX_FENCE_PARENTHESES);
    TeX_t *result = reciprocal && numerator ? from_owned_source(string_sprintf("%s\\,%s",
        TeX_source(numerator), TeX_source(reciprocal))) : NULL;
    TeX_free(numerator);
    TeX_free(reciprocal);
    TeX_free(exponent);
    TeX_free(denominator);
    return result;
}


static const TeX_t *render_node(dictionary_t *nodes, TeX_node_ref_t ref, size_t depth)
{
    if (!ref.node || !ref.provider || !ref.provider->describe || depth >= 256u) {
        invalid_node();
        return NULL;
    }
    TeX_cached_node_t cached;
    if (dictionary_get(nodes, &ref, &cached)) {
        if (cached.active) {
            errno = ELOOP;
            return NULL;
        }
        return cached.notation;
    }
    cached = (TeX_cached_node_t){.active = true};
    if (!dictionary_set(nodes, &ref, &cached))
        return NULL;
    TeX_node_view_t view = {0};
    if (!ref.provider->describe(ref.context, ref.node, &view) || (unsigned)view.kind >= TeX_NODE_COUNT ||
        view.child_count > 65536u || (view.child_count && !ref.provider->child)) {
        invalid_node();
        return NULL;
    }
    string_t *text = view.text ? string_clone(view.text) : NULL;
    if (view.text && !text)
        return NULL;
    view.text = text;
    const TeX_t **children = view.child_count ? calloc(view.child_count, sizeof *children) : NULL;
    TeX_t **fenced = view.child_count ? calloc(view.child_count, sizeof *fenced) : NULL;
    if (view.child_count && (!children || !fenced)) {
        free(children);
        free(fenced);
        string_free(text);
        return NULL;
    }
    bool valid = true;
    TeX_node_kind_t first_kind = TeX_NODE_COUNT, second_kind = TeX_NODE_COUNT;
    bool projection = view.kind == TeX_NODE_REAL_PROJECTION || view.kind == TeX_NODE_IMAG_PROJECTION;
    bool projection_factor = false;
    bool first_negative_exponential = false;
    bool calculus = view.kind == TeX_NODE_DERIVATIVE || view.kind == TeX_NODE_VARIABLE_DERIVATIVE ||
                    view.kind == TeX_NODE_INTEGRAL ||
                    view.kind == TeX_NODE_TRANSFORM;
    for (size_t i = 0; i < view.child_count && valid; ++i) {
        TeX_node_ref_t child = {0};
        valid = ref.provider->child(ref.context, ref.node, i, &child) &&
                (children[i] = render_node(nodes, child, depth + 1u));
        TeX_cached_node_t entry;
        if (valid && dictionary_get(nodes, &child, &entry)) {
            if (i == 0u) {
                first_kind = entry.kind;
                first_negative_exponential = entry.negative_exponential;
            } else if (i == 1u)
                second_kind = entry.kind;
            calculus = calculus || entry.calculus;
            if (view.kind == TeX_NODE_PRODUCT)
                projection_factor = projection_factor || entry.projection;
            if (i == 0u && (view.kind == TeX_NODE_NEGATIVE || view.kind == TeX_NODE_POWER))
                projection = projection || entry.projection;
            if (TeX_needs_fence(view.kind, entry.kind, i)) {
                fenced[i] = from_owned_source(string_sprintf("\\left(%s\\right)", TeX_source(children[i])));
                children[i] = fenced[i];
                valid = children[i] != NULL;
            }
        }
    }
    TeX_t *result = valid ? (view.kind == TeX_NODE_FRACTION && calculus && view.child_count == 2u
        ? render_calculus_quotient(children) : renderers[view.kind](&view, children)) : NULL;
    if (result && view.kind == TeX_NODE_PRODUCT && calculus && view.child_count == 2u &&
        (first_kind == TeX_NODE_FRACTION || second_kind == TeX_NODE_FRACTION || first_negative_exponential ||
         (first_kind == TeX_NODE_VARIABLE_DERIVATIVE && second_kind == TeX_NODE_VARIABLE_DERIVATIVE))) {
        TeX_free(result);
        result = from_owned_source(string_sprintf("%s\\,%s", TeX_source(children[0]), TeX_source(children[1])));
    }
    if (result && view.kind == TeX_NODE_PRODUCT && projection_factor && view.child_count == 2u) {
        TeX_free(result);
        result = from_owned_source(string_sprintf("%s\\,%s", TeX_source(children[0]), TeX_source(children[1])));
    }
    bool negative_exponential = result && view.kind == TeX_NODE_POWER && view.child_count == 2u &&
                                first_kind == TeX_NODE_IDENTIFIER && second_kind == TeX_NODE_NEGATIVE &&
                                !strcmp(TeX_source(children[0]), "e");
    for (size_t i = 0; i < view.child_count; ++i)
        TeX_free(fenced[i]);
    free(fenced);
    free(children);
    string_free(text);
    if (!result)
        return NULL;
    TeX_classify(result, view.kind);
    cached = (TeX_cached_node_t){.notation = result, .kind = view.kind, .calculus = calculus,
                                 .projection = projection, .negative_exponential = negative_exponential};
    if (!dictionary_set(nodes, &ref, &cached)) {
        TeX_free(result);
        return NULL;
    }
    return result;
}

/* Copy the root reference without acquiring ownership of native objects. */
TeX_context_t *TeX_context_new(TeX_node_ref_t root)
{
    if (!root.node || !root.provider || !root.provider->describe) {
        errno = EINVAL;
        return NULL;
    }
    TeX_context_t *context = malloc(sizeof *context);
    if (context) {
        context->root = root;
        context->release = NULL;
    }
    return context;
}

/* Keep all rendering state local, permitting independent and nested render calls. */
TeX_t *TeX_from_context(const TeX_context_t *context)
{
    if (!context)
        return invalid_node();
    dictionary_t *nodes = dictionary_create(sizeof(TeX_node_ref_t), sizeof(TeX_cached_node_t), node_hash,
        node_compare, NULL, NULL, NULL, NULL, node_destroy);
    if (!nodes)
        return NULL;
    const TeX_t *root = render_node(nodes, context->root, 0u);
    TeX_t *result = TeX_copy(root);
    dictionary_destroy(nodes);
    return result;
}

/* Release only the context wrapper, never its borrowed graph. */
void TeX_context_free(TeX_context_t *context)
{
    if (context && context->release)
        context->release(context->root.context);
    free(context);
}

/* Transfer native provider state only after the context allocation succeeds. */
TeX_context_t *TeX_context_adopt(TeX_node_ref_t root, void (*release)(const void *))
{
    TeX_context_t *context = TeX_context_new(root);
    if (context)
        context->release = release;
    return context;
}

/* Expose a borrowed semantic root for composition by another native provider. */
TeX_node_ref_t TeX_context_root(const TeX_context_t *context)
{
    return context ? context->root : (TeX_node_ref_t){0};
}
