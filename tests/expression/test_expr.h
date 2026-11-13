#ifndef TESTS_EXPR_EXPR_TEST_H
#define TESTS_EXPR_EXPR_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "qfloat-t.h"
#include "test_harness.h"
#include "test_expr_support.h"

/**
 * @brief Run database-backed numerical callable and rendering regressions.
 * @return No value; failures are recorded by the sequential test harness. Owns and releases its fixtures.
 */
void test_expr_numeric_callables(void);

/**
 * @brief Run the exact-integer callable programme synchronised with docs/expression.md.
 * @return No value; prints documented output and records failures. Run after ordinary tests.
 */
void example_expression_integer_callables(void);

int str_eq(const char *a, const char *b);
void to_string_pass(const char *msg, const char *got, const char *want);
void to_string_fail(const char *file, int line, int col, const char *msg, const char *got, const char *want);

void check_q_at(const char *file, int line, int col, const char *label, qfloat_t got, qfloat_t want);
void print_expr_of(const expr_t *f);
const test_validity_contract_t *expr_validity_contract_number_exact(void);
const test_validity_contract_t *expr_validity_contract_number_close(void);

#define TEST_ASSERT_EXPR_NUMBER_EQ(got, want)                                                                   \
    do {                                                                                                               \
        number_t test_expr_got__ = (got);                                                                        \
        number_t test_expr_want__ = (want);                                                                    \
        TEST_ASSERT_VALID_NAMED("expr-number-exact", &test_expr_got__, &test_expr_want__);                      \
    } while (0)

#define TEST_ASSERT_EXPR_NUMBER_CLOSE(got, want)                                                                \
    do {                                                                                                               \
        number_t test_expr_got__ = (got);                                                                        \
        number_t test_expr_want__ = (want);                                                                    \
        TEST_ASSERT_VALID_NAMED("expr-number-close", &test_expr_got__, &test_expr_want__);                      \
    } while (0)

#define ASSERT_EXPR_NUMBER_EQ(got, want) TEST_ASSERT_EXPR_NUMBER_EQ((got), (want))

#define ASSERT_EXPR_NUMBER_CLOSE(got, want) TEST_ASSERT_EXPR_NUMBER_CLOSE((got), (want))

void test_arithmetic(void);
void test_d_variants(void);
void test_maths_functions(void);
void test_first_derivatives(void);
void test_second_derivatives(void);
void test_expr_t_to_string(void);
void test_expr_t_from_string(void);
void test_expr_t_goal_seek(void);

/** Check imaginary-axis identities and principal-branch presentation. */
void test_expr_t_imaginary_axis(void);
void test_partial_derivatives(void);
void test_expr_tree_match_helpers(void);
void test_symbolic_integration(void);
void test_runtime_regressions(void);
void test_fourier_and_signal_functions(void);
void test_orthogonal_polynomials(void);
void example_orthopoly_readme_examples(void);
void example_signal_readme_examples(void);
void test_reverse_mode(void);

void test_to_string_all(void);
void test_expressions(void);
void test_expressions_unnamed(void);
void test_expressions_longname(void);
void test_erf(void);
void test_erfc(void);
void test_erfinv(void);
void test_erfcinv(void);
void test_gamma(void);
void test_gammainv(void);
void test_lgamma(void);
void test_digamma(void);
void test_trigamma(void);
void test_polygamma(void);
void test_dilog_polylog(void);
void test_bessel(void);
void test_lambert_w0(void);
void test_lambert_wm1(void);
void test_normal_pdf(void);
void test_normal_cdf(void);
void test_normal_logpdf(void);
void test_Ei(void);
void test_E1(void);
void test_beta(void);
void test_logbeta(void);
void test_gammainc(void);
void test_beta_pdf(void);
void test_logbeta_pdf(void);
void test_binomial(void);
void test_deriv_trigamma(void);
void test_second_deriv_digamma(void);

void check_roundtrip(const char *label, expr_t *f, int line);
void check_parse_val(const char *label, const char *s, double want_d, int line);
void check_parse_null(const char *label, const char *s, int line);
void check_parse_null_stderr_contains(const char *label, const char *s, const char *want_substring, int line);

expr_t *make_expr_u01(void);
expr_t *make_expr_u02(void);
expr_t *make_expr_u03(void);
expr_t *make_expr_u04(void);
expr_t *make_expr_u05(void);
expr_t *make_expr_u06(void);
expr_t *make_expr_c01(void);
expr_t *make_expr_c02(void);
expr_t *make_expr_c03(void);
expr_t *make_expr_c04(void);
expr_t *make_expr_l01(void);
expr_t *make_expr_l02(void);
expr_t *make_expr_l03(void);
expr_t *make_expr_l04(void);
expr_t *make_expr_l05(void);
expr_t *make_expr_l06(void);
expr_t *make_expr_l07(void);
expr_t *make_expr_l08(void);
expr_t *make_expr_l09(void);

#endif
