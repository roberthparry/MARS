#include "test_expr.h"

static void test_partial_xy_product(void)
{
    expr_t *x = test_expr_new_var_d(2.0);
    expr_t *y = test_expr_new_var_d(3.0);
    expr_t *f = expr_mul(x, y);

    expr_t *df_dx = expr_create_deriv(f, x);
    expr_t *df_dy = expr_create_deriv(f, y);
    expr_t *d2f   = expr_create_2nd_deriv(f, x, y);

    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂x at x=2,y=3", expr_eval_qf(df_dx), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂y at x=2,y=3", expr_eval_qf(df_dy), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²(x*y)/∂x∂y",         expr_eval_qf(d2f),   qf_from_double(1.0));

    expr_free(d2f);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f);
    expr_free(y);
    expr_free(x);
}

/* f(x,y) = x² + y³  →  ∂f/∂x = 2x,  ∂f/∂y = 3y²,  ∂²f/∂x² = 2,  ∂²f/∂y² = 6y */
static void test_partial_poly(void)
{
    expr_t *x  = test_expr_new_var_d(2.0);
    expr_t *y  = test_expr_new_var_d(3.0);
    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *y3 = expr_pow_d(y, 3.0);
    expr_t *f  = expr_add(x2, y3);

    expr_t *df_dx   = expr_create_deriv(f, x);
    expr_t *df_dy   = expr_create_deriv(f, y);
    expr_t *d2f_dx2 = expr_create_2nd_deriv(f, x, x);
    expr_t *d2f_dy2 = expr_create_2nd_deriv(f, y, y);

    /* at x=2: ∂f/∂x = 2*2 = 4 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂x at x=2",   expr_eval_qf(df_dx),   qf_from_double(4.0));
    /* at y=3: ∂f/∂y = 3*9 = 27 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂y at y=3",   expr_eval_qf(df_dy),   qf_from_double(27.0));
    /* ∂²f/∂x² = 2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂x² = 2",    expr_eval_qf(d2f_dx2), qf_from_double(2.0));
    /* at y=3: ∂²f/∂y² = 6y = 18 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂y² at y=3", expr_eval_qf(d2f_dy2), qf_from_double(18.0));

    expr_free(d2f_dy2);
    expr_free(d2f_dx2);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f);
    expr_free(y3);
    expr_free(x2);
    expr_free(y);
    expr_free(x);
}

/* f(x,y) = sin(x) * exp(y)  →  ∂f/∂x = cos(x)*exp(y),  ∂f/∂y = sin(x)*exp(y),
   ∂²f/∂x∂y = cos(x)*exp(y) */
static void test_partial_sin_exp(void)
{
    expr_t *x    = test_expr_new_var_d(1.0);
    expr_t *y    = test_expr_new_var_d(2.0);
    expr_t *sinx = expr_sin(x);
    expr_t *expy = expr_exp(y);
    expr_t *f    = expr_mul(sinx, expy);

    expr_t *df_dx = expr_create_deriv(f, x);
    expr_t *df_dy = expr_create_deriv(f, y);
    expr_t *d2f   = expr_create_2nd_deriv(f, x, y);

    qfloat_t qcos1   = qf_cos(qf_from_double(1.0));
    qfloat_t qsin1   = qf_sin(qf_from_double(1.0));
    qfloat_t qexp2   = qf_exp(qf_from_double(2.0));
    qfloat_t cos_exp = qf_mul(qcos1, qexp2);
    qfloat_t sin_exp = qf_mul(qsin1, qexp2);

    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂x",    expr_eval_qf(df_dx), cos_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂y",    expr_eval_qf(df_dy), sin_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂²(sin(x)exp(y))/∂x∂y", expr_eval_qf(d2f),   cos_exp);

    expr_free(d2f);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f);
    expr_free(expy);
    expr_free(sinx);
    expr_free(y);
    expr_free(x);
}

