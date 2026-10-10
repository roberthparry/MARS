/**
 * @file lab_flow_state_sync.c
 * @brief Synchronous worksheet save, history and workspace projection plans.
 *
 * Keeps policy in C without turning synchronous browser APIs into promises. Each
 * plan performs one allowlisted capability; the host resumes immediately with its
 * result. Deferred saves use the existing native serial owner and read keepalive
 * only when publishing. Failed browser storage does not suppress server saves.
 */
#include "lab_dom.h"
#include "lab_events.h"
#include "lab_persist.h"
#include "lab_workspace.h"
#include "lab_flow.h"
#include "lab_flow_state_sync.h"

static unsigned lab_state_sync_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static unsigned lab_state_sync_index(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
}

static int lab_state_sync_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_state_sync_next(int frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

static int lab_state_sync_save(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    int text = lab_dom_get(frame, "text");
    switch (lab_state_sync_stage(frame)) {
        case 0:
            if (!lab_persist_count(index))
                return lab_flow_done(0);
            text = lab_dom_clean(lab_dom_text(lab_state_sync_or(text, lab_dom_string(""))), 0);
            lab_dom_set(frame, "text", text);
            if (lab_persist_canonical(index))
                return lab_flow_call(frame, 1, "stateCanonical", "text", 0, 1, text);
            /* Fall through when no prepared canonical metadata is needed. */
        case 1:
            return lab_flow_call(frame, 2, "stateNow", "updatedAt", 0, 0);
        case 2:
            if (!index)
                return lab_flow_call(frame, 3, "stateTimestamp", 0, 0, 1, lab_dom_get(frame, "updatedAt"));
            /* Fall through for non-expression worksheets. */
        case 3:
            return lab_flow_call(frame, 4, "stateRetainText", 0, 0, 2, lab_dom_numeric(index), text);
        case 4:
            if (index == 3)
                return lab_flow_call(frame, 5, "stateMatrixOperation", "operation", 0, 0);
            if (index == 4)
                return lab_flow_call(frame, 6, "stateCurrentBounds", "bounds", 0, 0);
            goto records;
        case 5:
            return lab_flow_call(frame, 7, "stateMatrixOperand", "operand", 0, 0);
        case 6:
            return lab_flow_call(frame, 7, "stateIntervalCap", "cap", 0, 0);
        case 7:
        records:
            {
                int values = lab_dom_object(4);
                lab_dom_push(values, text);
                lab_dom_push(values, lab_dom_get(frame, "updatedAt"));
                lab_dom_push(values, index == 3 ? lab_dom_get(frame, "operation") : lab_dom_string(""));
                lab_dom_push(values, index == 3 ? lab_dom_get(frame, "operand") : lab_dom_string(""));
                lab_dom_push(values, index == 4 ? lab_dom_get(frame, "bounds") : lab_dom_string(""));
                lab_dom_push(values, index == 4 ? lab_dom_get(frame, "cap") : lab_dom_numeric(0));
                return lab_flow_call(frame, 8, "stateRecords", "records", 0, 2, lab_dom_numeric(index), values);
            }
        case 8:
            lab_dom_set(frame, "failed", lab_dom_numeric(9));
            return lab_flow_call(frame, 9, "stateLocalRecords", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "records"), "local"));
        case 9: {
            lab_dom_delete(frame, "failed");
            unsigned token = lab_persist_begin(index, lab_dom_truth(text));
            if (!token)
                return lab_flow_done(0);
            lab_dom_set(frame, "token", lab_dom_numeric(token));
            lab_dom_set(frame, "patch", lab_dom_get(lab_dom_get(frame, "records"), "server"));
            return lab_flow_call(frame, 10, "stateCancelSaveTimer", 0, 0, 1, lab_dom_numeric(index));
        }
        case 10: {
            unsigned delay =
                lab_persist_delay(index, lab_dom_truth(lab_dom_get(lab_dom_get(frame, "options"), "debounce")));
            lab_dom_set(frame, "delay", lab_dom_numeric(delay));
            return lab_flow_call(frame, 11, delay ? "stateScheduleSave" : "statePublishSave", 0, 0, 1, frame);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_publish(int frame)
{
    int save = lab_dom_get(frame, "save");
    unsigned index = lab_state_sync_index(save);
    switch (lab_state_sync_stage(frame)) {
        case 0:
            if (!lab_persist_take(index, (unsigned)lab_dom_number(lab_dom_get(save, "token"))))
                return lab_flow_done(0);
            return lab_flow_call(frame, 1, "stateForgetSaveTimer", 0, 0, 1, lab_dom_numeric(index));
        case 1: {
            int options = lab_dom_object(5);
            int keepalive = !lab_dom_truth(lab_dom_get(save, "delay")) &&
                            lab_dom_truth(lab_dom_get(lab_dom_get(save, "options"), "keepalive"));
            lab_dom_set(options, "keepalive", lab_dom_scalar(1, keepalive));
            return lab_flow_call(frame, 2, "stateSave", 0, 0, 2, lab_dom_get(save, "patch"), options);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_calendar(int frame)
{
    int mode = lab_dom_get(frame, "mode"), state = lab_dom_get(frame, "state");
    int datetime = lab_dom_equal(mode, lab_dom_string("datetime"));
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, datetime ? "stateDatetime" : "stateAlmanac", "state", 0, 0);
        case 1:
            return lab_flow_call(frame, 2, "stateDefaultEditor", 0, 0, 2, mode, lab_dom_numeric(datetime ? 5 : 6));
        case 2:
            lab_dom_set(frame, "failed", lab_dom_numeric(3));
            return lab_flow_call(
                frame, 3, "stateWriteCalendar", 0, 0, 2,
                lab_dom_string(datetime ? "mars.exprLab.lastDatetimeState" : "mars.exprLab.lastAlmanacState"), state);
        case 3:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, 4, "stateCalendarPatch", "patch", 0, 2, mode, state);
        case 4:
            return lab_flow_call(frame, 5, "stateSave", 0, 0, 1, lab_dom_get(frame, "patch"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_mode_save(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateMode", "selected", 0, 1, lab_dom_get(frame, "mode"));
        case 1:
            lab_dom_set(frame, "failed", lab_dom_numeric(2));
            return lab_flow_call(frame, 2, "stateWriteMode", 0, 0, 1, lab_dom_get(frame, "selected"));
        case 2: {
            lab_dom_delete(frame, "failed");
            int patch = lab_dom_object(5);
            lab_dom_set(patch, "lab_mode", lab_dom_get(frame, "selected"));
            return lab_flow_call(frame, 3, "stateSave", 0, 0, 1, patch);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_editor(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    int text = lab_dom_get(frame, "text");
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateRestoredText", "text", 0, 1, lab_dom_numeric(index));
        case 1:
            return lab_flow_call(frame, 2, "stateBindingParts", "bound", 0, 1, text);
        case 2:
            if (lab_workspace_editor_bound(index, lab_dom_truth(lab_dom_get(frame, "bound"))))
                return lab_flow_call(frame, 5, "stateSetExpression", 0, 0, 1, text);
            lab_dom_write(lab_dom_id("expr"), 5, "", text);
            return lab_state_sync_next(frame, 3, "stateClearSource");
        case 3:
            return lab_state_sync_next(frame, 5, "stateClearBindings");
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_result(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_state_sync_next(frame, 1, "stateClearFunction");
        case 1:
            return lab_flow_call(frame, 2, "stateResultProjection", "result", 0, 1, lab_dom_get(frame, "mode"));
        case 2:
            if (!lab_dom_truth(lab_dom_get(frame, "result")))
                return lab_state_sync_next(frame, 6, "stateClearResult");
            return lab_flow_call(frame, 3, "stateResultValues", 0, 0, 1, lab_dom_get(frame, "result"));
        case 3:
            return lab_state_sync_next(frame, 4, "stateDerivativeButtons");
        case 4:
            return lab_state_sync_next(frame, 5, "stateRenderedFit");
        case 5:
            return lab_state_sync_next(frame, 6, "stateSolverFit");
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_select(int frame)
{
    int changed = lab_dom_truth(lab_dom_get(frame, "changed"));
    switch (lab_state_sync_stage(frame)) {
        case 0:
            changed = lab_workspace_select(lab_state_sync_index(frame));
            lab_dom_set(frame, "changed", lab_dom_scalar(1, changed));
            if (changed)
                return lab_state_sync_next(frame, 1, "stateNotifyMode");
            goto tabs;
        case 1:
            return lab_state_sync_next(frame, 2, "stateResetEditorSize");
        case 2:
        tabs:
            return lab_state_sync_next(frame, 3, "stateSyncTabs");
        default:
            return lab_flow_done(
                lab_dom_scalar(1, changed || lab_dom_truth(lab_dom_get(lab_dom_get(frame, "options"), "force"))));
    }
}

static int lab_state_sync_apply(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateForceMode", 0, 0, 1, lab_dom_get(frame, "mode"));
        case 1:
            return lab_state_sync_next(frame, 2, "stateRestoreCurrentEditor");
        case 2:
            return lab_flow_call(frame, 3, "stateModeIndex", "index", 0, 0);
        case 3:
            if (index == 4)
                return lab_state_sync_next(frame, 4, "stateRenderActiveRows");
            /* Fall through to the next mode-dependent operation. */
        case 4:
            return lab_flow_call(frame, 5, "stateModeIndex", "index", 0, 0);
        case 5:
            if (index == 5)
                return lab_state_sync_next(frame, 6, "stateDatetimeDefaults");
            goto almanac;
        case 6:
            return lab_state_sync_next(frame, 7, "stateRefreshLocation");
        case 7:
        almanac:
            return lab_flow_call(frame, 8, "stateModeIndex", "index", 0, 0);
        case 8:
            if (index == 6)
                return lab_state_sync_next(frame, 9, "stateAlmanacDefaults");
            goto sync;
        case 9:
            return lab_state_sync_next(frame, 10, "stateSaveAlmanac");
        case 10:
        sync:
            return lab_state_sync_next(frame, 11, "stateSyncUI");
        case 11:
            return lab_flow_call(frame, 12, "stateModeIndex", "index", 0, 0);
        case 12:
            if (index == 4)
                return lab_flow_call(frame, 13, "stateBoundCount", "count", 0, 0);
            goto normalise;
        case 13:
            if (!lab_dom_truth(lab_dom_get(frame, "count")))
                return lab_state_sync_next(frame, 14, "stateResetBounds");
            /* Fall through: intentionally do not await the legacy default-bounds refresh. */
        case 14:
        normalise:
            return lab_flow_call(frame, 15, "stateModeIndex", "index", 0, 0);
        case 15:
            if (index == 4)
                return lab_state_sync_next(frame, 16, "stateNormaliseCap");
            /* Fall through when no integrator control is present. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_keyboard(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateReady", "ready", 0, 0);
        case 1:
            if (!lab_dom_truth(lab_dom_get(frame, "ready")))
                return lab_state_sync_next(frame, 5, "stateHistoryButtons");
            return lab_state_sync_next(frame, 2, "stateClearForward");
        case 2:
            return lab_flow_call(frame, 3, "stateModeIndex", "index", 0, 0);
        case 3: {
            static const char *const services[] = {
                "stateEvaluateExpression", "stateEvaluateEquation", "stateEvaluateDiffequation", "stateEvaluateMatrix",
                "stateEvaluateIntegrator", "stateEvaluateDatetime", "stateEvaluateExpression"};
            unsigned index = lab_state_sync_index(frame);
            return lab_state_sync_next(frame, 5, services[index < 7 ? index : 0]);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_history(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    switch (lab_state_sync_stage(frame)) {
        case 0:
            if (index == 5 || index == 6)
                return lab_flow_call(frame, 1, index == 5 ? "stateDatetime" : "stateAlmanac", "calendar", 0, 0);
            /* Fall through for mathematical worksheets. */
        case 1:
            if (lab_dom_type(lab_dom_get(frame, "textOverride"))) {
                lab_dom_set(frame, "text", lab_dom_get(frame, "textOverride"));
                goto bounds;
            }
            return lab_flow_call(frame, 2,
                                 index == 5   ? "stateDatetimeSummary"
                                 : index == 6 ? "stateAlmanacSummary"
                                              : "stateEditorText",
                                 "text", 0, 1, lab_dom_get(frame, "calendar"));
        case 2:
        bounds:
            if (index == 4)
                return lab_flow_call(frame, 3, "stateCurrentBounds", "bounds", 0, 0);
            goto project;
        case 3:
            return lab_flow_call(frame, 4, "stateHistoryCap", "cap", 0, 0);
        case 4:
        project:
            return lab_flow_call(frame, 5, "stateHistoryRecord", "record", 0, 5, lab_dom_numeric(index),
                                 lab_dom_get(frame, "text"), lab_dom_get(frame, "calendar"),
                                 index == 4 ? lab_dom_get(frame, "bounds") : lab_dom_string(""),
                                 index == 4 ? lab_dom_get(frame, "cap") : lab_dom_string(""));
        default:
            return lab_flow_done(lab_dom_get(frame, "record"));
    }
}

static int lab_state_sync_push(int frame)
{
    int entry = lab_dom_get(frame, "entry");
    switch (lab_state_sync_stage(frame)) {
        case 0:
            if (lab_dom_type(entry) == 3)
                return lab_flow_call(frame, 1, "stateCurrentHistory", "snapshot", 0, 1, entry);
            if (!lab_dom_truth(entry))
                return lab_flow_call(frame, 1, "stateCurrentHistory", "snapshot", 0, 0);
            lab_dom_set(frame, "snapshot", entry);
            /* Fall through with the supplied opaque snapshot. */
        case 1:
            return lab_flow_call(frame, 2, "statePushSnapshot", 0, 0, 1, lab_dom_get(frame, "snapshot"));
        case 2:
            return lab_state_sync_next(frame, 3, "stateHistoryButtons");
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_control(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    static const char *const modes[] = {"matrix", "equation", "integrator"};
    if (index >= 3)
        return lab_flow_done(0);
    int node = lab_dom_item(lab_dom_get(frame, "controls"), index);
    switch (lab_state_sync_stage(frame)) {
        case 0: {
            if (!node)
                return lab_flow_done(0);
            int value = lab_dom_read(node, 5, "");
            if (index != 1)
                return lab_flow_call(frame, 1, index ? "stateValidCap" : "stateValidOperation", "value", 0, 1, value);
            int fallback = lab_dom_get(lab_dom_get(frame, "config"), "DEFAULT_EQUATION_VARIABLE");
            value = lab_dom_clean(lab_dom_text(lab_state_sync_or(value, fallback)), 0);
            lab_dom_set(frame, "value", lab_state_sync_or(value, fallback));
            /* Fall through after native equation-variable normalisation. */
        }
        case 1:
            lab_dom_write(node, 5, "", lab_dom_get(frame, "value"));
            if (!index)
                return lab_state_sync_next(frame, 2, "stateSyncMatrix");
            /* Fall through: only matrix operation changes need matrix UI synchronisation. */
        case 2:
            return lab_flow_call(frame, 3, "currentMode", "mode", 0, 0);
        case 3:
            if (lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string(modes[index])))
                return lab_flow_call(frame, 4, "stateSaveWorksheet", 0, 0, 1, lab_dom_string(modes[index]));
            /* Fall through when changing an inactive control. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_flush(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "currentMode", "mode", 0, 0);
        case 1:
            return lab_flow_call(frame, 2, "stateNamedIndex", "index", 0, 1, lab_dom_get(frame, "mode"));
        case 2:
            if (lab_events_flush(lab_state_sync_index(frame)))
                return lab_flow_call(frame, 3, "stateFlushSave", 0, 0, 1, lab_dom_get(frame, "mode"));
            /* Fall through for calendars. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_location(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateModeIndex", "index", 0, 0);
        case 1:
            if (lab_state_sync_index(frame) == 5)
                return lab_state_sync_next(frame, 2, "stateSaveDatetime");
            /* Fall through: a later worksheet owns the visible state. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_integrator_result(int frame)
{
    int rows = lab_dom_get(frame, "rows");
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateMergeRows", "rows", 0, 1, lab_dom_get(frame, "data"));
        case 1:
            if (lab_dom_type(rows) == 1 && !lab_dom_truth(rows))
                return lab_flow_call(frame, 2, "fail", 0, 0, 1,
                                     lab_dom_string("Integrator result exceeds the 256-row form limit"));
            if (lab_dom_truth(rows))
                return lab_flow_call(frame, 2, "stateRenderRows", 0, 0, 1, rows);
            /* Fall through for an absent row update. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_capture_save(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    switch (lab_state_sync_stage(frame)) {
        case 0:
            if (!index)
                return lab_flow_call(frame, 1, "stateExpressionSavedText", "text", 0, 0);
            return lab_flow_call(frame, 2, "stateSaveNamedEditor", 0, 0, 1, lab_dom_numeric(index));
        case 1:
            return lab_flow_call(frame, 2, "stateSaveWorksheet", 0, 0, 2, lab_dom_string("expression"),
                                 lab_dom_get(frame, "text"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_state_sync_needs_bounds(int frame)
{
    switch (lab_state_sync_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateModeIndex", "index", 0, 0);
        case 1:
            if (lab_state_sync_index(frame) != 4)
                return lab_flow_done(lab_dom_scalar(1, 0));
            return lab_flow_call(frame, 2, "stateBoundCount", "count", 0, 0);
        default:
            return lab_flow_done(lab_dom_scalar(1, lab_dom_number(lab_dom_get(frame, "count")) == 0));
    }
}

/* Browser callbacks retain resources, while this fixed dispatch owns synchronous decisions. */
void lab_state_sync(unsigned kind, int frame)
{
    static int (*const handlers[])(int) = {
        lab_state_sync_save,         lab_state_sync_publish,     lab_state_sync_calendar,
        lab_state_sync_mode_save,    lab_state_sync_editor,      lab_state_sync_result,
        lab_state_sync_select,       lab_state_sync_apply,       lab_state_sync_keyboard,
        lab_state_sync_history,      lab_state_sync_push,        lab_state_sync_control,
        lab_state_sync_flush,        lab_state_sync_location,    lab_state_sync_integrator_result,
        lab_state_sync_capture_save, lab_state_sync_needs_bounds};
    lab_dom_return(kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0));
}
