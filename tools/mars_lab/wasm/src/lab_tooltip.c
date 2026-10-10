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
} lab_tooltip_labels[] = {{"back", "Return to the previous input in this mode"},
                          {"clear", "Clear the current input and its results"},
                          {"forward", "Move to the next input in this mode"},
                          {"goalSeek", "Find a variable value that reaches the requested target"},
                          {"help", "Show or hide help for the current mode"},
                          {"inputCopy", "Copy the current input"},
                          {"lessPrecision", "Calculate and display fewer significant digits"},
                          {"marsDatePickerClose", "Close the date picker"},
                          {"marsDatePickerToday", "Use the current date"},
                          {"morePrecision", "Calculate and display more significant digits"},
                          {"resultUseInput", "Put this result into the current mode as a new input"},
                          {"run", "Evaluate the current input"}};

static int lab_tooltip_label(int button)
{
    int id = lab_dom_read(button, 2, "id");
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

static int lab_tooltip_card(int button)
{
    int card = lab_dom_closest(button, ".result-card");
    int title = card ? lab_dom_query(card, ".card-title > span:first-child") : 0;
    int text = lab_dom_clean(lab_dom_read(title, 0, ""), 1);
    return lab_dom_length(text) ? text : lab_dom_string("result");
}

static int lab_tooltip_text(int button)
{
    int title = lab_dom_clean(lab_dom_read(button, 2, "title"), 0);
    if (lab_dom_has(button, "title")) {
        if (lab_dom_length(title))
            lab_dom_write(button, 3, "marsTooltipTitle", title);
        else
            lab_dom_remove(button, "data-mars-tooltip-title");
    }
    title = lab_dom_clean(lab_dom_read(button, 3, "marsTooltipTitle"), 0);
    if (lab_dom_length(title))
        return title;
    int label = lab_tooltip_label(button);
    if (label)
        return label;
    int text = lab_dom_clean(lab_dom_read(button, 0, ""), 0);
    if (lab_dom_has_class(button, "mode-tab"))
        return lab_dom_join(lab_dom_string("Switch to "), text, " mode");
    if (lab_dom_has(button, "data-expand-card"))
        return lab_dom_join(lab_dom_join(text, lab_dom_string(" the "), ""), lab_tooltip_card(button), " card");
    if (lab_dom_has_class(button, "more-digits"))
        return lab_dom_join(lab_dom_string("Show the full value in the "), lab_tooltip_card(button), " card");
    if (lab_dom_equal(lab_dom_read(button, 3, "copyTarget"), lab_dom_string("mobile")))
        return lab_dom_string("Copy the private mobile-access URL");
    if (lab_dom_has_class(button, "copy-result"))
        return lab_dom_join(lab_dom_string("Copy the "), lab_tooltip_card(button), "");
    if (lab_dom_has_class(button, "variable-copy"))
        return lab_dom_string("Copy this binding value");
    if (lab_dom_has_class(button, "select-button"))
        return lab_dom_join(lab_dom_string("Choose "), lab_dom_length(text) ? text : lab_dom_string("an option"), "");
    label = lab_dom_clean(lab_dom_read(button, 2, "aria-label"), 0);
    if (lab_dom_length(label))
        return label;
    text = lab_dom_clean(text, 2);
    return lab_dom_length(text) ? text : lab_dom_string("Activate this control");
}

static int lab_tooltip_node(void)
{
    int node = lab_dom_query(0, "#marsButtonTooltip");
    if (!node) {
        node = lab_dom_create("div");
        lab_dom_class(node, "mars-button-tooltip", 1);
        lab_dom_write(node, 2, "id", lab_dom_string("marsButtonTooltip"));
        lab_dom_write(node, 2, "role", lab_dom_string("tooltip"));
        lab_dom_append(lab_dom_query(0, "body"), node);
    }
    return node;
}

/* Restore exact original spelling, whitespace and absent/empty attribute distinction. */
void lab_tooltip_hide(int button)
{
    if (lab_dom_has(button, "data-mars-tooltip-description")) {
        if (lab_dom_has(button, "data-mars-tooltip-described"))
            lab_dom_write(button, 2, "aria-describedby", lab_dom_read(button, 3, "marsTooltipDescription"));
        else
            lab_dom_remove(button, "aria-describedby");
        lab_dom_remove(button, "data-mars-tooltip-description");
        lab_dom_remove(button, "data-mars-tooltip-described");
    }
    lab_dom_class(lab_dom_query(0, "#marsButtonTooltip"), "visible", 0);
}

/* Refresh repeated hover/focus without replacing the saved original description. */
int lab_tooltip_show(int button, int previous, double width, double height)
{
    if (!button || lab_dom_closest(button, ".hidden"))
        return 0;
    if (!lab_dom_equal(button, previous))
        lab_tooltip_hide(previous);
    int node = lab_tooltip_node();
    lab_dom_write(node, 0, "", lab_tooltip_text(button));
    lab_dom_remove(button, "title");
    if (!lab_dom_has(button, "data-mars-tooltip-description")) {
        if (lab_dom_has(button, "aria-describedby"))
            lab_dom_write(button, 3, "marsTooltipDescribed", lab_dom_string("1"));
        lab_dom_write(button, 3, "marsTooltipDescription", lab_dom_read(button, 2, "aria-describedby"));
    }
    int original = lab_dom_clean(lab_dom_read(button, 3, "marsTooltipDescription"), 2);
    int tokens = lab_dom_join(lab_dom_string(" "), original, " ");
    if (!lab_dom_contains(tokens, lab_dom_string(" marsButtonTooltip ")))
        original = lab_dom_join(original, lab_dom_string(lab_dom_length(original) ? " " : ""), "marsButtonTooltip");
    lab_dom_write(button, 2, "aria-describedby", original);
    const double *position = lab_view_tooltip_rect(
        width, height, lab_dom_measure(button, 7), lab_dom_measure(button, 8), lab_dom_measure(button, 3),
        lab_dom_measure(button, 9), lab_dom_measure(node, 3), lab_dom_measure(node, 4));
    if (position) {
        lab_dom_write(node, 4, "left", lab_dom_format(position[0], "", "px"));
        lab_dom_write(node, 4, "top", lab_dom_format(position[1], "", "px"));
    }
    lab_dom_class(node, "visible", 1);
    return 1;
}
