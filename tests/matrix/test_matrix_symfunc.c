#include "test_matrix.h"

/* Borrow immortal constants in mutable matrix fixture arrays without changing the public API. */
#define EXPR_ZERO ((expr_t *)EXPR_ZERO)
#define EXPR_ONE ((expr_t *)EXPR_ONE)

static void test_symfunc_jordan_special(void)
{
    expr_t *x = test_expr_new_named_var_d(0.2, "x");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *vals[9] = {x, EXPR_ZERO, EXPR_ZERO, one, x, EXPR_ZERO, EXPR_ZERO, one, x};
    matrix_t *T = mat_create_expr(3, 3, vals);
    matrix_t *R = NULL;
    matrix_t *G = NULL;
    matrix_t *W = NULL;
    expr_t *v = NULL;

    R = mat_erf(T);
    G = mat_gamma(T);
    W = mat_lambert_w0(T);

    check_bool("mat_erf(expr 3x3 lower Jordan) not NULL", R != NULL);
    check_bool("mat_gamma(expr 3x3 lower Jordan) not NULL", G != NULL);
    check_bool("mat_lambert_w0(expr 3x3 lower Jordan) not NULL", W != NULL);

    if (T)
        print_mdv("T", T);
    if (R)
        print_mdv("erf(T)", R);
    if (G)
        print_mdv("gamma(T)", G);
    if (W)
        print_mdv("lambert_w0(T)", W);

    if (R) {
        check_bool("erf(T) preserves lower-triangular structure", mat_is_lower_triangular(R));
        mat_get(R, 0, 0, &v);
        check_expr_text_contains("erf(T)[0,0] stays symbolic in x", v, "erf(x)");
        test_expr_set_val_d(x, 0.3);
        check_d("erf(T)[0,0] tracks x", expr_eval_d(v), erf(0.3), 1e-12);
    }

    if (G) {
        check_bool("gamma(T) preserves lower-triangular structure", mat_is_lower_triangular(G));
        mat_get(G, 0, 0, &v);
        test_expr_set_val_d(x, 3.0);
        check_d("gamma(T)[0,0] tracks x", expr_eval_d(v), tgamma(3.0), 1e-12);
    }

    if (W) {
        check_bool("lambert_w0(T) preserves lower-triangular structure", mat_is_lower_triangular(W));
        mat_get(W, 0, 0, &v);
        check_expr_text_contains("lambert_w0(T)[0,0] stays symbolic in x", v, "W₀");
        test_expr_set_val_d(x, 0.1);
        check_bool("lambert_w0(T)[0,0] numerically finite", isfinite(expr_eval_d(v)));
    }

    mat_free(T);
    mat_free(R);
    mat_free(G);
    mat_free(W);
    expr_free(x);
    expr_free(one);
}

static void test_symfunc_jordan_exp(void)
{
    expr_t *x = test_expr_new_named_var_d(1.5, "x");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *vals[9] = {x, EXPR_ZERO, EXPR_ZERO, one, x, EXPR_ZERO, EXPR_ZERO, one, x};
    matrix_t *T = mat_create_expr(3, 3, vals);
    matrix_t *E = mat_exp(T);
    expr_t *v = NULL;

    check_bool("mat_exp(expr 3x3 lower Jordan) not NULL", E != NULL);
    if (T)
        print_mdv("T", T);
    if (E)
        print_mdv("exp(T)", E);

    if (E) {
        check_bool("exp(lower Jordan) preserves lower-triangular structure", mat_is_lower_triangular(E));
        mat_get(E, 0, 0, &v);
        check_d("exp(lower Jordan)[0,0] = exp(1.5)", expr_eval_d(v), exp(1.5), 1e-12);
        mat_get(E, 1, 0, &v);
        check_d("exp(lower Jordan)[1,0] = exp(1.5)", expr_eval_d(v), exp(1.5), 1e-12);
        mat_get(E, 2, 0, &v);
        check_d("exp(lower Jordan)[2,0] = exp(1.5)/2", expr_eval_d(v), 0.5 * exp(1.5), 1e-12);
        mat_get(E, 2, 0, &v);
        check_expr_text_contains("exp(lower Jordan)[2,0] stays symbolic in x", v, "exp(x)");
        test_expr_set_val_d(x, 2.0);
        mat_get(E, 2, 0, &v);
        check_d("exp(lower Jordan)[2,0] tracks x", expr_eval_d(v), 0.5 * exp(2.0), 1e-12);
    }

    mat_free(T);
    mat_free(E);
    expr_free(x);
    expr_free(one);
}

