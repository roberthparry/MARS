#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix.h"
#include "matrix_internal.h"
#include "number.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

enum { MATRIX_HERMITIAN_JACOBI_MAX_SWEEPS = 50 };

static double mat_numeric_epsilon_from_precision_bits(size_t precision_bits)
{
    if (precision_bits == 0u)
        precision_bits = 106u;
    if (precision_bits > 106u)
        precision_bits = 106u;

    return ldexp(1.0, 4 - (int)precision_bits);
}

static double mat_numeric_relative_epsilon(const matrix_t *A)
{
    size_t precision_bits = 106u;

    if (A && A->elem == &number_elem) {
        if (A->meta.numeric_inexact_count == 0u)
            return 0.0;
        precision_bits = mat_cached_numeric_precision_bits(A);
    }

    return mat_numeric_epsilon_from_precision_bits(precision_bits);
}

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
    number_t *Z = NULL, *Qn = NULL, *Rn = NULL;

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

    Z = calloc(z_count ? z_count : 1u, sizeof(number_t));
    Qn = calloc(q_count ? q_count : 1u, sizeof(number_t));
    Rn = calloc(r_count ? r_count : 1u, sizeof(number_t));
    if (!Z || !Qn || !Rn) {
        free(Z);
        free(Qn);
        free(Rn);
        return -3;
    }

    for (size_t i = 0; i < z_count; ++i)
        Z[i] = NUM_ZERO;
    for (size_t i = 0; i < q_count; ++i)
        Qn[i] = NUM_ZERO;
    for (size_t i = 0; i < r_count; ++i)
        Rn[i] = NUM_ZERO;

    for (size_t i = 0; i < m; ++i)
        for (size_t j = 0; j < n; ++j)
            Z[i * n + j] = mat_get_num(A, i, j);

    for (size_t j = 0; j < n; j++) {
        number_t *v = malloc(m * sizeof(number_t));
        size_t imax = (j < kdim) ? j : kdim;
        if (!v) {
            for (size_t idx = 0; idx < z_count; ++idx)
                num_destroy(&Z[idx]);
            for (size_t idx = 0; idx < q_count; ++idx)
                num_destroy(&Qn[idx]);
            for (size_t idx = 0; idx < r_count; ++idx)
                num_destroy(&Rn[idx]);
            free(Z);
            free(Qn);
            free(Rn);
            return -3;
        }

        for (size_t r = 0; r < m; ++r)
            v[r] = NUM_ZERO;
        for (size_t r = 0; r < m; r++)
            v[r] = num_clone(Z[r * n + j]);

        for (size_t i = 0; i < imax; i++) {
            number_t rij = NUM_ZERO;
            for (size_t r = 0; r < m; r++) {
                NUM_SCOPE(iter_scope);
                number_t qri_conj = num_conj(Qn[r * kdim + i]);
                number_t term = num_mul(qri_conj, v[r]);
                number_t next = num_add(rij, term);
                num_destroy(&rij);
                rij = num_scope_detach(next);
            }
            num_destroy(&Rn[i * n + j]);
            Rn[i * n + j] = num_clone(rij);
            for (size_t r = 0; r < m; r++) {
                NUM_SCOPE(iter_scope);
                number_t qri_rij = num_mul(Qn[r * kdim + i], rij);
                number_t next = num_sub(v[r], qri_rij);
                num_destroy(&v[r]);
                v[r] = num_scope_detach(next);
            }
            num_destroy(&rij);
        }

        if (j < kdim) {
            number_t norm2 = NUM_ZERO;
            for (size_t r = 0; r < m; r++) {
                NUM_SCOPE(iter_scope);
                number_t abs_vr = num_abs(v[r]);
                number_t abs2_vr = num_mul(abs_vr, abs_vr);
                number_t next = num_add(norm2, abs2_vr);
                num_destroy(&norm2);
                norm2 = num_scope_detach(next);
            }

            double norm2_d = num_to_double(norm2);
            if (norm2_d < 1e-300) {
                num_destroy(&Rn[j * n + j]);
                Rn[j * n + j] = NUM_ZERO;
                for (size_t r = 0; r < m; r++) {
                    num_destroy(&Qn[r * kdim + j]);
                    Qn[r * kdim + j] = NUM_ZERO;
                }
            } else {
                number_t rjj = num_sqrt(norm2);
                number_t inv_rjj = num_inv(rjj);
                num_destroy(&Rn[j * n + j]);
                Rn[j * n + j] = num_clone(rjj);
                for (size_t r = 0; r < m; r++) {
                    number_t qval = num_mul(v[r], inv_rjj);
                    num_destroy(&Qn[r * kdim + j]);
                    Qn[r * kdim + j] = qval;
                }
                num_destroy(&inv_rjj);
                num_destroy(&rjj);
            }
            num_destroy(&norm2);
        }

        for (size_t r = 0; r < m; ++r)
            num_destroy(&v[r]);
        free(v);
    }

    out->Q = mat_create_dense_with_elem(m, kdim, &number_elem);
    out->R = mat_create_upper_triangular_with_elem(kdim, n, &number_elem);
    if (out->Q && out->R) {
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < kdim; ++j) {
                mat_set_num_owned(out->Q, i, j, &Qn[i * kdim + j]);
            }
        }
        for (size_t i = 0; i < kdim; ++i) {
            for (size_t j = i; j < n; ++j) {
                mat_set_num_owned(out->R, i, j, &Rn[i * n + j]);
            }
        }
    }
    for (size_t idx = 0; idx < z_count; ++idx)
        num_destroy(&Z[idx]);
    for (size_t idx = 0; idx < q_count; ++idx)
        num_destroy(&Qn[idx]);
    for (size_t idx = 0; idx < r_count; ++idx)
        num_destroy(&Rn[idx]);
    free(Z);
    free(Qn);
    free(Rn);

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
    number_t *Z = NULL, *Ln = NULL;
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
    Z = calloc(count ? count : 1u, sizeof(number_t));
    Ln = calloc(count ? count : 1u, sizeof(number_t));
    if (!Z || !Ln) {
        free(Z);
        free(Ln);
        return -3;
    }

    for (size_t i = 0; i < count; ++i) {
        Z[i] = NUM_ZERO;
        Ln[i] = NUM_ZERO;
    }

    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            Z[i * n + j] = mat_get_num(A, i, j);

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j <= i; j++) {
            number_t sum = num_clone(Z[i * n + j]);

            for (size_t k = 0; k < j; k++) {
                NUM_SCOPE(iter_scope);
                number_t lik = Ln[i * n + k];
                number_t ljk_conj = num_conj(Ln[j * n + k]);
                number_t prod = num_mul(lik, ljk_conj);
                number_t next = num_sub(sum, prod);
                num_destroy(&sum);
                sum = num_scope_detach(next);
            }

            if (i == j) {
                NUM_SCOPE(branch_scope);
                number_t imag = num_imag_part(sum);
                number_t real = num_real_part(sum);
                double imag_abs = fabs(num_to_double(imag));
                double real_val = num_to_double(real);
                if (imag_abs > 1e-12 || real_val <= 0.0) {
                    num_destroy(&sum);
                    for (size_t idx = 0; idx < count; ++idx) {
                        num_destroy(&Z[idx]);
                        num_destroy(&Ln[idx]);
                    }
                    free(Z);
                    free(Ln);
                    return -4;
                }
                {
                    number_t root = num_sqrt(real);
                    num_destroy(&Ln[i * n + j]);
                    Ln[i * n + j] = num_scope_detach(root);
                }
            } else {
                NUM_SCOPE(branch_scope);
                number_t abs_ljj = num_abs(Ln[j * n + j]);
                double d = num_to_double(abs_ljj);
                if (d < 1e-300) {
                    num_destroy(&sum);
                    for (size_t idx = 0; idx < count; ++idx) {
                        num_destroy(&Z[idx]);
                        num_destroy(&Ln[idx]);
                    }
                    free(Z);
                    free(Ln);
                    return -4;
                }
                {
                    number_t ljj_conj = num_conj(Ln[j * n + j]);
                    number_t quot = num_div(sum, ljj_conj);
                    num_destroy(&Ln[i * n + j]);
                    Ln[i * n + j] = num_scope_detach(quot);
                }
            }
            num_destroy(&sum);
        }
    }

    out->L = mat_create_lower_triangular_with_elem(n, n, &number_elem);
    if (out->L) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j <= i; ++j) {
                mat_set_num_owned(out->L, i, j, &Ln[i * n + j]);
            }
        }
        if (lower_store != &lower_triangular_store) {
            matrix_t *converted = mat_copy_with_store(out->L, lower_store);
            mat_free(out->L);
            out->L = converted;
        }
    }
    for (size_t idx = 0; idx < count; ++idx) {
        num_destroy(&Z[idx]);
        num_destroy(&Ln[idx]);
    }
    free(Z);
    free(Ln);

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
    double dim = (double)((A->rows > A->cols) ? A->rows : A->cols);
    double tol = sigma_max * dim * mat_numeric_relative_epsilon(A);
    const double algo_floor = 1.0e4 * 2.2204460492503131e-16;

    if (tol < algo_floor)
        tol = algo_floor;

    return tol;
}

static matrix_t *mat_number_array_to_diagonal(size_t n, const number_t *data);

static bool mat_numeric_is_diagonal(const matrix_t *A)
{
    double tol;

    if (!A || !elem_supports_numeric_algorithms(A->elem))
        return false;
    if (mat_has_diagonal_structure(A))
        return true;
    if (A->rows != A->cols)
        return false;

    tol = mat_numeric_relative_epsilon(A);
    if (!(tol > 0.0))
        tol = 1e-30;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            NUM_SCOPE(scope);
            number_t value;
            number_t abs_value;
            bool is_zero;

            if (i == j)
                continue;

            value = mat_get_num(A, i, j);
            abs_value = num_abs(value);
            is_zero = num_to_double(abs_value) <= tol;
            if (!is_zero)
                return false;
        }
    }

    return true;
}

static int mat_norm_diagonal_exact(const matrix_t *A, mat_norm_type_t type, number_t *out)
{
    NUM_SCOPE(scope);
    size_t kdim;
    number_t best = NUM_ZERO;
    number_t sumsq = NUM_ZERO;
    bool have_best = false;

    if (!A || !out)
        return -1;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; ++i) {
        number_t value = mat_get_num(A, i, i);
        number_t abs_value = num_abs(value);
        number_t mag2 = num_mul(abs_value, abs_value);

        if (!have_best || num_cmp(abs_value, best) > 0) {
            num_destroy(&best);
            best = num_clone(abs_value);
            have_best = true;
        }

        {
            number_t next_sumsq = num_add(sumsq, mag2);
            num_destroy(&sumsq);
            sumsq = next_sumsq;
        }

        num_destroy(&mag2);
        num_destroy(&abs_value);
        num_destroy(&value);
    }

    switch (type) {
        case MAT_NORM_1:
        case MAT_NORM_INF:
        case MAT_NORM_2:
            *out = num_scope_detach(num_clone(best));
            break;
        case MAT_NORM_FRO:
            *out = num_scope_detach(num_sqrt(sumsq));
            break;
        default:
            num_destroy(&best);
            num_destroy(&sumsq);
            return -2;
    }

    num_destroy(&best);
    num_destroy(&sumsq);
    return 0;
}

