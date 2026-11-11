/**
 * @file test_lab_integer_plot.c
 * @brief Database-backed endpoint regressions for discrete integer solution-family plots.
 *
 * Checks native complex-plane coordinates, direct distant-viewport selection, empty views and bounded dense
 * diagrams. Authored inputs and expected point counts live in the shared test database; no browser maths is used.
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

static void integer_plot_row(const void *data)
{
    const test_database_case_t *row = data;
    json_t *payload = json_from_text(row->input);
    lab_eval_set(payload, "action", "plot");
    lab_eval_set(payload, "plot_source_mode", "equation");
    lab_eval_set(payload, "wrt", "x");
    string_t *route = string_new_with("/eval");
    unsigned status = 0;
    json_t *result = lab_eval_request(route, payload, &status);
    const char *svg = lab_eval_text(result, "plot_svg"), *cursor = svg;
    size_t points = 0;
    while ((cursor = strstr(cursor, "<circle "))) {
        ++points;
        cursor += 8;
    }
    bool bounded = !strcmp(string_c_str(row->expected), "bounded");
    bool limited = !strcmp(string_c_str(row->expected), "limited");
    bool ok = status == 200 && test_lab_ok(result, true) && strstr(svg, "data-integer-family=\"true\"") &&
              strstr(svg, "Re(x)") && strstr(svg, "Im(x)") &&
              strstr(lab_eval_text(result, "plot_notice"), "Points are not joined") &&
              (bounded ? points && points <= 256 : points == strtoul(string_c_str(row->expected), NULL, 10));
    if (bounded)
        ok = ok && strstr(lab_eval_text(result, "plot_notice"), "Dense view");
    if (!points && !limited)
        ok = ok && strstr(lab_eval_text(result, "plot_notice"), "No solution points");
    if (limited)
        ok = ok && !points && strstr(lab_eval_text(result, "plot_notice"), "indices are limited") &&
             !strstr(lab_eval_text(result, "plot_notice"), "No solution points");
    if (!strcmp(string_c_str(row->name), "integer_plot_distant_view"))
        ok = ok && strstr(svg, "n = 9;") && strstr(svg, "n = 11;");
    if (!ok)
        printf("%s: HTTP %u, %zu points, notice %s, error %s\n", string_c_str(row->name), status, points,
               lab_eval_text(result, "plot_notice"), lab_eval_text(result, "error"));
    json_free(result);
    string_free(route);
    json_free(payload);
    TEST_ASSERT_TRUE(ok, "integer families render genuine discrete branches within the requested complex viewport");
}

static void test_lab_integer_family_plots(void)
{
    test_run_database_cases(__FILE__, __LINE__, "mars_lab.integer_family_plots", NULL, integer_plot_row);
}

/* Register the data-driven cases with the sequential native Lab suite. */
void test_lab_integer_plot_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_integer_family_plots, tests, "plot,equation,integer-family");
}
