/**
 * @file test_expr_support.h
 * @brief Optional mathematical adapters shared by expression and matrix suites.
 *
 * These helpers use public library APIs and are linked only by suites requesting them. They are independent
 * of expression-suite registration and private fixtures. The generic harness never links this module.
 */
#ifndef TEST_EXPR_SUPPORT_H
#define TEST_EXPR_SUPPORT_H

#include "expression-t.h"
#include "qfloat-t.h"
#include "qcomplex-t.h"

/**
 * @brief Create a constant from a double and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_const_d(double x);

/**
 * @brief Create a constant from a qfloat and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_const_qf(qfloat_t x);

/**
 * @brief Create a constant from a complex value and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_const_qc(qcomplex_t x);

/**
 * @brief Create a named constant from a double and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_const_d(double x, const char *name);

/**
 * @brief Create a named constant from a qfloat and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_const_qf(qfloat_t x, const char *name);

/**
 * @brief Create a named constant from a complex value and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_const_qc(qcomplex_t x, const char *name);

/**
 * @brief Create a variable from a double and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_var_d(double x);

/**
 * @brief Create a variable from a qfloat and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_var_qf(qfloat_t x);

/**
 * @brief Create a variable from a complex value and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_var_qc(qcomplex_t x);

/**
 * @brief Create a named variable from a double and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_var_d(double x, const char *name);

/**
 * @brief Create a named variable from a string and release the temporary number.
 * @param[in] text Caller-owned native numeric text, parsed synchronously.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_var_s(const char *text, const char *name);

/**
 * @brief Create a named variable from a qfloat and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_var_qf(qfloat_t x, const char *name);

/**
 * @brief Create a named variable from a complex value and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] name Caller-owned NUL-terminated symbol name, copied by the expression API.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_new_named_var_qc(qcomplex_t x, const char *name);

/**
 * @brief Set an expression value from a double and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return No value; the expression API supplies mutation behaviour and error handling.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
void test_expr_set_val_d(expr_t *expr, double x);

/**
 * @brief Set an expression value from a qfloat and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return No value; the expression API supplies mutation behaviour and error handling.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
void test_expr_set_val_qf(expr_t *expr, qfloat_t x);

/**
 * @brief Set an expression value from a complex value and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return No value; the expression API supplies mutation behaviour and error handling.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
void test_expr_set_val_qc(expr_t *expr, qcomplex_t x);

/**
 * @brief Add a double to an expression and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_add_d(const expr_t *expr, double x);

/**
 * @brief Subtract a double from an expression and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_sub_d(const expr_t *expr, double x);

/**
 * @brief Subtract an expression from a double and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_d_sub(double x, const expr_t *expr);

/**
 * @brief Multiply an expression by a double and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_mul_d(const expr_t *expr, double x);

/**
 * @brief Divide an expression by a double and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_div_d(const expr_t *expr, double x);

/**
 * @brief Divide a double by an expression and release the temporary number.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_d_div(double x, const expr_t *expr);

/**
 * @brief Raise an expression to a double power and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_pow_d(const expr_t *expr, double x);

/**
 * @brief Raise an expression to a complex power and release the temporary number.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @param[in] x Numeric input copied into a temporary number that is released before returning.
 * @return Owned expression released with expr_free, or NULL if expression construction fails.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
expr_t *test_expr_pow_qc(const expr_t *expr, qcomplex_t x);

/**
 * @brief Evaluate an expression as a double and release temporary numbers.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Numeric value returned by value; invalid/unset evaluation follows the number API's NaN/error behaviour.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
double test_expr_eval_d(const expr_t *expr);

/**
 * @brief Evaluate an expression as a qfloat and release temporary numbers.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Numeric value returned by value; invalid/unset evaluation follows the number API's NaN/error behaviour.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
qfloat_t test_expr_eval_qf(const expr_t *expr);

/**
 * @brief Evaluate an expression as a complex value and release temporary numbers.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Numeric value returned by value; invalid/unset evaluation follows the number API's NaN/error behaviour.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
qcomplex_t test_expr_eval_qc(const expr_t *expr);

/**
 * @brief Read an expression value as a qfloat and release temporary numbers.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Numeric value returned by value; invalid/unset evaluation follows the number API's NaN/error behaviour.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
qfloat_t test_expr_get_val_qf(const expr_t *expr);

/**
 * @brief Read an expression value as a complex value and release temporary numbers.
 * @param[in] expr Caller-owned source expression, not consumed.
 * @return Numeric value returned by value; invalid/unset evaluation follows the number API's NaN/error behaviour.
 * @note No input is consumed. Use sequentially with mutations of the same expression; no temporary number escapes.
 */
qcomplex_t test_expr_get_val_qc(const expr_t *expr);

/** @brief Test-only convenience name for test_expr_eval_d; arguments, ownership and failures follow that function. */
#define expr_eval_d test_expr_eval_d
/** @brief Test-only convenience name for test_expr_eval_qf; arguments, ownership and failures follow that function. */
#define expr_eval_qf test_expr_eval_qf
/** @brief Test-only convenience name for test_expr_eval_qc; arguments, ownership and failures follow that function. */
#define expr_eval_qc test_expr_eval_qc
/** @brief Test-only convenience name for test_expr_get_val_qf; arguments, ownership and failures follow that function. */
#define expr_get_val_qf test_expr_get_val_qf
/** @brief Test-only convenience name for test_expr_get_val_qc; arguments, ownership and failures follow that function. */
#define expr_get_val_qc test_expr_get_val_qc
/** @brief Test-only convenience name for test_expr_add_d; arguments, ownership and failures follow that function. */
#define expr_add_d test_expr_add_d
/** @brief Test-only convenience name for test_expr_sub_d; arguments, ownership and failures follow that function. */
#define expr_sub_d test_expr_sub_d
/** @brief Test-only convenience name for test_expr_d_sub; arguments, ownership and failures follow that function. */
#define expr_d_sub test_expr_d_sub
/** @brief Test-only convenience name for test_expr_mul_d; arguments, ownership and failures follow that function. */
#define expr_mul_d test_expr_mul_d
/** @brief Test-only convenience name for test_expr_div_d; arguments, ownership and failures follow that function. */
#define expr_div_d test_expr_div_d
/** @brief Test-only convenience name for test_expr_d_div; arguments, ownership and failures follow that function. */
#define expr_d_div test_expr_d_div
/** @brief Test-only convenience name for test_expr_pow_d; arguments, ownership and failures follow that function. */
#define expr_pow_d test_expr_pow_d
/** @brief Test-only convenience name for test_expr_pow_qc; arguments, ownership and failures follow that function. */
#define expr_pow_qc test_expr_pow_qc

#endif
