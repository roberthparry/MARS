/**
 * @file lab_flow_evaluation.c
 * @brief Native continuations for worksheet evaluation and result installation.
 *
 * Owns all seven modes' preparation, history, transport, recovery and completion
 * ordering. Explicit stages are suspension points, not mathematical cases. Existing
 * native presentation plans remain authoritative; the browser only executes their
 * capabilities. A request-local frame preserves values across awaits without
 * retaining scoped handles or sharing mutable state between concurrent requests.
 */
#include "lab_dom.h"
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

/* The switch is a finite continuation graph: each case performs one suspended stage. */
int lab_flow_evaluation(int frame, int view)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    int mode = lab_dom_get(frame, "mode"), options = lab_dom_get(frame, "options");
    int context = lab_dom_get(frame, "context"), request = lab_dom_get(frame, "request");
    int preparation = lab_dom_get(frame, "preparation"), plan = lab_dom_get(frame, "plan");
    int data = lab_dom_get(frame, "data");
    switch (stage) {
        case lab_flow_eval_start:
            lab_dom_set(frame, "context", lab_dom_object(5));
            lab_dom_set(frame, "phase", lab_dom_numeric(0));
            return lab_flow_call(frame, lab_flow_eval_prepare, "native", "preparation", 0, 3,
                                 lab_dom_string("lab_evaluation_prepare_plan"), mode,
                                 lab_dom_numeric(lab_dom_truth(lab_dom_get(options, "skipHistoryUpdate"))));
        case lab_flow_eval_prepare:
            if (!preparation)
                return lab_flow_done(0);
            /* Fall through: the first preparation stage uses the newly allocated frame. */
        case lab_flow_eval_setup:
            return lab_flow_eval_native(frame, lab_flow_eval_setup_apply, "lab_evaluation_setup", mode,
                                        lab_dom_get(frame, "phase"), context, options, view);
        case lab_flow_eval_setup_apply:
            return lab_flow_effects(frame,
                                    lab_dom_truth(lab_dom_get(plan, "wait")) ? lab_flow_eval_setup_current
                                                                             : lab_flow_eval_setup_next,
                                    plan);
        case lab_flow_eval_setup_current:
            return lab_flow_eval_current(frame, lab_flow_eval_setup_next);
        case lab_flow_eval_setup_next:
            if (lab_dom_truth(lab_dom_get(plan, "wait")) && !lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            if (!lab_dom_truth(lab_dom_get(plan, "done"))) {
                lab_dom_set(frame, "phase", lab_dom_numeric(lab_dom_number(lab_dom_get(frame, "phase")) + 1));
                return lab_flow_eval_native(frame, lab_flow_eval_setup_apply, "lab_evaluation_setup", mode,
                                            lab_dom_get(frame, "phase"), context, options, view);
            }
            return lab_flow_call(frame, lab_flow_eval_history, "historyState", "nextState", 0, 1,
                                 lab_dom_get(context, "text"));
        case lab_flow_eval_history:
            if (lab_dom_truth(lab_dom_get(preparation, "history")))
                return lab_flow_call(frame, lab_flow_eval_previous, "previousHistory", "previousState", 0, 1,
                                     lab_dom_get(frame, "nextState"));
            /* Fall through: no history lookup is needed for precision-only evaluation. */
        case lab_flow_eval_previous:
            if (lab_dom_truth(lab_dom_get(preparation, "input")))
                return lab_flow_call(frame, lab_flow_eval_input, "requestInput", "accepted", 0, 2, request,
                                     lab_dom_get(context, "text"));
            return lab_flow_call(frame, lab_flow_eval_status, "showResults", 0, 0, 0);
        case lab_flow_eval_input:
            if (!lab_dom_truth(lab_dom_get(frame, "accepted")))
                return lab_flow_done(0);
            /* Fall through after successful native input validation. */
        case lab_flow_eval_show:
            return lab_flow_call(frame, lab_flow_eval_status, "showResults", 0, 0, 0);
        case lab_flow_eval_status:
            return lab_flow_call(frame, lab_flow_eval_guard, "setStatus", 0, 0, 1, lab_dom_get(preparation, "label"));
        case lab_flow_eval_guard:
            lab_dom_set(frame, "failed", lab_dom_numeric(lab_flow_eval_catch));
            if (lab_dom_truth(lab_dom_get(frame, "previousState")))
                return lab_flow_call(frame, lab_flow_eval_fetch, "pushHistory", 0, 0, 1,
                                     lab_dom_get(frame, "previousState"));
            /* Fall through: absent history has no browser effect. */
        case lab_flow_eval_fetch: {
            static const char *const fetchers[] = {"fetchExpression", "fetchEquation",   "fetchDiffequation",
                                                   "fetchMatrix",     "fetchIntegrator", "fetchDatetime",
                                                   "fetchAlmanac"};
            unsigned index = (unsigned)lab_dom_number(mode);
            if (index > 6)
                return lab_flow_done(0);
            return lab_flow_call(frame, lab_flow_eval_outcome, fetchers[index], "response", 1, 2, context, request);
        }
        case lab_flow_eval_outcome: {
            int response = lab_dom_get(frame, "response");
            data = lab_dom_get(response, "data");
            lab_dom_set(frame, "data", data);
            return lab_flow_call(frame, lab_flow_eval_response, "requestOutcome", "outcome", 0, 3, request,
                                 lab_dom_get(response, "response"), data);
        }
        case lab_flow_eval_response:
            return lab_flow_call(frame, lab_flow_eval_recover, "native", "responsePlan", 0, 6,
                                 lab_dom_string("lab_evaluation_response"), mode, lab_dom_get(frame, "outcome"), data,
                                 context, 0);
        case lab_flow_eval_recover:
            plan = lab_dom_get(frame, "responsePlan");
            if (!plan)
                return lab_flow_eval_current(frame, lab_flow_eval_finish);
            return lab_flow_effects(frame, lab_flow_eval_install, plan);
        case lab_flow_eval_install:
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "responsePlan"), "install")))
                return lab_flow_call(frame, lab_flow_eval_installed, "installEvaluation", 0, 1, 5, mode, data, context,
                                     lab_dom_get(frame, "outcome"), request);
            return lab_flow_call(frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
        case lab_flow_eval_installed:
            return lab_flow_eval_current(frame, lab_flow_eval_commit);
        case lab_flow_eval_commit:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_eval_current(frame, lab_flow_eval_finish);
            return lab_flow_call(frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
        case lab_flow_eval_ready:
            return lab_flow_call(frame, lab_flow_eval_weather_state, "setStatus", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "responsePlan"), "status"));
        case lab_flow_eval_weather_state:
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "responsePlan"), "weather")))
                return lab_flow_call(frame, lab_flow_eval_weather, "native", "weatherState", 0, 3,
                                     lab_dom_string("lab_evaluation_weather_state"), lab_dom_get(context, "state"),
                                     data);
            return lab_flow_eval_current(frame, lab_flow_eval_finish);
        case lab_flow_eval_weather:
            return lab_flow_call(frame, lab_flow_eval_finally, "refreshWeather", 0, 0, 3, lab_dom_get(request, "token"),
                                 lab_dom_get(frame, "weatherState"), data);
        case lab_flow_eval_finally:
            return lab_flow_eval_current(frame, lab_flow_eval_finish);
        case lab_flow_eval_finish:
            lab_dom_delete(frame, "failed");
            if (lab_dom_truth(lab_dom_get(frame, "current")) && lab_dom_truth(lab_dom_get(preparation, "history")))
                return lab_flow_call(frame, lab_flow_eval_done, "updateHistoryButtons", 0, 0, 0);
            return lab_flow_done(0);
        case lab_flow_eval_catch:
            lab_dom_set(frame, "failed", lab_dom_numeric(lab_flow_eval_rethrow_current));
            return lab_flow_eval_current(frame, lab_flow_eval_caught);
        case lab_flow_eval_caught:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_eval_current(frame, lab_flow_eval_finish);
            return lab_flow_call(frame, lab_flow_eval_error_plan, "native", "responsePlan", 0, 6,
                                 lab_dom_string("lab_evaluation_response"), mode, lab_dom_numeric(4), lab_dom_object(5),
                                 context, lab_dom_get(frame, "error"));
        case lab_flow_eval_error_plan:
            return lab_flow_effects(frame, lab_flow_eval_error_effects, lab_dom_get(frame, "responsePlan"));
        case lab_flow_eval_error_effects:
            return lab_flow_call(frame, lab_flow_eval_ready, "commitModeState", 0, 0, 0);
        case lab_flow_eval_rethrow_current:
            return lab_flow_eval_current(frame, lab_flow_eval_rethrow_history);
        case lab_flow_eval_rethrow_history:
            if (lab_dom_truth(lab_dom_get(frame, "current")) && lab_dom_truth(lab_dom_get(preparation, "history")))
                return lab_flow_call(frame, lab_flow_eval_rethrow, "updateHistoryButtons", 0, 0, 0);
            /* Fall through: preserve a failure raised by the recovery services themselves. */
        case lab_flow_eval_rethrow:
            return lab_flow_call(frame, lab_flow_eval_done, "rethrow", 0, 0, 1, lab_dom_get(frame, "exception"));
        default:
            return lab_flow_done(0);
    }
}

