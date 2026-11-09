/**
 * @file diffequ_notation_internal.h
 * @brief Private differential-equation semantic preparation boundary.
 *
 * Native adapters share the equation projection through this header. TeX receives
 * only the resulting context callbacks; it does not include this interface.
 */
#ifndef MARS_DIFFEQU_NOTATION_INTERNAL_H
#define MARS_DIFFEQU_NOTATION_INTERNAL_H
#if !defined(MARS_DIFFEQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_DIFFEQUATION_INTERNAL_ACCESS."
#endif

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/**
 * @brief Label the two coordinates of a quadrature family as functions of their shared parameter.
 * @param[in,out] result Borrowed result receiving independent display strings on its first two equations.
 * @return True for a non-quadrature result or after successful formatting; false for missing coordinates,
 * allocation or rendering failure. Mathematical equation graphs and ownership remain unchanged. Call serially.
 */
bool de_emden_quadrature_display(diffequ_solve_result_t *result);

/**
 * @brief Present the exact translation/scaling reduction without introducing typesetting syntax into the solver.
 * @param[in,out] result Borrowed result receiving independent copies of plain and semantic explanations.
 * @param[in] independent Borrowed original independent coordinate; neither changed nor consumed.
 * @param[in] dependent Borrowed dependent coordinate; neither changed nor consumed.
 * @param[in] first Borrowed original first derivative node.
 * @param[in] a Borrowed normalised coefficient of the mixed dependent/velocity term.
 * @param[in] b Borrowed normalised cubic coefficient; independent of a.
 * @param[in] parameter Borrowed local curve parameter.
 * @param[in] reference Borrowed fixed regular reference; not an integration constant.
 * @param[in] polynomial Borrowed quadratic in the curve parameter.
 * @param[in] primitive Borrowed bounded primitive defining the integrating factor.
 * @return True after both explanations are installed; false on invalid input, allocation or formatting failure.
 * @note No input ownership transfers. Returned descriptions survive the inputs; call serially with result.
 */
bool de_emden_quadrature_steps(diffequ_solve_result_t *result, const expr_t *independent,
    const expr_t *dependent, const expr_t *first, const expr_t *a, const expr_t *b,
    const expr_t *parameter, const expr_t *reference, const expr_t *polynomial, const expr_t *primitive);

/**
 * @brief Describe a simplified Riccati reduction and its optional linear basis without typesetting syntax.
 * @param result Borrowed mutable result receiving independent copies of both explanations.
 * @param independent Borrowed differentiation coordinate; not retained.
 * @param dependent Borrowed original dependent variable; not retained.
 * @param quadratic Borrowed coefficient of the dependent variable squared; not retained.
 * @param shift Borrowed half coefficient of the lifted equation's negative first derivative; not retained.
 * @param potential Borrowed normal-form potential in the auxiliary linear equation; not retained.
 * @param first Borrowed first fundamental solution, or NULL when the linear equation remains unsolved.
 * @param second Borrowed second fundamental solution; NULL exactly when first is NULL.
 * @param bessel Whether the supplied basis uses Bessel functions with continuous values at the origin.
 * @return True after installing both explanations; false for invalid input, rendering or allocation failure.
 * All temporary notation is released internally. No input ownership transfers or thread sharing is introduced.
 * Native coefficient simplification removes zero terms and unit factors in both explanations. The logarithmic
 * derivative is presented as v′/v; plain mathematical powers use the native superscript formatter.
 */
bool de_riccati_steps(diffequ_solve_result_t *result, const expr_t *independent, const expr_t *dependent,
                      const expr_t *quadratic, const expr_t *shift, const expr_t *potential,
                      const expr_t *first, const expr_t *second, bool bessel);

/** @brief Return a borrowed entry from a verified generator matrix. */
const expr_t *de_lie_entry(const matrix_t *matrix, size_t row, size_t column);

