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
#include <limits.h>
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
bool TeX_raw(TeX_builder_t *builder, const char *source)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_compose_raw(source));
}

/* Push a mathematical name without making its caller write or parse TeX commands. */
bool TeX_symbol(TeX_builder_t *builder, const char *name)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_compose_symbol(name));
}

/* Escape literal prose and place its owned copy on the construction stack. */
bool TeX_text(TeX_builder_t *builder, const string_t *text)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_compose_text(text));
}

/* Delegate mathematical interpretation to the established native input adapters. */
bool TeX_expression(TeX_builder_t *builder, const string_t *text)
{
    if (!TeX_builder_ready(builder))
        return false;
    TeX_t *notation = TeX_from_expression(text);
    return notation ? TeX_replace(builder, 0, notation) : TeX_builder_fail(builder, errno ? errno : EINVAL);
}

/* Adapt a convenient C string without exposing temporary string ownership. */
bool TeX_math(TeX_builder_t *builder, const char *text)
{
    if (!TeX_builder_ready(builder))
        return false;
    if (!text)
        return TeX_builder_fail(builder, EINVAL);
    string_t *input = string_new_with(text);
    bool ok = input ? TeX_expression(builder, input) : TeX_builder_fail(builder, ENOMEM);
    string_free(input);
    return ok;
}

/* Supply integer notation without requiring callers to create a source string. */
bool TeX_integer(TeX_builder_t *builder, long value)
{
    return TeX_builder_ready(builder) && TeX_replace(builder, 0, TeX_compose_integer(value));
}

/* Copy a finished or stack-owned value; the original remains with its owner. */
bool TeX_notation(TeX_builder_t *builder, const TeX_t *notation)
{
    if (!TeX_builder_ready(builder))
        return false;
    return notation ? TeX_replace(builder, 0, TeX_copy(notation)) : TeX_builder_fail(builder, EINVAL);
}

/* Select fixed spacing syntax by direct enum indexing. */
bool TeX_space(TeX_builder_t *builder, TeX_space_t space)
{
    static const char *const source[] = {"\\,", "\\qquad "};
    if (!TeX_builder_ready(builder))
        return false;
    if ((unsigned)space >= sizeof source / sizeof *source)
        return TeX_builder_fail(builder, EINVAL);
    return TeX_raw(builder, source[space]);
}

/* Duplicate level one with independently owned notation and literal metadata. */
bool TeX_dup(TeX_builder_t *builder)
{
    if (!TeX_builder_ready(builder))
        return false;
    const TeX_t *value = TeX_builder_peek(builder, 1);
    return value ? TeX_notation(builder, value) : TeX_builder_fail(builder, ERANGE);
}

/* Release the validated top operand through the array's notation destructor. */
bool TeX_drop(TeX_builder_t *builder)
{
    if (!TeX_operands(builder, 1))
        return false;
    array_remove(builder->values, array_size(builder->values) - 1);
    return true;
}

