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
    expr_t *derivative;
    expr_t *second_derivative;
    const expr_t *variable;
    expr_context_t *context;
};

static number_t sample_value(const struct expression_sample *sample, const expr_t *expression)
{
    /* Sampling needs fresh bindings, not a new symbolic beautification at every
     * point. Retain the existing graph arrangement while applying the context. */
    expr_t *snapshot = expr_context_refine(sample->context, expression);
    number_t value = snapshot ? expr_eval(snapshot) : num_clone(NUM_NAN);
    expr_free(snapshot);
    return value;
}

static sampler_status_t evaluate_expression(number_t parameter, number_t *x, number_t *y, void *argument)
{
    struct expression_sample *sample = argument;
    if (!expr_context_set(sample->context, sample->variable, EXPR_CONTEXT_FREE, 0, parameter))
        return SAMPLER_ERROR;
    *x = num_clone(parameter);
    *y = sample_value(sample, sample->expression);
    return SAMPLER_VALUE;
}

static sampler_status_t evaluate_derivatives(number_t parameter, number_t *x, number_t *y,
                                              number_t *dx, number_t *dy, number_t *ddx, number_t *ddy, void *argument)
{
    struct expression_sample *sample = argument;
    sampler_status_t status = evaluate_expression(parameter, x, y, argument);
    if (status == SAMPLER_VALUE) {
        *dx = num_clone(NUM_ONE);
        *ddx = num_clone(NUM_ZERO);
        if (sample->derivative)
            *dy = sample_value(sample, sample->derivative);
        if (sample->second_derivative)
            *ddy = sample_value(sample, sample->second_derivative);
    }
    return status;
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
    bool adaptive = !options || options->method == SAMPLER_ADAPTIVE;
    if (adaptive && sample.expression)
        sample.derivative = expr_create_deriv(sample.expression, variable);
    if (sample.derivative)
        sample.second_derivative = expr_create_deriv(sample.derivative, variable);
    sampler_t *result = NULL;
    if (sample.expression && sample.context)
        result = adaptive ? sampler_new_with_derivatives(evaluate_derivatives, &sample, lower, upper, options) :
                            sampler_new(evaluate_expression, &sample, lower, upper, options);
    int saved = errno;
    expr_context_free(sample.context);
    expr_free(sample.expression);
    expr_free(sample.derivative);
    expr_free(sample.second_derivative);
    errno = saved;
    return result;
}
