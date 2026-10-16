/**
 * @file test_plot.c
 * @brief SVG plot ownership, escaping, clipping, numerical precision and cancellation tests.
 *
 * Exercises the public scene API and parses generated SVG with MARS XML. The
 * README example runs last and leaves its dynamically generated image in memory.
 */
#include <errno.h>
#include <math.h>
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
              strstr(string_c_str(svg), "#55aacc") && plot_series_count(plot) == 1 &&
              strstr(string_c_str(svg), "data-plot-axes") && strstr(string_c_str(svg), "stroke-dasharray=\"3 6\"") &&
              strstr(string_c_str(svg), "data-plot-tick=\"0\"") && strstr(string_c_str(svg), "data-plot-tick=\"1\"") &&
              !strstr(string_c_str(svg), "<rect") && !strstr(string_c_str(svg), "Vertical range clipped");
    xml_free(document);
    plot_free(plot);
    ok = ok && svg && strstr(string_c_str(svg), "</svg>");
    string_free(svg);
    TEST_ASSERT_TRUE(ok, "valid independently owned SVG with axes and series colour");
    plot = simple_plot(PLOT_LINES);
    ok = plot_shade_interval(plot, NUM_ONE, NUM_ZERO);
    svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), "data-integral-shading") &&
         strstr(string_c_str(svg), "data-sign=\"negative\"") &&
         strstr(string_c_str(svg), "data-integral-limits");
    string_free(svg);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "reversed intervals shade negative contributions and retain limit markers");
    const number_t intervals[][2] = {{NUM_NINF, NUM_ONE}, {NUM_ZERO, NUM_INF},
                                    {NUM_NINF, NUM_INF}, {NUM_INF, NUM_NINF}};
    for (size_t i = 0; i < sizeof(intervals) / sizeof(*intervals); ++i) {
        plot = simple_plot(PLOT_LINES);
        ok = plot_shade_interval(plot, intervals[i][0], intervals[i][1]);
        svg = ok ? plot_to_svg(plot) : NULL;
        ok = svg && strstr(string_c_str(svg), "data-integral-shading") &&
             strstr(string_c_str(svg), i == 3 ? "data-sign=\"negative\"" : "data-sign=\"positive\"") &&
             !strstr(string_c_str(svg), "nan") && !strstr(string_c_str(svg), "inf");
        string_free(svg);
        TEST_ASSERT_TRUE(!plot_shade_interval(plot, NUM_NAN, NUM_ONE), "NaN shading limits remain invalid");
        plot_free(plot);
        TEST_ASSERT_TRUE(ok, "infinite intervals retain signed shading with finite SVG coordinates");
    }
}

static void test_plot_edge_axes(void)
{
    const long ranges[][2] = {{1, 2}, {-2, -1}, {-1, 0}, {-1, 1}};
    bool ok = true;
    for (size_t i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        plot_t *plot = simple_plot(PLOT_LINES);
        number_t lower = num_create_from_long(ranges[i][0]), upper = num_create_from_long(ranges[i][1]);
        bool bounded = plot && plot_set_bounds(plot, lower, upper, lower, upper);
        string_t *svg = bounded ? plot_to_svg(plot) : NULL;
        const char *axes = svg ? strstr(string_c_str(svg), "data-plot-axes") : NULL;
        const char *end = axes ? strstr(axes, "</g>") : NULL;
        const char *bottom = axes ? strstr(axes, "M 96.000 436.000 L 768.000 436.000") : NULL;
        const char *left = axes ? strstr(axes, "M 96.000 48.000 L 96.000 436.000") : NULL;
        size_t paths = 0;
        for (const char *path = axes ? strstr(axes, "<path") : NULL; path && end && path < end;
             path = strstr(path + 5, "<path"))
            ++paths;
        ok = ok && end && bottom && left && bottom < end && left < end && paths == (i == 3 ? 4 : 2);
        string_free(svg);
        num_destroy(&upper);
        num_destroy(&lower);
        plot_free(plot);
    }
    TEST_ASSERT_TRUE(ok, "bottom and left axes remain visible for every range, with top and right edges open");
}

static void test_plot_escaping(void)
{
    plot_options_t options = plot_options_default();
    options.title = "σ <script>& test";
    options.legend = false;
    plot_t *plot = plot_new(&options);
    plot_point_t points[] = {{NUM_ZERO, NUM_ZERO, true}, {NUM_ONE, NUM_ONE, false}};
    bool ok = plot && plot_add_points(plot, points, 2, "</text><script>", PLOT_POINTS, 0xabcdef);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    xml_t *document = svg ? xml_from_text(svg) : NULL;
    ok = document && strstr(string_c_str(svg), "σ &lt;script&gt;&amp; test") &&
         strstr(string_c_str(svg), "<title>&lt;/text&gt;&lt;script&gt;</title>") &&
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
    options = plot_options_default();
    options.axis_colour = 0x1000000;
    ok = ok && !plot_new(&options) && errno == EINVAL;
    options = plot_options_default();
    options.grid_colour = 0x1000000;
    ok = ok && !plot_new(&options) && errno == EINVAL;
    options = plot_options_default();
    options.label_colour = 0x1000000;
    ok = ok && !plot_new(&options) && errno == EINVAL;
    TEST_ASSERT_TRUE(ok, "atomic scene limits, singleton bounds and no-data errors");
}

