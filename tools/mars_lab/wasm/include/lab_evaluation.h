/**
 * @file lab_evaluation.h
 * @brief Application API: evaluation preparation and result lifecycle policy for the Lab browser.
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
 * Boolean arguments cross the WebAssembly i32 boundary as canonical 0 or 1, never scoped handles.
 */
#ifndef LAB_WASM_EVALUATION_H
#define LAB_WASM_EVALUATION_H

#include <stdbool.h>

#include "lab_mode.h"
#include "lab_requests.h"

/** @brief Evaluation label selector, including the status returned at completion. */
typedef enum {
    LAB_EVALUATION_LABEL_WORKING = 0,      /**< Mode-specific in-progress message. */
    LAB_EVALUATION_LABEL_FAILURE = 1,      /**< Mode-specific fallback diagnostic. */
    LAB_EVALUATION_LABEL_READY = 2,        /**< Successful completion. */
    LAB_EVALUATION_LABEL_ERROR = 3,        /**< Failed, partial or exceptional completion. */
    LAB_EVALUATION_LABEL_LOCAL_SERIES = 4, /**< Differential equation reduced to a local series. */
    LAB_EVALUATION_LABEL_NOT_SOLVED = 5,   /**< Differential equation not solved symbolically. */
    LAB_EVALUATION_LABEL_EMPTY = 6         /**< No status for stale or invalid work; renders empty text. */
} lab_evaluation_label_t;

/** @brief Source of the diagnostic shown for a rejected evaluation response. */
typedef enum {
    LAB_EVALUATION_ERROR_FALLBACK = 0, /**< Mode-specific fallback diagnostic. */
    LAB_EVALUATION_ERROR_ORIGINAL = 1, /**< Original native error without trimming. */
    LAB_EVALUATION_ERROR_TRIMMED = 2,  /**< Trimmed integration diagnostic. */
    LAB_EVALUATION_ERROR_RAW = 3       /**< Trimmed raw integration diagnostic. */
} lab_evaluation_error_source_t;

/**
 * @brief Select evaluation preparation and history policy.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param skip_history True suppresses history lookup, push and finalisation.
 * @return Flags: bindings 1, expression preparation 2, validate input 4, trim input 8,
 * DateTime snapshot 16, almanac snapshot 32, history 64. Invalid modes return zero.
 */
unsigned lab_evaluation_prepare(lab_mode_t mode, bool skip_history);

/**
 * @brief Select result installation and failure recovery actions.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param outcome Stale 0, failure 1, success 2, partial Expression 3, exception 4.
 * @return Flags: install 1, error 2, clear details 4, clear error 8, clear DateTime local 16,
 * clear scalar 32, recover integration bindings/bounds 64, await installer 128, dependent weather 256.
 * Invalid/stale outcomes and partial results outside Expression mode return zero.
 */
unsigned lab_evaluation_actions(lab_mode_t mode, lab_request_outcome_t outcome);

/**
 * @brief Borrow an immutable evaluation label.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param field Working 0, fallback error 1, Ready 2, Error 3, Local series 4, or Not solved 5.
 * @return Module-lifetime UTF-8 text, or empty for invalid indices.
 */
const char *lab_evaluation_text(lab_mode_t mode, lab_evaluation_label_t field);

/**
 * @brief Measure an evaluation label.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param field Label index 0..5, as for lab_evaluation_text.
 * @return Byte count excluding NUL; zero for invalid indices.
 */
unsigned lab_evaluation_text_length(lab_mode_t mode, lab_evaluation_label_t field);

/**
 * @brief Select completion status without interpreting mathematics.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param outcome Outcome 0..4, as for lab_evaluation_actions.
 * @param series True when native status is exactly "series"; only successful differential equations inspect it.
 * @param solved True when native status is exactly "solved"; ignored when series is true.
 * @return Ready 2, Error 3, Local series 4, Not solved 5, or empty label 6 for invalid/stale outcomes.
 * Solver flags affect successful differential-equation results only; series takes precedence.
 */
lab_evaluation_label_t lab_evaluation_status(lab_mode_t mode, lab_request_outcome_t outcome, bool series,
                                            bool solved);

/**
 * @brief Choose a rejected response's diagnostic source.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param has_error True when the original error is truthy; for Integrator, true when its trimmed text is non-empty.
 * @param generic True when the trimmed error equals the mode's fallback label; relevant only for Integrator.
 * @param has_raw True when the trimmed raw diagnostic is non-empty; relevant only for Integrator.
 * @return Fallback 0, original error 1, trimmed integration error 2, or trimmed raw diagnostic 3.
 * Invalid modes return zero; a specific integration error takes precedence over its raw diagnostic.
 */
lab_evaluation_error_source_t lab_evaluation_error_source(lab_mode_t mode, bool has_error, bool generic,
                                                        bool has_raw);

/**
 * @brief Publish a scoped UTF-8 label through lab_dom_return.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param field Working 0, fallback error 1, Ready 2, Error 3, Local series 4, or Not solved 5;
 * invalid indices publish empty text.
 */
void lab_evaluation_label(lab_mode_t mode, lab_evaluation_label_t field);

/**
 * @brief Publish named preparation flags and working status through lab_dom_return.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Invalid modes publish null.
 * @param skip_history True suppresses history lookup and finalisation.
 */
void lab_evaluation_prepare_plan(lab_mode_t mode, bool skip_history);

/**
 * @brief Publish ordered recovery services and response installation metadata.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param outcome Stale 0, failure 1, success 2, partial Expression 3 or exception 4.
 * @param data Scoped native response; mathematical text remains opaque.
 * @param context Scoped evaluation context containing the authored text.
 * @param error Scoped explicit exception text, or zero to use the native response's diagnostic precedence.
 * @details Publishes install, awaitInstall, weather, status and calls through lab_dom_return.
 * Invalid/stale outcomes publish null. Services run after the native call returns;
 * this function does not alter cards, bindings or browser state itself.
 */
void lab_evaluation_response(lab_mode_t mode, lab_request_outcome_t outcome, int data, int context, int error);

#endif
