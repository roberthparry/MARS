#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "expression.h"
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
    string_t *text = expr_to_text(expr, style);
    char *copy = text ? xstrdup_local(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static int value_is_defined_for_integrand_at(const expr_t *integrand,
                                             const expr_t *var,
                                             number_t point)
{
    expr_t *point_const;
    expr_t *eval_expr;
    number_t value;
    int ok;

    if (!integrand || !var)
        return 0;

    point_const = expr_new_const(point);
    eval_expr = point_const ? expr_substitute(integrand, var, point_const) : NULL;
    if (!point_const || !eval_expr) {
        expr_free(eval_expr);
        expr_free(point_const);
        return 0;
    }

    value = expr_eval(eval_expr);
    ok = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    expr_free(eval_expr);
    expr_free(point_const);
    return ok;
}

static int find_integral_value_note(const expr_t *expr, char *out, size_t out_size)
{
    const expr_t *lower_expr;
    const expr_t *upper_expr;
    const expr_t *dummy_expr;
    expr_t *local_var = NULL;
    expr_t *upper_const = NULL;
    expr_t *lower_const = NULL;
    expr_t *upper_integrand = NULL;
    expr_t *lower_integrand = NULL;
    number_t upper = num_new();
    number_t lower = num_new();
    char *upper_text = NULL;
    char *lower_text = NULL;
    char *integrand_text = NULL;
    const expr_t *integrand = NULL;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;
    int found = 0;

    if (!expr || !out || out_size == 0u)
        return 0;

    if (expr_match_integral_expr(expr, &integrand, NULL)) {
        lower_expr = expr_integral_lower_bound_expr(expr);
        upper_expr = expr_integral_upper_bound_expr(expr);
        dummy_expr = expr_integral_dummy_expr(expr);

        upper = upper_expr ? expr_eval(upper_expr) : num_clone(NUM_NAN);
        lower = lower_expr ? expr_eval(lower_expr) : num_clone(NUM_ZERO);
        if (upper_expr && dummy_expr &&
            num_is_real(lower) && num_is_finite(lower) &&
            num_is_real(upper) && num_is_finite(upper)) {
            local_var = expr_clone(dummy_expr);
            upper_const = expr_new_const(upper);
            lower_const = expr_new_const(lower);
            upper_integrand = (dummy_expr && upper_const)
                ? expr_substitute(integrand, dummy_expr, upper_const)
                : NULL;
            lower_integrand = (dummy_expr && lower_const)
                ? expr_substitute(integrand, dummy_expr, lower_const)
                : NULL;
            if (local_var && upper_const && lower_const &&
                upper_integrand && lower_integrand) {
                upper_text = expr_text_dup(upper_expr, style_UNBOUND);
                lower_text = lower_expr ? expr_text_dup(lower_expr, style_UNBOUND) : NULL;
                integrand_text = expr_text_dup(integrand, style_UNBOUND);

                if (!lower_expr &&
                    !value_is_defined_for_integrand_at(integrand, local_var, NUM_ZERO)) {
                    snprintf(out, out_size,
                             "Here ∫^%s means ∫₀^%s. The integrand %s is not finite at %s = 0, so that definite integral is undefined.",
                             upper_text ? upper_text : "?",
                             upper_text ? upper_text : "?",
                             integrand_text ? integrand_text : "f(t)",
                             expr_symbol_name(local_var) ? expr_symbol_name(local_var) : "t");
                    found = 1;
                } else if (!value_is_defined_for_integrand_at(integrand, local_var, lower)) {
                    snprintf(out, out_size,
                             "The integrand %s is not finite at %s = %s, so that the lower bound makes this definite integral undefined.",
                             integrand_text ? integrand_text : "f(t)",
                             expr_symbol_name(local_var) ? expr_symbol_name(local_var) : "t",
                             lower_text ? lower_text : "0");
                    found = 1;
                } else if (!value_is_defined_for_integrand_at(integrand, local_var, upper)) {
                    snprintf(out, out_size,
                             "The integrand %s is not finite at %s = %s, so that the upper bound makes this definite integral undefined.",
                             integrand_text ? integrand_text : "f(t)",
                             expr_symbol_name(local_var) ? expr_symbol_name(local_var) : "t",
                             upper_text ? upper_text : "?");
                    found = 1;
                }
            }
        }
    }

    free(integrand_text);
    free(lower_text);
    free(upper_text);
    expr_free(lower_integrand);
    expr_free(upper_integrand);
    expr_free(lower_const);
    expr_free(upper_const);
    expr_free(local_var);
    num_destroy(&lower);
    num_destroy(&upper);

    if (found)
        return 1;

    if (!expr_child_exprs(expr, &child_left, &child_right))
        return 0;
    return find_integral_value_note(child_left, out, out_size) ||
           find_integral_value_note(child_right, out, out_size);
}

static char *expr_tex_body_dup(const expr_t *expr)
{
    char *body = NULL;
    char *bindings = NULL;

    if (!expr)
        return NULL;
    if (expr_to_tex_parts(expr, &body, &bindings) == 0) {
        free(bindings);
        return body;
    }
    free(body);
    free(bindings);
    return expr_text_dup(expr, style_TEX);
}

static expr_t *clone_expr_local(const expr_t *expr)
{
    number_t needle_value = num_new();
    expr_t *needle = expr_new_named_var(needle_value, "__mars_display_clone__");
    expr_t *replacement = expr_new_const(NUM_ZERO);
    expr_t *copy = NULL;

    num_destroy(&needle_value);
    if (needle && replacement)
        copy = expr_substitute(expr, needle, replacement);

    expr_free(replacement);
    expr_free(needle);
    return copy;
}

static expr_t *expanded_display_expr(const expr_t *expr);

static expr_t *expanded_display_product(const expr_t *left, const expr_t *right)
{
    expr_t *left_expr;
    expr_t *right_expr;
    expr_t *out;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;
    bool is_sub = false;

    if (!left || !right)
        return NULL;

    if (expr_match_add_sub_expr(left, &child_left, &child_right, &is_sub) &&
        expr_match_add_sub_expr(right, &child_left, &child_right, &is_sub)) {
        left_expr = expanded_display_expr(left);
        right_expr = expanded_display_expr(right);
        if (!left_expr || !right_expr) {
            expr_free(left_expr);
            expr_free(right_expr);
            return NULL;
        }

        out = expr_mul(left_expr, right_expr);
        expr_free(left_expr);
        expr_free(right_expr);
        return out;
    }

    if (expr_match_add_expr(left, &child_left, &child_right)) {
        expr_t *first = expanded_display_product(child_left, right);
        expr_t *second = expanded_display_product(child_right, right);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_add(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_match_sub_expr(left, &child_left, &child_right)) {
        expr_t *first = expanded_display_product(child_left, right);
        expr_t *second = expanded_display_product(child_right, right);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_sub(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_match_add_sub_expr(right, &child_left, &child_right, &is_sub))
        return expanded_display_product(right, left);

    left_expr = expanded_display_expr(left);
    right_expr = expanded_display_expr(right);
    if (!left_expr || !right_expr) {
        expr_free(left_expr);
        expr_free(right_expr);
        return NULL;
    }

    out = expr_mul(left_expr, right_expr);
    expr_free(left_expr);
    expr_free(right_expr);
    return out;
}

static expr_t *expanded_display_expr(const expr_t *expr)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;

    if (!expr)
        return NULL;

    if (expr_match_add_expr(expr, &child_left, &child_right)) {
        left = expanded_display_expr(child_left);
        right = expanded_display_expr(child_right);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_add(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_match_sub_expr(expr, &child_left, &child_right)) {
        left = expanded_display_expr(child_left);
        right = expanded_display_expr(child_right);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_sub(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_match_mul_expr(expr, &child_left, &child_right))
        return expanded_display_product(child_left, child_right);

    return clone_expr_local(expr);
}

static expr_t *display_simplified_expr(const expr_t *expr)
{
    expr_t *expanded;
    expr_t *simplified;

    if (!expr)
        return NULL;

    expanded = expanded_display_expr(expr);
    if (!expanded)
        return expr_simplify(expr);

    simplified = expr_simplify(expanded);
    if (!simplified)
        return expanded;

    expr_free(expanded);
    return simplified;
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

    *out = negative && value == (unsigned long)LONG_MAX + 1u
        ? LONG_MIN
        : (negative ? -(long)value : (long)value);
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
                    memmove(&out[tail_pos - 1u],
                            &out[tail_pos],
                            strlen(&out[tail_pos]) + 1u);
                    tail_pos--;
                }
                if (tail_pos == dot_pos + 1u)
                    memmove(&out[dot_pos],
                            &out[dot_pos + 1u],
                            strlen(&out[dot_pos + 1u]) + 1u);
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
        memcpy(&out[pos],
               &digits[decimal_pos],
               digit_count - (size_t)decimal_pos);
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

static void print_owned_number(const char *label, number_t value, int precision)
{
    char *text = NULL;

    if (num_is_inf(value)) {
        text = xstrdup_local(num_get_sign(value) < 0 ? "-∞" : "∞");
    } else if (precision >= 0) {
        text = num_is_real(value)
             ? format_real_number(num_clone(value), precision)
             : format_complex_number(value, precision);
    } else {
        string_t *number_text = num_to_string(value);

        text = number_text ? xstrdup_local(string_c_str(number_text)) : NULL;
        string_free(number_text);
    }

    printf("%-12s %s\n", label, text ? text : "(num_to_string failed)");
    free(text);
    num_destroy(&value);
}

static char *wrap_expression(const char *raw_input)
{
    size_t n;
    char *wrapped_input;

    if (!raw_input || raw_input[0] == '{')
        return NULL;

    n = strlen(raw_input);
    wrapped_input = malloc(n + 5u);
    if (!wrapped_input)
        return NULL;

    memcpy(wrapped_input, "{ ", 2u);
    memcpy(wrapped_input + 2u, raw_input, n);
    memcpy(wrapped_input + 2u + n, " }", 3u);
    return wrapped_input;
}

static int parse_number_expression(const char *text, int precision, number_t *out)
{
    const char *input = text;
    char *wrapped_input = NULL;
    expr_t *expr = NULL;
    int rc = 1;

    if (!text || !out)
        return 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(text);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, NULL);
    if (!expr) {
        goto cleanup;
    }

    *out = expr_eval(expr);
    rc = 0;

cleanup:
    expr_free(expr);
    free(wrapped_input);
    return rc;
}

static int apply_goal_start(expr_bindings_t *bindings,
                            const char *assignment,
                            int precision)
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
    const char *input;
    const char *target_text;
    int precision;
    char *wrapped_input = NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    number_t target = (number_t){0};
    expr_goal_seek_options_t options = {0};
    expr_goal_seek_result_t result;
    char *expr_text = NULL;
    char *unbound_text = NULL;
    char *func_text = NULL;
    char *tex_text = NULL;
    int rc = 1;

    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s --goal-seek <expression> <target> <precision> [name=start ...]\n",
                argv[0]);
        return 1;
    }

    raw_input = argv[2];
    input = raw_input;
    target_text = argv[3];
    precision = atoi(argv[4]);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(raw_input);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, &bindings);
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
    tex_text = expr_text_dup(result.expr, style_TEX);

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("unbound     %s\n", unbound_text ? unbound_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    print_owned_number("value", num_clone(result.value), precision);
    print_owned_number("residual", num_clone(result.residual), precision);
    printf("iterations  %zu\n", result.iterations);
    printf("complex     %s\n", result.used_complex ? "yes" : "no");

    expr_goal_seek_result_clear(&result);
    rc = 0;

cleanup:
    free(tex_text);
    free(func_text);
    free(unbound_text);
    free(expr_text);
    num_destroy(&target);
    expr_free(expr);
    expr_bindings_free(bindings);
    free(wrapped_input);
    return rc;
}

