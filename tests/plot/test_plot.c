/**
 * @file test_plot.c
 * @brief SVG plot ownership, escaping, clipping, numerical precision and cancellation tests.
 *
 * Exercises the public scene API and parses generated SVG with MARS XML. The
 * README example runs last and leaves its dynamically generated image in memory.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "computation-t.h"
#include "xml-t.h"
#include "plot-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static plot_t *simple_plot(plot_style_t style)
{
    plot_t *plot = plot_new(NULL);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_HALF, NUM_ONE, false}, {NUM_ONE, NUM_ZERO, false}};
    if (plot && !plot_add_points(plot, points, 3, "Curve", style, 0x55aacc)) {
        plot_free(plot);
        plot = NULL;
    }
    return plot;
}

static void test_plot_svg(void)
{
    plot_t *plot = simple_plot(PLOT_LINES);
    string_t *svg = plot_to_svg(plot);
    xml_t *document = svg ? xml_from_text(svg) : NULL;
    bool ok = document && strstr(string_c_str(svg), "data-series=\"0\"") &&
              strstr(string_c_str(svg), "#55aacc") && plot_series_count(plot) == 1;
    xml_free(document);
    plot_free(plot);
    ok = ok && svg && strstr(string_c_str(svg), "</svg>");
    string_free(svg);
    TEST_ASSERT_TRUE(ok, "valid independently owned SVG with axes and series colour");
}

static void test_plot_escaping(void)
{
    plot_options_t options = plot_options_default();
    options.title = "σ <script>& test";
    plot_t *plot = plot_new(&options);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_ONE, NUM_ONE, false}};
    bool ok = plot && plot_add_points(plot, points, 2, "</text><script>", PLOT_POINTS, 0xabcdef);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    xml_t *document = svg ? xml_from_text(svg) : NULL;
    ok = document && strstr(string_c_str(svg), "σ &lt;script&gt;&amp; test") &&
         !strstr(string_c_str(svg), "<script>") && strstr(string_c_str(svg), "<circle");
    xml_free(document);
    string_free(svg);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "plain UTF-8 labels cannot inject markup");
}

static void test_plot_gaps(void)
{
    plot_t *plot = plot_new(NULL);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_HALF, NUM_ONE, true}, {NUM_ONE, NUM_ZERO, true}};
    bool ok = plot && plot_add_points(plot, points, 3, "", PLOT_LINES, 0);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    const char *group = svg ? strstr(string_c_str(svg), "data-series=\"0\"") : NULL;
    const char *end = group ? strstr(group, "</g>") : NULL;
    const char *path = group ? strstr(group, "<path") : NULL;
    ok = group && end && (!path || path > end);
    string_free(svg);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "explicit gaps never produce connecting lines");
}

static void test_plot_limits(void)
{
    plot_options_t options = plot_options_default();
    options.max_points = 1;
    plot_t *plot = plot_new(&options);
    plot_point_t point = {NUM_ONE, NUM_ONE, false};
    bool ok = plot && plot_add_points(plot, &point, 1, "", PLOT_POINTS, 1);
    ok = ok && !plot_add_points(plot, &point, 1, "", PLOT_POINTS, 1) && errno == E2BIG && plot_series_count(plot) == 1;
    ok = ok && !plot_set_bounds(plot, NUM_ONE, NUM_ZERO, NUM_ZERO, NUM_ONE) && errno == EINVAL;
    string_t *svg = plot_to_svg(plot);
    ok = ok && svg;
    string_free(svg);
    plot_free(plot);
    plot = plot_new(NULL);
    ok = ok && !plot_to_svg(plot) && errno == ENODATA;
    plot_free(plot);
    options.width = 0;
    ok = ok && !plot_new(&options) && errno == EINVAL;
    TEST_ASSERT_TRUE(ok, "atomic scene limits, singleton bounds and no-data errors");
}

static void test_plot_precision_and_clip(void)
{
    number_t base = num_create_from_string("1000000000000000000000000000000");
    number_t end = num_add(base, NUM_ONE), outside = num_add(end, NUM_ONE);
    plot_point_t points[] = {{base, NUM_ZERO, true}, {outside, NUM_ONE, false}};
    plot_t *plot = plot_new(NULL);
    bool ok = plot && plot_add_points(plot, points, 2, "", PLOT_LINES, 0) &&
              plot_set_bounds(plot, base, end, NUM_ZERO, NUM_ONE);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), "M 96.000 436.000 L 768.000 242.000");
    plot_auto_bounds(plot);
    string_t *automatic = plot_to_svg(plot);
    ok = ok && automatic && strstr(string_c_str(automatic), "M 96.000 436.000 L 768.000 48.000");
    string_free(automatic);
    string_free(svg);
    plot_free(plot);
    num_destroy(&outside);
    num_destroy(&end);
    num_destroy(&base);
    TEST_ASSERT_TRUE(ok, "normalise exact large offsets before drawing and clip at viewport edges");
}

static bool render_under_control(void *argument)
{
    string_t *svg = plot_to_svg(argument);
    bool ok = svg != NULL;
    string_free(svg);
    return ok;
}

static void test_plot_control(void)
{
    plot_t *plot = simple_plot(PLOT_LINES);
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    bool ok = plot && control && !computation_run(control, render_under_control, plot) &&
              computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    computation_free(control);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "cancelled rendering publishes no partial SVG");
}

static void test_plot_sampler_ownership(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string("x^2", &bindings);
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 4;
    sampler_t *samples = sampler_expression(expression, expr_bindings_get(bindings, "x"), NULL,
                                            NUM_ZERO, NUM_ONE, &options);
    plot_t *plot = plot_new(NULL);
    bool ok = samples && plot && plot_add_sampler(plot, samples, "Line", PLOT_LINES, 0x123456) &&
              plot_add_sampler(plot, samples, "Points", PLOT_POINTS, 0x654321);
    sampler_free(samples);
    expr_free(expression);
    expr_bindings_free(bindings);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && plot_series_count(plot) == 2 && strstr(string_c_str(svg), "data-series=\"1\"") &&
         strstr(string_c_str(svg), "color=\"#654321\"") && strstr(string_c_str(svg), "<circle");
    string_free(svg);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "multiple series own sampler copies after source destruction");
}

static void test_plot_label_notation(void)
{
    plot_t *plot = plot_new(NULL);
    number_t small = num_create_from_double(1e-9);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_ONE, small, false}};
    bool ok = plot && plot_add_points(plot, points, 2, "", PLOT_LINES, 0);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), "×10⁻") && !strstr(string_c_str(svg), "e-09");
    string_free(svg);
    plot_free(plot);
    num_destroy(&small);
    plot_options_t options = plot_options_default();
    options.title = "bad\001title";
    plot = plot_new(&options);
    plot_point_t point = {NUM_ZERO, NUM_ZERO, true};
    ok = ok && plot && plot_add_points(plot, &point, 1, "", PLOT_POINTS, 0) &&
         !plot_to_svg(plot) && errno == EINVAL;
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "scientific ticks use superscripts and invalid XML controls are rejected");
}

static bool axis_labels_match(const string_t *svg, const char *anchor, const char *const labels[], size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        char expected[128];
        snprintf(expected, sizeof(expected), "text-anchor=\"%s\">%s</text>", anchor, labels[i]);
        if (!svg || !strstr(string_c_str(svg), expected)) {
            fprintf(stderr, "Missing axis label: %s\n", expected);
            return false;
        }
    }
    return true;
}

static void test_plot_decimal_ticks(void)
{
    number_t xmin = num_create_from_long(-20), xmax = num_create_from_long(-6);
    number_t ymin = num_create_from_frac(-1, 3), ymax = num_create_from_frac(2, 3);
    plot_t *plot = plot_new(NULL);
    plot_point_t points[] = {{xmin, ymin, true}, {xmax, ymax, false}};
    bool ok = plot && plot_add_points(plot, points, 2, "", PLOT_LINES, 0);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    const char *const xlabels[] = {"-20", "-17.5", "-15", "-12.5", "-10", "-7.5"};
    const char *const ylabels[] = {"-0.2", "0", "0.2", "0.4", "0.6"};
    ok = axis_labels_match(svg, "middle", xlabels, 6) && axis_labels_match(svg, "end", ylabels, 5);
    string_free(svg);
    plot_free(plot);
    num_destroy(&ymax);
    num_destroy(&ymin);
    num_destroy(&xmax);
    num_destroy(&xmin);
    TEST_ASSERT_TRUE(ok, "both axes use rounded decimal steps even with recurring fractional bounds");
}

static void test_plot_extreme_ticks(void)
{
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(2048);
    number_t ten = num_create_from_long(10), large = num_pow_int(ten, 400), small = num_div(NUM_ONE, large);
    plot_t *plot = plot_new(NULL);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {large, small, false}};
    bool ok = plot && plot_add_points(plot, points, 2, "", PLOT_LINES, 0);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    const char *const xlabels[] = {"0", "2×10³⁹⁹", "4×10³⁹⁹", "6×10³⁹⁹", "8×10³⁹⁹", "1×10⁴⁰⁰"};
    const char *const ylabels[] = {"0", "2×10⁻⁴⁰¹", "4×10⁻⁴⁰¹", "6×10⁻⁴⁰¹", "8×10⁻⁴⁰¹", "1×10⁻⁴⁰⁰"};
    ok = axis_labels_match(svg, "middle", xlabels, 6) && axis_labels_match(svg, "end", ylabels, 6);
    string_free(svg);
    plot_free(plot);
    num_destroy(&small);
    num_destroy(&large);
    num_destroy(&ten);
    num_set_default_prec_bits(precision);
    TEST_ASSERT_TRUE(ok, "exact tick labels outside the double range retain superscript scientific notation");
}

static void test_plot_smart_range(void)
{
    plot_options_t options = plot_options_default();
    options.smart_y = true;
    plot_t *smart = plot_new(&options), *full = plot_new(NULL);
    plot_point_t points[129];
    for (size_t i = 0; i < 129; ++i) {
        points[i].x = num_create_from_long((long)i);
        points[i].y = num_create_from_long(i == 64 ? 100000 : (long)i);
        points[i].break_before = false;
    }
    bool ok = smart && full && plot_add_points(smart, points, 129, "", PLOT_LINES, 0) &&
              plot_add_points(full, points, 129, "", PLOT_LINES, 0);
    for (size_t i = 0; i < 129; ++i) {
        num_destroy(&points[i].y);
        num_destroy(&points[i].x);
    }
    string_t *a = ok ? plot_to_svg(smart) : NULL, *b = ok ? plot_to_svg(full) : NULL;
    ok = a && b && strstr(string_c_str(a), "Vertical range clipped") &&
         strstr(string_c_str(a), ">100</text>") && !strstr(string_c_str(b), "Vertical range clipped") &&
         strstr(string_c_str(b), ">100000</text>");
    string_free(a);
    string_free(b);
    ok = ok && plot_set_bounds(smart, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
    a = ok ? plot_to_svg(smart) : NULL;
    ok = ok && a && strstr(string_c_str(a), ">0.2</text>");
    string_free(a);
    plot_free(full);
    plot_free(smart);
    TEST_ASSERT_TRUE(ok, "robust Y view labels cropping, Fit all retains outliers and manual bounds override it");
}

/* README example: create a native SVG from three measured points, entirely in memory. */
static void example_plot_svg(void)
{
    plot_options_t options = plot_options_default();
    options.title = "Measured curve";
    options.smart_y = true;
    plot_t *plot = plot_new(&options);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_HALF, NUM_ONE, false}, {NUM_ONE, NUM_ZERO, false}};
    bool ok = plot && plot_add_points(plot, points, 3, "Measurements", PLOT_LINES, 0x55aacc);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg)
        printf("SVG ready; series: %zu\n", plot_series_count(plot));
    ok = svg != NULL;
    string_free(svg);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "README in-memory SVG");
}

int tests_main(void)
{
    TEST_RUN_CASE(test_plot_svg, NULL);
    TEST_RUN_CASE(test_plot_escaping, NULL);
    TEST_RUN_CASE(test_plot_gaps, NULL);
    TEST_RUN_CASE(test_plot_limits, NULL);
    TEST_RUN_CASE(test_plot_precision_and_clip, NULL);
    TEST_RUN_CASE(test_plot_control, NULL);
    TEST_RUN_CASE(test_plot_sampler_ownership, NULL);
    TEST_RUN_CASE(test_plot_label_notation, NULL);
    TEST_RUN_CASE(test_plot_decimal_ticks, NULL);
    TEST_RUN_CASE(test_plot_extreme_ticks, NULL);
    TEST_RUN_CASE(test_plot_smart_range, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_plot_svg);
    return TEST_EXIT_CODE();
}
