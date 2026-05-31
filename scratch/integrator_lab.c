#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "integrator.h"
#include "number.h"

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

static int parse_number_bound(const char *text, int precision, number_t *out)
{
    if (!out)
        return 1;
    *out = num_new();
    return parse_number_expression(text, precision, out);
}

static char *number_text(number_t value)
{
    return num_to_string(value);
}

static char *integral_tex(const char *body,
                          size_t ndim,
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

        if (!lo[i] || !hi[i]) {
            free(result);
            return NULL;
        }

        wrote = snprintf(result + len, cap - len,
                         "\\int_{%s}^{%s} ", lo[i], hi[i]);
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
    const char **var_names = NULL;
    const char **lo_inputs = NULL;
    const char **hi_inputs = NULL;
    number_t *lo_num = NULL;
    number_t *hi_num = NULL;
    number_t value_num = num_new();
    number_t error_num = num_new();
    char *expr_text = NULL;
    char *tex_text = NULL;
    char *integrand_tex = NULL;
    char *value_text = NULL;
    char *error_text = NULL;
    char *display_input = NULL;
    expr_t *display_expr = NULL;
    int rc = 1;
    int intg_rc;

    while (argi < argc && strncmp(argv[argi], "--", 2u) == 0) {
        if (strcmp(argv[argi], "--max-intervals") == 0) {
            char *end = NULL;
            unsigned long parsed;

            if (argi + 1 >= argc) {
                fprintf(stderr, "--max-intervals needs a value\n");
                return 1;
            }
            parsed = strtoul(argv[argi + 1], &end, 10);
            if (!end || *end != '\0' || parsed == 0ul) {
                fprintf(stderr, "Bad --max-intervals value\n");
                return 1;
            }
            max_intervals = (size_t)parsed;
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
    if (!expr || !bindings) {
        fprintf(stderr, "Could not parse integrand expression\n");
        goto cleanup;
    }

    vars = calloc(ndim, sizeof(*vars));
    var_names = calloc(ndim, sizeof(*var_names));
    lo_inputs = calloc(ndim, sizeof(*lo_inputs));
    hi_inputs = calloc(ndim, sizeof(*hi_inputs));
    lo_num = calloc(ndim, sizeof(*lo_num));
    hi_num = calloc(ndim, sizeof(*hi_num));
    if (!vars || !var_names || !lo_inputs || !hi_inputs || !lo_num || !hi_num)
        goto cleanup;

    if (argi + 2 >= argc) {
        vars[0] = expr_bindings_get(bindings, "x");
        var_names[0] = "x";
        lo_inputs[0] = "0";
        hi_inputs[0] = "pi";
        lo_num[0] = num_create_from_long(0);
        hi_num[0] = num_clone(NUM_PI);
        ndim = 1u;
    } else {
        for (size_t i = 0; i < ndim; ++i) {
            const char *name = argv[argi + 2 + (int)(i * 3u)];
            const char *lo_text_in = argv[argi + 3 + (int)(i * 3u)];
            const char *hi_text_in = argv[argi + 4 + (int)(i * 3u)];

            vars[i] = expr_bindings_get(bindings, name);
            if (!vars[i]) {
                fprintf(stderr, "No binding named '%s'\n", name);
                goto cleanup;
            }
            if (parse_number_bound(lo_text_in, precision, &lo_num[i]) != 0 ||
                parse_number_bound(hi_text_in, precision, &hi_num[i]) != 0) {
                fprintf(stderr, "Could not parse bounds for %s\n", name);
                goto cleanup;
            }
            var_names[i] = name;
            lo_inputs[i] = lo_text_in;
            hi_inputs[i] = hi_text_in;
        }
    }

    ig = intg_new();
    if (!ig)
        goto cleanup;
    if (has_max_intervals)
        intg_set_interval_count_max(ig, max_intervals);

    expr_text = expr_to_string(expr, style_UNBOUND);
    if (expr_text) {
        display_input = wrap_expression(expr_text);
        display_expr = expr_from_string(display_input ? display_input : expr_text, NULL);
    }
    integrand_tex = display_expr ? expr_to_string(display_expr, style_TEX) : NULL;

    if (ndim == 1u)
        intg_rc = intg_single_integral(ig, expr, vars[0], lo_num[0], hi_num[0],
                                   &value_num, &error_num);
    else
        intg_rc = intg_integral_multi(ig, expr, ndim, vars, lo_num, hi_num,
                                  &value_num, &error_num);
    if (intg_rc < 0) {
        fprintf(stderr, "Integration failed\n");
        goto cleanup;
    }
    value_text = number_text(value_num);
    error_text = number_text(error_num);

    tex_text = integrand_tex ? integral_tex(integrand_tex, ndim, var_names, lo_inputs, hi_inputs) : NULL;

    printf("input       %s\n", input);
    printf("expression  %s\n", expr_text ? expr_text : "(null)");
    printf("dimensions  %zu\n", ndim);
    for (size_t i = 0; i < ndim; ++i)
        printf("bound       %s in [%s, %s]\n", var_names[i], lo_inputs[i], hi_inputs[i]);
    printf("tex         %s\n", tex_text ? tex_text : "(null)");
    printf("value       %s\n", value_text ? value_text : "(null)");
    printf("error       %s\n", error_text ? error_text : "(null)");
    printf("work_units  %zu\n", intg_get_interval_count_used(ig));
    printf("work_cap    %zu\n", has_max_intervals ? max_intervals : 5000u);
    printf("intervals   %zu\n", intg_get_interval_count_used(ig));
    printf("max_intervals   %zu\n", has_max_intervals ? max_intervals : 5000u);
    if (intg_rc == 0) {
        printf("status      converged\n");
    } else if (intg_get_interval_count_used(ig) >=
               (has_max_intervals ? max_intervals : 5000u)) {
        printf("status      max intervals reached\n");
    } else {
        printf("status      precision limit reached\n");
    }
    rc = 0;

cleanup:
    free(error_text);
    free(value_text);
    num_destroy(&error_num);
    num_destroy(&value_num);
    free(integrand_tex);
    free(tex_text);
    free(expr_text);
    free(display_input);
    if (hi_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&hi_num[i]);
    }
    if (lo_num) {
        for (size_t i = 0; i < ndim; ++i)
            num_destroy(&lo_num[i]);
    }
    free(hi_num);
    free(lo_num);
    free(hi_inputs);
    free(lo_inputs);
    free(var_names);
    free(vars);
    intg_free(ig);
    expr_free(display_expr);
    expr_bindings_free(bindings);
    expr_free(expr);
    free(wrapped_input);
    return rc;
}