static number_t mat_num_positive_ratio(number_t numer, number_t denom)
{
    NUM_SCOPE(scope);

    if (num_is_exact(numer) && num_is_exact(denom))
        return num_scope_detach(num_div(numer, denom));

    {
        number_t log_numer = num_log(numer);
        number_t log_denom = num_log(denom);
        number_t log_ratio = num_sub(log_numer, log_denom);

        return num_scope_detach(num_exp(log_ratio));
    }
}

static int mat_norm_via_svd(const matrix_t *A, double *out)
{
    mat_svd_factor_t svd = {0};
    size_t kdim;
    double best = 0.0;

    if (!A || !out)
        return -1;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        number_t sig = mat_get_num(svd.S, i, i);
        number_t abs_sig = num_abs(sig);
        double d = num_to_double(abs_sig);
        if (d > best)
            best = d;
        num_destroy(&abs_sig);
        num_destroy(&sig);
    }

    *out = best;
    mat_svd_factor_free(&svd);
    return 0;
}

int mat_svd_factor(const matrix_t *A, mat_svd_factor_t *out)
{
    size_t m, n, kdim;
    number_t *Z = NULL, *LeftN = NULL, *RightN = NULL;
    matrix_t *Gram = NULL;
    matrix_t *EigVecs = NULL;
    number_t *evals = NULL;
    number_t *sigma = NULL;
    number_t *sigma_sorted = NULL;
    size_t *order = NULL;
    int rc;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    m = A->rows;
    n = A->cols;
    kdim = (m < n) ? m : n;

    {
        size_t count = m * n;
        Z = calloc(count ? count : 1u, sizeof(number_t));
    }
    if (!Z)
        return -3;
    for (size_t i = 0; i < ((m != 0 && n != 0) ? (m * n) : 1u); ++i)
        Z[i] = NUM_ZERO;
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < n; ++j) {
            number_t v = mat_get_num(A, i, j);
            Z[i * n + j] = num_clone(v);
            num_destroy(&v);
        }
    }

    evals = calloc(kdim ? kdim : 1, sizeof(number_t));
    sigma = calloc(kdim ? kdim : 1, sizeof(number_t));
    sigma_sorted = calloc(kdim ? kdim : 1, sizeof(number_t));
    order = calloc(kdim ? kdim : 1, sizeof(size_t));
    if (!evals || !sigma || !sigma_sorted || !order) {
        rc = -3;
        goto fail;
    }
    for (size_t i = 0; i < (kdim ? kdim : 1); ++i)
        evals[i] = NUM_ZERO;
    for (size_t i = 0; i < (kdim ? kdim : 1); ++i)
        sigma[i] = NUM_ZERO;
    for (size_t i = 0; i < (kdim ? kdim : 1); ++i)
        sigma_sorted[i] = NUM_ZERO;

    if (m >= n) {
        Gram = mat_create_dense_with_elem(n, n, &number_elem);
        if (!Gram) {
            rc = -3;
            goto fail;
        }
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                number_t sum = NUM_ZERO;
                for (size_t r = 0; r < m; ++r) {
                    number_t a_conj = num_conj(Z[r * n + i]);
                    number_t prod = num_mul(a_conj, Z[r * n + j]);
                    number_t next = num_add(sum, prod);
                    num_destroy(&a_conj);
                    num_destroy(&prod);
                    num_destroy(&sum);
                    sum = next;
                }
                mat_set_num_owned(Gram, i, j, &sum);
            }
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < n; i++) {
            number_t re_num = num_real_part(evals[i]);
            order[i] = i;
            if (num_cmp(re_num, NUM_ZERO) > 0) {
                sigma[i] = num_sqrt(re_num);
            } else {
                sigma[i] = NUM_ZERO;
            }
            num_destroy(&re_num);
        }

        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++)
                if (num_cmp(sigma[order[j]], sigma[order[i]]) > 0) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        size_t right_count = n * kdim;
        size_t left_count = m * kdim;
        RightN = calloc(right_count ? right_count : 1u, sizeof(number_t));
        LeftN = calloc(left_count ? left_count : 1u, sizeof(number_t));
        if (!RightN || !LeftN) {
            rc = -3;
            goto fail;
        }
        for (size_t i = 0; i < (right_count ? right_count : 1u); ++i)
            RightN[i] = NUM_ZERO;
        for (size_t i = 0; i < (left_count ? left_count : 1u); ++i)
            LeftN[i] = NUM_ZERO;

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            number_t invsig = NUM_ZERO;
            bool sigma_is_zero = num_is_zero(sigma[idx]);

            if (!sigma_is_zero) {
                invsig = num_inv(sigma[idx]);
            }

            for (size_t r = 0; r < n; r++) {
                number_t v_num = mat_get_num(EigVecs, r, idx);
                RightN[r * kdim + j] = num_clone(v_num);
                num_destroy(&v_num);
            }

            for (size_t r = 0; r < m; r++) {
                number_t sum = NUM_ZERO;
                for (size_t c = 0; c < n; c++) {
                    number_t prod = num_mul(Z[r * n + c], RightN[c * kdim + j]);
                    number_t next = num_add(sum, prod);
                    num_destroy(&prod);
                    num_destroy(&sum);
                    sum = next;
                }
                if (!sigma_is_zero) {
                    number_t scaled = num_mul(sum, invsig);
                    num_destroy(&sum);
                    sum = scaled;
                }
                LeftN[r * kdim + j] = num_clone(sum);
                num_destroy(&sum);
            }
            num_destroy(&invsig);
        }
    } else {
        Gram = mat_create_dense_with_elem(m, m, &number_elem);
        if (!Gram) {
            rc = -3;
            goto fail;
        }
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < m; ++j) {
                number_t sum = NUM_ZERO;
                for (size_t c = 0; c < n; ++c) {
                    number_t b_conj = num_conj(Z[j * n + c]);
                    number_t prod = num_mul(Z[i * n + c], b_conj);
                    number_t next = num_add(sum, prod);
                    num_destroy(&b_conj);
                    num_destroy(&prod);
                    num_destroy(&sum);
                    sum = next;
                }
                mat_set_num_owned(Gram, i, j, &sum);
            }
        }
        rc = mat_eigendecompose(Gram, evals, &EigVecs);
        if (rc != 0)
            goto fail;

        for (size_t i = 0; i < m; i++) {
            number_t re_num = num_real_part(evals[i]);
            order[i] = i;
            if (num_cmp(re_num, NUM_ZERO) > 0) {
                sigma[i] = num_sqrt(re_num);
            } else {
                sigma[i] = NUM_ZERO;
            }
            num_destroy(&re_num);
        }

        for (size_t i = 0; i < m; i++)
            for (size_t j = i + 1; j < m; j++)
                if (num_cmp(sigma[order[j]], sigma[order[i]]) > 0) {
                    size_t tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

        size_t left_count = m * kdim;
        size_t right_count = n * kdim;
        LeftN = calloc(left_count ? left_count : 1u, sizeof(number_t));
        RightN = calloc(right_count ? right_count : 1u, sizeof(number_t));
        if (!RightN || !LeftN) {
            rc = -3;
            goto fail;
        }
        for (size_t i = 0; i < (left_count ? left_count : 1u); ++i)
            LeftN[i] = NUM_ZERO;
        for (size_t i = 0; i < (right_count ? right_count : 1u); ++i)
            RightN[i] = NUM_ZERO;

        for (size_t j = 0; j < kdim; j++) {
            size_t idx = order[j];
            number_t invsig = NUM_ZERO;
            bool sigma_is_zero = num_is_zero(sigma[idx]);

            if (!sigma_is_zero) {
                invsig = num_inv(sigma[idx]);
            }

            for (size_t r = 0; r < m; r++) {
                number_t u_num = mat_get_num(EigVecs, r, idx);
                LeftN[r * kdim + j] = num_clone(u_num);
                num_destroy(&u_num);
            }

            for (size_t r = 0; r < n; r++) {
                number_t sum = NUM_ZERO;
                for (size_t c = 0; c < m; c++) {
                    number_t z_conj = num_conj(Z[c * n + r]);
                    number_t prod = num_mul(z_conj, LeftN[c * kdim + j]);
                    number_t next = num_add(sum, prod);
                    num_destroy(&z_conj);
                    num_destroy(&prod);
                    num_destroy(&sum);
                    sum = next;
                }
                if (!sigma_is_zero) {
                    number_t scaled = num_mul(sum, invsig);
                    num_destroy(&sum);
                    sum = scaled;
                }
                RightN[r * kdim + j] = num_clone(sum);
                num_destroy(&sum);
            }
            num_destroy(&invsig);
        }
    }

    for (size_t j = 0; j < kdim; ++j) {
        sigma_sorted[j] = num_clone(sigma[order[j]]);
    }

    /*
     * Keep S construction separate from the sigma workspaces so singular
     * values are captured before the temporary buffers are torn down.
     */

    out->U = mat_create_dense_with_elem(m, kdim, &number_elem);
    out->S = mat_number_array_to_diagonal(kdim, sigma_sorted);
    out->V = mat_create_dense_with_elem(n, kdim, &number_elem);
    if (out->U) {
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < kdim; ++j) {
                mat_set_num_owned(out->U, i, j, &LeftN[i * kdim + j]);
            }
        }
    }
    if (out->V) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < kdim; ++j) {
                mat_set_num_owned(out->V, i, j, &RightN[i * kdim + j]);
            }
        }
    }
    if (!out->U || !out->S || !out->V) {
        rc = -3;
        goto fail;
    }

    rc = 0;

fail:
    if (Z) {
        for (size_t i = 0; i < ((m != 0 && n != 0) ? (m * n) : 1u); ++i)
            num_destroy(&Z[i]);
    }
    free(Z);
    mat_free(Gram);
    mat_free(EigVecs);
    if (LeftN) {
        size_t count = m * kdim;
        for (size_t i = 0; i < (count ? count : 1u); ++i)
            num_destroy(&LeftN[i]);
    }
    if (RightN) {
        size_t count = n * kdim;
        for (size_t i = 0; i < (count ? count : 1u); ++i)
            num_destroy(&RightN[i]);
    }
    free(LeftN);
    free(RightN);
    if (evals) {
        for (size_t i = 0; i < (kdim ? kdim : 1); i++)
            num_destroy(&evals[i]);
    }
    if (sigma) {
        for (size_t i = 0; i < (kdim ? kdim : 1); i++)
            num_destroy(&sigma[i]);
    }
    if (sigma_sorted) {
        for (size_t i = 0; i < (kdim ? kdim : 1); i++)
            num_destroy(&sigma_sorted[i]);
    }
    free(evals);
    free(sigma);
    free(sigma_sorted);
    free(order);
    if (rc != 0)
        mat_svd_factor_free(out);
    return rc;
}

typedef int (*mat_norm_function_t)(const matrix_t *A, double *out);

