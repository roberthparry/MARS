/**
 * @file lab_payload.c
 * @brief Request payload construction and background reply application for MARS Lab.
 *
 * Selects evaluation fields, persistence flags and control defaults, validates
 * structured integration bounds, and projects native land-totality and mobile
 * records. Mathematical source and native metadata remain opaque. JavaScript
 * retains fetch, promises, cancellation and freshness checks across awaits.
 * All browser handles are scoped to one synchronous call. Event lists are
 * traversed once to construct payloads or an exact-key reply index; temporary
 * handles are released per entry so list size does not exhaust the handle table.
 */
#include "lab_dom.h"
#include "lab_requests.h"
#include "lab_plot_controls.h"
#include "lab_plot_slices.h"
#include "lab_payload.h"

/**
 * @brief Populate mode-specific fields of an evaluation or auxiliary request payload.
 * @param payload Scoped handle for the output object whose fields the builder fills in place.
 * @param inputs Scoped array of positional source values and options for the selected request kind.
 * @param request Scoped request-ownership record supplying action and persistence flags, or zero when absent.
 * @param updated_at Expression modification timestamp copied to expression_updated_at; unused by other builders.
 */
typedef void (*lab_payload_builder_t)(jsref payload, jsref inputs, jsref request, double updated_at);

static jsref lab_payload_or(jsref value, jsref fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static jsref lab_payload_control(const char *selector)
{
    int control = lab_dom_query(0, selector);
    int text = lab_dom_read(control, LAB_DOM_VALUE, "");
    int exact = lab_dom_get(control, "marsZoomValue");
    return lab_dom_truth(exact) && lab_dom_equal(text, lab_dom_get(control, "marsZoomText")) ? exact : text;
}

static unsigned lab_payload_request_flags(jsref request)
{
    return lab_request_flags((unsigned)lab_dom_number(lab_dom_get(request, "channel")),
                             (uint32_t)lab_dom_number(lab_dom_get(request, "token")));
}

static void lab_payload_expression(jsref payload, jsref inputs, jsref request, double updated_at)
{
    lab_dom_set(payload, "expression", lab_dom_join(0, lab_dom_item(inputs, 0), ""));
    lab_dom_set(payload, "binding_source", lab_dom_join(0, lab_dom_item(inputs, 1), ""));
    lab_dom_set(payload, "binding_value", lab_dom_item(inputs, 2));
    lab_dom_set(payload, "wrt", lab_dom_item(inputs, 3));
    int action = lab_dom_item(inputs, 4);
    if (request) {
        unsigned channel = (unsigned)lab_dom_number(lab_dom_get(request, "channel"));
        uint32_t token = (uint32_t)lab_dom_number(lab_dom_get(request, "token"));
        action = lab_dom_string(lab_request_text(LAB_REQUEST_CATALOGUE_ACTIONS, lab_request_action(channel, token)));
    }
    lab_dom_set(payload, "action", action);
    if (lab_dom_equal(action, lab_dom_string("plot"))) {
        int axis = lab_dom_item(inputs, 3);
        lab_dom_set(payload, "plot_source_mode", lab_dom_item(inputs, 5));
        if (lab_dom_equal(lab_dom_item(inputs, 5), lab_dom_string("integrator"))) {
            lab_dom_set(payload, "bounds", lab_dom_item(inputs, 7));
            int rows = lab_dom_item(inputs, 7), names = lab_dom_object(LAB_VALUE_ARRAY);
            for (unsigned i = 0; i < lab_dom_count(rows); ++i)
                lab_dom_push(names, lab_dom_get(lab_dom_item(rows, i), "name"));
            lab_plot_controls_variables(names, names);
            axis = lab_dom_item(names, 0);
            lab_dom_set(payload, "wrt", axis);
        }
        lab_plot_controls_prepare(axis);
        int slices = lab_plot_slices_payload();
        lab_dom_set(payload, "plot_slices", slices);
        if (request)
            lab_dom_set(request, "sliceInput", slices);
        lab_dom_class(lab_dom_id("plotCard"), "hidden", 0);
        lab_dom_set(payload, "plot_lower", lab_payload_control("#plotLower"));
        lab_dom_set(payload, "plot_upper", lab_payload_control("#plotUpper"));
        lab_dom_set(payload, "plot_kind", lab_payload_control("#plotKind"));
        if (request && lab_dom_equal(lab_dom_get(request, "plotKind"), lab_dom_string("complex_curve")))
            lab_dom_set(payload, "plot_kind", lab_dom_string("complex_curve"));
        lab_dom_set(payload, "plot_second", lab_payload_control("#plotSecond"));
        lab_dom_set(payload, "plot_second_lower", lab_payload_control("#plotSecondLower"));
        lab_dom_set(payload, "plot_second_upper", lab_payload_control("#plotSecondUpper"));
        lab_dom_set(payload, "plot_azimuth", lab_payload_control("#plotAzimuth"));
        /* Sampling still carries a legacy SVG fallback camera; free rotation lives in the client viewport. */
        double elevation = lab_dom_to_number(lab_payload_control("#plotElevation"));
        lab_dom_set(payload, "plot_elevation", lab_dom_format(elevation < 5 ? 5 : elevation > 85 ? 85 : elevation,
                                                              "", ""));
        lab_dom_set(payload, "plot_roll", lab_payload_control("#plotRoll"));
        lab_dom_set(payload, "plot_quality", lab_dom_string(lab_dom_truth(lab_dom_item(inputs, 6)) ? "preview" : "full"));
        lab_dom_write(lab_dom_query(0, "#plotRange"), LAB_DOM_DATASET, "rotationVariable", axis);
        lab_dom_set(payload, "plot_scale", lab_payload_control("#plotScale"));
        lab_dom_set(payload, "plot_y_lower", lab_payload_control("#plotYLower"));
        lab_dom_set(payload, "plot_y_upper", lab_payload_control("#plotYUpper"));
        int plane = lab_dom_get(lab_dom_id("plotRendered"), "marsComplexPlaneBounds");
        static const char *const plane_keys[] = {"plot_plane_real_lower", "plot_plane_real_upper",
                                                "plot_plane_imag_lower", "plot_plane_imag_upper"};
        for (unsigned i = 0; i < 4; ++i)
            lab_dom_set(payload, plane_keys[i], lab_dom_join(0, lab_dom_item(plane, i), ""));
        int mode = lab_dom_item(inputs, 5);
        if (lab_dom_equal(mode, lab_dom_string("equation")) || lab_dom_equal(mode, lab_dom_string("diffequation"))) {
            double dimensions = lab_dom_to_number(lab_dom_get(lab_dom_id("plotRange"), "marsPlotVariables"));
            int pde = (lab_dom_equal(mode, lab_dom_string("diffequation")) && dimensions == 2) ||
                      (lab_dom_equal(mode, lab_dom_string("equation")) && dimensions == 3);
            lab_dom_set(payload, "plot_kind", lab_dom_string(pde ? "surface" : "curve"));
            if (!pde)
                lab_dom_set(payload, "plot_second", lab_dom_string(""));
        }
    }
    lab_dom_set(payload, "expression_updated_at", lab_dom_numeric(updated_at));
    int persist = request ? (lab_payload_request_flags(request) & 1) != 0
                          : lab_dom_equal(lab_dom_item(inputs, 5), lab_dom_string("expression"));
    lab_dom_set(payload, "persist_expression", lab_dom_scalar(LAB_VALUE_BOOLEAN, persist));
}

static void lab_payload_matrix(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)updated_at;
    int options = lab_dom_item(inputs, 0);
    int source =
        lab_payload_or(lab_dom_get(options, "matrixText"),
                       lab_payload_or(lab_dom_item(inputs, 1), lab_dom_clean(lab_payload_control("#expr"), LAB_DOM_CLEAN_TRIM)));
    lab_dom_set(payload, "matrix", lab_dom_clean(source, LAB_DOM_CLEAN_TRIM));
    lab_dom_set(payload, "operation",
                lab_payload_or(lab_dom_get(options, "operation"), lab_payload_control("#matrixOperation")));
    int operand = lab_dom_truth(lab_dom_item(inputs, 2)) ? lab_dom_get(options, "operand")
                                                         : lab_payload_control("#matrixOperand");
    lab_dom_set(payload, "operand", lab_dom_clean(operand, LAB_DOM_CLEAN_TRIM));
    int transient =
        request ? (lab_payload_request_flags(request) & 2) != 0 : lab_dom_truth(lab_dom_get(options, "skipSave"));
    lab_dom_set(payload, "transient", lab_dom_scalar(LAB_VALUE_BOOLEAN, transient));
}

