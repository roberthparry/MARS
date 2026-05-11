#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "matrix_internal.h"
#include "number.h"
#include "qfloat.h"
#include "qcomplex.h"
#include "matrix.h"
#include "internal/dval_internal.h"
#include "internal/number_internal.h"

void mat_qr_factor_free(mat_qr_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->Q);
    mat_free(out->R);
    out->Q = out->R = NULL;
}

int mat_qr_factor(const matrix_t *A, mat_qr_factor_t *out)
{
    size_t m, n, kdim;
    size_t z_count, q_count, r_count;
    qcomplex_t *Z = NULL, *Qq = NULL, *Rq = NULL;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    m = A->rows;
    n = A->cols;
    kdim = (m < n) ? m : n;
    z_count = m * n;
    q_count = m * kdim;
    r_count = kdim * n;

    Z = calloc(z_count ? z_count : 1u, sizeof(qcomplex_t));
    Qq = calloc(q_count ? q_count : 1u, sizeof(qcomplex_t));
    Rq = calloc(r_count ? r_count : 1u, sizeof(qcomplex_t));
    if (!Z || !Qq || !Rq) {
        free(Z);
        free(Qq);
        free(Rq);
        return -3;
    }

    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < n; ++j) {
            unsigned char raw[64];

            mat_get(A, i, j, raw);
            A->elem->to_qc(&Z[i * n + j], raw);
        }
    }

    for (size_t j = 0; j < n; j++) {
        qcomplex_t *v = malloc(m * sizeof(qcomplex_t));
        size_t imax = (j < kdim) ? j : kdim;
        if (!v) {
            free(Z);
            free(Qq);
            free(Rq);
            return -3;
        }

        for (size_t r = 0; r < m; r++)
            v[r] = Z[r * n + j];

        for (size_t i = 0; i < imax; i++) {
            qcomplex_t rij = QC_ZERO;
            for (size_t r = 0; r < m; r++) {
                qcomplex_t qri = Qq[r * kdim + i];
                rij = qc_add(rij, qc_mul(qc_conj(qri), v[r]));
            }
            Rq[i * n + j] = rij;
            for (size_t r = 0; r < m; r++) {
                qcomplex_t qri = Qq[r * kdim + i];
                v[r] = qc_sub(v[r], qc_mul(qri, rij));
            }
        }

        if (j < kdim) {
            qfloat_t norm2 = QF_ZERO;
            qcomplex_t rjj;
            for (size_t r = 0; r < m; r++)
                norm2 = qf_add(norm2, qf_add(qf_mul(qc_real(v[r]), qc_real(v[r])),
                                             qf_mul(qc_imag(v[r]), qc_imag(v[r]))));

            if (qf_to_double(norm2) < 1e-300) {
                rjj = QC_ZERO;
                Rq[j * n + j] = rjj;
                for (size_t r = 0; r < m; r++) {
                    Qq[r * kdim + j] = QC_ZERO;
                }
            } else {
                qfloat_t norm = qf_sqrt(norm2);
                qfloat_t inv = qf_div(QF_ONE, norm);
                rjj = qc_make(norm, QF_ZERO);
                Rq[j * n + j] = rjj;
                for (size_t r = 0; r < m; r++) {
                    Qq[r * kdim + j] = qc_make(qf_mul(inv, qc_real(v[r])),
                                               qf_mul(inv, qc_imag(v[r])));
                }
            }
        }

        free(v);
    }

    out->Q = mat_create_dense_with_elem(m, kdim, &number_elem);
    out->R = mat_create_upper_triangular_with_elem(kdim, n, &number_elem);
    if (out->Q && out->R) {
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < kdim; ++j) {
                number_t value = num_create_from_qcomplex(Qq[i * kdim + j]);
                mat_set(out->Q, i, j, &value);
                num_destroy(&value);
            }
        }
        for (size_t i = 0; i < kdim; ++i) {
            for (size_t j = i; j < n; ++j) {
                number_t value = num_create_from_qcomplex(Rq[i * n + j]);
                mat_set(out->R, i, j, &value);
                num_destroy(&value);
            }
        }
    }
    free(Z);
    free(Qq);
    free(Rq);

    if (!out->Q || !out->R) {
        mat_qr_factor_free(out);
        return -3;
    }

    return 0;
}

void mat_cholesky_free(mat_cholesky_t *out)
{
    if (!out)
        return;
    mat_free(out->L);
    out->L = NULL;
}

int mat_cholesky(const matrix_t *A, mat_cholesky_t *out)
{
    const struct store_vtable *lower_store;
    qcomplex_t *Z = NULL, *Lq = NULL;
    size_t n, count;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;
    if (A->rows != A->cols)
        return -2;

    lower_store = mat_sparse_factor_store(A, &lower_triangular_store);
    n = A->rows;
    count = n * n;
    Z = calloc(count ? count : 1u, sizeof(qcomplex_t));
    Lq = calloc(count ? count : 1u, sizeof(qcomplex_t));
    if (!Z || !Lq) {
        free(Z);
        free(Lq);
        return -3;
    }

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            unsigned char raw[64];

            mat_get(A, i, j, raw);
            A->elem->to_qc(&Z[i * n + j], raw);
        }
    }

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j <= i; j++) {
            qcomplex_t sum, aij;
            aij = Z[i * n + j];
            sum = aij;

            for (size_t k = 0; k < j; k++) {
                qcomplex_t lik = Lq[i * n + k];
                qcomplex_t ljk = Lq[j * n + k];
                sum = qc_sub(sum, qc_mul(lik, qc_conj(ljk)));
            }

            if (i == j) {
                double imag_abs = fabs(qf_to_double(qc_imag(sum)));
                double real_val = qf_to_double(qc_real(sum));
                if (imag_abs > 1e-12 || real_val <= 0.0) {
                    free(Z);
                    free(Lq);
                    return -4;
                }
                Lq[i * n + j] = qc_make(qf_sqrt(qc_real(sum)), QF_ZERO);
            } else {
                qcomplex_t ljj = Lq[j * n + j];
                if (qf_to_double(qc_abs(ljj)) < 1e-300) {
                    free(Z);
                    free(Lq);
                    return -4;
                }
                Lq[i * n + j] = qc_div(sum, qc_conj(ljj));
            }
        }

    }

    out->L = mat_create_lower_triangular_with_elem(n, n, &number_elem);
    if (out->L) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j <= i; ++j) {
                number_t value = num_create_from_qcomplex(Lq[i * n + j]);
                mat_set(out->L, i, j, &value);
                num_destroy(&value);
            }
        }
        if (lower_store != &lower_triangular_store) {
            matrix_t *converted = mat_copy_with_store(out->L, lower_store);
            mat_free(out->L);
            out->L = converted;
        }
    }
    free(Z);
    free(Lq);

    if (!out->L)
        return -3;

    return 0;
}

void mat_svd_factor_free(mat_svd_factor_t *out)
{
    if (!out)
        return;
    mat_free(out->U);
    mat_free(out->S);
    mat_free(out->V);
    out->U = out->S = out->V = NULL;
}

static double mat_singular_tolerance(const matrix_t *A, double sigma_max)
{
    qfloat_t sigma_qf = qf_from_double(sigma_max);
    qfloat_t dim_qf = qf_from_double((double)((A->rows > A->cols) ? A->rows : A->cols));
    qfloat_t tol_qf = qf_mul(qf_mul(sigma_qf, dim_qf), A->elem->relative_epsilon);
    qfloat_t algo_floor = qf_mul(qf_from_double(1.0e4),
                                 qf_from_double(2.2204460492503131e-16));

    if (qf_lt(tol_qf, algo_floor))
        tol_qf = algo_floor;

    return qf_to_double(tol_qf);
}

static qcomplex_t *mat_load_qcomplex_array(const matrix_t *A);
static matrix_t *mat_qcomplex_array_to_number_dense(size_t rows, size_t cols,
                                                    const qcomplex_t *data);
static matrix_t *mat_qcomplex_array_to_number_diagonal(size_t n,
                                                       const qcomplex_t *data);
static qcomplex_t *mat_qcomplex_array_hermitian(const qcomplex_t *A,
                                                size_t rows, size_t cols);
static qcomplex_t *mat_qcomplex_array_mul(const qcomplex_t *A, size_t a_rows,
                                          size_t a_cols, const qcomplex_t *B,
                                          size_t b_cols);

static qcomplex_t num_as_qcomplex(number_t value)
{
    number_t real = num_real_part(value);
    number_t imag = num_imag_part(value);
    qcomplex_t out = qc_make(num_to_qfloat(real), num_to_qfloat(imag));

    num_destroy(&imag);
    num_destroy(&real);
    return out;
}

static int mat_norm_via_svd(const matrix_t *A, qfloat_t *out)
{
    mat_svd_factor_t svd = {0};
    size_t kdim;
    qfloat_t best = QF_ZERO;

    if (!A || !out)
        return -1;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        sig = qf_abs(sig);
        if (qf_gt(sig, best))
            best = sig;
    }

    *out = best;
    mat_svd_factor_free(&svd);
    return 0;
}