static int mat_norm_one(const matrix_t *A, double *out)
{
    double best = 0.0;
    for (size_t j = 0; j < A->cols; j++) {
        double sum = 0.0;
        for (size_t i = 0; i < A->rows; i++) {
            number_t value = mat_get_num(A, i, j);
            number_t mag = num_abs(value);
            sum += num_to_double(mag);
            num_destroy(&mag);
            num_destroy(&value);
        }
        if (sum > best)
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_infinity(const matrix_t *A, double *out)
{
    double best = 0.0;

    for (size_t i = 0; i < A->rows; i++) {
        double sum = 0.0;
        for (size_t j = 0; j < A->cols; j++) {
            number_t value = mat_get_num(A, i, j);
            number_t mag = num_abs(value);
            sum += num_to_double(mag);
            num_destroy(&mag);
            num_destroy(&value);
        }
        if (sum > best)
            best = sum;
    }
    *out = best;
    return 0;
}

static int mat_norm_frobenius(const matrix_t *A, double *out)
{
    double sumsq = 0.0;

    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            number_t value = mat_get_num(A, i, j);
            number_t mag = num_abs(value);
            double d = num_to_double(mag);
            sumsq += d * d;
            num_destroy(&mag);
            num_destroy(&value);
        }
    }
    *out = sqrt(sumsq);
    return 0;
}

static const mat_norm_function_t mat_norm_functions[] = {[MAT_NORM_1] = mat_norm_one,
                                                         [MAT_NORM_INF] = mat_norm_infinity,
                                                         [MAT_NORM_FRO] = mat_norm_frobenius,
                                                         [MAT_NORM_2] = mat_norm_via_svd};

static mat_norm_function_t mat_norm_function_for(mat_norm_type_t type)
{
    size_t index = (size_t)type;

    if (index >= sizeof(mat_norm_functions) / sizeof(mat_norm_functions[0]))
        return NULL;

    return mat_norm_functions[index];
}

int mat_norm(const matrix_t *A, mat_norm_type_t type, number_t *out)
{
    mat_norm_function_t fun;
    double dout;
    int rc;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    if (mat_numeric_is_diagonal(A))
        return mat_norm_diagonal_exact(A, type, out);

    fun = mat_norm_function_for(type);
    if (!fun)
        return -2;

    rc = fun(A, &dout);
    if (rc != 0)
        return rc;

    *out = num_create_from_double(dout);
    return 0;
}

int mat_condition_number(const matrix_t *A, mat_norm_type_t type, number_t *out)
{
    int rank;
    size_t kdim;

    if (!A || !out)
        return -1;
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    if (mat_numeric_is_diagonal(A)) {
        number_t sigma_max = NUM_ZERO;
        number_t sigma_min = NUM_ZERO;
        number_t sumsq = NUM_ZERO;
        number_t inv_sumsq = NUM_ZERO;
        bool have_sigma = false;
        bool need_fro = (type == MAT_NORM_FRO);

        for (size_t i = 0; i < kdim; ++i) {
            number_t value;
            number_t abs_value;

            value = mat_get_num(A, i, i);
            abs_value = num_abs(value);
            if (num_is_zero(abs_value)) {
                num_destroy(&abs_value);
                num_destroy(&value);
                num_destroy(&sigma_max);
                num_destroy(&sigma_min);
                num_destroy(&sumsq);
                num_destroy(&inv_sumsq);
                *out = num_create_from_double(INFINITY);
                return 0;
            }
            if (!have_sigma) {
                sigma_max = num_clone(abs_value);
                sigma_min = num_clone(abs_value);
                have_sigma = true;
            } else {
                if (num_cmp(abs_value, sigma_max) > 0) {
                    num_destroy(&sigma_max);
                    sigma_max = num_clone(abs_value);
                }
                if (num_cmp(abs_value, sigma_min) < 0) {
                    num_destroy(&sigma_min);
                    sigma_min = num_clone(abs_value);
                }
            }

            if (need_fro) {
                number_t mag2 = num_mul(abs_value, abs_value);
                number_t inv_mag2 = num_inv(mag2);
                number_t next_sumsq = num_add(sumsq, mag2);
                number_t next_inv_sumsq = num_add(inv_sumsq, inv_mag2);

                num_destroy(&sumsq);
                num_destroy(&inv_sumsq);
                sumsq = next_sumsq;
                inv_sumsq = next_inv_sumsq;

                num_destroy(&inv_mag2);
                num_destroy(&mag2);
            }

            num_destroy(&abs_value);
            num_destroy(&value);
        }

        switch (type) {
            case MAT_NORM_1:
            case MAT_NORM_INF:
            case MAT_NORM_2:
                *out = mat_num_positive_ratio(sigma_max, sigma_min);
                num_destroy(&sigma_max);
                num_destroy(&sigma_min);
                num_destroy(&sumsq);
                num_destroy(&inv_sumsq);
                return 0;
            case MAT_NORM_FRO: {
                number_t root_sumsq = num_sqrt(sumsq);
                number_t root_inv_sumsq = num_sqrt(inv_sumsq);
                *out = num_mul(root_sumsq, root_inv_sumsq);
                num_destroy(&root_sumsq);
                num_destroy(&root_inv_sumsq);
                num_destroy(&sigma_max);
                num_destroy(&sigma_min);
                num_destroy(&sumsq);
                num_destroy(&inv_sumsq);
                return 0;
            }
            default:
                num_destroy(&sigma_max);
                num_destroy(&sigma_min);
                num_destroy(&sumsq);
                num_destroy(&inv_sumsq);
                break;
        }
    }

    rank = mat_rank(A);
    if (rank < 0)
        return -2;

    if ((size_t)rank < kdim) {
        *out = num_create_from_double(INFINITY);
        return 0;
    }

    if (mat_norm_function_for(type) == mat_norm_via_svd) {
        mat_svd_factor_t svd = {0};
        number_t sigma_max = NUM_ZERO;
        number_t sigma_min = NUM_ZERO;
        bool have_sigma = false;
        if (mat_svd_factor(A, &svd) != 0)
            return -2;
        for (size_t i = 0; i < kdim; i++) {
            number_t value = mat_get_num(svd.S, i, i);
            number_t abs_value = num_abs(value);
            if (!have_sigma) {
                sigma_max = num_clone(abs_value);
                sigma_min = num_clone(abs_value);
                have_sigma = true;
            } else {
                if (num_cmp(abs_value, sigma_max) > 0) {
                    num_destroy(&sigma_max);
                    sigma_max = num_clone(abs_value);
                }
                if (num_cmp(abs_value, sigma_min) < 0) {
                    num_destroy(&sigma_min);
                    sigma_min = num_clone(abs_value);
                }
            }
            num_destroy(&abs_value);
            num_destroy(&value);
        }
        *out = mat_num_positive_ratio(sigma_max, sigma_min);
        num_destroy(&sigma_max);
        num_destroy(&sigma_min);
        mat_svd_factor_free(&svd);
        return 0;
    }

    number_t na, ni;
    number_t prod;
    matrix_t *Aw = NULL;
    matrix_t *Ai = NULL;
    int rc_a, rc_i;

    Aw = mat_convert_dense(A, &number_elem);
    if (!Aw)
        return -2;

    if (Aw->rows == Aw->cols) {
        Ai = mat_inverse(Aw);
    } else {
        Ai = mat_pseudoinverse(Aw);
    }

    if (!Ai)
        goto fail_non2;

    rc_a = mat_norm(Aw, type, &na);
    rc_i = mat_norm(Ai, type, &ni);
    mat_free(Ai);
    mat_free(Aw);
    if (rc_a != 0 || rc_i != 0) {
        num_destroy(&na);
        num_destroy(&ni);
        return -2;
    }
    prod = num_mul(na, ni);
    *out = prod;
    num_destroy(&na);
    num_destroy(&ni);
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
    if (A->elem == &expr_elem)
        return mat_rank_expr_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return -2;

    if (mat_svd_factor(A, &svd) != 0)
        return -2;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        number_t sig = mat_get_num(svd.S, i, i);
        number_t abs_sig = num_abs(sig);
        double d = num_to_double(abs_sig);
        if (d > sigma_max)
            sigma_max = d;
        num_destroy(&abs_sig);
        num_destroy(&sig);
    }

    tol = mat_singular_tolerance(A, sigma_max);
    for (size_t i = 0; i < kdim; i++) {
        number_t sig = mat_get_num(svd.S, i, i);
        number_t abs_sig = num_abs(sig);
        double d = num_to_double(abs_sig);
        if (d > tol)
            rank++;
        num_destroy(&abs_sig);
        num_destroy(&sig);
    }

    mat_svd_factor_free(&svd);
    return rank;
}

matrix_t *mat_pseudoinverse(const matrix_t *A)
{
    mat_svd_factor_t svd = {0};
    matrix_t *UH = NULL;
    matrix_t *Sp = NULL;
    matrix_t *VSp = NULL;
    matrix_t *Pinv = NULL;
    size_t kdim;
    double sigma_max = 0.0, tol;

    if (!A)
        return NULL;
    if (A->elem == &expr_elem)
        return mat_pseudoinverse_expr_exact(A);
    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    if (mat_svd_factor(A, &svd) != 0)
        return NULL;

    kdim = (A->rows < A->cols) ? A->rows : A->cols;
    for (size_t i = 0; i < kdim; i++) {
        number_t sig = mat_get_num(svd.S, i, i);
        number_t abs_sig = num_abs(sig);
        double d = num_to_double(abs_sig);
        if (d > sigma_max)
            sigma_max = d;
        num_destroy(&abs_sig);
        num_destroy(&sig);
    }
    tol = mat_singular_tolerance(A, sigma_max);

    Sp = mat_create_diagonal_with_elem(kdim, &number_elem);
    if (!Sp)
        goto fail;

    for (size_t i = 0; i < kdim; i++) {
        number_t sig;
        number_t abs_sig;
        number_t inv_sig;
        double d;
        sig = mat_get_num(svd.S, i, i);
        abs_sig = num_abs(sig);
        d = num_to_double(abs_sig);
        if (d > tol) {
            inv_sig = num_inv(sig);
            mat_set_num_owned(Sp, i, i, &inv_sig);
        } else {
            number_t zero = NUM_ZERO;
            mat_set_num_owned(Sp, i, i, &zero);
        }
        num_destroy(&abs_sig);
        num_destroy(&sig);
    }

    UH = mat_hermitian(svd.U);
    VSp = mat_mul(svd.V, Sp);
    Pinv = (VSp && UH) ? mat_mul(VSp, UH) : NULL;
    if (!UH || !VSp || !Pinv)
        goto fail;

fail:
    mat_svd_factor_free(&svd);
    mat_free(UH);
    mat_free(Sp);
    mat_free(VSp);
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
    if (A->elem == &expr_elem)
        return mat_nullspace_expr_exact(A);
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
        evals[i] = NUM_ZERO;

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
            mat_set_num_owned(N, r, col, &value);
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

static matrix_t *mat_shift_subtract_number(const matrix_t *A, const number_t *eigenvalue)
{
    matrix_t *Shifted = NULL;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    if (!elem_supports_numeric_algorithms(A->elem)) {
        mat_free(Shifted);
        return NULL;
    }

    for (size_t i = 0; i < A->rows; ++i) {
        number_t diag = mat_get_num(Shifted, i, i);
        number_t shifted = num_sub(diag, *eigenvalue);

        mat_set_num_owned(Shifted, i, i, &shifted);
        num_destroy(&diag);
    }

    return Shifted;
}

static matrix_t *mat_shift_subtract_expr(const matrix_t *A, const expr_t *eigenvalue)
{
    matrix_t *Shifted = NULL;

    if (!A || !eigenvalue || A->rows != A->cols || A->elem != &expr_elem)
        return NULL;

    Shifted = mat_copy_as_dense(A);
    if (!Shifted)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        expr_t *diag = NULL;
        expr_t *new_diag = NULL;

        mat_get(Shifted, i, i, &diag);
        if (!diag)
            diag = (expr_t *)EXPR_ZERO;

        expr_retain(diag);
        expr_retain((expr_t *)eigenvalue);
        new_diag = expr_sub_simplify_owned(diag, (expr_t *)eigenvalue);
        if (!new_diag) {
            mat_free(Shifted);
            return NULL;
        }
        mat_set(Shifted, i, i, &new_diag);
        expr_free(new_diag);
    }

    return Shifted;
}

