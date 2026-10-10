/**
 * @file lab_flow_binding.h
 * @brief Native continuations for binding controls and authored editor values.
 *
 * Used by the private browser workflow dispatcher for local kinds 0–15 (global
 * kinds 40–55). Browser-owned frames retain values and promises between calls;
 * all request acceptance, ordering and recovery decisions belong to C. Opaque
 * mathematical source is sent to native MARS and is never interpreted here.
 */
#ifndef LAB_FLOW_BINDING_H
#define LAB_FLOW_BINDING_H

/**
 * @brief Advance one binding workflow without retaining scoped browser handles.
 * @param kind Local binding workflow index, from zero to fifteen.
 * @param frame Borrowed mutable continuation frame.
 * @param view Borrowed browser capability view; binding callbacks supply snapshots separately.
 * @return Scoped continuation plan, or a completed inert plan for unsupported indices.
 */
int lab_flow_binding(unsigned kind, int frame, int view);

/**
 * @brief Advance editor projection and refresh workflows within the binding module.
 * @param kind Projection zero, edited-source application one, refresh two.
 * @param frame Borrowed mutable continuation frame.
 * @return Scoped continuation plan.
 */
int lab_flow_binding_edit(unsigned kind, int frame);

#endif
