/**
 * @file TeX_builder.c
 * @brief Owned reverse Polish construction of immutable mathematical notation.
 *
 * Keeps operands in a directly indexed MARS array. Each operation constructs its
 * result before releasing consumed operands, so errors preserve the previous
 * stack. A saved error blocks later mutations until explicitly cleared. Syntax
 * remains supplied by the TeX module's private fragment constructors.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

struct _TeX_builder_t {
    array_t *values;
    int error;
};

static bool TeX_builder_fail(TeX_builder_t *builder, int error)
{
    if (builder && !builder->error)
        builder->error = error;
    errno = builder ? builder->error : EINVAL;
    return false;
}

static bool TeX_builder_ready(TeX_builder_t *builder)
{
    if (!builder || builder->error)
        return TeX_builder_fail(builder, EINVAL);
    errno = 0;
    return true;
}

static void TeX_stack_destroy(void *entry)
{
    TeX_free(*(TeX_t **)entry);
}

static const TeX_t *const *TeX_operands(TeX_builder_t *builder, size_t count)
{
    static const TeX_t *const empty[] = {NULL};
    if (!TeX_builder_ready(builder))
        return NULL;
    size_t size = array_size(builder->values);
    if (count > size) {
        TeX_builder_fail(builder, ERANGE);
        return NULL;
    }
    return count ? array_get(builder->values, size - count) : empty;
}

static bool TeX_replace(TeX_builder_t *builder, size_t count, TeX_t *result)
{
    if (!result)
        return TeX_builder_fail(builder, errno ? errno : ENOMEM);
    if (!count) {
        if (array_append_carray(builder->values, &result, 1))
            return true;
        TeX_free(result);
        return TeX_builder_fail(builder, ENOMEM);
    }
    size_t first = array_size(builder->values) - count;
    TeX_t **entry = array_get(builder->values, first);
    TeX_free(*entry);
    *entry = result;
    /* Removing a validated tail cannot allocate or move surviving operands. */
    if (count > 1)
        array_remove_elements(builder->values, first + 1, count - 1);
    return true;
}

/* Allocate an empty owner for notation operands and the first construction error. */
TeX_builder_t *TeX_builder_new(void)
{
    TeX_builder_t *builder = calloc(1, sizeof *builder);
    if (builder)
        builder->values = array_create(sizeof(TeX_t *), NULL, TeX_stack_destroy);
    if (!builder || !builder->values) {
        free(builder);
        errno = ENOMEM;
        return NULL;
    }
    return builder;
}

/* Release all unfinished fragments; completed values have already left the stack. */
void TeX_builder_free(TeX_builder_t *builder)
{
    if (builder) {
        array_destroy(builder->values);
        free(builder);
    }
}

/* Inspect stack depth without interpreting or traversing its operands. */
size_t TeX_builder_count(const TeX_builder_t *builder)
{
    return builder ? array_size(builder->values) : 0;
}

/* Report a saved error independently of the process's current errno. */
int TeX_builder_error(const TeX_builder_t *builder)
{
    return builder ? builder->error : EINVAL;
}

/* Allow an explicit retry with the operands preserved by the failed operation. */
void TeX_builder_clear_error(TeX_builder_t *builder)
{
    if (builder)
        builder->error = 0;
}

/* Start another formula while releasing all operands left from the previous one. */
void TeX_builder_clear(TeX_builder_t *builder)
{
    if (builder) {
        array_clear(builder->values);
        builder->error = 0;
    }
}

/* Borrow an operand by its directly indexed HP48 stack level. */
const TeX_t *TeX_builder_peek(const TeX_builder_t *builder, size_t level)
{
    if (!builder) {
        errno = EINVAL;
        return NULL;
    }
    size_t count = array_size(builder->values);
    if (!level || level > count) {
        errno = ERANGE;
        return NULL;
    }
    return *(TeX_t **)array_get(builder->values, count - level);
}

/* Transfer the sole operand without cloning or leaving shared ownership behind. */
TeX_t *TeX_builder_finish(TeX_builder_t *builder)
{
    if (!TeX_builder_ready(builder))
        return NULL;
    if (array_size(builder->values) != 1) {
        TeX_builder_fail(builder, EINVAL);
        return NULL;
    }
    TeX_t **entry = array_get(builder->values, 0), *result = *entry;
    *entry = NULL;
    array_remove(builder->values, 0);
    return result;
}