static bool mat_column_is_structural_zero(const matrix_t *A, size_t col)
{
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
    size_t row_begin = 0;
    size_t row_end;

    if (!A || col >= A->cols)
        return true;
    if (A->nnz == 0)
        return true;

    if (A->store == &identity_store)
        return false; /* col is in-range and identity has one on (col,col). */

    if (A->store == &diagonal_store) {
        if (col >= A->rows)
            return true;
        mat_get(A, col, col, raw);
        return elem_is_structural_zero(A->elem, raw);
    }

    if (A->store == &upper_triangular_store) {
        /* Only rows [0, min(col, rows-1)] can hold non-zeros in this column. */
        row_end = (col + 1 < A->rows) ? (col + 1) : A->rows;
    } else if (A->store == &lower_triangular_store) {
        /* Only rows [col, rows) can hold non-zeros in this column. */
        if (col >= A->rows)
            return true;
        row_begin = col;
        row_end = A->rows;
    } else {
        row_end = A->rows;
    }

    for (size_t i = row_begin; i < row_end; ++i) {
        mat_get(A, i, col, raw);
        if (!elem_is_structural_zero(A->elem, raw))
            return false;
    }

    return true;
}

static matrix_t *mat_extract_column_copy(const matrix_t *A, size_t col)
{
    matrix_t *C;
    unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];

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

matrix_t *mat_eigenspace(const matrix_t *A, const number_t *eigenvalue)
{
    matrix_t *Shifted = NULL;
    matrix_t *E = NULL;

    Shifted = mat_shift_subtract_number(A, eigenvalue);
    if (!Shifted)
        return NULL;

    E = mat_nullspace(Shifted);
    mat_free(Shifted);
    return E;
}

matrix_t *mat_eigenspace_expr(const matrix_t *A, const expr_t *eigenvalue)
{
    matrix_t *Shifted = NULL;
    matrix_t *E = NULL;

    Shifted = mat_shift_subtract_expr(A, eigenvalue);
    if (!Shifted)
        return NULL;

    E = mat_nullspace(Shifted);
    mat_free(Shifted);
    return E;
}

matrix_t *mat_generalized_eigenspace(const matrix_t *A, const number_t *eigenvalue, size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *G = NULL;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    if (order == 1)
        return mat_eigenspace(A, eigenvalue);

    Shifted = mat_shift_subtract_number(A, eigenvalue);
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

matrix_t *mat_generalized_eigenspace_expr(const matrix_t *A, const expr_t *eigenvalue, size_t order)
{
    matrix_t *Shifted = NULL;
    matrix_t *Power = NULL;
    matrix_t *G = NULL;

    if (!A || !eigenvalue || A->rows != A->cols || order == 0)
        return NULL;

    if (order == 1)
        return mat_eigenspace_expr(A, eigenvalue);

    Shifted = mat_shift_subtract_expr(A, eigenvalue);
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

matrix_t *mat_jordan_chain(const matrix_t *A, const number_t *eigenvalue, size_t order)
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

    Shifted = mat_shift_subtract_number(A, eigenvalue);
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
        unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];

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

matrix_t *mat_jordan_chain_expr(const matrix_t *A, const expr_t *eigenvalue, size_t order)
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

    Shifted = mat_shift_subtract_expr(A, eigenvalue);
    if (!Shifted)
        return NULL;

    G = mat_generalized_eigenspace_expr(A, eigenvalue, order);
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
        unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];

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

matrix_t *mat_jordan_profile(const matrix_t *A, const number_t *eigenvalue)
{
    size_t *dims = NULL;
    matrix_t *G = NULL;
    matrix_t *P = NULL;
    size_t n;
    size_t blocks = 0;
    size_t out = 0;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;
    if (!elem_supports_numeric_algorithms(A->elem))
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
    P = mat_create_dense_with_elem(blocks, 1, &number_elem);
    if (!P)
        goto fail;

    for (size_t k = n; k >= 1; --k) {
        size_t at_least_k = dims[k] - dims[k - 1];
        size_t at_least_next = (k < n) ? (dims[k + 1] - dims[k]) : 0;
        size_t exact_k = at_least_k - at_least_next;

        for (size_t c = 0; c < exact_k; ++c) {
            number_t block_size = num_create_from_long((long)k);

            if (out >= blocks) {
                num_destroy(&block_size);
                goto fail;
            }
            mat_set_num_owned(P, out++, 0, &block_size);
        }

        if (k == 1)
            break;
    }

    free(dims);
    return P;

fail:
    free(dims);
    mat_free(P);
    return NULL;
}

matrix_t *mat_jordan_profile_expr(const matrix_t *A, const expr_t *eigenvalue)
{
    size_t *dims = NULL;
    matrix_t *G = NULL;
    matrix_t *P = NULL;
    size_t n;
    size_t blocks = 0;
    size_t out = 0;

    if (!A || !eigenvalue || A->rows != A->cols)
        return NULL;
    if (A->elem != &expr_elem)
        return NULL;

    n = A->rows;
    dims = calloc(n + 1, sizeof(*dims));
    if (!dims)
        return NULL;

    for (size_t k = 1; k <= n; ++k) {
        G = mat_generalized_eigenspace_expr(A, eigenvalue, k);
        if (!G)
            goto fail;
        dims[k] = G->cols;
        mat_free(G);
        G = NULL;
        if (dims[k] < dims[k - 1])
            goto fail;
    }

    blocks = dims[1];
    P = mat_create_dense_with_elem(blocks, 1, &number_elem);
    if (!P)
        goto fail;

    for (size_t k = n; k >= 1; --k) {
        size_t at_least_k = dims[k] - dims[k - 1];
        size_t at_least_next = (k < n) ? (dims[k + 1] - dims[k]) : 0;
        size_t exact_k = at_least_k - at_least_next;
        number_t block_size = num_create_from_long((long)k);

        for (size_t c = 0; c < exact_k; ++c) {
            if (out >= blocks) {
                num_destroy(&block_size);
                goto fail;
            }
            mat_set_num_clone(P, out, 0, &block_size);
            out++;
        }
        num_destroy(&block_size);

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

typedef struct {
    number_t *W;
    number_t *V;
    size_t n;
} hermitian_jacobi_ws_t;

typedef struct {
    number_t *H;
    number_t *Q;
    size_t n;
} schur_workspace_t;

static double offdiag_norm2(const number_t *A, size_t n)
{
    double s = 0.0;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            number_t aij_abs, aij_abs2;
            if (i == j)
                continue;
            aij_abs = num_abs(A[i * n + j]);
            aij_abs2 = num_mul(aij_abs, aij_abs);
            s += num_to_double(aij_abs2);
            num_destroy(&aij_abs2);
            num_destroy(&aij_abs);
        }
    }
    return s;
}

static void jacobi_apply(number_t *A, number_t *V, size_t n, size_t p, size_t q)
{
    number_t a_pq = num_clone(A[p * n + q]);
    number_t b = num_abs(a_pq);
    double b_d = num_to_double(b);

    if (b_d < 1e-150) {
        num_destroy(&b);
        num_destroy(&a_pq);
        return;
    }

    number_t one = num_create_from_long(1);
    number_t two = num_create_from_long(2);
    number_t app = num_real_part(A[p * n + p]);
    number_t aqq = num_real_part(A[q * n + q]);
    number_t diff = num_sub(aqq, app);
    number_t two_b = num_mul(two, b);
    number_t tau = num_div(diff, two_b);
    number_t sign = num_create_from_long((num_to_double(tau) >= 0.0) ? 1 : -1);
    number_t tau_abs = num_abs(tau);
    number_t tau2 = num_mul(tau, tau);
    number_t one_plus_tau2 = num_add(one, tau2);
    number_t root = num_sqrt(one_plus_tau2);
    number_t denom = num_add(tau_abs, root);
    number_t t = num_div(sign, denom);
    number_t t2 = num_mul(t, t);
    number_t one_plus_t2 = num_add(one, t2);
    number_t root_t = num_sqrt(one_plus_t2);
    number_t c = num_div(one, root_t);
    number_t s = num_mul(t, c);
    number_t ns = num_neg(s);
    number_t inv_b = num_inv(b);
    number_t ph = num_mul(a_pq, inv_b);
    number_t ph_c = num_conj(ph);
    number_t cph = num_mul(ph, c);
    number_t cph_c = num_mul(ph_c, c);
    number_t sph = num_mul(ph, s);
    number_t sph_c = num_mul(ph_c, s);

    for (size_t i = 0; i < n; i++) {
        number_t t1p = num_mul(cph, A[i * n + p]);
        number_t t2p = num_mul(A[i * n + q], ns);
        number_t out_p = num_add(t1p, t2p);
        number_t t1q = num_mul(sph, A[i * n + p]);
        number_t t2q = num_mul(A[i * n + q], c);
        number_t out_q = num_add(t1q, t2q);
        num_destroy(&t1p);
        num_destroy(&t2p);
        num_destroy(&t1q);
        num_destroy(&t2q);
        num_destroy(&A[i * n + p]);
        num_destroy(&A[i * n + q]);
        A[i * n + p] = out_p;
        A[i * n + q] = out_q;
    }

    for (size_t k = 0; k < n; k++) {
        number_t t1p = num_mul(cph_c, A[p * n + k]);
        number_t t2p = num_mul(A[q * n + k], ns);
        number_t out_p = num_add(t1p, t2p);
        number_t t1q = num_mul(sph_c, A[p * n + k]);
        number_t t2q = num_mul(A[q * n + k], c);
        number_t out_q = num_add(t1q, t2q);
        num_destroy(&t1p);
        num_destroy(&t2p);
        num_destroy(&t1q);
        num_destroy(&t2q);
        num_destroy(&A[p * n + k]);
        num_destroy(&A[q * n + k]);
        A[p * n + k] = out_p;
        A[q * n + k] = out_q;
    }

    for (size_t i = 0; i < n; i++) {
        number_t t1p = num_mul(cph, V[i * n + p]);
        number_t t2p = num_mul(V[i * n + q], ns);
        number_t out_p = num_add(t1p, t2p);
        number_t t1q = num_mul(sph, V[i * n + p]);
        number_t t2q = num_mul(V[i * n + q], c);
        number_t out_q = num_add(t1q, t2q);
        num_destroy(&t1p);
        num_destroy(&t2p);
        num_destroy(&t1q);
        num_destroy(&t2q);
        num_destroy(&V[i * n + p]);
        num_destroy(&V[i * n + q]);
        V[i * n + p] = out_p;
        V[i * n + q] = out_q;
    }

    num_destroy(&sph_c);
    num_destroy(&sph);
    num_destroy(&cph_c);
    num_destroy(&cph);
    num_destroy(&ph_c);
    num_destroy(&ph);
    num_destroy(&inv_b);
    num_destroy(&ns);
    num_destroy(&s);
    num_destroy(&c);
    num_destroy(&root_t);
    num_destroy(&one_plus_t2);
    num_destroy(&t2);
    num_destroy(&t);
    num_destroy(&denom);
    num_destroy(&root);
    num_destroy(&one_plus_tau2);
    num_destroy(&tau2);
    num_destroy(&tau_abs);
    num_destroy(&sign);
    num_destroy(&tau);
    num_destroy(&two_b);
    num_destroy(&diff);
    num_destroy(&aqq);
    num_destroy(&app);
    num_destroy(&two);
    num_destroy(&one);
    num_destroy(&b);
    num_destroy(&a_pq);
}

