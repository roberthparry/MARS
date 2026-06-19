#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"

static void dump_step(const expr_t *expr, expr_t *var, expr_t *zero, expr_t *one)
{
    expr_t *anti = expr_integrate(expr, var);
    expr_t *upper = NULL;
    expr_t *lower = NULL;
    expr_t *diff = NULL;
    expr_t *next = NULL;
    char *anti_text = NULL;
    char *next_text = NULL;

    if (!anti) {
        const char *name = expr_symbol_name(var);
        printf("%s: no antiderivative\n", name ? name : "?");
        return;
    }

    upper = expr_substitute(anti, var, one);
    lower = expr_substitute(anti, var, zero);
    diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    next = diff ? expr_simplify(diff) : NULL;
    anti_text = expr_to_string(anti, style_UNBOUND);
    next_text = next ? expr_to_string(next, style_UNBOUND) : NULL;

    {
        const char *name = expr_symbol_name(var);

        printf("%s anti: %s\n", name ? name : "?", anti_text ? anti_text : "(null)");
        printf("%s step: %s\n", name ? name : "?", next_text ? next_text : "(null)");
    }

    free(next_text);
    free(anti_text);
    expr_free(next);
    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
}

static expr_t *step_expr(const expr_t *expr, expr_t *var, expr_t *zero, expr_t *one)
{
    expr_t *anti = expr_integrate(expr, var);
    expr_t *upper = NULL;
    expr_t *lower = NULL;
    expr_t *diff = NULL;
    expr_t *next = NULL;

    if (!anti)
        return NULL;

    upper = expr_substitute(anti, var, one);
    lower = expr_substitute(anti, var, zero);
    diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
    next = diff ? expr_simplify(diff) : NULL;

    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    expr_free(anti);
    return next;
}

int main(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *x = NULL;
    expr_t *y = NULL;
    expr_t *z = NULL;
    expr_t *bx = NULL;
    expr_t *by = NULL;
    expr_t *bz = NULL;
    expr_t *zero = NULL;
    expr_t *one = NULL;
    number_t seed = num_new();

    expr = expr_from_string("{ exp(-x^2*y*z) }", &bindings);
    x = expr_new_named_var(seed, "x");
    y = expr_new_named_var(seed, "y");
    z = expr_new_named_var(seed, "z");
    bx = bindings ? expr_bindings_get(bindings, "x") : NULL;
    by = bindings ? expr_bindings_get(bindings, "y") : NULL;
    bz = bindings ? expr_bindings_get(bindings, "z") : NULL;
    zero = expr_new_const(NUM_ZERO);
    one = expr_new_const(NUM_ONE);

    printf("bindings x=%p y=%p z=%p\n", (void *)bx, (void *)by, (void *)bz);
    if (bx) dump_step(expr, bx, zero, one);
    if (by) dump_step(expr, by, zero, one);
    if (bz) dump_step(expr, bz, zero, one);
    if (bz && by) {
        expr_t *after_z = step_expr(expr, bz, zero, one);
        char *after_z_text = after_z ? expr_to_string(after_z, style_UNBOUND) : NULL;

        printf("after z: %s\n", after_z_text ? after_z_text : "(null)");
        free(after_z_text);
        if (after_z) {
            dump_step(after_z, by, zero, one);
            expr_free(after_z);
        }
    }
    if (by && bz) {
        expr_t *after_y = step_expr(expr, by, zero, one);
        char *after_y_text = after_y ? expr_to_string(after_y, style_UNBOUND) : NULL;

        printf("after y: %s\n", after_y_text ? after_y_text : "(null)");
        free(after_y_text);
        if (after_y) {
            dump_step(after_y, bz, zero, one);
            expr_free(after_y);
        }
    }
    printf("-- synthetic --\n");
    dump_step(expr, x, zero, one);
    dump_step(expr, y, zero, one);
    dump_step(expr, z, zero, one);

    expr_free(one);
    expr_free(zero);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    expr_free(expr);
    expr_bindings_free(bindings);
    num_destroy(&seed);
    return 0;
}
