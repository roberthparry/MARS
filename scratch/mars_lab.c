#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#include "ustring.h"

static char *xstrdup_local(const char *text)
{
    size_t len;
    char *copy;

    if (!text)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (copy)
        memcpy(copy, text, len + 1u);
    return copy;
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    return expr_to_string(expr, style);
}

static char *expr_TeX_body_dup(const expr_t *expr)
{
    char *body = expr_to_TeX_body_wrapped(expr, 280u);

    return body ? body : expr_text_dup(expr, style_LATEX);
}

static expr_t *display_polynomial_simplified(const expr_t *expr, const expr_t *wrt)
{
    expr_t *zero = NULL;
    expr_t *display = NULL;
    equation_t *polynomial = NULL;
    equation_t *expanded = NULL;
    expr_t *result;
    expr_t *beautified;

    if (!expr)
        return NULL;
    if (expr_contains_integral_operation(expr))
        return NULL;

    if (wrt) {
        zero = expr_new_const(NUM_ZERO);
        polynomial = zero ? equ_new(expr, zero) : NULL;
        expanded = polynomial ? equ_display_expanded(polynomial, wrt) : NULL;
        if (expanded) {
            expr_t *rebound;

            display = expr_clone(equ_lhs(expanded));
            rebound = display ? expr_substitute(display, wrt, wrt) : NULL;
            if (rebound) {
                expr_free(display);
                display = rebound;
            }
        }
    }

    equ_free(expanded);
    equ_free(polynomial);
    expr_free(zero);
    result = display ? display : expr_simplify(expr);
    beautified = result ? expr_beautify_presimplified(result) : NULL;
    if (beautified) {
        expr_free(result);
        result = beautified;
    }
    return result;
}

static char *trim_ascii_in_place(char *text)
{
    size_t start = 0u;
    size_t end;

    if (!text)
        return NULL;

    end = strlen(text);
    while (start < end && isspace((unsigned char)text[start]))
        start++;
    while (end > start && isspace((unsigned char)text[end - 1u]))
        end--;
    if (start > 0u)
        memmove(text, &text[start], end - start);
    text[end - start] = '\0';
    return text;
}

static void trim_fraction_tail(char *text)
{
    size_t dot;
    size_t end;

    if (!text)
        return;
    dot = strcspn(text, ".");
    if (text[dot] != '.')
        return;

    end = strlen(text);
    while (end > dot + 1u && text[end - 1u] == '0') {
        end--;
        text[end] = '\0';
    }
    if (end == dot + 1u)
        text[dot] = '\0';
}

static int parse_long_suffix(const char *text, size_t start, long *out)
{
    size_t i = start;
    unsigned long value = 0u;
    unsigned long limit;
    int negative = 0;
    int saw_digit = 0;

    if (!text || !out)
        return 0;

    if (text[i] == '-' || text[i] == '+') {
        negative = text[i] == '-';
        i++;
    }

    limit = negative ? (unsigned long)LONG_MAX + 1u : (unsigned long)LONG_MAX;
    for (; text[i] != '\0'; ++i) {
        unsigned int digit;

        if (!isdigit((unsigned char)text[i]))
            return 0;
        digit = (unsigned int)(text[i] - '0');
        if (value > (limit - digit) / 10u)
            return 0;
        value = value * 10u + digit;
        saw_digit = 1;
    }

    if (!saw_digit)
        return 0;

    *out = negative && value == (unsigned long)LONG_MAX + 1u ? LONG_MIN : (negative ? -(long)value : (long)value);
    return 1;
}

