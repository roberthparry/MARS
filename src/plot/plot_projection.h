/**
 * @file plot_projection.h
 * @brief Private pinhole projection and solid axis geometry for native and retained 3D plots.
 *
 * All geometry uses bounded display coordinates, after native mathematical ranges
 * have been normalised. SVG, WebGL and Canvas share the same eye, projection,
 * cylinder/cone meshes, depth-based label scale and projected tick spacing. Scene callers use plot-t.h;
 * retained rendering accesses this interface through src/internal/plot_internal.h.
 */
#ifndef MARS_PLOT_PROJECTION_H
#define MARS_PLOT_PROJECTION_H
#if !defined(MARS_PLOT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_PLOT_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_axis_frame.h"

/** @brief Display camera; positive camera depth runs away from the viewer. */
typedef struct _plot_projection_t {
    double rows[9]; /**< Row-major orthogonal camera basis with physical coordinate-unit scales in its columns. */
    double fit; /**< Image-plane scale at the coordinate-volume centre, in pixels per normalised world unit. */
    double centre[2]; /**< Principal point in SVG pixels. */
    double distance; /**< Eye distance from the volume centre; zero selects an orthographic diagnostic camera. */
    double radius; /**< Enclosing-sphere radius of the coordinate volume, in normalised world units. */
} plot_projection_t;

/**
 * @brief Plan label normals for three coordinate axes passing through a projected mathematical origin.
 * @param projection Borrowed finite camera, neither changed nor retained.
 * @param origin Required normalised position of zero in each coordinate interval, without clamping.
 * @param[out] frame Required output receiving perpendicular label normals and zeroed, unused corner indices.
 * @return True on success; false for invalid pointers or non-finite origin coordinates, leaving output unchanged.
 * No allocation or ownership transfer occurs. Axis endpoint order stays increasing in each coordinate.
 * Independent calls are safe concurrently. Scene callers calculate origin fractions at native precision.
 */
bool plot_projection_origin_frame(const plot_projection_t *projection, const double origin[3], plot_axis_frame_t *frame);

/** @brief Fixed numerical-scale and solid-axis limits, independent of sampled surface size. */
enum {
    PLOT_AXIS_MAX_TICKS = 12, /**< Maximum catalogue size of one numerical coordinate scale. */
    PLOT_AXIS_SIDES = 16, /**< Polygonal approximation of a circular cylinder or cone cross-section. */
    PLOT_AXIS_FACES = PLOT_AXIS_SIDES * 2 + 2 /**< Cylinder sides, two caps and cone sides for one axis. */
};

/** @brief One opaque, shaded, projected face of an axis cylinder or cone. */
typedef struct _plot_axis_face_t {
    double point[PLOT_AXIS_SIDES][2]; /**< Projected polygon vertices; only count entries are initialised. */
    double depths[PLOT_AXIS_SIDES]; /**< Camera depths at the matching vertices, for projective visibility. */
    unsigned count; /**< Number of vertices, between three and PLOT_AXIS_SIDES. */
    unsigned axis; /**< Physical coordinate-axis index, zero through two. */
    bool cone; /**< True for the cone and its base; false for the cylinder and its lower cap. */
    double depth; /**< Mean camera depth for bounded far-to-near painter ordering. */
    double shade; /**< Ambient-plus-diffuse illumination multiplier in the closed interval [0, 1]. */
} plot_axis_face_t;

/** @brief Fixed-size solid axis, with a positive-direction arrow and depth-scaled endpoint sizes. */
typedef struct _plot_axis_solid_t {
    plot_axis_face_t face[PLOT_AXIS_FACES]; /**< Cylinder and cone faces; valid entries precede count. */
    unsigned count; /**< Number of valid faces; zero for a collapsed projected axis. */
    double start[2]; /**< Projected cylinder start, including any extension below the numerical interval. */
    plot_axis_arrow_t arrow; /**< Projected centreline direction, cone tip, silhouette wings and name anchor. */
    double name_scale; /**< Pinhole size multiplier at the upper axis endpoint labelled by the annotation. */
} plot_axis_solid_t;

/**
 * @brief Construct a fitted pinhole camera from the three physical display-coordinate columns.
 * @param[in] rows Required nine-number row-major rotated and scaled basis.
 * @param width Positive SVG viewport width in pixels.
 * @param height Positive SVG viewport height in pixels.
 * @param label_width Non-negative conservative numeric label width at the minimum readable font size.
 * @param sphere True for a rotation-stable enclosing-sphere fit; false to fit the projected coordinate box.
 * @param[out] projection Required output receiving the camera on success; left unchanged on failure.
 * @return True for a finite non-degenerate camera; false for invalid dimensions, basis or output.
 * @details The eye is four enclosing radii from the centre. Extra space above the fitted volume separates
 * arrowheads and axis names from the title, reducing proportionally in short viewports.
 * No allocations or retained state; thread-safe.
 */
bool plot_projection_init(const double rows[9], double width, double height, double label_width,
                          bool sphere, plot_projection_t *projection);

/**
 * @brief Reduce the camera scale only as needed to keep solid axes and their names inside the viewport.
 * @param[in,out] projection Required fitted camera, updated on success and unchanged on failure.
 * @param width Finite viewport width in pixels, greater than eighty.
 * @param height Finite viewport height in pixels, greater than one hundred and twenty.
 * @param frame Borrowed selected exterior edges or origin-axis label normals; never changed or retained.
 * @param origin Optional three-number normalised mathematical origin; NULL selects exterior frame edges.
 * Axes outside the current coordinate volume do not change its framing.
 * @param name_widths Required three-number conservative name widths at the base fourteen-pixel font size.
 * @return True when the axes fit with sixteen pixels of edge clearance, false for invalid geometry or
 * names too large to fit within eight bounded adjustments. Title clearance is retained above the volume.
 * No allocation, mathematical evaluation or ownership transfer occurs. Independent cameras are thread-safe.
 */
