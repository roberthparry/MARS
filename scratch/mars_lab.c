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

static bool replace_literal_once_owned(char **text, const char *needle, const char *replacement)
{
    char *match;
    char *replaced;
    size_t prefix_length;
    size_t needle_length;
    size_t replacement_length;
    size_t suffix_length;

    if (!text || !*text || !needle || !replacement)
        return false;
    match = strstr(*text, needle);
    if (!match)
        return true;
    prefix_length = (size_t)(match - *text);
    needle_length = strlen(needle);
    replacement_length = strlen(replacement);
    suffix_length = strlen(match + needle_length);
    replaced = malloc(prefix_length + replacement_length + suffix_length + 1u);
    if (!replaced)
        return false;
    memcpy(replaced, *text, prefix_length);
    memcpy(replaced + prefix_length, replacement, replacement_length);
    memcpy(replaced + prefix_length + replacement_length, match + needle_length, suffix_length + 1u);
    free(*text);
    *text = replaced;
    return true;
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

static bool explicit_reciprocal_power(const expr_t *expr, expr_t **base_out, long *order_out)
{
    if (!expr || !base_out || !order_out)
        return false;
    *base_out = expr_explicit_root_base(expr, order_out);
    return *base_out != NULL;
}

static bool root_family_texts_dup(const expr_t *seed, long order, char **expression_out, char **function_out)
{
    expr_bindings_t *bindings = NULL;
    char *source = NULL;
    size_t source_size = 0u;
    FILE *stream;
    expr_t *rotation_expr = NULL;
    expr_t *family_expr = NULL;
    char *expression = NULL;
    char *function = NULL;
    char *name;

    if (!seed || order < 2L || !expression_out || !function_out)
        return false;
    *expression_out = NULL;
    *function_out = NULL;
    stream = open_memstream(&source, &source_size);
    if (!stream)
        return false;
    fputs("{ ", stream);
    if (order == 2L)
        fputs("(-1)^k", stream);
    else if (order % 2L == 0L)
        fprintf(stream, "exp(i*k*pi/%ld)", order / 2L);
    else
        fprintf(stream, "exp(2*i*k*pi/%ld)", order);
    fputs(" | ; k = [", stream);
    for (long root_index = 0L; root_index < order; ++root_index) {
        if (root_index > 0L)
            fputs(", ", stream);
        fprintf(stream, "%ld", root_index);
    }
    fputs("] }", stream);
    fclose(stream);

    rotation_expr = expr_from_string(source, &bindings);
    family_expr = rotation_expr ? expr_mul(seed, rotation_expr) : NULL;
    expression = family_expr ? expr_text_dup(family_expr, style_EXPRESSION) : NULL;
    function = family_expr ? expr_text_dup(family_expr, style_FUNCTION) : NULL;
    if (function) {
        if (!replace_literal_once_owned(&function, "expression expr(", "expression roots(") ||
            !replace_literal_once_owned(&function, "output(expr(", "output(roots(")) {
            free(function);
            function = NULL;
        }
    }
    if (function) {
        name = strstr(function, "\narray const k = [");
        if (name)
            memmove(name + 1u, name + 1u + strlen("array "), strlen(name + 1u + strlen("array ")) + 1u);
        name = strstr(function, "π.i.k");
        if (name)
            memcpy(name, "i.k.π", strlen("i.k.π"));
    }

    expr_free(family_expr);
    expr_free(rotation_expr);
    expr_bindings_free(bindings);
    free(source);
    if (!expression || !function) {
        free(expression);
        free(function);
        return false;
    }
    *expression_out = expression;
    *function_out = function;
    return true;
}

static char *numeric_root_family_dup(const number_t seed, long order, int precision)
{
    char *roots = NULL;
    size_t roots_size = 0u;
    FILE *stream;

    if (!num_is_finite(seed) || order < 2L)
        return NULL;
    stream = open_memstream(&roots, &roots_size);
    if (!stream)
        return NULL;

    fputs("[", stream);
    for (long root_index = 0L; root_index < order; ++root_index) {
        number_t root;
        char *root_text;

        if (root_index == 0L) {
            root = num_clone(seed);
        } else if (order == 2L) {
            root = num_neg(seed);
        } else {
            number_t turn = num_new();
            number_t angle;
            number_t sine = num_new();
            number_t cosine = num_new();
            number_t imaginary;
            number_t rotation;

            num_set_frac(&turn, 2L * root_index, order);
            angle = num_mul(NUM_PI, turn);
            num_sincos(angle, &sine, &cosine);
            imaginary = num_mul(NUM_I, sine);
            rotation = num_add(cosine, imaginary);
            root = num_mul(seed, rotation);
            num_destroy(&rotation);
            num_destroy(&imaginary);
            num_destroy(&cosine);
            num_destroy(&sine);
            num_destroy(&angle);
            num_destroy(&turn);
        }

        root_text = owned_number_text(root, precision);
        if (root_index > 0L)
            fputs(", ", stream);
        fputs(root_text ? root_text : "NAN", stream);
        free(root_text);
    }
    fputs("]", stream);
    fclose(stream);
    return roots;
}

static expr_t *expr_principal_root_display(const expr_t *base, long order)
{
    expr_bindings_t *bindings = NULL;
    char *base_text = base ? expr_text_dup(base, style_UNBOUND) : NULL;
    char *source = NULL;
    size_t source_size = 0u;
    FILE *stream;
    expr_t *parsed = NULL;
    expr_t *display = NULL;

    if (!base_text)
        return NULL;
    stream = open_memstream(&source, &source_size);
    if (stream) {
        fprintf(stream, "root(%s,%ld)", base_text, order);
        fclose(stream);
        parsed = expr_from_string(source, &bindings);
        display = parsed ? expr_beautify_presimplified(parsed) : NULL;
    }
    expr_bindings_free(bindings);
    expr_free(parsed);
    free(source);
    free(base_text);
    return display;
}

static bool expr_imaginary_product_parts(const expr_t *expr, const expr_t **coefficient_out, int *sign_out)
{
    const expr_t *left;
    const expr_t *right;
    number_t value = (number_t){0};
    bool matched = false;

    if (!expr || !coefficient_out || !sign_out || !expr_match_mul_expr(expr, &left, &right))
        goto cleanup;
    value = expr_eval(left);
    if (num_eq(value, NUM_I) || num_eq(value, NUM_NEG_I)) {
        *coefficient_out = right;
        *sign_out = num_eq(value, NUM_I) ? 1 : -1;
        matched = true;
        goto cleanup;
    }
    num_destroy(&value);
    value = (number_t){0};
    value = expr_eval(right);
    if (num_eq(value, NUM_I) || num_eq(value, NUM_NEG_I)) {
        *coefficient_out = left;
        *sign_out = num_eq(value, NUM_I) ? 1 : -1;
        matched = true;
    }

cleanup:
    num_destroy(&value);
    return matched;
}

static bool expr_scaled_cartesian_parts(const expr_t *expr, const expr_t **scale_out, const expr_t **real_out,
                                        const expr_t **imaginary_out, int *imaginary_sign_out)
{
    const expr_t *left;
    const expr_t *right;
    const expr_t *inner_left;
    const expr_t *inner_right;
    bool subtract;
    int imaginary_sign;

    if (!expr || !scale_out || !real_out || !imaginary_out || !imaginary_sign_out)
        return false;
    *scale_out = NULL;
    if (expr_match_add_sub_expr(expr, &inner_left, &inner_right, &subtract) &&
        expr_imaginary_product_parts(inner_right, imaginary_out, &imaginary_sign)) {
        *real_out = inner_left;
        *imaginary_sign_out = subtract ? -imaginary_sign : imaginary_sign;
        return true;
    }
    if (!expr_match_mul_expr(expr, &left, &right))
        return false;
    if (expr_match_add_sub_expr(right, &inner_left, &inner_right, &subtract) &&
        expr_imaginary_product_parts(inner_right, imaginary_out, &imaginary_sign)) {
        *scale_out = left;
        *real_out = inner_left;
        *imaginary_sign_out = subtract ? -imaginary_sign : imaginary_sign;
        return true;
    }
    if (expr_match_add_sub_expr(left, &inner_left, &inner_right, &subtract) &&
        expr_imaginary_product_parts(inner_right, imaginary_out, &imaginary_sign)) {
        *scale_out = right;
        *real_out = inner_left;
        *imaginary_sign_out = subtract ? -imaginary_sign : imaginary_sign;
        return true;
    }
    return false;
}

static char *expr_quarter_turn_root_TeX_dup(const expr_t *seed, long root_index)
{
    const expr_t *scale;
    const expr_t *real;
    const expr_t *imaginary;
    const expr_t *real_source;
    const expr_t *imaginary_source;
    char *scale_TeX = NULL;
    char *real_TeX = NULL;
    char *imaginary_TeX = NULL;
    char *display_TeX = NULL;
    FILE *stream = NULL;
    size_t display_TeX_size = 0U;
    int seed_imaginary_sign;
    int real_sign;
    int imaginary_sign;

    if (!seed || root_index < 0L || root_index > 3L ||
        !expr_scaled_cartesian_parts(seed, &scale, &real, &imaginary, &seed_imaginary_sign))
        return root_index == 0L ? expr_TeX_body_dup(seed) : NULL;

    switch (root_index) {
        case 0L:
            real_source = real;
            imaginary_source = imaginary;
            real_sign = 1;
            imaginary_sign = seed_imaginary_sign;
            break;

        case 1L:
            real_source = imaginary;
            imaginary_source = real;
            real_sign = -seed_imaginary_sign;
            imaginary_sign = 1;
            break;

        case 2L:
            real_source = real;
            imaginary_source = imaginary;
            real_sign = -1;
            imaginary_sign = -seed_imaginary_sign;
            break;

        default:
            real_source = imaginary;
            imaginary_source = real;
            real_sign = seed_imaginary_sign;
            imaginary_sign = -1;
            break;
    }

    scale_TeX = scale ? expr_TeX_body_dup(scale) : NULL;
    real_TeX = expr_TeX_body_dup(real_source);
    imaginary_TeX = expr_TeX_body_dup(imaginary_source);
    if (!real_TeX || !imaginary_TeX || (scale && !scale_TeX))
        goto cleanup;
    stream = open_memstream(&display_TeX, &display_TeX_size);
    if (!stream)
        goto cleanup;
    if (scale_TeX) {
        fputs(scale_TeX, stream);
        fputs("\\mkern-2mu \\left(", stream);
    }
    if (real_sign < 0)
        fputs("\\mathord{-}\\mkern-2mu ", stream);
    fputs(real_TeX, stream);
    fputs(imaginary_sign > 0 ? " + i\\mkern-2mu " : " - i\\mkern-2mu ", stream);
    fputs(imaginary_TeX, stream);
    if (scale_TeX)
        fputs("\\right)", stream);
    fclose(stream);
    stream = NULL;

cleanup:
    if (stream)
        fclose(stream);
    free(imaginary_TeX);
    free(real_TeX);
    free(scale_TeX);
    return display_TeX;
}

static bool expr_contains_root_turn_trig(const expr_t *expr)
{
    char *text = expr ? expr_text_dup(expr, style_UNBOUND) : NULL;
    bool contains_trig = text && (strstr(text, "sin(") || strstr(text, "cos("));

    free(text);
    return contains_trig;
}

static expr_t *expr_root_turn_trig_display(long root_index, long order, bool sine)
{
    expr_bindings_t *bindings = NULL;
    char source[96];
    expr_t *parsed;
    expr_t *display;

    if (root_index < 0L || order < 2L)
        return NULL;
    snprintf(source, sizeof(source), "%s(%ld*pi/%ld)", sine ? "sin" : "cos", 2L * root_index, order);
    parsed = expr_from_string(source, &bindings);
    display = parsed ? expr_beautify(parsed) : NULL;
    expr_free(parsed);
    expr_bindings_free(bindings);
    if (expr_contains_root_turn_trig(display)) {
        expr_free(display);
        return NULL;
    }
    return display;
}

static expr_t *expr_reorder_positive_sum_for_display(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    number_t left_value = (number_t){0};
    number_t right_value = (number_t){0};
    expr_t *left_magnitude = NULL;
    expr_t *reordered = NULL;
    bool subtract;

    if (!expr_match_add_sub_expr(expr, &left, &right, &subtract) || subtract)
        goto cleanup;
    left_value = expr_eval(left);
    right_value = expr_eval(right);
    if (!num_is_real(left_value) || !num_is_real(right_value) || num_get_sign(left_value) >= 0 ||
        num_get_sign(right_value) < 0)
        goto cleanup;
    left_magnitude = expr_distribute_negative_for_display(left);
    reordered = left_magnitude ? expr_sub(right, left_magnitude) : NULL;

cleanup:
    expr_free(left_magnitude);
    num_destroy(&right_value);
    num_destroy(&left_value);
    return reordered;
}

static char *expr_rotated_cartesian_root_TeX_dup(const expr_t *seed, long root_index, long order)
{
    const expr_t *scale;
    const expr_t *real;
    const expr_t *imaginary;
    number_t seed_value = (number_t){0};
    number_t real_value = (number_t){0};
    number_t imaginary_value = (number_t){0};
    number_t imaginary_magnitude = (number_t){0};
    expr_t *owned_real = NULL;
    expr_t *owned_imaginary = NULL;
    expr_t *cosine = NULL;
    expr_t *sine = NULL;
    expr_t *signed_imaginary = NULL;
    expr_t *real_cosine = NULL;
    expr_t *imaginary_sine = NULL;
    expr_t *real_sine = NULL;
    expr_t *imaginary_cosine = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *scaled_real = NULL;
    expr_t *scaled_imaginary = NULL;
    expr_t *real_display = NULL;
    expr_t *imaginary_display = NULL;
    expr_t *positive_imaginary = NULL;
    expr_t *positive_imaginary_display = NULL;
    expr_t *ordered_display = NULL;
    number_t displayed_real_value = (number_t){0};
    number_t displayed_imaginary_value = (number_t){0};
    char *real_TeX = NULL;
    char *imaginary_TeX = NULL;
    char *display_TeX = NULL;
    size_t display_TeX_size = 0u;
    FILE *stream = NULL;
    bool real_is_zero;
    bool imaginary_is_zero;
    bool imaginary_is_unit;
    bool imaginary_is_negative;
    int imaginary_sign;

    if (!seed || root_index <= 0L || order < 2L)
        goto cleanup;
    if (!expr_scaled_cartesian_parts(seed, &scale, &real, &imaginary, &imaginary_sign)) {
        seed_value = expr_eval(seed);
        real_value = num_real_part(seed_value);
        imaginary_value = num_imag_part(seed_value);
        if (!num_is_finite(seed_value) || num_is_zero(imaginary_value))
            goto cleanup;
        imaginary_sign = num_get_sign(imaginary_value) < 0 ? -1 : 1;
        imaginary_magnitude = num_abs(imaginary_value);
        owned_real = expr_new_const(real_value);
        owned_imaginary = expr_new_const(imaginary_magnitude);
        scale = NULL;
        real = owned_real;
        imaginary = owned_imaginary;
    }
    cosine = expr_root_turn_trig_display(root_index, order, false);
    sine = expr_root_turn_trig_display(root_index, order, true);
    if (!cosine || !sine)
        goto cleanup;

    signed_imaginary = imaginary_sign > 0 ? expr_clone(imaginary) : expr_neg(imaginary);
    real_cosine = expr_mul(real, cosine);
    imaginary_sine = signed_imaginary ? expr_mul(signed_imaginary, sine) : NULL;
    real_sine = expr_mul(real, sine);
    imaginary_cosine = signed_imaginary ? expr_mul(signed_imaginary, cosine) : NULL;
    real_part = real_cosine && imaginary_sine ? expr_sub(real_cosine, imaginary_sine) : NULL;
    imaginary_part = real_sine && imaginary_cosine ? expr_add(real_sine, imaginary_cosine) : NULL;
    scaled_real = real_part ? (scale ? expr_mul(scale, real_part) : expr_clone(real_part)) : NULL;
    scaled_imaginary = imaginary_part ? (scale ? expr_mul(scale, imaginary_part) : expr_clone(imaginary_part)) : NULL;
    real_display = scaled_real ? expr_display_expanded(scaled_real) : NULL;
    imaginary_display = scaled_imaginary ? expr_beautify(scaled_imaginary) : NULL;
    if (!real_display || !imaginary_display)
        goto cleanup;
    ordered_display = expr_reorder_positive_sum_for_display(real_display);
    if (ordered_display) {
        expr_free(real_display);
        real_display = ordered_display;
        ordered_display = NULL;
    }

    displayed_real_value = expr_eval(real_display);
    displayed_imaginary_value = expr_eval(imaginary_display);
    real_is_zero = num_is_zero(displayed_real_value);
    imaginary_is_zero = num_is_zero(displayed_imaginary_value);
    imaginary_is_negative = num_get_sign(displayed_imaginary_value) < 0;
    if (imaginary_is_negative) {
        positive_imaginary = expr_distribute_negative_for_display(imaginary_display);
        positive_imaginary_display = positive_imaginary ? expr_display_expanded(positive_imaginary) : NULL;
        if (!positive_imaginary_display)
            goto cleanup;
        expr_free(imaginary_display);
        imaginary_display = positive_imaginary_display;
        positive_imaginary_display = NULL;
        num_destroy(&displayed_imaginary_value);
        displayed_imaginary_value = expr_eval(imaginary_display);
    }
    ordered_display = expr_reorder_positive_sum_for_display(imaginary_display);
    if (ordered_display) {
        expr_free(imaginary_display);
        imaginary_display = ordered_display;
        ordered_display = NULL;
    }
    imaginary_is_unit = num_eq(displayed_imaginary_value, NUM_ONE);
    real_TeX = real_is_zero ? NULL : expr_TeX_body_dup(real_display);
    imaginary_TeX = imaginary_is_zero || imaginary_is_unit ? NULL : expr_TeX_body_dup(imaginary_display);
    if ((!real_is_zero && !real_TeX) || (!imaginary_is_zero && !imaginary_is_unit && !imaginary_TeX))
        goto cleanup;

    stream = open_memstream(&display_TeX, &display_TeX_size);
    if (!stream)
        goto cleanup;
    if (!real_is_zero)
        fputs(real_TeX, stream);
    if (!imaginary_is_zero) {
        if (!real_is_zero)
            fputs(imaginary_is_negative ? " - " : " + ", stream);
        else if (imaginary_is_negative)
            fputs("\\mathord{-}\\mkern-2mu ", stream);
        if (!imaginary_is_unit) {
            const expr_t *sum_left;
            const expr_t *sum_right;
            bool sum_subtract;
            bool needs_parentheses = expr_match_add_sub_expr(imaginary_display, &sum_left, &sum_right, &sum_subtract);

            if (needs_parentheses)
                fputs("\\left(", stream);
            fputs(imaginary_TeX, stream);
            if (needs_parentheses)
                fputs("\\right)", stream);
            fputs("\\mkern-2mu ", stream);
        }
        fputs("i", stream);
    } else if (real_is_zero) {
        fputs("0", stream);
    }
    fclose(stream);
    stream = NULL;

cleanup:
    if (stream)
        fclose(stream);
    free(imaginary_TeX);
    free(real_TeX);
    num_destroy(&displayed_imaginary_value);
    num_destroy(&displayed_real_value);
    expr_free(ordered_display);
    expr_free(positive_imaginary_display);
    expr_free(positive_imaginary);
    expr_free(imaginary_display);
    expr_free(real_display);
    expr_free(owned_imaginary);
    expr_free(owned_real);
    num_destroy(&imaginary_magnitude);
    num_destroy(&imaginary_value);
    num_destroy(&real_value);
    num_destroy(&seed_value);
    expr_free(scaled_imaginary);
    expr_free(scaled_real);
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(imaginary_cosine);
    expr_free(real_sine);
    expr_free(imaginary_sine);
    expr_free(real_cosine);
    expr_free(signed_imaginary);
    expr_free(sine);
    expr_free(cosine);
    return display_TeX;
}

static void print_explicit_root_family(const expr_t *expr, int precision)
{
    number_t seed = (number_t){0};
    number_t seed_value = (number_t){0};
    expr_t *base = NULL;
    long order = 0L;
    expr_t *seed_expr = NULL;
    expr_t *negative_seed = NULL;
    char *seed_TeX;
    char *negative_expression = NULL;
    char *negative_TeX = NULL;
    char *root_expression = NULL;
    char *root_TeX = NULL;
    char *root_function = NULL;
    char *root_value = NULL;
    size_t root_TeX_size = 0u;
    FILE *TeX_stream;
    bool exact_seed;

    if (!explicit_reciprocal_power(expr, &base, &order)) {
        num_destroy(&seed);
        return;
    }

    exact_seed = expr_exact_complex_root_seed(expr, &seed, &order);
    if (exact_seed) {
        seed_expr = expr_new_const(seed);
    } else if (base) {
        seed_expr = expr_principal_root_display(base, order);
    } else {
        seed_expr = expr_clone(expr);
    }
    seed_TeX = seed_expr ? expr_TeX_body_dup(seed_expr) : NULL;
    if (!seed_TeX)
        goto cleanup;
    seed_value = expr_eval(seed_expr);

    if (order == 2L) {
        negative_seed = expr_distribute_negative_for_display(seed_expr);
        negative_expression = negative_seed ? expr_text_dup(negative_seed, style_UNBOUND) : NULL;
        negative_TeX = negative_seed ? expr_TeX_body_dup(negative_seed) : NULL;
        if (!negative_expression || !negative_TeX)
            goto cleanup;
    }

    TeX_stream = open_memstream(&root_TeX, &root_TeX_size);
    if (!TeX_stream)
        goto cleanup;

    fputs("\\begin{aligned}[t]", TeX_stream);
    for (long root_index = 0L; root_index < order; ++root_index) {
        char *quarter_turn_TeX = NULL;
        char *cartesian_TeX = NULL;

        if (root_index > 0L)
            fputs("\\\\[0.65em]", TeX_stream);
        fputs("&", TeX_stream);
        if (order == 4L)
            quarter_turn_TeX = expr_quarter_turn_root_TeX_dup(seed_expr, root_index);
        else if (root_index > 0L && order > 2L)
            cartesian_TeX = expr_rotated_cartesian_root_TeX_dup(seed_expr, root_index, order);
        if (quarter_turn_TeX) {
            fputs(quarter_turn_TeX, TeX_stream);
        } else if (cartesian_TeX) {
            fputs(cartesian_TeX, TeX_stream);
        } else if (root_index == 0L) {
            fputs(seed_TeX, TeX_stream);
        } else if (order == 2L) {
            if (negative_TeX[0] == '-') {
                fputs("\\mathord{-}\\mkern-2mu ", TeX_stream);
                fputs(negative_TeX + 1, TeX_stream);
            } else {
                fputs(negative_TeX, TeX_stream);
            }
        } else {
            fprintf(TeX_stream, "\\left(%s\\right)e^{\\frac{%ld\\pi i}{%ld}}", seed_TeX, 2L * root_index,
                    order);
        }
        free(cartesian_TeX);
        free(quarter_turn_TeX);
    }
    fputs("\\end{aligned}", TeX_stream);

    fclose(TeX_stream);
    if (!root_family_texts_dup(seed_expr, order, &root_expression, &root_function))
        goto cleanup;
    root_value = numeric_root_family_dup(seed_value, order, precision);
    printf("root_expression  %s\n", root_expression);
    printf("root_tex    %s\n", root_TeX);
    printf("root_function  %s\n", root_function);
    if (root_value)
        printf("root_value  %s\n", root_value);

cleanup:
    free(root_value);
    free(root_function);
    free(root_TeX);
    free(root_expression);
    free(seed_TeX);
    free(negative_TeX);
    free(negative_expression);
    expr_free(negative_seed);
    expr_free(seed_expr);
    expr_free(base);
    num_destroy(&seed_value);
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
        print_explicit_root_family(expr, precision);
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