static char *format_scientific_as_general(char *scientific, int precision)
{
    size_t exponent_pos;
    size_t mantissa_pos = 0u;
    char *digits;
    char *out;
    char sign = '\0';
    long exponent;
    size_t digit_count = 0u;
    size_t out_cap;
    size_t pos = 0u;
    long decimal_pos;

    exponent_pos = strcspn(scientific, "Ee");
    if (scientific[exponent_pos] == '\0') {
        trim_fraction_tail(scientific);
        return scientific;
    }

    if (!parse_long_suffix(scientific, exponent_pos + 1u, &exponent))
        return scientific;
    scientific[exponent_pos] = '\0';
    if (scientific[mantissa_pos] == '-' || scientific[mantissa_pos] == '+') {
        sign = scientific[mantissa_pos];
        mantissa_pos++;
    }

    digits = malloc(strlen(&scientific[mantissa_pos]) + 1u);
    if (!digits)
        return scientific;

    for (size_t i = mantissa_pos; scientific[i] != '\0'; ++i) {
        if (isdigit((unsigned char)scientific[i]))
            digits[digit_count++] = scientific[i];
    }
    digits[digit_count] = '\0';

    while (digit_count > 1u && digits[digit_count - 1u] == '0')
        digits[--digit_count] = '\0';

    if (exponent < -4 || exponent >= precision) {
        if (digit_count > 1u) {
            memmove(digits + 2u, digits + 1u, digit_count);
            digits[1] = '.';
        }
        out_cap = digit_count + 32u;
        out = malloc(out_cap);
        if (!out) {
            free(digits);
            return scientific;
        }
        snprintf(out, out_cap, "%s%sE%+ld", sign == '-' ? "-" : "", digits, exponent);
        free(digits);
        free(scientific);
        {
            size_t dot_pos = strcspn(out, ".");

            if (out[dot_pos] == '.') {
                size_t tail_pos = strcspn(out, "E");

                while (tail_pos > dot_pos + 1u && out[tail_pos - 1u] == '0') {
                    memmove(&out[tail_pos - 1u], &out[tail_pos], strlen(&out[tail_pos]) + 1u);
                    tail_pos--;
                }
                if (tail_pos == dot_pos + 1u)
                    memmove(&out[dot_pos], &out[dot_pos + 1u], strlen(&out[dot_pos + 1u]) + 1u);
            }
        }
        return out;
    }

    decimal_pos = 1 + exponent;
    out_cap = digit_count + (size_t)labs(decimal_pos) + 8u;
    out = malloc(out_cap);
    if (!out) {
        free(digits);
        return scientific;
    }

    if (sign == '-')
        out[pos++] = '-';

    if (decimal_pos <= 0) {
        out[pos++] = '0';
        out[pos++] = '.';
        for (long i = 0; i < -decimal_pos; ++i)
            out[pos++] = '0';
        memcpy(&out[pos], digits, digit_count);
        pos += digit_count;
    } else if ((size_t)decimal_pos >= digit_count) {
        memcpy(&out[pos], digits, digit_count);
        pos += digit_count;
        for (size_t i = digit_count; i < (size_t)decimal_pos; ++i)
            out[pos++] = '0';
    } else {
        memcpy(&out[pos], digits, (size_t)decimal_pos);
        pos += (size_t)decimal_pos;
        out[pos++] = '.';
        memcpy(&out[pos], &digits[decimal_pos], digit_count - (size_t)decimal_pos);
        pos += digit_count - (size_t)decimal_pos;
    }
    out[pos] = '\0';

    trim_fraction_tail(out);
    free(digits);
    free(scientific);
    return out;
}

static char *format_real_number(number_t value, int precision)
{
    number_t zero = num_new();
    number_t floating = num_add(value, zero);
    char fmt[32];
    char *text = NULL;
    int scientific_precision = precision > 0 ? precision - 1 : 0;
    int needed;

    if (num_is_inf(value)) {
        char *text = xstrdup_local(num_get_sign(value) < 0 ? "-∞" : "∞");

        num_destroy(&zero);
        num_destroy(&value);
        return text;
    }

    num_destroy(&zero);
    num_destroy(&value);
    value = floating;

    snprintf(fmt, sizeof(fmt), "%%.%dN", scientific_precision);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed >= 0) {
        text = malloc((size_t)needed + 1u);
        if (text) {
            num_sprintf(text, (size_t)needed + 1u, fmt, value);
            text = format_scientific_as_general(text, precision);
        }
    }

    num_destroy(&value);
    return text;
}