/**
 * @brief Format native mathematical text while preserving the name of a bare symbolic coefficient.
 * @param expression Borrowed expression; not changed or retained. Unset variable and constant names remain symbolic.
 * @return Independently owned text released with string_free(), or NULL on invalid input or formatting failure.
 * Used privately by solver and derivation adapters; does not substitute or modify binding values.
 */
string_t *de_notation_symbol_text(const expr_t *expression);

/** @brief Return the borrowed base if the squared wave speed is a perfect square. */
const expr_t *de_wave_square_base(const expr_t *squared);

/** @brief Create a fresh owned wave-equation symbol avoiding the supplied problem. */
expr_t *de_wave_fresh_symbol(const diffequ_t *de, const expr_t *residual, const char *preferred);

/** @brief Describe computed Lie invariants, generators and any reduced equation. */
bool de_lie_set_steps(diffequ_solve_result_t *result, expr_t *const *invariants, char *const *plain,
                      const matrix_t *generators, const equation_t *reduced);

/** @brief Describe affine transport characteristics and their assumptions. */
bool de_affine_steps(diffequ_solve_result_t *result, const expr_t *time, const expr_t *clock,
                     const expr_t *arbitrary, const expr_t *particular, bool coupled);

/** @brief Describe autonomous transport characteristics and their local assumptions. */
bool de_autonomous_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *along,
                         const expr_t *across, const expr_t *coefficient, const expr_t *speed,
                         const expr_t *invariant, const expr_t *relation, bool assumption);

/** @brief Describe the generalised solitary-wave solution and its assumptions. */
bool de_gkdv_present(equation_t *solution, const expr_t *n, const expr_t *v, const expr_t *position,
                     const expr_t *z, bool singular);

/** @brief Describe the verified gradient-envelope and affine solution families. */
bool de_gradient_present(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *parameter,
                         const expr_t *denominator, const expr_t *slope, const expr_t *offset,
                         const expr_t *profile_symbol, bool include_steps);

/** @brief Describe a radial Euler PDE and its characteristic reduction. */
bool de_pde_euler_steps(const diffequ_t *de, diffequ_solve_result_t *result, const number_t *parameters);

/** @brief Describe factorisation and a verified particular solution of a second-order PDE. */
bool de_pde_second_order_steps(const diffequ_t *de, diffequ_solve_result_t *result, const expr_t *particular);

/** @brief Describe the integrating-factor derivation for a parameter-linear PDE. */
bool de_pde_parameter_linear_steps(const expr_t *independent, const expr_t *dependent, expr_t *const *parameters,
                                   size_t parameter_count, const expr_t *coefficient, const expr_t *forcing,
                                   const expr_t *coefficient_integral, const expr_t *integrating_factor,
                                   const expr_t *weighted_forcing, const expr_t *forcing_integral,
                                   const expr_t *arbitrary, const expr_t *solved_right, char **steps_out,
                                   char **steps_TeX_out);

/** @brief Describe the polar harmonic family and its chain-rule verification. */
bool de_polar_laplace_set_steps(diffequ_solve_result_t *result);

/** @brief Describe a local Taylor series and its coefficient recurrence. */
bool de_series_steps(const diffequ_t *de, diffequ_solve_result_t *result);

/** @brief Describe and verify the Cartesian two-dimensional harmonic family. */
bool de_pde_cartesian_laplace_steps(const diffequ_t *de, const equation_t *solution,
                                    char **steps_out, char **steps_TeX_out);

/** @brief Describe a verified stationary-eigenfunction evolution. */
bool de_pde_set_stationary_steps(diffequ_solve_result_t *result, const expr_t *dependent,
                                 const expr_t *time_coordinate, const expr_t *time_point,
                                 const expr_t *time_coefficient, const expr_t *spatial_operator,
                                 const expr_t *initial, const expr_t *rate, const equation_t *solution);

