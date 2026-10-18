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
 * @brief Pick a visible independent segment within ten CSS pixels in one bounded call.
 * @param view Scoped retained-view record with chart metadata, names and at most 65536 primitives.
 * @param frame Scoped projection/clipping frame returned by lab_surface_frame.
 * @param screen Scoped SVG-to-client affine transform with numeric a, b, c, d, e and f fields.
 * @param mouse_x Pointer clientX coordinate in CSS pixels.
 * @param mouse_y Pointer clientY coordinate in CSS pixels.
 * @param previous Scoped prior tooltip-plan handle, or zero; retained as the result if no closer hit is found.
 * @details Publishes a tooltip plan or null through lab_dom_return. Temporary handles are released after
 * each primitive; non-segment primitives are skipped. Oversized primitive lists, singular/non-finite transforms,
 * invalid dimensions and pointers outside the chart publish null regardless of previous. Selected markers
 * must also lie inside the chart. A valid frame with drawMesh false publishes previous without traversal.
 */
void lab_surface_hover(int view, int frame, int screen, double mouse_x, double mouse_y, int previous);

#endif
