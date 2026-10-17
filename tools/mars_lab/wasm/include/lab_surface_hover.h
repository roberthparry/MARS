/**
 * @file lab_surface_hover.h
 * @brief Native screen-space inspection of retained curve segments.
 *
 * The browser supplies a retained frame and screen transform. Native code selects
 * a nearby visible sample and formats approximate coordinates for display.
 */
#ifndef MARS_LAB_SURFACE_HOVER_H
#define MARS_LAB_SURFACE_HOVER_H

/** @brief Pick an independent segment within ten CSS pixels, returning a tooltip plan or null. */
void lab_surface_hover(int view, int frame, int screen, double mouse_x, double mouse_y, int previous);

#endif