/** @brief Describe the half-line heat solution and its assumptions. */
bool de_heat_present(equation_t *solution, const expr_t *space, const expr_t *time,
                     const expr_t *diffusivity, const expr_t *reaction);

/** @brief Describe the solitary-wave solution and its assumptions. */
bool de_kdv_present(equation_t *solution, const expr_t *k, const expr_t *position);

/** @brief Populate the heat-family derivation. */
bool de_heat_set_steps(diffequ_solve_result_t *result);

/** @brief Populate the solitary-wave derivation. */
bool de_kdv_set_steps(diffequ_solve_result_t *result);

/** @brief Populate the generalised solitary-wave derivation. */
bool de_gkdv_set_steps(diffequ_solve_result_t *result);

/** @brief Build a Fourier evolution explanation from the native equation's symbols. */
bool de_fourier_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *real,
                      const expr_t *imaginary, const expr_t *frequency, const expr_t *time,
                      const expr_t *initial_time);

/** @brief Describe the triangular characteristic equations and their verified domain. */
bool de_triangular_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *x, const expr_t *y,
                         const expr_t *first, const expr_t *second, const expr_t *forcing,
                         const expr_t *invariant, const expr_t *particular, bool repeated);

/** @brief Present nonzero contributions to a one-dimensional wave initial-value solution. */
bool de_wave_ivp_notation(equation_t *solution, const expr_t *displacement, const expr_t *velocity,
                          const expr_t *source, const expr_t *twice_speed);

/** @brief Describe the one-dimensional wave initial-value method and its assumptions. */
bool de_wave_ivp_steps(diffequ_solve_result_t *result, const expr_t *space, const expr_t *time,
                       const expr_t *initial_time, const expr_t *speed_squared, const expr_t *speed);

/** @brief Describe the three-dimensional wave solution and its spherical-average derivation. */
bool de_wave_notation(equation_t *solution, diffequ_solve_result_t *result, expr_t *const *space,
                      const expr_t *time, const expr_t *speed_squared, const expr_t *speed,
                      const expr_t *f, const expr_t *g, const diffequ_t *de, const expr_t *residual,
                      bool include_steps);

/** @brief Project the authored governing equation; the builder owns the returned node. */
const TeX_model_node_t *de_notation_equation(notation_build_t *build, const diffequ_t *problem);

/** @brief Project a governing equation and its conditions into an existing builder. */
const TeX_model_node_t *de_notation_problem(notation_build_t *build, const diffequ_t *problem);

/** @brief Return owned notation for the governing equation alone. */
string_t *de_to_TeX(const diffequ_t *problem);

/** @brief One mathematical derivation row, with optional prose and native expression sides. */
typedef struct {
    const char *caption; /**< Optional plain prose, never typesetting commands. */
    const char *left;    /**< Optional native mathematical expression. */
    const char *right;   /**< Optional right side; when supplied, the row is an equality. */
} de_notation_step_t;

/**
 * @brief Snapshot native derivation rows with the solver's ordinary or partial derivative convention.
 * @param result Borrowed mutable result receiving independently owned plain and TeX explanations.
 * @param plain Borrowed native explanation, copied on success; not typesetting syntax.
 * @param steps Borrowed array of native mathematical sides and literal captions, read during this call only.
 * @param count Number of rows, from one to 65,536.
 * @param partial Whether derivatives use partial rather than ordinary differentials.
 * @return True when both representations are stored, or false for invalid input, parsing or allocation failure.
 * No input ownership transfers. The result owns its strings until de_solve_result_free and must not be mutated
 * concurrently. TeX alone supplies typesetting syntax; callers supply mathematical meaning.
 */
bool de_notation_steps(diffequ_solve_result_t *result, const char *plain,
                       const de_notation_step_t *steps, size_t count, bool partial);

/** @brief Render a prepared semantic right side and install its accompanying native display text. */
bool de_notation_display(equation_t *solution, const string_t *left, const string_t *right,
                         notation_build_t *build, const TeX_model_node_t *body);

#endif
