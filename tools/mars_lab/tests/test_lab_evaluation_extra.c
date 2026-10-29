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
#include <time.h>

#include "xml-t.h"
#include "lab_presentation.h"
#include "lab_wire.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
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

static bool paired_mesh_edges(const char *svg, bool open);

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
    bool ok = status == 200 && test_lab_ok(fields, true) &&
              !strstr(svg, "Detail reduced for this view") && !strstr(svg, "<text data-detail-reduced") &&
              facets >= 2 * 69 * 69 && paired_mesh_edges(svg, true);
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
    TEST_ASSERT_TRUE(ok, "rapid bounded surface retains a complete grid within the transport budget");
}

typedef struct {
    long long coordinate[6];
} mesh_test_edge_t;

static int compare_mesh_edge(const void *left, const void *right)
{
    const mesh_test_edge_t *a = left, *b = right;
    for (unsigned i = 0; i < 6; ++i)
        if (a->coordinate[i] != b->coordinate[i])
            return a->coordinate[i] < b->coordinate[i] ? -1 : 1;
    return 0;
}

/* Every edge of these closed fixtures must have exactly two incident facets.
 * Quantise below sampling resolution to merge round-off at shared grid vertices. */
static bool paired_mesh_edges(const char *svg, bool open)
{
    size_t capacity = 196608, count = 0;
    mesh_test_edge_t *edges = calloc(capacity, sizeof(*edges));
    bool ok = edges != NULL;
    for (const char *p = strstr(svg, "data-vertices=\""); ok && p; p = strstr(p, "data-vertices=\"")) {
        p += strlen("data-vertices=\"");
        long long vertices[3][3];
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned axis = 0; axis < 3; ++axis) {
                char *end = NULL;
                double value = strtod(p, &end);
                ok &= end != p && isfinite(value);
                vertices[i][axis] = llround(value * 1e7);
                p = end;
            }
        bool degenerate = false;
        for (unsigned i = 0; i < 3; ++i)
            degenerate |= !memcmp(vertices[i], vertices[(i + 1) % 3], sizeof(vertices[i]));
        if (degenerate)
            continue;
        for (unsigned i = 0; ok && i < 3; ++i) {
            const long long *a = vertices[i], *b = vertices[(i + 1) % 3];
            for (unsigned axis = 0; axis < 3; ++axis) {
                if (a[axis] == b[axis])
                    continue;
                if (a[axis] > b[axis]) {
                    const long long *swap = a;
                    a = b;
                    b = swap;
                }
                break;
            }
            ok = count < capacity;
            if (ok) {
                memcpy(edges[count].coordinate, a, sizeof(vertices[0]));
                memcpy(edges[count++].coordinate + 3, b, sizeof(vertices[0]));
            }
        }
    }
    if (ok)
        qsort(edges, count, sizeof(*edges), compare_mesh_edge);
    size_t unmatched = 0;
    for (size_t i = 0; ok && i < count;) {
        size_t end = i + 1;
        while (end < count && !compare_mesh_edge(&edges[i], &edges[end]))
            ++end;
        bool border = false;
        for (unsigned axis = 0; open && axis < 2; ++axis) {
            long long value = edges[i].coordinate[axis];
            border |= (value == 0 || value == 10000000) && value == edges[i].coordinate[axis + 3];
        }
        unmatched += end - i != (border ? 1 : 2);
        i = end;
    }
    if (unmatched)
        string_printf("Unpaired mesh edges: %zu of %zu\n", unmatched, count);
    free(edges);
    return ok && count > 0 && unmatched == 0;
}

static bool closed_mesh(const char *svg)
{
    return paired_mesh_edges(svg, false);
}

static double fold_error(const char *svg, size_t *facets, double *area)
{
    const char *lower = strstr(svg, "data-z-min=\""), *upper = strstr(svg, "data-z-max=\"");
    if (!lower || !upper)
        return INFINITY;
    double lo = strtod(lower + strlen("data-z-min=\""), NULL);
    double hi = strtod(upper + strlen("data-z-max=\""), NULL), error = 0;
    *facets = 0;
    *area = 0;
    for (const char *p = strstr(svg, "data-vertices=\""); p; p = strstr(p, "data-vertices=\"")) {
        p += strlen("data-vertices=\"");
        double v[3][3], centre[3] = {0};
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned axis = 0; axis < 3; ++axis) {
                char *end = NULL;
                v[i][axis] = strtod(p, &end);
                if (end == p)
                    return INFINITY;
                p = end;
                centre[axis] += v[i][axis] / 3;
            }
        error = fmax(error, fabs(lo + (hi - lo) * centre[2] - fabs(centre[0] - centre[1] - 0.1)));
        *area += fabs((v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) -
                      (v[2][0] - v[0][0]) * (v[1][1] - v[0][1])) * 0.5;
        ++*facets;
    }
    return error;
}

static void test_lab_extra_surface_refinement(void)
{
    number_t axis[5], z[25];
    for (unsigned i = 0; i < 5; ++i)
        axis[i] = num_create_from_frac(i, 4);
    for (unsigned r = 0; r < 5; ++r)
        for (unsigned c = 0; c < 5; ++c)
            z[r * 5 + c] = num_create_from_double(fabs((double)c / 4 - (double)r / 4 - 0.1));
    plot_t *plot = plot_new(NULL);
    bool ok = plot && plot_set_surface(plot, axis, 5, axis, 5, z);
    string_t *before = ok ? plot_to_svg(plot) : NULL;
    ok = ok && before && lab_worker_surface_refine(plot, "abs(x-y-1/10)", "x", "y", axis, axis, z, 5, 53);
    string_t *after = ok ? plot_to_svg(plot) : NULL;
    size_t coarse = 0, fine = 0;
    double area_before = 0, area_after = 0;
    double error_before = before ? fold_error(string_c_str(before), &coarse, &area_before) : INFINITY;
    double error_after = after ? fold_error(string_c_str(after), &fine, &area_after) : INFINITY;
    ok = ok && after && coarse == 32 && fine > coarse && fine < 512 && error_after < error_before * 0.5 &&
         fabs(area_before - 1) < 1e-7 && fabs(area_after - 1) < 1e-7 &&
         strstr(string_c_str(after), "data-mesh=\"false\"") && paired_mesh_edges(string_c_str(after), true);
    TEST_ASSERT_TRUE(ok, "local folds gain accuracy with fewer facets than global refinement and no interior cracks");
    string_free(before);
    string_free(after);
    plot_free(plot);
    for (unsigned r = 0; r < 5; ++r) {
        for (unsigned c = 0; c < 5; ++c) {
            num_destroy(&z[r * 5 + c]);
            double argument = (double)c / 4 - 0.1;
            z[r * 5 + c] = argument < 0 ? num_clone(NUM_NAN) :
                          num_create_from_double(sqrt(argument) + (double)r / 4);
        }
    }
    plot = plot_new(NULL);
    ok = plot && plot_set_surface(plot, axis, 5, axis, 5, z) &&
         lab_worker_surface_refine(plot, "sqrt(x-1/10)+y", "x", "y", axis, axis, z, 5, 53);
    after = ok ? plot_to_svg(plot) : NULL;
    if (after)
        fold_error(string_c_str(after), &fine, &area_after);
    ok = ok && after && area_after > 0.85 && area_after <= 0.9;
    TEST_ASSERT_TRUE(ok, "mixed domain cells retain valid subcells without filling the non-real region");
    string_free(after);
    plot_free(plot);
    for (unsigned i = 0; i < 5; ++i)
        num_destroy(&axis[i]);
    for (unsigned i = 0; i < 25; ++i)
        num_destroy(&z[i]);

    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"acos(sin(2-x*y))\",\"action\":\"plot\","
                             "\"plot_lower\":\"-4\",\"plot_upper\":\"4\",\"plot_second_lower\":\"-4\","
                             "\"plot_second_upper\":\"4\",\"plot_scale\":\"auto\"}", &status);
    const char *svg = lab_eval_text(fields, "plot_svg");
    size_t count = 0;
    for (const char *p = strstr(svg, "data-vertices=\""); p; p = strstr(p + 15, "data-vertices=\""))
        ++count;
    ok = status == 200 && test_lab_ok(fields, true) && count >= 2 * 69 * 69 &&
         contains(fields, "plot_svg", "data-sample-side=\"70\"") && paired_mesh_edges(svg, true);
    TEST_ASSERT_TRUE(ok, "oversized folded-surface refinement preserves the full uniform grid before reducing resolution");
    json_free(fields);
}

