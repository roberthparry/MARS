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

static int lab_storage_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_storage_text(int value, int trim)
{
    int text = lab_dom_text(lab_storage_or(value, lab_dom_string("")));
    return trim ? lab_dom_clean(text, 0) : text;
}

static double lab_storage_cap(int value, int config)
{
    return lab_browser_intervals(lab_dom_parse_int(value, 10),
                                 lab_dom_to_number(lab_dom_get(config, "DEFAULT_INTEGRATOR_INTERVAL_CAP")));
}

static int lab_storage_matrix_operation(int value)
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
double lab_storage_precision(int values)
{
    return lab_browser_precision(lab_dom_parse_int(lab_dom_item(values, 0), 10),
                                 lab_dom_to_number(lab_dom_item(values, 1)));
}

/* Select only an offered integrator budget, retaining the configured fallback. */
double lab_storage_intervals(int values, int config)
{
    return lab_storage_cap(lab_dom_item(values, 0), config);
}

/* Validate an operation against the actual browser select, not a second application catalogue. */
void lab_storage_operation(int values)
{
    lab_dom_return(lab_storage_matrix_operation(lab_dom_item(values, 0)));
}

/* Server editors are trimmed; local editor bytes and native restoration flags are preserved. */
void lab_storage_editor(unsigned mode, int values, int local)
{
    int text = lab_storage_text(lab_dom_item(values, 0), !local), result = lab_dom_object(5);
    unsigned flags =
        lab_persist_restore(mode, lab_dom_truth(text), lab_dom_contains(text, lab_dom_string("...")), local);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "flags", lab_dom_numeric(flags));
    lab_dom_return(result);
}

/* Publish the small ordered I/O schema; local failure boundaries retain their established order. */
void lab_storage_fields(int local)
{
    static const unsigned local_order[] = {1, 2, 0, 3, 4};
    int fields = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < 5; ++i) {
        unsigned field = local ? local_order[i] : i;
        int entry = lab_dom_object(5);
        lab_dom_set(entry, "field", lab_dom_numeric(field));
        lab_dom_set(
            entry, "key",
            lab_dom_string(local ? lab_storage_fields_table[field].local : lab_storage_fields_table[field].server));
        lab_dom_push(fields, entry);
        lab_dom_release(mark);
    }
    lab_dom_return(fields);
}