int mat_svd_factor(const matrix_t *A, mat_svd_factor_t *out)
{
    size_t m, n, kdim;
    qcomplex_t *Z = NULL, *ZH = NULL, *Gramq = NULL, *LeftQ = NULL, *RightQ = NULL, *Sq = NULL;
    matrix_t *Gram = NULL;
    matrix_t *EigVecs = NULL;
    number_t *evals = NULL;
    qfloat_t *sigma = NULL;
    size_t *order = NULL;
    int rc;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    m = A->rows;
    n = A->cols;
    kdim = (m < n) ? m : n;

    Z = mat_load_qcomplex_array(A);
    if (!Z)
        return -3;

    evals = calloc(kdim ? kdim : 1, sizeof(number_t));
    sigma = calloc(kdim ? kdim : 1, sizeof(qfloat_t));
    order = calloc(kdim ? kdim : 1, sizeof(size_t));
    if (!evals || !sigma || !order) {
        rc = -3;
        goto fail;
    }
    for (size_t i = 0; i < (kdim ? kdim : 1); ++i)
        evals[i] = num_new();

    if (m >= n) {
        ZH = mat_qcomplex_array_hermitian(Z, m, n);
        Gramq = ZH ? mat_qcomplex_array_mul(ZH, n, m, Z, n) : NULL;
        Gram = Gramq ? mat_qcomplex_array_to_number_dense(n, n, Gramq) : NULL;
        if (!ZH || !Gramq || !Gram) {
            rc = -3;
            goto fail;
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < n; i++) {
            number_t re_num = num_real_part(evals[i]);
            double re = num_to_double(re_num);
            order[i] = i;
            sigma[i] = (re > 0.0) ? qf_sqrt(num_to_qfloat(re_num)) : QF_ZERO;
            num_destroy(&re_num);
        }

        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++)
                if (qf_to_double(sigma[order[j]]) > qf_to_double(sigma[order[i]])) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        size_t right_count = n * kdim;
        size_t left_count = m * kdim;
        size_t sq_count = kdim * kdim;
        RightQ = calloc(right_count ? right_count : 1u, sizeof(qcomplex_t));
        LeftQ = calloc(left_count ? left_count : 1u, sizeof(qcomplex_t));
        Sq = calloc(sq_count ? sq_count : 1u, sizeof(qcomplex_t));
        if (!RightQ || !LeftQ || !Sq) {
            rc = -3;
            goto fail;
        }

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            qcomplex_t sigqc = qc_make(sigma[idx], QF_ZERO);
            qcomplex_t invsig = qf_to_double(sigma[idx]) > 1e-300
                              ? qc_make(qf_div(QF_ONE, sigma[idx]), QF_ZERO)
                              : QC_ZERO;

            for (size_t r = 0; r < n; r++) {
                number_t v_num = mat_get_num(EigVecs, r, idx);
                RightQ[r * kdim + j] = num_as_qcomplex(v_num);
                num_destroy(&v_num);
            }

            Sq[j * kdim + j] = sigqc;

            for (size_t r = 0; r < m; r++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t c = 0; c < n; c++) {
                    sum = qc_add(sum, qc_mul(Z[r * n + c], RightQ[c * kdim + j]));
                }
                if (qf_to_double(sigma[idx]) > 1e-300)
                    sum = qc_mul(sum, invsig);
                else
                    sum = QC_ZERO;
                LeftQ[r * kdim + j] = sum;
            }
        }
    } else {
        ZH = mat_qcomplex_array_hermitian(Z, m, n);
        Gramq = ZH ? mat_qcomplex_array_mul(Z, m, n, ZH, m) : NULL;
        Gram = Gramq ? mat_qcomplex_array_to_number_dense(m, m, Gramq) : NULL;
        if (!ZH || !Gramq || !Gram) {
            rc = -3;
            goto fail;
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < m; i++) {
            number_t re_num = num_real_part(evals[i]);
            double re = num_to_double(re_num);
            order[i] = i;
            sigma[i] = (re > 0.0) ? qf_sqrt(num_to_qfloat(re_num)) : QF_ZERO;
            num_destroy(&re_num);
        }

        for (size_t i = 0; i < m; i++)
            for (size_t j = i + 1; j < m; j++)
                if (qf_to_double(sigma[order[j]]) > qf_to_double(sigma[order[i]])) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        size_t left_count = m * kdim;
        size_t right_count = n * kdim;
        size_t sq_count = kdim * kdim;
        LeftQ = calloc(left_count ? left_count : 1u, sizeof(qcomplex_t));
        RightQ = calloc(right_count ? right_count : 1u, sizeof(qcomplex_t));
        Sq = calloc(sq_count ? sq_count : 1u, sizeof(qcomplex_t));
        if (!RightQ || !LeftQ || !Sq) {
            rc = -3;
            goto fail;
        }

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            qcomplex_t sigqc = qc_make(sigma[idx], QF_ZERO);
            qcomplex_t invsig = qf_to_double(sigma[idx]) > 1e-300
                              ? qc_make(qf_div(QF_ONE, sigma[idx]), QF_ZERO)
                              : QC_ZERO;

            for (size_t r = 0; r < m; r++) {
                number_t u_num = mat_get_num(EigVecs, r, idx);
                LeftQ[r * kdim + j] = num_as_qcomplex(u_num);
                num_destroy(&u_num);
            }

            Sq[j * kdim + j] = sigqc;

            for (size_t r = 0; r < n; r++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t c = 0; c < m; c++)
                    sum = qc_add(sum, qc_mul(ZH[r * m + c], LeftQ[c * kdim + j]));
                if (qf_to_double(sigma[idx]) > 1e-300)
                    sum = qc_mul(sum, invsig);
                else
                    sum = QC_ZERO;
                RightQ[r * kdim + j] = sum;
            }
        }
    }

    out->U = mat_qcomplex_array_to_number_dense(m, kdim, LeftQ);
    out->S = mat_qcomplex_array_to_number_diagonal(kdim, Sq);
    out->V = mat_qcomplex_array_to_number_dense(n, kdim, RightQ);
    if (!out->U || !out->S || !out->V) {
        rc = -3;
        goto fail;
    }

    rc = 0;

fail:
    free(Z);
    free(ZH);
    free(Gramq);
    mat_free(Gram);
    mat_free(EigVecs);
    free(LeftQ);
    free(RightQ);
    free(Sq);
    if (evals) {
        for (size_t i = 0; i < (kdim ? kdim : 1); i++)
            num_destroy(&evals[i]);
    }
    free(evals);
    free(sigma);
    free(order);
    if (rc != 0)
        mat_svd_factor_free(out);
    return rc;
}

typedef int (*mat_norm_function_t)(const matrix_t *A, qfloat_t *out);

static int mat_norm_one(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e;

    e = A->elem;

    qfloat_t best = QF_ZERO;
    for (size_t j = 0; j < A->cols; j++) {
        qfloat_t sum = QF_ZERO;
        for (size_t i = 0; i < A->rows; i++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sum = qf_add(sum, mag);
        }
        if (qf_gt(sum, best))
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_infinity(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e = A->elem;
    qfloat_t best = QF_ZERO;

    for (size_t i = 0; i < A->rows; i++) {
        qfloat_t sum = QF_ZERO;
        for (size_t j = 0; j < A->cols; j++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sum = qf_add(sum, mag);
        }
        if (qf_gt(sum, best))
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_frobenius(const matrix_t *A, qfloat_t *out)
{
    const struct elem_vtable *e = A->elem;
    qfloat_t sumsq = QF_ZERO;

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            unsigned char raw[64];
            qfloat_t mag;
            mat_get(A, i, j, raw);
            e->abs_qf(&mag, raw);
            sumsq = qf_add(sumsq, qf_mul(mag, mag));
        }
    }
    *out = qf_sqrt(sumsq);
    return 0;
}

static const mat_norm_function_t mat_norm_functions[] = {
    [MAT_NORM_1] = mat_norm_one,
    [MAT_NORM_INF] = mat_norm_infinity,
    [MAT_NORM_FRO] = mat_norm_frobenius,
    [MAT_NORM_2] = mat_norm_via_svd
};

static mat_norm_function_t mat_norm_function_for(mat_norm_type_t type)
{
    size_t index = (size_t)type;

    if (index >= sizeof(mat_norm_functions) / sizeof(mat_norm_functions[0]))
        return NULL;

    return mat_norm_functions[index];
}

int mat_norm(const matrix_t *A, mat_norm_type_t type, qfloat_t *out)
{
    mat_norm_function_t fun;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    fun = mat_norm_function_for(type);
    if (!fun)
        return -2;

    return fun(A, out);
}

int mat_condition_number(const matrix_t *A, mat_norm_type_t type, qfloat_t *out)
{
    int rank;
    size_t kdim;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    rank = mat_rank(A);
    if (rank < 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    if ((size_t)rank < kdim) {
        *out = QF_INF;
        return 0;
    }

    if (mat_norm_function_for(type) == mat_norm_via_svd) {
        mat_svd_factor_t svd = {0};
        qfloat_t sigma_max = QF_ZERO;
        qfloat_t sigma_min = QF_INF;
        if (mat_svd_factor(A, &svd) != 0)
            return -2;
        for (size_t i = 0; i < kdim; i++) {
            unsigned char raw[64];
            qfloat_t sig;
            mat_get(svd.S, i, i, raw);
            svd.S->elem->to_qf(&sig, raw);
            sig = qf_abs(sig);
            if (qf_gt(sig, sigma_max))
                sigma_max = sig;
            if (qf_lt(sig, sigma_min))
                sigma_min = sig;
        }
        *out = qf_div(sigma_max, sigma_min);
        mat_svd_factor_free(&svd);
        return 0;
    }

    qfloat_t na, ni;
    matrix_t *Aw = NULL;
    matrix_t *Ai = NULL;
    int rc_a, rc_i;

    Aw = mat_convert_dense(A, &qcomplex_elem);
    if (!Aw)
        return -2;

    if (Aw->rows == Aw->cols) {
        matrix_t *I = mat_create_identity_with_elem(Aw->rows, Aw->elem);
        if (!I)
            goto fail_non2;
        Ai = mat_solve(Aw, I);
        mat_free(I);
    } else {
        Ai = mat_pseudoinverse(Aw);
    }

    if (!Ai)
        goto fail_non2;

    rc_a = mat_norm(Aw, type, &na);
    rc_i = mat_norm(Ai, type, &ni);
    mat_free(Ai);
    mat_free(Aw);
    if (rc_a != 0 || rc_i != 0)
        return -2;
    *out = qf_mul(na, ni);
    return 0;

fail_non2:
    mat_free(Aw);
    mat_free(Ai);
    return -2;
}

int mat_rank(const matrix_t *A)
{
    mat_svd_factor_t svd = {0};
    size_t kdim;
    double sigma_max = 0.0, tol;
    int rank = 0;

    if (!A)
        return -1;
    if (A->elem == &dval_elem)
        return mat_rank_dval_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > sigma_max)
            sigma_max = d;
    }

    tol = mat_singular_tolerance(A, sigma_max);
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > tol)
            rank++;
    }

    mat_svd_factor_free(&svd);
    return rank;
}