bool plot_projection_fit_axes(plot_projection_t *projection, double width, double height,
                              const plot_axis_frame_t *frame, const double origin[3], const double name_widths[3]);

/**
 * @brief Transform a normalised coordinate-volume point into camera coordinates.
 * @param[in] projection Borrowed camera with orthogonal scaled columns.
 * @param[in] point Required three-number unit-volume point; [0.5, 0.5, 0.5] is the volume centre.
 * @param[out] camera Required three-number camera-space output; may alias point.
 * @return Nothing. Invalid inputs propagate non-finite values; no allocation or retained state.
 */
void plot_projection_camera(const plot_projection_t *projection, const double point[3], double camera[3]);

/**
 * @brief Project a camera-space point using the eye distance and principal point.
 * @param[in] projection Borrowed camera; distance zero retains orthographic diagnostic projection.
 * @param[in] camera Required camera-space point, with positive depth away from the eye.
 * @param[out] screen Required [x, y, depth] output, which may alias camera; points behind the eye yield NaN x/y.
 * @return Nothing. Does not clip, allocate or retain any input.
 */
void plot_projection_point(const plot_projection_t *projection, const double camera[3], double screen[3]);

/**
 * @brief Calculate a readable reference size with the same pinhole distance law as projected geometry.
 * @param[in] projection Borrowed display camera.
 * @param depth Camera depth, increasing away from the viewer.
 * @return (distance + radius)/(distance + depth), or one for an orthographic camera; NaN behind the eye.
 * The far enclosing-sphere plane has unit scale; there is no per-label clamping or linear depth heuristic.
 */
double plot_projection_size(const plot_projection_t *projection, double depth);

/**
 * @brief Place an upright numeric label close to its outward tick endpoint.
 * @param[in] tip Required two-number projected tick endpoint in drawing pixels.
 * @param[in] normal Required two-number unit vector pointing away from the coordinate volume.
 * @param font_size Positive perspective-scaled label height in drawing pixels.
 * @param[out] out Required two-number text anchor and baseline; may alias tip.
 * @return Minus one for end alignment, zero for centred alignment, or one for start alignment.
 * @details Reserves 1.5 pixels at the thirteen-pixel reference font and accounts for ascent and descent.
 * Callers perform collision and viewport checks. Invalid inputs propagate non-finite coordinates;
 * no allocation, retained state or ownership transfer occurs. Safe for concurrent calls with separate outputs.
 */
int plot_axis_tick_label_position(const double tip[2], const double normal[2], double font_size, double out[2]);

/**
 * @brief Select readable tick strokes from their current projected positions and perspective sizes.
 * @param[in] positions Borrowed count-by-two array of finite screen positions in coordinate order.
 * @param[in] scales Borrowed count-element array of finite positive perspective size multipliers.
 * @param count Number of catalogue entries, from one through PLOT_AXIS_MAX_TICKS.
 * @param zero Index of the zero tick to prioritise, or count when zero is absent.
 * @return Bit mask of strokes separated by at least four times the sum of their size multipliers;
 * zero for invalid inputs. Prioritises zero, then the outer ticks, then remaining entries.
 * @details Only drawing is thinned: callers retain the complete numerical catalogue for later views.
 * At most twelve positions are considered; no allocation, ownership transfer or retained state occurs.
 * Safe for concurrent calls with separate input arrays.
 */
unsigned plot_axis_tick_mask(const double positions[][2], const double scales[], unsigned count, unsigned zero);

/**
 * @brief Intersect the eye ray through a screen point with the normalised coordinate box.
 * @param[in] projection Borrowed camera with orthogonal scaled columns.
 * @param x Finite SVG horizontal coordinate.
 * @param y Finite SVG vertical coordinate.
 * @return True inside or on the projected volume outline; false for invalid inputs or a missed box.
 * @details Uses three slab intersections, not sampled geometry or a screen bounding rectangle. Thread-safe.
 */
bool plot_projection_contains(const plot_projection_t *projection, double x, double y);

/**
 * @brief Build a circular cylinder with a cone pointing beyond the axis's upper coordinate endpoint.
 * @param[in] projection Borrowed fitted camera.
 * @param[in] lower Required normalised unit-volume lower coordinate endpoint.
 * @param[in] upper Required normalised upper coordinate endpoint; determines increasing direction.
 * @param axis Physical axis index, zero through two, retained in every face.
 * @param extend Extend the shaft by eight per cent of the coordinate interval at each end before adding the cone.
 * @param[out] solid Required output receiving a bounded shaded mesh and projected arrow on success.
 * @return True on success; false for invalid, collapsed or eye-crossing geometry, leaving output unchanged.
 * @details Uses constant physical radii, a camera-fixed light and sixteen-sided circular sections.
 * All taper, tick spacing and label magnification follow the same perspective denominator.
 * No allocations, expression evaluation or retained state; thread-safe with separate outputs.
 */
bool plot_axis_solid(const plot_projection_t *projection, const double lower[3], const double upper[3],
                     unsigned axis, bool extend, plot_axis_solid_t *solid);

/**
 * @brief Compare projected solid faces for far-to-near painter ordering.
 * @param[in] first Borrowed plot_axis_face_t pointer, suitable for qsort.
 * @param[in] second Borrowed plot_axis_face_t pointer, suitable for qsort.
 * @return Negative when first is farther away, positive when nearer, or zero at equal depth.
 * No allocation or state; unrelated sampled surface facets are not accepted.
 */
int plot_axis_face_order(const void *first, const void *second);

#endif