static jsref lab_payload_editor_source(jsref inputs)
{
    return lab_payload_or(lab_dom_item(inputs, 0), lab_dom_clean(lab_payload_control("#expr"), LAB_DOM_CLEAN_TRIM));
}

static void lab_payload_equation(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "equation", lab_payload_editor_source(inputs));
}

static void lab_payload_diffequation(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "diffequation", lab_payload_editor_source(inputs));
}

static void lab_payload_integrator(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "expression", lab_dom_item(inputs, 0));
    lab_dom_set(payload, "bounds", lab_dom_item(inputs, 1));
    lab_dom_set(payload, "max_intervals", lab_dom_item(inputs, 2));
}

static void lab_payload_function(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "source", lab_dom_item(inputs, 0));
}

static void lab_payload_weather(jsref payload, jsref inputs, jsref request, double updated_at)
{
    (void)request;
    (void)updated_at;
    static const char *const fields[] = {"date", "latitude", "longitude"};
    int state = lab_dom_item(inputs, 0);
    for (unsigned field = 0; field < sizeof fields / sizeof *fields; ++field)
        lab_dom_set(payload, fields[field], lab_dom_get(state, fields[field]));
}

/* Construct a wire record; positional inputs contain opaque caller values, including boxed numbers. */
void lab_payload_build(lab_payload_kind_t kind, jsref inputs, jsref request, double precision,
                       double updated_at)
{
    static const lab_payload_builder_t builders[] = {
        [LAB_PAYLOAD_EXPRESSION]            = lab_payload_expression,
        [LAB_PAYLOAD_MATRIX]                = lab_payload_matrix,
        [LAB_PAYLOAD_EQUATION]              = lab_payload_equation,
        [LAB_PAYLOAD_DIFFERENTIAL_EQUATION] = lab_payload_diffequation,
        [LAB_PAYLOAD_INTEGRATOR]            = lab_payload_integrator,
        [LAB_PAYLOAD_FUNCTION]              = lab_payload_function,
        [LAB_PAYLOAD_WEATHER]               = lab_payload_weather,
    };
    if ((unsigned)kind >= sizeof builders / sizeof *builders) {
        lab_dom_return(0);
        return;
    }
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    builders[kind](payload, inputs, lab_dom_truth(request) ? request : 0, updated_at);
    if (kind != LAB_PAYLOAD_WEATHER)
        lab_dom_set(payload, "precision", lab_dom_numeric(precision));
    lab_dom_return(payload);
}

