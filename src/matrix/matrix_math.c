#include <stdlib.h>
#include <string.h>

#include "matrix_internal.h"
#include "qfloat.h"

/* Apply a scalar qfloat function f to each eigenvalue of a Hermitian matrix A.
 * Returns V · diag(f(λ_j)) · V†, or NULL on error. */
static matrix_t *apply_scalar_fn(const matrix_t *A, qfloat_t (*fn)(qfloat_t))
{
    if (!A || A->rows != A->cols)
        return NULL;

    size_t n = A->rows;
    const struct elem_vtable *e = A->elem;

    void *eigenvalues = malloc(n * e->size);
    if (!eigenvalues)
        return NULL;

    matrix_t *V = NULL;
    if (mat_eigendecompose(A, eigenvalues, &V) != 0) {
        free(eigenvalues);
        return NULL;
    }

    /* W[:,j] = f(λ_j) · V[:,j] */
    matrix_t *W = e->create_matrix(n, n);
    if (!W) {
        free(eigenvalues);
        mat_free(V);
        return NULL;
    }

    unsigned char eig_raw[64], fval_raw[64], v_raw[64], w_raw[64];
    for (size_t j = 0; j < n; j++) {
        memcpy(eig_raw, (char *)eigenvalues + j * e->size, e->size);
        qfloat_t lam_qf;
        e->to_qf(&lam_qf, eig_raw);
        qfloat_t fval = fn(lam_qf);
        e->from_qf(fval_raw, &fval);
        for (size_t i = 0; i < n; i++) {
            mat_get(V, i, j, v_raw);
            e->mul(w_raw, fval_raw, v_raw);
            mat_set(W, i, j, w_raw);
        }
    }

    free(eigenvalues);

    matrix_t *Vh = mat_hermitian(V);
    mat_free(V);
    if (!Vh) {
        mat_free(W);
        return NULL;
    }

    matrix_t *R = mat_mul(W, Vh);
    mat_free(W);
    mat_free(Vh);
    return R;
}

matrix_t *mat_exp(const matrix_t *A)  { return apply_scalar_fn(A, qf_exp);  }
matrix_t *mat_sin(const matrix_t *A)  { return apply_scalar_fn(A, qf_sin);  }
matrix_t *mat_cos(const matrix_t *A)  { return apply_scalar_fn(A, qf_cos);  }
matrix_t *mat_tan(const matrix_t *A)  { return apply_scalar_fn(A, qf_tan);  }
matrix_t *mat_sinh(const matrix_t *A) { return apply_scalar_fn(A, qf_sinh); }
matrix_t *mat_cosh(const matrix_t *A) { return apply_scalar_fn(A, qf_cosh); }
matrix_t *mat_tanh(const matrix_t *A) { return apply_scalar_fn(A, qf_tanh); }
