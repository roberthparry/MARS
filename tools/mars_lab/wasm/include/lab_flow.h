/**
 * @file lab_flow.h
 * @brief Scoped continuation plans for asynchronous Lab browser workflows.
 *
 * Native workflow modules select each operation, await boundary and error handler.
 * The browser stores request-local frames between calls; no C handle survives a
 * yield. Plans expose only named, allowlisted browser capabilities, never scripts.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_FLOW_H
#define LAB_FLOW_H

#include <stdbool.h>

/** @brief Native browser workflows; explicit values preserve the browser dispatch ABI and reserved gaps. */
typedef enum {
    LAB_FLOW_KIND_EVALUATE                   = 0, /**< Evaluate the worksheet. */
    LAB_FLOW_KIND_GOAL                       = 1, /**< Perform goal seeking. */
    LAB_FLOW_KIND_WEATHER                    = 2, /**< Fetch dependent weather data. */
    LAB_FLOW_KIND_SOLVER                     = 3, /**< Render solver output. */
    LAB_FLOW_KIND_STARTUP                    = 4, /**< Start the restored worksheet. */
    LAB_FLOW_KIND_INSTALL                    = 5, /**< Install an accepted result. */
    LAB_FLOW_KIND_REQUEST_FUNCTION           = 6, /**< Run a Function programme. */
    LAB_FLOW_KIND_REQUEST_CALCULUS           = 7, /**< Request calculus or plot output. */
    LAB_FLOW_KIND_REQUEST_INTEGRATOR         = 8, /**< Evaluate an integrator worksheet. */
    LAB_FLOW_KIND_REQUEST_LAND               = 9, /**< Fetch land-totality data. */
    LAB_FLOW_KIND_REQUEST_HOLIDAYS           = 10, /**< Fetch local holidays. */
    LAB_FLOW_KIND_REQUEST_MOBILE             = 11, /**< Refresh mobile access information. */
    LAB_FLOW_KIND_REQUEST_FORMS              = 12, /**< Prepare forms. */
    LAB_FLOW_KIND_REQUEST_PRESENTATION       = 13, /**< Prepare evaluation presentation. */
    LAB_FLOW_KIND_REQUEST_SOLVER             = 14, /**< Render solver output on resize. */
    LAB_FLOW_KIND_REQUEST_FETCH              = 15, /**< Fetch a worksheet evaluation. */
    LAB_FLOW_KIND_STATE_EDITORS              = 16, /**< Restore saved editors. */
    LAB_FLOW_KIND_STATE_CONTROLS             = 17, /**< Restore saved controls. */
    LAB_FLOW_KIND_STATE_SAVED                = 18, /**< Apply saved state. */
    LAB_FLOW_KIND_STATE_RECOVERY             = 19, /**< Recover authored input. */
    LAB_FLOW_KIND_STATE_LOAD                 = 20, /**< Load initial state. */
    LAB_FLOW_KIND_STATE_HISTORY              = 21, /**< Restore history. */
    LAB_FLOW_KIND_STATE_NAVIGATION           = 22, /**< Navigate history. */
    LAB_FLOW_KIND_STATE_CAPTURE              = 23, /**< Capture state. */
    LAB_FLOW_KIND_STATE_SELECTION            = 24, /**< Select a worksheet. */
    LAB_FLOW_KIND_STATE_PRECISION            = 25, /**< Apply a precision change. */
    LAB_FLOW_KIND_LOCATION_FORMS             = 26, /**< Prepare calendar forms. */
    LAB_FLOW_KIND_LOCATION_RESTORE           = 27, /**< Restore a selected town. */
    LAB_FLOW_KIND_LOCATION_POPULATE          = 28, /**< Populate town choices. */
    LAB_FLOW_KIND_LOCATION_SYNC              = 29, /**< Synchronise location selectors. */
    LAB_FLOW_KIND_LOCATION_HISTORY           = 30, /**< Restore location history. */
    LAB_FLOW_KIND_LOCATION_TOTALITY          = 31, /**< Apply an eclipse action. */
    LAB_FLOW_KIND_LOCATION_REFRESH           = 32, /**< Refresh jurisdiction data. */
    LAB_FLOW_KIND_LOCATION_AUTO_EVALUATE     = 33, /**< Automatically evaluate a calendar. */
    LAB_FLOW_KIND_LOCATION_TIME              = 34, /**< Format authored time. */
    LAB_FLOW_KIND_LOCATION_CONTROL           = 35, /**< Handle a calendar control change. */
    LAB_FLOW_KIND_BINDING_SYNC               = 40, /**< Synchronise binding controls. */
    LAB_FLOW_KIND_BINDING_VISIBLE            = 41, /**< Refresh visible bindings. */
    LAB_FLOW_KIND_BINDING_COMMIT             = 42, /**< Commit authored values. */
    LAB_FLOW_KIND_BINDING_TOGGLE             = 43, /**< Toggle binding kind. */
    LAB_FLOW_KIND_BINDING_REFRESH            = 44, /**< Refresh binding metadata. */
    LAB_FLOW_KIND_BINDING_QUEUE              = 45, /**< Queue an input commit. */
    LAB_FLOW_KIND_BINDING_GOAL               = 46, /**< Start goal seeking. */
    LAB_FLOW_KIND_BINDING_ROW                = 47, /**< Update an integrator row. */
    LAB_FLOW_KIND_BINDING_PREPARE_INPUT      = 48, /**< Prepare authored input. */
    LAB_FLOW_KIND_BINDING_ACTION             = 49, /**< Handle a binding-card action. */
    LAB_FLOW_KIND_BINDING_DEFERRED           = 50, /**< Apply a deferred edit. */
    LAB_FLOW_KIND_BINDING_BEGIN_COMMIT       = 51, /**< Begin a guarded commit. */
    LAB_FLOW_KIND_BINDING_EDITOR_PROJECT     = 52, /**< Project editor metadata. */
    LAB_FLOW_KIND_BINDING_EDITOR_APPLY       = 53, /**< Apply edited source. */
    LAB_FLOW_KIND_BINDING_EDITOR_REFRESH     = 54, /**< Refresh editor metadata. */
    LAB_FLOW_KIND_BINDING_KIND_REQUEST       = 55, /**< Request a binding-kind change. */
    LAB_FLOW_KIND_RESULT_TRANSFER            = 56, /**< Transfer result to input. */
    LAB_FLOW_KIND_RESULT_PRETTY_MATRIX       = 57, /**< Render a matrix in pretty form. */
    LAB_FLOW_KIND_RESULT_EXPAND_DIGITS       = 58, /**< Expand rendered digits. */
    LAB_FLOW_KIND_RESULT_COPY_INPUT          = 59, /**< Copy authored input. */
    LAB_FLOW_KIND_RESULT_COPY_CARD           = 60, /**< Copy a result card. */
    LAB_FLOW_KIND_RESULT_RENDER_LATEX        = 61, /**< Request TeX rendering. */
    LAB_FLOW_KIND_RESULT_INITIALISE          = 62, /**< Initialise the worksheet. */
    LAB_FLOW_KIND_BOOTSTRAP                  = 64, /**< Install bootstrap data. */
    LAB_FLOW_KIND_TRANSPORT_POST             = 65, /**< Publish a POST response. */
    LAB_FLOW_KIND_TRANSPORT_RUN              = 66, /**< Execute a request. */
    LAB_FLOW_KIND_TRANSPORT_UI_RUN           = 67, /**< Execute a request with UI recovery. */
    LAB_FLOW_KIND_STATE_FORMS_CLEAR          = 72, /**< Clear the current worksheet. */
    LAB_FLOW_KIND_STATE_FORMS_REFRESH        = 73, /**< Refresh integrator forms. */
    LAB_FLOW_KIND_STATE_FORMS_PARSE_BOUNDS   = 74, /**< Parse integrator bounds. */
    LAB_FLOW_KIND_STATE_FORMS_RESTORE_BOUNDS = 75, /**< Restore integrator bounds. */
} lab_flow_kind_t;

