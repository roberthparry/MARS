/**
 * @file plot_internal.h
 * @brief Controlled access to display-only plot-axis geometry and projective visibility.
 *
 * Retained WebAssembly rendering uses the native cube-frame and positive-arrow plans without
 * importing scene storage, mathematical values or the full plot implementation.
 * A shared triangle hierarchy clips visible axis fragments for software and SVG output.
 * Ordinary scene consumers use the public plot-t.h API.
 */
#ifndef MARS_SHARED_PLOT_INTERNAL_H
#define MARS_SHARED_PLOT_INTERNAL_H
#if !defined(MARS_SHARED_PLOT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Controlled header requires MARS_SHARED_PLOT_INTERNAL_ACCESS."
#endif

#define MARS_PLOT_INTERNAL_ACCESS
#include "../plot/plot_axis_frame.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "../plot/plot_projection.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "../plot/plot_occlusion.h"

#endif