matrix_t *mat_pseudoinverse(const matrix_t *A)
{
    mat_svd_factor_t svd = {0};
    qcomplex_t *Uq = NULL, *Vq = NULL, *UH = NULL, *Sp = NULL;
    qcomplex_t *VSp = NULL, *Pinvq = NULL;
    matrix_t *Pinv = NULL;
    size_t kdim;
    double sigma_max = 0.0, tol;

    if (!A)
        return NULL;
    if (A->elem == &dval_elem)
        return mat_pseudoinverse_dval_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    if (mat_svd_factor(A, &svd) != 0)
        return NULL;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > sigma_max)
            sigma_max = d;
    }
    tol = mat_singular_tolerance(A, sigma_max);

    Uq = mat_load_qcomplex_array(svd.U);
    Vq = mat_load_qcomplex_array(svd.V);
    {
        size_t sp_count = kdim * kdim;
        Sp = calloc(sp_count ? sp_count : 1u, sizeof(qcomplex_t));
    }
    if (!Uq || !Vq || !Sp)
        goto fail;

    for (size_t i = 0; i < kdim; i++) {
        unsigned char raw[64];
        qfloat_t sig;
        double d;
        qcomplex_t val;
        mat_get(svd.S, i, i, raw);
        svd.S->elem->to_qf(&sig, raw);
        d = qf_to_double(qf_abs(sig));
        if (d > tol)
            val = qc_make(qf_div(QF_ONE, sig), QF_ZERO);
        else
            val = QC_ZERO;
        Sp[i * kdim + i] = val;
    }

    UH = mat_qcomplex_array_hermitian(Uq, svd.U->rows, svd.U->cols);
    VSp = (Vq && Sp) ? mat_qcomplex_array_mul(Vq, svd.V->rows, svd.V->cols, Sp, kdim) : NULL;
    Pinvq = (VSp && UH) ? mat_qcomplex_array_mul(VSp, svd.V->rows, kdim, UH, svd.U->rows) : NULL;
    if (!UH || !VSp || !Pinvq)
        goto fail;

    Pinv = mat_qcomplex_array_to_number_dense(svd.V->rows, svd.U->rows, Pinvq);

fail:
    mat_svd_factor_free(&svd);
    free(Uq);
    free(Vq);
    free(UH);
    free(Sp);
    free(VSp);
    free(Pinvq);
    return Pinv;
}

matrix_t *mat_nullspace(const matrix_t *A)
{
    matrix_t *AH = NULL, *Gram = NULL, *V = NULL, *N = NULL;
    number_t *evals = NULL;
    size_t nullity = 0, col = 0;
    double sigma_max = 0.0, tol;
    int rc;

    if (!A)
        return NULL;
    if (A->elem == &dval_elem)
        return mat_nullspace_dval_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    AH = mat_hermitian(A);
    Gram = AH ? mat_mul(AH, A) : NULL;
    if (!AH || !Gram)
        goto fail;

    evals = calloc(A->cols ? A->cols : 1, sizeof(number_t));
    if (!evals)
        goto fail;
    for (size_t i = 0; i < (A->cols ? A->cols : 1); ++i)
        evals[i] = num_new();

    rc = mat_eigendecompose(Gram, evals, &V);
    if (rc != 0 || !V)
        goto fail;

    for (size_t i = 0; i < A->cols; i++) {
        number_t re_num = num_real_part(evals[i]);
        double d = fabs(num_to_double(re_num));
        num_destroy(&re_num);
        if (d > sigma_max)
            sigma_max = d;
    }
    tol = mat_singular_tolerance(A, sqrt(sigma_max));
    tol *= tol;

    for (size_t i = 0; i < A->cols; i++) {
        number_t re_num = num_real_part(evals[i]);
        double d = fabs(num_to_double(re_num));
        num_destroy(&re_num);
        if (d <= tol)
            nullity++;
    }

    N = mat_create_dense_with_elem(A->cols, nullity, &number_elem);
    if (!N)
        goto fail;

    for (size_t i = 0; i < A->cols; i++) {
        number_t re_num = num_real_part(evals[i]);
        double d = fabs(num_to_double(re_num));
        num_destroy(&re_num);
        if (d > tol)
            continue;
        for (size_t r = 0; r < A->cols; r++) {
            number_t value = mat_get_num(V, r, i);
            mat_set(N, r, col, &value);
            num_destroy(&value);
        }
        col++;
    }

fail:
    mat_free(AH);
    mat_free(Gram);
    mat_free(V);
    if (evals) {
        for (size_t i = 0; i < (A->cols ? A->cols : 1); i++)
            num_destroy(&evals[i]);
    }
    free(evals);
    return N;
}

static matrix_t *mat_shift_subtract_eigenvalue(const matrix_t *A, const void *eigenvalue)
{
    matrix_t *Shifted = NULL;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    if (A->elem == &dval_elem) {
        dval_t *lambda = *(dval_t *const *)eigenvalue;

        if (!lambda) {
            mat_free(Shifted);
            return NULL;
        }

        for (size_t i = 0; i < A->rows; ++i) {
            dval_t *diag = NULL;
            dval_t *new_diag = NULL;

            mat_get(Shifted, i, i, &diag);
            if (!diag)
                diag = (dval_t *)DV_ZERO;

            dv_retain(diag);
            dv_retain(lambda);
            new_diag = dval_sub_simplify(diag, lambda);
            if (!new_diag) {
                mat_free(Shifted);
                return NULL;
            }
            mat_set(Shifted, i, i, &new_diag);
            dv_free(new_diag);
        }
    } else {
        unsigned char diag[64];
        unsigned char shifted[64];

        if (!elem_supports_numeric_algorithms(A->elem)) {
            mat_free(Shifted);
            return NULL;
        }

        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(Shifted, i, i, diag);
            A->elem->sub(shifted, diag, eigenvalue);
            mat_set(Shifted, i, i, shifted);
        }
    }

    return Shifted;
}

static bool mat_column_is_structural_zero(const matrix_t *A, size_t col)
{
    unsigned char raw[64];

    if (!A || col >= A->cols)
        return true;

    for (size_t i = 0; i < A->rows; ++i) {
        mat_get(A, i, col, raw);
        if (!elem_is_structural_zero(A->elem, raw))
            return false;
    }

    return true;
}

static matrix_t *mat_extract_column_copy(const matrix_t *A, size_t col)
{
    matrix_t *C;
    unsigned char raw[64];

    if (!A || col >= A->cols)
        return NULL;

    C = mat_create_dense_with_elem(A->rows, 1, A->elem);
    if (!C)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        mat_get(A, i, col, raw);
        mat_set(C, i, 0, raw);
    }

    return C;
}

matrix_t *mat_eigenspace(const matrix_t *A, const void *eigenvalue)
{
    matrix_t *Shifted = NULL;
    matrix_t *E = NULL;

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    E = mat_nullspace(Shifted);
    mat_free(Shifted);
    return E;
}

matrix_t *mat_generalized_eigenspace(const matrix_t *A, const void *eigenvalue,
                                     size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *G = NULL;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    if (order == 1)
        return mat_eigenspace(A, eigenvalue);

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    Power = mat_pow_int(Shifted, (int)order);
    if (!Power) {
        mat_free(Shifted);
        return NULL;
    }

    G = mat_nullspace(Power);
    mat_free(Shifted);
    mat_free(Power);
    return G;
}

matrix_t *mat_jordan_chain(const matrix_t *A, const void *eigenvalue,
                           size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *G = NULL;
    matrix_t *Chain = NULL;
    matrix_t *Tail = NULL;
    matrix_t *Probe = NULL;
    matrix_t *Current = NULL;
    bool found = false;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    Shifted = mat_shift_subtract_eigenvalue(A, eigenvalue);
    if (!Shifted)
        return NULL;

    G = mat_generalized_eigenspace(A, eigenvalue, order);
    if (!G)
        goto fail;

    for (size_t col = 0; col < G->cols && !found; ++col) {
        Tail = mat_extract_column_copy(G, col);
        if (!Tail)
            goto fail;

        Probe = mat_copy_preserving_store(Tail);
        if (!Probe)
            goto fail;

        for (size_t step = 1; step < order; ++step) {
            matrix_t *Next = mat_mul(Shifted, Probe);
            mat_free(Probe);
            Probe = Next;
            if (!Probe)
                goto fail;
        }

        if (!mat_column_is_structural_zero(Probe, 0))
            found = true;
        else {
            mat_free(Tail);
            Tail = NULL;
        }

        mat_free(Probe);
        Probe = NULL;
    }

    if (!found || !Tail)
        goto fail;

    Chain = mat_create_dense_with_elem(A->rows, order, A->elem);
    if (!Chain)
        goto fail;

    Current = Tail;
    Tail = NULL;
    for (size_t j = order; j-- > 0;) {
        unsigned char raw[64];

        for (size_t i = 0; i < A->rows; ++i) {
            mat_get(Current, i, 0, raw);
            mat_set(Chain, i, j, raw);
        }

        if (j > 0) {
            matrix_t *Prev = mat_mul(Shifted, Current);
            if (!Prev)
                goto fail;
            mat_free(Current);
            Current = Prev;
        }
    }

    mat_free(Current);
    mat_free(Shifted);
    mat_free(G);
    return Chain;

fail:
    mat_free(Current);
    mat_free(Tail);
    mat_free(Probe);
    mat_free(Chain);
    mat_free(G);
    mat_free(Shifted);
    return NULL;
}

