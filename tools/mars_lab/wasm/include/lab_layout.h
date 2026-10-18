/**
 * @file lab_layout.h
 * @brief Application API: result-card projection and responsive SVG layout in C/WebAssembly.
 *
 * Drives card zoom, exclusive expansion, error state and native compact/wrapped
 * rendering through scoped browser DOM capabilities. The host supplies CSS/SVG
 * measurements and DOM operations; C owns the choices and update order. Opaque
 * handles are valid only during one synchronous entry point and are never retained.
 * No mathematical text or SVG grammar is interpreted here.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_LAYOUT_H
#define LAB_WASM_LAYOUT_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Result-card expansion operations. */
typedef enum {
    LAB_LAYOUT_EXPANSION_RENDER = 0, /**< Project current expansion state. */
    LAB_LAYOUT_EXPANSION_TOGGLE = 1, /**< Toggle the selected card or panel. */
    LAB_LAYOUT_EXPANSION_COLLAPSE = 2 /**< Collapse expanded cards. */
} lab_layout_expansion_t;

/** @brief Interpretation of a card zoom request; values retain the browser ABI. */
typedef enum {
    LAB_LAYOUT_ZOOM_ABSOLUTE = 0, /**< Select an absolute zoom-table entry. */
    LAB_LAYOUT_ZOOM_STEP = 1      /**< Move one entry in the supplied direction. */
} lab_layout_zoom_operation_t;

/**
 * @brief Measure native SVG markup using browser geometry.
 * @param markup Scoped browser handle to the SVG markup string to parse in a temporary element.
 * @return Intrinsic width in CSS pixels, falling back to view-box width; zero when no SVG is present.
 */
double lab_layout_markup_width(jsref markup);

/**
 * @brief Project a registered card's native zoom into CSS, SVG dimensions and toolbar controls.
 * @param card Scoped browser handle to a registered result-card node; zero or an unregistered card is ignored.
 */
void lab_layout_zoom(jsref card);

/**
 * @brief Change a card's native zoom, project it and schedule responsive fitting.
 * @param card Scoped browser handle to a registered result-card node.
 * @param index Zero-based zoom-table index for absolute zoom; otherwise negative moves down one entry,
 * and zero or positive moves up one entry. Absolute indices are rounded and clamped to the zoom table;
 * a non-finite absolute index selects the 100% entry.
 * @param operation Absolute entry selection or a relative step.
 * Other non-zero values retain step behaviour for ABI compatibility.
 */
void lab_layout_set_zoom(jsref card, double index, lab_layout_zoom_operation_t operation);

/**
 * @brief Project exclusive result-card or complex-panel expansion.
 * @param card Scoped browser handle to the result card or complex panel to toggle; may be zero otherwise.
 * @param operation Render current expansion 0, toggle the selected card/panel 1 or collapse cards 2;
 * other operations do nothing.
 */
void lab_layout_expand(jsref card, lab_layout_expansion_t operation);

/**
 * @brief Install native rendered output in the main rendered-result region.
 * @param svg Scoped browser handle to native SVG markup; empty text selects fallback.
 * @param fallback Scoped browser handle to plain text displayed when svg is empty.
 */
void lab_layout_content(jsref svg, jsref fallback);

/**
 * @brief Install a standalone plot without changing the evaluation cards.
 * @param result Scoped browser handle to native plot data containing svg, TeX, optional projection SVGs and notice;
 * zero denotes no result. gestureProjection restricts an eligible update to one existing projection panel.
 * @param error Scoped browser handle to a diagnostic string, or zero for success; a diagnostic retains an
 * existing coordinate-bearing chart. Both result and error zero display pending work.
 */
void lab_layout_plot(jsref result, jsref error);

/**
 * @brief Move live camera controls beside their curve, or home before replacing or saving plot markup.
 * @param home True parks controls outside the replaceable plot content; false attaches them to the 3-D curve
 * panel when available, otherwise to their home container.
 */
void lab_layout_plot_camera(bool home);

/** @brief Fit rendered output using native variant metadata and current browser geometry. */
void lab_layout_fit(void);

/**
 * @brief Decide whether current solver geometry needs the wrapped variant.
 * @return True when a distinct wrapped variant is available and the scaled compact SVG exceeds the available width;
 * false otherwise.
 */
bool lab_layout_solver_wrapped(void);

/**
 * @brief Install native solver output and record the selected rendering variant.
 * @param markup Scoped browser handle to native SVG markup; empty text leaves the current output unchanged.
 * @param variant Scoped browser handle to the variant name, normally compact or wrapped; an unchanged name
 * suppresses replacement of the existing markup.
 */
void lab_layout_solver_install(jsref markup, jsref variant);

/**
 * @brief Reset a digit-expansion button to its collapsed label and visibility.
 * @param button Scoped browser handle to the Show more digits button.
 * @param enabled True shows the button because native metadata permits expansion; false hides it.
 */
void lab_layout_more(jsref button, bool enabled);

/**
 * @brief Set or clear rendered-result error presentation.
 * @param message Scoped browser handle to diagnostic text; empty text uses Evaluation failed when setting an error.
 * Ignored when clearing error presentation.
 * @param error True invalidates rendered metadata and installs the diagnostic; false removes error styling.
 */
void lab_layout_error(jsref message, bool error);

#endif
