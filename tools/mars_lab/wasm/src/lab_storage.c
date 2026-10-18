/**
 * @file lab_storage.c
 * @brief Saved worksheet normalisation and restoration policy for MARS Lab.
 *
 * Resolves server/local field defaults, precision settings, recovery precedence
 * and history projection. Existing persistence schemas and native numeric rules
 * remain authoritative. The host supplies storage access, clocks and asynchronous
 * editor preparation, invoking projection only after ownership checks. Mathematical
 * text remains opaque. No borrowed browser handle survives a synchronous call.
 */
#include "lab_mode.h"
#include "lab_browser.h"
#include "lab_dom.h"
#include "lab_persist.h"
#include "lab_workspace.h"
#include "lab_storage.h"

typedef struct {
    const char *server, *local, *selector;
} lab_storage_field_t;

static const lab_storage_field_t lab_storage_fields_table[] = {
    {"equation_variable", "mars.exprLab.lastEquationVariable", "#equationVariable"},
    {"matrix_operation", "mars.exprLab.lastMatrixOperation", "#matrixOperation"},
    {"matrix_operand", "mars.exprLab.lastMatrixOperand", "#matrixOperand"},
    {"integrator_bounds", "mars.exprLab.lastIntegratorBounds", ""},
    {"integrator_interval_cap", "mars.exprLab.lastIntegratorIntervalCap", "#integratorIntervalCap"}};

static jsref lab_storage_or(jsref value, jsref fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static jsref lab_storage_text(jsref value, int trim)
{
    int text = lab_dom_text(lab_storage_or(value, lab_dom_string("")));
    return trim ? lab_dom_clean(text, LAB_DOM_CLEAN_TRIM) : text;
}

static double lab_storage_cap(jsref value, jsref config)
{
    return lab_browser_intervals(lab_dom_parse_int(value, 10),
                                 lab_dom_to_number(lab_dom_get(config, "DEFAULT_INTEGRATOR_INTERVAL_CAP")));
}

static jsref lab_storage_matrix_operation(jsref value)
{
    int operation = lab_storage_text(value, 1);
    int options = lab_dom_get(lab_dom_query(0, "#matrixOperation"), "options");
    unsigned mark = lab_dom_mark();
    /* The small DOM option list is authoritative, including dynamically supplied operations. */
    for (unsigned i = 0; i < lab_dom_count(options); ++i) {
        int matches = lab_dom_equal(operation, lab_dom_get(lab_dom_item(options, i), "value"));
        lab_dom_release(mark);
        if (matches)
            return operation;
    }
    return lab_dom_string("eval");
}

/* Preserve decimal-prefix conversion and the existing finite precision clamp. */
double lab_storage_precision(jsref values)
{
    return lab_browser_precision(lab_dom_parse_int(lab_dom_item(values, 0), 10),
                                 lab_dom_to_number(lab_dom_item(values, 1)));
}

/* Select only an offered integrator budget, retaining the configured fallback. */
double lab_storage_intervals(jsref values, jsref config)
{
    return lab_storage_cap(lab_dom_item(values, 0), config);
}

/* Validate an operation against the actual browser select, not a second application catalogue. */
void lab_storage_operation(jsref values)
{
    lab_dom_return(lab_storage_matrix_operation(lab_dom_item(values, 0)));
}

/* Server editors are trimmed; local editor bytes and native restoration flags are preserved. */
void lab_storage_editor(lab_mode_t mode, jsref values, lab_persist_origin_t origin)
{
    int text = lab_storage_text(lab_dom_item(values, 0), origin == LAB_PERSIST_SERVER);
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    unsigned flags =
        lab_persist_restore(mode, lab_dom_truth(text), lab_dom_contains(text, lab_dom_string("...")), origin);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "flags", lab_dom_numeric(flags));
    lab_dom_return(result);
}

/* Publish the small ordered I/O schema; local failure boundaries retain their established order. */
void lab_storage_fields(lab_persist_origin_t origin)
{
    static const lab_storage_control_t local_order[] = {
        LAB_STORAGE_MATRIX_OPERATION, LAB_STORAGE_MATRIX_OPERAND, LAB_STORAGE_EQUATION_VARIABLE,
        LAB_STORAGE_INTEGRATOR_BOUNDS, LAB_STORAGE_INTERVAL_CAP};
    int fields = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < 5; ++i) {
        lab_storage_control_t field = origin != LAB_PERSIST_SERVER ? local_order[i] : (lab_storage_control_t)i;
        int entry = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(entry, "field", lab_dom_numeric(field));
        lab_dom_set(
            entry, "key",
            lab_dom_string(origin != LAB_PERSIST_SERVER ? lab_storage_fields_table[field].local
                                                       : lab_storage_fields_table[field].server));
        lab_dom_push(fields, entry);
        lab_dom_release(mark);
    }
    lab_dom_return(fields);
}