matrix_t *mat_jordan_profile(const matrix_t *A, const void *eigenvalue)
{
    size_t *dims = NULL;
    matrix_t *G = NULL;
    matrix_t *P = NULL;
    size_t n;
    size_t blocks = 0;
    size_t out = 0;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;
    if (A->elem != &dval_elem)
        return NULL;

    n = A->rows;
    dims = calloc(n + 1, sizeof(*dims));
    if (!dims)
        return NULL;

    for (size_t k = 1; k <= n; ++k) {
        G = mat_generalized_eigenspace(A, eigenvalue, k);
        if (!G)
            goto fail;
        dims[k] = G->cols;
        mat_free(G);
        G = NULL;
        if (dims[k] < dims[k - 1])
            goto fail;
    }

    blocks = dims[1];
    P = mat_create_dense_with_elem(blocks, 1, &double_elem);
    if (!P)
        goto fail;

    for (size_t k = n; k >= 1; --k) {
        size_t at_least_k = dims[k] - dims[k - 1];
        size_t at_least_next = (k < n) ? (dims[k + 1] - dims[k]) : 0;
        size_t exact_k = at_least_k - at_least_next;
        double block_size = (double)k;

        for (size_t c = 0; c < exact_k; ++c) {
            if (out >= blocks)
                goto fail;
            mat_set(P, out, 0, &block_size);
            out++;
        }

        if (k == 1)
            break;
    }

    if (out != blocks)
        goto fail;

    free(dims);
    return P;

fail:
    free(dims);
    mat_free(G);
    mat_free(P);
    return NULL;
}

/* ============================================================
   Eigenvalues / eigenvectors (Hermitian Jacobi implementation)
   ============================================================ */

/* Squared Frobenius norm of all off-diagonal elements (convergence probe). */
static double offdiag_norm2_qc(const qcomplex_t *A, size_t n)
{
    double s = 0.0;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j)
                continue;
            s += qf_to_double(qf_add(qf_mul(qc_real(A[i * n + j]), qc_real(A[i * n + j])),
                                     qf_mul(qc_imag(A[i * n + j]), qc_imag(A[i * n + j]))));
        }
    }

    return s;
}

/* Zero A[p][q] with a complex Givens rotation; accumulate into V.
 *
 * Handles the Hermitian case: the rotation has a real magnitude (c, s)
 * and a complex phase derived from A[p][q] itself.
 *
 * After this call: (J† A J)[p][q] == 0,  V_new = V J.
 */
static void jacobi_apply_qc(qcomplex_t *A, qcomplex_t *V, size_t n, size_t p, size_t q)
{
    qcomplex_t a_pq = A[p * n + q];
    qfloat_t b_qf = qc_abs(a_pq);

    if (qf_to_double(b_qf) < 1e-150)
        return;

    qfloat_t app_qf = qc_real(A[p * n + p]);
    qfloat_t aqq_qf = qc_real(A[q * n + q]);
    qfloat_t two = qf_from_double(2.0);
    qfloat_t tau = qf_div(qf_sub(aqq_qf, app_qf), qf_mul(two, b_qf));
    qfloat_t sign = (qf_to_double(tau) >= 0.0) ? QF_ONE : qf_neg(QF_ONE);
    qfloat_t t_qf = qf_div(sign, qf_add(qf_abs(tau), qf_sqrt(qf_add(QF_ONE, qf_mul(tau, tau)))));
    qfloat_t c_qf = qf_div(QF_ONE, qf_sqrt(qf_add(QF_ONE, qf_mul(t_qf, t_qf))));
    qfloat_t s_qf = qf_mul(t_qf, c_qf);
    qfloat_t ns_qf = qf_neg(s_qf);
    qcomplex_t ph = qc_make(qf_mul(qf_div(QF_ONE, b_qf), qc_real(a_pq)),
                            qf_mul(qf_div(QF_ONE, b_qf), qc_imag(a_pq)));
    qcomplex_t ph_c = qc_conj(ph);
    qcomplex_t cph = qc_make(qf_mul(c_qf, qc_real(ph)), qf_mul(c_qf, qc_imag(ph)));
    qcomplex_t cph_c = qc_make(qf_mul(c_qf, qc_real(ph_c)), qf_mul(c_qf, qc_imag(ph_c)));
    qcomplex_t sph = qc_make(qf_mul(s_qf, qc_real(ph)), qf_mul(s_qf, qc_imag(ph)));
    qcomplex_t sph_c = qc_make(qf_mul(s_qf, qc_real(ph_c)), qf_mul(s_qf, qc_imag(ph_c)));

    for (size_t i = 0; i < n; i++) {
        qcomplex_t ai_p = A[i * n + p];
        qcomplex_t ai_q = A[i * n + q];
        A[i * n + p] = qc_add(qc_mul(cph, ai_p),
                              qc_make(qf_mul(ns_qf, qc_real(ai_q)), qf_mul(ns_qf, qc_imag(ai_q))));
        A[i * n + q] = qc_add(qc_mul(sph, ai_p),
                              qc_make(qf_mul(c_qf, qc_real(ai_q)), qf_mul(c_qf, qc_imag(ai_q))));
    }

    for (size_t k = 0; k < n; k++) {
        qcomplex_t ap_k = A[p * n + k];
        qcomplex_t aq_k = A[q * n + k];
        A[p * n + k] = qc_add(qc_mul(cph_c, ap_k),
                              qc_make(qf_mul(ns_qf, qc_real(aq_k)), qf_mul(ns_qf, qc_imag(aq_k))));
        A[q * n + k] = qc_add(qc_mul(sph_c, ap_k),
                              qc_make(qf_mul(c_qf, qc_real(aq_k)), qf_mul(c_qf, qc_imag(aq_k))));
    }

    for (size_t i = 0; i < n; i++) {
        qcomplex_t vi_p = V[i * n + p];
        qcomplex_t vi_q = V[i * n + q];
        V[i * n + p] = qc_add(qc_mul(cph, vi_p),
                              qc_make(qf_mul(ns_qf, qc_real(vi_q)), qf_mul(ns_qf, qc_imag(vi_q))));
        V[i * n + q] = qc_add(qc_mul(sph, vi_p),
                              qc_make(qf_mul(c_qf, qc_real(vi_q)), qf_mul(c_qf, qc_imag(vi_q))));
    }
}

/* ============================================================
   Hermitian fast path (Jacobi)
   ============================================================ */

static int mat_eigendecompose_hermitian(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors)
{
    size_t n = A->rows;
    size_t count = n * n;
    qcomplex_t *W = NULL;
    qcomplex_t *V = NULL;
    matrix_t *Vnum = NULL;

    W = calloc(count ? count : 1u, sizeof(qcomplex_t));
    V = calloc(count ? count : 1u, sizeof(qcomplex_t));
    if (!W || !V) {
        free(W);
        free(V);
        return -3;
    }

    for (size_t i = 0; i < n; i++) {
        V[i * n + i] = QC_ONE;
        for (size_t j = 0; j < n; j++) {
            unsigned char raw[64];

            mat_get(A, i, j, raw);
            A->elem->to_qc(&W[i * n + j], raw);
        }
    }

    double fro2 = 0.0;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++)
            fro2 += qf_to_double(qf_add(qf_mul(qc_real(W[i * n + j]), qc_real(W[i * n + j])),
                                        qf_mul(qc_imag(W[i * n + j]), qc_imag(W[i * n + j]))));
    double tol = fro2 * 1e-29;

    for (int sweep = 0; sweep < 50; sweep++) {
        for (size_t p = 0; p < n - 1; p++)
            for (size_t q = p + 1; q < n; q++)
                jacobi_apply_qc(W, V, n, p, q);
        if (offdiag_norm2_qc(W, n) < tol) break;
    }

    if (eigenvalues) {
        number_t *ev = (number_t *)eigenvalues;
        for (size_t i = 0; i < n; i++)
            ev[i] = num_create_from_qfloat(qc_real(W[i * n + i]));
    }

    if (eigenvectors) {
        Vnum = mat_create_dense_with_elem(n, n, &number_elem);
        if (!Vnum) {
            free(V);
            free(W);
            return -3;
        }
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                number_t value = num_create_from_qcomplex(V[i * n + j]);
                mat_set(Vnum, i, j, &value);
                num_destroy(&value);
            }
        }
        *eigenvectors = Vnum;
    }

    free(V);
    free(W);
    return 0;
}

/* ============================================================
   General QR eigensolver — Francis implicit single-shift
   All internal arithmetic is in qcomplex_t (quad precision).
   ============================================================ */

/* Index into flat n×n qcomplex array */
#define QCM(a,i,j,n) ((a)[(size_t)(i)*(n)+(size_t)(j)])

static inline qfloat_t qc_abs2_qf(qcomplex_t z)
{
    return qf_add(qf_mul(qc_real(z), qc_real(z)), qf_mul(qc_imag(z), qc_imag(z)));
}

static inline qcomplex_t qcs(qfloat_t s, qcomplex_t z)
{
    return qc_make(qf_mul(s, qc_real(z)), qf_mul(s, qc_imag(z)));
}

