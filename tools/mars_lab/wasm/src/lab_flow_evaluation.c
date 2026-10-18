/**
 * @file lab_flow_evaluation.c
 * @brief Native continuations for worksheet evaluation and result installation.
 *
 * Owns all seven modes' preparation, history, transport, recovery and completion
 * ordering. Explicit stages are suspension points, not mathematical cases. Existing
 * native presentation plans remain authoritative; the browser only executes their
 * capabilities. A request-local frame preserves values across awaits without
 * retaining scoped handles or sharing mutable state between concurrent requests.
 * All evaluation modes use one fetch capability with a native-selected kind;
 * only expression evaluation passes captured text to that capability.
 */
#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_evaluation_setup.h"
#include "lab_evaluation_install.h"
#include "lab_plot_slices.h"
#include "lab_layout.h"
#include "lab_flow.h"

enum {
    lab_flow_eval_start,
    lab_flow_eval_prepare,
    lab_flow_eval_setup,
    lab_flow_eval_setup_apply,
    lab_flow_eval_setup_current,
    lab_flow_eval_setup_next,
    lab_flow_eval_history,
    lab_flow_eval_previous,
    lab_flow_eval_input,
    lab_flow_eval_show,
    lab_flow_eval_status,
    lab_flow_eval_guard,
    lab_flow_eval_fetch,
    lab_flow_eval_outcome,
    lab_flow_eval_response,
    lab_flow_eval_recover,
    lab_flow_eval_install,
    lab_flow_eval_installed,
    lab_flow_eval_commit,
    lab_flow_eval_ready,
    lab_flow_eval_weather_state,
    lab_flow_eval_plot_scheduled,
    lab_flow_eval_weather,
    lab_flow_eval_finally,
    lab_flow_eval_finish,
    lab_flow_eval_done,
    lab_flow_eval_catch,
    lab_flow_eval_caught,
    lab_flow_eval_error_plan,
    lab_flow_eval_error_effects,
    lab_flow_eval_rethrow_current,
    lab_flow_eval_rethrow_history,
    lab_flow_eval_rethrow
};

static int lab_flow_eval_native(int frame, unsigned next, const char *entry, int mode, int phase, int context,
                                int options, int view)
{
    return lab_flow_call(frame, next, "native", "plan", 0, 6, lab_dom_string(entry), mode, phase, context, options,
                         view);
}

