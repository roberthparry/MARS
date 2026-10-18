/**
 * @file lab_tooltip.c
 * @brief Accessible control hints and their DOM projection for the WASM client.
 *
 * Owns label precedence, authored-title caching, description restoration and
 * viewport placement. The host retains only the active node for event delivery,
 * including restoration after detachment. No scoped DOM handles survive a call.
 * Browser primitives supply Unicode casing/whitespace and measured rectangles;
 * this module never interprets mathematical content.
 */
#include "lab_dom.h"
#include "lab_view.h"
#include "lab_tooltip.h"

static const struct {
    const char *id;
    const char *text;
} lab_tooltip_labels[] = {
    {"back", "Return to the previous input in this mode"},
    {"clear", "Clear the current input and its results"},
    {"forward", "Move to the next input in this mode"},
    {"goalSeek", "Find a variable value that reaches the requested target"},
    {"help", "Show or hide help for the current mode"},
    {"inputCopy", "Copy the current input"},
    {"marsDatePickerClose", "Close the date picker"},
    {"marsDatePickerToday", "Use the current date"},
    {"precisionSlider", "Choose calculation precision; release to recalculate"},
    {"resultUseInput", "Put this result into the current mode as a new input"},
    {"run", "Evaluate the current input"},
};

static jsref lab_tooltip_label(jsref button)
{
    int id = lab_dom_read(button, LAB_DOM_ATTRIBUTE, "id");
    unsigned low = 0, high = sizeof(lab_tooltip_labels) / sizeof(*lab_tooltip_labels);
    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        int order = lab_dom_compare(id, lab_dom_string(lab_tooltip_labels[mid].id));
        if (!order)
            return lab_dom_string(lab_tooltip_labels[mid].text);
        if (order < 0)
            high = mid;
        else
            low = mid + 1;
    }
    return 0;
}

static jsref lab_tooltip_card(jsref button)
{
    int card = lab_dom_closest(button, ".result-card");
    int title = card ? lab_dom_query(card, ".card-title > span:first-child") : 0;
    int text = lab_dom_clean(lab_dom_read(title, LAB_DOM_TEXT, ""), LAB_DOM_CLEAN_LOWER);
    return lab_dom_length(text) ? text : lab_dom_string("result");
}

static jsref lab_tooltip_text(jsref button)
{
    int title = lab_dom_clean(lab_dom_read(button, LAB_DOM_ATTRIBUTE, "title"), LAB_DOM_CLEAN_TRIM);
    if (lab_dom_has(button, "title")) {
        if (lab_dom_length(title))
            lab_dom_write(button, LAB_DOM_DATASET, "marsTooltipTitle", title);
        else
            lab_dom_remove(button, "data-mars-tooltip-title");
    }
    title = lab_dom_clean(lab_dom_read(button, LAB_DOM_DATASET, "marsTooltipTitle"), LAB_DOM_CLEAN_TRIM);
    if (lab_dom_length(title))
        return title;
    int label = lab_tooltip_label(button);
    if (label)
        return label;
    int text = lab_dom_clean(lab_dom_read(button, LAB_DOM_TEXT, ""), LAB_DOM_CLEAN_TRIM);
    if (lab_dom_has_class(button, "mode-tab"))
        return lab_dom_join(lab_dom_string("Switch to "), text, " mode");
    if (lab_dom_has(button, "data-expand-card"))
        return lab_dom_join(lab_dom_join(text, lab_dom_string(" the "), ""), lab_tooltip_card(button), " card");
    if (lab_dom_has_class(button, "more-digits"))
        return lab_dom_join(lab_dom_string("Show the full value in the "), lab_tooltip_card(button), " card");
    if (lab_dom_equal(lab_dom_read(button, LAB_DOM_DATASET, "copyTarget"), lab_dom_string("mobile")))
        return lab_dom_string("Copy the private mobile-access URL");
    if (lab_dom_has_class(button, "copy-result"))
        return lab_dom_join(lab_dom_string("Copy the "), lab_tooltip_card(button), "");
    if (lab_dom_has_class(button, "variable-copy"))
        return lab_dom_string("Copy this binding value");
    if (lab_dom_has_class(button, "select-button"))
        return lab_dom_join(lab_dom_string("Choose "), lab_dom_length(text) ? text : lab_dom_string("an option"), "");
    label = lab_dom_clean(lab_dom_read(button, LAB_DOM_ATTRIBUTE, "aria-label"), LAB_DOM_CLEAN_TRIM);
    if (lab_dom_length(label))
        return label;
    text = lab_dom_clean(text, LAB_DOM_CLEAN_COLLAPSE);
    return lab_dom_length(text) ? text : lab_dom_string("Activate this control");
}

