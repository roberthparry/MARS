/**
 * @file lab_plot_controls.c
 * @brief Grouped plot ranges, native variable radio choices and dimensional visibility.
 *
 * Names and labels come from the native binding catalogue; no formula parsing or
 * numerical range selection occurs here. Browser-owned controls retain selections.
 * Blank range text is sent unchanged to the native worker's automatic-range policy.
 */
#include "lab_dom.h"
#include "lab_plot_controls.h"

static int value(const char *id)
{
    return lab_dom_read(lab_dom_id(id), 5, "");
}

static const char *const controls[] = {"plotFirst", "plotSecond", "plotKind", "plotScale"};

static int control_index(int name)
{
    /* Four fixed UI groups; mathematical symbol lookup remains with native binding metadata. */
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i)
        if (lab_dom_equal(name, lab_dom_string(controls[i])))
            return (int)i;
    return -1;
}

static int automatic_value(unsigned index)
{
    return lab_dom_string(index < 2 ? "" : "auto");
}

/* Project a bounded UI catalogue, not a symbol lookup or mathematical rewrite. */
void lab_plot_controls_sync(void)
{
    int first = value("plotFirst"), second = value("plotSecond");
    lab_dom_write(lab_dom_id("expressionPlot"), 3, "variable", first);
    if (lab_dom_length(first) && lab_dom_equal(first, second)) {
        second = lab_dom_string("");
        lab_dom_write(lab_dom_id("plotSecond"), 5, "", second);
    }
    int choices = lab_dom_all(lab_dom_id("plotRange"), ".plot-choices input");
    for (unsigned i = 0; i < lab_dom_count(choices); ++i) {
        unsigned mark = lab_dom_mark();
        int radio = lab_dom_item(choices, i), name = lab_dom_read(radio, 5, "");
        int index = control_index(lab_dom_get(radio, "name"));
        if (index >= 0) {
            lab_dom_checked(radio, lab_dom_equal(name, value(controls[index])));
            lab_dom_disabled(radio, index == 1 && lab_dom_equal(name, first));
        }
        lab_dom_release(mark);
    }
    int resets = lab_dom_all(lab_dom_id("plotRange"), "[data-plot-reset]");
    for (unsigned i = 0; i < lab_dom_count(resets); ++i) {
        unsigned mark = lab_dom_mark();
        int button = lab_dom_item(resets, i), index = control_index(lab_dom_read(button, 3, "plotReset"));
        if (index > 0)
            lab_dom_disabled(button, lab_dom_equal(value(controls[index]), automatic_value((unsigned)index)));
        lab_dom_release(mark);
    }
    int kind = value("plotKind"), range = lab_dom_id("plotRange");
    int curve = lab_dom_equal(kind, lab_dom_string("curve"));
    int surface = lab_dom_equal(kind, lab_dom_string("surface"));
    if (lab_dom_query(0, "#rendered [data-series]"))
        lab_dom_set(range, "marsPlotSurface", lab_dom_scalar(1, !!lab_dom_query(0, "#rendered [data-surface]")));
    if (!curve && !surface)
        surface = lab_dom_truth(lab_dom_get(range, "marsPlotSurface")) || lab_dom_length(second);
    int multiple = lab_dom_to_number(lab_dom_get(range, "marsPlotVariables")) > 1;
    lab_dom_class(lab_dom_id("plotSecondGroup"), "hidden", curve || (!surface && !multiple));
    lab_dom_class(lab_dom_id("plotCamera"), "hidden", !surface);
    lab_dom_class(lab_dom_id("plotVertical"), "hidden", 0);
    lab_dom_write(lab_dom_id("plotVerticalTitle"), 0, "",
                  lab_dom_string(surface ? "Height range" : "Vertical range"));
    lab_dom_class(lab_dom_id("plotManual"), "hidden", !lab_dom_equal(value("plotScale"), lab_dom_string("manual")));
}

static void choice(int group, const char *name, int key, int label)
{
    int wrapper = lab_dom_create("label"), radio = lab_dom_create("input"), text = lab_dom_create("span");
    lab_dom_write(radio, 2, "type", lab_dom_string("radio"));
    lab_dom_write(radio, 2, "name", lab_dom_string(name));
    lab_dom_write(radio, 5, "", key);
    lab_dom_write(text, 0, "", label);
    lab_dom_append(wrapper, radio);
    lab_dom_append(wrapper, text);
    lab_dom_append(group, wrapper);
}

/* Build accessible native radio groups, preserving selections only while their variables exist. */
void lab_plot_controls_variables(int names, int labels)
{
    int groups[] = {lab_dom_id("plotFirstChoices"), lab_dom_id("plotSecondChoices")};
    int index = lab_dom_object(5);
    for (unsigned axis = 0; axis < 2; ++axis)
        lab_dom_write(groups[axis], 0, "", lab_dom_string(""));
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        unsigned mark = lab_dom_mark();
        int name = lab_dom_item(names, i), label = lab_dom_item(labels, i);
        lab_dom_key_set(index, name, lab_dom_scalar(1, 1));
        choice(groups[0], "plotFirst", name, label);
        choice(groups[1], "plotSecond", name, label);
        lab_dom_release(mark);
    }
    if (!lab_dom_truth(lab_dom_key_get(index, value("plotFirst"))))
        lab_dom_write(lab_dom_id("plotFirst"), 5, "", lab_dom_count(names) ? lab_dom_item(names, 0) : lab_dom_string(""));
    if (!lab_dom_truth(lab_dom_key_get(index, value("plotSecond"))))
        lab_dom_write(lab_dom_id("plotSecond"), 5, "", lab_dom_string(""));
    lab_dom_set(lab_dom_id("plotRange"), "marsPlotVariables", lab_dom_numeric(lab_dom_count(names)));
    lab_dom_subscribe(lab_dom_id("plotRange"), "change", "lab_plot_controls_dispatch", 0, 0, 0);
    lab_dom_subscribe(lab_dom_id("plotRange"), "click", "lab_plot_controls_dispatch", 1, 0, 0);
    lab_plot_controls_sync();
}

/* The Plot action and the card's radio selection share one first-axis value. */
void lab_plot_controls_prepare(int variable)
{
    lab_dom_write(lab_dom_id("plotFirst"), 5, "", variable);
    lab_plot_controls_sync();
}

/* Radios are explicit overrides; Reset clears the selection and restores native automatic policy. */
void lab_plot_controls_dispatch(unsigned action, int event, int context)
{
    (void)context;
    int plan = lab_dom_object(5);
    lab_dom_set(plan, "calls", lab_dom_object(4));
    lab_dom_return(plan);
    int target = lab_dom_get(event, "target");
    if (action == 1) {
        int button = lab_dom_closest(target, "[data-plot-reset]");
        int index = button ? control_index(lab_dom_read(button, 3, "plotReset")) : -1;
        if (index <= 0 || lab_dom_is_disabled(button))
            return;
        lab_dom_write(lab_dom_id(controls[index]), 5, "", automatic_value((unsigned)index));
    } else if (!action && lab_dom_closest(target, ".plot-choices")) {
        int index = control_index(lab_dom_get(target, "name"));
        if (index >= 0)
            lab_dom_write(lab_dom_id(controls[index]), 5, "", lab_dom_read(target, 5, ""));
    }
    lab_plot_controls_sync();
}