static void test_lab_extra_mesh_closed(void)
{
    const char *equations[] = {"x^2+y^2+z^2=9", "(sqrt(x^2+y^2)-2)^2+z^2=1"};
    for (unsigned i = 0; i < 2; ++i) {
        unsigned status = 0;
        string_t *payload = string_sprintf(
            "{\"expression\":\"%s\",\"wrt\":\"x\",\"action\":\"plot\","
            "\"plot_source_mode\":\"equation\",\"plot_scale\":\"auto\"}", equations[i]);
        json_t *fields = request("/eval", string_c_str(payload), &status);
        const char *svg = lab_eval_text(fields, "plot_svg");
        const char *lo = strstr(svg, "data-x-min=\""), *hi = strstr(svg, "data-x-max=\"");
        bool ok = status == 200 && test_lab_ok(fields, true) && lo && hi &&
                  strtod(lo + strlen("data-x-min=\""), NULL) < -3 &&
                  strtod(hi + strlen("data-x-max=\""), NULL) > 3 && closed_mesh(svg);
        json_free(fields);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "automatic sphere and torus contain complete closed surfaces beyond the initial box");
    }
}

static void test_lab_extra_mesh_crossings(void)
{
    /* Within this box exactly two separate phase sheets meet. Their union must
     * retain both branches, rather than triangulating across the intersection. */
    const char *equations[] = {"cos(z)-sin(2-xy)=0", "z-(x*y+pi/2-2)=0", "z+(x*y+pi/2-2)=0",
                              "sin(2-xy)=cos(z)", "0=cos(z)-sin(2-xy)"};
    const char *qualities[] = {"preview", "full"};
    for (unsigned quality = 0; quality < 2; ++quality) {
        size_t counts[5] = {0};
        for (unsigned form = 0; form < 5; ++form) {
            unsigned status = 0;
            string_t *payload = string_sprintf(
                "{\"expression\":\"%s\",\"wrt\":\"z\",\"action\":\"plot\",\"plot_source_mode\":\"equation\","
                "\"plot_quality\":\"%s\",\"plot_scale\":\"manual\",\"plot_lower\":\"-1.2\",\"plot_upper\":\"1.2\","
                "\"plot_second_lower\":\"-1.2\",\"plot_second_upper\":\"1.2\","
                "\"plot_y_lower\":\"-1.2\",\"plot_y_upper\":\"1.2\"}", equations[form], qualities[quality]);
            json_t *fields = request("/eval", string_c_str(payload), &status);
            bool ok = status == 200 && test_lab_ok(fields, true);
            const char *svg = lab_eval_text(fields, "plot_svg");
            for (const char *p = strstr(svg, "data-uv="); p; p = strstr(p + 8, "data-uv="))
                ++counts[form];
            json_free(fields);
            string_free(payload);
            TEST_ASSERT_TRUE(ok, "each phase sheet and equivalent equation produces a bounded surface");
        }
        TEST_ASSERT_TRUE(counts[0] > 0 && counts[0] == counts[1] + counts[2],
                         "periodic crossings retain the complete union of independently meshed sheets");
        TEST_ASSERT_TRUE(counts[0] == counts[3] && counts[0] == counts[4],
                         "reversing the equation preserves the periodic sheets");
    }
}

static void test_lab_extra_mesh_budget(void)
{
    expect_response("/eval", "{\"expression\":\"cos(z)-sin(2-xy)=0\",\"wrt\":\"z\",\"action\":\"plot\","
                              "\"plot_source_mode\":\"equation\",\"plot_scale\":\"manual\","
                              "\"plot_lower\":\"-100\",\"plot_upper\":\"100\","
                              "\"plot_second_lower\":\"-100\",\"plot_second_upper\":\"100\","
                              "\"plot_y_lower\":\"-100\",\"plot_y_upper\":\"100\"}",
                    422u, "error", "Too many surface sheets");
    const char *ranges[][6] = {
        {"-6.55", "5.17", "-4.04", "7.68", "-5.86", "5.86"},
        {"-25", "25", "-25", "25", "-25", "25"},
        {"-2", "2", "-2", "2", "-2", "2"}
    };
    for (size_t i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        unsigned status = 0;
        string_t *payload = string_sprintf(
            "{\"expression\":\"cos(z)-sin(2-xy)=0\",\"wrt\":\"z\",\"action\":\"plot\","
            "\"plot_source_mode\":\"equation\",\"plot_scale\":\"manual\","
            "\"plot_lower\":\"%s\",\"plot_upper\":\"%s\",\"plot_second_lower\":\"%s\","
            "\"plot_second_upper\":\"%s\",\"plot_y_lower\":\"%s\",\"plot_y_upper\":\"%s\"}",
            ranges[i][0], ranges[i][1], ranges[i][2], ranges[i][3], ranges[i][4], ranges[i][5]);
        json_t *fields = request("/eval", string_c_str(payload), &status);
        const char *svg = lab_eval_text(fields, "plot_svg");
        bool ok = status == 200 && test_lab_ok(fields, true) &&
                  contains(fields, "plot_svg", "data-detail-reduced=\"true\"") == (i < 2) &&
                  contains(fields, "plot_svg", "data-uv=") && strlen(svg) < 7u * 1024u * 1024u;
        const char *attributes[] = {"x-min", "x-max", "y-min", "y-max", "z-min", "z-max"};
        for (unsigned axis = 0; ok && axis < 6; ++axis) {
            string_t *attribute = string_sprintf("data-%s=\"", attributes[axis]);
            const char *value = strstr(svg, string_c_str(attribute));
            ok = value && strtod(value + string_byte_length(attribute), NULL) == strtod(ranges[i][axis], NULL);
            string_free(attribute);
        }
        bool failed = true;
        ok = ok && lab_presentation_adapt(fields);
        array_t *encoded = ok ? lab_wire_encode_response(fields, &failed) : NULL;
        json_t *decoded = encoded ? lab_wire_decode(array_get(encoded, 0), array_size(encoded)) : NULL;
        const json_t *card = lab_eval_get(lab_eval_get(lab_eval_get(decoded, "presentation"), "calculus"), "plot");
        ok = ok && !failed && decoded && !strcmp(svg, lab_eval_text(card, "svg"));
        json_free(decoded);
        array_destroy(encoded);
        json_free(fields);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "wide implicit views preserve all bounds and fit the encoded response without truncation");
    }
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
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-segment=") &&
              contains(fields, "plot_complex_plane", "data-series=") &&
              contains(fields, "plot_components", "Imaginary") && contains(fields, "plot_amplitude", "data-series=");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "automatic range selection supports a curve with purely imaginary values");

    /* A real-height surface still requires finite real samples in its trial windows. */
    fields = request("/eval", "{\"expression\":\"sqrt(-1-x^2-y^2)\",\"action\":\"plot\","
                             "\"plot_kind\":\"surface\",\"wrt\":\"x\",\"plot_second\":\"y\"}", &status);
    ok = status == 422 && test_lab_ok(fields, false) &&
         contains(fields, "error", "Automatic range selection failed") &&
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