static int lab_flow_eval_current(int frame, unsigned next)
{
    return lab_flow_call(frame, next, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
}

/* Each handler advances one stage of the request-local continuation graph. */
struct lab_flow_evaluation_context {
    int frame;
    int view;
    int mode;
    int options;
    int context;
    int request;
    int preparation;
    int plan;
    int data;
};

/**
 * @brief Request the native setup plan for the current evaluation phase.
 * @param ctx Evaluation state borrowing the frame phase, mode, captured input context, options and view.
 * @return Scoped operation-plan handle that stores the setup plan and resumes at setup application.
 */
static int lab_flow_evaluation_setup(struct lab_flow_evaluation_context *ctx);

/**
 * @brief Record accepted input when required, otherwise reveal the result cards.
 * @param ctx Evaluation state carrying preparation flags, request ownership and captured expression text.
 * @return Scoped plan for input acceptance or result visibility.
 */
static int lab_flow_evaluation_previous(struct lab_flow_evaluation_context *ctx);

/**
 * @brief Reveal the result cards before updating evaluation status.
 * @param ctx Evaluation state whose frame advances to the status stage.
 * @return Scoped plan calling the browser result-visibility service.
 */
static int lab_flow_evaluation_show(struct lab_flow_evaluation_context *ctx);

/**
 * @brief Dispatch the mode-specific evaluation request.
 * @param ctx Evaluation state carrying the numeric mode, request handle and captured expression text.
 * @return Scoped fetch plan awaiting a response, or a completion plan for an invalid mode.
 */
static int lab_flow_evaluation_fetch(struct lab_flow_evaluation_context *ctx);

/**
 * @brief Propagate the retained evaluation exception after recovery.
 * @param ctx Evaluation state whose frame holds the exception and advances to completion.
 * @return Scoped plan asking the host to rethrow the stored exception.
 */
static int lab_flow_evaluation_rethrow(struct lab_flow_evaluation_context *ctx);

static int lab_flow_evaluation_start(struct lab_flow_evaluation_context *ctx)
{
    lab_dom_set(ctx->frame, "context", lab_dom_object(LAB_VALUE_OBJECT));
    lab_dom_set(ctx->frame, "phase", lab_dom_numeric(LAB_EVALUATION_SETUP_BINDINGS));
    return lab_flow_call(ctx->frame, lab_flow_eval_prepare, "native", "preparation", 0, 3,
                         lab_dom_string("lab_evaluation_prepare_plan"), ctx->mode,
                         lab_dom_numeric(lab_dom_truth(lab_dom_get(ctx->options, "skipHistoryUpdate"))));
}

static int lab_flow_evaluation_prepare(struct lab_flow_evaluation_context *ctx)
{
    if (!ctx->preparation)
        return lab_flow_done(0);
    /* Continue: the first preparation stage uses the newly allocated frame. */
    return lab_flow_evaluation_setup(ctx);
}

static int lab_flow_evaluation_setup(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_native(ctx->frame, lab_flow_eval_setup_apply, "lab_evaluation_setup", ctx->mode,
                                lab_dom_get(ctx->frame, "phase"), ctx->context, ctx->options, ctx->view);
}

static int lab_flow_evaluation_setup_apply(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_effects(ctx->frame,
                            lab_dom_truth(lab_dom_get(ctx->plan, "wait")) ? lab_flow_eval_setup_current
                                                                          : lab_flow_eval_setup_next,
                            ctx->plan);
}

static int lab_flow_evaluation_setup_current(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_setup_next);
}

static int lab_flow_evaluation_setup_next(struct lab_flow_evaluation_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->plan, "wait")) && !lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    if (!lab_dom_truth(lab_dom_get(ctx->plan, "done"))) {
        lab_dom_set(ctx->frame, "phase", lab_dom_numeric(lab_dom_number(lab_dom_get(ctx->frame, "phase")) + 1));
        return lab_flow_eval_native(ctx->frame, lab_flow_eval_setup_apply, "lab_evaluation_setup", ctx->mode,
                                    lab_dom_get(ctx->frame, "phase"), ctx->context, ctx->options, ctx->view);
    }
    return lab_flow_call(ctx->frame, lab_flow_eval_history, "historyState", "nextState", 0, 1,
                         lab_dom_get(ctx->context, "text"));
}

static int lab_flow_evaluation_history(struct lab_flow_evaluation_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->preparation, "history")))
        return lab_flow_call(ctx->frame, lab_flow_eval_previous, "previousHistory", "previousState", 0, 1,
                             lab_dom_get(ctx->frame, "nextState"));
    /* Continue: no history lookup is needed for precision-only evaluation. */
    return lab_flow_evaluation_previous(ctx);
}

static int lab_flow_evaluation_previous(struct lab_flow_evaluation_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->preparation, "input")))
        return lab_flow_call(ctx->frame, lab_flow_eval_input, "requestInput", "accepted", 0, 2, ctx->request,
                             lab_dom_get(ctx->context, "text"));
    return lab_flow_call(ctx->frame, lab_flow_eval_status, "showResults", 0, 0, 0);
}

static int lab_flow_evaluation_input(struct lab_flow_evaluation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "accepted")))
        return lab_flow_done(0);
    /* Continue after successful native input validation. */
    return lab_flow_evaluation_show(ctx);
}

static int lab_flow_evaluation_show(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_status, "showResults", 0, 0, 0);
}

static int lab_flow_evaluation_status(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_guard, "setStatus", 0, 0, 1, lab_dom_get(ctx->preparation, "label"));
}

static int lab_flow_evaluation_guard(struct lab_flow_evaluation_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(lab_flow_eval_catch));
    if (lab_dom_truth(lab_dom_get(ctx->frame, "previousState")))
        return lab_flow_call(ctx->frame, lab_flow_eval_fetch, "pushHistory", 0, 0, 1,
                             lab_dom_get(ctx->frame, "previousState"));
    /* Continue: absent history has no browser effect. */
    return lab_flow_evaluation_fetch(ctx);
}

