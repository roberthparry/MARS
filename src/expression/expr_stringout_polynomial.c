/* Polynomial, series and transform term ordering. */

#include <ctype.h>
#include <gmp.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#include "expression.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

#define DISPLAY_POLY_MAX_VARS 16u

typedef struct {
    const expr_t *expr;
    bool subtract;
    long degree[DISPLAY_POLY_MAX_VARS];
} display_poly_term_t;

typedef struct {
    const char *name[DISPLAY_POLY_MAX_VARS];
    size_t count;
} display_poly_var_list_t;

static bool display_poly_vars_add(display_poly_var_list_t *vars, const char *name)
{
    size_t pos;

    if (!vars || !name || !*name || !expr_tostring_is_primary_variable_name(name))
        return true;

    for (size_t i = 0u; i < vars->count; ++i) {
        int cmp = strcmp(vars->name[i], name);

        if (cmp == 0)
            return true;
        if (cmp > 0)
            break;
    }

    if (vars->count >= DISPLAY_POLY_MAX_VARS)
        return false;

    pos = vars->count;
    while (pos > 0u && strcmp(vars->name[pos - 1u], name) > 0) {
        vars->name[pos] = vars->name[pos - 1u];
        --pos;
    }
    vars->name[pos] = name;
    ++vars->count;
    return true;
}

static bool display_poly_collect_vars(const expr_t *expr, display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return true;

    if (expr_is_var(expr) && expr->name)
        return display_poly_vars_add(vars, expr->name);

    return display_poly_collect_vars(expr->a, vars) && display_poly_collect_vars(expr->b, vars);
}

