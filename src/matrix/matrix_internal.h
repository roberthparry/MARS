#ifndef MATRIX_INTERNAL_H
#define MATRIX_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "matrix.h"
#include "matrix_eval_qc.h"

/* ============================================================
   Element kinds
   ============================================================ */

typedef enum {
    ELEM_DOUBLE = 0,
    ELEM_QFLOAT = 1,
    ELEM_QCOMPLEX = 2,
    ELEM_DVAL = 3,
    ELEM_MAX
} elem_kind;

/* ============================================================
   Scalar function vtable (per element type)
   ============================================================ */

struct elem_fun_vtable {
    /* elementary functions */
    void (*exp)(void *out, const void *a);
    void (*sin)(void *out, const void *a);
    void (*cos)(void *out, const void *a);
    void (*tan)(void *out, const void *a);

    void (*sinh)(void *out, const void *a);
    void (*cosh)(void *out, const void *a);
    void (*tanh)(void *out, const void *a);

    void (*sqrt)(void *out, const void *a);
    void (*log)(void *out, const void *a);

    /* inverse trig */
    void (*asin)(void *out, const void *a);
    void (*acos)(void *out, const void *a);
    void (*atan)(void *out, const void *a);

    /* inverse hyperbolic */
    void (*asinh)(void *out, const void *a);
    void (*acosh)(void *out, const void *a);
    void (*atanh)(void *out, const void *a);

    /* special functions */
    void (*erf)(void *out, const void *a);
    void (*erfc)(void *out, const void *a);
    void (*erfinv)(void *out, const void *a);
    void (*erfcinv)(void *out, const void *a);
    void (*gamma)(void *out, const void *a);
    void (*lgamma)(void *out, const void *a);
    void (*digamma)(void *out, const void *a);
    void (*trigamma)(void *out, const void *a);
    void (*tetragamma)(void *out, const void *a);
    void (*gammainv)(void *out, const void *a);
    void (*normal_pdf)(void *out, const void *a);
    void (*normal_cdf)(void *out, const void *a);
    void (*normal_logpdf)(void *out, const void *a);
    void (*lambert_w0)(void *out, const void *a);
    void (*lambert_wm1)(void *out, const void *a);
    void (*productlog)(void *out, const void *a);
    void (*ei)(void *out, const void *a);
    void (*e1)(void *out, const void *a);
};

/* ============================================================
   Element vtable
   ============================================================ */

struct elem_vtable {
    size_t size;
    elem_kind kind;
    mat_type_t public_type;

    /* arithmetic */
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
    void (*inv)(void *out, const void *a);

    /* scalar queries */
    double (*abs2)(const void *a);
    double (*to_real)(const void *a);
    void   (*from_real)(void *out, double x);
    void   (*conj_elem)(void *out, const void *a);

    /* qfloat-precision scalar queries */
    void (*to_qf)(qfloat_t *out, const void *a);
    void (*abs_qf)(qfloat_t *out, const void *a);
    void (*from_qf)(void *out, const qfloat_t *x);

    /* qcomplex cast */
    void (*to_qc)(qcomplex_t *out, const void *a);
    void (*from_qc)(void *out, const qcomplex_t *z);

    /* constants */
    const void *zero;
    const void *one;

    /* comparison */
    int (*cmp)(const void *a, const void *b);

    /* printing */
    void (*print)(const void *val, char *buf, size_t buflen);

    /* numerical tolerance scale for rank/conditioning decisions */
    qfloat_t relative_epsilon;

    const struct elem_fun_vtable *fun;
};

/* ============================================================
   Storage vtable
   ============================================================ */

struct store_vtable {
    struct matrix_t *(*create)(size_t rows, size_t cols,
                               const struct elem_vtable *elem);

    bool (*alloc)(struct matrix_t *A);
    void (*free)(struct matrix_t *A);

