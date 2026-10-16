/**
 * @file lab_bootstrap.c
 * @brief Browser startup ordering, native ABI validation and application asset selection.
 *
 * Runs after the host has instantiated WebAssembly. Responses, script-loading
 * promises and DOM events stay browser-owned; this module chooses their ordering
 * and diagnostics. Definition scripts may load concurrently, but workspace state
 * is created only after all definitions and the native catalogue are available.
 */
#include "lab_dom.h"
#include "lab_browser.h"
#include "lab_flow.h"
#include "lab_bootstrap.h"

/* Fixed controls share themed menus and native option policy rather than browser business logic. */
void lab_bootstrap_widgets(void)
{
    static const struct {
        const char *id;
        unsigned town;
        unsigned searchable;
    } controls[] = {
        {"datetimeJurisdiction", 0, 1}, {"almanacJurisdiction", 0, 1}, {"datetimeTown", 1, 1}, {"almanacTown", 1, 1}};
    int result = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(result, "calls", calls);
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i) {
        int options = lab_dom_object(5), args = lab_dom_object(4), call = lab_dom_object(5);
        lab_dom_set(options, "searchable", lab_dom_scalar(1, controls[i].searchable));
        if (controls[i].searchable) {
            lab_dom_set(options, "searchPlaceholder",
                        lab_dom_string(controls[i].town ? "Search towns" : "Search jurisdictions"));
            lab_dom_set(
                options, "emptyText",
                lab_dom_string(controls[i].town ? "No towns for this jurisdiction" : "No matching jurisdiction"));
        }
        if (controls[i].town) {
            lab_dom_set(options, "placeholder", lab_dom_string("Custom location"));
            lab_dom_set(options, "details", lab_dom_scalar(1, 1));
        }
        lab_dom_push(args, lab_dom_id(controls[i].id));
        lab_dom_push(args, options);
        lab_dom_set(call, "service", lab_dom_string("enhance"));
        lab_dom_set(call, "args", args);
        lab_dom_push(calls, call);
    }
    lab_dom_return(result);
}

/* Startup policy must not require the later workspace globals or the application service registry. */
void lab_bootstrap_workspace(unsigned phase, int context)
{
    int result = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(result, "calls", calls);
    const char *service = 0;
    int args = lab_dom_object(4);
    if (phase == 0 && lab_dom_truth(lab_dom_get(context, "token"))) {
        int search = lab_dom_get(context, "search");
        int prefix = lab_dom_get(context, "prefix");
        if (lab_dom_contains(search, prefix)) {
            int pathname = lab_dom_get(context, "pathname");
            int hash = lab_dom_get(context, "hash");
            service = "replaceLocation";
            lab_dom_push(args, lab_dom_join(pathname, hash, ""));
        }
    } else if (phase == 1 || phase == 2) {
        int control = lab_dom_get(context, "control"), value = lab_dom_get(control, "value");
        if (phase == 2) {
            lab_dom_return(lab_dom_text(lab_dom_truth(value) ? value : lab_dom_get(context, "fallback")));
            return;
        }
        if (control && !lab_dom_truth(value)) {
            service = "writeValue";
            lab_dom_push(args, control);
            lab_dom_push(args, lab_dom_get(context, "fallback"));
        }
    }
    if (service) {
        int call = lab_dom_object(5);
        lab_dom_set(call, "service", lab_dom_string(service));
        lab_dom_set(call, "args", args);
        lab_dom_push(calls, call);
    }
    lab_dom_return(result);
}

/* Each stage is one asynchronous or synchronous browser capability boundary. */
struct lab_bootstrap_step_context {
    int frame;
    int response;
};

static int lab_bootstrap_step_fetch(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "fetch", "response", 1, 1, lab_dom_string("/bootstrap"));
}

static int lab_bootstrap_step_fail(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->response, "ok"))) {
        int prefix = lab_dom_string(lab_dom_number(lab_dom_get(ctx->frame, "stage")) == 1
                                        ? "Could not load Lab configuration ("
                                        : "Could not load the Lab catalogue (");
        return lab_flow_call(ctx->frame, 13, "fail", 0, 0, 1,
                             lab_dom_join(prefix, lab_dom_text(lab_dom_get(ctx->response, "status")), ")"));
    }
    return lab_flow_call(ctx->frame, lab_dom_number(lab_dom_get(ctx->frame, "stage")) == 1 ? 2 : 6, "decode", "data", 1,
                         1, ctx->response);
}

static int lab_bootstrap_step_fail_3(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(lab_dom_get(ctx->frame, "data"), "BROWSER_ABI_VERSION"),
                       lab_dom_text(lab_dom_numeric(lab_browser_abi_version()))))
        return lab_flow_call(ctx->frame, 13, "fail", 0, 0, 1,
                             lab_dom_string("The running MARS Lab server does not match the browser files. "
                                            "Run make mars-lab-restart, then reload this page."));
    return lab_flow_call(ctx->frame, 3, "config", 0, 0, 1, lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_fetch_4(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "fetch", "response", 1, 1, lab_dom_string("/jurisdictions"));
}

static int lab_bootstrap_step_catalogue(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 7, "catalogue", 0, 0, 1, lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_native(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "native", "valid", 0, 2, lab_dom_string("lab_location_catalogue"),
                         lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_fail_7(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "valid")))
        return lab_flow_call(ctx->frame, 13, "fail", 0, 0, 1, lab_dom_string("The Lab catalogue is invalid"));
    int scripts = lab_dom_object(4);
    static const char *const names[] = {"api",    "bindings",       "locations",      "results",
                                        "state",  "surface_camera", "surface_export", "surface_volume",
                                        "surface"};
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
        lab_dom_push(scripts, lab_dom_string(names[i]));
    return lab_flow_call(ctx->frame, 9, "definitions", 0, 1, 1, scripts);
}

static int lab_bootstrap_step_script(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 10, "script", 0, 1, 1, lab_dom_string("/js/workspace.js"));
}

static int lab_bootstrap_step_events(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "events", 0, 0, 0);
}

static int lab_bootstrap_step_widgets(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 12, "widgets", 0, 0, 0);
}

static int lab_bootstrap_step_worksheet(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, 13, "worksheet", 0, 1, 0);
}

static int lab_bootstrap_step_otherwise(struct lab_bootstrap_step_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

/* Advance startup through the bounded catalogue of browser capabilities. */
int lab_bootstrap_step(int frame, int view)
{
    (void)view;
    int response = lab_dom_get(frame, "response");
    struct lab_bootstrap_step_context ctx = {
        .frame = frame,
        .response = response,
    };
    static int (*const handlers[])(struct lab_bootstrap_step_context *) = {
        [0]  = lab_bootstrap_step_fetch,
        [1]  = lab_bootstrap_step_fail,
        [5]  = lab_bootstrap_step_fail,
        [2]  = lab_bootstrap_step_fail_3,
        [3]  = lab_bootstrap_step_fetch_4,
        [6]  = lab_bootstrap_step_catalogue,
        [7]  = lab_bootstrap_step_native,
        [8]  = lab_bootstrap_step_fail_7,
        [9]  = lab_bootstrap_step_script,
        [10] = lab_bootstrap_step_events,
        [11] = lab_bootstrap_step_widgets,
        [12] = lab_bootstrap_step_worksheet,
    };
    unsigned selected = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_bootstrap_step_otherwise(&ctx);
}
