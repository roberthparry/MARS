#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dval.h"

static void trim_fraction_tail(char *text)
{
    char *dot = strchr(text, '.');
    char *end;

    if (!dot)
        return;

    end = text + strlen(text);
    while (end > dot + 1 && end[-1] == '0')
        *--end = '\0';
    if (end == dot + 1)
        *dot = '\0';
}

static char *format_scientific_as_general(char *scientific, int precision)
{
    char *e = strchr(scientific, 'E');
    char *mantissa;
    char *digits;
    char *out;
    char sign = '\0';
    long exponent;
    size_t digit_count = 0u;
    size_t out_cap;
    size_t pos = 0u;
    long decimal_pos;

    if (!e)
        e = strchr(scientific, 'e');
    if (!e) {
        trim_fraction_tail(scientific);
        return scientific;
    }

    exponent = strtol(e + 1, NULL, 10);
    *e = '\0';
    mantissa = scientific;
    if (*mantissa == '-' || *mantissa == '+')
        sign = *mantissa++;

    digits = malloc(strlen(mantissa) + 1u);
    if (!digits)
        return scientific;

    for (char *p = mantissa; *p; ++p) {
        if (isdigit((unsigned char)*p))
            digits[digit_count++] = *p;
    }
    digits[digit_count] = '\0';

    while (digit_count > 1u && digits[digit_count - 1u] == '0')
        digits[--digit_count] = '\0';

    if (exponent < -4 || exponent >= precision) {
        char *dot;

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
        dot = strchr(out, '.');
        if (dot) {
            char *exp_part = strchr(out, 'E');
            char *tail = exp_part;

            while (tail > dot + 1 && tail[-1] == '0') {
                memmove(tail - 1, tail, strlen(tail) + 1u);
                tail--;
                exp_part--;
            }
            if (tail == dot + 1)
                memmove(dot, dot + 1, strlen(dot + 1) + 1u);
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
        memcpy(out + pos, digits, digit_count);
        pos += digit_count;
    } else if ((size_t)decimal_pos >= digit_count) {
        memcpy(out + pos, digits, digit_count);
        pos += digit_count;
        for (size_t i = digit_count; i < (size_t)decimal_pos; ++i)
            out[pos++] = '0';
    } else {
        memcpy(out + pos, digits, (size_t)decimal_pos);
        pos += (size_t)decimal_pos;
        out[pos++] = '.';
        memcpy(out + pos, digits + decimal_pos, digit_count - (size_t)decimal_pos);
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

    num_destroy(&value);

    if (format_is_negligible(imag, precision)) {
        num_destroy(&imag);
        return format_real_number(real, precision);
    }

    if (format_is_negligible(real, precision)) {
        num_destroy(&real);
        imag_text = format_real_number(num_clone(imag), precision);
        if (imag_text) {
            len = strlen(imag_text) + 2u;
            text = malloc(len);
            if (text)
                snprintf(text, len, "%si", imag_text);
        }
        free(imag_text);
        num_destroy(&imag);
        return text;
    }

    real_text = format_real_number(real, precision);
    if (num_get_sign(imag) < 0) {
        number_t abs_imag = num_abs(imag);

        imag_text = format_real_number(abs_imag, precision);
        if (real_text && imag_text) {
            len = strlen(real_text) + strlen(imag_text) + 6u;
            text = malloc(len);
            if (text)
                snprintf(text, len, "%s - %si", real_text, imag_text);
        }
    } else {
        imag_text = format_real_number(num_clone(imag), precision);
        if (real_text && imag_text) {
            len = strlen(real_text) + strlen(imag_text) + 6u;
            text = malloc(len);
            if (text)
                snprintf(text, len, "%s + %si", real_text, imag_text);
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

    if (precision >= 0) {
        text = num_is_real(value)
             ? format_real_number(num_clone(value), precision)
             : format_complex_number(value, precision);
    } else {
        text = num_to_string(value);
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
    dval_t *expr = NULL;
    int rc = 1;

    if (!text || !out)
        return 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(text);
    if (wrapped_input)
        input = wrapped_input;

    expr = dval_from_string(input, NULL);
    if (!expr) {
        fprintf(stderr, "Failed to parse numeric expression:\n  %s\n", input);
        goto cleanup;
    }

    *out = dv_eval(expr);
    rc = 0;

cleanup:
    dv_free(expr);
    free(wrapped_input);
    return rc;
}

static int apply_goal_start(dval_bindings_t *bindings,
                            const char *assignment,
                            int precision)
{
    char *copy;
    char *eq;
    char *name;
    char *value_text;
    dval_t *binding;
    number_t value;
    int rc = 1;

    if (!bindings || !assignment)
        return 1;

    copy = strdup(assignment);
    if (!copy)
        return 1;

    eq = strchr(copy, '=');
    if (!eq) {
        fprintf(stderr, "Goal start must be name=value: %s\n", assignment);
        goto cleanup;
    }

    *eq = '\0';
    name = copy;
    value_text = eq + 1;
    while (isspace((unsigned char)*name))
        name++;
    while (isspace((unsigned char)*value_text))
        value_text++;

    eq--;
    while (eq >= name && isspace((unsigned char)*eq))
        *eq-- = '\0';

    binding = dval_bindings_get(bindings, name);
    if (!binding) {
        fprintf(stderr, "No binding named '%s'\n", name);
        goto cleanup;
    }

    if (parse_number_expression(value_text, precision, &value) != 0)
        goto cleanup;

    dv_set_val(binding, value);
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
    dval_bindings_t *bindings = NULL;
    dval_t *expr = NULL;
    number_t target = (number_t){0};
    number_t tolerance = (number_t){0};
    dv_goal_seek_options_t options = {0};
    dv_goal_seek_result_t result;
    char *expr_text = NULL;
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

    expr = dval_from_string(input, &bindings);
    if (!expr) {
        fprintf(stderr, "Failed to parse expression:\n  %s\n", input);
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

    tolerance = num_pow10(-(precision > 0 ? precision : 64));
    options.precision_digits = precision > 0 ? (size_t)precision : 64u;
    options.max_iterations = 0u;
    options.allow_complex = true;
    options.simplify_result = false;
    options.tolerance = tolerance;

    if (dv_goal_seek(expr, bindings, target, &options, &result) != 0) {
        fprintf(stderr, "Goal seek failed\n");
        goto cleanup;
    }

    expr_text = dv_to_string(result.expr, style_EXPRESSION);
    func_text = dv_to_string(result.expr, style_FUNCTION);
    tex_text = dv_to_string(result.expr, style_TEX);

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    print_owned_number("value", num_clone(result.value), precision);
    print_owned_number("residual", num_clone(result.residual), precision);
    printf("iterations  %zu\n", result.iterations);
    printf("complex     %s\n", result.used_complex ? "yes" : "no");

    dv_goal_seek_result_clear(&result);
    rc = 0;

cleanup:
    free(tex_text);
    free(func_text);
    free(expr_text);
    num_destroy(&tolerance);
    num_destroy(&target);
    dv_free(expr);
    dval_bindings_free(bindings);
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
    dval_bindings_t *bindings = NULL;
    dval_t *expr = NULL;
    dval_t *deriv = NULL;
    dval_t *wrt = NULL;
    char *expr_text = NULL;
    char *func_text = NULL;
    char *tex_text = NULL;
    char *deriv_text = NULL;
    int rc = 0;

    if (argc > 1 && strcmp(argv[1], "--goal-seek") == 0)
        return run_goal_seek(argc, argv);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(raw_input);
    if (wrapped_input)
        input = wrapped_input;

    expr = dval_from_string(input, &bindings);

    if (!expr) {
        fprintf(stderr, "Failed to parse expression:\n  %s\n", input);
        return 1;
    }

    expr_text = dv_to_string(expr, style_EXPRESSION);
    func_text = dv_to_string(expr, style_FUNCTION);
    tex_text = dv_to_string(expr, style_TEX);

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("function    %s\n", func_text ? func_text : "(null)");
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    print_owned_number("value", dv_eval(expr), precision);

    if (bindings)
        wrt = dval_bindings_get(bindings, wrt_name);
    if (wrt) {
        deriv = dv_create_deriv(expr, wrt);
        if (!deriv) {
            fprintf(stderr, "Failed to build derivative with respect to %s\n",
                    wrt_name);
            rc = 1;
            goto cleanup;
        }
        deriv_text = dv_to_string(deriv, style_EXPRESSION);
        printf("derivative  d/d%s = %s\n",
               wrt_name,
               deriv_text ? deriv_text : "(null)");
        print_owned_number("d value", dv_eval(deriv), precision);
    } else {
        printf("derivative  no binding named '%s'\n", wrt_name);
    }

cleanup:
    free(wrapped_input);
    free(deriv_text);
    free(tex_text);
    free(func_text);
    free(expr_text);
    dv_free(deriv);
    dv_free(expr);
    dval_bindings_free(bindings);
    return rc;
}
