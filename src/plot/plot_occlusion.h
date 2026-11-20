/**
 * @file plot_occlusion.h
 * @brief Private projective visibility clipping for axis faces behind opaque plot surfaces.
 *
 * Native SVG and the retained Canvas/export paths use this small spatial index.
 * Screen coordinates carry an affine nearness value: reciprocal eye distance for
 * perspective cameras, or negative camera depth for orthographic cameras. Ordinary
 * plot consumers use plot-t.h; browser adapters use the controlled plot facade.
 */
#ifndef MARS_PLOT_OCCLUSION_H
#define MARS_PLOT_OCCLUSION_H
#if !defined(MARS_PLOT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_PLOT_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/** @brief Covered axis sections retain forty per cent opacity over the opaque surface. */
#define PLOT_AXIS_HIDDEN_OPACITY 0.4

/** @brief Bounded convex polygon capacity, including vertices introduced by clipping. */
enum { PLOT_OCCLUSION_POINTS = 24 };

/** @brief Mutable, independently owned triangle index and reusable visibility work space. */
typedef struct _plot_occlusion_t plot_occlusion_t;

/** @brief Convex projected polygon with an affine nearness value at every vertex. */
typedef struct {
    double point[PLOT_OCCLUSION_POINTS][3]; /**< Screen x, screen y and nearness; valid entries precede count. */
    unsigned count; /**< Number of vertices, from zero through PLOT_OCCLUSION_POINTS. */
} plot_occlusion_polygon_t;

/**
 * @brief Consume one convex visible fragment during a visibility query.
 * @param[in] polygon Borrowed fragment, valid only during the call; copy it if it must be retained.
 * @param context Caller-owned state passed unchanged to every callback.
 * @return True to continue; false to abort the query. No ownership is transferred.
 */
typedef bool (*plot_occlusion_emit_t)(const plot_occlusion_polygon_t *polygon, void *context);

/**
 * @brief Allocate an empty index, with triangle storage grown only as needed.
 * @return Owned index, or NULL on allocation failure. Release with plot_occlusion_free().
 * Independent indices may be used concurrently; one index is not re-entrant.
 */
plot_occlusion_t *plot_occlusion_new(void);

/**
 * @brief Copy an opaque triangle into an unbuilt index.
 * @param index Borrowed mutable index, not yet passed to plot_occlusion_build().
 * @param[in] points Required three-by-three finite screen x, screen y and nearness values.
 * @return True when copied; false for invalid arguments, a built index or allocation failure.
 * No pointers are retained; failure leaves all previously added triangles usable.
 */
bool plot_occlusion_add(plot_occlusion_t *index, const double points[3][3]);

/**
 * @brief Seal the triangle set and construct a bounding-volume hierarchy.
 * @param index Borrowed mutable index, which owns all allocated nodes.
 * @return True on success, including an empty or already built index; false on allocation failure or NULL.
 * Queries are permitted only after success. No caller data is retained.
 */
bool plot_occlusion_build(plot_occlusion_t *index);

/**
 * @brief Emit the parts of a convex face lying in front of every indexed triangle.
 * @param index Borrowed built index; its bounded temporary buffers are reused and may grow.
 * @param[in] polygon Borrowed convex polygon containing finite affine nearness coordinates.
 * @param emit Required synchronous callback receiving visible convex fragments.
 * @param context Caller-owned callback state, never inspected or retained.
 * @return True on completion, including complete occlusion; false on invalid input, callback failure,
 * allocation failure or more than 4096 intermediate fragments. Original geometry is never changed.
 * Queries on one index must be serial and callbacks must not recursively query that index.
 */
bool plot_occlusion_visible(plot_occlusion_t *index, const plot_occlusion_polygon_t *polygon,
                            plot_occlusion_emit_t emit, void *context);

/**
 * @brief Release an index, all copied triangles, hierarchy nodes and temporary buffers.
 * @param index Owned index, or NULL for no action. Do not use it after this call.
 * @return Nothing. No caller-owned geometry or callback state is released.
 */
void plot_occlusion_free(plot_occlusion_t *index);

#endif
