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
 * @brief Borrowed symbolic binding returned by mat_from_string().
 *
 * The @p name pointer and @p symbol handle remain valid for as long as the
 * matrix returned by mat_from_string() remains alive. Releasing the bindings
 * array itself only requires a plain free(bindings).
 */
typedef struct {
    const char *name;
    dval_t *symbol;
    bool is_constant;
} binding_t;

/**
 * @brief Matrix string rendering style.
 */
typedef enum {
    MAT_STRING_INLINE_SCIENTIFIC,
    MAT_STRING_INLINE_PRETTY,
    MAT_STRING_LAYOUT_SCIENTIFIC,
    MAT_STRING_LAYOUT_PRETTY
} mat_string_style_t;

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

/**
 * @brief Parse a matrix from a string.
 *
 * Supported forms are:
 *
 *   (a, b, c; d, e, f)
 *   { (a, b; c, d) | x = 1, y = 2; c1 = 3 }
 *
 * Purely numeric matrices become qfloat or qcomplex matrices depending on
 * whether any entry is genuinely complex. Symbolic matrices become dval
 * matrices. For bare symbolic input without outer braces, all discovered
 * bindings start as NaN and symbol kind is inferred from the name. The bare
 * inference rule treats `a`, `b`, `c`, `d`, and indexed forms such as `c1`,
 * `c_2`, and `d₃` as constants by default; ordinary Latin names such as `x`
 * or `radius`, symbolic parameter names such as `Δ` or `Ω`, and names such
 * as `e`, `π`, and `τ` are treated as variables unless an explicit
 * matrix-wide binding section says otherwise. When @p bindings_out is
 * non-NULL for a symbolic matrix, it receives a heap-allocated array of
 * borrowed bindings for the symbols actually referenced by the matrix body
 * that remain valid while the returned matrix remains alive; releasing that
 * array only requires free(*bindings_out).
 */
matrix_t *mat_from_string(const char *s, binding_t **bindings_out, size_t *number_out);

/**
 * @brief Parse a matrix from a string while reusing an existing symbolic binding table.
 *
 * This behaves like mat_from_string(...), but any symbol name that matches one
 * of the provided @p shared_bindings reuses that underlying symbolic leaf
 * instead of creating a fresh one. This is the easiest way to make separately
 * parsed matrices genuinely share symbols for workflows such as
 * mat_deriv_solve_by_name(...).
 *
 * Explicit bindings in @p s update reused named leaves in place. Reusing the
 * same normalised name with conflicting variable/constant roles is rejected.
 * Newly discovered symbols are still inferred and created as usual.
 *
 * When @p bindings_out is non-NULL, it receives a fresh heap-allocated array
 * describing only the symbols referenced by the parsed matrix body. Reused
 * entries point at the shared underlying leaves.
 */
matrix_t *mat_from_string_with_bindings(const char *s,
                                        binding_t *shared_bindings,
                                        size_t shared_number,
                                        binding_t **bindings_out,
                                        size_t *number_out);

/**
 * @brief Find a returned symbolic binding by name.
 *
 * The lookup uses the normalised binding names returned by mat_from_string(),
 * such as `c₁` for `c1`. Convenience aliases such as `@DELTA`, `@OMEGA`,
 * `@pi`, and `@tau` are also accepted.
 *
 * @return Pointer to the matching borrowed binding, or NULL if not found.
 */
binding_t *mat_binding_get(binding_t *bindings, size_t number, const char *name);

/**
 * @brief Find a returned symbolic binding by name.
 *
 * Synonym for mat_binding_get().
 *
 * @return Pointer to the matching borrowed binding, or NULL if not found.
 */
binding_t *mat_binding_find(binding_t *bindings, size_t number, const char *name);

/**
 * @brief Set a returned symbolic binding from a qfloat_t value.
 *
 * @return 0 on success, or -1 if the named binding is not present.
 */
int mat_binding_set_qf(binding_t *bindings, size_t number, const char *name, qfloat_t value);

/**
 * @brief Set a returned symbolic binding from a qcomplex_t value.
 *
 * @return 0 on success, or -1 if the named binding is not present.
 */
int mat_binding_set_qc(binding_t *bindings, size_t number, const char *name, qcomplex_t value);

/**
 * @brief Set a returned symbolic binding from a double value.
 *
 * @return 0 on success, or -1 if the named binding is not present.
 */
int mat_binding_set_d(binding_t *bindings, size_t number, const char *name, double value);

