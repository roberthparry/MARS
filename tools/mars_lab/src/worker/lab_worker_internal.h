/**
 * @file lab_worker_internal.h
 * @brief Calculation entry points linked into the MARS Lab executable.
 *
 * Private to the worker module and its dispatcher. Each entry point runs sequentially
 * in a bounded resident calculation process; callers must use lab_process.h for supervised execution.
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
#include "diffequation-t.h"
#include "sampler-t.h"
#include "plot-t.h"

/** @brief Shared soft source-width budget for complete equation captions and solution rows. */
enum {
    LAB_WORKER_EQUATION_LINE_LIMIT_LATEX = 280 /**< Keep compact equations together before native TeX wrapping. */
};

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

/**
 * @brief Parse authored bounds and retain a private, guard-precision equation.
 * @param source Borrowed original equation text, including its exact symbolic bindings.
 * @param choices Borrowed equation supplying plot-local values for originally unset constants, or NULL.
 * @param names Borrowed array of count coordinate names.
 * @param limits Borrowed array of twice count lower and upper bound strings; empty strings request defaults.
 * @param count Number of coordinates, from one to three.
 * @param[out] bounds Array of twice count algorithm bounds, using unit coordinates when extra precision is needed.
 * @return Owned sampling frame, released with lab_worker_implicit_free(), or NULL on invalid input or allocation failure.
 * The inputs are not changed or retained. The caller's numerical precision is restored before returning.
 */