static bool viewport_segment(const string_t *svg, double end_height)
{
    const char *view = svg ? strstr(string_c_str(svg), "data-plot-left=") : NULL;
    const char *series = svg ? strstr(string_c_str(svg), "data-series=") : NULL;
    double left, top, width, height;
    if (!view || !series || sscanf(view, "data-plot-left=\"%lf\" data-plot-top=\"%lf\" "
                                  "data-plot-width=\"%lf\" data-plot-height=\"%lf\"",
                                  &left, &top, &width, &height) != 4)
        return false;
    char expected[160];
    snprintf(expected, sizeof(expected), "M %.3f %.3f L %.3f %.3f",
             left, top + height, left + width, top + end_height * height);
    return strstr(series, expected) != NULL;
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
    ok = viewport_segment(svg, 0.5);
    plot_auto_bounds(plot);
    string_t *automatic = plot_to_svg(plot);
    ok = ok && viewport_segment(automatic, 0);
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
    ok = svg && strstr(string_c_str(svg), "×10⁻") && strstr(string_c_str(svg), "data-y-max=\"");
    /* Machine-readable viewport attributes may use exponents; visible tick labels must not. */
    const char *cursor = svg ? string_c_str(svg) : NULL;
    while (ok && (cursor = strstr(cursor, "<text"))) {
        const char *end = strstr(cursor, "</text>"), *exponent = strstr(cursor, "e-09");
        ok = end && (!exponent || exponent > end);
        cursor = end ? end + 7 : NULL;
    }
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

static void test_plot_extreme_clipping(void)
{
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(2048);
    number_t huge = num_pow10(400), negative = num_neg(huge);
    plot_point_t edges[][2] = {
        {{negative, NUM_HALF, true}, {huge, NUM_HALF, false}},
        {{NUM_HALF, huge, true}, {NUM_HALF, negative, false}},
        {{NUM_HALF, NUM_HALF, true}, {huge, huge, false}},
        {{huge, huge, true}, {NUM_HALF, NUM_HALF, false}},
        {{negative, negative, true}, {huge, huge, false}}
    };
    const char *const expected[] = {
        "M 96.000 242.000 L 768.000 242.000", "M 432.000 48.000 L 432.000 436.000",
        "M 432.000 242.000 L 768.000 48.000", "M 768.000 48.000 L 432.000 242.000",
        "M 96.000 436.000 L 768.000 48.000"
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(edges) / sizeof(*edges); ++i) {
        plot_t *plot = plot_new(NULL);
        bool added = plot && plot_add_points(plot, edges[i], 2, "", PLOT_LINES, 0) &&
                     plot_set_bounds(plot, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
        string_t *svg = added ? plot_to_svg(plot) : NULL;
        const char *series = svg ? strstr(string_c_str(svg), "data-series=\"0\"") : NULL;
        ok = ok && series && strstr(series, expected[i]);
        string_free(svg);
        plot_free(plot);
    }
    plot_t *plot = plot_new(NULL);
    plot_point_t points[] = {{huge, huge, true}, {huge, negative, false}, {NUM_HALF, NUM_HALF, true}};
    bool added = plot && plot_add_points(plot, points, 3, "", PLOT_LINES, 0) &&
                 plot_add_points(plot, points, 3, "", PLOT_POINTS, 0) &&
                 plot_set_bounds(plot, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
    string_t *svg = added ? plot_to_svg(plot) : NULL;
    const char *series = svg ? strstr(string_c_str(svg), "data-series=\"0\"") : NULL;
    ok = ok && series && !strstr(series, "<path") && strstr(series, "<circle cx=\"432.000\" cy=\"242.000\"") &&
         !strstr(series, "Vertical range clipped");
    string_free(svg);
    plot_free(plot);
    num_destroy(&negative);
    num_destroy(&huge);
    num_set_default_prec_bits(precision);
    TEST_ASSERT_TRUE(ok, "clip huge crossing segments before conversion; omit outside points and preserve explicit gaps");
}

static void test_plot_smart_range(void)
{
    plot_options_t options = plot_options_default();
    options.smart_y = true;
    plot_t *smart = plot_new(&options), *full = plot_new(NULL);
    options.smart_y = false;
    options.guard_gaps = true;
    plot_t *continuous = plot_new(&options), *broken = plot_new(&options);
    plot_point_t points[129];
    for (size_t i = 0; i < 129; ++i) {
        points[i].x = num_create_from_long((long)i);
        points[i].y = num_create_from_long(i == 64 ? 100000 : (long)i);
        points[i].break_before = false;
    }
    bool ok = smart && full && plot_add_points(smart, points, 129, "", PLOT_LINES, 0) &&
              plot_add_points(full, points, 129, "", PLOT_LINES, 0);
    ok = ok && continuous && broken && plot_add_points(continuous, points, 129, "", PLOT_LINES, 0);
    points[64].break_before = true;
    ok = ok && plot_add_points(broken, points, 129, "", PLOT_LINES, 0);
    for (size_t i = 0; i < 129; ++i) {
        num_destroy(&points[i].y);
        num_destroy(&points[i].x);
    }
    string_t *a = ok ? plot_to_svg(smart) : NULL, *b = ok ? plot_to_svg(full) : NULL;
    ok = a && b && !strstr(string_c_str(a), "Vertical range clipped") &&
         strstr(string_c_str(a), ">100</text>") && !strstr(string_c_str(b), "Vertical range clipped") &&
         strstr(string_c_str(b), ">100000</text>");
    string_free(a);
    string_free(b);
    a = ok ? plot_to_svg(continuous) : NULL;
    b = ok ? plot_to_svg(broken) : NULL;
    ok = ok && a && b && strstr(string_c_str(a), ">100000</text>") &&
         !strstr(string_c_str(b), ">100000</text>") && strstr(string_c_str(b), ">100</text>");
    string_free(a);
    string_free(b);
    ok = ok && plot_set_bounds(broken, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
    a = ok ? plot_to_svg(broken) : NULL;
    ok = ok && a && strstr(string_c_str(a), ">0.2</text>");
    string_free(a);
    plot_free(broken);
    plot_free(continuous);
    ok = ok && plot_set_bounds(smart, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
    a = ok ? plot_to_svg(smart) : NULL;
    ok = ok && a && strstr(string_c_str(a), ">0.2</text>");
    string_free(a);
    plot_free(full);
    plot_free(smart);
    TEST_ASSERT_TRUE(ok, "robust Y view crops silently, Fit all retains outliers and manual bounds override it");
}

static void test_plot_stationary_range(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string("100/(1+10000*(x-0.371)^2)", &bindings);
    sampler_options_t sampling = sampler_options_default();
    sampling.intervals = 1;
    sampling.max_depth = 16;
    sampler_t *samples = sampler_expression(expression, expr_bindings_get(bindings, "x"), NULL,
                                             NUM_ZERO, NUM_ONE, &sampling);
    plot_options_t options = plot_options_default();
    options.smart_y = true;
    plot_t *plot = plot_new(&options);
    bool ok = samples && plot && plot_add_sampler(plot, samples, "", PLOT_LINES, 0);
    sampler_free(samples);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), ">80</text>");
    string_free(svg);
    ok = ok && plot_set_bounds(plot, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE);
    svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), ">0.2</text>");
    string_free(svg);
    plot_free(plot);
    expr_free(expression);
    expr_bindings_free(bindings);
    plot = plot_new(&options);
    number_t coordinates[] = {NUM_ZERO, NUM_ONE}, heights[] = {NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ZERO};
    number_t peak = num_create_from_long(100);
    ok = ok && plot && plot_set_surface(plot, coordinates, 2, coordinates, 2, heights) &&
         plot_include_height(plot, peak) && !plot_include_height(plot, NUM_INF);
    svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && strstr(string_c_str(svg), ">80</text>");
    string_free(svg);
    ok = ok && plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE);
    svg = ok ? plot_to_svg(plot) : NULL;
    ok = svg && !strstr(string_c_str(svg), ">80</text>");
    string_free(svg);
    num_destroy(&peak);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "smart viewport preserves a located finite peak; explicit limits still win");
}

