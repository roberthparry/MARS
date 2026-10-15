/**
 * @file test_lab_evaluation_extra.c
 * @brief Additional native Lab protocol, endpoint and binding regressions.
 *
 * Runs prebuilt mathematical workers through the native adapter, with separate
 * harness registrations for every migrated endpoint case. Each registration
 * honours tests/test_config.json and runs sequentially in the lab_native suite.
 * TeX success coverage requires latex/dvisvgm; rejection cases need no renderer.
 * No standalone main, conditional test build or Python runtime is used.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lab_presentation.h"
#include "lab_wire.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "worker/lab_worker_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"
#include "test_lab_math_support.h"

static json_t *request(const char *route, const char *payload, unsigned *status)
{
    string_t *path = string_new_with(route);
    string_t *text = string_new_with(payload);
    json_t *input = json_from_text(text);
    json_t *result = lab_eval_request(path, input, status);
    json_free(input);
    string_free(text);
    string_free(path);
    return result;
}

static bool contains(const json_t *fields, const char *key, const char *expected)
{
    const string_t *value = json_string_value(lab_eval_get(fields, key));
    return value && string_find(value, expected) >= 0;
}

static void show_failure(const json_t *result)
{
    string_t *output = result ? json_to_string(result) : string_new_with("null");
    string_printf("%s\n", output ? string_c_str(output) : "allocation failure");
    string_free(output);
}

static void test_lab_extra_native_records(void)
{
    string_t *raw = string_new_with("input       {x | x=?}\nfunction    number f(x) {\n\nreturn x.\n}\n"
                                    "binding     variable\tx\tNAN\nbound_var   x\nbound_lower \nbound_upper 1\n"
                                    "bound_var   y\nbound_lower 0\nbound_upper 2\nd values    1\n");
    json_t *fields = lab_eval_fields(raw);
    bool ok = !strcmp(lab_eval_text(fields, "function"), "number f(x) {\n\nreturn x.\n}") &&
              !strcmp(lab_eval_text(fields, "bound_lower"), "\n0") &&
              !strcmp(lab_eval_text(fields, "derivative_values"), "1");
    json_free(fields);
    string_free(raw);
    TEST_ASSERT_TRUE(ok, "native multiline records preserve empty lines and bound slots");
    raw = string_new_with("{ [label = NAN,] + x | x = NAN; c = NAN }\nNAN\n");
    string_t *display = lab_eval_display_bindings(raw);
    ok = display && !strcmp(string_c_str(display), "{ [label = NAN,] + x | x = ?; c = ? }\nNAN\n");
    string_free(display);
    string_free(raw);
    TEST_ASSERT_TRUE(ok, "only unset envelope values change notation; quoted names and literal NAN remain intact");
}

static void test_lab_extra_editor_and_cards(void)
{
    const char *source = "{ x+@mu+@sigma+exp(-inf) | x=0; @mu=100, @sigma=15 }";
    json_t *payload = json_new_object();
    lab_eval_set(payload, "expression", source);
    string_t *route = string_new_with("/eval");
    unsigned status = 0u;
    json_t *fields = lab_eval_request(route, payload, &status);
    bool ok = status == 200u && !strcmp(lab_eval_text(fields, "editor_expression"), source) &&
              !strcmp(lab_eval_text(fields, "display_expression"), lab_eval_text(fields, "expression")) &&
              !strcmp(lab_eval_text(fields, "display_function"), lab_eval_text(fields, "function")) &&
              contains(fields, "display_expression", "x") && contains(fields, "display_TeX", "x") &&
              contains(fields, "display_function", "x") &&
              json_array_size(lab_eval_get(fields, "binding_values")) == 3u;
    if (!ok)
        show_failure(fields);
    json_free(fields);
    json_free(payload);
    string_free(route);
    TEST_ASSERT_TRUE(ok, "editor source preserves Greek aliases and -inf while native cards retain variables");
}

static void expect_response(const char *route, const char *payload, unsigned expected_status, const char *field,
                            const char *expected)
{
    unsigned status = 0u;
    json_t *response = request(route, payload, &status);
    bool ok = response && status == expected_status && contains(response, field, expected) &&
              test_lab_ok(response, expected_status == 200u);
    if (!ok) {
        string_printf("route %s: expected HTTP %u, received %u\n", route, expected_status, status);
        show_failure(response);
    }
    json_free(response);
    TEST_ASSERT_TRUE(ok, "native endpoint returns the expected status and field");
}

static void test_lab_extra_expression(void)
{
    expect_response("/eval", "{\"expression\":\"{ x^2 | x=3 }\",\"precision\":40}", 200u, "value", "9");
    expect_response("/eval", "{\"expression\":\"{ x^(1/3) | x=8 }\",\"precision\":40}", 200u, "value", "[2,");
}

static void test_lab_extra_plot(void)
{
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"{sin(x)+a | x=7; a=2}\",\"action\":\"plot\","
                            "\"wrt\":\"x\",\"plot_lower\":\"-@pi\",\"plot_upper\":\"@pi\",\"precision\":40}", &status);
    bool adapted = fields && lab_presentation_adapt(fields);
    const json_t *cards = lab_eval_get(lab_eval_get(fields, "presentation"), "calculus");
    const json_t *plot = lab_eval_get(cards, "plot");
    bool ok = adapted && status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "<svg") &&
              contains(plot, "svg", "data-series") && contains(plot, "render_title", "Plot") &&
              contains(fields, "plot_svg", "data-plot-title") && contains(fields, "plot_title_TeX", "\\sin") &&
              !contains(fields, "plot_svg", "Function plot") && !contains(fields, "plot_title_TeX", "=7") &&
              json_array_size(lab_eval_get(fields, "binding_values")) == 2;
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "native curve and presentation metadata preserve worksheet bindings");
    fields = request("/eval", "{\"expression\":\"x^2\",\"action\":\"plot\",\"plot_lower\":\"2\",\"plot_upper\":\"1\"}", &status);
    ok = !test_lab_ok(fields, true) && contains(fields, "error", "bounds");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "reversed plot ranges produce a useful error");
    fields = request("/eval", "{\"expression\":\"x+a\",\"action\":\"plot\",\"plot_kind\":\"curve\"}", &status);
    ok = !test_lab_ok(fields, true) && contains(fields, "error", "Automatic range selection failed");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "unset parameters never become invented plotted values");
    fields = request("/eval", "{\"expression\":\"zeta(x)\",\"action\":\"plot\",\"precision\":40}", &status);
    ok = test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series") &&
         !contains(fields, "plot_svg", "Vertical range clipped") && !contains(fields, "plot_svg", "<rect") &&
         contains(fields, "plot_svg", "#78d8cb") && contains(fields, "plot_svg", "#94b8b1") &&
         contains(fields, "plot_svg", "#b8ccc5") && !contains(fields, "plot_svg", ">x</text>") &&
         !contains(fields, "plot_svg", ">f</text>");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "automatic zeta view has themed colours without a frame, clipping label or axis titles");
    fields = request("/eval", "{\"expression\":\"zeta(x)\",\"action\":\"plot\",\"plot_scale\":\"all\",\"precision\":40}", &status);
    ok = test_lab_ok(fields, true) && contains(fields, "plot_svg", "<svg") &&
         !contains(fields, "plot_svg", "Vertical range clipped");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "Fit all preserves the complete sampled vertical range");
    fields = request("/eval", "{\"expression\":\"sin(x)\",\"action\":\"plot\",\"plot_scale\":\"manual\","
                             "\"plot_y_lower\":\"-1/2\",\"plot_y_upper\":\"1/2\",\"precision\":40}", &status);
    ok = test_lab_ok(fields, true) && !contains(fields, "plot_svg", "Vertical range clipped") &&
         contains(fields, "plot_svg", ">0.2</text>");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "manual mathematical bounds control the native viewport");
    fields = request("/eval", "{\"expression\":\"x\",\"action\":\"plot\",\"plot_scale\":\"manual\","
                             "\"plot_y_lower\":\"1\",\"plot_y_upper\":\"-1\"}", &status);
    ok = !test_lab_ok(fields, true) && contains(fields, "error", "manual bounds");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "invalid vertical bounds are rejected before sampling");
}

static void test_lab_extra_surface_continuity(void)
{
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"acos(sin(@pi-x*y))\",\"action\":\"plot\","
                             "\"plot_lower\":\"-20\",\"plot_upper\":\"20\",\"plot_second_lower\":\"-20\","
                             "\"plot_second_upper\":\"20\",\"plot_scale\":\"all\"}", &status);
    size_t facets = 0;
    const char *svg = lab_eval_text(fields, "plot_svg");
    for (const char *p = strstr(svg, "<path fill="); p; p = strstr(p + 11, "<path fill="))
        ++facets;
    bool ok = status == 200 && test_lab_ok(fields, true) && facets == 2 * 138 * 138;
    bool failed = true;
    ok = ok && lab_presentation_adapt(fields);
    array_t *encoded = ok ? lab_wire_encode_response(fields, &failed) : NULL;
    json_t *decoded = encoded ? lab_wire_decode(array_get(encoded, 0), array_size(encoded)) : NULL;
    const json_t *card = lab_eval_get(lab_eval_get(lab_eval_get(decoded, "presentation"), "calculus"), "plot");
    ok = ok && !failed && decoded && !strcmp(svg, lab_eval_text(card, "svg")) &&
         !*lab_eval_text(decoded, "plot_svg") && !*lab_eval_text(decoded, "raw");
    json_free(decoded);
    array_destroy(encoded);
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "rapid bounded surface is refined without deleting finite steep facets");
}

static void test_lab_extra_plot_stepped_ranges(void)
{
    expect_response("/eval", "{\"expression\":\"floor(e^(abs(x*y/2))+round(1/cos(x*y)))\","
                              "\"action\":\"plot\",\"precision\":116}", 200u, "plot_svg", "data-surface");
    expect_response("/eval", "{\"expression\":\"floor(x)+0*y\",\"action\":\"plot\","
                              "\"plot_kind\":\"surface\",\"wrt\":\"x\",\"plot_second\":\"y\","
                              "\"plot_lower\":\"-0.5\",\"plot_upper\":\"0.5\"}",
                    200u, "plot_svg", "data-surface");
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"sqrt(-x^2-1)\",\"action\":\"plot\"}", &status);
    bool ok = status == 422 && contains(fields, "error", "Automatic range selection failed") &&
              contains(fields, "error", "trial windows") && !contains(fields, "error", "bind any remaining");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "failed automatic search reports its limits without claiming parameters are missing");
}

static void test_lab_extra_plot_series_domain(void)
{
    unsigned status = 0;
    const char *source = "{ 1/1^x + 1/2^x + 1/3^x + ... | x = ? }";
    string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\",\"wrt\":\"x\","
                                     "\"plot_lower\":\"-5\",\"plot_upper\":\"5\",\"precision\":155}", source);
    json_t *fields = request("/eval", string_c_str(payload), &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series") &&
              contains(fields, "plot_title_TeX", "\\sum_{n=1}");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    string_free(payload);
    TEST_ASSERT_TRUE(ok, "reciprocal series plots its convergent portion across a partly divergent range");

    size_t saved_precision = num_get_default_prec_bits(), bits = 0;
    number_t lower = num_create_from_long(-5), upper = num_create_from_long(5);
    sampler_t *samples = lab_worker_plot_samples(source, "x", lower, upper, NUM_NAN, NUM_NAN, &bits);
    size_t valid = 0, gaps = 0;
    ok = samples != NULL;
    for (size_t i = 0; i < sampler_count(samples); ++i) {
        const sampler_point_t *point = sampler_point(samples, i);
        if (num_le(point->x, NUM_ONE)) {
            ok = ok && !point->valid;
            ++gaps;
        } else if (point->valid) {
            ++valid;
            ok = ok && num_gt(point->y, NUM_ONE);
        }
    }
    sampler_free(samples);
    num_destroy(&upper);
    num_destroy(&lower);
    num_set_default_prec_bits(saved_precision);
    TEST_ASSERT_TRUE(ok && valid > 10 && gaps > 10, "series samples preserve divergence instead of analytic continuation");

    fields = request("/eval", "{\"expression\":\"{1/1^x + 1/2^x + 1/3^x + ... | x=?}\","
                             "\"action\":\"plot\",\"wrt\":\"x\",\"plot_lower\":\"-5\",\"plot_upper\":\"1\"}", &status);
    ok = status == 422 && !test_lab_ok(fields, true) && contains(fields, "error", "No finite real points") &&
         !contains(fields, "error", "binding") && !contains(fields, "error", "NAN") &&
         contains(fields, "raw_error", "binding") && contains(fields, "error", "convergence") &&
         !strcmp(lab_eval_text(fields, "error"), lab_eval_text(lab_eval_get(fields, "failure_diagnostic"), "message"));
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "wholly divergent range reports a clean diagnostic without internal worker records");
}

static double surface_maximum_tick(const json_t *fields)
{
    const char *p = lab_eval_text(fields, "plot_svg");
    double maximum = -INFINITY;
    while ((p = strstr(p, "<text data-plot-label=\"2\""))) {
        p = strchr(p, '>');
        if (!p)
            return NAN;
        double value = strtod(++p, NULL);
        maximum = fmax(maximum, value);
    }
    return maximum;
}

static void test_lab_extra_surface_height(void)
{
    const char *scales[] = {"auto", "all", "manual"};
    double maxima[3] = {0};
    bool ok = true;
    for (unsigned i = 0; i < 3; ++i) {
        string_t *payload = string_sprintf("{\"expression\":\"(exp(-x^2-y^2)*tan(x-y))^2\",\"action\":\"plot\","
            "\"plot_lower\":\"-1\",\"plot_upper\":\"1\",\"plot_second_lower\":\"-1\",\"plot_second_upper\":\"1\","
            "\"plot_scale\":\"%s\",\"plot_y_lower\":\"0\",\"plot_y_upper\":\"1/2\"}", scales[i]);
        unsigned status = 0;
        json_t *fields = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        bool rendered = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "<path fill=");
        maxima[i] = fields ? surface_maximum_tick(fields) : NAN;
        ok = ok && rendered && isfinite(maxima[i]);
        if (!rendered)
            show_failure(fields);
        json_free(fields);
        string_free(payload);
    }
    TEST_ASSERT_TRUE(ok && maxima[0] < 100 && maxima[1] > 1000 && maxima[2] == 0.5,
                     "tangent poles are cropped automatically; fit-all and exact manual height bounds remain available");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_scale\":\"manual\","
                              "\"plot_y_lower\":\"2\",\"plot_y_upper\":\"1\"}", 422u, "error", "height scale");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_scale\":\"manual\"}",
                    422u, "error", "height scale");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_scale\":\"invalid\"}",
                    422u, "error", "height scale");
    unsigned status = 0;
    json_t *preview = request("/eval", "{\"expression\":\"x^2+y^2\",\"action\":\"plot\",\"plot_scale\":\"manual\","
        "\"plot_y_lower\":\"0\",\"plot_y_upper\":\"1/2\",\"plot_quality\":\"preview\",\"plot_azimuth\":\"120\"}", &status);
    ok = status == 200 && test_lab_ok(preview, true) && surface_maximum_tick(preview) == 0.5 &&
         contains(preview, "plot_svg", "data-azimuth=\"120.000\"");
    json_free(preview);
    TEST_ASSERT_TRUE(ok, "coarse rotation previews preserve manual height limits");
}

static void test_lab_extra_surface(void)
{
    const char *cases[] = {
        "{\"expression\":\"x^2+y^2\",\"action\":\"plot\",\"precision\":155}",
        "{\"expression\":\"{x^2+y^2+a | x=9, y=8; a=2}\",\"action\":\"plot\",\"plot_kind\":\"surface\","
        "\"plot_second\":\"y\"}",
        "{\"expression\":\"sqrt(x+y)\",\"action\":\"plot\",\"plot_lower\":\"-1\",\"plot_upper\":\"1\","
        "\"plot_second_lower\":\"-1\",\"plot_second_upper\":\"1\"}",
        "{\"expression\":\"(exp(x)-1)/x+y\",\"action\":\"plot\",\"plot_lower\":\"1e-100\",\"plot_upper\":\"2e-100\","
        "\"plot_second_lower\":\"0\",\"plot_second_upper\":\"1\"}",
        "{\"expression\":\"x-10^30+y\",\"action\":\"plot\",\"plot_lower\":\"10^30+1/10\",\"plot_upper\":\"10^30+1/5\"}",
        "{\"expression\":\"x^2+@mu^2\",\"action\":\"plot\",\"plot_second\":\"@mu\"}",
        "{\"expression\":\"atan2(y,x)\",\"action\":\"plot\",\"plot_azimuth\":\"-240\",\"plot_elevation\":\"40\","
        "\"plot_roll\":\"-270\"}"
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned status = 0;
        json_t *fields = request("/eval", cases[i], &status);
        bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-surface") &&
                  contains(fields, "plot_svg", "<path fill=") && contains(fields, "plot_svg", "data-plot-title");
        if (i == 1)
            ok = ok && contains(fields, "expression", "x = 9") && contains(fields, "expression", "y = 8");
        if (i == 5)
            ok = ok && contains(fields, "plot_svg", ">μ</text>");
        if (i != 6)
            ok = ok && contains(fields, "plot_svg", "data-azimuth=\"315.000\"") &&
                 contains(fields, "plot_svg", "data-elevation=\"30.000\"") &&
                 contains(fields, "plot_svg", "data-roll=\"0.000\"");
        if (i == 6)
            ok = ok && contains(fields, "plot_svg", "data-azimuth=\"120.000\"") &&
                 contains(fields, "plot_svg", "data-elevation=\"40.000\"") &&
                 contains(fields, "plot_svg", "data-roll=\"90.000\"") &&
                 contains(fields, "plot_svg", "data-axis-arrow=\"2\"") &&
                 contains(fields, "plot_svg", "data-plot-tick=\"2\"");
        if (!ok)
            show_failure(fields);
        json_free(fields);
        TEST_ASSERT_TRUE(ok, "native surfaces preserve bindings, holes and range-aware numerical precision");
    }
    expect_response("/eval", "{\"expression\":\"x+y+z\",\"action\":\"plot\"}", 422u, "error", "second variable");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"wrt\":\"q\"}",
                    422u, "error", "Select a variable");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_second\":\"x\"}",
                    422u, "error", "second variable");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_second_lower\":\"2\","
                              "\"plot_second_upper\":\"1\"}", 422u, "error", "bounds");
    expect_response("/eval", "{\"expression\":\"sqrt(-1-x^2-y^2)\",\"action\":\"plot\"}",
                    422u, "error", "Automatic range selection failed");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_azimuth\":\"nan\"}",
                    422u, "error", "finite azimuth");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_elevation\":\"90\"}",
                    422u, "error", "5 to 85");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_azimuth\":\"45junk\"}",
                    422u, "error", "finite azimuth");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_roll\":\"inf\"}",
                    422u, "error", "roll");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_roll\":\"20junk\"}",
                    422u, "error", "roll");
}

static void test_lab_extra_plot_ranges(void)
{
    static const struct {
        const char *source, *lower, *upper;
        double expected_lower, expected_upper;
    } cases[] = {
        {"asin(x)", "", "", -1, 1},
        {"asin(100*x)", " ", "", -0.01, 0.01},
        {"sqrt(x)", "", "", 0, 10},
        {"log(x)", "", "", 0.625, 10},
        {"log(x-100)", "", "", 125, 1000},
        {"x^2", "2", "", 2, 22},
        {"x^2", "", "-2", -22, -2},
        {"x^2", "2", "3", 2, 3}
    };
    size_t precision = num_get_default_prec_bits();
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        const char *limits[] = {cases[i].lower, cases[i].upper, "", ""};
        number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
        bool ok = lab_worker_plot_ranges(cases[i].source, "x", NULL, limits, bounds);
        ok = ok && num_to_double(bounds[0]) == cases[i].expected_lower &&
             num_to_double(bounds[1]) == cases[i].expected_upper && num_get_default_prec_bits() == precision;
        num_destroy(&bounds[0]);
        num_destroy(&bounds[1]);
        TEST_ASSERT_TRUE(ok, "automatic ranges follow sampled domain and scale while preserving explicit bounds");
    }
    expect_response("/eval", "{\"expression\":\"asin(x)\",\"action\":\"plot\","
                              "\"plot_lower\":\"\",\"plot_upper\":\" \"}", 200u, "plot_svg", "data-series");
    const char *narrow[] = {"pi", "pi+1e-50", "", ""};
    number_t rectangle[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    bool guarded = lab_worker_plot_ranges("x+y", "x", "y", narrow, rectangle) &&
                   num_lt(rectangle[0], rectangle[1]) && num_get_default_prec_bits() == precision;
    for (unsigned i = 0; i < 4; ++i)
        num_destroy(&rectangle[i]);
    TEST_ASSERT_TRUE(guarded, "an automatic second range preserves close explicit first-axis bounds");
    expect_response("/eval", "{\"expression\":\"sqrt(1-x^2-y^2)\",\"action\":\"plot\","
                              "\"plot_lower\":\"\",\"plot_upper\":\"\","
                              "\"plot_second_lower\":\"\",\"plot_second_upper\":\"\"}", 200u, "plot_svg", "data-surface");
    expect_response("/eval", "{\"expression\":\"sqrt(-1-x^2)\",\"action\":\"plot\"}",
                    422u, "error", "Automatic range selection failed");
    expect_response("/eval", "{\"expression\":\"x\",\"action\":\"plot\",\"plot_lower\":\"bad(\"}",
                    422u, "error", "bounds");
}

static size_t surface_facets(const string_t *svg)
{
    size_t count = 0;
    const char *cursor = svg ? string_c_str(svg) : "";
    while ((cursor = strstr(cursor, "<path fill="))) {
        ++count;
        ++cursor;
    }
    return count;
}

static void test_lab_extra_plot_preview(void)
{
    unsigned status = 0;
    json_t *full = request("/eval", "{\"expression\":\"x^2+y^2\",\"action\":\"plot\"}", &status);
    json_t *preview = request("/eval", "{\"expression\":\"x^2+y^2\",\"action\":\"plot\","
                                      "\"plot_quality\":\"preview\"}", &status);
    const string_t *a = json_string_value(lab_eval_get(full, "plot_svg"));
    const string_t *b = json_string_value(lab_eval_get(preview, "plot_svg"));
    bool ok = test_lab_ok(full, true) && test_lab_ok(preview, true) && a && b &&
              surface_facets(a) == 2 * 69 * 69 && surface_facets(b) == 2 * 12 * 12 &&
              strlen(string_c_str(b)) < strlen(string_c_str(a)) / 3 &&
              contains(preview, "plot_svg", "data-plot-title") && contains(preview, "plot_svg", "data-axis-arrow");
    json_free(preview);
    json_free(full);
    TEST_ASSERT_TRUE(ok, "70-by-70 full grids and 13-by-13 drag previews retain native titles and axes");
    expect_response("/eval", "{\"expression\":\"x+y\",\"action\":\"plot\",\"plot_quality\":\"unknown\"}",
                    422u, "error", "plotting quality");
    const char *const modes[] = {"expression", "equation", "diffequation"};
    const char *const sources[] = {"sin(30*x)", "sin(30*x)=0", "Dx(y)=30*cos(30*x); y(0)=0"};
    for (unsigned i = 0; i < 3; ++i) {
        size_t lengths[2] = {0};
        for (unsigned quality = 0; quality < 2; ++quality) {
            string_t *payload = string_sprintf("{\"expression\":\"%s\",\"plot_source_mode\":\"%s\","
                "\"action\":\"plot\",\"precision\":17,\"plot_lower\":\"-2\",\"plot_upper\":\"2\","
                "\"plot_quality\":\"%s\"}", sources[i], modes[i], quality ? "full" : "preview");
            json_t *result = request("/eval", string_c_str(payload), &status);
            const string_t *svg = json_string_value(lab_eval_get(result, "plot_svg"));
            if (status == 200u && test_lab_ok(result, true) && svg)
                lengths[quality] = string_byte_length(svg);
            else
                show_failure(result);
            json_free(result);
            string_free(payload);
        }
        TEST_ASSERT_TRUE(lengths[0] > 0 && lengths[1] > lengths[0],
                         "all curve modes use reduced sampling for previews and restore full detail");
    }
}

static void test_lab_extra_solver_plots(void)
{
    unsigned status = 0;
    expect_response("/eval", "{\"expression\":\"sin(x)\",\"action\":\"plot\",\"plot_source_mode\":\"integrator\","
                             "\"bounds\":[{\"name\":\"x\",\"lo\":\"-pi\",\"hi\":\"pi\"}]}",
                    200u, "plot_svg", "data-integral-shading");
    expect_response("/eval", "{\"expression\":\"x^2\",\"action\":\"plot\",\"plot_source_mode\":\"integrator\","
                             "\"bounds\":[{\"name\":\"x\",\"lo\":\"1\",\"hi\":\"0\"}]}",
                    200u, "plot_svg", "data-sign=\"negative\"");
    expect_response("/eval", "{\"expression\":\"sin(t)\",\"action\":\"plot\",\"plot_source_mode\":\"integrator\","
                             "\"bounds\":[{\"name\":\"t\",\"lo\":\"\",\"hi\":\"\"}]}",
                    200u, "plot_svg", "data-series");
    expect_response("/eval", "{\"expression\":\"a*x\",\"action\":\"plot\",\"plot_source_mode\":\"integrator\","
                             "\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}]}",
                    422u, "error", "parameters");
    json_t *circle = request("/eval", "{\"expression\":\"x^2+y^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                                     "\"plot_source_mode\":\"equation\"}", &status);
    bool circle_ok = status == 200 && test_lab_ok(circle, true) &&
                     contains(circle, "plot_svg", "data-series=\"0\"") &&
                     !contains(circle, "plot_svg", "data-series=\"1\"") &&
                     !contains(circle, "plot_svg", "data-surface") &&
                     contains(circle, "plot_svg", ">y</text>");
    if (!circle_ok)
        show_failure(circle);
    json_free(circle);
    TEST_ASSERT_TRUE(circle_ok, "a two-coordinate equation plots its real contour, not two height functions");
    expect_response("/eval", "{\"expression\":\"x*y=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 200u, "plot_svg", "data-series");
    expect_response("/eval", "{\"expression\":\"(x-10)^2+(y+3)^2=4\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 200u, "plot_svg", "data-series");
    expect_response("/eval", "{\"expression\":\"1/(x*y)=0\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 422u, "error", "No real contour");
    expect_response("/eval", "{\"expression\":\"x^2+y^2=-1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 422u, "error", "No real contour");
    expect_response("/eval", "{\"expression\":\"x+y+z+w=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 422u, "error", "parameters");
    expect_response("/eval", "{\"expression\":\"x^2+y^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_lower\":\"-8.77\",\"plot_upper\":\"4.48\","
                             "\"plot_scale\":\"manual\",\"plot_y_lower\":\"-201.3\",\"plot_y_upper\":\"238.9\"}",
                    200u, "plot_svg", "data-series");
    expect_response("/eval", "{\"expression\":\"x^2+y^2+z^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\"}", 200u, "plot_svg", "data-surface");
    json_t *sphere = request("/eval", "{\"expression\":\"x^2+y^2+z^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                                     "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\"}", &status);
    bool sphere_ok = status == 200u && contains(sphere, "plot_svg", "data-mesh=\"true\"") &&
                     contains(sphere, "plot_svg", "data-sample-z-min=\"-1\"") &&
                     contains(sphere, "plot_svg", "data-sample-z-max=\"1\"");
    if (!sphere_ok)
        show_failure(sphere);
    json_free(sphere);
    TEST_ASSERT_TRUE(sphere_ok, "coarse cached sphere edges retain both hemispheres");
    expect_response("/eval", "{\"expression\":\"(x-0.2)^2+2*(y+0.1)^2+3*z^2=1\",\"wrt\":\"x\","
                             "\"action\":\"plot\",\"plot_source_mode\":\"equation\"}",
                    200u, "plot_svg", "data-mesh=\"true\"");
    expect_response("/eval", "{\"expression\":\"1/(x+y+z-0.123)=0\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\"}",
                    422u, "error", "No real isosurface");
    expect_response("/eval", "{\"expression\":\"x+y+z=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\"}",
                    200u, "plot_svg", "data-surface");
    expect_response("/eval", "{\"expression\":\"1/(x*y*z)=0\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\"}",
                    422u, "error", "No real isosurface");
    expect_response("/eval", "{\"expression\":\"x^2+y^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_lower\":\"2\",\"plot_upper\":\"-2\"}",
                    422u, "error", "bounds");
    expect_response("/eval", "{\"expression\":\"(x-10)^2+y^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_lower\":\"8\"}",
                    200u, "plot_svg", "data-x-min=\"8\"");
    json_t *result = request("/eval", "{\"expression\":\"x^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                                     "\"plot_source_mode\":\"equation\",\"plot_lower\":\"-2\",\"plot_upper\":\"2\"}", &status);
    bool ok = status == 200 && test_lab_ok(result, true) && contains(result, "plot_svg", "data-series=\"1\"") &&
              !strcmp(lab_eval_text(result, "plot_solutions"), "x = -1\nx = 1") &&
              !contains(result, "plot_svg", "Left side") && !contains(result, "plot_svg", "Right side");
    if (!ok)
        show_failure(result);
    json_free(result);
    result = request("/eval", "{\"expression\":\"Dx(y)=x*y; y(0)=1\",\"wrt\":\"x\",\"action\":\"plot\","
                              "\"plot_source_mode\":\"diffequation\",\"plot_lower\":\"-2\",\"plot_upper\":\"2\"}", &status);
    bool de_ok = status == 200 && test_lab_ok(result, true) && contains(result, "plot_svg", "data-series=\"0\"") &&
                 !contains(result, "plot_svg", "Solution 1") && contains(result, "plot_title", "y = ") &&
                 !contains(result, "plot_title_TeX", "\\frac{d");
    if (!de_ok)
        show_failure(result);
    json_free(result);
    TEST_ASSERT_TRUE(ok && de_ok, "native equation sides and initial-value solution share the plot-card protocol");
    expect_response("/eval", "{\"expression\":\"x^2=4\",\"action\":\"plot\",\"plot_source_mode\":\"equation\"}",
                    200u, "plot_solutions", "x = 2");
    expect_response("/eval", "{\"expression\":\"{2*Dx(u)+Dy(u)=0 | x=?, y=?; ; u(x,0)=x^2}\","
                             "\"wrt\":\"x\",\"action\":\"plot\",\"plot_source_mode\":\"diffequation\","
                             "\"plot_lower\":\"-2\",\"plot_upper\":\"2\",\"plot_second_lower\":\"-2\","
                             "\"plot_second_upper\":\"2\"}", 200u, "plot_svg", "data-surface");
    expect_response("/eval", "{\"expression\":\"{2*Dx(u)+Dy(u)=0 | x=?, y=?; ; u(x,0)=0}\","
                             "\"wrt\":\"y\",\"action\":\"plot\",\"plot_source_mode\":\"diffequation\"}",
                    200u, "plot_svg", "data-surface");
    expect_response("/eval", "{\"expression\":\"x^2=x+2\",\"action\":\"plot\",\"plot_source_mode\":\"equation\"}",
                    200u, "plot_svg", "data-series=\"1\"");
    expect_response("/eval", "{\"expression\":\"Dx(y)=0; y(0)=1\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"diffequation\",\"plot_lower\":\"-2\",\"plot_upper\":\"2\"}",
                    200u, "plot_title", "y = 1");
    result = request("/eval", "{\"expression\":\"y''+6yy'+4y^3=0; y(0)=0; y'(0)=1\",\"action\":\"plot\","
                              "\"plot_source_mode\":\"diffequation\",\"plot_lower\":\"-10\",\"plot_upper\":\"10\"}", &status);
    ok = status == 200 && contains(result, "plot_title_TeX", "\\frac{x}{") &&
         contains(result, "plot_title", "y = (x)/(x² + 1)") && !contains(result, "plot_svg", "Solution 1");
    if (!ok)
        show_failure(result);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "ODE plot title identifies the exact solution without a redundant single-series legend");
    expect_response("/eval", "{\"expression\":\"{x=a | x=?; a=?}\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_lower\":\"-2\",\"plot_upper\":\"2\"}",
                    422u, "error", "parameters");
    expect_response("/eval", "{\"expression\":\"x^2=1\",\"action\":\"plot\",\"plot_source_mode\":\"equation\","
                             "\"plot_lower\":\"2\",\"plot_upper\":\"-2\"}", 422u, "error", "bounds");
    expect_response("/eval", "{\"expression\":\"Dx(y)=y\",\"action\":\"plot\",\"plot_source_mode\":\"diffequation\","
                             "\"plot_lower\":\"-2\",\"plot_upper\":\"2\"}", 200u, "plot_svg", "data-series=\"4\"");
    expect_response("/eval", "{\"expression\":\"x^2=1\",\"action\":\"plot\",\"plot_source_mode\":\"equation\","
                             "\"plot_kind\":\"surface\"}", 200u, "plot_svg", "data-series");
    expect_response("/eval", "{\"expression\":\"x\",\"action\":\"plot\",\"plot_source_mode\":\"unknown\"}",
                    422u, "error", "source mode");
}

static void test_lab_extra_equation_frame(void)
{
    static const struct {
        const char *left, *right, *axis;
        bool found;
    } cases[] = {
        {"x^4-x^3-26x^2+4x+8", "0", "x", true},
        {"t^2", "t+2", "t", true},
        {"x^4", "0", "x", true},
        {"sin(x)", "0", "x", true},
        {"exp(x)", "2", "x", true},
        {"1/x", "0", "x", false},
        {"x", "x+1", "x", false}
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        expr_t *sides[] = {expr_from_string(cases[i].left, NULL), expr_from_string(cases[i].right, NULL)};
        char *sources[] = {expr_to_string(sides[0], style_EXPRESSION), expr_to_string(sides[1], style_EXPRESSION)};
        number_t bounds[] = {num_create_from_long(-10), num_create_from_long(10), NUM_NAN, NUM_NAN};
        bool found = lab_worker_plot_equation_frame((const expr_t *const *)sides, sources, cases[i].axis, true, bounds);
        ok = ok && found == cases[i].found;
        if (found)
            ok = ok && num_lt(bounds[0], bounds[1]) && num_lt(bounds[2], bounds[3]);
        if (!i)
            ok = ok && num_to_double(bounds[0]) > -7 && num_to_double(bounds[0]) < -5 &&
                 num_to_double(bounds[1]) > 6 && num_to_double(bounds[1]) < 8 &&
                 num_to_double(bounds[2]) < -200 && num_to_double(bounds[2]) > -300 &&
                 num_to_double(bounds[3]) > 200 && num_to_double(bounds[3]) < 300;
        for (unsigned j = 0; j < 2; ++j) {
            free(sources[j]);
            expr_free(sides[j]);
        }
        for (unsigned j = 0; j < 4; ++j)
            num_destroy(&bounds[j]);
    }
    TEST_ASSERT_TRUE(ok, "equation framing uses generic turning points and crossings, not polynomial-specific rules or poles");
    unsigned status = 0;
    json_t *result = request("/eval", "{\"expression\":\"x^4-x^3-26x^2+4x+8=0\",\"action\":\"plot\","
                                     "\"plot_source_mode\":\"equation\"}", &status);
    ok = status == 200 && contains(result, "plot_svg", ">-200</text>") &&
         contains(result, "plot_svg", ">200</text>") && !contains(result, "plot_svg", ">2000</text>") &&
         !contains(result, "plot_svg", "Left side") && !contains(result, "plot_svg", "Right side");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "automatic quartic plot exposes all turning points at a useful vertical scale");
    expect_response("/eval", "{\"expression\":\"x^4-x^3-26x^2+4x+8=0\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_scale\":\"all\"}",
                    200u, "plot_svg", ">2000</text>");
    expect_response("/eval", "{\"expression\":\"x^4-x^3-26x^2+4x+8=0\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_lower\":\"-10\",\"plot_upper\":\"10\"}",
                    200u, "plot_svg", ">10</text>");
}

static void test_lab_extra_solution_families(void)
{
    char *frame_sources[] = {"x/(x^2+1)", "1/x"};
    number_t frame[4] = {num_create_from_long(-10), num_create_from_long(10), NUM_NAN, NUM_NAN};
    lab_worker_plot_frame(frame_sources, 2, "x", frame);
    bool framed = num_to_double(frame[0]) > -8 && num_to_double(frame[1]) < 8 &&
                  num_to_double(frame[0]) < -1 && num_to_double(frame[1]) > 1;
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&frame[i]);
    TEST_ASSERT_TRUE(framed, "sampler framing crops quiet tails but retains the turning points and pole region");
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{C*x+D | x=?; C=?, D=?}", &bindings);
    const expr_t *branches[] = {source};
    expr_t *curves[16] = {NULL};
    string_t *labels[16] = {NULL};
    size_t count = 0;
    bool family = false;
    bool ok = source && lab_worker_plot_family(NULL, "x", branches, 1, curves, labels, &count, &family) &&
              family && count == 9;
    for (size_t i = 0; i < count; ++i) {
        char *text = expr_to_string(curves[i], style_EXPRESSION);
        expr_bindings_t *values = NULL;
        expr_t *curve = text ? expr_from_string(text, &values) : NULL;
        expr_t *axis = values ? expr_bindings_get(values, "x") : NULL;
        if (axis)
            expr_set_val(axis, NUM_TWO);
        number_t actual = curve ? expr_eval(curve) : num_clone(NUM_NAN);
        double c = 0, d = 0;
        const char *label = string_c_str(labels[i]);
        bool parsed = label && sscanf(label, "C = %lf, D = %lf", &c, &d) == 2;
        ok = ok && parsed && fabs(num_to_double(actual) - (2 * c + d)) < 1e-9 &&
             (i != 0 || fabs(c + 1) > 0.01) && string_find(labels[i], "C = ") >= 0 &&
             string_find(labels[i], "D = ") >= 0;
        num_destroy(&actual);
        expr_free(curve);
        expr_bindings_free(values);
        free(text);
        expr_free(curves[i]);
        string_free(labels[i]);
    }
    if (bindings && expr_bindings_get(bindings, "x"))
        expr_set_val(expr_bindings_get(bindings, "x"), NUM_TWO);
    number_t original = expr_eval(source);
    ok = ok && num_is_nan(original);
    num_destroy(&original);
    expr_free(source);
    expr_bindings_free(bindings);
    TEST_ASSERT_TRUE(ok, "family members carry distinct native constant values without mutating the general formula");

    const char *scaled[] = {"{C*x | x=?; C=?}", "{10*C*x | x=?; C=?}"};
    double first[2] = {0};
    for (size_t scale = 0; scale < 2; ++scale) {
        source = expr_from_string(scaled[scale], &bindings);
        branches[0] = source;
        ok = source && lab_worker_plot_family(NULL, "x", branches, 1, curves, labels, &count, &family) &&
             family && count == 5;
        if (ok)
            ok = sscanf(string_c_str(labels[0]), "C = %lf", &first[scale]) == 1;
        for (size_t i = 0; i < count; ++i) {
            expr_free(curves[i]);
            string_free(labels[i]);
        }
        expr_free(source);
        expr_bindings_free(bindings);
        TEST_ASSERT_TRUE(ok, "one-parameter sensitivity sweep produces five labelled members");
    }
    TEST_ASSERT_TRUE(fabs(first[1]) < fabs(first[0]),
                     "higher parameter sensitivity concentrates representatives nearer the active region");

    unsigned status = 0;
    json_t *result = request("/eval", "{\"expression\":\"y''+6yy'+4y^3=0\",\"action\":\"plot\","
                                     "\"plot_source_mode\":\"diffequation\"}", &status);
    ok = status == 200 && contains(result, "plot_svg", "data-series=\"8\"") &&
         contains(result, "plot_svg", "<title>C₁ = ") &&
         contains(result, "plot_svg", ", C₂ = ") &&
         !contains(result, "plot_svg", "C₂ = 1</text>") &&
         contains(result, "plot_title", "C₁") && contains(result, "plot_title", "representative real members");
    if (!ok)
        show_failure(result);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "unconditioned Emden plots a labelled two-constant family instead of failing");
    result = request("/eval", "{\"expression\":\"y''+6yy'+4y^3=0\",\"action\":\"plot\","
                     "\"plot_source_mode\":\"diffequation\",\"plot_lower\":\"-5\",\"plot_upper\":\"5\","
                     "\"plot_scale\":\"all\"}", &status);
    const char *upper = strstr(test_lab_text(result, "plot_svg"), "data-y-max=\"");
    double height = 0;
    ok = status == 200 && upper && sscanf(upper, "data-y-max=\"%lf", &height) == 1 &&
         height > 0 && height < 100;
    json_free(result);
    TEST_ASSERT_TRUE(ok, "Fit all protects solution families from sampled asymptote tails");
    result = request("/eval", "{\"expression\":\"y''+6yy'+4y^3=0\",\"action\":\"plot\","
                     "\"plot_source_mode\":\"diffequation\",\"plot_lower\":\"-10\",\"plot_upper\":\"10\","
                     "\"plot_scale\":\"manual\",\"plot_y_lower\":\"-2\",\"plot_y_upper\":\"2\"}", &status);
    ok = status == 200 && contains(result, "plot_svg", ">-10</text>") &&
         contains(result, "plot_svg", ">10</text>") && contains(result, "plot_svg", ">-1</text>") &&
         !contains(result, "plot_svg", ">5</text>\n<g data-plot-tick=\"1\"");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "family plotting preserves explicit horizontal and vertical bounds");
    expect_response("/eval", "{\"expression\":\"y''+6yy'+4y^3=0; y(0)=0\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"diffequation\"}", 200u, "plot_svg", "data-series=\"4\"");
    expect_response("/eval", "{\"expression\":\"{Dx(y)=a*y | x=?; a=?;}\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"diffequation\"}", 422u, "error", "parameter a");
    expect_response("/eval", "{\"expression\":\"{Dx(y)=a*y | x=?; a=2;}\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"diffequation\"}", 200u, "plot_svg", "data-series=\"4\"");
    expect_response("/eval", "{\"expression\":\"Dxxx(y)=0\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"diffequation\"}", 422u, "error", "two free integration constants");
}

static void test_lab_extra_plot_turning_points(void)
{
    unsigned status = 0;
    json_t *curve = request("/eval", "{\"expression\":\"100/(1+10000*(x-0.371)^2)\",\"action\":\"plot\","
                                      "\"plot_lower\":\"0\",\"plot_upper\":\"1\"}", &status);
    bool ok = status == 200 && test_lab_ok(curve, true) && contains(curve, "plot_svg", ">80</text>");
    if (!ok)
        show_failure(curve);
    json_free(curve);
    json_t *surface = request("/eval", "{\"expression\":\"100*exp(-100*((x-0.13)^2+(y+0.07)^2))\","
                                        "\"action\":\"plot\",\"plot_lower\":\"-1\",\"plot_upper\":\"1\","
                                        "\"plot_second_lower\":\"-1\",\"plot_second_upper\":\"1\"}", &status);
    bool surface_ok = status == 200 && test_lab_ok(surface, true) && contains(surface, "plot_svg", ">80</text>");
    if (!surface_ok)
        show_failure(surface);
    ok = ok && surface_ok;
    json_free(surface);
    TEST_ASSERT_TRUE(ok, "curve and surface automatic height limits preserve derivative-located narrow peaks");
}

static void test_lab_extra_plot_tangent_bounds(void)
{
    /* The native request field is decimal digits, not the browser's bit count. */
    const unsigned precisions[] = {17, 116, 386};
    bool ok = true;
    for (size_t i = 0; i < sizeof(precisions) / sizeof(*precisions); ++i) {
        unsigned status = 0;
        string_t *payload = string_sprintf("{\"expression\":\"tan(x)\",\"action\":\"plot\",\"plot_scale\":\"manual\","
                            "\"plot_lower\":\"-2pi\",\"plot_upper\":\"2pi\",\"plot_y_lower\":\"-12\","
                            "\"plot_y_upper\":\"12\",\"precision\":%u}", precisions[i]);
        json_t *fields = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        bool rendered = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series") &&
                        !contains(fields, "plot_svg", "Vertical range clipped");
        if (!rendered)
            show_failure(fields);
        ok = ok && rendered;
        json_free(fields);
        string_free(payload);
    }
    TEST_ASSERT_TRUE(ok, "tangent over multiples of pi renders with manual limits at double and high precision");
}

