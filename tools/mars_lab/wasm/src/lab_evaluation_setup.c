/**
 * @file lab_evaluation_setup.c
 * @brief Ordered request preparation and authored-input selection for Lab evaluation.
 *
 * Separates synchronous worksheet policy from browser promises: visible bindings
 * are captured before their commit, editor text afterwards, and precision reuse
 * only after asynchronous binding assembly. Calendar state remains a host service
 * because its existing native projection needs its own scoped call. No mathematical
 * text is parsed or rewritten, and no scoped handles survive a function return.
 */
#include "lab_dom.h"
#include "lab_editor.h"
#include "lab_evaluation.h"
#include "lab_evaluation_setup.h"

static void lab_evaluation_setup_emit(int calls, const char *name, unsigned count, int first, int second)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(name));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static int lab_evaluation_setup_input(unsigned mode, int view)
{
    int text = lab_editor_current_value(view);
    if (!lab_dom_truth(text)) {
        text = lab_dom_get(view, "bodyText");
        if (mode == 1 || mode == 2) {
            if (!lab_dom_truth(text))
                text = lab_dom_string("");
        } else {
            return lab_dom_clean(text, 0);
        }
    }
    return mode == 1 || mode == 2 ? lab_dom_clean(text, 0) : text;
}

/* Each stage runs synchronously; only the published final service may require a browser await. */
void lab_evaluation_setup(unsigned mode, unsigned phase, int context, int options, int view)
{
    if (mode > 6 || phase > 2 || (phase == 2 && mode != 0)) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    int wait = 0, done = phase == 2 || (phase == 1 && mode != 0);
    lab_dom_set(plan, "calls", calls);
    if (phase == 0) {
        if (mode == 0)
            lab_evaluation_setup_emit(calls, "captureBindings", 1, context, 0);
        /* The shared mode policy owns which worksheets commit visible bindings. */
        wait = (lab_evaluation_prepare(mode, 1) & 1u) != 0;
        if (wait)
            lab_evaluation_setup_emit(calls, "commitBindings", 0, 0, 0);
    } else if (phase == 1) {
        if (mode >= 5) {
            static const char *const snapshots[] = {"captureDatetime", "captureAlmanac"};
            lab_evaluation_setup_emit(calls, snapshots[mode - 5], 1, context, 0);
        } else if (mode == 0) {
            lab_dom_set(context, "editorText", lab_editor_current_value(view));
            int body = lab_dom_get(view, "bodyText");
            lab_dom_set(context, "editorBodyText", lab_dom_clean(lab_dom_truth(body) ? body : lab_dom_string(""), 0));
            lab_evaluation_setup_emit(calls, "assemble", 1, context, 0);
            wait = 1;
        } else {
            lab_dom_set(context, "text", lab_evaluation_setup_input(mode, view));
        }
    } else {
        int text = 0;
        if (lab_dom_truth(lab_dom_get(options, "reuseLastInput")))
            text = lab_dom_get(view, "lastInput");
        if (!lab_dom_truth(text)) {
            text = lab_dom_get(context, "entered");
            if (!lab_dom_truth(text))
                text = lab_dom_get(context, "editorText");
        }
        lab_dom_set(context, "text", text);
        int editor = lab_dom_get(context, "editorText");
        lab_evaluation_setup_emit(calls, "saveWorksheetState", 2, lab_dom_string("expression"),
                                  lab_dom_truth(editor) ? editor : text);
    }
    lab_dom_set(plan, "wait", lab_dom_scalar(1, wait));
    lab_dom_set(plan, "done", lab_dom_scalar(1, done));
    lab_dom_return(plan);
}