static bool ascending_surface_ticks(const string_t *svg)
{
    double previous[3] = {0};
    size_t counts[3] = {0};
    const char *p = svg ? string_c_str(svg) : "";
    while ((p = strstr(p, "<g data-plot-tick="))) {
        unsigned axis;
        double x, y;
        const char *path = strstr(p, "<path d=");
        if (sscanf(p, "<g data-plot-tick=\"%u\"", &axis) != 1 || axis > 2 || !path ||
            sscanf(path, "<path d=\"M %lf %lf", &x, &y) != 2)
            return false;
        double position = axis == 2 ? -y : x;
        if (counts[axis] && position <= previous[axis])
            return false;
        previous[axis] = position;
        ++counts[axis];
        ++p;
    }
    return counts[0] > 1 && counts[1] > 1 && counts[2] > 1;
}

static void test_plot_surface(void)
{
    {
        plot_t *empty = plot_new(NULL);
        bool ok = empty && plot_set_mesh(empty, NULL, NULL, NULL, 0);
        string_t *missing_bounds = ok ? plot_to_svg(empty) : NULL;
        ok = ok && !missing_bounds && plot_set_bounds(empty, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE) &&
             plot_set_height_bounds(empty, NUM_ZERO, NUM_ONE);
        string_t *svg = ok ? plot_to_svg(empty) : NULL;
        xml_t *document = svg ? xml_from_text(svg) : NULL;
        ok = ok && document && strstr(string_c_str(svg), "data-empty=\"true\"") &&
             strstr(string_c_str(svg), "data-axis-arrow") && !strstr(string_c_str(svg), "<path fill=") &&
             !strstr(string_c_str(svg), "nan");
        xml_free(document);
        string_free(svg);
        string_free(missing_bounds);
        plot_free(empty);
        TEST_ASSERT_TRUE(ok, "empty meshes require explicit bounds and retain valid axes without invented geometry");
    }
    {
        number_t x[] = {NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO, NUM_ONE, NUM_ZERO};
        number_t y[] = {NUM_ZERO, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ZERO, NUM_ONE};
        number_t z[] = {NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ONE};
        plot_t *mesh = plot_new(NULL);
        bool ok = plot_set_mesh(mesh, x, y, z, 6);
        string_t *svg = ok ? plot_to_svg(mesh) : NULL;
        xml_t *document = svg ? xml_from_text(svg) : NULL;
        ok = document && strstr(string_c_str(svg), "data-surface") && plot_series_count(mesh) == 1;
        xml_free(document);
        string_free(svg);
        plot_free(mesh);
        TEST_ASSERT_TRUE(ok, "independent facets support multiple heights at the same horizontal coordinates");
    }
    plot_t *plot = plot_new(NULL);
    number_t axis[4], heights[16];
    for (size_t i = 0; i < 4; ++i)
        axis[i] = num_create_from_long((long)i);
    for (size_t i = 0; i < 16; ++i)
        heights[i] = num_create_from_long((long)(i / 4 + i % 4));
    num_destroy(&heights[0]);
    heights[0] = num_clone(NUM_NAN);
    bool ok = plot && plot_set_surface(plot, axis, 4, axis, 4, heights) && plot_series_count(plot) == 1;
    ok = ok && !plot_set_surface(plot, axis, 4, axis, 4, heights) && errno == EINVAL;
    ok = ok && !plot_set_bounds(plot, NUM_ZERO, NUM_ONE, NUM_ZERO, NUM_ONE) && errno == EINVAL;
    for (size_t i = 0; i < 16; ++i)
        num_destroy(&heights[i]);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&axis[i]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    xml_t *document = svg ? xml_from_text(svg) : NULL;
    size_t faces = 0;
    for (const char *p = svg ? strstr(string_c_str(svg), "<path fill=") : NULL; p; p = strstr(p + 10, "<path fill="))
        ++faces;
    ok = ok && document && faces == 16 && strstr(string_c_str(svg), "data-surface=\"true\"") &&
         !strstr(string_c_str(svg), "nan") && !strstr(string_c_str(svg), "<rect");
    TEST_ASSERT_TRUE(svg && strstr(string_c_str(svg), "data-azimuth=\"315.000\"") && ascending_surface_ticks(svg),
                     "default surface ticks increase left to right on both horizontal axes and upwards in height");
    xml_free(document);
    string_free(svg);
    computation_options_t limits = {.work_limit = 1};
    computation_t *control = computation_new(&limits);
    ok = ok && control && !computation_run(control, render_under_control, plot) &&
         computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    computation_free(control);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "owned surface leaves missing cells open and honours rendering cancellation");

    number_t steep_axis[17], steep_heights[17 * 17];
    for (size_t i = 0; i < 17; ++i)
        steep_axis[i] = num_create_from_long((long)i);
    for (size_t i = 0; i < 17 * 17; ++i)
        steep_heights[i] = num_create_from_long(i % 2 ? -1 : 1);
    plot = plot_new(NULL);
    ok = plot && plot_set_surface(plot, steep_axis, 17, steep_axis, 17, steep_heights);
    svg = ok ? plot_to_svg(plot) : NULL;
    faces = 0;
    for (const char *p = svg ? strstr(string_c_str(svg), "<path fill=") : NULL; p; p = strstr(p + 10, "<path fill="))
        ++faces;
    ok = svg && faces == 512 && strstr(string_c_str(svg), "data-uv=") &&
         strstr(string_c_str(svg), "data-vertices=") && strstr(string_c_str(svg), "data-units=") &&
         strstr(string_c_str(svg), "data-tick-fraction=") &&
         strstr(string_c_str(svg), "data-x-min=\"0\" data-x-max=\"16\"") &&
         strstr(string_c_str(svg), "data-y-min=\"0\" data-y-max=\"16\"");
    string_free(svg);
    plot_free(plot);
    for (size_t i = 0; i < 17; ++i)
        num_destroy(&steep_axis[i]);
    for (size_t i = 0; i < 17 * 17; ++i)
        num_destroy(&steep_heights[i]);
    TEST_ASSERT_TRUE(ok, "finite alternating steep facets are never mistaken for missing data");

    number_t coords[] = {NUM_ZERO, NUM_ONE}, missing[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    plot_options_t options = plot_options_default();
    options.max_points = 3;
    plot = plot_new(&options);
    ok = plot && !plot_set_surface(plot, coords, 2, coords, 2, missing) && errno == E2BIG;
    plot_free(plot);
    plot = plot_new(NULL);
    ok = ok && plot && !plot_set_surface(plot, coords, 1, coords, 2, missing) && errno == EINVAL &&
         plot_set_surface(plot, coords, 2, coords, 2, missing) && !plot_to_svg(plot) && errno == ENODATA;
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "invalid, oversized and empty surfaces report explicit errors");
}

