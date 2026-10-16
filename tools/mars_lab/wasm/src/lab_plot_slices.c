/**
 * @file lab_plot_slices.c
 * @brief Compact slice controls driven by native exact-value and range metadata.
 *
 * Browser controls hold opaque mathematical strings and integer slider positions.
 * No algebra or coordinate arithmetic is performed here. Existing nodes survive
 * replies during dragging; request snapshots prevent late replies overwriting
 * newer edits. The worksheet binding controls are never modified.
 */
#include "lab_dom.h"
#include "lab_plot_slices.h"

static const char *const fields[] = {".slice-lower", ".slice-upper", ".slice-value", ".slice-position"};

/* Serialise opaque coordinate fields for native interpolation. */
int lab_plot_slices_payload(void)
{
    int rows = lab_dom_all(lab_dom_id("plotSlices"), ".plot-slice"), text = lab_dom_string("");
    for (unsigned i = 0; i < lab_dom_count(rows); ++i) {
        int row = lab_dom_item(rows, i);
        text = lab_dom_join(text, lab_dom_read(row, 3, "name"), "");
        for (unsigned j = 0; j < 4; ++j) {
            text = lab_dom_join(text, lab_dom_string("\t"), "");
            text = lab_dom_join(text, lab_dom_read(lab_dom_query(row, fields[j]), 5, ""), "");
        }
        if (i + 1 < lab_dom_count(rows))
            text = lab_dom_join(text, lab_dom_string("\n"), "");
    }
    return text;
}

/* Remove plot-local controls without touching worksheet bindings. */
void lab_plot_slices_clear(void)
{
    lab_dom_write(lab_dom_id("plotSlices"), 0, "", lab_dom_string(""));
    lab_dom_class(lab_dom_id("plotSlices"), "hidden", 1);
    lab_dom_class(lab_dom_id("plotStage"), "has-slices", 0);
    lab_dom_delete(lab_dom_id("plotSlices"), "marsSliceRecords");
    lab_dom_delete(lab_dom_id("plotSlices"), "marsSliceSelected");
    lab_dom_delete(lab_dom_id("plotSlices"), "marsSliceModes");
}

static void range_mode(int row, int manual)
{
    lab_dom_checked(lab_dom_query(row, ".slice-auto"), !manual);
    lab_dom_checked(lab_dom_query(row, ".slice-manual"), manual);
    lab_dom_class(lab_dom_query(row, ".slice-limits"), "hidden", !manual);
}

/* A fresh Evaluate requests native automatic limits, even when the source is unchanged. */
void lab_plot_slices_auto(void)
{
    int root = lab_dom_id("plotSlices"), rows = lab_dom_all(root, ".plot-slice");
    lab_dom_delete(root, "marsSliceModes");
    for (unsigned i = 0; i < lab_dom_count(rows); ++i) {
        int row = lab_dom_item(rows, i);
        range_mode(row, 0);
        lab_dom_write(lab_dom_query(row, ".slice-position"), 5, "", lab_dom_string("-2"));
    }
}

/* At most eight native parameters: keep one visible editor and retain every value. */
static void select_slice(int root, int name)
{
    int rows = lab_dom_all(root, ".plot-slice"), radios = lab_dom_all(root, ".slice-choice");
    int selected = 0;
    for (unsigned i = 0; i < lab_dom_count(rows); ++i) {
        if (lab_dom_equal(name, lab_dom_read(lab_dom_item(rows, i), 3, "name")))
            selected = (int)i;
    }
    for (unsigned i = 0; i < lab_dom_count(rows); ++i) {
        lab_dom_class(lab_dom_item(rows, i), "hidden", (int)i != selected);
        lab_dom_checked(lab_dom_item(radios, i), (int)i == selected);
    }
    lab_dom_set(root, "marsSliceSelected", lab_dom_read(lab_dom_item(rows, selected), 3, "name"));
}

static int input(int row, const char *class_name, const char *type, int label)
{
    int wrapper = lab_dom_create("label"), text = lab_dom_create("span"), node = lab_dom_create("input");
    lab_dom_write(text, 0, "", label);
    lab_dom_write(node, 2, "type", lab_dom_string(type));
    lab_dom_write(node, 2, "class", lab_dom_string(class_name));
    lab_dom_write(node, 2, "spellcheck", lab_dom_string("false"));
    lab_dom_append(wrapper, text);
    lab_dom_append(wrapper, node);
    lab_dom_append(row, wrapper);
    return node;
}

