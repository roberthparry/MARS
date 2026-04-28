#include "test_dval.h"

static void test_partial_xy_product(void)
{
    dval_t *x = dv_new_var_d(2.0);
    dval_t *y = dv_new_var_d(3.0);
    dval_t *f = dv_mul(x, y);

    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);
    dval_t *d2f   = dv_create_2nd_deriv(f, x, y);

    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂x at x=2,y=3", dv_eval_qf(df_dx), qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "∂(x*y)/∂y at x=2,y=3", dv_eval_qf(df_dy), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²(x*y)/∂x∂y",         dv_eval_qf(d2f),   qf_from_double(1.0));

    dv_free(d2f);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(y);
    dv_free(x);
}

/* f(x,y) = x² + y³  →  ∂f/∂x = 2x,  ∂f/∂y = 3y²,  ∂²f/∂x² = 2,  ∂²f/∂y² = 6y */
static void test_partial_poly(void)
{
    dval_t *x  = dv_new_var_d(2.0);
    dval_t *y  = dv_new_var_d(3.0);
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *y3 = dv_pow_d(y, 3.0);
    dval_t *f  = dv_add(x2, y3);

    dval_t *df_dx   = dv_create_deriv(f, x);
    dval_t *df_dy   = dv_create_deriv(f, y);
    dval_t *d2f_dx2 = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dy2 = dv_create_2nd_deriv(f, y, y);

    /* at x=2: ∂f/∂x = 2*2 = 4 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂x at x=2",   dv_eval_qf(df_dx),   qf_from_double(4.0));
    /* at y=3: ∂f/∂y = 3*9 = 27 */
    check_q_at(__FILE__, __LINE__, 1, "∂(x²+y³)/∂y at y=3",   dv_eval_qf(df_dy),   qf_from_double(27.0));
    /* ∂²f/∂x² = 2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂x² = 2",    dv_eval_qf(d2f_dx2), qf_from_double(2.0));
    /* at y=3: ∂²f/∂y² = 6y = 18 */
    check_q_at(__FILE__, __LINE__, 1, "∂²(x²+y³)/∂y² at y=3", dv_eval_qf(d2f_dy2), qf_from_double(18.0));

    dv_free(d2f_dy2);
    dv_free(d2f_dx2);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(y3);
    dv_free(x2);
    dv_free(y);
    dv_free(x);
}

/* f(x,y) = sin(x) * exp(y)  →  ∂f/∂x = cos(x)*exp(y),  ∂f/∂y = sin(x)*exp(y),
   ∂²f/∂x∂y = cos(x)*exp(y) */
static void test_partial_sin_exp(void)
{
    dval_t *x    = dv_new_var_d(1.0);
    dval_t *y    = dv_new_var_d(2.0);
    dval_t *sinx = dv_sin(x);
    dval_t *expy = dv_exp(y);
    dval_t *f    = dv_mul(sinx, expy);

    dval_t *df_dx = dv_create_deriv(f, x);
    dval_t *df_dy = dv_create_deriv(f, y);
    dval_t *d2f   = dv_create_2nd_deriv(f, x, y);

    qfloat_t qcos1   = qf_cos(qf_from_double(1.0));
    qfloat_t qsin1   = qf_sin(qf_from_double(1.0));
    qfloat_t qexp2   = qf_exp(qf_from_double(2.0));
    qfloat_t cos_exp = qf_mul(qcos1, qexp2);
    qfloat_t sin_exp = qf_mul(qsin1, qexp2);

    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂x",    dv_eval_qf(df_dx), cos_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂(sin(x)exp(y))/∂y",    dv_eval_qf(df_dy), sin_exp);
    check_q_at(__FILE__, __LINE__, 1, "∂²(sin(x)exp(y))/∂x∂y", dv_eval_qf(d2f),   cos_exp);

    dv_free(d2f);
    dv_free(df_dy);
    dv_free(df_dx);
    dv_free(f);
    dv_free(expy);
    dv_free(sinx);
    dv_free(y);
    dv_free(x);
}

