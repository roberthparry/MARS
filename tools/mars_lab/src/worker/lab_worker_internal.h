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

/**
 * @brief Solve an equation once, retaining formatted numerical results and finite complex roots for plotting.
 * @param[in] equation Borrowed equation, valid throughout this isolated-worker call; bindings remain unchanged.
 * @param precision Decimal precision for the unchanged numerical-result text.
 * @param[out] roots Required destination for an owned numerical array, or NULL when no finite non-real roots exist.
 * @param[out] root_count Required destination for the number of populated roots, at most 256.
 * @return Owned labelled result text, released with string_free(), or NULL on allocation failure. Destroy each
 * returned root with num_destroy() and release its array with free(). No input ownership transfers.
 * This operation performs a solve and must not be used when a completed replay record is available.
 */
string_t *lab_worker_equation_plot_solutions(const equation_t *equation, int precision,
                                             number_t **roots, size_t *root_count);

/**
 * @brief Render finite equation roots as unconnected points in their complex coordinate plane.
 * @param[in] roots Borrowed finite numerical values; at least one must be non-real.
 * @param count Number of values, from one to 256.
 * @param[in] equation Borrowed original equation, used to extend supported zero searches outside the retained roots.
 * @param[in] axis Borrowed variable name used in the real and imaginary axis labels.
 * @param[in] title Borrowed plain-text authored equation title.
 * @param[in] TeX Borrowed native TeX equation title, or NULL to omit it.
 * @param[in] argv Borrowed validated solver-plot arguments; horizontal and manual vertical limits define the viewport.
 * @return Zero after emitting a native SVG and explanatory caption, or one on invalid bounds, invalid roots or
 * allocation failure. Inputs remain caller-owned, are not retained, and numerical precision is restored on return.
 * Retained roots are never solved again. Newly exposed bands of supported zeta equations use display-precision
 * searches; worksheet solutions are unchanged. Use only within an isolated calculation process.
 */
int lab_worker_plot_roots(const number_t *roots, size_t count, const equation_t *equation,
                         const char *axis, const char *title, const char *TeX, char **argv);

/**
 * @brief Emit one bounded, resumable page of verified zeta-zero conjugate pairs at worksheet precision.
 * @param argc Number of validated arguments, at least five: worker, source, precision, flag and height cursor.
 * @param argv Borrowed NUL-terminated argument vector; no pointers are retained or modified.
 * @return Zero on success, one on failure, or two when an initial equation is unsupported and ordinary solving
 * should be used. The calculation owns and releases its equation, roots and notation. Run in an isolated worker.
 */
int lab_worker_equation_pages(int argc, char **argv);

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

/**
 * @brief Fit an adaptive curve's requested depth within its complete binary-subdivision budget.
 * @param intervals Number of initial intervals; must be positive.
 * @param max_evaluations Total callback allowance, including endpoints; allow at least 2*intervals+1.
 * @param preferred Requested maximum depth; the result never exceeds it.
 * @return Largest permitted depth, or zero when even the first midpoint level cannot fit or inputs are invalid.
 * No storage is allocated or borrowed. This isolated-worker policy preserves the sampler's strict budget semantics.
 */
unsigned lab_worker_plot_depth(size_t intervals, size_t max_evaluations, unsigned preferred);

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
 * @brief Decide whether a numerical error estimate is significant at the requested display accuracy.
 * @param value Borrowed real or complex result; ownership and numerical value are unchanged.
 * @param error Borrowed absolute error estimate; negative, non-real or non-finite estimates remain visible.
 * @param precision Requested decimal digits, or non-positive to use the current native number precision.
 * @return False only for a finite non-negative estimate strictly below max(1, |value|) times 10^-precision.
 * True for equality, a larger estimate, an unknown result, or arithmetic failure. No handles are retained.
 */
bool lab_worker_error_visible(number_t value, number_t error, int precision);

/**
 * @brief Format a significant numerical error estimate to three significant decimal digits for result cards.
 * @param value Borrowed result used to determine whether the full estimate exceeds the display cutoff.
 * @param error Borrowed absolute error estimate; its stored value and precision are unchanged.
 * @param precision Requested decimal digits, or non-positive to use the current native number precision.
 * @return Caller-owned text, released with string_free, or NULL on allocation failure. The text is empty
 * when lab_worker_error_visible returns false. Otherwise finite estimates are rounded to three significant
 * digits, using Unicode powers of ten where appropriate; invalid or unknown estimates remain visible.
 * No numeric handles are retained, and the current native number precision is unchanged.
 */
string_t *lab_worker_error_text(number_t value, number_t error, int precision);

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
