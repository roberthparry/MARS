#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "expression.h"
#include "integrator.h"
#include "number.h"
#include "ustring.h"

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
    if (!expr)
        goto cleanup;

    *out = expr_eval(expr);
    rc = 0;

cleanup:
    expr_free(expr);
    free(wrapped_input);
    return rc;
}

static int text_is_ci_literal(const char *text, const char *literal)
{
    size_t i = 0u;

    while (text[i] && literal[i]) {
        if (tolower((unsigned char)text[i]) !=
            tolower((unsigned char)literal[i]))
            return 0;
        i++;
    }
    return text[i] == '\0' && literal[i] == '\0';
}

static int parse_size_text(const char *text, size_t *out)
{
    size_t i = 0u;
    size_t value = 0u;
    int saw_digit = 0;

    if (!text || !out)
        return 1;

    while (isspace((unsigned char)text[i]))
        i++;
    if (text[i] == '+')
        i++;

    for (; text[i] != '\0'; ++i) {
        unsigned int digit;

        if (isspace((unsigned char)text[i]))
            break;
        if (!isdigit((unsigned char)text[i]))
            return 1;

        digit = (unsigned int)(text[i] - '0');
        if (value > (((size_t)-1) - digit) / 10u)
            return 1;
        value = value * 10u + digit;
        saw_digit = 1;
    }

    while (isspace((unsigned char)text[i]))
        i++;
    if (!saw_digit || text[i] != '\0')
        return 1;

    *out = value;
    return 0;
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

static int parse_infinity_bound(const char *text, number_t *out)
{
    int sign = 1;
    size_t len;
    char *copy;
    char *trimmed;
    size_t pos = 0u;
    int ok;

    if (!text || !out)
        return 1;

    len = strlen(text);
    copy = malloc(len + 1u);
    if (!copy)
        return 1;
    memcpy(copy, text, len + 1u);

    trimmed = trim_ascii_in_place(copy);

    if (trimmed[pos] == '+' || trimmed[pos] == '-') {
        if (trimmed[pos] == '-')
            sign = -1;
        pos++;
    }

    ok = text_is_ci_literal(&trimmed[pos], "inf") ||
         text_is_ci_literal(&trimmed[pos], "infinity") ||
         strcmp(&trimmed[pos], "∞") == 0;
    if (ok)
        *out = num_clone(sign < 0 ? NUM_NINF : NUM_INF);

    free(copy);
    return ok ? 0 : 1;
}

static int parse_number_bound(const char *text, int precision, number_t *out)
{
    int rc;

    if (!out)
        return 1;
    *out = num_new();
    if (parse_infinity_bound(text, out) == 0)
        return 0;
    rc = parse_number_expression(text, precision, out);
    if (rc != 0)
        return rc;
    if (num_is_nan(*out) || !num_is_finite(*out) || !num_is_real(*out))
        return 1;
    return 0;
}

static expr_t *parse_bound_expression(const char *text, int precision)
{
    const char *input = text;
    char *wrapped_input = NULL;
    expr_t *expr = NULL;
    expr_t *simplified = NULL;
    number_t value = num_new();

    if (!text) {
        num_destroy(&value);
        return NULL;
    }

    if (parse_infinity_bound(text, &value) == 0) {
        simplified = expr_new_const(value);
        num_destroy(&value);
        return simplified;
    }
    num_destroy(&value);

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(text);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, NULL);
    if (!expr)
        goto cleanup;

    simplified = expr_simplify(expr);

cleanup:
    expr_free(expr);
    free(wrapped_input);
    return simplified;
}

static expr_t *binding_or_shadowing_var(expr_t **expr_io,
                                        expr_bindings_t *bindings,
                                        const char *name,
                                        int *owned)
{
    expr_t *bound = bindings ? expr_bindings_get(bindings, name) : NULL;
    expr_t *needle = NULL;
    expr_t *shadow = NULL;
    expr_t *rewritten = NULL;
    int owns_needle = 0;

    if (owned)
        *owned = 0;

    if (bound && expr_is_variable(bound))
        return bound;

    needle = bound;
    if (!needle) {
        needle = expr_new_named_var(NUM_NAN, name);
        if (!needle)
            return NULL;
        owns_needle = 1;
    }

    shadow = expr_new_named_var(NUM_NAN, name);
    if (!shadow) {
        if (owns_needle)
            expr_free(needle);
        return NULL;
    }

    if (expr_io && *expr_io) {
        rewritten = expr_substitute(*expr_io, needle, shadow);
        if (!rewritten) {
            if (owns_needle)
                expr_free(needle);
            expr_free(shadow);
            return NULL;
        }
        expr_free(*expr_io);
        *expr_io = rewritten;
    }

    if (owns_needle)
        expr_free(needle);

    if (owned)
        *owned = 1;
    return shadow;
}

