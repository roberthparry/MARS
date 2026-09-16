#include "test_matrix.h"
#include "test_matrix_solve.h"

/* Borrow immortal constants in mutable matrix fixture arrays without changing the public API. */
#define EXPR_ZERO ((expr_t *)EXPR_ZERO)
#define EXPR_ONE ((expr_t *)EXPR_ONE)

/* Solve with pivoting and multiple RHSs. */
static void test_solve_numeric_pivoting(void)
{
    double A_vals[4] = {0.0, 2.0, 1.0, 3.0};
    double X_want_vals[4] = {1.0, -1.0, 2.0, 4.0};
    double B_vals[4] = {4.0, 8.0, 7.0, 11.0};
    matrix_t *A = test_mat_create_d(2, 2, A_vals);
    matrix_t *B = test_mat_create_d(2, 2, B_vals);
    matrix_t *X_want = test_mat_create_d(2, 2, X_want_vals);

    print_mnum("A", A);
    print_mnum("B", B);

    matrix_t *X = mat_solve(A, B);
    check_bool("mat_solve(double) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(A);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Lower-triangular direct solve. */
static void test_solve_lower_triangular(void)
{
    double L_vals[9] = {2.0, 0.0, 0.0, 3.0, 1.0, 0.0, 1.0, -2.0, 4.0};
    double X_want_vals[3] = {1.0, 2.0, -1.0};
    double B_vals[3] = {2.0, 5.0, -7.0};
    matrix_t *L = test_mat_create_d(3, 3, L_vals);
    matrix_t *B = test_mat_create_d(3, 1, B_vals);
    matrix_t *X_want = test_mat_create_d(3, 1, X_want_vals);

    print_md("L (lower triangular)", L);
    print_md("B", B);

    matrix_t *X = mat_solve(L, B);
    check_bool("mat_solve(lower triangular) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(L);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(L);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Upper-triangular direct solve. */
static void test_solve_upper_triangular(void)
{
    double U_vals[9] = {2.0, 1.0, -1.0, 0.0, 3.0, 2.0, 0.0, 0.0, 4.0};
    double X_want_vals[3] = {1.0, -2.0, 0.5};
    double B_vals[3] = {-0.5, -5.0, 2.0};
    matrix_t *U = test_mat_create_d(3, 3, U_vals);
    matrix_t *B = test_mat_create_d(3, 1, B_vals);
    matrix_t *X_want = test_mat_create_d(3, 1, X_want_vals);

    print_md("U (upper triangular)", U);
    print_md("B", B);

    matrix_t *X = mat_solve(U, B);
    check_bool("mat_solve(upper triangular) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(U);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(U);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Sparse lower-triangular solve exercises sparse-aware direct substitution. */
static void test_solve_sparse_lower(void)
{
    matrix_t *L = test_mat_sparse_d(3, 3);
    matrix_t *B = test_mat_create_d(3, 1, (double[]){4.0, 5.0, 7.0});
    matrix_t *X_want = test_mat_create_d(3, 1, (double[]){2.0, 0.75, 1.125});
    double v;

    check_bool("sparse lower-triangular input allocated", L != NULL && B != NULL && X_want != NULL);
    if (!L || !B || !X_want) {
        mat_free(L);
        mat_free(B);
        mat_free(X_want);
        return;
    }

    v = 2.0;
    mat_set(L, 0, 0, &v);
    v = 1.0;
    mat_set(L, 1, 0, &v);
    v = 4.0;
    mat_set(L, 1, 1, &v);
    v = -1.0;
    mat_set(L, 2, 0, &v);
    v = 3.0;
    mat_set(L, 2, 1, &v);
    v = 6.0;
    mat_set(L, 2, 2, &v);

    check_bool("sparse matrix recognised as lower triangular", mat_is_lower_triangular(L));
    print_md("L (sparse lower triangular)", L);
    print_md("B", B);

    matrix_t *X = mat_solve(L, B);
    check_bool("mat_solve(sparse lower triangular) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(L);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(L);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Diagonal solve preserves the right-hand-side layout. */
static void test_solve_diagonal_sparse_rhs(void)
{
    matrix_t *D = test_mat_diagonal_d(3, (double[]){2.0, 4.0, 8.0});
    matrix_t *B = test_mat_sparse_d(3, 3);
    matrix_t *X_want = test_mat_sparse_d(3, 3);
    double v;

    check_bool("diagonal solve inputs allocated", D != NULL && B != NULL && X_want != NULL);
    if (!D || !B || !X_want) {
        mat_free(D);
        mat_free(B);
        mat_free(X_want);
        return;
    }

    v = 4.0;
    mat_set(B, 0, 0, &v);
    v = 12.0;
    mat_set(B, 1, 2, &v);
    v = 16.0;
    mat_set(B, 2, 1, &v);

    v = 2.0;
    mat_set(X_want, 0, 0, &v);
    v = 3.0;
    mat_set(X_want, 1, 2, &v);
    v = 2.0;
    mat_set(X_want, 2, 1, &v);

    print_md("D (diagonal)", D);
    print_md("B (sparse right-hand side)", B);

    matrix_t *X = mat_solve(D, B);
    check_bool("mat_solve(diagonal,sparse RHS) not NULL", X != NULL);
    if (X) {
        check_bool("diagonal solve preserves sparse layout of RHS", mat_is_sparse(X));
        {
            bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(D);
                mat_free(B);
                mat_free(X_want);
                mat_free(X);
                return;
            }
        }
    }

    mat_free(D);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* General sparse solve goes through LU plus substitution. */
static void test_solve_sparse_general(void)
{
    matrix_t *A = test_mat_sparse_d(3, 3);
    matrix_t *B = test_mat_create_d(3, 1, (double[]){7.0, 8.0, 3.0});
    matrix_t *X_want = test_mat_create_d(3, 1, (double[]){7.0 / 3.0, 2.0 / 3.0, 3.0});
    double v;

    check_bool("general sparse solve inputs allocated", A != NULL && B != NULL && X_want != NULL);
    if (!A || !B || !X_want) {
        mat_free(A);
        mat_free(B);
        mat_free(X_want);
        return;
    }

    v = 4.0;
    mat_set(A, 0, 0, &v);
    v = 1.0;
    mat_set(A, 0, 1, &v);
    v = -1.0;
    mat_set(A, 0, 2, &v);
    v = 2.0;
    mat_set(A, 1, 0, &v);
    v = 5.0;
    mat_set(A, 1, 1, &v);
    v = 1.0;
    mat_set(A, 2, 2, &v);

    print_md("A (general sparse)", A);
    print_md("B", B);

    matrix_t *X = mat_solve(A, B);
    check_bool("mat_solve(general sparse) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(A);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* General sparse solve with pivoting exercises sparse row swaps too. */
static void test_solve_sparse_pivoting(void)
{
    matrix_t *A = test_mat_sparse_d(3, 3);
    matrix_t *B = test_mat_create_d(3, 1, (double[]){4.0, 11.0, 2.0});
    matrix_t *X_want = test_mat_create_d(3, 1, (double[]){1.0, 2.0, 2.0});
    double v;

    check_bool("general sparse pivoting solve inputs allocated", A != NULL && B != NULL && X_want != NULL);
    if (!A || !B || !X_want) {
        mat_free(A);
        mat_free(B);
        mat_free(X_want);
        return;
    }

    v = 2.0;
    mat_set(A, 0, 1, &v);
    v = 1.0;
    mat_set(A, 1, 0, &v);
    v = 1.0;
    mat_set(A, 1, 1, &v);
    v = 1.0;
    mat_set(A, 2, 2, &v);
    v = 4.0;
    mat_set(A, 1, 2, &v);

    print_md("A (general sparse with pivoting)", A);
    print_md("B", B);

    matrix_t *X = mat_solve(A, B);
    check_bool("mat_solve(general sparse with pivoting) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_d_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(A);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            return;
        }
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Rank-deficient overdetermined system falls back to pseudoinverse. */
static void test_solve_rank_deficient(void)
{
    number_t A_vals[6] = {num_create_from_long(1), num_create_from_long(0), num_create_from_long(2),
                          num_create_from_long(0), num_create_from_long(3), num_create_from_long(0)};
    number_t B_vals[3] = {num_create_from_long(1), num_create_from_long(2), num_create_from_long(3)};
    number_t X_want_vals[2] = {num_create_from_long(1), num_create_from_long(0)};
    matrix_t *A = mat_create_num(3, 2, A_vals);
    matrix_t *B = mat_create_num(3, 1, B_vals);
    matrix_t *X_want = mat_create_num(2, 1, X_want_vals);

    for (size_t i = 0; i < 6; ++i)
        num_destroy(&A_vals[i]);
    for (size_t i = 0; i < 3; ++i)
        num_destroy(&B_vals[i]);
    for (size_t i = 0; i < 2; ++i)
        num_destroy(&X_want_vals[i]);

    print_mnum("A", A);
    print_mnum("B", B);

    matrix_t *X = mat_least_squares(A, B);
    check_bool("mat_least_squares(rank-deficient) not NULL", X != NULL);
    if (X) {
        matrix_t *Xq = test_mat_evaluate_complex(X);
        matrix_t *Xeq = test_mat_evaluate_complex(X_want);
        check_bool("mat_least_squares(rank-deficient) -> MAT_TYPE_NUMBER", mat_typeof(X) == MAT_TYPE_NUMBER);
        if (Xq && Xeq) {
            bool ok = test_assert_matrix_complex_close(Xq, Xeq, 1e-10, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Xq);
                mat_free(Xeq);
                mat_free(A);
                mat_free(B);
                mat_free(X_want);
                mat_free(X);
                return;
            }
        }
        mat_free(Xq);
        mat_free(Xeq);
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Underdetermined system returns the minimum-norm solution. */
static void test_solve_minimum_norm(void)
{
    number_t A_vals[6] = {num_create_from_long(1), num_create_from_long(0), num_create_from_long(0),
                          num_create_from_long(0), num_create_from_long(1), num_create_from_long(0)};
    number_t B_vals[2] = {num_create_from_long(2), num_create_from_long(3)};
    number_t X_want_vals[3] = {num_create_from_long(2), num_create_from_long(3), num_create_from_long(0)};
    matrix_t *A = mat_create_num(2, 3, A_vals);
    matrix_t *B = mat_create_num(2, 1, B_vals);
    matrix_t *X_want = mat_create_num(3, 1, X_want_vals);

    for (size_t i = 0; i < 6; ++i)
        num_destroy(&A_vals[i]);
    for (size_t i = 0; i < 2; ++i)
        num_destroy(&B_vals[i]);
    for (size_t i = 0; i < 3; ++i)
        num_destroy(&X_want_vals[i]);

    print_mnum("A", A);
    print_mnum("B", B);

    matrix_t *X = mat_least_squares(A, B);
    check_bool("mat_least_squares(underdetermined) not NULL", X != NULL);
    if (X) {
        matrix_t *Xq = test_mat_evaluate_complex(X);
        matrix_t *Xeq = test_mat_evaluate_complex(X_want);
        check_bool("mat_least_squares(underdetermined) -> MAT_TYPE_NUMBER", mat_typeof(X) == MAT_TYPE_NUMBER);
        if (Xq && Xeq) {
            bool ok = test_assert_matrix_complex_close(Xq, Xeq, 1e-10, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Xq);
                mat_free(Xeq);
                mat_free(A);
                mat_free(B);
                mat_free(X_want);
                mat_free(X);
                return;
            }
        }
        mat_free(Xq);
        mat_free(Xeq);
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Exact least-squares recovery for an overdetermined system. */
static void test_solve_least_squares(void)
{
    number_t A_vals[6] = {num_create_from_long(1), num_create_from_long(0), num_create_from_long(1),
                          num_create_from_long(1), num_create_from_long(1), num_create_from_long(2)};
    number_t X_want_vals[2] = {num_create_from_long(2), num_create_from_long(-1)};
    number_t B_vals[3] = {num_create_from_long(2), num_create_from_long(1), num_create_from_long(0)};
    matrix_t *A = mat_create_num(3, 2, A_vals);
    matrix_t *B = mat_create_num(3, 1, B_vals);
    matrix_t *X_want = mat_create_num(2, 1, X_want_vals);

    for (size_t i = 0; i < 6; ++i)
        num_destroy(&A_vals[i]);
    for (size_t i = 0; i < 3; ++i)
        num_destroy(&B_vals[i]);
    for (size_t i = 0; i < 2; ++i)
        num_destroy(&X_want_vals[i]);

    print_mnum("A", A);
    print_md("B", B);

    matrix_t *X = mat_least_squares(A, B);
    check_bool("mat_least_squares(double) not NULL", X != NULL);
    if (X) {
        matrix_t *Xq = test_mat_evaluate_complex(X);
        matrix_t *Xeq = test_mat_evaluate_complex(X_want);
        check_bool("mat_least_squares(double) -> MAT_TYPE_NUMBER", mat_typeof(X) == MAT_TYPE_NUMBER);
        if (Xq && Xeq) {
            bool ok = test_assert_matrix_complex_close(Xq, Xeq, 1e-12, __FILE__, __LINE__);
            if (!ok) {
                mat_free(Xq);
                mat_free(Xeq);
                mat_free(A);
                mat_free(B);
                mat_free(X_want);
                mat_free(X);
                return;
            }
        }
        mat_free(Xq);
        mat_free(Xeq);
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
}

/* Exact symbolic least-squares recovery for a full-column-rank system. */
static void test_solve_symbolic_least_squares(void)
{
    expr_t *p = test_expr_new_named_var_d(3.0, "p");
    expr_t *q = test_expr_new_named_var_d(4.0, "q");
    expr_t *u = test_expr_new_named_var_d(5.0, "u");
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *A_vals[6] = {p, EXPR_ZERO, EXPR_ZERO, q, EXPR_ONE, EXPR_ONE};
    expr_t *X_vals[2] = {u, two};
    matrix_t *A = mat_create_expr(3, 2, A_vals);
    matrix_t *X_want = mat_create_expr(2, 1, X_vals);
    matrix_t *B = mat_mul(A, X_want);
    matrix_t *X = NULL;
    matrix_t *AX = NULL;

    print_mdv("A (least-squares expr)", A);
    print_mdv("B = A*X", B);

    X = mat_least_squares(A, B);
    check_bool("mat_least_squares(expr full-column-rank) not NULL", X != NULL);
    check_bool("mat_least_squares(expr full-column-rank) -> MAT_TYPE_EXPR",
               X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x00 = NULL, *x10 = NULL;

        print_mdv("X least-squares result (expr)", X);
        mat_get(X, 0, 0, &x00);
        mat_get(X, 1, 0, &x10);
        check_d("lstsq(A,B)[0,0] = u", expr_eval_d(x00), 5.0, 1e-12);
        check_d("lstsq(A,B)[1,0] = 2", expr_eval_d(x10), 2.0, 1e-12);

        test_expr_set_val_d(p, 7.0);
        test_expr_set_val_d(q, 11.0);
        test_expr_set_val_d(u, 13.0);
        check_d("lstsq(A,B)[0,0] tracks u", expr_eval_d(x00), 13.0, 1e-12);
        check_d("lstsq(A,B)[1,0] remains 2", expr_eval_d(x10), 2.0, 1e-12);

        AX = mat_mul(A, X);
        check_bool("A*lstsq(A,B) not NULL", AX != NULL);
        if (AX) {
            expr_t *got = NULL, *want = NULL;
            for (size_t i = 0; i < 3; ++i) {
                mat_get(AX, i, 0, &got);
                mat_get(B, i, 0, &want);
                check_d("expr least-squares residual entry", expr_eval_d(got), expr_eval_d(want), 1e-10);
            }
        }
    }

    mat_free(A);
    mat_free(X_want);
    mat_free(B);
    mat_free(X);
    mat_free(AX);
    expr_free(p);
    expr_free(q);
    expr_free(u);
    expr_free(two);
}

/* Exact symbolic least-squares for a rank-deficient rectangular system. */
static void test_solve_symbolic_rank_deficient(void)
{
    expr_t *p = test_expr_new_named_var_d(3.0, "p");
    expr_t *A_vals[6] = {p, p, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO};
    expr_t *X_want_vals[2] = {EXPR_ONE, EXPR_ONE};
    matrix_t *A = mat_create_expr(3, 2, A_vals);
    matrix_t *X_want = mat_create_expr(2, 1, X_want_vals);
    matrix_t *B = mat_mul(A, X_want);
    matrix_t *X = NULL;
    matrix_t *AX = NULL;

    print_mdv("A (rank-deficient least-squares expr)", A);
    print_mdv("B = A*X", B);

    X = mat_least_squares(A, B);
    check_bool("mat_least_squares(expr rank-deficient) not NULL", X != NULL);
    check_bool("mat_least_squares(expr rank-deficient) -> MAT_TYPE_EXPR",
               X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x00 = NULL, *x10 = NULL;

        print_mdv("X least-squares result (rank-deficient expr)", X);
        mat_get(X, 0, 0, &x00);
        mat_get(X, 1, 0, &x10);
        check_d("rank-deficient lstsq(A,B)[0,0] = 1", expr_eval_d(x00), 1.0, 1e-12);
        check_d("rank-deficient lstsq(A,B)[1,0] = 1", expr_eval_d(x10), 1.0, 1e-12);

        test_expr_set_val_d(p, 7.0);
        check_d("rank-deficient lstsq(A,B)[0,0] stays 1", expr_eval_d(x00), 1.0, 1e-12);
        check_d("rank-deficient lstsq(A,B)[1,0] stays 1", expr_eval_d(x10), 1.0, 1e-12);

        AX = mat_mul(A, X);
        check_bool("A*lstsq(A,B) for rank-deficient expr not NULL", AX != NULL);
        if (AX) {
            expr_t *got = NULL, *want = NULL;
            for (size_t i = 0; i < 3; ++i) {
                mat_get(AX, i, 0, &got);
                mat_get(B, i, 0, &want);
                check_d("rank-deficient expr least-squares residual entry", expr_eval_d(got), expr_eval_d(want),
                        1e-10);
            }
        }
    }

    mat_free(A);
    mat_free(X_want);
    mat_free(B);
    mat_free(X);
    mat_free(AX);
    expr_free(p);
}

/* Complex solve exercises promotion and Hermitian-free elimination. */
static void test_solve_complex(void)
{
    number_t A_vals[4] = {num_create_from_string("1 + i"), num_create_from_string("2.0"),
                          num_create_from_string("i"), num_create_from_string("3 - i")};
    number_t X_want_vals[2] = {num_create_from_string("1 - i"), num_create_from_string("2 + 0.5i")};
    matrix_t *A = mat_create_num(2, 2, A_vals);
    matrix_t *X_want = mat_create_num(2, 1, X_want_vals);
    matrix_t *B = mat_mul(A, X_want);

    print_mnum("A", A);
    print_mnum("B", B);

    matrix_t *X = mat_solve(A, B);
    check_bool("mat_solve(complex number) not NULL", X != NULL);
    if (X) {
        bool ok = test_assert_matrix_complex_close(X, X_want, 1e-12, __FILE__, __LINE__);
        if (!ok) {
            mat_free(A);
            mat_free(B);
            mat_free(X_want);
            mat_free(X);
            for (size_t k = 0; k < 4; ++k)
                num_destroy(&A_vals[k]);
            for (size_t k = 0; k < 2; ++k)
                num_destroy(&X_want_vals[k]);
            return;
        }
    }

    mat_free(A);
    mat_free(B);
    mat_free(X_want);
    mat_free(X);
    for (size_t k = 0; k < 4; ++k)
        num_destroy(&A_vals[k]);
    for (size_t k = 0; k < 2; ++k)
        num_destroy(&X_want_vals[k]);
}

/* Symbolic lower-triangular solve. */
static void test_solve_symbolic_lower(void)
{
    expr_t *x = test_expr_new_named_var_d(2.0, "x");
    expr_t *y = test_expr_new_named_var_d(3.0, "y");
    expr_t *z = test_expr_new_named_var_d(4.0, "z");
    expr_t *s = test_expr_new_named_var_d(5.0, "s");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *three = test_expr_new_const_d(3.0);
    expr_t *X_vals[3] = {s, two, one};
    expr_t *L_vals[9] = {x, EXPR_ZERO, EXPR_ZERO, one, y, EXPR_ZERO, two, three, z};
    matrix_t *L = mat_create_expr(3, 3, L_vals);
    matrix_t *X_want = mat_create_expr(3, 1, X_vals);
    matrix_t *B = mat_mul(L, X_want);
    matrix_t *X = NULL;
    matrix_t *LB = NULL;

    print_mdv("L (lower triangular expr)", L);
    print_mdv("X want", X_want);
    print_mdv("B = L*X", B);

    X = mat_solve(L, B);
    check_bool("mat_solve(lower triangular expr) not NULL", X != NULL);
    check_bool("mat_solve(lower triangular expr) -> MAT_TYPE_EXPR", X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        char *x_text = mat_to_string(X, MAT_STRING_INLINE_PRETTY);
        expr_t *x00 = NULL, *x10 = NULL, *x20 = NULL;

        print_mdv("X solve result", X);
        check_bool("solve(L,B) exact text simplified", x_text && strcmp(x_text, "{ (s; 2; 1) | s = 5 }") == 0);
        mat_get(X, 0, 0, &x00);
        mat_get(X, 1, 0, &x10);
        mat_get(X, 2, 0, &x20);
        check_d("solve(L,B)[0,0] = s", expr_eval_d(x00), 5.0, 1e-12);
        check_d("solve(L,B)[1,0] = 2", expr_eval_d(x10), 2.0, 1e-12);
        check_d("solve(L,B)[2,0] = 1", expr_eval_d(x20), 1.0, 1e-12);

        test_expr_set_val_d(x, 7.0);
        test_expr_set_val_d(y, 11.0);
        test_expr_set_val_d(z, 13.0);
        test_expr_set_val_d(s, 17.0);
        check_d("solve(L,B)[0,0] tracks s only", expr_eval_d(x00), 17.0, 1e-12);
        check_d("solve(L,B)[1,0] remains 2", expr_eval_d(x10), 2.0, 1e-12);
        check_d("solve(L,B)[2,0] remains 1", expr_eval_d(x20), 1.0, 1e-12);

        LB = mat_mul(L, X);
        check_bool("L*solve(L,B) not NULL", LB != NULL);
        if (LB) {
            expr_t *got = NULL, *want = NULL;
            for (size_t i = 0; i < 3; ++i) {
                mat_get(LB, i, 0, &got);
                mat_get(B, i, 0, &want);
                check_d("lower triangular expr residual row", expr_eval_d(got), expr_eval_d(want), 1e-10);
            }
        }

        free(x_text);
    }

    mat_free(L);
    mat_free(X_want);
    mat_free(B);
    mat_free(X);
    mat_free(LB);
    expr_free(x);
    expr_free(y);
    expr_free(z);
    expr_free(s);
    expr_free(one);
    expr_free(two);
    expr_free(three);
}

/* General dense symbolic solve with multiple right-hand sides. */
static void test_solve_symbolic_dense(void)
{
    expr_t *a = test_expr_new_named_var_d(4.0, "a");
    expr_t *b = test_expr_new_named_var_d(5.0, "b");
    expr_t *c = test_expr_new_named_var_d(6.0, "c");
    expr_t *u = test_expr_new_named_var_d(2.0, "u");
    expr_t *v = test_expr_new_named_var_d(3.0, "v");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *three = test_expr_new_const_d(3.0);
    expr_t *four = test_expr_new_const_d(4.0);
    expr_t *A_vals[9] = {a, one, EXPR_ZERO, one, b, one, EXPR_ZERO, one, c};
    expr_t *X_vals[6] = {u, one, two, v, three, four};
    matrix_t *A = mat_create_expr(3, 3, A_vals);
    matrix_t *X_want = mat_create_expr(3, 2, X_vals);
    matrix_t *B = mat_mul(A, X_want);
    matrix_t *X = NULL;
    matrix_t *AX = NULL;

    print_mdv("A (dense expr)", A);
    print_mdv("X want", X_want);
    print_mdv("B = A*X", B);

    X = mat_solve(A, B);
    check_bool("mat_solve(dense expr) not NULL", X != NULL);
    check_bool("mat_solve(dense expr) -> MAT_TYPE_EXPR", X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x00 = NULL, *x01 = NULL, *x11 = NULL, *x20 = NULL;

        print_mdv("X solve result (dense expr)", X);
        mat_get(X, 0, 0, &x00);
        mat_get(X, 0, 1, &x01);
        mat_get(X, 1, 1, &x11);
        mat_get(X, 2, 0, &x20);
        check_d("solve(A,B)[0,0] = u", expr_eval_d(x00), 2.0, 1e-12);
        check_d("solve(A,B)[0,1] = 1", expr_eval_d(x01), 1.0, 1e-12);
        check_d("solve(A,B)[1,1] = v", expr_eval_d(x11), 3.0, 1e-12);
        check_d("solve(A,B)[2,0] = 3", expr_eval_d(x20), 3.0, 1e-12);

        test_expr_set_val_d(a, 7.0);
        test_expr_set_val_d(b, 8.0);
        test_expr_set_val_d(c, 9.0);
        test_expr_set_val_d(u, 11.0);
        test_expr_set_val_d(v, 13.0);
        check_d("solve(A,B)[0,0] tracks u", expr_eval_d(x00), 11.0, 1e-12);
        check_d("solve(A,B)[0,1] remains 1", expr_eval_d(x01), 1.0, 1e-12);
        check_d("solve(A,B)[1,1] tracks v", expr_eval_d(x11), 13.0, 1e-12);
        check_d("solve(A,B)[2,0] remains 3", expr_eval_d(x20), 3.0, 1e-12);

        AX = mat_mul(A, X);
        check_bool("A*solve(A,B) not NULL", AX != NULL);
        if (AX) {
            expr_t *got = NULL, *want = NULL;
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 2; ++j) {
                    mat_get(AX, i, j, &got);
                    mat_get(B, i, j, &want);
                    check_d("dense expr residual entry", expr_eval_d(got), expr_eval_d(want), 1e-10);
                }
            }
        }
    }

    mat_free(A);
    mat_free(X_want);
    mat_free(B);
    mat_free(X);
    mat_free(AX);
    expr_free(a);
    expr_free(b);
    expr_free(c);
    expr_free(u);
    expr_free(v);
    expr_free(one);
    expr_free(two);
    expr_free(three);
    expr_free(four);
}

/* Symbolic coefficient matrix with a numeric RHS should promote RHS to expr. */
static void test_solve_symbolic_numeric_rhs(void)
{
    expr_t *a = test_expr_new_named_var_d(3.0, "a");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *A_vals[4] = {one, a, EXPR_ZERO, two};
    number_t B_vals[2] = {test_num_from_d(5.0), test_num_from_d(4.0)};
    matrix_t *A = mat_create_expr(2, 2, A_vals);
    matrix_t *B = mat_create_num(2, 1, B_vals);
    matrix_t *X = mat_solve(A, B);

    check_bool("mat_solve(symbolic A, numeric B) not NULL", X != NULL);
    check_bool("mat_solve(symbolic A, numeric B) -> MAT_TYPE_EXPR", X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x0 = NULL, *x1 = NULL;

        mat_get(X, 0, 0, &x0);
        mat_get(X, 1, 0, &x1);
        check_d("mixed solve X[0,0] = -1", expr_eval_d(x0), -1.0, 1e-12);
        check_d("mixed solve X[1,0] = 2", expr_eval_d(x1), 2.0, 1e-12);

        test_expr_set_val_d(a, 7.0);
        check_d("mixed solve X[0,0] tracks symbolic A", expr_eval_d(x0), -9.0, 1e-12);
        check_d("mixed solve X[1,0] remains 2", expr_eval_d(x1), 2.0, 1e-12);
    }

    mat_free(A);
    mat_free(B);
    mat_free(X);
    num_destroy(&B_vals[0]);
    num_destroy(&B_vals[1]);
    expr_free(a);
    expr_free(one);
    expr_free(two);
}

/* Exact 2x2 symbolic solve should keep Cramer's-rule fractions readable. */
static void test_solve_symbolic_cramer(void)
{
    matrix_t *A = mat_from_string_expr("(θ, γ; μ, λ)", NULL);
    matrix_t *B = mat_from_string_expr("(x; y)", NULL);
    matrix_t *X = mat_solve(A, B);

    check_bool("mat_solve(2x2 symbolic) not NULL", X != NULL);
    check_bool("mat_solve(2x2 symbolic) -> MAT_TYPE_EXPR", X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x0 = NULL, *x1 = NULL;

        mat_get(X, 0, 0, &x0);
        mat_get(X, 1, 0, &x1);
        check_expr_text_contains("2x2 symbolic solve X[0] numerator simplified", x0, "λx - γy");
        check_expr_text_contains("2x2 symbolic solve X[0] denominator simplified", x0, "θλ - γμ");
        check_expr_text_contains("2x2 symbolic solve X[1] numerator simplified", x1, "θy - μx");
        check_expr_text_contains("2x2 symbolic solve X[1] denominator simplified", x1, "θλ - γμ");
    }

    mat_free(A);
    mat_free(B);
    mat_free(X);
}

/* Larger dense symbolic solve with multiple right-hand sides. */
static void test_solve_symbolic_dense_six(void)
{
    expr_t *a = test_expr_new_named_var_d(5.0, "a");
    expr_t *b = test_expr_new_named_var_d(6.0, "b");
    expr_t *c = test_expr_new_named_var_d(7.0, "c");
    expr_t *d = test_expr_new_named_var_d(8.0, "d");
    expr_t *e = test_expr_new_named_var_d(9.0, "e");
    expr_t *f = test_expr_new_named_var_d(10.0, "f");
    expr_t *u = test_expr_new_named_var_d(11.0, "u");
    expr_t *v = test_expr_new_named_var_d(13.0, "v");
    expr_t *one = test_expr_new_const_d(1.0);
    expr_t *two = test_expr_new_const_d(2.0);
    expr_t *three = test_expr_new_const_d(3.0);
    expr_t *four = test_expr_new_const_d(4.0);
    expr_t *five = test_expr_new_const_d(5.0);
    expr_t *six = test_expr_new_const_d(6.0);
    expr_t *seven = test_expr_new_const_d(7.0);
    expr_t *A_vals[36] = {a,         one,       two,       EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, one,       b,
                          one,       EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, two,       one,       c,         one,
                          EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, one,       d,         one,       two,
                          EXPR_ZERO, EXPR_ZERO, EXPR_ZERO, one,       e,         one,       EXPR_ZERO, EXPR_ZERO,
                          EXPR_ZERO, two,       one,       f};
    expr_t *X_vals[12] = {u, one, two, v, three, four, four, five, five, six, six, seven};
    matrix_t *A = mat_create_expr(6, 6, A_vals);
    matrix_t *X_want = mat_create_expr(6, 2, X_vals);
    matrix_t *B = mat_mul(A, X_want);
    matrix_t *X = NULL;
    matrix_t *AX = NULL;

    print_mdv("A (dense 6x6 expr)", A);
    print_mdv("B = A*X", B);

    X = mat_solve(A, B);
    check_bool("mat_solve(dense 6x6 expr) not NULL", X != NULL);
    check_bool("mat_solve(dense 6x6 expr) -> MAT_TYPE_EXPR", X != NULL && mat_typeof(X) == MAT_TYPE_EXPR);
    if (X) {
        expr_t *x00 = NULL, *x11 = NULL, *x32 = NULL;

        mat_get(X, 0, 0, &x00);
        mat_get(X, 1, 1, &x11);
        mat_get(X, 5, 1, &x32);
        check_d("solve(A,B)[0,0] = u", expr_eval_d(x00), 11.0, 1e-12);
        check_d("solve(A,B)[1,1] = v", expr_eval_d(x11), 13.0, 1e-12);
        check_d("solve(A,B)[5,1] = 7", expr_eval_d(x32), 7.0, 1e-12);

        test_expr_set_val_d(a, 15.0);
        test_expr_set_val_d(b, 16.0);
        test_expr_set_val_d(c, 17.0);
        test_expr_set_val_d(d, 18.0);
        test_expr_set_val_d(e, 19.0);
        test_expr_set_val_d(f, 20.0);
        test_expr_set_val_d(u, 23.0);
        test_expr_set_val_d(v, 29.0);
        check_d("solve(A,B)[0,0] tracks u on 6x6", expr_eval_d(x00), 23.0, 1e-12);
        check_d("solve(A,B)[1,1] tracks v on 6x6", expr_eval_d(x11), 29.0, 1e-12);
        check_d("solve(A,B)[5,1] remains 7 on 6x6", expr_eval_d(x32), 7.0, 1e-12);

        AX = mat_mul(A, X);
        check_bool("A*solve(A,B) 6x6 not NULL", AX != NULL);
        if (AX) {
            expr_t *got = NULL, *want = NULL;
            for (size_t i = 0; i < 6; ++i) {
                for (size_t j = 0; j < 2; ++j) {
                    mat_get(AX, i, j, &got);
                    mat_get(B, i, j, &want);
                    check_d("dense 6x6 expr solve residual entry", expr_eval_d(got), expr_eval_d(want), 1e-10);
                }
            }
        }
    }

    mat_free(A);
    mat_free(X_want);
    mat_free(B);
    mat_free(X);
    mat_free(AX);
    expr_free(a);
    expr_free(b);
    expr_free(c);
    expr_free(d);
    expr_free(e);
    expr_free(f);
    expr_free(u);
    expr_free(v);
    expr_free(one);
    expr_free(two);
    expr_free(three);
    expr_free(four);
    expr_free(five);
    expr_free(six);
    expr_free(seven);
}

/* Keep all solve scenarios available as independently selectable regression tests. */
void test_solve_and_lstsq(void)
{
    TEST_RUN_SUBTEST(test_solve_numeric_pivoting, NULL);
    TEST_RUN_SUBTEST(test_solve_lower_triangular, NULL);
    TEST_RUN_SUBTEST(test_solve_upper_triangular, NULL);
    TEST_RUN_SUBTEST(test_solve_sparse_lower, NULL);
    TEST_RUN_SUBTEST(test_solve_diagonal_sparse_rhs, NULL);
    TEST_RUN_SUBTEST(test_solve_sparse_general, NULL);
    TEST_RUN_SUBTEST(test_solve_sparse_pivoting, NULL);
    TEST_RUN_SUBTEST(test_solve_rank_deficient, NULL);
    TEST_RUN_SUBTEST(test_solve_minimum_norm, NULL);
    TEST_RUN_SUBTEST(test_solve_least_squares, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_least_squares, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_rank_deficient, NULL);
    TEST_RUN_SUBTEST(test_solve_complex, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_lower, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_dense, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_numeric_rhs, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_cramer, NULL);
    TEST_RUN_SUBTEST(test_solve_symbolic_dense_six, NULL);
}