static int lab_flow_evaluation_fetch(struct lab_flow_evaluation_context *ctx)
{
    static const char *const kinds[] = {"expression", "equation", "diffequation", "matrix",
                                       "integrator", "datetime", "almanac"};
    lab_mode_t index = (lab_mode_t)(unsigned)lab_dom_number(ctx->mode);
    if ((unsigned)index >= LAB_MODE_COUNT)
        return lab_flow_done(0);
    int fetch = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(fetch, "kind", lab_dom_string(kinds[index]));
    if (index == LAB_MODE_EXPRESSION)
        lab_dom_set(fetch, "text", lab_dom_get(ctx->context, "text"));
    return lab_flow_call(ctx->frame, lab_flow_eval_outcome, "fetch", "response", 1, 2, fetch, ctx->request);
}

static int lab_flow_evaluation_outcome(struct lab_flow_evaluation_context *ctx)
{
    int response = lab_dom_get(ctx->frame, "response");
    ctx->data = lab_dom_get(response, "data");
    lab_dom_set(ctx->frame, "data", ctx->data);
    return lab_flow_call(ctx->frame, lab_flow_eval_response, "requestOutcome", "outcome", 0, 3, ctx->request,
                         lab_dom_get(response, "response"), ctx->data);
}

static int lab_flow_evaluation_response(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_recover, "native", "responsePlan", 0, 6,
                         lab_dom_string("lab_evaluation_response"), ctx->mode, lab_dom_get(ctx->frame, "outcome"),
                         ctx->data, ctx->context, 0);
}

static int lab_flow_evaluation_recover(struct lab_flow_evaluation_context *ctx)
{
    ctx->plan = lab_dom_get(ctx->frame, "responsePlan");
    if (!ctx->plan)
        return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
    return lab_flow_effects(ctx->frame, lab_flow_eval_install, ctx->plan);
}

static int lab_flow_evaluation_install(struct lab_flow_evaluation_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "responsePlan"), "install")))
        return lab_flow_call(ctx->frame, lab_flow_eval_installed, "installEvaluation", 0, 1, 5, ctx->mode, ctx->data,
                             ctx->context, lab_dom_get(ctx->frame, "outcome"), ctx->request);
    return lab_flow_call(ctx->frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
}

static int lab_flow_evaluation_installed(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_commit);
}

static int lab_flow_evaluation_commit(struct lab_flow_evaluation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
    return lab_flow_call(ctx->frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
}

static int lab_flow_evaluation_ready(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_weather_state, "setStatus", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "responsePlan"), "status"));
}

