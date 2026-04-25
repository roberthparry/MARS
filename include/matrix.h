#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

#include "qfloat.h"
#include "qcomplex.h"

/**
 * @file matrix.h
 * @brief Generic high‑precision matrix type with pluggable element types.
 *
 * This API exposes a uniform matrix abstraction while hiding all internal
 * details such as element type, storage representation, and vtables.
 *
 * Matrices may be:
 *   - fully materialised (standard matrix)
 *   - identity (zero storage; materialises on write)
 *   - diagonal (future extension)
 *
 * All operations dispatch through internal vtables. No type switches or
 * storage switches appear in user code.
 */

typedef struct matrix_t matrix_t;

/**
 * @brief Matrix element type.
 */
typedef enum {
    MAT_TYPE_DOUBLE,
    MAT_TYPE_QFLOAT,
    MAT_TYPE_QCOMPLEX
} mat_type_t;

/* -------------------------------------------------------------------------
   Construction
   ------------------------------------------------------------------------- */

/**
 * @brief Allocate a new (incomplete) matrix of doubles.
 *
 * The returned matrix is allocated but contains unspecified values.
 * The caller must fill it using mat_set().
 */
matrix_t *mat_new_d(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of qfloat_t.
 */
matrix_t *mat_new_qf(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of qcomplex_t.
 */
matrix_t *mat_new_qc(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) square matrix of doubles.
 */
matrix_t *matsq_new_d(size_t n);

/**
 * @brief Allocate a new (incomplete) square matrix of qfloat_t.
 */
matrix_t *matsq_new_qf(size_t n);

/**
 * @brief Allocate a new (incomplete) square matrix of qcomplex_t.
 */
matrix_t *matsq_new_qc(size_t n);

/**
 * @brief Create a complete identity matrix of doubles.
 */
matrix_t *mat_create_identity_d(size_t n);

/**
 * @brief Create a complete identity matrix of qfloat_t.
 */
matrix_t *mat_create_identity_qf(size_t n);

/**
 * @brief Create a complete identity matrix of qcomplex_t.
 */
matrix_t *mat_create_identity_qc(size_t n);

/**
 * @brief Create a complete matrix of doubles from a flat array.
 *
 * @param rows       Number of rows.
 * @param cols       Number of columns.
 * @param data       Pointer to rows*cols values.
 */
matrix_t *mat_create_d(size_t rows, size_t cols, const double *data);

/**
 * @brief Create a complete matrix of qfloat_t from a flat array.
 */
matrix_t *mat_create_qf(size_t rows, size_t cols, const qfloat_t *data);

/**
 * @brief Create a complete matrix of qcomplex_t from a flat array.
 */
matrix_t *mat_create_qc(size_t rows, size_t cols, const qcomplex_t *data);

/* -------------------------------------------------------------------------
   Destruction
   ------------------------------------------------------------------------- */

void mat_free(matrix_t *A);

/* -------------------------------------------------------------------------
   Element access
   ------------------------------------------------------------------------- */

void mat_get(const matrix_t *A, size_t i, size_t j, void *out);
void mat_set(matrix_t *A, size_t i, size_t j, const void *val);

size_t mat_get_row_count(const matrix_t *A);
size_t mat_get_col_count(const matrix_t *A);

/**
 * @brief Query the element type of a matrix.
 *
 * This function returns the public element type associated with a matrix.
 * It allows callers to detect when an operation has promoted the matrix to
 * a wider numerical type.  For example, functions such as mat_log(),
 * mat_sqrt(), or mat_pow() may legitimately produce complex-valued results
 * even when the input matrix is real.  In such cases the returned matrix
 * will have a different element type from the input.
 *
 * This mechanism is important when performing bulk data extraction into
 * user-provided buffers: callers must ensure that the buffer element type
 * matches the actual matrix element type.  By checking mat_typeof() before
 * a bulk get, the caller can avoid accidental overwrites or misinterpretation
 * of the underlying data.
 *
 * The returned type is a stable, public-facing enumeration and does not
 * expose any internal representation details.
 *
 * @param A  The matrix whose element type is to be queried.
 * @return   The public element type of the matrix.
 */
mat_type_t mat_typeof(const matrix_t *A);

/* -------------------------------------------------------------------------
   Bulk settors/gettors
   ------------------------------------------------------------------------- */

/**
 * @brief Set all matrix elements from a flat row‑major buffer.
 *
 * The buffer must contain rows*cols elements of the matrix's element type
 * (double, qfloat_t, or qcomplex_t depending on A).
 *
 * @param A     The matrix to modify.
 * @param data  Pointer to a flat row‑major array of elements.
 */
void mat_set_data(matrix_t *A, const void *data);

/**
 * @brief Get all matrix elements into a flat row‑major buffer.
 *
 * The buffer must have space for rows*cols elements of the matrix's
 * element type (double, qfloat_t, or qcomplex_t depending on A).
 *
 * @param A     The matrix to read from.
 * @param data  Pointer to a flat row‑major array to receive the elements.
 */
void mat_get_data(const matrix_t *A, void *data);

/* -------------------------------------------------------------------------
   Basic operations
   ------------------------------------------------------------------------- */

matrix_t *mat_scalar_mul_d(matrix_t *A, double s);
matrix_t *mat_scalar_mul_qf(matrix_t *A, qfloat_t s);
matrix_t *mat_scalar_mul_qc(matrix_t *A, qcomplex_t s);

matrix_t *mat_scalar_div_d(matrix_t *A, double s);
matrix_t *mat_scalar_div_qf(matrix_t *A, qfloat_t s);
matrix_t *mat_scalar_div_qc(matrix_t *A, qcomplex_t s);

matrix_t *mat_add(const matrix_t *A, const matrix_t *B);
matrix_t *mat_sub(const matrix_t *A, const matrix_t *B);
matrix_t *mat_mul(const matrix_t *A, const matrix_t *B);

matrix_t *mat_transpose(const matrix_t *A);
matrix_t *mat_conj(const matrix_t *A);
matrix_t *mat_hermitian(const matrix_t *A);

int       mat_det(const matrix_t *A, void *determinant);
matrix_t *mat_inverse(const matrix_t *A);

/* -------------------------------------------------------------------------
   Eigenvalues / Eigenvectors
   ------------------------------------------------------------------------- */

int       mat_eigenvalues(const matrix_t *A, void *eigenvalues);
int       mat_eigendecompose(const matrix_t *A, void *eigenvalues,
                             matrix_t **eigenvectors);
matrix_t *mat_eigenvectors(const matrix_t *A);

/* -------------------------------------------------------------------------
   Matrix functions (Hermitian matrices via eigendecomposition)
   ------------------------------------------------------------------------- */

matrix_t *mat_exp(const matrix_t *A);
matrix_t *mat_sin(const matrix_t *A);
matrix_t *mat_cos(const matrix_t *A);
matrix_t *mat_tan(const matrix_t *A);

matrix_t *mat_sinh(const matrix_t *A);
matrix_t *mat_cosh(const matrix_t *A);
matrix_t *mat_tanh(const matrix_t *A);

matrix_t *mat_sqrt(const matrix_t *A);
matrix_t *mat_log(const matrix_t *A);

matrix_t *mat_asin(const matrix_t *A);
matrix_t *mat_acos(const matrix_t *A);
matrix_t *mat_atan(const matrix_t *A);

matrix_t *mat_asinh(const matrix_t *A);
matrix_t *mat_acosh(const matrix_t *A);
matrix_t *mat_atanh(const matrix_t *A);

matrix_t *mat_erf(const matrix_t *A);
matrix_t *mat_erfc(const matrix_t *A);

/* -------------------------------------------------------------------------
   Power functions
   ------------------------------------------------------------------------- */

/**
 * @brief Integer power: A^n via binary exponentiation.
 *
 * n may be negative (uses mat_inverse internally).
 * Returns NULL if A is NULL, not square, or inversion fails for n < 0.
 */
matrix_t *mat_pow_int(const matrix_t *A, int n);

/**
 * @brief Real power: A^s = exp(s * log(A)).
 *
 * Requires A to have a well-defined matrix logarithm (positive definite
 * for real matrices).  Returns NULL on error.
 */
matrix_t *mat_pow(const matrix_t *A, double s);

/* -------------------------------------------------------------------------
   Debugging / I/O
   ------------------------------------------------------------------------- */

void mat_print(const matrix_t *A);

#endif /* MATRIX_H */
