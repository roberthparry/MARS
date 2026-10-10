/**
 * @file lab_binding_projection.h
 * @brief Synchronous editor-effect plans for binding source and metadata updates.
 *
 * Private browser ABI used by the binding adapter. Plans retain ordinary browser
 * values and run after their native scope closes. They order workspace writes,
 * DOM updates and deferred refreshes without interpreting mathematical source.
 */
#ifndef LAB_BINDING_PROJECTION_H
#define LAB_BINDING_PROJECTION_H

/** @brief Plan clearing authored source or only the remembered goal request.
 * @param goal_only Non-zero clears only goal source and target; zero clears all authored source state.
 */
void lab_binding_projection_clear(unsigned goal_only);

/** @brief Select the editor update appropriate to the current worksheet.
 * @param mode Worksheet index from zero to six.
 * @param updated Scoped opaque replacement text.
 * @param metadata Scoped native editor metadata, or zero when unavailable.
 */
void lab_binding_projection_update(unsigned mode, int updated, int metadata);

/** @brief Plan deferred metadata preparation only when the cache has no record.
 * @param context Scoped captured editor arguments and mode.
 * @param metadata Scoped cached metadata, or zero when absent.
 */
void lab_binding_projection_prepare(int context, int metadata);

/** @brief Plan synchronous installation of selected editor metadata and refresh policy.
 * @param selected Scoped result from lab_binding_editor.
 */
void lab_binding_projection_finish(int selected);

/** @brief Plan request admission and scheduling for a delayed edited-binding refresh.
 * @param phase Zero captures the trimmed body, cancels the old timer and begins a request;
 * one schedules an accepted request. Other values produce no effects.
 * @param context Borrowed mutable record containing editedBody and sourceExpression.
 * The browser begin callback stores request in this record between the two phases.
 * Request creation runs outside the native scope and retains its native current-mode guards.
 */
void lab_binding_projection_refresh(unsigned phase, int context);

#endif
