#include "test_matrix.h"

static void test_eigen_d(void)
{
    printf(C_CYAN "TEST: eigendecomposition (double)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    double A_vals[4] = {5, 2, 2, 8};
    matrix_t *A = mat_create_d(2, 2, A_vals);

    print_md("A", A);

    /* eigenvalues only */
    double ev[2];
    mat_eigenvalues(A, ev);
    printf("    eigenvalues (mat_eigenvalues): [%.15g, %.15g]\n", ev[0], ev[1]);

    double lmin = fmin(ev[0], ev[1]);
    double lmax = fmax(ev[0], ev[1]);
    check_d("eigenvalue min = 4", lmin, 4.0, 1e-10);
    check_d("eigenvalue max = 9", lmax, 9.0, 1e-10);

    /* full decomposition */
    double ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_md("eigenvectors V (columns)", V);
    printf("    eigenvalues (mat_eigendecompose): [%.15g, %.15g]\n", ev2[0], ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] for each column k */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            double Av_ik = 0.0;
            for (size_t j = 0; j < 2; j++)
            {
                double aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik += aij * vjk;
            }
            double vik;
            mat_get(V, i, k, &vik);
            double lv_ik = ev2[k] * vik;

            char label[64];
            snprintf(label, sizeof(label), "d: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_d(label, Av_ik, lv_ik, 1e-10);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_md("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qfloat */

static void test_eigen_qf(void)
{
    printf(C_CYAN "TEST: eigendecomposition (qfloat)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    qfloat_t A_vals[4] = {
        qf_from_double(5), qf_from_double(2),
        qf_from_double(2), qf_from_double(8)};
    matrix_t *A = mat_create_qf(2, 2, A_vals);

    print_mqf("A", A);

    /* eigenvalues only */
    qfloat_t ev[2];
    mat_eigenvalues(A, ev);
    print_qf("eigenvalue[0]", ev[0]);
    print_qf("eigenvalue[1]", ev[1]);

    int e0_smaller = qf_to_double(ev[0]) < qf_to_double(ev[1]);
    qfloat_t ev_min = e0_smaller ? ev[0] : ev[1];
    qfloat_t ev_max = e0_smaller ? ev[1] : ev[0];
    check_qf_val("eigenvalue min = 4", ev_min, qf_from_double(4.0), 1e-25);
    check_qf_val("eigenvalue max = 9", ev_max, qf_from_double(9.0), 1e-25);

    /* full decomposition */
    qfloat_t ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_mqf("eigenvectors V (columns)", V);
    print_qf("eigenvalue2[0]", ev2[0]);
    print_qf("eigenvalue2[1]", ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            qfloat_t Av_ik = QF_ZERO;
            for (size_t j = 0; j < 2; j++)
            {
                qfloat_t aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik = qf_add(Av_ik, qf_mul(aij, vjk));
            }
            qfloat_t vik;
            mat_get(V, i, k, &vik);
            qfloat_t lv_ik = qf_mul(ev2[k], vik);

            char label[64];
            snprintf(label, sizeof(label), "qf: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_qf_val(label, Av_ik, lv_ik, 1e-25);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mqf("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qcomplex */

static void test_eigen_qc(void)
{
    printf(C_CYAN "TEST: eigendecomposition (qcomplex Hermitian)\n" C_RESET);

    /* A = [[2, 1+i], [1-i, 3]] — eigenvalues 1 and 4 */
    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(2), QF_ZERO),
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(1), qf_from_double(-1)),
        qc_make(qf_from_double(3), QF_ZERO)};
    matrix_t *A = mat_create_qc(2, 2, A_vals);
    print_mqc("A", A);

    /* eigenvalues only */
    qcomplex_t ev[2];
    mat_eigenvalues(A, ev);
    print_qc("eigenvalue[0]", ev[0]);
    print_qc("eigenvalue[1]", ev[1]);

    int e0_smaller = qf_to_double(ev[0].re) < qf_to_double(ev[1].re);
    qcomplex_t ev_min = e0_smaller ? ev[0] : ev[1];
    qcomplex_t ev_max = e0_smaller ? ev[1] : ev[0];
    qcomplex_t exp_min = qc_make(qf_from_double(1), QF_ZERO);
    qcomplex_t exp_max = qc_make(qf_from_double(4), QF_ZERO);
    check_qc_val("eigenvalue min = 1+0i", ev_min, exp_min, 1e-25);
    check_qc_val("eigenvalue max = 4+0i", ev_max, exp_max, 1e-25);

    /* full decomposition */
    qcomplex_t ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);
    print_mqc("eigenvectors V (columns)", V);
    print_qc("eigenvalue2[0]", ev2[0]);
    print_qc("eigenvalue2[1]", ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            qcomplex_t Av_ik = QC_ZERO;
            for (size_t j = 0; j < 2; j++)
            {
                qcomplex_t aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik = qc_add(Av_ik, qc_mul(aij, vjk));
            }
            qcomplex_t vik;
            mat_get(V, i, k, &vik);
            qcomplex_t lv_ik = qc_mul(ev2[k], vik);

            char label[64];
            snprintf(label, sizeof(label), "qc: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_qc_val(label, Av_ik, lv_ik, 1e-25);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mqc("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigenvalues: dval */

static void check_dval_eigen_relation(const char *label_prefix,
                                      const matrix_t *A,
                                      dval_t **ev,
                                      const matrix_t *V,
                                      double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    matrix_t *D = mat_create_diagonal_dv(rows, ev);
    matrix_t *AV = mat_mul(A, V);
    matrix_t *VD = mat_mul(V, D);
    matrix_t *AVq = mat_evaluate_qf(AV);
    matrix_t *VDq = mat_evaluate_qf(VD);

    check_bool("dval eig relation D not NULL", D != NULL);
    check_bool("dval eig relation AV not NULL", AV != NULL);
    check_bool("dval eig relation VD not NULL", VD != NULL);
    check_bool("dval eig relation AVq not NULL", AVq != NULL);
    check_bool("dval eig relation VDq not NULL", VDq != NULL);

    if (AVq && VDq) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                qfloat_t lhs;
                qfloat_t rhs;
                char label[128];

                mat_get(AVq, i, j, &lhs);
                mat_get(VDq, i, j, &rhs);
                snprintf(label, sizeof(label), "%s: AV[%zu,%zu]=VD[%zu,%zu]",
                         label_prefix, i, j, i, j);
                check_qf_val(label, lhs, rhs, tol);
            }
        }
    }

    mat_free(D);
    mat_free(AV);
    mat_free(VD);
    mat_free(AVq);
    mat_free(VDq);
}

static void check_dval_eigenspace_relation(const char *label_prefix,
                                           const matrix_t *A,
                                           dval_t *lambda,
                                           const matrix_t *E,
                                           double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(E);
    dval_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *AE = mat_mul(A, E);
    matrix_t *ED = NULL;
    matrix_t *AEq = NULL;
    matrix_t *EDq = NULL;

    diag_vals = cols ? malloc(cols * sizeof(*diag_vals)) : NULL;
    check_bool("dval eigenspace diag alloc ok", cols == 0 || diag_vals != NULL);
    if (cols && !diag_vals)
        goto cleanup;

    for (size_t j = 0; j < cols; ++j)
        diag_vals[j] = lambda;

    D = mat_create_diagonal_dv(cols, diag_vals);
    ED = mat_mul(E, D);
    AEq = mat_evaluate_qf(AE);
    EDq = mat_evaluate_qf(ED);

    check_bool("dval eigenspace D not NULL", D != NULL);
    check_bool("dval eigenspace AE not NULL", AE != NULL);
    check_bool("dval eigenspace ED not NULL", ED != NULL);
    check_bool("dval eigenspace AEq not NULL", AEq != NULL);
    check_bool("dval eigenspace EDq not NULL", EDq != NULL);

    if (AEq && EDq) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                qfloat_t lhs;
                qfloat_t rhs;
                char label[128];

                mat_get(AEq, i, j, &lhs);
                mat_get(EDq, i, j, &rhs);
                snprintf(label, sizeof(label), "%s: AE[%zu,%zu]=LE[%zu,%zu]",
                         label_prefix, i, j, i, j);
                check_qf_val(label, lhs, rhs, tol);
            }
        }
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(AE);
    mat_free(ED);
    mat_free(AEq);
    mat_free(EDq);
}

static void check_dval_generalized_eigenspace_relation(const char *label_prefix,
                                                       const matrix_t *A,
                                                       dval_t *lambda,
                                                       size_t order,
                                                       const matrix_t *E,
                                                       double tol)
{
    dval_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *Residual = NULL;
    matrix_t *Residual_qf = NULL;

    check_bool("dval generalized eigenspace input not NULL",
               A != NULL && lambda != NULL && E != NULL);
    if (!A || !lambda || !E)
        return;

    diag_vals = malloc(mat_get_row_count(A) * sizeof(*diag_vals));
    check_bool("dval generalized eigenspace diag alloc ok", diag_vals != NULL);
    if (!diag_vals)
        goto cleanup;

    for (size_t i = 0; i < mat_get_row_count(A); ++i)
        diag_vals[i] = lambda;

    D = mat_create_diagonal_dv(mat_get_row_count(A), diag_vals);
    Shifted = mat_sub(A, D);
    check_bool("dval generalized eigenspace diag matrix ok", D != NULL);
    check_bool("dval generalized eigenspace shifted ok", Shifted != NULL);
    if (!D || !Shifted)
        goto cleanup;

    Power = mat_pow_int(Shifted, (int)order);
    Residual = mat_mul(Power, E);
    Residual_qf = mat_evaluate_qf(Residual);

    check_bool("dval generalized eigenspace power not NULL", Power != NULL);
    check_bool("dval generalized eigenspace residual not NULL", Residual != NULL);
    check_bool("dval generalized eigenspace residual_qf not NULL", Residual_qf != NULL);

    if (Residual_qf) {
        for (size_t i = 0; i < mat_get_row_count(Residual_qf); ++i) {
            for (size_t j = 0; j < mat_get_col_count(Residual_qf); ++j) {
                qfloat_t got;
                char label[128];

                mat_get(Residual_qf, i, j, &got);
                snprintf(label, sizeof(label), "%s: ((A-LI)^k E)[%zu,%zu]",
                         label_prefix, i, j);
                check_qf_val(label, got, QF_ZERO, tol);
            }
        }
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(Shifted);
    mat_free(Power);
    mat_free(Residual);
    mat_free(Residual_qf);
}

static matrix_t *copy_dval_column(const matrix_t *A, size_t col)
{
    matrix_t *C;

    if (!A || col >= mat_get_col_count(A))
        return NULL;

    C = mat_new_dv(mat_get_row_count(A), 1);
    if (!C)
        return NULL;

    for (size_t i = 0; i < mat_get_row_count(A); ++i) {
        dval_t *v = NULL;
        mat_get(A, i, col, &v);
        mat_set(C, i, 0, &v);
    }

    return C;
}

static void check_dval_jordan_chain_relation(const char *label_prefix,
                                             const matrix_t *A,
                                             dval_t *lambda,
                                             const matrix_t *Chain,
                                             double tol)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(Chain);
    dval_t **diag_vals = NULL;
    matrix_t *D = NULL;
    matrix_t *Shifted = NULL;
    matrix_t *Prev = NULL;
    matrix_t *SC = NULL;
    matrix_t *SCq = NULL;
    matrix_t *Prevq = NULL;

    check_bool("dval jordan chain input not NULL",
               A != NULL && lambda != NULL && Chain != NULL);
    if (!A || !lambda || !Chain)
        return;

    diag_vals = malloc(rows * sizeof(*diag_vals));
    check_bool("dval jordan chain diag alloc ok", diag_vals != NULL);
    if (!diag_vals)
        goto cleanup;

    for (size_t i = 0; i < rows; ++i)
        diag_vals[i] = lambda;

    D = mat_create_diagonal_dv(rows, diag_vals);
    Shifted = mat_sub(A, D);
    check_bool("dval jordan chain D not NULL", D != NULL);
    check_bool("dval jordan chain shifted not NULL", Shifted != NULL);
    if (!D || !Shifted)
        goto cleanup;

    for (size_t j = 0; j < cols; ++j) {
        matrix_t *Col = copy_dval_column(Chain, j);
        check_bool("dval jordan chain column copy ok", Col != NULL);
        if (!Col)
            goto cleanup;

        mat_free(SC);
        mat_free(SCq);
        SC = mat_mul(Shifted, Col);
        SCq = mat_evaluate_qf(SC);
        check_bool("dval jordan chain shifted col not NULL", SC != NULL);
        check_bool("dval jordan chain shifted col qf not NULL", SCq != NULL);
        if (!SC || !SCq) {
            mat_free(Col);
            goto cleanup;
        }

        if (j == 0) {
            for (size_t i = 0; i < rows; ++i) {
                qfloat_t got;
                char label[128];

                mat_get(SCq, i, 0, &got);
                snprintf(label, sizeof(label), "%s: (A-LI)v1[%zu]", label_prefix, i);
                check_qf_val(label, got, QF_ZERO, tol);
            }
        } else {
            mat_free(Prev);
            mat_free(Prevq);
            Prev = copy_dval_column(Chain, j - 1);
            Prevq = mat_evaluate_qf(Prev);
            check_bool("dval jordan chain prev col not NULL", Prev != NULL);
            check_bool("dval jordan chain prev col qf not NULL", Prevq != NULL);
            if (!Prev || !Prevq) {
                mat_free(Col);
                goto cleanup;
            }

            for (size_t i = 0; i < rows; ++i) {
                qfloat_t got;
                qfloat_t expected;
                char label[128];

                mat_get(SCq, i, 0, &got);
                mat_get(Prevq, i, 0, &expected);
                snprintf(label, sizeof(label), "%s: (A-LI)v%zu[%zu]=v%zu[%zu]",
                         label_prefix, j + 1, i, j, i);
                check_qf_val(label, got, expected, tol);
            }
        }

        mat_free(Col);
    }

cleanup:
    free(diag_vals);
    mat_free(D);
    mat_free(Shifted);
    mat_free(Prev);
    mat_free(SC);
    mat_free(SCq);
    mat_free(Prevq);
}

static void test_eigen_dval(void)
{
    printf(C_CYAN "TEST: eigendecomposition (dval)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(3.0, "y");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *two = dv_new_const_d(2.0);
        dval_t *five = dv_new_const_d(5.0);
        dval_t *seven = dv_new_const_d(7.0);
        dval_t *vals[16] = {
            x,   one, two, DV_ZERO,
            DV_ZERO, y, one, DV_ZERO,
            DV_ZERO, DV_ZERO, five, one,
            DV_ZERO, DV_ZERO, DV_ZERO, seven};
        dval_t *ev[4] = {NULL, NULL, NULL, NULL};
        dval_t *ev2[4] = {NULL, NULL, NULL, NULL};
        matrix_t *A = mat_create_dv(4, 4, vals);
        matrix_t *V = NULL;

        print_mdv("A", A);
        check_bool("mat_eigenvalues(dval triangular) rc = 0",
                   mat_eigenvalues(A, ev) == 0);
        check_bool("dval triangular eigenvalue[0] non-null", ev[0] != NULL);
        check_bool("dval triangular eigenvalue[1] non-null", ev[1] != NULL);
        check_bool("dval triangular eigenvalue[2] non-null", ev[2] != NULL);
        check_bool("dval triangular eigenvalue[3] non-null", ev[3] != NULL);
        if (ev[0] && ev[1] && ev[2] && ev[3]) {
            check_d("dval triangular eigenvalue[0] = x", dv_eval_d(ev[0]), 2.0, 1e-12);
            check_d("dval triangular eigenvalue[1] = y", dv_eval_d(ev[1]), 3.0, 1e-12);
            check_d("dval triangular eigenvalue[2] = 5", dv_eval_d(ev[2]), 5.0, 1e-12);
            check_d("dval triangular eigenvalue[3] = 7", dv_eval_d(ev[3]), 7.0, 1e-12);
            dv_set_val_d(x, 11.0);
            dv_set_val_d(y, 13.0);
            check_d("dval triangular eigenvalue[0] tracks x", dv_eval_d(ev[0]), 11.0, 1e-12);
            check_d("dval triangular eigenvalue[1] tracks y", dv_eval_d(ev[1]), 13.0, 1e-12);
        }

        check_bool("mat_eigendecompose(dval triangular distinct) rc = 0",
                   mat_eigendecompose(A, ev2, &V) == 0);
        check_bool("dval triangular eigenvectors not NULL", V != NULL);
        if (V)
            check_dval_eigen_relation("dval triangular", A, ev2, V, 1e-20);

        for (size_t i = 0; i < 4; ++i)
            dv_free(ev[i]);
        for (size_t i = 0; i < 4; ++i)
            dv_free(ev2[i]);
        mat_free(A);
        mat_free(V);
        dv_free(x);
        dv_free(y);
        dv_free(one);
        dv_free(two);
        dv_free(five);
        dv_free(seven);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(5.0, "y");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, y, DV_ZERO,
            DV_ZERO, DV_ZERO, x};
        dval_t *ev[3] = {NULL, NULL, NULL};
        dval_t *ev2[3] = {NULL, NULL, NULL};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *V = NULL;

        print_mdv("A (triangular repeated dval)", A);
        check_bool("mat_eigenvalues(dval triangular repeated) rc = 0",
                   mat_eigenvalues(A, ev) == 0);
        check_bool("mat_eigendecompose(dval triangular repeated diagonalizable) rc = 0",
                   mat_eigendecompose(A, ev2, &V) == 0);
        check_bool("dval triangular repeated eigenvectors not NULL", V != NULL);
        if (V)
            check_dval_eigen_relation("dval triangular repeated", A, ev2, V, 1e-20);

        for (size_t i = 0; i < 3; ++i)
            dv_free(ev[i]);
        for (size_t i = 0; i < 3; ++i)
            dv_free(ev2[i]);
        mat_free(A);
        mat_free(V);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, one, x};
        dval_t *ev[2] = {NULL, NULL};
        dval_t *ev2[2] = {NULL, NULL};
        matrix_t *A = mat_create_dv(2, 2, vals);
        double ev0, ev1;
        double lo, hi;
        matrix_t *V = NULL;
        matrix_t *V2 = NULL;

        print_mdv("A (dense 2x2 dval)", A);
        check_bool("mat_eigenvalues(dval dense 2x2) rc = 0",
                   mat_eigenvalues(A, ev) == 0);
        check_bool("dval dense eigenvalue[0] non-null", ev[0] != NULL);
        check_bool("dval dense eigenvalue[1] non-null", ev[1] != NULL);
        if (ev[0] && ev[1]) {
            ev0 = dv_eval_d(ev[0]);
            ev1 = dv_eval_d(ev[1]);
            lo = fmin(ev0, ev1);
            hi = fmax(ev0, ev1);
            check_d("dval dense 2x2 eigenvalue min = x-1", lo, 2.0, 1e-12);
            check_d("dval dense 2x2 eigenvalue max = x+1", hi, 4.0, 1e-12);

            dv_set_val_d(x, 10.0);
            ev0 = dv_eval_d(ev[0]);
            ev1 = dv_eval_d(ev[1]);
            lo = fmin(ev0, ev1);
            hi = fmax(ev0, ev1);
            check_d("dval dense 2x2 eigenvalue min tracks x", lo, 9.0, 1e-12);
            check_d("dval dense 2x2 eigenvalue max tracks x", hi, 11.0, 1e-12);
        }

        check_bool("mat_eigendecompose(dval dense 2x2) rc = 0",
                   mat_eigendecompose(A, ev2, &V) == 0);
        check_bool("dval dense 2x2 eigenvectors not NULL", V != NULL);
        if (V)
            check_dval_eigen_relation("dval dense 2x2", A, ev2, V, 1e-20);

        V2 = mat_eigenvectors(A);
        check_bool("mat_eigenvectors(dval dense 2x2) not NULL", V2 != NULL);

        dv_free(ev[0]);
        dv_free(ev[1]);
        dv_free(ev2[0]);
        dv_free(ev2[1]);
        mat_free(A);
        mat_free(V);
        mat_free(V2);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, DV_ZERO, x};
        dval_t *ev[2] = {NULL, NULL};
        dval_t *ev2[2] = {NULL, NULL};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *V = NULL;

        check_bool("mat_eigenvalues(dval Jordan 2x2) rc = 0",
                   mat_eigenvalues(A, ev) == 0);
        check_bool("mat_eigendecompose(dval Jordan 2x2) remains unsupported",
                   mat_eigendecompose(A, ev2, &V) < 0 && V == NULL);

        dv_free(ev[0]);
        dv_free(ev[1]);
        dv_free(ev2[0]);
        dv_free(ev2[1]);
        mat_free(A);
        dv_free(x);
        dv_free(one);
    }
}

