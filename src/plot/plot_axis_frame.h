/**
 * @file plot_axis_frame.h
 * @brief Private bounded geometry for connected exterior frames and positive-direction axis arrows.
 *
 * Native SVG and retained browser renderers share these display-only drawing plans.
 * Inputs are eight projected viewport corners, never mathematical coordinates.
 * Other modules use the controlled src/internal/plot_internal.h facade; public
 * scene construction remains in plot-t.h.
 */
#ifndef MARS_PLOT_AXIS_FRAME_H
#define MARS_PLOT_AXIS_FRAME_H
#if !defined(MARS_PLOT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_PLOT_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/** @brief Three adjoining box edges, indexed by coordinate axis, and their outward screen normals. */
typedef struct _plot_axis_frame_t {
    unsigned corner[3]; /**< Lower-coordinate endpoint corner; bits 0, 1 and 2 encode the x, y and z maxima. */
    double normal[3][2]; /**< Unit screen normals pointing away from the projected viewport, one per axis. */
} plot_axis_frame_t;

/** @brief Screen-space arrow extending beyond an axis's upper coordinate endpoint. */
typedef struct _plot_axis_arrow_t {
    double direction[2]; /**< Unit screen vector from the lower to the upper coordinate endpoint. */
    double tip[2]; /**< Arrow tip beyond the upper endpoint, clear of the shared frame corner. */
    double wing[2][2]; /**< Two arrowhead wings behind the tip, on opposite sides of the axis. */
    double name[2]; /**< Axis-name anchor beyond the tip, before the renderer's outward normal offset. */
} plot_axis_arrow_t;

/**
 * @brief Plan an arrow pointing strictly towards increasing coordinate values.
 * @param[in] lower Required two-number screen position of the axis's lower coordinate endpoint.
 * @param[in] upper Required two-number screen position of its upper coordinate endpoint.
 * @param[out] arrow Required output receiving the positive unit direction, tip, wings and axis-name anchor.
 * @return True on success; false for missing pointers, non-finite positions or a collapsed projected axis,
 * leaving the output unchanged. No allocation, ownership transfer or state retention occurs.
 * @details Screen position and the exterior frame's traversal order never reverse the direction.
 * The arrowhead extends clear of a shared corner. Native and retained renderers use the same plan.
 * Safe for concurrent calls with separate outputs.
 */
bool plot_axis_arrow(const double lower[2], const double upper[2], plot_axis_arrow_t *arrow);

/**
 * @brief Select three connected exterior scale edges of a projected coordinate box.
 * @param[in] corners Eight finite screen points indexed by coordinate-maximum bits x=1, y=2 and z=4.
 * @param[in] previous Optional borrowed previous frame; NULL selects an independent initial arrangement.
 * @param[out] frame Required output receiving lower endpoint corners and outward unit normals on success.
 * @return True when a non-degenerate connected frame is found; false for invalid or collapsed projections,
 * leaving the output unchanged. No allocation or ownership transfer occurs.
 * @details Examines the fixed 48 three-edge paths of a cube, retaining at least two silhouette scales.
 * Projected lengths and separation at shared corners take priority over small reductions in intrusion,
 * preventing a depth scale folding over another axis when a clearer connected path exists.
 * Among equally readable paths, prefers minimum intrusion and the lower-left silhouette.
 * Retains the previous edges while they keep two silhouette scales, at least three quarters of the best
 * projected clearance, and no more than 2.5 per cent extra intrusion relative to the viewport span.
 * The output may alias previous; invalid previous corners are ignored. Normals are always recomputed.
 * Cardinal views retain collinear adjoining edges. The bounded cost is independent of mesh size.
 * This pure function retains no state and is safe for concurrent calls with separate outputs.
 */
bool plot_axis_frame(const double corners[8][2], const plot_axis_frame_t *previous, plot_axis_frame_t *frame);

#endif