/* Exchange the top two owned pointers while preserving their notation and metadata. */
bool TeX_swap(TeX_builder_t *builder)
{
    if (!TeX_operands(builder, 2))
        return false;
    TeX_t **entries = array_get(builder->values, array_size(builder->values) - 2);
    TeX_t *first = entries[0];
    entries[0] = entries[1];
    entries[1] = first;
    return true;
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

static bool TeX_control(TeX_builder_t *builder, const TeX_t *value, unsigned long maximum,
                       bool positive, unsigned long *control)
{
    long integer;
    if (!TeX_integer_value(value, &integer) || integer < 0 || (positive && !integer))
        return TeX_builder_fail(builder, EINVAL);
    if ((unsigned long)integer > maximum)
        return TeX_builder_fail(builder, EOVERFLOW);
    *control = (unsigned long)integer;
    return true;
}

static const TeX_t *const *TeX_list_members(TeX_builder_t *builder, const TeX_t *value, size_t *count)
{
    static const TeX_t *const empty[] = {NULL};
    if (TeX_object_kind(value) != TeX_OBJECT_LIST) {
        TeX_builder_fail(builder, EINVAL);
        return NULL;
    }
    const TeX_t *const *items = TeX_object_items(value, count);
    return *count ? items : empty;
}

/* HP48 ->LIST consumes its literal count and replaces that many preceding objects. */
bool TeX_list(TeX_builder_t *builder)
{
    const TeX_t *const *top = TeX_operands(builder, 1);
    unsigned long count;
    if (!top || !TeX_control(builder, top[0], SIZE_MAX - 1, false, &count))
        return false;
    const TeX_t *const *items = TeX_operands(builder, (size_t)count + 1);
    return items && TeX_replace(builder, (size_t)count + 1,
                               TeX_object_new(TeX_OBJECT_LIST, items, (size_t)count));
}

/* Expand a list and push its count, constructing the replacement stack before mutation. */
bool TeX_unlist(TeX_builder_t *builder)
{
    const TeX_t *const *top = TeX_operands(builder, 1);
    size_t count;
    const TeX_t *const *items = top ? TeX_list_members(builder, top[0], &count) : NULL;
    if (!items)
        return false;
    if (count > LONG_MAX)
        return TeX_builder_fail(builder, EOVERFLOW);
    array_t *replacement = array_create(sizeof(TeX_t *), NULL, TeX_stack_destroy);
    if (!replacement)
        return TeX_builder_fail(builder, ENOMEM);
    size_t previous = array_size(builder->values) - 1;
    if (previous && !array_append_carray(replacement, array_get(builder->values, 0), previous)) {
        array_destroy(replacement);
        return TeX_builder_fail(builder, ENOMEM);
    }
    for (size_t i = 0; i <= count; ++i) {
        TeX_t *copy = i < count ? TeX_copy(items[i]) : TeX_compose_integer((long)count);
        if (!copy || !array_append_carray(replacement, &copy, 1)) {
            TeX_free(copy);
            /* Lower pointers still belong to the unchanged original stack. */
            for (size_t j = 0; j < previous; ++j)
                *(TeX_t **)array_get(replacement, j) = NULL;
            array_destroy(replacement);
            return TeX_builder_fail(builder, errno ? errno : ENOMEM);
        }
    }
    /* Transfer surviving operands without changing their identity or cloning them. */
    for (size_t i = 0; i < previous; ++i)
        *(TeX_t **)array_get(builder->values, i) = NULL;
    array_destroy(builder->values);
    builder->values = replacement;
    return true;
}

/* Push HP48 DEPTH as a literal control, leaving all previous objects in place. */
bool TeX_depth(TeX_builder_t *builder)
{
    if (!TeX_builder_ready(builder))
        return false;
    size_t count = array_size(builder->values);
    if (count > LONG_MAX)
        return TeX_builder_fail(builder, EOVERFLOW);
    return TeX_integer(builder, (long)count);
}

/* Mark an upper index as a single stack operand, including its original label data. */
bool TeX_upper(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_object_index(p[0], true));
}

/* Mark a lower index without requiring a separate positions array. */
bool TeX_lower(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_object_index(p[0], false));
}

/* Mark an index operand without consuming its tensor base or evaluating a derivative. */
bool TeX_comma(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_object_derivative_index(p[0], false));
}

/* Preserve a covariant-derivative index group as one independently owned operand. */
bool TeX_semicolon(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_object_derivative_index(p[0], true));
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
bool TeX_function(TeX_builder_t *builder, const char *name)
{
    const TeX_t *const *top = TeX_operands(builder, 1);
    size_t count;
    const TeX_t *const *p = top ? TeX_list_members(builder, top[0], &count) : NULL;
    if (!p)
        return false;
    TeX_t *validated = TeX_compose_symbol(name);
    string_t *input = validated ? string_new_with(name) : NULL;
    TeX_t *result = input ? TeX_named_call(input, p, count) : NULL;
    TeX_free(validated);
    string_free(input);
    return TeX_replace(builder, 1, result);
}

/* Consume function notation followed by arguments, retaining their push order. */
bool TeX_apply(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    size_t count;
    const TeX_t *const *arguments = p ? TeX_list_members(builder, p[1], &count) : NULL;
    return arguments && TeX_replace(builder, 2, TeX_applied(p[0], arguments, count));
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

/* Mark a complete operand or index label with primes without differentiating it. */
bool TeX_prime(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    unsigned long order;
    return p && TeX_control(builder, p[1], UINT_MAX, true, &order) &&
           TeX_replace(builder, 2, TeX_arithmetic_prime(p[0], (unsigned)order));
}

/* Decorate the top operand without treating the bar as a mathematical operation. */
bool TeX_bar(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_classify(TeX_compose_bar(p[0]), TeX_NODE_FENCED));
}

