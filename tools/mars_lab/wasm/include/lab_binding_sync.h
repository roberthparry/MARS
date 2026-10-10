/**
 * @file lab_binding_sync.h
 * @brief Private request normalisation and accepted binding-edit projection for MARS Lab.
 *
 * Prepares opaque binding records for the native presentation service and projects
 * an accepted edited expression into its editor flags and variable metadata. No
 * mathematical text is parsed or rewritten. Native continuations own freshness
 * checks and workspace source policy; JavaScript supplies promises and browser events.
 * Handles are borrowed for one synchronous call; returned objects retain browser
 * values rather than handles. These functions run on the browser main thread and
 * are not part of the installed MARS library API.
 */
#ifndef LAB_WASM_BINDING_SYNC_H
#define LAB_WASM_BINDING_SYNC_H

/**
 * @brief Prepare a binding replacement request with browser-compatible defaults.
 * @param inputs Scoped record containing body and bindings.
 * @details Publishes source and an optional request through lab_dom_return. Empty
 * source requires no service; a non-empty source without a request requires editor
 * preparation only. Present nullish binding entries publish invalid=true, which
 * the host raises as TypeError before calling a service. Array holes and inherited
 * indices retain Array.map semantics. Values remain opaque and inputs are not mutated.
 */
void lab_binding_sync_request(int inputs);

/**
 * @brief Project editor metadata after the host accepts an asynchronous binding edit.
 * @param editor Scoped editor node receiving the accepted display text and readiness flags.
 * @param inputs Scoped record containing displayText, bindings and native response data.
 * @details Publishes variables and differentiable through lab_dom_return. Variable
 * names use the existing binding selection policy; response flags retain the original
 * truthy defaults and browser Unicode conversion. The caller must check request and
 * editor freshness before calling and retains source storage and subsequent services.
 */
void lab_binding_sync_apply(int editor, int inputs);

#endif
