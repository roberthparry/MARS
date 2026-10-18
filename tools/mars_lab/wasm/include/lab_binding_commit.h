/**
 * @file lab_binding_commit.h
 * @brief Application API for request and completion policy for browser binding edits.
 *
 * Builds native editor merge requests from captured binding records and orders
 * accepted value/kind updates without interpreting mathematical text. Native
 * continuations own freshness checks; JavaScript executes transport. Plans retain browser values,
 * never handles, and are executed after their synchronous scope closes. This
 * main-thread interface is not an installed MARS library API.
 */
#ifndef LAB_WASM_BINDING_COMMIT_H
#define LAB_WASM_BINDING_COMMIT_H

#include "lab_handle.h"
#include "lab_mode.h"

/** @brief Accepted binding edit whose completion services must be planned. */
typedef enum {
    LAB_BINDING_COMMIT_VALUE = 0, /**< Commit captured input values. */
    LAB_BINDING_COMMIT_KIND = 1   /**< Toggle a binding between constant and variable. */
} lab_binding_commit_operation_t;

/**
 * @brief Publish a merge request for captured binding inputs.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Only Expression preserves unset constants; other modes set the request flag unset_constants.
 * @param source Scoped opaque authored source, retained unchanged.
 * @param snapshot Scoped array of input records from lab_binding_snapshot; each present entry must be a record.
 * @details Publishes null for an empty snapshot or invalid mode. Binding identities,
 * array order and sparse positions are retained. No input, editor or cache is changed.
 */
void lab_binding_commit_request(lab_mode_t mode, browser_handle source, snapshot_handle snapshot);

/**
 * @brief Publish ordered completion services for a current binding edit.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param operation LAB_BINDING_COMMIT_VALUE (0) or LAB_BINDING_COMMIT_KIND (1).
 * @param source Scoped original authored expression.
 * @param updated Scoped native replacement expression, copied without interpretation.
 * @param context Scoped record containing snapshot, editor node and isCurrent browser callback.
 * @details Publishes calls, result and wait. Unchanged updates, and empty value
 * commits, have false result and no services. Expression kind changes await their
 * sole service; other refreshes remain background work. Invalid indices publish
 * null. The caller must verify request, editor and input freshness first. Planning
 * has no effects; the service executor stops on an exception.
 */
void lab_binding_commit_plan(lab_mode_t mode, lab_binding_commit_operation_t operation, browser_handle source,
                             browser_handle updated, browser_handle context);

#endif
