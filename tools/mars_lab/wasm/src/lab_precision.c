/**
 * @file lab_precision.c
 * @brief Custom-precision draft, unit conversion and accessible popover events.
 *
 * Keeps precision validation in native C and uses the existing bounded request
 * workflow only on Apply. Draft text is strictly decimal integer input. Exact
 * binary precision survives unit changes, including non-decimal saved settings.
 * The ordinary slider remains bounded independently of the custom-entry ceiling.
 */
#include "lab_dom.h"
#include "lab_workspace.h"
#include "lab_precision.h"

/* The existing persisted and transport precision ceiling; no allocation is attempted here. */
unsigned lab_precision_bits(double value, lab_precision_unit_t unit)
{
    if (!(value >= (unit ? 17 : 53)) || value > 262144 ||
        (unsigned)unit > LAB_PRECISION_UNIT_DIGITS || value != (unsigned)value)
        return 0;
    double converted = unit ? value * 3.32192809488736234787 : value;
    unsigned bits = (unsigned)converted;
    bits += converted > bits;
    return bits <= 262144 ? bits : 0;
}

static unsigned draft_bits(void)
{
    int panel = lab_dom_id("precisionCustomPanel");
    int text = lab_dom_clean(lab_dom_read(lab_dom_id("precisionCustomInput"), LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM);
    if (lab_dom_equal(text, lab_dom_get(panel, "precisionDraftText")))
        return (unsigned)lab_dom_number(lab_dom_get(panel, "precisionDraftBits"));
    unsigned length = lab_dom_length(text), value = 0;
    if (!length || length > 7)
        return 0;
    for (unsigned i = 0; i < length; ++i) {
        int digit = lab_dom_slice(text, i, i + 1);
        if (lab_dom_compare(digit, lab_dom_string("0")) < 0 || lab_dom_compare(digit, lab_dom_string("9")) > 0)
            return 0;
        value = value * 10 + (unsigned)lab_dom_to_number(digit);
    }
    lab_precision_unit_t unit =
        (lab_precision_unit_t)(unsigned)lab_dom_number(lab_dom_get(panel, "precisionDraftUnit"));
    return lab_precision_bits(value, unit);
}

static void draft_show(unsigned bits, lab_precision_unit_t unit)
{
    int panel = lab_dom_id("precisionCustomPanel");
    int text = lab_dom_format(unit ? lab_workspace_digits(bits) : bits, "", "");
    if (unit && bits == (unsigned)lab_dom_number(lab_dom_get(panel, "precisionDraftBits")) &&
        lab_dom_truth(lab_dom_get(panel, "precisionDraftDigits")))
        text = lab_dom_get(panel, "precisionDraftDigits");
    lab_dom_set(panel, "precisionDraftUnit", lab_dom_numeric(unit));
    lab_dom_set(panel, "precisionDraftBits", lab_dom_numeric(bits));
    lab_dom_set(panel, "precisionDraftText", text);
    lab_dom_write(lab_dom_id("precisionCustomInput"), LAB_DOM_VALUE, "", text);
    lab_dom_write(lab_dom_id("precisionCustomBits"), LAB_DOM_ATTRIBUTE, "aria-pressed",
                  lab_dom_string(unit ? "false" : "true"));
    lab_dom_write(lab_dom_id("precisionCustomDigits"), LAB_DOM_ATTRIBUTE, "aria-pressed",
                  lab_dom_string(unit ? "true" : "false"));
}

static void draft_preview(void)
{
    unsigned bits = draft_bits();
    int panel = lab_dom_id("precisionCustomPanel"), input = lab_dom_id("precisionCustomInput");
    int text = lab_dom_clean(lab_dom_read(input, LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM);
    int changed = !lab_dom_equal(text, lab_dom_get(panel, "precisionDraftText"));
    int unit = lab_dom_truth(lab_dom_get(panel, "precisionDraftUnit"));
    if (bits && (unit || changed))
        lab_dom_set(panel, "precisionDraftDigits", unit ? text : lab_dom_format(lab_workspace_digits(bits), "", ""));
    lab_dom_set(panel, "precisionDraftBits", lab_dom_numeric(bits));
    lab_dom_set(panel, "precisionDraftText", text);
    lab_dom_write(lab_dom_id("precisionCustomPreview"), LAB_DOM_TEXT, "", bits ?
                  (unit ? lab_dom_format(bits, "Uses ", " bits") :
                          lab_dom_format(lab_workspace_digits(bits), "Displays ", " digits")) : lab_dom_string(""));
    lab_dom_write(lab_dom_id("precisionCustomError"), LAB_DOM_TEXT, "", lab_dom_string(unit ?
                  "Enter a whole number from 17 to 78913 digits." :
                  "Enter a whole number from 53 to 262144 bits."));
    lab_dom_class(lab_dom_id("precisionCustomError"), "hidden", bits != 0);
    lab_dom_write(input, LAB_DOM_ATTRIBUTE, "aria-invalid", lab_dom_string(bits ? "false" : "true"));
    lab_dom_disabled(lab_dom_id("precisionCustomApply"), bits == 0);
}

/* Discarding a draft never changes worksheet precision. */
void lab_precision_close(bool focus)
{
    lab_dom_class(lab_dom_id("precisionCustomPanel"), "hidden", 1);
    lab_dom_write(lab_dom_id("precisionCustomToggle"), LAB_DOM_ATTRIBUTE, "aria-expanded", lab_dom_string("false"));
    if (focus)
        lab_dom_effect(lab_dom_id("precisionCustomToggle"), LAB_DOM_EFFECT_FOCUS);
}

static void draft_apply(browser_handle calls)
{
    unsigned bits = draft_bits();
    draft_preview();
    if (!bits || lab_dom_truth(lab_dom_get(lab_dom_id("precisionCustomToggle"), "disabled")))
        return;
    lab_precision_close(1);
    if (bits == lab_workspace_requested_precision(lab_workspace_mode()))
        return;
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(call, "service", lab_dom_string("setWorksheetPrecision"));
    lab_dom_push(args, lab_dom_numeric(bits));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/** @brief Borrowed handles for one precision action; never retained after dispatch. */
typedef struct {
    event_handle event;
    browser_handle plan, calls, panel;
} lab_precision_context_t;

/* Toggle visibility before applying the ordinary open-panel action guard. */
static void draft_toggle(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    int panel = context->panel;
    if (lab_dom_truth(lab_dom_get(lab_dom_id("precisionCustomToggle"), "disabled")))
        return;
    if (!lab_dom_has_class(panel, "hidden")) {
        lab_precision_close(1);
        return;
    }
    lab_dom_delete(panel, "precisionDraftDigits");
    draft_show(lab_workspace_requested_precision(lab_workspace_mode()), LAB_PRECISION_UNIT_DIGITS);
    draft_preview();
    lab_dom_class(panel, "hidden", 0);
    lab_dom_write(lab_dom_id("precisionCustomToggle"), LAB_DOM_ATTRIBUTE, "aria-expanded", lab_dom_string("true"));
    lab_dom_effect(lab_dom_id("precisionCustomInput"), LAB_DOM_EFFECT_FOCUS);
}

/* Close the visible draft and restore its trigger focus. */
static void draft_close(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    (void)context;
    lab_precision_close(1);
}

/* Refresh validation after an input event. */
static void draft_input(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    (void)context;
    draft_preview();
}

/* Preserve the exact draft bits while selecting either supported display unit. */
static void draft_select(lab_precision_action_t action, const lab_precision_context_t *context)
{
    int panel = context->panel;
    unsigned bits = draft_bits();
    lab_precision_unit_t unit = action == LAB_PRECISION_SELECT_DIGITS
                                    ? LAB_PRECISION_UNIT_DIGITS : LAB_PRECISION_UNIT_BITS;
    if (bits)
        draft_show(bits, unit);
    else {
        lab_dom_set(panel, "precisionDraftText", 0);
        lab_dom_set(panel, "precisionDraftUnit", lab_dom_numeric(unit));
        lab_dom_write(lab_dom_id("precisionCustomBits"), LAB_DOM_ATTRIBUTE, "aria-pressed",
                      lab_dom_string(unit ? "false" : "true"));
        lab_dom_write(lab_dom_id("precisionCustomDigits"), LAB_DOM_ATTRIBUTE, "aria-pressed",
                      lab_dom_string(unit ? "true" : "false"));
    }
    draft_preview();
}

/* Submit a validated draft through the current deferred service list. */
static void draft_submit(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    draft_apply(context->calls);
}

/* Only Escape and Enter on the draft input consume keyboard events. */
static void draft_key(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    int key = lab_dom_get(context->event, "key");
    int escape = lab_dom_equal(key, lab_dom_string("Escape"));
    int enter = lab_dom_equal(key, lab_dom_string("Enter")) &&
                lab_dom_equal(lab_dom_get(context->event, "target"), lab_dom_id("precisionCustomInput"));
    if (escape || enter) {
        lab_dom_set(context->plan, "prevent", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        lab_dom_set(context->plan, "stop", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        if (escape)
            lab_precision_close(1);
        else
            draft_apply(context->calls);
    }
}

/* Outside dismissal leaves focus with the pointer target. */
static void draft_outside(lab_precision_action_t action, const lab_precision_context_t *context)
{
    (void)action;
    int target = lab_dom_get(context->event, "target");
    if (!lab_dom_closest(target, "#precisionCustomPanel, #precisionCustomToggle"))
        lab_precision_close(0);
}

/* Only native-selected precision crosses into the existing asynchronous evaluation service. */
void lab_precision_dispatch(lab_precision_action_t action, event_handle event)
{
    /** @brief Precision action implementation sharing the current borrowed handles. */
    typedef void (*lab_precision_handler_t)(lab_precision_action_t, const lab_precision_context_t *);
    static const lab_precision_handler_t handlers[] = {
        [LAB_PRECISION_TOGGLE]        = draft_toggle,
        [LAB_PRECISION_CLOSE]         = draft_close,
        [LAB_PRECISION_PREVIEW]       = draft_input,
        [LAB_PRECISION_SELECT_BITS]   = draft_select,
        [LAB_PRECISION_SELECT_DIGITS] = draft_select,
        [LAB_PRECISION_APPLY]         = draft_submit,
        [LAB_PRECISION_KEY]           = draft_key,
        [LAB_PRECISION_OUTSIDE]       = draft_outside,
    };
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY),
        panel = lab_dom_id("precisionCustomPanel");
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    if (!panel)
        return;
    if (action != LAB_PRECISION_TOGGLE && lab_dom_has_class(panel, "hidden"))
        return;
    if ((unsigned)action >= sizeof handlers / sizeof *handlers || !handlers[action])
        return;
    const lab_precision_context_t context = {event, plan, calls, panel};
    handlers[action](action, &context);
}

/* Bounded control registration; the browser deduplicates subscriptions across restoration. */
void lab_precision_install(browser_handle document)
{
    static const struct {
        const char *id, *event;
        lab_precision_action_t action;
    } controls[] = {
        {"precisionCustomToggle", "click", LAB_PRECISION_TOGGLE},
        {"precisionCustomClose", "click", LAB_PRECISION_CLOSE},
        {"precisionCustomCancel", "click", LAB_PRECISION_CLOSE},
        {"precisionCustomInput", "input", LAB_PRECISION_PREVIEW},
        {"precisionCustomBits", "click", LAB_PRECISION_SELECT_BITS},
        {"precisionCustomDigits", "click", LAB_PRECISION_SELECT_DIGITS},
        {"precisionCustomApply", "click", LAB_PRECISION_APPLY},
    };
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i)
        lab_dom_subscribe(lab_dom_id(controls[i].id), controls[i].event, "lab_precision_dispatch",
                          controls[i].action, 0, 0);
    lab_dom_subscribe(document, "keydown", "lab_precision_dispatch", LAB_PRECISION_KEY, 0, 0);
    lab_dom_subscribe(document, "pointerdown", "lab_precision_dispatch", LAB_PRECISION_OUTSIDE, 0, 0);
}