/**
 * @brief Build one browser operation and advance its native continuation stage.
 * @param frame Scoped mutable browser frame; its stage property is updated to next before publication.
 * @param next Next workflow-specific stage index to enter after the capability completes.
 * @param service Borrowed NUL-terminated allowlisted browser capability name.
 * @param save Borrowed NUL-terminated frame property receiving the capability result, or NULL to discard it.
 * @param wait True to await the capability before continuing; false for synchronous execution.
 * @param count Number of trailing arguments; each must be an int containing a scoped browser-value handle.
 * @param ... Ordered scoped handles passed as capability arguments; zero denotes null.
 * @return Scoped calls/wait/save plan retaining browser values, not handle numbers.
 */
int lab_flow_call(int frame, unsigned next, const char *service, const char *save, bool wait, unsigned count, ...);

/**
 * @brief Execute an existing service plan at its continuation boundary.
 * @param frame Scoped mutable browser frame whose stage is advanced to next.
 * @param next Next workflow-specific stage after the effects capability finishes.
 * @param plan Scoped ordered service plan with calls and optional wait, or zero for no plan.
 * @return Scoped effects-capability plan preserving the supplied wait flag, but not its done flag.
 */
int lab_flow_effects(int frame, unsigned next, int plan);

/**
 * @brief Finish a workflow with an optional browser result.
 * @param value Scoped result handle; zero omits the value property. A handle containing false preserves explicit false.
 * @return Scoped completion record with done set to true.
 */
