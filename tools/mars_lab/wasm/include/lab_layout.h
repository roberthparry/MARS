/**
 * @file lab_layout.h
 * @brief Private interface: result-card projection and responsive SVG layout in C/WebAssembly.
 *
 * Drives card zoom, exclusive expansion, error state and native compact/wrapped
 * rendering through scoped browser DOM capabilities. The host supplies CSS/SVG
 * measurements and DOM operations; C owns the choices and update order. Opaque
 * handles are valid only during one synchronous entry point and are never retained.
 * No mathematical text or SVG grammar is interpreted here.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_LAYOUT_H
#define LAB_WASM_LAYOUT_H

/** @brief Measure native markup. @param markup Scoped SVG string. @return Intrinsic width in pixels. */
double lab_layout_markup_width(int markup);

/** @brief Project card zoom. @param card Scoped registered result card. */
void lab_layout_zoom(int card);

/** @brief Change and project zoom. @param card Scoped card. @param index Index or signed step.
 * @param step Non-zero interprets index as step direction. */
void lab_layout_set_zoom(int card, double index, int step);

/** @brief Project exclusive expansion. @param card Scoped card, required only for toggle.
 * @param operation Render 0, toggle 1, collapse 2; other operations do nothing. */
void lab_layout_expand(int card, unsigned operation);

/** @brief Install native rendered output. @param svg Scoped SVG string. @param fallback Scoped text fallback. */
void lab_layout_content(int svg, int fallback);

/** @brief Fit rendered output using native variant metadata and current browser geometry. */
void lab_layout_fit(void);

/** @brief Decide whether current solver geometry needs the wrapped variant. @return Non-zero when needed. */
int lab_layout_solver_wrapped(void);

/** @brief Install native solver output. @param markup Scoped SVG string. @param variant Scoped variant name. */
void lab_layout_solver_install(int markup, int variant);

/** @brief Reset expansion controls. @param button Scoped button. @param enabled Native expansion availability. */
void lab_layout_more(int button, int enabled);

/** @brief Set/clear rendered error state. @param message Scoped error text. @param error Non-zero sets an error. */
void lab_layout_error(int message, int error);

#endif
