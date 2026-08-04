#ifndef DIFFEQU_SOLVE_INTERNAL_H
#define DIFFEQU_SOLVE_INTERNAL_H

#if !defined(MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "diffequ_solve_internal.h is private to the diffequation solver."
#endif

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef enum {
    DE_ATTEMPT_NOT_MATCHED,
    DE_ATTEMPT_SOLVED,
    DE_ATTEMPT_FAILED
} de_attempt_t;

expr_t *de_simplify_unary_owned(
    expr_t *owned,
    expr_t *(*operation)(const expr_t *));
expr_t *de_integrate_or_formal(const expr_t *integrand,
                               const expr_t *wrt);
bool de_expr_contains(const expr_t *expr, const expr_t *needle);
bool de_linear_decompose(const expr_t *expr,
                         const expr_t *needle,
                         expr_t **coefficient_out,
                         expr_t **constant_out);
bool de_expr_uses(const expr_t *expr, const expr_t *variable);
bool de_find_initial_condition(const diffequ_t *de,
                               const expr_t *dependent,
                               const expr_t **point_out,
                               const expr_t **value_out);
bool de_find_derivative_condition(const diffequ_t *de,
                                  const expr_t *dependent,
                                  const expr_t *independent,
                                  size_t order,
                                  const expr_t **point_out,
                                  const expr_t **value_out);
bool de_has_zero_initial_condition(const diffequ_t *de,
                                   const expr_t *dependent);
expr_t *de_arbitrary_constant(void);
bool de_split_separable(const expr_t *expr,
                        const expr_t *independent,
                        const expr_t *dependent,
                        expr_t **independent_factor_out,
                        expr_t **dependent_factor_out);

de_attempt_t de_attempt_separable(const diffequ_t *de,
                                  const expr_t *independent,
                                  const expr_t *dependent,
                                  const expr_t *derivative_right,
                                  equation_t **solution_out);
de_attempt_t de_attempt_exact_first_order(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out);
de_attempt_t de_attempt_linear(const diffequ_t *de,
                               const expr_t *independent,
                               const expr_t *dependent,
                               const expr_t *derivative_right,
                               equation_t **solution_out);
de_attempt_t de_attempt_bernoulli_square(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out);
de_attempt_t de_attempt_derivative_quadratic(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out);
de_attempt_t de_attempt_exact_derivative_linearization(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *first_derivative,
    const expr_t *second_derivative,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out);
de_attempt_t de_attempt_homogeneous(const diffequ_t *de,
                                    const expr_t *independent,
                                    const expr_t *dependent,
                                    const expr_t *derivative_right,
                                    equation_t **solutions_out,
                                    size_t *solution_count_out);
de_attempt_t de_attempt_linear_substitution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out);
de_attempt_t de_attempt_linear_transformation(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out);
de_attempt_t de_attempt_sturm_liouville(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *second_derivative,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solution_out);
int de_sturm_liouville_cubic_basis(
    const expr_t *independent,
    const expr_t *parameter,
    const expr_t *dependent,
    equation_t **solutions_out,
    size_t *solution_count_out);
de_attempt_t de_attempt_constant_coefficient_linear(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    size_t order,
    const expr_t *residual,
    equation_t **solution_out);
#endif /* DIFFEQU_SOLVE_INTERNAL_H */