static jsref lab_tooltip_node(void)
{
    int node = lab_dom_query(0, "#marsButtonTooltip");
    if (!node) {
        node = lab_dom_create("div");
        lab_dom_class(node, "mars-button-tooltip", 1);
        lab_dom_write(node, LAB_DOM_ATTRIBUTE, "id", lab_dom_string("marsButtonTooltip"));
        lab_dom_write(node, LAB_DOM_ATTRIBUTE, "role", lab_dom_string("tooltip"));
        lab_dom_append(lab_dom_query(0, "body"), node);
    }
    return node;
}

/* Restore exact original spelling, whitespace and absent/empty attribute distinction. */
void lab_tooltip_hide(jsref button)
{
    if (lab_dom_has(button, "data-mars-tooltip-description")) {
        if (lab_dom_has(button, "data-mars-tooltip-described"))
            lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-describedby",
                          lab_dom_read(button, LAB_DOM_DATASET, "marsTooltipDescription"));
        else
            lab_dom_remove(button, "aria-describedby");
        lab_dom_remove(button, "data-mars-tooltip-description");
        lab_dom_remove(button, "data-mars-tooltip-described");
    }
    lab_dom_class(lab_dom_query(0, "#marsButtonTooltip"), "visible", 0);
}

/* Refresh repeated hover/focus without replacing the saved original description. */
bool lab_tooltip_show(jsref button, jsref previous, double width, double height)
{
    if (!button || lab_dom_closest(button, ".hidden"))
        return false;
    if (!lab_dom_equal(button, previous))
        lab_tooltip_hide(previous);
    int node = lab_tooltip_node();
    lab_dom_write(node, LAB_DOM_TEXT, "", lab_tooltip_text(button));
    lab_dom_remove(button, "title");
    if (!lab_dom_has(button, "data-mars-tooltip-description")) {
        if (lab_dom_has(button, "aria-describedby"))
            lab_dom_write(button, LAB_DOM_DATASET, "marsTooltipDescribed", lab_dom_string("1"));
        lab_dom_write(button, LAB_DOM_DATASET, "marsTooltipDescription",
                      lab_dom_read(button, LAB_DOM_ATTRIBUTE, "aria-describedby"));
    }
    int original = lab_dom_clean(lab_dom_read(button, LAB_DOM_DATASET, "marsTooltipDescription"), LAB_DOM_CLEAN_COLLAPSE);
    int tokens = lab_dom_join(lab_dom_string(" "), original, " ");
    if (!lab_dom_contains(tokens, lab_dom_string(" marsButtonTooltip ")))
        original = lab_dom_join(original, lab_dom_string(lab_dom_length(original) ? " " : ""), "marsButtonTooltip");
    lab_dom_write(button, LAB_DOM_ATTRIBUTE, "aria-describedby", original);
    const double *position = lab_view_tooltip_rect(
        width, height, lab_dom_measure(button, LAB_DOM_MEASURE_RECT_LEFT),
        lab_dom_measure(button, LAB_DOM_MEASURE_RECT_TOP), lab_dom_measure(button, LAB_DOM_MEASURE_RECT_WIDTH),
        lab_dom_measure(button, LAB_DOM_MEASURE_RECT_BOTTOM), lab_dom_measure(node, LAB_DOM_MEASURE_RECT_WIDTH),
        lab_dom_measure(node, LAB_DOM_MEASURE_RECT_HEIGHT));
    if (position) {
        lab_dom_write(node, LAB_DOM_STYLE, "left", lab_dom_format(position[0], "", "px"));
        lab_dom_write(node, LAB_DOM_STYLE, "top", lab_dom_format(position[1], "", "px"));
    }
    lab_dom_class(node, "visible", 1);
    return true;
}
