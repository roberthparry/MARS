/**
 * @file lab_bootstrap.c
 * @brief Browser startup ordering, native ABI validation and application asset selection.
 *
 * Runs after the host has instantiated WebAssembly. Responses, script-loading
 * promises and DOM events stay browser-owned; this module chooses their ordering
 * and diagnostics. Definition scripts may load concurrently, but workspace state
 * is created only after all definitions and the native catalogue are available.
 */
#include "lab_browser.h"
#include "lab_dom.h"
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
    int result = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(result, "calls", calls);
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i) {
        int options = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY),
            call = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "searchable", lab_dom_scalar(LAB_VALUE_BOOLEAN, controls[i].searchable));
        if (controls[i].searchable) {
            lab_dom_set(options, "searchPlaceholder",
                        lab_dom_string(controls[i].town ? "Search towns" : "Search jurisdictions"));
            lab_dom_set(
                options, "emptyText",
                lab_dom_string(controls[i].town ? "No towns for this jurisdiction" : "No matching jurisdiction"));
        }
        if (controls[i].town) {
            lab_dom_set(options, "placeholder", lab_dom_string("Custom location"));
            lab_dom_set(options, "details", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        }
        lab_dom_push(args, lab_dom_id(controls[i].id));
        lab_dom_push(args, options);
        lab_dom_set(call, "service", lab_dom_string("enhance"));
        lab_dom_set(call, "args", args);
        lab_dom_push(calls, call);
    }
    lab_dom_return(result);
}

/** @brief Scoped workspace inputs and the optional service selected by one startup phase. */
struct lab_bootstrap_workspace_context {
    int context, result, args;
    const char *service;
};

/* Inspect URL components only when an authentication token is present. */
static int lab_bootstrap_workspace_token(struct lab_bootstrap_workspace_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->context, "token"))) {
        int search = lab_dom_get(ctx->context, "search");
        int prefix = lab_dom_get(ctx->context, "prefix");
        if (lab_dom_contains(search, prefix)) {
            int pathname = lab_dom_get(ctx->context, "pathname");
            int hash = lab_dom_get(ctx->context, "hash");
            ctx->service = "replaceLocation";
            lab_dom_push(ctx->args, lab_dom_join(pathname, hash, ""));
        }
    }
    return ctx->result;
}

/* Supply the default only after observing an existing empty jurisdiction control. */
static int lab_bootstrap_workspace_jurisdiction(struct lab_bootstrap_workspace_context *ctx)
{
    int control = lab_dom_get(ctx->context, "control"), value = lab_dom_get(control, "value");
    if (control && !lab_dom_truth(value)) {
        ctx->service = "writeValue";
        lab_dom_push(ctx->args, control);
        lab_dom_push(ctx->args, lab_dom_get(ctx->context, "fallback"));
    }
    return ctx->result;
}

/* Read GMT separately after the jurisdiction effects have been executed by the host. */
static int lab_bootstrap_workspace_gmt(struct lab_bootstrap_workspace_context *ctx)
{
    int control = lab_dom_get(ctx->context, "control"), value = lab_dom_get(control, "value");
    return lab_dom_text(lab_dom_truth(value) ? value : lab_dom_get(ctx->context, "fallback"));
}

/* Startup policy must not require the later workspace globals or the application service registry. */
void lab_bootstrap_workspace(lab_bootstrap_workspace_phase_t phase, int context)
{
    /** @brief Workspace phase handlers; unknown phases retain the empty calls plan. */
    static int (*const handlers[])(struct lab_bootstrap_workspace_context *) = {
        [LAB_BOOTSTRAP_WORKSPACE_TOKEN]        = lab_bootstrap_workspace_token,
        [LAB_BOOTSTRAP_WORKSPACE_JURISDICTION] = lab_bootstrap_workspace_jurisdiction,
        [LAB_BOOTSTRAP_WORKSPACE_GMT]          = lab_bootstrap_workspace_gmt,
    };
    int result = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(result, "calls", calls);
    int args = lab_dom_object(LAB_VALUE_ARRAY);
    struct lab_bootstrap_workspace_context ctx = {context, result, args, 0};
    if ((unsigned)phase < sizeof handlers / sizeof *handlers)
        result = handlers[phase](&ctx);
    if (ctx.service) {
        int call = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(call, "service", lab_dom_string(ctx.service));
        lab_dom_set(call, "args", args);
        lab_dom_push(calls, call);
    }
    lab_dom_return(result);
}

