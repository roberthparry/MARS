/**
 * @file lab_persist_dom.c
 * @brief Persistence and history record assembly for the WASM worksheet.
 *
 * Builds local/server save records and captures mode-specific history fields
 * using the native schemas. Browser storage, timers and asynchronous restoration
 * remain host capabilities. Mathematical editor text arrives already prepared
 * by native MARS; this module does not interpret it or retain borrowed handles.
 */
#include "lab_dom.h"
#include "lab_persist.h"
#include "lab_profile.h"
#include "lab_persist_dom.h"

static const char *const lab_persist_names[] = {"expression", "equation", "diffequation", "matrix",
                                                "integrator", "datetime", "almanac"};

/* Materialise the small indexed schema for storage readers and diagnostics. */
void lab_persist_schema(unsigned mode)
{
    int result = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned field = 0; field < lab_persist_count(mode); ++field) {
        int entry = lab_dom_object(5);
        lab_dom_set(entry, "field", lab_dom_numeric(field));
        lab_dom_set(entry, "server", lab_dom_string(lab_persist_text(mode, field, 0)));
        lab_dom_set(entry, "local", lab_dom_string(lab_persist_text(mode, field, 1)));
        lab_dom_set(entry, "source", lab_dom_numeric(lab_persist_source(mode, field)));
        lab_dom_push(result, entry);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Local omissions preserve useful previous text; the independent server record is complete. */
void lab_persist_records(unsigned mode, int sources, int precision)
{
    int result = lab_dom_object(5), local = lab_dom_object(5), server = lab_dom_object(5);
    unsigned mark = lab_dom_mark();
    for (unsigned field = 0; field < lab_persist_count(mode); ++field) {
        int value = lab_dom_item(sources, lab_persist_source(mode, field));
        lab_dom_set(server, lab_persist_text(mode, field, 0), value);
        if (lab_persist_local(mode, field, lab_dom_truth(value)))
            lab_dom_set(local, lab_persist_text(mode, field, 1), value);
        lab_dom_release(mark);
    }
    lab_dom_set(server, "precision_bits", precision);
    lab_dom_set(result, "local", local);
    lab_dom_set(result, "server", server);
    lab_dom_return(result);
}

static int lab_persist_control(const char *selector, int fallback, int trim)
{
    int text = lab_dom_read(lab_dom_query(0, selector), 5, "");
    if (trim)
        text = lab_dom_clean(text, 0);
    return lab_dom_length(text) ? text : fallback;
}

/* Capture native history shape without asynchronous work or expression parsing. */
void lab_persist_history(unsigned mode, int text, int calendar, int bounds, int cap, int config)
{
    if (mode > 6) {
        lab_dom_return(0);
        return;
    }
    int state = lab_dom_object(5), empty = lab_dom_string("");
    lab_dom_set(state, "mode", lab_dom_string(lab_persist_names[mode]));
    lab_dom_set(state, "text", lab_dom_clean(text, 0));
    if (mode == 1 && lab_dom_query(0, "#equationVariable"))
        lab_dom_set(state, "variable",
                    lab_persist_control("#equationVariable", lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE"), 1));
    else if (mode == 3) {
        lab_dom_set(state, "operation", lab_persist_control("#matrixOperation", empty, 0));
        lab_dom_set(state, "operand", lab_persist_control("#matrixOperand", empty, 1));
    } else if (mode == 4) {
        lab_dom_set(state, "bounds", bounds);
        lab_dom_set(state, "intervalCap", cap);
    } else if (mode >= 5) {
        lab_dom_set(state, lab_persist_names[mode], calendar);
        if (!lab_dom_length(lab_dom_get(state, "text")))
            lab_dom_set(state, "text",
                        lab_dom_get(config, mode == 5 ? "DEFAULT_DATETIME_TEXT" : "DEFAULT_ALMANAC_TEXT"));
    }
    lab_dom_return(state);
}

/* Calendar persistence uses exactly the same field order as the capture schema. */
void lab_persist_calendar(unsigned mode, int state, int precision)
{
    int patch = lab_dom_object(5);
    if (mode == 5 || mode == 6) {
        int prefix = lab_dom_string(mode == 5 ? "datetime_" : "almanac_");
        unsigned mark = lab_dom_mark();
        for (unsigned field = 0; field < lab_profile_count(mode); ++field) {
            const char *key = lab_profile_text(mode, field, 0);
            lab_dom_key_set(patch, lab_dom_join(prefix, lab_dom_string(key), ""), lab_dom_get(state, key));
            lab_dom_release(mark);
        }
        lab_dom_set(patch, "precision_bits", precision);
    }
    lab_dom_return(patch);
}
