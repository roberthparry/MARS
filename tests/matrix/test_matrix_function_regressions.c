#include "test_matrix.h"

static void test_mat_fun_3x3(void)
{
    printf(C_CYAN "TEST: 3×3 double matrix functions — general SPD\n" C_RESET);

    /* 3×3 symmetric positive definite — all off-diagonal entries nonzero, */
    /* exercises the full Q·f(T)·Q* round-trip in mat_fun_schur.            */
    double avals[9] = {
        4.0, 2.0, 1.0,
        2.0, 3.0, 1.5,
        1.0, 1.5, 2.0};
    double negavals[9] = {
        -4.0, -2.0, -1.0,
        -2.0, -3.0, -1.5,
        -1.0, -1.5, -2.0};

    matrix_t *A = test_mat_create_d(3, 3, avals);
    matrix_t *negA = test_mat_create_d(3, 3, negavals);

    check_bool("A 3×3 allocated", A != NULL);
    check_bool("negA 3×3 allocated", negA != NULL);
    if (!A || !negA)
    {
        mat_free(A);
        mat_free(negA);
        return;
    }

    print_md("A (3×3 SPD)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("exp(A) not NULL", E != NULL);
        check_bool("exp(-A) not NULL", En != NULL);
        if (E && En)
        {
            print_md("exp(A)", E);
            print_md("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                    check_mat_identity_d("3×3 exp(A)·exp(-A)=I", I, 3, 1e-10);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) 3×3 not NULL", S != NULL);
        check_bool("cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_md("sin(A)", S);
            print_md("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_d("3×3 sin²(A)+cos²(A)=I", I, 3, 1e-10);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_md("cosh(A)", CH);
            print_md("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("3×3 cosh²(A)-sinh²(A)=I", I, 3, 1e-10);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A (A has positive diagonal → positive eigenvalues) */
    {
        matrix_t *L = mat_log(A);
        check_bool("log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("3×3 exp(log(A))=A", R, A, 1e-10);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 4×4 matrix function tests */

/*
 * Symmetric positive-definite 4×4 input gives a larger dense Schur-roundtrip
 * case than the 3×3 tests and helps cover matrix-function products on
 * nontrivial square inputs.
 */
static void test_mat_fun_4x4(void)
{
    printf(C_CYAN "TEST: 4×4 double matrix functions — general SPD\n" C_RESET);

    /* 4×4 symmetric positive definite — diagonally dominant ensures SPD,   */
    /* dense off-diagonal entries exercise the full Q·f(T)·Q* round-trip.   */
    double avals[16] = {
        5.0, 1.0, 0.5, 0.2,
        1.0, 4.0, 1.0, 0.5,
        0.5, 1.0, 3.0, 1.0,
        0.2, 0.5, 1.0, 2.0};
    double negavals[16] = {
        -5.0, -1.0, -0.5, -0.2,
        -1.0, -4.0, -1.0, -0.5,
        -0.5, -1.0, -3.0, -1.0,
        -0.2, -0.5, -1.0, -2.0};

    matrix_t *A = test_mat_create_d(4, 4, avals);
    matrix_t *negA = test_mat_create_d(4, 4, negavals);

    check_bool("A 4×4 allocated", A != NULL);
    check_bool("negA 4×4 allocated", negA != NULL);
    if (!A || !negA)
    {
        mat_free(A);
        mat_free(negA);
        return;
    }

    print_md("A (4×4 SPD)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("exp(A) 4×4 not NULL", E != NULL);
        check_bool("exp(-A) 4×4 not NULL", En != NULL);
        if (E && En)
        {
            print_md("exp(A)", E);
            print_md("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_d("4×4 exp(A)·exp(-A)=I", I, 4, 1e-8);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) 4×4 not NULL", S != NULL);
        check_bool("cos(A) 4×4 not NULL", C != NULL);
        if (S && C)
        {
            print_md("sin(A)", S);
            print_md("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_d("4×4 sin²(A)+cos²(A)=I", I, 4, 1e-8);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("cosh(A) 4×4 not NULL", CH != NULL);
        check_bool("sinh(A) 4×4 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_md("cosh(A)", CH);
            print_md("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("4×4 cosh²(A)-sinh²(A)=I", I, 4, 1e-8);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A (A has positive diagonal) */
    {
        matrix_t *L = mat_log(A);
        check_bool("log(A) 4×4 not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) 4×4 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("4×4 exp(log(A))=A", R, A, 1e-8);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    /* sqrt(A)·sqrt(A) = A (A has positive diagonal) */
    {
        matrix_t *Sq = mat_sqrt(A);
        check_bool("sqrt(A) 4×4 not NULL", Sq != NULL);
        if (Sq)
        {
            print_md("sqrt(A)", Sq);
            matrix_t *R = mat_mul(Sq, Sq);
            check_bool("sqrt(A)² 4×4 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("4×4 sqrt(A)²=A", R, A, 1e-8);
                mat_free(R);
            }
            mat_free(Sq);
        }
    }

    /* erf(A) + erfc(A) = I */
    {
        matrix_t *E = mat_erf(A);
        matrix_t *EC = mat_erfc(A);
        check_bool("erf(A) 4×4 not NULL", E != NULL);
        check_bool("erfc(A) 4×4 not NULL", EC != NULL);
        if (E && EC)
        {
            print_md("erf(A)", E);
            print_md("erfc(A)", EC);
            matrix_t *I = mat_add(E, EC);
            if (I)
            {
                check_mat_identity_d("4×4 erf(A)+erfc(A)=I", I, 4, 1e-8);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(EC);
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 3×3 qfloat matrix function tests */

static void test_mat_fun_3x3_mp_real(void)
{
    printf(C_CYAN "TEST: 3×3 number matrix functions — general SPD\n" C_RESET);

    number_t avals[9];
    number_t negavals[9];
    const double ainit[9] = {4.0, 2.0, 1.0,
                             2.0, 3.0, 1.5,
                             1.0, 1.5, 2.0};
    const double neginit[9] = {-4.0, -2.0, -1.0,
                               -2.0, -3.0, -1.5,
                               -1.0, -1.5, -2.0};
    matrix_t *A;
    matrix_t *negA;

    for (size_t i = 0; i < 9u; ++i) {
        avals[i] = num_create_from_double(ainit[i]);
        negavals[i] = num_create_from_double(neginit[i]);
    }

    A = mat_create(3, 3, avals);
    negA = mat_create(3, 3, negavals);
    for (size_t i = 0; i < 9u; ++i) {
        num_destroy(&avals[i]);
        num_destroy(&negavals[i]);
    }

    check_bool("num A 3×3 allocated", A != NULL);
    check_bool("num negA 3×3 allocated", negA != NULL);
    if (!A || !negA) { mat_free(A); mat_free(negA); return; }

    print_mnum("A (3×3 SPD number)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("num exp(A) 3×3 not NULL", E != NULL);
        check_bool("num exp(-A) 3×3 not NULL", En != NULL);
        if (E && En)
        {
            print_mnum("exp(A)", E);
            print_mnum("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_d("3×3 num exp(A)·exp(-A)=I", I, 3, 1e-12);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("num sin(A) 3×3 not NULL", S != NULL);
        check_bool("num cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_mnum("sin(A)", S);
            print_mnum("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_d("3×3 num sin²(A)+cos²(A)=I", I, 3, 1e-12);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("num cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("num sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_mnum("cosh(A)", CH);
            print_mnum("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("3×3 num cosh²(A)-sinh²(A)=I", I, 3, 1e-10);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A */
    {
        matrix_t *L = mat_log(A);
        check_bool("num log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_mnum("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("num exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("3×3 num exp(log(A))=A", R, A, 1e-12);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 3×3 qcomplex matrix function tests */

static void test_mat_fun_3x3_complex(void)
{
    printf(C_CYAN "TEST: 3×3 qcomplex matrix functions — Hermitian positive definite\n" C_RESET);

    /* Hermitian positive definite — diagonally dominant with Hermitian off-diagonal */
    qcomplex_t avals[9] = {
        qc_make(qf_from_double(4.0), QF_ZERO),
        qc_make(qf_from_double(1.0), qf_from_double( 0.5)),
        qc_make(qf_from_double(0.5), qf_from_double(-0.3)),
        qc_make(qf_from_double(1.0), qf_from_double(-0.5)),
        qc_make(qf_from_double(3.0), QF_ZERO),
        qc_make(qf_from_double(0.8), qf_from_double( 0.2)),
        qc_make(qf_from_double(0.5), qf_from_double( 0.3)),
        qc_make(qf_from_double(0.8), qf_from_double(-0.2)),
        qc_make(qf_from_double(2.0), QF_ZERO)};
    qcomplex_t negavals[9] = {
        qc_make(qf_from_double(-4.0), QF_ZERO),
        qc_make(qf_from_double(-1.0), qf_from_double(-0.5)),
        qc_make(qf_from_double(-0.5), qf_from_double( 0.3)),
        qc_make(qf_from_double(-1.0), qf_from_double( 0.5)),
        qc_make(qf_from_double(-3.0), QF_ZERO),
        qc_make(qf_from_double(-0.8), qf_from_double(-0.2)),
        qc_make(qf_from_double(-0.5), qf_from_double(-0.3)),
        qc_make(qf_from_double(-0.8), qf_from_double( 0.2)),
        qc_make(qf_from_double(-2.0), QF_ZERO)};

    matrix_t *A    = test_mat_create_complex(3, 3, avals);
    matrix_t *negA = test_mat_create_complex(3, 3, negavals);

    check_bool("qc A 3×3 allocated", A != NULL);
    check_bool("qc negA 3×3 allocated", negA != NULL);
    if (!A || !negA) { mat_free(A); mat_free(negA); return; }

    print_mqc("A (3×3 HPD qcomplex)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("qc exp(A) 3×3 not NULL", E != NULL);
        check_bool("qc exp(-A) 3×3 not NULL", En != NULL);
        if (E && En)
        {
            print_mqc("exp(A)", E);
            print_mqc("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_complex("3×3 complex exp(A)·exp(-A)=I", I, 3, 1e-25);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("qc sin(A) 3×3 not NULL", S != NULL);
        check_bool("qc cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_mqc("sin(A)", S);
            print_mqc("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_complex("3×3 complex sin²(A)+cos²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("qc cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("qc sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_mqc("cosh(A)", CH);
            print_mqc("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_complex("3×3 complex cosh²(A)-sinh²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A */
    {
        matrix_t *L = mat_log(A);
        check_bool("qc log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_mqc("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("qc exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_complex("3×3 complex exp(log(A))=A", R, A, 1e-25);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ error-handling tests */

static void test_mat_error_handling(void)
{
    printf(C_CYAN "TEST: error handling — NULL and non-square inputs\n" C_RESET);

    /* mat_det */
    {
        double out = 0.0;
        check_bool("mat_det(NULL) = -1", mat_det(NULL, &out) == -1);
        matrix_t *rect = test_mat_dense_d(2, 3);
        check_bool("mat_det(2×3) = -2", mat_det(rect, &out) == -2);
        mat_free(rect);
    }

    /* mat_inverse */
    {
        check_bool("mat_inverse(NULL) = NULL", mat_inverse(NULL) == NULL);
        matrix_t *rect = test_mat_dense_d(3, 2);
        check_bool("mat_inverse(3×2) = NULL", mat_inverse(rect) == NULL);
        mat_free(rect);
    }

    /* mat_solve / mat_least_squares */
    {
        matrix_t *A = test_mat_dense_d(2, 3);
        matrix_t *B = test_mat_dense_d(2, 1);
        matrix_t *C = test_mat_dense_d(3, 1);
        matrix_t *D = test_mat_dense_d(2, 2);
        check_bool("mat_solve(NULL,NULL) = NULL", mat_solve(NULL, NULL) == NULL);
        check_bool("mat_solve(2×3,2×1) = NULL", mat_solve(A, B) == NULL);
        check_bool("mat_solve(2×2,3×1) = NULL", mat_solve(D, C) == NULL);
        check_bool("mat_least_squares(NULL,NULL) = NULL", mat_least_squares(NULL, NULL) == NULL);
        check_bool("mat_least_squares(2×3,3×1) = NULL", mat_least_squares(A, C) == NULL);
        mat_free(A);
        mat_free(B);
        mat_free(C);
        mat_free(D);
    }

    /* factorisation entry points */
    {
        matrix_t *rect = test_mat_dense_d(2, 3);
        mat_lu_factor_t lu = {0};
        mat_qr_factor_t qr = {0};
        mat_schur_factor_t schur = {0};
        mat_cholesky_t chol = {0};
        mat_svd_factor_t svd = {0};

        check_bool("mat_lu_factor(NULL) < 0", mat_lu_factor(NULL, &lu) < 0);
        check_bool("mat_lu_factor(2x3) < 0", mat_lu_factor(rect, &lu) < 0);
        check_bool("mat_qr_factor(NULL) < 0", mat_qr_factor(NULL, &qr) < 0);
        check_bool("mat_qr_factor(2x3) = 0 or better", mat_qr_factor(rect, &qr) == 0);
        check_bool("mat_schur_factor(NULL) < 0", mat_schur_factor(NULL, &schur) < 0);
        check_bool("mat_schur_factor(2x3) < 0", mat_schur_factor(rect, &schur) < 0);
        check_bool("mat_cholesky(NULL) < 0", mat_cholesky(NULL, &chol) < 0);
        check_bool("mat_cholesky(2x3) < 0", mat_cholesky(rect, &chol) < 0);
        check_bool("mat_svd_factor(NULL) < 0", mat_svd_factor(NULL, &svd) < 0);
        check_bool("mat_svd_factor(2x3) = 0 or better", mat_svd_factor(rect, &svd) == 0);
        check_bool("mat_rank(NULL) < 0", mat_rank(NULL) < 0);
        check_bool("mat_pseudoinverse(NULL) = NULL", mat_pseudoinverse(NULL) == NULL);
        check_bool("mat_nullspace(NULL) = NULL", mat_nullspace(NULL) == NULL);
        check_bool("mat_norm(NULL,1,NULL) < 0", mat_norm(NULL, MAT_NORM_1, NULL) < 0);
        check_bool("mat_condition_number(NULL,1,NULL) < 0", mat_condition_number(NULL, MAT_NORM_1, NULL) < 0);
        check_bool("mat_is_sparse(NULL) = 0", mat_is_sparse(NULL) == 0);
        check_bool("mat_nonzero_count(NULL) = 0", mat_nonzero_count(NULL) == 0);
        check_bool("mat_to_sparse(NULL) = NULL", mat_to_sparse(NULL) == NULL);
        check_bool("mat_to_dense(NULL) = NULL", mat_to_dense(NULL) == NULL);

        mat_qr_factor_free(&qr);
        mat_schur_factor_free(&schur);
        mat_svd_factor_free(&svd);
        mat_free(rect);
    }

    /* mat_eigenvalues */
    {
        number_t ev[4] = {num_new(), num_new(), num_new(), num_new()};
        check_bool("mat_eigenvalues(NULL) < 0", mat_eigenvalues(NULL, ev) < 0);
        matrix_t *rect = test_mat_dense_d(2, 3);
        check_bool("mat_eigenvalues(2×3) < 0", mat_eigenvalues(rect, ev) < 0);
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&ev[i]);
        mat_free(rect);
    }

    /* mat_eigendecompose */
    {
        number_t ev[4] = {num_new(), num_new(), num_new(), num_new()};
        matrix_t *evecs = NULL;
        check_bool("mat_eigendecompose(NULL) < 0",
                   mat_eigendecompose(NULL, ev, &evecs) < 0);
        matrix_t *rect = test_mat_dense_d(2, 3);
        check_bool("mat_eigendecompose(2×3) < 0",
                   mat_eigendecompose(rect, ev, &evecs) < 0);
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&ev[i]);
        mat_free(rect);
    }

    /* dimension mismatch in arithmetic */
    {
        matrix_t *A = test_mat_dense_d(2, 3);
        matrix_t *B = test_mat_dense_d(3, 2);
        matrix_t *C = test_mat_dense_d(4, 4);

        check_bool("mat_add(2×3, 3×2) = NULL", mat_add(A, B) == NULL);
        check_bool("mat_sub(2×3, 3×2) = NULL", mat_sub(A, B) == NULL);
        check_bool("mat_mul(2×3, 4×4) = NULL", mat_mul(A, C) == NULL);

        mat_free(A);
        mat_free(B);
        mat_free(C);
    }

    /* mat_pow_int on non-square */
    {
        matrix_t *rect = test_mat_dense_d(2, 3);
        check_bool("mat_pow_int(2×3, 2) = NULL", mat_pow_int(rect, 2) == NULL);
        mat_free(rect);
    }
}

/* ------------------------------------------------------------------ qf/qc variants of under-tested functions */

static void test_mat_fun_mp_real_complex(void)
{
    printf(C_CYAN "TEST: number variants of tan, hyperbolic, log, erf, erfc, pow_int\n" C_RESET);

    number_t vals[4];
    matrix_t *A_num;

    vals[0] = num_create_from_double(0.3);
    vals[1] = num_create_from_double(0.1);
    vals[2] = num_create_from_double(0.1);
    vals[3] = num_create_from_double(0.4);
    A_num = mat_create(2, 2, vals);
    for (size_t i = 0; i < 4u; ++i)
        num_destroy(&vals[i]);
    check_bool("A_num allocated", A_num != NULL);
    if (!A_num)
        return;

    /* tan: cos(A)·tan(A) = sin(A) */
    {
        matrix_t *T = mat_tan(A_num);
        matrix_t *S = mat_sin(A_num);
        matrix_t *C = mat_cos(A_num);
        check_bool("num tan(A) not NULL", T != NULL);
        if (T && S && C)
        {
            matrix_t *CT = mat_mul(C, T);
            if (CT)
            {
                check_mat_d("num 2×2 cos·tan=sin", CT, S, 1e-12);
                mat_free(CT);
            }
        }
        mat_free(T);
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A_num);
        matrix_t *SH = mat_sinh(A_num);
        check_bool("num cosh(A) not NULL", CH != NULL);
        check_bool("num sinh(A) not NULL", SH != NULL);
        if (CH && SH)
        {
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("num 2×2 cosh²-sinh²=I", I, 2, 1e-12);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* tanh: cosh(A)·tanh(A) = sinh(A) */
    {
        matrix_t *TH = mat_tanh(A_num);
        matrix_t *CH = mat_cosh(A_num);
        matrix_t *SH = mat_sinh(A_num);
        check_bool("num tanh(A) not NULL", TH != NULL);
        if (TH && CH && SH)
        {
            matrix_t *CT = mat_mul(CH, TH);
            if (CT)
            {
                check_mat_d("num 2×2 cosh·tanh=sinh", CT, SH, 1e-12);
                mat_free(CT);
            }
        }
        mat_free(TH);
        mat_free(CH);
        mat_free(SH);
    }

    /* log: exp(log(A)) = A for positive-definite A */
    {
        number_t pdvals[4];
        matrix_t *PD;
        matrix_t *L;
        pdvals[0] = num_create_from_double(2.0);
        pdvals[1] = num_create_from_double(0.5);
        pdvals[2] = num_create_from_double(0.5);
        pdvals[3] = num_create_from_double(2.0);
        PD = mat_create(2, 2, pdvals);
        for (size_t i = 0; i < 4u; ++i)
            num_destroy(&pdvals[i]);
        L = mat_log(PD);
        check_bool("num log(PD) not NULL", L != NULL);
        if (L)
        {
            matrix_t *R = mat_exp(L);
            check_bool("num exp(log(PD)) not NULL", R != NULL);
            if (R)
            {
                check_mat_d("num 2×2 exp(log(A))=A", R, PD, 1e-12);
                mat_free(R);
            }
            mat_free(L);
        }
        mat_free(PD);
    }

    /* erf + erfc = I */
    {
        matrix_t *E = mat_erf(A_num);
        matrix_t *EC = mat_erfc(A_num);
        check_bool("num erf(A) not NULL", E != NULL);
        check_bool("num erfc(A) not NULL", EC != NULL);
        if (E && EC)
        {
            matrix_t *I = mat_add(E, EC);
            if (I)
            {
                check_mat_identity_d("num 2×2 erf+erfc=I", I, 2, 1e-12);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(EC);
    }

    /* pow_int for num: diag(2,3)^3 = diag(8,27) */
    {
        number_t dvals[4];
        matrix_t *D;
        matrix_t *R;
        double r00, r11, r01, r10;
        dvals[0] = num_create_from_double(2.0);
        dvals[1] = NUM_ZERO;
        dvals[2] = NUM_ZERO;
        dvals[3] = num_create_from_double(3.0);
        D = mat_create(2, 2, dvals);
        num_destroy(&dvals[0]);
        num_destroy(&dvals[3]);
        R = mat_pow_int(D, 3);
        check_bool("num pow_int(diag(2,3),3) not NULL", R != NULL);
        if (R)
        {
            mat_get(R, 0, 0, &r00);
            mat_get(R, 1, 1, &r11);
            mat_get(R, 0, 1, &r01);
            mat_get(R, 1, 0, &r10);
            check_d("num diag(2,3)^3 [0,0]=8", r00, 8.0, 1e-12);
            check_d("num diag(2,3)^3 [1,1]=27", r11, 27.0, 1e-12);
            check_d("num diag(2,3)^3 [0,1]=0", r01, 0.0, 1e-12);
            check_d("num diag(2,3)^3 [1,0]=0", r10, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(D);
    }

    /* real 2x2: exp(A)·exp(-A) = I */
    {
        number_t expvals[4];
        number_t neg_expvals[4];
        matrix_t *A_exp;
        matrix_t *negA_exp;
        matrix_t *E;
        matrix_t *En;

        expvals[0] = num_create_from_double(0.3);
        expvals[1] = num_create_from_double(0.1);
        expvals[2] = num_create_from_double(0.1);
        expvals[3] = num_create_from_double(0.4);
        neg_expvals[0] = num_create_from_double(-0.3);
        neg_expvals[1] = num_create_from_double(-0.1);
        neg_expvals[2] = num_create_from_double(-0.1);
        neg_expvals[3] = num_create_from_double(-0.4);
        A_exp = mat_create(2, 2, expvals);
        negA_exp = mat_create(2, 2, neg_expvals);
        for (size_t i = 0; i < 4u; ++i) {
            num_destroy(&expvals[i]);
            num_destroy(&neg_expvals[i]);
        }
        E = mat_exp(A_exp);
        En = mat_exp(negA_exp);
        check_bool("num exp(A) not NULL", E != NULL);
        check_bool("num exp(-A) not NULL", En != NULL);
        if (E && En)
        {
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_d("num 2×2 exp(A)·exp(-A)=I", I, 2, 1e-12);
                mat_free(I);
            }
        }
        mat_free(A_exp);
        mat_free(negA_exp);
        mat_free(E);
        mat_free(En);
    }

    mat_free(A_num);
}

/* ------------------------------------------------------------------ tests_main */

void run_matrix_function_regression_tests(void)
{
    RUN_TEST_CASE(test_mat_fun_mp_real_complex);
    RUN_TEST_CASE(test_mat_error_handling);
    RUN_TEST_CASE(test_mat_fun_3x3);
    RUN_TEST_CASE(test_mat_fun_4x4);
    RUN_TEST_CASE(test_mat_fun_3x3_mp_real);
    RUN_TEST_CASE(test_mat_fun_3x3_complex);
}