static int display_poly_var_index(const display_poly_var_list_t *vars, const char *name)
{
    if (!vars || !name)
        return -1;
    for (size_t i = 0u; i < vars->count; ++i) {
        if (strcmp(vars->name[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static bool display_poly_expr_contains_any_var(const expr_t *expr, const display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return false;
    if (expr_is_var(expr) && expr->name && display_poly_var_index(vars, expr->name) >= 0)
        return true;
    return display_poly_expr_contains_any_var(expr->a, vars) || display_poly_expr_contains_any_var(expr->b, vars);
}

static bool display_poly_expr_contains_imaginary_unit(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_const(expr) && expr->name && strcmp(expr->name, "i") == 0)
        return true;
    return display_poly_expr_contains_imaginary_unit(expr->a) || display_poly_expr_contains_imaginary_unit(expr->b);
}

static bool display_poly_is_pure_imag_const(const expr_t *expr)
{
    number_t real;
    bool pure_imag;

    if (!expr || !expr_is_const(expr) || num_is_real(expr->c))
        return false;

    real = num_real_part(expr->c);
    pure_imag = num_eq(real, NUM_ZERO);
    num_destroy(&real);
    return pure_imag;
}

static bool display_poly_is_real_const(const expr_t *expr)
{
    return expr && expr_is_const(expr) && num_is_real(expr->c);
}

static bool display_poly_is_i_const(const expr_t *expr)
{
    return expr && expr_is_const(expr) && (num_eq(expr->c, NUM_I) || num_eq(expr->c, NUM_NEG_I));
}

static bool display_poly_is_imaginary_term(const expr_t *expr)
{
    if (!expr)
        return false;
    if (display_poly_is_i_const(expr) || display_poly_is_pure_imag_const(expr))
        return true;
    return expr_is_mul(expr) && ((display_poly_is_real_const(expr->a) && display_poly_is_i_const(expr->b)) ||
                                 (display_poly_is_i_const(expr->a) && display_poly_is_real_const(expr->b)));
}

static bool display_poly_is_negative_real_complex_const(const expr_t *expr)
{
    number_t real;
    bool negative_real;

    if (!expr || !expr_is_const(expr) || num_is_real(expr->c))
        return false;

    real = num_real_part(expr->c);
    negative_real = num_lt(real, NUM_ZERO);
    num_destroy(&real);
    return negative_real;
}

bool match_add_negative_complex_rhs(const expr_t *expr, const expr_t **base_out, const expr_t **complex_out)
{
    if (!expr || !expr_is_op(expr, &ops_add) || !expr->a || !display_poly_is_negative_real_complex_const(expr->b))
        return false;

    if (base_out)
        *base_out = expr->a;
    if (complex_out)
        *complex_out = expr->b;
    return true;
}

bool match_additive_complex_shift(const expr_t *expr, const expr_t **base_out, const expr_t **real_out,
                                         const expr_t **imag_out)
{
    if (!expr || !expr_is_op(expr, &ops_add) || !expr->a || !expr->b || !expr_is_op(expr->a, &ops_sub) || !expr->a->a ||
        !expr->a->b || !display_poly_is_real_const(expr->a->b) || !display_poly_is_imaginary_term(expr->b))
        return false;

    if (base_out)
        *base_out = expr->a->a;
    if (real_out)
        *real_out = expr->a->b;
    if (imag_out)
        *imag_out = expr->b;
    return true;
}

static bool display_poly_add_degree(long *degree, long add)
{
    if (!degree || add < 0 || *degree > LONG_MAX - add)
        return false;
    *degree += add;
    return true;
}

static bool display_poly_term_degrees(const expr_t *expr, const display_poly_var_list_t *vars, long *degree)
{
    number_t exponent = num_new();
    long power = 0;
    int index;
    bool ok = false;

    if (!expr || !vars || !degree)
        goto cleanup;

    if (expr_is_const(expr)) {
        ok = true;
        goto cleanup;
    }

    if (expr_is_var(expr)) {
        index = expr->name ? display_poly_var_index(vars, expr->name) : -1;
        ok = index < 0 || display_poly_add_degree(&degree[index], 1);
        goto cleanup;
    }

    if (expr_is_neg(expr)) {
        ok = display_poly_term_degrees(expr->a, vars, degree);
        goto cleanup;
    }

    if (expr_is_mul(expr)) {
        ok = display_poly_term_degrees(expr->a, vars, degree) && display_poly_term_degrees(expr->b, vars, degree);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_div)) {
        ok = !display_poly_expr_contains_any_var(expr->b, vars) && display_poly_term_degrees(expr->a, vars, degree);
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name) ? display_poly_var_index(vars, expr->a->name) : -1;
        if (index >= 0) {
            ok = expr_try_get_small_integer_exponent(expr->c, &power) && display_poly_add_degree(&degree[index], power);
        } else {
            ok = !display_poly_expr_contains_any_var(expr->a, vars);
        }
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_pow)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name) ? display_poly_var_index(vars, expr->a->name) : -1;
        if (index >= 0) {
            ok = expr_match_const_value(expr->b, &exponent) && expr_try_get_small_integer_exponent(exponent, &power) &&
                 display_poly_add_degree(&degree[index], power);
        } else {
            ok = !display_poly_expr_contains_any_var(expr, vars);
        }
        goto cleanup;
    }

    ok = !display_poly_expr_contains_any_var(expr, vars);

cleanup:
    num_destroy(&exponent);
    return ok;
}

static bool display_poly_collect_add_terms(const expr_t *expr, bool subtract, const display_poly_var_list_t *vars,
                                           display_poly_term_t *terms, size_t *count, size_t max_terms)
{
    if (!expr || !vars || !terms || !count)
        return false;

    if (display_poly_expr_contains_any_var(expr, vars) && expr_is_op(expr, &ops_add))
        return display_poly_collect_add_terms(expr->a, subtract, vars, terms, count, max_terms) &&
               display_poly_collect_add_terms(expr->b, subtract, vars, terms, count, max_terms);

    if (display_poly_expr_contains_any_var(expr, vars) && expr_is_op(expr, &ops_sub))
        return display_poly_collect_add_terms(expr->a, subtract, vars, terms, count, max_terms) &&
               display_poly_collect_add_terms(expr->b, !subtract, vars, terms, count, max_terms);

    if (*count >= max_terms)
        return false;

    terms[*count].expr = expr;
    terms[*count].subtract = subtract;
    memset(terms[*count].degree, 0, sizeof(terms[*count].degree));
    ++*count;
    return true;
}

