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

#include "lab_handle.h"

/** @brief Exact metadata projection requested by an editor consumer. */
typedef enum {
    LAB_EDITOR_LOOKUP = 0, /**< Complete native metadata record. */
    LAB_EDITOR_EXPRESSION = 1, /**< Canonical expression with trimmed-source fallback. */
    LAB_EDITOR_RESTORE = 2, /**< Authored text with trimmed-source fallback. */
    LAB_EDITOR_BODY = 3, /**< Expression body with trimmed-source fallback. */
    LAB_EDITOR_BINDINGS = 4, /**< Wrapped binding metadata, when present. */
    LAB_EDITOR_COMPACT = 5 /**< Compact display, bindings and shortening status. */
} lab_editor_metadata_t;

/**
 * @brief Look up exact metadata after browser-compatible source conversion and trimming.
 * @param text Scoped input value; falsey values select the empty string.
 * @param editors Scoped Map containing native editor metadata.
 * @return Scoped borrowed metadata handle, or zero for missing or falsey entries.
 */
browser_jsref lab_editor_lookup(browser_jsref text, browser_jsref editors);

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
browser_jsref lab_editor_current_value(view_jsref view);

/**
 * @brief Publish one editor metadata projection without interpreting mathematics.
 * @param kind Lookup 0, canonical expression 1, restored source 2, body 3,
 * wrapped binding record 4, or compact presentation 5; other values return null.
 * @param inputs Scoped one-element array boxing the source-text input, including numeric values.
 * @param editors Scoped Map of native editor metadata keyed by trimmed source or display text.
 * @post Publishes the selected projection through lab_dom_return; invalid kinds publish null.
 */
void lab_editor_metadata(lab_editor_metadata_t kind, browser_jsref inputs, browser_jsref editors);

/**
 * @brief Publish current-source selection through the scoped browser bridge.
 * @param view Scoped production or custom view as for lab_editor_current_value.
 * @post Publishes the selected source through lab_dom_return.
 */
void lab_editor_current(view_jsref view);

/**
 * @brief Publish whether the selected worksheet has enough editor input to evaluate.
 * @param inputs Scoped one-element array containing a worksheet mode name; only "expression" requires source text.
 * @param view Scoped current-source view, inspected only for Expression mode.
 * @post Publishes a boolean through lab_dom_return, true for any mode other than Expression or a truthy source.
 */
void lab_editor_ready(browser_jsref inputs, view_jsref view);

#endif
