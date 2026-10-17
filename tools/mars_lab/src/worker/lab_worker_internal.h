/**
 * @file lab_worker_internal.h
 * @brief Calculation entry points linked into the MARS Lab executable.
 *
 * Private to the worker module and its dispatcher. Each entry point runs once
 * in a fresh child process; callers must use lab_process.h for bounded execution.
 * These routines may initialise process-wide mathematical state and are not
 * re-entrant services for the listener or request processes.
 */
#ifndef MARS_LAB_WORKER_INTERNAL_H
#define MARS_LAB_WORKER_INTERNAL_H

#if !defined(MARS_LAB_WORKER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_WORKER_INTERNAL_ACCESS."
#endif

#include "number-t.h"
#include "string-t.h"
#include "expression-t.h"
#include "equation-t.h"
#include "sampler-t.h"
#include "plot-t.h"

/** @brief Optional guarded double evaluator over a private plotting graph. */
typedef struct _lab_worker_sample_t lab_worker_sample_t;

/** @brief Borrow a graph and up to three axes; enable doubles only at ordinary working precision. */
lab_worker_sample_t *lab_worker_sample_new(expr_t *expression, expr_t *const *axes, size_t count, size_t bits);

/** @brief Evaluate precise coordinates, falling back to the native graph when the approximation is uncertain. */
number_t lab_worker_sample_eval(lab_worker_sample_t *sample, const number_t *coordinates);

/** @brief Release the evaluator without releasing its borrowed graph or axes. */
void lab_worker_sample_free(lab_worker_sample_t *sample);

/** @brief Convert a coordinate to doubles at 53 bits, retaining native precision for escalated passes. */
number_t lab_worker_sample_coordinate(number_t value, size_t bits);

/** @brief Precise coordinate frame and guarded evaluator for an implicit equation. */
typedef struct _lab_worker_implicit_t lab_worker_implicit_t;

/** @brief Parse authored bounds before rounding and retain a private, guard-precision equation. */
lab_worker_implicit_t *lab_worker_implicit_new(const char *source, const char *const *names,
                                              const char *const *limits, size_t count, double *bounds);

/** @brief Evaluate world coordinates, or local unit coordinates when the frame requires extra precision. */
double lab_worker_implicit_eval(lab_worker_implicit_t *sample, const double *point);

/** @brief Return independent periodic phase families, or one ordinary residual field. */
unsigned lab_worker_implicit_layers(const lab_worker_implicit_t *sample);

/** @brief Select a mesh field and return its level spacing; zero denotes the original residual. */
double lab_worker_implicit_layer(lab_worker_implicit_t *sample, unsigned layer);

/** @brief Set the current contour level within the selected mesh field. */
void lab_worker_implicit_level(lab_worker_implicit_t *sample, double level);

/** @brief Convert an algorithm coordinate back to an owning, precision-preserving world coordinate. */
number_t lab_worker_implicit_coordinate(const lab_worker_implicit_t *sample, size_t axis, double value);

/** @brief Return the precision required to render the frame without collapsing its coordinates. */
size_t lab_worker_implicit_precision(const lab_worker_implicit_t *sample);

/** @brief Release the private equation and its exact coordinate frame. */
void lab_worker_implicit_free(lab_worker_implicit_t *sample);

/** @brief Plot equation sides, two-coordinate contours or explicit differential-equation curves and surfaces. */
int lab_worker_solver_plot(int argc, char **argv);

/** @brief Render a bounded real zero contour for an equation with two selected coordinates. */
int lab_worker_equation_contour(equation_t *equation, expr_t *first, expr_t *second, char **argv);

/** @brief Render a bounded isosurface for three free real equation coordinates. */
int lab_worker_equation_mesh(equation_t *equation, expr_t *first, expr_t *second, expr_t *third, char **argv);

/** @brief Plot a one-variable integrand, shading the supplied definite integration interval. */
int lab_worker_integrand_plot(int argc, char **argv);

/** @brief Return owning worksheet-precision solution records, using the normal equation solution formatter. */
string_t *lab_worker_equation_plot_solutions(const equation_t *equation, int precision);

/** @brief Build up to sixteen labelled representative ODE curves, varying only generated integration constants. */
bool lab_worker_plot_family(expr_bindings_t *authored, const char *axis, const expr_t *const *branches,
                             size_t branch_count, expr_t **curves, string_t **labels, size_t *count, bool *family);

/** @brief Plot a native solution surface with two declared axes and a named dependent variable. */
int lab_worker_plot_surface(const char *source, const expr_t *expression, const expr_t *first,
                            const expr_t *second, const expr_t *function, int argc, char **argv);

/** @brief Refine automatic family bounds using binned sampler activity and located stationary points. */
void lab_worker_plot_frame(char *const *sources, size_t count, const char *axis, number_t bounds[4]);

/** @brief Frame native equation intersections and turning points; optionally refine automatic horizontal bounds. */
bool lab_worker_plot_equation_frame(const expr_t *const *sides, char *const *sources, const char *axis,
                                    bool horizontal, number_t bounds[4]);

/** @brief Refine bounded surface-extremum candidates with gradients and Hessians for automatic height limits. */
bool lab_worker_surface_extrema(plot_t *plot, const char *source, const char *first, const char *second,
                                const number_t *x, const number_t *y, const number_t *z, size_t side);

