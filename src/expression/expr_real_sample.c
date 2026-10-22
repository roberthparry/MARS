/**
 * @file expr_real_sample.c
 * @brief Guarded native double evaluation plans for real plot sampling.
 *
 * Compiles a small supported arithmetic DAG into bounded post-order instructions,
 * without replacing its bindings, algebra or ordinary numerical evaluator. Constants
 * carry conversion error estimates; operations propagate round-off estimates and
 * reject ill-conditioned results. These estimates are conservative heuristics, not
 * certified interval arithmetic. Unsupported graphs and uncertain points fall back
 * to the caller's ordinary expression evaluator. The controlled expression façade
 * exposes this optional optimisation to native plotting workers only.
 */
#include <float.h>
#include <math.h>
#include <stdlib.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

enum { SAMPLE_NODES = 256, SAMPLE_DEPTH = 64, SAMPLE_AXES = 3 };

typedef struct {
    double value, error;
} sample_value_t;

typedef sample_value_t (*sample_operation_t)(sample_value_t, sample_value_t);

typedef struct {
    sample_operation_t operation;
    sample_value_t constant;
    size_t left, right;
    int axis;
} sample_instruction_t;

typedef struct _expr_real_sample_t {
    size_t count, axes;
    sample_instruction_t instructions[SAMPLE_NODES];
} expr_real_sample_t;

static sample_value_t rounded(double value, double error)
{
    return (sample_value_t){value, error + 8 * DBL_EPSILON * fabs(value) + DBL_MIN};
}

static sample_value_t add(sample_value_t a, sample_value_t b)
{
    return rounded(a.value + b.value, a.error + b.error);
}

static sample_value_t subtract(sample_value_t a, sample_value_t b)
{
    return rounded(a.value - b.value, a.error + b.error);
}

static sample_value_t multiply(sample_value_t a, sample_value_t b)
{
    return rounded(a.value * b.value, fabs(a.value) * b.error + fabs(b.value) * a.error + a.error * b.error);
}

static sample_value_t divide(sample_value_t a, sample_value_t b)
{
    double denominator = fabs(b.value) - b.error;
    if (!(denominator > 0))
        return (sample_value_t){NAN, INFINITY};
    double value = a.value / b.value;
    return rounded(value, (a.error + fabs(value) * b.error) / denominator);
}

static sample_value_t negate(sample_value_t a, sample_value_t b)
{
    (void)b;
    return (sample_value_t){-a.value, a.error};
}

static sample_value_t absolute(sample_value_t a, sample_value_t b)
{
    (void)b;
    return (sample_value_t){fabs(a.value), a.error};
}

static sample_value_t sine(sample_value_t a, sample_value_t b)
{
    (void)b;
    if (fabs(a.value) > 1048576)
        return (sample_value_t){NAN, INFINITY};
    return rounded(sin(a.value), a.error + 8 * DBL_EPSILON);
}

static sample_value_t cosine(sample_value_t a, sample_value_t b)
{
    (void)b;
    if (fabs(a.value) > 1048576)
        return (sample_value_t){NAN, INFINITY};
    return rounded(cos(a.value), a.error + 8 * DBL_EPSILON);
}

static sample_value_t square(sample_value_t a, sample_value_t b)
{
    (void)b;
    return multiply(a, a);
}

static sample_value_t exponential(sample_value_t a, sample_value_t b)
{
    (void)b;
    double value = exp(a.value);
    return rounded(value, fabs(value) * expm1(a.error));
}

static sample_value_t logarithm(sample_value_t a, sample_value_t b)
{
    (void)b;
    if (!(a.value > a.error))
        return (sample_value_t){NAN, INFINITY};
    return rounded(log(a.value), a.error / (a.value - a.error));
}

static sample_value_t arccosine(sample_value_t a, sample_value_t b)
{
    (void)b;
    double edge = fabs(a.value) + a.error;
    if (!(edge < 1))
        return (sample_value_t){NAN, INFINITY};
    return rounded(acos(a.value), a.error / sqrt((1 - edge) * (1 + edge)));
}

static sample_value_t common_logarithm(sample_value_t a, sample_value_t b)
{
    sample_value_t natural = logarithm(a, b);
    return rounded(natural.value / log(10.0), natural.error / log(10.0));
}

