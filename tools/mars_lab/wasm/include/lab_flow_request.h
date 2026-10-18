/**
 * @file lab_flow_request.h
 * @brief Request-specific browser continuations for the Lab's private WebAssembly runtime.
 *
 * Separates application sequencing and response policy from browser-owned network
 * resources. The central continuation dispatcher calls this module with a local
 * workflow index; all frame and view handles expire at the end of that call.
 */
#ifndef LAB_FLOW_REQUEST_H
#define LAB_FLOW_REQUEST_H

#include "lab_handle.h"

/** @brief Closed workflow selectors for lab_flow_request; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_REQUEST_FUNCTION     = 0, /**< Run a Function programme. */
    LAB_FLOW_REQUEST_CALCULUS     = 1, /**< Request calculus or plot output. */
    LAB_FLOW_REQUEST_INTEGRATOR   = 2, /**< Evaluate an integrator worksheet. */
    LAB_FLOW_REQUEST_LAND         = 3, /**< Fetch land-totality data. */
    LAB_FLOW_REQUEST_HOLIDAYS     = 4, /**< Fetch local holidays. */
    LAB_FLOW_REQUEST_MOBILE       = 5, /**< Refresh mobile access information. */
    LAB_FLOW_REQUEST_FORMS        = 6, /**< Prepare forms. */
    LAB_FLOW_REQUEST_PRESENTATION = 7, /**< Prepare evaluation presentation. */
    LAB_FLOW_REQUEST_SOLVER       = 8, /**< Render solver output on resize. */
    LAB_FLOW_REQUEST_FETCH        = 9, /**< Fetch a worksheet evaluation. */
} lab_flow_request_kind_t;

/**
 * @brief Advance a request workflow without retaining browser handles.
 * @param kind Local workflow: Function 0, calculus 1, integrator 2, land 3,
 * holidays 4, mobile 5, forms 6, presentation 7, solver resize 8, ordinary fetch 9.
 * @param frame Scoped browser handle to the mutable continuation frame storing stage, inputs and service results.
 * @param view Scoped browser handle to the application capability view used for presentation; ignored by other kinds.
 * @return Scoped browser handle to a continuation plan for the host interpreter; an inert completion for invalid kinds.
 */
browser_handle lab_flow_request(lab_flow_request_kind_t kind, frame_handle frame, view_handle view);

#endif
