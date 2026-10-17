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
unsigned lab_precision_bits(double value, unsigned unit)
{
    if (!(value >= (unit ? 17 : 53)) || value > 262144 || unit > 1 || value != (unsigned)value)
        return 0;
    double converted = unit ? value * 3.32192809488736234787 : value;
    unsigned bits = (unsigned)converted;
    bits += converted > bits;
    return bits <= 262144 ? bits : 0;
}

static unsigned draft_bits(void)
{
    int panel = lab_dom_id("precisionCustomPanel");
    int text = lab_dom_clean(lab_dom_read(lab_dom_id("precisionCustomInput"), 5, ""), 0);
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
    return lab_precision_bits(value, (unsigned)lab_dom_number(lab_dom_get(panel, "precisionDraftUnit")));
}

static void draft_show(unsigned bits, unsigned unit)
{
    int panel = lab_dom_id("precisionCustomPanel");
    int text = lab_dom_format(unit ? lab_workspace_digits(bits) : bits, "", "");
    if (unit && bits == (unsigned)lab_dom_number(lab_dom_get(panel, "precisionDraftBits")) &&
        lab_dom_truth(lab_dom_get(panel, "precisionDraftDigits")))
        text = lab_dom_get(panel, "precisionDraftDigits");
    lab_dom_set(panel, "precisionDraftUnit", lab_dom_numeric(unit));
    lab_dom_set(panel, "precisionDraftBits", lab_dom_numeric(bits));
    lab_dom_set(panel, "precisionDraftText", text);
    lab_dom_write(lab_dom_id("precisionCustomInput"), 5, "", text);
    lab_dom_write(lab_dom_id("precisionCustomBits"), 2, "aria-pressed", lab_dom_string(unit ? "false" : "true"));
    lab_dom_write(lab_dom_id("precisionCustomDigits"), 2, "aria-pressed", lab_dom_string(unit ? "true" : "false"));
}

static void draft_preview(void)
{
    unsigned bits = draft_bits();
    int panel = lab_dom_id("precisionCustomPanel"), input = lab_dom_id("precisionCustomInput");
    int text = lab_dom_clean(lab_dom_read(input, 5, ""), 0);
    int changed = !lab_dom_equal(text, lab_dom_get(panel, "precisionDraftText"));
    int unit = lab_dom_truth(lab_dom_get(panel, "precisionDraftUnit"));
    if (bits && (unit || changed))
        lab_dom_set(panel, "precisionDraftDigits", unit ? text : lab_dom_format(lab_workspace_digits(bits), "", ""));
    lab_dom_set(panel, "precisionDraftBits", lab_dom_numeric(bits));
    lab_dom_set(panel, "precisionDraftText", text);
    lab_dom_write(lab_dom_id("precisionCustomPreview"), 0, "", bits ?
                  (unit ? lab_dom_format(bits, "Uses ", " bits") :
                          lab_dom_format(lab_workspace_digits(bits), "Displays ", " digits")) : lab_dom_string(""));
    lab_dom_write(lab_dom_id("precisionCustomError"), 0, "", lab_dom_string(unit ?
                  "Enter a whole number from 17 to 78913 digits." :
                  "Enter a whole number from 53 to 262144 bits."));
    lab_dom_class(lab_dom_id("precisionCustomError"), "hidden", bits != 0);
    lab_dom_write(input, 2, "aria-invalid", lab_dom_string(bits ? "false" : "true"));
    lab_dom_disabled(lab_dom_id("precisionCustomApply"), bits == 0);
}

/* Discarding a draft never changes worksheet precision. */
void lab_precision_close(int focus)
{
    lab_dom_class(lab_dom_id("precisionCustomPanel"), "hidden", 1);
    lab_dom_write(lab_dom_id("precisionCustomToggle"), 2, "aria-expanded", lab_dom_string("false"));
    if (focus)
        lab_dom_effect(lab_dom_id("precisionCustomToggle"), 0);
}

