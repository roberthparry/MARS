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
#include "lab_payload.h"

typedef void (*lab_payload_builder_t)(int payload, int inputs, int request, double updated_at);

static int lab_payload_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_payload_control(const char *selector)
{
    return lab_dom_read(lab_dom_query(0, selector), 5, "");
}

static unsigned lab_payload_request_flags(int request)
{
    return lab_request_flags((unsigned)lab_dom_number(lab_dom_get(request, "channel")),
                             (uint32_t)lab_dom_number(lab_dom_get(request, "token")));
}

static void lab_payload_expression(int payload, int inputs, int request, double updated_at)
{
    lab_dom_set(payload, "expression", lab_dom_join(0, lab_dom_item(inputs, 0), ""));
    lab_dom_set(payload, "binding_source", lab_dom_join(0, lab_dom_item(inputs, 1), ""));
    lab_dom_set(payload, "binding_value", lab_dom_item(inputs, 2));
    lab_dom_set(payload, "wrt", lab_dom_item(inputs, 3));
    int action = lab_dom_item(inputs, 4);
    if (request) {
        unsigned channel = (unsigned)lab_dom_number(lab_dom_get(request, "channel"));
        uint32_t token = (uint32_t)lab_dom_number(lab_dom_get(request, "token"));
        action = lab_dom_string(lab_request_text(2, lab_request_action(channel, token)));
    }
    lab_dom_set(payload, "action", action);
    lab_dom_set(payload, "expression_updated_at", lab_dom_numeric(updated_at));
    int persist = request ? (lab_payload_request_flags(request) & 1) != 0
                          : lab_dom_equal(lab_dom_item(inputs, 5), lab_dom_string("expression"));
    lab_dom_set(payload, "persist_expression", lab_dom_scalar(1, persist));
}

static void lab_payload_matrix(int payload, int inputs, int request, double updated_at)
{
    (void)updated_at;
    int options = lab_dom_item(inputs, 0);
    int source =
        lab_payload_or(lab_dom_get(options, "matrixText"),
                       lab_payload_or(lab_dom_item(inputs, 1), lab_dom_clean(lab_payload_control("#expr"), 0)));
    lab_dom_set(payload, "matrix", lab_dom_clean(source, 0));
    lab_dom_set(payload, "operation",
                lab_payload_or(lab_dom_get(options, "operation"), lab_payload_control("#matrixOperation")));
    int operand = lab_dom_truth(lab_dom_item(inputs, 2)) ? lab_dom_get(options, "operand")
                                                         : lab_payload_control("#matrixOperand");
    lab_dom_set(payload, "operand", lab_dom_clean(operand, 0));
    int transient =
        request ? (lab_payload_request_flags(request) & 2) != 0 : lab_dom_truth(lab_dom_get(options, "skipSave"));
    lab_dom_set(payload, "transient", lab_dom_scalar(1, transient));
}

static int lab_payload_editor_source(int inputs)
{
    return lab_payload_or(lab_dom_item(inputs, 0), lab_dom_clean(lab_payload_control("#expr"), 0));
}

static void lab_payload_equation(int payload, int inputs, int request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "equation", lab_payload_editor_source(inputs));
}

static void lab_payload_diffequation(int payload, int inputs, int request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "diffequation", lab_payload_editor_source(inputs));
}

static void lab_payload_integrator(int payload, int inputs, int request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "expression", lab_dom_item(inputs, 0));
    lab_dom_set(payload, "bounds", lab_dom_item(inputs, 1));
    lab_dom_set(payload, "max_intervals", lab_dom_item(inputs, 2));
}

static void lab_payload_function(int payload, int inputs, int request, double updated_at)
{
    (void)request;
    (void)updated_at;
    lab_dom_set(payload, "source", lab_dom_item(inputs, 0));
}

static void lab_payload_weather(int payload, int inputs, int request, double updated_at)
{
    (void)request;
    (void)updated_at;
    static const char *const fields[] = {"date", "latitude", "longitude"};
    int state = lab_dom_item(inputs, 0);
    for (unsigned field = 0; field < sizeof fields / sizeof *fields; ++field)
        lab_dom_set(payload, fields[field], lab_dom_get(state, fields[field]));
}