static bool height_ticks_between(const string_t *svg, double minimum, double maximum)
{
    const char *p = svg ? string_c_str(svg) : "";
    size_t count = 0;
    while ((p = strstr(p, "<text data-plot-label=\"2\""))) {
        p = strchr(p, '>');
        double value;
        if (!p || sscanf(++p, "%lf", &value) != 1 || value < minimum || value > maximum)
            return false;
        ++count;
    }
    return count >= 2;
}

static void test_plot_surface_height(void)
{
    plot_options_t options = plot_options_default();
    options.smart_y = true;
    plot_t *plot = plot_new(&options);
    number_t axis[5], heights[25];
    for (unsigned i = 0; i < 5; ++i)
        axis[i] = num_create_from_long(i);
    for (unsigned i = 0; i < 25; ++i)
        heights[i] = num_create_from_long(i == 12 ? 10000 : i % 5);
    bool ok = plot && !plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE) && errno == EINVAL &&
              plot_set_surface(plot, axis, 5, axis, 5, heights);
    string_t *automatic = ok ? plot_to_svg(plot) : NULL;
    ok = ok && height_ticks_between(automatic, 0, 5) && plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE);
    string_t *manual = ok ? plot_to_svg(plot) : NULL;
    ok = ok && height_ticks_between(manual, 0, 1) && strstr(string_c_str(manual), "<path fill=") &&
         !plot_set_height_bounds(plot, NUM_ONE, NUM_ZERO) && errno == EINVAL &&
         !plot_set_height_bounds(plot, NUM_NAN, NUM_ONE) && !plot_set_height_bounds(NULL, NUM_ZERO, NUM_ONE);
    string_t *unchanged = plot_to_svg(plot);
    ok = ok && unchanged && !strcmp(string_c_str(manual), string_c_str(unchanged));
    plot_auto_bounds(plot);
    string_t *restored = plot_to_svg(plot);
    ok = ok && restored && !strcmp(string_c_str(automatic), string_c_str(restored));
    string_free(restored);
    string_free(unchanged);
    string_free(manual);
    string_free(automatic);
    plot_free(plot);
    options.smart_y = false;
    plot = plot_new(&options);
    ok = ok && plot_set_surface(plot, axis, 5, axis, 5, heights);
    string_t *all = plot_to_svg(plot);
    ok = ok && all && strstr(string_c_str(all), ">10000</text>");
    string_free(all);
    plot_free(plot);
    for (unsigned i = 0; i < 25; ++i)
        num_destroy(&heights[i]);
    for (unsigned i = 0; i < 5; ++i)
        num_destroy(&axis[i]);
    TEST_ASSERT_TRUE(ok, "surface automatic cropping, fit-all, manual height bounds and atomic reset");

    number_t level[25];
    for (unsigned i = 0; i < 5; ++i)
        axis[i] = num_create_from_long(i);
    for (unsigned i = 0; i < 25; ++i)
        level[i] = i == 12 ? num_create_from_long(10000) : NUM_ZERO;
    options.smart_y = true;
    plot = plot_new(&options);
    ok = plot && plot_set_surface(plot, axis, 5, axis, 5, level);
    automatic = ok ? plot_to_svg(plot) : NULL;
    ok = ok && height_ticks_between(automatic, 0, 0.25);
    string_free(automatic);
    plot_free(plot);
    for (unsigned i = 0; i < 5; ++i)
        num_destroy(&axis[i]);
    for (unsigned i = 0; i < 25; ++i)
        num_destroy(&level[i]);
    TEST_ASSERT_TRUE(ok, "an isolated spike cannot dominate an otherwise constant surface");

    number_t huge = num_create_from_string("1e1000"), negative = num_neg(huge);
    number_t coordinates[] = {NUM_ZERO, NUM_ONE}, crossing[] = {negative, huge, negative, huge};
    plot = plot_new(NULL);
    ok = plot && plot_set_surface(plot, coordinates, 2, coordinates, 2, crossing) &&
         plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE);
    manual = ok ? plot_to_svg(plot) : NULL;
    ok = ok && height_ticks_between(manual, 0, 1) && strstr(string_c_str(manual), "<path fill=") &&
         !strstr(string_c_str(manual), "nan") && !strstr(string_c_str(manual), "inf");
    string_free(manual);
    plot_free(plot);
    num_destroy(&negative);
    num_destroy(&huge);
    TEST_ASSERT_TRUE(ok, "huge finite heights are intersected before conversion to drawing doubles");

    number_t flat[] = {NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO};
    plot = plot_new(NULL);
    ok = plot && plot_set_surface(plot, coordinates, 2, coordinates, 2, flat) &&
         plot_set_height_bounds(plot, NUM_HALF, NUM_ONE);
    manual = ok ? plot_to_svg(plot) : NULL;
    ok = ok && height_ticks_between(manual, 0.5, 1) && !strstr(string_c_str(manual), "<path fill=");
    string_free(manual);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "manual viewport excluding the surface shows axes without invented plateaus");

    number_t wide_axis[] = {NUM_ZERO, num_create_from_long(4)};
    number_t short_axis[] = {NUM_ZERO, num_create_from_long(2)};
    plot = plot_new(NULL);
    ok = plot && plot_set_surface(plot, wide_axis, 2, short_axis, 2, flat) &&
         plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE);
    manual = ok ? plot_to_svg(plot) : NULL;
    ok = manual && strstr(string_c_str(manual), "data-units=\"1 0.5 0.25\"");
    string_free(manual);
    plot_free(plot);
    num_destroy(&wide_axis[1]);
    num_destroy(&short_axis[1]);
    TEST_ASSERT_TRUE(ok, "height grids preserve equal coordinate units across unequal axis ranges");
}

