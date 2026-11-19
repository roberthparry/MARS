/**
 * @file lab_surface_axes.h
 * @brief Native display-axis layout for retained three-dimensional plots.
 *
 * The retained geometry renderer supplies the camera and normalised viewport.
 * This application interface builds bounded drawing plans for Canvas and SVG
 * export without reading or interpreting mathematical expressions.
 */
#ifndef MARS_LAB_SURFACE_AXES_H
#define MARS_LAB_SURFACE_AXES_H

#include "lab_handle.h"

/**
 * @brief Attach connected exterior axes and collision-spaced labels to a retained camera frame.
 * @param[in] view Borrowed scoped view containing axis names, ticks and volume status.
 * @param[in,out] frame Borrowed scoped frame containing rows, fit, dimensions, scaling and offset arrays.
 * Receives browser-owned paths, texts and axisCorners arrays, replacing any previous axis plan.
 * @details Selects three adjoining silhouette edges using the same bounded native routine as SVG output.
 * Collapsed projections receive empty drawing arrays. Labels remain upright; crowded numeric labels are omitted.
 * No handles are retained after this synchronous operation, and neither mathematical bounds nor geometry change.
 */
void lab_surface_axes(view_jsref view, frame_jsref frame);

#endif