static int display_poly_compare_terms(const display_poly_term_t *left, const display_poly_term_t *right,
                                      size_t var_count, bool ascending)
{
    for (size_t i = 0u; i < var_count; ++i) {
        if (left->degree[i] > right->degree[i])
            return ascending ? 1 : -1;
        if (left->degree[i] < right->degree[i])
            return ascending ? -1 : 1;
    }
    return 0;
}

static void display_poly_sort_terms(display_poly_term_t *terms, size_t count, size_t var_count, bool ascending)
{
    for (size_t i = 1u; i < count; ++i) {
        display_poly_term_t key = terms[i];
        size_t j = i;

        while (j > 0u && display_poly_compare_terms(&key, &terms[j - 1u], var_count, ascending) < 0) {
            terms[j] = terms[j - 1u];
            --j;
        }
        terms[j] = key;
    }
}

bool display_poly_is_indexed_arbitrary_constant(const expr_t *expr)
{
    const unsigned char *name;

    if (!expr || !expr_is_const(expr) || !expr->name || expr->name[0] != 'C')
        return false;

    name = (const unsigned char *)expr->name + 1u;
    if (!isdigit(*name) && !(strlen((const char *)name) >= 3u && name[0] == 0xe2u && name[1] == 0x82u &&
                             name[2] >= 0x80u && name[2] <= 0x89u))
        return false;
    while (*name) {
        if (isdigit(*name)) {
            ++name;
            continue;
        }
        if (strlen((const char *)name) >= 3u && name[0] == 0xe2u && name[1] == 0x82u && name[2] >= 0x80u &&
            name[2] <= 0x89u) {
            name += 3u;
            continue;
        }
        return false;
    }
    return *name == '\0';
}

bool display_poly_contains_indexed_arbitrary_constant(const expr_t *expr)
{
    if (!expr)
        return false;
    if (display_poly_is_indexed_arbitrary_constant(expr))
        return true;
    return display_poly_contains_indexed_arbitrary_constant(expr->a) ||
           display_poly_contains_indexed_arbitrary_constant(expr->b);
}

static bool display_poly_is_explicit_arbitrary_amplitude(const display_poly_term_t *terms, size_t count,
                                                         size_t var_count)
{
    bool degrees[96] = {false};

    if (count < 2u || count > sizeof(degrees) / sizeof(degrees[0]))
        return false;

    for (size_t i = 0u; i < count; ++i) {
        long total_degree = 0;

        for (size_t j = 0u; j < var_count; ++j)
            total_degree += terms[i].degree[j];

        if (total_degree < 0 || (size_t)total_degree != i || (size_t)total_degree >= count || degrees[total_degree] ||
            !display_poly_contains_indexed_arbitrary_constant(terms[i].expr))
            return false;
        degrees[total_degree] = true;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (!degrees[i])
            return false;
    }
    return true;
}

/* An explicit remainder identifies a series; ordinary polynomials retain their usual ordering. */
const expr_t *display_series_remainder(const expr_t *expr)
{
    if (!expr)
        return NULL;
    if (expr_is_arbitrary_function(expr) && expr->name && strcmp(expr->name, "O") == 0)
        return expr;
    if (!expr_is_addsub(expr))
        return NULL;
    const expr_t *left = display_series_remainder(expr->a);
    return left ? left : display_series_remainder(expr->b);
}

static bool display_series_term_degree(const expr_t *expr, const expr_t *base,
                                       const display_poly_var_list_t *vars, long *degree)
{
    if (!expr)
        return false;
    if (expr_struct_eq(expr, base))
        return display_poly_add_degree(degree, 1L);
    if (!display_poly_expr_contains_any_var(expr, vars))
        return true;
    if (expr_is_neg(expr))
        return display_series_term_degree(expr->a, base, vars, degree);
    if (expr_is_mul(expr))
        return display_series_term_degree(expr->a, base, vars, degree) &&
               display_series_term_degree(expr->b, base, vars, degree);
    if (expr_is_op(expr, &ops_div))
        return !display_poly_expr_contains_any_var(expr->b, vars) &&
               display_series_term_degree(expr->a, base, vars, degree);
    if ((expr_is_pow_d_expr(expr) || expr_is_op(expr, &ops_pow)) && expr_struct_eq(expr->a, base)) {
        bool literal_power = expr_is_pow_d_expr(expr);
        number_t exponent = literal_power ? num_clone(expr->c) : num_new();
        long power = 0L;
        bool matched = literal_power || expr_match_const_value(expr->b, &exponent);
        bool valid = matched && expr_try_get_small_integer_exponent(exponent, &power) &&
                     display_poly_add_degree(degree, power);
        num_destroy(&exponent);
        return valid;
    }
    return false;
}

