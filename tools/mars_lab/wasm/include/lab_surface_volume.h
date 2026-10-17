/**
 * @file lab_surface_volume.h
 * @brief Exact native range recipes and normalised browser zoom state.
 *
 * Native MARS evaluates opaque coordinate expressions at adaptive precision.
 * WebAssembly stores only a dimensionless binary zoom exponent and display-space
 * transforms; it never subtracts rounded absolute bounds for sampling.
 * Explicit height grids in Auto retain their sampled height during horizontal
 * zoom and adopt the new native height on refinement. Space curves zoom only the
 * input interval, preserving output-axis controls and adopting fresh output ranges.
 * Other policies and implicit meshes retain strict three-axis volume zoom.
 */
#ifndef MARS_LAB_SURFACE_VOLUME_H
#define MARS_LAB_SURFACE_VOLUME_H

/**
 * @brief Initialise exact range metadata and preserve its baseline through owned refinements.
 * Segment refinements replace sampled bounds and units, not camera orientation or
 * the baseline exponent recipe. The segment baseline comes only from its chart,
 * never shared controls that a sibling plot may have changed; reset retains it.
 */
void lab_surface_volume_init(int view, int saved, unsigned same);

/**
 * @brief Apply factor (0), slider exponent (1), wheel event (2), or pinch state (3).
 * Marks the volume dirty for resampling. Segments modify only input-axis controls;
 * reset restores their original chart interval explicitly without changing output-axis
 * controls. Selecting Auto input range is a separate request for native range selection.
 * Pinch cancellation restores the starting volume, camera scale and control metadata,
 * including the original dirty state; invalid pinch distances do not mutate native state.
 */
int lab_surface_zoom(int view, unsigned kind, double value, int event, unsigned reset);

/** @brief Update the zoom slider and its accessible readout from the binary exponent. */
void lab_surface_zoom_sync(int view);

/** @brief Borrow a scoped preview tick label, retaining native text for the sampled volume. */
int lab_surface_tick_label(int volume, int tick);

#endif
