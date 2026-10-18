/**
 * @file lab_binding_editor.h
 * @brief Application API: editor binding selection and goal-start precedence for MARS Lab.
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

#include "lab_handle.h"
#include "lab_mode.h"

/**
 * @brief Share the native variable-kind and trimmed-name selection policy.
 * @param binding Scoped native binding record; absent kind means variable.
 * @return Scoped non-empty trimmed name, or zero for constants and unnamed records.
 */
jsref lab_binding_variable_name(jsref binding);

/**
 * @brief Publish non-empty variable names in native discovery order through lab_dom_return.
 * @param bindings Scoped native binding array; constants are excluded.
 */
void lab_binding_variables(jsref bindings);

/**
 * @brief Select editable bindings without changing native binding records.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Only mode 4 excludes integration-bound names.
 * @param bindings Scoped native binding array.
 * @param bound_names Scoped Set of exact integration-bound names, or zero outside integrator mode.
 * @post The selected array is published through lab_dom_return.
 */
void lab_binding_select(lab_mode_t mode, jsref bindings, jsref bound_names);

/**
 * @brief Select native editor metadata and project editor text and readiness.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Mode 0 may request readiness refresh; mode 4 excludes integration-bound names.
 * @param inputs Scoped record containing text, metadata, body, bodyMetadata, bindings and ready.
 * @param bound_names Scoped Set of exact integration-bound names, or zero outside integrator mode.
 * @post Publishes fullText, displayText, bindings and refresh through lab_dom_return.
 * The caller retains source ownership.
 */
void lab_binding_editor(lab_mode_t mode, jsref inputs, jsref bound_names);

/**
 * @brief Prefer authored cached then solved values over supplied starts, excluding source constants.
 * @param source Scoped native source editor metadata, or zero when unavailable.
 * @param solved Scoped native solved editor metadata, or zero when unavailable.
 * @param provided Scoped supplied start record; this record is never mutated.
 * @param cache Scoped Map of exact binding names to authored values.
 * @post Publishes the independent start record through lab_dom_return; absent source or solved metadata leaves
 * the supplied starts unchanged.
 */
void lab_binding_goal_starts(jsref source, jsref solved, jsref provided, jsref cache);

/**
 * @brief Fill falsey supplied starts from native goal metadata without interpreting expressions.
 * @param editor Scoped native editor metadata containing goal_expression and starts.
 * @param provided Scoped supplied start record; this record is never mutated.
 * @post Publishes an expression/start record through lab_dom_return for asynchronous preparation by the caller.
 */
void lab_binding_goal_prepare(jsref editor, jsref provided);

#endif