static const sample_operation_t operations[EXPR_KIND_COUNT] = {
    [EXPR_KIND_ADD]   = add,
    [EXPR_KIND_SUB]   = subtract,
    [EXPR_KIND_MUL]   = multiply,
    [EXPR_KIND_DIV]   = divide,
    [EXPR_KIND_NEG]   = negate,
    [EXPR_KIND_ABS]   = absolute,
    [EXPR_KIND_SIN]   = sine,
    [EXPR_KIND_COS]   = cosine,
    [EXPR_KIND_EXP]   = exponential,
    [EXPR_KIND_LOG]   = logarithm,
    [EXPR_KIND_LOG10] = common_logarithm,
    [EXPR_KIND_ACOS]  = arccosine,
};

static bool compile(expr_real_sample_t *sample, const expr_t *expr, const expr_t *const *axes,
                    unsigned depth, size_t *index)
{
    if (!expr || depth > SAMPLE_DEPTH || sample->count == SAMPLE_NODES || !expr_computation_step("expression.sample"))
        return false;
    sample_instruction_t instruction = {.axis = -1};
    expr_op_kind_t kind = expr->ops->kind;
    if (kind == EXPR_KIND_CONST || kind == EXPR_KIND_VAR) {
        /* At most three explicitly supplied coordinate identities, not a symbol-table search. */
        for (size_t i = 0; i < sample->axes; ++i)
            if (expr == axes[i] || (expr->var_id && expr->var_id == axes[i]->var_id))
                instruction.axis = (int)i;
        if (instruction.axis < 0) {
            number_t value = expr_eval(expr);
            double real = num_to_double(value);
            number_t converted = num_create_from_double(real), difference = num_sub(value, converted);
            double error = fabs(num_to_double(difference));
            bool ok = num_is_real(value) && num_is_finite(value) && isfinite(real) && isfinite(error);
            instruction.constant = rounded(real, error);
            num_destroy(&difference);
            num_destroy(&converted);
            num_destroy(&value);
            if (!ok)
                return false;
        }
    } else {
        instruction.operation = kind < EXPR_KIND_COUNT ? operations[kind] : NULL;
        bool squared = (kind == EXPR_KIND_POW_D && num_eq(expr->c, NUM_TWO)) ||
                       (kind == EXPR_KIND_POW && expr->b && expr->b->ops == &ops_const &&
                        num_eq(expr->b->c, NUM_TWO));
        if (squared)
            instruction.operation = square;
        if (!instruction.operation || !compile(sample, expr->a, axes, depth + 1, &instruction.left))
            return false;
        if (!squared && expr->ops->arity == EXPR_OP_BINARY &&
            !compile(sample, expr->b, axes, depth + 1, &instruction.right))
            return false;
    }
    if (sample->count == SAMPLE_NODES)
        return false;
    *index = sample->count;
    sample->instructions[sample->count++] = instruction;
    return true;
}

/* Plans own their constants and coordinate indices, never the source expression. */
expr_real_sample_t *expr_real_sample_new(const expr_t *expression, const expr_t *const *axes, size_t count)
{
    if (!axes || !count || count > SAMPLE_AXES)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if (!axes[i])
            return NULL;
    expr_real_sample_t *sample = calloc(1, sizeof(*sample));
    if (!sample)
        return NULL;
    sample->axes = count;
    size_t root;
    if (!compile(sample, expression, axes, 0, &root)) {
        free(sample);
        return NULL;
    }
    return sample;
}

/* A failed approximation must never be treated as a hole or as an accepted root. */
bool expr_real_sample_eval(const expr_real_sample_t *sample, const double *coordinates, double *value)
{
    if (!sample || !coordinates || !value)
        return false;
    sample_value_t values[SAMPLE_NODES];
    for (size_t i = 0; i < sample->count; ++i) {
        if (!expr_computation_step("expression.sample"))
            return false;
        const sample_instruction_t *instruction = &sample->instructions[i];
        sample_value_t result = instruction->constant;
        if (instruction->axis >= 0)
            result = rounded(coordinates[instruction->axis], 0);
        else if (instruction->operation)
            result = instruction->operation(values[instruction->left], values[instruction->right]);
        if (!isfinite(result.value) || !isfinite(result.error))
            return false;
        values[i] = result;
    }
    sample_value_t result = values[sample->count - 1];
    if (result.error >= fabs(result.value) || result.error > 1e-10 * fmax(1, fabs(result.value)))
        return false;
    *value = result.value;
    return true;
}

/* Dispose of the optional plan independently of the original graph. */
void expr_real_sample_free(expr_real_sample_t *sample)
{
    free(sample);
}