static bool display_series_collect_terms(const expr_t *expr, bool subtract, const expr_t *base,
                                         const expr_t *remainder, const display_poly_var_list_t *vars,
                                         display_poly_term_t *terms, size_t *count, size_t max_terms)
{
    long degree = 0L;
    if (expr == remainder) {
        degree = LONG_MAX;
    } else if (!display_series_term_degree(expr, base, vars, &degree)) {
        if (!expr_is_addsub(expr))
            return false;
        return display_series_collect_terms(expr->a, subtract, base, remainder, vars, terms, count, max_terms) &&
               display_series_collect_terms(expr->b, subtract != expr_is_op(expr, &ops_sub), base, remainder,
                                             vars, terms, count, max_terms);
    }
    if (*count >= max_terms)
        return false;
    terms[*count] = (display_poly_term_t){.expr = expr, .subtract = subtract, .degree = {degree}};
    ++*count;
    return true;
}

static bool display_series_prepare_terms(const expr_t *expr, display_poly_term_t *terms,
                                         size_t *count, size_t max_terms)
{
    const expr_t *remainder = display_series_remainder(expr);
    if (!remainder || !remainder->a)
        return false;
    const expr_t *argument = remainder->a;
    const expr_t *base = expr_is_pow_d_expr(argument) || expr_is_op(argument, &ops_pow) ? argument->a : argument;
    display_poly_var_list_t vars = {0};
    if (!display_poly_collect_vars(base, &vars) || vars.count != 1u)
        return false;
    *count = 0u;
    if (!display_series_collect_terms(expr, false, base, remainder, &vars, terms, count, max_terms))
        return false;
    display_poly_sort_terms(terms, *count, 1u, true);
    return true;
}

/* A transform is not a polynomial coefficient: retain its term ahead of the initial-value polynomial. */
static const expr_t *display_transform_factor(const expr_t *expr)
{
    if (!expr)
        return NULL;
    if (expr_is_integral_transform(expr))
        return expr;
    if (expr_is_neg(expr))
        return display_transform_factor(expr->a);
    if (expr_is_mul(expr)) {
        const expr_t *left = display_transform_factor(expr->a);
        const expr_t *right = display_transform_factor(expr->b);
        return left && right ? NULL : left ? left : right;
    }
    return NULL;
}

bool display_sum_has_transform(const expr_t *expr)
{
    if (expr_is_addsub(expr))
        return display_sum_has_transform(expr->a) || display_sum_has_transform(expr->b);
    return display_transform_factor(expr) != NULL;
}

static bool display_transform_collect_terms(const expr_t *expr, bool subtract, display_poly_term_t *terms,
                                            size_t *count, size_t max_terms)
{
    if (expr_is_addsub(expr))
        return display_transform_collect_terms(expr->a, subtract, terms, count, max_terms) &&
               display_transform_collect_terms(expr->b, subtract != expr_is_op(expr, &ops_sub),
                                                terms, count, max_terms);
    if (*count >= max_terms)
        return false;
    terms[(*count)++] = (display_poly_term_t){.expr = expr, .subtract = subtract};
    return true;
}