/** @brief Add bounded, conforming drawing facets where native height-grid interpolation is unresolved. */
bool lab_worker_surface_refine(plot_t *plot, const char *source, const char *first, const char *second,
                               const number_t *x, const number_t *y, const number_t *z, size_t side, size_t bits);

/** @brief Return owning plot-local bound source and emit exact slice metadata without changing the worksheet. */
char *lab_worker_surface_slice(const char *source, const char *first, const char *second, const char *request);

/** @brief Parse a plotting range with guard precision; return owning bounds without changing worksheet precision. */
bool lab_worker_plot_bounds(const char *lower, const char *upper, number_t *lo, number_t *hi);

/** @brief Choose omitted curve or surface bounds with bounded real-domain probes; preserve explicit endpoints. */
bool lab_worker_plot_ranges(const char *source, const char *first, const char *second,
                            const char *const limits[4], number_t bounds[4]);

/** @brief Determine guarded coordinate precision for a finite range; zero denotes invalid bounds. */
size_t lab_worker_plot_range_precision(number_t lower, number_t upper);

/** @brief Choose validation precision for cancellation on a tiny range. */
size_t lab_worker_plot_check_precision(number_t lower, number_t upper);

/** @brief Check the chart byte budget, reserving transport space for titles and other result fields. */
bool lab_worker_plot_fits(const string_t *svg);

/** @brief Record reduced sampling detail in SVG metadata without a visible chart annotation. */
bool lab_worker_plot_reduced(string_t *svg);

/** @brief Tag the sampled grid and visibly identify temporary gesture previews. */
bool lab_worker_plot_quality(string_t *svg, bool preview, size_t side);

/** @brief Emit plot records from a borrowed SVG; optional function and axis symbols name a surface title. */
int lab_worker_plot_output(const char *source, const expr_t *expression, const string_t *svg, int error,
                           const expr_t *function, const expr_t *first, const expr_t *second);

/** @brief Dispatch a curve or surface request using explicit axes or native unset-variable metadata. */
int lab_worker_plot_request(const char *source, const expr_t *expression, expr_bindings_t *bindings,
                            const expr_t *variable, int argc, char **argv);

/**
 * @brief Sample a privately reparsed formula at spot-checked precision, returned through precision_bits on success.
 * Bounds are borrowed; pass NUM_NAN for both vertical bounds when not using a manual viewport.
 * Returns an owning sampler, or NULL on failure. Worksheet precision is restored on every exit.
 */
sampler_t *lab_worker_plot_samples(const char *source, const char *variable, number_t lower, number_t upper,
                                  number_t ymin, number_t ymax, size_t *precision_bits);

/** @brief Sample with range-aware precision; previews reduce subdivision work and omit derivative construction. */
sampler_t *lab_worker_plot_samples_quality(const char *source, const char *variable, number_t lower, number_t upper,
                                          number_t ymin, number_t ymax, size_t *precision_bits, bool preview);

/** Try a native distribution plot; return -1 when this path is not applicable. */
int lab_worker_distribution_plot(const char *source, const expr_t *expression, const expr_t *variable,
                                 const char *lower, const char *upper, const char *scale,
                                 const char *y_lower, const char *y_upper, bool preview);

/** @brief Try four projections with independent component and Argand bounds; Auto returns -1 for real-only output.
 * @param plane_bounds Four borrowed real/imaginary bound strings, or NULL for automatic Argand bounds.
 * @param force_complex Retain the complex views during targeted refinement, including wholly real subsets.
 */
int lab_worker_complex_plot(const char *source, const expr_t *expression, const expr_t *variable,
                             const char *lower, const char *upper, const char *scale,
                             const char *y_lower, const char *y_upper, const char *const plane_bounds[4], bool preview,
                             bool force_complex);

/** @brief Emit native plots for one variable; force_complex preserves complex views during targeted refinement. */
int lab_worker_plot(const char *source, const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper,
                    const char *const plane_bounds[4], bool preview, bool force_complex);

/**
 * @brief Formats a numerical card value with decimal or Unicode power-of-ten notation.
 * @param value Borrowed value; nonzero imaginary components are retained.
 * @param precision Significant decimal digits, or non-positive to use the current default.
 * @return Caller-owned text, released with string_free, or NULL on allocation failure.
 */
string_t *lab_worker_number_text(number_t value, int precision);

/**
 * @brief Runs the built-in expression calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_expression(int argc, char **argv);

/**
 * @brief Runs the built-in equation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_equation(int argc, char **argv);

/**
 * @brief Runs the built-in diffequation calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_diffequation(int argc, char **argv);

/**
 * @brief Runs the built-in matrix calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_matrix(int argc, char **argv);

/**
 * @brief Runs the built-in integrator calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_integrator(int argc, char **argv);

/**
 * @brief Runs the built-in datetime calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_datetime(int argc, char **argv);

/**
 * @brief Runs the built-in almanac calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac(int argc, char **argv);

/**
 * @brief Runs the built-in almanac event calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_almanac_event(int argc, char **argv);

/**
 * @brief Runs the built-in holiday calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_holiday(int argc, char **argv);

/**
 * @brief Runs the built-in ophelia calculation.
 * @param argc Argument count including the mode name.
 * @param argv Borrowed argument vector with the mode name at index zero.
 * @return Conventional process exit status.
 */
int lab_worker_ophelia(int argc, char **argv);

#endif
