/**
 * @file tba_view.h
 * @brief Native result, SVG, legend and tooltip presentation.
 *
 * Generates markup from structured native results without mathematical parsing.
 * Browser event callbacks supply pointer coordinates and DOM capabilities only.
 */
#ifndef TBA_VIEW_H
#define TBA_VIEW_H

#include "tba_handle.h"
/**
 * @brief Display a structured successful forecast.
 * @param data Scoped Protobuf result.
 */
void tba_view_result(browser_jsref data);

/**
 * @brief Draw a structured chart.
 * @param data Scoped chart object.
 */
void tba_view_chart(browser_jsref data);

/**
 * @brief Apply chart legend visibility.
 */
void tba_view_visibility(void);

/**
 * @brief Update a tooltip from a pointer event.
 * @param event Scoped event.
 */
void tba_view_hover(event_jsref event);

/**
 * @brief Hide the current tooltip.
 */
void tba_view_leave(void);

/**
 * @brief Activate a comparison tab.
 * @param event Scoped click event.
 */
void tba_view_tab(event_jsref event);
#endif