static bool separated_tick_labels(const string_t *svg, double font_size, size_t *visible)
{
    double boxes[39][4];
    size_t count = 0;
    const char *p = svg ? string_c_str(svg) : "";
    while ((p = strstr(p, "<text data-plot-label="))) {
        unsigned axis;
        double x, y;
        char anchor[16];
        if (count == 39 || sscanf(p, "<text data-plot-label=\"%u\" x=\"%lf\" y=\"%lf\" text-anchor=\"%15[^\"]",
                                  &axis, &x, &y, anchor) != 4)
            return false;
        const char *begin = strchr(p, '>'), *end = begin ? strchr(begin, '<') : NULL;
        if (!end)
            return false;
        size_t characters = 0;
        for (const unsigned char *s = (const unsigned char *)begin + 1; s < (const unsigned char *)end; ++s)
            characters += (*s & 0xc0) != 0x80;
        double width = characters * font_size * 0.8;
        double left = x - (!strcmp(anchor, "end") ? width : !strcmp(anchor, "middle") ? width / 2 : 0);
        boxes[count][0] = left;
        boxes[count][1] = y - font_size;
        boxes[count][2] = left + width;
        boxes[count][3] = y + font_size * 0.3;
        for (size_t i = 0; i < count; ++i)
            if (!(boxes[count][2] + 3.99 <= boxes[i][0] || boxes[i][2] + 3.99 <= boxes[count][0] ||
                  boxes[count][3] + 3.99 <= boxes[i][1] || boxes[i][3] + 3.99 <= boxes[count][1]))
                return false;
        ++count;
        p = end;
    }
    *visible = count;
    return count > 0;
}