/* Detect whether A is Hermitian: A[i,j] == conj(A[j,i]) within tolerance */
int mat_is_hermitian(const matrix_t *A)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;
    unsigned char aij[64], aji[64], cji[64], diff[64], diag[64];
    double tol2 = 0.0;
    /* build tolerance from Frobenius norm */
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get(A, i, j, aij);
            tol2 += e->abs2(aij);
        }
    tol2 *= 1e-28;
    for (size_t i = 0; i < n; i++) {
        mat_get(A, i, i, diag);
        e->conj_elem(cji, diag);
        e->sub(diff, diag, cji);
        if (e->abs2(diff) > tol2) return 0;
    }
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            mat_get(A, i, j, aij);
            mat_get(A, j, i, aji);
            e->conj_elem(cji, aji);
            e->sub(diff, aij, cji);
            if (e->abs2(diff) > tol2) return 0;
        }
    return 1;
}

/* Householder reduction to upper Hessenberg form in-place.
 * H is n×n qcomplex array. Q accumulates transformations (Q·H·Q†). */
static void eigen_hess_reduce(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    qfloat_t two = qf_from_double(2.0);

    for (size_t k = 0; k + 2 <= n; k++) {
        /* Build Householder vector v from H[k+1..n-1, k] */
        size_t len = n - k - 1;
        qcomplex_t *col = (qcomplex_t *)malloc(len * sizeof(qcomplex_t));
        if (!col) return;
        for (size_t i = 0; i < len; i++)
            col[i] = QCM(H, k+1+i, k, n);

        /* norm of column */
        qfloat_t norm2 = QF_ZERO;
        for (size_t i = 0; i < len; i++)
            norm2 = qf_add(norm2, qc_abs2_qf(col[i]));
        qfloat_t norm = qf_sqrt(norm2);

        if (qf_to_double(norm) < 1e-150) { free(col); continue; }

        /* phase of first element: alpha = -norm * (col[0] / |col[0]|) */
        qfloat_t abs0 = qc_abs(col[0]);
        qcomplex_t alpha;
        if (qf_to_double(abs0) < 1e-150) {
            alpha = qc_make(qf_neg(norm), QF_ZERO);
        } else {
            /* alpha = -norm * col[0]/|col[0]| */
            qfloat_t inv0 = qf_div(QF_ONE, abs0);
            qcomplex_t phase = qcs(inv0, col[0]);
            alpha = qcs(qf_neg(norm), phase);
        }

        /* v = col; v[0] -= alpha */
        col[0] = qc_sub(col[0], alpha);

        /* beta = 2 / (v† v) */
        qfloat_t vdotv = QF_ZERO;
        for (size_t i = 0; i < len; i++)
            vdotv = qf_add(vdotv, qc_abs2_qf(col[i]));
        qfloat_t beta = qf_div(two, vdotv);

        /* Apply P = I - beta v v† from left: H ← P H */
        /* w = beta * v† H[k+1:, :] */
        for (size_t j = 0; j < n; j++) {
            qcomplex_t s = QC_ZERO;
            for (size_t i = 0; i < len; i++)
                s = qc_add(s, qc_mul(qc_conj(col[i]), QCM(H, k+1+i, j, n)));
            s = qcs(beta, s);
            for (size_t i = 0; i < len; i++)
                QCM(H, k+1+i, j, n) = qc_sub(QCM(H, k+1+i, j, n), qc_mul(col[i], s));
        }

        /* Apply P from right: H ← H P */
        for (size_t i = 0; i < n; i++) {
            qcomplex_t s = QC_ZERO;
            for (size_t j = 0; j < len; j++)
                s = qc_add(s, qc_mul(QCM(H, i, k+1+j, n), col[j]));
            s = qcs(beta, s);
            for (size_t j = 0; j < len; j++)
                QCM(H, i, k+1+j, n) = qc_sub(QCM(H, i, k+1+j, n), qc_mul(qc_conj(col[j]), s));
        }

        /* Accumulate into Q: Q ← Q P */
        for (size_t i = 0; i < n; i++) {
            qcomplex_t s = QC_ZERO;
            for (size_t j = 0; j < len; j++)
                s = qc_add(s, qc_mul(QCM(Q, i, k+1+j, n), col[j]));
            s = qcs(beta, s);
            for (size_t j = 0; j < len; j++)
                QCM(Q, i, k+1+j, n) = qc_sub(QCM(Q, i, k+1+j, n), qc_mul(qc_conj(col[j]), s));
        }

        free(col);
    }
}

/* Givens rotation: find c (real), s (complex) such that
 *   [ c    conj(s) ] [a]   [r]
 *   [-s      c    ] [b] = [0]
 * where r = hypot(|a|,|b|). */
static void givens_cs(qcomplex_t a, qcomplex_t b,
                      qfloat_t *c_out, qcomplex_t *s_out)
{
    qfloat_t abs_a = qc_abs(a);
    qfloat_t abs_b = qc_abs(b);
    qfloat_t r = qf_sqrt(qf_add(qf_mul(abs_a, abs_a), qf_mul(abs_b, abs_b)));
    if (qf_to_double(r) < 1e-150) {
        *c_out = QF_ONE;
        *s_out = QC_ZERO;
        return;
    }
    qfloat_t c = qf_div(abs_a, r);
    /* s = c * b / a  (complex division; if a≈0 fall back) */
    qcomplex_t s;
    if (qf_to_double(abs_a) < 1e-150) {
        s = qc_make(QF_ZERO, QF_ZERO);
    } else {
        qfloat_t inv_a_abs = qf_div(QF_ONE, abs_a);
        qcomplex_t a_unit = qcs(inv_a_abs, a); /* a/|a| */
        /* s = c * conj(a_unit) * b */
        s = qcs(c, qc_mul(qc_conj(a_unit), b));
    }
    *c_out = c;
    *s_out = s;
}

/* Apply one Givens rotation to columns k and k+1 of H (rows j0..n-1)
 * and rows k and k+1 of H (cols 0..n-1), and Q cols k and k+1. */
static void givens_apply(qcomplex_t *H, qcomplex_t *Q, size_t n,
                         size_t k, size_t row_start,
                         qfloat_t c, qcomplex_t s)
{
    qcomplex_t cs = qc_conj(s);

    /* left multiply rows k, k+1 of H */
    for (size_t j = row_start; j < n; j++) {
        qcomplex_t x = QCM(H, k,   j, n);
        qcomplex_t y = QCM(H, k+1, j, n);
        QCM(H, k,   j, n) = qc_add(qcs(c, x), qc_mul(cs, y));
        QCM(H, k+1, j, n) = qc_add(qc_neg(qc_mul(s, x)), qcs(c, y));
    }

    /* right multiply cols k, k+1 of H */
    for (size_t i = 0; i < n; i++) {
        qcomplex_t x = QCM(H, i, k,   n);
        qcomplex_t y = QCM(H, i, k+1, n);
        QCM(H, i, k,   n) = qc_add(qcs(c, x), qc_mul(s, y));
        QCM(H, i, k+1, n) = qc_add(qc_mul(qc_neg(cs), x), qcs(c, y));
    }

    /* accumulate into Q (eigenvector columns) */
    for (size_t i = 0; i < n; i++) {
        qcomplex_t x = QCM(Q, i, k,   n);
        qcomplex_t y = QCM(Q, i, k+1, n);
        QCM(Q, i, k,   n) = qc_add(qcs(c, x), qc_mul(s, y));
        QCM(Q, i, k+1, n) = qc_add(qc_mul(qc_neg(cs), x), qcs(c, y));
    }
}

/* Wilkinson shift: eigenvalue of bottom 2×2 submatrix of T[0..sz-1, 0..sz-1]
 * that is closest to T[sz-1, sz-1]. */
static qcomplex_t wilkinson_shift(const qcomplex_t *T, size_t sz, size_t n)
{
    if (sz < 2) return QCM(T, sz-1, sz-1, n);
    qcomplex_t a = QCM(T, sz-2, sz-2, n);
    qcomplex_t b = QCM(T, sz-2, sz-1, n);
    qcomplex_t c = QCM(T, sz-1, sz-2, n);
    qcomplex_t d = QCM(T, sz-1, sz-1, n);
    /* eigenvalues of [[a,b],[c,d]]:
     * mu = (a+d)/2 ± sqrt(((a-d)/2)^2 + bc) */
    qfloat_t half = qf_from_double(0.5);
    qcomplex_t tr_half = qcs(half, qc_add(a, d));
    qcomplex_t disc = qc_add(
        qc_mul(qcs(half, qc_sub(a, d)), qcs(half, qc_sub(a, d))),
        qc_mul(b, c));
    qcomplex_t sq = qc_sqrt(disc);
    qcomplex_t e1 = qc_add(tr_half, sq);
    qcomplex_t e2 = qc_sub(tr_half, sq);
    /* pick eigenvalue closer to d */
    qfloat_t d1 = qc_abs(qc_sub(e1, d));
    qfloat_t d2 = qc_abs(qc_sub(e2, d));
    return (qf_to_double(d1) <= qf_to_double(d2)) ? e1 : e2;
}

