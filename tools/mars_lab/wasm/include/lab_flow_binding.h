/**
 * @file lab_flow_binding.h
 * @brief Native continuations for binding controls and authored editor values.
 *
 * Used by the browser workflow dispatcher for local kinds 0–15 (global
 * kinds 40–55). Browser-owned frames retain values and promises between calls;
 * all request acceptance, ordering and recovery decisions belong to C. Opaque
 * mathematical source is sent to native MARS and is never interpreted here.
 */
#ifndef LAB_FLOW_BINDING_H
#define LAB_FLOW_BINDING_H

#include "lab_handle.h"

/** @brief Binding-card operations carried in an action workflow's frame.operation property. */
typedef enum {
    LAB_FLOW_BINDING_CARD_EVALUATE = 0, /**< Commit the binding and evaluate. */
    LAB_FLOW_BINDING_CARD_CLEAR    = 1,    /**< Queue the cleared binding and refocus its input. */
    LAB_FLOW_BINDING_CARD_COPY     = 2      /**< Copy the authored binding text. */
} lab_flow_binding_card_action_t;

/** @brief Closed workflow selectors for lab_flow_binding; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_BINDING_SYNC           = 0, /**< Synchronise binding controls. */
    LAB_FLOW_BINDING_VISIBLE        = 1, /**< Refresh visible bindings. */
    LAB_FLOW_BINDING_COMMIT         = 2, /**< Commit authored values. */
    LAB_FLOW_BINDING_TOGGLE         = 3, /**< Toggle binding kind. */
    LAB_FLOW_BINDING_REFRESH        = 4, /**< Refresh binding metadata. */
    LAB_FLOW_BINDING_QUEUE          = 5, /**< Queue an input commit. */
    LAB_FLOW_BINDING_GOAL           = 6, /**< Start goal seeking. */
    LAB_FLOW_BINDING_ROW            = 7, /**< Update an integrator row. */
    LAB_FLOW_BINDING_PREPARE_INPUT  = 8, /**< Prepare authored input. */
    LAB_FLOW_BINDING_ACTION         = 9, /**< Handle a binding-card action. */
    LAB_FLOW_BINDING_DEFERRED       = 10, /**< Apply a deferred edit. */
    LAB_FLOW_BINDING_BEGIN_COMMIT   = 11, /**< Begin a guarded commit. */
    LAB_FLOW_BINDING_EDITOR_PROJECT = 12, /**< Project editor metadata. */
    LAB_FLOW_BINDING_EDITOR_APPLY   = 13, /**< Apply edited source. */
    LAB_FLOW_BINDING_EDITOR_REFRESH = 14, /**< Refresh editor metadata. */
    LAB_FLOW_BINDING_KIND_REQUEST   = 15, /**< Request a binding-kind change. */
} lab_flow_binding_kind_t;

/**
 * @brief Advance one binding workflow without retaining scoped browser handles.
 * @param kind Local workflow: synchronise 0, visible bindings 1, commit 2, toggle 3, refresh 4, queue 5,
 * goal seek 6, integrator row 7, prepare input 8, action 9, deferred edit 10, begin commit 11,
 * editor projection 12, apply edited source 13, editor refresh 14 or binding-kind request 15.
 * @param frame Scoped browser handle to the mutable continuation frame holding stage, inputs and service results.
 * @param view Reserved scoped browser capability-view handle; unused because callbacks supply snapshots separately.
 * @return Scoped browser handle to a continuation plan, or a completed inert plan for unsupported indices.
 */
browser_handle lab_flow_binding(lab_flow_binding_kind_t kind, frame_handle frame, view_handle view);

/** @brief Closed workflow selectors for lab_flow_binding_edit; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_BINDING_EDIT_PROJECT = 0, /**< Project editor metadata. */
    LAB_FLOW_BINDING_EDIT_APPLY   = 1, /**< Apply edited source. */
    LAB_FLOW_BINDING_EDIT_REFRESH = 2, /**< Refresh editor metadata. */
} lab_flow_binding_edit_kind_t;

/**
 * @brief Advance editor projection and refresh workflows within the binding module.
 * @param kind Projection zero, edited-source application one, refresh two.
 * @param frame Scoped browser handle to the mutable continuation frame holding stage, inputs and service results.
 * @return Scoped browser handle to a continuation plan, or an inert completion plan for unsupported indices.
 */
browser_handle lab_flow_binding_edit(lab_flow_binding_edit_kind_t kind, frame_handle frame);

#endif