static void test_plot_label_spacing(void)
{
    bool ok = true;
    number_t coordinates[] = {num_create_from_long(-10), num_create_from_long(10)};
    number_t heights[] = {NUM_NEG_ONE, NUM_ONE, NUM_ONE, NUM_NEG_ONE};
    const unsigned sizes[][2] = {{800, 500}, {320, 240}};
    const double elevations[] = {5, 30, 85};
    for (size_t s = 0; s < 2; ++s) {
        plot_options_t options = plot_options_default();
        options.width = sizes[s][0];
        options.height = sizes[s][1];
        plot_t *plot = plot_new(&options);
        ok = ok && plot && plot_set_surface(plot, coordinates, 2, coordinates, 2, heights);
        for (unsigned a = 0; a < 360; a += 15) {
            for (size_t e = 0; e < 3; ++e) {
                for (unsigned roll = 0; roll < 360; roll += 45) {
                    bool set = plot_set_view(plot, a, elevations[e]) && plot_set_roll(plot, roll);
                    string_t *svg = set ? plot_to_svg(plot) : NULL;
                    size_t visible = 0;
                    bool fits = separated_tick_labels(svg, 11, &visible) && visible >= 6 &&
                         strstr(string_c_str(svg), "data-plot-label=\"3\"") &&
                         strstr(string_c_str(svg), "data-plot-label=\"4\"") &&
                         strstr(string_c_str(svg), "data-plot-label=\"5\"");
                    if (!fits)
                        printf("Label layout failed: size %u, azimuth %u, elevation %.0f, roll %u, visible %zu\n",
                               options.width, a, elevations[e], roll, visible);
                    ok = ok && fits;
                    string_free(svg);
                }
            }
        }
        plot_free(plot);
        plot = plot_new(&options);
        number_t large = num_create_from_double(1.23456789e100);
        plot_point_t points[] = {{coordinates[0], NUM_ZERO, true}, {coordinates[1], large, false}};
        bool added = plot && plot_add_points(plot, points, 2, "", PLOT_LINES, 0);
        string_t *svg = added ? plot_to_svg(plot) : NULL;
        size_t visible = 0;
        ok = ok && separated_tick_labels(svg, 12, &visible) && visible >= 3 &&
             strstr(string_c_str(svg), "×10");
        string_free(svg);
        plot_free(plot);
        num_destroy(&large);
    }
    num_destroy(&coordinates[0]);
    num_destroy(&coordinates[1]);
    TEST_ASSERT_TRUE(ok, "tick labels have clearance at shared corners, narrow sizes and rotated views");
}

