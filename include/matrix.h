#ifndef MATRIX_H
#define MATRIX_H

#include <stdbool.h>
#include <stddef.h>

#include "dval.h"
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
 *   - sparse
 *   - identity (zero storage; materialises on write)
 *   - diagonal
 *   - upper triangular
 *   - lower triangular
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
    MAT_TYPE_QCOMPLEX,
    MAT_TYPE_DVAL
} mat_type_t;

/**
 * @brief Matrix norm selector.
 *
 * This enumeration identifies the matrix norm to be computed by functions
 * such as mat_norm() and mat_condition_number().
 */
typedef enum {
    /** Maximum absolute column sum. */
    MAT_NORM_1,
    /** Maximum absolute row sum. */
    MAT_NORM_INF,
    /** Frobenius norm. */
    MAT_NORM_FRO,
    /** Spectral norm (largest singular value). */
    MAT_NORM_2
} mat_norm_type_t;

/**
 * @brief Result of an LU factorisation.
 *
 * On success, these matrices satisfy P A = L U.
 */
typedef struct {
    /** Permutation matrix. */
    matrix_t *P;
    /** Unit lower-triangular factor. */
    matrix_t *L;
    /** Upper-triangular factor. */
    matrix_t *U;
} mat_lu_factor_t;

/**
 * @brief Result of a QR factorisation.
 *
 * On success, these matrices satisfy A = Q R.
 */
typedef struct {
    /** Orthogonal or unitary factor. */
    matrix_t *Q;
    /** Upper-triangular factor. */
    matrix_t *R;
} mat_qr_factor_t;

/**
 * @brief Result of a Cholesky factorisation.
 *
 * On success, L satisfies A = L L^H.
 */
typedef struct {
    /** Lower-triangular Cholesky factor. */
    matrix_t *L;
} mat_cholesky_t;

/**
 * @brief Result of a singular value decomposition.
 *
 * On success, these matrices describe the factorisation A = U S V^H.
 */
typedef struct {
    /** Left singular vectors. */
    matrix_t *U;
    /** Diagonal matrix of singular values. */
    matrix_t *S;
    /** Right singular vectors. */
    matrix_t *V;
} mat_svd_factor_t;

/**
 * @brief Result of a Schur factorisation.
 *
 * On success, these matrices satisfy A = Q T Q^H, where Q is unitary and
 * T is upper triangular.
 */
