#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

#include "qfloat.h"
#include "qcomplex.h"

/**
 * @file matrix.h
 * @brief Generic high‑precision matrix type with pluggable element types
 *        and storage kinds (dense, identity, diagonal, etc.).
 *
 * This API exposes a uniform matrix abstraction while hiding all internal
 * details such as element type, storage representation, and vtables.
 *
 * Matrices may be:
 *   - dense (fully materialised)
 *   - identity (zero storage; materialises on write)
 *   - diagonal (future extension)
 *
 * All operations dispatch through internal vtables. No type switches or
 * storage switches appear in user code.
 */

typedef struct matrix_t matrix_t;

/* -------------------------------------------------------------------------
   Construction
   ------------------------------------------------------------------------- */

/**
 * @brief Create a dense matrix of doubles.
 */
matrix_t *mat_create_d(size_t rows, size_t cols);

/**
 * @brief Create a dense matrix of qfloat_t.
 */
matrix_t *mat_create_qf(size_t rows, size_t cols);

/**
 * @brief Create a dense matrix of qcomplex_t.
 */
matrix_t *mat_create_qc(size_t rows, size_t cols);

/**
 * @brief Create a square dense matrix of doubles.
 */
matrix_t *matsq_create_d(size_t n);

/**
 * @brief Create a square dense matrix of qfloat_t.
 */
matrix_t *matsq_create_qf(size_t n);

/**
 * @brief Create a square dense matrix of qcomplex_t.
 */
matrix_t *matsq_create_qc(size_t n);

/**
 * @brief Create an identity matrix of doubles.
 */
matrix_t *matsq_ident_d(size_t n);

/**
 * @brief Create an identity matrix of qfloat_t.
 */
matrix_t *matsq_ident_qf(size_t n);

/**
 * @brief Create an identity matrix of qcomplex_t.
 */
matrix_t *matsq_ident_qc(size_t n);

/* -------------------------------------------------------------------------
   Destruction
   ------------------------------------------------------------------------- */

void mat_free(matrix_t *A);

/* -------------------------------------------------------------------------
   Element access
   ------------------------------------------------------------------------- */

/**
 * @brief Get the value of an element in the matrix.
 */
void mat_get(const matrix_t *A, size_t i, size_t j, void *out);

/**
 * @brief Set the value of an element in the matrix.
 */
void mat_set(matrix_t *A, size_t i, size_t j, const void *val);

/**
 * @brief Get the number of rows in the matrix.
 */
size_t mat_get_row_count(const matrix_t *A);

/**
 * @brief Get the number of columns in the matrix.
 */
size_t mat_get_col_count(const matrix_t *A);


/* -------------------------------------------------------------------------
   Basic operations
   ------------------------------------------------------------------------- */

/* Scalar multiplication */

/**
 * @brief Multiply a matrix by a double scalar.
 */
matrix_t *mat_scalar_mul_d(double s, matrix_t *A);

/**
 * @brief Multiply a matrix by a qfloat_t scalar.
 */
matrix_t *mat_scalar_mul_qf(qfloat_t s, matrix_t *A);

/**
 * @brief Multiply a matrix by a qcomplex_t scalar.
 */
matrix_t *mat_scalar_mul_qc(qcomplex_t s, matrix_t *A);

/**
 * @brief Divide a matrix by a double scalar.
 */
matrix_t *mat_scalar_div_d(double s, matrix_t *A);

/**
 * @brief Divide a matrix by a qfloat_t scalar.
 */
matrix_t *mat_scalar_div_qf(qfloat_t s, matrix_t *A);

/**
 * @brief Divide a matrix by a qcomplex_t scalar.
 */
matrix_t *mat_scalar_div_qc(qcomplex_t s, matrix_t *A);

/**
 * @brief Add two matrices.
 */
matrix_t *mat_add(const matrix_t *A, const matrix_t *B);

/**
 * @brief Subtract matrix B from matrix A.
 */
matrix_t *mat_sub(const matrix_t *A, const matrix_t *B);

/**
 * @brief Multiply two matrices.
 */
matrix_t *mat_mul(const matrix_t *A, const matrix_t *B);

/**
 * @brief Transpose a matrix.
 */
matrix_t *mat_transpose(const matrix_t *A);

/**
 * @brief Conjugate a matrix.
 */
matrix_t *mat_conj(const matrix_t *A);

/**
 * @brief Compute the Hermitian (conjugate transpose) of a matrix.
 * @param A The input matrix.
 * @return A new matrix representing the Hermitian of A, or NULL on error.
 */
matrix_t *mat_hermitian(const matrix_t *A);

/**
 * @brief Compute the determinant of a square matrix.
 * @param A The input square matrix.
 * @param determinant Pointer to a buffer where the determinant will be stored.
 * @return 0 on success, negative on error:
 *         -1 : A is NULL
 *         -2 : A is not square
 *         -3 : allocation failure
 */
int mat_det(const matrix_t *A, void *determinant);

/**
 * @brief Compute the inverse of a square matrix.
 * @param A The input square matrix.
 * @return A new matrix representing the inverse of A, or NULL on error.
 */
matrix_t *mat_inverse(const matrix_t *A);

