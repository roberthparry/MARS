/**
 * @file TeX_quantum.c
 * @brief Compose operator accents, adjoints and Dirac state notation without evaluation.
 *
 * The RPN builder and semantic renderer share these fixed-arity presentation operations. Direct node-kind
 * indexing selects the notation and validates its arity; state labels and operators retain their authored order.
 * Native mathematical modules provide meaning through the semantic interface and never supply TeX syntax.
 */
#include <errno.h>

#include "string-t.h"
#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/* Compose only a recognised fixed-arity construct; every format string is owned by TeX. */
TeX_t *TeX_compose_quantum(TeX_node_kind_t kind, const TeX_t *const *operands, size_t count)
{
    static const struct {
        size_t arity;
        const char *format;
    } forms[] = {
        {1u, "\\widehat{%s}"},
        {1u, "{%s}^{\\dagger}"},
        {1u, "\\left\\langle %s \\right|"},
        {1u, "\\left| %s \\right\\rangle"},
        {2u, "\\left\\langle %s \\middle| %s \\right\\rangle"},
        {2u, "\\left| %s \\right\\rangle\\!\\left\\langle %s \\right|"},
        {3u, "\\left\\langle %s \\middle| %s \\middle| %s \\right\\rangle"}
    };
    _Static_assert(sizeof forms / sizeof *forms == TeX_NODE_MATRIX_ELEMENT - TeX_NODE_HAT + 1u,
                   "Quantum presentation kinds must remain a contiguous range.");
    /* HAT through MATRIX_ELEMENT form one contiguous range; unrelated node kinds allocate no empty slots. */
    unsigned form = (unsigned)kind - (unsigned)TeX_NODE_HAT;
    if (form >= sizeof forms / sizeof *forms || forms[form].arity != count || !operands) {
        errno = EINVAL;
        return NULL;
    }
    /* At most three operands: this validates a fixed-arity operation, rather than searching a name table. */
    for (size_t i = 0; i < count; ++i) {
        if (!operands[i] || !TeX_source(operands[i])) {
            errno = EINVAL;
            return NULL;
        }
    }
    const char *format = forms[form].format;
    string_t *source = count == 1u ? string_sprintf(format, TeX_source(operands[0]))
        : count == 2u ? string_sprintf(format, TeX_source(operands[0]), TeX_source(operands[1]))
        : string_sprintf(format, TeX_source(operands[0]), TeX_source(operands[1]), TeX_source(operands[2]));
    TeX_t *result = source ? TeX_compose_raw(string_c_str(source)) : NULL;
    string_free(source);
    return TeX_classify(result, kind);
}