static void test_lab_extra_plot_precision(void)
{
    size_t saved = num_get_default_prec_bits(), bits = 0;
    num_set_default_prec_bits(1536);
    number_t lo = num_create_from_long(-2), hi = num_create_from_long(2);
    sampler_t *samples = lab_worker_plot_samples("{sin(x)+a | x=7; a=1.23456789123456789123456789}", "x",
                                                 lo, hi, NUM_NAN, NUM_NAN, &bits);
    bool ordinary = samples && bits == 53 && num_get_default_prec_bits() == 1536;
    if (samples) {
        const sampler_point_t *point = sampler_point(samples, 1);
        ordinary = ordinary && point->valid && num_get_effective_prec_bits(point->parameter) == 53 &&
                   num_get_effective_prec_bits(point->y) == 53;
    }
    sampler_free(samples);
    num_destroy(&hi);
    num_destroy(&lo);
    num_set_default_prec_bits(53);
    bool parsed = lab_worker_plot_bounds("1000000000000000000000000000000.1",
                                         "1000000000000000000000000000000.2", &lo, &hi);
    samples = parsed ? lab_worker_plot_samples("x", "x", lo, hi, NUM_NAN, NUM_NAN, &bits) : NULL;
    bool narrow = parsed && samples && bits > 100 && num_get_default_prec_bits() == 53;
    if (samples)
        narrow = narrow && num_lt(sampler_point(samples, 0)->x, sampler_point(samples, sampler_count(samples) - 1)->x);
    sampler_free(samples);
    num_destroy(&hi);
    num_destroy(&lo);
    parsed = lab_worker_plot_bounds("1e-100", "2e-100", &lo, &hi);
    samples = parsed ? lab_worker_plot_samples("exp(x)-1", "x", lo, hi, NUM_NAN, NUM_NAN, &bits) : NULL;
    bool cancellation = samples && bits > 53 && num_get_default_prec_bits() == 53;
    if (samples) {
        number_t ratio = num_div(sampler_point(samples, 0)->y, lo);
        number_t tolerance = num_create_from_double(0.999999);
        number_t ceiling = num_create_from_double(1.000001);
        cancellation = cancellation && num_gt(ratio, tolerance) && num_lt(ratio, ceiling);
        num_destroy(&ceiling);
        num_destroy(&tolerance);
        num_destroy(&ratio);
    }
    sampler_free(samples);
    num_destroy(&hi);
    num_destroy(&lo);
    number_t ymin = NUM_NAN, ymax = NUM_NAN;
    parsed = lab_worker_plot_bounds("1e30-2", "1e30+2", &ymin, &ymax) &&
             lab_worker_plot_bounds("-1", "1", &lo, &hi);
    samples = parsed ? lab_worker_plot_samples("1e30+sin(x)", "x", lo, hi, ymin, ymax, &bits) : NULL;
    bool vertical = samples && bits > 100 && num_get_default_prec_bits() == 53;
    sampler_free(samples);
    num_destroy(&ymax);
    num_destroy(&ymin);
    num_destroy(&hi);
    num_destroy(&lo);
    parsed = lab_worker_plot_bounds("0", "2", &lo, &hi);
    samples = parsed ? lab_worker_plot_samples("{sum(n,1,inf,a/n^x) | a=2; x=?}", "x",
                                                lo, hi, NUM_NAN, NUM_NAN, &bits) : NULL;
    bool scoped = samples && !sampler_point(samples, 0)->valid;
    if (samples) {
        const sampler_point_t *last = sampler_point(samples, sampler_count(samples) - 1);
        number_t minimum = num_create_from_double(3.2898681), maximum = num_create_from_double(3.2898682);
        scoped = scoped && last->valid && num_gt(last->y, minimum) && num_lt(last->y, maximum);
        num_destroy(&maximum);
        num_destroy(&minimum);
    }
    sampler_free(samples);
    num_destroy(&hi);
    num_destroy(&lo);
    parsed = lab_worker_plot_bounds("pi", "pi+1e-50", &lo, &hi);
    bool close_symbolic = parsed && num_lt(lo, hi) && num_get_default_prec_bits() == 53;
    sampler_t *invalid = lab_worker_plot_samples("(", "x", lo, hi, NUM_NAN, NUM_NAN, &bits);
    bool restored = !invalid && num_get_default_prec_bits() == 53;
    sampler_free(invalid);
    num_destroy(&hi);
    num_destroy(&lo);
    num_set_default_prec_bits(saved);
    TEST_ASSERT_TRUE(ordinary, "ordinary samples use 53 bits even with a 1536-bit worksheet and bindings");
    TEST_ASSERT_TRUE(narrow, "narrow decimal ranges are parsed accurately and raise sampling precision");
    TEST_ASSERT_TRUE(cancellation, "higher-precision probes detect cancellation and retry the formula");
    TEST_ASSERT_TRUE(vertical, "a narrow manual vertical viewport also raises working precision");
    TEST_ASSERT_TRUE(scoped, "private plot bindings reach summand parameters without removing convergence gaps");
    TEST_ASSERT_TRUE(close_symbolic, "symbolic bounds closer than worksheet resolution remain distinct");
    TEST_ASSERT_TRUE(restored, "failed private parses restore worksheet precision");
}