/** @brief Native bootstrap continuation stages retained in the browser-owned frame. */
typedef enum {
    LAB_BOOTSTRAP_FETCH_CONFIG = 0,       /**< Start loading the bootstrap response. */
    LAB_BOOTSTRAP_CHECK_CONFIG = 1,       /**< Check the bootstrap HTTP response. */
    LAB_BOOTSTRAP_INSTALL_CONFIG = 2,     /**< Validate ABI compatibility and install decoded configuration. */
    LAB_BOOTSTRAP_FETCH_CATALOGUE = 3,    /**< Start loading the jurisdiction catalogue. */
    LAB_BOOTSTRAP_CHECK_CATALOGUE = 5,    /**< Check the catalogue HTTP response; stage 4 remains unused. */
    LAB_BOOTSTRAP_INSTALL_CATALOGUE = 6,  /**< Publish decoded catalogue data to browser services. */
    LAB_BOOTSTRAP_VALIDATE_CATALOGUE = 7, /**< Ask native location policy to validate the catalogue. */
    LAB_BOOTSTRAP_LOAD_DEFINITIONS = 8,   /**< Validate the catalogue result and load controller definitions. */
    LAB_BOOTSTRAP_LOAD_WORKSPACE = 9,     /**< Load the workspace composition script. */
    LAB_BOOTSTRAP_INSTALL_EVENTS = 10,    /**< Register browser event handlers. */
    LAB_BOOTSTRAP_INSTALL_WIDGETS = 11,   /**< Enhance the initial controls. */
    LAB_BOOTSTRAP_START_WORKSHEET = 12,   /**< Evaluate the initial worksheet. */
    LAB_BOOTSTRAP_FINISHED = 13           /**< No further startup capabilities are required. */
} lab_bootstrap_stage_t;

/* Each stage is one asynchronous or synchronous browser capability boundary. */
struct lab_bootstrap_step_context {
    lab_bootstrap_stage_t stage;
    int frame;
    int response;
};

static int lab_bootstrap_step_fetch(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_CHECK_CONFIG, "fetch", "response", 1, 1,
                         lab_dom_string("/bootstrap"));
}

static int lab_bootstrap_step_fail(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->response, "ok"))) {
        int prefix = lab_dom_string(ctx->stage == LAB_BOOTSTRAP_CHECK_CONFIG
                                        ? "Could not load Lab configuration ("
                                        : "Could not load the Lab catalogue (");
        return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_FINISHED, "fail", 0, 0, 1,
                             lab_dom_join(prefix, lab_dom_text(lab_dom_get(ctx->response, "status")), ")"));
    }
    lab_bootstrap_stage_t next = ctx->stage == LAB_BOOTSTRAP_CHECK_CONFIG ? LAB_BOOTSTRAP_INSTALL_CONFIG
                                                                         : LAB_BOOTSTRAP_INSTALL_CATALOGUE;
    return lab_flow_call(ctx->frame, next, "decode", "data", 1, 1, ctx->response);
}

static int lab_bootstrap_step_fail_3(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(lab_dom_get(ctx->frame, "data"), "BROWSER_ABI_VERSION"),
                       lab_dom_text(lab_dom_numeric(lab_browser_abi_version()))))
        return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_FINISHED, "fail", 0, 0, 1,
                             lab_dom_string("The running MARS Lab server does not match the browser files. "
                                            "Run make mars-lab-restart, then reload this page."));
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_FETCH_CATALOGUE, "config", 0, 0, 1, lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_fetch_4(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_CHECK_CATALOGUE, "fetch", "response", 1, 1,
                         lab_dom_string("/jurisdictions"));
}