/* Copy trusted syntax before any operand-array allocation can invalidate a view. */
bool TeX_push_raw(TeX_builder_t *builder, const char *source)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_raw(source));
}

/* Push a mathematical name without making its caller write or parse TeX commands. */
bool TeX_push_symbol(TeX_builder_t *builder, const char *name)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_symbol(name));
}

/* Escape literal prose and place its owned copy on the construction stack. */
bool TeX_push_text(TeX_builder_t *builder, const string_t *text)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_text(text));
}

/* Delegate mathematical interpretation to the established native input adapters. */
bool TeX_push_expression(TeX_builder_t *builder, const string_t *text)
{
    if (!TeX_builder_ready(builder))
        return false;
    TeX_t *notation = TeX_from_expression(text);
    return notation ? TeX_replace(builder, 0, notation) : TeX_builder_fail(builder, errno ? errno : EINVAL);
}

/* Adapt a convenient C string without exposing temporary string ownership. */
bool TeX_push_math(TeX_builder_t *builder, const char *text)
{
    if (!TeX_builder_ready(builder))
        return false;
    if (!text)
        return TeX_builder_fail(builder, EINVAL);
    string_t *input = string_new_with(text);
    bool ok = input ? TeX_push_expression(builder, input) : TeX_builder_fail(builder, ENOMEM);
    string_free(input);
    return ok;
}

/* Supply integer notation without requiring callers to create a source string. */
bool TeX_push_integer(TeX_builder_t *builder, long value)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_integer(value));
}

/* Copy a finished or stack-owned value; the original remains with its owner. */
bool TeX_push_notation(TeX_builder_t *builder, const TeX_t *notation)
{
    if (!TeX_builder_ready(builder))
        return false;
    return notation ? TeX_replace(builder, 0, TeX_copy(notation)) : TeX_builder_fail(builder, EINVAL);
}

/* Select fixed spacing syntax by direct enum indexing. */
bool TeX_push_space(TeX_builder_t *builder, TeX_space_t space)
{
    static const char *const source[] = {"\\,", "\\qquad "};
    if (!TeX_builder_ready(builder))
        return false;
    if ((unsigned)space >= sizeof source / sizeof *source)
        return TeX_builder_fail(builder, EINVAL);
    return TeX_push_raw(builder, source[space]);
}

/* Duplicate level one with independently owned notation and literal metadata. */
bool TeX_dup(TeX_builder_t *builder)
{
    if (!TeX_builder_ready(builder))
        return false;
    const TeX_t *value = TeX_builder_peek(builder, 1);
    return value ? TeX_push_notation(builder, value) : TeX_builder_fail(builder, ERANGE);
}

/* Consume a literal level and shift only the selected HP48 ROLL pointer range. */
bool TeX_roll(TeX_builder_t *builder)
{
    const TeX_t *const *control = TeX_operands(builder, 1);
    if (!control)
        return false;
    long level;
    if (!TeX_integer_value(control[0], &level))
        return TeX_builder_fail(builder, EINVAL);
    size_t size = array_size(builder->values);
    if (level < 1 || (uintmax_t)level > size - 1)
        return TeX_builder_fail(builder, ERANGE);
    size_t count = (size_t)level;
    TeX_t **entries = array_get(builder->values, size - count - 1);
    TeX_t *selected = entries[0];
    memmove(entries, entries + 1, (count - 1) * sizeof *entries);
    entries[count - 1] = selected;
    array_remove(builder->values, size - 1);
    return true;
}

/* Rotate the top three HP48 levels without allocating or cloning their values. */
bool TeX_rot(TeX_builder_t *builder)
{
    if (!TeX_operands(builder, 3))
        return false;
    TeX_t **entries = array_get(builder->values, array_size(builder->values) - 3);
    TeX_t *first = entries[0];
    entries[0] = entries[1];
    entries[1] = entries[2];
    entries[2] = first;
    return true;
}

/* Consume two operands and let TeX supply the plus sign. */
bool TeX_add(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_arithmetic_binary(p[0], p[1], false));
}

