/**
 * @file lab_storage.h
 * @brief Saved worksheet normalisation and restoration interfaces for MARS Lab.
 *
 * Declares the private C/WebAssembly storage controller used by the browser's
 * state adapters. The controller supplies numeric setting validation, ordered
 * server/local field restoration, local recovery records and phased history
 * projection. Persistence schemas, numeric limits and workspace precision remain
 * owned by their corresponding modules; this interface coordinates those policies.
 *
 * The host retains storage I/O, clocks, asynchronous editor preparation and
 * ownership checks around awaits. Mathematical text remains opaque and must be
 * prepared by native MARS. Browser-value parameters are scoped handles, except
 * explicit scalar indices, flags and clock readings. Numeric browser values are
 * boxed inside the documented arrays rather than passed as raw handle arguments.
 * Calls are synchronous on the browser main thread, and no handle is retained.
 * Functions publishing structured results use lab_dom_return; those results are
 * browser values, not C return values or persistent integer handles.
 */
#ifndef LAB_WASM_STORAGE_H
#define LAB_WASM_STORAGE_H

/**
 * @brief Normalise a saved precision using decimal-prefix conversion and native limits.
 * @param values Scoped two-item array containing the saved value and numeric fallback.
 * @return Parsed precision clamped to 17..1048576 bits, or the fallback when parsing is non-finite.
 */
double lab_storage_precision(int values);

/**
 * @brief Select an offered integrator work budget from a saved setting.
 * @param values Scoped one-item array containing the saved interval-cap value.
 * @param config Scoped configuration record containing DEFAULT_INTEGRATOR_INTERVAL_CAP.
 * @return Parsed budget when it is 500, 5000, 20000, 50000 or 100000; otherwise the configured fallback.
 */
double lab_storage_intervals(int values, int config);

/**
 * @brief Validate a saved matrix operation against the current browser select options.
 * @param values Scoped one-item array containing the saved operation.
 * @return No C value; publishes the trimmed recognised operation, or "eval", through lab_dom_return.
 */
void lab_storage_operation(int values);

/**
 * @brief Prepare saved editor text and its native restoration actions.
 * @param mode Mathematical worksheet index: expression 0, equation 1, differential equation 2, matrix 3 or
 * integrator 4.
 * @param values Scoped one-item array containing the saved editor text.
 * @param local Non-zero preserves local text whitespace; zero trims server text.
 * @return No C value; publishes a record containing text and flags through lab_dom_return.
 *
 * Flags use lab_persist_restore semantics: restore 1, prepare editor 2, use native
 * canonical text 4, display expression and restore its timestamp 8, and retain the
 * server integrator source 16. Empty or rejected copies and invalid modes have zero flags.
 */
void lab_storage_editor(unsigned mode, int values, int local);

/**
 * @brief Describe control restoration in its established storage-read order.
 * @param local Non-zero selects local storage keys and ordering; zero selects server fields and ordering.
 * @return No C value; publishes an array of field/key records through lab_dom_return.
 *
 * Field identifiers are equation variable 0, matrix operation 1, matrix operand 2,
 * integrator bounds 3 and interval cap 4. The host reads and restores one field at
 * a time, awaiting native bounds preparation before accessing subsequent fields.
 */
void lab_storage_fields(int local);

/**
 * @brief Apply one saved control value without interpreting mathematical text.
 * @param field Control identifier returned by lab_storage_fields, in the range 0..4.
 * @param values Scoped one-item array containing the saved field value.
 * @param local Non-zero applies local missing-value and whitespace policy; zero applies server defaults and trimming.
 * @param config Scoped configuration containing DEFAULT_EQUATION_VARIABLE and DEFAULT_INTEGRATOR_INTERVAL_CAP.
 * @return No C value; publishes bounds requiring asynchronous preparation, or null for all other outcomes.
 *
 * Non-bounds controls are projected synchronously when present. Missing local
 * values preserve current controls, except a present empty operand clears its
 * control. Invalid field identifiers do nothing and publish null.
 */
void lab_storage_control(unsigned field, int values, int local, int config);

/**
 * @brief Restore recognised saved precision settings into native workspace ownership.
 * @param data Scoped saved-state record whose precision_bits property is a per-mode record or a legacy scalar.
 * @return No value; updates native precision in place without publishing a browser result.
 *
 * A legacy scalar updates expression mode only. Per-mode records use own fields
 * for the seven known modes; unknown or inherited fields cannot overwrite state.
 * Missing and non-finite values retain the existing precision.
 */
void lab_storage_precisions(int data);

/**
 * @brief Select a local editor copy using native timestamp and placeholder precedence.
 * @param mode Expression 0 or equation 1; other indices cannot request local recovery.
 * @param server Scoped server state containing the corresponding editor text and update timestamp.
 * @param local Scoped two-item array containing local editor text and its saved timestamp.
 * @param config Scoped configuration containing DEFAULT_EXPRESSION and DEFAULT_EQUATION.
 * @return No C value; publishes a text/updatedAt recovery record, or null when the local copy is not preferred.
 *
 * Text is trimmed without mathematical interpretation. A non-empty local copy
 * wins when newer, or when the server copy is empty or its configured placeholder.
 */
void lab_storage_recovery(unsigned mode, int server, int local, int config);

/**
 * @brief Assemble a recovered server patch after native editor preparation.
 * @param mode Expression 0 or equation 1; other indices publish an empty record.
 * @param recovery Scoped record returned by lab_storage_recovery, including its numeric updatedAt field.
 * @param values Scoped one-item array containing the prepared opaque editor text.
 * @param now Current host clock reading in Unix milliseconds, used when the saved timestamp is falsey.
 * @return No C value; publishes a mode-specific editor/timestamp patch through lab_dom_return.
 *
 * The host uses the patch timestamp for both expression ownership and persistence,
 * ensuring a single coherent fallback reading. This function performs no storage I/O.
 */
void lab_storage_recovered(unsigned mode, int recovery, int values, double now);

/**
 * @brief Plan or project history restoration at explicit asynchronous ownership boundaries.
 * @param phase Plan 0, equation/matrix controls 1, integrator bounds completion 2, or calendar editor completion 3.
 * @param state Scoped history snapshot containing its exact mode and mode-specific saved fields.
 * @param config Scoped configuration containing equation-variable, integrator-bound/budget and calendar-text defaults.
 * @return No C value; phase 0 publishes a plan, while projection phases publish no browser result.
 *
 * The plan contains mode, text, calendar, integrator, bounds and calendarState.
 * Phase 0 does not mutate controls. The host must recheck ownership after editor,
 * bounds or calendar preparation before invoking the corresponding projection.
 * Only controls relevant to the snapshot's exact mode are changed; unsupported
 * phases do nothing. Source and binding cleanup remain separately sequenced by the host.
 */
void lab_storage_history(unsigned phase, int state, int config);

#endif