static void test_symfunc_dense_numeric_fallback(void)
{
    expr_t *x = test_expr_new_named_var_d(1.0, "x");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *vals[9] = {x, one, EXPR_ZERO, one, two, one, EXPR_ZERO, one, x};
    matrix_t *A = mat_create_expr(3, 3, vals);
    matrix_t *Aqc = NULL;
    matrix_t *Eqc = NULL;
    matrix_t *Lqc = NULL;
    matrix_t *Sqc = NULL;
    matrix_t *Gqc = NULL;

    print_mdv("A", A);
    check_bool("mat_exp(expr dense 3x3) currently unsupported", mat_exp(A) == NULL);
    check_bool("mat_log(expr dense 3x3) currently unsupported", mat_log(A) == NULL);
    check_bool("mat_sin(expr dense 3x3) currently unsupported", mat_sin(A) == NULL);
    check_bool("mat_gamma(expr dense 3x3) currently unsupported", mat_gamma(A) == NULL);

    Aqc = test_mat_evaluate_complex(A);
    check_bool("test_mat_evaluate_complex(expr dense 3x3) not NULL", Aqc != NULL);

    if (Aqc) {
        Eqc = mat_exp(Aqc);
        Lqc = mat_log(Aqc);
        Sqc = mat_sin(Aqc);
        Gqc = mat_gamma(Aqc);
    }

    check_bool("manual qc exp(expr dense 3x3) not NULL", Eqc != NULL);
    check_bool("manual qc log(expr dense 3x3) not NULL", Lqc != NULL);
    check_bool("manual qc sin(expr dense 3x3) not NULL", Sqc != NULL);
    check_bool("manual qc gamma(expr dense 3x3) not NULL", Gqc != NULL);

    check_bool("manual qc exp(expr dense 3x3) -> MAT_TYPE_NUMBER",
               Eqc != NULL && mat_typeof(Eqc) == MAT_TYPE_NUMBER);
    check_bool("manual qc log(expr dense 3x3) -> MAT_TYPE_NUMBER",
               Lqc != NULL && mat_typeof(Lqc) == MAT_TYPE_NUMBER);
    check_bool("manual qc sin(expr dense 3x3) -> MAT_TYPE_NUMBER",
               Sqc != NULL && mat_typeof(Sqc) == MAT_TYPE_NUMBER);
    check_bool("manual qc gamma(expr dense 3x3) -> MAT_TYPE_NUMBER",
               Gqc != NULL && mat_typeof(Gqc) == MAT_TYPE_NUMBER);

    mat_free(Aqc);
    mat_free(Eqc);
    mat_free(Lqc);
    mat_free(Sqc);
    mat_free(Gqc);
    mat_free(A);
    expr_free(x);
    expr_free(one);
    expr_free(two);
}

