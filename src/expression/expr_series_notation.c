/**
 * @file expr_series_notation.c
 * @brief Select mathematical alternatives for recognised series derivations.
 *
 * Expression recognises special closed forms and chooses their conditions;
 * the TeX layout interface arranges the resulting mathematical fragments.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_series_notation.h"

static char *series_parameter(const expr_t *parameter)
{
    const char *name = parameter ? expr_symbol_name(parameter) : NULL;
    if (name)
        return strdup(name);
    const expr_t *positive = parameter && expr_is_neg(parameter) ? parameter->a : NULL;
    name = positive ? expr_symbol_name(positive) : NULL;
    if (name) {
        size_t length = strlen(name);
        char *negative = malloc(length + 2u);
        if (negative) {
            negative[0] = '-';
            memcpy(negative + 1u, name, length + 1u);
        }
        return negative;
    }
    return parameter ? expr_to_TeX_body(parameter) : NULL;
}

/* Select the recognised limiting case without exposing expression structures to TeX. */
string_t *expr_series_derivation_TeX(const string_t *series, const expr_t *result)
{
    if (!series || !result)
        return NULL;
    char *rendered = expr_to_TeX_body(result);
    if (!rendered)
        return NULL;
    const expr_t *parameter = NULL, *endpoint = NULL;
    string_t *output = NULL;
    if (!strcmp(string_c_str(series), rendered)) {
        output = TeX_align_derivation(series, NULL, NULL, NULL, NULL);
    } else if (expr_series_zeta_difference_parts(result, &parameter, &endpoint)) {
        expr_t *digamma = expr_digamma(endpoint);
        expr_t *gamma = expr_from_string("gamma", NULL);
        expr_t *harmonic = digamma && gamma ? expr_add(digamma, gamma) : NULL;
        char *alternative = harmonic ? expr_to_TeX_body(harmonic) : NULL;
        char *condition = series_parameter(parameter);
        if (alternative && condition)
            output = TeX_align_derivation(series, rendered, alternative, condition, "1");
        free(condition);
        free(alternative);
        expr_free(harmonic);
        expr_free(gamma);
        expr_free(digamma);
    } else {
        output = TeX_align_derivation(series, rendered, NULL, NULL, NULL);
    }
    free(rendered);
    return output;
}