/**
 * @brief Differentiate a matrix entrywise with respect to a returned binding name.
 *
 * This is a convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv(...). The lookup accepts normalised names such as `Δ`, convenience
 * aliases such as `@DELTA`, and bracketed identifiers such as `[radius]`.
 *
 * @param A         Matrix to differentiate.
 * @param bindings  Borrowed bindings previously returned by mat_from_string().
 * @param number    Number of bindings in @p bindings.
 * @param name      Binding name to differentiate with respect to.
 * @return          Newly allocated derivative matrix on success, or NULL if
 *                  the named binding is not present or inputs are invalid.
 */
matrix_t *mat_deriv_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                            const char *name);

/**
 * @brief Differentiate the trace of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_trace(...).
 *
 * @return Newly allocated symbolic derivative, or NULL on error.
 */
dval_t *mat_deriv_trace_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                                const char *name);

/**
 * @brief Differentiate the determinant of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_det(...).
 *
 * @return Newly allocated symbolic derivative, or NULL on error.
 */
dval_t *mat_deriv_det_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                              const char *name);

/**
 * @brief Build a Jacobian for a matrix-valued symbolic output by binding names.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_jacobian(...). Every requested name must be present in the returned
 * bindings.
 *
 * @param A       Matrix-valued symbolic output.
 * @param bindings Borrowed bindings previously returned by mat_from_string().
 * @param number  Number of bindings in @p bindings.
 * @param names   Array of binding names to differentiate with respect to.
 * @param nnames  Number of names in @p names.
 * @return        Newly allocated Jacobian matrix on success, or NULL if any
 *                name is missing or inputs are invalid.
 */
matrix_t *mat_jacobian_by_names(const matrix_t *A, binding_t *bindings, size_t number,
                                const char *const *names, size_t nnames);

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
 * @brief Evaluate a matrix into qfloat_t form.
 *
 * For dval matrices, each symbolic entry is evaluated at the current variable
 * values and copied into a newly allocated qfloat matrix. The result is a
 * numeric snapshot and does not continue to track later variable changes.
 *
 * For non-dval matrices, this returns a qfloat-valued copy in the same shape.
 *
 * @param A  Input matrix.
 * @return   Newly allocated qfloat matrix on success, or NULL on error.
 */
matrix_t *mat_evaluate_qf(const matrix_t *A);

/**
 * @brief Evaluate a matrix into qcomplex_t form.
 *
 * For dval matrices, each symbolic entry is evaluated at the current variable
 * values and copied into a newly allocated qcomplex matrix with zero imaginary
 * part. The result is a numeric snapshot and does not continue to track later
 * variable changes.
 *
 * For non-dval matrices, this returns a qcomplex-valued copy in the same
 * shape.
 *
 * @param A  Input matrix.
 * @return   Newly allocated qcomplex matrix on success, or NULL on error.
 */
matrix_t *mat_evaluate_qc(const matrix_t *A);

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

/**
 * @brief Return the transpose of a matrix.
 *
 * The result has dimensions `cols(A) × rows(A)` and preserves the element
 * type of `A`.
 *
 * @param A  Matrix to transpose.
 * @return   Newly allocated transpose on success, or NULL on error.
 */
matrix_t *mat_transpose(const matrix_t *A);

/**
 * @brief Return the entrywise complex conjugate of a matrix.
 *
 * For real-valued element types this leaves the numeric values unchanged. For
 * symbolic `dval_t *` matrices, the operation applies the elementwise
 * conjugation rule provided by the symbolic element layer.
 *
 * @param A  Matrix to conjugate.
 * @return   Newly allocated conjugated matrix on success, or NULL on error.
 */
matrix_t *mat_conj(const matrix_t *A);

/**
 * @brief Return the Hermitian transpose of a matrix.
 *
 * This is the conjugate transpose `A^H = conj(transpose(A))`.
 *
 * @param A  Matrix to transform.
 * @return   Newly allocated Hermitian transpose on success, or NULL on error.
 */
matrix_t *mat_hermitian(const matrix_t *A);

/**
 * @brief Differentiate a matrix entrywise with respect to a symbolic variable.
 *
 * Each output entry is the derivative of the corresponding input entry with
 * respect to `wrt`. For non-`dval` matrices the input is treated as constant,
 * so the result is a zero matrix of matching shape.
 *
 * @param A    Matrix to differentiate.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv(const matrix_t *A, dval_t *wrt);

/**
 * @brief Differentiate the trace of a matrix with respect to a symbolic variable.
 *
 * For symbolic matrices this returns the exact derivative of `tr(A)`. For
 * non-`dval` matrices the matrix is treated as constant and symbolic zero is
 * returned.
 *
 * @param A    Matrix whose trace is to be differentiated.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated symbolic derivative, or NULL on error.
 */
dval_t   *mat_deriv_trace(const matrix_t *A, dval_t *wrt);

