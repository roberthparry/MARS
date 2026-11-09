/**
 * @file diffequ_solve_internal.h
 * @brief Private differential-equation solver attempt contracts.
 *
 * Defines matched, solved and failed attempt outcomes and common expression-construction helpers. Specialised ODE
 * and PDE strategies use these contracts to return results consistently to the solver dispatcher.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef DIFFEQU_SOLVE_INTERNAL_H
#define DIFFEQU_SOLVE_INTERNAL_H

#if !defined(MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS."
#endif

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef enum { DE_ATTEMPT_NOT_MATCHED, DE_ATTEMPT_SOLVED, DE_ATTEMPT_FAILED } de_attempt_t;

/**
 * @brief Extract constant coefficients of a modified-Emden residual independently of product association.
 * @param[in] residual Borrowed governing expression; not consumed or changed.
 * @param[in] independent Borrowed independent coordinate.
 * @param[in] dependent Borrowed dependent coordinate.
 * @param[in] first Borrowed first derivative node.
 * @param[in] second Borrowed second derivative node.
 * @param[out] leading Receives an owned second-derivative coefficient on success, otherwise NULL.
 * @param[out] mixed Receives an owned dependent-times-first-derivative coefficient, otherwise NULL.
 * @param[out] cubic Receives an owned cubic coefficient, otherwise NULL.
 * @return True for exactly the supported constant-coefficient form; false for other terms or allocation failure.
 * @note Release each successful output with expr_free(). Inputs are never consumed; use serially with their graphs.
 */
bool de_decompose_emden(const expr_t *residual, const expr_t *independent, const expr_t *dependent,
    const expr_t *first, const expr_t *second, expr_t **leading, expr_t **mixed, expr_t **cubic);

/**
 * @brief Reduce a constant-coefficient modified-Emden equation to exact parametric quadratures.
 * @param[in] de Borrowed original problem; initial/boundary data or coordinate intervals cause this strategy to decline.
 * @param[in] independent Borrowed independent coordinate, not retained separately from returned equations.
 * @param[in] dependent Borrowed dependent coordinate, not changed.
 * @param[in] second Borrowed second derivative node used to extract the leading coefficient.
 * @param[in] first Borrowed first derivative node; must be present.
 * @param[in] residual Borrowed governing residual; neither changed nor consumed.
 * @param[in] include_steps Whether to construct native and semantic derivation text.
 * @return Owned result released with de_solve_result_free(), or NULL when unmatched or allocation fails.
 * @note Existing point-linearisation is tried first. The first two returned equations form one local
 * parametric family; later equations retain equilibrium and constant-ratio branches. Use in a serial solve.
 */
diffequ_solve_result_t *de_solve_emden_quadrature(const diffequ_t *de, const expr_t *independent,
    const expr_t *dependent, const expr_t *second, const expr_t *first, const expr_t *residual, bool include_steps);

/**
 * @brief Reduce a first-order Riccati equation and solve supported monomial normal potentials.
 * @param de Borrowed original problem; not changed or retained.
 * @param independent Borrowed independent variable of the isolated derivative.
 * @param dependent Borrowed dependent variable of the isolated derivative.
 * @param derivative_right Borrowed right side of the isolated first derivative.
 * @param include_steps Whether to construct native and TeX explanations through semantic notation.
 * @return Owned result, released with de_solve_result_free, or NULL when the equation is not quadratic in
 * the dependent variable or allocation fails. A recognised but unsolved reduction reports unsupported;
 * solved results retain the exceptional projective branch and any supported single finite real value condition.
 * The caller's current computation control applies; no input ownership transfers.
 */
diffequ_solve_result_t *de_solve_riccati(const diffequ_t *de, const expr_t *independent,
                                       const expr_t *dependent, const expr_t *derivative_right, bool include_steps);

/**
 * @brief Integrate a real autonomous exponential force using its conserved energy.
 * @param de Borrowed problem; no input is changed or consumed.
 * @param independent Borrowed independent coordinate, retained by returned formulae.
 * @param dependent Borrowed dependent field, retained by returned equations.
 * @param second Borrowed second derivative isolated from the residual.
 * @param first Borrowed first derivative when present, or NULL.
 * @param residual Borrowed governing residual, equal to zero.
 * @param include_steps Whether to generate semantic derivation rows and native explanatory text.
 * @return Owned result released with de_solve_result_free, or NULL when unmatched or allocation fails.
 * Recognises y'' = A*exp(B*y) with finite real non-zero, evaluable A and B. Without point data it returns
 * every real energy branch, including the zero-energy limit where present. With two finite real initial data
 * at one point it returns the corresponding local solution. Other conditions leave this method unmatched.
 * Formulae retain their real-coordinate and pole restrictions and may outlive all inputs. Mutable graphs
 * require caller synchronisation; the current computation scope and number precision remain in force.
 */
