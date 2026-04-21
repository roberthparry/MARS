#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

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

/* -------------------------------------------------------------------------
   Debugging / I/O
   ------------------------------------------------------------------------- */

/**
 * @brief Print the matrix to standard output.
 */
void mat_print(const matrix_t *A);

#endif /* MATRIX_H */