static void test_eigenspace_dval(void)
{
    printf(C_CYAN "TEST: eigenspace (dval)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(5.0, "y");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, y, DV_ZERO,
            DV_ZERO, DV_ZERO, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *E = mat_eigenspace(A, &x);

        print_mdv("A (eigenspace repeated dval)", A);
        check_bool("mat_eigenspace(dval repeated triangular) not NULL", E != NULL);
        if (E) {
            print_mdv("eigenspace_x(A)", E);
            check_bool("eigenspace rows = 3", mat_get_row_count(E) == 3);
            check_bool("eigenspace cols = 2", mat_get_col_count(E) == 2);
            check_dval_eigenspace_relation("dval repeated eigenspace", A, x, E, 1e-20);
            dv_set_val_d(x, 11.0);
            dv_set_val_d(y, 13.0);
            check_dval_eigenspace_relation("dval repeated eigenspace tracks", A, x, E, 1e-20);
        }

        mat_free(A);
        mat_free(E);
        dv_free(x);
        dv_free(y);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, DV_ZERO, x};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *E = mat_eigenspace(A, &x);

        print_mdv("A (Jordan eigenspace dval)", A);
        check_bool("mat_eigenspace(dval Jordan 2x2) not NULL", E != NULL);
        if (E) {
            print_mdv("eigenspace_x(Jordan A)", E);
            check_bool("Jordan eigenspace rows = 2", mat_get_row_count(E) == 2);
            check_bool("Jordan eigenspace cols = 1", mat_get_col_count(E) == 1);
            check_dval_eigenspace_relation("dval Jordan eigenspace", A, x, E, 1e-20);
            dv_set_val_d(x, 9.0);
            check_dval_eigenspace_relation("dval Jordan eigenspace tracks", A, x, E, 1e-20);
        }

        mat_free(A);
        mat_free(E);
        dv_free(x);
        dv_free(one);
    }
}

static void test_generalized_eigenspace_dval(void)
{
    printf(C_CYAN "TEST: generalized eigenspace (dval)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, DV_ZERO, x};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *G = mat_generalized_eigenspace(A, &x, 2);

        print_mdv("A (Jordan generalized eigenspace dval)", A);
        check_bool("mat_generalized_eigenspace(dval Jordan 2x2,2) not NULL", G != NULL);
        if (G) {
            print_mdv("gen_eigenspace_x^2(A)", G);
            check_bool("Jordan generalized eigenspace rows = 2", mat_get_row_count(G) == 2);
            check_bool("Jordan generalized eigenspace cols = 2", mat_get_col_count(G) == 2);
            check_dval_generalized_eigenspace_relation("dval Jordan generalized eigenspace",
                                                       A, x, 2, G, 1e-20);
            dv_set_val_d(x, 9.0);
            check_dval_generalized_eigenspace_relation("dval Jordan generalized eigenspace tracks",
                                                       A, x, 2, G, 1e-20);
        }

        mat_free(A);
        mat_free(G);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, x, one,
            DV_ZERO, DV_ZERO, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *G2 = mat_generalized_eigenspace(A, &x, 2);
        matrix_t *G3 = mat_generalized_eigenspace(A, &x, 3);

        print_mdv("A (3x3 Jordan generalized eigenspace dval)", A);
        check_bool("mat_generalized_eigenspace(dval Jordan 3x3,2) not NULL", G2 != NULL);
        check_bool("mat_generalized_eigenspace(dval Jordan 3x3,3) not NULL", G3 != NULL);
        if (G2) {
            print_mdv("gen_eigenspace_x^2(A)", G2);
            check_bool("3x3 Jordan generalized eigenspace order-2 rows = 3",
                       mat_get_row_count(G2) == 3);
            check_bool("3x3 Jordan generalized eigenspace order-2 cols = 2",
                       mat_get_col_count(G2) == 2);
            check_dval_generalized_eigenspace_relation("dval 3x3 Jordan generalized eigenspace order-2",
                                                       A, x, 2, G2, 1e-20);
        }
        if (G3) {
            print_mdv("gen_eigenspace_x^3(A)", G3);
            check_bool("3x3 Jordan generalized eigenspace order-3 rows = 3",
                       mat_get_row_count(G3) == 3);
            check_bool("3x3 Jordan generalized eigenspace order-3 cols = 3",
                       mat_get_col_count(G3) == 3);
            check_dval_generalized_eigenspace_relation("dval 3x3 Jordan generalized eigenspace order-3",
                                                       A, x, 3, G3, 1e-20);
            dv_set_val_d(x, 5.0);
            check_dval_generalized_eigenspace_relation("dval 3x3 Jordan generalized eigenspace order-3 tracks",
                                                       A, x, 3, G3, 1e-20);
        }

        mat_free(A);
        mat_free(G2);
        mat_free(G3);
        dv_free(x);
        dv_free(one);
    }
}

static void test_jordan_chain_dval(void)
{
    printf(C_CYAN "TEST: Jordan chain (dval)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, DV_ZERO, x};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *J = mat_jordan_chain(A, &x, 2);

        print_mdv("A (Jordan chain 2x2 dval)", A);
        check_bool("mat_jordan_chain(dval Jordan 2x2,2) not NULL", J != NULL);
        if (J) {
            print_mdv("jordan_chain_x^2(A)", J);
            check_bool("Jordan chain 2x2 rows = 2", mat_get_row_count(J) == 2);
            check_bool("Jordan chain 2x2 cols = 2", mat_get_col_count(J) == 2);
            check_dval_jordan_chain_relation("dval Jordan chain 2x2", A, x, J, 1e-20);
            dv_set_val_d(x, 9.0);
            check_dval_jordan_chain_relation("dval Jordan chain 2x2 tracks", A, x, J, 1e-20);
        }

        mat_free(A);
        mat_free(J);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, x, one,
            DV_ZERO, DV_ZERO, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *J = mat_jordan_chain(A, &x, 3);

        print_mdv("A (Jordan chain 3x3 dval)", A);
        check_bool("mat_jordan_chain(dval Jordan 3x3,3) not NULL", J != NULL);
        if (J) {
            print_mdv("jordan_chain_x^3(A)", J);
            check_bool("Jordan chain 3x3 rows = 3", mat_get_row_count(J) == 3);
            check_bool("Jordan chain 3x3 cols = 3", mat_get_col_count(J) == 3);
            check_dval_jordan_chain_relation("dval Jordan chain 3x3", A, x, J, 1e-20);
            dv_set_val_d(x, 5.0);
            check_dval_jordan_chain_relation("dval Jordan chain 3x3 tracks", A, x, J, 1e-20);
        }

        mat_free(A);
        mat_free(J);
        dv_free(x);
        dv_free(one);
    }
}

