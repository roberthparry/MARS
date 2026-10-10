/**
 * @file lab_binding_editor.h
 * @brief Private interface: editor binding selection and goal-start precedence for MARS Lab.
 *
 * Selects visible parameters and native editor metadata, projects editor
 * readiness, and combines supplied, authored and solved goal starts. Native
 * mathematical text is copied without interpretation. Native continuations own
 * asynchronous request ordering and stale-response checks. Browser handles are
 * scoped to a synchronous call; name indexes retain exact native identities.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_BINDING_EDITOR_H
#define LAB_WASM_BINDING_EDITOR_H

/** @brief Share the native variable-kind and trimmed-name selection policy.
 * @param binding Scoped native binding record; absent kind means variable.
 * @return Scoped non-empty trimmed name, or zero for constants and unnamed records.
 */
int lab_binding_variable_name(int binding);

/** @brief Publish non-empty variable names in native discovery order through lab_dom_return.
 * @param bindings Scoped native binding array; constants are excluded.
 */
void lab_binding_variables(int bindings);

/** @brief Select editable bindings without changing native binding records.
 * @param mode Workspace mode identifier; integrator mode 4 excludes bound names.
 * @param bindings Scoped native binding array.
 * @param bound_names Scoped Set of exact integration-bound names, or zero outside integrator mode.
 * The selected array is published through lab_dom_return.
 */
void lab_binding_select(unsigned mode, int bindings, int bound_names);

/** @brief Select native editor metadata and project editor text and readiness.
 * @param mode Workspace mode identifier; expression mode 0 may request readiness refresh.
 * @param inputs Scoped record containing text, metadata, body, bodyMetadata, bindings and ready.
 * @param bound_names Scoped Set of exact integration-bound names, or zero outside integrator mode.
 * Publishes fullText, displayText, bindings and refresh through lab_dom_return; the caller retains source ownership.
 */
void lab_binding_editor(unsigned mode, int inputs, int bound_names);

/** @brief Prefer authored cached then solved values over supplied starts, excluding source constants.
 * @param source Scoped native source editor metadata, or zero when unavailable.
 * @param solved Scoped native solved editor metadata, or zero when unavailable.
 * @param provided Scoped supplied start record; this record is never mutated.
 * @param cache Scoped Map of exact binding names to authored values.
 * The independent start record is published through lab_dom_return.
 */
void lab_binding_goal_starts(int source, int solved, int provided, int cache);

/** @brief Fill falsey supplied starts from native goal metadata without interpreting expressions.
 * @param editor Scoped native editor metadata containing goal_expression and starts.
 * @param provided Scoped supplied start record; this record is never mutated.
 * Publishes an expression/start record through lab_dom_return for asynchronous preparation by the caller.
 */
void lab_binding_goal_prepare(int editor, int provided);

#endif
