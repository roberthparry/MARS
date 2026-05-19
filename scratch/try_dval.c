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

static void print_owned_number(const char *label, number_t value, int precision)
{
    char *text = NULL;

    if (precision >= 0) {
        int needed;
        char fmt[32];
        int scientific_precision = precision > 0 ? precision - 1 : 0;

        snprintf(fmt, sizeof(fmt), "%%.%dN", scientific_precision);
        needed = num_sprintf(NULL, 0u, fmt, value);
        if (needed >= 0) {
            text = malloc((size_t)needed + 1u);
            if (text) {
                num_sprintf(text, (size_t)needed + 1u, fmt, value);
                text = format_scientific_as_general(text, precision);
            }
        }
    } else {
        text = num_to_string(value);
    }

    printf("%-12s %s\n", label, text ? text : "(num_to_string failed)");
    free(text);
    num_destroy(&value);
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
    const dval_t *deriv = NULL;
    dval_t *wrt = NULL;
    char *expr_text = NULL;
    char *func_text = NULL;
    char *tex_text = NULL;
    char *deriv_text = NULL;
    int rc = 0;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    if (raw_input[0] != '{') {
        size_t n = strlen(raw_input);

        wrapped_input = malloc(n + 5u);
        if (!wrapped_input) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        memcpy(wrapped_input, "{ ", 2u);
        memcpy(wrapped_input + 2u, raw_input, n);
        memcpy(wrapped_input + 2u + n, " }", 3u);
        input = wrapped_input;
    }

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
    dv_free(expr);
    dval_bindings_free(bindings);
    return rc;
}