/* Preserve the visual scope and order of a subtrahend. */
bool TeX_sub(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_arithmetic_binary(p[0], p[1], true));
}

/* Apply negation to the complete top operand. */
bool TeX_neg(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_arithmetic_negative(p[0]));
}

/* Provide the ordinary binary RPN multiplication operation. */
bool TeX_mul(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_arithmetic_multiply(p[0], p[1]));
}

/* Apply a validated function name to already selected notation, without evaluation. */
bool TeX_function(TeX_builder_t *builder, const char *name, size_t count)
{
    const TeX_t *const *p = TeX_operands(builder, count);
    if (!p)
        return false;
    TeX_t *validated = TeX_symbol(name);
    string_t *input = validated ? string_new_with(name) : NULL;
    TeX_t *result = input ? TeX_named_call(input, p, count) : NULL;
    TeX_free(validated);
    string_free(input);
    return TeX_replace(builder, count, result);
}

/* Consume function notation followed by arguments, retaining their push order. */
bool TeX_call(TeX_builder_t *builder, size_t count)
{
    if (!TeX_builder_ready(builder))
        return false;
    if (count == SIZE_MAX)
        return TeX_builder_fail(builder, EOVERFLOW);
    const TeX_t *const *p = TeX_operands(builder, count + 1);
    return p && TeX_replace(builder, count + 1, TeX_applied(p[0], p + 1, count));
}

/* Consume numerator followed by denominator. */
bool TeX_frac(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_fraction(p[0], p[1]), TeX_NODE_FRACTION));
}

/* Apply an unindexed square root to the current top operand. */
bool TeX_sqrt(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_classify(TeX_compose_root(p[0], NULL), TeX_NODE_ROOT));
}

/* Consume a radicand followed by its explicit root index. */
bool TeX_root(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_root(p[0], p[1]), TeX_NODE_ROOT));
}

/* Consume base followed by exponent. */
bool TeX_pow(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_arithmetic_scripts(p[0], NULL, p[1]));
}

/* Consume base followed by its lower index. */
bool TeX_subscript(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_arithmetic_scripts(p[0], p[1], NULL));
}

/* Consume base, lower index and upper index in that order. */
bool TeX_scripts(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 3);
    return p && TeX_replace(builder, 3, TeX_arithmetic_scripts(p[0], p[1], p[2]));
}

/* Compose exactly the requested tail, retaining its original push order. */
bool TeX_sequence(TeX_builder_t *builder, size_t count)
{
    const TeX_t *const *p = TeX_operands(builder, count);
    return p && TeX_replace(builder, count, TeX_compose_sequence(p, count));
}

/* Consume left side followed by right side. */
bool TeX_equation(TeX_builder_t *builder, TeX_equation_style_t style)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_equation(p[0], p[1], style), TeX_NODE_EQUATION));
}

/* Apply visible delimiters without interpreting the enclosed notation. */
bool TeX_fence(TeX_builder_t *builder, TeX_fence_t fence)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_classify(TeX_compose_fenced(p[0], fence), TeX_NODE_FENCED));
}

/* Consume a base and ordered tensor slots, validating variance before mutation. */
bool TeX_tensor(TeX_builder_t *builder, const TeX_index_position_t *positions, size_t count)
{
    if (!TeX_builder_ready(builder))
        return false;
    if (count == SIZE_MAX || count > SIZE_MAX / sizeof(TeX_index_t))
        return TeX_builder_fail(builder, EOVERFLOW);
    if (count && !positions)
        return TeX_builder_fail(builder, EINVAL);
    const TeX_t *const *p = TeX_operands(builder, count + 1);
    if (!p)
        return false;
    for (size_t i = 0; i < count; ++i)
        if ((unsigned)positions[i] > TeX_INDEX_UPPER)
            return TeX_builder_fail(builder, EINVAL);
    TeX_index_t *indices = count ? malloc(count * sizeof *indices) : NULL;
    if (count && !indices)
        return TeX_builder_fail(builder, ENOMEM);
    for (size_t i = 0; i < count; ++i)
        indices[i] = (TeX_index_t){p[i + 1], positions[i]};
    TeX_t *result = TeX_compose_tensor(p[0], indices, count);
    free(indices);
    return TeX_replace(builder, count + 1, TeX_classify(result, TeX_NODE_IDENTIFIER));
}

