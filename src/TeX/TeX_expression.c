/**
 * @file TeX_expression.c
 * @brief Convenience entry point for native mathematical text.
 *
 * The native input adapters own grammar and mathematical interpretation. This
 * entry point knows no expression trees, matrix layout or equation grammar.
 */
#include "TeX-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"


/* Ask the native mathematical input adapters to compose the requested notation. */
TeX_t *TeX_from_expression(const string_t *text)
{
    return expr_notation_from_text(text);
}
