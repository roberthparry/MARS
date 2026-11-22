/**
 * @file test_lab_equation_parameters.c
 * @brief Database-backed equation parameter and dependent-coordinate regressions.
 *
 * Exercises native plot-local constant choices, supplied values, coordinate limits, specialised captions and
 * the default result orientation through real Lab endpoints. Exact caption values and binding roles are checked
 * together. Inputs and expected metadata live in tests/mars_lab/test_mars_lab.sql.
 * These checks do not reinterpret equations or change saved worksheets.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json-t.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_lab_support.h"

static bool svg_number(const char *svg, const char *name, double *value)
{
    char key[64];
    snprintf(key, sizeof key, "%s=\"", name);
    const char *field = strstr(svg, key);
    if (!field) return false;
    char *end = NULL;
    *value = strtod(field + strlen(key), &end);
    return end && *end == '"' && isfinite(*value);
}

static bool contour_radii(const char *svg, const json_t *expected)
{
    const json_t *radii = lab_eval_get(expected, "contour_radii");
    size_t count = json_array_size(radii);
    if (!count) return true;
    if (count > 16) return false;
    double frame[8], centre[2] = {0, 0}, wanted[16];
    static const char *const names[] = {"data-plot-left", "data-plot-top", "data-plot-width", "data-plot-height",
                                        "data-x-min", "data-x-max", "data-y-min", "data-y-max"};
    for (unsigned i = 0; i < 8; ++i)
        if (!svg_number(svg, names[i], &frame[i])) return false;
    for (size_t i = 0; i < count; ++i) {
        const string_t *value = json_number_text(json_array_get(radii, i));
        if (!value) return false;
        wanted[i] = strtod(string_c_str(value), NULL);
    }
    for (unsigned i = 0; i < 2; ++i) {
        const string_t *value = json_number_text(json_array_get(lab_eval_get(expected, "contour_centre"), i));
        if (value) centre[i] = strtod(string_c_str(value), NULL);
    }
    const char *series = strstr(svg, "data-series=\"0\""), *end = series ? strstr(series, "</g>") : NULL;
    if (!end || !(frame[2] > 0) || !(frame[3] > 0)) return false;
    unsigned found = 0, points = 0;
    for (const char *path = strstr(series, "<path d=\"M "); path && path < end;
         path = strstr(path + 1, "<path d=\"M ")) {
        double xy[4];
        if (sscanf(path, "<path d=\"M %lf %lf L %lf %lf", xy, xy + 1, xy + 2, xy + 3) != 4) return false;
        for (unsigned vertex = 0; vertex < 2; ++vertex) {
            double x = frame[4] + (xy[vertex * 2] - frame[0]) / frame[2] * (frame[5] - frame[4]);
            double y = frame[7] - (xy[vertex * 2 + 1] - frame[1]) / frame[3] * (frame[7] - frame[6]);
            double radius = hypot(x - centre[0], y - centre[1]);
            bool matches = false;
            /* At most sixteen expected components; every native sampled endpoint must belong to one. */
            for (size_t i = 0; i < count; ++i)
                if (fabs(radius - wanted[i]) < wanted[i] * 0.001 + 0.001) {
                    found |= 1u << i;
                    matches = true;
                }
            if (!matches) return false;
            ++points;
        }
    }
    return points > 20 && found == (1u << count) - 1;
}

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
    const json_t *fragments = lab_eval_get(expected, "display_fragments");
    for (size_t i = 0; i < json_array_size(fragments); ++i) {
        const string_t *fragment = json_string_value(json_array_get(fragments, i));
        ok = ok && fragment && strstr(lab_eval_text(response, "display_TeX"), string_c_str(fragment));
    }
    const char *condition_count = lab_eval_text(expected, "display_min_condition_rows");
    if (*condition_count) {
        const char *display = lab_eval_text(response, "display_TeX"), *cursor = display;
        size_t rows = 0u;
        while ((cursor = strstr(cursor, "\\\\\n&\\quad "))) {
            ++rows;
            cursor += strlen("\\\\\n&\\quad ");
        }
        const char *conditions = strstr(display, "\\\\\n&\\text{provided that}");
        const char *formula = strstr(display, " &=");
        ok = ok && conditions && formula && formula < conditions &&
             rows >= strtoul(condition_count, NULL, 10);
    }
    const char *excluded = lab_eval_text(expected, "plot_excludes"), *svg = lab_eval_text(response, "plot_svg");
    if (*excluded) ok = ok && !strstr(svg, excluded);
    ok = ok && contour_radii(svg, expected);
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
        printf("Bindings: %s\n", lab_eval_text(response, "bindings"));
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