typedef struct {
    /** Unitary Schur vectors. */
    matrix_t *Q;
    /** Upper-triangular Schur form. */
    matrix_t *T;
} mat_schur_factor_t;

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
matrix_t *mat_new_sparse_d(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of qfloat_t.
 */
matrix_t *mat_new_qf(size_t rows, size_t cols);
matrix_t *mat_new_sparse_qf(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of qcomplex_t.
 */
matrix_t *mat_new_qc(size_t rows, size_t cols);
matrix_t *mat_new_sparse_qc(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of dval_t* handles.
 *
 * Each stored handle is retained by the matrix. Callers remain responsible
 * for their own references after passing a value to mat_set() or mat_create_dv().
 */
matrix_t *mat_new_dv(size_t rows, size_t cols);
matrix_t *mat_new_sparse_dv(size_t rows, size_t cols);

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
 * @brief Allocate a new (incomplete) square matrix of dval_t* handles.
 */
matrix_t *matsq_new_dv(size_t n);

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
 * @brief Create a complete identity matrix of dval_t* handles.
 */
matrix_t *mat_create_identity_dv(size_t n);

/**
 * @brief Create a diagonal matrix of doubles from its diagonal entries.
 *
 * Only the main diagonal is stored explicitly. All off-diagonal entries are
 * zero.
 *
 * @param n         Matrix order.
 * @param diagonal  Pointer to n diagonal entries in row order.
 * @return          Newly allocated diagonal matrix on success, or NULL on error.
 */
matrix_t *mat_create_diagonal_d(size_t n, const double *diagonal);

/**
 * @brief Create a diagonal matrix of qfloat_t values from its diagonal entries.
 */
matrix_t *mat_create_diagonal_qf(size_t n, const qfloat_t *diagonal);

/**
 * @brief Create a diagonal matrix of qcomplex_t values from its diagonal entries.
 */
matrix_t *mat_create_diagonal_qc(size_t n, const qcomplex_t *diagonal);

/**
 * @brief Create a diagonal matrix of dval_t* handles from its diagonal entries.
 */
matrix_t *mat_create_diagonal_dv(size_t n, dval_t *const *diagonal);

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

/**
 * @brief Create a complete matrix of dval_t* handles from a flat array.
 *
 * Each handle is retained by the created matrix.
 */
matrix_t *mat_create_dv(size_t rows, size_t cols, dval_t *const *data);

/* -------------------------------------------------------------------------
   Destruction
   ------------------------------------------------------------------------- */

void mat_free(matrix_t *A);

/* -------------------------------------------------------------------------
   Element access
   ------------------------------------------------------------------------- */

/**
 * @brief Read one matrix element into @p out.
 *
 * For dval matrices, the returned dval_t* handle is borrowed from the matrix.
 * Do not call dv_free() on it unless you first create or retain your own
 * owning reference by other means.
 */
void mat_get(const matrix_t *A, size_t i, size_t j, void *out);

/**
 * @brief Store one matrix element from @p val.
 *
 * For dval matrices, the matrix retains the incoming dval_t* handle. The caller
 * still owns any reference it already held.
 */
void mat_set(matrix_t *A, size_t i, size_t j, const void *val);

size_t mat_get_row_count(const matrix_t *A);
size_t mat_get_col_count(const matrix_t *A);

/**
 * @brief Sparse storage helpers.
 *
 * Sparse matrices are useful when most entries are zero and only a small
 * number of values need to be stored explicitly. Typical examples include
 * diagonal matrices, banded matrices, graph adjacency matrices, and large
 * linear systems where only a few coefficients appear in each row.
 *
 * Use these helpers when you want to:
 * - check whether a matrix is currently using sparse storage,
 * - inspect how many nonzero entries it contains,
 * - convert a dense matrix into sparse form to save space, or
 * - materialise a sparse matrix as dense form for inspection or algorithms
 *   that expect all entries to be stored explicitly.
 *
 * Sparse storage is worth using when the matrix contains many zeros, because
 * it can reduce memory use and help sparse-aware operations avoid unnecessary
 * work. Dense storage is often simpler for very small matrices or for
 * algorithms that naturally touch nearly every entry.
 */
bool   mat_is_sparse(const matrix_t *A);
size_t mat_nonzero_count(const matrix_t *A);
matrix_t *mat_to_sparse(const matrix_t *A);
matrix_t *mat_to_dense(const matrix_t *A);

/**
 * @brief Structural queries.
 *
 * These predicates report whether a matrix has diagonal, upper-triangular, or
 * lower-triangular structure. They answer the mathematical question about the
 * entries of the matrix, not merely which internal storage backend is in use.
 *
 * Use them when you want to:
 * - verify the output of a factorisation,
 * - check whether a specialised algorithm is applicable, or
 * - confirm that a matrix built from a structured constructor has kept its
 *   shape after further operations.
 */
bool mat_is_diagonal(const matrix_t *A);
bool mat_is_upper_triangular(const matrix_t *A);
bool mat_is_lower_triangular(const matrix_t *A);

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
 * (double, qfloat_t, qcomplex_t, or dval_t* depending on A).
 *
 * @param A     The matrix to modify.
 * @param data  Pointer to a flat row‑major array of elements.
 */
void mat_set_data(matrix_t *A, const void *data);

/**
 * @brief Get all matrix elements into a flat row‑major buffer.
 *
 * The buffer must have space for rows*cols elements of the matrix's
 * element type (double, qfloat_t, qcomplex_t, or dval_t* depending on A).
 * For dval matrices, the copied handles are borrowed from the matrix.
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
matrix_t *mat_neg(const matrix_t *A);

matrix_t *mat_transpose(const matrix_t *A);
matrix_t *mat_conj(const matrix_t *A);
matrix_t *mat_hermitian(const matrix_t *A);

int       mat_det(const matrix_t *A, void *determinant);
matrix_t *mat_inverse(const matrix_t *A);

/**
 * @brief Solve the linear matrix equation A X = B.
 *
 * The input matrix A must be square and nonsingular. The matrix B may
 * contain one or more right-hand sides. When A is diagonal or triangular,
 * the solve is performed directly by substitution rather than first reducing
 * the system to a dense general form. General sparse systems are solved
 * through an LU factorisation followed by triangular substitution while
 * keeping sparse-like working storage. Compatible sparse right-hand sides
 * keep their layout through diagonal solves.
 *
 * @param A  Coefficient matrix.
 * @param B  Right-hand-side matrix.
 * @return   Newly allocated solution matrix on success, or NULL on error.
 */
matrix_t *mat_solve(const matrix_t *A, const matrix_t *B);

/**
 * @brief Compute a best-fit solution to A X = B.
 *
 * When an exact solution may not exist, this routine returns a matrix X that
 * minimises the residual norm ||A X - B||. For full-column-rank overdetermined
 * systems it uses a QR-based solve. Rank-deficient or underdetermined systems
 * fall back to the Moore-Penrose pseudoinverse so the returned solution remains
 * well defined.
 *
 * Example:
 * @code
 * double A_data[] = {
 *     0.0, 1.0,
 *     1.0, 1.0,
 *     2.0, 1.0
 * };
 * double B_data[] = {
 *     1.0,
 *     3.0,
 *     5.1
 * };
 *
 * matrix_t *A = mat_create_d(3, 2, A_data);
 * matrix_t *B = mat_create_d(3, 1, B_data);
 * matrix_t *X = mat_least_squares(A, B);
 *
 * mat_print(X);
 *
 * mat_free(X);
 * mat_free(B);
 * mat_free(A);
 * @endcode
 *
 * @param A  Coefficient matrix.
 * @param B  Right-hand-side matrix.
 * @return   Newly allocated least-squares solution matrix on success, or
 *           NULL on error.
 */
matrix_t *mat_least_squares(const matrix_t *A, const matrix_t *B);

/**
 * @brief Compute the numerical rank of a matrix.
 *
 * The rank is determined from the singular values of A using the library's
 * internal tolerance policy.
 *
 * @param A  Input matrix.
 * @return   The computed rank, or a negative value on error.
 */
int       mat_rank(const matrix_t *A);

/**
 * @brief Compute the Moore-Penrose pseudoinverse of a matrix.
 *
 * The pseudoinverse generalises the ordinary matrix inverse to rectangular
 * or singular matrices. It is useful for least-squares problems, minimum-
 * norm solutions, projection operators, and for working with matrices that
 * do not admit a true inverse.
 *
 * In particular, the pseudoinverse is useful when solving systems that are
 * overdetermined, underdetermined, or rank-deficient. In those settings it
 * provides the standard inverse-like object used to compute best-fit or
 * minimum-norm solutions.
 *
 * When A is square and nonsingular, the pseudoinverse coincides with the
 * ordinary inverse.
 *
 * @param A  Input matrix.
 * @return   Newly allocated pseudoinverse on success, or NULL on error.
 */
matrix_t *mat_pseudoinverse(const matrix_t *A);

/**
 * @brief Compute a basis for the right nullspace of a matrix.
 *
 * The returned matrix stores basis vectors as its columns. If the nullspace
 * is trivial, the result may have zero columns.
 *
 * @param A  Input matrix.
 * @return   Newly allocated nullspace basis matrix on success, or NULL on
 *           error.
 */
matrix_t *mat_nullspace(const matrix_t *A);

/**
 * @brief Compute a matrix norm.
 *
 * The selected norm is written to @p out as a qfloat_t regardless of the
 * element type of the input matrix.
 *
 * @param A     Input matrix.
 * @param type  Norm to compute.
 * @param out   Output location for the norm value.
 * @return      0 on success, nonzero on error.
 */
int       mat_norm(const matrix_t *A, mat_norm_type_t type, qfloat_t *out);

/**
 * @brief Compute a matrix condition number in a chosen norm.
 *
 * The condition number is written to @p out as a qfloat_t.
 *
 * @param A     Input matrix.
 * @param type  Norm in which to compute the condition number.
 * @param out   Output location for the condition number.
 * @return      0 on success, nonzero on error.
 */
int       mat_condition_number(const matrix_t *A, mat_norm_type_t type, qfloat_t *out);

/**
 * @brief Compute an LU factorisation with pivoting.
 *
 * On success, @p out receives matrices P, L, and U such that P A = L U.
 * The caller becomes responsible for releasing them with mat_lu_factor_free().
 * When the input already uses a sparse-like layout, the permutation and
 * triangular factors keep sparse storage where possible, and the factorisation
 * uses sparse row operations for elimination rather than first materialising
 * the working matrices densely.
 *
 * @param A    Input matrix.
 * @param out  Output factorisation structure.
 * @return     0 on success, nonzero on error.
 */
int mat_lu_factor(const matrix_t *A, mat_lu_factor_t *out);

/**
 * @brief Release storage owned by an LU factorisation result.
 *
 * @param out  Factorisation structure previously filled by mat_lu_factor().
 */
void mat_lu_factor_free(mat_lu_factor_t *out);

/**
 * @brief Compute a QR factorisation.
 *
 * On success, @p out receives matrices Q and R such that A = Q R. The caller
 * becomes responsible for releasing them with mat_qr_factor_free().
 *
 * @param A    Input matrix.
 * @param out  Output factorisation structure.
 * @return     0 on success, nonzero on error.
 */
int mat_qr_factor(const matrix_t *A, mat_qr_factor_t *out);

/**
 * @brief Release storage owned by a QR factorisation result.
 *
 * @param out  Factorisation structure previously filled by mat_qr_factor().
 */
void mat_qr_factor_free(mat_qr_factor_t *out);

/**
 * @brief Compute a Cholesky factorisation.
 *
 * On success, @p out receives a lower-triangular matrix L such that
 * A = L L^H. The input must be Hermitian positive-definite. When the input
 * uses a sparse-like layout, the returned factor keeps sparse storage.
 *
 * @param A    Input matrix.
 * @param out  Output factorisation structure.
 * @return     0 on success, nonzero on error.
 */
int mat_cholesky(const matrix_t *A, mat_cholesky_t *out);

/**
 * @brief Release storage owned by a Cholesky factorisation result.
 *
 * @param out  Factorisation structure previously filled by mat_cholesky().
 */
void mat_cholesky_free(mat_cholesky_t *out);

/**
 * @brief Compute a singular value decomposition.
 *
 * This routine factorises A as
 *
 *   A = U S V^H
 *
 * where U and V contain left and right singular vectors, and S contains the
 * singular values on its diagonal.
 *
 * The singular value decomposition is useful for rank analysis, numerical
 * conditioning, pseudoinverses, least-squares problems, and for working
 * robustly with rectangular or rank-deficient matrices.
 *
 * On success, @p out receives matrices U, S, and V describing this
 * factorisation. The caller becomes responsible for releasing them with
 * mat_svd_factor_free().
 *
 * @param A    Input matrix.
 * @param out  Output factorisation structure.
 * @return     0 on success, nonzero on error.
 */
int mat_svd_factor(const matrix_t *A, mat_svd_factor_t *out);

/**
 * @brief Release storage owned by an SVD result.
 *
 * @param out  Factorisation structure previously filled by mat_svd_factor().
 */
void mat_svd_factor_free(mat_svd_factor_t *out);

/**
 * @brief Compute the Schur factorisation A = Q T Q^H.
 *
 * The Schur factorisation is a numerically stable way to represent a square
 * matrix using a unitary change of basis Q and an upper-triangular matrix T.
 * It is especially useful for non-Hermitian eigenvalue problems and for matrix
 * functions such as exp(A), log(A), and sqrt(A).
 *
 * The returned factors are stored as qcomplex matrices even when the input is
 * real-valued, since a general real matrix may have complex Schur data.
 *
 * @param A    Input square matrix.
 * @param out  Output factorisation structure.
 * @return     0 on success, nonzero on error.
 */
int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out);

/**
 * @brief Release the matrices owned by a Schur factorisation.
 *
 * @param out  Factorisation structure previously filled by
 *             mat_schur_factor().
 */
void mat_schur_factor_free(mat_schur_factor_t *out);

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
matrix_t *mat_erfinv(const matrix_t *A);
matrix_t *mat_erfcinv(const matrix_t *A);
matrix_t *mat_gamma(const matrix_t *A);
matrix_t *mat_lgamma(const matrix_t *A);
matrix_t *mat_digamma(const matrix_t *A);
matrix_t *mat_trigamma(const matrix_t *A);
matrix_t *mat_tetragamma(const matrix_t *A);
matrix_t *mat_gammainv(const matrix_t *A);
matrix_t *mat_normal_pdf(const matrix_t *A);
matrix_t *mat_normal_cdf(const matrix_t *A);
matrix_t *mat_normal_logpdf(const matrix_t *A);
matrix_t *mat_lambert_w0(const matrix_t *A);
matrix_t *mat_lambert_wm1(const matrix_t *A);
matrix_t *mat_productlog(const matrix_t *A);
matrix_t *mat_ei(const matrix_t *A);
matrix_t *mat_e1(const matrix_t *A);

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