static void draft_apply(int calls)
{
    unsigned bits = draft_bits();
    draft_preview();
    if (!bits || lab_dom_truth(lab_dom_get(lab_dom_id("precisionCustomToggle"), "disabled")))
        return;
    lab_precision_close(1);
    if (bits == lab_workspace_requested_precision(lab_workspace_mode()))
        return;
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_set(call, "service", lab_dom_string("setWorksheetPrecision"));
    lab_dom_push(args, lab_dom_numeric(bits));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Only native-selected precision crosses into the existing asynchronous evaluation service. */
void lab_precision_dispatch(unsigned action, int event)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4), panel = lab_dom_id("precisionCustomPanel");
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    if (!panel)
        return;
    if (action == 0) {
        if (lab_dom_truth(lab_dom_get(lab_dom_id("precisionCustomToggle"), "disabled")))
            return;
        if (!lab_dom_has_class(panel, "hidden")) {
            lab_precision_close(1);
            return;
        }
        lab_dom_delete(panel, "precisionDraftDigits");
        draft_show(lab_workspace_requested_precision(lab_workspace_mode()), 1);
        draft_preview();
        lab_dom_class(panel, "hidden", 0);
        lab_dom_write(lab_dom_id("precisionCustomToggle"), 2, "aria-expanded", lab_dom_string("true"));
        lab_dom_effect(lab_dom_id("precisionCustomInput"), 0);
        return;
    }
    if (lab_dom_has_class(panel, "hidden"))
        return;
    if (action == 1) {
        lab_precision_close(1);
    } else if (action == 2) {
        draft_preview();
    } else if (action == 3 || action == 4) {
        unsigned bits = draft_bits();
        if (bits)
            draft_show(bits, action == 4);
        else {
            lab_dom_set(panel, "precisionDraftText", 0);
            lab_dom_set(panel, "precisionDraftUnit", lab_dom_numeric(action == 4));
            lab_dom_write(lab_dom_id("precisionCustomBits"), 2, "aria-pressed", lab_dom_string(action == 3 ? "true" : "false"));
            lab_dom_write(lab_dom_id("precisionCustomDigits"), 2, "aria-pressed", lab_dom_string(action == 4 ? "true" : "false"));
        }
        draft_preview();
    } else if (action == 5) {
        draft_apply(calls);
    } else if (action == 6) {
        int key = lab_dom_get(event, "key");
        int escape = lab_dom_equal(key, lab_dom_string("Escape"));
        int enter = lab_dom_equal(key, lab_dom_string("Enter")) &&
                    lab_dom_equal(lab_dom_get(event, "target"), lab_dom_id("precisionCustomInput"));
        if (escape || enter) {
            lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
            lab_dom_set(plan, "stop", lab_dom_scalar(1, 1));
            if (escape)
                lab_precision_close(1);
            else
                draft_apply(calls);
        }
    } else if (action == 7) {
        int target = lab_dom_get(event, "target");
        if (!lab_dom_closest(target, "#precisionCustomPanel, #precisionCustomToggle"))
            lab_precision_close(0);
    }
}

/* Bounded control registration; the browser deduplicates subscriptions across restoration. */
void lab_precision_install(int document)
{
    static const struct { const char *id, *event; unsigned action; } controls[] = {
        {"precisionCustomToggle", "click", 0}, {"precisionCustomClose", "click", 1},
        {"precisionCustomCancel", "click", 1}, {"precisionCustomInput", "input", 2},
        {"precisionCustomBits", "click", 3}, {"precisionCustomDigits", "click", 4},
        {"precisionCustomApply", "click", 5}
    };
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i)
        lab_dom_subscribe(lab_dom_id(controls[i].id), controls[i].event, "lab_precision_dispatch", controls[i].action, 0, 0);
    lab_dom_subscribe(document, "keydown", "lab_precision_dispatch", 6, 0, 0);
    lab_dom_subscribe(document, "pointerdown", "lab_precision_dispatch", 7, 0, 0);
}
