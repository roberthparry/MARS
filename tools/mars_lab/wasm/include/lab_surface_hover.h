/**
 * @file lab_surface_hover.h
 * @brief Native screen-space inspection of retained curve segments.
 *
 * The browser supplies a retained frame and screen transform. Native code selects
 * a nearby visible sample and formats approximate coordinates for display.
 */
#ifndef MARS_LAB_SURFACE_HOVER_H
#define MARS_LAB_SURFACE_HOVER_H

/**
 * @brief Pick an independent segment within ten CSS pixels in one bounded call.
 * @param view Retained chart and at most 65536 primitives; larger lists return null, even with a previous hit.
 * @param frame Retained projection and clipping frame.
 * @param screen SVG-to-screen affine transform.
 * @param mouse_x Pointer horizontal coordinate in CSS pixels.
 * @param mouse_y Pointer vertical coordinate in CSS pixels.
 * @param previous Optional prior tooltip plan retained for compatibility when no closer sample is found.
 * Temporary handles are released after each primitive; non-segment primitives are skipped.
 * Singular or non-finite transforms, invalid dimensions and pointers outside the chart return null
 * before traversal, regardless of previous. Both pointers and selected markers must lie inside the chart.
 * Returns a tooltip plan or null through lab_dom_return, without retaining host handles.
 */
void lab_surface_hover(int view, int frame, int screen, double mouse_x, double mouse_y, int previous);

#endif
