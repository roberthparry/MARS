#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression.h"
#include "number.h"
#include "ustring.h"

static char *dup_string(const char *text)
{
    size_t n;
    char *copy;

    if (!text)
        text = "";
    n = strlen(text);
    copy = malloc(n + 1u);
    if (copy)
        memcpy(copy, text, n + 1u);
    return copy;
}

static char *equ_text_dup(const equation_t *equation, style_t style)
{
    string_t *text = equ_to_text(equation, style);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    return expr_to_string(expr, style);
}

static char *number_text_dup(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *number_precision_text_dup(number_t value, int precision)
{
    char fmt[32];
    int digits = precision > 0 ? precision - 1 : 0;
    int needed;
    char *out;

    if (precision <= 0)
        return number_text_dup(value);

    snprintf(fmt, sizeof(fmt), "%%.%dN", digits);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0)
        return NULL;

    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, fmt, value) < 0) {
        free(out);
        return NULL;
    }

    return out;
}

static char *expr_TeX_body_dup(const expr_t *expr)
{
    char *body = expr_to_TeX_body_wrapped(expr, 110u);

    return body ? body : expr_text_dup(expr, style_LATEX);
}

static bool TeX_wrapped_aligned_body(const char *tex, const char **body_out, size_t *body_len_out)
{
    static const char prefix[] = "\\begin{aligned}[t]\n";
    static const char suffix[] = "\n\\end{aligned}";
    const char *body;
    const char *end;
    size_t suffix_len = strlen(suffix);

    if (body_out)
        *body_out = NULL;
    if (body_len_out)
        *body_len_out = 0u;
    if (!tex || strncmp(tex, prefix, strlen(prefix)) != 0)
        return false;

    end = tex + strlen(tex);
    if ((size_t)(end - tex) < suffix_len || strcmp(end - suffix_len, suffix) != 0)
        return false;

    body = tex + strlen(prefix);
    if (*body == '&')
        body++;
    end -= suffix_len;
    while (end > body && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\t'))
        end--;

    if (body_out)
        *body_out = body;
    if (body_len_out)
        *body_len_out = (size_t)(end - body);
    return true;
}

static void print_aligned_equation_fragment(const char *lhs, const char *rhs)
{
    const char *body = NULL;
    size_t body_len = 0u;

    if (!lhs)
        lhs = "\\text{null}";
    if (!rhs)
        rhs = "\\text{null}";
    if (TeX_wrapped_aligned_body(rhs, &body, &body_len))
        printf("%s &= %.*s", lhs, (int)body_len, body);
    else
        printf("%s &= %s", lhs, rhs);
}

static char *equation_display_TeX_dup(const equation_t *equation)
{
    return equ_to_TeX_body_wrapped(equation, 110u);
}

static equation_t *display_expanded_equation(const equation_t *equation)
{
    expr_bindings_t *bindings;
    const expr_t *wrt = NULL;

    if (!equation)
        return NULL;

    bindings = equ_bindings(equation);
    for (size_t i = 0u; bindings && i < expr_bindings_count(bindings); ++i) {
        if (!expr_bindings_is_constant_at(bindings, i)) {
            wrt = expr_bindings_expr_at(bindings, i);
            break;
        }
    }

    return equ_display_expanded(equation, wrt);
}

static expr_t *substitute_bound_constants(const expr_t *expr, expr_bindings_t *bindings)
{
    expr_t *current;
    bool have_constant_bindings = false;

    if (!expr)
        return NULL;

    if (!bindings) {
        current = (expr_t *)expr;
        expr_retain(current);
        return current;
    }

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i))
            continue;
        if (!name || !name[0])
            continue;
        have_constant_bindings = true;
        break;
    }

    current = (expr_t *)expr;
    expr_retain(current);
    if (!have_constant_bindings)
        return current;

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        number_t value;
        number_t needle_seed;
        expr_t *needle;
        expr_t *replacement;
        expr_t *next;
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding_expr = expr_bindings_expr_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i))
            continue;
        if (!name || !name[0])
            continue;

        value = expr_eval(binding_expr);
        if (!num_is_finite(value)) {
            num_destroy(&value);
            continue;
        }

        needle_seed = num_new();
        needle = expr_new_named_var(needle_seed, name);
        num_destroy(&needle_seed);
        replacement = expr_new_const(value);
        num_destroy(&value);
        if (!needle || !replacement) {
            expr_free(needle);
            expr_free(replacement);
            continue;
        }

        next = expr_substitute(current, needle, replacement);
        expr_free(needle);
        expr_free(replacement);
        if (!next)
            continue;

        expr_free(current);
        current = next;
        if (!current)
            return NULL;
    }

    return current;
}

