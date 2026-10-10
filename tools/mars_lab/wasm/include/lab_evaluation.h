/**
 * @file lab_evaluation.h
 * @brief Private interface: evaluation preparation and result lifecycle policy for the Lab browser.
 *
 * Directly indexed mode policies choose binding preparation, input validation,
 * history updates, failure recovery and asynchronous result installation.
 * Named preparation records and ordered recovery plans keep flag interpretation
 * and diagnostic precedence here. The browser retains all awaits and
 * request ownership checks. Request outcomes come from lab_requests.c; native
 * MARS remains responsible for mathematics and its presentation. This module
 * retains no request, browser handle or authored text across calls. Returned plans
 * are browser-owned values; no persistent native storage is allocated.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_EVALUATION_H
#define LAB_WASM_EVALUATION_H

/**
 * @brief Select evaluation preparation and history policy.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param skip_history Non-zero suppresses history lookup, push and finalisation.
 * @return Flags: bindings 1, expression preparation 2, validate input 4, trim input 8,
 * DateTime snapshot 16, almanac snapshot 32, history 64. Invalid modes return zero.
 */
unsigned lab_evaluation_prepare(unsigned mode, unsigned skip_history);

/**
 * @brief Select result installation and failure recovery actions.
 * @param mode Mode index 0..6. @param outcome Stale 0, failure 1, success 2, partial Expression 3, exception 4.
 * @return Flags: install 1, error 2, clear details 4, clear error 8, clear DateTime local 16,
 * clear scalar 32, recover integration bindings/bounds 64, await installer 128, dependent weather 256.
 * Invalid/stale outcomes and partial results outside Expression mode return zero.
 */
unsigned lab_evaluation_actions(unsigned mode, unsigned outcome);

/**
 * @brief Borrow an immutable evaluation label.
 * @param mode Mode index 0..6. @param field Working 0, fallback error 1, Ready 2, Error 3, Local series 4, Not
 * solved 5.
 * @return Module-lifetime UTF-8 text, or empty for invalid indices.
 */
const char *lab_evaluation_text(unsigned mode, unsigned field);

/**
 * @brief Measure an evaluation label.
 * @param mode Mode index 0..6. @param field Label index 0..5, as for lab_evaluation_text.
 * @return Byte count excluding NUL; zero for invalid indices.
 */
unsigned lab_evaluation_text_length(unsigned mode, unsigned field);

/**
 * @brief Select completion status without interpreting mathematics.
 * @param mode Mode index 0..6. @param outcome Outcome 0..4, as for lab_evaluation_actions.
 * @param series Whether native status is exactly "series". @param solved Whether it is exactly "solved".
 * @return Ready 2, Error 3, Local series 4, Not solved 5, or empty label 6 for invalid/stale outcomes.
 * Solver flags affect successful differential-equation results only; series takes precedence.
 */
unsigned lab_evaluation_status(unsigned mode, unsigned outcome, unsigned series, unsigned solved);

/**
 * @brief Choose a rejected response's diagnostic source.
 * @param mode Mode index 0..6. @param has_error Original error truthiness; trimmed text presence for Integrator.
 * @param generic Whether trimmed error equals the fallback label. @param has_raw Whether trimmed raw error is
 * non-empty.
 * @return Fallback 0, original error 1, trimmed integration error 2, or trimmed raw diagnostic 3.
 * Invalid modes return zero; a specific integration error takes precedence over its raw diagnostic.
 */
unsigned lab_evaluation_error_source(unsigned mode, unsigned has_error, unsigned generic, unsigned has_raw);

/** @brief Publish a scoped UTF-8 label through lab_dom_return.
 * @param mode Mode index 0..6.
 * @param field Label index 0..5; invalid indices publish empty text.
 */
void lab_evaluation_label(unsigned mode, unsigned field);

/** @brief Publish named preparation flags and working status through lab_dom_return.
 * @param mode Mode index 0..6; invalid modes publish null.
 * @param skip_history Non-zero suppresses history lookup and finalisation.
 */
void lab_evaluation_prepare_plan(unsigned mode, int skip_history);

/**
 * @brief Publish ordered recovery services and response installation metadata.
 * @param mode Mode index 0..6.
 * @param outcome Stale 0, failure 1, success 2, partial Expression 3 or exception 4.
 * @param data Scoped native response; mathematical text remains opaque.
 * @param context Scoped evaluation context containing the authored text.
 * @param error Scoped explicit exception text, or null to use native diagnostic precedence.
 * @details Publishes install, awaitInstall, weather, status and calls through lab_dom_return.
 * Invalid/stale outcomes publish null. Services run after the native call returns;
 * this function does not alter cards, bindings or browser state itself.
 */
void lab_evaluation_response(unsigned mode, unsigned outcome, int data, int context, int error);

#endif
