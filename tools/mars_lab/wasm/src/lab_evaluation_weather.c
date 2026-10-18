/**
 * @file lab_evaluation_weather.c
 * @brief Deferred weather installation and completion status for the Lab browser.
 *
 * Selects bounded ordered service plans without accessing weather payload fields
 * or merging native markup. The JavaScript adapter executes installation after
 * the scoped bridge returns, catches rendering failures and rejects stale requests.
 * Browser-owned response references survive the call; no handle or native allocation
 * is retained. Weather presentation remains the existing native card module's job.
 */
#include "lab_dom.h"
#include "lab_evaluation_weather.h"

static void lab_evaluation_weather_emit(browser_jsref calls, const char *service, unsigned count, browser_jsref first,
                                browser_jsref second)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_push(args, first);
    if (count == 2)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Publish inert service descriptions; the host owns freshness checks and catches installation failures. */
void lab_evaluation_weather_response(lab_request_outcome_t outcome, browser_jsref overview, browser_jsref data)
{
    if (outcome != LAB_REQUEST_OUTCOME_FAILURE && outcome != LAB_REQUEST_OUTCOME_SUCCESS &&
        outcome != LAB_REQUEST_OUTCOME_EXCEPTION) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(plan, "calls", calls);
    if (outcome == LAB_REQUEST_OUTCOME_SUCCESS)
        lab_evaluation_weather_emit(calls, "installWeather", 2, overview, data);
    const char *status = outcome == LAB_REQUEST_OUTCOME_SUCCESS ? "Ready" : "Weather unavailable";
    lab_evaluation_weather_emit(calls, "setStatus", 1, lab_dom_string(status), 0);
    lab_dom_return(plan);
}
