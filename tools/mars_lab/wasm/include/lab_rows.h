/**
 * @file lab_rows.h
 * @brief Private interface: integrator row planning, structured text formatting and form revision ownership.
 *
 * Owns row activity, fallback, editing and free-parameter retention policy. The host
 * supplies presence flags and applies returned indices to its structured rows.
 * Revision tokens reject obsolete asynchronous form preparations. Structured row
 * text uses the shared workspace transfer buffers, copying authored fields verbatim.
 * No names, bounds or mathematical expressions are parsed here. Fixed planning buffers
 * match the native form limit; bounded passes visit each supplied row once per
 * phase and replace repeated host crossings and temporary JavaScript sets.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_ROWS_H
#define LAB_WASM_ROWS_H

#include <stdint.h>

/**
 * @brief Borrow the 256-word row input buffer.
 * @return Writable flags: free 1, lower present 2, upper present 4, referenced 8.
 * For reconciliation, referenced means the free parameter exists in the result.
 */
uint32_t *lab_rows_input(void);

/**
 * @brief Invalidate earlier form preparations and issue a new revision.
 * @return Non-zero revision, or zero after counter exhaustion, when no revision is accepted again.
 */
uint32_t lab_rows_revision_next(void);

/**
 * @brief Check whether a form response still owns the current revision.
 * @param revision Revision returned before starting asynchronous preparation.
 * @param same_expression Whether the browser's exact expression bytes still match the submitted text.
 * @return Non-zero only for the current non-exhausted revision and unchanged expression.
 */
int lab_rows_revision_accept(uint32_t revision, int same_expression);

/**
 * @brief Format one structured row without parsing or changing its authored text.
 * @param name Byte length of the name at the beginning of workspace staging buffer zero.
 * @param lower Byte length of the immediately following lower bound.
 * @param upper Byte length of the immediately following upper bound.
 * @param free_row Non-zero formats a free parameter and ignores its bounds.
 * @return Byte count in workspace staging buffer one, or -1 for invalid/oversized input without modifying output.
 * @details Input fields are copied verbatim. Combined input and output must each fit the workspace's 4 MiB buffer.
 */
int lab_rows_text(unsigned name, unsigned lower, unsigned upper, int free_row);

/**
 * @brief Append one structured row to workspace staging buffer one.
 * @param name Name byte count in staging buffer zero.
 * @param lower Lower-bound byte count following the name.
 * @param upper Upper-bound byte count following the lower bound.
 * @param free_row Non-zero formats a free parameter, ignoring bounds.
 * @param offset Existing output byte count; non-zero inserts a newline before the row.
 * @return New output byte count, or -1 without changing output when input or output exceeds 4 MiB.
 */
int lab_rows_text_append(unsigned name, unsigned lower, unsigned upper, int free_row, unsigned offset);

/**
 * @brief Plan active bounds and rows, retaining the original ordering.
 * @param count Number of staged rows, at most 256.
 * @return Borrowed words: bound count, active count, bound indices, active indices.
 * Index 256 denotes the fallback x = 0 .. 1 row. Null indicates invalid input;
 * failure leaves the previous plan unchanged. A sole bound always stays active.
 */
const uint32_t *lab_rows_select(unsigned count);

/**
 * @brief Reconcile result bounds with retained free parameters.
 * @param count Number of staged previous rows, at most 256.
 * @param bounds Number of new result bounds, at most 256.
 * @return Borrowed words: count followed by indices. Indices below 256 identify
 * previous rows; 256 plus an index identifies a result bound. No result bounds
 * leaves previous rows unchanged. Invalid input or more than 256 resulting rows
 * returns null without changing the previous plan.
 */
const uint32_t *lab_rows_merge(unsigned count, unsigned bounds);

/**
 * @brief Plan a row edit without changing the staged input or authored text.
 * @param count Number of staged rows, from 1 to 256.
 * @param index Existing row to edit or insert after; must be below count.
 * @param operation Toggle 1, insert bound 2 or remove 3.
 * @return Borrowed count followed by index/flags pairs. Index 256 requests a new
 * blank bound. Toggle-to-free clears bound-presence flags. At least one bound
 * is retained or appended; removing the last bound or sole row is rejected.
 * Invalid input or a result over 256 rows returns null, preserving the previous
 * output. The host copies the plan before calling another planner.
 */
const uint32_t *lab_rows_edit(unsigned count, unsigned index, unsigned operation);

#endif