/* Check every active bound without parsing or rewriting its mathematical contents. */
void lab_payload_bounds_error(jsref bounds)
{
    unsigned count = lab_dom_count(bounds), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int bound = lab_dom_item(bounds, index);
        if (lab_dom_truth(lab_dom_get(bound, "lo")) && !lab_dom_truth(lab_dom_get(bound, "hi"))) {
            int message =
                lab_dom_join(lab_dom_string("A one-sided bound for "), lab_dom_get(bound, "name"),
                             " should be entered as an upper bound. Leave lower blank and put the value in upper.");
            lab_dom_return(message);
            return;
        }
        lab_dom_release(mark);
    }
    lab_dom_return(0);
}

/* Snapshot native event identifiers and resolve only request-field fallbacks. */
void lab_payload_land(jsref data, jsref cells, jsref config)
{
    static const struct {
        const char *key, *selector, *fallback;
    } fields[] = {{"jurisdiction", "#almanacJurisdiction", "DEFAULT_DATETIME_JURISDICTION"},
                  {"zone", "#almanacZone", "DEFAULT_ALMANAC_ZONE"},
                  {"latitude", "#almanacLatitude", "DEFAULT_ALMANAC_LATITUDE"},
                  {"longitude", "#almanacLongitude", "DEFAULT_ALMANAC_LONGITUDE"}};
    int payload = lab_dom_object(LAB_VALUE_OBJECT), native_fields = lab_dom_get(data, "fields");
    lab_dom_set(payload, "event_year",
                lab_payload_or(lab_dom_get(data, "event_year"),
                               lab_payload_or(lab_dom_get(native_fields, "event_year"), lab_dom_string(""))));
    unsigned mark = lab_dom_mark();
    for (unsigned field = 0; field < sizeof fields / sizeof *fields; ++field) {
        int value = lab_payload_or(
            lab_dom_get(native_fields, fields[field].key),
            lab_payload_or(lab_payload_control(fields[field].selector), lab_dom_get(config, fields[field].fallback)));
        lab_dom_set(payload, fields[field].key, value);
        lab_dom_release(mark);
    }
    int events = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(payload, "events", events);
    unsigned count = lab_dom_count(cells);
    mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int event = lab_dom_object(LAB_VALUE_OBJECT), cell = lab_dom_item(cells, index);
        lab_dom_set(event, "jd", lab_dom_clean(lab_dom_read(cell, LAB_DOM_DATASET, "almanacLandTotality"), LAB_DOM_CLEAN_TRIM));
        lab_dom_push(events, event);
        lab_dom_release(mark);
    }
    lab_dom_return(payload);
}