static bool format_is_negligible(number_t value, int precision)
{
    number_t mag = num_abs(value);
    number_t tolerance = num_pow10(-(precision > 0 ? precision : 64));
    bool negligible = num_lt(mag, tolerance);

    num_destroy(&tolerance);
    num_destroy(&mag);
    return negligible;
}

static char *format_complex_number(number_t value, int precision)
{
    number_t real = num_real_part(value);
    number_t imag = num_imag_part(value);
    char *real_text = NULL;
    char *imag_text = NULL;
    char *text = NULL;
    size_t len;

    if (format_is_negligible(imag, precision)) {
        num_destroy(&imag);
        return format_real_number(real, precision);
    }

    if (format_is_negligible(real, precision)) {
        num_destroy(&real);
        imag_text = format_real_number(num_clone(imag), precision);
        if (imag_text) {
            if (strcmp(imag_text, "1") == 0) {
                text = xstrdup_local("i");
            } else if (strcmp(imag_text, "-1") == 0) {
                text = xstrdup_local("-i");
            } else {
                len = strlen(imag_text) + 2u;
                text = malloc(len);
                if (text)
                    snprintf(text, len, "%si", imag_text);
            }
        }
        free(imag_text);
        num_destroy(&imag);
        return text;
    }

    real_text = format_real_number(real, precision);
    if (num_get_sign(imag) < 0) {
        number_t abs_imag = num_abs(imag);
        bool imag_is_unit = num_eq(abs_imag, NUM_ONE);

        imag_text = format_real_number(abs_imag, precision);
        if (real_text && imag_text) {
            const char *imag_coeff = imag_is_unit ? "" : imag_text;

            len = strlen(real_text) + strlen(imag_coeff) + 6u;
            text = malloc(len);
            if (text)
                snprintf(text, len, "%s - %si", real_text, imag_coeff);
        }
    } else {
        number_t abs_imag = num_clone(imag);
        bool imag_is_unit = num_eq(imag, NUM_ONE);

        imag_text = format_real_number(abs_imag, precision);
        if (real_text && imag_text) {
            const char *imag_coeff = imag_is_unit ? "" : imag_text;

            len = strlen(real_text) + strlen(imag_coeff) + 6u;
            text = malloc(len);
            if (text)
                snprintf(text, len, "%s + %si", real_text, imag_coeff);
        }
    }

    free(imag_text);
    free(real_text);
    num_destroy(&imag);
    return text;
}

static char *owned_number_text(number_t value, int precision)
{
    char *text = NULL;
    int display_precision = precision > 0 ? precision : 64;

    if (num_is_nan(value)) {
        text = xstrdup_local("NAN");
    } else if (num_is_inf(value)) {
        text = xstrdup_local(num_get_sign(value) < 0 ? "-∞" : "∞");
    } else {
        text = num_is_real(value) ? format_real_number(num_clone(value), display_precision)
                                  : format_complex_number(value, display_precision);
    }

    num_destroy(&value);
    return text;
}

static void print_owned_number(const char *label, number_t value, int precision)
{
    char *text = owned_number_text(value, precision);

    printf("%-12s %s\n", label, text ? text : "(num_to_string failed)");
    free(text);
}

static bool explicit_reciprocal_power(const expr_t *expr, const expr_t **base_out, long *order_out)
{
    const expr_t *base = NULL;
    number_t exponent = (number_t){0};
    bool matched;

    if (!expr || !base_out || !order_out)
        return false;

    matched = expr_explicit_root_order(expr, order_out);
    if (expr_match_pow_const(expr, &base, &exponent))
        *base_out = base;
    num_destroy(&exponent);
    if (!matched)
        return false;
    return true;
}