static void qr_schur_2x2(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    qfloat_t eps = qf_from_double(1e-30);
    qcomplex_t a = QCM(H, 0, 0, n);
    qcomplex_t b = QCM(H, 0, 1, n);
    qcomplex_t c = QCM(H, 1, 0, n);
    qcomplex_t d = QCM(H, 1, 1, n);

    if (qf_lt(qc_abs(c), eps)) {
        QCM(H, 1, 0, n) = QC_ZERO;
        return;
    }

    qcomplex_t half_tr = qcs(qf_div(QF_ONE, qf_from_double(2.0)),
                             qc_add(a, d));
    qcomplex_t det = qc_sub(qc_mul(a, d), qc_mul(b, c));
    qcomplex_t disc = qc_sub(qc_mul(half_tr, half_tr), det);
    qcomplex_t root = qc_sqrt(disc);
    qcomplex_t lam1 = qc_add(half_tr, root);
    qcomplex_t lam2 = qc_sub(half_tr, root);

    qcomplex_t v0 = qc_sub(lam1, d);
    qcomplex_t v1 = c;
    qfloat_t vnrm = qf_sqrt(qf_add(qc_abs2_qf(v0), qc_abs2_qf(v1)));
    if (qf_eq(vnrm, QF_ZERO))
        return;

    qfloat_t inv = qf_div(QF_ONE, vnrm);
    qcomplex_t q0 = qcs(inv, v0);
    qcomplex_t q1 = qcs(inv, v1);
    qcomplex_t cq0 = qc_conj(q0);
    qcomplex_t cq1 = qc_conj(q1);
    qcomplex_t neg_cq1 = qc_sub(QC_ZERO, cq1);

    qcomplex_t hq01 = qc_add(qc_mul(a, neg_cq1), qc_mul(b, cq0));
    qcomplex_t hq11 = qc_add(qc_mul(c, neg_cq1), qc_mul(d, cq0));
    qcomplex_t t01 = qc_add(qc_mul(cq0, hq01), qc_mul(cq1, hq11));

    QCM(H, 0, 0, n) = lam1;
    QCM(H, 0, 1, n) = t01;
    QCM(H, 1, 0, n) = QC_ZERO;
    QCM(H, 1, 1, n) = lam2;

    for (size_t i = 0; i < n; ++i) {
        qcomplex_t qi0 = QCM(Q, i, 0, n);
        qcomplex_t qi1 = QCM(Q, i, 1, n);
        QCM(Q, i, 0, n) = qc_add(qc_mul(qi0, q0), qc_mul(qi1, q1));
        QCM(Q, i, 1, n) = qc_add(qc_mul(qi0, neg_cq1), qc_mul(qi1, cq0));
    }
}

/* Single QR step with shift mu on active submatrix [0..sz-1].
 * H and Q are n×n flat arrays. */
static void qr_step(qcomplex_t *H, qcomplex_t *Q, size_t sz, size_t n,
                    qcomplex_t mu)
{
    /* Shift: H ← H - mu I */
    for (size_t i = 0; i < sz; i++)
        QCM(H, i, i, n) = qc_sub(QCM(H, i, i, n), mu);

    /* QR via Givens on Hessenberg structure */
    qfloat_t c[256];
    qcomplex_t s[256];
    for (size_t k = 0; k + 1 < sz; k++) {
        givens_cs(QCM(H, k, k, n), QCM(H, k+1, k, n), &c[k], &s[k]);
        givens_apply(H, Q, n, k, k, c[k], s[k]);
    }

    /* Unshift */
    for (size_t i = 0; i < sz; i++)
        QCM(H, i, i, n) = qc_add(QCM(H, i, i, n), mu);
}

/* Implicit single-shift QR: drive H to quasi-upper-triangular (Schur form). */
static void qr_schur(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    size_t sz = n;
    for (int iter = 0; iter < (int)(30 * n) && sz > 1; iter++) {
        /* deflate trailing near-zero subdiagonals */
        while (sz > 1) {
            qfloat_t sub = qc_abs(QCM(H, sz-1, sz-2, n));
            qfloat_t d1  = qc_abs(QCM(H, sz-2, sz-2, n));
            qfloat_t d2  = qc_abs(QCM(H, sz-1, sz-1, n));
            qfloat_t tol = qf_mul(qf_from_double(1e-29),
                                   qf_add(d1, d2));
            if (qf_to_double(sub) <= qf_to_double(tol)) {
                QCM(H, sz-1, sz-2, n) = QC_ZERO;
                sz--;
            } else {
                break;
            }
        }
        if (sz <= 1) break;
        if (sz == 2) {
            qr_schur_2x2(H, Q, n);
            break;
        }
        qcomplex_t mu = wilkinson_shift(H, sz, n);
        qr_step(H, Q, sz, n, mu);
    }
}

/* Back-substitution: given upper-triangular T and eigenvalue T[k,k],
 * compute the k-th eigenvector by back-solving (T - lambda I) x = -e_k
 * for x[0..k-1] (x[k]=1 by convention). */
static void backsub_eigenvec(const qcomplex_t *T, qcomplex_t *Y, size_t n, size_t k)
{
    qcomplex_t lambda = QCM(T, k, k, n);
    /* Y[:,k] will be the eigenvector; zero it first */
    for (size_t i = 0; i < n; i++)
        QCM(Y, i, k, n) = QC_ZERO;
    QCM(Y, k, k, n) = QC_ONE;

    /* Back-substitute rows k-1 down to 0 */
    for (size_t i = k; i-- > 0; ) {
        /* (T[i,i] - lambda) y[i] = -sum_{j=i+1}^{k} T[i,j] y[j] */
        qcomplex_t rhs = QC_ZERO;
        for (size_t j = i + 1; j <= k; j++)
            rhs = qc_sub(rhs, qc_mul(QCM(T, i, j, n), QCM(Y, j, k, n)));
        qcomplex_t diag = qc_sub(QCM(T, i, i, n), lambda);
        double dabs = qf_to_double(qc_abs(diag));
        if (dabs < 1e-150)
            QCM(Y, i, k, n) = QC_ZERO;
        else
            QCM(Y, i, k, n) = qc_div(rhs, diag);
    }

    /* Normalize */
    qfloat_t nrm = QF_ZERO;
    for (size_t i = 0; i <= k; i++)
        nrm = qf_add(nrm, qc_abs2_qf(QCM(Y, i, k, n)));
    nrm = qf_sqrt(nrm);
    if (qf_to_double(nrm) > 1e-150) {
        qfloat_t inv = qf_div(QF_ONE, nrm);
        for (size_t i = 0; i <= k; i++)
            QCM(Y, i, k, n) = qcs(inv, QCM(Y, i, k, n));
    }
}

static int mat_eigendecompose_general(const matrix_t *A, void *eigenvalues,
                                      matrix_t **eigenvectors)
{
    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    /* Load A into flat qcomplex array H */
    qcomplex_t *H = (qcomplex_t *)malloc(n * n * sizeof(qcomplex_t));
    unsigned char raw[64];
    if (!H) return -3;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            mat_get(A, i, j, raw);
            e->to_qc(&QCM(H, i, j, n), raw);
        }

    /* Q = identity (accumulates similarity transforms) */
    qcomplex_t *Qm = (qcomplex_t *)calloc(n * n, sizeof(qcomplex_t));
    if (!Qm) { free(H); return -3; }
    for (size_t i = 0; i < n; i++) QCM(Qm, i, i, n) = QC_ONE;

    /* Hessenberg reduction then Schur form */
    eigen_hess_reduce(H, Qm, n);
    qr_schur(H, Qm, n);

    /* Extract eigenvalues from diagonal of Schur matrix */
    if (eigenvalues) {
        number_t *ev = (number_t *)eigenvalues;
        for (size_t i = 0; i < n; i++)
            ev[i] = num_create_from_qcomplex(QCM(H, i, i, n));
    }

    /* Compute eigenvectors if requested */
    if (eigenvectors) {
        /* Back-substitution: eigenvectors of T in Schur basis */
        qcomplex_t *Y = (qcomplex_t *)calloc(n * n, sizeof(qcomplex_t));
        if (!Y) { free(H); free(Qm); return -3; }

        for (size_t k = 0; k < n; k++)
            backsub_eigenvec(H, Y, n, k);

        /* Transform back: eigvec_A = Q * Y */
        matrix_t *V = mat_create_dense_with_elem(n, n, &number_elem);
        if (!V) { free(H); free(Qm); free(Y); return -3; }

        number_t value;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                qcomplex_t sum = QC_ZERO;
                for (size_t k = 0; k < n; k++)
                    sum = qc_add(sum, qc_mul(QCM(Qm, i, k, n), QCM(Y, k, j, n)));
                value = num_create_from_qcomplex(sum);
                mat_set(V, i, j, &value);
                num_destroy(&value);
            }
        }
        free(Y);
        *eigenvectors = V;
    }

    free(H);
    free(Qm);
    return 0;
}

int mat_eigendecompose(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors)
{
    if (!A) return -1;
    if (A->rows != A->cols) return -2;

    if (matrix_is_symbolic(A))
        return mat_eigendecompose_dval(A, (dval_t **)eigenvalues, eigenvectors);

    if (!elem_supports_numeric_algorithms(A->elem)) return -3;

    if (mat_is_hermitian(A))
        return mat_eigendecompose_hermitian(A, eigenvalues, eigenvectors);
    return mat_eigendecompose_general(A, eigenvalues, eigenvectors);
}

int mat_eigenvalues(const matrix_t *A, void *eigenvalues)
{
    return mat_eigendecompose(A, eigenvalues, NULL);
}

matrix_t *mat_eigenvectors(const matrix_t *A)
{
    matrix_t *V = NULL;
    if (mat_eigendecompose(A, NULL, &V) != 0)
        return NULL;
    return V;
}

/* ============================================================
   Debug printing

   ============================================================ */


   /* ---------- small helpers on qcomplex ---------- */

static inline void qc_add_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_add(*x, y);
}

static inline void qc_sub_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_sub(*x, y);
}

static inline void qc_mul_inplace(qcomplex_t *x, qcomplex_t y)
{
    *x = qc_mul(*x, y);
}

static inline qcomplex_t qc_scale(qcomplex_t z, qfloat_t s)
{
    return qc_make(qf_mul(qc_real(z), s), qf_mul(qc_imag(z), s));
}