static equation_t *solution_with_bound_constants(const equation_t *solution, expr_bindings_t *bindings)
{
    expr_t *lhs;
    expr_t *rhs;
    equation_t *out;

    if (!solution)
        return NULL;

    lhs = substitute_bound_constants(equ_lhs(solution), bindings);
    rhs = substitute_bound_constants(equ_rhs(solution), bindings);
    if (!lhs || !rhs) {
        expr_free(lhs);
        expr_free(rhs);
        return NULL;
    }

    out = equ_new(lhs, rhs);
    expr_free(rhs);
    expr_free(lhs);
    return out;
}

static const char *solution_binding_name(expr_bindings_t *bindings, const equation_t *solution)
{
    const expr_t *lhs = equ_lhs(solution);

    if (!bindings || !lhs)
        return NULL;

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding_expr = expr_bindings_expr_at(bindings, i);

        if (expr_bindings_is_constant_at(bindings, i) || binding_expr != lhs || !name)
            continue;
        return name;
    }

    return NULL;
}

static size_t solution_binding_order(expr_bindings_t *bindings, const equation_t *solution)
{
    const expr_t *lhs = equ_lhs(solution);
    size_t count = bindings ? expr_bindings_count(bindings) : 0u;

    if (!bindings || !lhs)
        return count;

    for (size_t i = 0u; i < count; ++i) {
        expr_t *binding_expr = expr_bindings_expr_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i) && binding_expr == lhs)
            return i;
    }

    return count;
}

static int compare_real_numbers(number_t a, number_t b)
{
    if (num_lt(a, b))
        return -1;
    if (num_gt(a, b))
        return 1;
    return 0;
}

static int compare_solution_values(number_t a, number_t b)
{
    bool a_numeric = num_is_finite(a) && !num_is_nan(a);
    bool b_numeric = num_is_finite(b) && !num_is_nan(b);
    bool a_real;
    bool b_real;
    int comparison = 0;

    if (a_numeric != b_numeric)
        return a_numeric ? -1 : 1;
    if (!a_numeric)
        return 0;

    a_real = num_is_real(a);
    b_real = num_is_real(b);
    if (a_real != b_real)
        return a_real ? -1 : 1;
    if (a_real)
        return compare_real_numbers(a, b);

    {
        number_t a_conjugate = num_conj(a);
        number_t b_conjugate = num_conj(b);
        number_t a_product = num_mul(a, a_conjugate);
        number_t b_product = num_mul(b, b_conjugate);
        number_t a_magnitude_squared = num_real_part(a_product);
        number_t b_magnitude_squared = num_real_part(b_product);

        comparison = compare_real_numbers(a_magnitude_squared, b_magnitude_squared);
        num_destroy(&b_magnitude_squared);
        num_destroy(&a_magnitude_squared);
        num_destroy(&b_product);
        num_destroy(&a_product);
        num_destroy(&b_conjugate);
        num_destroy(&a_conjugate);
    }

    if (comparison == 0) {
        number_t a_real_part = num_real_part(a);
        number_t b_real_part = num_real_part(b);

        comparison = compare_real_numbers(a_real_part, b_real_part);
        num_destroy(&b_real_part);
        num_destroy(&a_real_part);
    }

    if (comparison == 0) {
        number_t a_imaginary_part = num_imag_part(a);
        number_t b_imaginary_part = num_imag_part(b);

        comparison = -compare_real_numbers(a_imaginary_part, b_imaginary_part);
        num_destroy(&b_imaginary_part);
        num_destroy(&a_imaginary_part);
    }

    return comparison;
}

