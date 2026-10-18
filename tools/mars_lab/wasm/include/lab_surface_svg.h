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

/**
 * @brief Return an SVG drawing plan through lab_dom_return without modifying the inputs.
 * @param polygons Array of projected records containing points and a string colour, in drawing order.
 * @param paths Array of flat numeric axis paths, ordered x, y for each point.
 * @param segments Non-zero groups adjacent same-colour primitives without closing their paths.
 * @details The result contains series records with path and colour strings, and an axes array of strings.
 * Each accepted primitive starts with M followed by L commands; mesh paths additionally end with Z.
 * Primitive point arrays must contain two to twelve finite numeric coordinate pairs. Invalid records
 * are skipped and interrupt segment grouping. Axis arrays require an even number of finite numeric
 * coordinates and at least two pairs; invalid axes are skipped. Empty input arrays produce empty outputs.
 * Array joins keep command construction linear in output size. Per-record and per-point scopes bound
 * temporary handles independently of list length; all retained strings belong to browser-owned arrays.
 */
void lab_surface_svg_plan(int polygons, int paths, int segments);

#endif