static void test_lab_extra_surface_names(void)
{
    const struct {
        const char *source, *first, *second, *name, *prefix;
    } cases[] = {
        {"exp(-x^2-y^2)", "x", "y", "u", "u(x,y) = "},
        {"u^2+x^2", "x", "u", "v", "v(x,u) = "},
        {"{x+y+u | x=?, y=?; u=2}", "x", "y", "v", "v(x,y) = "},
        {"{x+y+u+v | x=?, y=?; u=2, v=3}", "x", "y", "w", "w(x,y) = "},
        {"{x+y+u+v+w+u1 | x=?, y=?; u=2, v=3, w=4, u1=5}", "x", "y", "u₂", "u_{2}(x,y) = "},
        {"x^2+@mu^2", "x", "@mu", "u", "u(x,\\mu) = "},
        {"x^2+y^2", "y", "x", "u", "u(y,x) = "},
        {"{x^2+y^2 | x=9, y=8}", "x", "y", "u", "u(x,y) = "}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        json_t *payload = json_new_object();
        lab_eval_set(payload, "expression", cases[i].source);
        lab_eval_set(payload, "action", "plot");
        lab_eval_set(payload, "wrt", cases[i].first);
        lab_eval_set(payload, "plot_second", cases[i].second);
        lab_eval_set(payload, "plot_lower", "-2");
        lab_eval_set(payload, "plot_upper", "2");
        lab_eval_set(payload, "plot_second_lower", "-2");
        lab_eval_set(payload, "plot_second_upper", "2");
        lab_eval_set(payload, "plot_quality", i & 1 ? "preview" : "full");
        string_t *route = string_new_with("/eval");
        unsigned status = 0;
        json_t *fields = lab_eval_request(route, payload, &status);
        string_t *label = string_sprintf(">%s</text>", cases[i].name);
        bool ok = status == 200 && test_lab_ok(fields, true) &&
                  !strncmp(lab_eval_text(fields, "plot_title_TeX"), cases[i].prefix, strlen(cases[i].prefix)) &&
                  !strncmp(lab_eval_text(fields, "plot_title"), cases[i].name, strlen(cases[i].name)) &&
                  contains(fields, "plot_svg", string_c_str(label)) &&
                  !contains(fields, "plot_svg", ">f</text>") && !*lab_eval_text(fields, "plot_title_render_error");
        if (!ok)
            show_failure(fields);
        string_free(label);
        json_free(fields);
        string_free(route);
        json_free(payload);
        TEST_ASSERT_TRUE(ok, "surface title and height axis share a fresh name and preserve selected axis order");
    }
}