static int compare_solution_indices(const equation_solutions_t *solutions, expr_bindings_t *bindings, size_t a_index,
                                    size_t b_index)
{
    const equation_t *a_solution = equ_solutions_at(solutions, a_index);
    const equation_t *b_solution = equ_solutions_at(solutions, b_index);
    size_t a_binding = solution_binding_order(bindings, a_solution);
    size_t b_binding = solution_binding_order(bindings, b_solution);
    number_t a_value;
    number_t b_value;
    int comparison;

    if (a_binding != b_binding)
        return a_binding < b_binding ? -1 : 1;

    a_value = expr_eval(equ_rhs(a_solution));
    b_value = expr_eval(equ_rhs(b_solution));
    comparison = compare_solution_values(a_value, b_value);
    num_destroy(&b_value);
    num_destroy(&a_value);

    if (comparison != 0)
        return comparison;
    return a_index < b_index ? -1 : a_index > b_index;
}

static size_t *ordered_solution_indices(const equation_solutions_t *solutions, expr_bindings_t *bindings)
{
    size_t count = equ_solutions_count(solutions);
    size_t *indices = malloc(count * sizeof(*indices));

    if (!indices)
        return NULL;

    for (size_t i = 0u; i < count; ++i)
        indices[i] = i;

    for (size_t i = 1u; i < count; ++i) {
        size_t current = indices[i];
        size_t j = i;

        while (j > 0u && compare_solution_indices(solutions, bindings, current, indices[j - 1u]) < 0) {
            indices[j] = indices[j - 1u];
            j--;
        }
        indices[j] = current;
    }

    return indices;
}

static const equation_t *ordered_solution_at(const equation_solutions_t *solutions, const size_t *order, size_t index)
{
    return equ_solutions_at(solutions, order ? order[index] : index);
}

static void print_solutions(const equation_solutions_t *solutions, expr_bindings_t *bindings, const size_t *order)
{
    size_t count = equ_solutions_count(solutions);

    if (count == 0u) {
        printf("solutions   \n");
        return;
    }

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = ordered_solution_at(solutions, order, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_text = expr_text_dup(equ_rhs(shown), style_UNBOUND);
        char *text = equ_text_dup(shown, style_UNBOUND);

        if (name && rhs_text)
            printf("%s%s = %s\n", i == 0u ? "solutions   " : "            ", name, rhs_text);
        else
            printf("%s%s\n", i == 0u ? "solutions   " : "            ", text ? text : "(null)");
        equ_free(display);
        free(text);
        free(rhs_text);
    }
}

static void print_solution_TeX_rows(const equation_solutions_t *solutions, expr_bindings_t *bindings,
                                    const size_t *order, const char *first_separator)
{
    size_t count = equ_solutions_count(solutions);

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = ordered_solution_at(solutions, order, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_TeX = expr_TeX_body_dup(equ_rhs(shown));
        char *tex = equ_text_dup(shown, style_LATEX);

        if (name && rhs_TeX) {
            printf("%s", i == 0u ? first_separator : " \\\\\n");
            print_aligned_equation_fragment(name, rhs_TeX);
        } else
            printf("%s%s", i == 0u ? first_separator : " \\\\\n", tex ? tex : "\\text{null}");
        equ_free(display);
        free(tex);
        free(rhs_TeX);
    }
}