/* Cross-partial symmetry: ∂²f/∂x∂y == ∂²f/∂y∂x for f = x*y + x²*y */
static void test_partial_symmetry(void)
{
    expr_t *x   = test_expr_new_var_d(2.0);
    expr_t *y   = test_expr_new_var_d(3.0);
    expr_t *xy  = expr_mul(x, y);
    expr_t *x2  = expr_pow_d(x, 2.0);
    expr_t *x2y = expr_mul(x2, y);
    expr_t *f   = expr_add(xy, x2y);  /* f = xy + x²y */

    expr_t *dxy  = expr_create_2nd_deriv(f, x, y);
    expr_t *dyx  = expr_create_2nd_deriv(f, y, x);

    /* ∂²f/∂x∂y = x + 2x*1 ... actually ∂f/∂x = y + 2xy, ∂²f/∂x∂y = 1 + 2x = 5 at x=2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂x∂y at x=2,y=3", expr_eval_qf(dxy), qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂y∂x at x=2,y=3", expr_eval_qf(dyx), qf_from_double(5.0));

    expr_free(dyx);
    expr_free(dxy);
    expr_free(f);
    expr_free(x2y);
    expr_free(x2);
    expr_free(xy);
    expr_free(y);
    expr_free(x);
}

/* expr_get_deriv returns a borrowed pointer; verify it evaluates correctly
   and that repeated calls return the cached result (same pointer) */
static void test_partial_get_borrowed(void)
{
    expr_t *x = test_expr_new_var_d(4.0);
    expr_t *y = test_expr_new_var_d(5.0);
    expr_t *f = expr_mul(x, y);  /* f = x*y */

    const expr_t *p1 = expr_get_deriv(f, x);
    const expr_t *p2 = expr_get_deriv(f, x);  /* should be cached */

    if (p1 != p2) {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " expr_get_deriv not cached %s:%d:1\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " expr_get_deriv returns cached pointer\n");
    }

    check_q_at(__FILE__, __LINE__, 1, "expr_get_deriv(x*y, x) = y = 5", expr_eval_qf(p1), qf_from_double(5.0));

    expr_free(f);
    expr_free(y);
    expr_free(x);
}

/* Symbolic expression-style string output for partial derivative nodes */
static void test_partial_to_string(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    expr_t *x = test_expr_new_named_var_d(2.0, "x");
    expr_t *y = test_expr_new_named_var_d(3.0, "y");

    /* f = xy */
    expr_t *f        = expr_mul(x, y);
    expr_t *df_dx    = expr_create_deriv(f, x);   /* simplifies to y */
    expr_t *df_dy    = expr_create_deriv(f, y);   /* simplifies to x */
    expr_t *d2f_dxdy = expr_create_2nd_deriv(f, x, y); /* simplifies to 1 */

    char *s;

    s = expr_to_string(f, style_EXPRESSION);
    to_string_pass("f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    if (!str_eq(s, "{ xy | x = 2, y = 3 }"))
        to_string_fail(__FILE__, __LINE__, 1, "f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    free(s);

    s = expr_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ y | y = 3 }"))
        to_string_pass("∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    free(s);

    s = expr_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x | x = 2 }"))
        to_string_pass("∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    free(s);

    s = expr_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "1"))
        to_string_pass("∂²(xy)/∂x∂y (EXPR)", s, "1");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(xy)/∂x∂y (EXPR)", s, "1");
    free(s);

    /* g = x² + xy + y² */
    expr_t *x2 = expr_pow_d(x, 2.0);
    expr_t *xy = expr_mul(x, y);
    expr_t *y2 = expr_pow_d(y, 2.0);
    expr_t *t0 = expr_add(x2, xy);
    expr_t *g  = expr_add(t0, y2);

    expr_t *dg_dx = expr_create_deriv(g, x);  /* 2x + y */
    expr_t *dg_dy = expr_create_deriv(g, y);  /* x + 2y */

    s = expr_to_string(dg_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x + y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    free(s);

    s = expr_to_string(dg_dy, style_EXPRESSION);
    if (str_eq(s, "{ x + 2y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    free(s);

    expr_free(dg_dy); expr_free(dg_dx);
    expr_free(g); expr_free(t0); expr_free(y2); expr_free(xy); expr_free(x2);
    expr_free(d2f_dxdy); expr_free(df_dy); expr_free(df_dx); expr_free(f);
    expr_free(y); expr_free(x);
}

/* f(x,y) = sin(x)·exp(y) — symbolic output checks with elementary functions */
static void test_partial_to_string_functions(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    expr_t *x    = test_expr_new_named_var_d(1.0, "x");
    expr_t *y    = test_expr_new_named_var_d(2.0, "y");
    expr_t *sinx = expr_sin(x);
    expr_t *expy = expr_exp(y);
    expr_t *f    = expr_mul(sinx, expy);

    expr_t *df_dx    = expr_create_deriv(f, x);   /* cos(x)·exp(y)  */
    expr_t *df_dy    = expr_create_deriv(f, y);   /* sin(x)·exp(y) = f */
    expr_t *d2f_dx2  = expr_create_2nd_deriv(f, x, x); /* -sin(x)·exp(y) */
    expr_t *d2f_dxdy = expr_create_2nd_deriv(f, x, y); /* cos(x)·exp(y)  */

    char *s;

    s = expr_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    expr_free(d2f_dxdy); expr_free(d2f_dx2); expr_free(df_dy); expr_free(df_dx);
    expr_free(f); expr_free(expy); expr_free(sinx); expr_free(y); expr_free(x);
}

/* f(x,y) = log(x² + y²)  — harmonic function; all partials involve the
   denominator (x² + y²) and its powers, exercising quotient-rule simplification */
static void test_partial_to_string_log_r2(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    expr_t *x   = test_expr_new_named_var_d(1.0, "x");
    expr_t *y   = test_expr_new_named_var_d(2.0, "y");
    expr_t *x2  = expr_pow_d(x, 2.0);
    expr_t *y2  = expr_pow_d(y, 2.0);
    expr_t *sum = expr_add(x2, y2);
    expr_t *f   = expr_log(sum);

    expr_t *df_dx    = expr_create_deriv(f, x);
    expr_t *df_dy    = expr_create_deriv(f, y);
    expr_t *d2f_dx2  = expr_create_2nd_deriv(f, x, x);
    expr_t *d2f_dxdy = expr_create_2nd_deriv(f, x, y);

    char *s;

    s = expr_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ ln(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("ln(x²+y²) (EXPR)", s, "{ ln(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "ln(x²+y²) (EXPR)", s, "{ ln(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x/(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ 2y/(x² + y²) | y = 2, x = 1 }"))
        to_string_pass("∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    free(s);

    s = expr_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ 2·(-x² + y²)/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x² (EXPR)", s, "{ 2·(-x² + y²)/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x² (EXPR)", s, "{ 2·(-x² + y²)/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    expr_free(d2f_dxdy); expr_free(d2f_dx2); expr_free(df_dy); expr_free(df_dx);
    expr_free(f); expr_free(sum); expr_free(y2); expr_free(x2); expr_free(y); expr_free(x);
}

/* f(x,y) = sin(xy) + x·ln(y)  — chain rule through a product argument plus
   a cross term; the mixed second partial -xy·sin(xy) + cos(xy) + 1/y exercises
   several simplification paths simultaneously */
static void test_partial_to_string_sin_xy(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    expr_t *x     = test_expr_new_named_var_d(1.0, "x");
    expr_t *y     = test_expr_new_named_var_d(2.0, "y");
    expr_t *xy    = expr_mul(x, y);
    expr_t *sinxy = expr_sin(xy);
    expr_t *logy  = expr_log(y);
    expr_t *xlogy = expr_mul(x, logy);
    expr_t *f     = expr_add(sinxy, xlogy);

    expr_t *df_dx    = expr_create_deriv(f, x);
    expr_t *df_dy    = expr_create_deriv(f, y);
    expr_t *d2f_dx2  = expr_create_2nd_deriv(f, x, x);
    expr_t *d2f_dxdy = expr_create_2nd_deriv(f, x, y);

    char *s;

    s = expr_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(xy) + x·ln(y) | x = 1, y = 2 }"))
        to_string_pass("sin(xy)+x·ln(y) (EXPR)", s, "{ sin(xy) + x·ln(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(xy)+x·ln(y) (EXPR)", s, "{ sin(xy) + x·ln(y) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ ln(y) + y·cos(xy) | y = 2, x = 1 }"))
        to_string_pass("∂(sin(xy)+x·ln(y))/∂x (EXPR)", s, "{ ln(y) + y·cos(xy) | y = 2, x = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·ln(y))/∂x (EXPR)", s, "{ ln(y) + y·cos(xy) | y = 2, x = 1 }");
    free(s);

    s = expr_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x·cos(xy) + x/y | x = 1, y = 2 }"))
        to_string_pass("∂(sin(xy)+x·ln(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·ln(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -y²·sin(xy) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·ln(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·ln(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    free(s);

    s = expr_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ cos(xy) - xy·sin(xy) + 1/y | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·ln(y))/∂x∂y (EXPR)", s, "{ cos(xy) - xy·sin(xy) + 1/y | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·ln(y))/∂x∂y (EXPR)", s, "{ cos(xy) - xy·sin(xy) + 1/y | x = 1, y = 2 }");
    free(s);

    expr_free(d2f_dxdy); expr_free(d2f_dx2); expr_free(df_dy); expr_free(df_dx);
    expr_free(f); expr_free(xlogy); expr_free(logy); expr_free(sinxy); expr_free(xy);
    expr_free(y); expr_free(x);
}

void test_partial_derivatives(void)
{
    TEST_RUN_SUBTEST(test_partial_xy_product, NULL);
    TEST_RUN_SUBTEST(test_partial_poly, NULL);
    TEST_RUN_SUBTEST(test_partial_sin_exp, NULL);
    TEST_RUN_SUBTEST(test_partial_symmetry, NULL);
    TEST_RUN_SUBTEST(test_partial_get_borrowed, NULL);
    TEST_RUN_SUBTEST(test_partial_to_string, NULL);
    TEST_RUN_SUBTEST(test_partial_to_string_functions, NULL);
    TEST_RUN_SUBTEST(test_partial_to_string_log_r2, NULL);
    TEST_RUN_SUBTEST(test_partial_to_string_sin_xy, NULL);
}

/* ------------------------------------------------------------------------- */
/* Precision / cache regressions                                             */
/* ------------------------------------------------------------------------- */