/* Keep local variable bytes while supplying the server's normalised default. */
static jsref lab_storage_variable(jsref value, lab_persist_origin_t origin, jsref config)
{
    if (origin != LAB_PERSIST_SERVER)
        return value;
    value = lab_storage_text(value, 1);
    return lab_storage_or(value, lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE"));
}

/* Validate both storage origins against the live operation menu. */
static jsref lab_storage_operation_value(jsref value, lab_persist_origin_t origin, jsref config)
{
    (void)origin;
    (void)config;
    return lab_storage_matrix_operation(value);
}

/* Preserve local operand bytes and normalise only server text. */
static jsref lab_storage_operand(jsref value, lab_persist_origin_t origin, jsref config)
{
    (void)config;
    return origin == LAB_PERSIST_SERVER ? lab_storage_text(value, 1) : value;
}

/* Apply the interval budget policy to either storage origin. */
static jsref lab_storage_cap_value(jsref value, lab_persist_origin_t origin, jsref config)
{
    (void)origin;
    return lab_dom_numeric(lab_storage_cap(value, config));
}

/* Project one restored control; only bounds return text requiring asynchronous native preparation. */
void lab_storage_control(lab_storage_control_t field, jsref values, lab_persist_origin_t origin, jsref config)
{
    /** @brief Value normalisers for controls with synchronous DOM projection. */
    static jsref (*const normalise[])(jsref, lab_persist_origin_t, jsref) = {
        [LAB_STORAGE_EQUATION_VARIABLE] = lab_storage_variable,
        [LAB_STORAGE_MATRIX_OPERATION]  = lab_storage_operation_value,
        [LAB_STORAGE_MATRIX_OPERAND]    = lab_storage_operand,
        [LAB_STORAGE_INTERVAL_CAP]      = lab_storage_cap_value,
    };
    lab_dom_return(0);
    if ((unsigned)field > LAB_STORAGE_INTERVAL_CAP)
        return;
    int value = lab_dom_item(values, 0);
    if (origin != LAB_PERSIST_SERVER &&
        (field == LAB_STORAGE_MATRIX_OPERAND ? !lab_dom_type(value) : !lab_dom_truth(value)))
        return;
    if (field == LAB_STORAGE_INTEGRATOR_BOUNDS) {
        int bounds = origin != LAB_PERSIST_SERVER ? value : lab_storage_text(value, 1);
        if (lab_dom_truth(bounds))
            lab_dom_return(bounds);
        return;
    }
    int node = lab_dom_query(0, lab_storage_fields_table[field].selector);
    if (!node)
        return;
    value = normalise[field](value, origin, config);
    lab_dom_write(node, LAB_DOM_VALUE, "", value);
}

/* Restore recognised precision modes directly into their native owner, including legacy scalar saves. */
void lab_storage_precisions(jsref data)
{
    int values = lab_dom_get(data, "precision_bits");
    lab_value_kind_t type = lab_dom_type(values);
    unsigned mark = lab_dom_mark();
    if (type == 4 || type == 5) {
        static const char *const names[] = {"expression", "equation", "diffequation", "matrix",
                                            "integrator", "datetime", "almanac"};
        /* Seven direct lookups avoid scanning untrusted saved keys or accepting inherited properties. */
        for (lab_mode_t mode = LAB_MODE_EXPRESSION; mode < LAB_MODE_COUNT; ++mode) {
            int value = lab_dom_key_get(values, lab_dom_string(names[mode]));
            lab_workspace_precision_set(
                mode, lab_browser_precision(lab_dom_parse_int(value, 10), lab_workspace_precision(mode)));
            lab_dom_release(mark);
        }
    } else {
        lab_workspace_precision_set(
            LAB_MODE_EXPRESSION,
            lab_browser_precision(lab_dom_parse_int(values, 10), lab_workspace_precision(LAB_MODE_EXPRESSION)));
    }
}

/* Compare opaque local/server copies using the shared timestamp and default-placeholder policy. */
void lab_storage_recovery(lab_mode_t mode, jsref server, jsref local, jsref config)
{
    if ((unsigned)mode > LAB_MODE_EQUATION) {
        lab_dom_return(0);
        return;
    }
    int server_text = lab_storage_text(lab_dom_get(server, lab_persist_text(mode, 0, LAB_PERSIST_SERVER)), 1);
    int text = lab_storage_text(lab_dom_item(local, 0), 1);
    double saved_at = lab_dom_to_number(lab_storage_or(lab_dom_item(local, 1), lab_dom_numeric(0)));
    double server_at =
        lab_dom_to_number(lab_storage_or(lab_dom_get(server, lab_persist_text(mode, 1, LAB_PERSIST_SERVER)), lab_dom_numeric(0)));
    int placeholder = lab_dom_equal(
        server_text, lab_dom_get(config, mode == LAB_MODE_EQUATION ? "DEFAULT_EQUATION" : "DEFAULT_EXPRESSION"));
    if (!lab_workspace_prefer_local(lab_dom_truth(text), saved_at, server_at, lab_dom_truth(server_text),
                                    placeholder)) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "updatedAt", lab_dom_numeric(saved_at));
    lab_dom_return(result);
}