/* Cover the complete top operand with an operator hat. */
bool TeX_hat(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1u);
    return p && TeX_replace(builder, 1u, TeX_compose_quantum(TeX_NODE_HAT, p, 1u));
}

/* Mark the complete top operand as an adjoint without changing its mathematics. */
bool TeX_dagger(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1u);
    return p && TeX_replace(builder, 1u, TeX_arithmetic_dagger(p[0]));
}

/* Enclose one state label as a bra. */
bool TeX_bra(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1u);
    return p && TeX_replace(builder, 1u, TeX_compose_quantum(TeX_NODE_BRA, p, 1u));
}

/* Enclose one state label as a ket. */
bool TeX_ket(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1u);
    return p && TeX_replace(builder, 1u, TeX_compose_quantum(TeX_NODE_KET, p, 1u));
}

/* Construct an inner product from left and right state labels. */
bool TeX_braket(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2u);
    return p && TeX_replace(builder, 2u, TeX_compose_quantum(TeX_NODE_BRAKET, p, 2u));
}

/* Construct an outer product from ket and bra labels. */
bool TeX_ketbra(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2u);
    return p && TeX_replace(builder, 2u, TeX_compose_quantum(TeX_NODE_KETBRA, p, 2u));
}

/* Retain the operator between the left and right state labels. */
bool TeX_matrix_element(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 3u);
    return p && TeX_replace(builder, 3u, TeX_compose_quantum(TeX_NODE_MATRIX_ELEMENT, p, 3u));
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
bool TeX_sequence(TeX_builder_t *builder)
{
    const TeX_t *const *top = TeX_operands(builder, 1);
    size_t count;
    const TeX_t *const *p = top ? TeX_list_members(builder, top[0], &count) : NULL;
    return p && TeX_replace(builder, 1, TeX_compose_sequence(p, count));
}

/* Add rows without requiring callers to know a TeX environment or line-break command. */
bool TeX_newline(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_object_newline(p[0], p[1]));
}

/* Keep indentation magnitude on the stack alongside the body it affects. */
bool TeX_indent(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    unsigned long spacing;
    return p && TeX_control(builder, p[1], UINT_MAX, false, &spacing) &&
           TeX_replace(builder, 2, TeX_object_indent(p[0], (unsigned)spacing));
}

/* Align typed relations or explicit column lists using their retained structure. */
bool TeX_align(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    if (!p)
        return false;
    TeX_object_kind_t kind = TeX_object_kind(p[0]);
    if (kind != TeX_OBJECT_LIST && kind != TeX_OBJECT_LINES && kind != TeX_OBJECT_ALIGNED)
        return TeX_builder_fail(builder, EINVAL);
    size_t count;
    const TeX_t *const *rows = TeX_object_items(p[0], &count);
    return TeX_replace(builder, 1, TeX_object_lines(rows, count, true));
}

/* Preserve the native equation renderer's wrapped-side layout behind its private interface. */
bool TeX_builder_aligned_equation(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2,
        TeX_classify(TeX_compose_equation(p[0], p[1], TeX_EQUATION_ALIGNED), TeX_NODE_EQUATION));
}

static bool TeX_relation(TeX_builder_t *builder, TeX_relation_t relation)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_object_relation(p[0], p[1], relation));
}

/* Join the two top operands by an equality sign. */
bool TeX_eq(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_EQUAL);
}

/* Join the two top operands by a not-equal sign. */
bool TeX_ne(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_NOT_EQUAL);
}

/* Join the two top operands by a strictly-less-than sign. */
bool TeX_lt(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_LESS);
}

/* Join the two top operands by a less-than-or-equal sign. */
bool TeX_le(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_LESS_EQUAL);
}

/* Join the two top operands by a strictly-greater-than sign. */
bool TeX_gt(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_GREATER);
}

/* Join the two top operands by a greater-than-or-equal sign. */
bool TeX_ge(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_GREATER_EQUAL);
}

/* Join the two top operands by approximate equality without selecting a tolerance. */
bool TeX_approx(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_APPROXIMATE);
}

/* Join the two top operands by congruence without comparing their values. */
bool TeX_cong(TeX_builder_t *builder)
{
    return TeX_relation(builder, TeX_RELATION_CONGRUENT);
}

/* Apply visible delimiters without interpreting the enclosed notation. */
bool TeX_fence(TeX_builder_t *builder, TeX_fence_t fence)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_classify(TeX_compose_fenced(p[0], fence), TeX_NODE_FENCED));
}