static int lab_bootstrap_step_catalogue(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_VALIDATE_CATALOGUE, "catalogue", 0, 0, 1,
                         lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_native(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_LOAD_DEFINITIONS, "native", "valid", 0, 2,
                         lab_dom_string("lab_location_catalogue"), lab_dom_get(ctx->frame, "data"));
}

static int lab_bootstrap_step_fail_7(struct lab_bootstrap_step_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "valid")))
        return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_FINISHED, "fail", 0, 0, 1,
                             lab_dom_string("The Lab catalogue is invalid"));
    int scripts = lab_dom_object(LAB_VALUE_ARRAY);
    static const char *const names[] = {"api", "bindings_controller", "bindings", "locations", "results",
                                       "results_services", "state", "surface_camera", "surface_export", "surface_hover",
                                       "surface_volume", "surface", "workspace_native", "workspace_controllers"};
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
        lab_dom_push(scripts, lab_dom_string(names[i]));
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_LOAD_WORKSPACE, "definitions", 0, 1, 1, scripts);
}

static int lab_bootstrap_step_script(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_INSTALL_EVENTS, "script", 0, 1, 1,
                         lab_dom_string("/js/workspace.js"));
}

static int lab_bootstrap_step_events(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_INSTALL_WIDGETS, "events", 0, 0, 0);
}

static int lab_bootstrap_step_widgets(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_START_WORKSHEET, "widgets", 0, 0, 0);
}

static int lab_bootstrap_step_worksheet(struct lab_bootstrap_step_context *ctx)
{
    return lab_flow_call(ctx->frame, LAB_BOOTSTRAP_FINISHED, "worksheet", 0, 1, 0);
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
    double selected = lab_dom_number(lab_dom_get(frame, "stage"));
    if (!(selected >= (double)LAB_BOOTSTRAP_FETCH_CONFIG && selected <= (double)LAB_BOOTSTRAP_FINISHED))
        return lab_flow_done(0);
    struct lab_bootstrap_step_context ctx = {
        .stage = (lab_bootstrap_stage_t)(unsigned)selected,
        .frame = frame,
        .response = response,
    };
    static int (*const handlers[])(struct lab_bootstrap_step_context *) = {
        [LAB_BOOTSTRAP_FETCH_CONFIG]       = lab_bootstrap_step_fetch,
        [LAB_BOOTSTRAP_CHECK_CONFIG]       = lab_bootstrap_step_fail,
        [LAB_BOOTSTRAP_INSTALL_CONFIG]     = lab_bootstrap_step_fail_3,
        [LAB_BOOTSTRAP_FETCH_CATALOGUE]    = lab_bootstrap_step_fetch_4,
        [LAB_BOOTSTRAP_CHECK_CATALOGUE]    = lab_bootstrap_step_fail,
        [LAB_BOOTSTRAP_INSTALL_CATALOGUE]  = lab_bootstrap_step_catalogue,
        [LAB_BOOTSTRAP_VALIDATE_CATALOGUE] = lab_bootstrap_step_native,
        [LAB_BOOTSTRAP_LOAD_DEFINITIONS]   = lab_bootstrap_step_fail_7,
        [LAB_BOOTSTRAP_LOAD_WORKSPACE]     = lab_bootstrap_step_script,
        [LAB_BOOTSTRAP_INSTALL_EVENTS]     = lab_bootstrap_step_events,
        [LAB_BOOTSTRAP_INSTALL_WIDGETS]    = lab_bootstrap_step_widgets,
        [LAB_BOOTSTRAP_START_WORKSHEET]    = lab_bootstrap_step_worksheet,
    };
    return (unsigned)ctx.stage < sizeof handlers / sizeof *handlers && handlers[ctx.stage]
               ? handlers[ctx.stage](&ctx)
               : lab_bootstrap_step_otherwise(&ctx);
}
