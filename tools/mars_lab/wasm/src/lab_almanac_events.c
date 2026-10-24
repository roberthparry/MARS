/**
 * @file lab_almanac_events.c
 * @brief Native subscription and visibility decisions for almanac worksheet actions.
 *
 * Registers actions on opaque native markup, preserves exact server copy text and
 * returns ordered browser service plans. Visibility state remains accessible through
 * a live browser object; listeners retain browser contexts, never WASM handles.
 * Browser adapters retain asynchronous request ownership and cancellation checks.
 * Also projects a pending observer summary from resident controls while calculations
 * run. No astronomical data or mathematical markup is parsed, reconstructed or rewritten.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_result.h"
#include "lab_almanac_events.h"

static jsref lab_almanac_preview_element(jsref parent, const char *tag, const char *class_name, jsref text)
{
    jsref element = lab_dom_create(tag);
    if (class_name)
        lab_dom_write(element, LAB_DOM_ATTRIBUTE, "class", lab_dom_string(class_name));
    if (text)
        lab_dom_write(element, LAB_DOM_TEXT, "", text);
    lab_dom_append(parent, element);
    return element;
}

static jsref lab_almanac_preview_value(const char *selector)
{
    return lab_dom_read(lab_dom_query(0, selector), LAB_DOM_VALUE, "");
}

/* Match the completed native observer summary, including minute carry and hemisphere at zero. */
static jsref lab_almanac_preview_angle(const char *selector, bool latitude)
{
    jsref input = lab_almanac_preview_value(selector);
    double angle = lab_dom_to_number(input);
    if (!lab_dom_length(input) || !isfinite(angle) || fabs(angle) > (latitude ? 90 : 180))
        return lab_dom_string("Unavailable");
    unsigned hundredths = (unsigned)floor(fabs(angle) * 6000 + 0.5), minutes = hundredths % 6000;
    char suffix[] = {' ', '0' + minutes / 1000, '0' + minutes / 100 % 10, '.',
                     '0' + minutes / 10 % 10, '0' + minutes % 10, 0};
    const char *hemisphere = latitude ? (signbit(angle) ? "′ S" : "′ N") : (signbit(angle) ? "′ W" : "′ E");
    jsref degrees = lab_dom_format(hundredths / 6000, "", "°");
    return lab_dom_join(lab_dom_join(degrees, 0, suffix), 0, hemisphere);
}

/* Display only resident observer information; never reuse the previous observer's calculated results. */
void lab_almanac_events_preview(jsref target, jsref country)
{
    if (!target)
        return;
    lab_dom_write(target, LAB_DOM_TEXT, "", lab_dom_string(""));
    lab_dom_write(target, LAB_DOM_DATASET, "copyText", lab_dom_string(""));
    jsref sheet = lab_almanac_preview_element(target, "div", "almanac-sheet", 0);
    jsref header = lab_almanac_preview_element(sheet, "div", "almanac-sheet-header", 0);
    lab_almanac_preview_element(header, "div", "almanac-sheet-title",
                                lab_dom_string("AstroNav Navigation Almanac"));
    jsref summary = lab_almanac_preview_element(header, "dl", "almanac-summary", 0);
    jsref town_select = lab_dom_query(0, "#almanacTown");
    jsref town = lab_dom_length(lab_dom_read(town_select, LAB_DOM_VALUE, "")) ?
                     lab_dom_read(lab_dom_selected(town_select), LAB_DOM_TEXT, "") : 0;
    jsref name = lab_dom_get(country, "name");
    jsref pending = lab_dom_string("Updating…");
    jsref location = lab_dom_length(name) ?
        (lab_dom_length(town) ? lab_dom_join(lab_dom_join(town, 0, ", "), name, "") : name) :
        (lab_dom_length(town) ? town : pending);
    jsref values[] = {
        lab_dom_join(lab_dom_join(lab_almanac_preview_value("#almanacDate"), 0, " "),
                     lab_almanac_preview_value("#almanacTime"), ""),
        location,
        lab_almanac_preview_value("#almanacZone"),
        lab_almanac_preview_angle("#almanacLatitude", true),
        lab_almanac_preview_angle("#almanacLongitude", false),
        lab_dom_join(lab_almanac_preview_value("#almanacElevation"), lab_dom_string(" m · Updating…"), ""),
        pending, pending, pending,
    };
    static const char *const labels[] = {"GMT moment", "Location", "Zone", "Latitude", "Longitude",
                                        "Observer altitude", "Temperature", "Pressure", "Weather source"};
    for (unsigned i = 0; i < sizeof values / sizeof *values; ++i) {
        jsref row = lab_almanac_preview_element(summary, "div", "almanac-summary-row", 0);
        lab_almanac_preview_element(row, "dt", 0, lab_dom_string(labels[i]));
        jsref value = lab_almanac_preview_element(row, "dd", 0, 0);
        jsref flag = lab_dom_get(country, "flag");
        if (i == 1 && lab_dom_starts_with(flag, "data:image/svg+xml;base64,")) {
            jsref icon = lab_almanac_preview_element(value, "img", "country-flag", 0);
            lab_dom_write(icon, LAB_DOM_ATTRIBUTE, "src", flag);
            lab_dom_write(icon, LAB_DOM_ATTRIBUTE, "alt", lab_dom_string(""));
        }
        lab_almanac_preview_element(value, "span", 0, values[i]);
    }
    jsref status = lab_almanac_preview_element(header, "div", "almanac-summary-corrections",
                                               lab_dom_string("Calculating for the selected observer…"));
    lab_dom_write(status, LAB_DOM_ATTRIBUTE, "role", lab_dom_string("status"));
}