static void test_lab_extra_plot_title_fallback(void)
{
    json_t *fields = json_new_object();
    lab_eval_set(fields, "plot_svg", "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 500\">"
                                    "<g data-series=\"0\"><path d=\"M 0 0 L 1 1\"/></g></svg>");
    lab_eval_set(fields, "plot_title", "<formula>&");
    lab_eval_set(fields, "plot_title_TeX", "\\input{not-permitted}");
    lab_eval_plot_title(fields);
    bool ok = contains(fields, "plot_svg", "data-plot-chart") && contains(fields, "plot_svg", "&lt;formula&gt;&amp;") &&
              !contains(fields, "plot_svg", "<formula>") && *lab_eval_text(fields, "plot_title_render_error");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "failed title rendering preserves the curve, escapes the formula and records a diagnostic");
}

static void test_lab_plot_title_readme(void)
{
    /* README example in docs/mars-lab.md: authored reciprocal series retains its sigma title. */
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"1/1^x + 1/2^x + 1/3^x + ...\",\"action\":\"plot\","
                            "\"wrt\":\"x\",\"plot_lower\":\"-5\",\"plot_upper\":\"5\",\"precision\":17}", &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_title_TeX", "\\sum_{n=1}") &&
              contains(fields, "plot_title_TeX", "\\infty") && contains(fields, "plot_title_TeX", "\\frac{1}{n^{x}}") &&
              !contains(fields, "plot_title_TeX", "\\begin{aligned}") &&
              contains(fields, "plot_svg", "data-plot-title") && contains(fields, "plot_svg", "data-series");
    if (!ok)
        show_failure(fields);
    string_printf("README plot title: %s\n", lab_eval_text(fields, "plot_title_TeX"));
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "README reciprocal-series plot displays the documented sigma title");
}

