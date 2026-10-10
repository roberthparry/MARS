/**
 * @file lab_binding_commit.h
 * @brief Private request and completion policy for browser binding edits.
 *
 * Builds native editor merge requests from captured binding records and orders
 * accepted value/kind updates without interpreting mathematical text. Native
 * continuations own freshness checks; JavaScript executes transport. Plans retain browser values,
 * never handles, and are executed after their synchronous scope closes. This
 * main-thread interface is not an installed MARS library API.
 */
#ifndef LAB_WASM_BINDING_COMMIT_H
#define LAB_WASM_BINDING_COMMIT_H

/**
 * @brief Publish a merge request for captured binding inputs.
 * @param mode Worksheet index 0..6; only Expression 0 retains unset constants.
 * @param source Scoped opaque authored source, retained unchanged.
 * @param snapshot Scoped array of input records from lab_binding_snapshot; each present entry must be a record.
 * @details Publishes null for an empty snapshot or invalid mode. Binding identities,
 * array order and sparse positions are retained. No input, editor or cache is changed.
 */
void lab_binding_commit_request(unsigned mode, int source, int snapshot);

/**
 * @brief Publish ordered completion services for a current binding edit.
 * @param mode Worksheet index 0..6.
 * @param operation Value commit 0 or kind toggle 1.
 * @param source Scoped original authored expression.
 * @param updated Scoped native replacement expression, copied without interpretation.
 * @param context Scoped record containing snapshot, editor node and isCurrent browser callback.
 * @details Publishes calls, result and wait. Unchanged updates, and empty value
 * commits, have false result and no services. Expression kind changes await their
 * sole service; other refreshes remain background work. Invalid indices publish
 * null. The caller must verify request, editor and input freshness first. Planning
 * has no effects; the service executor stops on an exception.
 */
void lab_binding_commit_plan(unsigned mode, unsigned operation, int source, int updated, int context);

#endif