static void test_plot_surface_view(void)
{
    plot_t *plot = plot_new(NULL);
    number_t axis[] = {NUM_ZERO, NUM_ONE}, heights[] = {NUM_ZERO, NUM_ONE, NUM_ONE, NUM_ZERO};
    bool ok = plot && plot_set_surface(plot, axis, 2, axis, 2, heights);
    string_t *original = ok ? plot_to_svg(plot) : NULL;
    const double views[][2] = {{0, 5}, {90, 30}, {180, 85}, {270, 45}, {135, 40}, {225, 30}, {315, 30}};
    for (size_t i = 0; i < sizeof(views) / sizeof(*views); ++i) {
        bool set = plot_set_view(plot, views[i][0], views[i][1]) && plot_set_roll(plot, i * 45);
        string_t *svg = set ? plot_to_svg(plot) : NULL;
        xml_t *document = svg ? xml_from_text(svg) : NULL;
        ok = ok && document && original && strcmp(string_c_str(svg), string_c_str(original)) &&
             strstr(string_c_str(svg), "data-plot-tick=\"0\"") &&
             strstr(string_c_str(svg), "data-plot-tick=\"1\"") &&
             strstr(string_c_str(svg), "data-plot-tick=\"2\"") &&
             strstr(string_c_str(svg), "data-axis-arrow=\"0\"") &&
             strstr(string_c_str(svg), "data-axis-arrow=\"1\"") &&
             strstr(string_c_str(svg), "data-axis-arrow=\"2\"") &&
             strstr(string_c_str(svg), "data-axis-handle=\"0\"") &&
             strstr(string_c_str(svg), "data-axis-handle=\"1\"") &&
             strstr(string_c_str(svg), "data-axis-handle=\"2\"") &&
             strstr(string_c_str(svg), "vector-effect=\"non-scaling-stroke\" pointer-events=\"stroke\"") &&
             strstr(string_c_str(svg), ">x</text>") && strstr(string_c_str(svg), ">y</text>") &&
             strstr(string_c_str(svg), ">f</text>") &&
             !strstr(string_c_str(svg), "nan") && !strstr(string_c_str(svg), "inf");
        /* Every tick is a six-pixel stroke, including cardinal and almost edge-on views. */
        const char *p = svg ? string_c_str(svg) : "";
        size_t ticks = 0;
        while ((p = strstr(p, "data-plot-tick="))) {
            p = strstr(p, "<path d=\"M ");
            double x, y, end_x, end_y;
            if (!p || sscanf(p, "<path d=\"M %lf %lf L %lf %lf", &x, &y, &end_x, &end_y) != 4) {
                ok = false;
                break;
            }
            double length = hypot(end_x - x, end_y - y);
            ok = ok && length > 5.99 && length < 6.01 && x > 0 && x < 800 && y > 0 && y < 500;
            ++ticks;
            ++p;
        }
        ok = ok && ticks >= 6;
        xml_free(document);
        string_free(svg);
    }
    ok = ok && plot_set_view(plot, -45, 30) && !plot_set_view(plot, NAN, 30) && errno == EINVAL &&
         !plot_set_view(plot, 45, INFINITY) && !plot_set_view(plot, 0, 4) && !plot_set_view(plot, 0, 86) &&
         !plot_set_view(NULL, 0, 30) && plot_set_roll(plot, -360) && !plot_set_roll(plot, NAN) &&
         errno == EINVAL && !plot_set_roll(plot, INFINITY) && !plot_set_roll(NULL, 0);
    string_t *wrapped = plot_to_svg(plot);
    ok = ok && original && wrapped && !strcmp(string_c_str(original), string_c_str(wrapped));
    /* A quarter-turn rolls the projected axis clockwise, not around another world axis. */
    const char *axes = original ? strstr(string_c_str(original), "data-plot-axes=") : NULL;
    const char *line = axes ? strstr(axes, "<path d=\"M ") : NULL;
    double x0, y0, x1, y1;
    bool measured = line && sscanf(line, "<path d=\"M %lf %lf L %lf %lf", &x0, &y0, &x1, &y1) == 4;
    for (unsigned roll = 90; measured && roll < 360; roll += 90) {
        bool set = plot_set_roll(plot, roll);
        string_t *svg = set ? plot_to_svg(plot) : NULL;
        axes = svg ? strstr(string_c_str(svg), "data-plot-axes=") : NULL;
        line = axes ? strstr(axes, "<path d=\"M ") : NULL;
        double a0, b0, a1, b1;
        bool found = line && sscanf(line, "<path d=\"M %lf %lf L %lf %lf", &a0, &b0, &a1, &b1) == 4;
        if (found) {
            double dx = x1 - x0, dy = y1 - y0, rx = a1 - a0, ry = b1 - b0;
            double lengths = hypot(dx, dy) * hypot(rx, ry), radians = roll * acos(-1) / 180;
            found = fabs((dx * rx + dy * ry) / lengths - cos(radians)) < 0.0001 &&
                    fabs((dx * ry - dy * rx) / lengths - sin(radians)) < 0.0001;
        }
        ok = ok && found;
        string_free(svg);
    }
    ok = ok && measured;
    string_free(wrapped);
    string_free(original);
    plot_free(plot);
    plot_options_t options = plot_options_default();
    options.x_label = "u<&";
    options.y_label = "μ";
    options.z_label = "value";
    plot = plot_new(&options);
    bool set = plot && plot_set_surface(plot, axis, 2, axis, 2, heights);
    string_t *named = set ? plot_to_svg(plot) : NULL;
    ok = ok && named && strstr(string_c_str(named), ">u&lt;&amp;</text>") &&
         strstr(string_c_str(named), ">μ</text>") && strstr(string_c_str(named), ">value</text>");
    string_free(named);
    plot_free(plot);
    TEST_ASSERT_TRUE(ok, "surface view rotates in all quadrants with tick strokes; invalid changes are atomic");
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

/* README example: the copied three-by-three height field is a small paraboloid. */
static void example_plot_surface(void)
{
    plot_t *plot = plot_new(NULL);
    number_t axis[] = {NUM_NEG_ONE, NUM_ZERO, NUM_ONE};
    number_t heights[9];
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 3; ++c)
            heights[r * 3 + c] = num_create_from_long((long)((r != 1) + (c != 1)));
    bool added = plot_set_surface(plot, axis, 3, axis, 3, heights) &&
                 plot_set_height_bounds(plot, NUM_ZERO, NUM_ONE) &&
                 plot_set_view(plot, 120, 40) && plot_set_roll(plot, 20);
    for (size_t i = 0; i < 9; ++i)
        num_destroy(&heights[i]);
    string_t *svg = added ? plot_to_svg(plot) : NULL;
    if (svg)
        printf("Surface SVG ready; series: %zu; height: 0 to 1\n", plot_series_count(plot));
    TEST_ASSERT_TRUE(svg != NULL, "README native height-surface SVG");
    string_free(svg);
    plot_free(plot);
}