/* Consume a base and ordered tensor slots, validating variance before mutation. */
bool TeX_tensor(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    return p && TeX_replace(builder, 2, TeX_compose_tensor_indices(p[0], p[1]));
}

static const TeX_t *const *TeX_limit_operands(TeX_builder_t *builder, size_t base,
                                            const TeX_t **lower, const TeX_t **upper)
{
    const TeX_t *const *p = TeX_operands(builder, base + 1);
    size_t count;
    const TeX_t *const *bounds = p ? TeX_list_members(builder, p[base], &count) : NULL;
    if (!bounds)
        return NULL;
    if (count > 2) {
        TeX_builder_fail(builder, EINVAL);
        return NULL;
    }
    *lower = *upper = NULL;
    for (size_t i = 0; i < count; ++i) {
        TeX_object_kind_t kind = TeX_object_kind(bounds[i]);
        size_t n;
        const TeX_t *const *label = TeX_object_items(bounds[i], &n);
        bool is_upper = kind == TeX_OBJECT_UPPER || (kind != TeX_OBJECT_LOWER && i == 1);
        const TeX_t **slot = is_upper ? upper : lower;
        if (*slot) {
            TeX_builder_fail(builder, EINVAL);
            return NULL;
        }
        *slot = kind == TeX_OBJECT_UPPER || kind == TeX_OBJECT_LOWER ? label[0] : bounds[i];
    }
    return p;
}

/* Compose optional sum bounds in the same lower-then-upper order as their pushes. */
bool TeX_sum(TeX_builder_t *builder)
{
    const TeX_t *lower, *upper;
    const TeX_t *const *p = TeX_limit_operands(builder, 1, &lower, &upper);
    return p && TeX_replace(builder, 2, TeX_classify(TeX_compose_sum(p[0], lower, upper), TeX_NODE_SUMMATION));
}

/* Compose optional product bounds after its mathematical body. */
bool TeX_prod(TeX_builder_t *builder)
{
    const TeX_t *lower, *upper;
    const TeX_t *const *p = TeX_limit_operands(builder, 1, &lower, &upper);
    return p && TeX_replace(builder, 2,
                           TeX_classify(TeX_compose_product(p[0], lower, upper), TeX_NODE_INDEXED_PRODUCT));
}

/* Consume the body and explicit source and target variables without evaluating the transform. */
bool TeX_transform(TeX_builder_t *builder, TeX_transform_kind_t kind)
{
    const TeX_t *const *p = TeX_operands(builder, 3);
    return p && TeX_replace(builder, 3, TeX_classify(TeX_compose_transform(p, kind), TeX_NODE_TRANSFORM));
}

/* Consume the body and integration variable before optional bounds. */
bool TeX_integral(TeX_builder_t *builder)
{
    const TeX_t *lower, *upper;
    const TeX_t *const *p = TeX_limit_operands(builder, 2, &lower, &upper);
    return p && TeX_replace(builder, 3, TeX_classify(TeX_compose_integral(p[0], p[1], lower, upper), TeX_NODE_INTEGRAL));
}

/* Supply derivative presentation without asking the builder to differentiate. */
bool TeX_derivative(TeX_builder_t *builder, TeX_derivative_style_t style)
{
    const TeX_t *const *p = TeX_operands(builder, 3);
    unsigned long order;
    return p && TeX_control(builder, p[2], UINT_MAX, true, &order) &&
           TeX_replace(builder, 3, TeX_classify(TeX_compose_derivative(p[0], p[1], (unsigned)order, style),
                                               TeX_NODE_DERIVATIVE));
}

/* Consume ordered coordinate variables and retain the native mixed-derivative presentation. */
bool TeX_partial(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    size_t count;
    const TeX_t *const *variables = p ? TeX_list_members(builder, p[1], &count) : NULL;
    if (!variables)
        return false;
    if (!count)
        return TeX_builder_fail(builder, EINVAL);
    if (count > UINT_MAX || count >= SIZE_MAX / sizeof(TeX_t *))
        return TeX_builder_fail(builder, EOVERFLOW);
    const TeX_t **operands = malloc((count + 1) * sizeof *operands);
    if (!operands)
        return TeX_builder_fail(builder, ENOMEM);
    operands[0] = p[0];
    memcpy(operands + 1, variables, count * sizeof *operands);
    TeX_t *result = TeX_compose_partial(operands, count);
    free(operands);
    return TeX_replace(builder, 2, TeX_classify(result, TeX_NODE_DERIVATIVE));
}