diffequ_solve_result_t *de_solve_autonomous_exponential(const diffequ_t *de, const expr_t *independent,
    const expr_t *dependent, const expr_t *second, const expr_t *first, const expr_t *residual, bool include_steps);

/** @brief Private owner of a real Bessel basis and its analytic normalisation for a Riccati reduction. */
typedef struct _de_riccati_bessel_t de_riccati_bessel_t;

/**
 * @brief Construct a real Bessel presentation for a positive even monomial normal potential.
 * @param potential Borrowed coefficient in v'' + potential*v = 0; not changed or retained.
 * @param independent Borrowed real coordinate; retained by the constructed expressions.
 * @return Owned presentation, released with de_riccati_bessel_free, or NULL for an unsupported potential or failure.
 * Only the differential-equation implementation uses this interface; it exposes no notation context publicly.
 */
de_riccati_bessel_t *de_riccati_bessel_new(const expr_t *potential, const expr_t *independent);

/**
 * @brief Release a private Bessel presentation and every expression it owns.
 * @param bessel Owned presentation to release; may be NULL. All borrowed getters become invalid afterwards.
 * @return Nothing. No caller-owned expressions or solver results are freed.
 */
void de_riccati_bessel_free(de_riccati_bessel_t *bessel);

/**
 * @brief Borrow the scale converting the displayed Bessel constant into the analytic basis constant.
 * @param bessel Borrowed non-NULL presentation; not changed or retained.
 * @return Borrowed expression, valid until de_riccati_bessel_free; do not free it. NULL for a NULL presentation.
 */
const expr_t *de_riccati_bessel_scale(const de_riccati_bessel_t *bessel);

/**
 * @brief Borrow the two real Bessel basis functions with their removable origin singularities understood.
 * @param bessel Borrowed presentation; not changed or retained.
 * @param first Required output receiving the first borrowed basis function, or NULL for a NULL presentation.
 * @param second Required output receiving the second borrowed basis function, or NULL for a NULL presentation.
 * @return Nothing. Both outputs remain valid until de_riccati_bessel_free; neither is independently freed.
 */
void de_riccati_bessel_basis(const de_riccati_bessel_t *bessel, const expr_t **first, const expr_t **second);

/**
 * @brief Attach faithful one-constant Bessel notation and the real continuation at the origin to a solution.
 * @param bessel Borrowed presentation; not changed or retained.
 * @param solution Borrowed mutable equation whose independently owned display metadata is replaced.
 * @param quadratic Borrowed quadratic Riccati coefficient; not retained.
 * @param shift Borrowed logarithmic-derivative shift; not retained.
 * @param constant Borrowed sole arbitrary constant, or NULL for initial data or the limiting member.
 * @param point Borrowed initial coordinate, or NULL without an initial value.
 * @param value Borrowed initial value; non-NULL exactly when point is supplied.
 * @return True on success; false for invalid operands, allocation or rendering failure. With neither constant
 * nor point, the second basis function supplies the limiting member. Evaluation graphs remain independently owned.
 */
bool de_riccati_bessel_present(const de_riccati_bessel_t *bessel, equation_t *solution,
                              const expr_t *quadratic, const expr_t *shift, const expr_t *constant,
                              const expr_t *point, const expr_t *value);

/** @brief Construct native derivation text for the scaled modified-Emden family. */
bool de_modified_emden_steps(const expr_t *independent, const expr_t *dependent, const expr_t *scale,
                             char **steps_out, char **steps_TeX_out);

/** @brief Recognise a modified-Emden equation and apply supported initial conditions. */
de_attempt_t de_attempt_modified_emden_linearization(const diffequ_t *de, const expr_t *independent,
                                                     const expr_t *dependent, const expr_t *first_derivative,
                                                     const expr_t *second_derivative, const expr_t *residual,
                                                     equation_t **solutions_out, size_t *solution_count_out,
                                                     expr_t **scale_out);

expr_t *de_simplify_unary_owned(expr_t *owned, expr_t *(*operation)(const expr_t *));

/**
 * @brief Construct a primitive for an ODE rate, retaining a formal integral when no rule succeeds.
 * @param integrand Borrowed rate expression; ownership and bindings are unchanged.
 * @param wrt Borrowed independent-variable node, interpreted as real for recognised real affine trigonometric rates.
 * @return Owned primitive without an integration constant, released with expr_free, or NULL for missing inputs
 * or allocation failure. Real affine tangent, cotangent and hyperbolic cotangent rates use logarithms of absolute
 * values on each regular real interval. Other rates use the expression integrator and its branch conventions,
 * then an exact formal integral if integration is unsupported. The returned graph owns its required nodes
 * and may outlive the input expressions.
 * As with the source expression graph, evaluation and mutable bindings are not safe for concurrent use.
 */
