/**
 * @file lab_flow_state_forms.h
 * @brief Clear-button and integrator-form continuations for the Lab workspace.
 *
 * Extends the native state controller with request-bound clearing and guarded
 * forms preparation. Browser-owned frames hold callbacks and responses across
 * awaits; C retains no handles. Opaque mathematical text is forwarded unchanged
 * to the native forms endpoint, apart from established empty-input defaults.
 */
#ifndef LAB_FLOW_STATE_FORMS_H
#define LAB_FLOW_STATE_FORMS_H

#include "lab_handle.h"

/** @brief Closed workflow selectors for lab_flow_state_forms; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_STATE_FORMS_CLEAR          = 0, /**< Clear the current worksheet. */
    LAB_FLOW_STATE_FORMS_REFRESH        = 1, /**< Refresh integrator forms. */
    LAB_FLOW_STATE_FORMS_PARSE_BOUNDS   = 2, /**< Parse integrator bounds. */
    LAB_FLOW_STATE_FORMS_RESTORE_BOUNDS = 3, /**< Restore integrator bounds. */
} lab_flow_state_forms_kind_t;

/**
 * @brief Advance one clear or integrator-form workflow.
 * @param kind Clear 0, refresh forms 1, parse bounds 2, restore bounds 3.
 * @param frame Scoped browser handle to the mutable continuation frame storing stage, inputs and service results.
 * @param view Reserved scoped browser-view handle; unused because expression reads use deferred services.
 * @return Scoped browser handle to the next continuation plan, or an inert completion for invalid kinds.
 * Global dispatcher kinds are 72 through 75.
 */
browser_jsref lab_flow_state_forms(lab_flow_state_forms_kind_t kind, frame_jsref frame, view_jsref view);

#endif
