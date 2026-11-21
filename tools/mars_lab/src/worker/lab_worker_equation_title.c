/**
 * @file lab_worker_equation_title.c
 * @brief Native equation plot captions specialised to the sampled slice.
 *
 * Copies the displayed equation and substitutes finite bindings outside the
 * selected plotting coordinates. Exact authored values remain native expression
 * graphs, so radicals and rational values simplify without a rounded text round trip.
 * The same graph supplies plain and TeX captions. Lossless decimal alternatives stay readable; long decimal
 * literals are abbreviated with an explicit ellipsis in captions only. Sampling, worksheet bindings and
 * variable or constant roles are unchanged; no client-side mathematics is required.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "equation-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* Shorten only decimal tokens in an already native-rendered caption; integer indices and algebra stay intact. */
static string_t *caption_numbers(const char *source, bool TeX)
{
    size_t length = source ? strlen(source) : 0, position = 0, copied = 0;
    string_t *out = source ? string_new() : NULL;
    bool ok = out != NULL;
    while (ok && position < length) {
        unsigned char byte = (unsigned char)source[position];
        if (!isdigit(byte)) {
            ++position;
            continue;
        }
        size_t start = position;
        while (position < length && isdigit((unsigned char)source[position]))
            ++position;
        bool decimal = position < length && source[position] == '.' && position + 1 < length &&
                       isdigit((unsigned char)source[position + 1]);
        if (decimal) {
            ++position;
            while (position < length && isdigit((unsigned char)source[position]))
                ++position;
        }
        bool identifier = start && (isalnum((unsigned char)source[start - 1]) || source[start - 1] == '_');
        if (!decimal || identifier || position - start <= 20)
            continue;
        ok = !string_append_bytes(out, source + copied, start + 18 - copied) &&
             !string_append_cstr(out, TeX ? "\\ldots{}" : "...");
        copied = position;
    }
    ok = ok && !string_append_bytes(out, source + copied, length - copied);
    if (!ok) {
        string_free(out);
        out = NULL;
    }
    return out;
}

/* Build both caption styles from the exact equation being sampled, retaining only its free coordinates. */
bool lab_worker_equation_plot_titles(const equation_t *equation, const expr_t *first, const expr_t *second,
                                      const expr_t *third, string_t **plain, char **TeX)
{
    if (!plain || !TeX)
        return false;
    *plain = NULL;
    *TeX = NULL;
    if (!equation || !first)
        return false;
    expr_t *sides[] = {expr_retain_expr(equ_lhs(equation)), expr_retain_expr(equ_rhs(equation))};
    expr_bindings_t *bindings = equ_bindings(equation);
    bool ok = sides[0] && sides[1];
    for (size_t i = 0; ok && i < expr_bindings_count(bindings); ++i) {
        const expr_t *binding = expr_bindings_expr_at(bindings, i);
        if (binding == first || binding == second || binding == third)
            continue;
        number_t value = expr_get_val(binding);
        bool supplied = num_is_finite(value);
        num_destroy(&value);
        if (!supplied)
            continue;
        expr_t *replacement = expr_binding_value_expression(binding);
        /* Each substitution may clone other named constants. Match their names through a temporary variable
         * needle rather than relying on the original constant's pointer identity. Worksheet roles do not change. */
        expr_t *needle = expr_new_named_var(NUM_NAN, expr_bindings_name_at(bindings, i));
        ok = replacement && needle;
        for (unsigned side = 0; ok && side < 2; ++side) {
            expr_t *next = expr_substitute(sides[side], needle, replacement);
            ok = next != NULL;
            expr_free(sides[side]);
            sides[side] = next;
        }
        expr_free(needle);
        expr_free(replacement);
    }
    for (unsigned side = 0; ok && side < 2; ++side) {
        expr_t *simplified = expr_display_simplified(sides[side]);
        ok = simplified != NULL;
        expr_free(sides[side]);
        sides[side] = simplified;
    }
    equation_t *source = ok ? equ_new(sides[0], sides[1]) : NULL;
    equation_t *display = source ? equ_simplify_with_context(source, NULL) : NULL;
    if (display) {
        expr_t *left = expr_decimal_coefficients_for_display(equ_lhs(display));
        expr_t *right = expr_decimal_coefficients_for_display(equ_rhs(display));
        equation_t *caption = left && right ? equ_new(left, right) : NULL;
        string_t *full = caption ? equ_to_text(caption, style_UNBOUND) : NULL;
        char *full_TeX = caption ? equ_to_TeX_body_wrapped(caption, LAB_WORKER_EQUATION_LINE_LIMIT_LATEX) : NULL;
        *plain = full ? caption_numbers(string_c_str(full), false) : NULL;
        string_t *compact_TeX = caption_numbers(full_TeX, true);
        *TeX = compact_TeX ? strdup(string_c_str(compact_TeX)) : NULL;
        string_free(compact_TeX);
        free(full_TeX);
        string_free(full);
        equ_free(caption);
        expr_free(right);
        expr_free(left);
    }
    ok = *plain && *TeX;
    if (!ok) {
        string_free(*plain);
        free(*TeX);
        *plain = NULL;
        *TeX = NULL;
    }
    equ_free(display);
    equ_free(source);
    expr_free(sides[1]);
    expr_free(sides[0]);
    return ok;
}