static expr_t *make_unevaluated_antiderivative(const expr_t *integrand,
                                               const expr_t *wrt)
{
    expr_t *simplified = NULL;
    expr_t *out = NULL;

    if (!integrand || !wrt || !expr_is_variable(wrt))
        return NULL;

    simplified = expr_simplify(integrand);
    out = expr_integral(simplified ? simplified : integrand, wrt);
    expr_free(simplified);
    return out;
}

static char *number_text(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

typedef intg_bound_kind_t bound_kind_t;

static char *dup_string(const char *text)
{
    size_t n;
    char *copy;

    if (!text)
        text = "";
    n = strlen(text);
    copy = malloc(n + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, text, n + 1u);
    return copy;
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    return expr_to_string(expr, style);
}

static int is_ascii_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static int is_plain_decimal_bound(const char *text)
{
    size_t pos = 0u;
    int saw_digit = 0;
    int saw_dot = 0;

    if (!text || !text[pos])
        return 0;
    if (text[pos] == '+' || text[pos] == '-')
        pos++;

    while (is_ascii_digit(text[pos])) {
        saw_digit = 1;
        pos++;
    }

    if (text[pos] == '.') {
        saw_dot = 1;
        pos++;
        while (is_ascii_digit(text[pos])) {
            saw_digit = 1;
            pos++;
        }
    }

    if (!saw_dot || !saw_digit)
        return 0;

    if (text[pos] == 'e' || text[pos] == 'E') {
        int saw_exponent_digit = 0;

        pos++;
        if (text[pos] == '+' || text[pos] == '-')
            pos++;
        while (is_ascii_digit(text[pos])) {
            saw_exponent_digit = 1;
            pos++;
        }
        if (!saw_exponent_digit)
            return 0;
    }

    return text[pos] == '\0';
}

static char *bound_tex_input(const char *raw_input, const expr_t *expr)
{
    if (is_plain_decimal_bound(raw_input))
        return dup_string(raw_input);
    if (expr) {
        char *body = expr_to_tex_body(expr);

        if (body)
            return body;
    }
    return expr ? expr_text_dup(expr, style_TEX) : dup_string(raw_input);
}

static char *expr_tex_body(const expr_t *expr)
{
    char *body;

    if (!expr)
        return NULL;
    body = expr_to_tex_body(expr);
    if (body)
        return body;
    return expr_text_dup(expr, style_TEX);
}

static char *expr_tex_body_display(const expr_t *expr)
{
    char *body;

    if (!expr)
        return NULL;
    body = expr_to_tex_body_wrapped(expr, 110u);
    if (body)
        return body;
    return expr_tex_body(expr);
}

static char *combine_aligned_equation_tex(const char *lhs, const char *rhs,
                                          int add_constant);

static char *combine_equation_tex(const char *lhs, const char *rhs)
{
    size_t lhs_n;
    size_t rhs_n;
    char *out;

    if (!lhs || !*lhs || !rhs || !*rhs)
        return NULL;
    out = combine_aligned_equation_tex(lhs, rhs, 0);
    if (out)
        return out;
    lhs_n = strlen(lhs);
    rhs_n = strlen(rhs);
    out = malloc(lhs_n + rhs_n + 6u);
    if (!out)
        return NULL;
    snprintf(out, lhs_n + rhs_n + 6u, "%s = %s", lhs, rhs);
    return out;
}

static char *combine_aligned_equation_tex(const char *lhs, const char *rhs,
                                          int add_constant)
{
    static const char prefix[] = "\\begin{aligned}[t]\n";
    static const char suffix[] = "\n\\end{aligned}";
    const char *body;
    const char *end;
    size_t lhs_n;
    size_t body_n;
    size_t suffix_n = strlen(suffix);
    size_t extra_n;
    char *out;

    if (!lhs || !*lhs || !rhs || !*rhs)
        return NULL;
    if (strncmp(rhs, prefix, strlen(prefix)) != 0)
        return NULL;
    end = rhs + strlen(rhs);
    if ((size_t)(end - rhs) < suffix_n ||
        strcmp(end - suffix_n, suffix) != 0)
        return NULL;

    body = rhs + strlen(prefix);
    if (*body == '&')
        body++;
    end -= suffix_n;
    while (end > body && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' ||
                          end[-1] == '\t'))
        end--;

    lhs_n = strlen(lhs);
    body_n = (size_t)(end - body);
    extra_n = add_constant ? strlen(" \\\\\n&{} + C") : 0u;
    out = malloc(strlen(prefix) + lhs_n + strlen(" ={}& ") + body_n +
                 extra_n + suffix_n + 1u);
    if (!out)
        return NULL;

    snprintf(out,
             strlen(prefix) + lhs_n + strlen(" ={}& ") + body_n +
                 extra_n + suffix_n + 1u,
             "%s%s ={}& %.*s%s%s",
             prefix,
             lhs,
             (int)body_n,
             body,
             add_constant ? " \\\\\n&{} + C" : "",
             suffix);
    return out;
}

