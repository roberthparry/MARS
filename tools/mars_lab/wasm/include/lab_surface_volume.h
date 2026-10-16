/**
 * @file lab_surface_volume.h
 * @brief Exact native range recipes and normalised browser zoom state.
 *
 * Native MARS evaluates opaque coordinate expressions at adaptive precision.
 * WebAssembly stores only a dimensionless binary zoom exponent and display-space
 * transforms; it never subtracts rounded absolute bounds for sampling.
 */
#ifndef MARS_LAB_SURFACE_VOLUME_H
#define MARS_LAB_SURFACE_VOLUME_H

/** @brief Initialise exact range metadata and preserve it through owned refinements. */
void lab_surface_volume_init(int view, int saved, unsigned same);

/** @brief Apply factor (0), slider exponent (1), wheel event (2), or pinch state (3); reset restores original controls. */
int lab_surface_zoom(int view, unsigned kind, double value, int event, unsigned reset);

/** @brief Update the zoom slider and its accessible readout from the binary exponent. */
void lab_surface_zoom_sync(int view);

/** @brief Borrow a scoped preview tick label, retaining native text for the sampled volume. */
int lab_surface_tick_label(int volume, int tick);

#endif
