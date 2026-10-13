/**
 * @file sampler_expression.c
 * @brief Expression/context adapter for real function sampling without modifying caller bindings.
 *
 * A private graph and context snapshot keep each evaluation independent of the
 * caller. Complex or undefined values become gaps. Samples never serve as proof
 * of a symbolic assumption, and no expression work is performed in the browser.
 */
#include <errno.h>

#include "sampler-t.h"

struct expression_sample {
    expr_t *expression;
    const expr_t *variable;
    expr_context_t *context;
};

static sampler_status_t evaluate_expression(number_t parameter, number_t *x, number_t *y, void *argument)
{
    struct expression_sample *sample = argument;
    if (!expr_context_set(sample->context, sample->variable, EXPR_CONTEXT_FREE, 0, parameter))
        return SAMPLER_ERROR;
    *x = num_clone(parameter);
    *y = expr_context_evaluate(sample->context, sample->expression);
    return SAMPLER_VALUE;
}

/* Capture source state once; each point is evaluated under a private binding snapshot. */
sampler_t *sampler_expression(const expr_t *expression, const expr_t *variable, const expr_context_t *context,
                              number_t lower, number_t upper, const sampler_options_t *options)
{
    if (!expression || !variable || !expr_is_variable(variable)) {
        errno = EINVAL;
        return NULL;
    }
    struct expression_sample sample = {.expression = expr_clone(expression), .variable = variable,
        .context = context ? expr_context_clone(context) : expr_context_from_expression(expression, variable)};
    sampler_t *result = sample.expression && sample.context ?
        sampler_new(evaluate_expression, &sample, lower, upper, options) : NULL;
    int saved = errno;
    expr_context_free(sample.context);
    expr_free(sample.expression);
    errno = saved;
    return result;
}
