/**
 * @file lab_almanac_events.c
 * @brief Native subscription and visibility decisions for almanac worksheet actions.
 *
 * Registers actions on opaque native markup, preserves exact server copy text and
 * returns ordered browser service plans. Visibility state remains accessible through
 * a live browser object; listeners retain browser contexts, never WASM handles.
 * Browser adapters retain asynchronous request ownership and cancellation checks.
 * No astronomical data or mathematical markup is parsed, reconstructed or rewritten.
 */
#include "lab_dom.h"
#include "lab_result.h"
#include "lab_almanac_events.h"

static int lab_almanac_events_visibility(int value, int fallback)
{
    int text = lab_dom_clean(value, LAB_DOM_CLEAN_LOWER);
    return lab_dom_equal(text, lab_dom_string("all")) || lab_dom_equal(text, lab_dom_string("visible"))
               ? text
               : fallback;
}

static void lab_almanac_events_emit(int calls, const char *service, unsigned count, int first, int second)
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

static int lab_almanac_events_response(int data, int visibility)
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
void lab_almanac_events_install(int root, int state)
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
void lab_almanac_events_render(int target, int data, int state)
{
    int visibility = lab_almanac_events_visibility(lab_dom_get(data, "visibility"), lab_dom_get(state, "visibility"));
    int accepted = target && state && lab_result_almanac_variant(data, visibility);
    if (accepted) {
        int variant = lab_dom_key_get(lab_dom_get(data, "almanac_presentation"), visibility);
        lab_result_almanac_render(target, variant);
        lab_almanac_events_install(target, state);
    }
    lab_dom_return(accepted ? visibility : 0);
}

/* Forward the authored action button to the existing service. */
static void lab_almanac_events_totality(int button, int context, int calls)
{
    (void)context;
    lab_almanac_events_emit(calls, "applyAlmanacTotalityAction", 1, button, 0);
}

/* Save changed visibility before rendering cached data or requesting a fresh result. */
static void lab_almanac_events_change_visibility(int button, int context, int calls)
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
void lab_almanac_events_dispatch(lab_almanac_event_action_t action, int event, int context)
{
    /** @brief Action implementation receiving the button, live context and deferred calls. */
    typedef void (*lab_almanac_event_handler_t)(int, int, int);
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
