/**
 * @file lab_binding_projection.h
 * @brief Synchronous editor-effect plans for binding source and metadata updates.
 *
 * Private browser ABI used by the binding adapter. Plans retain ordinary browser
 * values and run after their native scope closes. They order workspace writes,
 * DOM updates and deferred refreshes without interpreting mathematical source.
 * Boolean arguments cross the WebAssembly i32 boundary as canonical 0 or 1, never scoped handles.
 */
#ifndef LAB_BINDING_PROJECTION_H
#define LAB_BINDING_PROJECTION_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_mode.h"

/** @brief Stage of a delayed binding refresh, separated by browser request admission. */
typedef enum {
    LAB_BINDING_REFRESH_BEGIN = 0,   /**< Capture text, cancel the old timer and request admission. */
    LAB_BINDING_REFRESH_SCHEDULE = 1 /**< Schedule the admitted refresh after 300 milliseconds. */
} lab_binding_refresh_phase_t;

/**
 * @brief Plan clearing authored source or only the remembered goal request.
 * @param goal_only True plans clearing goal source and target only; false plans clearing fullText,
 * displayText, lastInput, cached values, editor flags and the remembered goal request.
 * @post Publishes an ordered calls plan through lab_dom_return without executing services.
 */
void lab_binding_projection_clear(bool goal_only);

/**
 * @brief Select the editor update appropriate to the current worksheet.
 * @param mode Current worksheet identity. Expression, equation and differential-equation modes use the full editor;
 * other modes do so only for wrapped metadata.
 * @param updated Scoped opaque replacement text.
 * @param metadata Scoped native editor metadata; its wrapped flag selects full-editor projection.
 * @post Publishes an ordered calls plan through lab_dom_return.
 */
void lab_binding_projection_update(lab_mode_t mode, jsref updated, jsref metadata);

/**
 * @brief Plan deferred metadata preparation only when the cache has no record.
 * @param context Scoped captured editor arguments and mode.
 * @param metadata Scoped cached metadata, or zero when absent.
 * @post Publishes a calls plan through lab_dom_return; a truthy cache entry requires no deferred service.
 */
void lab_binding_projection_prepare(jsref context, jsref metadata);

/**
 * @brief Plan synchronous installation of selected editor metadata and refresh policy.
 * @param selected Scoped result from lab_binding_editor with fullText, displayText, bindings and refresh fields.
 * @post Publishes ordered source, control and optional refresh effects through lab_dom_return.
 */
void lab_binding_projection_finish(jsref selected);

/**
 * @brief Plan request admission and scheduling for a delayed edited-binding refresh.
 * @param phase LAB_BINDING_REFRESH_BEGIN (0) captures text and requests admission;
 * LAB_BINDING_REFRESH_SCHEDULE (1) schedules an accepted request. Other values produce no effects.
 * @param context Scoped mutable browser record containing editedBody and sourceExpression.
 * The browser begin callback stores request in this record between the two phases.
 * @post Publishes an ordered calls plan through lab_dom_return. Phase 1 schedules a 300-millisecond refresh
 * only when the request is truthy; request creation runs outside the native scope.
 */
void lab_binding_projection_refresh(lab_binding_refresh_phase_t phase, jsref context);

#endif