/* Tick labels may be thinned on short equal-unit axes; viewport metadata remains authoritative. */
static double surface_height_bound(const json_t *fields, bool upper)
{
    const char *key = upper ? "data-z-max=\"" : "data-z-min=\"";
    const char *attribute = strstr(lab_eval_text(fields, "plot_svg"), key);
    if (!attribute)
        return NAN;
    const char *start = attribute + strlen(key);
    char *end = NULL;
    double value = strtod(start, &end);
    return end != start && *end == '"' ? value : NAN;
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
        maxima[i] = fields ? surface_height_bound(fields, true) : NAN;
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
    ok = status == 200 && test_lab_ok(preview, true) && surface_height_bound(preview, false) == 0 &&
         surface_height_bound(preview, true) == 0.5 &&
         contains(preview, "plot_svg", "data-axis-handle=\"2\"") &&
         contains(preview, "plot_svg", "data-azimuth=\"120.000\"");
    json_free(preview);
    TEST_ASSERT_TRUE(ok, "coarse rotation previews preserve manual height limits");
    for (unsigned radius = 1; radius <= 3; radius += 2) {
        string_t *payload = string_sprintf(
            "{\"expression\":\"acos(sin(2-x*y))\",\"action\":\"plot\",\"plot_scale\":\"auto\","
            "\"plot_lower\":\"-%u\",\"plot_upper\":\"%u\",\"plot_second_lower\":\"-%u\","
            "\"plot_second_upper\":\"%u\",\"plot_y_lower\":\"1\",\"plot_y_upper\":\"2\"}",
            radius, radius, radius, radius);
        json_t *fields = request("/eval", string_c_str(payload), &status);
        double low = surface_height_bound(fields, false), high = surface_height_bound(fields, true);
        ok = status == 200 && test_lab_ok(fields, true) && low < 0.02 &&
             (radius == 1 ? high > 1.42 && high < 1.44 : high > 3.12 && high < 3.15);
        if (!ok) show_failure(fields);
        TEST_ASSERT_TRUE(ok, "Auto recomputes the finite folded surface height for each horizontal zoom region");
        json_free(fields);
        string_free(payload);
    }
}

static void test_lab_extra_folded_laplace_plot(void)
{
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"@L{acos(sin(2-x))}\",\"action\":\"plot\","
                            "\"wrt\":\"s\",\"plot_quality\":\"preview\"}", &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-series") &&
              !contains(fields, "function", "laplace(");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "a closed periodic Laplace spectrum automatically plots within its real convergence domain");
}

static void test_lab_extra_complex_plot(void)
{
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"exp(i*x)\",\"action\":\"plot\","
        "\"wrt\":\"x\",\"plot_lower\":\"-pi\",\"plot_upper\":\"pi\",\"plot_quality\":\"preview\"}", &status);
    bool ok = status == 200 && contains(fields, "plot_svg", "data-segments=\"true\"") &&
        contains(fields, "plot_complex_plane", "<svg") && contains(fields, "plot_components", "Imaginary") &&
        contains(fields, "plot_amplitude", "data-series=") &&
        !contains(fields, "plot_amplitude", ">Amplitude</text>") &&
        contains(fields, "plot_components", "data-plot-legend=\"true\"") &&
        !contains(fields, "plot_components", ">Value</text>") &&
        contains(fields, "plot_components", "stroke=\"#d6aa55\"") &&
        contains(fields, "plot_components", "stroke=\"#78d8cb\"");
    const char *projections[] = {"plot_svg", "plot_complex_plane", "plot_components", "plot_amplitude"};
    for (unsigned i = 0; i < sizeof(projections) / sizeof(*projections); ++i)
        ok = ok && contains(fields, projections[i], "data-plot-title=\"true\"") &&
             contains(fields, projections[i], "data-plot-figure=\"true\"");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "complex exponential shares native samples across helix, circle, components and amplitude");
    expect_response("/eval", "{\"expression\":\"delta(x)^2+i*x\",\"action\":\"plot\",\"wrt\":\"x\"}",
                    422u, "error", "distribution");
    fields = request("/eval", "{\"expression\":\"(1+i)*(x-1)\",\"action\":\"plot\",\"wrt\":\"x\","
        "\"plot_lower\":\"1\",\"plot_upper\":\"1+1e-30\",\"plot_quality\":\"preview\"}", &status);
    ok = status == 200 && contains(fields, "plot_svg", "data-segment=") &&
         contains(fields, "plot_svg", "1.000000000000000000000000000001") &&
         contains(fields, "plot_amplitude", "data-series=");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "complex projections preserve native input bounds narrower than a double can distinguish");
    fields = request("/eval", "{\"expression\":\"exp(i*x)\",\"action\":\"plot\",\"wrt\":\"x\","
        "\"plot_lower\":\"-12\",\"plot_upper\":\"8\",\"plot_scale\":\"manual\","
        "\"plot_y_lower\":\"-0.5\",\"plot_y_upper\":\"1.5\"}", &status);
    ok = status == 200 && contains(fields, "plot_components", "data-x-min=\"-12\"") &&
         contains(fields, "plot_components", "data-x-max=\"8\"") &&
         contains(fields, "plot_components", "data-y-min=\"-0.5\"") &&
         contains(fields, "plot_components", "data-y-max=\"1.5\"") &&
         contains(fields, "plot_amplitude", "data-y-min=\"-0.5\"") &&
         contains(fields, "plot_amplitude", "data-y-max=\"1.5\"");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "component and amplitude panning preserve both requested axis intervals");
    fields = request("/eval", "{\"expression\":\"exp(i*x)\",\"action\":\"plot\",\"wrt\":\"x\","
        "\"plot_plane_real_lower\":\"-3\",\"plot_plane_real_upper\":\"1\","
        "\"plot_plane_imag_lower\":\"-2\",\"plot_plane_imag_upper\":\"2\"}", &status);
    ok = status == 200 && contains(fields, "plot_complex_plane", "data-x-min=\"-3\"") &&
         contains(fields, "plot_complex_plane", "data-x-max=\"1\"") &&
         contains(fields, "plot_complex_plane", "data-y-min=\"-2\"") &&
         contains(fields, "plot_complex_plane", "data-y-max=\"2\"") &&
         contains(fields, "plot_complex_plane", ">ℜ</text>") &&
         contains(fields, "plot_complex_plane", ">ℑ</text>") &&
         contains(fields, "plot_components", "data-x-min=\"-10\"") &&
         contains(fields, "plot_components", "data-x-max=\"10\"");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "Argand viewport bounds are independent of sampling, with Gothic axis titles");
    fields = request("/eval", "{\"expression\":\"exp(i*x)\",\"action\":\"plot\",\"wrt\":\"x\","
        "\"plot_plane_real_lower\":\"-3\"}", &status);
    ok = status == 422;
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "partial Argand viewport bounds are rejected rather than silently changing the sample");
}

static void test_lab_extra_complex_real_subset(void)
{
    const char *sources[] = {"ln(-x)", "x"};
    const char *qualities[] = {"preview", "full"};
    for (unsigned i = 0; i < 2; ++i) {
        for (unsigned q = 0; q < 2; ++q) {
            unsigned status = 0;
            string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\",\"wrt\":\"x\","
                "\"plot_kind\":\"complex_curve\",\"plot_lower\":\"-10\",\"plot_upper\":\"-1\","
                "\"plot_quality\":\"%s\"}", sources[i], qualities[q]);
            json_t *fields = payload ? request("/eval", string_c_str(payload), &status) : NULL;
            string_free(payload);
            bool ok = status == 200 && contains(fields, "plot_svg", "data-segments=\"true\"") &&
                contains(fields, "plot_svg", "data-segment=\"") &&
                contains(fields, "plot_complex_plane", "data-series=\"0\"") &&
                contains(fields, "plot_components", "Imaginary") &&
                contains(fields, "plot_amplitude", "data-series=\"0\"");
            if (!ok)
                show_failure(fields);
            json_free(fields);
            TEST_ASSERT_TRUE(ok, "targeted complex refinement retains all views on wholly real subsets");
        }
        unsigned status = 0;
        string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\",\"wrt\":\"x\","
            "\"plot_kind\":\"auto\",\"plot_lower\":\"-10\",\"plot_upper\":\"-1\"}", sources[i]);
        json_t *fields = payload ? request("/eval", string_c_str(payload), &status) : NULL;
        string_free(payload);
        bool ok = status == 200 && contains(fields, "plot_svg", "<svg") &&
                  !contains(fields, "plot_svg", "data-segments=\"true\"") &&
                  !contains(fields, "plot_complex_plane", "<svg");
        if (!ok)
            show_failure(fields);
        json_free(fields);
        TEST_ASSERT_TRUE(ok, "automatic real-only plotting still selects the ordinary real representation");
    }
}