static void test_lab_surface_readme(void)
{
    /* README example in docs/mars-lab.md: Automatic selects the two unset variables. */
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"x^2 + y^2\",\"action\":\"plot\",\"wrt\":\"x\","
                            "\"plot_lower\":\"-2\",\"plot_upper\":\"2\",\"plot_second_lower\":\"-2\","
                            "\"plot_second_upper\":\"2\",\"plot_kind\":\"auto\","
                            "\"plot_azimuth\":\"120\",\"plot_elevation\":\"40\"}", &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-surface") &&
              contains(fields, "plot_title_TeX", "u(x,y) = ") && contains(fields, "plot_svg", ">u</text>") &&
              contains(fields, "plot_title_TeX", "x^{2}") && contains(fields, "plot_title_TeX", "y^{2}") &&
              contains(fields, "plot_svg", "data-azimuth=\"120.000\"") &&
              contains(fields, "plot_svg", "data-elevation=\"40.000\"");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "README two-variable expression produces a shaded paraboloid with its TeX title");
    string_printf("Surface plot ready: u(x,y) = x^2 + y^2; height axis: u\n");
}

static void test_lab_auto_range_readme(void)
{
    /* README example in docs/mars-lab.md: blank bounds choose the inverse sine's real interval. */
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"asin(x)\",\"action\":\"plot\",\"wrt\":\"x\","
                                     "\"plot_lower\":\"\",\"plot_upper\":\"\"}", &status);
    const char *limits[] = {"", "", "", ""};
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series") &&
              lab_worker_plot_ranges("asin(x)", "x", NULL, limits, bounds) &&
              num_to_double(bounds[0]) == -1 && num_to_double(bounds[1]) == 1;
    num_destroy(&bounds[0]);
    num_destroy(&bounds[1]);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "README blank bounds plot inverse sine over its real interval");
    string_printf("Automatic curve: asin(x), x from -1 to 1\n");
    /* README example in docs/mars-lab.md: a two-coordinate equation is a planar contour. */
    fields = request("/eval", "{\"expression\":\"x^2+y^2=1\",\"action\":\"plot\",\"wrt\":\"x\","
                              "\"plot_source_mode\":\"equation\"}", &status);
    ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series=\"0\"") &&
         contains(fields, "plot_svg", "data-x-min=\"-1.2\"") &&
         contains(fields, "plot_svg", "data-y-max=\"1.2\"");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "README unit circle receives a padded equal-scale coordinate frame");
    string_printf("Implicit curve: x^2+y^2=1; x and y from -1.2 to 1.2\n");
}

/* Run the documented plot inputs after the ordinary Lab regressions. */
void test_lab_plot_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_plot_title_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_surface_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_auto_range_readme, readme, NULL);
}

static void test_lab_extra_help_examples(void)
{
    /* Help-card examples in assets/index.html: execute the public adapter paths, not browser arithmetic. */
    expect_response("/eval", "{\"expression\":\"sin(x)\"}", 200u, "evaluation_ready", "yes");
    expect_response("/eval", "{\"expression\":\"{ x+a | x=2; a=3 }\"}", 200u, "value", "5");
    expect_response("/eval", "{\"expression\":\"sum(k,1,4,k)\"}", 200u, "value", "10");
    expect_response("/eval", "{\"expression\":\"Laplace(exp(-t),t,s)\"}", 200u, "expression", "Re(s)");
    expect_response("/matrix-eval", "{\"matrix\":\"trace(1 2; 3 4)\",\"operation\":\"eval\"}",
                    200u, "value", "5");
    expect_response("/matrix-eval", "{\"matrix\":\"det(1 2; 3 4)\",\"operation\":\"eval\"}",
                    200u, "value", "-2");
    expect_response("/equation-eval", "{\"equation\":\"abs(x-3) = 0\"}", 200u, "solutions", "x = 3");
    expect_response("/equation-eval", "{\"equation\":\"abs(x-3) = 2\"}", 200u, "solutions", "x = 1");
    expect_response("/equation-eval", "{\"equation\":\"{ abs(x-3) = 2 | x=6 }\"}", 200u, "solutions", "x = 5");
    expect_response("/equation-eval", "{\"equation\":\"x^2 = 4\"}", 200u, "solutions", "x = -2");
    expect_response("/equation-eval", "{\"equation\":\"(x+1)^4 = 1\"}", 200u, "display_solutions", "± i");
}