static int lab_flow_evaluation_weather_state(struct lab_flow_evaluation_context *ctx)
{
    if ((lab_dom_number(ctx->mode) <= 2 || lab_dom_number(ctx->mode) == 4) &&
        lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "responsePlan"), "install")) &&
        lab_dom_truth(lab_dom_get(ctx->frame, "current"))) {
        int range = lab_dom_id("plotRange"), source = lab_dom_get(ctx->context, "text");
        int previous = lab_dom_read(range, LAB_DOM_DATASET, "plotSource");
        if (lab_dom_length(previous) && !lab_dom_equal(source, previous)) {
            static const char *const limits[] = {"plotLower",       "plotUpper",  "plotSecondLower",
                                                 "plotSecondUpper", "plotYLower", "plotYUpper"};
            for (unsigned i = 0; i < sizeof limits / sizeof *limits; ++i) {
                int control = lab_dom_id(limits[i]);
                lab_dom_write(control, LAB_DOM_VALUE, "", lab_dom_string(""));
                lab_dom_delete(control, "marsZoomValue");
                lab_dom_delete(control, "marsZoomText");
            }
            lab_dom_write(lab_dom_id("plotScale"), LAB_DOM_VALUE, "", lab_dom_string("auto"));
            lab_dom_delete(range, "marsZoomOriginal");
            lab_dom_delete(range, "marsZoomChart");
            lab_dom_delete(lab_dom_id("plotRendered"), "marsComplexPlaneBounds");
        }
        if (!lab_dom_equal(previous, source))
            lab_plot_slices_clear();
        else
            lab_plot_slices_auto();
        lab_dom_write(range, LAB_DOM_DATASET, "plotSource", source);
        /* Retain symbolic cards without scheduling plots for constants or unresolved native operations. */
        int unavailable = lab_dom_equal(lab_dom_get(ctx->data, "plot_available"), lab_dom_string("no"));
        if (lab_dom_number(ctx->mode) == 0 &&
            (unavailable || !lab_dom_count(lab_dom_get(range, "marsPlotNames")))) {
            lab_plot_slices_clear();
            lab_layout_plot_camera(1);
            int plot = lab_dom_id("plotRendered");
            lab_dom_write(plot, LAB_DOM_TEXT, "", lab_dom_string(""));
            lab_dom_remove(plot, "data-plot-pending");
            lab_dom_remove(plot, "data-display-svg");
            lab_dom_remove(plot, "data-display-tex");
            lab_dom_write(plot, LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string("false"));
            lab_dom_class(plot, "error", 0);
            lab_dom_class(lab_dom_id("plotCard"), "hidden", 1);
            return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
        }
        lab_layout_plot(0, 0);
        return lab_flow_call(ctx->frame, lab_flow_eval_plot_scheduled, "automaticPlot", 0, 0, 1, ctx->request);
    }
    if (lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "responsePlan"), "weather")))
        return lab_flow_call(ctx->frame, lab_flow_eval_weather, "native", "weatherState", 0, 3,
                             lab_dom_string("lab_evaluation_weather_state"), lab_dom_get(ctx->context, "state"),
                             ctx->data);
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
}

static int lab_flow_evaluation_plot_scheduled(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
}

static int lab_flow_evaluation_weather(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_finally, "refreshWeather", 0, 0, 3,
                         lab_dom_get(ctx->request, "token"), lab_dom_get(ctx->frame, "weatherState"), ctx->data);
}

static int lab_flow_evaluation_finally(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
}

static int lab_flow_evaluation_finish(struct lab_flow_evaluation_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    if (lab_dom_truth(lab_dom_get(ctx->frame, "current")) && lab_dom_truth(lab_dom_get(ctx->preparation, "history")))
        return lab_flow_call(ctx->frame, lab_flow_eval_done, "updateHistoryButtons", 0, 0, 0);
    return lab_flow_done(0);
}

static int lab_flow_evaluation_catch(struct lab_flow_evaluation_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(lab_flow_eval_rethrow_current));
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_caught);
}

static int lab_flow_evaluation_caught(struct lab_flow_evaluation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_eval_current(ctx->frame, lab_flow_eval_finish);
    return lab_flow_call(ctx->frame, lab_flow_eval_error_plan, "native", "responsePlan", 0, 6,
                         lab_dom_string("lab_evaluation_response"), ctx->mode, lab_dom_numeric(LAB_REQUEST_OUTCOME_EXCEPTION),
                         lab_dom_object(LAB_VALUE_OBJECT),
                         ctx->context, lab_dom_get(ctx->frame, "error"));
}

static int lab_flow_evaluation_error_plan(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_effects(ctx->frame, lab_flow_eval_error_effects, lab_dom_get(ctx->frame, "responsePlan"));
}

static int lab_flow_evaluation_error_effects(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
}

static int lab_flow_evaluation_rethrow_current(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, lab_flow_eval_rethrow_history);
}

static int lab_flow_evaluation_rethrow_history(struct lab_flow_evaluation_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "current")) && lab_dom_truth(lab_dom_get(ctx->preparation, "history")))
        return lab_flow_call(ctx->frame, lab_flow_eval_rethrow, "updateHistoryButtons", 0, 0, 0);
    /* Continue: preserve a failure raised by the recovery services themselves. */
    return lab_flow_evaluation_rethrow(ctx);
}

static int lab_flow_evaluation_rethrow(struct lab_flow_evaluation_context *ctx)
{
    return lab_flow_call(ctx->frame, lab_flow_eval_done, "rethrow", 0, 0, 1, lab_dom_get(ctx->frame, "exception"));
}