/**
 * @brief Differentiate the determinant of a matrix with respect to a symbolic variable.
 *
 * For symbolic matrices this returns the exact derivative of `det(A)`. For
 * non-`dval` matrices the matrix is treated as constant and symbolic zero is
 * returned.
 *
 * @param A    Matrix whose determinant is to be differentiated.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated symbolic derivative, or NULL on error.
 */
dval_t   *mat_deriv_det(const matrix_t *A, dval_t *wrt);

/**
 * @brief Differentiate the inverse of a matrix with respect to a symbolic variable.
 *
 * Symbolic matrices use the exact matrix-calculus identity
 * `d(A^{-1}) = -A^{-1}(dA)A^{-1}`. Non-`dval` matrices are treated as
 * constant, so a zero matrix of the appropriate shape is returned.
 *
 * @param A    Matrix whose inverse derivative is requested.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_inverse(const matrix_t *A, dval_t *wrt);

/**
 * @brief Differentiate the inverse of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_inverse(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, binding_t *bindings, size_t number,
                                    const char *name);

/**
 * @brief Differentiate the block inverse of a matrix with respect to a symbolic variable.
 *
 * The matrix is partitioned using the same top-left `split × split` block as
 * `mat_block_inverse(...)`. Symbolic matrices use the block inverse path and
 * exact matrix-calculus rules; non-`dval` matrices are treated as constant and
 * yield a zero matrix.
 *
 * @param A     Matrix whose block inverse derivative is requested.
 * @param split Size of the leading square block.
 * @param wrt   Symbolic differentiation variable.
 * @return      Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_inverse(const matrix_t *A, size_t split, dval_t *wrt);

/**
 * @brief Differentiate the block inverse of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_block_inverse(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_inverse_by_name(const matrix_t *A, size_t split,
                                          binding_t *bindings, size_t number,
                                          const char *name);

/**
 * @brief Build a Jacobian for a matrix-valued symbolic output.
 *
 * The input matrix is flattened in row-major order. The resulting Jacobian has
 * `rows(A) * cols(A)` rows and `nvars` columns, where row
 * `i * cols(A) + j` corresponds to entry `A[i,j]`.
 *
 * For non-`dval` matrices the input is treated as constant, so the returned
 * Jacobian is symbolic zero throughout.
 *
 * @param A      Matrix-valued symbolic output.
 * @param vars   Array of symbolic differentiation variables.
 * @param nvars  Number of variables in `vars`.
 * @return       Newly allocated Jacobian matrix on success, or NULL on error.
 */
matrix_t *mat_jacobian(const matrix_t *A, dval_t *const *vars, size_t nvars);

/**
 * @brief Compute the trace of a square matrix.
 *
 * The output buffer must match the matrix element type: `double`, `qfloat_t`,
 * `qcomplex_t`, or `dval_t *`. For symbolic matrices, a newly built symbolic
 * value is written through `trace`.
 *
 * @param A      Matrix whose trace is requested.
 * @param trace  Output buffer for the trace value.
 * @return       0 on success, or a negative value on error.
 */
int       mat_trace(const matrix_t *A, void *trace);

/**
 * @brief Compute the determinant of a square matrix.
 *
 * The output buffer must match the matrix element type: `double`, `qfloat_t`,
 * `qcomplex_t`, or `dval_t *`. Symbolic `dval` matrices use the exact
 * symbolic determinant path.
 *
 * @param A            Matrix whose determinant is requested.
 * @param determinant  Output buffer for the determinant value.
 * @return             0 on success, or a negative value on error.
 */
int       mat_det(const matrix_t *A, void *determinant);

/**
 * @brief Compute the characteristic polynomial of a square matrix.
 *
 * The polynomial is returned as a column vector of coefficients in descending
 * order, so for an `n × n` matrix the result has shape `(n + 1) × 1`.
 *
 * @param A  Matrix whose characteristic polynomial is requested.
 * @return   Newly allocated coefficient vector on success, or NULL on error.
 */
matrix_t *mat_charpoly(const matrix_t *A);

/**
 * @brief Compute the minimal polynomial of a square matrix.
 *
 * The result is returned as a column vector of coefficients in descending
 * order.
 *
 * @param A  Matrix whose minimal polynomial is requested.
 * @return   Newly allocated coefficient vector on success, or NULL on error.
 */
matrix_t *mat_minpoly(const matrix_t *A);

/**
 * @brief Apply a scalar polynomial to a matrix.
 *
 * The coefficient vector must be supplied in descending order, matching the
 * layout returned by `mat_charpoly(...)` and `mat_minpoly(...)`.
 *
 * @param A       Matrix argument.
 * @param coeffs  Column vector of polynomial coefficients.
 * @return        Newly allocated matrix value `p(A)` on success, or NULL on error.
 */