/* Inspect only data-series paths: the zero axis is allowed to cross a domain gap. */
static bool complex_series_gap(const char *svg, unsigned index, double singularity)
{
    if (!svg)
        return false;
    const char *names[] = {"data-plot-left=\"", "data-plot-width=\"", "data-x-min=\"", "data-x-max=\""};
    double frame[4];
    for (unsigned i = 0; i < 4; ++i) {
        const char *attribute = strstr(svg, names[i]);
        if (!attribute)
            return false;
        char *end = NULL;
        frame[i] = strtod(attribute + strlen(names[i]), &end);
        if (!end || *end != '"' || !isfinite(frame[i]))
            return false;
    }
    if (!(frame[1] > 0) || !(frame[3] > frame[2]))
        return false;
    double gap = frame[0] + frame[1] * (singularity - frame[2]) / (frame[3] - frame[2]);
    string_t *marker = string_sprintf("<g data-series=\"%u\"", index);
    const char *series = marker ? strstr(svg, string_c_str(marker)) : NULL;
    string_free(marker);
    const char *end = series ? strstr(series, "</g>") : NULL;
    bool left = false, right = false;
    if (!end)
        return false;
    /* Every path in this bounded group is one native clipped line segment. */
    for (const char *path = strstr(series, "<path "); path && path < end; path = strstr(path + 1, "<path ")) {
        double x1, y1, x2, y2;
        if (sscanf(path, "<path d=\"M %lf %lf L %lf %lf", &x1, &y1, &x2, &y2) != 4 ||
            !isfinite(x1) || !isfinite(y1) || !isfinite(x2) || !isfinite(y2))
            return false;
        /* Account for the renderer's three-decimal coordinate rounding. */
        if (fmin(x1, x2) < gap - 0.001 && fmax(x1, x2) > gap + 0.001)
            return false;
        left |= fmax(x1, x2) < gap - 0.001;
        right |= fmin(x1, x2) > gap + 0.001;
    }
    return left && right;
}

static void test_lab_extra_complex_domain_gaps(void)
{
    const char *sources[] = {"ln(-x)", "ln(-2*x)", "ln(1/3-x)"};
    const double singularities[] = {0, 0, 1.0 / 3};
    const char *qualities[] = {"preview", "full"};
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned q = 0; q < 2; ++q) {
            string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\",\"wrt\":\"x\","
                "\"plot_lower\":\"-10\",\"plot_upper\":\"10\",\"plot_quality\":\"%s\"}", sources[i], qualities[q]);
            unsigned status = 0;
            json_t *fields = payload ? request("/eval", string_c_str(payload), &status) : NULL;
            string_free(payload);
            bool ok = status == 200 &&
                complex_series_gap(lab_eval_text(fields, "plot_components"), 0, singularities[i]) &&
                complex_series_gap(lab_eval_text(fields, "plot_components"), 1, singularities[i]) &&
                complex_series_gap(lab_eval_text(fields, "plot_amplitude"), 0, singularities[i]) &&
                contains(fields, "plot_components", "data-plot-axes=\"true\"") &&
                contains(fields, "plot_components", "stroke-dasharray=\"5 4\"");
            if (!ok)
                show_failure(fields);
            json_free(fields);
            TEST_ASSERT_TRUE(ok, "complex components and amplitude retain domain gaps independently of axes");
        }
    }
}

static void test_lab_extra_distribution_plot(void)
{
    expect_response("/eval", "{\"expression\":\"@F{sin(t)}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_svg", "data-x-min=\"-1.1");
    expect_response("/eval", "{\"expression\":\"@F{sin(t)}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_svg", "data-x-max=\"1.1");
    expect_response("/eval", "{\"expression\":\"@F{zeta(sigma+it)}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_svg", "data-spectrum-pending");
    expect_response("/eval", "{\"expression\":\"{@F{zeta(sigma+it)} | sigma=2}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_svg", "data-impulse");
    expect_response("/eval", "{\"expression\":\"{@F{zeta(sigma+it)} | sigma=2+i}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_title", "Fourier spectrum");
    expect_response("/eval", "{\"expression\":\"{@F{zeta(sigma+it)} | sigma=2}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_svg", "data-schematic-impulse");
    expect_response("/eval", "{\"expression\":\"{@F{zeta(sigma+it)} | sigma=2}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_title_TeX", "\\mathcal{F}");
    expect_response("/eval", "{\"expression\":\"{@F{zeta(sigma+it)} | sigma=2}\",\"action\":\"plot\",\"wrt\":\"ω\"}",
                    200u, "plot_title_TeX", "\\sigma + i\\,t");
    const char *sources[] = {"@F{step(t)}", "@F{acos(sin(pi-x))}", "@F{step(x)}", "@F{asin(t)}"};
    const char *variables[] = {"ω", "k", "k", "ω"};
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        unsigned status = 0;
        string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\","
                                           "\"wrt\":\"%s\",\"plot_quality\":\"preview\"}", sources[i], variables[i]);
        json_t *fields = request("/eval", string_c_str(payload), &status);
        bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-impulse") &&
                  contains(fields, "plot_svg", "Imaginary") && !contains(fields, "function", "fourier(") &&
                  (i == 1 || contains(fields, "plot_svg", "Imaginary curve")) &&
                  contains(fields, "plot_svg", "impulse weight") &&
                  (i == 1 || contains(fields, "plot_svg", "Value / weight")) &&
                  contains(fields, "plot_svg", "data-plot-legend=\"true\"") &&
                  contains(fields, "plot_svg", "data-distribution-spectrum=\"true\"") &&
                  (i != 3 || contains(fields, "plot_svg", "#b5a0ef")) &&
                  (i == 1 ? contains(fields, "plot_svg", "data-schematic-impulse") :
                            contains(fields, "plot_svg", "data-impulse-key=\"true\"")) &&
                  (i == 1 ? !*lab_eval_text(fields, "plot_notice") :
                            contains(fields, "plot_notice", "not point values")) &&
                  !*lab_eval_text(fields, "plot_complex_plane") && !*lab_eval_text(fields, "plot_amplitude");
        const char *svg = lab_eval_text(fields, "plot_svg");
        const char *curve = svg ? strstr(svg, "data-series=\"0\"") : NULL;
        const char *end = curve ? strstr(curve, "</g>") : NULL;
        const char *path = curve ? strstr(curve, "<path") : NULL;
        ok = ok && curve && end && path && path < end &&
             (i == 1 || contains(fields, "plot_svg", "data-x-min=\"-10\""));
        if (!ok)
            show_failure(fields);
        json_free(fields);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "native Fourier distributions plot regular components and labelled impulse weights");
        payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"evaluate\",\"wrt\":\"%s\"}",
                                 sources[i], variables[i]);
        fields = request("/eval", string_c_str(payload), &status);
        ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "svg", "<svg") &&
             !*lab_eval_text(fields, "render_error");
        if (i != 1)
            ok = ok && contains(fields, "transform_identity_TeX", " = ") &&
                 contains(fields, "transform_identity_TeX", "\\delta") &&
                 !contains(fields, "transform_identity_TeX", "NAN") &&
                 !contains(fields, "transform_identity_TeX", "Non-impulse part") &&
                 !contains(fields, "transform_identity_TeX", "Curve values only where") &&
                 !contains(fields, "transform_identity_TeX", "\\ne 0") &&
                 contains(fields, "transform_identity_TeX", i == 3 ? "\\operatorname{Fp}" : "principal value");
        if (!ok)
            show_failure(fields);
        json_free(fields);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "distribution result cards render native TeX rather than plain-text fallback");
    }
    expect_response("/eval", "{\"expression\":\"@F{ln(abs(x))}\",\"action\":\"plot\",\"wrt\":\"k\"}",
                    200u, "plot_svg", "data-impulse");
}