int lab_flow_done(int value);

/**
 * @brief Select request ownership before starting a native workflow.
 * @param kind Evaluation 0, goal 1, weather 2, solver rendering 3, startup 4, or installation 5.
 * @param frame Scoped request-local browser frame with parent; evaluation also supplies an integer mode index 0..6.
 * @post Publishes operation, mode, options and ui for kinds 0..3, an empty plan for 4/5,
 * or null for invalid kinds or an invalid evaluation mode, through lab_dom_return.
 */
void lab_flow_begin(lab_flow_kind_t kind, int frame);

/**
 * @brief Advance one native continuation without retaining scoped handles.
 * @param kind Evaluation 0, goal 1, weather 2, solver rendering 3, startup 4, installation 5;
 * request 6..15, persistence 16..25, location 26..35, binding 40..55, result action 56..62,
 * bootstrap 64, transport 65..67, or state/forms 72..75. Reserved and invalid selectors finish without a value.
 * Only kinds 0..5 use lab_flow_begin; grouped selectors are offsets into their owning dispatchers.
 * @param frame Scoped mutable browser frame with an integer stage from 0 through 65535;
 * kinds 0 and 5 also require an integer mode from 0 through 6.
 * @param view Scoped read-only browser view supplying the selected workflow's live getters and caches.
 * @post Publishes the selected continuation plan through lab_dom_return; invalid stage/mode finishes without a value.
 */
void lab_flow_step(lab_flow_kind_t kind, int frame, int view);

/**
 * @brief Advance evaluation preparation, transport and completion.
 * @param frame Scoped mutable evaluation frame with stage, mode, options and request;
 * context, preparation, data and plan fields retain intermediate browser results between calls.
 * @param view Scoped read-only browser view with editor/source getters and presentation caches used during preparation
 * and installation; getters must not start a nested DOM scope.
 * @return Scoped continuation or completion plan for the browser interpreter.
 */
int lab_flow_evaluation(int frame, int view);

/**
 * @brief Advance two-phase result installation and optional solver rendering.
 * @param frame Scoped mutable installation frame with stage, mode, data, context, outcome and request;
 * cards and solver-render results are stored as the workflow advances.
 * @param view Scoped browser view supplying presentation caches and live editor/source getters.
 * @return Scoped continuation or completion plan; asynchronous solver work retains the parent-request ownership checks.
 */
int lab_flow_install(int frame, int view);

#endif