static int hermitian_jacobi_ws_init(hermitian_jacobi_ws_t *ws, const matrix_t *A)
{
    size_t n;
    size_t count;

    if (!ws || !A)
        return -1;

    n = A->rows;
    count = n * n;
    ws->n = n;
    ws->W = calloc(count ? count : 1u, sizeof(number_t));
    ws->V = calloc(count ? count : 1u, sizeof(number_t));
    if (!ws->W || !ws->V) {
        free(ws->W);
        free(ws->V);
        ws->W = NULL;
        ws->V = NULL;
        return -3;
    }

    for (size_t i = 0; i < count; i++) {
        ws->W[i] = NUM_ZERO;
        ws->V[i] = NUM_ZERO;
    }

    for (size_t i = 0; i < n; i++) {
        num_destroy(&ws->V[i * n + i]);
        ws->V[i * n + i] = num_create_from_long(1);
        for (size_t j = 0; j < n; j++) {
            number_t v = mat_get_num(A, i, j);
            num_destroy(&ws->W[i * n + j]);
            ws->W[i * n + j] = num_clone(v);
            num_destroy(&v);
        }
    }

    return 0;
}

static void hermitian_jacobi_ws_free(hermitian_jacobi_ws_t *ws)
{
    if (!ws)
        return;
    for (size_t i = 0; i < ws->n * ws->n; ++i) {
        num_destroy(&ws->W[i]);
        num_destroy(&ws->V[i]);
    }
    free(ws->W);
    free(ws->V);
    ws->W = NULL;
    ws->V = NULL;
    ws->n = 0u;
}

static void hermitian_jacobi_ws_run(hermitian_jacobi_ws_t *ws)
{
    size_t n;
    double fro2 = 0.0;
    double tol;

    if (!ws || !ws->W || !ws->V)
        return;

    n = ws->n;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            number_t aij_abs = num_abs(ws->W[i * n + j]);
            number_t aij_abs2 = num_mul(aij_abs, aij_abs);
            fro2 += num_to_double(aij_abs2);
            num_destroy(&aij_abs2);
            num_destroy(&aij_abs);
        }
    }
    tol = fro2 * 1e-29;

    for (int sweep = 0; sweep < MATRIX_HERMITIAN_JACOBI_MAX_SWEEPS; sweep++) {
        for (size_t p = 0; p < n - 1; p++)
            for (size_t q = p + 1; q < n; q++)
                jacobi_apply(ws->W, ws->V, n, p, q);
        if (offdiag_norm2(ws->W, n) < tol)
            break;
    }
}

static int schur_workspace_init_from_matrix(schur_workspace_t *ws, const matrix_t *A)
{
    size_t count;

    if (!ws || !A || A->rows != A->cols)
        return -1;

    ws->n = A->rows;
    count = ws->n * ws->n;
    ws->H = calloc(count ? count : 1u, sizeof(number_t));
    ws->Q = calloc(count ? count : 1u, sizeof(number_t));
    if (!ws->H || !ws->Q) {
        free(ws->H);
        free(ws->Q);
        ws->H = NULL;
        ws->Q = NULL;
        ws->n = 0u;
        return -3;
    }

    for (size_t i = 0; i < ws->n; ++i) {
        for (size_t j = 0; j < ws->n; ++j) {
            number_t v = mat_get_num(A, i, j);
            ws->H[i * ws->n + j] = num_clone(v);
            num_destroy(&v);
        }
    }
    for (size_t i = 0; i < count; ++i)
        ws->Q[i] = NUM_ZERO;
    for (size_t i = 0; i < ws->n; ++i) {
        num_destroy(&ws->Q[i * ws->n + i]);
        ws->Q[i * ws->n + i] = num_clone(NUM_ONE);
    }

    return 0;
}

static void schur_workspace_free(schur_workspace_t *ws)
{
    if (!ws)
        return;
    if (ws->H) {
        for (size_t i = 0; i < ws->n * ws->n; ++i)
            num_destroy(&ws->H[i]);
    }
    if (ws->Q) {
        for (size_t i = 0; i < ws->n * ws->n; ++i)
            num_destroy(&ws->Q[i]);
    }
    free(ws->H);
    free(ws->Q);
    ws->H = NULL;
    ws->Q = NULL;
    ws->n = 0u;
}

static int mat_eigendecompose_hermitian_2x2(const matrix_t *A, number_t *eigenvalues, matrix_t **eigenvectors)
{
    number_t a = mat_get_num(A, 0, 0);
    number_t b = mat_get_num(A, 0, 1);
    number_t d = mat_get_num(A, 1, 1);
    number_t tr = num_add(a, d);
    number_t diff = num_sub(a, d);
    number_t diff2 = num_mul(diff, diff);
    number_t b_conj = num_conj(b);
    number_t b_norm2 = num_mul(b_conj, b);
    number_t b_abs2 = num_real_part(b_norm2);
    number_t four_b_abs2 = num_add(b_abs2, b_abs2);
    number_t four_b_abs2_twice = num_add(four_b_abs2, four_b_abs2);
    number_t disc = num_add(diff2, four_b_abs2_twice);
    number_t root = num_sqrt(disc);
    number_t tr_minus_root = num_sub(tr, root);
    number_t tr_plus_root = num_add(tr, root);
    number_t lam0 = num_mul(NUM_HALF, tr_minus_root);
    number_t lam1 = num_mul(NUM_HALF, tr_plus_root);

    if (eigenvalues) {
        number_t *ev = eigenvalues;
        ev[0] = num_real_part(lam0);
        ev[1] = num_real_part(lam1);
    }

    if (eigenvectors) {
        matrix_t *V = mat_create_dense_with_elem(2, 2, &number_elem);
        if (!V) {
            num_destroy(&lam1);
            num_destroy(&lam0);
            num_destroy(&tr_plus_root);
            num_destroy(&tr_minus_root);
            num_destroy(&root);
            num_destroy(&disc);
            num_destroy(&four_b_abs2_twice);
            num_destroy(&four_b_abs2);
            num_destroy(&b_abs2);
            num_destroy(&b_norm2);
            num_destroy(&b_conj);
            num_destroy(&diff2);
            num_destroy(&diff);
            num_destroy(&tr);
            num_destroy(&d);
            num_destroy(&b);
            num_destroy(&a);
            return -3;
        }

        for (size_t col = 0; col < 2; ++col) {
            number_t lambda = (col == 0) ? num_clone(lam0) : num_clone(lam1);
            number_t v0;
            number_t v1;

            if (num_is_zero(b)) {
                number_t a_re = num_real_part(a);
                number_t d_re = num_real_part(d);
                int cmp = num_cmp(a_re, d_re);

                if (cmp == 0) {
                    if (col == 0) {
                        v0 = num_clone(NUM_ONE);
                        v1 = num_clone(NUM_ZERO);
                    } else {
                        v0 = num_clone(NUM_ZERO);
                        v1 = num_clone(NUM_ONE);
                    }
                } else if ((cmp < 0 && col == 0) || (cmp > 0 && col == 1)) {
                    v0 = num_clone(NUM_ONE);
                    v1 = num_clone(NUM_ZERO);
                } else {
                    v0 = num_clone(NUM_ZERO);
                    v1 = num_clone(NUM_ONE);
                }

                num_destroy(&d_re);
                num_destroy(&a_re);
            } else {
                v0 = num_clone(b);
                v1 = num_sub(lambda, a);
            }

            {
                number_t v0_abs = num_abs(v0);
                number_t v1_abs = num_abs(v1);
                number_t v0_abs2 = num_mul(v0_abs, v0_abs);
                number_t v1_abs2 = num_mul(v1_abs, v1_abs);
                number_t norm2 = num_add(v0_abs2, v1_abs2);
                number_t norm = num_sqrt(norm2);

                if (!num_is_zero(norm)) {
                    number_t inv_norm = num_div(NUM_ONE, norm);
                    number_t nv0 = num_mul(v0, inv_norm);
                    number_t nv1 = num_mul(v1, inv_norm);
                    num_destroy(&v0);
                    num_destroy(&v1);
                    v0 = nv0;
                    v1 = nv1;
                    num_destroy(&inv_norm);
                }

                mat_set_num_owned(V, 0, col, &v0);
                mat_set_num_owned(V, 1, col, &v1);

                num_destroy(&norm);
                num_destroy(&norm2);
                num_destroy(&v1_abs2);
                num_destroy(&v0_abs2);
                num_destroy(&v1_abs);
                num_destroy(&v0_abs);
            }

            num_destroy(&v1);
            num_destroy(&v0);
            num_destroy(&lambda);
        }

        *eigenvectors = V;
    }

    num_destroy(&lam1);
    num_destroy(&lam0);
    num_destroy(&tr_plus_root);
    num_destroy(&tr_minus_root);
    num_destroy(&root);
    num_destroy(&disc);
    num_destroy(&four_b_abs2_twice);
    num_destroy(&four_b_abs2);
    num_destroy(&b_abs2);
    num_destroy(&b_norm2);
    num_destroy(&b_conj);
    num_destroy(&diff2);
    num_destroy(&diff);
    num_destroy(&tr);
    num_destroy(&d);
    num_destroy(&b);
    num_destroy(&a);
    return 0;
}

/* Zero A[p][q] with a complex Givens rotation; accumulate into V.
 *
 * Handles the Hermitian case: the rotation has a real magnitude (c, s)
 * and a complex phase derived from A[p][q] itself.
 *
 * After this call: (J† A J)[p][q] == 0,  V_new = V J.
 */
static int mat_eigendecompose_hermitian(const matrix_t *A, number_t *eigenvalues, matrix_t **eigenvectors)
{
    size_t n = A->rows;
    hermitian_jacobi_ws_t ws = {0};
    matrix_t *Vnum = NULL;

    if (n == 2)
        return mat_eigendecompose_hermitian_2x2(A, eigenvalues, eigenvectors);

    if (hermitian_jacobi_ws_init(&ws, A) != 0) {
        hermitian_jacobi_ws_free(&ws);
        return -3;
    }
    hermitian_jacobi_ws_run(&ws);

    if (eigenvalues) {
        number_t *ev = eigenvalues;
        for (size_t i = 0; i < n; i++)
            ev[i] = num_real_part(ws.W[i * n + i]);
    }

    if (eigenvectors) {
        Vnum = mat_create_dense_with_elem(n, n, &number_elem);
        if (!Vnum) {
            hermitian_jacobi_ws_free(&ws);
            return -3;
        }
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                mat_set_num_clone(Vnum, i, j, &ws.V[i * n + j]);
            }
        }
        *eigenvectors = Vnum;
    }

    hermitian_jacobi_ws_free(&ws);
    return 0;
}