static void test_symfunc_cubic_three(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[0 x 0][x 0 x][0 x 0]]", &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;
    double r;

    check_bool("dense expr cubic-linear 3x3 input not NULL", A != NULL);
    if (bindings) {
        check_bool("dense expr cubic-linear 3x3 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("dense expr cubic-linear 3x3 exp not NULL", E != NULL);
    check_bool("dense expr cubic-linear 3x3 sin not NULL", S != NULL);

    r = sqrt(8.0);
    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(dense cubic-linear 3x3)[0,0]", expr_eval_d(v), 0.5 * (cosh(r) + 1.0), 1e-12);
        check_expr_text_contains("exp(dense cubic-linear 3x3)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 1, &v);
        check_d("exp(dense cubic-linear 3x3)[0,1]", expr_eval_d(v), sinh(r) / r * 2.0, 1e-12);
        mat_get(E, 0, 2, &v);
        check_d("exp(dense cubic-linear 3x3)[0,2]", expr_eval_d(v), 0.5 * (cosh(r) - 1.0), 1e-12);
    }

    if (S) {
        mat_get(S, 0, 0, &v);
        check_d("sin(dense cubic-linear 3x3)[0,0]", expr_eval_d(v), 0.0, 1e-12);
        mat_get(S, 0, 1, &v);
        check_d("sin(dense cubic-linear 3x3)[0,1]", expr_eval_d(v), sin(r) / r * 2.0, 1e-12);
        check_expr_text_contains("sin(dense cubic-linear 3x3)[0,1] stays symbolic", v, "sin");
        mat_get(S, 0, 2, &v);
        check_d("sin(dense cubic-linear 3x3)[0,2]", expr_eval_d(v), 0.0, 1e-12);
    }

    if (bindings) {
        check_bool("dense expr cubic-linear 3x3 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
    }

    r = sqrt(18.0);
    if (E) {
        mat_get(E, 0, 1, &v);
        check_d("exp(dense cubic-linear 3x3)[0,1] tracks x", expr_eval_d(v), sinh(r) / r * 3.0, 1e-12);
    }
    if (S) {
        mat_get(S, 1, 0, &v);
        check_d("sin(dense cubic-linear 3x3)[1,0] tracks x", expr_eval_d(v), sin(r) / r * 3.0, 1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_quadratic_three(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[0 x x][x 0 x][x x 0]]", &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;

    check_bool("dense expr quadratic 3x3 input not NULL", A != NULL);
    if (bindings) {
        check_bool("dense expr quadratic 3x3 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("dense expr quadratic 3x3 exp not NULL", E != NULL);
    check_bool("dense expr quadratic 3x3 sin not NULL", S != NULL);

    if (A)
        print_mdv("A (dense quadratic 3x3)", A);
    if (E)
        print_mdv("exp(A)", E);
    if (S)
        print_mdv("sin(A)", S);

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(dense quadratic 3x3)[0,0]", expr_eval_d(v), (exp(4.0) + 2.0 * exp(-2.0)) / 3.0, 1e-12);
        check_expr_text_contains("exp(dense quadratic 3x3)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 1, &v);
        check_d("exp(dense quadratic 3x3)[0,1]", expr_eval_d(v), (exp(4.0) - exp(-2.0)) / 3.0, 1e-12);
    }

    if (S) {
        mat_get(S, 0, 0, &v);
        check_d("sin(dense quadratic 3x3)[0,0]", expr_eval_d(v), (sin(4.0) - 2.0 * sin(2.0)) / 3.0, 1e-12);
        check_expr_text_contains("sin(dense quadratic 3x3)[0,0] stays symbolic", v, "sin");
        mat_get(S, 0, 1, &v);
        check_d("sin(dense quadratic 3x3)[0,1]", expr_eval_d(v), (sin(4.0) + sin(2.0)) / 3.0, 1e-12);
    }

    if (bindings) {
        check_bool("dense expr quadratic 3x3 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
    }

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(dense quadratic 3x3)[0,0] tracks x", expr_eval_d(v), (exp(6.0) + 2.0 * exp(-3.0)) / 3.0,
                1e-12);
    }
    if (S) {
        mat_get(S, 0, 1, &v);
        check_d("sin(dense quadratic 3x3)[0,1] tracks x", expr_eval_d(v), (sin(6.0) + sin(3.0)) / 3.0, 1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_uniform_five(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[x 1 1 1 1]"
                                       "[1 x 1 1 1]"
                                       "[1 1 x 1 1]"
                                       "[1 1 1 x 1]"
                                       "[1 1 1 1 x]]",
                                       &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;

    check_bool("uniform dense expr 5x5 input not NULL", A != NULL);
    if (bindings) {
        check_bool("uniform dense expr 5x5 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("uniform dense expr 5x5 exp not NULL", E != NULL);
    check_bool("uniform dense expr 5x5 sin not NULL", S != NULL);

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(uniform dense 5x5)[0,0]", expr_eval_d(v), (4.0 * exp(1.0) + exp(6.0)) / 5.0, 1e-12);
        check_expr_text_contains("exp(uniform dense 5x5)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 1, &v);
        check_d("exp(uniform dense 5x5)[0,1]", expr_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
        mat_get(E, 3, 4, &v);
        check_d("exp(uniform dense 5x5)[3,4]", expr_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
    }

    if (S) {
        mat_get(S, 0, 0, &v);
        check_d("sin(uniform dense 5x5)[0,0]", expr_eval_d(v), (4.0 * sin(1.0) + sin(6.0)) / 5.0, 1e-12);
        check_expr_text_contains("sin(uniform dense 5x5)[0,0] stays symbolic", v, "sin");
        mat_get(S, 0, 2, &v);
        check_d("sin(uniform dense 5x5)[0,2]", expr_eval_d(v), (sin(6.0) - sin(1.0)) / 5.0, 1e-12);
    }

    if (bindings) {
        check_bool("uniform dense expr 5x5 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
    }

    if (E) {
        mat_get(E, 0, 1, &v);
        check_d("exp(uniform dense 5x5)[0,1] tracks x", expr_eval_d(v), (exp(7.0) - exp(2.0)) / 5.0, 1e-12);
    }
    if (S) {
        mat_get(S, 0, 0, &v);
        check_d("sin(uniform dense 5x5)[0,0] tracks x", expr_eval_d(v), (4.0 * sin(2.0) + sin(7.0)) / 5.0, 1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_rank_one_four(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("(7, x, 2, 1;"
                                       " 10, 2*x + 2, 4, 2;"
                                       " 15, 3*x, 8, 3;"
                                       " 20, 4*x, 8, 6)",
                                       &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;
    double cexp;
    double csin;

    check_bool("rank-one perturbation dense expr 4x4 input not NULL", A != NULL);
    if (bindings) {
        check_bool("rank-one perturbation dense expr 4x4 set x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("rank-one perturbation dense expr 4x4 exp not NULL", E != NULL);
    check_bool("rank-one perturbation dense expr 4x4 sin not NULL", S != NULL);

    cexp = (exp(23.0) - exp(2.0)) / 21.0;
    csin = (sin(23.0) - sin(2.0)) / 21.0;

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(rank-one perturbation 4x4)[0,0]", expr_eval_d(v), exp(2.0) + 5.0 * cexp, 1e-5);
        check_expr_text_contains("exp(rank-one perturbation 4x4)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 1, &v);
        check_d("exp(rank-one perturbation 4x4)[0,1]", expr_eval_d(v), 3.0 * cexp, 1e-12);
        mat_get(E, 3, 2, &v);
        check_d("exp(rank-one perturbation 4x4)[3,2]", expr_eval_d(v), 8.0 * cexp, 1e-5);
    }

    if (S) {
        mat_get(S, 1, 1, &v);
        check_d("sin(rank-one perturbation 4x4)[1,1]", expr_eval_d(v), sin(2.0) + 6.0 * csin, 1e-12);
        check_expr_text_contains("sin(rank-one perturbation 4x4)[1,1] stays symbolic", v, "sin");
        mat_get(S, 2, 0, &v);
        check_d("sin(rank-one perturbation 4x4)[2,0]", expr_eval_d(v), 15.0 * csin, 1e-12);
        mat_get(S, 0, 3, &v);
        check_d("sin(rank-one perturbation 4x4)[0,3]", expr_eval_d(v), csin, 1e-12);
    }

    if (bindings) {
        check_bool("rank-one perturbation dense expr 4x4 update x",
                   test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
    }

    cexp = (exp(25.0) - exp(2.0)) / 23.0;
    csin = (sin(25.0) - sin(2.0)) / 23.0;

    if (E) {
        mat_get(E, 0, 1, &v);
        check_d("exp(rank-one perturbation 4x4)[0,1] tracks x", expr_eval_d(v), 4.0 * cexp, 1e-5);
    }
    if (S) {
        mat_get(S, 1, 1, &v);
        check_d("sin(rank-one perturbation 4x4)[1,1] tracks x", expr_eval_d(v), sin(2.0) + 8.0 * csin, 1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_biquadratic_four(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[0 x 0 0]"
                                       "[x 0 x 0]"
                                       "[0 x 0 x]"
                                       "[0 0 x 0]]",
                                       &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    matrix_t *Aqc = NULL;
    matrix_t *Eqc = NULL;
    matrix_t *Eqc_want = NULL;
    matrix_t *Sqc = NULL;
    matrix_t *Sqc_want = NULL;
    expr_t *v = NULL;

    check_bool("dense expr biquadratic quartic 4x4 input not NULL", A != NULL);
    if (bindings) {
        check_bool("dense expr biquadratic quartic 4x4 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("dense expr biquadratic quartic 4x4 exp not NULL", E != NULL);
    check_bool("dense expr biquadratic quartic 4x4 sin not NULL", S != NULL);

    if (E && S) {
        Aqc = test_mat_evaluate_complex(A);
        Eqc = test_mat_evaluate_complex(E);
        Sqc = test_mat_evaluate_complex(S);
        Eqc_want = Aqc ? mat_exp(Aqc) : NULL;
        Sqc_want = Aqc ? mat_sin(Aqc) : NULL;

        check_bool("dense expr biquadratic quartic 4x4 evaluated exp not NULL", Eqc != NULL);
        check_bool("dense expr biquadratic quartic 4x4 evaluated sin not NULL", Sqc != NULL);
        check_bool("dense expr biquadratic quartic 4x4 numeric exp baseline not NULL", Eqc_want != NULL);
        check_bool("dense expr biquadratic quartic 4x4 numeric sin baseline not NULL", Sqc_want != NULL);
        if (Eqc && Eqc_want) {
            bool ok = test_assert_matrix_complex_close(Eqc, Eqc_want, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Aqc);
                mat_free(Eqc);
                mat_free(Eqc_want);
                mat_free(Sqc);
                mat_free(Sqc_want);
                mat_free(A);
                mat_free(E);
                mat_free(S);
                mat_bindings_free(bindings);
                return;
            }
        }
        if (Sqc && Sqc_want) {
            bool ok = test_assert_matrix_complex_close(Sqc, Sqc_want, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Aqc);
                mat_free(Eqc);
                mat_free(Eqc_want);
                mat_free(Sqc);
                mat_free(Sqc_want);
                mat_free(A);
                mat_free(E);
                mat_free(S);
                mat_bindings_free(bindings);
                return;
            }
        }
    }

    if (E) {
        mat_get(E, 0, 0, &v);
        check_expr_text_contains("exp(biquadratic quartic 4x4)[0,0] stays symbolic", v, "exp");
    }
    if (S) {
        mat_get(S, 0, 1, &v);
        check_expr_text_contains("sin(biquadratic quartic 4x4)[0,1] stays symbolic", v, "sin");
    }

    mat_free(Aqc);
    mat_free(Eqc);
    mat_free(Eqc_want);
    mat_free(Sqc);
    mat_free(Sqc_want);
    Aqc = NULL;
    Eqc = NULL;
    Eqc_want = NULL;
    Sqc = NULL;
    Sqc_want = NULL;

    if (bindings) {
        check_bool("dense expr biquadratic quartic 4x4 update x", test_mat_bindings_set_d(bindings, "x", 3.0) == 0);
    }

    if (E && S) {
        Aqc = test_mat_evaluate_complex(A);
        Eqc = test_mat_evaluate_complex(E);
        Sqc = test_mat_evaluate_complex(S);
        Eqc_want = Aqc ? mat_exp(Aqc) : NULL;
        Sqc_want = Aqc ? mat_sin(Aqc) : NULL;

        if (Eqc && Eqc_want) {
            bool ok = test_assert_matrix_complex_close(Eqc, Eqc_want, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Aqc);
                mat_free(Eqc);
                mat_free(Eqc_want);
                mat_free(Sqc);
                mat_free(Sqc_want);
                mat_free(A);
                mat_free(E);
                mat_free(S);
                mat_bindings_free(bindings);
                return;
            }
        }
        if (Sqc && Sqc_want) {
            bool ok = test_assert_matrix_complex_close(Sqc, Sqc_want, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Aqc);
                mat_free(Eqc);
                mat_free(Eqc_want);
                mat_free(Sqc);
                mat_free(Sqc_want);
                mat_free(A);
                mat_free(E);
                mat_free(S);
                mat_bindings_free(bindings);
                return;
            }
        }
    }

    mat_free(Aqc);
    mat_free(Eqc);
    mat_free(Eqc_want);
    mat_free(Sqc);
    mat_free(Sqc_want);
    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_block_diagonal_four(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[x 1 0 0][1 x 0 0][0 0 y 1][0 0 1 y]]", &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;

    check_bool("block-diagonal dense expr 4x4 input not NULL", A != NULL);
    if (bindings) {
        check_bool("block-diagonal dense expr 4x4 set x", test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        check_bool("block-diagonal dense expr 4x4 set y", test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("block-diagonal dense expr 4x4 exp not NULL", E != NULL);
    check_bool("block-diagonal dense expr 4x4 sin not NULL", S != NULL);

    if (A)
        print_mdv("A (block-diagonal dense 4x4)", A);
    if (E)
        print_mdv("exp(A)", E);
    if (S)
        print_mdv("sin(A)", S);

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(block-diagonal 4x4)[0,0]", expr_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
        check_expr_text_contains("exp(block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 1, &v);
        check_d("exp(block-diagonal 4x4)[0,1]", expr_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
        mat_get(E, 0, 2, &v);
        check_d("exp(block-diagonal 4x4)[0,2] stays zero", expr_eval_d(v), 0.0, 1e-12);
        mat_get(E, 2, 2, &v);
        check_d("exp(block-diagonal 4x4)[2,2]", expr_eval_d(v), 0.5 * (exp(4.0) + exp(2.0)), 1e-12);
    }

    if (S) {
        mat_get(S, 2, 2, &v);
        check_d("sin(block-diagonal 4x4)[2,2]", expr_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
        check_expr_text_contains("sin(block-diagonal 4x4)[2,2] stays symbolic", v, "sin");
        mat_get(S, 2, 3, &v);
        check_d("sin(block-diagonal 4x4)[2,3]", expr_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
        mat_get(S, 1, 3, &v);
        check_d("sin(block-diagonal 4x4)[1,3] stays zero", expr_eval_d(v), 0.0, 1e-12);
    }

    if (bindings) {
        check_bool("block-diagonal dense expr 4x4 update x", test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
        check_bool("block-diagonal dense expr 4x4 update y", test_mat_bindings_set_d(bindings, "y", 5.0) == 0);
    }

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(block-diagonal 4x4)[0,0] tracks x", expr_eval_d(v), 0.5 * (exp(5.0) + exp(3.0)), 1e-12);
    }
    if (S) {
        mat_get(S, 2, 3, &v);
        check_d("sin(block-diagonal 4x4)[2,3] tracks y", expr_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)), 1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

static void test_symfunc_permuted_blocks_four(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *A = mat_from_string_expr("[[x 0 1 0][0 y 0 1][1 0 x 0][0 1 0 y]]", &bindings);
    matrix_t *E = NULL;
    matrix_t *S = NULL;
    expr_t *v = NULL;

    check_bool("permuted block-diagonal dense expr 4x4 input not NULL", A != NULL);
    if (bindings) {
        check_bool("permuted block-diagonal dense expr 4x4 set x",
                   test_mat_bindings_set_d(bindings, "x", 2.0) == 0);
        check_bool("permuted block-diagonal dense expr 4x4 set y",
                   test_mat_bindings_set_d(bindings, "y", 3.0) == 0);
    }

    E = mat_exp(A);
    S = mat_sin(A);
    check_bool("permuted block-diagonal dense expr 4x4 exp not NULL", E != NULL);
    check_bool("permuted block-diagonal dense expr 4x4 sin not NULL", S != NULL);

    if (E) {
        mat_get(E, 0, 0, &v);
        check_d("exp(permuted block-diagonal 4x4)[0,0]", expr_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
        check_expr_text_contains("exp(permuted block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
        mat_get(E, 0, 2, &v);
        check_d("exp(permuted block-diagonal 4x4)[0,2]", expr_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
        mat_get(E, 0, 1, &v);
        check_d("exp(permuted block-diagonal 4x4)[0,1] stays zero", expr_eval_d(v), 0.0, 1e-12);
        mat_get(E, 1, 3, &v);
        check_d("exp(permuted block-diagonal 4x4)[1,3]", expr_eval_d(v), 0.5 * (exp(4.0) - exp(2.0)), 1e-12);
    }

    if (S) {
        mat_get(S, 1, 1, &v);
        check_d("sin(permuted block-diagonal 4x4)[1,1]", expr_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
        check_expr_text_contains("sin(permuted block-diagonal 4x4)[1,1] stays symbolic", v, "sin");
        mat_get(S, 1, 3, &v);
        check_d("sin(permuted block-diagonal 4x4)[1,3]", expr_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
        mat_get(S, 2, 3, &v);
        check_d("sin(permuted block-diagonal 4x4)[2,3] stays zero", expr_eval_d(v), 0.0, 1e-12);
    }

    if (bindings) {
        check_bool("permuted block-diagonal dense expr 4x4 update x",
                   test_mat_bindings_set_d(bindings, "x", 4.0) == 0);
        check_bool("permuted block-diagonal dense expr 4x4 update y",
                   test_mat_bindings_set_d(bindings, "y", 5.0) == 0);
    }

    if (E) {
        mat_get(E, 0, 2, &v);
        check_d("exp(permuted block-diagonal 4x4)[0,2] tracks x", expr_eval_d(v), 0.5 * (exp(5.0) - exp(3.0)),
                1e-12);
    }
    if (S) {
        mat_get(S, 1, 3, &v);
        check_d("sin(permuted block-diagonal 4x4)[1,3] tracks y", expr_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)),
                1e-12);
    }

    mat_free(A);
    mat_free(E);
    mat_free(S);
    mat_bindings_free(bindings);
}

/* Keep the complete symbolic-function scenarios independently selectable. */
void test_expr_matrix_functions_extended(void)
{
    TEST_RUN_SUBTEST(test_symfunc_jordan_special, NULL);
    TEST_RUN_SUBTEST(test_symfunc_jordan_exp, NULL);
    TEST_RUN_SUBTEST(test_symfunc_dense_numeric_fallback, NULL);
    TEST_RUN_SUBTEST(test_symfunc_cubic_three, NULL);
    TEST_RUN_SUBTEST(test_symfunc_quadratic_three, NULL);
    TEST_RUN_SUBTEST(test_symfunc_uniform_five, NULL);
    TEST_RUN_SUBTEST(test_symfunc_rank_one_four, NULL);
    TEST_RUN_SUBTEST(test_symfunc_biquadratic_four, NULL);
    TEST_RUN_SUBTEST(test_symfunc_block_diagonal_four, NULL);
    TEST_RUN_SUBTEST(test_symfunc_permuted_blocks_four, NULL);
}