static void print_explicit_root_family(const expr_t *expr)
{
    number_t seed = (number_t){0};
    const expr_t *base = NULL;
    long order = 0L;
    expr_t *seed_expr = NULL;
    expr_t *negative_seed = NULL;
    char *seed_expression;
    char *seed_TeX;
    char *negative_expression = NULL;
    char *negative_TeX = NULL;
    char *root_expression = NULL;
    char *root_TeX = NULL;
    char *root_function = NULL;
    size_t root_expression_size = 0u;
    size_t root_TeX_size = 0u;
    size_t root_function_size = 0u;
    FILE *expression_stream;
    FILE *TeX_stream;
    FILE *function_stream;
    bool exact_seed;

    if (!explicit_reciprocal_power(expr, &base, &order)) {
        num_destroy(&seed);
        return;
    }

    exact_seed = expr_exact_complex_root_seed(expr, &seed, &order);
    if (exact_seed) {
        seed_expr = expr_new_const(seed);
    } else if (order == 2L && base) {
        seed_expr = expr_sqrt(base);
    } else {
        seed_expr = expr_clone(expr);
    }
    seed_expression = seed_expr ? expr_text_dup(seed_expr, style_UNBOUND) : NULL;
    seed_TeX = seed_expr ? expr_TeX_body_dup(seed_expr) : NULL;
    if (!seed_expression || !seed_TeX)
        goto cleanup;

    if (order == 2L) {
        expr_t *raw_negative = expr_neg(seed_expr);

        negative_seed = exact_seed && raw_negative ? expr_simplify(raw_negative) : raw_negative;
        if (negative_seed == raw_negative)
            raw_negative = NULL;
        expr_free(raw_negative);
        negative_expression = negative_seed ? expr_text_dup(negative_seed, style_UNBOUND) : NULL;
        negative_TeX = negative_seed ? expr_TeX_body_dup(negative_seed) : NULL;
        if (!negative_expression || !negative_TeX)
            goto cleanup;
    }

    expression_stream = open_memstream(&root_expression, &root_expression_size);
    TeX_stream = open_memstream(&root_TeX, &root_TeX_size);
    function_stream = open_memstream(&root_function, &root_function_size);
    if (!expression_stream || !TeX_stream || !function_stream) {
        if (expression_stream)
            fclose(expression_stream);
        if (TeX_stream)
            fclose(TeX_stream);
        if (function_stream)
            fclose(function_stream);
        goto cleanup;
    }

    fputs("{ ", expression_stream);
    fputs("\\left\\{", TeX_stream);
    for (long root_index = 0L; root_index < order; ++root_index) {
        if (root_index > 0L) {
            fputs(", ", expression_stream);
            fputs(",\\;", TeX_stream);
        }
        if (root_index == 0L) {
            fputs(seed_expression, expression_stream);
            fputs(seed_TeX, TeX_stream);
        } else if (order == 2L) {
            fputs(negative_expression, expression_stream);
            fputs(negative_TeX, TeX_stream);
        } else {
            fprintf(expression_stream, "(%s)·exp(%ld·π·i/%ld)", seed_expression, 2L * root_index, order);
            fprintf(TeX_stream, "\\left(%s\\right)e^{\\frac{%ld\\pi i}{%ld}}", seed_TeX, 2L * root_index,
                    order);
        }
    }
    fputs(" }", expression_stream);
    fputs("\\right\\}", TeX_stream);

    fprintf(function_stream,
            "expression root(k) {\n    return (%s) * exp(2 * pi * i * k / %ld);\n}\n\nk = { ",
            seed_expression, order);
    for (long root_index = 0L; root_index < order; ++root_index) {
        if (root_index > 0L)
            fputs(", ", function_stream);
        fprintf(function_stream, "%ld", root_index);
    }
    fputs(" }\noutput(root(k));", function_stream);

    fclose(function_stream);
    fclose(TeX_stream);
    fclose(expression_stream);
    printf("root_expression  %s\n", root_expression);
    printf("root_tex    %s\n", root_TeX);
    printf("root_function  %s\n", root_function);
    if (exact_seed)
        printf("root_value  %s\n", root_expression);

cleanup:
    free(root_function);
    free(root_TeX);
    free(root_expression);
    free(seed_TeX);
    free(seed_expression);
    free(negative_TeX);
    free(negative_expression);
    expr_free(negative_seed);
    expr_free(seed_expr);
    num_destroy(&seed);
}

