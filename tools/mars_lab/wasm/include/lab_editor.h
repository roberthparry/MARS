/**
 * @file lab_editor.h
 * @brief Native selection of opaque editor metadata and retained worksheet sources.
 *
 * Looks up complete server-supplied source/display mappings and selects their
 * canonical expression, body and binding records. No mathematical text is parsed,
 * reordered or reconstructed here. Browser maps and views are borrowed for one
 * scoped call. Current-source resolution uses the workspace's existing UTF-8
 * buffers; the host validates and stages editor bytes immediately before use.
 */
#ifndef LAB_EDITOR_H
#define LAB_EDITOR_H

/**
 * @brief Look up exact metadata after browser-compatible source conversion and trimming.
 * @param text Scoped input value; falsey values select the empty string.
 * @param editors Scoped Map containing native editor metadata.
 * @return Borrowed metadata handle, or zero for missing or falsey entries.
 */
int lab_editor_lookup(int text, int editors);

/**
 * @brief Select the current source from a lazy production or custom evaluation view.
 * @param view Scoped view. A production rawEditor getter stages UTF-8 bytes and
 * returns their length and trimmed text; editors supplies native metadata. Custom
 * views without rawEditor instead supply the existing lazy expressionText getter.
 * @return Scoped retained source, exact metadata source or trimmed browser text.
 * @details The rawEditor getter must stage buffer zero immediately before returning
 * and must not open another DOM scope. The selected output is copied before further
 * workspace reads. No view property is overwritten.
 */
int lab_editor_current_value(int view);

/**
 * @brief Publish one editor metadata projection without interpreting mathematics.
 * @param kind Lookup 0, canonical expression 1, restored source 2, body 3,
 * wrapped binding record 4, or compact presentation 5; other values return null.
 * @param inputs Scoped one-element array boxing the input, including numeric values.
 * @param editors Scoped Map of native editor metadata.
 */
void lab_editor_metadata(unsigned kind, int inputs, int editors);

/**
 * @brief Publish current-source selection through the scoped browser bridge.
 * @param view Scoped production or custom view as for lab_editor_current_value.
 */
void lab_editor_current(int view);

/**
 * @brief Publish whether the selected worksheet has enough editor input to evaluate.
 * @param inputs Scoped one-element array containing the mode name.
 * @param view Scoped current-source view, inspected only for Expression mode.
 */
void lab_editor_ready(int inputs, int view);

#endif