static int lab_flow_evaluation_otherwise(struct lab_flow_evaluation_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

/* Advance one evaluation stage using only this call's borrowed frame handles. */
int lab_flow_evaluation(int frame, int view)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    int mode = lab_dom_get(frame, "mode"), options = lab_dom_get(frame, "options");
    int context = lab_dom_get(frame, "context"), request = lab_dom_get(frame, "request");
    int preparation = lab_dom_get(frame, "preparation"), plan = lab_dom_get(frame, "plan");
    int data = lab_dom_get(frame, "data");
    struct lab_flow_evaluation_context ctx = {
        .frame = frame,
        .view = view,
        .mode = mode,
        .options = options,
        .context = context,
        .request = request,
        .preparation = preparation,
        .plan = plan,
        .data = data,
    };
    static int (*const handlers[])(struct lab_flow_evaluation_context *) = {
        [lab_flow_eval_start]           = lab_flow_evaluation_start,
        [lab_flow_eval_prepare]         = lab_flow_evaluation_prepare,
        [lab_flow_eval_setup]           = lab_flow_evaluation_setup,
        [lab_flow_eval_setup_apply]     = lab_flow_evaluation_setup_apply,
        [lab_flow_eval_setup_current]   = lab_flow_evaluation_setup_current,
        [lab_flow_eval_setup_next]      = lab_flow_evaluation_setup_next,
        [lab_flow_eval_history]         = lab_flow_evaluation_history,
        [lab_flow_eval_previous]        = lab_flow_evaluation_previous,
        [lab_flow_eval_input]           = lab_flow_evaluation_input,
        [lab_flow_eval_show]            = lab_flow_evaluation_show,
        [lab_flow_eval_status]          = lab_flow_evaluation_status,
        [lab_flow_eval_guard]           = lab_flow_evaluation_guard,
        [lab_flow_eval_fetch]           = lab_flow_evaluation_fetch,
        [lab_flow_eval_outcome]         = lab_flow_evaluation_outcome,
        [lab_flow_eval_response]        = lab_flow_evaluation_response,
        [lab_flow_eval_recover]         = lab_flow_evaluation_recover,
        [lab_flow_eval_install]         = lab_flow_evaluation_install,
        [lab_flow_eval_installed]       = lab_flow_evaluation_installed,
        [lab_flow_eval_commit]          = lab_flow_evaluation_commit,
        [lab_flow_eval_ready]           = lab_flow_evaluation_ready,
        [lab_flow_eval_weather_state]   = lab_flow_evaluation_weather_state,
        [lab_flow_eval_plot_scheduled]  = lab_flow_evaluation_plot_scheduled,
        [lab_flow_eval_weather]         = lab_flow_evaluation_weather,
        [lab_flow_eval_finally]         = lab_flow_evaluation_finally,
        [lab_flow_eval_finish]          = lab_flow_evaluation_finish,
        [lab_flow_eval_catch]           = lab_flow_evaluation_catch,
        [lab_flow_eval_caught]          = lab_flow_evaluation_caught,
        [lab_flow_eval_error_plan]      = lab_flow_evaluation_error_plan,
        [lab_flow_eval_error_effects]   = lab_flow_evaluation_error_effects,
        [lab_flow_eval_rethrow_current] = lab_flow_evaluation_rethrow_current,
        [lab_flow_eval_rethrow_history] = lab_flow_evaluation_rethrow_history,
        [lab_flow_eval_rethrow]         = lab_flow_evaluation_rethrow,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_evaluation_otherwise(&ctx);
}

/* Both normal evaluation and focused browser tests use this same installation continuation. */
struct lab_flow_install_context {
    int frame;
    int view;
    int mode;
    int data;
    int context;
    int cards;
    int request;
};

static int lab_flow_install_native(struct lab_flow_install_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "native", "cards", 0, 5, lab_dom_string("lab_evaluation_cards"), ctx->mode,
                         ctx->data, lab_dom_get(ctx->context, "text"), lab_dom_get(ctx->view, "caches"));
}