expr_t *de_integrate_or_formal(const expr_t *integrand, const expr_t *wrt);

bool de_expr_contains(const expr_t *expr, const expr_t *needle);
bool de_linear_decompose(const expr_t *expr, const expr_t *needle, expr_t **coefficient_out, expr_t **constant_out);
bool de_expr_uses(const expr_t *expr, const expr_t *variable);
bool de_find_derivatives(const expr_t *expr, const expr_t *independent, const expr_t **dependent_out,
                          const expr_t **first_out, const expr_t **second_out, size_t *highest_order_out);
diffequ_solve_result_t *de_lie_unsolved_analysis(const diffequ_t *de, bool include_steps);
bool de_find_initial_condition(const diffequ_t *de, const expr_t *dependent, const expr_t **point_out,
                               const expr_t **value_out);
bool de_find_derivative_condition(const diffequ_t *de, const expr_t *dependent, const expr_t *independent, size_t order,
                                  const expr_t **point_out, const expr_t **value_out);
bool de_has_zero_initial_condition(const diffequ_t *de, const expr_t *dependent);
expr_t *de_arbitrary_constant(void);
bool de_split_separable(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
                        expr_t **independent_factor_out, expr_t **dependent_factor_out);

de_attempt_t de_attempt_separable(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                  const expr_t *derivative_right, equation_t **solution_out);
de_attempt_t de_attempt_exact_first_order(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                          const expr_t *first_derivative, const expr_t *residual,
                                          equation_t **solutions_out, size_t *solution_count_out);
de_attempt_t de_attempt_linear(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                               const expr_t *derivative_right, equation_t **solution_out);
de_attempt_t de_attempt_bernoulli_square(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                         const expr_t *derivative_right, equation_t **solution_out);
de_attempt_t de_attempt_derivative_quadratic(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                             const expr_t *first_derivative, const expr_t *residual,
                                             equation_t **solutions_out, size_t *solution_count_out);
de_attempt_t de_attempt_exact_derivative_linearization(const diffequ_t *de, const expr_t *independent,
                                                       const expr_t *dependent, const expr_t *first_derivative,
                                                       const expr_t *second_derivative, const expr_t *residual,
                                                       equation_t **solutions_out, size_t *solution_count_out);
de_attempt_t de_attempt_homogeneous(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                    const expr_t *derivative_right, equation_t **solutions_out,
                                    size_t *solution_count_out);
de_attempt_t de_attempt_linear_substitution(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                            const expr_t *derivative_right, equation_t **solution_out);
de_attempt_t de_attempt_linear_transformation(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                              const expr_t *derivative_right, equation_t **solution_out);
de_attempt_t de_attempt_sturm_liouville(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                        const expr_t *second_derivative, const expr_t *first_derivative,
                                        const expr_t *residual, equation_t **solution_out, de_solver_t *solver_out);
int de_sturm_liouville_cubic_basis(const expr_t *independent, const expr_t *parameter, const expr_t *dependent,
                                   equation_t **solutions_out, size_t *solution_count_out);
/**
 * @brief Impose complete or partial point conditions on a known scalar linear solution basis.
 * @param[in] de Borrowed ordinary differential problem with one to count linear point conditions.
 * @param[in] coordinate Borrowed independent coordinate used for differentiation and point substitution.
 * @param[in] dependent Borrowed scalar dependent field identifying the conditions.
 * @param[in] basis Borrowed array of count homogeneous basis functions; neither array nor entries are consumed.
 * @param[in] count Number of independent basis functions, equal to the differential order.
 * @param[in] particular Borrowed particular solution, including zero for a homogeneous problem.
 * @return Owned solution released with expr_free(), retaining arbitrary constants for undetermined directions,
 * or NULL for invalid inputs, unsupported conditions, inconsistent data or allocation/algebra failure.
 * @note Owning linear ODE solvers use this module-private interface. Exact matrix operations retain symbolic
 * coefficient restrictions; inputs remain unchanged and must not be mutated concurrently.
 */
expr_t *de_apply_basis_conditions(const diffequ_t *de, const expr_t *coordinate, const expr_t *dependent,
                                  expr_t *const *basis, size_t count, const expr_t *particular);

de_attempt_t de_attempt_constant_coefficient_linear(const diffequ_t *de, const expr_t *independent,
                                                    const expr_t *dependent, size_t order, const expr_t *residual,
                                                    equation_t **solution_out);
de_attempt_t de_attempt_repeated_quadratic_operator(const diffequ_t *de, const expr_t *independent,
                                                    const expr_t *dependent, const expr_t *signed_square,
                                                    size_t multiplicity, equation_t **solution_out);
#endif /* DIFFEQU_SOLVE_INTERNAL_H */