static void test_lab_extra_constant_identity(void)
{
    expect_response("/eval", "{\"expression\":\"{ sin(x)^2+cos(x)^2 | x=? }\"}", 200u, "value", "1");
    expect_response("/eval", "{\"expression\":\"{ sin(x)^2+cos(x)^2-1 | x=? }\"}", 200u, "value", "0");
    unsigned status = 0u;
    json_t *response = request("/eval", "{\"expression\":\"{x+1|x=?}\"}", &status);
    bool ok = status == 200u && test_lab_ok(response, true) && !*test_lab_text(response, "value");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "simplified-value fallback does not invent a value for a remaining unset variable");
}

static void test_lab_extra_bindings(void)
{
    expect_response("/eval", "{\"expression\":\"{x+c | x=?; c=2}\",\"action\":\"bindings\"}", 200u, "bindings",
                    "constant");
}

static void test_lab_extra_binding_edit(void)
{
    expect_response("/eval",
                    "{\"expression\":\"{x^2|x=2}\",\"action\":\"binding-edit\",\"wrt\":\"x\",\"binding_val"
                    "ue\":\"3\"}",
                    200u, "editor_expression", "3");
}

static void test_lab_extra_derivative(void)
{
    expect_response("/eval", "{\"expression\":\"{x^2|x=3}\",\"action\":\"derivative\",\"wrt\":\"x\"}", 200u,
                    "derivative", "x");
}

static void test_lab_extra_integral(void)
{
    expect_response("/eval", "{\"expression\":\"{x|x=?}\",\"action\":\"integral\",\"wrt\":\"x\"}", 200u, "integral",
                    "x");
}

static void test_lab_extra_gaussian_derivative_round_trip(void)
{
    static const unsigned precisions[] = {17u, 78u, 155u};
    for (size_t i = 0u; i < sizeof precisions / sizeof *precisions; ++i) {
        unsigned status = 0u;
        string_t *payload = string_sprintf(
            "{\"expression\":\"{½·(exp(-x²)+2Cx+x·√π·erf(x))+C₁ | x=160; C=?; C₁=?}\","
            "\"action\":\"derivative\",\"wrt\":\"x\",\"precision\":%u}", precisions[i]);
        json_t *result = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        bool ok = status == 200u && test_lab_ok(result, true) &&
                  contains(result, "derivative", "½·√π·erf(x) + C |") &&
                  !contains(result, "derivative", "exp") && !contains(result, "derivative", "C₁") &&
                  !contains(result, "derivative_TeX", "e^{") && contains(result, "derivative_TeX", " + C");
        string_t *route = string_new_with("/function-run"), *key = string_new_with("source");
        json_t *input = json_new_object();
        const json_t *programme = test_lab_member(result, "derivative_function");
        bool ready = route && key && input && programme && json_object_set(input, key, programme);
        json_t *run = ready ? lab_eval_request(route, input, &status) : NULL;
        bool run_ok = status == 200u && test_lab_ok(run, true) &&
                      contains(run, "output", "½·√π·erf(x) + C |") && !contains(run, "output", "exp");
        if (!ok)
            show_failure(result);
        if (!run_ok)
            show_failure(run);
        json_free(run);
        json_free(input);
        string_free(key);
        string_free(route);
        json_free(result);
        string_free(payload);
        TEST_ASSERT_TRUE(ok && run_ok, "differentiating the Gaussian primitive cancels exponential terms in cards and Run");
    }
}

static void test_lab_extra_exact_derivative_round_trip(void)
{
    const unsigned precisions[] = {17u, 116u};
    const char *variables[] = {"x", "y"};
    const double points[][2] = {{0.25, 0.1}, {0.2, 0.2}, {0, 0}, {-0.3, 0.2}, {0.200001, 0.2}};
    for (size_t p = 0; p < 2; ++p) {
        for (size_t v = 0; v < 2; ++v) {
            for (size_t point = 0; point < sizeof(points) / sizeof(points[0]); ++point) {
                double x = points[point][0], y = points[point][1], difference = x - y;
                double pi = acos(-1.0), angle = pi * difference;
                double sinc = difference ? sin(angle) / angle : 1;
                double slope = difference ? (angle * cos(angle) - sin(angle)) / (pi * difference * difference) : 0;
                if (fabs(angle) < 0.01)
                    slope = pi * angle * (-1.0/3 + angle*angle/30 - pow(angle, 4)/840);
                double expected = exp(-x*x-y*y) * ((v ? -slope : slope) - 2 * (v ? y : x) * sinc);
                const char *source = lab_math_format("{exp(-x^2-y^2)*sinc(x-y) | x=%.17g, y=%.17g}", x, y);
                const json_t *derivative = lab_math_fields(source, variables[v], "derivative", precisions[p]);
                lab_math_close(lab_math_number(derivative, "derivative_value"), expected, 2e-12);
                const char *copy = lab_math_after(lab_math_text(derivative, "derivative"), " = ");
                for (unsigned pass = 0; pass < 2; ++pass) {
                    const json_t *result = lab_math_fields(copy, variables[v], "evaluate", precisions[p]);
                    lab_math_contains(lab_math_text(result, "expression"), "π", true);
                    lab_math_contains(lab_math_text(result, "expression"), "Hypergeometric", false);
                    lab_math_contains(lab_math_text(result, "function"), "@pi", true);
                    lab_math_contains(lab_math_text(result, "function"), "hypergeometric", false);
                    lab_math_contains(lab_math_text(result, "tex"), "\\cos", true);
                    lab_math_contains(lab_math_text(result, "tex"), "\\sin", true);
                    lab_math_contains(lab_math_text(result, "tex"), "F_{", false);
                    lab_math_close(lab_math_number(result, "value"), expected, 2e-12);
                    copy = lab_math_text(result, "expression");
                }
                lab_math_reset();
            }
        }
        /* Preserve the exact-coefficient regression independently of the sinc derivative rule. */
        const char *copy = "{exp(-x^2-y^2)*(pi^2*(y-x)*HypergeometricpFq(0,1,5/2,-pi^2*(x-y)^2/4)/3"
                           "-2*x*sinc(x-y)) | x=0.25, y=0.1}";
        for (unsigned pass = 0; pass < 2; ++pass) {
            const json_t *result = lab_math_fields(copy, "x", "evaluate", precisions[p]);
            lab_math_contains(lab_math_text(result, "expression"), "⅓", true);
            lab_math_contains(lab_math_text(result, "expression"), "π²", true);
            lab_math_contains(lab_math_text(result, "function"), "(1/3)", true);
            lab_math_contains(lab_math_text(result, "function"), "@pi^2", true);
            lab_math_contains(lab_math_text(result, "tex"), "\\frac{1}{3}", true);
            lab_math_contains(lab_math_text(result, "tex"), "\\pi^{2}", true);
            copy = lab_math_text(result, "expression");
        }
        lab_math_reset();
    }
}

static void test_lab_extra_repeated_gaussian_integral(void)
{
    static const unsigned precisions[] = {17u, 78u, 155u};
    for (size_t i = 0u; i < sizeof precisions / sizeof *precisions; ++i) {
        unsigned status = 0u;
        string_t *payload = string_sprintf(
            "{\"expression\":\"{sqrt(pi)*erf(x)/2+C | x=?; C=?}\","
            "\"action\":\"integral\",\"wrt\":\"x\",\"precision\":%u}", precisions[i]);
        json_t *result = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        bool ok = status == 200u && test_lab_ok(result, true) &&
                  contains(result, "integral", "½·(exp(-x²) + 2Cx + x·√π·erf(x)) + C₁ |") &&
                  !contains(result, "integral_TeX", "\\frac{1}{\\sqrt{\\pi}}") &&
                  contains(result, "integral_TeX", " + C_{1}");
        string_t *route = string_new_with("/function-run"), *key = string_new_with("source");
        json_t *input = json_new_object();
        const json_t *programme = test_lab_member(result, "integral_function");
        bool ready = route && key && input && programme && json_object_set(input, key, programme);
        json_t *run = ready ? lab_eval_request(route, input, &status) : NULL;
        bool run_ok = status == 200u && test_lab_ok(run, true) &&
                      contains(run, "output", "exp(-x²)") && contains(run, "output", " + C₁ |") &&
                      !contains(run, "output", "/√π");
        if (!ok)
            show_failure(result);
        if (!run_ok)
            show_failure(run);
        json_free(run);
        json_free(input);
        string_free(key);
        string_free(route);
        json_free(result);
        string_free(payload);
        TEST_ASSERT_TRUE(ok && run_ok, "repeated integration cancels reciprocal radicals in cards and Run output");
    }
}

static void test_lab_extra_gaussian_integral(void)
{
    static const unsigned precisions[] = {17u, 78u, 155u};
    for (size_t i = 0u; i < sizeof precisions / sizeof *precisions; ++i) {
        unsigned status = 0u;
        string_t *payload = string_sprintf(
            "{\"expression\":\"{100*(1 - 1/(√(2π)σ)·∫^x_-∞ e^(-½·(t - μ)²/σ²)·dt)"
            " | x=160; μ=100; σ=15}\",\"action\":\"integral\",\"wrt\":\"x\",\"precision\":%u}", precisions[i]);
        json_t *result = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        const string_t *integral = json_string_value(test_lab_member(result, "integral"));
        bool bound_condition = integral && string_find(integral, " | ") >= 0 &&
                               string_find(integral, "Re(") < 0 &&
                               string_find(integral, "where") < 0;
        bool ok = status == 200u && test_lab_ok(result, true) && contains(result, "integral", "erfc") &&
                  contains(result, "integral", " + C |") && contains(result, "integral_TeX", " + C") &&
                  !contains(result, "integral_TeX", "\\mathfrak{Re}") &&
                  !contains(result, "integral_TeX", "\\sqrt{\\sigma^{2}}") &&
                  !contains(result, "integral_TeX", "\\int") && contains(result, "integral_TeX", "erfc") &&
                  !contains(result, "integral_TeX", "\\sqrt{\\frac{1}") &&
                  contains(result, "value", "0.00316712418331") && bound_condition;
        if (!ok)
            show_failure(result);
        string_t *route = string_new_with("/function-run");
        string_t *source_key = string_new_with("source"), *precision_key = string_new_with("precision");
        json_t *run_payload = json_new_object();
        const json_t *programme = test_lab_member(result, "integral_function");
        string_t *precision_text = string_sprintf("%u", precisions[i]);
        json_t *precision = precision_text ? json_new_number(precision_text) : NULL;
        bool ready = route && source_key && precision_key && run_payload && programme && precision &&
                     json_object_set(run_payload, source_key, programme) &&
                     json_object_set(run_payload, precision_key, precision);
        json_t *run = ready ? lab_eval_request(route, run_payload, &status) : NULL;
        bool run_ok = status == 200u && test_lab_ok(run, true) && contains(run, "output", " + C |") &&
                      contains(run, "output", "erfc") && contains(run, "output", "σ") &&
                      !contains(run, "output", "Re(") && !contains(run, "output", "where") &&
                      !contains(run, "output", "√(σ²)") && !contains(run, "output", "√(2·σ²)");
        if (!run_ok)
            show_failure(run);
        json_free(run);
        json_free(precision);
        string_free(precision_text);
        json_free(run_payload);
        string_free(precision_key);
        string_free(source_key);
        string_free(route);
        json_free(result);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "the Unicode Gaussian integral action returns its closed form at each precision");
        TEST_ASSERT_TRUE(run_ok, "the generated Function card retains the formula, omits proven guards and ends in C");
    }
}