static double mat_numeric_algo_epsilon(const matrix_t *A)
{
    double eps = mat_numeric_relative_epsilon(A);
    if (!(eps > 0.0))
        eps = 1e-30;
    if (eps < 1e-36)
        eps = 1e-36;
    if (eps > 1e-12)
        eps = 1e-12;
    return eps * 8.0;
}

/* ============================================================
   General QR eigensolver — Francis implicit single-shift
   All internal arithmetic uses the matrix complex scalar backend.
   ============================================================ */

/* Index into flat n×n qcomplex array */
#define QCM(a, i, j, n) ((a)[(size_t)(i) * (n) + (size_t)(j)])
#define NCM(a, i, j, n) ((a)[(size_t)(i) * (n) + (size_t)(j)])

static void mat_num_assign(number_t *slot, number_t value)
{
    num_destroy(slot);
    *slot = value;
}

static number_t mat_num_abs2(const number_t value)
{
    number_t mag = num_abs(value);
    number_t mag2 = num_mul(mag, mag);
    num_destroy(&mag);
    return mag2;
}

static double mat_elem_abs2_double(const struct elem_vtable *elem, const void *raw)
{
    number_t value = mat_raw_value_to_number(elem, raw);
    number_t abs2 = mat_num_abs2(value);
    double out = num_to_double(abs2);

    num_destroy(&value);
    num_destroy(&abs2);
    return out;
}

static int hessenberg_reduce(number_t *Hn, number_t *Qn, size_t n);
static int schur_qr(number_t *Hn, number_t *Qn, size_t n, double eps);

/* Detect whether A is Hermitian: A[i,j] == conj(A[j,i]) within tolerance */
bool mat_is_hermitian(const matrix_t *A)
{
    if (!A || A->rows != A->cols)
        return false;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;
    unsigned char aij[MATRIX_SCALAR_STORAGE_BYTES], aji[MATRIX_SCALAR_STORAGE_BYTES], cji[MATRIX_SCALAR_STORAGE_BYTES],
        diff[MATRIX_SCALAR_STORAGE_BYTES], diag[MATRIX_SCALAR_STORAGE_BYTES];
    double rel_eps = mat_numeric_relative_epsilon(A);
    if (!(rel_eps > 0.0))
        rel_eps = 1e-30;
    rel_eps *= 8.0;
    double rel_eps2 = rel_eps * rel_eps;

    for (size_t i = 0; i < n; i++) {
        elem_init_zero_value(e, cji);
        elem_init_zero_value(e, diff);
        mat_get(A, i, i, diag);
        e->conj_elem(cji, diag);
        e->sub(diff, diag, cji);
        {
            double tol2 = (mat_elem_abs2_double(e, diag) + 1.0) * rel_eps2;
            if (mat_elem_abs2_double(e, diff) > tol2) {
                elem_destroy_value(e, diff);
                elem_destroy_value(e, cji);
                return false;
            }
        }
        elem_destroy_value(e, diff);
        elem_destroy_value(e, cji);
    }
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            double scale;
            double tol2;

            elem_init_zero_value(e, cji);
            elem_init_zero_value(e, diff);
            mat_get(A, i, j, aij);
            mat_get(A, j, i, aji);
            e->conj_elem(cji, aji);
            e->sub(diff, aij, cji);

            scale = mat_elem_abs2_double(e, aij) + mat_elem_abs2_double(e, aji) + 1.0;
            tol2 = scale * rel_eps2;
            if (mat_elem_abs2_double(e, diff) > tol2) {
                elem_destroy_value(e, diff);
                elem_destroy_value(e, cji);
                return false;
            }
            elem_destroy_value(e, diff);
            elem_destroy_value(e, cji);
        }
    return true;
}

/* General eigensolver helpers use the shared Hessenberg + Schur QR path
 * defined later in this file. */

/* Back-substitution: given upper-triangular T and eigenvalue T[k,k],
 * compute the k-th eigenvector by back-solving (T - lambda I) x = -e_k
 * for x[0..k-1] (x[k]=1 by convention). */
static void backsub_eigenvec(const number_t *T, number_t *Y, size_t n, size_t k, double eps)
{
    number_t lambda = num_clone(NCM(T, k, k, n));
    /* Y[:,k] will be the eigenvector; zero it first */
    for (size_t i = 0; i < n; i++) {
        num_destroy(&QCM(Y, i, k, n));
        QCM(Y, i, k, n) = num_clone(NUM_ZERO);
    }
    num_destroy(&QCM(Y, k, k, n));
    QCM(Y, k, k, n) = num_clone(NUM_ONE);

    /* Back-substitute rows k-1 down to 0 */
    for (size_t i = k; i-- > 0;) {
        /* (T[i,i] - lambda) y[i] = -sum_{j=i+1}^{k} T[i,j] y[j] */
        number_t rhs = num_clone(NUM_ZERO);
        for (size_t j = i + 1; j <= k; j++) {
            number_t tij = num_clone(NCM(T, i, j, n));
            number_t yjk = num_clone(QCM(Y, j, k, n));
            number_t prod = num_mul(tij, yjk);
            number_t next_rhs = num_sub(rhs, prod);
            num_destroy(&rhs);
            rhs = next_rhs;
            num_destroy(&prod);
            num_destroy(&yjk);
            num_destroy(&tij);
        }
        number_t tii = num_clone(NCM(T, i, i, n));
        number_t diag = num_sub(tii, lambda);
        number_t diag_abs = num_abs(diag);
        double dabs = num_to_double(diag_abs);
        num_destroy(&diag_abs);
        num_destroy(&QCM(Y, i, k, n));
        if (dabs < eps * eps) {
            QCM(Y, i, k, n) = num_clone(NUM_ZERO);
        } else {
            QCM(Y, i, k, n) = num_div(rhs, diag);
        }
        num_destroy(&diag);
        num_destroy(&tii);
        num_destroy(&rhs);
    }

    /* Normalise */
    double nrm2 = 0.0;
    for (size_t i = 0; i <= k; i++) {
        number_t a = num_abs(QCM(Y, i, k, n));
        number_t a2 = num_mul(a, a);
        nrm2 += num_to_double(a2);
        num_destroy(&a2);
        num_destroy(&a);
    }
    if (nrm2 > eps * eps) {
        double inv = 1.0 / sqrt(nrm2);
        number_t inv_num = num_create_from_double(inv);
        for (size_t i = 0; i <= k; i++) {
            number_t scaled = num_mul(inv_num, QCM(Y, i, k, n));
            num_destroy(&QCM(Y, i, k, n));
            QCM(Y, i, k, n) = scaled;
        }
        num_destroy(&inv_num);
    }
    num_destroy(&lambda);
}

/* ============================================================
   Debug printing

   ============================================================ */

/* ---------- small helpers on qcomplex ---------- */

static matrix_t *mat_number_array_to_diagonal(size_t n, const number_t *data)
{
    matrix_t *A = mat_create_diagonal_with_elem(n, &number_elem);

    if (!A)
        return NULL;

    for (size_t i = 0; i < n; ++i)
        mat_set(A, i, i, &data[i]);

    return A;
}

/* ============================================================
   Number-backed Hessenberg reduction and QR kernels.
   ============================================================ */
static int hessenberg_reduce(number_t *Hn, number_t *Qn, size_t n)
{
    if (!Hn || !Qn)
        return -1;

    if (n <= 2)
        return 0;

    for (size_t k = 0; k + 2 < n; ++k) {
        size_t m = n - (k + 1);
        number_t *v = calloc(m ? m : 1u, sizeof(*v));

        if (!v)
            return -1;

        for (size_t i = 0; i < m; ++i)
            v[i] = NUM_ZERO;

        {
            number_t x0 = num_clone(NCM(Hn, k + 1, k, n));
            number_t sigma = num_clone(NUM_ZERO);

            for (size_t i = 1; i < m; ++i) {
                number_t term = mat_num_abs2(NCM(Hn, k + 1 + i, k, n));
                number_t next_sigma = num_add(sigma, term);
                num_destroy(&sigma);
                sigma = next_sigma;
                num_destroy(&term);
            }

            if (!num_is_zero(sigma)) {
                number_t x0_abs = num_abs(x0);
                number_t x0_abs2 = num_mul(x0_abs, x0_abs);
                number_t mu2 = num_add(x0_abs2, sigma);
                number_t mu = num_sqrt(mu2);
                number_t v0;

                if (num_is_zero(x0_abs)) {
                    v0 = num_clone(mu);
                } else {
                    number_t phase = num_div(x0, x0_abs);
                    number_t phase_mu = num_mul(phase, mu);
                    v0 = num_add(x0, phase_mu);
                    num_destroy(&phase_mu);
                    num_destroy(&phase);
                }

                mat_num_assign(&v[0], v0);
                for (size_t i = 1; i < m; ++i)
                    mat_num_assign(&v[i], num_clone(NCM(Hn, k + 1 + i, k, n)));

                {
                    number_t vnorm2 = num_clone(NUM_ZERO);

                    for (size_t i = 0; i < m; ++i) {
                        number_t term = mat_num_abs2(v[i]);
                        number_t next_vnorm2 = num_add(vnorm2, term);
                        num_destroy(&vnorm2);
                        vnorm2 = next_vnorm2;
                        num_destroy(&term);
                    }

                    if (!num_is_zero(vnorm2)) {
                        number_t vnrm = num_sqrt(vnorm2);
                        number_t inv = num_div(NUM_ONE, vnrm);

                        for (size_t i = 0; i < m; ++i) {
                            number_t scaled = num_mul(v[i], inv);
                            mat_num_assign(&v[i], scaled);
                        }

                        num_destroy(&inv);
                        num_destroy(&vnrm);
                    }

                    num_destroy(&vnorm2);
                }

                for (size_t j = k; j < n; ++j) {
                    number_t w = num_clone(NUM_ZERO);

                    for (size_t i = 0; i < m; ++i) {
                        number_t vi_conj = num_conj(v[i]);
                        number_t term = num_mul(vi_conj, NCM(Hn, k + 1 + i, j, n));
                        number_t next_w = num_add(w, term);
                        num_destroy(&w);
                        w = next_w;
                        num_destroy(&term);
                        num_destroy(&vi_conj);
                    }

                    {
                        number_t two_w = num_add(w, w);
                        for (size_t i = 0; i < m; ++i) {
                            number_t corr = num_mul(v[i], two_w);
                            number_t updated = num_sub(NCM(Hn, k + 1 + i, j, n), corr);
                            mat_num_assign(&NCM(Hn, k + 1 + i, j, n), updated);
                            num_destroy(&corr);
                        }
                        num_destroy(&two_w);
                    }

                    num_destroy(&w);
                }

                for (size_t i = 0; i < n; ++i) {
                    number_t w = num_clone(NUM_ZERO);

                    for (size_t j = 0; j < m; ++j) {
                        number_t term = num_mul(NCM(Hn, i, k + 1 + j, n), v[j]);
                        number_t next_w = num_add(w, term);
                        num_destroy(&w);
                        w = next_w;
                        num_destroy(&term);
                    }

                    {
                        number_t two_w = num_add(w, w);
                        for (size_t j = 0; j < m; ++j) {
                            number_t vj_conj = num_conj(v[j]);
                            number_t corr = num_mul(two_w, vj_conj);
                            number_t updated = num_sub(NCM(Hn, i, k + 1 + j, n), corr);
                            mat_num_assign(&NCM(Hn, i, k + 1 + j, n), updated);
                            num_destroy(&corr);
                            num_destroy(&vj_conj);
                        }
                        num_destroy(&two_w);
                    }

                    num_destroy(&w);
                }

                for (size_t i = 0; i < n; ++i) {
                    number_t w = num_clone(NUM_ZERO);

                    for (size_t j = 0; j < m; ++j) {
                        number_t term = num_mul(NCM(Qn, i, k + 1 + j, n), v[j]);
                        number_t next_w = num_add(w, term);
                        num_destroy(&w);
                        w = next_w;
                        num_destroy(&term);
                    }

                    {
                        number_t two_w = num_add(w, w);
                        for (size_t j = 0; j < m; ++j) {
                            number_t vj_conj = num_conj(v[j]);
                            number_t corr = num_mul(two_w, vj_conj);
                            number_t updated = num_sub(NCM(Qn, i, k + 1 + j, n), corr);
                            mat_num_assign(&NCM(Qn, i, k + 1 + j, n), updated);
                            num_destroy(&corr);
                            num_destroy(&vj_conj);
                        }
                        num_destroy(&two_w);
                    }

                    num_destroy(&w);
                }

                for (size_t i = k + 2; i < n; ++i)
                    mat_num_assign(&NCM(Hn, i, k, n), num_clone(NUM_ZERO));

                num_destroy(&mu);
                num_destroy(&mu2);
                num_destroy(&x0_abs2);
                num_destroy(&x0_abs);
            }

            num_destroy(&sigma);
            num_destroy(&x0);
        }

        for (size_t i = 0; i < m; ++i)
            num_destroy(&v[i]);
        free(v);
    }

    return 0;
}

