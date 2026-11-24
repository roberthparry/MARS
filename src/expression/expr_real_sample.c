/**
 * @file expr_real_sample.c
 * @brief Guarded native double evaluation plans for real plot sampling.
 *
 * Compiles a small supported arithmetic DAG into bounded post-order instructions,
 * without replacing its bindings, algebra or ordinary numerical evaluator. Constants
 * carry conversion error estimates; operations propagate round-off estimates and
 * reject ill-conditioned results. These estimates are conservative heuristics, not
 * certified interval arithmetic. Unsupported graphs and uncertain points fall back
 * to the caller's ordinary expression evaluator. Bounded finite sums compile their
 * body once and retain a private index coordinate, without expanding the graph.
 * The controlled expression façade exposes this optional optimisation to native
 * plotting workers only.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

enum { SAMPLE_NODES = 256, SAMPLE_DEPTH = 64, SAMPLE_AXES = 3, SAMPLE_TERMS = 4096, SAMPLE_WORK = 65536 };

typedef struct {
    double value, error;
} sample_value_t;

typedef sample_value_t (*sample_operation_t)(sample_value_t, sample_value_t);

typedef struct {
    sample_operation_t operation;
    sample_value_t constant;
    size_t left, right;
    int axis;
    expr_real_sample_t *sum;
    long lower, upper;
} sample_instruction_t;

typedef struct _expr_real_sample_t {
    size_t count, axes, work;
    bool allow_sum;
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

static sample_value_t tangent(sample_value_t a, sample_value_t b)
{
    return divide(sine(a, b), cosine(a, b));
}

static sample_value_t cotangent(sample_value_t a, sample_value_t b)
{
    return divide(cosine(a, b), sine(a, b));
}

static sample_value_t secant(sample_value_t a, sample_value_t b)
{
    return divide((sample_value_t){1, 0}, cosine(a, b));
}

static sample_value_t cosecant(sample_value_t a, sample_value_t b)
{
    return divide((sample_value_t){1, 0}, sine(a, b));
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

static sample_value_t power(sample_value_t a, sample_value_t b)
{
    /* The positive real branch is unambiguous; all other bases use ordinary evaluation. */
    if (!(a.value > a.error))
        return (sample_value_t){NAN, INFINITY};
    double logarithm = log(a.value);
    double exponent_error = fabs(b.value) * a.error / (a.value - a.error) +
                            b.error * (fabs(logarithm) + a.error / (a.value - a.error));
    double value = pow(a.value, b.value);
    return rounded(value, fabs(value) * expm1(exponent_error));
}

static sample_value_t root(sample_value_t a, sample_value_t b)
{
    /* Compilation checks that this degree is an exact supported constant. */
    return power(a, divide((sample_value_t){1, 0}, b));
}

static sample_value_t square_root(sample_value_t a, sample_value_t b)
{
    (void)b;
    if (!(a.value > a.error))
        return (sample_value_t){NAN, INFINITY};
    return rounded(sqrt(a.value), a.error / (2 * sqrt(a.value - a.error)));
}

static sample_value_t cube_root(sample_value_t a, sample_value_t b)
{
    (void)b;
    return root(a, (sample_value_t){3, 0});
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
    [EXPR_KIND_TAN]   = tangent,
    [EXPR_KIND_COT]   = cotangent,
    [EXPR_KIND_SEC]   = secant,
    [EXPR_KIND_COSEC] = cosecant,
    [EXPR_KIND_EXP]   = exponential,
    [EXPR_KIND_LOG]   = logarithm,
    [EXPR_KIND_LOG10] = common_logarithm,
    [EXPR_KIND_ACOS]  = arccosine,
    [EXPR_KIND_POW]   = power,
    [EXPR_KIND_POW_D] = power,
    [EXPR_KIND_ROOT]  = root,
    [EXPR_KIND_SQRT]  = square_root,
    [EXPR_KIND_CUBRT] = cube_root,
};

static bool constant_value(number_t value, sample_value_t *output)
{
    double real = num_to_double(value);
    number_t converted = num_create_from_double(real), difference = num_sub(value, converted);
    double error = fabs(num_to_double(difference));
    bool ok = num_is_real(value) && num_is_finite(value) && isfinite(real) && isfinite(error);
    *output = rounded(real, error);
    num_destroy(&difference);
    num_destroy(&converted);
    return ok;
}

static expr_real_sample_t *build(const expr_t *expression, const expr_t *const *axes, size_t count, bool allow_sum);