/* Both normal evaluation and focused browser tests use this same installation continuation. */
int lab_flow_install(int frame, int view)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    int mode = lab_dom_get(frame, "mode"), data = lab_dom_get(frame, "data");
    int context = lab_dom_get(frame, "context"), cards = lab_dom_get(frame, "cards");
    int request = lab_dom_get(frame, "request");
    switch (stage) {
        case 0:
            return lab_flow_call(frame, 1, "native", "cards", 0, 5, lab_dom_string("lab_evaluation_cards"), mode, data,
                                 lab_dom_get(context, "text"), lab_dom_get(view, "caches"));
        case 1:
            return lab_flow_call(frame, 2, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), mode,
                                 lab_dom_numeric(0), lab_dom_get(frame, "outcome"), data, context, cards, view);
        case 2:
            return lab_flow_effects(frame, 3, lab_dom_get(frame, "plan"));
        case 3:
            if (lab_dom_number(mode) == 2 && lab_dom_truth(lab_dom_get(cards, "solver_source"))) {
                lab_dom_set(frame, "failed", lab_dom_numeric(5));
                return lab_flow_call(frame, 4, "renderSolver", "solver", 1, 2, lab_dom_get(cards, "solver_source"),
                                     lab_dom_get(request, "token"));
            }
            return lab_flow_call(frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), mode,
                                 lab_dom_numeric(1), lab_dom_get(frame, "outcome"), data, context, cards, view);
        case 4:
            return lab_flow_eval_current(frame, 6);
        case 5:
            lab_dom_delete(frame, "solver");
            return lab_flow_eval_current(frame, 6);
        case 6: {
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            int svg = lab_dom_get(lab_dom_get(frame, "solver"), "svg");
            if (lab_dom_truth(svg))
                return lab_flow_call(frame, 7, "solverSVG", 0, 0, 5, data, lab_dom_get(cards, "solver_source"),
                                     lab_dom_get(cards, "function"), svg, lab_dom_get(request, "token"));
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), mode,
                                 lab_dom_numeric(1), lab_dom_get(frame, "outcome"), data, context, cards, view);
        }
        case 7:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, 8, "native", "plan", 0, 8, lab_dom_string("lab_evaluation_install"), mode,
                                 lab_dom_numeric(1), lab_dom_get(frame, "outcome"), data, context, cards, view);
        case 8:
            return lab_flow_effects(frame, 9, lab_dom_get(frame, "plan"));
        default:
            return lab_flow_done(0);
    }
}