static void test_jordan_profile_dval(void)
{
    printf(C_CYAN "TEST: Jordan profile (dval)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(3.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[4] = {x, one, DV_ZERO, x};
        matrix_t *A = mat_create_dv(2, 2, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile 2x2 dval)", A);
        check_bool("mat_jordan_profile(dval Jordan 2x2) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile 2x2 rows = 1", mat_get_row_count(P) == 1);
            check_bool("Jordan profile 2x2 cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            check_d("Jordan profile 2x2[0] = 2", p0, 2.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *y = dv_new_named_var_d(5.0, "y");
        dval_t *vals[9] = {
            x, DV_ZERO, DV_ZERO,
            DV_ZERO, x, DV_ZERO,
            DV_ZERO, DV_ZERO, y};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile repeated diagonal dval)", A);
        check_bool("mat_jordan_profile(repeated diagonal dval) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            double p1 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile repeated diagonal rows = 2", mat_get_row_count(P) == 2);
            check_bool("Jordan profile repeated diagonal cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            mat_get(P, 1, 0, &p1);
            check_d("Jordan profile repeated diagonal[0] = 1", p0, 1.0, 1e-12);
            check_d("Jordan profile repeated diagonal[1] = 1", p1, 1.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        dv_free(x);
        dv_free(y);
    }

    {
        dval_t *x = dv_new_named_var_d(2.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            DV_ZERO, x, DV_ZERO,
            DV_ZERO, DV_ZERO, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *P = mat_jordan_profile(A, &x);

        print_mdv("A (Jordan profile mixed 3x3 dval)", A);
        check_bool("mat_jordan_profile(mixed 3x3 dval) not NULL", P != NULL);
        if (P) {
            double p0 = 0.0;
            double p1 = 0.0;
            print_md("jordan_profile_x(A)", P);
            check_bool("Jordan profile mixed 3x3 rows = 2", mat_get_row_count(P) == 2);
            check_bool("Jordan profile mixed 3x3 cols = 1", mat_get_col_count(P) == 1);
            mat_get(P, 0, 0, &p0);
            mat_get(P, 1, 0, &p1);
            check_d("Jordan profile mixed 3x3[0] = 2", p0, 2.0, 1e-12);
            check_d("Jordan profile mixed 3x3[1] = 1", p1, 1.0, 1e-12);
        }

        mat_free(A);
        mat_free(P);
        dv_free(x);
        dv_free(one);
    }
}

/* ------------------------------------------------------------------ mat_exp */

static void test_mat_exp_d(void)
{
    printf(C_CYAN "TEST: mat_exp (double)\n" C_RESET);

    /* 2×2 diagonal: exp(diag(a,b)) = diag(exp(a),exp(b)) */
    {
        double A_vals[4] = {1.0, 0.0, 0.0, 2.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(diag) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            check_d("exp(diag)[0,0] = e", e[0], exp(1.0), 1e-12);
            check_d("exp(diag)[1,1] = e²", e[3], exp(2.0), 1e-12);
            check_d("exp(diag)[0,1] = 0", e[1], 0.0, 1e-12);
            check_d("exp(diag)[1,0] = 0", e[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(sym) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            double ch = cosh(1.0), sh = sinh(1.0);
            check_d("exp([[0,1],[1,0]])[0,0] = cosh(1)", e[0], ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,1] = cosh(1)", e[3], ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[0,1] = sinh(1)", e[1], sh, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,0] = sinh(1)", e[2], sh, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }

    /* zero matrix: exp(0) = I */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(zero) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            check_d("exp(0)[0,0] = 1", e[0], 1.0, 1e-12);
            check_d("exp(0)[1,1] = 1", e[3], 1.0, 1e-12);
            check_d("exp(0)[0,1] = 0", e[1], 0.0, 1e-12);
            check_d("exp(0)[1,0] = 0", e[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_qf(void)
{
    printf(C_CYAN "TEST: mat_exp (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        qfloat_t A_vals[4] = {QF_ZERO, QF_ONE, QF_ONE, QF_ZERO};
        matrix_t *A = mat_create_qf(2, 2, A_vals);
        print_mqf("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(sym) not NULL", E != NULL);
        if (E)
        {
            print_mqf("exp(A)", E);
            qfloat_t e[4];
            mat_get_data(E, e);
            /* cosh(1) = (e + 1/e) / 2, sinh(1) = (e - 1/e) / 2 */
            qfloat_t e1 = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t ch = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh = qf_div(qf_sub(e1, inv1), two);
            check_qf_val("qf exp(sym)[0,0] = cosh(1)", e[0], ch, 1e-25);
            check_qf_val("qf exp(sym)[1,1] = cosh(1)", e[3], ch, 1e-25);
            check_qf_val("qf exp(sym)[0,1] = sinh(1)", e[1], sh, 1e-25);
            check_qf_val("qf exp(sym)[1,0] = sinh(1)", e[2], sh, 1e-25);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_qc(void)
{
    printf(C_CYAN "TEST: mat_exp (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        qcomplex_t A_vals[4] = {
            QC_ZERO,
            qc_make(QF_ZERO, QF_ONE),
            qc_make(QF_ZERO, qf_neg(QF_ONE)),
            QC_ZERO};
        matrix_t *A = mat_create_qc(2, 2, A_vals);
        print_mqc("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qc(herm) not NULL", E != NULL);
        if (E)
        {
            print_mqc("exp(A)", E);
            qcomplex_t e[4];
            mat_get_data(E, e);
            qfloat_t e1 = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t ch = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh = qf_div(qf_sub(e1, inv1), two);
            qcomplex_t ch_c = qc_make(ch, QF_ZERO);
            qcomplex_t ish = qc_make(QF_ZERO, sh);
            qcomplex_t nish = qc_make(QF_ZERO, qf_neg(sh));
            check_qc_val("qc exp(herm)[0,0] = cosh(1)", e[0], ch_c, 1e-25);
            check_qc_val("qc exp(herm)[1,1] = cosh(1)", e[3], ch_c, 1e-25);
            check_qc_val("qc exp(herm)[0,1] = i·sinh(1)", e[1], ish, 1e-25);
            check_qc_val("qc exp(herm)[1,0] = -i·sinh(1)", e[2], nish, 1e-25);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_singular(void)
{
    printf(C_CYAN "TEST: mat_exp on singular square matrices\n" C_RESET);

    /* singular diagonal: exp(diag(0,2)) = diag(1,e^2) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 2.0};
        double expected_vals[4] = {1.0, 0.0, 0.0, exp(2.0)};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        matrix_t *E_expected = mat_create_d(2, 2, expected_vals);
        check_bool("singular diagonal allocated", A != NULL);
        check_bool("singular diagonal expected allocated", E_expected != NULL);
        if (!A || !E_expected) {
            mat_free(A);
            mat_free(E_expected);
            return;
        }

        print_md("A (singular diagonal)", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(singular diagonal) not NULL", E != NULL);
        if (E)
        {
            check_bool("exp(diag(0,2)) preserves diagonal structure", mat_is_diagonal(E));
            check_mat_d("exp(diag(0,2)) = diag(1,e^2)", E, E_expected, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(E_expected);
    }

    /* singular nilpotent Jordan block: exp(N) = I + N */
    {
        double N_vals[4] = {0.0, 1.0, 0.0, 0.0};
        double expected_vals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *N = mat_create_d(2, 2, N_vals);
        matrix_t *E_expected = mat_create_d(2, 2, expected_vals);
        check_bool("singular nilpotent allocated", N != NULL);
        check_bool("singular nilpotent expected allocated", E_expected != NULL);
        if (!N || !E_expected) {
            mat_free(N);
            mat_free(E_expected);
            return;
        }

        print_md("N (singular nilpotent)", N);
        matrix_t *E = mat_exp(N);
        check_bool("mat_exp(singular nilpotent) not NULL", E != NULL);
        if (E)
        {
            check_bool("exp(N) preserves upper-triangular structure", mat_is_upper_triangular(E));
            check_mat_d("exp(N) = I + N", E, E_expected, 1e-12);
        }

        mat_free(N);
        mat_free(E);
        mat_free(E_expected);
    }
}

static void test_matrix_function_structure_preservation(void)
{
    printf(C_CYAN "TEST: matrix functions preserve structured layouts when possible\n" C_RESET);

    {
        double A_vals[4] = {2.0, 0.0, 0.0, 3.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        matrix_t *L = mat_log(A);
        matrix_t *E = NULL;

        check_bool("positive diagonal input allocated", A != NULL);
        check_bool("mat_log(positive diagonal) not NULL", L != NULL);
        if (L) {
            check_bool("mat_log(positive diagonal) preserves diagonal structure", mat_is_diagonal(L));
            E = mat_exp(L);
            check_bool("mat_exp(mat_log(positive diagonal)) not NULL", E != NULL);
            if (E) {
                check_bool("mat_exp(mat_log(positive diagonal)) preserves diagonal structure",
                           mat_is_diagonal(E));
                check_mat_d("exp(log(diag(2,3))) = diag(2,3)", E, A, 1e-12);
            }
        }

        mat_free(E);
        mat_free(L);
        mat_free(A);
    }

    {
        double A_vals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        matrix_t *L = mat_log(A);
        matrix_t *S = mat_sqrt(A);
        matrix_t *E = NULL;

        check_bool("upper-triangular Jordan input allocated", A != NULL);
        check_bool("mat_log(upper-triangular Jordan) not NULL", L != NULL);
        check_bool("mat_sqrt(upper-triangular Jordan) not NULL", S != NULL);
        if (L) {
            check_bool("mat_log(upper-triangular Jordan) preserves upper-triangular structure",
                       mat_is_upper_triangular(L));
            E = mat_exp(L);
            check_bool("mat_exp(mat_log(upper-triangular Jordan)) not NULL", E != NULL);
            if (E) {
                check_bool("mat_exp(mat_log(upper-triangular Jordan)) preserves upper-triangular structure",
                           mat_is_upper_triangular(E));
                check_mat_d("exp(log(I+N)) = I+N", E, A, 1e-12);
            }
        }
        if (S) {
            check_bool("mat_sqrt(upper-triangular Jordan) preserves upper-triangular structure",
                       mat_is_upper_triangular(S));
        }

        mat_free(E);
        mat_free(L);
        mat_free(S);
        mat_free(A);
    }
}

static void test_mat_fun_singular_entire_d(void)
{
    printf(C_CYAN "TEST: entire matrix functions on singular square matrices\n" C_RESET);

    /* For the nilpotent Jordan block N with N^2 = 0, any entire function
     * satisfies f(N) = f(0) I + f'(0) N. */
    {
        double N_vals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = mat_create_d(2, 2, N_vals);
        check_bool("nilpotent singular test matrix allocated", N != NULL);
        if (!N)
            return;

        print_md("N (nilpotent singular)", N);

        {
            matrix_t *R = mat_sin(N);
            check_bool("mat_sin(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("sin(N)=N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_cos(N);
            check_bool("mat_cos(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, 0.0, 0.0, 1.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("cos(N)=I", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_sinh(N);
            check_bool("mat_sinh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("sinh(N)=N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_cosh(N);
            check_bool("mat_cosh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, 0.0, 0.0, 1.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("cosh(N)=I", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_tan(N);
            check_bool("mat_tan(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("tan(N)=N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_tanh(N);
            check_bool("mat_tanh(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, 1.0, 0.0, 0.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("tanh(N)=N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_erf(N);
            double c = 2.0 / sqrt(M_PI);
            check_bool("mat_erf(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {0.0, c, 0.0, 0.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("erf(N)=(2/sqrt(pi))N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        {
            matrix_t *R = mat_erfc(N);
            double c = 2.0 / sqrt(M_PI);
            check_bool("mat_erfc(N) not NULL", R != NULL);
            if (R) {
                double expected_vals[4] = {1.0, -c, 0.0, 1.0};
                matrix_t *E = mat_create_d(2, 2, expected_vals);
                check_mat_d("erfc(N)=I-(2/sqrt(pi))N", R, E, 1e-12);
                mat_free(E);
            }
            mat_free(R);
        }

        mat_free(N);
    }
}

static void test_mat_exp_null_safety(void)
{
    printf(C_CYAN "TEST: mat_exp null safety\n" C_RESET);
    check_bool("mat_exp(NULL) = NULL", mat_exp(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_exp(non-square) = NULL", mat_exp(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sin */

static void test_mat_sin_d(void)
{
    printf(C_CYAN "TEST: mat_sin (double)\n" C_RESET);

    /* 2×2 diagonal: sin(diag(0, π/2)) = diag(0, 1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI / 2.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            check_d("sin(diag)[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin(diag)[1,1] = 1", s[3], 1.0, 1e-12);
            check_d("sin(diag)[0,1] = 0", s[1], 0.0, 1e-12);
            check_d("sin(diag)[1,0] = 0", s[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            double s1 = sin(1.0);
            check_d("sin([[0,1],[1,0]])[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[1,1] = 0", s[3], 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[0,1] = sin(1)", s[1], s1, 1e-12);
            check_d("sin([[0,1],[1,0]])[1,0] = sin(1)", s[2], s1, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* zero matrix: sin(0) = 0 */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(zero) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            check_d("sin(0)[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin(0)[1,1] = 0", s[3], 0.0, 1e-12);
            check_d("sin(0)[0,1] = 0", s[1], 0.0, 1e-12);
            check_d("sin(0)[1,0] = 0", s[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_qf(void)
{
    printf(C_CYAN "TEST: mat_sin (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → sin(A) = sin(1)·A */
    {
        qfloat_t A_vals[4] = {QF_ZERO, QF_ONE, QF_ONE, QF_ZERO};
        matrix_t *A = mat_create_qf(2, 2, A_vals);
        print_mqf("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(sym) not NULL", S != NULL);
        if (S)
        {
            print_mqf("sin(A)", S);
            qfloat_t s[4];
            mat_get_data(S, s);
            qfloat_t s1 = qf_sin(QF_ONE);
            check_qf_val("qf sin(sym)[0,0] = 0", s[0], QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[1,1] = 0", s[3], QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[0,1] = sin(1)", s[1], s1, 1e-25);
            check_qf_val("qf sin(sym)[1,0] = sin(1)", s[2], s1, 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_qc(void)
{
    printf(C_CYAN "TEST: mat_sin (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A = [[0, i·sin(1)], [-i·sin(1), 0]] */
    {
        qcomplex_t A_vals[4] = {
            QC_ZERO,
            qc_make(QF_ZERO, QF_ONE),
            qc_make(QF_ZERO, qf_neg(QF_ONE)),
            QC_ZERO};
        matrix_t *A = mat_create_qc(2, 2, A_vals);
        print_mqc("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qc(herm) not NULL", S != NULL);
        if (S)
        {
            print_mqc("sin(A)", S);
            qcomplex_t s[4];
            mat_get_data(S, s);
            qfloat_t s1 = qf_sin(QF_ONE);
            qcomplex_t zero_c = QC_ZERO;
            qcomplex_t ish = qc_make(QF_ZERO, s1);
            qcomplex_t nish = qc_make(QF_ZERO, qf_neg(s1));
            check_qc_val("qc sin(herm)[0,0] = 0", s[0], zero_c, 1e-25);
            check_qc_val("qc sin(herm)[1,1] = 0", s[3], zero_c, 1e-25);
            check_qc_val("qc sin(herm)[0,1] = i·sin(1)", s[1], ish, 1e-25);
            check_qc_val("qc sin(herm)[1,0] = -i·sin(1)", s[2], nish, 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sin null safety\n" C_RESET);
    check_bool("mat_sin(NULL) = NULL", mat_sin(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_sin(non-square) = NULL", mat_sin(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_cos */

static void test_mat_cos_d(void)
{
    printf(C_CYAN "TEST: mat_cos (double)\n" C_RESET);

    /* 2×2 diagonal: cos(diag(0, π)) = diag(1, -1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(diag) not NULL", C != NULL);
        if (C)
        {
            print_md("cos(A)", C);
            double c[4];
            mat_get_data(C, c);
            check_d("cos(diag)[0,0] =  1", c[0], 1.0, 1e-12);
            check_d("cos(diag)[1,1] = -1", c[3], -1.0, 1e-12);
            check_d("cos(diag)[0,1] =  0", c[1], 0.0, 1e-12);
            check_d("cos(diag)[1,0] =  0", c[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(sym) not NULL", C != NULL);
        if (C)
        {
            print_md("cos(A)", C);
            double c[4];
            mat_get_data(C, c);
            double c1 = cos(1.0);
            check_d("cos([[0,1],[1,0]])[0,0] = cos(1)", c[0], c1, 1e-12);
            check_d("cos([[0,1],[1,0]])[1,1] = cos(1)", c[3], c1, 1e-12);
            check_d("cos([[0,1],[1,0]])[0,1] = 0", c[1], 0.0, 1e-12);
            check_d("cos([[0,1],[1,0]])[1,0] = 0", c[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }
}

static void test_mat_cos_qf(void)
{
    printf(C_CYAN "TEST: mat_cos (qfloat)\n" C_RESET);

    /* 2×2 symmetric: A = [[0,1],[1,0]] → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_new_qf(2, 2);
        qfloat_t z = QF_ZERO, o = QF_ONE;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_mqf("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(sym) not NULL", C != NULL);
        if (C)
        {
            print_mqf("cos(A)", C);
            qfloat_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            check_qf_val("qf cos(sym)[0,0] = cos(1)", c00, c1, 1e-25);
            check_qf_val("qf cos(sym)[1,1] = cos(1)", c11, c1, 1e-25);
            check_qf_val("qf cos(sym)[0,1] = 0", c01, QF_ZERO, 1e-25);
            check_qf_val("qf cos(sym)[1,0] = 0", c10, QF_ZERO, 1e-25);
        }
        mat_free(A);
        mat_free(C);
    }
}

static void test_mat_cos_qc(void)
{
    printf(C_CYAN "TEST: mat_cos (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_new_qc(2, 2);
        qcomplex_t z = QC_ZERO;
        qcomplex_t pi = qc_make(QF_ZERO, QF_ONE);
        qcomplex_t ni = qc_make(QF_ZERO, qf_neg(QF_ONE));
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &pi);
        mat_set(A, 1, 0, &ni);
        mat_set(A, 1, 1, &z);
        print_mqc("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qc(herm) not NULL", C != NULL);
        if (C)
        {
            print_mqc("cos(A)", C);
            qcomplex_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            qcomplex_t c1_c = qc_make(c1, QF_ZERO);
            qcomplex_t zero_c = QC_ZERO;
            check_qc_val("qc cos(herm)[0,0] = cos(1)", c00, c1_c, 1e-25);
            check_qc_val("qc cos(herm)[1,1] = cos(1)", c11, c1_c, 1e-25);
            check_qc_val("qc cos(herm)[0,1] = 0", c01, zero_c, 1e-25);
            check_qc_val("qc cos(herm)[1,0] = 0", c10, zero_c, 1e-25);
        }
        mat_free(A);
        mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tan */

static void test_mat_tan_d(void)
{
    printf(C_CYAN "TEST: mat_tan (double)\n" C_RESET);

    /* 2×2 diagonal: tan(diag(0, π/4)) = diag(0, 1) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, h = M_PI / 4.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &h);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            check_d("tan(diag)[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tan(diag)[1,1] = 1", t11, 1.0, 1e-12);
            check_d("tan(diag)[0,1] = 0", t01, 0.0, 1e-12);
            check_d("tan(diag)[1,0] = 0", t10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tan is odd → tan(A) = tan(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(sym) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            double t1 = tan(1.0);
            check_d("tan([[0,1],[1,0]])[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[1,1] = 0", t11, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[0,1] = tan(1)", t01, t1, 1e-12);
            check_d("tan([[0,1],[1,0]])[1,0] = tan(1)", t10, t1, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }
}

/* ------------------------------------------------------------------ mat_sinh */

static void test_mat_sinh_d(void)
{
    printf(C_CYAN "TEST: mat_sinh (double)\n" C_RESET);

    /* 2×2 diagonal: sinh(diag(0, 1)) = diag(0, sinh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("sinh(diag)[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sinh(diag)[1,1] = sinh(1)", s11, sinh(1.0), 1e-12);
            check_d("sinh(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sinh(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * sinh is odd → sinh(A) = sinh(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            double sh = sinh(1.0);
            check_d("sinh([[0,1],[1,0]])[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[1,1] = 0", s11, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[0,1] = sinh(1)", s01, sh, 1e-12);
            check_d("sinh([[0,1],[1,0]])[1,0] = sinh(1)", s10, sh, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }
}

/* ------------------------------------------------------------------ mat_cosh */

static void test_mat_cosh_d(void)
{
    printf(C_CYAN "TEST: mat_cosh (double)\n" C_RESET);

    /* 2×2 diagonal: cosh(diag(0, 1)) = diag(1, cosh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(diag) not NULL", C != NULL);
        if (C)
        {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            check_d("cosh(diag)[0,0] = 1", c00, 1.0, 1e-12);
            check_d("cosh(diag)[1,1] = cosh(1)", c11, cosh(1.0), 1e-12);
            check_d("cosh(diag)[0,1] = 0", c01, 0.0, 1e-12);
            check_d("cosh(diag)[1,0] = 0", c10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cosh is even → cosh(A) = cosh(1)·I */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(sym) not NULL", C != NULL);
        if (C)
        {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            double ch = cosh(1.0);
            check_d("cosh([[0,1],[1,0]])[0,0] = cosh(1)", c00, ch, 1e-12);
            check_d("cosh([[0,1],[1,0]])[1,1] = cosh(1)", c11, ch, 1e-12);
            check_d("cosh([[0,1],[1,0]])[0,1] = 0", c01, 0.0, 1e-12);
            check_d("cosh([[0,1],[1,0]])[1,0] = 0", c10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tanh */

static void test_mat_tanh_d(void)
{
    printf(C_CYAN "TEST: mat_tanh (double)\n" C_RESET);

    /* 2×2 diagonal: tanh(diag(0, 1)) = diag(0, tanh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            check_d("tanh(diag)[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tanh(diag)[1,1] = tanh(1)", t11, tanh(1.0), 1e-12);
            check_d("tanh(diag)[0,1] = 0", t01, 0.0, 1e-12);
            check_d("tanh(diag)[1,0] = 0", t10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tanh is odd → tanh(A) = tanh(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(sym) not NULL", T != NULL);
        if (T)
        {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            double th = tanh(1.0);
            check_d("tanh([[0,1],[1,0]])[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[1,1] = 0", t11, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[0,1] = tanh(1)", t01, th, 1e-12);
            check_d("tanh([[0,1],[1,0]])[1,0] = tanh(1)", t10, th, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }
}

static void test_mat_trig_null_safety(void)
{
    printf(C_CYAN "TEST: mat_cos/tan/sinh/cosh/tanh null safety\n" C_RESET);

    check_bool("mat_cos(NULL) = NULL", mat_cos(NULL) == NULL);
    check_bool("mat_tan(NULL) = NULL", mat_tan(NULL) == NULL);
    check_bool("mat_sinh(NULL) = NULL", mat_sinh(NULL) == NULL);
    check_bool("mat_cosh(NULL) = NULL", mat_cosh(NULL) == NULL);
    check_bool("mat_tanh(NULL) = NULL", mat_tanh(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_cos(non-square) = NULL", mat_cos(A) == NULL);
    check_bool("mat_tan(non-square) = NULL", mat_tan(A) == NULL);
    check_bool("mat_sinh(non-square) = NULL", mat_sinh(A) == NULL);
    check_bool("mat_cosh(non-square) = NULL", mat_cosh(A) == NULL);
    check_bool("mat_tanh(non-square) = NULL", mat_tanh(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sqrt */

static void test_mat_sqrt_d(void)
{
    printf(C_CYAN "TEST: mat_sqrt (double)\n" C_RESET);

    /* 2×2 diagonal: sqrt(diag(1,4)) = diag(1,2) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0, f = 4.0;
        mat_set(A, 0, 0, &o);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &f);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("sqrt(diag)[0,0] = 1", s00, 1.0, 1e-12);
            check_d("sqrt(diag)[1,1] = 2", s11, 2.0, 1e-12);
            check_d("sqrt(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sqrt(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* Verify: sqrt(A)^2 = A for [[2,1],[1,2]] (eigenvalues 1 and 3) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double o = 1.0, t = 2.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &t);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            matrix_t *S2 = mat_mul(S, S);
            check_bool("sqrt(A)^2 not NULL", S2 != NULL);
            if (S2)
            {
                print_md("sqrt(A)^2", S2);
                double r00, r01, r10, r11;
                mat_get(S2, 0, 0, &r00);
                mat_get(S2, 0, 1, &r01);
                mat_get(S2, 1, 0, &r10);
                mat_get(S2, 1, 1, &r11);
                check_d("sqrt(A)^2[0,0] = 2", r00, 2.0, 1e-10);
                check_d("sqrt(A)^2[1,1] = 2", r11, 2.0, 1e-10);
                check_d("sqrt(A)^2[0,1] = 1", r01, 1.0, 1e-10);
                check_d("sqrt(A)^2[1,0] = 1", r10, 1.0, 1e-10);
                mat_free(S2);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_sqrt_qf(void)
{
    printf(C_CYAN "TEST: mat_sqrt (qfloat)\n" C_RESET);

    /* 2×2 diagonal: sqrt(diag(1,9)) = diag(1,3) */
    {
        matrix_t *A = mat_new_qf(2, 2);
        qfloat_t z = QF_ZERO;
        qfloat_t o = QF_ONE;
        qfloat_t n = qf_from_double(9.0);
        mat_set(A, 0, 0, &o);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &n);
        print_mqf("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("qf mat_sqrt(diag) not NULL", S != NULL);
        if (S)
        {
            print_mqf("sqrt(A)", S);
            qfloat_t s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_qf_val("qf sqrt(diag)[0,0] = 1", s00, QF_ONE, 1e-25);
            check_qf_val("qf sqrt(diag)[0,1] = 0", s01, QF_ZERO, 1e-25);
            check_qf_val("qf sqrt(diag)[1,0] = 0", s10, QF_ZERO, 1e-25);
            check_qf_val("qf sqrt(diag)[1,1] = 3", s11, qf_from_double(3.0), 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }
}

/* ------------------------------------------------------------------ mat_log */

static void test_mat_log_d(void)
{
    printf(C_CYAN "TEST: mat_log (double)\n" C_RESET);

    /* exp(log(A)) = A for diagonal [[2,0],[0,3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, t = 2.0, th = 3.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &th);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log(diag) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *E = mat_exp(L);
            check_bool("exp(log(A)) not NULL", E != NULL);
            if (E)
            {
                print_md("exp(log(A))", E);
                double e00, e01, e10, e11;
                mat_get(E, 0, 0, &e00);
                mat_get(E, 0, 1, &e01);
                mat_get(E, 1, 0, &e10);
                mat_get(E, 1, 1, &e11);
                check_d("exp(log(diag))[0,0] = 2", e00, 2.0, 1e-10);
                check_d("exp(log(diag))[1,1] = 3", e11, 3.0, 1e-10);
                check_d("exp(log(diag))[0,1] = 0", e01, 0.0, 1e-10);
                check_d("exp(log(diag))[1,0] = 0", e10, 0.0, 1e-10);
                mat_free(E);
            }
            mat_free(L);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_asin / mat_acos */

static void test_mat_asin_d(void)
{
    printf(C_CYAN "TEST: mat_asin (double)\n" C_RESET);

    /* sin(asin(A)) = A for 2×2 symmetric with small eigenvalues */
    {
        matrix_t *A = mat_new_d(2, 2);
        double v00 = 0.3, v01 = 0.1, v10 = 0.1, v11 = 0.2;
        mat_set(A, 0, 0, &v00);
        mat_set(A, 0, 1, &v01);
        mat_set(A, 1, 0, &v10);
        mat_set(A, 1, 1, &v11);
        print_md("A", A);
        matrix_t *S = mat_asin(A);
        check_bool("mat_asin(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("asin(A)", S);
            matrix_t *R = mat_sin(S);
            check_bool("sin(asin(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sin(asin(A))", R);
                double r00, r01, r10, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 0, 1, &r01);
                mat_get(R, 1, 0, &r10);
                mat_get(R, 1, 1, &r11);
                check_d("sin(asin(A))[0,0] = 0.3", r00, 0.3, 1e-10);
                check_d("sin(asin(A))[0,1] = 0.1", r01, 0.1, 1e-10);
                check_d("sin(asin(A))[1,0] = 0.1", r10, 0.1, 1e-10);
                check_d("sin(asin(A))[1,1] = 0.2", r11, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_acos_d(void)
{
    printf(C_CYAN "TEST: mat_acos (double)\n" C_RESET);

    /* asin(A) + acos(A) = (π/2)·I for diagonal [[0.3, 0], [0, 0.4]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, v0 = 0.3, v1 = 0.4;
        mat_set(A, 0, 0, &v0);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &v1);
        print_md("A", A);
        matrix_t *AS = mat_asin(A);
        matrix_t *AC = mat_acos(A);
        check_bool("mat_asin+mat_acos: both not NULL", AS != NULL && AC != NULL);
        if (AS && AC)
        {
            matrix_t *SUM = mat_add(AS, AC);
            check_bool("asin+acos not NULL", SUM != NULL);
            if (SUM)
            {
                print_md("asin(A)+acos(A)", SUM);
                double s00, s01, s10, s11;
                mat_get(SUM, 0, 0, &s00);
                mat_get(SUM, 0, 1, &s01);
                mat_get(SUM, 1, 0, &s10);
                mat_get(SUM, 1, 1, &s11);
                check_d("asin+acos[0,0] = π/2", s00, M_PI / 2.0, 1e-10);
                check_d("asin+acos[1,1] = π/2", s11, M_PI / 2.0, 1e-10);
                check_d("asin+acos[0,1] = 0", s01, 0.0, 1e-10);
                check_d("asin+acos[1,0] = 0", s10, 0.0, 1e-10);
                mat_free(SUM);
            }
        }
        mat_free(AS);
        mat_free(AC);
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_atan */

static void test_mat_atan_d(void)
{
    printf(C_CYAN "TEST: mat_atan (double)\n" C_RESET);

    /* tan(atan(A)) = A for small diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atan(A);
        check_bool("mat_atan(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("atan(A)", T);
            matrix_t *R = mat_tan(T);
            check_bool("tan(atan(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("tan(atan(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("tan(atan(A))[0,0] = 0.5", r00, 0.5, 1e-10);
                check_d("tan(atan(A))[1,1] = 0.3", r11, 0.3, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_asinh / mat_acosh / mat_atanh */

static void test_mat_asinh_d(void)
{
    printf(C_CYAN "TEST: mat_asinh (double)\n" C_RESET);

    /* sinh(asinh(A)) = A for diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("mat_asinh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sinh(asinh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("sinh(asinh(A))[0,0] = 0.5", r00, 0.5, 1e-10);
                check_d("sinh(asinh(A))[1,1] = 0.3", r11, 0.3, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_acosh_d(void)
{
    printf(C_CYAN "TEST: mat_acosh (double)\n" C_RESET);

    /* cosh(acosh(A)) = A for diagonal [[2, 0],[0, 3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 2.0, b = 3.0;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_acosh(A);
        check_bool("mat_acosh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("acosh(A)", S);
            matrix_t *R = mat_cosh(S);
            check_bool("cosh(acosh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("cosh(acosh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("cosh(acosh(A))[0,0] = 2", r00, 2.0, 1e-10);
                check_d("cosh(acosh(A))[1,1] = 3", r11, 3.0, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_atanh_d(void)
{
    printf(C_CYAN "TEST: mat_atanh (double)\n" C_RESET);

    /* tanh(atanh(A)) = A for diagonal [[0.4, 0],[0, 0.2]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.4, b = 0.2;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atanh(A);
        check_bool("mat_atanh(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("atanh(A)", T);
            matrix_t *R = mat_tanh(T);
            check_bool("tanh(atanh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("tanh(atanh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("tanh(atanh(A))[0,0] = 0.4", r00, 0.4, 1e-10);
                check_d("tanh(atanh(A))[1,1] = 0.2", r11, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

static void test_mat_inv_trig_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sqrt/log/asin/acos/atan/asinh/acosh/atanh null safety\n" C_RESET);
    check_bool("mat_sqrt(NULL)  = NULL", mat_sqrt(NULL) == NULL);
    check_bool("mat_log(NULL)   = NULL", mat_log(NULL) == NULL);
    check_bool("mat_asin(NULL)  = NULL", mat_asin(NULL) == NULL);
    check_bool("mat_acos(NULL)  = NULL", mat_acos(NULL) == NULL);
    check_bool("mat_atan(NULL)  = NULL", mat_atan(NULL) == NULL);
    check_bool("mat_asinh(NULL) = NULL", mat_asinh(NULL) == NULL);
    check_bool("mat_acosh(NULL) = NULL", mat_acosh(NULL) == NULL);
    check_bool("mat_atanh(NULL) = NULL", mat_atanh(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_sqrt(non-sq)  = NULL", mat_sqrt(A) == NULL);
    check_bool("mat_log(non-sq)   = NULL", mat_log(A) == NULL);
    check_bool("mat_asin(non-sq)  = NULL", mat_asin(A) == NULL);
    check_bool("mat_acos(non-sq)  = NULL", mat_acos(A) == NULL);
    check_bool("mat_atan(non-sq)  = NULL", mat_atan(A) == NULL);
    check_bool("mat_asinh(non-sq) = NULL", mat_asinh(A) == NULL);
    check_bool("mat_acosh(non-sq) = NULL", mat_acosh(A) == NULL);
    check_bool("mat_atanh(non-sq) = NULL", mat_atanh(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ general eigendecompose */

static void test_eigen_general_d(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, double)\n" C_RESET);

    /* A = [[3, 1], [0, 2]] — upper triangular, eigenvalues 3 and 2.
     * eigenvector for λ=3: [1, 0]
     * eigenvector for λ=2: [-1, 1] (normalised) */
    matrix_t *A = mat_new_d(2, 2);
    double a00 = 3.0, a01 = 1.0, a10 = 0.0, a11 = 2.0;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);
    print_md("A", A);

    double eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("mat_eigendecompose_general: rc = 0", rc == 0);
    check_bool("mat_eigendecompose_general: V not NULL", V != NULL);

    if (V)
    {
        print_md("V (eigenvectors)", V);
        /* Verify A·v_j = lambda_j · v_j for each column */
        for (int j = 0; j < 2; j++)
        {
            double lam = eigenvalues[j];
            double v0, v1;
            mat_get(V, 0, j, &v0);
            mat_get(V, 1, j, &v1);
            /* [Av]_0 = 3*v0 + 1*v1, [Av]_1 = 2*v1 */
            double av0 = 3.0 * v0 + 1.0 * v1;
            double av1 = 2.0 * v1;
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "(Av)[0,%d] = lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "(Av)[1,%d] = lam*v[1,%d]", j, j);
            check_d(label0, av0, lam * v0, 1e-10);
            check_d(label1, av1, lam * v1, 1e-10);
        }
        mat_free(V);
    }
    mat_free(A);

    /* Dense non-Hermitian case:
     *   P = [[1,1],[1,2]], D = diag(3,2), A = P D P^-1 = [[4,-1],[2,1]]
     * so the eigenpairs are exact but the input is no longer triangular. */
    {
        double avals[4] = {4.0, -1.0, 2.0, 1.0};
        matrix_t *B = mat_create_d(2, 2, avals);
        double evals[2];
        matrix_t *W = NULL;

        check_bool("dense non-Hermitian B allocated", B != NULL);
        if (!B)
            return;

        print_md("B (dense non-Hermitian)", B);

        int rc2 = mat_eigendecompose(B, evals, &W);
        check_bool("dense non-Hermitian eigendecompose rc = 0", rc2 == 0);
        check_bool("dense non-Hermitian eigenvectors not NULL", W != NULL);

        if (W)
        {
            print_md("W (eigenvectors)", W);
            for (int j = 0; j < 2; j++)
            {
                double lam = evals[j];
                double w0, w1;
                double bw0, bw1;
                char label0[80], label1[80];

                mat_get(W, 0, j, &w0);
                mat_get(W, 1, j, &w1);
                bw0 = 4.0 * w0 - 1.0 * w1;
                bw1 = 2.0 * w0 + 1.0 * w1;

                snprintf(label0, sizeof(label0), "(Bw)[0,%d] = lam*w[0,%d]", j, j);
                snprintf(label1, sizeof(label1), "(Bw)[1,%d] = lam*w[1,%d]", j, j);
                check_d(label0, bw0, lam * w0, 1e-10);
                check_d(label1, bw1, lam * w1, 1e-10);
            }

            double ev_min = evals[0] < evals[1] ? evals[0] : evals[1];
            double ev_max = evals[0] < evals[1] ? evals[1] : evals[0];
            check_d("dense non-Hermitian eigenvalue min = 2", ev_min, 2.0, 1e-10);
            check_d("dense non-Hermitian eigenvalue max = 3", ev_max, 3.0, 1e-10);
            mat_free(W);
        }

        mat_free(B);
    }
}

static void test_eigen_general_qf(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, qfloat)\n" C_RESET);

    /* A = [[4, 1], [0, 1]] — eigenvalues 1 and 4 */
    matrix_t *A = mat_new_qf(2, 2);
    qfloat_t a00 = qf_from_double(4.0), a01 = qf_from_double(1.0);
    qfloat_t a10 = QF_ZERO, a11 = QF_ONE;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);
    print_mqf("A", A);

    qfloat_t eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("qf eigendecompose_general: rc = 0", rc == 0);
    check_bool("qf eigendecompose_general: V not NULL", V != NULL);

    if (V)
    {
        print_mqf("V", V);
        for (int j = 0; j < 2; j++)
        {
            qfloat_t lam = eigenvalues[j];
            qfloat_t v0, v1;
            mat_get(V, 0, j, &v0);
            mat_get(V, 1, j, &v1);
            qfloat_t av0 = qf_add(qf_mul(a00, v0), qf_mul(a01, v1));
            qfloat_t av1 = qf_mul(a11, v1);
            qfloat_t lv0 = qf_mul(lam, v0);
            qfloat_t lv1 = qf_mul(lam, v1);
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "qf (Av)[0,%d]=lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "qf (Av)[1,%d]=lam*v[1,%d]", j, j);
            check_qf_val(label0, av0, lv0, 1e-25);
            check_qf_val(label1, av1, lv1, 1e-25);
        }
        mat_free(V);
    }
    mat_free(A);

    /* Same dense non-Hermitian similarity transform as the double test:
     * A = [[4,-1],[2,1]] has eigenvalues 3 and 2 but is not triangular. */
    {
        qfloat_t avals[4] = {
            qf_from_double(4.0), qf_from_double(-1.0),
            qf_from_double(2.0), qf_from_double(1.0)};
        matrix_t *B = mat_create_qf(2, 2, avals);
        qfloat_t evals[2];
        matrix_t *W = NULL;

        check_bool("qf dense non-Hermitian B allocated", B != NULL);
        if (!B)
            return;

        print_mqf("B (dense non-Hermitian)", B);

        int rc2 = mat_eigendecompose(B, evals, &W);
        check_bool("qf dense non-Hermitian eigendecompose rc = 0", rc2 == 0);
        check_bool("qf dense non-Hermitian eigenvectors not NULL", W != NULL);

        if (W)
        {
            qfloat_t four = qf_from_double(4.0);
            qfloat_t minus_one = qf_from_double(-1.0);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t one = QF_ONE;
            print_mqf("W", W);
            for (int j = 0; j < 2; j++)
            {
                qfloat_t lam = evals[j];
                qfloat_t w0, w1;
                qfloat_t bw0, bw1;
                qfloat_t lw0, lw1;
                char label0[96], label1[96];

                mat_get(W, 0, j, &w0);
                mat_get(W, 1, j, &w1);
                bw0 = qf_add(qf_mul(four, w0), qf_mul(minus_one, w1));
                bw1 = qf_add(qf_mul(two, w0), qf_mul(one, w1));
                lw0 = qf_mul(lam, w0);
                lw1 = qf_mul(lam, w1);

                snprintf(label0, sizeof(label0), "qf (Bw)[0,%d]=lam*w[0,%d]", j, j);
                snprintf(label1, sizeof(label1), "qf (Bw)[1,%d]=lam*w[1,%d]", j, j);
                check_qf_val(label0, bw0, lw0, 1e-25);
                check_qf_val(label1, bw1, lw1, 1e-25);
            }

            int e0_smaller = qf_cmp(evals[0], evals[1]) < 0;
            qfloat_t ev_min = e0_smaller ? evals[0] : evals[1];
            qfloat_t ev_max = e0_smaller ? evals[1] : evals[0];
            check_qf_val("qf dense non-Hermitian eigenvalue min = 2", ev_min,
                         qf_from_double(2.0), 1e-25);
            check_qf_val("qf dense non-Hermitian eigenvalue max = 3", ev_max,
                         qf_from_double(3.0), 1e-25);
            mat_free(W);
        }

        mat_free(B);
    }
}

/* ------------------------------------------------------------------ README example */

static void test_readme_example(void)
{
    printf(C_CYAN "TEST: README example — Hermitian eigendecomposition\n" C_RESET);

    /* Matrix from docs/matrix.md:
     *   [ 2    1+i ]
     *   [ 1-i  3   ]
     * Eigenvalues: 1 and 4
     */
    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(2.0), qf_from_double(0.0)),
        qc_make(qf_from_double(1.0), qf_from_double(1.0)),
        qc_make(qf_from_double(1.0), qf_from_double(-1.0)),
        qc_make(qf_from_double(3.0), qf_from_double(0.0))
    };
    matrix_t *A = mat_create_qc(2, 2, A_vals);

    qcomplex_t ev[2];
    matrix_t *V = NULL;
    tests_run++;

    if (!A)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: README example input allocation\n" C_RESET);
        return;
    }

    if (mat_eigendecompose(A, ev, &V) != 0 || !V)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: README example eigendecomposition\n" C_RESET);
        mat_free(A);
        mat_free(V);
        return;
    }

    printf("    example output from matrix.md:\n");
    qc_printf("    eigenvalue[0] = %z\n", ev[0]);
    qc_printf("    eigenvalue[1] = %z\n", ev[1]);
    printf("    eigenvectors V (columns) = ");
    mat_print(V);
    printf("\n");

    mat_free(A);
    mat_free(V);
}

static void test_readme_string_quantum_example(void)
{
    printf(C_CYAN "TEST: README example — symbolic two-level Hamiltonian\n" C_RESET);

    binding_t *bindings = NULL;
    size_t nbindings = 0;
    matrix_t *H = mat_from_string(
        "[[@DELTA @OMEGA][@OMEGA -@DELTA]]",
        &bindings, &nbindings);
    matrix_t *H2 = NULL;
    matrix_t *P = NULL;
    dval_t *ev[2] = {NULL, NULL};
    dval_t *trace = NULL;
    dval_t *c2 = NULL;
    dval_t *v = NULL;
    tests_run++;

    if (!H) {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: README string example input allocation\n" C_RESET);
        return;
    }

    check_bool("README string example @DELTA binding present",
               mat_binding_find(bindings, nbindings, "@DELTA") != NULL);
    check_bool("README string example @OMEGA binding present",
               mat_binding_find(bindings, nbindings, "@OMEGA") != NULL);
    check_bool("README string example set @DELTA",
               mat_binding_set_d(bindings, nbindings, "@DELTA", 1.5) == 0);
    check_bool("README string example set @OMEGA",
               mat_binding_set_d(bindings, nbindings, "@OMEGA", 0.25) == 0);

    H2 = mat_pow_int(H, 2);
    P = mat_charpoly(H);
    if (mat_eigenvalues(H, ev) != 0 || !H2 || !P) {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: README string example matrix functions\n" C_RESET);
        for (size_t i = 0; i < 2; ++i)
            dv_free(ev[i]);
        mat_free(P);
        mat_free(H2);
        free(bindings);
        mat_free(H);
        return;
    }

    check_bool("README string example mat_trace rc = 0", mat_trace(H, &trace) == 0);
    mat_get(P, 2, 0, &c2);

    check_bool("README string example trace non-null", trace != NULL);
    check_bool("README string example charpoly constant non-null", c2 != NULL);
    check_bool("README string example eigenvalue[0] non-null", ev[0] != NULL);
    check_bool("README string example eigenvalue[1] non-null", ev[1] != NULL);

    if (trace)
        check_d("README string example trace = 0", dv_eval_d(trace), 0.0, 1e-12);
    if (c2)
        check_d("README string example charpoly constant = -(Δ²+Ω²)",
                dv_eval_d(c2), -(1.5 * 1.5 + 0.25 * 0.25), 1e-12);

    if (H2) {
        mat_get(H2, 0, 0, &v);
        check_d("README string example H²[0,0] = Δ²+Ω²",
                dv_eval_d(v), 1.5 * 1.5 + 0.25 * 0.25, 1e-12);
        mat_get(H2, 0, 1, &v);
        check_d("README string example H²[0,1] = 0",
                dv_eval_d(v), 0.0, 1e-12);
        mat_get(H2, 1, 0, &v);
        check_d("README string example H²[1,0] = 0",
                dv_eval_d(v), 0.0, 1e-12);
        mat_get(H2, 1, 1, &v);
        check_d("README string example H²[1,1] = Δ²+Ω²",
                dv_eval_d(v), 1.5 * 1.5 + 0.25 * 0.25, 1e-12);
    }

    printf("    example output from matrix.md:\n");
    mat_printf("    H = %ml\n", H);
    mat_printf("    H² = %m\n", H2);
    if (H2) {
        char *h2_text = mat_to_string(H2, MAT_STRING_INLINE_PRETTY);
        check_bool("README string example H² text simplified",
                   h2_text && strcmp(h2_text,
                   "{ (Δ² + Ω², 0; 0, Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }") == 0);
        free(h2_text);
    }
    if (trace) {
        char *trace_text = dv_to_string(trace, style_EXPRESSION);
        printf("    tr(H) = %s\n", trace_text ? trace_text : "(null)");
        free(trace_text);
    }
    if (c2) {
        char *c2_text = dv_to_string(c2, style_EXPRESSION);
        check_bool("README string example charpoly constant text simplified",
                   c2_text && strcmp(c2_text, "{ -(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }") == 0);
        printf("    charpoly constant term = %s\n", c2_text ? c2_text : "(null)");
        free(c2_text);
    }
    if (ev[0] && ev[1]) {
        char *ev0_text = dv_to_string(ev[0], style_EXPRESSION);
        char *ev1_text = dv_to_string(ev[1], style_EXPRESSION);
        check_bool("README string example eigenvalue[0] text simplified",
                   ev0_text && strcmp(ev0_text, "{ √(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }") == 0);
        check_bool("README string example eigenvalue[1] text simplified",
                   ev1_text && strcmp(ev1_text, "{ -√(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }") == 0);
        printf("    eigenvalues = %s, %s\n",
               ev0_text ? ev0_text : "(null)",
               ev1_text ? ev1_text : "(null)");
        free(ev0_text);
        free(ev1_text);
    }
    printf("\n");

    dv_free(trace);
    for (size_t i = 0; i < 2; ++i)
        dv_free(ev[i]);
    free(bindings);
    mat_free(P);
    mat_free(H2);
    mat_free(H);
}

static void test_mat_simplify_symbolic_helper(void)
{
    dval_t *delta = dv_new_named_var_d(1.5, "Δ");
    dval_t *omega = dv_new_named_var_d(0.25, "Ω");
    dval_t *prod1 = NULL;
    dval_t *prod2 = NULL;
    dval_t *neg_prod1 = NULL;
    dval_t *entry = NULL;
    dval_t *vals[4] = {NULL, NULL, NULL, NULL};
    matrix_t *A = NULL;
    matrix_t *S = NULL;
    char *text = NULL;

    check_bool("mat_simplify_symbolic helper Δ allocated", delta != NULL);
    check_bool("mat_simplify_symbolic helper Ω allocated", omega != NULL);
    if (!delta || !omega)
        goto cleanup;

    prod1 = dv_mul(delta, omega);

    prod2 = dv_mul(delta, omega);

    neg_prod1 = dv_neg(prod1);
    entry = dv_add(neg_prod1, prod2);

    vals[0] = entry;
    vals[1] = entry;
    vals[2] = entry;
    vals[3] = entry;
    A = mat_create_dv(2, 2, vals);
    check_bool("mat_simplify_symbolic helper source matrix non-null", A != NULL);

    S = mat_simplify_symbolic(A);
    check_bool("mat_simplify_symbolic helper simplified matrix non-null", S != NULL);
    text = S ? mat_to_string(S, MAT_STRING_INLINE_PRETTY) : NULL;
    check_bool("mat_simplify_symbolic helper collapses symbolic zero",
               text && strcmp(text, "{ (0, 0; 0, 0) }") == 0);

cleanup:
    free(text);
    mat_free(S);
    mat_free(A);
    dv_free(entry);
    dv_free(neg_prod1);
    dv_free(prod2);
    dv_free(prod1);
    dv_free(omega);
    dv_free(delta);
}

/* ------------------------------------------------------------------ generic matrix check (double) */

void check_mat_d(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    double *g   = malloc(n * sizeof(double));
    double *e   = malloc(n * sizeof(double));
    double *err = malloc(n * sizeof(double));
    size_t *wg  = calloc(cols, sizeof(size_t));
    size_t *we  = calloc(cols, sizeof(size_t));
    size_t *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = fabs(g[k] - e[k]);
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    if (current_matrix_input_label[0] != '\0')
    {
        printf("    input context = %s\n", current_matrix_input_label);
        print_current_input_matrix();
    }

    char buf[256];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            d_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            d_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ matrix comparison helper */

static void check_mat2x2_d(const char *label,
                           matrix_t *R,
                           double e00, double e01,
                           double e10, double e11,
                           double tol)
{
    double ev[4] = {e00, e01, e10, e11};
    matrix_t *E = mat_create_d(2, 2, ev);
    check_mat_d(label, R, E, tol);
    mat_free(E);
}

/* ------------------------------------------------------------------ matrix identity helper (n×n double) */

void check_mat_identity_d(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_d(n);
    check_mat_d(label, R, I, tol);
    mat_free(I);
}

/* ------------------------------------------------------------------ matrix comparison helpers (qfloat) */

void check_mat_qf(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    qfloat_t *g   = malloc(n * sizeof(qfloat_t));
    qfloat_t *e   = malloc(n * sizeof(qfloat_t));
    double   *err = malloc(n * sizeof(double));
    size_t   *wg  = calloc(cols, sizeof(size_t));
    size_t   *we  = calloc(cols, sizeof(size_t));
    size_t   *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = qf_abs(qf_sub(g[k], e[k])).hi;
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    if (current_matrix_input_label[0] != '\0')
    {
        printf("    input context = %s\n", current_matrix_input_label);
        print_current_input_matrix();
    }

    char buf[512];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            qf_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            qf_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qf_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qf_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ matrix comparison helpers (qcomplex) */

void check_mat_qc(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    qcomplex_t *g   = malloc(n * sizeof(qcomplex_t));
    qcomplex_t *e   = malloc(n * sizeof(qcomplex_t));
    double     *err = malloc(n * sizeof(double));
    size_t     *wg  = calloc(cols, sizeof(size_t));
    size_t     *we  = calloc(cols, sizeof(size_t));
    size_t     *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = qf_to_double(qc_abs(qc_sub(g[k], e[k])));
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    if (current_matrix_input_label[0] != '\0')
    {
        printf("    input context = %s\n", current_matrix_input_label);
        print_current_input_matrix();
    }

    char buf[512];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            qc_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            qc_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qc_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" (%*s)", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qc_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" (%*s)", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ identity helpers (qfloat / qcomplex) */

void check_mat_identity_qf(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_qf(n);
    check_mat_qf(label, R, I, tol);
    mat_free(I);
}

void check_mat_identity_qc(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_qc(n);
    check_mat_qc(label, R, I, tol);
    mat_free(I);
}

static void check_unary_jordan_2x2_d(const char *label,
                                     matrix_t *(*fun)(const matrix_t *),
                                     double a, double fa, double fpa, double tol);
static void check_unary_diagonal_2x2_qc(const char *label,
                                        matrix_t *(*fun)(const matrix_t *),
                                        qcomplex_t a, qcomplex_t b,
                                        qcomplex_t fa, qcomplex_t fb,
                                        double tol);

static void check_unary_jordan_2x2_qf(const char *label,
                                      matrix_t *(*fun)(const matrix_t *),
                                      qfloat_t a, qfloat_t fa, qfloat_t fpa, double tol)
{
    qfloat_t avals[4] = {a, QF_ONE, QF_ZERO, a};
    qfloat_t evals[4] = {fa, fpa, QF_ZERO, fa};
    matrix_t *A = mat_create_qf(2, 2, avals);
    matrix_t *E = mat_create_qf(2, 2, evals);

    check_bool("2x2 qfloat Jordan input allocated", A != NULL);
    check_bool("2x2 qfloat Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqf("A (qfloat Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_qf(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_d(const char *label,
                                       matrix_t *(*fun)(const matrix_t *),
                                       double a, double b,
                                       double fa, double fb,
                                       double tol)
{
    double avals[4] = {a, 0.0, 0.0, b};
    double evals[4] = {fa, 0.0, 0.0, fb};
    matrix_t *A = mat_create_d(2, 2, avals);
    matrix_t *E = mat_create_d(2, 2, evals);

    check_bool("2x2 diagonal input allocated", A != NULL);
    check_bool("2x2 diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (2x2 diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_d(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_qf(const char *label,
                                        matrix_t *(*fun)(const matrix_t *),
                                        qfloat_t a, qfloat_t b,
                                        qfloat_t fa, qfloat_t fb,
                                        double tol)
{
    qfloat_t avals[4] = {a, QF_ZERO, QF_ZERO, b};
    qfloat_t evals[4] = {fa, QF_ZERO, QF_ZERO, fb};
    matrix_t *A = mat_create_qf(2, 2, avals);
    matrix_t *E = mat_create_qf(2, 2, evals);

    check_bool("2x2 qfloat diagonal input allocated", A != NULL);
    check_bool("2x2 qfloat diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqf("A (2x2 qfloat diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_qf(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void test_mat_special_unary_extensions(void)
{
    printf(C_CYAN "TEST: extended unary matrix special functions\n" C_RESET);

    {
        qfloat_t x = qf_from_double(0.25);
        qfloat_t y = qf_erfinv(x);
        qfloat_t fp = qf_mul(qf_from_double(0.5), qf_mul(QF_SQRT_PI, qf_exp(qf_mul(y, y))));
        check_unary_jordan_2x2_d("erfinv(aI+N)=erfinv(a)I+erfinv'(a)N",
                                 mat_erfinv,
                                 0.25,
                                 qf_to_double(y),
                                 qf_to_double(fp),
                                 1e-12);
    }

    {
        qfloat_t x = qf_from_double(0.75);
        qfloat_t y = qf_erfcinv(x);
        qfloat_t fp = qf_neg(qf_mul(qf_from_double(0.5), qf_mul(QF_SQRT_PI, qf_exp(qf_mul(y, y)))));
        check_unary_jordan_2x2_d("erfcinv(aI+N)=erfcinv(a)I+erfcinv'(a)N",
                                 mat_erfcinv,
                                 0.75,
                                 qf_to_double(y),
                                 qf_to_double(fp),
                                 1e-12);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        check_unary_jordan_2x2_d("lgamma(aI+N)=lgamma(a)I+digamma(a)N",
                                 mat_lgamma,
                                 2.5,
                                 qf_to_double(qf_lgamma(x)),
                                 qf_to_double(qf_digamma(x)),
                                 1e-12);

        check_unary_diagonal_2x2_d("tetragamma(diag(a,b)) = diag(tetragamma(a), tetragamma(b))",
                                   mat_tetragamma,
                                   2.5, 1.75,
                                   qf_to_double(qf_tetragamma(qf_from_double(2.5))),
                                   qf_to_double(qf_tetragamma(qf_from_double(1.75))),
                                   1e-12);
    }

    {
        qfloat_t a = qf_from_double(1.329340388179137);
        qfloat_t y = qf_gammainv(a);
        qfloat_t fp = qf_div(QF_ONE, qf_mul(a, qf_digamma(y)));
        check_unary_jordan_2x2_d("gammainv(aI+N)=gammainv(a)I+gammainv'(a)N",
                                 mat_gammainv,
                                 1.329340388179137,
                                 qf_to_double(y),
                                 qf_to_double(fp),
                                 1e-10);
    }

    {
        qfloat_t x = qf_from_double(0.5);
        qfloat_t pdf = qf_normal_pdf(x);
        check_unary_jordan_2x2_d("normal_pdf(aI+N)=pdf(a)I+pdf'(a)N",
                                 mat_normal_pdf,
                                 0.5,
                                 qf_to_double(pdf),
                                 qf_to_double(qf_neg(qf_mul(x, pdf))),
                                 1e-12);

        check_unary_jordan_2x2_d("normal_cdf(aI+N)=cdf(a)I+cdf'(a)N",
                                 mat_normal_cdf,
                                 0.5,
                                 qf_to_double(qf_normal_cdf(x)),
                                 qf_to_double(pdf),
                                 1e-12);

        check_unary_jordan_2x2_d("normal_logpdf(aI+N)=logpdf(a)I+logpdf'(a)N",
                                 mat_normal_logpdf,
                                 0.5,
                                 qf_to_double(qf_normal_logpdf(x)),
                                 qf_to_double(qf_neg(x)),
                                 1e-12);
    }

    {
        qfloat_t a = qf_from_double(0.2);
        qfloat_t b = qf_from_double(-0.05);
        check_unary_diagonal_2x2_d("productlog(diag(a,b)) = diag(W0(a),W0(b))",
                                   mat_productlog,
                                   0.2, -0.05,
                                   qf_to_double(qf_productlog(a)),
                                   qf_to_double(qf_productlog(b)),
                                   1e-12);

        check_unary_diagonal_2x2_qf("qf productlog(diag(a,b)) = diag(W0(a),W0(b))",
                                    mat_productlog,
                                    a, b,
                                    qf_productlog(a),
                                    qf_productlog(b),
                                    1e-25);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t a = qf_from_double(0.2);
        qfloat_t b = qf_from_double(-0.2);
        qfloat_t ei_x = qf_from_double(0.5);

        check_unary_jordan_2x2_qf("qf digamma(aI+N)=digamma(a)I+trigamma(a)N",
                                  mat_digamma,
                                  x,
                                  qf_digamma(x),
                                  qf_trigamma(x),
                                  1e-25);

        check_unary_jordan_2x2_qf("qf lambert_w0(aI+N)=W0(a)I+W0'(a)N",
                                  mat_lambert_w0,
                                  a,
                                  qf_lambert_w0(a),
                                  qf_div(qf_lambert_w0(a), qf_mul(a, qf_add(QF_ONE, qf_lambert_w0(a)))),
                                  1e-25);

        check_unary_jordan_2x2_qf("qf lambert_wm1(aI+N)=Wm1(a)I+Wm1'(a)N",
                                  mat_lambert_wm1,
                                  b,
                                  qf_lambert_wm1(b),
                                  qf_div(qf_lambert_wm1(b), qf_mul(b, qf_add(QF_ONE, qf_lambert_wm1(b)))),
                                  1e-25);

        check_unary_jordan_2x2_qf("qf ei(aI+N)=Ei(a)I+Ei'(a)N",
                                  mat_ei,
                                  ei_x,
                                  qf_ei(ei_x),
                                  qf_div(qf_exp(ei_x), ei_x),
                                  1e-25);
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(-0.05), qf_from_double(0.08));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), QF_ZERO);
        qcomplex_t wm1b = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));

        check_unary_diagonal_2x2_qc("qc gamma(diag(z1,z2)) = diag(gamma(z1),gamma(z2))",
                                    mat_gamma,
                                    z1, z2,
                                    qc_gamma(z1), qc_gamma(z2), 1e-24);

        check_unary_diagonal_2x2_qc("qc digamma(diag(z1,z2)) = diag(digamma(z1),digamma(z2))",
                                    mat_digamma,
                                    z1, z2,
                                    qc_digamma(z1), qc_digamma(z2), 1e-24);

        check_unary_diagonal_2x2_qc("qc productlog(diag(a,b)) = diag(W0(a),W0(b))",
                                    mat_productlog,
                                    w0a, w0b,
                                    qc_productlog(w0a), qc_productlog(w0b), 1e-24);

        check_unary_diagonal_2x2_qc("qc lambert_wm1(diag(a,b)) = diag(Wm1(a),Wm1(b))",
                                    mat_lambert_wm1,
                                    wm1a, wm1b,
                                    qc_make(qf_lambert_wm1(qf_from_double(-0.2)), QF_ZERO),
                                    qc_lambert_wm1(wm1b), 1e-24);

        check_unary_diagonal_2x2_qc("qc ei(diag(z1,z2)) = diag(Ei(z1),Ei(z2))",
                                    mat_ei,
                                    z1, z2,
                                    qc_ei(z1), qc_ei(z2), 1e-24);
    }
}

static void check_unary_jordan_2x2_d(const char *label,
                                     matrix_t *(*fun)(const matrix_t *),
                                     double a, double fa, double fpa, double tol)
{
    double avals[4] = {a, 1.0, 0.0, a};
    double evals[4] = {fa, fpa, 0.0, fa};
    matrix_t *A = mat_create_d(2, 2, avals);
    matrix_t *E = mat_create_d(2, 2, evals);

    check_bool("2x2 Jordan input allocated", A != NULL);
    check_bool("2x2 Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_d(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_2x2_qc(const char *label,
                                        matrix_t *(*fun)(const matrix_t *),
                                        qcomplex_t a, qcomplex_t b,
                                        qcomplex_t fa, qcomplex_t fb,
                                        double tol)
{
    qcomplex_t avals[4] = {a, QC_ZERO, QC_ZERO, b};
    qcomplex_t evals[4] = {fa, QC_ZERO, QC_ZERO, fb};
    matrix_t *A = mat_create_qc(2, 2, avals);
    matrix_t *E = mat_create_qc(2, 2, evals);

    check_bool("2x2 qcomplex diagonal input allocated", A != NULL);
    check_bool("2x2 qcomplex diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqc("A (qcomplex diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_qc(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_jordan_3x3_d(const char *label,
                                     matrix_t *(*fun)(const matrix_t *),
                                     double a, double fa, double fpa,
                                     double fppa_over_2, double tol)
{
    double avals[9] = {
        a, 1.0, 0.0,
        0.0, a, 1.0,
        0.0, 0.0, a
    };
    double evals[9] = {
        fa, fpa, fppa_over_2,
        0.0, fa, fpa,
        0.0, 0.0, fa
    };
    matrix_t *A = mat_create_d(3, 3, avals);
    matrix_t *E = mat_create_d(3, 3, evals);

    check_bool("3x3 Jordan input allocated", A != NULL);
    check_bool("3x3 Jordan expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_md("A (3x3 Jordan block)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_d(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void check_unary_diagonal_3x3_qc(const char *label,
                                        matrix_t *(*fun)(const matrix_t *),
                                        qcomplex_t a, qcomplex_t b, qcomplex_t c,
                                        qcomplex_t fa, qcomplex_t fb, qcomplex_t fc,
                                        double tol)
{
    qcomplex_t avals[9] = {
        a, QC_ZERO, QC_ZERO,
        QC_ZERO, b, QC_ZERO,
        QC_ZERO, QC_ZERO, c
    };
    qcomplex_t evals[9] = {
        fa, QC_ZERO, QC_ZERO,
        QC_ZERO, fb, QC_ZERO,
        QC_ZERO, QC_ZERO, fc
    };
    matrix_t *A = mat_create_qc(3, 3, avals);
    matrix_t *E = mat_create_qc(3, 3, evals);

    check_bool("3x3 qcomplex diagonal input allocated", A != NULL);
    check_bool("3x3 qcomplex diagonal expected allocated", E != NULL);
    if (!A || !E) {
        mat_free(A);
        mat_free(E);
        return;
    }

    print_mqc("A (3x3 qcomplex diagonal)", A);
    matrix_t *R = fun(A);
    check_bool(label, R != NULL);
    if (R)
        check_mat_qc(label, R, E, tol);

    mat_free(R);
    mat_free(A);
    mat_free(E);
}

static void test_mat_special_unary_square_extensions(void)
{
    printf(C_CYAN "TEST: extended unary functions on square matrices\n" C_RESET);

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t gamma_x = qf_gamma(x);
        qfloat_t digamma_x = qf_digamma(x);
        qfloat_t trigamma_x = qf_trigamma(x);
        qfloat_t one = qf_from_double(1.0);

        check_unary_jordan_2x2_d("gamma(aI+N)=gamma(a)I+gamma'(a)N",
                                 mat_gamma,
                                 2.5,
                                 qf_to_double(gamma_x),
                                 qf_to_double(qf_mul(gamma_x, digamma_x)),
                                 1e-12);

        check_unary_jordan_2x2_d("digamma(aI+N)=digamma(a)I+trigamma(a)N",
                                 mat_digamma,
                                 2.5,
                                 qf_to_double(digamma_x),
                                 qf_to_double(trigamma_x),
                                 1e-12);

        check_unary_jordan_2x2_d("trigamma(aI+N)=trigamma(a)I+tetragamma(a)N",
                                 mat_trigamma,
                                 2.5,
                                 qf_to_double(trigamma_x),
                                 qf_to_double(qf_tetragamma(x)),
                                 1e-12);

        {
            qfloat_t a = qf_from_double(0.2);
            qfloat_t w = qf_lambert_w0(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            check_unary_jordan_2x2_d("lambert_w0(aI+N)=W0(a)I+W0'(a)N",
                                     mat_lambert_w0,
                                     0.2,
                                     qf_to_double(w),
                                     qf_to_double(wp),
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(-0.2);
            qfloat_t w = qf_lambert_wm1(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            check_unary_jordan_2x2_d("lambert_wm1(aI+N)=Wm1(a)I+Wm1'(a)N",
                                     mat_lambert_wm1,
                                     -0.2,
                                     qf_to_double(w),
                                     qf_to_double(wp),
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(0.5);
            check_unary_jordan_2x2_d("ei(aI+N)=Ei(a)I+Ei'(a)N",
                                     mat_ei,
                                     0.5,
                                     qf_to_double(qf_ei(a)),
                                     qf_to_double(qf_div(qf_exp(a), a)),
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(1.0);
            check_unary_jordan_2x2_d("e1(aI+N)=E1(a)I+E1'(a)N",
                                     mat_e1,
                                     1.0,
                                     qf_to_double(qf_e1(a)),
                                     qf_to_double(qf_div(qf_neg(qf_exp(qf_neg(a))), a)),
                                     1e-12);
        }
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(0.1), qf_from_double(0.05));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));
        qcomplex_t wm1b = qc_make(qf_from_double(-0.1), qf_from_double(-0.05));

        check_unary_diagonal_2x2_qc("qc gamma(diag(z1,z2)) = diag(gamma(z1),gamma(z2))",
                                    mat_gamma,
                                    z1, z2,
                                    qc_gamma(z1), qc_gamma(z2), 1e-24);

        check_unary_diagonal_2x2_qc("qc digamma(diag(z1,z2)) = diag(digamma(z1),digamma(z2))",
                                    mat_digamma,
                                    z1, z2,
                                    qc_digamma(z1), qc_digamma(z2), 1e-24);

        check_unary_diagonal_2x2_qc("qc lambert_w0(diag(a,b)) = diag(W0(a),W0(b))",
                                    mat_lambert_w0,
                                    w0a, w0b,
                                    qc_productlog(w0a), qc_productlog(w0b), 1e-24);

        check_unary_diagonal_2x2_qc("qc lambert_wm1(diag(a,b)) = diag(Wm1(a),Wm1(b))",
                                    mat_lambert_wm1,
                                    wm1a, wm1b,
                                    qc_lambert_wm1(wm1a), qc_lambert_wm1(wm1b), 1e-24);

        check_unary_diagonal_2x2_qc("qc ei(diag(z1,z2)) = diag(Ei(z1),Ei(z2))",
                                    mat_ei,
                                    z1, z2,
                                    qc_ei(z1), qc_ei(z2), 1e-24);
    }

    {
        qfloat_t x = qf_from_double(2.5);
        qfloat_t gamma_x = qf_gamma(x);
        qfloat_t digamma_x = qf_digamma(x);
        qfloat_t trigamma_x = qf_trigamma(x);
        qfloat_t tetragamma_x = qf_tetragamma(x);
        qfloat_t one = qf_from_double(1.0);

        check_unary_jordan_3x3_d("gamma(aI+N+N^2)=gamma(a)I+gamma'(a)N+gamma''(a)N^2/2",
                                 mat_gamma,
                                 2.5,
                                 qf_to_double(gamma_x),
                                 qf_to_double(qf_mul(gamma_x, digamma_x)),
                                 qf_to_double(qf_mul(gamma_x,
                                                     qf_add(trigamma_x,
                                                            qf_mul(digamma_x, digamma_x))))
                                     / 2.0,
                                 1e-12);

        check_unary_jordan_3x3_d("digamma(aI+N+N^2)=digamma(a)I+trigamma(a)N+tetragamma(a)N^2/2",
                                 mat_digamma,
                                 2.5,
                                 qf_to_double(digamma_x),
                                 qf_to_double(trigamma_x),
                                 qf_to_double(tetragamma_x) / 2.0,
                                 1e-12);

        {
            qfloat_t a = qf_from_double(0.2);
            qfloat_t w = qf_lambert_w0(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            qfloat_t wpp = qf_neg(qf_div(qf_mul(qf_mul(w, w), qf_add(w, qf_from_double(2.0))),
                                         qf_mul(qf_mul(a, a),
                                                qf_mul(qf_add(one, w),
                                                       qf_mul(qf_add(one, w), qf_add(one, w))))));
            check_unary_jordan_3x3_d("lambert_w0(aI+N+N^2)=W0(a)I+W0'(a)N+W0''(a)N^2/2",
                                     mat_lambert_w0,
                                     0.2,
                                     qf_to_double(w),
                                     qf_to_double(wp),
                                     qf_to_double(wpp) / 2.0,
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(-0.2);
            qfloat_t w = qf_lambert_wm1(a);
            qfloat_t wp = qf_div(w, qf_mul(a, qf_add(one, w)));
            qfloat_t wpp = qf_neg(qf_div(qf_mul(qf_mul(w, w), qf_add(w, qf_from_double(2.0))),
                                         qf_mul(qf_mul(a, a),
                                                qf_mul(qf_add(one, w),
                                                       qf_mul(qf_add(one, w), qf_add(one, w))))));
            check_unary_jordan_3x3_d("lambert_wm1(aI+N+N^2)=Wm1(a)I+Wm1'(a)N+Wm1''(a)N^2/2",
                                     mat_lambert_wm1,
                                     -0.2,
                                     qf_to_double(w),
                                     qf_to_double(wp),
                                     qf_to_double(wpp) / 2.0,
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(0.5);
            check_unary_jordan_3x3_d("ei(aI+N+N^2)=Ei(a)I+Ei'(a)N+Ei''(a)N^2/2",
                                     mat_ei,
                                     0.5,
                                     qf_to_double(qf_ei(a)),
                                     qf_to_double(qf_div(qf_exp(a), a)),
                                     qf_to_double(qf_div(qf_mul(qf_exp(a),
                                                                qf_sub(a, one)),
                                                         qf_mul(a, a))) / 2.0,
                                     1e-12);
        }

        {
            qfloat_t a = qf_from_double(1.0);
            check_unary_jordan_3x3_d("e1(aI+N+N^2)=E1(a)I+E1'(a)N+E1''(a)N^2/2",
                                     mat_e1,
                                     1.0,
                                     qf_to_double(qf_e1(a)),
                                     qf_to_double(qf_div(qf_neg(qf_exp(qf_neg(a))), a)),
                                     qf_to_double(qf_div(qf_mul(qf_exp(qf_neg(a)),
                                                                qf_add(a, one)),
                                                         qf_mul(a, a))) / 2.0,
                                     1e-12);
        }
    }

    {
        qcomplex_t z1 = qc_make(qf_from_double(1.2), qf_from_double(0.3));
        qcomplex_t z2 = qc_make(qf_from_double(0.5), qf_from_double(0.2));
        qcomplex_t z3 = qc_make(qf_from_double(0.8), qf_from_double(-0.25));
        qcomplex_t w0a = qc_make(qf_from_double(0.2), qf_from_double(0.1));
        qcomplex_t w0b = qc_make(qf_from_double(0.1), qf_from_double(0.05));
        qcomplex_t w0c = qc_make(qf_from_double(0.15), qf_from_double(-0.08));
        qcomplex_t wm1a = qc_make(qf_from_double(-0.2), qf_from_double(-0.1));
        qcomplex_t wm1b = qc_make(qf_from_double(-0.1), qf_from_double(-0.05));
        qcomplex_t wm1c = qc_make(qf_from_double(-0.16), qf_from_double(-0.07));

        check_unary_diagonal_3x3_qc("qc gamma(diag(z1,z2,z3)) = diag(gamma(z1),gamma(z2),gamma(z3))",
                                    mat_gamma,
                                    z1, z2, z3,
                                    qc_gamma(z1), qc_gamma(z2), qc_gamma(z3), 1e-24);

        check_unary_diagonal_3x3_qc("qc digamma(diag(z1,z2,z3)) = diag(digamma(z1),digamma(z2),digamma(z3))",
                                    mat_digamma,
                                    z1, z2, z3,
                                    qc_digamma(z1), qc_digamma(z2), qc_digamma(z3), 1e-24);

        check_unary_diagonal_3x3_qc("qc lambert_w0(diag(a,b,c)) = diag(W0(a),W0(b),W0(c))",
                                    mat_lambert_w0,
                                    w0a, w0b, w0c,
                                    qc_productlog(w0a), qc_productlog(w0b), qc_productlog(w0c), 1e-24);

        check_unary_diagonal_3x3_qc("qc lambert_wm1(diag(a,b,c)) = diag(Wm1(a),Wm1(b),Wm1(c))",
                                    mat_lambert_wm1,
                                    wm1a, wm1b, wm1c,
                                    qc_lambert_wm1(wm1a), qc_lambert_wm1(wm1b), qc_lambert_wm1(wm1c), 1e-24);

        check_unary_diagonal_3x3_qc("qc ei(diag(z1,z2,z3)) = diag(Ei(z1),Ei(z2),Ei(z3))",
                                    mat_ei,
                                    z1, z2, z3,
                                    qc_ei(z1), qc_ei(z2), qc_ei(z3), 1e-24);
    }
}

static void test_mat_neg_convenience(void)
{
    printf(C_CYAN "TEST: mat_neg convenience wrapper\n" C_RESET);

    double dvals[4] = {1.0, -2.0, 3.5, 0.0};
    double dexp[4] = {-1.0, 2.0, -3.5, 0.0};
    matrix_t *A = mat_create_d(2, 2, dvals);
    matrix_t *E = mat_create_d(2, 2, dexp);
    check_bool("double neg input allocated", A != NULL);
    check_bool("double neg expected allocated", E != NULL);
    if (A && E) {
        print_md("A", A);
        matrix_t *N = mat_neg(A);
        check_bool("mat_neg(double) not NULL", N != NULL);
        if (N) {
            check_mat_d("mat_neg(double) = -A", N, E, 1e-30);
            mat_free(N);
        }
    }
    mat_free(A);
    mat_free(E);

    qcomplex_t qvals[2] = {
        qc_make(qf_from_double(1.0), qf_from_double(-2.0)),
        qc_make(qf_from_double(-0.5), qf_from_double(0.25))
    };
    qcomplex_t qexp[2] = {
        qc_make(qf_from_double(-1.0), qf_from_double(2.0)),
        qc_make(qf_from_double(0.5), qf_from_double(-0.25))
    };
    matrix_t *Q = mat_create_qc(1, 2, qvals);
    matrix_t *QE = mat_create_qc(1, 2, qexp);
    check_bool("qcomplex neg input allocated", Q != NULL);
    check_bool("qcomplex neg expected allocated", QE != NULL);
    if (Q && QE) {
        print_mqc("Q", Q);
        matrix_t *QN = mat_neg(Q);
        check_bool("mat_neg(qcomplex) not NULL", QN != NULL);
        if (QN) {
            check_mat_qc("mat_neg(qcomplex) = -Q", QN, QE, 1e-28);
            mat_free(QN);
        }
    }
    mat_free(Q);
    mat_free(QE);

    {
        matrix_t *S = mat_new_sparse_d(2, 2);
        matrix_t *SE = NULL;
        double vals[4] = {0.0, 3.0, -2.0, 0.0};
        double minus_three = -3.0, two = 2.0;

        check_bool("sparse neg input allocated", S != NULL);
        if (S) {
            mat_set(S, 0, 1, &minus_three);
            mat_set(S, 1, 0, &two);
            SE = mat_create_d(2, 2, vals);
            print_md("S", S);
            matrix_t *SN = mat_neg(S);
            check_bool("mat_neg(sparse) not NULL", SN != NULL);
            check_bool("mat_neg(sparse) stays sparse", SN && mat_is_sparse(SN));
            if (SN) {
                check_mat_d("mat_neg(sparse) = -S", SN, SE, 1e-30);
                mat_free(SN);
            }
        }

        mat_free(SE);
        mat_free(S);
    }
}

/* ------------------------------------------------------------------ nilpotent matrix tests */

/*
 * N = [[0,1],[0,0]] is nilpotent: N² = 0.  Every Taylor series truncates
 * after the linear term, giving exact closed-form results for all functions.
 */
static void test_mat_nilpotent_d(void)
{
    printf(C_CYAN "TEST: nilpotent matrix N=[[0,1],[0,0]] exact values\n" C_RESET);

    double nvals[4] = {0.0, 1.0, 0.0, 0.0};
    matrix_t *N = mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;
    print_md("N", N);

    /* exp(N) = I + N = [[1,1],[0,1]] */
    {
        matrix_t *R = mat_exp(N);
        print_md("exp(N)", R);
        check_mat2x2_d("exp(N)", R, 1.0, 1.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* sin(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_sin(N);
        print_md("sin(N)", R);
        check_mat2x2_d("sin(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* cos(N) = I = [[1,0],[0,1]] */
    {
        matrix_t *R = mat_cos(N);
        print_md("cos(N)", R);
        check_mat2x2_d("cos(N)", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* tan(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_tan(N);
        print_md("tan(N)", R);
        check_mat2x2_d("tan(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* sinh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_sinh(N);
        print_md("sinh(N)", R);
        check_mat2x2_d("sinh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* cosh(N) = I = [[1,0],[0,1]] */
    {
        matrix_t *R = mat_cosh(N);
        print_md("cosh(N)", R);
        check_mat2x2_d("cosh(N)", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* tanh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_tanh(N);
        print_md("tanh(N)", R);
        check_mat2x2_d("tanh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* asin(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_asin(N);
        print_md("asin(N)", R);
        check_mat2x2_d("asin(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* acos(N) = π/2·I - N = [[π/2,-1],[0,π/2]] */
    {
        matrix_t *R = mat_acos(N);
        print_md("acos(N)", R);
        check_mat2x2_d("acos(N)", R, M_PI / 2.0, -1.0, 0.0, M_PI / 2.0, 1e-12);
        mat_free(R);
    }

    /* atan(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_atan(N);
        print_md("atan(N)", R);
        check_mat2x2_d("atan(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* asinh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_asinh(N);
        print_md("asinh(N)", R);
        check_mat2x2_d("asinh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* atanh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_atanh(N);
        print_md("atanh(N)", R);
        check_mat2x2_d("atanh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* erf(N) = (2/√π)·N = [[0, 2/√π],[0,0]] */
    {
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erf(N);
        print_md("erf(N)", R);
        check_mat2x2_d("erf(N)", R, 0.0, two_over_sqrtpi, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* erfc(N) = I - (2/√π)·N = [[1, -2/√π],[0,1]] */
    {
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erfc(N);
        print_md("erfc(N)", R);
        check_mat2x2_d("erfc(N)", R, 1.0, -two_over_sqrtpi, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    mat_free(N);

    /* sqrt(I+N) = I + N/2 = [[1,0.5],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_sqrt(IN);
        print_md("sqrt(I+N)", R);
        check_mat2x2_d("sqrt(I+N)", R, 1.0, 0.5, 0.0, 1.0, 1e-12);
        mat_free(IN);
        mat_free(R);
    }

    /* log(I+N) = N = [[0,1],[0,0]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_log(IN);
        print_md("log(I+N)", R);
        check_mat2x2_d("log(I+N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(IN);
        mat_free(R);
    }
}

/* ------------------------------------------------------------------ algebraic identity tests */

/*
 * For any square matrix A:
 *   sin²(A) + cos²(A) = I
 *   cosh²(A) - sinh²(A) = I
 *   exp(A) · exp(-A) = I
 * These hold exactly (up to floating-point rounding) because the doubling
 * formulas used internally preserve Pythagorean relations.
 */
static void test_mat_algebraic_ids_d(void)
{
    printf(C_CYAN "TEST: algebraic identities for non-diagonal matrices\n" C_RESET);

    /* A = [[0.3, 0.1],[0.1, 0.4]] — symmetric, non-diagonal */
    double avals[4] = {0.3, 0.1, 0.1, 0.4};
    matrix_t *A = mat_create_d(2, 2, avals);
    check_bool("A allocated", A != NULL);
    if (!A)
        return;
    print_md("A", A);

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) not NULL", S != NULL);
        check_bool("cos(A) not NULL", C != NULL);
        if (S && C)
        {
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            check_bool("sin²(A) not NULL", S2 != NULL);
            check_bool("cos²(A) not NULL", C2 != NULL);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                print_md("sin²(A)+cos²(A)", I);
                check_mat2x2_d("sin²+cos²=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
                mat_free(I);
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
        check_bool("cosh(A) not NULL", CH != NULL);
        check_bool("sinh(A) not NULL", SH != NULL);
        if (CH && SH)
        {
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            check_bool("cosh²(A) not NULL", CH2 != NULL);
            check_bool("sinh²(A) not NULL", SH2 != NULL);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                print_md("cosh²(A)-sinh²(A)", I);
                check_mat2x2_d("cosh²-sinh²=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
                mat_free(I);
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        double negvals[4] = {-0.3, -0.1, -0.1, -0.4};
        matrix_t *negA = mat_create_d(2, 2, negvals);
        matrix_t *EnA = mat_exp(negA);
        check_bool("exp(A) not NULL", E != NULL);
        check_bool("exp(-A) not NULL", EnA != NULL);
        if (E && EnA)
        {
            matrix_t *I = mat_mul(E, EnA);
            print_md("exp(A)·exp(-A)", I);
            check_mat2x2_d("exp(A)·exp(-A)=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
            mat_free(I);
        }
        mat_free(E);
        mat_free(negA);
        mat_free(EnA);
    }

    mat_free(A);
}

/* ------------------------------------------------------------------ round-trip tests */

static void test_mat_roundtrips_d(void)
{
    printf(C_CYAN "TEST: round-trip identities for non-diagonal matrices\n" C_RESET);

    /* exp(log(A)) = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *L = mat_log(A);
        check_bool("log(A) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("exp(log(A))", R);
                check_mat2x2_d("exp(log(A))=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
                mat_free(R);
            }
            mat_free(L);
        }
        mat_free(A);
    }

    /* sinh(asinh(A)) = A for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("asinh(A) not NULL", S != NULL);
        if (S)
        {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sinh(asinh(A))", R);
                check_mat2x2_d("sinh(asinh(A))=A", R, 0.3, 0.1, 0.1, 0.4, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }

    /* sqrt(A)·sqrt(A) = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("sqrt(A) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            matrix_t *R = mat_mul(S, S);
            check_bool("sqrt(A)·sqrt(A) not NULL", R != NULL);
            if (R)
            {
                print_md("sqrt(A)²", R);
                check_mat2x2_d("sqrt(A)²=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }

    /* atan(tan(A)) = A for small symmetric A = [[0.3,0.1],[0.1,0.2]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.2};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("tan(A) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            matrix_t *R = mat_atan(T);
            check_bool("atan(tan(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("atan(tan(A))", R);
                check_mat2x2_d("atan(tan(A))=A", R, 0.3, 0.1, 0.1, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_pow_int tests */

static void test_mat_pow_int_d(void)
{
    printf(C_CYAN "TEST: mat_pow_int (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_pow_int(NULL,0) = NULL", mat_pow_int(NULL, 0) == NULL);

    double nvals[4] = {0.0, 1.0, 0.0, 0.0};
    matrix_t *N = mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;
    print_md("N", N);

    /* N^0 = I */
    {
        matrix_t *R = mat_pow_int(N, 0);
        print_md("N^0", R);
        check_mat2x2_d("N^0=I", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* N^1 = N */
    {
        matrix_t *R = mat_pow_int(N, 1);
        print_md("N^1", R);
        check_mat2x2_d("N^1=N", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* N^2 = 0 */
    {
        matrix_t *R = mat_pow_int(N, 2);
        print_md("N^2", R);
        check_mat2x2_d("N^2=0", R, 0.0, 0.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    mat_free(N);

    /* (I+N)^n = I + n·N  for upper-triangular Jordan blocks */
    double invals[4] = {1.0, 1.0, 0.0, 1.0};
    matrix_t *IN = mat_create_d(2, 2, invals);
    check_bool("I+N allocated", IN != NULL);
    if (!IN)
        return;
    print_md("I+N", IN);

    /* (I+N)^0 = I */
    {
        matrix_t *R = mat_pow_int(IN, 0);
        print_md("(I+N)^0", R);
        check_mat2x2_d("(I+N)^0=I", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^2 = I + 2N = [[1,2],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, 2);
        print_md("(I+N)^2", R);
        check_mat2x2_d("(I+N)^2", R, 1.0, 2.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^3 = I + 3N = [[1,3],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, 3);
        print_md("(I+N)^3", R);
        check_mat2x2_d("(I+N)^3", R, 1.0, 3.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^-1 = I - N = [[1,-1],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, -1);
        print_md("(I+N)^-1", R);
        check_mat2x2_d("(I+N)^-1", R, 1.0, -1.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^-2 = I - 2N = [[1,-2],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, -2);
        print_md("(I+N)^-2", R);
        check_mat2x2_d("(I+N)^-2", R, 1.0, -2.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    mat_free(IN);

    /* diagonal matrix: diag(2,3)^4 = diag(16,81) */
    {
        double dvals[4] = {2.0, 0.0, 0.0, 3.0};
        matrix_t *D = mat_create_d(2, 2, dvals);
        print_md("D", D);
        matrix_t *R = mat_pow_int(D, 4);
        print_md("diag(2,3)^4", R);
        check_mat2x2_d("diag(2,3)^4=diag(16,81)", R, 16.0, 0.0, 0.0, 81.0, 1e-12);
        mat_free(D);
        mat_free(R);
    }

    /* symbolic Jordan block: [[x,1],[0,x]]^n */
    {
        binding_t *bindings = NULL;
        size_t number = 0;
        matrix_t *J = mat_from_string("(x, 1; 0, x)", &bindings, &number);
        matrix_t *J2 = NULL;
        matrix_t *J3 = NULL;
        char *j2_text = NULL;
        char *j3_text = NULL;

        check_bool("symbolic Jordan block allocated", J != NULL);
        check_bool("symbolic Jordan block bindings returned", bindings != NULL);
        check_bool("symbolic Jordan block x binding present",
                   bindings && mat_binding_find(bindings, number, "x") != NULL);

        J2 = mat_pow_int(J, 2);
        J3 = mat_pow_int(J, 3);
        check_bool("symbolic Jordan block squared", J2 != NULL);
        check_bool("symbolic Jordan block cubed", J3 != NULL);

        j2_text = J2 ? mat_to_string(J2, MAT_STRING_INLINE_PRETTY) : NULL;
        j3_text = J3 ? mat_to_string(J3, MAT_STRING_INLINE_PRETTY) : NULL;

        check_bool("symbolic Jordan block J^2 exact text",
                   j2_text && strcmp(j2_text, "(x², 2x; 0, x²)") == 0);
        check_bool("symbolic Jordan block J^3 exact text",
                   j3_text && strcmp(j3_text, "(x³, 3x²; 0, x³)") == 0);

        free(j3_text);
        free(j2_text);
        mat_free(J3);
        mat_free(J2);
        free(bindings);
        mat_free(J);
    }
}

/* ------------------------------------------------------------------ mat_pow tests */

static void test_mat_pow_d(void)
{
    printf(C_CYAN "TEST: mat_pow (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_pow(NULL,1.0) = NULL", mat_pow(NULL, 1.0) == NULL);

    /* (I+N)^0.5 = I + 0.5·N = [[1,0.5],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_pow(IN, 0.5);
        print_md("(I+N)^0.5", R);
        check_mat2x2_d("(I+N)^0.5", R, 1.0, 0.5, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
    }

    /* (I+N)^2.0 = I + 2N = [[1,2],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        print_md("I+N", IN);
        matrix_t *R = mat_pow(IN, 2.0);
        print_md("(I+N)^2.0", R);
        check_mat2x2_d("(I+N)^2.0", R, 1.0, 2.0, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
    }

    /* A^1.0 = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (positive-definite)", A);
        matrix_t *R = mat_pow(A, 1.0);
        print_md("A^1.0", R);
        check_mat2x2_d("A^1.0=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
        mat_free(A);
        mat_free(R);
    }

    /* pow(pow_int): (A^2.0)[i,j] ≈ (A²)[i,j] for positive-definite A */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (positive-definite)", A);
        matrix_t *Rp = mat_pow(A, 2.0);
        matrix_t *Ri = mat_pow_int(A, 2);
        check_bool("A^2.0 not NULL", Rp != NULL);
        check_bool("A^2   not NULL", Ri != NULL);
        if (Rp && Ri)
        {
            double p00, p01, i00, i01;
            mat_get(Rp, 0, 0, &p00);
            mat_get(Rp, 0, 1, &p01);
            mat_get(Ri, 0, 0, &i00);
            mat_get(Ri, 0, 1, &i01);
            check_d("A^2.0[0,0] = A²[0,0]", p00, i00, 1e-10);
            check_d("A^2.0[0,1] = A²[0,1]", p01, i01, 1e-10);
        }
        mat_free(A);
        mat_free(Rp);
        mat_free(Ri);
    }
}

/* ------------------------------------------------------------------ mat_erf tests */

static void test_mat_erf_d(void)
{
    printf(C_CYAN "TEST: mat_erf (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_erf(NULL) = NULL", mat_erf(NULL) == NULL);

    /* nilpotent: erf(N) = (2/√π)·N = [[0, 2/√π],[0,0]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = mat_create_d(2, 2, nvals);
        print_md("N (nilpotent)", N);
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erf(N);
        print_md("erf(N)", R);
        check_mat2x2_d("erf(N)=(2/√π)N", R, 0.0, two_over_sqrtpi, 0.0, 0.0, 1e-12);
        mat_free(N);
        mat_free(R);
    }

    /* odd symmetry: erf(-A) = -erf(A) for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        double navals[4] = {-0.3, -0.1, -0.1, -0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        matrix_t *nA = mat_create_d(2, 2, navals);
        print_md("A", A);
        print_md("-A", nA);
        matrix_t *E = mat_erf(A);
        matrix_t *En = mat_erf(nA);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erf(-A) not NULL", En != NULL);
        if (E && En)
        {
            print_md("erf(A)", E);
            print_md("erf(-A)", En);
            double e00, en00, e01, en01;
            mat_get(E, 0, 0, &e00);
            mat_get(E, 0, 1, &e01);
            mat_get(En, 0, 0, &en00);
            mat_get(En, 0, 1, &en01);
            check_d("erf(-A)[0,0] = -erf(A)[0,0]", en00, -e00, 1e-12);
            check_d("erf(-A)[0,1] = -erf(A)[0,1]", en01, -e01, 1e-12);
        }
        mat_free(A);
        mat_free(nA);
        mat_free(E);
        mat_free(En);
    }
}

/* ------------------------------------------------------------------ mat_erfc tests */

static void test_mat_erfc_d(void)
{
    printf(C_CYAN "TEST: mat_erfc (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_erfc(NULL) = NULL", mat_erfc(NULL) == NULL);

    /* nilpotent: erfc(N) = I - (2/√π)·N = [[1,-2/√π],[0,1]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = mat_create_d(2, 2, nvals);
        print_md("N (nilpotent)", N);
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erfc(N);
        print_md("erfc(N)", R);
        check_mat2x2_d("erfc(N)=I-(2/√π)N", R, 1.0, -two_over_sqrtpi, 0.0, 1.0, 1e-12);
        mat_free(N);
        mat_free(R);
    }

    /* erf(A) + erfc(A) = I for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *E = mat_erf(A);
        matrix_t *EC = mat_erfc(A);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erfc(A) not NULL", EC != NULL);
        if (E && EC)
        {
            matrix_t *Sum = mat_add(E, EC);
            print_md("erf(A)+erfc(A)", Sum);
            check_mat2x2_d("erf+erfc=I", Sum, 1.0, 0.0, 0.0, 1.0, 1e-12);
            mat_free(Sum);
        }
        mat_free(A);
        mat_free(E);
        mat_free(EC);
    }
}

/* ------------------------------------------------------------------ mat_typeof tests */

static void test_mat_typeof(void)
{
    printf(C_CYAN "TEST: mat_typeof\n" C_RESET);

    matrix_t *Ad = matsq_new_d(2);
    matrix_t *Aqf = matsq_new_qf(2);
    matrix_t *Aqc = matsq_new_qc(2);
    matrix_t *Adv = matsq_new_dv(2);

    check_bool("mat_typeof(double)   = MAT_TYPE_DOUBLE", mat_typeof(Ad) == MAT_TYPE_DOUBLE);
    check_bool("mat_typeof(qfloat)   = MAT_TYPE_QFLOAT", mat_typeof(Aqf) == MAT_TYPE_QFLOAT);
    check_bool("mat_typeof(qcomplex) = MAT_TYPE_QCOMPLEX", mat_typeof(Aqc) == MAT_TYPE_QCOMPLEX);
    check_bool("mat_typeof(dval)     = MAT_TYPE_DVAL", Adv != NULL && mat_typeof(Adv) == MAT_TYPE_DVAL);

    matrix_t *Id = mat_create_identity_d(2);
    matrix_t *Iqf = mat_create_identity_qf(2);
    matrix_t *Iqc = mat_create_identity_qc(2);

    check_bool("mat_typeof(identity double)   = MAT_TYPE_DOUBLE", mat_typeof(Id) == MAT_TYPE_DOUBLE);
    check_bool("mat_typeof(identity qfloat)   = MAT_TYPE_QFLOAT", mat_typeof(Iqf) == MAT_TYPE_QFLOAT);
    check_bool("mat_typeof(identity qcomplex) = MAT_TYPE_QCOMPLEX", mat_typeof(Iqc) == MAT_TYPE_QCOMPLEX);

    /* matrix functions preserve element type */
    double dvals[4] = {0.5, 0.1, 0.1, 0.6};
    matrix_t *A_d = mat_create_d(2, 2, dvals);

    qfloat_t qfvals[4] = {
        qf_from_double(0.5), qf_from_double(0.1),
        qf_from_double(0.1), qf_from_double(0.6)};
    matrix_t *A_qf = mat_create_qf(2, 2, qfvals);

    matrix_t *E_d = mat_exp(A_d);
    matrix_t *E_qf = mat_exp(A_qf);

    check_bool("mat_exp(double) → double", E_d != NULL && mat_typeof(E_d) == MAT_TYPE_DOUBLE);
    check_bool("mat_exp(qfloat) → qfloat", E_qf != NULL && mat_typeof(E_qf) == MAT_TYPE_QFLOAT);

    mat_free(Ad);
    mat_free(Aqf);
    mat_free(Aqc);
    mat_free(Adv);
    mat_free(Id);
    mat_free(Iqf);
    mat_free(Iqc);
    mat_free(A_d);
    mat_free(A_qf);
    mat_free(E_d);
    mat_free(E_qf);
}

static void check_dval_expr_contains(const char *label,
                                     dval_t *dv,
                                     const char *needle)
{
    char *s = dv_to_string(dv, style_EXPRESSION);
    check_bool(label, s != NULL && strstr(s, needle) != NULL);
    free(s);
}

static void test_dval_matrix_functions(void)
{
    printf(C_CYAN "TEST: dval matrix functions\n" C_RESET);

    dval_t *x = dv_new_named_var_d(2.0, "x");
    dval_t *one = dv_new_const_d(1.0);

    {
        dval_t *diag_vals[4] = {x, DV_ZERO, DV_ZERO, one};
        matrix_t *A = mat_create_dv(2, 2, diag_vals);
        matrix_t *E = mat_exp(A);
        dval_t *e00 = NULL;
        dval_t *e11 = NULL;

        check_bool("mat_exp(dval diagonal) not NULL", E != NULL);
        print_mdv("A", A);
        if (E) {
            print_mdv("exp(A)", E);
            mat_get(E, 0, 0, &e00);
            mat_get(E, 1, 1, &e11);
            check_d("exp(dval diag)[0,0] = exp(2)", dv_eval_d(e00), exp(2.0), 1e-12);
            check_d("exp(dval diag)[1,1] = exp(1)", dv_eval_d(e11), exp(1.0), 1e-12);
            dv_set_val_d(x, 3.0);
            check_d("exp(dval diag)[0,0] tracks x", dv_eval_d(e00), exp(3.0), 1e-12);
        }

        mat_free(A);
        mat_free(E);
    }

    dv_set_val_d(x, 2.0);

    {
        dval_t *tri_vals[4] = {x, one, DV_ZERO, x};
        matrix_t *T = mat_create_dv(2, 2, tri_vals);
        matrix_t *E = mat_exp(T);
        dval_t *e00 = NULL;
        dval_t *e01 = NULL;
        dval_t *e11 = NULL;

        check_bool("mat_exp(dval Jordan block) not NULL", E != NULL);
        print_mdv("T", T);
        if (E) {
            print_mdv("exp(T)", E);
            mat_get(E, 0, 0, &e00);
            mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 1, &e11);
            check_d("exp([[x,1],[0,x]])[0,0] = exp(2)", dv_eval_d(e00), exp(2.0), 1e-12);
            check_d("exp([[x,1],[0,x]])[0,1] = exp(2)", dv_eval_d(e01), exp(2.0), 1e-12);
            check_d("exp([[x,1],[0,x]])[1,1] = exp(2)", dv_eval_d(e11), exp(2.0), 1e-12);
            dv_set_val_d(x, 3.0);
            check_d("Jordan exp tracks x on diagonal", dv_eval_d(e00), exp(3.0), 1e-12);
            check_d("Jordan exp tracks x on superdiag", dv_eval_d(e01), exp(3.0), 1e-12);
        }

        mat_free(T);
        mat_free(E);
    }

    {
        dval_t *dense_vals[4] = {x, one, one, x};
        matrix_t *A = mat_create_dv(2, 2, dense_vals);
        matrix_t *E = mat_exp(A);
        matrix_t *L = mat_log(A);
        matrix_t *S = mat_sin(A);
        matrix_t *Ai = mat_inverse(A);
        int rank = mat_rank(A);
        dval_t *v = NULL;

        dv_set_val_d(x, 2.0);
        print_mdv("A", A);
        check_bool("mat_exp(dval dense 2x2 diagonalizable) not NULL", E != NULL);
        check_bool("mat_log(dval dense 2x2 diagonalizable) not NULL", L != NULL);
        check_bool("mat_sin(dval dense 2x2 diagonalizable) not NULL", S != NULL);
        check_bool("mat_inverse(dval 2x2) now supported", Ai != NULL);
        if (Ai)
            print_mdv("A^{-1}", Ai);
        check_bool("mat_rank(dval 2x2) = 2", rank == 2);
        if (E) {
            print_mdv("exp(A)", E);
            mat_get(E, 0, 0, &v);
            check_d("exp(dval dense 2x2)[0,0]", dv_eval_d(v), exp(2.0) * cosh(1.0), 1e-12);
            check_dval_expr_contains("exp(dval dense 2x2)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(dval dense 2x2)[0,1]", dv_eval_d(v), exp(2.0) * sinh(1.0), 1e-12);
        }
        if (L) {
            print_mdv("log(A)", L);
            mat_get(L, 0, 0, &v);
            check_d("log(dval dense 2x2)[0,0]", dv_eval_d(v),
                    0.5 * (log(3.0) + log(1.0)), 1e-12);
            check_dval_expr_contains("log(dval dense 2x2)[0,0] stays symbolic", v, "log");
            mat_get(L, 0, 1, &v);
            check_d("log(dval dense 2x2)[0,1]", dv_eval_d(v),
                    0.5 * (log(3.0) - log(1.0)), 1e-12);
        }
        if (S) {
            print_mdv("sin(A)", S);
            mat_get(S, 0, 0, &v);
            check_d("sin(dval dense 2x2)[0,0]", dv_eval_d(v), sin(2.0) * cos(1.0), 1e-12);
            check_dval_expr_contains("sin(dval dense 2x2)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 1, &v);
            check_d("sin(dval dense 2x2)[0,1]", dv_eval_d(v), cos(2.0) * sin(1.0), 1e-12);
        }

        dv_set_val_d(x, 3.0);
        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dval dense 2x2)[0,0] tracks x", dv_eval_d(v), exp(3.0) * cosh(1.0), 1e-12);
        }
        if (L) {
            mat_get(L, 0, 1, &v);
            check_d("log(dval dense 2x2)[0,1] tracks x", dv_eval_d(v),
                    0.5 * (log(4.0) - log(2.0)), 1e-12);
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_d("sin(dval dense 2x2)[0,1] tracks x", dv_eval_d(v), cos(3.0) * sin(1.0), 1e-12);
        }

        mat_free(Ai);
        mat_free(A);
        mat_free(E);
        mat_free(L);
        mat_free(S);
    }

    dv_free(one);
    dv_free(x);
}

static void test_dval_matrix_functions_extended(void)
{
    printf(C_CYAN "TEST: dval matrix functions (extended symbolic coverage)\n" C_RESET);

    {
        dval_t *x = dv_new_named_var_d(0.2, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, DV_ZERO, DV_ZERO,
            one, x, DV_ZERO,
            DV_ZERO, one, x};
        matrix_t *T = mat_create_dv(3, 3, vals);
        matrix_t *R = NULL;
        matrix_t *G = NULL;
        matrix_t *W = NULL;
        dval_t *v = NULL;

        R = mat_erf(T);
        G = mat_gamma(T);
        W = mat_lambert_w0(T);

        check_bool("mat_erf(dval 3x3 lower Jordan) not NULL", R != NULL);
        check_bool("mat_gamma(dval 3x3 lower Jordan) not NULL", G != NULL);
        check_bool("mat_lambert_w0(dval 3x3 lower Jordan) not NULL", W != NULL);

        if (T)
            print_mdv("T", T);
        if (R)
            print_mdv("erf(T)", R);
        if (G)
            print_mdv("gamma(T)", G);
        if (W)
            print_mdv("lambert_w0(T)", W);

        if (R) {
            check_bool("erf(T) preserves lower-triangular structure",
                       mat_is_lower_triangular(R));
            mat_get(R, 0, 0, &v);
            check_dval_expr_contains("erf(T)[0,0] stays symbolic in x", v, "erf(x)");
            dv_set_val_d(x, 0.3);
            check_d("erf(T)[0,0] tracks x", dv_eval_d(v), erf(0.3), 1e-12);
        }

        if (G) {
            check_bool("gamma(T) preserves lower-triangular structure",
                       mat_is_lower_triangular(G));
            mat_get(G, 0, 0, &v);
            dv_set_val_d(x, 3.0);
            check_d("gamma(T)[0,0] tracks x", dv_eval_d(v), tgamma(3.0), 1e-12);
        }

        if (W) {
            check_bool("lambert_w0(T) preserves lower-triangular structure",
                       mat_is_lower_triangular(W));
            mat_get(W, 0, 0, &v);
            check_dval_expr_contains("lambert_w0(T)[0,0] stays symbolic in x", v, "lambert_w0");
            dv_set_val_d(x, 0.1);
            check_bool("lambert_w0(T)[0,0] numerically finite", isfinite(dv_eval_d(v)));
        }

        mat_free(T);
        mat_free(R);
        mat_free(G);
        mat_free(W);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(1.5, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *vals[9] = {
            x, DV_ZERO, DV_ZERO,
            one, x, DV_ZERO,
            DV_ZERO, one, x};
        matrix_t *T = mat_create_dv(3, 3, vals);
        matrix_t *E = mat_exp(T);
        dval_t *v = NULL;

        check_bool("mat_exp(dval 3x3 lower Jordan) not NULL", E != NULL);
        if (T)
            print_mdv("T", T);
        if (E)
            print_mdv("exp(T)", E);

        if (E) {
            check_bool("exp(lower Jordan) preserves lower-triangular structure",
                       mat_is_lower_triangular(E));
            mat_get(E, 0, 0, &v);
            check_d("exp(lower Jordan)[0,0] = exp(1.5)", dv_eval_d(v), exp(1.5), 1e-12);
            mat_get(E, 1, 0, &v);
            check_d("exp(lower Jordan)[1,0] = exp(1.5)", dv_eval_d(v), exp(1.5), 1e-12);
            mat_get(E, 2, 0, &v);
            check_d("exp(lower Jordan)[2,0] = exp(1.5)/2", dv_eval_d(v), 0.5 * exp(1.5), 1e-12);
            mat_get(E, 2, 0, &v);
            check_dval_expr_contains("exp(lower Jordan)[2,0] stays symbolic in x", v, "exp(x)");
            dv_set_val_d(x, 2.0);
            mat_get(E, 2, 0, &v);
            check_d("exp(lower Jordan)[2,0] tracks x", dv_eval_d(v), 0.5 * exp(2.0), 1e-12);
        }

        mat_free(T);
        mat_free(E);
        dv_free(x);
        dv_free(one);
    }

    {
        dval_t *x = dv_new_named_var_d(1.0, "x");
        dval_t *one = dv_new_const_d(1.0);
        dval_t *two = dv_new_const_d(2.0);
        dval_t *vals[9] = {
            x, one, DV_ZERO,
            one, two, one,
            DV_ZERO, one, x};
        matrix_t *A = mat_create_dv(3, 3, vals);
        matrix_t *Aqc = NULL;
        matrix_t *Eqc = NULL;
        matrix_t *Lqc = NULL;
        matrix_t *Sqc = NULL;
        matrix_t *Gqc = NULL;

        print_mdv("A", A);
        check_bool("mat_exp(dval dense 3x3) currently unsupported", mat_exp(A) == NULL);
        check_bool("mat_log(dval dense 3x3) currently unsupported", mat_log(A) == NULL);
        check_bool("mat_sin(dval dense 3x3) currently unsupported", mat_sin(A) == NULL);
        check_bool("mat_gamma(dval dense 3x3) currently unsupported", mat_gamma(A) == NULL);

        Aqc = mat_evaluate_qc(A);
        check_bool("mat_evaluate_qc(dval dense 3x3) not NULL", Aqc != NULL);

        if (Aqc) {
            Eqc = mat_exp(Aqc);
            Lqc = mat_log(Aqc);
            Sqc = mat_sin(Aqc);
            Gqc = mat_gamma(Aqc);
        }

        check_bool("manual qc exp(dval dense 3x3) not NULL", Eqc != NULL);
        check_bool("manual qc log(dval dense 3x3) not NULL", Lqc != NULL);
        check_bool("manual qc sin(dval dense 3x3) not NULL", Sqc != NULL);
        check_bool("manual qc gamma(dval dense 3x3) not NULL", Gqc != NULL);

        check_bool("manual qc exp(dval dense 3x3) -> MAT_TYPE_QCOMPLEX",
                   Eqc != NULL && mat_typeof(Eqc) == MAT_TYPE_QCOMPLEX);
        check_bool("manual qc log(dval dense 3x3) -> MAT_TYPE_QCOMPLEX",
                   Lqc != NULL && mat_typeof(Lqc) == MAT_TYPE_QCOMPLEX);
        check_bool("manual qc sin(dval dense 3x3) -> MAT_TYPE_QCOMPLEX",
                   Sqc != NULL && mat_typeof(Sqc) == MAT_TYPE_QCOMPLEX);
        check_bool("manual qc gamma(dval dense 3x3) -> MAT_TYPE_QCOMPLEX",
                   Gqc != NULL && mat_typeof(Gqc) == MAT_TYPE_QCOMPLEX);

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Lqc);
        mat_free(Sqc);
        mat_free(Gqc);
        mat_free(A);
        dv_free(x);
        dv_free(one);
        dv_free(two);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[0 x 0][x 0 x][0 x 0]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;
        double r;

        check_bool("dense dval cubic-linear 3x3 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense dval cubic-linear 3x3 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense dval cubic-linear 3x3 exp not NULL", E != NULL);
        check_bool("dense dval cubic-linear 3x3 sin not NULL", S != NULL);

        r = sqrt(8.0);
        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense cubic-linear 3x3)[0,0]",
                    dv_eval_d(v), 0.5 * (cosh(r) + 1.0), 1e-12);
            check_dval_expr_contains("exp(dense cubic-linear 3x3)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(dense cubic-linear 3x3)[0,1]",
                    dv_eval_d(v), sinh(r) / r * 2.0, 1e-12);
            mat_get(E, 0, 2, &v);
            check_d("exp(dense cubic-linear 3x3)[0,2]",
                    dv_eval_d(v), 0.5 * (cosh(r) - 1.0), 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(dense cubic-linear 3x3)[0,0]",
                    dv_eval_d(v), 0.0, 1e-12);
            mat_get(S, 0, 1, &v);
            check_d("sin(dense cubic-linear 3x3)[0,1]",
                    dv_eval_d(v), sin(r) / r * 2.0, 1e-12);
            check_dval_expr_contains("sin(dense cubic-linear 3x3)[0,1] stays symbolic", v, "sin");
            mat_get(S, 0, 2, &v);
            check_d("sin(dense cubic-linear 3x3)[0,2]",
                    dv_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("dense dval cubic-linear 3x3 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 3.0) == 0);
        }

        r = sqrt(18.0);
        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(dense cubic-linear 3x3)[0,1] tracks x",
                    dv_eval_d(v), sinh(r) / r * 3.0, 1e-12);
        }
        if (S) {
            mat_get(S, 1, 0, &v);
            check_d("sin(dense cubic-linear 3x3)[1,0] tracks x",
                    dv_eval_d(v), sin(r) / r * 3.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[0 x x][x 0 x][x x 0]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;

        check_bool("dense dval quadratic 3x3 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense dval quadratic 3x3 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense dval quadratic 3x3 exp not NULL", E != NULL);
        check_bool("dense dval quadratic 3x3 sin not NULL", S != NULL);

        if (A)
            print_mdv("A (dense quadratic 3x3)", A);
        if (E)
            print_mdv("exp(A)", E);
        if (S)
            print_mdv("sin(A)", S);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense quadratic 3x3)[0,0]",
                    dv_eval_d(v), (exp(4.0) + 2.0 * exp(-2.0)) / 3.0, 1e-12);
            check_dval_expr_contains("exp(dense quadratic 3x3)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(dense quadratic 3x3)[0,1]",
                    dv_eval_d(v), (exp(4.0) - exp(-2.0)) / 3.0, 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(dense quadratic 3x3)[0,0]",
                    dv_eval_d(v), (sin(4.0) - 2.0 * sin(2.0)) / 3.0, 1e-12);
            check_dval_expr_contains("sin(dense quadratic 3x3)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 1, &v);
            check_d("sin(dense quadratic 3x3)[0,1]",
                    dv_eval_d(v), (sin(4.0) + sin(2.0)) / 3.0, 1e-12);
        }

        if (bindings) {
            check_bool("dense dval quadratic 3x3 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 3.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(dense quadratic 3x3)[0,0] tracks x",
                    dv_eval_d(v), (exp(6.0) + 2.0 * exp(-3.0)) / 3.0, 1e-12);
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_d("sin(dense quadratic 3x3)[0,1] tracks x",
                    dv_eval_d(v), (sin(6.0) + sin(3.0)) / 3.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[x 1 1 1 1]"
             "[1 x 1 1 1]"
             "[1 1 x 1 1]"
             "[1 1 1 x 1]"
             "[1 1 1 1 x]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;

        check_bool("uniform dense dval 5x5 input not NULL", A != NULL);
        if (bindings) {
            check_bool("uniform dense dval 5x5 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("uniform dense dval 5x5 exp not NULL", E != NULL);
        check_bool("uniform dense dval 5x5 sin not NULL", S != NULL);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(uniform dense 5x5)[0,0]",
                    dv_eval_d(v), (4.0 * exp(1.0) + exp(6.0)) / 5.0, 1e-12);
            check_dval_expr_contains("exp(uniform dense 5x5)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(uniform dense 5x5)[0,1]",
                    dv_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
            mat_get(E, 3, 4, &v);
            check_d("exp(uniform dense 5x5)[3,4]",
                    dv_eval_d(v), (exp(6.0) - exp(1.0)) / 5.0, 1e-12);
        }

        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(uniform dense 5x5)[0,0]",
                    dv_eval_d(v), (4.0 * sin(1.0) + sin(6.0)) / 5.0, 1e-12);
            check_dval_expr_contains("sin(uniform dense 5x5)[0,0] stays symbolic", v, "sin");
            mat_get(S, 0, 2, &v);
            check_d("sin(uniform dense 5x5)[0,2]",
                    dv_eval_d(v), (sin(6.0) - sin(1.0)) / 5.0, 1e-12);
        }

        if (bindings) {
            check_bool("uniform dense dval 5x5 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 3.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(uniform dense 5x5)[0,1] tracks x",
                    dv_eval_d(v), (exp(7.0) - exp(2.0)) / 5.0, 1e-12);
        }
        if (S) {
            mat_get(S, 0, 0, &v);
            check_d("sin(uniform dense 5x5)[0,0] tracks x",
                    dv_eval_d(v), (4.0 * sin(2.0) + sin(7.0)) / 5.0, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "(7, x, 2, 1;"
             " 10, 2*x + 2, 4, 2;"
             " 15, 3*x, 8, 3;"
             " 20, 4*x, 8, 6)",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;
        double cexp;
        double csin;

        check_bool("rank-one perturbation dense dval 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("rank-one perturbation dense dval 4x4 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("rank-one perturbation dense dval 4x4 exp not NULL", E != NULL);
        check_bool("rank-one perturbation dense dval 4x4 sin not NULL", S != NULL);

        cexp = (exp(23.0) - exp(2.0)) / 21.0;
        csin = (sin(23.0) - sin(2.0)) / 21.0;

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(rank-one perturbation 4x4)[0,0]",
                    dv_eval_d(v), exp(2.0) + 5.0 * cexp, 1e-5);
            check_dval_expr_contains("exp(rank-one perturbation 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(rank-one perturbation 4x4)[0,1]",
                    dv_eval_d(v), 3.0 * cexp, 1e-12);
            mat_get(E, 3, 2, &v);
            check_d("exp(rank-one perturbation 4x4)[3,2]",
                    dv_eval_d(v), 8.0 * cexp, 1e-5);
        }

        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(rank-one perturbation 4x4)[1,1]",
                    dv_eval_d(v), sin(2.0) + 6.0 * csin, 1e-12);
            check_dval_expr_contains("sin(rank-one perturbation 4x4)[1,1] stays symbolic", v, "sin");
            mat_get(S, 2, 0, &v);
            check_d("sin(rank-one perturbation 4x4)[2,0]",
                    dv_eval_d(v), 15.0 * csin, 1e-12);
            mat_get(S, 0, 3, &v);
            check_d("sin(rank-one perturbation 4x4)[0,3]",
                    dv_eval_d(v), csin, 1e-12);
        }

        if (bindings) {
            check_bool("rank-one perturbation dense dval 4x4 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 4.0) == 0);
        }

        cexp = (exp(25.0) - exp(2.0)) / 23.0;
        csin = (sin(25.0) - sin(2.0)) / 23.0;

        if (E) {
            mat_get(E, 0, 1, &v);
            check_d("exp(rank-one perturbation 4x4)[0,1] tracks x",
                    dv_eval_d(v), 4.0 * cexp, 1e-5);
        }
        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(rank-one perturbation 4x4)[1,1] tracks x",
                    dv_eval_d(v), sin(2.0) + 8.0 * csin, 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[0 x 0 0]"
             "[x 0 x 0]"
             "[0 x 0 x]"
             "[0 0 x 0]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        matrix_t *Aqc = NULL;
        matrix_t *Eqc = NULL;
        matrix_t *Eqc_expected = NULL;
        matrix_t *Sqc = NULL;
        matrix_t *Sqc_expected = NULL;
        dval_t *v = NULL;

        check_bool("dense dval biquadratic quartic 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("dense dval biquadratic quartic 4x4 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("dense dval biquadratic quartic 4x4 exp not NULL", E != NULL);
        check_bool("dense dval biquadratic quartic 4x4 sin not NULL", S != NULL);

        if (E && S) {
            Aqc = mat_evaluate_qc(A);
            Eqc = mat_evaluate_qc(E);
            Sqc = mat_evaluate_qc(S);
            Eqc_expected = Aqc ? mat_exp(Aqc) : NULL;
            Sqc_expected = Aqc ? mat_sin(Aqc) : NULL;

            check_bool("dense dval biquadratic quartic 4x4 evaluated exp not NULL", Eqc != NULL);
            check_bool("dense dval biquadratic quartic 4x4 evaluated sin not NULL", Sqc != NULL);
            check_bool("dense dval biquadratic quartic 4x4 numeric exp baseline not NULL",
                       Eqc_expected != NULL);
            check_bool("dense dval biquadratic quartic 4x4 numeric sin baseline not NULL",
                       Sqc_expected != NULL);
            if (Eqc && Eqc_expected)
                check_mat_qc("exp(biquadratic quartic 4x4) matches numeric snapshot",
                             Eqc, Eqc_expected, 1e-17);
            if (Sqc && Sqc_expected)
                check_mat_qc("sin(biquadratic quartic 4x4) matches numeric snapshot",
                             Sqc, Sqc_expected, 1e-20);
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_dval_expr_contains("exp(biquadratic quartic 4x4)[0,0] stays symbolic", v, "exp");
        }
        if (S) {
            mat_get(S, 0, 1, &v);
            check_dval_expr_contains("sin(biquadratic quartic 4x4)[0,1] stays symbolic", v, "sin");
        }

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Eqc_expected);
        mat_free(Sqc);
        mat_free(Sqc_expected);
        Aqc = NULL;
        Eqc = NULL;
        Eqc_expected = NULL;
        Sqc = NULL;
        Sqc_expected = NULL;

        if (bindings) {
            check_bool("dense dval biquadratic quartic 4x4 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 3.0) == 0);
        }

        if (E && S) {
            Aqc = mat_evaluate_qc(A);
            Eqc = mat_evaluate_qc(E);
            Sqc = mat_evaluate_qc(S);
            Eqc_expected = Aqc ? mat_exp(Aqc) : NULL;
            Sqc_expected = Aqc ? mat_sin(Aqc) : NULL;

            if (Eqc && Eqc_expected)
                check_mat_qc("exp(biquadratic quartic 4x4) tracks x",
                             Eqc, Eqc_expected, 1e-17);
            if (Sqc && Sqc_expected)
                check_mat_qc("sin(biquadratic quartic 4x4) tracks x",
                             Sqc, Sqc_expected, 1e-19);
        }

        mat_free(Aqc);
        mat_free(Eqc);
        mat_free(Eqc_expected);
        mat_free(Sqc);
        mat_free(Sqc_expected);
        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[x 1 0 0][1 x 0 0][0 0 y 1][0 0 1 y]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;

        check_bool("block-diagonal dense dval 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("block-diagonal dense dval 4x4 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
            check_bool("block-diagonal dense dval 4x4 set y",
                       mat_binding_set_d(bindings, nbindings, "y", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("block-diagonal dense dval 4x4 exp not NULL", E != NULL);
        check_bool("block-diagonal dense dval 4x4 sin not NULL", S != NULL);

        if (A)
            print_mdv("A (block-diagonal dense 4x4)", A);
        if (E)
            print_mdv("exp(A)", E);
        if (S)
            print_mdv("sin(A)", S);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(block-diagonal 4x4)[0,0]",
                    dv_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
            check_dval_expr_contains("exp(block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 1, &v);
            check_d("exp(block-diagonal 4x4)[0,1]",
                    dv_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
            mat_get(E, 0, 2, &v);
            check_d("exp(block-diagonal 4x4)[0,2] stays zero",
                    dv_eval_d(v), 0.0, 1e-12);
            mat_get(E, 2, 2, &v);
            check_d("exp(block-diagonal 4x4)[2,2]",
                    dv_eval_d(v), 0.5 * (exp(4.0) + exp(2.0)), 1e-12);
        }

        if (S) {
            mat_get(S, 2, 2, &v);
            check_d("sin(block-diagonal 4x4)[2,2]",
                    dv_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
            check_dval_expr_contains("sin(block-diagonal 4x4)[2,2] stays symbolic", v, "sin");
            mat_get(S, 2, 3, &v);
            check_d("sin(block-diagonal 4x4)[2,3]",
                    dv_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
            mat_get(S, 1, 3, &v);
            check_d("sin(block-diagonal 4x4)[1,3] stays zero",
                    dv_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("block-diagonal dense dval 4x4 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 4.0) == 0);
            check_bool("block-diagonal dense dval 4x4 update y",
                       mat_binding_set_d(bindings, nbindings, "y", 5.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(block-diagonal 4x4)[0,0] tracks x",
                    dv_eval_d(v), 0.5 * (exp(5.0) + exp(3.0)), 1e-12);
        }
        if (S) {
            mat_get(S, 2, 3, &v);
            check_d("sin(block-diagonal 4x4)[2,3] tracks y",
                    dv_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)), 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

    {
        binding_t *bindings = NULL;
        size_t nbindings = 0;
        matrix_t *A = mat_from_string(
            "[[x 0 1 0][0 y 0 1][1 0 x 0][0 1 0 y]]",
            &bindings, &nbindings);
        matrix_t *E = NULL;
        matrix_t *S = NULL;
        dval_t *v = NULL;

        check_bool("permuted block-diagonal dense dval 4x4 input not NULL", A != NULL);
        if (bindings) {
            check_bool("permuted block-diagonal dense dval 4x4 set x",
                       mat_binding_set_d(bindings, nbindings, "x", 2.0) == 0);
            check_bool("permuted block-diagonal dense dval 4x4 set y",
                       mat_binding_set_d(bindings, nbindings, "y", 3.0) == 0);
        }

        E = mat_exp(A);
        S = mat_sin(A);
        check_bool("permuted block-diagonal dense dval 4x4 exp not NULL", E != NULL);
        check_bool("permuted block-diagonal dense dval 4x4 sin not NULL", S != NULL);

        if (E) {
            mat_get(E, 0, 0, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,0]",
                    dv_eval_d(v), 0.5 * (exp(3.0) + exp(1.0)), 1e-12);
            check_dval_expr_contains("exp(permuted block-diagonal 4x4)[0,0] stays symbolic", v, "exp");
            mat_get(E, 0, 2, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,2]",
                    dv_eval_d(v), 0.5 * (exp(3.0) - exp(1.0)), 1e-12);
            mat_get(E, 0, 1, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,1] stays zero",
                    dv_eval_d(v), 0.0, 1e-12);
            mat_get(E, 1, 3, &v);
            check_d("exp(permuted block-diagonal 4x4)[1,3]",
                    dv_eval_d(v), 0.5 * (exp(4.0) - exp(2.0)), 1e-12);
        }

        if (S) {
            mat_get(S, 1, 1, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,1]",
                    dv_eval_d(v), 0.5 * (sin(4.0) + sin(2.0)), 1e-12);
            check_dval_expr_contains("sin(permuted block-diagonal 4x4)[1,1] stays symbolic", v, "sin");
            mat_get(S, 1, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,3]",
                    dv_eval_d(v), 0.5 * (sin(4.0) - sin(2.0)), 1e-12);
            mat_get(S, 2, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[2,3] stays zero",
                    dv_eval_d(v), 0.0, 1e-12);
        }

        if (bindings) {
            check_bool("permuted block-diagonal dense dval 4x4 update x",
                       mat_binding_set_d(bindings, nbindings, "x", 4.0) == 0);
            check_bool("permuted block-diagonal dense dval 4x4 update y",
                       mat_binding_set_d(bindings, nbindings, "y", 5.0) == 0);
        }

        if (E) {
            mat_get(E, 0, 2, &v);
            check_d("exp(permuted block-diagonal 4x4)[0,2] tracks x",
                    dv_eval_d(v), 0.5 * (exp(5.0) - exp(3.0)), 1e-12);
        }
        if (S) {
            mat_get(S, 1, 3, &v);
            check_d("sin(permuted block-diagonal 4x4)[1,3] tracks y",
                    dv_eval_d(v), 0.5 * (sin(6.0) - sin(4.0)), 1e-12);
        }

        mat_free(A);
        mat_free(E);
        mat_free(S);
        free(bindings);
    }

}

/* ------------------------------------------------------------------ 3×3 matrix function tests */

/*
 * Symmetric positive-definite 3×3 input exercises the full Schur-roundtrip
 * path on a genuinely dense matrix, rather than the simpler diagonal,
 * nilpotent, or 2×2 cases covered elsewhere in this file.
 */

void run_matrix_function_tests(void)
{
    RUN_TEST(test_mat_neg_convenience, NULL);
    RUN_TEST(test_eigen_d, NULL);
    RUN_TEST(test_eigen_qf, NULL);
    RUN_TEST(test_eigen_qc, NULL);
    RUN_TEST(test_eigen_dval, NULL);
    RUN_TEST(test_eigenspace_dval, NULL);
    RUN_TEST(test_generalized_eigenspace_dval, NULL);
    RUN_TEST(test_jordan_chain_dval, NULL);
    RUN_TEST(test_jordan_profile_dval, NULL);
    RUN_TEST(test_mat_exp_d, NULL);
    RUN_TEST(test_mat_exp_qf, NULL);
    RUN_TEST(test_mat_exp_qc, NULL);
    RUN_TEST(test_mat_exp_singular, NULL);
    RUN_TEST(test_matrix_function_structure_preservation, NULL);
    RUN_TEST(test_mat_fun_singular_entire_d, NULL);
    RUN_TEST(test_mat_exp_null_safety, NULL);
    RUN_TEST(test_mat_sin_d, NULL);
    RUN_TEST(test_mat_sin_qf, NULL);
    RUN_TEST(test_mat_sin_qc, NULL);
    RUN_TEST(test_mat_sin_null_safety, NULL);
    RUN_TEST(test_mat_cos_d, NULL);
    RUN_TEST(test_mat_cos_qf, NULL);
    RUN_TEST(test_mat_cos_qc, NULL);
    RUN_TEST(test_mat_tan_d, NULL);
    RUN_TEST(test_mat_sinh_d, NULL);
    RUN_TEST(test_mat_cosh_d, NULL);
    RUN_TEST(test_mat_tanh_d, NULL);
    RUN_TEST(test_mat_trig_null_safety, NULL);
    RUN_TEST(test_mat_sqrt_d, NULL);
    RUN_TEST(test_mat_sqrt_qf, NULL);
    RUN_TEST(test_mat_log_d, NULL);
    RUN_TEST(test_mat_asin_d, NULL);
    RUN_TEST(test_mat_acos_d, NULL);
    RUN_TEST(test_mat_atan_d, NULL);
    RUN_TEST(test_mat_asinh_d, NULL);
    RUN_TEST(test_mat_acosh_d, NULL);
    RUN_TEST(test_mat_atanh_d, NULL);
    RUN_TEST(test_mat_inv_trig_null_safety, NULL);
    RUN_TEST(test_eigen_general_d, NULL);
    RUN_TEST(test_eigen_general_qf, NULL);
    RUN_TEST(test_mat_nilpotent_d, NULL);
    RUN_TEST(test_mat_algebraic_ids_d, NULL);
    RUN_TEST(test_mat_roundtrips_d, NULL);
    RUN_TEST(test_mat_pow_int_d, NULL);
    RUN_TEST(test_mat_pow_d, NULL);
    RUN_TEST(test_mat_erf_d, NULL);
    RUN_TEST(test_mat_erfc_d, NULL);
    RUN_TEST(test_mat_special_unary_extensions, NULL);
    RUN_TEST(test_mat_special_unary_square_extensions, NULL);
    RUN_TEST(test_mat_typeof, NULL);
    RUN_TEST(test_dval_matrix_functions, NULL);
    RUN_TEST(test_dval_matrix_functions_extended, NULL);
    RUN_TEST(test_mat_simplify_symbolic_helper, NULL);
}

void run_matrix_readme_example(void)
{
    RUN_TEST(test_readme_example, NULL);
    RUN_TEST(test_readme_string_quantum_example, NULL);
}