static bool uses_coordinate(const expr_t *expression, const expr_t *const *axes, size_t count, unsigned depth)
{
    if (!expression)
        return false;
    if (depth > SAMPLE_DEPTH)
        return true;
    /* A bounded coordinate tuple, including the sum's private index, not a symbol-table lookup. */
    for (size_t i = 0; i < count; ++i)
        if (expression == axes[i] || (expression->var_id && expression->var_id == axes[i]->var_id))
            return true;
    return uses_coordinate(expression->a, axes, count, depth + 1) ||
           uses_coordinate(expression->b, axes, count, depth + 1);
}

static bool compile_sum(expr_real_sample_t *sample, const expr_t *expr, const expr_t *const *axes,
                        sample_instruction_t *instruction)
{
    const expr_t *bounds = expr->b, *lower = EXPR_ONE, *upper;
    if (!sample->allow_sum || !bounds || bounds->ops->kind != EXPR_KIND_ARGUMENT_LIST || !expr_is_var(bounds->a))
        return false;
    upper = bounds->b;
    if (upper && upper->ops->kind == EXPR_KIND_ARGUMENT_LIST) {
        lower = upper->a;
        upper = upper->b;
    }
    const expr_t *coordinates[SAMPLE_AXES + 1];
    for (size_t i = 0; i < sample->axes; ++i)
        coordinates[i] = axes[i];
    coordinates[sample->axes] = bounds->a;
    if (!upper || uses_coordinate(lower, coordinates, sample->axes + 1, 0) ||
        uses_coordinate(upper, coordinates, sample->axes + 1, 0))
        return false;
    number_t lo = expr_eval(lower), hi = expr_eval(upper);
    bool valid = num_is_real(lo) && num_is_integer(lo) && num_is_real(hi) && num_is_integer(hi) &&
                 num_is_finite(lo) && num_is_finite(hi);
    double first = num_to_double(lo), last = num_to_double(hi);
    num_destroy(&hi);
    num_destroy(&lo);
    if (!valid || first < -1048576 || last > 1048576 || last < first || last - first >= SAMPLE_TERMS)
        return false;
    expr_real_sample_t *body = build(expr->a, coordinates, sample->axes + 1, false);
    size_t terms = (size_t)(last - first) + 1;
    if (!body || body->work > (SAMPLE_WORK - sample->work) / terms) {
        expr_real_sample_free(body);
        return false;
    }
    instruction->sum = body;
    instruction->lower = (long)first;
    instruction->upper = (long)last;
    sample->work += terms * body->work;
    return true;
}

static bool compile(expr_real_sample_t *sample, const expr_t *expr, const expr_t *const *axes,
                    unsigned depth, size_t *index)
{
    if (!expr || depth > SAMPLE_DEPTH || sample->count == SAMPLE_NODES || !expr_computation_step("expression.sample"))
        return false;
    sample_instruction_t instruction = {.axis = -1};
    expr_op_kind_t kind = expr->ops->kind;
    if (kind == EXPR_KIND_CONST || kind == EXPR_KIND_VAR) {
        /* A fixed coordinate tuple plus an optional private index, not a symbol-table search. */
        for (size_t i = 0; i < sample->axes; ++i)
            if (expr == axes[i] || (expr->var_id && expr->var_id == axes[i]->var_id))
                instruction.axis = (int)i;
        if (instruction.axis < 0) {
            number_t value = expr_eval(expr);
            bool ok = constant_value(value, &instruction.constant);
            num_destroy(&value);
            if (!ok)
                return false;
        }
    } else if (kind == EXPR_KIND_SUMMATION) {
        if (!compile_sum(sample, expr, axes, &instruction))
            return false;
    } else {
        if (kind == EXPR_KIND_ROOT && (!expr->b || expr->b->ops != &ops_const ||
            !num_is_integer(expr->b->c) || !num_gt(expr->b->c, NUM_ONE) ||
            num_to_double(expr->b->c) > INT_MAX))
            return false;
        instruction.operation = kind < EXPR_KIND_COUNT ? operations[kind] : NULL;
        bool squared = (kind == EXPR_KIND_POW_D && num_eq(expr->c, NUM_TWO)) ||
                       (kind == EXPR_KIND_POW && expr->b && expr->b->ops == &ops_const &&
                        num_eq(expr->b->c, NUM_TWO));
        if (squared)
            instruction.operation = square;
        if (!instruction.operation || !compile(sample, expr->a, axes, depth + 1, &instruction.left))
            return false;
        if (!squared && kind == EXPR_KIND_POW_D) {
            if (sample->count == SAMPLE_NODES)
                return false;
            sample_instruction_t exponent = {.axis = -1};
            if (!constant_value(expr->c, &exponent.constant))
                return false;
            instruction.right = sample->count;
            sample->instructions[sample->count++] = exponent;
        } else if (!squared && expr->ops->arity == EXPR_OP_BINARY &&
            !compile(sample, expr->b, axes, depth + 1, &instruction.right))
            return false;
    }
    if (sample->count == SAMPLE_NODES || sample->work == SAMPLE_WORK) {
        expr_real_sample_free(instruction.sum);
        return false;
    }
    ++sample->work;
    *index = sample->count;
    sample->instructions[sample->count++] = instruction;
    return true;
}