static int lab_flow_install_native_1(struct lab_flow_install_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), ctx->mode,
                         lab_dom_numeric(LAB_EVALUATION_INSTALL_PROJECT), lab_dom_get(ctx->frame, "outcome"),
                         ctx->data, ctx->context, ctx->cards,
                         ctx->view);
}

static int lab_flow_install_apply_initial(struct lab_flow_install_context *ctx)
{
    return lab_flow_effects(ctx->frame, 3, lab_dom_get(ctx->frame, "plan"));
}

static int lab_flow_install_render_solver(struct lab_flow_install_context *ctx)
{
    if (lab_dom_number(ctx->mode) == 2 && lab_dom_truth(lab_dom_get(ctx->cards, "solver_source"))) {
        lab_dom_set(ctx->frame, "failed", lab_dom_numeric(5));
        return lab_flow_call(ctx->frame, 4, "renderSolver", "solver", 1, 2, lab_dom_get(ctx->cards, "solver_source"),
                             lab_dom_get(ctx->request, "token"));
    }
    return lab_flow_call(ctx->frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), ctx->mode,
                         lab_dom_numeric(LAB_EVALUATION_INSTALL_COMPLETE), lab_dom_get(ctx->frame, "outcome"),
                         ctx->data, ctx->context, ctx->cards,
                         ctx->view);
}

static int lab_flow_install_solver_current(struct lab_flow_install_context *ctx)
{
    return lab_flow_eval_current(ctx->frame, 6);
}

static int lab_flow_install_solver_failed(struct lab_flow_install_context *ctx)
{
    lab_dom_delete(ctx->frame, "solver");
    return lab_flow_eval_current(ctx->frame, 6);
}

static int lab_flow_install_solver_svg(struct lab_flow_install_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    int svg = lab_dom_get(lab_dom_get(ctx->frame, "solver"), "svg");
    if (lab_dom_truth(svg))
        return lab_flow_call(ctx->frame, 7, "solverSVG", 0, 0, 5, ctx->data, lab_dom_get(ctx->cards, "solver_source"),
                             lab_dom_get(ctx->cards, "function"), svg, lab_dom_get(ctx->request, "token"));
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), ctx->mode,
                         lab_dom_numeric(LAB_EVALUATION_INSTALL_COMPLETE), lab_dom_get(ctx->frame, "outcome"),
                         ctx->data, ctx->context, ctx->cards,
                         ctx->view);
}

static int lab_flow_install_native_7(struct lab_flow_install_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), ctx->mode,
                         lab_dom_numeric(LAB_EVALUATION_INSTALL_COMPLETE), lab_dom_get(ctx->frame, "outcome"),
                         ctx->data, ctx->context, ctx->cards,
                         ctx->view);
}

static int lab_flow_install_apply_final(struct lab_flow_install_context *ctx)
{
    return lab_flow_effects(ctx->frame, 9, lab_dom_get(ctx->frame, "plan"));
}

static int lab_flow_install_otherwise(struct lab_flow_install_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

/* Select the next guarded installation stage for the current evaluation. */
int lab_flow_install(int frame, int view)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    int mode = lab_dom_get(frame, "mode"), data = lab_dom_get(frame, "data");
    int context = lab_dom_get(frame, "context"), cards = lab_dom_get(frame, "cards");
    int request = lab_dom_get(frame, "request");
    struct lab_flow_install_context ctx = {
        .frame = frame,
        .view = view,
        .mode = mode,
        .data = data,
        .context = context,
        .cards = cards,
        .request = request,
    };
    static int (*const handlers[])(struct lab_flow_install_context *) = {
        [0] = lab_flow_install_native,
        [1] = lab_flow_install_native_1,
        [2] = lab_flow_install_apply_initial,
        [3] = lab_flow_install_render_solver,
        [4] = lab_flow_install_solver_current,
        [5] = lab_flow_install_solver_failed,
        [6] = lab_flow_install_solver_svg,
        [7] = lab_flow_install_native_7,
        [8] = lab_flow_install_apply_final,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_install_otherwise(&ctx);
}
