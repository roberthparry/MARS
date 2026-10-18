/**
 * @file lab_tooltip.h
 * @brief Application API: accessible control hints and their DOM projection for the WASM client.
 *
 * Owns label precedence, authored-title caching, description restoration and
 * viewport placement. The host retains only the active node for event delivery,
 * including restoration after detachment. No scoped DOM handles survive a call.
 * Browser primitives supply Unicode casing/whitespace and measured rectangles;
 * this module never interprets mathematical content.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_TOOLTIP_H
#define LAB_WASM_TOOLTIP_H

#include <stdbool.h>

#include "lab_handle.h"

/**
 * @brief Project tooltip text, accessibility and viewport placement.
 * @param button Scoped candidate button-node handle.
 * @param previous Scoped previously active node handle, including a detached node, or zero for none.
 * @param width Viewport width in CSS pixels.
 * @param height Viewport height in CSS pixels.
 * @return True when accepted; false for an absent node or one within a hidden container, leaving the tooltip unchanged.
 */
bool lab_tooltip_show(jsref button, jsref previous, double width, double height);

/**
 * @brief Hide the tooltip and restore original accessibility attributes exactly.
 * @param button Scoped active button-node handle, or zero when no control owns the tooltip.
 */
void lab_tooltip_hide(jsref button);

#endif