/* Apply an already-current reply, indexing exact identifiers once and keeping native HTML intact. */
void lab_payload_land_apply(jsref cells, jsref payload, unsigned failure)
{
    const char *fallback = failure == 2 ? "Nearest land totality unavailable"
                           : failure || lab_dom_truth(lab_dom_get(payload, "timed_out"))
                               ? "Nearest land totality search timed out"
                               : "No land totality found";
    int lookup = lab_dom_map_new(), items = lab_dom_get(payload, "items");
    unsigned count = failure ? 0 : lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int item = lab_dom_item(items, index);
        lab_dom_map_set(lookup, lab_dom_clean(lab_dom_get(item, "jd"), LAB_DOM_CLEAN_TRIM), item);
        lab_dom_release(mark);
    }
    int missing = lab_dom_string(fallback);
    count = lab_dom_count(cells);
    mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int cell = lab_dom_item(cells, index);
        int key = lab_dom_clean(lab_dom_read(cell, LAB_DOM_DATASET, "almanacLandTotality"), LAB_DOM_CLEAN_TRIM);
        int html = lab_dom_get(lab_dom_map_get(lookup, key), "html");
        int found = !failure && lab_dom_type(html) == LAB_VALUE_STRING;
        lab_dom_write(cell, found ? LAB_DOM_HTML : LAB_DOM_TEXT, "", found ? html : missing);
        lab_dom_release(mark);
    }
}

/* Project the server's access description and QR markup without interpreting either. */
void lab_payload_mobile(jsref data)
{
    lab_dom_class(lab_dom_query(0, "#mobileAccess"), "hidden", 0);
    lab_dom_write(lab_dom_query(0, "#mobileTitle"), LAB_DOM_TEXT, "",
                  lab_payload_or(lab_dom_get(data, "title"), lab_dom_string("Mobile access")));
    lab_dom_write(lab_dom_query(0, "#mobileHint"), LAB_DOM_TEXT, "", lab_dom_get(data, "hint"));
    int url = lab_dom_join(0, lab_dom_get(data, "url"), "");
    lab_dom_write(lab_dom_query(0, "#mobileUrl"), LAB_DOM_TEXT, "", lab_payload_or(url, lab_dom_string("Unavailable")));
    lab_dom_write(lab_dom_query(0, "#mobileQr"), LAB_DOM_HTML, "", lab_dom_get(data, "qr"));
}