static void test_lab_extra_surface_slices(void)
{
    const char *examples[] = {
        "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\",\"plot_quality\":\"preview\"}",
        "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\",\"plot_quality\":\"preview\","
        "\"plot_slices\":\"z\\t-2\\t2\\t0\\t625\"}",
        "{\"expression\":\"x+y+z-10^80\",\"action\":\"plot\",\"plot_quality\":\"preview\","
        "\"plot_slices\":\"z\\t10^80\\t10^80+1/10^40\\t0\\t500\"}",
        "{\"expression\":\"x+y+log(z-5)\",\"action\":\"plot\",\"plot_quality\":\"preview\"}",
        "{\"expression\":\"x+y+z+w\",\"action\":\"plot\",\"plot_quality\":\"preview\"}",
        "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\",\"plot_quality\":\"preview\","
        "\"plot_slices\":\"z\\t-20\\t20\\t1/3\\t-2\"}",
        "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\",\"plot_quality\":\"preview\","
        "\"plot_slices\":\"z\\t-20\\t20\\t3\\t-2\"}"
    };
    for (unsigned i = 0; i < sizeof examples / sizeof *examples; ++i) {
        unsigned status = 0;
        json_t *fields = request("/eval", examples[i], &status);
        bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-surface") &&
                  contains(fields, "expression", "z = ?") && contains(fields, "plot_title", "z = ");
        if (i == 0)
            ok = ok && !strcmp(lab_eval_text(fields, "plot_slices"), "z\t-2\t2\t0\t500");
        if (i == 1)
            ok = ok && contains(fields, "plot_slices", "\t625") && contains(fields, "plot_title_TeX", "z = ") &&
                 contains(fields, "plot_svg", "data-plot-title");
        if (i == 2) {
            string_t *record = string_new_with(lab_eval_text(fields, "plot_slices"));
            size_t n = 0;
            string_t **parts = string_split(record, "\t", &n);
            expr_t *actual = n == 5 ? expr_from_string(string_c_str(parts[3]), NULL) : NULL;
            expr_t *expected = expr_from_string("10^80+1/(2*10^40)", NULL);
            number_t a = actual ? expr_eval(actual) : NUM_NAN, b = expected ? expr_eval(expected) : NUM_NAN;
            number_t origin = num_create_from_string("1e80");
            ok = ok && actual && expected && num_eq(a, b) && num_gt(a, origin);
            num_destroy(&origin);
            num_destroy(&a);
            num_destroy(&b);
            expr_free(actual);
            expr_free(expected);
            string_split_free(parts, n);
            string_free(record);
        }
        if (i == 4)
            ok = ok && contains(fields, "plot_slices", "\nw\t") && contains(fields, "plot_title", "w = ");
        if (i == 5)
            ok = ok && contains(fields, "plot_slices", "z\t-2\t2\t⅓\t");
        if (i == 6)
            ok = ok && !strcmp(lab_eval_text(fields, "plot_slices"), "z\t-2\t2\t0\t500");
        if (!ok)
            show_failure(fields);
        json_free(fields);
        TEST_ASSERT_TRUE(ok, "native slices preserve worksheet bindings, label the plot and retain exact parameter coordinates");
    }
    expect_response("/eval", "{\"expression\":\"x+y+z\",\"action\":\"plot\","
                    "\"plot_slices\":\"z\\t1\\t0\\t0\\t500\"}", 422u, "error", "Invalid slice");
    unsigned status = 0;
    json_t *empty = request("/eval", "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\","
                            "\"plot_slices\":\"z\\t-3\\t3\\t3\\t-1\"}", &status);
    TEST_ASSERT_TRUE(status == 422 && contains(empty, "plot_slices", "z\t-3\t3\t3\t1000"),
                     "empty slices retain their controls so the user can return to a real surface");
    json_free(empty);
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
    expect_response("/eval", "{\"expression\":\"x+y+z\",\"action\":\"plot\"}", 200u, "plot_slices", "z\t-2\t2\t0\t500");
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

/* Exercise both coordinate scopes without rendering or launching calculation workers. */
static void test_lab_extra_plot_range_ownership(void)
{
    const char *limits[] = {"", "", "", ""};
    size_t precision = num_get_default_prec_bits();

    for (unsigned dimensions = 1; dimensions <= 2; ++dimensions) {
        number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
        bool ok = lab_worker_plot_ranges(dimensions == 1 ? "x^2" : "x^2+y^2", "x",
                                         dimensions == 1 ? NULL : "y", limits, bounds);
        for (unsigned axis = 0; axis < dimensions; ++axis)
            ok = ok && num_to_double(bounds[2 * axis]) == -10 && num_to_double(bounds[2 * axis + 1]) == 10;
        for (unsigned i = 0; i < 4; ++i)
            num_destroy(&bounds[i]);
        TEST_ASSERT_TRUE(ok, "automatic range coordinates retain their expected bounds");
        TEST_ASSERT_TRUE(num_get_default_prec_bits() == precision, "range probing restores worksheet precision");
    }
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
                    200u, "plot_components", "Imaginary");
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
    const char *intervals[][2] = {{"-inf", "2"}, {"-2", "inf"}, {"-inf", "inf"}, {"inf", "-inf"}};
    for (size_t i = 0; i < sizeof(intervals) / sizeof(*intervals); ++i) {
        string_t *payload = string_sprintf("{\"expression\":\"exp(-x^2/2)\",\"action\":\"plot\","
            "\"plot_source_mode\":\"integrator\",\"bounds\":[{\"name\":\"x\",\"lo\":\"%s\",\"hi\":\"%s\"}]}",
            intervals[i][0], intervals[i][1]);
        expect_response("/eval", string_c_str(payload), 200u, "plot_svg",
                        i == 3 ? "data-sign=\"negative\"" : "data-sign=\"positive\"");
        string_free(payload);
    }
    const char *integrands[] = {"{exp(Li(x)) | x=?}", "{sin(x)+a | x=?; a=2}"};
    for (size_t i = 0; i < sizeof(integrands) / sizeof(*integrands); ++i) {
        string_t *payload = string_sprintf("{\"expression\":\"%s\",\"action\":\"plot\","
            "\"plot_source_mode\":\"integrator\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0.1\",\"hi\":\"0.9\"}]}",
            integrands[i]);
        json_t *result = request("/eval", string_c_str(payload), &status);
        bool title_ok = status == 200u && test_lab_ok(result, true) &&
                        contains(result, "plot_svg", "data-plot-title") &&
                        contains(result, "plot_title_TeX", i ? "\\sin" : "e^{") &&
                        !contains(result, "plot_title_TeX", "=") &&
                        !contains(result, "plot_title_TeX", "$") &&
                        !contains(result, "plot_title", "|") && !contains(result, "plot_title", "=");
        if (!title_ok)
            show_failure(result);
        TEST_ASSERT_TRUE(title_ok, "integrand titles use native TeX bodies and omit unset and supplied bindings");
        json_free(result);
        string_free(payload);
    }
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
    const char *height_modes[] = {"auto", "all", "manual"};
    const char *qualities[] = {"preview", "full"};
    for (unsigned mode = 0; mode < 3; ++mode) {
        for (unsigned quality = 0; quality < 2; ++quality) {
            string_t *payload = string_sprintf("{\"expression\":\"cos(z)-sin(2-x*y)=0\",\"wrt\":\"z\","
                "\"action\":\"plot\",\"plot_source_mode\":\"equation\",\"plot_quality\":\"%s\","
                "\"plot_lower\":\"-0.1\",\"plot_upper\":\"0.1\","
                "\"plot_second_lower\":\"-0.1\",\"plot_second_upper\":\"0.1\","
                "\"plot_scale\":\"%s\",\"plot_y_lower\":\"-2\",\"plot_y_upper\":\"2\"}",
                qualities[quality], height_modes[mode]);
            json_t *zoomed = request("/eval", string_c_str(payload), &status);
            bool zoom_ok = status == 200 && contains(zoomed, "plot_svg", "data-mesh=\"true\"") &&
                           contains(zoomed, "plot_svg", "data-x-min=\"-0.1") &&
                           contains(zoomed, "plot_svg", "data-y-min=\"-0.1") &&
                           contains(zoomed, "plot_svg", mode == 2 ? "data-empty=\"true\"" : "data-uv=");
            if (!zoom_ok)
                show_failure(zoomed);
            json_free(zoomed);
            string_free(payload);
            TEST_ASSERT_TRUE(zoom_ok, "zoomed implicit views search automatic height but preserve explicit empty volumes");
        }
    }
    expect_response("/eval", "{\"expression\":\"(x-1e30)^2+y^2+z^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\","
                             "\"plot_lower\":\"1e30-2\",\"plot_upper\":\"1e30+2\"}",
                    200u, "plot_svg", "data-mesh=\"true\"");
    expect_response("/eval", "{\"expression\":\"(x-1e30)^2+y^2=1\",\"wrt\":\"x\",\"action\":\"plot\","
                             "\"plot_source_mode\":\"equation\",\"plot_quality\":\"preview\","
                             "\"plot_lower\":\"1e30-2\",\"plot_upper\":\"1e30+2\","
                             "\"plot_scale\":\"manual\",\"plot_y_lower\":\"-2\",\"plot_y_upper\":\"2\"}",
                    200u, "plot_svg", "data-series");
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
        double minimum_height;
    } cases[] = {
        {"x^4-x^3-26x^2+4x+8", "0", "x", true, 0},
        {"t^2", "t+2", "t", true, 0},
        {"x^4", "0", "x", true, 0},
        {"sin(x)", "0", "x", true, 0},
        {"exp(x)", "2", "x", true, 1},
        {"sqrt(7^x)", "49", "x", true, 20},
        {"1/x", "0", "x", false, 0},
        {"x", "x+1", "x", false, 0}
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        expr_t *sides[] = {expr_from_string(cases[i].left, NULL), expr_from_string(cases[i].right, NULL)};
        char *sources[] = {expr_to_string(sides[0], style_EXPRESSION), expr_to_string(sides[1], style_EXPRESSION)};
        number_t bounds[] = {num_create_from_long(-10), num_create_from_long(10), NUM_NAN, NUM_NAN};
        bool found = lab_worker_plot_equation_frame((const expr_t *const *)sides, sources, cases[i].axis, true, bounds);
        ok = ok && found == cases[i].found;
        if (found)
            ok = ok && num_lt(bounds[0], bounds[1]) && num_lt(bounds[2], bounds[3]) &&
                 num_to_double(bounds[3]) - num_to_double(bounds[2]) > cases[i].minimum_height;
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

    bool microscopic = true;
    const unsigned zooms[] = {80, 512, 4096};
    for (unsigned i = 0; i < sizeof zooms / sizeof *zooms; ++i) {
        string_t *lower = string_sprintf("((0)+(2))/2-((2)-(0))/2*2^(-(%u))", zooms[i]);
        string_t *upper = string_sprintf("((0)+(2))/2+((2)-(0))/2*2^(-(%u))", zooms[i]);
        bool ordered = lower && upper && lab_worker_plot_bounds(string_c_str(lower), string_c_str(upper), &lo, &hi);
        microscopic = microscopic && ordered && num_lt(lo, hi) && num_to_double(lo) == 1 && num_to_double(hi) == 1 &&
                      lab_worker_plot_range_precision(lo, hi) > zooms[i];
        if (ordered && zooms[i] == 512) {
            samples = lab_worker_plot_samples("x-1", "x", lo, hi, NUM_NAN, NUM_NAN, &bits);
            microscopic = microscopic && samples && bits > 512 &&
                          num_lt(sampler_point(samples, 0)->y, NUM_ZERO) &&
                          num_gt(sampler_point(samples, sampler_count(samples)-1)->y, NUM_ZERO);
            sampler_free(samples);
        }
        num_destroy(&lo); num_destroy(&hi);
        string_free(lower); string_free(upper);
    }
    TEST_ASSERT_TRUE(microscopic, "native zoom recipes retain microscopic spans and samples around a nonzero centre");

    const char *names[] = {"x", "y", "z"};
    const char *limits[] = {"1e30-2", "1e30+2", "-2", "2", "-2", "2"};
    double box[6], point[] = {0.75, 0.5, 0.5};
    lab_worker_implicit_t *implicit = lab_worker_implicit_new("(x-1e30)^2+y^2+z^2=1", names, limits, 3, box);
    number_t left = lab_worker_implicit_coordinate(implicit, 0, 0.5);
    number_t right = lab_worker_implicit_coordinate(implicit, 0, 0.75);
    bool precise_mesh = implicit && box[0] == 0 && box[1] == 1 && num_lt(left, right) &&
                        fabs(lab_worker_implicit_eval(implicit, point)) < 1e-12 &&
                        num_get_default_prec_bits() == saved;
    if (!precise_mesh)
        fprintf(stderr, "implicit precision: context=%p box=%g,%g ordered=%d residual=%.17g precision=%zu/%zu\n",
                (void *)implicit, box[0], box[1], num_lt(left, right), lab_worker_implicit_eval(implicit, point),
                num_get_default_prec_bits(), saved);
    num_destroy(&right);
    num_destroy(&left);
    lab_worker_implicit_free(implicit);
    TEST_ASSERT_TRUE(precise_mesh, "implicit local frames preserve distinct vertices beyond double coordinate resolution");

    const char *ordinary_limits[] = {"-2", "2", "-2", "2"};
    implicit = lab_worker_implicit_new("exp(x)-1=y", names, ordinary_limits, 2, box);
    double small[] = {1e-20, 0};
    double residual = lab_worker_implicit_eval(implicit, small);
    TEST_ASSERT_TRUE(implicit && residual > 0.999999e-20 && residual < 1.000001e-20,
                     "uncertain implicit double residuals fall back to a freshly parsed guard-precision graph");
    lab_worker_implicit_free(implicit);
    const char *tiny_limits[] = {"1e-100", "2e-100", "0", "1e-99"};
    implicit = lab_worker_implicit_new("exp(x)-1=y", names, tiny_limits, 2, box);
    small[0] = 1e-100;
    residual = lab_worker_implicit_eval(implicit, small);
    TEST_ASSERT_TRUE(implicit && residual > 0.999999e-100 && residual < 1.000001e-100,
                     "small implicit viewports raise guard precision sufficiently to retain cancellation residuals");
    lab_worker_implicit_free(implicit);
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
    lab_eval_set(fields, "plot_components", lab_eval_text(fields, "plot_svg"));
    lab_eval_plot_title(fields);
    bool ok = contains(fields, "plot_svg", "data-plot-chart") && contains(fields, "plot_svg", "&lt;formula&gt;&amp;") &&
              !contains(fields, "plot_svg", "<formula>") && *lab_eval_text(fields, "plot_title_render_error") &&
              contains(fields, "plot_components", "&lt;formula&gt;&amp;") &&
              contains(fields, "plot_components", "data-plot-figure");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "failed title rendering preserves the curve, escapes the formula and records a diagnostic");

    const char *valid[] = {
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 500\" data-note=\"a > b\">"
        "<!--mars-native-chart-body--><text><![CDATA[a < b & c]]></text><svg><path d=\"M 0 0\"/></svg></svg>",
        "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:q=\"urn:plot-test\" viewBox=\"0 0 800 500\">"
        "<q:g q:label=\"x &amp; y\"><text>π &lt; τ</text></q:g></svg>",
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 500\"></svg>",
        "<?xml version=\"1.0\"?><!--prologue--><svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"0 0 800 500\"><g/></svg>\n",
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 500\"/>"
    };
    for (size_t i = 0; i < sizeof(valid) / sizeof(*valid); ++i) {
        fields = json_new_object();
        lab_eval_set(fields, "plot_svg", valid[i]);
        lab_eval_set(fields, "plot_title", "<formula>&");
        lab_eval_set(fields, "plot_title_TeX", "\\input{not-permitted}");
        lab_eval_plot_title(fields);
        const string_t *output = json_string_value(lab_eval_get(fields, "plot_svg"));
        xml_t *parsed = output ? xml_from_text(output) : NULL;
        TEST_ASSERT_TRUE(parsed && contains(fields, "plot_svg", "data-plot-chart"),
                         "title composition preserves valid quoted attributes, namespaces, nested SVG and prologues");
        if (!i)
            TEST_ASSERT_TRUE(contains(fields, "plot_svg", "<!--mars-native-chart-body--><text><![CDATA[a < b & c]]>"),
                             "original marker comments and CDATA remain verbatim chart content");
        xml_free(parsed);
        json_free(fields);
    }
    const char *invalid[] = {
        "<svg viewBox=\"0 0 800 500\"><g></svg>",
        "<svg viewBox=\"0 0 800 500\" viewBox=\"0 0 800 500\"></svg>",
        "<svg viewBox=\"0 0 800 500\"></svg><svg></svg>",
        "<!DOCTYPE svg [<!ENTITY x SYSTEM 'file:///not-read'>]><svg viewBox=\"0 0 800 500\">&x;</svg>"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        fields = json_new_object();
        lab_eval_set(fields, "plot_svg", invalid[i]);
        lab_eval_set(fields, "plot_title", "formula");
        lab_eval_set(fields, "plot_title_TeX", "\\input{not-permitted}");
        lab_eval_plot_title(fields);
        TEST_ASSERT_TRUE(!strcmp(lab_eval_text(fields, "plot_svg"), invalid[i]),
                         "invalid chart XML is not composed or accepted by the fast path");
        json_free(fields);
    }

    string_t *large = string_new_with("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 650\">"
                                       "<g data-series=\"0\">");
    const char *facet = "<path fill=\"#4495ab\" d=\"M 310.765 326.560 L 310.765 321.634 L 310.765 318.911 Z\" "
                        "data-uv=\"91645412 -0 -2.8480163e+10 -91645412 -0 2.8480163e+10\"/>\n";
    for (size_t i = 0; large && i < 13376; ++i)
        if (string_append_cstr(large, facet) < 0) {
            string_free(large);
            large = NULL;
        }
    fields = json_new_object();
    ok = large && string_append_cstr(large, "</g></svg>") == 0 &&
         lab_eval_set(fields, "plot_svg", string_c_str(large)) &&
         lab_eval_set(fields, "plot_title", "<formula>&") &&
         lab_eval_set(fields, "plot_title_TeX", "\\input{not-permitted}");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (ok)
        lab_eval_plot_title(fields);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;
    string_printf("Large plot title composition: %.6fs (%zu bytes)\n", elapsed, string_byte_length(large));
    TEST_ASSERT_TRUE(ok && contains(fields, "plot_svg", "data-plot-chart") &&
                     contains(fields, "plot_svg", "&lt;formula&gt;&amp;"),
                     "large surface retains chart metadata and escaped fallback title");
    const char *body_start = large ? strchr(string_c_str(large), '>') : NULL;
    const char *copied = body_start ? strstr(lab_eval_text(fields, "plot_svg"), "<g data-series=\"0\">") : NULL;
    size_t body_length = body_start ? string_byte_length(large) - (size_t)(body_start + 1 - string_c_str(large)) - 6 : 0;
    TEST_ASSERT_TRUE(copied && strlen(copied) >= body_length && !memcmp(copied, body_start + 1, body_length),
                     "all triangle geometry, hover metadata and whitespace are preserved byte for byte");
    json_free(fields);
    string_free(large);
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