/* Assemble a recovered server patch after editor preparation, using one coherent fallback timestamp. */
void lab_storage_recovered(lab_mode_t mode, jsref recovery, jsref values, double now)
{
    int patch = lab_dom_object(LAB_VALUE_OBJECT);
    if ((unsigned)mode <= LAB_MODE_EQUATION) {
        int timestamp = lab_storage_or(lab_dom_get(recovery, "updatedAt"), lab_dom_numeric(now));
        lab_dom_set(patch, lab_persist_text(mode, 0, LAB_PERSIST_SERVER), lab_dom_item(values, 0));
        lab_dom_set(patch, lab_persist_text(mode, 1, LAB_PERSIST_SERVER), timestamp);
    }
    lab_dom_return(patch);
}

/* Describe asynchronous restoration before any controls are changed. */
static void lab_storage_history_plan(jsref state, jsref config, jsref mode)
{
    int integrator = lab_dom_equal(mode, lab_dom_string("integrator"));
    int calendar = lab_dom_equal(mode, lab_dom_string("datetime")) || lab_dom_equal(mode, lab_dom_string("almanac"));
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(plan, "mode", mode);
    lab_dom_set(plan, "text", lab_storage_or(lab_dom_get(state, "text"), lab_dom_string("")));
    lab_dom_set(plan, "calendar", lab_dom_scalar(LAB_VALUE_BOOLEAN, calendar));
    lab_dom_set(plan, "integrator", lab_dom_scalar(LAB_VALUE_BOOLEAN, integrator));
    lab_dom_set(plan, "bounds",
                lab_storage_or(lab_dom_get(state, "bounds"), lab_dom_get(config, "DEFAULT_INTEGRATOR_BOUNDS")));
    lab_dom_set(plan, "calendarState", lab_storage_or(lab_dom_key_get(state, mode), lab_dom_object(LAB_VALUE_OBJECT)));
    lab_dom_return(plan);
}

/* Project only the scalar controls belonging to this restored worksheet. */
static void lab_storage_history_controls(jsref state, jsref config, jsref mode)
{
    if (lab_dom_equal(mode, lab_dom_string("equation"))) {
        int value = lab_storage_text(lab_dom_get(state, "variable"), 1);
        lab_dom_write(lab_dom_query(0, "#equationVariable"), LAB_DOM_VALUE, "",
                      lab_storage_or(value, lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE")));
    } else if (lab_dom_equal(mode, lab_dom_string("matrix"))) {
        lab_dom_write(lab_dom_query(0, "#matrixOperation"), LAB_DOM_VALUE, "",
                      lab_storage_or(lab_dom_get(state, "operation"), lab_dom_string("eval")));
        lab_dom_write(lab_dom_query(0, "#matrixOperand"), LAB_DOM_VALUE, "", lab_storage_text(lab_dom_get(state, "operand"), 1));
    }
}

/* Bounds preparation has completed before its interval budget is restored. */
static void lab_storage_history_bounds(jsref state, jsref config, jsref mode)
{
    if (lab_dom_equal(mode, lab_dom_string("integrator")))
        lab_dom_write(lab_dom_query(0, "#integratorIntervalCap"), LAB_DOM_VALUE, "",
                      lab_dom_numeric(lab_storage_cap(lab_dom_get(state, "intervalCap"), config)));
}

/* Calendar restoration finishes by projecting the configured editor text. */
static void lab_storage_history_calendar(jsref state, jsref config, jsref mode)
{
    (void)state;
    int datetime = lab_dom_equal(mode, lab_dom_string("datetime"));
    if (datetime || lab_dom_equal(mode, lab_dom_string("almanac")))
        lab_dom_write(lab_dom_query(0, "#expr"), LAB_DOM_VALUE, "",
                      lab_dom_get(config, datetime ? "DEFAULT_DATETIME_TEXT" : "DEFAULT_ALMANAC_TEXT"));
}

/* Split restoration around awaits: plan, guarded controls, guarded bounds completion, then final editor. */
void lab_storage_history(lab_storage_history_phase_t phase, jsref state, jsref config)
{
    static void (*const handlers[])(jsref, jsref, jsref) = {
        [LAB_STORAGE_HISTORY_PLAN]     = lab_storage_history_plan,
        [LAB_STORAGE_HISTORY_CONTROLS] = lab_storage_history_controls,
        [LAB_STORAGE_HISTORY_BOUNDS]   = lab_storage_history_bounds,
        [LAB_STORAGE_HISTORY_CALENDAR] = lab_storage_history_calendar,
    };
    int mode = lab_dom_get(state, "mode");
    if ((unsigned)phase < sizeof handlers / sizeof *handlers)
        handlers[phase](state, config, mode);
}