static void print_bindings(const char *label, expr_bindings_t *bindings, int precision)
{
    size_t count = expr_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding = expr_bindings_expr_at(bindings, i);
        char *value_text;

        if (!name || !binding)
            continue;

        value_text = owned_number_text(expr_get_val(binding), precision);
        printf("%-20s %s\t%s\t%s\n", label, expr_bindings_is_constant_at(bindings, i) ? "constant" : "variable", name,
               value_text ? value_text : "(num_to_string failed)");
        free(value_text);
    }
}

static bool expression_evaluation_ready(const expr_t *expr)
{
    return expr != NULL;
}

static void print_expression_bindings(const char *label, const char *expression_text, int precision)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr;

    if (!expression_text)
        return;

    expr = expr_from_string(expression_text, &bindings);
    if (!expr)
        return;

    print_bindings(label, bindings, precision);
    expr_free(expr);
    expr_bindings_free(bindings);
}

static void preserve_matching_binding_values(expr_bindings_t *bindings, const char *source_expression)
{
    expr_bindings_t *source_bindings = NULL;
    expr_t *source_expr;
    size_t count;

    if (!bindings || !source_expression || source_expression[0] == '\0')
        return;

    source_expr = expr_from_string(source_expression, &source_bindings);
    if (!source_expr)
        return;

    count = expr_bindings_count(bindings);
    for (size_t i = 0u; i < count; ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding = expr_bindings_expr_at(bindings, i);
        expr_t *source_binding;

        if (!name || !binding)
            continue;
        source_binding = expr_bindings_get(source_bindings, name);
        if (source_binding) {
            number_t value = expr_get_val(source_binding);

            expr_set_val(binding, value);
            num_destroy(&value);
        }
    }

    expr_free(source_expr);
    expr_bindings_free(source_bindings);
}

static int parse_number_expression(const char *text, int precision, number_t *out)
{
    expr_t *expr = NULL;
    int rc = 1;

    if (!text || !out)
        return 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    expr = expr_from_string(text, NULL);
    if (!expr) {
        goto cleanup;
    }

    *out = expr_eval(expr);
    rc = 0;

cleanup:
    expr_free(expr);
    return rc;
}

static int apply_goal_start(expr_bindings_t *bindings, const char *assignment, int precision)
{
    char *copy;
    char *name;
    char *value_text;
    size_t eq_pos;
    expr_t *binding;
    number_t value;
    int rc = 1;

    if (!bindings || !assignment)
        return 1;

    copy = xstrdup_local(assignment);
    if (!copy)
        return 1;

    eq_pos = strcspn(copy, "=");
    if (copy[eq_pos] != '=') {
        fprintf(stderr, "Goal start must be name=value: %s\n", assignment);
        goto cleanup;
    }

    copy[eq_pos] = '\0';
    name = trim_ascii_in_place(copy);
    value_text = trim_ascii_in_place(&copy[eq_pos + 1u]);

    binding = expr_bindings_get(bindings, name);
    if (!binding) {
        fprintf(stderr, "No binding named '%s'\n", name);
        goto cleanup;
    }

    if (parse_number_expression(value_text, precision, &value) != 0)
        goto cleanup;

    expr_set_val(binding, value);
    num_destroy(&value);
    rc = 0;

cleanup:
    free(copy);
    return rc;
}