static const TeX_t *const *TeX_limit_operands(TeX_builder_t *builder, size_t base, TeX_limits_t limits,
                                            size_t *count, const TeX_t **lower, const TeX_t **upper)
{
    if (!TeX_builder_ready(builder))
        return NULL;
    if ((unsigned)limits > TeX_LIMIT_BOTH) {
        TeX_builder_fail(builder, EINVAL);
        return NULL;
    }
    bool has_lower = limits & TeX_LIMIT_LOWER, has_upper = limits & TeX_LIMIT_UPPER;
    *count = base + has_lower + has_upper;
    const TeX_t *const *p = TeX_operands(builder, *count);
    if (p) {
        *lower = has_lower ? p[base] : NULL;
        *upper = has_upper ? p[base + has_lower] : NULL;
    }
    return p;
}

/* Compose optional sum bounds in the same lower-then-upper order as their pushes. */
bool TeX_sum(TeX_builder_t *builder, TeX_limits_t limits)
{
    size_t count;
    const TeX_t *lower, *upper;
    const TeX_t *const *p = TeX_limit_operands(builder, 1, limits, &count, &lower, &upper);
    return p && TeX_replace(builder, count, TeX_classify(TeX_compose_sum(p[0], lower, upper), TeX_NODE_SUMMATION));
}

/* Consume the body and integration variable before optional bounds. */
bool TeX_integral(TeX_builder_t *builder, TeX_limits_t limits)
{
    size_t count;
    const TeX_t *lower, *upper;
    const TeX_t *const *p = TeX_limit_operands(builder, 2, limits, &count, &lower, &upper);
    return p && TeX_replace(builder, count, TeX_classify(TeX_compose_integral(p[0], p[1], lower, upper), TeX_NODE_INTEGRAL));
}

/* Supply derivative presentation without asking the builder to differentiate. */
bool TeX_derivative(TeX_builder_t *builder, unsigned order, TeX_derivative_style_t style)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_derivative(p[0], p[1], order, style), TeX_NODE_DERIVATIVE));
}

/* Pass a directly indexed row-major tail to matrix layout after overflow validation. */
bool TeX_matrix(TeX_builder_t *builder, size_t rows, size_t columns,
                      TeX_matrix_style_t style, unsigned row_spacing)
{
    if (!TeX_builder_ready(builder))
        return false;
    if (!rows || !columns)
        return TeX_builder_fail(builder, EINVAL);
    if (rows > SIZE_MAX / columns)
        return TeX_builder_fail(builder, EOVERFLOW);
    size_t count = rows * columns;
    const TeX_t *const *p = TeX_operands(builder, count);
    return p && TeX_replace(builder, count,
                           TeX_classify(TeX_compose_matrix(p, rows, columns, style, row_spacing), TeX_NODE_MATRIX));
}

/* Wrap a body, optionally followed by its native binding list. */
bool TeX_bound(TeX_builder_t *builder, bool bindings)
{
    size_t count = bindings ? 2 : 1;
    const TeX_t *const *p = TeX_operands(builder, count);
    return p && TeX_replace(builder, count, TeX_classify(TeX_compose_bound(p[0], bindings ? p[1] : NULL), TeX_NODE_BOUND));
}

/* Consume a scalar followed by the body it scales. */
bool TeX_scaled(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_scaled(p[0], p[1]), TeX_NODE_PRODUCT));
}

/* Consume real and imaginary components without numerical interpretation. */
bool TeX_cartesian_sum(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_compose_cartesian_sum(p[0], p[1]));
}

/* Consume a common centre, offset and optional denominator already selected by mathematics. */
bool TeX_symmetric_pair(TeX_builder_t *builder, bool denominator, bool group_offset)
{
    size_t count = denominator ? 3 : 2;
    const TeX_t *const *p = TeX_operands(builder, count);
    return p && TeX_replace(builder, count, TeX_compose_symmetric_pair(p[0], p[1], denominator ? p[2] : NULL, group_offset));
}
