/**
 * @file lab_solver_view.h
 * @brief Responsive solver-render snapshots and publication policy for the Lab browser.
 *
 * Prepares browser-owned request records, deduplicates current wrapping work and
 * validates asynchronous completions against their mode, card, parent, exact input
 * and restored-card owner. The browser owns request resources and supplies liveness
 * and the validated owner from lab_result_solver_owner. Existing layout APIs select
 * and project native SVG; no mathematical markup is interpreted here. All handles
 * are scoped to one synchronous call, including those copied into returned records.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_SOLVER_VIEW_H
#define LAB_WASM_SOLVER_VIEW_H

#include <stdbool.h>

#include "lab_handle.h"

/**
 * @brief Select cached layout or prepare one deferred wrapping request.
 * @param mode Scoped current worksheet mode-name string; only diffequation enables solver wrapping.
 * @param pending Scoped browser-owned pending request record, or zero.
 * @param pending_live True when the pending request still owns its native request channel.
 * @param restored_owner Scoped current restored-card owner, or zero for an evaluated card.
 * @details Returns a snapshot/request plan through lab_dom_return, or null when no
 * request is needed. Cached layout may be projected immediately. Returned compactSvg,
 * wrappedTex, parentToken, node and restoredOwner values are ordinary browser values;
 * operation, mode, options, payload and endpoint describe the deferred request.
 */
void lab_solver_view_prepare(browser_handle mode, browser_handle pending, bool pending_live, browser_handle restored_owner);

/**
 * @brief Check every solver snapshot guard without changing state or layout.
 * @param mode Scoped current worksheet mode-name string; only diffequation enables solver wrapping.
 * @param snapshot Scoped prepared request record.
 * @param active Scoped record currently owned by the browser renderer.
 * @param live True when the request controller still accepts the snapshot's request.
 * @param restored_owner Scoped currently validated restored-card owner, or zero.
 * @return True only while mode, card identity/class, parent token, exact SVG/TeX inputs,
 * restored owner and active request identity still match the snapshot.
 */
bool lab_solver_view_current(browser_handle mode, snapshot_handle snapshot, browser_handle active, bool live,
                             browser_handle restored_owner);

/**
 * @brief Publish a current render response using the latest viewport geometry.
 * @param mode Scoped current worksheet mode-name string; only diffequation enables solver wrapping.
 * @param snapshot Scoped prepared request record.
 * @param active Scoped record currently owned by the browser renderer.
 * @param live True when the native request controller accepts this request.
 * @param restored_owner Scoped currently validated restored-card owner, or zero.
 * @param response Scoped HTTP response supplying its ok flag; unused for exceptions.
 * @param data Scoped native render payload supplying ok and opaque svg fields.
 * @param failed True for a transport exception; preserve the cache and show compact SVG.
 * @return True when a current completion was handled, false for a stale snapshot.
 * @details HTTP/application failures clear the wrapped cache. Successful responses
 * retain their wrapped SVG even if the viewport has widened during the request.
 */
bool lab_solver_view_publish(browser_handle mode, snapshot_handle snapshot, browser_handle active, bool live,
                             browser_handle restored_owner, browser_handle response, browser_handle data, bool failed);

#endif