/* norm2 of a complex vector (as qfloat) */
static qfloat_t qc_vec_norm2(const qcomplex_t *x, size_t n)
{
    qfloat_t s = QF_ZERO;
    for (size_t i = 0; i < n; ++i) {
        qfloat_t a = qc_abs(x[i]);
        s = qf_add(s, qf_mul(a, a));
    }
    return s;
}

/* ============================================================
   Convert arbitrary A to dense qcomplex working storage
   ============================================================ */

static qcomplex_t *mat_load_qcomplex_array(const matrix_t *A)
{
    qcomplex_t *Z = NULL;
    size_t count = 0;

    if (!A)
        return NULL;

    count = A->rows * A->cols;
    Z = calloc(count ? count : 1u, sizeof(qcomplex_t));
    if (!Z)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            unsigned char v_raw[64];

            mat_get(A, i, j, v_raw);
            A->elem->to_qc(&Z[i * A->cols + j], v_raw);
        }
    }

    return Z;
}

static matrix_t *mat_qcomplex_array_to_number_dense(size_t rows, size_t cols,
                                                    const qcomplex_t *data)
{
    matrix_t *A = mat_create_dense_with_elem(rows, cols, &number_elem);

    if (!A)
        return NULL;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            number_t value = num_create_from_qcomplex(data[i * cols + j]);
            mat_set(A, i, j, &value);
            num_destroy(&value);
        }
    }

    return A;
}

static matrix_t *mat_qcomplex_array_to_number_upper(size_t n, const qcomplex_t *data)
{
    matrix_t *A = mat_create_upper_triangular_with_elem(n, n, &number_elem);

    if (!A)
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i; j < n; ++j) {
            number_t value = num_create_from_qcomplex(data[i * n + j]);
            mat_set(A, i, j, &value);
            num_destroy(&value);
        }
    }

    return A;
}

static matrix_t *mat_qcomplex_array_to_number_diagonal(size_t n, const qcomplex_t *data)
{
    matrix_t *A = mat_create_diagonal_with_elem(n, &number_elem);

    if (!A)
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        number_t value = num_create_from_qcomplex(data[i * n + i]);
        mat_set(A, i, i, &value);
        num_destroy(&value);
    }

    return A;
}

static qcomplex_t *mat_qcomplex_array_hermitian(const qcomplex_t *A, size_t rows, size_t cols)
{
    size_t count = rows * cols;
    qcomplex_t *AH = calloc(count ? count : 1u, sizeof(qcomplex_t));

    if (!AH)
        return NULL;

    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            AH[j * rows + i] = qc_conj(A[i * cols + j]);

    return AH;
}

static qcomplex_t *mat_qcomplex_array_mul(const qcomplex_t *A, size_t a_rows, size_t a_cols,
                                          const qcomplex_t *B, size_t b_cols)
{
    size_t count = a_rows * b_cols;
    qcomplex_t *C = calloc(count ? count : 1u, sizeof(qcomplex_t));

    if (!C)
        return NULL;

    for (size_t i = 0; i < a_rows; ++i) {
        for (size_t j = 0; j < b_cols; ++j) {
            qcomplex_t sum = QC_ZERO;

            for (size_t k = 0; k < a_cols; ++k)
                sum = qc_add(sum, qc_mul(A[i * a_cols + k], B[k * b_cols + j]));
            C[i * b_cols + j] = sum;
        }
    }

    return C;
}

/* ============================================================
   Safe complex Hessenberg reduction: Q* H Q = Hessenberg(H)
   H is overwritten with its Hessenberg form.
   *Qptr is accumulated with the unitary similarity (created if NULL).
   ============================================================ */
static int hessenberg_reduce_qc(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    if (!H || !Q)
        return -1;

    if (n <= 2)
        return 0; /* already Hessenberg */

    for (size_t k = 0; k + 2 < n; ++k) {

        size_t m = n - (k + 1);          /* length of Householder vector */
        qcomplex_t *v = malloc(m * sizeof(qcomplex_t));
        if (!v) return -1;

        /* x = H[k+1..n-1, k] */
        qcomplex_t x0 = QCM(H, k + 1, k, n);

        qfloat_t sigma = QF_ZERO;
        for (size_t i = 1; i < m; ++i) {
            qfloat_t a = qc_abs(QCM(H, k + 1 + i, k, n));
            sigma = qf_add(sigma, qf_mul(a, a));
        }

        if (qf_eq(sigma, QF_ZERO)) {
            free(v);
            continue; /* nothing to do */
        }

        qfloat_t x0_abs = qc_abs(x0);
        qfloat_t mu = qf_sqrt(qf_add(qf_mul(x0_abs, x0_abs), sigma));

        qcomplex_t v0;
        if (qf_eq(x0_abs, QF_ZERO)) {
            v0 = qc_make(mu, QF_ZERO);
        } else {
            qcomplex_t x0_scaled = qc_scale(x0, qf_div(QF_ONE, x0_abs));
            v0 = qc_add(x0_scaled, qc_scale(x0_scaled, qf_div(mu, x0_abs)));
        }

        v[0] = v0;
        for (size_t i = 1; i < m; ++i)
            v[i] = QCM(H, k + 1 + i, k, n);

        /* normalise v */
        qfloat_t vnorm2 = qc_vec_norm2(v, m);
        qfloat_t vnorm  = qf_sqrt(vnorm2);
        if (!qf_eq(vnorm, QF_ZERO)) {
            qfloat_t inv = qf_div(QF_ONE, vnorm);
            for (size_t i = 0; i < m; ++i)
                v[i] = qc_scale(v[i], inv);
        }

        qfloat_t two = qf_add(QF_ONE, QF_ONE);

        /* apply from left: H := (I - 2 v v*) H, rows k+1..n-1, cols k..n-1 */
        for (size_t j = k; j < n; ++j) {
            qcomplex_t w = QC_ZERO;
            for (size_t i = 0; i < m; ++i) {
                w = qc_add(w, qc_mul(qc_conj(v[i]), QCM(H, k + 1 + i, j, n)));
            }
            w = qc_scale(w, two);

            for (size_t i = 0; i < m; ++i) {
                QCM(H, k + 1 + i, j, n) = qc_sub(QCM(H, k + 1 + i, j, n),
                                                  qc_mul(v[i], w));
            }
        }

        /* apply from right: H := H (I - 2 v v*), rows 0..n-1, cols k+1..n-1 */
        for (size_t i = 0; i < n; ++i) {
            qcomplex_t w = QC_ZERO;
            for (size_t j = 0; j < m; ++j)
                w = qc_add(w, qc_mul(QCM(H, i, k + 1 + j, n), v[j]));
            w = qc_scale(w, two);

            for (size_t j = 0; j < m; ++j) {
                QCM(H, i, k + 1 + j, n) = qc_sub(QCM(H, i, k + 1 + j, n),
                                                  qc_mul(w, qc_conj(v[j])));
            }
        }

        /* accumulate into Q: Q := Q (I - 2 v v*), rows 0..n-1, cols k+1..n-1 */
        for (size_t i = 0; i < n; ++i) {
            qcomplex_t w = QC_ZERO;
            for (size_t j = 0; j < m; ++j)
                w = qc_add(w, qc_mul(QCM(Q, i, k + 1 + j, n), v[j]));
            w = qc_scale(w, two);

            for (size_t j = 0; j < m; ++j) {
                QCM(Q, i, k + 1 + j, n) = qc_sub(QCM(Q, i, k + 1 + j, n),
                                                  qc_mul(w, qc_conj(v[j])));
            }
        }

        /* enforce exact zeros below subdiagonal in column k */
        for (size_t i = k + 2; i < n; ++i)
            QCM(H, i, k, n) = QC_ZERO;

        free(v);
    }

    return 0;
}

/* ============================================================
   Wilkinson shift for trailing 2x2 block of H
   ============================================================ */

static qcomplex_t wilkinson_shift_qc(const qcomplex_t *H, size_t m, size_t n)
{
    /* m is last index (inclusive), use 2x2 block H[m-1:m, m-1:m] */
    qcomplex_t a = QCM(H, m - 1, m - 1, n);
    qcomplex_t b = QCM(H, m - 1, m, n);
    qcomplex_t c = QCM(H, m, m - 1, n);
    qcomplex_t d = QCM(H, m, m, n);

    qcomplex_t tr = qc_add(a, d);
    qcomplex_t det = qc_sub(qc_mul(a, d), qc_mul(b, c));

    qcomplex_t half_tr = qc_scale(tr, qf_div(QF_ONE, qf_from_double(2.0)));
    qcomplex_t disc = qc_sub(qc_mul(half_tr, half_tr), det);
    qcomplex_t root = qc_sqrt(disc);

    qcomplex_t mu1 = qc_add(half_tr, root);
    qcomplex_t mu2 = qc_sub(half_tr, root);

    /* choose eigenvalue closer to d */
    qcomplex_t diff1 = qc_sub(d, mu1);
    qcomplex_t diff2 = qc_sub(d, mu2);
    qfloat_t n1 = qc_abs(diff1);
    qfloat_t n2 = qc_abs(diff2);

    return (qf_lt(n1, n2) ? mu1 : mu2);
}

/* ============================================================
   Implicit double-shift QR on Hessenberg H, accumulate Q
   ============================================================ */
