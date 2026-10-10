/**
 * @file lab_tooltip.h
 * @brief Private interface: accessible control hints and their DOM projection for the WASM client.
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

/** @brief Project tooltip text, accessibility and viewport placement. @param button Candidate button node.
 * @param previous Previous active node, including a detached node, or zero.
 * @param width Viewport width. @param height Viewport height.
 * @return One when accepted; zero for absent or hidden candidates, leaving the current tooltip unchanged. */
int lab_tooltip_show(int button, int previous, double width, double height);

/** @brief Hide the tooltip and restore original accessibility attributes exactly. @param button Active node or zero. */
void lab_tooltip_hide(int button);

#endif
