/**
 * @file lab_worker_de_parameters.c
 * @brief Native editable ODE parameter envelopes and plot-local slider choices.
 *
 * Uses the differential problem's own constant catalogue and parameter assumptions. Slider values affect only
 * an isolated plotting problem; the original worksheet and its symbolic solution remain unchanged. The shared
 * native editor preserves authored restrictions, and the existing slider protocol supplies browser controls.
 * Scalar time plots share a lower-endpoint policy derived from the problem's point conditions.
 */
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "json-t.h"
#include "lab_presentation.h"
#define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS
#include "internal/diffequation_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static bool put(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static bool text(json_t *object, const char *name, const char *value)
{
    string_t *source = string_new_with(value);
    json_t *node = source ? json_new_string(source) : NULL;
    bool ok = put(object, name, node);
    json_free(node); string_free(source);
    return ok;
}

/* Keep all unknown coefficients editable, retaining the native envelope's conditions and coordinate names. */
string_t *lab_worker_de_source(const diffequ_t *de, const char *authored)
{
    string_t *source = authored ? string_new_with(authored) : de_to_text(de, style_EXPRESSION);
    json_t *request = json_new_object(), *bindings = json_new_array();
    expr_bindings_t *constants = de_constants(de);
    bool ok = source && request && bindings;
    for (size_t i = 0; ok && i < expr_bindings_count(constants); ++i) {
        number_t value = expr_get_val(expr_bindings_expr_at(constants, i));
        if (!num_is_nan(value)) {
            num_destroy(&value);
            continue; /* Existing exact assignments remain in the native problem envelope. */
        }
        string_t *rendered = string_new_with("?");
        json_t *entry = json_new_object();
        ok = entry && rendered && text(entry, "name", expr_bindings_name_at(constants, i)) &&
             text(entry, "kind", "constant") && text(entry, "value", string_c_str(rendered)) &&
             json_array_append(bindings, entry);
        json_free(entry); string_free(rendered); num_destroy(&value);
    }
    ok = ok && text(request, "action", "editor") && text(request, "operation", "bindings") &&
         text(request, "syntax", "diffequation") &&
         text(request, "mode", "merge") && text(request, "text", string_c_str(source)) &&
         put(request, "bindings", bindings);
    unsigned status = 0;
    json_t *response = ok ? lab_presentation_request(request, &status) : NULL;
    string_t *key = string_new_with("editor"), *expression_key = string_new_with("expression");
    const json_t *editor = response && key ? json_object_get(response, key) : NULL;
    const string_t *formatted = editor && expression_key ? json_string_value(json_object_get(editor, expression_key)) : NULL;
    string_t *result = status == 200u && formatted ? string_clone(formatted) : NULL;
    string_free(expression_key); string_free(key);
    json_free(response); json_free(bindings); json_free(request); string_free(source);
    return result;
}

/* Time advances from zero unless authored point data begin earlier; explicit plot bounds are handled by callers. */
string_t *lab_worker_de_time_lower(const diffequ_t *de, const char *axis)
{
    if (!de || !axis || strcmp(axis, "t") || de_independent_count(de) != 1u)
        return NULL;
    number_t lower = num_clone(NUM_ZERO);
    /* Every authored condition may supply the earliest time; work is bounded by the problem's condition count. */
    for (size_t i = 0u; i < de_condition_count(de); ++i) {
        if (de_condition_argument_count(de, i) != 1u)
            continue;
        number_t point = expr_eval(de_condition_argument_at(de, i, 0u));
        if (num_is_real(point) && num_is_finite(point) && num_lt(point, lower)) {
            num_destroy(&lower);
            lower = num_clone(point);
        }
        num_destroy(&point);
    }
    string_t *text = num_to_string(lower);
    num_destroy(&lower);
    return text;
}

/* Re-solve the actual native problem after selecting temporary coefficients, including repeated or complex roots. */
bool lab_worker_de_plot_parameters(diffequ_t *de, const char *request)
{
    expr_bindings_t *constants = de_constants(de);
    bool needed = false;
    for (size_t i = 0; i < expr_bindings_count(constants); ++i) {
        number_t value = expr_get_val(expr_bindings_expr_at(constants, i));
        needed |= num_is_nan(value);
        num_destroy(&value);
    }
    if (!needed) return true;
    expr_t *domain = de_parameter_domain(de);
    char *body = domain ? expr_to_string(domain, style_UNBOUND) : NULL;
    string_t *source = body ? string_sprintf("{ %s | ; ", body) : NULL;
    bool ok = source != NULL;
    for (size_t i = 0; ok && i < expr_bindings_count(constants); ++i) {
        expr_t *parameter = expr_bindings_expr_at(constants, i);
        char *name = expr_to_string(parameter, style_UNBOUND);
        number_t value = expr_get_val(parameter);
        string_t *rendered = num_is_nan(value) ? string_new_with("?") : num_to_string(value);
        ok = name && rendered && string_append_format(source, "%s%s = %S", i ? ", " : "", name, rendered) >= 0;
        string_free(rendered); free(name); num_destroy(&value);
    }
    ok = ok && string_append_cstr(source, " }") == 0;
    expr_bindings_t *selected = NULL;
    char *sliced = ok ? lab_worker_parameter_slices(string_c_str(source), NULL, NULL, request, &selected) : NULL;
    ok = sliced != NULL;
    for (size_t i = 0; ok && i < expr_bindings_count(constants); ++i) {
        expr_t *parameter = expr_bindings_expr_at(constants, i);
        number_t value = expr_get_val(parameter);
        if (num_is_nan(value)) {
            expr_t *choice = expr_bindings_get(selected, expr_bindings_name_at(constants, i));
            number_t chosen = choice ? expr_get_val(choice) : NUM_NAN;
            ok = num_is_real(chosen) && num_is_finite(chosen);
            if (ok) expr_set_val(parameter, chosen);
            num_destroy(&chosen);
        }
        num_destroy(&value);
    }
    free(sliced); expr_bindings_free(selected); string_free(source); free(body); expr_free(domain);
    return ok;
}