static int run_goal_seek(int argc, char **argv)
{
    const char *raw_input;
    const char *target_text;
    int precision;
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    number_t target = (number_t){0};
    expr_goal_seek_options_t options = {0};
    expr_goal_seek_result_t result;
    char *expr_text = NULL;
    char *unbound_text = NULL;
    char *func_text = NULL;
    char *TeX_text = NULL;
    int rc = 1;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s --goal-seek <expression> <target> <precision> [name=start ...]\n", argv[0]);
        return 1;
    }

    raw_input = argv[2];
    target_text = argv[3];
    precision = atoi(argv[4]);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    expr = expr_from_string(raw_input, &bindings);
    if (!expr) {
        goto cleanup;
    }
    if (!bindings) {
        fprintf(stderr, "Goal seek needs variable bindings\n");
        goto cleanup;
    }

    for (int i = 5; i < argc; ++i) {
        if (apply_goal_start(bindings, argv[i], precision) != 0)
            goto cleanup;
    }

    if (parse_number_expression(target_text, precision, &target) != 0)
        goto cleanup;

    options.precision_digits = precision > 0 ? (size_t)precision : 64u;
    options.max_iterations = 0u;
    options.allow_complex = true;
    options.simplify_result = false;

    if (expr_goal_seek(expr, bindings, target, &options, &result) != 0) {
        fprintf(stderr, "Goal seek failed\n");
        goto cleanup;
    }

    expr_text = expr_text_dup(result.expr, style_EXPRESSION);
    unbound_text = expr_text_dup(result.expr, style_UNBOUND);
    func_text = expr_text_dup(result.expr, style_FUNCTION);
    TeX_text = expr_TeX_body_dup(result.expr);

    printf("input       %s\n", raw_input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("unbound     %s\n", unbound_text ? unbound_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", TeX_text ? TeX_text : "(null)");
    print_bindings("binding", bindings, precision);
    print_owned_number("value", num_clone(result.value), precision);
    print_owned_number("residual", num_clone(result.residual), precision);
    printf("iterations  %zu\n", result.iterations);
    printf("complex     %s\n", result.used_complex ? "yes" : "no");

    expr_goal_seek_result_clear(&result);
    rc = 0;

cleanup:
    free(TeX_text);
    free(func_text);
    free(unbound_text);
    free(expr_text);
    num_destroy(&target);
    expr_free(expr);
    expr_bindings_free(bindings);
    return rc;
}

int main(int argc, char **argv)
{
    const char *raw_input = argc > 1 ? argv[1] : "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }";
    const char *wrt_name = argc > 2 ? argv[2] : "x";
    int precision = argc > 3 ? atoi(argv[3]) : -1;
    const char *action = argc > 4 ? argv[4] : "";
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *display_expr = NULL;
    expr_t *deriv = NULL;
    expr_t *display_deriv = NULL;
    expr_t *integral = NULL;
    expr_t *display_integral = NULL;
    expr_t *wrt = NULL;
    char *expr_text = NULL;
    char *unbound_text = NULL;
    char *func_text = NULL;
    char *TeX_text = NULL;
    char *deriv_text = NULL;
    char *deriv_func_text = NULL;
    char *deriv_TeX_text = NULL;
    char *integral_text = NULL;
    char *integral_func_text = NULL;
    char *integral_TeX_text = NULL;
    char value_note[512];
    bool integral_request = strcmp(action, "integral") == 0;
    bool bindings_request = strcmp(action, "bindings") == 0;
    bool binding_edit_request = strcmp(action, "binding-edit") == 0;
    bool evaluate_request = strcmp(action, "evaluate") == 0 || bindings_request || binding_edit_request;
    bool derivative_request = !integral_request && !evaluate_request;
    bool wrt_is_variable = false;
    bool display_expr_owned = false;
    bool display_deriv_owned = false;
    int rc = 0;

    if (argc > 1 && strcmp(argv[1], "--goal-seek") == 0)
        return run_goal_seek(argc, argv);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    expr = expr_from_string(raw_input, &bindings);

    if (!expr) {
        rc = 1;
        goto cleanup;
    }

    if (binding_edit_request) {
        expr_bindings_t *edited_bindings = NULL;
        expr_t *edited = expr_edit_binding(expr, bindings, wrt_name, argc > 5 ? argv[5] : "", &edited_bindings);

        if (!edited) {
            fprintf(stderr, "Could not edit binding '%s'\n", wrt_name);
            rc = 1;
            goto cleanup;
        }
        expr_free(expr);
        expr_bindings_free(bindings);
        expr = edited;
        bindings = edited_bindings;
    }

    if (bindings_request && argc > 5)
        preserve_matching_binding_values(bindings, argv[5]);

    if (bindings)
        wrt = expr_bindings_get(bindings, wrt_name);
    wrt_is_variable = wrt && expr_is_variable(wrt);
    if (evaluate_request) {
        display_expr = display_polynomial_simplified(expr, wrt_is_variable ? wrt : NULL);
        display_expr_owned = display_expr != NULL;
    }
    if (!display_expr)
        display_expr = expr;

    expr_text = expr_text_dup(display_expr, style_EXPRESSION);
    unbound_text = expr_text_dup(display_expr, style_UNBOUND);
    func_text = expr_text_dup(display_expr, style_FUNCTION);
    TeX_text = expr_TeX_body_dup(display_expr);

    printf("input       %s\n", raw_input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("unbound     %s\n", unbound_text ? unbound_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", TeX_text ? TeX_text : "(null)");
    print_bindings("binding", bindings, precision);
    printf("differentiable  %s\n", expr_is_differentiable(expr) ? "yes" : "no");
    printf("evaluation_ready  %s\n", expression_evaluation_ready(expr) ? "yes" : "no");
    if (evaluate_request)
        print_explicit_root_family(expr);
    value_note[0] = '\0';
    {
        number_t value_number = expr_eval(expr);

        print_owned_number("value", num_clone(value_number), precision);
        if (expr_integral_value_note(expr, value_note, sizeof(value_note)))
            printf("value_note  %s\n", value_note);
        num_destroy(&value_number);
    }

    if (wrt_is_variable && derivative_request) {
        deriv = expr_create_deriv(expr, wrt);
        if (!deriv) {
            fprintf(stderr, "Failed to build derivative with respect to %s\n", wrt_name);
            rc = 1;
            goto cleanup;
        }
        display_deriv = display_polynomial_simplified(deriv, wrt);
        if (display_deriv) {
            display_deriv_owned = true;
        } else {
            display_deriv = deriv;
        }
        deriv_text = expr_text_dup(display_deriv, style_EXPRESSION);
        deriv_func_text = expr_text_dup(display_deriv, style_FUNCTION);
        deriv_TeX_text = expr_TeX_body_dup(display_deriv);
        printf("derivative  d/d%s = %s\n", wrt_name, deriv_text ? deriv_text : "(null)");
        printf("derivative_function  %s\n", deriv_func_text ? deriv_func_text : "(null)");
        printf("derivative_TeX  %s\n", deriv_TeX_text ? deriv_TeX_text : "");
        print_expression_bindings("derivative_binding", deriv_text, precision);
        print_owned_number("d value", expr_eval(deriv), precision);
    } else if (derivative_request) {
        printf("derivative  no variable binding named '%s'\n", wrt_name);
    }

    if (integral_request) {
        if (wrt_is_variable) {
            integral = expr_integrate_family(expr, wrt);
            if (!integral) {
                printf("integral  no symbolic integral with respect to %s\n", wrt_name);
            } else {
                display_integral = integral;
                integral_text = expr_text_dup(display_integral, style_EXPRESSION);
                integral_func_text = expr_text_dup(display_integral, style_FUNCTION);
                integral_TeX_text = expr_TeX_body_dup(display_integral);
                printf("integral  ∫d%s = %s\n", wrt_name, integral_text ? integral_text : "(null)");
                printf("integral_function  %s\n", integral_func_text ? integral_func_text : "(null)");
                printf("integral_TeX  %s\n", integral_TeX_text ? integral_TeX_text : "");
                print_expression_bindings("integral_binding", integral_text, precision);
                print_owned_number("i value", expr_eval(integral), precision);
            }
        } else {
            printf("integral  no variable binding named '%s'\n", wrt_name);
        }
    }

cleanup:
    if (display_expr_owned)
        expr_free(display_expr);
    free(integral_TeX_text);
    free(integral_func_text);
    free(integral_text);
    free(deriv_TeX_text);
    free(deriv_func_text);
    free(deriv_text);
    free(TeX_text);
    free(func_text);
    free(unbound_text);
    free(expr_text);
    if (display_deriv_owned)
        expr_free(display_deriv);
    if (display_integral && display_integral != integral)
        expr_free(display_integral);
    expr_free(integral);
    expr_free(deriv);
    expr_free(expr);
    expr_bindings_free(bindings);
    return rc;
}