static void test_lab_surface_slice_readme(void)
{
    /* README example in docs/mars-lab.md: an unset third parameter receives a plot-local slice. */
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"acos(sin(2-x*y)+z)\",\"action\":\"plot\"}", &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-surface") &&
              !strcmp(lab_eval_text(fields, "plot_slices"), "z\t-2\t2\t0\t500") &&
              contains(fields, "plot_title_TeX", "u(x,y) = ") && contains(fields, "plot_title", "z = 0") &&
              contains(fields, "expression", "z = ?");
    if (!ok)
        show_failure(fields);
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "README slice uses native limits and labels without changing worksheet bindings");
    string_printf("Surface slice ready: x,y axes; z = 0; slider limits: -2 to 2\n");
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

static void test_lab_screenshots_readme(void)
{
    /* README screenshot examples in docs/mars-lab.md: current surface and symbolic-result captures. */
    unsigned status = 0;
    json_t *fields = request("/eval", "{\"expression\":\"acos(sin(2-x*y))\",\"action\":\"plot\",\"wrt\":\"x\","
                            "\"plot_lower\":\"-3\",\"plot_upper\":\"3\",\"plot_second_lower\":\"-3\","
                            "\"plot_second_upper\":\"3\",\"plot_kind\":\"auto\"}", &status);
    bool ok = status == 200 && test_lab_ok(fields, true) && contains(fields, "plot_svg", "data-surface") &&
              contains(fields, "plot_title_TeX", "u(x,y) = ") && contains(fields, "plot_title_TeX", "\\sin");
    const char *surface = lab_eval_text(fields, "plot_svg");
    const char *lower = strstr(surface, "data-exact-z-min=\"");
    const char *upper = strstr(surface, "data-exact-z-max=\"");
    ok = ok && lower && upper && strtod(lower + strlen("data-exact-z-min=\""), NULL) < 0.02 &&
         strtod(upper + strlen("data-exact-z-max=\""), NULL) > 3.12;
    if (!ok) show_failure(fields);
    TEST_ASSERT_TRUE(ok, "README screenshot plots the principal inverse-cosine surface over [-3,3] squared");
    json_free(fields);
    string_printf("README Expression screenshot: u(x,y) = acos(sin(2-x*y)); x,y from -3 to 3\n");

    fields = request("/eval", "{\"expression\":\"(sqrt(x^2+y^2)-2)^2+z^2=1\",\"action\":\"plot\","
                             "\"wrt\":\"x\",\"plot_source_mode\":\"equation\",\"plot_scale\":\"auto\"}", &status);
    ok = status == 200 && test_lab_ok(fields, true) && closed_mesh(lab_eval_text(fields, "plot_svg"));
    if (!ok) show_failure(fields);
    TEST_ASSERT_TRUE(ok, "README screenshot automatic torus is a complete closed surface");
    json_free(fields);
    string_printf("README Equation screenshot: (sqrt(x^2+y^2)-2)^2+z^2=1; complete torus\n");

    fields = request("/diffequation-eval", "{\"diffequation\":\"y''+6y*y'+4y^3=0; y'(0)=0\"}", &status);
    ok = status == 200 && contains(fields, "solutions", "C₁") && !contains(fields, "solutions", "C₂") &&
         contains(fields, "solutions", "2C₁x");
    if (!ok) show_failure(fields);
    TEST_ASSERT_TRUE(ok, "README screenshot zero-slope Emden family retains one arbitrary constant");
    string_printf("README Differential Equation screenshot: %s\n", lab_eval_text(fields, "solutions"));
    json_free(fields);

    fields = request("/integrator-eval",
                     "{\"expression\":\"exp(-x^2/2)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"-inf\",\"hi\":\"2\"}]}",
                     &status);
    ok = status == 200 && contains(fields, "symbolic", "√(π/2)·(erf(√(2)) + 1)") &&
         !contains(fields, "symbolic", "∞");
    if (!ok) show_failure(fields);
    TEST_ASSERT_TRUE(ok, "README screenshot Gaussian integral evaluates its infinite endpoint");
    string_printf("README Integrator screenshot: %s\n", lab_eval_text(fields, "symbolic"));
    json_free(fields);

    json_t *observer = test_lab_json(
        "{\"date\":\"2026-10-25\",\"time\":\"12:00:00\",\"lat\":\"51.5085\",\"lon\":\"-0.1257\","
        "\"elevation\":\"25\",\"zone\":\"0\",\"jurisdiction\":\"GB-ENG\","
        "\"country\":\"England\",\"observer_surface\":\"Land\",\"sea_horizon\":\"false\","
        "\"town\":\"London|51.5085|-0.1257|25\",\"visibility\":\"all\",\"defer_events\":\"true\"}");
    lab_cal_observing_conditions(observer, NULL);
    string_t *diagnostic = NULL;
    json_t *arguments = json_new_object();
    static const char *const keys[] = {"date", "time", "lat", "lon", "elevation", "zone",
                                       "temperature_c", "pressure_hpa", "sea_horizon"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; ++i)
        lab_cal_set(arguments, keys[i], lab_cal_text(observer, keys[i]));
    json_t *snapshot = lab_cal_run("almanac_lab", arguments, 10000, &diagnostic);
    json_free(arguments);
    status = 200;
    fields = snapshot ? lab_cal_almanac(snapshot, observer, &status) : NULL;
    const json_t *summary = test_lab_member(fields, "observer_summary");
    const json_t *presentation = test_lab_member(test_lab_member(fields, "almanac_presentation"), "all");
    ok = status == 200 && test_lab_ok(fields, true) &&
         strstr(test_lab_text(json_array_get(summary, 0), "value"), "2026-10-25 12:00:00") &&
         strstr(test_lab_text(json_array_get(summary, 1), "value"), "London") &&
         !strcmp(test_lab_text(json_array_get(summary, 5), "value"), "25 m · Land") &&
         strstr(test_lab_text(presentation, "html"), "hPa (observer)");
    if (!ok) show_failure(fields);
    TEST_ASSERT_TRUE(ok, "README Almanac screenshot retains London's GMT moment, whole-metre altitude and pressure reference");
    string_printf("README Almanac screenshot: London; 2026-10-25 12:00:00 GMT; 25 m; estimated observer pressure\n");
    json_free(fields);
    json_free(snapshot);
    json_free(observer);
    string_free(diagnostic);
}

