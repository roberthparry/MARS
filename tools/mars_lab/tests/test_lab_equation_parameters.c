/**
 * @file test_lab_equation_parameters.c
 * @brief Database-backed equation parameter and dependent-coordinate regressions.
 *
 * Exercises native plot-local constant choices, supplied values, coordinate limits, specialised captions and
 * the default result orientation through real Lab endpoints. Exact caption values and binding roles are checked
 * together. Inputs and expected metadata live in tests/mars_lab/test_mars_lab.sql.
 * These checks do not reinterpret equations or change saved worksheets.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json-t.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_lab_support.h"

static void equation_parameter_row(const void *data)
{
    const test_database_case_t *row = data;
    json_t *payload = json_from_text(row->input), *expected = json_from_text(row->expected);
    const char *route_name = lab_eval_text(expected, "route");
    string_t *route = string_new_with(*route_name ? route_name : "/eval");
    unsigned status = 0;
    json_t *response = lab_eval_request(route, payload, &status), *choices = json_new_object();
    bool ok = response && status == strtoul(lab_eval_text(expected, "status"), NULL, 10) &&
              test_lab_ok(response, status == 200);
    string_t *records = string_new_with(lab_eval_text(response, "plot_slices"));
    size_t count = 0;
    string_t **lines = records && string_length(records) ? string_split(records, "\n", &count) : NULL;
    /* A response has at most eight parameter rows, each containing exactly five protocol fields. */
    for (size_t i = 0; i < count; ++i) {
        size_t fields_count = 0;
        string_t **fields = string_split(lines[i], "\t", &fields_count);
        ok = ok && fields_count == 5;
        if (fields_count == 5)
            lab_eval_set(choices, string_c_str(fields[0]), string_c_str(fields[3]));
        string_split_free(fields, fields_count);
    }
    const json_t *parameters = lab_eval_get(expected, "parameters");
    ok = ok && count == json_object_size(parameters);
    for (size_t i = 0; i < json_object_size(parameters); ++i) {
        const string_t *name = json_object_key_at(parameters, i);
        const string_t *value = name ? json_string_value(json_object_get(parameters, name)) : NULL;
        ok = ok && name && value && !strcmp(lab_eval_text(choices, string_c_str(name)), string_c_str(value));
    }
    static const struct { const char *expected, *response; bool excluded; } fields[] = {
        {"plot_marker", "plot_svg", false},
        {"error_contains", "error", false},
        {"display_contains", "display_TeX", false},
        {"display_excludes", "display_TeX", true},
        {"solutions_contains", "solutions", false},
        {"title_contains", "plot_title", false},
        {"title_excludes", "plot_title", true},
        {"title_TeX_contains", "plot_title_TeX", false},
        {"title_TeX_excludes", "plot_title_TeX", true}
    };
    for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i) {
        const char *fragment = lab_eval_text(expected, fields[i].expected);
        if (*fragment)
            ok = ok && (strstr(lab_eval_text(response, fields[i].response), fragment) != NULL) != fields[i].excluded;
    }
    const char *excluded = lab_eval_text(expected, "plot_excludes"), *svg = lab_eval_text(response, "plot_svg");
    if (*excluded) ok = ok && !strstr(svg, excluded);
    const json_t *axes = lab_eval_get(expected, "axes"), *actual_axes = lab_eval_get(response, "plot_axis_names");
    if (axes) {
        ok = ok && json_array_size(axes) == json_array_size(actual_axes);
        for (size_t i = 0; ok && i < json_array_size(axes); ++i) {
            const string_t *wanted = json_string_value(json_array_get(axes, i));
            const string_t *actual = json_string_value(json_array_get(actual_axes, i));
            ok = wanted && actual && !strcmp(string_c_str(wanted), string_c_str(actual));
            string_t *label = wanted ? string_sprintf(">%s</text>", string_c_str(wanted)) : NULL;
            ok = ok && label && strstr(svg, string_c_str(label));
            string_free(label);
        }
    }
    const char *horizontal = lab_eval_text(expected, "horizontal");
    if (*horizontal)
        ok = ok && !strcmp(lab_eval_text(json_array_get(lab_eval_get(response, "binding_values"), 0), "name"), horizontal);
    const json_t *roles = lab_eval_get(expected, "bindings"), *bindings = lab_eval_get(response, "binding_values");
    const json_t *bound_values = lab_eval_get(expected, "values");
    size_t found = 0, values_found = 0;
    for (size_t i = 0; (roles || bound_values) && i < json_array_size(bindings); ++i) {
        const json_t *binding = json_array_get(bindings, i);
        const char *name = lab_eval_text(binding, "name"), *kind = lab_eval_text(roles, name);
        if (*kind) {
            ++found;
            ok = ok && !strcmp(lab_eval_text(binding, "kind"), kind);
        }
        const char *value = lab_eval_text(bound_values, name);
        if (*value) {
            ++values_found;
            ok = ok && !strcmp(lab_eval_text(binding, "value"), value);
        }
    }
    ok = ok && (!roles || found == json_object_size(roles));
    ok = ok && (!bound_values || values_found == json_object_size(bound_values));
    if (!ok) {
        printf("%s: HTTP %u; parameters %s; error %s; display %s; title %s; title TeX %s\n", string_c_str(row->name), status,
               lab_eval_text(response, "plot_slices"), lab_eval_text(response, "error"),
               lab_eval_text(response, "display_TeX"), lab_eval_text(response, "plot_title"),
               lab_eval_text(response, "plot_title_TeX"));
    }
    string_split_free(lines, count);
    string_free(records);
    json_free(choices);
    json_free(response);
    string_free(route);
    json_free(expected);
    json_free(payload);
    TEST_ASSERT_TRUE(ok, "equation constants remain parameters and native results prefer the dependent coordinate");
}

static void test_lab_equation_parameters(void)
{
    test_run_database_cases(__FILE__, __LINE__, "mars_lab.equation_parameters", NULL, equation_parameter_row);
}

/* Register the native endpoint cases before the suite's final README examples. */
void test_lab_equation_parameter_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_equation_parameters, tests, "equation,plot,parameters");
}