lab_worker_implicit_t *lab_worker_implicit_new(const char *source, const equation_t *choices, const char *const *names,
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

/**
 * @brief Render a bounded real zero contour with a caption specialised to its fixed bindings.
 * @param equation Borrowed private worker equation; worksheet bindings remain unchanged.
 * @param first Borrowed horizontal coordinate node.
 * @param second Borrowed vertical coordinate node, distinct from first.
 * @param argv Borrowed validated worker arguments containing source, bounds, camera and quality options.
 * @param title_text Borrowed native plain equation caption with non-coordinate values substituted.
 * @param title_TeX Borrowed native TeX caption for the same specialised equation.
 * @return Zero after emitting the plot protocol, or one after emitting a sampling or range diagnostic.
 * No input ownership transfers. Run only in the isolated calculation worker.
 */
int lab_worker_equation_contour(equation_t *equation, expr_t *first, expr_t *second, char **argv,
                                const char *title_text, const char *title_TeX);

/**
 * @brief Render a bounded isosurface with a caption specialised to its fixed bindings.
 * @param equation Borrowed private worker equation; worksheet bindings remain unchanged.
 * @param first Borrowed first coordinate node.
 * @param second Borrowed distinct second coordinate node.
 * @param third Borrowed distinct third coordinate node.
 * @param argv Borrowed validated worker arguments containing source, bounds, camera and quality options.
 * @param title_text Borrowed native plain equation caption with non-coordinate values substituted.
 * @param title_TeX Borrowed native TeX caption for the same specialised equation.
 * @return Zero after emitting the plot protocol, or one after emitting a sampling or range diagnostic.
 * No input ownership transfers. Run only in the isolated calculation worker.
 */
int lab_worker_equation_mesh(equation_t *equation, expr_t *first, expr_t *second, expr_t *third, char **argv,
                             const char *title_text, const char *title_TeX);

/**
 * @brief Format an equation plot after substituting finite bindings outside its plotting coordinates.
 * @param equation Borrowed private worker equation, never modified.
 * @param first Borrowed first plotting coordinate, retained symbolically even when supplied.
 * @param second Optional borrowed second plotting coordinate, or NULL for a curve.
 * @param third Optional borrowed third plotting coordinate, or NULL for a curve or contour.
 * @param[out] plain Required destination for an owned unbound equation caption, released with string_free().
 * @param[out] TeX Required destination for an owned wrapped TeX caption, released with free().
 * @return True with both captions on success; false with both outputs NULL on invalid input or failure.
 * Exact binding expressions are preserved and simplified by the expression library. Bindings and their
 * variable or constant roles remain unchanged. Run only in the isolated calculation worker.
 */
bool lab_worker_equation_plot_titles(const equation_t *equation, const expr_t *first, const expr_t *second,
                                      const expr_t *third, string_t **plain, char **TeX);

/** @brief Plot a one-variable integrand, shading the supplied definite integration interval. */
int lab_worker_integrand_plot(int argc, char **argv);

/**
 * @brief Solve an equation once, retaining formatted numerical results and finite complex roots for plotting.
 * @param[in] equation Borrowed equation, valid throughout this isolated-worker call; bindings remain unchanged.
 * @param precision Decimal precision for the unchanged numerical-result text.
 * @param[out] roots Required destination for an owned numerical array, or NULL when no finite non-real roots exist.
 * @param[out] root_count Required destination for the number of populated roots, at most 256.
 * @param[out] families Required zero-initialised sixteen-entry array receiving owned affine integer-family graphs.
 * @param[out] parameters Required zero-initialised sixteen-entry array receiving owned integer-parameter names.
 * @param[out] family_count Required output receiving the populated family count, or zero for ordinary plots.
 * @return Owned labelled result text, released with string_free(), or NULL on allocation failure. Destroy each
 * returned root with num_destroy() and release its array with free(). Release each returned family with expr_free()
 * and its parameter name with string_free(). No input ownership transfers.
 * This operation performs a solve and must not be used when a completed replay record is available.
 */
string_t *lab_worker_equation_plot_solutions(const equation_t *equation, int precision,
                                             number_t **roots, size_t *root_count, expr_t **families,
                                             string_t **parameters, size_t *family_count);

/**
 * @brief Read finite complex coefficients of a structurally affine integer family without sampling its index.
 * @param[in] family Borrowed native graph; neither its variable values nor its ownership are changed.
 * @param[in] parameter Borrowed name of the family's integer variable.
 * @param[in,out] origin Required initialised number receiving the owned constant coefficient on success.
 * @param[in,out] step Required initialised number receiving the owned non-zero linear coefficient on success.
 * @return True for a finite affine family; false for unresolved coefficients, non-affine graphs or failure.
 * Destroy output values with num_destroy(). No pointers are retained; use only inside an isolated worker.
 */
bool lab_worker_integer_family(const expr_t *family, const string_t *parameter, number_t *origin, number_t *step);

/**
 * @brief Draw the viewport's integer members of up to sixteen affine families as discrete complex-plane points.
 * @param[in] families Borrowed array of native affine expressions; unchanged and retained only for the call.
 * @param[in] parameters Borrowed array of integer-variable names, one for each family.
 * @param count Number of families, from one to sixteen.
 * @param[in] axis Borrowed equation-variable name for real and imaginary axis labels.
 * @param[in] title Borrowed plain-text equation heading, or NULL for an empty heading.
 * @param[in] TeX Borrowed native mathematical heading, or NULL to omit it.
 * @param[in] argv Borrowed validated solver-plot arguments specifying the real and imaginary viewport limits.
 * @return Zero after emitting SVG and a caption, or one for invalid bounds, unsupported families or failure.
 * The integer interval is obtained directly from both coordinate limits; no unbounded search is performed.
 * Views above 256 points show labelled representative integer members with a zoom-in notice. Numerical precision
 * limits integer indices to ±4503599627370495; a caption identifies a reached limit without claiming an empty set.
 * Precision is restored before return. Worksheet formulae and values remain unchanged; all work is native to the worker.
 */
int lab_worker_plot_integer_families(const expr_t *const *families, string_t *const *parameters, size_t count,
                                     const char *axis, const char *title, const char *TeX, char **argv);

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

/**
 * @brief Select native plot-local parameter values and emit exact slider metadata.
 * @param[in] source Borrowed expression and binding envelope; not retained or changed.
 * @param[in] first First plotting coordinate, or NULL for a parameter-domain predicate.
 * @param[in] second Second plotting coordinate, or NULL when absent.
 * @param[in] third Third plotting coordinate, or NULL when absent.
 * @param[in] initial Borrowed finite real starting value for automatic choices; not retained or changed.
 * @param[in] request Borrowed tab-separated slider records, or NULL for automatic choices.
 * @param[out] bindings_out Optional destination for owned selected bindings, released with expr_bindings_free().
 * @return Owned source released with free(), or NULL for invalid values, restrictions or allocation failure.
 * @note Run serially in the calculation worker. Worksheet bindings remain untouched.
 */
char *lab_worker_parameter_slices(const char *source, const char *first, const char *second, const char *third,
                                  number_t initial, const char *request, expr_bindings_t **bindings_out);

/**
 * @brief Assemble a differential problem's complete editable parameter envelope.
 * @param[in] de Borrowed native problem, inspected without mutation.
 * @param[in] authored Borrowed original input, preserving derivative and condition spelling; NULL uses native text.
 * @return Owned native source released with string_free(), or NULL on formatting or allocation failure.
 */
string_t *lab_worker_de_source(const diffequ_t *de, const char *authored);

/**
 * @brief Choose an automatic scalar time plot's lower endpoint from its initial data.
 * @param[in] de Borrowed differential problem, inspected without modification.
 * @param[in] axis Borrowed coordinate name; only the scalar time coordinate t uses this policy.
 * @return Owned native numerical text for the lesser of zero and all finite real condition points, released
 * with string_free(); NULL for other coordinates, invalid inputs or allocation failure.
 * @note Explicit user bounds and authored interval bounds take precedence at the caller. Use in an isolated worker.
 */
string_t *lab_worker_de_time_lower(const diffequ_t *de, const char *axis);

/**
 * @brief Apply illustrative slider choices to an isolated differential problem and emit plot metadata.
 * @param[in,out] de Worker-owned problem; only its previously unset constants receive values.
 * @param[in] request Borrowed slider records, or NULL for native automatic choices.
 * @return True on success or when no constants need values; false on invalid choices or allocation failure.
 * @note No worksheet source or supplied constant value is changed. Use before solving for the plotted curves.
 */
bool lab_worker_de_plot_parameters(diffequ_t *de, const char *request);

/**
 * @brief Select the conventional horizontal coordinate for a two-variable equation.
 * @param[in] bindings Borrowed equation bindings; constants are excluded and nothing is modified.
 * @return Binding index for t, then x, then the first variable; the binding count when there are not two variables.
 * @note The returned index is valid only while the binding catalogue remains unchanged. No ownership transfers.
 */
size_t lab_worker_equation_horizontal(const expr_bindings_t *bindings);

/**
 * @brief Emit an equation's original coordinate and constant bindings to the native response protocol.
 * @param[in] equation Borrowed parsed worksheet; no binding is changed or retained.
 * @return No value. Protocol rows are written to the calculation worker's standard output.
 * @note Call before selecting plot-local parameters, so temporary values never replace worksheet bindings.
 */
void lab_worker_equation_bindings(const equation_t *equation);

/**
 * @brief Draw representative real numerical trajectories when an ODE has no explicit symbolic coordinate function.
 * @param[in] de Borrowed scalar problem with plot-local numerical coefficients already selected; not modified.
 * @param[in] argv Borrowed validated solver-plot arguments, including finite optional coordinate and height bounds.
 * @return Zero after emitting SVG and native title fields, or one with an explanatory error field on failure.
 * The library owns all numerical integration; this adapter releases its samples and plot before returning.
 * This isolated-worker operation must not run concurrently against mutable problem bindings.
 */
int lab_worker_de_trajectory_plot(const diffequ_t *de, char **argv);

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

/**
 * @brief Try four linked projections using plot-local parameter values and the unchanged worksheet formula.
 * @param source Borrowed parseable sampling source, including plot-only slice values.
 * @param expression Borrowed numeric graph with those slice values applied.
 * @param variable Borrowed real input coordinate belonging to expression.
 * @param lower Borrowed input lower bound; empty selects an automatic bound.
 * @param upper Borrowed input upper bound; empty selects an automatic bound.
 * @param scale Borrowed component/amplitude scale policy: auto, all or manual.
 * @param y_lower Borrowed manual vertical lower bound, otherwise ignored.
 * @param y_upper Borrowed manual vertical upper bound, otherwise ignored.
 * @param plane_bounds Four borrowed real/imaginary bound strings, or NULL for automatic Argand bounds.
 * @param preview Select the bounded coarse sampling budget.
 * @param force_complex Retain the complex views during targeted refinement, including wholly real subsets.
 * @param display_expression Borrowed original symbolic graph, whose bindings are retained in output.
 * @return Zero after emitting all views, one on failure, or minus one when real-only plotting should be tried.
 * No arguments are consumed or retained. Call only in an isolated native calculation worker.
 */
int lab_worker_complex_plot(const char *source, const expr_t *expression, const expr_t *variable,
                             const char *lower, const char *upper, const char *scale,
                             const char *y_lower, const char *y_upper, const char *const plane_bounds[4], bool preview,
                             bool force_complex, const expr_t *display_expression);

/**
 * @brief Emit a real or complex curve without binding the displayed worksheet to its slice values.
 * @param source Borrowed sampling source including plot-local parameter values.
 * @param expression Borrowed numeric expression graph used for classification and complex sampling.
 * @param variable Borrowed plotting coordinate belonging to expression.
 * @param lower Borrowed input lower bound; empty selects an automatic bound.
 * @param upper Borrowed input upper bound; empty selects an automatic bound.
 * @param scale Borrowed vertical scale policy: auto, all or manual.
 * @param y_lower Borrowed manual vertical lower bound, otherwise ignored.
 * @param y_upper Borrowed manual vertical upper bound, otherwise ignored.
 * @param plane_bounds Four borrowed complex-plane bounds, or NULL for automatic bounds.
 * @param preview Select bounded coarse sampling suitable for an interactive gesture.
 * @param force_complex Preserve the four complex views during targeted refinement.
 * @param display_expression Borrowed original symbolic graph used for output, preserving its bindings.
 * @return Zero after emitting a plot, or one after a diagnostic on invalid input or sampling failure.
 * No arguments are consumed or retained. Worksheet precision is restored before returning.
 */
int lab_worker_plot(const char *source, const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper,
                    const char *const plane_bounds[4], bool preview, bool force_complex, const expr_t *display_expression);

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