static jsref lab_almanac_events_visibility(jsref value, jsref fallback)
{
    int text = lab_dom_clean(value, LAB_DOM_CLEAN_LOWER);
    return lab_dom_equal(text, lab_dom_string("all")) || lab_dom_equal(text, lab_dom_string("visible"))
               ? text
               : fallback;
}

static void lab_almanac_events_emit(jsref calls, const char *service, unsigned count, jsref first,
                                jsref second)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static jsref lab_almanac_events_response(jsref data, jsref visibility)
{
    int copy = lab_dom_object(LAB_VALUE_OBJECT), keys = lab_dom_keys(data);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        int key = lab_dom_item(keys, i);
        lab_dom_key_set(copy, key, lab_dom_key_get(data, key));
        lab_dom_release(mark);
    }
    lab_dom_set(copy, "visibility", visibility);
    return copy;
}

/* Register each button once, retaining ordinary browser context objects for live state access. */
void lab_almanac_events_install(jsref root, jsref state)
{
    int context = 0;
    if (root && state) {
        context = lab_dom_get(root, "__marsAlmanacEventContext");
        if (!context) {
            context = lab_dom_object(LAB_VALUE_OBJECT);
            lab_dom_set(context, "target", root);
            lab_dom_set(root, "__marsAlmanacEventContext", context);
        }
        lab_dom_set(context, "state", state);
    }
    static const char *const selectors[] = {
        [LAB_ALMANAC_EVENT_TOTALITY]   = "[data-almanac-use-totality]",
        [LAB_ALMANAC_EVENT_VISIBILITY] = "[data-almanac-visibility]",
    };
    unsigned mark = lab_dom_mark();
    for (lab_almanac_event_action_t action = LAB_ALMANAC_EVENT_TOTALITY;
         action <= (context ? LAB_ALMANAC_EVENT_VISIBILITY : LAB_ALMANAC_EVENT_TOTALITY); ++action) {
        int buttons = lab_dom_all(root, selectors[action]);
        unsigned button_mark = lab_dom_mark();
        for (unsigned i = 0; i < lab_dom_count(buttons); ++i) {
            lab_dom_subscribe(lab_dom_item(buttons, i), "click", "lab_almanac_events_dispatch", action,
                              action == LAB_ALMANAC_EVENT_VISIBILITY ? context : 0, 0);
            lab_dom_release(button_mark);
        }
        lab_dom_release(mark);
    }
}

/* Project validated markup and return visibility for the browser to commit after the scope closes. */
void lab_almanac_events_render(jsref target, jsref data, jsref state)
{
    int preferred = lab_dom_truth(lab_dom_get(data, "weather_refresh")) ? lab_dom_get(state, "visibility")
                                                                      : lab_dom_get(data, "visibility");
    int visibility = lab_almanac_events_visibility(preferred, lab_dom_get(state, "visibility"));
    int accepted = target && state && lab_result_almanac_variant(data, visibility);
    if (accepted) {
        int variant = lab_dom_key_get(lab_dom_get(data, "almanac_presentation"), visibility);
        lab_result_almanac_render(target, variant);
        lab_almanac_events_install(target, state);
    }
    lab_dom_return(accepted ? visibility : 0);
}

/* Forward the authored action button to the existing service. */
static void lab_almanac_events_totality(jsref button, jsref context, jsref calls)
{
    (void)context;
    lab_almanac_events_emit(calls, "applyAlmanacTotalityAction", 1, button, 0);
}

/* Save changed visibility before rendering cached data or requesting a fresh result. */
static void lab_almanac_events_change_visibility(jsref button, jsref context, jsref calls)
{
    if (!context)
        return;
    int state = lab_dom_get(context, "state"), current = lab_dom_get(state, "visibility");
    int next = lab_almanac_events_visibility(lab_dom_read(button, LAB_DOM_DATASET, "almanacVisibility"), current);
    if (state && !lab_dom_equal(current, next)) {
        lab_almanac_events_emit(calls, "setAlmanacVisibility", 1, next, 0);
        lab_almanac_events_emit(calls, "saveLastAlmanacState", 0, 0, 0);
        int data = lab_dom_get(state, "worksheet");
        if (lab_dom_truth(data)) {
            lab_almanac_events_emit(calls, "renderAlmanacWorksheet", 2, lab_dom_get(context, "target"),
                                    lab_almanac_events_response(data, next));
            lab_almanac_events_emit(calls, "refreshAlmanacLandTotality", 1, data, 0);
            lab_almanac_events_emit(calls, "setStatus", 1, lab_dom_string("Ready"), 0);
        } else {
            int options = lab_dom_object(LAB_VALUE_OBJECT);
            lab_dom_set(options, "skipHistoryUpdate", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
            lab_almanac_events_emit(calls, "evaluateAlmanac", 1, options, 0);
        }
    }
}

/* Select deferred services without assigning to the browser's getter-only live state. */
void lab_almanac_events_dispatch(lab_almanac_event_action_t action, event_jsref event, jsref context)
{
    /** @brief Action implementation receiving the button, live context and deferred calls. */
    typedef void (*lab_almanac_event_handler_t)(jsref, jsref, jsref);
    static const lab_almanac_event_handler_t handlers[] = {
        [LAB_ALMANAC_EVENT_TOTALITY]   = lab_almanac_events_totality,
        [LAB_ALMANAC_EVENT_VISIBILITY] = lab_almanac_events_change_visibility,
    };
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY),
        button = lab_dom_get(event, "currentTarget");
    lab_dom_set(plan, "calls", calls);
    if ((unsigned)action < sizeof handlers / sizeof *handlers && button && handlers[action])
        handlers[action](button, context, calls);
    lab_dom_return(plan);
}