/* Cross-partial symmetry: ∂²f/∂x∂y == ∂²f/∂y∂x for f = x*y + x²*y */
static void test_partial_symmetry(void)
{
    dval_t *x   = dv_new_var_d(2.0);
    dval_t *y   = dv_new_var_d(3.0);
    dval_t *xy  = dv_mul(x, y);
    dval_t *x2  = dv_pow_d(x, 2.0);
    dval_t *x2y = dv_mul(x2, y);
    dval_t *f   = dv_add(xy, x2y);  /* f = xy + x²y */

    dval_t *dxy  = dv_create_2nd_deriv(f, x, y);
    dval_t *dyx  = dv_create_2nd_deriv(f, y, x);

    /* ∂²f/∂x∂y = x + 2x*1 ... actually ∂f/∂x = y + 2xy, ∂²f/∂x∂y = 1 + 2x = 5 at x=2 */
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂x∂y at x=2,y=3", dv_eval_qf(dxy), qf_from_double(5.0));
    check_q_at(__FILE__, __LINE__, 1, "∂²f/∂y∂x at x=2,y=3", dv_eval_qf(dyx), qf_from_double(5.0));

    dv_free(dyx);
    dv_free(dxy);
    dv_free(f);
    dv_free(x2y);
    dv_free(x2);
    dv_free(xy);
    dv_free(y);
    dv_free(x);
}

/* dv_get_deriv returns a borrowed pointer; verify it evaluates correctly
   and that repeated calls return the cached result (same pointer) */
static void test_partial_get_borrowed(void)
{
    dval_t *x = dv_new_var_d(4.0);
    dval_t *y = dv_new_var_d(5.0);
    dval_t *f = dv_mul(x, y);  /* f = x*y */

    const dval_t *p1 = dv_get_deriv(f, x);
    const dval_t *p2 = dv_get_deriv(f, x);  /* should be cached */

    if (p1 != p2) {
        printf(C_BOLD C_RED "FAIL" C_RESET
               " dv_get_deriv not cached %s:%d:1\n", __FILE__, __LINE__);
        TEST_FAIL();
    } else {
        printf(C_BOLD C_GREEN "PASS" C_RESET " dv_get_deriv returns cached pointer\n");
    }

    check_q_at(__FILE__, __LINE__, 1, "dv_get_deriv(x*y, x) = y = 5", dv_eval_qf(p1), qf_from_double(5.0));

    dv_free(f);
    dv_free(y);
    dv_free(x);
}

/* Symbolic expression-style string output for partial derivative nodes */
static void test_partial_to_string(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x = dv_new_named_var_d(2.0, "x");
    dval_t *y = dv_new_named_var_d(3.0, "y");

    /* f = xy */
    dval_t *f        = dv_mul(x, y);
    dval_t *df_dx    = dv_create_deriv(f, x);   /* simplifies to y */
    dval_t *df_dy    = dv_create_deriv(f, y);   /* simplifies to x */
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y); /* simplifies to 1 */

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    to_string_pass("f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    if (!str_eq(s, "{ xy | x = 2, y = 3 }"))
        to_string_fail(__FILE__, __LINE__, 1, "f = xy (EXPR)", s, "{ xy | x = 2, y = 3 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ y | y = 3 }"))
        to_string_pass("∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂x (EXPR)", s, "{ y | y = 3 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x | x = 2 }"))
        to_string_pass("∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(xy)/∂y (EXPR)", s, "{ x | x = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ 1 }"))
        to_string_pass("∂²(xy)/∂x∂y (EXPR)", s, "{ 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(xy)/∂x∂y (EXPR)", s, "{ 1 }");
    free(s);

    /* g = x² + xy + y² */
    dval_t *x2 = dv_pow_d(x, 2.0);
    dval_t *xy = dv_mul(x, y);
    dval_t *y2 = dv_pow_d(y, 2.0);
    dval_t *t0 = dv_add(x2, xy);
    dval_t *g  = dv_add(t0, y2);

    dval_t *dg_dx = dv_create_deriv(g, x);  /* 2x + y */
    dval_t *dg_dy = dv_create_deriv(g, y);  /* x + 2y */

    s = dv_to_string(dg_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x + y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂x (EXPR)", s, "{ 2x + y | x = 2, y = 3 }");
    free(s);

    s = dv_to_string(dg_dy, style_EXPRESSION);
    if (str_eq(s, "{ x + 2y | x = 2, y = 3 }"))
        to_string_pass("∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(x²+xy+y²)/∂y (EXPR)", s, "{ x + 2y | x = 2, y = 3 }");
    free(s);

    dv_free(dg_dy); dv_free(dg_dx);
    dv_free(g); dv_free(t0); dv_free(y2); dv_free(xy); dv_free(x2);
    dv_free(d2f_dxdy); dv_free(df_dy); dv_free(df_dx); dv_free(f);
    dv_free(y); dv_free(x);
}