static int schur_qr_qc(qcomplex_t *H, qcomplex_t *Q, size_t n)
{
    if (!H || !Q)
        return -1;

    /* nothing to do for 1x1: already in Schur form */
    if (n <= 1)
        return 0;

    const int max_iter = 1000 * (int)n;
    qfloat_t eps = qf_from_double(1e-30);

    /* 2×2: compute Schur form analytically via eigenvector of one eigenvalue. */
    if (n == 2) {
        qcomplex_t a = QCM(H, 0, 0, n);
        qcomplex_t b = QCM(H, 0, 1, n);
        qcomplex_t c = QCM(H, 1, 0, n);
        qcomplex_t d = QCM(H, 1, 1, n);

        if (qf_lt(qc_abs(c), eps))
            return 0; /* already upper triangular */

        /* eigenvalues from quadratic */
        qcomplex_t half_tr = qc_scale(qc_add(a, d),
                                      qf_div(QF_ONE, qf_from_double(2.0)));
        qcomplex_t det     = qc_sub(qc_mul(a, d), qc_mul(b, c));
        qcomplex_t disc    = qc_sub(qc_mul(half_tr, half_tr), det);
        qcomplex_t root    = qc_sqrt(disc);
        qcomplex_t lam1    = qc_add(half_tr, root);
        qcomplex_t lam2    = qc_sub(half_tr, root);

        /* eigenvector for lam1: [lam1-d, c]^T (always valid since c ≠ 0) */
        qcomplex_t v0 = qc_sub(lam1, d);
        qcomplex_t v1 = c;
        qfloat_t vnrm = qf_sqrt(qf_add(qf_mul(qc_abs(v0), qc_abs(v0)),
                                        qf_mul(qc_abs(v1), qc_abs(v1))));
        if (qf_eq(vnrm, QF_ZERO))
            return -1;
        qfloat_t inv = qf_div(QF_ONE, vnrm);
        qcomplex_t q0 = qc_scale(v0, inv);   /* first column of Q_step */
        qcomplex_t q1 = qc_scale(v1, inv);

        /* T = Q* H Q:  T[0,0]=lam1, T[1,0]=0, T[1,1]=lam2,
         * T[0,1] = conj(q0)*(a*(-conj(q1))+b*conj(q0))
         *        + conj(q1)*(c*(-conj(q1))+d*conj(q0))  */
        qcomplex_t cq0 = qc_conj(q0), cq1 = qc_conj(q1);
        qcomplex_t neg_cq1 = qc_sub(QC_ZERO, cq1);
        qcomplex_t hq01 = qc_add(qc_mul(a, neg_cq1), qc_mul(b, cq0));
        qcomplex_t hq11 = qc_add(qc_mul(c, neg_cq1), qc_mul(d, cq0));
        qcomplex_t t01  = qc_add(qc_mul(cq0, hq01), qc_mul(cq1, hq11));
        qcomplex_t zero2 = QC_ZERO;

        QCM(H, 0, 0, n) = lam1;
        QCM(H, 0, 1, n) = t01;
        QCM(H, 1, 0, n) = zero2;
        QCM(H, 1, 1, n) = lam2;

        /* accumulate Q_total = Q_old * Q_step
         * Q_step = [[q0, -conj(q1)], [q1, conj(q0)]] */
        for (size_t i = 0; i < 2; ++i) {
            qcomplex_t qi0 = QCM(Q, i, 0, n);
            qcomplex_t qi1 = QCM(Q, i, 1, n);
            QCM(Q, i, 0, n) = qc_add(qc_mul(qi0, q0),      qc_mul(qi1, q1));
            QCM(Q, i, 1, n) = qc_add(qc_mul(qi0, neg_cq1), qc_mul(qi1, cq0));
        }
        return 0;
    }

    qcomplex_t zero = QC_ZERO;

    for (size_t m = n - 1; m > 0; ) {

        /* check for deflation at H[m, m-1] */
        qcomplex_t hml = QCM(H, m, m - 1, n);
        if (qf_lt(qc_abs(hml), eps)) {
            /* decouple 1x1 block */
            m--;
            continue;
        }

        int iter = 0;
        while (iter++ < max_iter) {

            /* Wilkinson shift */
            qcomplex_t mu = wilkinson_shift_qc(H, m, n);

            /* Single-shift implicit QR step (bulge-chase Householder) */
            for (size_t k = 0; k < m; ++k) {

                /* k=0: initial shifted vector [H[0,0]-mu, H[1,0]]
                 * k>0: chase the bulge using [H[k,k-1], H[k+1,k-1]] (no shift) */
                qcomplex_t a, b;
                if (k == 0) {
                    a = qc_sub(QCM(H, 0, 0, n), mu);
                    b = QCM(H, 1, 0, n);
                } else {
                    a = QCM(H, k, k - 1, n);
                    b = QCM(H, k + 1, k - 1, n);
                }

                qfloat_t a_abs = qc_abs(a);
                qfloat_t b_abs = qc_abs(b);
                qfloat_t nrm2  = qf_add(qf_mul(a_abs, a_abs),
                                        qf_mul(b_abs, b_abs));
                if (qf_lt(nrm2, eps)) continue;

                qfloat_t nrm = qf_sqrt(nrm2);

                /* Householder vector: v = [a + (a/|a|)*nrm, b], then normalise */
                qcomplex_t v0;
                if (qf_lt(a_abs, eps)) {
                    v0 = qc_make(nrm, QF_ZERO);
                } else {
                    qcomplex_t a_phase = qc_scale(a, qf_div(QF_ONE, a_abs));
                    v0 = qc_add(a, qc_scale(a_phase, nrm));
                }
                qcomplex_t v1 = b;

                qfloat_t v0_abs = qc_abs(v0);
                qfloat_t vnrm2  = qf_add(qf_mul(v0_abs, v0_abs),
                                         qf_mul(b_abs,  b_abs));
                if (qf_lt(vnrm2, eps)) continue;
                qfloat_t inv = qf_div(QF_ONE, qf_sqrt(vnrm2));
                qcomplex_t u0 = qc_scale(v0, inv);
                qcomplex_t u1 = qc_scale(v1, inv);

                /* apply from left: H := (I - 2 u u*) H, rows k, k+1
                 * include column k-1 for k>0 to zero the bulge H[k+1,k-1] */
                size_t jstart = (k > 0) ? k - 1 : (size_t)0;
                for (size_t j = jstart; j < n; ++j) {
                    qcomplex_t hk0 = QCM(H, k, j, n);
                    qcomplex_t hk1 = QCM(H, k + 1, j, n);

                    qcomplex_t dot = qc_add(qc_mul(qc_conj(u0), hk0),
                                            qc_mul(qc_conj(u1), hk1));
                    dot = qc_add(dot, dot);

                    QCM(H, k, j, n) = qc_sub(hk0, qc_mul(u0, dot));
                    QCM(H, k + 1, j, n) = qc_sub(hk1, qc_mul(u1, dot));
                }

                /* apply from right: H := H (I - 2 u u*), cols k, k+1 */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t hik0 = QCM(H, i, k, n);
                    qcomplex_t hik1 = QCM(H, i, k + 1, n);

                    qcomplex_t dot = qc_add(qc_mul(hik0, u0),
                                            qc_mul(hik1, u1));
                    dot = qc_add(dot, dot);

                    QCM(H, i, k, n) = qc_sub(hik0, qc_mul(dot, qc_conj(u0)));
                    QCM(H, i, k + 1, n) = qc_sub(hik1, qc_mul(dot, qc_conj(u1)));
                }

                /* accumulate into Q: Q := Q (I - 2 u u*), cols k, k+1 */
                for (size_t i = 0; i < n; ++i) {
                    qcomplex_t qik0 = QCM(Q, i, k, n);
                    qcomplex_t qik1 = QCM(Q, i, k + 1, n);

                    qcomplex_t dot = qc_add(qc_mul(qik0, u0),
                                            qc_mul(qik1, u1));
                    dot = qc_add(dot, dot);

                    QCM(Q, i, k, n) = qc_sub(qik0, qc_mul(dot, qc_conj(u0)));
                    QCM(Q, i, k + 1, n) = qc_sub(qik1, qc_mul(dot, qc_conj(u1)));
                }
            }

            /* check for deflation again */
            hml = QCM(H, m, m - 1, n);
            if (qf_lt(qc_abs(hml), eps)) {
                QCM(H, m, m - 1, n) = zero;
                break;
            }
        }

        if (iter >= max_iter) {
            return -1;
        }

        m--;
    }

    return 0;
}

/* ============================================================
   Public Schur API
   ============================================================ */

int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out)
{
    qcomplex_t *Z = NULL;
    qcomplex_t *Q0 = NULL;
    size_t count = 0;

    if (!A || !out) return -1;
    if (A->rows != A->cols) return -2;

    /* Step 1: convert to qcomplex */
    Z = mat_load_qcomplex_array(A);
    if (!Z) return -3;
    count = A->rows * A->cols;
    Q0 = calloc(count ? count : 1u, sizeof(qcomplex_t));
    if (!Q0) {
        free(Z);
        return -3;
    }
    for (size_t i = 0; i < A->rows; ++i)
        QCM(Q0, i, i, A->rows) = QC_ONE;

    /* Step 2: Hessenberg reduction Z -> H, Q0 */
    if (hessenberg_reduce_qc(Z, Q0, A->rows) != 0) {
        free(Z);
        free(Q0);
        return -4;
    }

    /* Step 3: QR iteration on H (in Z), accumulate into Q0 */
    if (schur_qr_qc(Z, Q0, A->rows) != 0) {
        free(Z);
        free(Q0);
        return -5;
    }

    /* Z is now T (Schur form), Q0 is Q. Expose both through the generic
     * number_t layer so callers no longer depend on qcomplex matrix types. */
    out->Q = mat_qcomplex_array_to_number_dense(A->rows, A->rows, Q0);
    out->T = mat_qcomplex_array_to_number_upper(A->rows, Z);
    free(Z);
    free(Q0);
    if (!out->Q || !out->T) {
        mat_schur_factor_free(out);
        return -3;
    }
    return 0;
}

void mat_schur_factor_free(mat_schur_factor_t *S)
{
    if (!S) return;
    if (S->Q) mat_free(S->Q);
    if (S->T) mat_free(S->T);
    S->Q = S->T = NULL;
}