static char *combine_antiderivative_tex(const char *lhs, const char *rhs)
{
    size_t lhs_n;
    size_t rhs_n;
    char *out;

    if (!lhs || !*lhs || !rhs || !*rhs)
        return NULL;
    out = combine_aligned_equation_tex(lhs, rhs, 1);
    if (out)
        return out;
    lhs_n = strlen(lhs);
    rhs_n = strlen(rhs);
    out = malloc(lhs_n + rhs_n + 10u);
    if (!out)
        return NULL;
    snprintf(out, lhs_n + rhs_n + 10u, "%s = %s + C", lhs, rhs);
    return out;
}

static void print_integrator_context(const char *input,
                                     const char *expr_text,
                                     const char *binding_expr_text,
                                     size_t ndim,
                                     const bound_kind_t *bound_kinds,
                                     const char *const *var_names,
                                     char *const *lo_display_inputs,
                                     char *const *hi_display_inputs,
                                     const char *const *lo_inputs,
                                     const char *const *hi_inputs)
{
    printf("input       %s\n", input ? input : "");
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("binding_expression  %s\n",
           binding_expr_text ? binding_expr_text : "");
    printf("dimensions  %zu\n", ndim);
    for (size_t i = 0; i < ndim; ++i) {
        const char *name = var_names && var_names[i] ? var_names[i] : "";
        const char *lo = lo_display_inputs && lo_display_inputs[i]
            ? lo_display_inputs[i]
            : (lo_inputs && lo_inputs[i] ? lo_inputs[i] : "");
        const char *hi = hi_display_inputs && hi_display_inputs[i]
            ? hi_display_inputs[i]
            : (hi_inputs && hi_inputs[i] ? hi_inputs[i] : "");

        if (bound_kinds && bound_kinds[i] == INTG_BOUND_DEFINITE)
            printf("bound       %s in [%s, %s]\n", name, lo, hi);
        else if (bound_kinds && bound_kinds[i] == INTG_BOUND_UPPER_ONLY)
            printf("bound       %s antiderivative at %s\n", name, hi);
        else
            printf("bound       antiderivative with respect to %s\n", name);
        printf("bound_var   %s\n", name);
        printf("bound_lower %s\n", lo);
        printf("bound_upper %s\n", hi);
    }
}

static int all_bounds_indefinite(size_t ndim, const bound_kind_t *kinds)
{
    if (!kinds)
        return 0;
    for (size_t i = 0; i < ndim; ++i) {
        if (kinds[i] != INTG_BOUND_INDEFINITE)
            return 0;
    }
    return 1;
}

static int should_try_symbolic_first(const expr_t *expr,
                                     size_t ndim,
                                     expr_t *const *vars,
                                     int all_bounds_numeric)
{
    if (!expr || !vars || ndim == 0u)
        return 0;

    if (ndim == 1u)
        return 1;

    if (!all_bounds_numeric)
        return 1;

    return intg_integrand_has_unbound_parameters(expr, ndim, vars);
}

static char *integral_tex(const char *body,
                          size_t ndim,
                          const bound_kind_t *kinds,
                          const char *const *var_names,
                          const char *const *lo,
                          const char *const *hi)
{
    char *result = NULL;
    size_t cap;
    size_t len = 0u;

    if (!body)
        return NULL;

    cap = strlen(body) + 128u * ndim + 32u;
    result = malloc(cap);
    if (!result) {
        return NULL;
    }

    result[0] = '\0';
    for (size_t i = ndim; i-- > 0;) {
        int wrote;

        if (kinds[i] == INTG_BOUND_DEFINITE && (!lo[i] || !hi[i])) {
            free(result);
            return NULL;
        }
        if (kinds[i] == INTG_BOUND_UPPER_ONLY && !hi[i]) {
            free(result);
            return NULL;
        }

        if (kinds[i] == INTG_BOUND_DEFINITE) {
            wrote = snprintf(result + len, cap - len,
                             "\\int_{%s}^{%s} ", lo[i], hi[i]);
        } else if (kinds[i] == INTG_BOUND_UPPER_ONLY) {
            wrote = snprintf(result + len, cap - len,
                             "\\int^{%s} ", hi[i]);
        } else {
            wrote = snprintf(result + len, cap - len, "\\int ");
        }
        if (wrote < 0 || (size_t)wrote >= cap - len) {
            free(result);
            return NULL;
        }
        len += (size_t)wrote;
    }

    {
        int wrote = snprintf(result + len, cap - len, "%s", body);

        if (wrote < 0 || (size_t)wrote >= cap - len) {
            free(result);
            return NULL;
        }
        len += (size_t)wrote;
    }

    for (size_t i = 0; i < ndim; ++i) {
        int wrote = snprintf(result + len, cap - len, "\\, d%s", var_names[i]);

        if (wrote < 0 || (size_t)wrote >= cap - len) {
            free(result);
            return NULL;
        }
        len += (size_t)wrote;
    }

    return result;
}

