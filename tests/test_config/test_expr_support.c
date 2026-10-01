/* Shared expression value adapters for the expression and matrix suites. */
#include "../expression/test_expr.h"

/* Create a constant from a double and release the temporary number. */
expr_t *test_expr_new_const_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *expr = expr_new_const(n);

    num_destroy(&n);
    return expr;
}

/* Create a constant from a qfloat and release the temporary number. */
expr_t *test_expr_new_const_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *expr = expr_new_const(n);

    num_destroy(&n);
    return expr;
}

/* Create a constant from a complex value and release the temporary number. */
expr_t *test_expr_new_const_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    expr_t *expr = expr_new_const(n);

    num_destroy(&n);
    return expr;
}

/* Create a named constant from a double and release the temporary number. */
expr_t *test_expr_new_named_const_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *expr = expr_new_named_const(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a named constant from a qfloat and release the temporary number. */
expr_t *test_expr_new_named_const_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *expr = expr_new_named_const(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a named constant from a complex value and release the temporary number. */
expr_t *test_expr_new_named_const_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    expr_t *expr = expr_new_named_const(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a variable from a double and release the temporary number. */
expr_t *test_expr_new_var_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *expr = expr_new_var(n);

    num_destroy(&n);
    return expr;
}

/* Create a variable from a qfloat and release the temporary number. */
expr_t *test_expr_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *expr = expr_new_var(n);

    num_destroy(&n);
    return expr;
}

/* Create a variable from a complex value and release the temporary number. */
expr_t *test_expr_new_var_qc(qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    expr_t *expr = expr_new_var(n);

    num_destroy(&n);
    return expr;
}

/* Create a named variable from a double and release the temporary number. */
expr_t *test_expr_new_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *expr = expr_new_named_var(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a named variable from a string and release the temporary number. */
expr_t *test_expr_new_named_var_s(const char *text, const char *name)
{
    number_t n = num_create_from_string(text);
    expr_t *expr = expr_new_named_var(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a named variable from a qfloat and release the temporary number. */
expr_t *test_expr_new_named_var_qf(qfloat_t x, const char *name)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *expr = expr_new_named_var(n, name);

    num_destroy(&n);
    return expr;
}

/* Create a named variable from a complex value and release the temporary number. */
expr_t *test_expr_new_named_var_qc(qcomplex_t x, const char *name)
{
    number_t n = num_create_from_qcomplex(x);
    expr_t *expr = expr_new_named_var(n, name);

    num_destroy(&n);
    return expr;
}

/* Set an expression value from a double and release the temporary number. */
void test_expr_set_val_d(expr_t *expr, double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));

    expr_set_val(expr, n);
    num_destroy(&n);
}

/* Set an expression value from a qfloat and release the temporary number. */
void test_expr_set_val_qf(expr_t *expr, qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);

    expr_set_val(expr, n);
    num_destroy(&n);
}

/* Set an expression value from a complex value and release the temporary number. */
void test_expr_set_val_qc(expr_t *expr, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);

    expr_set_val(expr, n);
    num_destroy(&n);
}

/* Add a double to an expression and release the temporary number. */
expr_t *test_expr_add_d(const expr_t *expr, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_add_num(expr, &n);

    num_destroy(&n);
    return out;
}

/* Subtract a double from an expression and release the temporary number. */
expr_t *test_expr_sub_d(const expr_t *expr, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_sub_num(expr, &n);

    num_destroy(&n);
    return out;
}

/* Subtract an expression from a double and release the temporary number. */
expr_t *test_expr_d_sub(double x, const expr_t *expr)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_num_sub(&n, expr);

    num_destroy(&n);
    return out;
}

/* Multiply an expression by a double and release the temporary number. */
expr_t *test_expr_mul_d(const expr_t *expr, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_mul_num(expr, &n);

    num_destroy(&n);
    return out;
}

/* Divide an expression by a double and release the temporary number. */
expr_t *test_expr_div_d(const expr_t *expr, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_div_num(expr, &n);

    num_destroy(&n);
    return out;
}

/* Divide a double by an expression and release the temporary number. */
expr_t *test_expr_d_div(double x, const expr_t *expr)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_num_div(&n, expr);

    num_destroy(&n);
    return out;
}

/* Raise an expression to a double power and release the temporary number. */
expr_t *test_expr_pow_d(const expr_t *expr, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_pow(expr, &n);

    num_destroy(&n);
    return out;
}

/* Raise an expression to a complex power and release the temporary number. */
expr_t *test_expr_pow_qc(const expr_t *expr, qcomplex_t x)
{
    number_t n = num_create_from_qcomplex(x);
    expr_t *out = expr_pow(expr, &n);

    num_destroy(&n);
    return out;
}

/* Evaluate an expression as a double and release temporary numbers. */
double test_expr_eval_d(const expr_t *expr)
{
    number_t n = expr_eval(expr);
    double out = num_to_double(n);

    num_destroy(&n);
    return out;
}

/* Evaluate an expression as a qfloat and release temporary numbers. */
qfloat_t test_expr_eval_qf(const expr_t *expr)
{
    number_t n = expr_eval(expr);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

/* Evaluate an expression as a complex value and release temporary numbers. */
qcomplex_t test_expr_eval_qc(const expr_t *expr)
{
    number_t n = expr_eval(expr);
    number_t re_n = num_real_part(n);
    number_t im_n = num_imag_part(n);
    qcomplex_t out = qc_make(num_to_qfloat(re_n), num_to_qfloat(im_n));

    num_destroy(&im_n);
    num_destroy(&re_n);
    num_destroy(&n);
    return out;
}

/* Read an expression value as a qfloat and release temporary numbers. */
qfloat_t test_expr_get_val_qf(const expr_t *expr)
{
    number_t n = expr_get_val(expr);
    qfloat_t out = num_to_qfloat(n);

    num_destroy(&n);
    return out;
}

/* Read an expression value as a complex value and release temporary numbers. */
qcomplex_t test_expr_get_val_qc(const expr_t *expr)
{
    number_t n = expr_get_val(expr);
    number_t re_n = num_real_part(n);
    number_t im_n = num_imag_part(n);
    qcomplex_t out = qc_make(num_to_qfloat(re_n), num_to_qfloat(im_n));

    num_destroy(&im_n);
    num_destroy(&re_n);
    num_destroy(&n);
    return out;
}
