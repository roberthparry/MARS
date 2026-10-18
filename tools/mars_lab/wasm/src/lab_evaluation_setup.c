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
#include <stdbool.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_editor.h"
#include "lab_evaluation.h"
#include "lab_evaluation_setup.h"

static void lab_evaluation_setup_emit(browser_handle calls, const char *name, unsigned count, browser_handle first,
                                browser_handle second)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(name));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static browser_handle lab_evaluation_setup_input(lab_mode_t mode, view_handle view)
{
    /** @brief Modes whose authored editor text is trimmed, selected only after mode validation. */
    static const unsigned trim[LAB_MODE_COUNT] = {
        [LAB_MODE_EQUATION]              = 1,
        [LAB_MODE_DIFFERENTIAL_EQUATION] = 1,
    };
    int text = lab_editor_current_value(view);
    if (!lab_dom_truth(text)) {
        text = lab_dom_get(view, "bodyText");
        if (trim[mode]) {
            if (!lab_dom_truth(text))
                text = lab_dom_string("");
        } else {
            return lab_dom_clean(text, LAB_DOM_CLEAN_TRIM);
        }
    }
    return trim[mode] ? lab_dom_clean(text, LAB_DOM_CLEAN_TRIM) : text;
}

/** @brief Validated worksheet mode and scoped records borrowed by one preparation stage. */
struct lab_evaluation_setup_context {
    lab_mode_t mode;
    browser_handle context, options;
    view_handle view;
    browser_handle calls;
};

/* Capture visible bindings before requesting their ordered commit. */
static bool lab_evaluation_setup_bindings(struct lab_evaluation_setup_context *ctx)
{
    if (ctx->mode == LAB_MODE_EXPRESSION)
        lab_evaluation_setup_emit(ctx->calls, "captureBindings", 1, ctx->context, 0);
    /* The shared mode policy owns which worksheets commit visible bindings. */
    bool wait = (lab_evaluation_prepare(ctx->mode, true) & 1u) != 0;
    if (wait)
        lab_evaluation_setup_emit(ctx->calls, "commitBindings", 0, 0, 0);
    return wait;
}

/* Preserve editor reads before the deferred assembly service. */
static bool lab_evaluation_setup_expression(struct lab_evaluation_setup_context *ctx)
{
    lab_dom_set(ctx->context, "editorText", lab_editor_current_value(ctx->view));
    int body = lab_dom_get(ctx->view, "bodyText");
    lab_dom_set(ctx->context, "editorBodyText",
                lab_dom_clean(lab_dom_truth(body) ? body : lab_dom_string(""), LAB_DOM_CLEAN_TRIM));
    lab_evaluation_setup_emit(ctx->calls, "assemble", 1, ctx->context, 0);
    return true;
}

/* Capture authored source for worksheets without asynchronous assembly. */
static bool lab_evaluation_setup_authored(struct lab_evaluation_setup_context *ctx)
{
    lab_dom_set(ctx->context, "text", lab_evaluation_setup_input(ctx->mode, ctx->view));
    return false;
}

/* Request the host snapshot belonging to the validated calendar mode. */
static bool lab_evaluation_setup_calendar(struct lab_evaluation_setup_context *ctx)
{
    /** @brief Host snapshot services for the two calendar worksheet modes. */
    static const char *const snapshots[LAB_MODE_COUNT] = {
        [LAB_MODE_DATETIME] = "captureDatetime",
        [LAB_MODE_ALMANAC]  = "captureAlmanac",
    };
    lab_evaluation_setup_emit(ctx->calls, snapshots[ctx->mode], 1, ctx->context, 0);
    return false;
}

/* Select input preparation only after the public entry point validates the mode. */
static bool lab_evaluation_setup_capture(struct lab_evaluation_setup_context *ctx)
{
    /** @brief Input preparation routines indexed directly by worksheet mode. */
    static bool (*const handlers[LAB_MODE_COUNT])(struct lab_evaluation_setup_context *) = {
        [LAB_MODE_EXPRESSION]            = lab_evaluation_setup_expression,
        [LAB_MODE_EQUATION]              = lab_evaluation_setup_authored,
        [LAB_MODE_DIFFERENTIAL_EQUATION] = lab_evaluation_setup_authored,
        [LAB_MODE_MATRIX]                = lab_evaluation_setup_authored,
        [LAB_MODE_INTEGRATOR]            = lab_evaluation_setup_authored,
        [LAB_MODE_DATETIME]              = lab_evaluation_setup_calendar,
        [LAB_MODE_ALMANAC]               = lab_evaluation_setup_calendar,
    };
    return handlers[ctx->mode](ctx);
}

/* Reuse precision input only after binding assembly, then save the editor state. */
static bool lab_evaluation_setup_assembled(struct lab_evaluation_setup_context *ctx)
{
    int text = 0;
    if (lab_dom_truth(lab_dom_get(ctx->options, "reuseLastInput")))
        text = lab_dom_get(ctx->view, "lastInput");
    if (!lab_dom_truth(text)) {
        text = lab_dom_get(ctx->context, "entered");
        if (!lab_dom_truth(text))
            text = lab_dom_get(ctx->context, "editorText");
    }
    lab_dom_set(ctx->context, "text", text);
    int editor = lab_dom_get(ctx->context, "editorText");
    lab_evaluation_setup_emit(ctx->calls, "saveWorksheetState", 2, lab_dom_string("expression"),
                              lab_dom_truth(editor) ? editor : text);
    return false;
}

/* Each stage runs synchronously; only the published final service may require a browser await. */
void lab_evaluation_setup(lab_mode_t mode, lab_evaluation_setup_phase_t phase, browser_handle context,
                          browser_handle options, view_handle view)
{
    /** @brief Ordered preparation stages returning whether their final service must be awaited. */
    static bool (*const handlers[])(struct lab_evaluation_setup_context *) = {
        [LAB_EVALUATION_SETUP_BINDINGS]  = lab_evaluation_setup_bindings,
        [LAB_EVALUATION_SETUP_INPUT]     = lab_evaluation_setup_capture,
        [LAB_EVALUATION_SETUP_ASSEMBLED] = lab_evaluation_setup_assembled,
    };
    if ((unsigned)mode >= LAB_MODE_COUNT || (unsigned)phase >= sizeof handlers / sizeof *handlers ||
        (phase == LAB_EVALUATION_SETUP_ASSEMBLED && mode != LAB_MODE_EXPRESSION)) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    int done = phase == LAB_EVALUATION_SETUP_ASSEMBLED ||
               (phase == LAB_EVALUATION_SETUP_INPUT && mode != LAB_MODE_EXPRESSION);
    lab_dom_set(plan, "calls", calls);
    struct lab_evaluation_setup_context ctx = {mode, context, options, view, calls};
    bool wait = handlers[phase](&ctx);
    lab_dom_set(plan, "wait", lab_dom_scalar(LAB_VALUE_BOOLEAN, wait));
    lab_dom_set(plan, "done", lab_dom_scalar(LAB_VALUE_BOOLEAN, done));
    lab_dom_return(plan);
}