int main(int argc, char **argv)
{
    int argi = 1;
    size_t max_intervals = 0u;
    int has_max_intervals = 0;
    const char *raw_input;
    int precision;
    size_t ndim;
    const char *input = NULL;
    char *wrapped_input = NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *expr = NULL;
    integrator_t *ig = NULL;
    expr_t **vars = NULL;
    int *owned_vars = NULL;
    const char **var_names = NULL;
    const char **lo_inputs = NULL;
    const char **hi_inputs = NULL;
    bound_kind_t *bound_kinds = NULL;
    number_t *lo_num = NULL;
    number_t *hi_num = NULL;
    expr_t **lo_expr = NULL;
    expr_t **hi_expr = NULL;
    char **lo_display_inputs = NULL;
    char **hi_display_inputs = NULL;
    char **lo_tex_inputs = NULL;
    char **hi_tex_inputs = NULL;
    number_t value_num = num_new();
    number_t error_num = num_new();
    number_t symbolic_num = num_new();
    char *expr_text = NULL;
    char *binding_expr_text = NULL;
    char *tex_text = NULL;
    char *base_tex_text = NULL;
    char *integrand_tex = NULL;
    char *value_text = NULL;
    char *error_text = NULL;
    char *symbolic_value_text = NULL;
    char *symbolic_text = NULL;
    char *symbolic_tex = NULL;
    char *antiderivative_text = NULL;
    char *antiderivative_tex = NULL;
    char *display_input = NULL;
    expr_t *display_expr = NULL;
    expr_t *symbolic_result = NULL;
    expr_t *first_antiderivative = NULL;
    expr_t *partially_symbolic_result = NULL;
    expr_t **remaining_numeric_vars = NULL;
    number_t *remaining_lo_num = NULL;
    number_t *remaining_hi_num = NULL;
    int rc = 1;
    int intg_rc = 0;
    int all_bounds_numeric = 1;
    int ran_numeric_integrator = 0;
    int used_symbolic_numeric_result = 0;
    int used_unevaluated_antiderivative_fallback = 0;
    int used_partial_symbolic_numeric = 0;
    int used_core_exact_result = 0;
    int has_unbound_params = 0;
    size_t partial_symbolic_steps = 0u;
    size_t remaining_numeric_ndim = 0u;

    while (argi < argc && strncmp(argv[argi], "--", 2u) == 0) {
        if (strcmp(argv[argi], "--max-intervals") == 0) {
            size_t parsed;

            if (argi + 1 >= argc) {
                fprintf(stderr, "--max-intervals needs a value\n");
                return 1;
            }
            if (parse_size_text(argv[argi + 1], &parsed) != 0 ||
                parsed == 0u) {
                fprintf(stderr, "Bad --max-intervals value\n");
                return 1;
            }
            max_intervals = parsed;
            has_max_intervals = 1;
            argi += 2;
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", argv[argi]);
        return 1;
    }

    raw_input = argi < argc ? argv[argi] : "{ exp(-x^2) | x = ? }";
    precision = argi + 1 < argc ? atoi(argv[argi + 1]) : 32;
    ndim = argi + 2 < argc ? (size_t)((argc - (argi + 2)) / 3) : 1u;
    input = raw_input;

    if (argi + 2 < argc && ((argc - (argi + 2)) % 3) != 0) {
        fprintf(stderr, "Bounds must be supplied as repeated <var> <lo> <hi> triples\n");
        return 1;
    }

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    wrapped_input = wrap_expression(raw_input);
    if (wrapped_input)
        input = wrapped_input;

    expr = expr_from_string(input, &bindings);
    if (!expr) {
        fprintf(stderr, "Could not parse integrand expression\n");
        goto cleanup;
    }

    vars = calloc(ndim, sizeof(*vars));
    owned_vars = calloc(ndim, sizeof(*owned_vars));
    var_names = calloc(ndim, sizeof(*var_names));
    lo_inputs = calloc(ndim, sizeof(*lo_inputs));
    hi_inputs = calloc(ndim, sizeof(*hi_inputs));
    bound_kinds = calloc(ndim, sizeof(*bound_kinds));
    lo_num = calloc(ndim, sizeof(*lo_num));
    hi_num = calloc(ndim, sizeof(*hi_num));
    lo_expr = calloc(ndim, sizeof(*lo_expr));
    hi_expr = calloc(ndim, sizeof(*hi_expr));
    remaining_numeric_vars = calloc(ndim, sizeof(*remaining_numeric_vars));
    remaining_lo_num = calloc(ndim, sizeof(*remaining_lo_num));
    remaining_hi_num = calloc(ndim, sizeof(*remaining_hi_num));
    lo_display_inputs = calloc(ndim, sizeof(*lo_display_inputs));
    hi_display_inputs = calloc(ndim, sizeof(*hi_display_inputs));
    lo_tex_inputs = calloc(ndim, sizeof(*lo_tex_inputs));
    hi_tex_inputs = calloc(ndim, sizeof(*hi_tex_inputs));
    if (!vars || !owned_vars || !var_names || !lo_inputs || !hi_inputs || !bound_kinds ||
        !lo_num || !hi_num || !lo_expr || !hi_expr ||
        !remaining_numeric_vars || !remaining_lo_num || !remaining_hi_num ||
        !lo_display_inputs || !hi_display_inputs || !lo_tex_inputs || !hi_tex_inputs)
        goto cleanup;

    if (argi + 2 >= argc) {
        vars[0] = binding_or_shadowing_var(&expr, bindings, "x", &owned_vars[0]);
        if (!vars[0])
            goto cleanup;
        var_names[0] = "x";
        lo_inputs[0] = "0";
        hi_inputs[0] = "pi";
        bound_kinds[0] = INTG_BOUND_DEFINITE;
        lo_num[0] = num_create_from_long(0);
        hi_num[0] = num_clone(NUM_PI);
        lo_expr[0] = parse_bound_expression(lo_inputs[0], precision);
        hi_expr[0] = parse_bound_expression(hi_inputs[0], precision);
        if (!lo_expr[0] || !hi_expr[0])
            goto cleanup;
        ndim = 1u;
    } else {
        for (size_t i = 0; i < ndim; ++i) {
            const char *name = argv[argi + 2 + (int)(i * 3u)];
            const char *lo_text_in = argv[argi + 3 + (int)(i * 3u)];
            const char *hi_text_in = argv[argi + 4 + (int)(i * 3u)];
            int has_lo;
            int has_hi;

            vars[i] = binding_or_shadowing_var(&expr, bindings, name, &owned_vars[i]);
            if (!vars[i]) {
                fprintf(stderr, "Could not create binding named '%s'\n", name);
                goto cleanup;
            }
            has_lo = lo_text_in && lo_text_in[0] != '\0';
            has_hi = hi_text_in && hi_text_in[0] != '\0';
            if (has_lo && has_hi) {
                bound_kinds[i] = INTG_BOUND_DEFINITE;
                lo_expr[i] = parse_bound_expression(lo_text_in, precision);
                hi_expr[i] = parse_bound_expression(hi_text_in, precision);
                if (!lo_expr[i] || !hi_expr[i]) {
                    fprintf(stderr, "Could not parse bounds for %s\n", name);
                    goto cleanup;
                }
            } else if (!has_lo && has_hi) {
                bound_kinds[i] = INTG_BOUND_UPPER_ONLY;
                hi_expr[i] = parse_bound_expression(hi_text_in, precision);
                if (!hi_expr[i]) {
                    fprintf(stderr, "Could not parse upper bound for %s\n", name);
                    goto cleanup;
                }
                all_bounds_numeric = 0;
            } else if (!has_lo && !has_hi) {
                bound_kinds[i] = INTG_BOUND_INDEFINITE;
                all_bounds_numeric = 0;
            } else {
                fprintf(stderr, "A one-sided bound for %s must be supplied as an upper bound\n", name);
                goto cleanup;
            }
            if (bound_kinds[i] == INTG_BOUND_DEFINITE) {
                int lo_numeric = parse_number_bound(lo_text_in, precision, &lo_num[i]) == 0;
                int hi_numeric = parse_number_bound(hi_text_in, precision, &hi_num[i]) == 0;

                if (!lo_numeric || !hi_numeric)
                    all_bounds_numeric = 0;
            }
            var_names[i] = name;
            lo_inputs[i] = lo_text_in;
            hi_inputs[i] = hi_text_in;
        }
    }

    for (size_t i = 0; i < ndim; ++i) {
        if (lo_inputs[i])
            lo_display_inputs[i] = dup_string(lo_inputs[i]);
        if (hi_inputs[i])
            hi_display_inputs[i] = dup_string(hi_inputs[i]);
        if (!lo_display_inputs[i] && lo_expr[i])
            lo_display_inputs[i] = expr_text_dup(lo_expr[i], style_UNBOUND);
        if (!hi_display_inputs[i] && hi_expr[i])
            hi_display_inputs[i] = expr_text_dup(hi_expr[i], style_UNBOUND);
        if (lo_expr[i]) {
            lo_tex_inputs[i] = bound_tex_input(lo_inputs[i], lo_expr[i]);
        }
        if (hi_expr[i]) {
            hi_tex_inputs[i] = bound_tex_input(hi_inputs[i], hi_expr[i]);
        }
        if (!lo_display_inputs[i] && lo_inputs[i])
            lo_display_inputs[i] = dup_string(lo_inputs[i]);
        if (!hi_display_inputs[i] && hi_inputs[i])
            hi_display_inputs[i] = dup_string(hi_inputs[i]);
        if (!lo_tex_inputs[i] && lo_inputs[i])
            lo_tex_inputs[i] = dup_string(lo_inputs[i]);
        if (!hi_tex_inputs[i] && hi_inputs[i])
            hi_tex_inputs[i] = dup_string(hi_inputs[i]);
    }

    ig = intg_new();
    if (!ig)
        goto cleanup;
    if (has_max_intervals)
        intg_set_interval_count_max(ig, max_intervals);

    expr_text = expr_text_dup(expr, style_UNBOUND);
    binding_expr_text = expr_text_dup(expr, style_EXPRESSION);
    if (expr_text) {
        display_input = wrap_expression(expr_text);
        display_expr = expr_from_string(display_input ? display_input : expr_text, NULL);
    }
    integrand_tex = expr_tex_body(display_expr);

    has_unbound_params = intg_integrand_has_unbound_parameters(expr, ndim, vars);
    if (should_try_symbolic_first(expr, ndim, vars, all_bounds_numeric)) {
        symbolic_result = intg_integrate_iterated_symbolic(
            expr, ndim, vars, bound_kinds, lo_expr, hi_expr,
            ndim, NULL, &first_antiderivative);
        if (!symbolic_result && !first_antiderivative && ndim == 1u)
            first_antiderivative = make_unevaluated_antiderivative(expr, vars[0]);
        if (first_antiderivative) {
            antiderivative_text = expr_text_dup(first_antiderivative, style_UNBOUND);
            antiderivative_tex = expr_tex_body_display(first_antiderivative);
        }
        if (symbolic_result) {
            symbolic_text = expr_text_dup(symbolic_result, style_UNBOUND);
            symbolic_tex = expr_tex_body_display(symbolic_result);
            symbolic_num = expr_eval(symbolic_result);
            if (!num_is_nan(symbolic_num) && num_is_finite(symbolic_num) && num_is_real(symbolic_num))
                symbolic_value_text = number_text(symbolic_num);
        }
    }

    if (all_bounds_numeric && symbolic_value_text) {
        used_symbolic_numeric_result = 1;
        intg_rc = 0;
    } else if (symbolic_result) {
        intg_rc = 0;
    } else if (first_antiderivative && ndim == 1u &&
               (!all_bounds_numeric || has_unbound_params)) {
        used_unevaluated_antiderivative_fallback = 1;
        intg_rc = 0;
    } else if (all_bounds_numeric && ndim == 1u) {
        intg_rc = intg_integral(ig, expr, vars[0], lo_num[0], hi_num[0],
                                   &value_num, &error_num);
        ran_numeric_integrator = 1;
    } else if (all_bounds_numeric) {
        partially_symbolic_result = intg_integrate_iterated_symbolic_best_effort(
            expr, ndim, vars, bound_kinds, lo_expr, hi_expr,
            &partial_symbolic_steps, &remaining_numeric_ndim,
            remaining_numeric_vars, remaining_lo_num, remaining_hi_num,
            lo_num, hi_num);

        if (partially_symbolic_result && remaining_numeric_ndim == 0u) {
            symbolic_result = partially_symbolic_result;
            partially_symbolic_result = NULL;
            symbolic_text = expr_text_dup(symbolic_result, style_UNBOUND);
            symbolic_tex = expr_tex_body_display(symbolic_result);
            symbolic_num = expr_eval(symbolic_result);
            if (!num_is_nan(symbolic_num) && num_is_finite(symbolic_num) &&
                num_is_real(symbolic_num)) {
                symbolic_value_text = number_text(symbolic_num);
            }
            if (symbolic_value_text)
                used_symbolic_numeric_result = 1;
            used_partial_symbolic_numeric = 1;
            intg_rc = 0;
        } else if (partially_symbolic_result && partial_symbolic_steps > 0u &&
                   remaining_numeric_ndim == 1u) {
            intg_rc = intg_integral(ig, partially_symbolic_result,
                                    remaining_numeric_vars[0],
                                    remaining_lo_num[0], remaining_hi_num[0],
                                    &value_num, &error_num);
            ran_numeric_integrator = 1;
            used_partial_symbolic_numeric = 1;
        } else if (partially_symbolic_result && partial_symbolic_steps > 0u &&
                   remaining_numeric_ndim > 1u &&
                   remaining_numeric_ndim < ndim) {
            intg_rc = intg_integral_multi(ig, partially_symbolic_result,
                                          remaining_numeric_ndim,
                                          remaining_numeric_vars,
                                          remaining_lo_num, remaining_hi_num,
                                          &value_num, &error_num);
            ran_numeric_integrator = 1;
            used_partial_symbolic_numeric = 1;
        } else {
            expr_free(partially_symbolic_result);
            partially_symbolic_result = NULL;
            partial_symbolic_steps = 0u;
            remaining_numeric_ndim = 0u;
            intg_rc = intg_integral_multi(ig, expr, ndim, vars, lo_num, hi_num,
                                          &value_num, &error_num);
            ran_numeric_integrator = 1;
        }
    } else {
        print_integrator_context(input, expr_text, binding_expr_text, ndim,
                                 bound_kinds, var_names, lo_display_inputs,
                                 hi_display_inputs, lo_inputs, hi_inputs);
        fprintf(stderr, "Symbolic bounds need an integrand with a supported symbolic antiderivative\n");
        goto cleanup;
    }

    if (ran_numeric_integrator && intg_rc < 0) {
        if (used_partial_symbolic_numeric && all_bounds_numeric && ndim > 1u) {
            num_destroy(&value_num);
            num_destroy(&error_num);
            value_num = num_new();
            error_num = num_new();
            intg_rc = intg_integral_multi(ig, expr, ndim, vars, lo_num, hi_num,
                                          &value_num, &error_num);
            if (intg_rc >= 0)
                used_partial_symbolic_numeric = 0;
        }
    }

    if (ran_numeric_integrator && intg_rc < 0) {
        print_integrator_context(input, expr_text, binding_expr_text, ndim,
                                 bound_kinds, var_names, lo_display_inputs,
                                 hi_display_inputs, lo_inputs, hi_inputs);
        fprintf(stderr, "Integration failed\n");
        goto cleanup;
    }

    if (ran_numeric_integrator && intg_rc >= 0 &&
        !symbolic_result && !symbolic_text && !symbolic_tex) {
        symbolic_result = (expr_t *)intg_get_exact_result(ig);
        if (symbolic_result)
            expr_retain(symbolic_result);
        if (symbolic_result) {
            used_core_exact_result = 1;
            symbolic_text = expr_text_dup(symbolic_result, style_UNBOUND);
            symbolic_tex = expr_tex_body_display(symbolic_result);
            symbolic_num = expr_eval(symbolic_result);
            if (!num_is_nan(symbolic_num) && num_is_finite(symbolic_num) &&
                num_is_real(symbolic_num)) {
                symbolic_value_text = number_text(symbolic_num);
            }
        }
    }

    if (ran_numeric_integrator) {
        value_text = number_text(value_num);
        error_text = number_text(error_num);
    } else {
        value_text = symbolic_value_text ? dup_string(symbolic_value_text) : dup_string("");
        error_text = dup_string("");
    }

    base_tex_text = integrand_tex ? integral_tex(integrand_tex, ndim, bound_kinds,
                                                var_names,
                                                (const char *const *)lo_tex_inputs,
                                                (const char *const *)hi_tex_inputs) : NULL;
    if (used_unevaluated_antiderivative_fallback)
        tex_text = dup_string(antiderivative_tex ? antiderivative_tex : "");
    else if (symbolic_tex && all_bounds_indefinite(ndim, bound_kinds))
        tex_text = combine_antiderivative_tex(base_tex_text, symbolic_tex);
    else
        tex_text = symbolic_tex ? combine_equation_tex(base_tex_text, symbolic_tex) : NULL;
    if (!tex_text)
        tex_text = dup_string(base_tex_text ? base_tex_text : "");

    print_integrator_context(input, expr_text, binding_expr_text, ndim,
                             bound_kinds, var_names, lo_display_inputs,
                             hi_display_inputs, lo_inputs, hi_inputs);
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    printf("antiderivative %s\n", antiderivative_text ? antiderivative_text : "");
    printf("antiderivative_tex %s\n", antiderivative_tex ? antiderivative_tex : "");
    printf("symbolic    %s\n", symbolic_text ? symbolic_text : "");
    printf("symbolic_tex %s\n", symbolic_tex ? symbolic_tex : "");
    printf("symbolic_value %s\n", symbolic_value_text ? symbolic_value_text : "");
    printf("value       %s\n", value_text ? value_text : "");
    printf("error       %s\n", error_text ? error_text : "");
    printf("work_units  %zu\n", ran_numeric_integrator ? intg_get_interval_count_used(ig) : 0u);
    printf("work_cap    %zu\n", ran_numeric_integrator ? (has_max_intervals ? max_intervals : 5000u) : 0u);
    printf("intervals   %zu\n", ran_numeric_integrator ? intg_get_interval_count_used(ig) : 0u);
    printf("max_intervals   %zu\n", ran_numeric_integrator ? (has_max_intervals ? max_intervals : 5000u) : 0u);
    if (!ran_numeric_integrator) {
        if (used_symbolic_numeric_result)
            printf("status      %s\n",
                   used_partial_symbolic_numeric ?
                       "symbolic reduction exact result" :
                       "symbolic exact result");
        else if (used_unevaluated_antiderivative_fallback)
            printf("status      symbolic antiderivative fallback\n");
        else if (ndim == 1u && bound_kinds[0] == INTG_BOUND_INDEFINITE)
            printf("status      symbolic antiderivative\n");
        else if (ndim == 1u && bound_kinds[0] == INTG_BOUND_UPPER_ONLY)
            printf("status      symbolic upper-bound result\n");
        else
            printf("status      symbolic result\n");
    } else if (intg_rc == 0) {
        printf("status      %s\n",
               used_core_exact_result ?
                   "closed-form fast path" :
               used_partial_symbolic_numeric ?
                   "symbolic reduction + converged" :
                   "converged");
    } else if (intg_get_interval_count_used(ig) >=
               (has_max_intervals ? max_intervals : 5000u)) {
        printf("status      max intervals reached\n");
    } else {
        printf("status      precision limit reached\n");
    }
    rc = 0;

cleanup:
    free(antiderivative_tex);
    free(antiderivative_text);
    free(symbolic_value_text);
    free(symbolic_tex);
    free(symbolic_text);
    free(error_text);
    free(value_text);
    num_destroy(&symbolic_num);
    num_destroy(&error_num);
    num_destroy(&value_num);
    free(integrand_tex);
    free(base_tex_text);
    free(tex_text);
    free(binding_expr_text);
    free(expr_text);
    free(display_input);
    expr_free(first_antiderivative);
    expr_free(partially_symbolic_result);
    expr_free(symbolic_result);
    if (hi_expr) {
        for (size_t i = 0; i < ndim; ++i)
            expr_free(hi_expr[i]);
    }
    if (lo_expr) {
        for (size_t i = 0; i < ndim; ++i)
            expr_free(lo_expr[i]);
    }
    if (lo_display_inputs) {
        for (size_t i = 0; i < ndim; ++i)
            free(lo_display_inputs[i]);
    }
    if (hi_display_inputs) {
        for (size_t i = 0; i < ndim; ++i)
            free(hi_display_inputs[i]);
    }
    if (lo_tex_inputs) {
        for (size_t i = 0; i < ndim; ++i)
            free(lo_tex_inputs[i]);
    }
    if (hi_tex_inputs) {
        for (size_t i = 0; i < ndim; ++i)
            free(hi_tex_inputs[i]);
    }
    if (hi_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&hi_num[i]);
    }
    if (lo_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&lo_num[i]);
    }
    if (remaining_hi_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&remaining_hi_num[i]);
    }
    if (remaining_lo_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&remaining_lo_num[i]);
    }
    free(remaining_hi_num);
    free(remaining_lo_num);
    free(remaining_numeric_vars);
    free(hi_num);
    free(lo_num);
    free(bound_kinds);
    free(hi_expr);
    free(lo_expr);
    free(hi_tex_inputs);
    free(lo_tex_inputs);
    free(hi_display_inputs);
    free(lo_display_inputs);
    free(hi_inputs);
    free(lo_inputs);
    free(var_names);
    if (owned_vars && vars) {
        for (size_t i = 0; i < ndim; ++i) {
            if (owned_vars[i])
                expr_free(vars[i]);
        }
    }
    free(owned_vars);
    free(vars);
    intg_free(ig);
    expr_free(display_expr);
    expr_bindings_free(bindings);
    expr_free(expr);
    free(wrapped_input);
    return rc;
}