/* Project one restored control; only bounds return text requiring asynchronous native preparation. */
void lab_storage_control(unsigned field, int values, int local, int config)
{
    lab_dom_return(0);
    if (field >= 5)
        return;
    int value = lab_dom_item(values, 0);
    if (local && (field == 2 ? !lab_dom_type(value) : !lab_dom_truth(value)))
        return;
    if (field == 3) {
        int bounds = local ? value : lab_storage_text(value, 1);
        if (lab_dom_truth(bounds))
            lab_dom_return(bounds);
        return;
    }
    int node = lab_dom_query(0, lab_storage_fields_table[field].selector);
    if (!node)
        return;
    if (field == 1)
        value = lab_storage_matrix_operation(value);
    else if (field == 4)
        value = lab_dom_numeric(lab_storage_cap(value, config));
    else if (!local) {
        value = lab_storage_text(value, 1);
        if (field == 0)
            value = lab_storage_or(value, lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE"));
    }
    lab_dom_write(node, 5, "", value);
}

/* Restore recognised precision modes directly into their native owner, including legacy scalar saves. */
void lab_storage_precisions(int data)
{
    int values = lab_dom_get(data, "precision_bits");
    unsigned type = lab_dom_type(values), mark = lab_dom_mark();
    if (type == 4 || type == 5) {
        static const char *const names[] = {"expression", "equation", "diffequation", "matrix",
                                            "integrator", "datetime", "almanac"};
        /* Seven direct lookups avoid scanning untrusted saved keys or accepting inherited properties. */
        for (unsigned mode = 0; mode < 7; ++mode) {
            int value = lab_dom_key_get(values, lab_dom_string(names[mode]));
            lab_workspace_precision_set(
                mode, lab_browser_precision(lab_dom_parse_int(value, 10), lab_workspace_precision(mode)));
            lab_dom_release(mark);
        }
    } else {
        lab_workspace_precision_set(0,
                                    lab_browser_precision(lab_dom_parse_int(values, 10), lab_workspace_precision(0)));
    }
}

/* Compare opaque local/server copies using the shared timestamp and default-placeholder policy. */
void lab_storage_recovery(unsigned mode, int server, int local, int config)
{
    if (mode > 1) {
        lab_dom_return(0);
        return;
    }
    int server_text = lab_storage_text(lab_dom_get(server, lab_persist_text(mode, 0, 0)), 1);
    int text = lab_storage_text(lab_dom_item(local, 0), 1);
    double saved_at = lab_dom_to_number(lab_storage_or(lab_dom_item(local, 1), lab_dom_numeric(0)));
    double server_at =
        lab_dom_to_number(lab_storage_or(lab_dom_get(server, lab_persist_text(mode, 1, 0)), lab_dom_numeric(0)));
    int placeholder = lab_dom_equal(server_text, lab_dom_get(config, mode ? "DEFAULT_EQUATION" : "DEFAULT_EXPRESSION"));
    if (!lab_workspace_prefer_local(lab_dom_truth(text), saved_at, server_at, lab_dom_truth(server_text),
                                    placeholder)) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(5);
    lab_dom_set(result, "text", text);
    lab_dom_set(result, "updatedAt", lab_dom_numeric(saved_at));
    lab_dom_return(result);
}

/* Assemble a recovered server patch after editor preparation, using one coherent fallback timestamp. */
void lab_storage_recovered(unsigned mode, int recovery, int values, double now)
{
    int patch = lab_dom_object(5);
    if (mode <= 1) {
        int timestamp = lab_storage_or(lab_dom_get(recovery, "updatedAt"), lab_dom_numeric(now));
        lab_dom_set(patch, lab_persist_text(mode, 0, 0), lab_dom_item(values, 0));
        lab_dom_set(patch, lab_persist_text(mode, 1, 0), timestamp);
    }
    lab_dom_return(patch);
}

/* Split restoration around awaits: plan, guarded controls, guarded bounds completion, then final editor. */
void lab_storage_history(unsigned phase, int state, int config)
{
    int mode = lab_dom_get(state, "mode");
    int equation = lab_dom_equal(mode, lab_dom_string("equation"));
    int matrix = lab_dom_equal(mode, lab_dom_string("matrix"));
    int integrator = lab_dom_equal(mode, lab_dom_string("integrator"));
    int datetime = lab_dom_equal(mode, lab_dom_string("datetime"));
    int calendar = datetime || lab_dom_equal(mode, lab_dom_string("almanac"));
    if (!phase) {
        int plan = lab_dom_object(5);
        lab_dom_set(plan, "mode", mode);
        lab_dom_set(plan, "text", lab_storage_or(lab_dom_get(state, "text"), lab_dom_string("")));
        lab_dom_set(plan, "calendar", lab_dom_scalar(1, calendar));
        lab_dom_set(plan, "integrator", lab_dom_scalar(1, integrator));
        lab_dom_set(plan, "bounds",
                    lab_storage_or(lab_dom_get(state, "bounds"), lab_dom_get(config, "DEFAULT_INTEGRATOR_BOUNDS")));
        lab_dom_set(plan, "calendarState", lab_storage_or(lab_dom_key_get(state, mode), lab_dom_object(5)));
        lab_dom_return(plan);
    } else if (phase == 1 && equation) {
        int value = lab_storage_text(lab_dom_get(state, "variable"), 1);
        lab_dom_write(lab_dom_query(0, "#equationVariable"), 5, "",
                      lab_storage_or(value, lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE")));
    } else if (phase == 1 && matrix) {
        lab_dom_write(lab_dom_query(0, "#matrixOperation"), 5, "",
                      lab_storage_or(lab_dom_get(state, "operation"), lab_dom_string("eval")));
        lab_dom_write(lab_dom_query(0, "#matrixOperand"), 5, "", lab_storage_text(lab_dom_get(state, "operand"), 1));
    } else if (phase == 2 && integrator) {
        lab_dom_write(lab_dom_query(0, "#integratorIntervalCap"), 5, "",
                      lab_dom_numeric(lab_storage_cap(lab_dom_get(state, "intervalCap"), config)));
    } else if (phase == 3 && calendar) {
        lab_dom_write(lab_dom_query(0, "#expr"), 5, "",
                      lab_dom_get(config, datetime ? "DEFAULT_DATETIME_TEXT" : "DEFAULT_ALMANAC_TEXT"));
    }
}
