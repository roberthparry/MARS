/**
 * @file TeX_wrap.c
 * @brief Lay out semantic domains and wrap sums using native-owned term-selection callbacks.
 *
 * TeX lays out outer domain restrictions beneath their formula and chooses when
 * additive terms need wrapping. It owns alignment, signs and spacing. The caller
 * selects mathematical terms and supplies independent semantic contexts only
 * when a longer rendering needs them. No native object
 * layout or mathematical transformation is interpreted here.
 */
#include <string.h>

#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/* Incorporate only this renderer's own outer alignment; nested mathematical groups remain untouched. */
static bool append_aligned_body(string_t *source, const TeX_t *body)
{
    static const char prefix[] = "\\begin{aligned}[t]\n";
    static const char suffix[] = "\n\\end{aligned}";
    const char *text = TeX_source(body);
    size_t length = strlen(text);
    if (length >= sizeof prefix + sizeof suffix - 2u && !strncmp(text, prefix, sizeof prefix - 1u) &&
        !strcmp(text + length - (sizeof suffix - 1u), suffix))
        return string_append_bytes(source, text + sizeof prefix - 1u,
                                   length - (sizeof prefix + sizeof suffix - 2u)) == 0;
    return string_append_format(source, "&%s", text) >= 0;
}

/* A domain qualifies its own body, not an enclosing sum, equation or neighbouring solution. */
static TeX_t *wrap_domain(TeX_node_ref_t root, const TeX_node_view_t *view, size_t line_limit,
                           const TeX_sum_provider_t *provider, void *state)
{
    TeX_node_ref_t child = {0};
    if (!root.provider->child(root.context, root.node, 0u, &child))
        return NULL;
    TeX_context_t *context = TeX_context_new(child);
    TeX_t *body = context ? TeX_wrap_sum(context, line_limit, provider, state) : NULL;
    TeX_context_free(context);
    string_t *source = body ? string_new_with("\\begin{aligned}[t]\n") : NULL;
    bool valid = source && append_aligned_body(source, body);
    TeX_free(body);
    const string_t *definitions = provider && provider->definitions ? provider->definitions(state) : NULL;
    if (valid && definitions && string_length(definitions))
        valid = string_append_format(source, " \\\\\n%S", definitions) >= 0;
    if (valid)
        valid = string_append_cstr(source, " \\\\\n&\\text{provided that}") == 0;
    for (size_t i = 1u; valid && i < view->child_count; ++i) {
        valid = root.provider->child(root.context, root.node, i, &child);
        context = valid ? TeX_context_new(child) : NULL;
        TeX_t *condition = context ? TeX_from_context(context) : NULL;
        TeX_context_free(context);
        valid = condition && string_append_format(source, " \\\\\n&\\quad %s", TeX_source(condition)) >= 0;
        TeX_free(condition);
    }
    if (valid)
        valid = string_append_cstr(source, "\n\\end{aligned}") == 0;
    TeX_t *result = valid ? TeX_compose_raw(string_c_str(source)) : NULL;
    string_free(source);
    return result;
}

/* Apply a soft source-line budget while retaining the ordinary body if wrapping is unavailable. */
TeX_t *TeX_wrap_sum(const TeX_context_t *context, size_t line_limit,
                    const TeX_sum_provider_t *provider, void *state)
{
    TeX_t *body = context ? TeX_from_context(context) : NULL;
    if (!body)
        return NULL;
    TeX_node_ref_t root = TeX_context_root(context);
    TeX_node_view_t view;
    bool described = root.provider->describe(root.context, root.node, &view);
    /* An explicit row layout already separates the complete formula from definitions or companion equations.
     * Replacing it with additive terms would detach signs and discard or duplicate those accompanying rows. */
    if (described && view.kind == TeX_NODE_LINES)
        return body;
    if (described && view.kind == TeX_NODE_DOMAIN) {
        TeX_t *wrapped = wrap_domain(root, &view, line_limit, provider, state);
        if (wrapped) {
            TeX_free(body);
            return wrapped;
        }
        if (provider && provider->definitions && provider->definitions(state)) {
            TeX_free(body);
            return NULL;
        }
        return body;
    }
    if (!line_limit)
        line_limit = 110u;
    if (strlen(TeX_source(body)) <= line_limit || !provider || !provider->count || !provider->term)
        return body;
    size_t count = provider->count(state);
    if (count < 2u)
        return body;
    string_t *source = string_new_with("\\begin{aligned}[t]\n");
    if (!source)
        return body;
    bool valid = true;
    if (described && view.kind == TeX_NODE_EQUATION && view.child_count == 2u) {
        TeX_node_ref_t left = {0};
        bool found = root.provider->child(root.context, root.node, 0u, &left);
        TeX_context_t *left_context = found ? TeX_context_new(left) : NULL;
        TeX_t *notation = left_context ? TeX_from_context(left_context) : NULL;
        TeX_context_free(left_context);
        valid = notation && string_append_format(source, "%s = {} ", TeX_source(notation)) >= 0;
        TeX_free(notation);
    }
    for (size_t i = 0u; i < count && valid; ++i) {
        int sign = 0;
        TeX_context_t *term_context = provider->term(state, i, &sign);
        TeX_t *notation = term_context ? TeX_from_context(term_context) : NULL;
        TeX_context_free(term_context);
        valid = notation && (sign == 1 || sign == -1);
        if (valid) {
            const char *term = TeX_source(notation);
            while (*term == ' ')
                ++term;
            if (*term == '-') {
                sign = -sign;
                ++term;
            }
            const char *prefix = sign < 0 ? (i ? "{} - " : "-") : (i ? "{} + " : "");
            valid = (!i || string_append_cstr(source, " \\\\\n") >= 0) &&
                    string_append_format(source, "&%s%s", prefix, term) >= 0;
        }
        TeX_free(notation);
    }
    if (valid)
        valid = string_append_cstr(source, "\n\\end{aligned}") >= 0;
    TeX_t *wrapped = valid ? TeX_compose_raw(string_c_str(source)) : NULL;
    string_free(source);
    if (!wrapped)
        return body;
    TeX_free(body);
    return wrapped;
}
