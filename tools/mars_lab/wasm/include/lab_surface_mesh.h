/**
 * @file lab_surface_mesh.h
 * @brief Native preparation of retained SVG display geometry for browser renderers.
 *
 * The plot adapter supplies an SVG chart produced by MARS. This interface returns
 * independent primitives, interleaved GPU attributes and validated coordinate units.
 * It reads normalised display metadata, never mathematical expressions.
 */
#ifndef MARS_LAB_SURFACE_MESH_H
#define MARS_LAB_SURFACE_MESH_H

/**
 * @brief Decode native SVG display geometry into retained primitives and GPU attributes.
 * @param chart Scoped native-produced SVG chart node carrying geometry, bounds, units, axis and tick metadata.
 * @details Publishes the decoded mesh record through lab_dom_return, or null for invalid data.
 * Segment records contain six coordinates; polygon records contain nine to fifteen.
 * Polygon fans preserve input ordering. Temporary handles are released per SVG record.
 * Empty geometry is accepted only when the chart explicitly carries data-empty="true".
 */
void lab_surface_mesh(int chart);

#endif