/* ============================================================
   Wilkinson shift for trailing 2x2 block of H
   ============================================================ */
static number_t wilkinson_shift(const number_t *H, size_t m, size_t n)
{
    number_t a = num_clone(NCM(H, m - 1, m - 1, n));
    number_t b = num_clone(NCM(H, m - 1, m, n));
    number_t c = num_clone(NCM(H, m, m - 1, n));
    number_t d = num_clone(NCM(H, m, m, n));

    number_t tr = num_add(a, d);
    number_t det_ad = num_mul(a, d);
    number_t det_bc = num_mul(b, c);
    number_t det = num_sub(det_ad, det_bc);

    number_t half_tr = num_mul(NUM_HALF, tr);
    number_t half_tr2 = num_mul(half_tr, half_tr);
    number_t disc = num_sub(half_tr2, det);
    number_t root = num_sqrt(disc);

    number_t mu1 = num_add(half_tr, root);
    number_t mu2 = num_sub(half_tr, root);

    number_t diff1 = num_sub(d, mu1);
    number_t diff2 = num_sub(d, mu2);
    number_t n1_num = num_abs(diff1);
    number_t n2_num = num_abs(diff2);
    double n1 = num_to_double(n1_num);
    double n2 = num_to_double(n2_num);
    number_t out = (n1 < n2) ? num_clone(mu1) : num_clone(mu2);

    num_destroy(&n2_num);
    num_destroy(&n1_num);
    num_destroy(&diff2);
    num_destroy(&diff1);
    num_destroy(&mu2);
    num_destroy(&mu1);
    num_destroy(&root);
    num_destroy(&disc);
    num_destroy(&half_tr2);
    num_destroy(&half_tr);
    num_destroy(&det);
    num_destroy(&det_bc);
    num_destroy(&det_ad);
    num_destroy(&tr);
    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&b);
    num_destroy(&a);
    return out;
}

/* ============================================================
   Implicit QR on Hessenberg H, accumulate Q
   ============================================================ */
static int schur_qr(number_t *Hn, number_t *Qn, size_t n, double eps)
{
    if (!Hn || !Qn)
        return -1;

    if (n <= 1)
        return 0;

    if (n == 2) {
        number_t a = num_clone(NCM(Hn, 0, 0, n));
        number_t b = num_clone(NCM(Hn, 0, 1, n));
        number_t c = num_clone(NCM(Hn, 1, 0, n));
        number_t d = num_clone(NCM(Hn, 1, 1, n));
        number_t c_abs = num_abs(c);
        double c_abs_d = num_to_double(c_abs);

        if (c_abs_d < eps) {
            num_destroy(&c_abs);
            num_destroy(&d);
            num_destroy(&c);
            num_destroy(&b);
            num_destroy(&a);
            return 0;
        }

        {
            number_t tr = num_add(a, d);
            number_t half_tr = num_mul(NUM_HALF, tr);
            number_t det_ad = num_mul(a, d);
            number_t det_bc = num_mul(b, c);
            number_t det = num_sub(det_ad, det_bc);
            number_t half_tr2 = num_mul(half_tr, half_tr);
            number_t disc = num_sub(half_tr2, det);
            number_t root = num_sqrt(disc);
            number_t lam1 = num_add(half_tr, root);
            number_t lam2 = num_sub(half_tr, root);
            number_t v0 = num_sub(lam1, d);
            number_t v1 = num_clone(c);
            number_t v0_abs = num_abs(v0);
            number_t v1_abs = num_abs(v1);
            number_t v0_abs2 = num_mul(v0_abs, v0_abs);
            number_t v1_abs2 = num_mul(v1_abs, v1_abs);
            number_t vnrm2 = num_add(v0_abs2, v1_abs2);
            number_t vnrm = num_sqrt(vnrm2);

            if (num_to_double(vnrm) == 0.0) {
                num_destroy(&vnrm);
                num_destroy(&vnrm2);
                num_destroy(&v1_abs2);
                num_destroy(&v0_abs2);
                num_destroy(&v1_abs);
                num_destroy(&v0_abs);
                num_destroy(&v1);
                num_destroy(&v0);
                num_destroy(&lam2);
                num_destroy(&lam1);
                num_destroy(&root);
                num_destroy(&disc);
                num_destroy(&half_tr2);
                num_destroy(&det);
                num_destroy(&det_bc);
                num_destroy(&det_ad);
                num_destroy(&half_tr);
                num_destroy(&tr);
                num_destroy(&c_abs);
                num_destroy(&d);
                num_destroy(&c);
                num_destroy(&b);
                num_destroy(&a);
                return -1;
            }

            {
                number_t inv = num_div(NUM_ONE, vnrm);
                number_t q0 = num_mul(v0, inv);
                number_t q1 = num_mul(v1, inv);
                number_t cq0 = num_conj(q0);
                number_t cq1 = num_conj(q1);
                number_t neg_cq1 = num_neg(cq1);
                number_t hq01_l = num_mul(a, neg_cq1);
                number_t hq01_r = num_mul(b, cq0);
                number_t hq01 = num_add(hq01_l, hq01_r);
                number_t hq11_l = num_mul(c, neg_cq1);
                number_t hq11_r = num_mul(d, cq0);
                number_t hq11 = num_add(hq11_l, hq11_r);
                number_t t01_l = num_mul(cq0, hq01);
                number_t t01_r = num_mul(cq1, hq11);
                number_t t01 = num_add(t01_l, t01_r);

                mat_num_assign(&NCM(Hn, 0, 0, n), num_clone(lam1));
                mat_num_assign(&NCM(Hn, 0, 1, n), t01);
                mat_num_assign(&NCM(Hn, 1, 0, n), num_clone(NUM_ZERO));
                mat_num_assign(&NCM(Hn, 1, 1, n), num_clone(lam2));

                for (size_t i = 0; i < 2; ++i) {
                    number_t old0 = num_clone(NCM(Qn, i, 0, n));
                    number_t old1 = num_clone(NCM(Qn, i, 1, n));
                    number_t col0_l = num_mul(old0, q0);
                    number_t col0_r = num_mul(old1, q1);
                    number_t col0 = num_add(col0_l, col0_r);
                    number_t col1_l = num_mul(old0, neg_cq1);
                    number_t col1_r = num_mul(old1, cq0);
                    number_t col1 = num_add(col1_l, col1_r);

                    mat_num_assign(&NCM(Qn, i, 0, n), col0);
                    mat_num_assign(&NCM(Qn, i, 1, n), col1);

                    num_destroy(&col1_r);
                    num_destroy(&col1_l);
                    num_destroy(&col0_r);
                    num_destroy(&col0_l);
                    num_destroy(&old1);
                    num_destroy(&old0);
                }

                num_destroy(&t01_r);
                num_destroy(&t01_l);
                num_destroy(&hq11);
                num_destroy(&hq11_r);
                num_destroy(&hq11_l);
                num_destroy(&hq01);
                num_destroy(&hq01_r);
                num_destroy(&hq01_l);
                num_destroy(&neg_cq1);
                num_destroy(&cq1);
                num_destroy(&cq0);
                num_destroy(&q1);
                num_destroy(&q0);
                num_destroy(&inv);
            }

            num_destroy(&vnrm);
            num_destroy(&vnrm2);
            num_destroy(&v1_abs2);
            num_destroy(&v0_abs2);
            num_destroy(&v1_abs);
            num_destroy(&v0_abs);
            num_destroy(&v1);
            num_destroy(&v0);
            num_destroy(&lam2);
            num_destroy(&lam1);
            num_destroy(&root);
            num_destroy(&disc);
            num_destroy(&half_tr2);
            num_destroy(&det);
            num_destroy(&det_bc);
            num_destroy(&det_ad);
            num_destroy(&half_tr);
            num_destroy(&tr);
        }

        num_destroy(&c_abs);
        num_destroy(&d);
        num_destroy(&c);
        num_destroy(&b);
        num_destroy(&a);
        return 0;
    }

    {
        const int max_iter = 1000 * (int)n;

        for (size_t m = n - 1; m > 0;) {
            number_t hml = num_clone(NCM(Hn, m, m - 1, n));
            number_t hml_abs = num_abs(hml);
            double hml_abs_d = num_to_double(hml_abs);
            num_destroy(&hml_abs);
            num_destroy(&hml);
            if (hml_abs_d < eps) {
                m--;
                continue;
            }

            int iter = 0;
            while (iter++ < max_iter) {
                for (size_t k = 0; k < m; ++k) {
                    number_t a;
                    number_t b;
                    number_t a_abs;
                    number_t b_abs;
                    number_t a_abs2;
                    number_t b_abs2;
                    number_t nrm2;

                    if (k == 0) {
                        number_t mu = wilkinson_shift(Hn, m, n);
                        a = num_sub(NCM(Hn, 0, 0, n), mu);
                        b = num_clone(NCM(Hn, 1, 0, n));
                        num_destroy(&mu);
                    } else {
                        a = num_clone(NCM(Hn, k, k - 1, n));
                        b = num_clone(NCM(Hn, k + 1, k - 1, n));
                    }

                    a_abs = num_abs(a);
                    b_abs = num_abs(b);
                    a_abs2 = num_mul(a_abs, a_abs);
                    b_abs2 = num_mul(b_abs, b_abs);
                    nrm2 = num_add(a_abs2, b_abs2);

                    if (num_to_double(nrm2) >= eps) {
                        number_t nrm = num_sqrt(nrm2);
                        number_t v0;

                        if (num_to_double(a_abs) < eps) {
                            v0 = num_clone(nrm);
                        } else {
                            number_t phase = num_div(a, a_abs);
                            number_t phase_nrm = num_mul(phase, nrm);
                            v0 = num_add(a, phase_nrm);
                            num_destroy(&phase_nrm);
                            num_destroy(&phase);
                        }

                        {
                            number_t v0_abs = num_abs(v0);
                            number_t v0_abs2 = num_mul(v0_abs, v0_abs);
                            number_t vnrm2 = num_add(v0_abs2, b_abs2);

                            if (num_to_double(vnrm2) >= eps) {
                                number_t vnrm = num_sqrt(vnrm2);
                                number_t inv = num_div(NUM_ONE, vnrm);
                                number_t u0 = num_mul(v0, inv);
                                number_t u1 = num_mul(b, inv);
                                size_t jstart = (k > 0) ? k - 1 : (size_t)0;

                                for (size_t j = jstart; j < n; ++j) {
                                    number_t u0_conj = num_conj(u0);
                                    number_t u1_conj = num_conj(u1);
                                    number_t dot0 = num_mul(u0_conj, NCM(Hn, k, j, n));
                                    number_t dot1 = num_mul(u1_conj, NCM(Hn, k + 1, j, n));
                                    number_t dot = num_add(dot0, dot1);
                                    number_t two_dot = num_add(dot, dot);
                                    number_t corr0 = num_mul(u0, two_dot);
                                    number_t corr1 = num_mul(u1, two_dot);
                                    number_t updated0 = num_sub(NCM(Hn, k, j, n), corr0);
                                    number_t updated1 = num_sub(NCM(Hn, k + 1, j, n), corr1);

                                    mat_num_assign(&NCM(Hn, k, j, n), updated0);
                                    mat_num_assign(&NCM(Hn, k + 1, j, n), updated1);

                                    num_destroy(&corr1);
                                    num_destroy(&corr0);
                                    num_destroy(&two_dot);
                                    num_destroy(&dot);
                                    num_destroy(&dot1);
                                    num_destroy(&dot0);
                                    num_destroy(&u1_conj);
                                    num_destroy(&u0_conj);
                                }

                                for (size_t i = 0; i < n; ++i) {
                                    number_t dot0 = num_mul(NCM(Hn, i, k, n), u0);
                                    number_t dot1 = num_mul(NCM(Hn, i, k + 1, n), u1);
                                    number_t dot = num_add(dot0, dot1);
                                    number_t two_dot = num_add(dot, dot);
                                    number_t u0_conj = num_conj(u0);
                                    number_t u1_conj = num_conj(u1);
                                    number_t corr0 = num_mul(two_dot, u0_conj);
                                    number_t corr1 = num_mul(two_dot, u1_conj);
                                    number_t updated0 = num_sub(NCM(Hn, i, k, n), corr0);
                                    number_t updated1 = num_sub(NCM(Hn, i, k + 1, n), corr1);

                                    mat_num_assign(&NCM(Hn, i, k, n), updated0);
                                    mat_num_assign(&NCM(Hn, i, k + 1, n), updated1);

                                    num_destroy(&corr1);
                                    num_destroy(&corr0);
                                    num_destroy(&u1_conj);
                                    num_destroy(&u0_conj);
                                    num_destroy(&two_dot);
                                    num_destroy(&dot);
                                    num_destroy(&dot1);
                                    num_destroy(&dot0);
                                }

                                for (size_t i = 0; i < n; ++i) {
                                    number_t dot0 = num_mul(NCM(Qn, i, k, n), u0);
                                    number_t dot1 = num_mul(NCM(Qn, i, k + 1, n), u1);
                                    number_t dot = num_add(dot0, dot1);
                                    number_t two_dot = num_add(dot, dot);
                                    number_t u0_conj = num_conj(u0);
                                    number_t u1_conj = num_conj(u1);
                                    number_t corr0 = num_mul(two_dot, u0_conj);
                                    number_t corr1 = num_mul(two_dot, u1_conj);
                                    number_t updated0 = num_sub(NCM(Qn, i, k, n), corr0);
                                    number_t updated1 = num_sub(NCM(Qn, i, k + 1, n), corr1);

                                    mat_num_assign(&NCM(Qn, i, k, n), updated0);
                                    mat_num_assign(&NCM(Qn, i, k + 1, n), updated1);

                                    num_destroy(&corr1);
                                    num_destroy(&corr0);
                                    num_destroy(&u1_conj);
                                    num_destroy(&u0_conj);
                                    num_destroy(&two_dot);
                                    num_destroy(&dot);
                                    num_destroy(&dot1);
                                    num_destroy(&dot0);
                                }

                                num_destroy(&u1);
                                num_destroy(&u0);
                                num_destroy(&inv);
                                num_destroy(&vnrm);
                            }

                            num_destroy(&vnrm2);
                            num_destroy(&v0_abs2);
                            num_destroy(&v0_abs);
                        }

                        num_destroy(&v0);
                        num_destroy(&nrm);
                    }

                    num_destroy(&nrm2);
                    num_destroy(&b_abs2);
                    num_destroy(&a_abs2);
                    num_destroy(&b_abs);
                    num_destroy(&a_abs);
                    num_destroy(&b);
                    num_destroy(&a);
                }

                {
                    number_t hml2 = num_clone(NCM(Hn, m, m - 1, n));
                    number_t hml2_abs = num_abs(hml2);
                    double hml2_abs_d = num_to_double(hml2_abs);
                    num_destroy(&hml2_abs);
                    num_destroy(&hml2);
                    if (hml2_abs_d < eps) {
                        mat_num_assign(&NCM(Hn, m, m - 1, n), num_clone(NUM_ZERO));
                        break;
                    }
                }
            }

            if (iter >= max_iter)
                return -1;

            m--;
        }
    }

    return 0;
}

