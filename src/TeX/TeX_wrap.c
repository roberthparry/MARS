/**
 * @file TeX_wrap.c
 * @brief Wrap semantic sums using native-owned term-selection callbacks.
 *
 * TeX chooses whether line wrapping is necessary and owns alignment, signs and
 * spacing. The caller selects mathematical terms and supplies independent
 * semantic contexts only when a longer rendering needs them. No native object
 * layout or mathematical transformation is interpreted here.
 */
#include <string.h>

#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/* Apply a soft source-line budget while retaining the ordinary body if wrapping is unavailable. */
TeX_t *TeX_wrap_sum(const TeX_context_t *context, size_t line_limit,
                    const TeX_sum_provider_t *provider, void *state)
{
    TeX_t *body = context ? TeX_from_context(context) : NULL;
    if (!body)
        return NULL;
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
    TeX_t *wrapped = valid ? TeX_raw(string_c_str(source)) : NULL;
    string_free(source);
    if (!wrapped)
        return body;
    TeX_free(body);
    return wrapped;
}