static void print_solutions_TeX(const equation_solutions_t *solutions, expr_bindings_t *bindings, const size_t *order)
{
    if (equ_solutions_count(solutions) == 0u) {
        printf("solutions_TeX \n");
        return;
    }

    printf("solutions_TeX \\begin{aligned}");
    print_solution_TeX_rows(solutions, bindings, order, " ");
    printf(" \\end{aligned}\n");
}

static void print_derivation_TeX(const char *equation_TeX, const equation_solutions_t *solutions,
                                 expr_bindings_t *bindings, const size_t *order)
{
    const char *equation_body = NULL;
    size_t equation_body_len = 0u;

    if (!equation_TeX || !strstr(equation_TeX, "\\sum_") || equ_solutions_count(solutions) == 0u ||
        !TeX_wrapped_aligned_body(equation_TeX, &equation_body, &equation_body_len)) {
        printf("derivation_TeX \n");
        return;
    }

    printf("derivation_TeX \\begin{aligned}[t]\n%.*s \\\\\n", (int)equation_body_len, equation_body);
    print_solution_TeX_rows(solutions, bindings, order, "");
    printf("\n\\end{aligned}\n");
}

static bool substitute_named_zero_if_present(expr_t **expr_io, const char *name)
{
    number_t zero_value;
    expr_t *needle;
    expr_t *zero;
    expr_t *next;
    char *before;
    char *after;
    bool changed = false;

    if (!expr_io || !*expr_io || !name)
        return false;

    before = expr_text_dup(*expr_io, style_UNBOUND);
    zero_value = num_create_from_long(0L);
    needle = expr_new_named_var(NUM_NAN, name);
    zero = expr_new_const(zero_value);
    num_destroy(&zero_value);
    next = (needle && zero) ? expr_substitute(*expr_io, needle, zero) : NULL;
    expr_free(needle);
    expr_free(zero);
    if (!next) {
        free(before);
        return false;
    }

    after = expr_text_dup(next, style_UNBOUND);
    changed = before && after && strcmp(before, after) != 0;
    free(before);
    free(after);

    expr_free(*expr_io);
    *expr_io = next;
    return changed;
}

static const char *sample_note(bool sampled_k, bool sampled_n)
{
    if (sampled_k && sampled_n)
        return "  (k = 0, n = 0)";
    if (sampled_k)
        return "  (k = 0)";
    if (sampled_n)
        return "  (n = 0)";
    return "";
}

static number_t eval_solution_rhs_with_sampled_indices(const expr_t *rhs, bool *sampled_k_out, bool *sampled_n_out)
{
    number_t value;

    if (sampled_k_out)
        *sampled_k_out = false;
    if (sampled_n_out)
        *sampled_n_out = false;

    value = expr_eval(rhs);
    if (num_is_finite(value) && !num_is_nan(value))
        return value;

    {
        bool sampled_k = false;
        bool sampled_n = false;
        expr_t *sampled = expr_clone(rhs);
        expr_t *simplified = sampled ? expr_simplify(sampled) : NULL;
        number_t sampled_value = simplified ? expr_eval(simplified) : num_new();

        expr_free(simplified);
        if (num_is_finite(sampled_value) && !num_is_nan(sampled_value)) {
            expr_free(sampled);
            num_destroy(&value);
            return sampled_value;
        }
        num_destroy(&sampled_value);

        sampled_k = substitute_named_zero_if_present(&sampled, "k");
        sampled_n = substitute_named_zero_if_present(&sampled, "n");
        simplified = sampled ? expr_simplify(sampled) : NULL;
        sampled_value = simplified ? expr_eval(simplified) : num_new();

        expr_free(simplified);
        expr_free(sampled);

        if (num_is_finite(sampled_value) && !num_is_nan(sampled_value)) {
            num_destroy(&value);
            if (sampled_k_out)
                *sampled_k_out = sampled_k;
            if (sampled_n_out)
                *sampled_n_out = sampled_n;
            return sampled_value;
        }

        num_destroy(&sampled_value);
    }

    return value;
}