static int mat_eigendecompose_general(const matrix_t *A, number_t *eigenvalues, matrix_t **eigenvectors)
{
    size_t n = A->rows;
    double eps = mat_numeric_algo_epsilon(A);
    schur_workspace_t ws = {0};

    if (schur_workspace_init_from_matrix(&ws, A) != 0)
        return -3;

    if (hessenberg_reduce(ws.H, ws.Q, n) != 0) {
        schur_workspace_free(&ws);
        return -3;
    }
    if (schur_qr(ws.H, ws.Q, n, eps) != 0) {
        schur_workspace_free(&ws);
        return -3;
    }

    if (eigenvalues) {
        number_t *ev = eigenvalues;
        for (size_t i = 0; i < n; i++)
            ev[i] = num_clone(NCM(ws.H, i, i, n));
    }

    if (eigenvectors) {
        number_t *Y = (number_t *)calloc(n * n, sizeof(number_t));
        if (!Y) {
            schur_workspace_free(&ws);
            return -3;
        }
        for (size_t i = 0; i < n * n; ++i)
            Y[i] = NUM_ZERO;

        for (size_t k = 0; k < n; k++)
            backsub_eigenvec(ws.H, Y, n, k, eps);

        matrix_t *V = mat_create_dense_with_elem(n, n, &number_elem);
        if (!V) {
            schur_workspace_free(&ws);
            free(Y);
            return -3;
        }

        number_t value;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                number_t sum = num_clone(NUM_ZERO);
                for (size_t k = 0; k < n; k++) {
                    number_t qik = num_clone(NCM(ws.Q, i, k, n));
                    number_t ykj = num_clone(QCM(Y, k, j, n));
                    number_t prod = num_mul(qik, ykj);
                    number_t next_sum = num_add(sum, prod);
                    num_destroy(&sum);
                    sum = next_sum;
                    num_destroy(&prod);
                    num_destroy(&ykj);
                    num_destroy(&qik);
                }
                value = sum;
                mat_set_num_owned(V, i, j, &value);
            }
        }
        for (size_t i = 0; i < n * n; ++i)
            num_destroy(&Y[i]);
        free(Y);
        *eigenvectors = V;
    }

    schur_workspace_free(&ws);
    return 0;
}

int mat_eigendecompose(const matrix_t *A, number_t *eigenvalues, matrix_t **eigenvectors)
{
    if (!A)
        return -1;
    if (A->rows != A->cols)
        return -2;

    if (matrix_is_symbolic(A))
        return -3;

    if (!elem_supports_numeric_algorithms(A->elem))
        return -3;

    if (mat_is_hermitian(A))
        return mat_eigendecompose_hermitian(A, eigenvalues, eigenvectors);
    return mat_eigendecompose_general(A, eigenvalues, eigenvectors);
}

int mat_eigenvalues(const matrix_t *A, number_t *eigenvalues)
{
    return mat_eigendecompose(A, eigenvalues, NULL);
}

matrix_t *mat_eigenvectors(const matrix_t *A)
{
    matrix_t *V = NULL;

    if (!A)
        return NULL;

    if (matrix_is_symbolic(A)) {
        if (mat_eigendecompose_expr(A, NULL, &V) != 0)
            return NULL;
        return V;
    }

    if (mat_eigendecompose(A, NULL, &V) != 0)
        return NULL;
    return V;
}

/* ============================================================
   Public Schur API
   ============================================================ */

int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out)
{
    schur_workspace_t ws = {0};
    matrix_t *Qmat = NULL;
    matrix_t *Tmat = NULL;
    double eps = 0.0;

    if (!A || !out)
        return -1;
    if (A->rows != A->cols)
        return -2;
    eps = mat_numeric_algo_epsilon(A);

    if (schur_workspace_init_from_matrix(&ws, A) != 0)
        return -3;

    /* Step 2: Hessenberg reduction Z -> H, Q0 */
    if (hessenberg_reduce(ws.H, ws.Q, A->rows) != 0) {
        schur_workspace_free(&ws);
        return -4;
    }

    /* Step 3: QR iteration on H (in Z), accumulate into Q0 */
    if (schur_qr(ws.H, ws.Q, A->rows, eps) != 0) {
        schur_workspace_free(&ws);
        return -5;
    }

    /* Z is now T (Schur form), Q0 is Q. Expose both through number_t. */
    Qmat = mat_create_dense_with_elem(A->rows, A->rows, &number_elem);
    Tmat = mat_create_upper_triangular_with_elem(A->rows, A->rows, &number_elem);
    if (!Qmat || !Tmat) {
        mat_free(Qmat);
        mat_free(Tmat);
        schur_workspace_free(&ws);
        return -3;
    }

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->rows; ++j) {
            mat_set_num_clone(Qmat, i, j, &NCM(ws.Q, i, j, A->rows));
        }
        for (size_t j = i; j < A->rows; ++j) {
            mat_set_num_clone(Tmat, i, j, &NCM(ws.H, i, j, A->rows));
        }
    }

    out->Q = Qmat;
    out->T = Tmat;
    schur_workspace_free(&ws);
    return 0;
}

void mat_schur_factor_free(mat_schur_factor_t *S)
{
    if (!S)
        return;
    if (S->Q)
        mat_free(S->Q);
    if (S->T)
        mat_free(S->T);
    S->Q = S->T = NULL;
}