static void test_lab_extra_equation(void)
{
    expect_response("/equation-eval", "{\"equation\":\"cos(z)-sin(pi-xy)=0\",\"precision\":78}",
                    200u, "svg", "<svg");
    unsigned trig_status = 0;
    json_t *trig = request("/equation-eval", "{\"equation\":\"cos(z)-sin(pi-xy)=0\",\"precision\":78}",
                           &trig_status);
    const json_t *bindings = test_lab_member(trig, "binding_values");
    bool trig_ok = trig_status == 200u && contains(trig, "solutions_TeX", "\\mathbb{Z}") &&
                   contains(trig, "svg", "<svg") && !*lab_eval_text(trig, "render_error") &&
                   json_array_size(bindings) == 3u;
    for (size_t i = 0; i < json_array_size(bindings); ++i)
        trig_ok = trig_ok && !strcmp(test_lab_text(json_array_get(bindings, i), "value"), "NAN");
    if (!trig_ok)
        show_failure(trig);
    json_free(trig);
    TEST_ASSERT_TRUE(trig_ok, "implicit trigonometric families render without assigning free coordinates");
    expect_response("/equation-eval", "{\"equation\":\"x^2=4\",\"precision\":40}", 200u, "solutions", "2");
    unsigned status = 0;
    json_t *sphere = request("/equation-eval", "{\"equation\":\"x^2+y^2+z^2=1\"}", &status);
    bool adapted = sphere && lab_presentation_adapt(sphere);
    const char *shown = test_lab_text(test_lab_member(sphere, "presentation"), "equation_solution_text");
    bool ok = adapted && status == 200u && strstr(shown, "x = ±√(1 - y² - z²)") &&
              strstr(shown, "y = ±√(1 - x² - z²)") && strstr(shown, "z = ±√(1 - x² - y²)") &&
              !strstr(shown, "nan") && !strstr(shown, "NAN") &&
              json_array_size(test_lab_member(sphere, "numeric_solutions")) == 0u &&
              contains(sphere, "solutions_TeX", "\\pm");
    if (!ok)
        show_failure(sphere);
    json_free(sphere);
    TEST_ASSERT_TRUE(ok, "symbolic opposite branches use plus-minus without unavailable numerical rows");
}

static void test_lab_extra_equation_real_surd_numerics(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval",
                               "{\"equation\":\"x^4-x^3-26x^2+4x+8=0\",\"precision\":78}", &status);
    const json_t *roots = test_lab_member(response, "numeric_solutions");
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 4u &&
              contains(response, "display_solutions", "(1 - 2·B ± √(211 + C - 73/B))/4") &&
              contains(response, "display_solutions", "(1 + 2·B ± √(211 + C + 73/B))/4") &&
              contains(response, "display_TeX", "8\\,\\mathfrak{Re}\\mathopen{}\\left\\{A\\right\\}") &&
              !contains(response, "display_TeX", "\\overline") && contains(response, "svg", "<svg");
    for (size_t i = 0u; i < json_array_size(roots); ++i) {
        const string_t *root = json_string_value(json_array_get(roots, i));
        ok = root && string_find(root, "i") < 0 && string_find(root, "≈") >= 0 &&
             string_find(root, "E") < 0 && string_find(root, "e") < 0 && string_find(root, "×10") < 0 && ok;
    }
    string_t *rows = string_new();
    for (size_t i = 0u; rows && i < json_array_size(roots); ++i) {
        const string_t *root = json_string_value(json_array_get(roots, i));
        if (root)
            string_append_format(rows, "%s%s", i ? "\n" : "", string_c_str(root));
    }
    json_t *plot = request("/eval", "{\"expression\":\"x^4-x^3-26x^2+4x+8=0\",\"precision\":78,"
                                  "\"action\":\"plot\",\"plot_source_mode\":\"equation\"}", &status);
    ok = ok && rows && status == 200u && test_lab_ok(plot, true) &&
         !strcmp(lab_eval_text(plot, "plot_solutions"), string_c_str(rows)) &&
         !strcmp(lab_eval_text(response, "numeric"), string_c_str(rows));
    if (!ok) {
        show_failure(response);
        show_failure(plot);
    }
    json_free(plot);
    string_free(rows);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "78-digit quartic roots retain their digits and align equally in evaluation and plot records");

    response = request("/equation-eval", "{\"equation\":\"x=1+i/10^90\",\"precision\":78}", &status);
    roots = test_lab_member(response, "numeric_solutions");
    ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 1u;
    for (size_t i = 0u; i < json_array_size(roots); ++i) {
        const string_t *root = json_string_value(json_array_get(roots, i));
        ok = root && string_find(root, "×10⁻⁹⁰i") >= 0 && string_find(root, "E") < 0 && ok;
    }
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "genuine imaginary components of 1e-90 are not suppressed at 78 digits");
}

static void test_lab_extra_equation_decimal_notation(void)
{
    const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"x=sqrt(2)/10000", "0.00014142135623730950488"},
        {"x=-sqrt(2)/10000", "-0.00014142135623730950488"},
        {"x=sqrt(2)/100000", "1.4142135623730950488×10⁻⁵"},
        {"x=-sqrt(2)*1e-12", "-1.4142135623730950488×10⁻¹²"},
        {"x=sqrt(2)*1e20", "141421356237309504880"},
        {"x=sqrt(2)*1e5000", "1.4142135623730950488×10⁵⁰⁰⁰"},
        {"x=0.00001+0.0000002i", "1×10⁻⁵ + 2×10⁻⁷i"},
        {"x=-0.0000002i", "-2×10⁻⁷i"},
        {"x=1+i", "1 + i"},
        {"x=0", "0"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *payload = string_sprintf("{\"equation\":\"%s\",\"precision\":20}", cases[i].input);
        unsigned status = 0u;
        json_t *response = payload ? request("/equation-eval", string_c_str(payload), &status) : NULL;
        const json_t *roots = test_lab_member(response, "numeric_solutions");
        const string_t *root = json_string_value(json_array_get(roots, 0u));
        string_offset_t separator = root ? string_find(root, "≈ ") : -1;
        size_t prefix = 4u;
        if (separator < 0) {
            separator = root ? string_find(root, "= ") : -1;
            prefix = 2u;
        }
        string_view_t rhs = root && separator >= 0
            ? string_view_slice(string_view_all(root), (size_t)separator + prefix, string_byte_length(root))
            : string_view_empty();
        bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 1u &&
                  string_view_equals_literal(rhs, cases[i].expected);
        if (!ok)
            string_printf("%s: expected %s; received %S\n", cases[i].input, cases[i].expected, root);
        json_free(response);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "equation numerics use decimals or Unicode powers of ten, never E notation");
    }
    static const unsigned precisions[] = {17u, 78u};
    for (size_t p = 0; p < sizeof(precisions) / sizeof(*precisions); ++p) {
        unsigned status = 0u;
        string_t *payload = string_sprintf("{\"equation\":\"x^4-x^3-26x^2+4x+1/10000=0\",\"precision\":%u}", precisions[p]);
        json_t *response = payload ? request("/equation-eval", string_c_str(payload), &status) : NULL;
        string_free(payload);
        const json_t *roots = test_lab_member(response, "numeric_solutions");
        bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(roots) == 4u &&
                  contains(response, "display_solutions", "A = ³√(") &&
                  contains(response, "display_TeX", "1957044392389164807") &&
                  contains(response, "display_TeX", "\\frac{\\sqrt{-1957044392389164807} - 8913952525}{500000}") &&
                  !contains(response, "display_TeX", "62500000000") &&
                  !contains(response, "display_TeX", "31312710.278226636912") && contains(response, "svg", "<svg");
        string_printf("  Compact quartic: %S\n", json_string_value(test_lab_member(response, "display_TeX")));
        bool small_root = false;
        for (size_t i = 0u; i < json_array_size(roots); ++i) {
            const string_t *root = json_string_value(json_array_get(roots, i));
            ok = root && string_find(root, " ×10") < 0 && ok;
            small_root = small_root || (root && string_find(root, "×10⁻⁵") >= 0);
        }
        json_free(response);
        TEST_ASSERT_TRUE(ok && small_root, "quartic surds remain reduced at double and higher precision");
    }
}

static void test_lab_extra_number_card_markup(void)
{
    static const struct { const char *route; const char *payload; const char *key; } cases[] = {
        {"/eval", "{\"expression\":\"exp(-20)\"}", "value"},
        {"/matrix-eval", "{\"matrix\":\"(exp(-20),0;0,0)\",\"operation\":\"trace\"}", "value"},
        {"/integrator-eval", "{\"expression\":\"exp(-20)\","
                             "\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}]}", "value"},
        {"/function-run", "{\"source\":\"output(exp(-20)).\"}", "output"},
    };
    const unsigned precisions[] = {17u, 78u};
    for (size_t p = 0; p < 2u; ++p) {
        bool ok = true;
        for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
            json_t *payload = test_lab_json(cases[i].payload);
            string_t *digits = string_sprintf("%u", precisions[p]);
            lab_eval_put(payload, "precision", json_new_number(digits));
            string_t *path = string_new_with(cases[i].route);
            unsigned status = 0u;
            json_t *result = lab_eval_request(path, payload, &status);
            bool adapted = result && lab_presentation_adapt(result);
            const json_t *metadata = test_lab_member(result, "presentation");
            const char *raw = test_lab_text(result, cases[i].key);
            bool displayed = false;
            if (!strcmp(cases[i].key, "output")) {
                displayed = strstr(test_lab_text(metadata, "output_display"), "×10⁻⁹") != NULL;
            } else {
                const json_t *entries = test_lab_member(metadata, "matrix_headings");
                /* Inspect the bounded native wire list used by the browser's source-keyed cache. */
                for (size_t j = 0; j < json_array_size(entries); ++j) {
                    const json_t *entry = json_array_get(entries, j);
                    if (!strcmp(test_lab_text(entry, "source"), raw))
                        displayed = strstr(test_lab_text(entry, "html"), "×10⁻⁹") != NULL;
                }
            }
            bool matched = adapted && status == 200u && test_lab_ok(result, true) && displayed;
            if (!matched) show_failure(result);
            ok = ok && matched;
            json_free(result);
            json_free(payload);
            string_free(path);
            string_free(digits);
        }
        TEST_ASSERT_TRUE(ok, "native workers retain executable values and supply power-of-ten card displays at both precisions");
    }
}

static void test_lab_extra_diffequation(void)
{
    unsigned status = 0;
    json_t *response = request("/diffequation-eval", "{\"diffequation\":\"Dx(y)=x*y; y(0)=1\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    bool ok = status == 200 && contains(response, "solutions", "y") && json_array_size(bindings) == 1 &&
              !strcmp(test_lab_text(json_array_get(bindings, 0), "name"), "x");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "ODE results identify the independent coordinate by name, not its unset value");
    response = request("/diffequation-eval", "{\"diffequation\":\"y''+3y*y'+y^4=0; y(0)=1; y'(0)=0\"}", &status);
    ok = status == 200 && contains(response, "status", "series") &&
         json_array_size(test_lab_member(response, "binding_values")) == 0;
    json_free(response);
    TEST_ASSERT_TRUE(ok, "Local-series results do not advertise plotting coordinates");
    expect_response("/diffequation-eval", "{\"diffequation\":\"y''+6y*y'+4y^3=0; y(0)=0; y'(0)=1\"}",
                    200u, "solutions", "y = (x)/(x² + 1)");
}