/**
 * @brief Compute all eigenvalues of a square matrix.
 *
 * Computes the complete set of eigenvalues of @p A. The matrix must
 * be square. The eigenvalues are written into the user-provided buffer
 * @p eigenvalues, which must have room for @c n values of the matrix's
 * element type (double, qfloat_t, or qcomplex_t depending on @p A).
 *
 * @param[in]  A            The input matrix (must be square).
 * @param[out] eigenvalues  Buffer of size @c n * elem_size to receive
 *                          the eigenvalues.
 *
 * @return 0 on success, negative on error.
 */
int mat_eigenvalues(const matrix_t *A, void *eigenvalues);

/**
 * @brief Compute eigenvalues and eigenvectors of a square matrix.
 *
 * Computes both the eigenvalues and the corresponding eigenvectors of
 * @p A. The matrix must be square. The eigenvalues are written into the
 * user-provided buffer @p eigenvalues, which must have room for @c n
 * values of the matrix's element type.
 *
 * The eigenvectors are returned as a newly allocated @c n×n matrix of
 * the same element type as @p A. Each column of the returned matrix is
 * an eigenvector. For Hermitian matrices, the eigenvectors are returned
 * orthonormal.
 *
 * @param[in]  A             The input matrix (must be square).
 * @param[out] eigenvalues   Buffer of size @c n * elem_size to receive
 *                           the eigenvalues (may be NULL if only
 *                           eigenvectors are desired).
 * @param[out] eigenvectors  On success, set to a newly allocated matrix
 *                           containing the eigenvectors.
 *
 * @return 0 on success, negative on error.
 */
int mat_eigendecompose(const matrix_t *A, void *eigenvalues, matrix_t **eigenvectors);

/**
 * @brief Compute only the eigenvectors of a square matrix.
 *
 * Convenience wrapper around mat_eigendecompose(). The eigenvalues are
 * discarded. The returned matrix contains the eigenvectors as its columns.
 * For Hermitian matrices, the eigenvectors are orthonormal.
 *
 * @param[in] A   The input matrix (must be square).
 *
 * @return A newly allocated matrix of eigenvectors, or NULL on error.
 */
matrix_t *mat_eigenvectors(const matrix_t *A);

/* -------------------------------------------------------------------------
   Matrix functions (Hermitian matrices via eigendecomposition)
   ------------------------------------------------------------------------- */

/**
 * @brief Compute the matrix exponential exp(A) of a square Hermitian matrix.
 *
 * Uses eigendecomposition: A = V D V†, then exp(A) = V · diag(exp(λᵢ)) · V†.
 *
 * @param A  Square Hermitian matrix (double, qfloat_t, or qcomplex_t elements).
 * @return   Newly allocated matrix exp(A), or NULL on error.
 */
matrix_t *mat_exp(const matrix_t *A);

/**
 * @brief Compute the matrix sine sin(A) of a square Hermitian matrix.
 *
 * Uses eigendecomposition: A = V D V†, then sin(A) = V · diag(sin(λᵢ)) · V†.
 *
 * @param A  Square Hermitian matrix (double, qfloat_t, or qcomplex_t elements).
 * @return   Newly allocated matrix sin(A), or NULL on error.
 */
matrix_t *mat_sin(const matrix_t *A);

/**
 * @brief Compute the matrix cosine cos(A) of a square Hermitian matrix.
 */
matrix_t *mat_cos(const matrix_t *A);

/**
 * @brief Compute the matrix tangent tan(A) of a square Hermitian matrix.
 */
matrix_t *mat_tan(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic sine sinh(A) of a square Hermitian matrix.
 */
matrix_t *mat_sinh(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic cosine cosh(A) of a square Hermitian matrix.
 */
matrix_t *mat_cosh(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic tangent tanh(A) of a square Hermitian matrix.
 */
matrix_t *mat_tanh(const matrix_t *A);

/**
 * @brief Compute the matrix square root sqrt(A) of a square matrix.
 */
matrix_t *mat_sqrt(const matrix_t *A);

/**
 * @brief Compute the principal matrix logarithm log(A) of a square matrix.
 */
matrix_t *mat_log(const matrix_t *A);

/**
 * @brief Compute the matrix arcsine asin(A) of a square matrix.
 */
matrix_t *mat_asin(const matrix_t *A);

/**
 * @brief Compute the matrix arccosine acos(A) of a square matrix.
 */
matrix_t *mat_acos(const matrix_t *A);

/**
 * @brief Compute the matrix arctangent atan(A) of a square matrix.
 */
matrix_t *mat_atan(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic arcsine asinh(A) of a square matrix.
 */
matrix_t *mat_asinh(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic arccosine acosh(A) of a square matrix.
 */
matrix_t *mat_acosh(const matrix_t *A);

/**
 * @brief Compute the matrix hyperbolic arctangent atanh(A) of a square matrix.
 */
matrix_t *mat_atanh(const matrix_t *A);

/* -------------------------------------------------------------------------
   Debugging / I/O
   ------------------------------------------------------------------------- */

/**
 * @brief Print the matrix to standard output.
 */
void mat_print(const matrix_t *A);

#endif /* MATRIX_H */