/* Pass a directly indexed row-major tail to matrix layout after overflow validation. */
bool TeX_matrix(TeX_builder_t *builder)
{
    const TeX_t *const *top = TeX_operands(builder, 1);
    size_t count;
    const TeX_t *const *items = top ? TeX_list_members(builder, top[0], &count) : NULL;
    if (!items)
        return false;
    if (count && TeX_object_kind(items[0]) == TeX_OBJECT_LIST)
        return TeX_replace(builder, 1, TeX_object_matrix(top[0], TeX_MATRIX_BRACKETS, 0));
    unsigned long rows, columns;
    if (count != 2)
        return TeX_builder_fail(builder, EINVAL);
    if (!TeX_control(builder, items[0], SIZE_MAX, true, &rows) ||
        !TeX_control(builder, items[1], SIZE_MAX, true, &columns))
        return false;
    if (rows > (SIZE_MAX - 1) / columns || rows > SIZE_MAX / sizeof(TeX_t *))
        return TeX_builder_fail(builder, EOVERFLOW);
    size_t cells = (size_t)rows * columns;
    const TeX_t *const *p = TeX_operands(builder, cells + 1);
    if (!p)
        return false;
    TeX_t **row = calloc(rows, sizeof *row);
    if (!row)
        return TeX_builder_fail(builder, ENOMEM);
    for (size_t r = 0; r < rows; ++r) {
        row[r] = TeX_object_new(TeX_OBJECT_LIST, p + r * columns, columns);
        if (!row[r]) {
            for (size_t i = 0; i < r; ++i)
                TeX_free(row[i]);
            free(row);
            return TeX_builder_fail(builder, errno ? errno : ENOMEM);
        }
    }
    TeX_t *row_list = TeX_object_new(TeX_OBJECT_LIST, (const TeX_t *const *)row, rows);
    for (size_t r = 0; r < rows; ++r)
        TeX_free(row[r]);
    free(row);
    TeX_t *result = row_list ? TeX_object_matrix(row_list, TeX_MATRIX_BRACKETS, 0) : NULL;
    TeX_free(row_list);
    return TeX_replace(builder, cells + 1, result);
}

/* Change matrix delimiters while preserving retained cell structure and spacing. */
bool TeX_matrix_style(TeX_builder_t *builder, TeX_matrix_style_t style)
{
    const TeX_t *const *p = TeX_operands(builder, 1);
    return p && TeX_replace(builder, 1, TeX_object_matrix_format(p[0], style, 0, true));
}

/* Consume matrix then spacing, keeping layout numbers on the operand stack too. */
bool TeX_matrix_spacing(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    unsigned long spacing;
    return p && TeX_control(builder, p[1], UINT_MAX, false, &spacing) &&
           TeX_replace(builder, 2, TeX_object_matrix_format(p[0], TeX_MATRIX_BRACKETS, (unsigned)spacing, false));
}

/* Wrap a body, optionally followed by its native binding list. */
bool TeX_bound(TeX_builder_t *builder)
{
    const TeX_t *const *p = TeX_operands(builder, 2);
    size_t count;
    const TeX_t *const *items = p ? TeX_list_members(builder, p[1], &count) : NULL;
    if (!items)
        return false;
    TeX_t *bindings = count ? TeX_compose_sequence(items, count) : NULL;
    if (count && !bindings)
        return TeX_builder_fail(builder, errno ? errno : ENOMEM);
    TeX_t *result = TeX_compose_bound(p[0], bindings);
    TeX_free(bindings);
    return TeX_replace(builder, 2, TeX_classify(result, TeX_NODE_BOUND));
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
bool TeX_symmetric_pair(TeX_builder_t *builder, bool group_offset)
{
    const TeX_t *const *p = TeX_operands(builder, 3);
    size_t count;
    const TeX_t *const *denominator = p ? TeX_list_members(builder, p[2], &count) : NULL;
    if (!denominator)
        return false;
    if (count > 1)
        return TeX_builder_fail(builder, EINVAL);
    return TeX_replace(builder, 3, TeX_compose_symmetric_pair(p[0], p[1], count ? denominator[0] : NULL, group_offset));
}
