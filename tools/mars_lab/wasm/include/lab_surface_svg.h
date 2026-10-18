/**
 * @file lab_surface_svg.h
 * @brief Native SVG command plans for retained, projected plot geometry.
 *
 * Browser exporters supply already clipped and depth-ordered primitives and axis
 * paths. This interface owns command generation and adjacent-colour grouping;
 * the browser creates SVG nodes and applies presentation attributes separately.
 */
#ifndef MARS_LAB_SURFACE_SVG_H
#define MARS_LAB_SURFACE_SVG_H

#include "lab_handle.h"
#include "lab_surface_geometry.h"

/**
 * @brief Publish an SVG drawing plan without modifying the projected geometry.
 * @param polygons Scoped array of projected records with points and string colour, already in drawing order.
 * @param paths Scoped array of flat numeric axis paths, ordered x, y for each point in SVG coordinate units.
 * @param kind Closed polygons or open segments. Segments group adjacent same-colour primitives without closing paths.
 * Other non-zero values retain segment handling for ABI compatibility.
 * @details lab_dom_return supplies series records with path and colour strings and an axes array of strings.
 * Each accepted primitive starts with M followed by L commands; mesh paths additionally end with Z.
 * Primitive point arrays require two to twelve finite numeric coordinate pairs in SVG coordinate units.
 * Invalid records are skipped and interrupt segment grouping. Axis arrays require an even number of finite
 * coordinates and at least two pairs; invalid axes are skipped. Empty input arrays produce empty outputs.
 * Array joins keep command construction linear in output size. Per-record and per-point scopes bound temporary
 * handles independently of list length; retained strings belong to browser-owned arrays.
 */
void lab_surface_svg_plan(browser_handle polygons, browser_handle paths, lab_surface_primitive_t kind);

#endif
