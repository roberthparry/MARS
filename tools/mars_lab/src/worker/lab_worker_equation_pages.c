/**
 * @file lab_worker_equation_pages.c
 * @brief Bounded native solution batches for asynchronously extended worksheet cards.
 *
 * Only recognised affine zero-target zeta equations with an unset variable participate. Each request searches
 * the next height interval at worksheet precision, checks both conjugates against the authored equation, and
 * returns native notation and an opaque continuation cursor. Ordinary finite symbolic results are unaffected.
 * No browser code parses or formats mathematics, and no completed page is redrawn or replaced.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation-t.h"
#include "TeX-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_plot_prepared.h"

typedef struct {
    number_t value;
    double height;
} solution_page_root_t;

static int compare_roots(const void *a, const void *b)
{
    double first = ((const solution_page_root_t *)a)->height, second = ((const solution_page_root_t *)b)->height;
    return (first > second) - (first < second);
}

static bool page_number(TeX_builder_t *builder, number_t value)
{
    expr_t *constant = expr_new_const(value);
    string_t *text = constant ? expr_to_text(constant, style_UNBOUND) : NULL;
    bool ok = text && TeX_expression(builder, text);
    string_free(text);
    expr_free(constant);
    return ok;
}

static bool page_pair(TeX_builder_t *builder, const char *name, number_t value)
{
    number_t real = num_real_part(value), imaginary = num_imag_part(value);
    bool ok = TeX_symbol(builder, name) && page_number(builder, real) && page_number(builder, imaginary) &&
              TeX_symbol(builder, "i") && TeX_mul(builder) && TeX_integer(builder, 0) && TeX_list(builder) &&
              TeX_symmetric_pair(builder, false) && TeX_approx(builder);
    num_destroy(&real);
    num_destroy(&imaginary);
    return ok;
}

/* Emit a small complete page; later requests continue at the searched boundary, not at a rounded root. */
int lab_worker_equation_pages(int argc, char **argv)
{
    if (argc < 5)
        return 1;
    int precision = atoi(argv[2]);
    if (precision < 17 || precision > 78914)
        return 1;
    num_set_default_prec_digits((size_t)precision + 8);
    number_t cursor = num_create_from_string(argv[4]);
    double start = num_to_double(cursor);
    bool first = num_is_zero(cursor);
    if (!num_is_real(cursor) || !isfinite(start) || start < 0 || start >= 10000) {
        num_destroy(&cursor);
        return 1;
    }
    equation_t *equation = equ_from_string(argv[1]);
    expr_bindings_t *bindings = equ_bindings(equation);
    const char *name = NULL;
    expr_t *variable = NULL;
    bool supported = equation != NULL;
    for (size_t i = 0; supported && i < expr_bindings_count(bindings); ++i) {
        if (expr_bindings_is_constant_at(bindings, i))
            continue;
        if (variable) {
            supported = false;
            break;
        }
        variable = expr_bindings_expr_at(bindings, i);
        name = expr_bindings_name_at(bindings, i);
    }
    number_t original = variable ? expr_eval(variable) : NUM_NAN;
    supported = supported && variable && num_is_nan(original);
    double end = first ? (precision > 128 ? 16 : 80) : fmin(10000, start + 8);
    number_t lower = num_create_from_double(first ? 0 : nextafter(start, INFINITY));
    number_t upper = num_create_from_double(end);
    bool limited = false;
    equation_solutions_t *solutions = supported
        ? equ_search_zeta_zeros(equation, lower, upper, 32, &limited) : NULL;
    int error = errno;
    size_t count = equ_solutions_count(solutions);
    solution_page_root_t roots[32];
    number_t both[64];
    size_t filled = 0;
    expr_t *residual = solutions ? equ_residual(equation) : NULL;
    number_t tolerance = num_pow10(-precision);
    bool ok = solutions && !limited && residual;
    for (size_t i = 0; ok && i < count; ++i) {
        number_t root = expr_eval(equ_rhs(equ_solutions_at(solutions, i)));
        number_t conjugate = num_conj(root), height = num_imag_part(root);
        roots[i] = (solution_page_root_t){root, num_to_double(height)};
        both[filled++] = num_clone(root);
        both[filled++] = conjugate;
        expr_set_val(variable, conjugate);
        number_t value = expr_eval(residual), magnitude = num_abs(value);
        ok = num_is_finite(magnitude) && num_le(magnitude, tolerance);
        num_destroy(&magnitude);
        num_destroy(&value);
        num_destroy(&height);
    }
    if (variable)
        expr_set_val(variable, original);
    if (ok)
        qsort(roots, count, sizeof(*roots), compare_roots);
    int old_precision = expr_set_number_precision_local(precision);
    TeX_builder_t *builder = TeX_builder_new();
    string_t *numeric = string_new();
    for (size_t i = 0; ok && i < count; ++i) {
        ok = page_pair(builder, name, roots[i].value) && (!i || TeX_newline(builder));
        number_t real = num_real_part(roots[i].value), imaginary = num_imag_part(roots[i].value);
        string_t *re = lab_worker_number_text(real, precision), *im = lab_worker_number_text(imaginary, precision);
        ok = ok && re && im && string_append_format(numeric, "plot_solutions %s ≈ %s ± %si\n", name,
                                                    string_c_str(re), string_c_str(im)) >= 0;
        string_free(re); string_free(im);
        num_destroy(&real); num_destroy(&imaginary);
    }
    if (ok && count > 1)
        ok = TeX_align(builder);
    TeX_t *notation = ok && count ? TeX_builder_finish(builder) : NULL;
    ok = ok && (!count || notation) && numeric;
    string_t *unbound = equation ? equ_to_text(equation, style_UNBOUND) : NULL;
    string_t *function = equation ? equ_to_text(equation, style_FUNCTION) : NULL;
    char *title = equation ? equ_to_TeX_body_wrapped(equation, 110) : NULL;
    if (ok) {
        printf("input %s\nequation %s\nunbound %s\nfunction %s\nstatus solved\n"
               "solutions_TeX %s\nsolutions_after %.17g\nsolutions_precision %d\nsolutions_more %s\n"
               "solutions_background %s\nsolution_page yes\ndifferentiable no\n",
               argv[1], unbound ? string_c_str(unbound) : argv[1], unbound ? string_c_str(unbound) : argv[1],
               function ? string_c_str(function) : "", notation ? TeX_source(notation) : "", end, precision,
               end < 10000 ? "yes" : "no", end < 80 ? "yes" : "no");
        printf("solution_copy_TeX \\[\n%s\n\\]\n", notation ? TeX_source(notation) : "");
        printf("solutions_rows %zu\nsearch_note Bounded non-trivial zero search; not exhaustive.\n"
               "family_note The negative even integers form the separate trivial real family.\n", count);
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i) {
            number_t bound = expr_get_val(expr_bindings_expr_at(bindings, i));
            string_t *value = num_to_string(bound);
            printf("binding %s\t%s\t%s\n", expr_bindings_is_constant_at(bindings, i) ? "constant" : "variable",
                   expr_bindings_name_at(bindings, i), value ? string_c_str(value) : "NAN");
            string_free(value);
            num_destroy(&bound);
        }
        for (size_t i = 0; i < filled; ++i) {
            string_t *value = lab_worker_number_text(both[i], precision);
            printf("numeric %s = %s\nnumeric_indices %zu\nsolutions %s = %s\n", name,
                   value ? string_c_str(value) : "", i, name, value ? string_c_str(value) : "");
            string_free(value);
        }
        if (first && argc > 5 && !strcmp(argv[5], "--plot-cache") && title) {
            const char *keys[] = {argv[1], name, argv[2], "equation"};
            const expr_t *curves[] = {equ_lhs(equation), equ_rhs(equation)};
            lab_worker_plot_prepared_emit(keys, argv[1], title, curves, NULL, 2, false, numeric, both, filled);
        }
    }
    free(title);
    string_free(unbound); string_free(function); string_free(numeric);
    TeX_free(notation); TeX_builder_free(builder);
    expr_set_number_precision_local(old_precision);
    for (size_t i = 0; i < filled / 2; ++i)
        num_destroy(&roots[i].value);
    for (size_t i = 0; i < filled; ++i)
        num_destroy(&both[i]);
    num_destroy(&tolerance); num_destroy(&original); num_destroy(&cursor);
    num_destroy(&lower); num_destroy(&upper);
    expr_free(residual);
    equ_solutions_free(solutions); equ_free(equation);
    return ok ? 0 : !supported || error == ENOTSUP ? 2 : 1;
}