static void print_solution_numerics(const equation_solutions_t *solutions, expr_bindings_t *bindings,
                                    const size_t *order, int precision)
{
    size_t count = equ_solutions_count(solutions);

    if (count == 0u) {
        printf("numeric     \n");
        return;
    }

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = ordered_solution_at(solutions, order, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        bool sampled_k = false;
        bool sampled_n = false;
        number_t value = eval_solution_rhs_with_sampled_indices(equ_rhs(shown), &sampled_k, &sampled_n);
        char *value_text = number_precision_text_dup(value, precision);
        const char *note = sample_note(sampled_k, sampled_n);

        if (name && value_text)
            printf("%s%s ≈ %s%s\n", i == 0u ? "numeric     " : "            ", name, value_text, note);
        else
            printf("%s%s%s\n", i == 0u ? "numeric     " : "            ", value_text ? value_text : "(null)", note);

        free(value_text);
        num_destroy(&value);
        equ_free(display);
    }
}

static void print_equation_fields(const equation_t *equation, const equation_solutions_t *solutions,
                                  expr_bindings_t *bindings, const char *input, const char *status, int precision)
{
    expr_t *residual = equ_residual(equation);
    expr_t *display_residual = residual ? expr_display_simplified(residual) : NULL;
    equation_t *display_equation = display_expanded_equation(equation);
    const equation_t *shown_equation = display_equation ? display_equation : equation;
    number_t residual_value = residual ? expr_eval(residual) : num_new();
    char *equation_text = equ_text_dup(equation, style_EXPRESSION);
    char *unbound_text = equ_text_dup(shown_equation, style_UNBOUND);
    char *function_text = equ_text_dup(shown_equation, style_FUNCTION);
    char *tex = equation_display_TeX_dup(shown_equation);
    char *residual_text = display_residual ? expr_text_dup(display_residual, style_UNBOUND) : NULL;
    char *residual_value_text = number_text_dup(residual_value);
    size_t *solution_order = ordered_solution_indices(solutions, bindings);

    printf("input       %s\n", input ? input : "");
    printf("equation    %s\n", equation_text ? equation_text : "");
    printf("unbound     %s\n", unbound_text ? unbound_text : "");
    printf("function    %s\n", function_text ? function_text : "");
    printf("tex         %s\n", tex ? tex : "");
    printf("residual    %s\n", residual_text ? residual_text : "");
    printf("error       %s\n", residual_value_text ? residual_value_text : "");
    printf("status      %s\n", status ? status : "unsolved");
    print_solutions(solutions, bindings, solution_order);
    print_solutions_TeX(solutions, bindings, solution_order);
    print_derivation_TeX(tex, solutions, bindings, solution_order);
    print_solution_numerics(solutions, bindings, solution_order, precision);

    free(solution_order);
    free(residual_value_text);
    free(residual_text);
    free(tex);
    free(function_text);
    free(unbound_text);
    free(equation_text);
    equ_free(display_equation);
    expr_free(display_residual);
    num_destroy(&residual_value);
    expr_free(residual);
}

int main(int argc, char **argv)
{
    const char *input = argc > 1 ? argv[1] : "{ 2*x + 3 = 7 | x = NAN }";
    int precision = argc > 2 ? atoi(argv[2]) : 64;
    equation_t *equation;
    equation_solutions_t *solutions = NULL;
    const char *status = "unsolved";
    int rc = 0;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    equation = equ_from_string(input);
    if (!equation) {
        fprintf(stderr, "could not parse equation\n");
        return 1;
    }

    solutions = equ_derive_solutions(equation);
    if (!solutions)
        rc = 1;
    else if (equ_solutions_count(solutions) > 0u)
        status = "solved";

    if (rc == 0)
        print_equation_fields(equation, solutions, equ_bindings(equation), input, status, precision);
    else
        fprintf(stderr, "could not solve equation\n");

    equ_solutions_free(solutions);
    equ_free(equation);
    return rc;
}