int tests_main(void)
{
    TEST_RUN_CASE(test_plot_svg, NULL);
    TEST_RUN_CASE(test_plot_edge_axes, NULL);
    TEST_RUN_CASE(test_plot_escaping, NULL);
    TEST_RUN_CASE(test_plot_gaps, NULL);
    TEST_RUN_CASE(test_plot_limits, NULL);
    TEST_RUN_CASE(test_plot_precision_and_clip, NULL);
    TEST_RUN_CASE(test_plot_control, NULL);
    TEST_RUN_CASE(test_plot_sampler_ownership, NULL);
    TEST_RUN_CASE(test_plot_label_notation, NULL);
    TEST_RUN_CASE(test_plot_decimal_ticks, NULL);
    TEST_RUN_CASE(test_plot_extreme_ticks, NULL);
    TEST_RUN_CASE(test_plot_extreme_clipping, NULL);
    TEST_RUN_CASE(test_plot_smart_range, NULL);
    TEST_RUN_CASE(test_plot_stationary_range, NULL);
    TEST_RUN_CASE(test_plot_surface, NULL);
    TEST_RUN_CASE(test_plot_surface_height, NULL);
    TEST_RUN_CASE(test_plot_surface_view, NULL);
    TEST_RUN_CASE(test_plot_label_spacing, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_plot_svg);
    TEST_RUN_OUTPUT(example_plot_surface);
    return TEST_EXIT_CODE();
}
