/**
 * @file lab_tooltip.c
 * @brief Accessible control hints and their DOM projection for the WASM client.
 *
 * Owns label precedence, authored-title caching, description restoration and
 * viewport placement, including a fourteenfold preview of the Location summary flag.
 * The host retains only the active node for event delivery and restoration after
 * detachment. No scoped DOM handles survive a call.
 * Browser primitives supply Unicode casing/whitespace and measured rectangles;
 * this module never interprets mathematical content.
 */
#include "lab_dom.h"
#include "lab_view.h"
#include "lab_location.h"
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

/* Reuse the exact artwork, including SVG animation and reduced-motion policy. */
static bool lab_tooltip_flag_preview(jsref node, jsref button, double viewport_width, double viewport_height)
{
    if (!lab_dom_has_class(button, "almanac-location-flag") || !lab_dom_closest(button, ".almanac-summary"))
        return false;
    int source = lab_dom_query(button, "img.country-flag");
    if (lab_dom_query(0, "html[data-almanac-jurisdiction-flag]")) {
        int parent = lab_dom_query(button, "img.jurisdiction-flag");
        if (parent) source = parent;
    }
    int flag = lab_dom_read(source, LAB_DOM_ATTRIBUTE, "src");
    double width = lab_dom_measure(source, LAB_DOM_MEASURE_RECT_WIDTH);
    double height = lab_dom_measure(source, LAB_DOM_MEASURE_RECT_HEIGHT);
    if ((!lab_dom_starts_with(flag, "data:image/svg+xml;base64,") &&
         !lab_dom_equal(flag, lab_dom_string("/offshore_flag.svg"))) || width <= 0 || height <= 0)
        return false;
    /* Leave room for the popup's padding and viewport margins on small screens. */
    double scale = 14;
    if (width * scale > viewport_width - 48) scale = (viewport_width - 48) / width;
    if (height * scale > viewport_height - 48) scale = (viewport_height - 48) / height;
    if (scale <= 0) return false;
    int image = lab_dom_create("img");
    lab_dom_class(image, "country-flag", 1);
    lab_dom_write(image, LAB_DOM_ATTRIBUTE, "alt", lab_dom_read(button, LAB_DOM_ATTRIBUTE, "aria-label"));
    lab_dom_write(image, LAB_DOM_ATTRIBUTE, "src", flag);
    lab_dom_write(image, LAB_DOM_STYLE, "width", lab_dom_format(width * scale, "", "px"));
    lab_dom_write(image, LAB_DOM_STYLE, "height", lab_dom_format(height * scale, "", "px"));
    lab_dom_write(node, LAB_DOM_TEXT, "", lab_dom_string(""));
    lab_dom_append(node, image);
    return true;
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
    if (lab_dom_has_class(button, "select-button") &&
        lab_dom_length(lab_dom_read(button, LAB_DOM_DATASET, "countryOption"))) {
        lab_tooltip_hide(previous);
        return false;
    }
    if (!lab_dom_equal(button, previous))
        lab_tooltip_hide(previous);
    int node = lab_tooltip_node();
    lab_dom_write(node, LAB_DOM_TEXT, "", lab_tooltip_text(button));
    int country = lab_dom_read(button, LAB_DOM_DATASET, "countryName");
    if (lab_dom_length(lab_dom_read(button, LAB_DOM_DATASET, "townTooltipName")) && lab_dom_length(country)) {
        int name = lab_dom_join(lab_dom_read(button, LAB_DOM_DATASET, "townTooltipName"), lab_dom_string(", "), "");
        name = lab_dom_join(name, country, "");
        lab_dom_write(node, LAB_DOM_TEXT, "", lab_dom_join(name,
            lab_dom_read(button, LAB_DOM_DATASET, "townTooltipCoordinates"), ""));
    }
    if (lab_dom_length(lab_dom_read(button, LAB_DOM_DATASET, "townTooltipName")))
        lab_dom_write(node, LAB_DOM_TEXT, "", lab_location_town_text(lab_dom_read(node, LAB_DOM_TEXT, "")));
    int flag = lab_dom_read(button, LAB_DOM_DATASET, "countryFlag");
    if (lab_dom_length(lab_dom_read(button, LAB_DOM_DATASET, "countryCode")) &&
        (lab_dom_starts_with(flag, "data:image/svg+xml;base64,") ||
         lab_dom_equal(flag, lab_dom_string("/offshore_flag.svg")))) {
        int text = lab_dom_read(node, LAB_DOM_TEXT, "");
        lab_dom_write(node, LAB_DOM_TEXT, "", lab_dom_string(""));
        int image = lab_dom_create("img"), label = lab_dom_create("span");
        lab_dom_class(image, "country-flag", 1);
        lab_dom_write(image, LAB_DOM_ATTRIBUTE, "alt", lab_dom_string(""));
        lab_dom_write(image, LAB_DOM_ATTRIBUTE, "src", flag);
        lab_dom_write(label, LAB_DOM_TEXT, "", text);
        lab_dom_append(node, image);
        lab_dom_append(node, label);
    }
    bool flag_preview = lab_tooltip_flag_preview(node, button, width, height);
    lab_dom_class(node, "mars-flag-tooltip", flag_preview);
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
    double anchor_top = lab_dom_measure(button, LAB_DOM_MEASURE_RECT_TOP);
    double tooltip_height = lab_dom_measure(node, LAB_DOM_MEASURE_RECT_HEIGHT);
    const double *position = lab_view_tooltip_rect(
        width, height, lab_dom_measure(button, LAB_DOM_MEASURE_RECT_LEFT),
        anchor_top, lab_dom_measure(button, LAB_DOM_MEASURE_RECT_WIDTH),
        lab_dom_measure(button, LAB_DOM_MEASURE_RECT_BOTTOM), lab_dom_measure(node, LAB_DOM_MEASURE_RECT_WIDTH),
        tooltip_height);
    if (position) {
        /* Only flag previews prefer above; viewport coordinates already account for scrolling.
         * The body-owned popup may overlap its card, retaining the usual eight-pixel gap and margin. */
        double above = anchor_top - tooltip_height - 8;
        double top = flag_preview && above >= 8 && above + tooltip_height <= height - 8 ? above : position[1];
        lab_dom_write(node, LAB_DOM_STYLE, "left", lab_dom_format(position[0], "", "px"));
        lab_dom_write(node, LAB_DOM_STYLE, "top", lab_dom_format(top, "", "px"));
    }
    lab_dom_class(node, "visible", 1);
    return true;
}
