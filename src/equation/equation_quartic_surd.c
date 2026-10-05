#include <stdbool.h>
#include <stddef.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* A fixed-size construction arena owns intermediate references, not the returned solutions. */
typedef struct {
    expr_t *nodes[96];
    size_t count;
    bool failed;
} quartic_radicals_t;

static expr_t *keep(quartic_radicals_t *work, expr_t *node)
{
    if (!node || work->count == sizeof(work->nodes) / sizeof(work->nodes[0])) {
        expr_free(node);
        work->failed = true;
        return NULL;
    }
    work->nodes[work->count++] = node;
    return node;
}

static expr_t *add(quartic_radicals_t *work, const expr_t *a, const expr_t *b)
{
    return keep(work, expr_add(a, b));
}

static expr_t *sub(quartic_radicals_t *work, const expr_t *a, const expr_t *b)
{
    return keep(work, expr_sub(a, b));
}

static expr_t *mul(quartic_radicals_t *work, const expr_t *a, const expr_t *b)
{
    return keep(work, expr_mul(a, b));
}

static expr_t *scale(quartic_radicals_t *work, const expr_t *a, long factor)
{
    return keep(work, expr_mul_long(a, factor));
}

static expr_t *divide(quartic_radicals_t *work, const expr_t *a, long divisor)
{
    return keep(work, expr_div_long(a, divisor));
}

static expr_t *power(quartic_radicals_t *work, const expr_t *a, long exponent)
{
    return keep(work, expr_pow_long(a, exponent));
}

static expr_t *constant_expression(quartic_radicals_t *work, const expr_t *a)
{
    /* Only rational coefficient arithmetic reaches this helper; radicals stay as expression nodes. */
    number_t value = expr_eval(a);
    expr_t *out = num_is_finite(value) && num_is_exact(value) ? keep(work, expr_new_const(value)) : NULL;

    num_destroy(&value);
    return out;
}

static void clear(quartic_radicals_t *work)
{
    while (work->count)
        expr_free(work->nodes[--work->count]);
}

/* General Ferrari formula for exact real coefficients, after the shorter rational-factor route. */
int equ_try_quartic_radicals(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions)
{
    quartic_radicals_t work = {0};
    expr_t *monic[4] = {0};
    equation_solutions_t exact = {0};
    int rc = 1;

    for (size_t i = 0u; i < 5u; ++i)
        if (!num_is_finite(coeffs[i]) || !num_is_exact(coeffs[i]) || !num_is_real(coeffs[i]))
            return 1;
    for (size_t i = 0u; i < 4u; ++i) {
        number_t value = num_div(coeffs[i], coeffs[4]);
        monic[i] = keep(&work, expr_new_const(value));
        num_destroy(&value);
    }
    expr_t *b = monic[3], *c = monic[2], *d = monic[1], *e = monic[0];
    expr_t *b2 = power(&work, b, 2L);
    expr_t *p = constant_expression(&work, sub(&work, c, divide(&work, scale(&work, b2, 3L), 8L)));
    expr_t *q = constant_expression(&work, add(&work, d,
        sub(&work, divide(&work, power(&work, b, 3L), 8L), divide(&work, mul(&work, b, c), 2L))));
    expr_t *delta0 = constant_expression(&work, add(&work,
        sub(&work, power(&work, c, 2L), scale(&work, mul(&work, b, d), 3L)), scale(&work, e, 12L)));
    expr_t *delta1 = constant_expression(&work, sub(&work,
        add(&work, add(&work, scale(&work, power(&work, c, 3L), 2L), scale(&work, mul(&work, b2, e), 27L)),
                   scale(&work, power(&work, d, 2L), 27L)),
        add(&work, scale(&work, mul(&work, mul(&work, b, c), d), 9L), scale(&work, mul(&work, c, e), 72L))));
    expr_t *discriminant = constant_expression(&work,
        sub(&work, power(&work, delta1, 2L), scale(&work, power(&work, delta0, 3L), 4L)));
    if (!p || !q || !delta0 || !delta1 || !discriminant || work.failed)
        goto cleanup;

    /* Degenerate cases retain the existing biquadratic/repeated-root handling. */
    number_t q_value = expr_eval(q);
    number_t discriminant_value = expr_eval(discriminant);
    bool degenerate = num_is_zero(q_value) || num_is_zero(discriminant_value);
    num_destroy(&discriminant_value);
    num_destroy(&q_value);
    if (degenerate)
        goto cleanup;

    expr_t *radical = keep(&work, expr_sqrt(discriminant));
    expr_t *argument = divide(&work, add(&work, delta1, radical), 2L);
    number_t argument_value = expr_eval(argument);
    bool zero_argument = num_is_zero(argument_value);
    num_destroy(&argument_value);
    if (zero_argument)
        argument = divide(&work, sub(&work, delta1, radical), 2L);
    /* Pair Q with delta0/Q: independently chosen cube-root branches are not interchangeable. */
    number_t third = num_create_from_frac(1L, 3L);
    expr_t *big_q = keep(&work, expr_pow(argument, &third));
    num_destroy(&third);
    expr_t *z = constant_expression(&work, divide(&work, scale(&work, p, -2L), 3L));
    expr_t *squared = add(&work, z, divide(&work,
        add(&work, big_q, keep(&work, expr_div(delta0, big_q))), 3L));
    expr_t *s = divide(&work, keep(&work, expr_sqrt(squared)), 2L);
    expr_t *shift = constant_expression(&work, divide(&work, scale(&work, b, -1L), 4L));
    expr_t *base = sub(&work, scale(&work, power(&work, s, 2L), -4L), scale(&work, p, 2L));
    expr_t *q_over_s = keep(&work, expr_div(q, s));
    if (work.failed)
        goto cleanup;
    for (size_t pair = 0u; pair < 2u; ++pair) {
        expr_t *centre = pair ? add(&work, shift, s) : sub(&work, shift, s);
        expr_t *radicand = pair ? sub(&work, base, q_over_s) : add(&work, base, q_over_s);
        expr_t *offset = divide(&work, keep(&work, expr_sqrt(radicand)), 2L);
        for (size_t sign = 0u; sign < 2u; ++sign) {
            expr_t *raw = sign ? expr_sub(centre, offset) : expr_add(centre, offset);
            expr_t *root = expr_simplify_owned(raw);
            int appended = root ? equ_append_solution_expr(wrt, root, &exact) : -1;
            expr_free(root);
            if (appended != 0) {
                rc = -1;
                goto cleanup;
            }
        }
    }
    if (work.failed)
        goto cleanup;
    for (size_t i = 0u; i < exact.count; ++i) {
        if (equ_append_solution_expr(wrt, equ_rhs(exact.solutions[i]), solutions) != 0) {
            rc = -1;
            goto cleanup;
        }
    }
    rc = 0;

cleanup:
    if (work.failed)
        rc = -1;
    equ_solutions_clear(&exact);
    clear(&work);
    return rc;
}
