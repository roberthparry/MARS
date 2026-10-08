/**
 * @file lab_evaluate_internal.h
 * @brief Private JSON and rendering facilities for the Lab evaluation adapter.
 *
 * Shared only by lab_evaluate-prefixed implementation files. These helpers copy
 * JSON values and preserve native text. The controlled test façade also exposes
 * the bounded integrator deadline policy and worker-failure response mapping.
 * Server callers should use lab_evaluate.h.
 */
#ifndef LAB_EVALUATE_INTERNAL_H
#define LAB_EVALUATE_INTERNAL_H

#include "lab_evaluate.h"

/** @brief Look up a borrowed JSON member using a UTF-8 key. */
const json_t *lab_eval_get(const json_t *object, const char *key);

/** @brief Return a borrowed UTF-8 member value, or an empty string. */
const char *lab_eval_text(const json_t *object, const char *key);

/** @brief Store an owned temporary value by copy and release the temporary. */
bool lab_eval_put(json_t *object, const char *key, json_t *value);

/** @brief Copy UTF-8 text into a JSON member. */
bool lab_eval_set(json_t *object, const char *key, const char *value);

/**
 * @brief Calculate the integrator worker deadline in milliseconds without overflow.
 * @param precision Requested decimal digits; zero uses the lowest precision allowance.
 * @param cap Numerical work-budget ceiling; zero adds no work-budget allowance.
 * @return Between 30000 and 120000 milliseconds, for every unsigned input.
 * @details Seconds are min(120, 30 + floor(cap/500) +
 * 10 * max(0, ceil(precision/96) - 1)). Saturation precedes multiplication.
 * The work ceiling does not remove the base or precision allowance.
 */
unsigned lab_eval_integrator_timeout(unsigned precision, unsigned cap);

/**
 * @brief Construct a failed mathematical-worker response without running a process.
 * @param status Optional destination for HTTP status 422.
 * @param process_error Saved errno from the failed process call.
 * @param timeout_ms Deadline supplied to that call, in milliseconds.
 * @return Owned JSON response, or NULL on allocation failure; release with json_free.
 * @details ETIMEDOUT yields error_code ETIMEDOUT, timeout_ms and a deadline-specific
 * diagnostic. Other failures retain a generic availability/output-limit diagnostic.
 */
json_t *lab_eval_worker_failure(unsigned *status, int process_error, unsigned timeout_ms);

/** @brief Convert native labelled worker output into owned JSON fields. */
json_t *lab_eval_fields(const string_t *output);

/**
 * @brief Display unset envelope bindings as question marks without changing literal NAN values or quoted names.
 * @param output Borrowed native Expression output; NULL returns NULL.
 * @return Owned formatted text, released with string_free, or NULL on allocation failure.
 */
string_t *lab_eval_display_bindings(const string_t *output);

/** @brief Add UI fields and native binding arrays without algebraic rewriting. */
void lab_eval_adapt(json_t *fields, const char *mode);

/**
 * @brief Render restricted mathematical TeX through bounded native processes.
 * @return Owned SVG on success; otherwise NULL and an owned diagnostic in error.
 */
string_t *lab_eval_render(const string_t *TeX, string_t **error);

/** @brief Render one native TeX field, retaining a diagnostic on failure. */
void lab_eval_render_field(json_t *fields, const char *source, const char *destination, const char *error_key);

#endif