/* Run the documented plot inputs after the ordinary Lab regressions. */
void test_lab_plot_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_plot_title_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_surface_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_surface_slice_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_auto_range_readme, readme, NULL);
    TEST_RUN_IN_GROUP(test_lab_screenshots_readme, readme, NULL);
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

static void test_lab_extra_plot_availability(void)
{
    static const struct {
        const char *source;
        const char *available;
    } cases[] = {
        {"Laplace(asin(-x^2),x,s)", "no"},
        {"1+Laplace(asin(-x^2),x,s)", "no"},
        {"{Laplace(asin(-x^2),x,s)|s=2}", "no"},
        {"Laplace(exp(-x),x,s)", "yes"},
        {"Fourier(exp(-x^2),x,k)", "yes"},
        {"sin(x)", "yes"},
        {"ln(-x^3)", "yes"},
        {"asin(x-1000)", "yes"},
        {"integral(sin(t^t),t,0,x)", "yes"}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        json_t *payload = json_new_object();
        string_t *route = string_new_with("/eval");
        lab_eval_set(payload, "expression", cases[i].source);
        unsigned status = 0;
        json_t *fields = lab_eval_request(route, payload, &status);
        bool ok = status == 200 && !strcmp(lab_eval_text(fields, "plot_available"), cases[i].available);
        if (!ok)
            show_failure(fields);
        json_free(fields);
        string_free(route);
        json_free(payload);
        TEST_ASSERT_TRUE(ok, "native plot availability distinguishes unresolved operations from numerical domains");
    }
    expect_response("/eval", "{\"expression\":\"Laplace(asin(-x^2),x,s)\",\"action\":\"plot\",\"wrt\":\"s\"}",
                    422u, "plot_available", "no");
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

static void test_lab_extra_nested_root_equation(void)
{
    expect_response("/equation-eval", "{\"equation\":\"sqrt(7^x)=49\",\"precision\":97}",
                    200u, "solutions", "x = 4");
    expect_response("/equation-eval", "{\"equation\":\"sqrt(7^x)=-49\",\"precision\":97}",
                    200u, "status", "no solutions");
    expect_response("/function-run", "{\"source\":\"equation equ(x) { return equation(sqrt(7^x) = 49). } "
                                     "outputa(solve(equ(x))).\",\"precision\":97}", 200u, "output", "x = 4");
    unsigned status = 0;
    json_t *result = request("/eval", "{\"expression\":\"sqrt(7^x)=49\",\"wrt\":\"x\",\"precision\":97,"
                                     "\"action\":\"plot\",\"plot_source_mode\":\"equation\"}", &status);
    bool ok = status == 200 && contains(result, "plot_svg", "data-series=\"0\"") &&
              contains(result, "plot_svg", "data-series=\"1\"") && contains(result, "plot_solutions", "x = 4");
    if (!ok) show_failure(result);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "nested exponential roots solve and draw both equation sides at worksheet precision");
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
                   contains(trig, "solutions_TeX", "\\pm") &&
                   contains(trig, "svg", "<svg") && !*lab_eval_text(trig, "render_error") &&
                   json_array_size(bindings) == 3u;
    trig_ok = trig_ok && lab_presentation_adapt(trig);
    const char *trig_text = test_lab_text(test_lab_member(trig, "presentation"), "equation_solution_text");
    trig_ok = trig_ok && strstr(trig_text, "±") && !strstr(trig_text, "2n + 1");
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

static void test_lab_extra_gaussian_endpoint(void)
{
    unsigned status = 0;
    json_t *result = request("/integrator-eval",
        "{\"expression\":\"exp(-x^2/2)\",\"bounds\":[{\"name\":\"x\",\"lo\":\"-inf\",\"hi\":\"2\"}]}",
        &status);
    bool ok = status == 200 && contains(result, "tex", "\\sqrt{\\frac{\\pi}{2}}") &&
              contains(result, "tex", "\\operatorname{erf}(\\sqrt{2})") &&
              contains(result, "symbolic", "√(π/2)·(erf(√(2)) + 1)") &&
              contains(result, "antiderivative", "√(π/2)·erf(x/√(2))") &&
              contains(result, "value", "2.449602150638108") &&
              !contains(result, "symbolic", "∞");
    if (!ok) show_failure(result);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "Gaussian improper endpoint reduces to an exact error-function value");

    /* Exercise the same positive-radical cancellation with another scale and both endpoint signs. */
    const char *bounds[] = {"3", "-3"};
    const char *arguments[] = {"erf(√(3))", "erf(-√(3))"};
    for (size_t i = 0u; i < 2u; ++i) {
        string_t *payload = string_sprintf(
            "{\"expression\":\"exp(-x^2/3)\",\"precision\":78,"
            "\"bounds\":[{\"name\":\"x\",\"lo\":\"-inf\",\"hi\":\"%s\"}]}", bounds[i]);
        result = request("/integrator-eval", string_c_str(payload), &status);
        ok = status == 200 && contains(result, "symbolic", arguments[i]);
        if (!ok) show_failure(result);
        json_free(result);
        string_free(payload);
        TEST_ASSERT_TRUE(ok, "exact endpoint radical cancellation preserves the sign at higher precision");
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
    TEST_RUN_IN_GROUP(test_lab_extra_surface_refinement, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_slices, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_folded_laplace_plot, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_distribution_plot, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_complex_plot, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_complex_domain_gaps, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_complex_real_subset, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_height, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_surface_names, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_ranges, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_range_ownership, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_stepped_ranges, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_preview, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_series_domain, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_tangent_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_turning_points, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_solver_plots, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_mesh_budget, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_mesh_crossings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_mesh_closed, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_solution_families, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_equation_frame, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_nested_root_equation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_precision, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_title_fallback, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_native_records, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_editor_and_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_expression, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_help_examples, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_constant_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_extra_plot_availability, tests, NULL);
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
    TEST_RUN_IN_GROUP(test_lab_extra_gaussian_endpoint, tests, NULL);
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