    void (*get)(const struct matrix_t *A, size_t i, size_t j, void *out);
    void (*set)(struct matrix_t *A, size_t i, size_t j, const void *val);
    void (*swap_rows)(struct matrix_t *A, size_t r1, size_t r2);
    void (*row_eliminate_from)(struct matrix_t *A, size_t dst_row, size_t src_row,
                               size_t col_start, const void *factor);

    void (*materialise)(struct matrix_t *A);
    bool (*is_sparse_storage)(const struct matrix_t *A);
    bool (*is_sparse_like)(const struct matrix_t *A);
    bool (*is_diagonal)(const struct matrix_t *A);
    bool (*is_upper_triangular)(const struct matrix_t *A);
    bool (*is_lower_triangular)(const struct matrix_t *A);
    size_t (*nonzero_count)(const struct matrix_t *A);
    const struct store_vtable *(*elementwise_unary_store)(const struct matrix_t *A);
    const struct store_vtable *(*transpose_store)(const struct matrix_t *A);
};

/* ============================================================
   Matrix object (opaque in matrix.h)
   ============================================================ */

struct matrix_t {
    size_t rows;
    size_t cols;
    size_t nnz;

    const struct elem_vtable  *elem;
    const struct store_vtable *store;

    void **data;   /* row pointers for dense/sparse; NULL for identity */
};

/* ============================================================
   Element vtable instances (defined in matrix_core.c)
   ============================================================ */

extern const struct elem_vtable double_elem;
extern const struct elem_vtable qfloat_elem;
extern const struct elem_vtable qcomplex_elem;
extern const struct elem_vtable dval_elem;

/* ============================================================
   Matrix construction helpers (internal)
   ============================================================ */

struct matrix_t *mat_create_dense_with_elem(size_t rows, size_t cols,
                                            const struct elem_vtable *elem);
struct matrix_t *mat_create_sparse_with_elem(size_t rows, size_t cols,
                                             const struct elem_vtable *elem);
struct matrix_t *mat_create_identity_with_elem(size_t n,
                                               const struct elem_vtable *elem);
struct matrix_t *mat_create_diagonal_with_elem(size_t n,
                                               const struct elem_vtable *elem);
struct matrix_t *mat_create_upper_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem);
struct matrix_t *mat_create_lower_triangular_with_elem(size_t rows, size_t cols,
                                                       const struct elem_vtable *elem);
struct matrix_t *mat_copy_with_store(const struct matrix_t *A,
                                     const struct store_vtable *store);
struct matrix_t *mat_copy_preserving_store(const struct matrix_t *A);
struct matrix_t *mat_copy_as_dense(const struct matrix_t *A);
struct matrix_t *mat_convert_with_store(const struct matrix_t *A,
                                        const struct elem_vtable *target,
                                        const struct store_vtable *store);

/* ============================================================
   Convenience accessor
   ============================================================ */

static inline const struct elem_vtable *elem_of(const struct matrix_t *A) {
    return A->elem;
}

/* ============================================================
   Schur decomposition API (internal use by matrix_maths.c)
   ============================================================ */

/**
 * Compute the Schur decomposition A = Q T Q*.
 *
 * A must be square. Q and T are allocated on success.
 * Returns 0 on success, nonzero on failure.
 */
int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out);

/**
 * Free the Q and T matrices inside a mat_schur_factor_t.
 */
void mat_schur_factor_free(mat_schur_factor_t *S);

/* ============================================================
   Matrix functions via Schur + Parlett (internal)
   ============================================================ */

/* Apply scalar function f to an upper triangular matrix T. */
matrix_t *mat_fun_triangular(const matrix_t *T,
                             void (*scalar_f)(void *out, const void *in));

/* High-level Schur-based matrix function engine. */
matrix_t *mat_fun_schur(const matrix_t *A,
                        void (*scalar_f)(void *out, const void *in));

/* Internal Hermitian detector used to select stable fast paths. */
int mat_is_hermitian(const matrix_t *A);

/* Drop any internal matrix-function cache associated with A. */
void mat_fun_cache_forget(const matrix_t *A);


#endif /* MATRIX_INTERNAL_H */