static expr_real_sample_t *build(const expr_t *expression, const expr_t *const *axes, size_t count, bool allow_sum)
{
    expr_real_sample_t *sample = calloc(1, sizeof(*sample));
    if (!sample)
        return NULL;
    sample->axes = count;
    sample->allow_sum = allow_sum;
    size_t root;
    if (!compile(sample, expression, axes, 0, &root)) {
        expr_real_sample_free(sample);
        return NULL;
    }
    return sample;
}

/* Plans own their constants and coordinate indices, never the source expression. */
expr_real_sample_t *expr_real_sample_new(const expr_t *expression, const expr_t *const *axes, size_t count)
{
    if (!axes || !count || count > SAMPLE_AXES)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if (!axes[i])
            return NULL;
    return build(expression, axes, count, true);
}

static bool evaluate(const expr_real_sample_t *sample, const double *coordinates, sample_value_t *output, bool control)
{
    sample_value_t values[SAMPLE_NODES];
    for (size_t i = 0; i < sample->count; ++i) {
        if (control && !expr_computation_step("expression.sample"))
            return false;
        const sample_instruction_t *instruction = &sample->instructions[i];
        sample_value_t result = instruction->constant;
        if (instruction->sum) {
            double point[SAMPLE_AXES + 1];
            for (size_t axis = 0; axis < sample->axes; ++axis)
                point[axis] = coordinates[axis];
            double total = 0, correction = 0, error = 0, magnitude = 0;
            for (long term = instruction->lower; term <= instruction->upper; ++term) {
                sample_value_t part;
                point[sample->axes] = (double)term;
                /* Each body has at most 256 instructions and no nested loop. */
                if (!expr_computation_step("expression.sample") || !evaluate(instruction->sum, point, &part, false))
                    return false;
                double next = total + part.value;
                correction += fabs(total) >= fabs(part.value) ? (total - next) + part.value :
                                                                (part.value - next) + total;
                total = next;
                error += part.error;
                magnitude += fabs(part.value);
            }
            /* Compensated accumulation avoids a quadratic round-off estimate from partial totals. */
            result = rounded(total + correction, error + 16 * DBL_EPSILON * magnitude);
        } else if (instruction->axis >= 0)
            result = !sample->allow_sum && (size_t)instruction->axis == sample->axes - 1 ?
                     (sample_value_t){coordinates[instruction->axis], 0} : rounded(coordinates[instruction->axis], 0);
        else if (instruction->operation)
            result = instruction->operation(values[instruction->left], values[instruction->right]);
        if (!isfinite(result.value) || !isfinite(result.error))
            return false;
        values[i] = result;
    }
    *output = values[sample->count - 1];
    return true;
}

/* The caller must compare this conservative estimate with its own approximation tolerance. */
bool expr_real_sample_estimate(const expr_real_sample_t *sample, const double *coordinates,
                               double *value, double *absolute_error)
{
    sample_value_t result;
    if (!sample || !coordinates || !value || !absolute_error || value == absolute_error ||
        !evaluate(sample, coordinates, &result, true))
        return false;
    if (result.error >= fabs(result.value))
        return false;
    *value = result.value;
    *absolute_error = result.error;
    return true;
}

/* A failed approximation must never be treated as a hole or as an accepted root. */
bool expr_real_sample_eval(const expr_real_sample_t *sample, const double *coordinates, double *value)
{
    double result, error;
    if (!value || !expr_real_sample_estimate(sample, coordinates, &result, &error) ||
        error > 1e-10 * fmax(1, fabs(result)))
        return false;
    *value = result;
    return true;
}

/* Dispose of the optional plan independently of the original graph. */
void expr_real_sample_free(expr_real_sample_t *sample)
{
    for (size_t i = 0; sample && i < sample->count; ++i)
        expr_real_sample_free(sample->instructions[i].sum);
    free(sample);
}