/* Install accepted metadata only while its input snapshot is still current. */
void lab_plot_slices_apply(int records, int expected)
{
    if (!lab_dom_equal(lab_plot_slices_payload(), expected))
        return;
    int root = lab_dom_id("plotSlices");
    lab_dom_subscribe(root, "change", "lab_plot_controls_dispatch", 0, 0, 0);
    lab_dom_subscribe(root, "input", "lab_plot_controls_dispatch", 1, 0, 0);
    if (!lab_dom_truth(records)) {
        lab_plot_slices_clear();
        return;
    }
    int lines = lab_dom_split(records, "\n"), rows = lab_dom_all(root, ".plot-slice");
    int rebuild = lab_dom_count(lines) != lab_dom_count(rows);
    for (unsigned i = 0; !rebuild && i < lab_dom_count(lines); ++i) {
        int parts = lab_dom_split(lab_dom_item(lines, i), "\t");
        rebuild = !lab_dom_equal(lab_dom_item(parts, 0), lab_dom_read(lab_dom_item(rows, i), 3, "name"));
    }
    if (rebuild) {
        lab_dom_write(root, 0, "", lab_dom_string(""));
        int choices = lab_dom_create("fieldset"), legend = lab_dom_create("legend");
        lab_dom_write(choices, 2, "class", lab_dom_string("slice-choices"));
        lab_dom_class(choices, "hidden", lab_dom_count(lines) < 2);
        lab_dom_write(legend, 0, "", lab_dom_string("Slice"));
        lab_dom_append(choices, legend);
        lab_dom_append(root, choices);
    }
    for (unsigned i = 0; i < lab_dom_count(lines); ++i) {
        int parts = lab_dom_split(lab_dom_item(lines, i), "\t");
        if (lab_dom_count(parts) != 5)
            continue;
        int row = rebuild ? lab_dom_create("div") : lab_dom_item(rows, i);
        if (rebuild) {
            int choice = input(lab_dom_query(root, ".slice-choices"), "slice-choice", "radio", lab_dom_item(parts, 0));
            lab_dom_write(choice, 2, "name", lab_dom_string("plotSliceVariable"));
            lab_dom_write(choice, 5, "", lab_dom_item(parts, 0));
            lab_dom_write(row, 2, "class", lab_dom_string("plot-group plot-slice"));
            lab_dom_write(row, 3, "name", lab_dom_item(parts, 0));
            int slider = input(row, "slice-slider", "range", lab_dom_join(lab_dom_string("Slice: "), lab_dom_item(parts, 0), ""));
            lab_dom_write(slider, 2, "min", lab_dom_string("0"));
            lab_dom_write(slider, 2, "max", lab_dom_string("1000"));
            lab_dom_write(slider, 2, "step", lab_dom_string("1"));
            input(row, "slice-value", "text", lab_dom_string("Value"));
            int modes = lab_dom_create("fieldset"), legend = lab_dom_create("legend");
            lab_dom_write(modes, 2, "class", lab_dom_string("slice-modes"));
            lab_dom_write(legend, 0, "", lab_dom_string("Range"));
            lab_dom_append(modes, legend);
            int automatic = input(modes, "slice-mode slice-auto", "radio", lab_dom_string("Auto"));
            int manual = input(modes, "slice-mode slice-manual", "radio", lab_dom_string("Manual"));
            int group = lab_dom_join(lab_dom_string("sliceRange:"), lab_dom_item(parts, 0), "");
            lab_dom_write(automatic, 2, "name", group);
            lab_dom_write(manual, 2, "name", group);
            lab_dom_append(row, modes);
            int limits = lab_dom_create("div");
            lab_dom_write(limits, 2, "class", lab_dom_string("slice-limits"));
            input(limits, "slice-lower", "text", lab_dom_string("From"));
            input(limits, "slice-upper", "text", lab_dom_string("To"));
            lab_dom_append(row, limits);
            int settings = lab_dom_get(root, "marsSliceModes");
            range_mode(row, lab_dom_truth(lab_dom_key_get(settings, lab_dom_item(parts, 0))));
            int position = lab_dom_create("input");
            lab_dom_write(position, 2, "type", lab_dom_string("hidden"));
            lab_dom_write(position, 2, "class", lab_dom_string("slice-position"));
            lab_dom_append(row, position);
            lab_dom_append(root, row);
        }
        for (unsigned j = 0; j < 3; ++j)
            lab_dom_write(lab_dom_query(row, fields[j]), 5, "", lab_dom_item(parts, j + 1));
        /* The rounded handle is display-only until the user moves it. */
        lab_dom_write(lab_dom_query(row, ".slice-position"), 5, "", lab_dom_string("-1"));
        lab_dom_write(lab_dom_query(row, ".slice-slider"), 5, "", lab_dom_item(parts, 4));
    }
    select_slice(root, lab_dom_get(root, "marsSliceSelected"));
    lab_dom_class(root, "hidden", 0);
    lab_dom_class(lab_dom_id("plotStage"), "has-slices", 1);
    lab_dom_set(root, "marsSliceRecords", records);
}

/* Distinguish handle movement from an exact-value or limit edit. */
int lab_plot_slices_edit(unsigned action, int target)
{
    if (lab_dom_has_class(target, "slice-choice")) {
        if (action == 0 || action == 1)
            select_slice(lab_dom_id("plotSlices"), lab_dom_read(target, 5, ""));
        return 2;
    }
    int row = lab_dom_closest(target, ".plot-slice");
    if (!row || (action != 0 && action != 1))
        return 0;
    if (lab_dom_has_class(target, "slice-mode")) {
        if (action != 0)
            return 2;
        int manual = lab_dom_has_class(target, "slice-manual"), root = lab_dom_id("plotSlices");
        int settings = lab_dom_get(root, "marsSliceModes");
        if (!lab_dom_truth(settings)) {
            settings = lab_dom_object(5);
            lab_dom_set(root, "marsSliceModes", settings);
        }
        lab_dom_key_set(settings, lab_dom_read(row, 3, "name"), lab_dom_scalar(1, manual));
        range_mode(row, manual);
        if (manual)
            return 2;
        /* Ask native range selection to refresh this parameter without browser arithmetic. */
        lab_dom_write(lab_dom_query(row, ".slice-position"), 5, "", lab_dom_string("-2"));
        return 1;
    }
    int slider = lab_dom_has_class(target, "slice-slider");
    lab_dom_write(lab_dom_query(row, ".slice-position"), 5, "",
                  slider ? lab_dom_read(target, 5, "") : lab_dom_string("-1"));
    return 1;
}