static bool display_transform_prepare_terms(const expr_t *expr, display_poly_term_t *terms,
                                            size_t *count, size_t max_terms)
{
    const expr_t *transform = NULL;
    size_t leading = 0u;
    *count = 0u;
    if (!display_transform_collect_terms(expr, false, terms, count, max_terms) || *count < 2u)
        return false;
    /* Bounded by the shared display-term capacity; reject ambiguous multiple-transform sums. */
    for (size_t i = 0u; i < *count; ++i) {
        const expr_t *candidate = display_transform_factor(terms[i].expr);
        if (!candidate)
            continue;
        if (transform)
            return false;
        transform = candidate;
        leading = i;
    }
    const expr_t *target = transform && transform->b && transform->b->b ? transform->b->b->a : NULL;
    if (!target || !expr_is_var(target) || !target->name)
        return false;
    display_poly_var_list_t vars = {.name = {target->name}, .count = 1u};
    for (size_t i = 0u; i < *count; ++i) {
        if (i != leading && !display_poly_term_degrees(terms[i].expr, &vars, terms[i].degree))
            return false;
    }
    display_poly_term_t first = terms[leading];
    memmove(terms + 1u, terms, leading * sizeof(*terms));
    terms[0] = first;
    display_poly_sort_terms(terms + 1u, *count - 1u, 1u, true);
    return true;
}

static bool display_poly_prepare_terms(const expr_t *expr, display_poly_term_t *terms, size_t *count, size_t max_terms)
{
    display_poly_var_list_t vars = {0};
    bool has_variable_term = false;

    if (!expr || !terms || !count || !expr_is_addsub(expr))
        return false;

    if (display_series_prepare_terms(expr, terms, count, max_terms))
        return true;

    if (display_transform_prepare_terms(expr, terms, count, max_terms))
        return true;

    if (display_poly_expr_contains_imaginary_unit(expr) || !display_poly_collect_vars(expr, &vars) || vars.count == 0u)
        return false;

    *count = 0u;
    if (!display_poly_collect_add_terms(expr, false, &vars, terms, count, max_terms) || *count < 2u)
        return false;

    for (size_t i = 0u; i < *count; ++i) {
        if (!display_poly_term_degrees(terms[i].expr, &vars, terms[i].degree))
            return false;
        for (size_t j = 0u; j < vars.count; ++j) {
            if (terms[i].degree[j] > 0) {
                has_variable_term = true;
                break;
            }
        }
    }

    if (!has_variable_term)
        return false;

    display_poly_sort_terms(terms, *count, vars.count,
                            display_poly_is_explicit_arbitrary_amplitude(terms, *count, vars.count));
    if (terms[0].subtract != (expr_renders_negative(terms[0].expr) ? true : false))
        return false;
    return true;
}

/* Share the display ordering with the multiline TeX renderer. */
bool expr_display_ordered_sum(const expr_t *expr, const expr_t **nodes, int *signs, size_t *count, size_t capacity)
{
    display_poly_term_t terms[96];
    size_t limit = capacity < 96u ? capacity : 96u;
    if (!expr_is_addsub(expr) || !display_poly_prepare_terms(expr, terms, count, limit))
        return false;
    for (size_t i = 0u; i < *count; ++i) {
        nodes[i] = terms[i].expr;
        signs[i] = terms[i].subtract ? -1 : 1;
    }
    return true;
}

bool emit_expr_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_putc(b, '(');

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_putc(b, '(');
        if (term_negative)
            emit_expr_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_expr(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_putc(b, ')');
    }

    if (need)
        sbuf_putc(b, ')');
    return true;
}


bool emit_TeX_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (expr_TeX_source_order_preserved() || !display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_puts(b, "\\left(");

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_puts(b, "\\left(");
        if (term_negative)
            emit_TeX_expr_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_TeX_expr(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_puts(b, "\\right)");
    }

    if (need)
        sbuf_puts(b, "\\right)");
    return true;
}

bool emit_func_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_putc(b, '(');

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_putc(b, '(');
        if (term_negative)
            emit_func_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_func(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_putc(b, ')');
    }

    if (need)
        sbuf_putc(b, ')');
    return true;
}

void emit_func_display(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (f && expr_is_addsub(f) && emit_func_display_polynomial_sum(f, b, parent_prec))
        return;
    emit_func(f, b, parent_prec);
}