static void test_lab_extra_context_results(void)
{
    unsigned status = 0;
    json_t *response = request("/equation-eval",
                               "{\"equation\":\"{x=sqrt(s^2) | x=?; s=15}\",\"precision\":40}", &status);
    bool ok = status == 200 && test_lab_ok(response, true) && contains(response, "solutions", "x = 15") &&
              !contains(response, "solutions", "√");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "equation cards retain their established bound-constant substitution policy");
    response = request("/matrix-eval",
                       "{\"matrix\":\"{ (sqrt([s]^2),0;0,1) | ; [s]=15 }\",\"operation\":\"simplify\"}",
                       &status);
    ok = status == 200 && test_lab_ok(response, true) && contains(response, "result", "s") &&
         !contains(response, "result", "√");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "matrix cards share assumption-aware symbolic simplification");
    response = request("/matrix-eval",
                       "{\"matrix\":\"{ (sqrt([s]^2),0;0,1) | [s]=15 }\",\"operation\":\"simplify\"}",
                       &status);
    ok = status == 200 && test_lab_ok(response, true) && contains(response, "result", "√") &&
         contains(response, "value", "15");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "a variable sample affects numerical evaluation but does not prove its sign");
}

static void test_lab_extra_matrix(void)
{
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"det\"}", 200u, "result", "-2");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"trace\"}", 200u, "result", "5");
    expect_response("/matrix-eval", "{\"matrix\":\"(2,0;0,3)\",\"operation\":\"eigenvalues\"}", 200u, "result", "3");
    expect_response("/matrix-eval", "{\"matrix\":\"(2,0;0,3)\",\"operation\":\"eigendecompose\"}", 200u,
                    "result", "eigenvectors");
    /* Output slots must be safe to release when a numeric operation rejects a non-square matrix. */
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"det\"}", 422u, "error", "det");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"trace\"}", 422u, "error", "trace");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"eigenvalues\"}", 422u,
                    "error", "eigenvalues");
    expect_response("/matrix-eval", "{\"matrix\":\"(1,2,3;4,5,6)\",\"operation\":\"eigendecompose\"}", 422u,
                    "error", "eigendecompose");
}

static void test_lab_extra_integrator(void)
{
    unsigned status = 0u;
    json_t *response = request("/integrator-eval",
                               "{\"expression\":\"{x|x=?}\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}],"
                               "\"precision\":40}",
                               &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    bool ok = status == 200u && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "value"), "½") &&
              !strcmp(test_lab_text(response, "expression"), "x") && contains(response, "binding_expression", "x") &&
              !contains(response, "tex", "NAN") && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(json_array_get(bindings, 0u), "name"), "x");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "exact half integral retains its root integrand and native editor binding");
    /* Exercise both infinity parsers and the numerical output slots under worker memory checks. */
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(x)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"-inf\",\"hi\":\"0\"}],"
                    "\"precision\":40}", 200u, "value", "1");
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(-x)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"inf\"}],"
                    "\"precision\":40}", 200u, "value", "1");
    expect_response("/integrator-eval",
                    "{\"expression\":\"exp(Li(x))\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}],"
                    "\"precision\":17}", 200u, "value", "0.6243299885");
}

static void test_lab_extra_function(void)
{
    expect_response("/function-run", "{\"source\":\"output(2+3).\",\"precision\":40}", 200u, "output", "5");
}

static void test_lab_extra_goal_seek(void)
{
    expect_response("/goal_seek", "{\"expression\":\"{x|x=0}\",\"target\":\"2\",\"start\":{\"x\":\"0\"}}", 200u,
                    "value", "2");
}

static void test_lab_extra_render_TeX(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\frac{1}{2}+x^2\"}", 200u, "svg", "<svg");
    expect_response("/render_TeX", "{\"tex\":\"2\\\\mkern-2mu x+3\\\\mkern2mu y\"}", 200u, "svg", "<svg");
    expect_response("/eval", "{\"expression\":\"2*x\"}", 200u, "svg", "<svg");
    expect_response("/matrix-eval", "{\"matrix\":\"{ (1,2;3,4)^x | x = @pi }\"}", 200u, "svg", "<svg");
    expect_response("/diffequation-eval", "{\"diffequation\":\"y'+2y=0\"}", 200u, "svg", "<svg");
}

static void test_lab_extra_matrix_power_bindings(void)
{
    const char *values[] = {"2", "@pi", "?"};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i) {
        string_t *payload = string_sprintf("{\"matrix\":\"{ (1,2;3,4)^x | x=%s }\"}", values[i]);
        unsigned status = 0;
        json_t *response = request("/matrix-eval", string_c_str(payload), &status);
        bool ok = status == 200 && contains(response, "expression", "x") && contains(response, "svg", "<svg") &&
                  !contains(response, "expression", "NAN");
        if (i < 2)
            ok = ok && *lab_eval_text(response, "value") && !contains(response, "value", "NAN");
        else
            ok = ok && !*lab_eval_text(response, "value");
        if (i == 1)
            ok = ok && contains(response, "expression", "x = π");
        if (!ok)
            show_failure(response);
        json_free(response);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "repeated binding snapshots retain symbolic powers and exact bindings across updates");
    }
}

static void test_lab_extra_conjugate_pair_identity(void)
{
    expect_response("/equation-eval", "{\"equation\":\"(x+1)^4 = 1\"}", 200u, "display_TeX", "\\pm");
    expect_response("/equation-eval", "{\"equation\":\"x^2+2*x+2 = 0\"}", 200u, "display_solutions", "-1 ± i");
    expect_response("/equation-eval", "{\"equation\":\"x^2+2*x+3 = 0\"}", 200u, "display_TeX", "\\pm");
}

static void test_lab_extra_projection_fonts(void)
{
    static const struct { const char *route; const char *payload; const char *field; } cases[] = {
        {"/eval", "{\"expression\":\"Re(a)+Im(a)\"}", "display_TeX"},
        {"/equation-eval", "{\"equation\":\"x=Re(a)+Im(a)\"}", "display_TeX"},
        {"/diffequation-eval", "{\"diffequation\":\"y'=Re(a)+Im(a)\"}", "display_TeX"},
        {"/matrix-eval", "{\"matrix\":\"(Re(a),Im(a);0,1)\"}", "tex"},
        {"/integrator-eval", "{\"expression\":\"(Re(a)+Im(a))*x\",\"bounds\":[{\"name\":\"x\"}]}", "tex"}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned status = 0u;
        json_t *response = request(cases[i].route, cases[i].payload, &status);
        bool ok = status == 200u && test_lab_ok(response, true) &&
                  contains(response, cases[i].field, "\\mathfrak{Re}") &&
                  contains(response, cases[i].field, "\\mathfrak{Im}") && contains(response, "svg", "<svg");
        if (!ok)
            show_failure(response);
        json_free(response);
        TEST_ASSERT_TRUE(ok, "every mathematical mode renders two-letter Fraktur projections");
    }
}

static void test_lab_extra_unsafe_TeX_input(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\input{/etc/passwd}\"}", 422u, "error", "unsupported");
}

static void test_lab_extra_unsafe_TeX_escape(void)
{
    expect_response("/render_TeX", "{\"tex\":\"^^5cinput{/etc/passwd}\"}", 422u, "error", "unsupported");
}

static void test_lab_extra_unsafe_TeX_environment(void)
{
    expect_response("/render_TeX", "{\"tex\":\"\\\\begin{filecontents}{x}data\\\\end{filecontents}\"}", 422u, "error",
                    "unsupported");
}

static void test_lab_extra_invalid_type(void)
{
    expect_response("/eval", "{\"expression\":3}", 400u, "error", "Input");
}

static void test_lab_extra_invalid_precision(void)
{
    expect_response("/eval", "{\"expression\":\"x\",\"precision\":true}", 400u, "error", "Precision");
}

static void test_lab_extra_excess_precision(void)
{
    expect_response("/eval", "{\"expression\":\"x\",\"precision\":315654}", 400u, "error", "Precision");
}

static void test_lab_extra_function_precision_limit(void)
{
    expect_response("/function-run", "{\"source\":\"output(1).\",\"precision\":10001}", 400u, "error",
                    "Function precision");
}

static void test_lab_extra_invalid_bounds(void)
{
    expect_response("/integrator-eval", "{\"expression\":\"x\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\"}]}", 400u,
                    "error", "Invalid");
}

static void test_lab_extra_invalid_operation(void)
{
    expect_response("/matrix-eval", "{\"matrix\":\"(1)\",\"operation\":\"shell\"}", 400u, "error", "Invalid");
}

static void test_lab_extra_calendar_fallback(void)
{
    unsigned status = 777u;
    json_t *response = request("/datetime-eval", "{}", &status);
    bool ok = !response && status == 777u;
    json_free(response);
    TEST_ASSERT_TRUE(ok, "unhandled calendar route preserves the caller's status");
}

static void test_lab_extra_equation_explicit_bindings(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"{ x = c | x = ?; c = 2 }\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    bool variable = false;
    bool constant = false;
    /* Inspect the two records in this fixture; native order is deliberately unconstrained. */
    for (size_t i = 0u; i < json_array_size(bindings) && i < 2u; ++i) {
        const json_t *binding = json_array_get(bindings, i);
        if (!strcmp(test_lab_text(binding, "name"), "x"))
            variable = !strcmp(test_lab_text(binding, "kind"), "variable") &&
                       !strcmp(test_lab_text(binding, "value"), "NAN") && !*test_lab_text(binding, "display");
        if (!strcmp(test_lab_text(binding, "name"), "c"))
            constant = !strcmp(test_lab_text(binding, "kind"), "constant") &&
                       !strcmp(test_lab_text(binding, "value"), "2") && !strcmp(test_lab_text(binding, "display"), "2");
    }
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 2u && variable && constant;
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "native equation records populate unset variables and supplied constant controls");
}

static void test_lab_extra_equation_inferred_binding(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"x = 2\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    const json_t *binding = json_array_get(bindings, 0u);
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(binding, "name"), "x") && !strcmp(test_lab_text(binding, "kind"), "variable");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "inferred equation bindings reach the UI without client expression parsing");
}

static void test_lab_extra_equation_greek_binding(void)
{
    unsigned status = 0u;
    json_t *response = request("/equation-eval", "{\"equation\":\"{ @mu = 2 | @mu = ? }\"}", &status);
    const json_t *bindings = test_lab_member(response, "binding_values");
    const json_t *binding = json_array_get(bindings, 0u);
    bool ok = status == 200u && test_lab_ok(response, true) && json_array_size(bindings) == 1u &&
              !strcmp(test_lab_text(binding, "name"), "μ") && !strcmp(test_lab_text(binding, "kind"), "variable");
    if (!ok)
        show_failure(response);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "equation binding names use native Expression rendering for Greek aliases");
}

/* Register each case independently so test_config can select or suppress it. */
void test_lab_evaluation_extra_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_extra_plot, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_continuity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_height, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_names, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_ranges, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_stepped_ranges, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_preview, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_series_domain, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_tangent_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_turning_points, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_solver_plots, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_solution_families, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_frame, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_title_fallback, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_native_records, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_editor_and_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_expression, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_help_examples, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_constant_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_binding_edit, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_derivative, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integral, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_gaussian_integral, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_repeated_gaussian_integral, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_gaussian_derivative_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_exact_derivative_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_real_surd_numerics, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_decimal_notation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_number_card_markup, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_diffequation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_context_results, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_matrix, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_integrator, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_function, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_goal_seek, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_render_TeX, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_matrix_power_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_conjugate_pair_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_projection_fonts, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_escape, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_unsafe_TeX_environment, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_type, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_excess_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_function_precision_limit, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_invalid_operation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_calendar_fallback, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_explicit_bindings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_inferred_binding, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_greek_binding, tests, NULL);
}