/* f(x,y) = sin(x)·exp(y) — symbolic output checks with elementary functions */
static void test_partial_to_string_functions(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x    = dv_new_named_var_d(1.0, "x");
    dval_t *y    = dv_new_named_var_d(2.0, "y");
    dval_t *sinx = dv_sin(x);
    dval_t *expy = dv_exp(y);
    dval_t *f    = dv_mul(sinx, expy);

    dval_t *df_dx    = dv_create_deriv(f, x);   /* cos(x)·exp(y)  */
    dval_t *df_dy    = dv_create_deriv(f, y);   /* sin(x)·exp(y) = f */
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x); /* -sin(x)·exp(y) */
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y); /* cos(x)·exp(y)  */

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(x)·exp(y) (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂x (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(x)·exp(y))/∂y (EXPR)", s, "{ sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -sin(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x² (EXPR)", s, "{ -sin(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ cos(x)·exp(y) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(x)·exp(y))/∂x∂y (EXPR)", s, "{ cos(x)·exp(y) | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(expy); dv_free(sinx); dv_free(y); dv_free(x);
}

/* f(x,y) = log(x² + y²)  — harmonic function; all partials involve the
   denominator (x² + y²) and its powers, exercising quotient-rule simplification */
static void test_partial_to_string_log_r2(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x   = dv_new_named_var_d(1.0, "x");
    dval_t *y   = dv_new_named_var_d(2.0, "y");
    dval_t *x2  = dv_pow_d(x, 2.0);
    dval_t *y2  = dv_pow_d(y, 2.0);
    dval_t *sum = dv_add(x2, y2);
    dval_t *f   = dv_log(sum);

    dval_t *df_dx    = dv_create_deriv(f, x);
    dval_t *df_dy    = dv_create_deriv(f, y);
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y);

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ log(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("log(x²+y²) (EXPR)", s, "{ log(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "log(x²+y²) (EXPR)", s, "{ log(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ 2x/(x² + y²) | x = 1, y = 2 }"))
        to_string_pass("∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂x (EXPR)", s, "{ 2x/(x² + y²) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ 2y/(x² + y²) | y = 2, x = 1 }"))
        to_string_pass("∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂log(x²+y²)/∂y (EXPR)", s, "{ 2y/(x² + y²) | y = 2, x = 1 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x² (EXPR)", s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x² (EXPR)", s, "{ (-2x² + 2y²)/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }"))
        to_string_pass("∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²log(x²+y²)/∂x∂y (EXPR)", s, "{ -4xy/(x² + y²)² | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(sum); dv_free(y2); dv_free(x2); dv_free(y); dv_free(x);
}

/* f(x,y) = sin(xy) + x·log(y)  — chain rule through a product argument plus
   a cross term; the mixed second partial -xy·sin(xy) + cos(xy) + 1/y exercises
   several simplification paths simultaneously */
static void test_partial_to_string_sin_xy(void)
{
    fprintf(stderr, "\n  [%s]\n", __func__);
    dval_t *x     = dv_new_named_var_d(1.0, "x");
    dval_t *y     = dv_new_named_var_d(2.0, "y");
    dval_t *xy    = dv_mul(x, y);
    dval_t *sinxy = dv_sin(xy);
    dval_t *logy  = dv_log(y);
    dval_t *xlogy = dv_mul(x, logy);
    dval_t *f     = dv_add(sinxy, xlogy);

    dval_t *df_dx    = dv_create_deriv(f, x);
    dval_t *df_dy    = dv_create_deriv(f, y);
    dval_t *d2f_dx2  = dv_create_2nd_deriv(f, x, x);
    dval_t *d2f_dxdy = dv_create_2nd_deriv(f, x, y);

    char *s;

    s = dv_to_string(f, style_EXPRESSION);
    if (str_eq(s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }"))
        to_string_pass("sin(xy)+x·log(y) (EXPR)", s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "sin(xy)+x·log(y) (EXPR)", s, "{ sin(xy) + x·log(y) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(df_dx, style_EXPRESSION);
    if (str_eq(s, "{ log(y) + y·cos(xy) | y = 2, x = 1 }"))
        to_string_pass("∂(sin(xy)+x·log(y))/∂x (EXPR)", s, "{ log(y) + y·cos(xy) | y = 2, x = 1 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·log(y))/∂x (EXPR)", s, "{ log(y) + y·cos(xy) | y = 2, x = 1 }");
    free(s);

    s = dv_to_string(df_dy, style_EXPRESSION);
    if (str_eq(s, "{ x·cos(xy) + x/y | x = 1, y = 2 }"))
        to_string_pass("∂(sin(xy)+x·log(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂(sin(xy)+x·log(y))/∂y (EXPR)", s, "{ x·cos(xy) + x/y | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dx2, style_EXPRESSION);
    if (str_eq(s, "{ -y²·sin(xy) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·log(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·log(y))/∂x² (EXPR)", s, "{ -y²·sin(xy) | x = 1, y = 2 }");
    free(s);

    s = dv_to_string(d2f_dxdy, style_EXPRESSION);
    if (str_eq(s, "{ cos(xy) + 1/y - xy·sin(xy) | x = 1, y = 2 }"))
        to_string_pass("∂²(sin(xy)+x·log(y))/∂x∂y (EXPR)", s, "{ cos(xy) + 1/y - xy·sin(xy) | x = 1, y = 2 }");
    else
        to_string_fail(__FILE__, __LINE__, 1, "∂²(sin(xy)+x·log(y))/∂x∂y (EXPR)", s, "{ cos(xy) + 1/y - xy·sin(xy) | x = 1, y = 2 }");
    free(s);

    dv_free(d2f_dxdy); dv_free(d2f_dx2); dv_free(df_dy); dv_free(df_dx);
    dv_free(f); dv_free(xlogy); dv_free(logy); dv_free(sinxy); dv_free(xy);
    dv_free(y); dv_free(x);
}

void test_partial_derivatives(void)
{
    RUN_TEST(test_partial_xy_product,   __func__);
    RUN_TEST(test_partial_poly,         __func__);
    RUN_TEST(test_partial_sin_exp,      __func__);
    RUN_TEST(test_partial_symmetry,     __func__);
    RUN_TEST(test_partial_get_borrowed, __func__);
    RUN_TEST(test_partial_to_string,            __func__);
    RUN_TEST(test_partial_to_string_functions,  __func__);
    RUN_TEST(test_partial_to_string_log_r2,     __func__);
    RUN_TEST(test_partial_to_string_sin_xy,     __func__);
}

/* ------------------------------------------------------------------------- */
/* Precision / cache regressions                                             */
/* ------------------------------------------------------------------------- */