/* Construct a wire record; positional inputs contain opaque caller values, including boxed numbers. */
void lab_payload_build(unsigned kind, int inputs, int request, double precision, double updated_at)
{
    static const lab_payload_builder_t builders[] = {
        lab_payload_expression, lab_payload_matrix,   lab_payload_equation, lab_payload_diffequation,
        lab_payload_integrator, lab_payload_function, lab_payload_weather};
    if (kind >= sizeof builders / sizeof *builders) {
        lab_dom_return(0);
        return;
    }
    int payload = lab_dom_object(5);
    builders[kind](payload, inputs, lab_dom_truth(request) ? request : 0, updated_at);
    if (kind != 6)
        lab_dom_set(payload, "precision", lab_dom_numeric(precision));
    lab_dom_return(payload);
}

/* Check every active bound without parsing or rewriting its mathematical contents. */
void lab_payload_bounds_error(int bounds)
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
void lab_payload_land(int data, int cells, int config)
{
    static const struct {
        const char *key, *selector, *fallback;
    } fields[] = {{"jurisdiction", "#almanacJurisdiction", "DEFAULT_DATETIME_JURISDICTION"},
                  {"zone", "#almanacZone", "DEFAULT_ALMANAC_ZONE"},
                  {"latitude", "#almanacLatitude", "DEFAULT_ALMANAC_LATITUDE"},
                  {"longitude", "#almanacLongitude", "DEFAULT_ALMANAC_LONGITUDE"}};
    int payload = lab_dom_object(5), native_fields = lab_dom_get(data, "fields");
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
    int events = lab_dom_object(4);
    lab_dom_set(payload, "events", events);
    unsigned count = lab_dom_count(cells);
    mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int event = lab_dom_object(5), cell = lab_dom_item(cells, index);
        lab_dom_set(event, "jd", lab_dom_clean(lab_dom_read(cell, 3, "almanacLandTotality"), 0));
        lab_dom_push(events, event);
        lab_dom_release(mark);
    }
    lab_dom_return(payload);
}

/* Apply an already-current reply, indexing exact identifiers once and keeping native HTML intact. */
void lab_payload_land_apply(int cells, int payload, unsigned failure)
{
    const char *fallback = failure == 2 ? "Nearest land totality unavailable"
                           : failure || lab_dom_truth(lab_dom_get(payload, "timed_out"))
                               ? "Nearest land totality search timed out"
                               : "No land totality found";
    int lookup = lab_dom_map_new(), items = lab_dom_get(payload, "items");
    unsigned count = failure ? 0 : lab_dom_count(items), mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int item = lab_dom_item(items, index);
        lab_dom_map_set(lookup, lab_dom_clean(lab_dom_get(item, "jd"), 0), item);
        lab_dom_release(mark);
    }
    int missing = lab_dom_string(fallback);
    count = lab_dom_count(cells);
    mark = lab_dom_mark();
    for (unsigned index = 0; index < count; ++index) {
        int cell = lab_dom_item(cells, index);
        int key = lab_dom_clean(lab_dom_read(cell, 3, "almanacLandTotality"), 0);
        int html = lab_dom_get(lab_dom_map_get(lookup, key), "html");
        int found = !failure && lab_dom_type(html) == 3;
        lab_dom_write(cell, found ? 1 : 0, "", found ? html : missing);
        lab_dom_release(mark);
    }
}

/* Project the server's access description and QR markup without interpreting either. */
void lab_payload_mobile(int data)
{
    lab_dom_class(lab_dom_query(0, "#mobileAccess"), "hidden", 0);
    lab_dom_write(lab_dom_query(0, "#mobileTitle"), 0, "",
                  lab_payload_or(lab_dom_get(data, "title"), lab_dom_string("Mobile access")));
    lab_dom_write(lab_dom_query(0, "#mobileHint"), 0, "", lab_dom_get(data, "hint"));
    int url = lab_dom_join(0, lab_dom_get(data, "url"), "");
    lab_dom_write(lab_dom_query(0, "#mobileUrl"), 0, "", lab_payload_or(url, lab_dom_string("Unavailable")));
    lab_dom_write(lab_dom_query(0, "#mobileQr"), 1, "", lab_dom_get(data, "qr"));
}
