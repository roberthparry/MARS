/**
 * @file lab_persist_dom.c
 * @brief Persistence and history record assembly for the WASM worksheet.
 *
 * Builds local/server save records and captures mode-specific history fields
 * using the native schemas. Browser storage, timers and asynchronous restoration
 * remain host capabilities. Mathematical editor text arrives already prepared
 * by native MARS; this module does not interpret it or retain borrowed handles.
 */
#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_persist.h"
#include "lab_profile.h"
#include "lab_persist_dom.h"

static const char *const lab_persist_names[] = {"expression", "equation", "diffequation", "matrix",
                                                "integrator", "datetime", "almanac"};

/* Materialise the small indexed schema for storage readers and diagnostics. */
void lab_persist_schema(lab_mode_t mode)
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (unsigned field = 0; field < lab_persist_count(mode); ++field) {
        int entry = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(entry, "field", lab_dom_numeric(field));
        lab_dom_set(entry, "server", lab_dom_string(lab_persist_text(mode, field, LAB_PERSIST_SERVER)));
        lab_dom_set(entry, "local", lab_dom_string(lab_persist_text(mode, field, LAB_PERSIST_LOCAL)));
        lab_dom_set(entry, "source", lab_dom_numeric(lab_persist_source(mode, field)));
        lab_dom_push(result, entry);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Local omissions preserve useful previous text; the independent server record is complete. */
void lab_persist_records(lab_mode_t mode, jsref sources, jsref precision)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT), local = lab_dom_object(LAB_VALUE_OBJECT),
        server = lab_dom_object(LAB_VALUE_OBJECT);
    unsigned mark = lab_dom_mark();
    for (unsigned field = 0; field < lab_persist_count(mode); ++field) {
        int value = lab_dom_item(sources, lab_persist_source(mode, field));
        lab_dom_set(server, lab_persist_text(mode, field, LAB_PERSIST_SERVER), value);
        if (lab_persist_local(mode, field, lab_dom_truth(value)))
            lab_dom_set(local, lab_persist_text(mode, field, LAB_PERSIST_LOCAL), value);
        lab_dom_release(mark);
    }
    lab_dom_set(server, "precision_bits", precision);
    lab_dom_set(result, "local", local);
    lab_dom_set(result, "server", server);
    lab_dom_return(result);
}

static jsref lab_persist_control(const char *selector, jsref fallback, int trim)
{
    int text = lab_dom_read(lab_dom_query(0, selector), LAB_DOM_VALUE, "");
    if (trim)
        text = lab_dom_clean(text, LAB_DOM_CLEAN_TRIM);
    return lab_dom_length(text) ? text : fallback;
}

/** @brief Borrowed values shared by the mode-specific history capture handlers. */
typedef struct {
    lab_mode_t mode; /**< Validated worksheet mode. */
    jsref empty, calendar, bounds, cap, config; /**< Synchronous DOM handles supplied by the caller. */
} lab_persist_history_values_t;

/* Capture a variable only while its control exists. */
static void lab_persist_history_equation(jsref state, const lab_persist_history_values_t *values)
{
    if (lab_dom_query(0, "#equationVariable")) {
        int fallback = lab_dom_get(values->config, "DEFAULT_EQUATION_VARIABLE");
        lab_dom_set(state, "variable", lab_persist_control("#equationVariable", fallback, 1));
    }
}

/* Capture operation before operand, preserving their distinct whitespace policies. */
static void lab_persist_history_matrix(jsref state, const lab_persist_history_values_t *values)
{
    lab_dom_set(state, "operation", lab_persist_control("#matrixOperation", values->empty, 0));
    lab_dom_set(state, "operand", lab_persist_control("#matrixOperand", values->empty, 1));
}

/* Capture the already prepared bounds and interval budget. */
static void lab_persist_history_integrator(jsref state, const lab_persist_history_values_t *values)
{
    lab_dom_set(state, "bounds", values->bounds);
    lab_dom_set(state, "intervalCap", values->cap);
}

/* Capture calendar state before resolving an empty editor's default. */
static void lab_persist_history_calendar(jsref state, const lab_persist_history_values_t *values)
{
    /** @brief Calendar editor defaults indexed by the validated capture mode. */
    static const char *const defaults[LAB_MODE_COUNT] = {
        [LAB_MODE_DATETIME] = "DEFAULT_DATETIME_TEXT",
        [LAB_MODE_ALMANAC]  = "DEFAULT_ALMANAC_TEXT",
    };
    lab_dom_set(state, lab_persist_names[values->mode], values->calendar);
    if (!lab_dom_length(lab_dom_get(state, "text")))
        lab_dom_set(state, "text", lab_dom_get(values->config, defaults[values->mode]));
}

/* Capture native history shape without asynchronous work or expression parsing. */
void lab_persist_history(lab_mode_t mode, jsref text, jsref calendar, jsref bounds,
                         jsref cap, jsref config)
{
    /** @brief Additional history capture selected directly by the validated mode. */
    static void (*const capture[LAB_MODE_COUNT])(jsref, const lab_persist_history_values_t *) = {
        [LAB_MODE_EQUATION]   = lab_persist_history_equation,
        [LAB_MODE_MATRIX]     = lab_persist_history_matrix,
        [LAB_MODE_INTEGRATOR] = lab_persist_history_integrator,
        [LAB_MODE_DATETIME]   = lab_persist_history_calendar,
        [LAB_MODE_ALMANAC]    = lab_persist_history_calendar,
    };
    if ((unsigned)mode > LAB_MODE_ALMANAC) {
        lab_dom_return(0);
        return;
    }
    int state = lab_dom_object(LAB_VALUE_OBJECT), empty = lab_dom_string("");
    lab_dom_set(state, "mode", lab_dom_string(lab_persist_names[mode]));
    lab_dom_set(state, "text", lab_dom_clean(text, LAB_DOM_CLEAN_TRIM));
    const lab_persist_history_values_t values = {mode, empty, calendar, bounds, cap, config};
    if (capture[mode])
        capture[mode](state, &values);
    lab_dom_return(state);
}

/* Calendar persistence uses exactly the same field order as the capture schema. */
void lab_persist_calendar(lab_mode_t mode, jsref state, jsref precision)
{
    int patch = lab_dom_object(LAB_VALUE_OBJECT);
    if (mode == LAB_MODE_DATETIME || mode == LAB_MODE_ALMANAC) {
        int prefix = lab_dom_string(mode == LAB_MODE_DATETIME ? "datetime_" : "almanac_");
        if (mode == LAB_MODE_ALMANAC)
            lab_dom_set(patch, "almanac_zone_mode", lab_dom_get(state, "zone_mode"));
        unsigned mark = lab_dom_mark();
        for (unsigned field = 0; field < lab_profile_count(mode); ++field) {
            const char *key = lab_profile_text(mode, field, LAB_PROFILE_KEY);
            lab_dom_key_set(patch, lab_dom_join(prefix, lab_dom_string(key), ""), lab_dom_get(state, key));
            lab_dom_release(mark);
        }
        lab_dom_set(patch, "precision_bits", precision);
    }
    lab_dom_return(patch);
}