int main(int argc, char **argv)
{
    const char *raw_input =
        argc > 1 ? argv[1] : "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }";
    const char *input = raw_input;
    const char *wrt_name = argc > 2 ? argv[2] : "x";
    int precision = argc > 3 ? atoi(argv[3]) : -1;
    char *wrapped_input = NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    expr_t *display_expr = NULL;
    expr_t *deriv = NULL;
    expr_t *display_deriv = NULL;
    expr_t *wrt = NULL;
    char *expr_text = NULL;
    char *unbound_text = NULL;
    char *func_text = NULL;
    char *tex_text = NULL;
    char *deriv_text = NULL;
    char *deriv_func_text = NULL;
    char *deriv_tex_text = NULL;
    char value_note[512];
    int rc = 0;

    if (argc > 1 && strcmp(argv[1], "--goal-seek") == 0)
        return run_goal_seek(argc, argv);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(raw_input);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, &bindings);

    if (!expr) {
        return 1;
    }

    display_expr = display_simplified_expr(expr);
    if (!display_expr)
        display_expr = expr;

    expr_text = expr_text_dup(expr, style_EXPRESSION);
    unbound_text = expr_text_dup(display_expr, style_UNBOUND);
    func_text = expr_text_dup(expr, style_FUNCTION);
    tex_text = expr_tex_body_dup(expr);

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("unbound     %s\n", unbound_text ? unbound_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    printf("differentiable  %s\n", expr_is_differentiable(expr) ? "yes" : "no");
    value_note[0] = '\0';
    {
        number_t value_number = expr_eval(expr);

        print_owned_number("value", num_clone(value_number), precision);
        if ((num_is_nan(value_number) || !num_is_finite(value_number)) &&
            find_integral_value_note(expr, value_note, sizeof(value_note)))
            printf("value_note  %s\n", value_note);
        num_destroy(&value_number);
    }

    if (bindings)
        wrt = expr_bindings_get(bindings, wrt_name);
    if (wrt) {
        deriv = expr_create_deriv(expr, wrt);
        if (!deriv) {
            fprintf(stderr, "Failed to build derivative with respect to %s\n",
                    wrt_name);
            rc = 1;
            goto cleanup;
        }
        display_deriv = expr_simplify(deriv);
        if (!display_deriv)
            display_deriv = deriv;
        deriv_text = expr_text_dup(display_deriv, style_EXPRESSION);
        deriv_func_text = expr_text_dup(display_deriv, style_FUNCTION);
        deriv_tex_text = expr_text_dup(display_deriv, style_TEX);
        printf("derivative  d/d%s = %s\n",
               wrt_name,
               deriv_text ? deriv_text : "(null)");
        printf("derivative_function  %s\n",
               deriv_func_text ? deriv_func_text : "(null)");
        printf("derivative_tex  %s\n", deriv_tex_text ? deriv_tex_text : "");
        print_owned_number("d value", expr_eval(deriv), precision);
    } else {
        printf("derivative  no binding named '%s'\n", wrt_name);
    }

cleanup:
    free(wrapped_input);
    free(deriv_tex_text);
    free(deriv_func_text);
    free(deriv_text);
    free(tex_text);
    free(func_text);
    free(unbound_text);
    free(expr_text);
    if (display_deriv && display_deriv != deriv)
        expr_free(display_deriv);
    if (display_expr && display_expr != expr)
        expr_free(display_expr);
    expr_free(deriv);
    expr_free(expr);
    expr_bindings_free(bindings);
    return rc;
}