matrix_t *mat_apply_poly(const matrix_t *A, const matrix_t *coeffs);

/**
 * @brief Compute the adjugate of a square matrix.
 *
 * The adjugate is the transpose of the cofactor matrix and satisfies
 * `A · adj(A) = det(A) I` whenever the product is defined.
 *
 * @param A  Matrix whose adjugate is requested.
 * @return   Newly allocated adjugate on success, or NULL on error.
 */
matrix_t *mat_adjugate(const matrix_t *A);

/**
 * @brief Compute the Schur complement of the leading block of a matrix.
 *
 * With the block partition
 *
 * `A = [A11 A12; A21 A22]`
 *
 * where `A11` is `split × split`, this returns
 *
 * `A22 - A21 A11^{-1} A12`.
 *
 * @param A      Matrix to partition.
 * @param split  Size of the leading square block.
 * @return       Newly allocated Schur complement on success, or NULL on error.
 */
matrix_t *mat_schur_complement(const matrix_t *A, size_t split);

/**
 * @brief Compute the inverse of a matrix from a top-left block partition.
 *
 * Uses the same `split × split` leading block convention as
 * `mat_schur_complement(...)`.
 *
 * @param A      Matrix to invert.
 * @param split  Size of the leading square block.
 * @return       Newly allocated inverse on success, or NULL on error.
 */
matrix_t *mat_block_inverse(const matrix_t *A, size_t split);

/**
 * @brief Differentiate the block solution of `A X = B` with respect to a symbolic variable.
 *
 * Uses the same top-left block partition as `mat_block_solve(...)`. For
 * non-`dval` inputs the matrices are treated as constant and a zero matrix of
 * the solution shape is returned.
 *
 * @param A      Coefficient matrix.
 * @param B      Right-hand-side matrix.
 * @param split  Size of the leading square block.
 * @param wrt    Symbolic differentiation variable.
 * @return       Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_solve(const matrix_t *A, const matrix_t *B, size_t split, dval_t *wrt);

/**
 * @brief Differentiate the block solution of `A X = B` with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_block_solve(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_solve_by_name(const matrix_t *A, const matrix_t *B, size_t split,
                                        binding_t *bindings, size_t number,
                                        const char *name);

/**
 * @brief Compute the inverse of a square matrix.
 *
 * For symbolic `dval` matrices this uses the exact symbolic inverse path.
 *
 * @param A  Matrix to invert.
 * @return   Newly allocated inverse on success, or NULL on error.
 */
matrix_t *mat_inverse(const matrix_t *A);

/**
 * @brief Differentiate the solution of `A X = B` with respect to a symbolic variable.
 *
 * For symbolic matrices this uses the exact matrix-calculus solve derivative.
 * For non-`dval` matrices both inputs are treated as constant and a zero matrix
 * of the solution shape is returned.
 *
 * @param A    Coefficient matrix.
 * @param B    Right-hand-side matrix.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_solve(const matrix_t *A, const matrix_t *B, dval_t *wrt);

/**
 * @brief Differentiate the solution of `A X = B` with respect to a returned binding name.
 *
 * Convenience wrapper around mat_binding_find(...) followed by
 * mat_deriv_solve(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_solve_by_name(const matrix_t *A, const matrix_t *B,
                                  binding_t *bindings, size_t number,
                                  const char *name);

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
matrix_t *mat_block_solve(const matrix_t *A, const matrix_t *B, size_t split);

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
matrix_t *mat_eigenspace(const matrix_t *A, const void *eigenvalue);
matrix_t *mat_generalized_eigenspace(const matrix_t *A, const void *eigenvalue,
                                     size_t order);
matrix_t *mat_jordan_chain(const matrix_t *A, const void *eigenvalue,
                           size_t order);
matrix_t *mat_jordan_profile(const matrix_t *A, const void *eigenvalue);

/* -------------------------------------------------------------------------
   Matrix-function helpers
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

/**
 * @brief Return a copy of a matrix with every symbolic entry simplified.
 *
 * For `MAT_TYPE_DVAL`, each entry is rewritten through the current `dval`
 * simplifier before being stored in the returned matrix. For non-symbolic
 * matrices this is equivalent to a plain copy.
 */
matrix_t *mat_simplify_symbolic(const matrix_t *A);

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

char *mat_to_string(const matrix_t *A, mat_string_style_t style);
int mat_sprintf(char *out, size_t out_size, const char *fmt, ...);
int mat_printf(const char *fmt, ...);
void mat_print(const matrix_t *A);

#endif /* MATRIX_H */
