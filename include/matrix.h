#ifndef MATRIX_H
#define MATRIX_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "expression.h"

/**
 * @file matrix.h
 * @brief Generic high-precision matrix type over `number_t` and `expr_t`.
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
 * Numeric matrices store `number_t` values and symbolic matrices store
 * retained `expr_t *` handles. All operations dispatch through internal
 * vtables. No type switches or storage switches appear in user code.
 */

typedef struct matrix_t matrix_t;
typedef struct mat_bindings_t mat_bindings_t;

/**
 * @brief Matrix string rendering style.
 */
typedef enum {
    MAT_STRING_INLINE_SCIENTIFIC,
    MAT_STRING_INLINE_PRETTY,
    MAT_STRING_LAYOUT_SCIENTIFIC,
    MAT_STRING_LAYOUT_PRETTY,
    MAT_STRING_LATEX,
    /** Native matrix expression with variable and constant bindings. */
    MAT_STRING_EXPRESSION,
    /** Native matrix expression with compact rows, or individually stacked entries when a row is long. */
    MAT_STRING_EXPRESSION_LAYOUT,
    /** Native MARS matrix-function representation with binding declarations. */
    MAT_STRING_FUNCTION
} mat_string_style_t;

/**
 * @brief Matrix element type.
 */
typedef enum { MAT_TYPE_NUMBER, MAT_TYPE_EXPR } mat_type_t;

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
 * @brief Allocate a new (incomplete) matrix of number_t values.
 *
 * Stored values are cloned into matrix-owned storage. When extracting values
 * back out, prefer `mat_get_num()` or `mat_get_data()` so the returned
 * `number_t` objects can be destroyed independently of the matrix.
 */
matrix_t *mat_new(size_t rows, size_t cols);
matrix_t *mat_new_sparse(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) matrix of expr_t* handles.
 *
 * Each stored handle is retained by the matrix. Callers remain responsible
 * for their own references after passing a value to mat_set() or mat_create_expr().
 */
matrix_t *mat_new_expr(size_t rows, size_t cols);
matrix_t *mat_new_sparse_expr(size_t rows, size_t cols);

/**
 * @brief Allocate a new (incomplete) square matrix of number_t values.
 *
 * Entries are initialised to structural zero and may be filled later with
 * mat_set() or mat_set_data().
 */
matrix_t *matsq_new(size_t n);

/**
 * @brief Create a complete identity matrix of number_t values.
 *
 * The result is an `n x n` number matrix with exact zeros off the diagonal
 * and exact ones on the diagonal.
 */
matrix_t *mat_create_identity(size_t n);

/**
 * @brief Create a complete identity matrix of expr_t* handles.
 */
matrix_t *mat_create_identity_expr(size_t n);

/**
 * @brief Create a diagonal matrix of number_t values from its diagonal entries.
 *
 * Each supplied entry is cloned into matrix-owned storage. Off-diagonal
 * entries are structural zero.
 */
matrix_t *mat_create_diagonal(size_t n, const number_t *diagonal);

/**
 * @brief Create a diagonal matrix of expr_t* handles from its diagonal entries.
 */
matrix_t *mat_create_diagonal_expr(size_t n, expr_t *const *diagonal);

/**
 * @brief Create a complete matrix of number_t values from a flat array.
 *
 * Each entry is cloned into matrix-owned storage.
 *
 * @param rows  Number of rows.
 * @param cols  Number of columns.
 * @param data  Pointer to `rows * cols` row-major `number_t` values.
 */
matrix_t *mat_create(size_t rows, size_t cols, const number_t *data);

/**
 * @brief Create a complete matrix of expr_t* handles from a flat array.
 *
 * Each handle is retained by the created matrix.
 */
matrix_t *mat_create_expr(size_t rows, size_t cols, expr_t *const *data);

/**
 * @brief Parse a matrix from a string and return a numeric matrix when fully resolvable.
 *
 * This helper accepts both numeric and symbolic syntax, but its return type is
 * always numeric:
 * - purely numeric input returns a `MAT_TYPE_NUMBER` matrix
 * - symbolic input returns a numeric matrix only if every symbol can be
 *   resolved to a concrete numeric value during parse/evaluation
 *
 * Resolution may come from explicit bindings in wrapped form, for example
 * `{ (x, y; y, x) | x = 2; y = 3/2 }`, and from built-in valued constants
 * such as `e`, `pi`/`π`, `@pi`, `@phi`, and `@gamma`.
 *
 * If any required symbol remains unbound (for example bare variables such as
 * `x`, or partially bound wrapped input), this function returns NULL.
 *
 * This is a convenience wrapper around mat_from_text(...), so internally the
 * parser works with the string module rather than C-string pointer arithmetic.
 *
 * For symbolic parsing where callers need mutable bindings or symbolic output,
 * use mat_from_string_expr(...) or mat_from_text_expr(...).
 */
matrix_t *mat_from_string(const char *s);

/**
 * @brief Parse a matrix from a string object and return a numeric matrix when fully resolvable.
 *
 * This is the string_t-based counterpart to mat_from_string(). The input is
 * borrowed and is not modified.
 */
matrix_t *mat_from_text(const string_t *text);

/**
 * @brief Parse a numeric or symbolic matrix from a string.
 *
 * Supported forms are:
 *
 *   (a, b, c; d, e, f)
 *   { (a, b; c, d) | x = 1, y = 2; c1 = 3 }
 *
 * Purely numeric matrices become `number_t` matrices, preserving exact,
 * floating, or complex numeric entries through the generic number layer.
 * Symbolic matrices become expr matrices. For bare symbolic input without
 * outer braces, all discovered bindings start as NaN and symbol kind is
 * inferred from the name. The bare inference rule matches expr parsing:
 *   - built-in valued constants: `e`, `pi`, `π`, `@pi`, `@phi`, `@gamma`
 *   - constant placeholders: `a`, `b`, `c`, `d`, and indexed forms such as
 *     `c1`, `c_2`, and `d₃`
 *   - variables: everything else, including `x`, `τ`, `@tau`, and bracketed
 *     names such as `[radius]`
 *
 * Suffixed names such as `@pi1` normalise to Greek-with-subscript names such
 * as `π₁`, but remain ordinary symbolic variables rather than built-in
 * constants.
 *
 * Within parenthesised matrices, columns may be separated by commas or by
 * unambiguous top-level whitespace. Thus `(1 2; 4 5)` is equivalent to
 * `(1, 2; 4, 5)`, while whitespace surrounding an operator inside an entry,
 * as in `1 + i`, remains part of that scalar expression.
 *
 * This is a convenience wrapper around mat_from_text_expr(...), so internally
 * the parser works with the string module rather than C-string pointer
 * arithmetic.
 *
 * If @p bnd_out is non-NULL and the parsed matrix is symbolic, the function
 * also returns an opaque bindings object that can be queried with
 * mat_bindings_get_text() or mat_bindings_get(), then later released with
 * mat_bindings_free(). If bindings are not needed, pass NULL.
 */
matrix_t *mat_from_string_expr(const char *s, mat_bindings_t **bnd_out);

/**
 * @brief Parse a numeric or symbolic matrix from a string object.
 *
 * This is the string_t-based counterpart to mat_from_string_expr(). The input
 * is borrowed and is not modified.
 */
matrix_t *mat_from_text_expr(const string_t *text, mat_bindings_t **bnd_out);

/**
 * @brief Parse and evaluate a complete matrix expression from a C string.
 *
 * In addition to matrix literals accepted by mat_from_string_expr(), this parser accepts grouped unary signs,
 * matrix products, scalar matrix powers such as `(1 2; 3 4)^(1/2)` with real or complex `number_t` exponents,
 * inverse(...), every registered unary expression function such as exp(...) and sin(...), and entrywise calculus forms
 * such as Dx(...) and @S^x(...).
 *
 * When @p operation_out is non-NULL, it receives a borrowed static operation name for an explicit operation, or NULL
 * when @p text is a matrix literal alone. When @p bnd_out is non-NULL, bindings referenced by a literal or unary
 * matrix-function argument are returned and remain shared by the resulting symbolic matrix expressions.
 *
 * @param text          Matrix expression to parse and evaluate.
 * @param bnd_out       Optional destination for symbolic bindings.
 * @param operation_out Optional destination for the recognised operation name.
 * @return              Newly allocated result matrix on success, or NULL on error.
 */
matrix_t *mat_expression_from_string(const char *text, mat_bindings_t **bnd_out, const char **operation_out);

/**
 * @brief Parse and evaluate a complete expression whose result may be a matrix or a scalar.
 *
 * This is the typed entry point for matrix-language expressions. In addition to the matrix-valued forms accepted by
 * mat_expression_from_string(), it accepts scalar-valued matrix operations such as `det(A)`, `determinant(A)`, and
 * `|A|`. Exactly one of @p matrix_out and @p scalar_out is populated on success. The caller owns that result.
 *
 * @param text          Expression to parse and evaluate.
 * @param bnd_out       Optional destination for symbolic bindings.
 * @param operation_out Optional destination for the recognised operation name.
 * @param matrix_out    Destination for a newly allocated matrix result, or NULL when the result is scalar.
 * @param scalar_out    Destination for a newly allocated scalar expression, or NULL when the result is a matrix.
 * @return              Zero on success, or non-zero on error.
 */
int mat_expression_evaluate(const char *text, mat_bindings_t **bnd_out, const char **operation_out,
                            matrix_t **matrix_out, expr_t **scalar_out);

/**
 * @brief Discover the symbolic bindings referenced by a matrix.
 *
 * The returned bindings are collected from every symbolic matrix entry and are independent of the parser call that
 * originally produced the matrix. This is useful after operations involving several matrices, where the result may
 * contain bindings contributed by any operand. The caller owns the returned object and must release it with
 * mat_bindings_free().
 *
 * @param A Matrix whose symbolic entries are to be inspected.
 * @return  Newly allocated bindings, or NULL when the matrix has no editable symbolic bindings or an error occurs.
 */
mat_bindings_t *mat_bindings_from_matrix(const matrix_t *A);

/**
 * @brief Look up a parsed matrix binding by string object name.
 *
 * The lookup accepts the same normalised names as the parser. Bracketed names
 * may be queried either as `[radius]` or `radius`. Greek-style aliases are
 * normalised too, so a parsed binding may be queried as either `@pi` or `π`,
 * `@phi` or `φ`, `@gamma` or `γ`, `@tau` or `τ`, `@DELTA` or `Δ`, and
 * `@OMEGA` or `Ω`.
 *
 * The returned @ref expr_t pointer is borrowed from the parsed matrix and
 * remains valid only while that matrix remains alive.
 *
 * @return Borrowed symbolic leaf on success, or NULL if not found.
 */
expr_t *mat_bindings_get_text(mat_bindings_t *bnd, const string_t *name);

/**
 * @brief Convenience wrapper for looking up a parsed matrix binding by name.
 *
 * This accepts a C string for user convenience, then delegates to
 * mat_bindings_get_text().
 *
 * @return Borrowed symbolic leaf on success, or NULL if not found.
 */
expr_t *mat_bindings_get(mat_bindings_t *bnd, const char *name);

/**
 * @brief Return the number of parsed matrix bindings.
 */
size_t mat_bindings_count(const mat_bindings_t *bnd);

/**
 * @brief Borrow a parsed matrix binding name by index.
 */
const char *mat_bindings_name_at(const mat_bindings_t *bnd, size_t index);

/**
 * @brief Borrow a parsed matrix binding name object by index.
 */
const string_t *mat_bindings_name_text_at(const mat_bindings_t *bnd, size_t index);

/**
 * @brief Borrow a parsed matrix binding expression by index.
 */
expr_t *mat_bindings_expr_at(mat_bindings_t *bnd, size_t index);

/**
 * @brief Report whether a parsed matrix binding is a constant.
 */
bool mat_bindings_is_constant_at(const mat_bindings_t *bnd, size_t index);

/**
 * @brief Release a bindings object previously returned by mat_from_string_expr().
 */
void mat_bindings_free(mat_bindings_t *bnd);

/**
 * @brief Differentiate a matrix entrywise with respect to a returned binding name.
 *
 * This is a convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv(...). The lookup accepts normalised names such as `Δ`, convenience
 * aliases such as `@DELTA`, and bracketed identifiers such as `[radius]`.
 *
 * @param A         Matrix to differentiate.
 * @param bindings  Borrowed bindings previously returned by mat_from_string_expr().
 * @param name      Binding name to differentiate with respect to.
 * @return          Newly allocated derivative matrix on success, or NULL if
 *                  the named binding is not present or inputs are invalid.
 */
matrix_t *mat_deriv_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);

/**
 * @brief Integrate a matrix entrywise with respect to a returned binding name.
 *
 * This is a convenience wrapper around mat_bindings_get(...) followed by
 * mat_integrate(...). It returns one antiderivative for each entry and does not
 * append arbitrary integration constants.
 *
 * @param A         Matrix to integrate.
 * @param bindings  Borrowed bindings previously returned by mat_from_string_expr().
 * @param name      Binding name to integrate with respect to.
 * @return          Newly allocated antiderivative matrix on success, or NULL if
 *                  the named binding is not present or an entry is unsupported.
 */
matrix_t *mat_integrate_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);

/**
 * @brief Differentiate the trace of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_trace(...).
 *
 * @return Newly allocated symbolic derivative, or NULL on error.
 */
expr_t *mat_deriv_trace_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);

/**
 * @brief Differentiate the determinant of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_det(...).
 *
 * @return Newly allocated symbolic derivative, or NULL on error.
 */
expr_t *mat_deriv_det_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);

/**
 * @brief Build a Jacobian for a matrix-valued symbolic output by binding names.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_jacobian(...). Every requested name must be present in the returned
 * bindings.
 *
 * @param A       Matrix-valued symbolic output.
 * @param bindings Borrowed bindings previously returned by mat_from_string_expr().
 * @param names   Array of binding names to differentiate with respect to.
 * @param nnames  Number of names in @p names.
 * @return        Newly allocated Jacobian matrix on success, or NULL if any
 *                name is missing or inputs are invalid.
 */
matrix_t *mat_jacobian_by_names(const matrix_t *A, mat_bindings_t *bindings, const char *const *names, size_t nnames);

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
 * This is the low-level accessor. It writes the entry into @p out using the
 * matrix's native stored element representation, so the caller must pass a
 * pointer to the matching underlying type.
 *
 * For numeric matrices, that means @p out should point to a `number_t` which
 * receives the stored numeric entry value directly.
 *
 * For expr matrices, the returned `expr_t *` handle is borrowed from the
 * matrix. Do not call `expr_free()` on it unless you first create or retain your
 * own owning reference by other means.
 *
 * If you want a uniform owning numeric result regardless of the matrix's
 * underlying storage type, use mat_get_num() instead.
 */
void mat_get(const matrix_t *A, size_t i, size_t j, void *out);

/**
 * @brief Read one matrix element as an owning `number_t`.
 *
 * This is the high-level numeric accessor. It always returns an owning
 * `number_t`, regardless of how the entry is stored internally.
 *
 * For `MAT_TYPE_NUMBER`, this returns an independent live clone of the stored
 * value. For other numeric matrix types, the entry is converted into a new
 * `number_t`. For symbolic `MAT_TYPE_EXPR`, the current entry value is
 * evaluated and returned as a new `number_t`.
 *
 * Call `num_destroy(&value)` when finished with the returned value.
 */
number_t mat_get_num(const matrix_t *A, size_t i, size_t j);

/**
 * @brief Store one matrix element from @p val.
 *
 * For expr matrices, the matrix retains the incoming expr_t* handle. The caller
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
bool mat_is_sparse(const matrix_t *A);
size_t mat_nonzero_count(const matrix_t *A);
matrix_t *mat_to_sparse(const matrix_t *A);
matrix_t *mat_to_dense(const matrix_t *A);

/**
 * @brief Evaluate a matrix into number_t form.
 *
 * For expr matrices, each symbolic entry is evaluated at the current variable
 * values and copied into a newly allocated number-valued matrix. The result is
 * a numeric snapshot and does not continue to track later variable changes.
 *
 * For non-expr matrices, this returns a number-valued copy in the same shape.
 * Existing number_t entries preserve their current backend and precision.
 *
 * @param A  Input matrix.
 * @return   Newly allocated number-valued matrix on success, or NULL on error.
 */
matrix_t *mat_evaluate(const matrix_t *A);

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
 * The buffer must contain `rows * cols` `number_t` values.
 *
 * @param A     The matrix to modify.
 * @param data  Pointer to a flat row‑major array of elements.
 */
void mat_set_data(matrix_t *A, const number_t *data);

/**
 * @brief Set all symbolic matrix elements from a flat row-major buffer of `expr_t *`.
 *
 * Each supplied handle is retained by the matrix.
 */
void mat_set_data_expr(matrix_t *A, expr_t *const *data);

/**
 * @brief Low-level bulk setter using the matrix's native stored element representation.
 *
 * This is the native/raw counterpart of `mat_set_data(...)` and
 * `mat_set_data_expr(...)`. The caller must supply a row-major buffer whose
 * element type already matches the matrix's public element family:
 * - `number_t[rows * cols]` for `MAT_TYPE_NUMBER`
 * - `expr_t *[rows * cols]` for `MAT_TYPE_EXPR`
 *
 * In ordinary user code, prefer the typed entry points:
 * - `mat_set_data(...)` for numeric matrices
 * - `mat_set_data_expr(...)` for symbolic matrices
 *
 * Use `mat_set_data_raw(...)` only when writing generic code that has already
 * inspected `mat_typeof(A)` and deliberately wants to feed the matrix its
 * native public element buffer without choosing between the typed APIs ahead
 * of time.
 */
void mat_set_data_raw(matrix_t *A, const void *data);

/**
 * @brief Get all matrix elements into a flat row‑major buffer.
 *
 * Each output slot receives an owning `number_t` value. Call
 * `num_destroy(&data[k])` for every element when finished with the buffer.
 *
 * @param A     The matrix to read from.
 * @param data  Pointer to a flat row‑major array to receive the elements.
 */
void mat_get_data(const matrix_t *A, number_t *data);

/**
 * @brief Get all symbolic matrix elements into a flat row-major `expr_t *` buffer.
 *
 * Returned handles are borrowed from the matrix.
 */
void mat_get_data_expr(const matrix_t *A, expr_t **data);

/**
 * @brief Low-level bulk getter using the matrix's native stored element representation.
 *
 * This is the native/raw counterpart of `mat_get_data(...)` and
 * `mat_get_data_expr(...)`. The destination buffer must already match the
 * matrix's public element family:
 * - `number_t[rows * cols]` for `MAT_TYPE_NUMBER`
 * - `expr_t *[rows * cols]` for `MAT_TYPE_EXPR`
 *
 * In ordinary user code, prefer:
 * - `mat_get_data(...)` when you want owning `number_t` results
 * - `mat_get_data_expr(...)` when you want borrowed symbolic handles
 *
 * Use `mat_get_data_raw(...)` only for generic code that has already branched
 * on `mat_typeof(A)` and wants one low-level bulk access path after making
 * that decision itself.
 */
void mat_get_data_raw(const matrix_t *A, void *data);

/* -------------------------------------------------------------------------
   Basic operations
   ------------------------------------------------------------------------- */

matrix_t *mat_scalar_mul(matrix_t *A, const number_t *s);

matrix_t *mat_scalar_div(matrix_t *A, const number_t *s);

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
 * symbolic `expr_t *` matrices, the operation applies the elementwise
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
 * respect to `wrt`. For non-`expr` matrices the input is treated as constant,
 * so the result is a zero matrix of matching shape.
 *
 * @param A    Matrix to differentiate.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv(const matrix_t *A, expr_t *wrt);

/**
 * @brief Differentiate a matrix successively with respect to an ordered variable sequence.
 *
 * Repeated variables produce higher-order derivatives and distinct variables produce mixed partial derivatives in the
 * supplied order. The sequence must contain at least one non-NULL variable.
 *
 * @param A           Matrix to differentiate.
 * @param count       Number of variables in @p wrts.
 * @param wrts        Ordered differentiation variables.
 * @return            Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_sequence(const matrix_t *A, size_t count, expr_t *const *wrts);

/**
 * @brief Integrate a matrix entrywise with respect to a symbolic variable.
 *
 * Each output entry is one antiderivative of the corresponding input entry
 * with respect to `wrt`. Arbitrary integration constants are deliberately
 * omitted; callers may add an independent constant matrix when required.
 *
 * @param A    Matrix to integrate.
 * @param wrt  Symbolic integration variable.
 * @return     Newly allocated antiderivative matrix on success, or NULL when
 *             an entry cannot be integrated by the available symbolic rules.
 */
matrix_t *mat_integrate(const matrix_t *A, expr_t *wrt);

/**
 * @brief Integrate a matrix successively with respect to an ordered variable sequence.
 *
 * Repeated variables produce repeated antiderivatives and distinct variables produce iterated integrals in the
 * supplied order. Arbitrary integration constants are omitted.
 *
 * @param A           Matrix to integrate.
 * @param count       Number of variables in @p wrts.
 * @param wrts        Ordered integration variables.
 * @return            Newly allocated antiderivative matrix on success, or NULL on error.
 */
matrix_t *mat_integrate_sequence(const matrix_t *A, size_t count, expr_t *const *wrts);

/**
 * @brief Integrate a matrix entrywise and append an arbitrary constant matrix.
 *
 * Each output entry is an antiderivative of the corresponding input entry plus
 * an independent symbolic constant. This represents the complete indefinite
 * integral family, unlike mat_integrate(), which returns one representative.
 *
 * @param A    Matrix to integrate.
 * @param wrt  Symbolic integration variable.
 * @return     Newly allocated antiderivative family on success, or NULL when
 *             an entry cannot be integrated by the available symbolic rules.
 */
matrix_t *mat_integrate_family(const matrix_t *A, expr_t *wrt);

/**
 * @brief Differentiate the trace of a matrix with respect to a symbolic variable.
 *
 * For symbolic matrices this returns the exact derivative of `tr(A)`. For
 * non-`expr` matrices the matrix is treated as constant and symbolic zero is
 * returned.
 *
 * @param A    Matrix whose trace is to be differentiated.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated symbolic derivative, or NULL on error.
 */
expr_t *mat_deriv_trace(const matrix_t *A, expr_t *wrt);

/**
 * @brief Differentiate the determinant of a matrix with respect to a symbolic variable.
 *
 * For symbolic matrices this returns the exact derivative of `det(A)`. For
 * non-`expr` matrices the matrix is treated as constant and symbolic zero is
 * returned.
 *
 * @param A    Matrix whose determinant is to be differentiated.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated symbolic derivative, or NULL on error.
 */
expr_t *mat_deriv_det(const matrix_t *A, expr_t *wrt);

/**
 * @brief Differentiate the inverse of a matrix with respect to a symbolic variable.
 *
 * Symbolic matrices use the exact matrix-calculus identity
 * `d(A^{-1}) = -A^{-1}(dA)A^{-1}`. Non-`expr` matrices are treated as
 * constant, so a zero matrix of the appropriate shape is returned.
 *
 * @param A    Matrix whose inverse derivative is requested.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_inverse(const matrix_t *A, expr_t *wrt);

/**
 * @brief Differentiate the inverse of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_inverse(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);

/**
 * @brief Differentiate the block inverse of a matrix with respect to a symbolic variable.
 *
 * The matrix is partitioned using the same top-left `split × split` block as
 * `mat_block_inverse(...)`. Symbolic matrices use the block inverse path and
 * exact matrix-calculus rules; non-`expr` matrices are treated as constant and
 * yield a zero matrix.
 *
 * @param A     Matrix whose block inverse derivative is requested.
 * @param split Size of the leading square block.
 * @param wrt   Symbolic differentiation variable.
 * @return      Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_inverse(const matrix_t *A, size_t split, expr_t *wrt);

/**
 * @brief Differentiate the block inverse of a matrix with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_block_inverse(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_inverse_by_name(const matrix_t *A, size_t split, mat_bindings_t *bindings, const char *name);

/**
 * @brief Build a Jacobian for a matrix-valued symbolic output.
 *
 * The input matrix is flattened in row-major order. The resulting Jacobian has
 * `rows(A) * cols(A)` rows and `nvars` columns, where row
 * `i * cols(A) + j` corresponds to entry `A[i,j]`.
 *
 * For non-`expr` matrices the input is treated as constant, so the returned
 * Jacobian is symbolic zero throughout.
 *
 * This constructs derivative expression DAGs. It is neither a numeric forward-mode JVP nor a reverse-mode VJP; those
 * evaluations may consume the resulting expressions, but do not alter symbolic matrix-calculus or simplification rules.
 *
 * @param A      Matrix-valued symbolic output.
 * @param vars   Array of symbolic differentiation variables.
 * @param nvars  Number of variables in `vars`.
 * @return       Newly allocated Jacobian matrix on success, or NULL on error.
 */
matrix_t *mat_jacobian(const matrix_t *A, expr_t *const *vars, size_t nvars);

/**
 * @brief Compute the trace of a square matrix as a `number_t`.
 *
 * @param A      Matrix whose trace is requested.
 * @param trace  Output buffer for the trace value.
 * @return       0 on success, or a negative value on error.
 */
int mat_trace(const matrix_t *A, number_t *trace);

/**
 * @brief Compute the trace of a square symbolic matrix as a `expr_t *`.
 *
 * A newly built symbolic value is written through @p trace.
 *
 * @param A      Matrix whose trace is requested.
 * @param trace  Output buffer for the trace value.
 * @return       0 on success, or a negative value on error.
 */
int mat_trace_expr(const matrix_t *A, expr_t **trace);

/**
 * @brief Compute the determinant of a square matrix as a `number_t`.
 *
 * @param A            Matrix whose determinant is requested.
 * @param determinant  Output buffer for the determinant value.
 * @return             0 on success, or a negative value on error.
 */
int mat_det(const matrix_t *A, number_t *determinant);

/**
 * @brief Compute the determinant of a square symbolic matrix as a `expr_t *`.
 *
 * Symbolic `expr` matrices use the exact symbolic determinant path.
 *
 * @param A            Matrix whose determinant is requested.
 * @param determinant  Output buffer for the determinant value.
 * @return             0 on success, or a negative value on error.
 */
int mat_det_expr(const matrix_t *A, expr_t **determinant);

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
 * non-`expr` inputs the matrices are treated as constant and a zero matrix of
 * the solution shape is returned.
 *
 * @param A      Coefficient matrix.
 * @param B      Right-hand-side matrix.
 * @param split  Size of the leading square block.
 * @param wrt    Symbolic differentiation variable.
 * @return       Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_solve(const matrix_t *A, const matrix_t *B, size_t split, expr_t *wrt);

/**
 * @brief Differentiate the block solution of `A X = B` with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_block_solve(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_block_solve_by_name(const matrix_t *A, const matrix_t *B, size_t split, mat_bindings_t *bindings,
                                        const char *name);

/**
 * @brief Compute the inverse of a square matrix.
 *
 * For symbolic `expr` matrices this uses the exact symbolic inverse path.
 *
 * @param A  Matrix to invert.
 * @return   Newly allocated inverse on success, or NULL on error.
 */
matrix_t *mat_inverse(const matrix_t *A);

/**
 * @brief Differentiate the solution of `A X = B` with respect to a symbolic variable.
 *
 * For symbolic matrices this uses the exact matrix-calculus solve derivative.
 * For non-`expr` matrices both inputs are treated as constant and a zero matrix
 * of the solution shape is returned.
 *
 * @param A    Coefficient matrix.
 * @param B    Right-hand-side matrix.
 * @param wrt  Symbolic differentiation variable.
 * @return     Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_solve(const matrix_t *A, const matrix_t *B, expr_t *wrt);

/**
 * @brief Differentiate the solution of `A X = B` with respect to a returned binding name.
 *
 * Convenience wrapper around mat_bindings_get(...) followed by
 * mat_deriv_solve(...).
 *
 * @return Newly allocated derivative matrix on success, or NULL on error.
 */
matrix_t *mat_deriv_solve_by_name(const matrix_t *A, const matrix_t *B, mat_bindings_t *bindings, const char *name);

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
 * minimises the residual norm ||A X - B||. Numeric matrix types use a QR-based
 * solve for full-column-rank overdetermined systems and fall back to the
 * Moore-Penrose pseudoinverse for underdetermined or rank-deficient cases.
 * MAT_TYPE_EXPR supports exact symbolic least-squares through exact symbolic
 * pseudoinverses, including rank-deficient rectangular systems.
 *
 * Example:
 * @code
 * number_t A_data[] = {
 *     NUM_ZERO, num_create_from_double(1.0),
 *     num_create_from_double(1.0), num_create_from_double(1.0),
 *     num_create_from_double(2.0), num_create_from_double(1.0)
 * };
 * number_t B_data[] = {
 *     num_create_from_double(1.0),
 *     num_create_from_double(3.0),
 *     num_create_from_double(5.1)
 * };
 *
 * matrix_t *A = mat_create(3, 2, A_data);
 * matrix_t *B = mat_create(3, 1, B_data);
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
 * @brief Compute the rank of a matrix.
 *
 * Numeric matrix types determine rank from the singular values of A using the
 * library's internal tolerance policy. MAT_TYPE_EXPR uses exact symbolic
 * elimination with exact-zero checks on reduced entries.
 *
 * @param A  Input matrix.
 * @return   The computed rank, or a negative value on error.
 */
int mat_rank(const matrix_t *A);

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
 * Numeric matrix types compute the pseudoinverse via SVD. MAT_TYPE_EXPR
 * supports exact symbolic pseudoinverses for rectangular and rank-deficient
 * inputs through exact full-rank factorisation.
 *
 * @param A  Input matrix.
 * @return   Newly allocated pseudoinverse on success, or NULL on error.
 */
matrix_t *mat_pseudoinverse(const matrix_t *A);

/**
 * @brief Compute a basis for the right nullspace of a matrix.
 *
 * The returned matrix stores basis vectors as its columns. If the nullspace
 * is trivial, the result may have zero columns. MAT_TYPE_EXPR computes this
 * basis exactly by symbolic reduction.
 *
 * @param A  Input matrix.
 * @return   Newly allocated nullspace basis matrix on success, or NULL on
 *           error.
 */
matrix_t *mat_nullspace(const matrix_t *A);

/**
 * @brief Compute a matrix norm.
 *
 * The selected norm is written to @p out as a `number_t`.
 *
 * @p out should point to a fresh or already-cleared `number_t`, such as a
 * value previously initialised with `num_new()`.
 *
 * @param A     Input matrix.
 * @param type  Norm to compute.
 * @param out   Output location for the norm value.
 * @return      0 on success, nonzero on error.
 */
int mat_norm(const matrix_t *A, mat_norm_type_t type, number_t *out);

/**
 * @brief Compute a matrix condition number in a chosen norm.
 *
 * The condition number is written to @p out as a `number_t`.
 *
 * @p out should point to a fresh or already-cleared `number_t`, such as a
 * value previously initialised with `num_new()`.
 *
 * @param A     Input matrix.
 * @param type  Norm in which to compute the condition number.
 * @param out   Output location for the condition number.
 * @return      0 on success, nonzero on error.
 */
int mat_condition_number(const matrix_t *A, mat_norm_type_t type, number_t *out);

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
 * The returned factors are exposed as numeric matrices through the `number_t`
 * layer, even when the underlying Schur data is complex.
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

int mat_eigenvalues(const matrix_t *A, number_t *eigenvalues);
int mat_eigenvalues_expr(const matrix_t *A, expr_t **eigenvalues);
int mat_eigendecompose(const matrix_t *A, number_t *eigenvalues, matrix_t **eigenvectors);
int mat_eigendecompose_expr(const matrix_t *A, expr_t **eigenvalues, matrix_t **eigenvectors);
matrix_t *mat_eigenvectors(const matrix_t *A);
matrix_t *mat_eigenspace(const matrix_t *A, const number_t *eigenvalue);
matrix_t *mat_eigenspace_expr(const matrix_t *A, const expr_t *eigenvalue);
matrix_t *mat_generalized_eigenspace(const matrix_t *A, const number_t *eigenvalue, size_t order);
matrix_t *mat_generalized_eigenspace_expr(const matrix_t *A, const expr_t *eigenvalue, size_t order);
matrix_t *mat_jordan_chain(const matrix_t *A, const number_t *eigenvalue, size_t order);
matrix_t *mat_jordan_chain_expr(const matrix_t *A, const expr_t *eigenvalue, size_t order);
matrix_t *mat_jordan_profile(const matrix_t *A, const number_t *eigenvalue);
matrix_t *mat_jordan_profile_expr(const matrix_t *A, const expr_t *eigenvalue);

/* -------------------------------------------------------------------------
   Matrix-function helpers
   ------------------------------------------------------------------------- */

matrix_t *mat_exp(const matrix_t *A);
matrix_t *mat_sin(const matrix_t *A);
matrix_t *mat_cos(const matrix_t *A);
matrix_t *mat_tan(const matrix_t *A);
/** @brief Return the matrix secant, the inverse of `cos(A)`. */
matrix_t *mat_sec(const matrix_t *A);
/** @brief Return the matrix cosecant, the inverse of `sin(A)`. */
matrix_t *mat_cosec(const matrix_t *A);
/** @brief Return the matrix cotangent. */
matrix_t *mat_cot(const matrix_t *A);
/** @brief Return the matrix versed sine. */
matrix_t *mat_versin(const matrix_t *A);
/** @brief Return the matrix versed cosine. */
matrix_t *mat_vercos(const matrix_t *A);
/** @brief Return the matrix coversed sine. */
matrix_t *mat_coversin(const matrix_t *A);
/** @brief Return the matrix coversed cosine. */
matrix_t *mat_covercos(const matrix_t *A);
/** @brief Return the matrix haversine. */
matrix_t *mat_haversin(const matrix_t *A);
/** @brief Return the matrix havercosine. */
matrix_t *mat_havercos(const matrix_t *A);
/** @brief Return the matrix hacoversine. */
matrix_t *mat_hacoversin(const matrix_t *A);
/** @brief Return the matrix hacovercosine. */
matrix_t *mat_hacovercos(const matrix_t *A);

matrix_t *mat_sinh(const matrix_t *A);
matrix_t *mat_cosh(const matrix_t *A);
matrix_t *mat_tanh(const matrix_t *A);
/** @brief Return the matrix hyperbolic secant. */
matrix_t *mat_sech(const matrix_t *A);
/** @brief Return the matrix hyperbolic cosecant. */
matrix_t *mat_cosech(const matrix_t *A);
/** @brief Return the matrix hyperbolic cotangent. */
matrix_t *mat_coth(const matrix_t *A);

matrix_t *mat_sqrt(const matrix_t *A);
/** @brief Return the principal matrix cube root. */
matrix_t *mat_cubrt(const matrix_t *A);
matrix_t *mat_log(const matrix_t *A);
/**
 * @brief Return the principal matrix natural logarithm of @p A.
 *
 * This is a shorthand for mat_log().
 *
 * @param A Input matrix.
 * @return A newly allocated matrix equal to mat_log(A), or NULL on error.
 */
matrix_t *mat_ln(const matrix_t *A);
matrix_t *mat_log10(const matrix_t *A);
/**
 * @brief Return the principal matrix common logarithm of @p A.
 *
 * This is a shorthand for mat_log10().
 *
 * @param A Input matrix.
 * @return A newly allocated matrix equal to mat_log10(A), or NULL on error.
 */
matrix_t *mat_lg(const matrix_t *A);

matrix_t *mat_asin(const matrix_t *A);
matrix_t *mat_acos(const matrix_t *A);
matrix_t *mat_atan(const matrix_t *A);
/** @brief Return the principal matrix arcsecant. */
matrix_t *mat_asec(const matrix_t *A);
/** @brief Return the principal matrix arccosecant. */
matrix_t *mat_acosec(const matrix_t *A);
/** @brief Return the principal matrix arccotangent. */
matrix_t *mat_acot(const matrix_t *A);
/** @brief Return the principal inverse matrix versed sine. */
matrix_t *mat_arcversin(const matrix_t *A);
/** @brief Return the principal inverse matrix versed cosine. */
matrix_t *mat_arcvercos(const matrix_t *A);
/** @brief Return the principal inverse matrix coversed sine. */
matrix_t *mat_arccoversin(const matrix_t *A);
/** @brief Return the principal inverse matrix coversed cosine. */
matrix_t *mat_arccovercos(const matrix_t *A);
/** @brief Return the principal inverse matrix haversine. */
matrix_t *mat_archaversin(const matrix_t *A);
/** @brief Return the principal inverse matrix havercosine. */
matrix_t *mat_archavercos(const matrix_t *A);
/** @brief Return the principal inverse matrix hacoversine. */
matrix_t *mat_archacoversin(const matrix_t *A);
/** @brief Return the principal inverse matrix hacovercosine. */
matrix_t *mat_archacovercos(const matrix_t *A);

matrix_t *mat_asinh(const matrix_t *A);
matrix_t *mat_acosh(const matrix_t *A);
matrix_t *mat_atanh(const matrix_t *A);
/** @brief Return the principal inverse matrix hyperbolic secant. */
matrix_t *mat_asech(const matrix_t *A);
/** @brief Return the principal inverse matrix hyperbolic cosecant. */
matrix_t *mat_acosech(const matrix_t *A);
/** @brief Return the principal inverse matrix hyperbolic cotangent. */
matrix_t *mat_acoth(const matrix_t *A);

matrix_t *mat_erf(const matrix_t *A);
matrix_t *mat_erfc(const matrix_t *A);
matrix_t *mat_erfinv(const matrix_t *A);
matrix_t *mat_erfcinv(const matrix_t *A);
matrix_t *mat_gamma(const matrix_t *A);
matrix_t *mat_lgamma(const matrix_t *A);
matrix_t *mat_digamma(const matrix_t *A);
/**
 * @brief Evaluate the q-digamma matrix function @f$\psi_q(Z)@f$.
 *
 * The matrix occupies the analytic argument and @p q is a scalar base.  The
 * defining Lambert series is used when its matrix series converges,
 * reciprocal-base continuation is used outside the unit circle, and
 * @f$q=1@f$ selects mat_digamma().
 *
 * @param Z Square matrix argument.
 * @param q Scalar base.
 * @return A newly allocated result matrix, or NULL outside implemented convergence coverage.
 */
matrix_t *mat_qdigamma(const matrix_t *Z, const number_t *q);
matrix_t *mat_trigamma(const matrix_t *A);
matrix_t *mat_tetragamma(const matrix_t *A);
/** @brief Return the analytically continued Riemann zeta matrix function. */
matrix_t *mat_zeta(const matrix_t *A);
/** @brief Return the derivative of the Riemann zeta matrix function. */
matrix_t *mat_zetap(const matrix_t *A);
/** @brief Return the principal matrix dilogarithm. */
matrix_t *mat_dilog(const matrix_t *A);
/** @brief Return the principal order-one polylogarithm matrix function. */
matrix_t *mat_polylog1(const matrix_t *A);
/**
 * @brief Evaluate the harmonic matrix polynomial @f$H_n(A)=\sum_{k=1}^{n}A^k/k@f$.
 *
 * @param A Square matrix argument.
 * @param degree Non-negative polynomial degree.
 * @return A newly allocated matrix containing the finite logarithmic sum, or NULL on error.
 */
matrix_t *mat_harmonic_poly(const matrix_t *A, unsigned int degree);
/**
 * @brief Evaluate the Lerch matrix function Phi(Z,s,a) by its convergent power series.
 *
 * @param Z Square matrix argument.
 * @param s Scalar exponent.
 * @param a Scalar shift.
 * @return A newly allocated result matrix, or NULL outside implemented convergence coverage.
 */
matrix_t *mat_lerch_phi(const matrix_t *Z, const number_t *s, const number_t *a);
matrix_t *mat_gammainv(const matrix_t *A);
matrix_t *mat_normal_pdf(const matrix_t *A);
matrix_t *mat_normal_cdf(const matrix_t *A);
matrix_t *mat_normal_logpdf(const matrix_t *A);
matrix_t *mat_lambert_w0(const matrix_t *A);
matrix_t *mat_lambert_wm1(const matrix_t *A);
matrix_t *mat_productlog(const matrix_t *A);
matrix_t *mat_Ei(const matrix_t *A);
/** @brief Evaluate the principal logarithmic integral of a square matrix. */
matrix_t *mat_Li(const matrix_t *A);
matrix_t *mat_E1(const matrix_t *A);

/**
 * @brief Return a copy of a matrix with every symbolic entry simplified.
 *
 * For `MAT_TYPE_EXPR`, each entry is rewritten through the current `expr`
 * simplifier before being stored in the returned matrix. For non-symbolic
 * matrices this is equivalent to a plain copy.
 */
matrix_t *mat_simplify_symbolic(const matrix_t *A);

/**
 * @brief Return a copy of a matrix whose symbolic entries have been arranged for readable presentation.
 *
 * The scalar expression beautifier is applied after ordinary symbolic simplification. Non-symbolic matrices are
 * copied unchanged. The caller owns the returned matrix.
 */
matrix_t *mat_beautify_symbolic(const matrix_t *A);

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
 * @brief Principal matrix power for a real or complex exponent.
 *
 * The exponent is supplied as a `number_t`, matching the matrix library's
 * numeric element model directly.
 *
 * Exact 1x1 and 2x2 matrices raised to one half retain an exact symbolic principal square root, including any required
 * real or complex surds. Other diagonalisable matrices are evaluated by applying the scalar principal power to every
 * eigenvalue. Defective matrices fall back to `exp(s * log(A))` and therefore require a principal matrix logarithm.
 * Returns NULL on error or if `s` is NULL.
 */
matrix_t *mat_pow(const matrix_t *A, const number_t *s);

/**
 * @brief Symbolic principal matrix power for an expression exponent.
 *
 * Exact real 1x1 and eligible 2x2 matrices use symbolic spectral projectors. Other diagonalizable numeric square
 * matrices use a numeric eigendecomposition while retaining the exponent in the resulting expression DAGs. Returns
 * NULL when no supported spectral representation is available.
 */
matrix_t *mat_pow_expr(const matrix_t *A, const expr_t *exponent);

/**
 * @brief Differentiate a constant square matrix raised to an expression exponent.
 *
 * This differentiates the scalar spectral powers before reconstructing the matrix. Exact symbolic projectors are used
 * when available; otherwise a numeric eigendecomposition is used for a diagonalizable numeric matrix. This operation
 * builds an exact symbolic derivative where the spectral representation is exact and is independent of the numeric
 * forward- or reverse-mode automatic-differentiation path. The caller owns the returned expression matrix.
 */
matrix_t *mat_deriv_pow_expr(const matrix_t *A, const expr_t *exponent, expr_t *wrt);

/**
 * @brief Differentiate a constant square-matrix power through an ordered variable sequence.
 *
 * The scalar spectral powers are differentiated before the matrix is reconstructed. Repeated and mixed partial
 * derivatives are retained in the order supplied by @p wrts.
 *
 * @param A           Constant square matrix.
 * @param exponent    Symbolic exponent.
 * @param count       Number of variables in @p wrts.
 * @param wrts        Ordered differentiation variables.
 * @return            Newly allocated derivative matrix on success, or NULL when no supported spectral rule is available.
 */
matrix_t *mat_deriv_pow_expr_sequence(const matrix_t *A, const expr_t *exponent, size_t count, expr_t *const *wrts);

/**
 * @brief Integrate a constant square matrix raised to an expression exponent.
 *
 * This integrates each scalar spectral power and then reconstructs the matrix. For an exponent equal to the integration
 * variable, this is equivalent to `A^exponent * inverse(log(A))` when the logarithm is invertible, while correctly
 * integrating an eigenvalue-one projector as a linear term. Exact symbolic projectors are used when available;
 * otherwise a numeric eigendecomposition is used for a diagonalizable numeric matrix. Arbitrary constants are omitted.
 */
matrix_t *mat_integrate_pow_expr(const matrix_t *A, const expr_t *exponent, expr_t *wrt);

/**
 * @brief Integrate a constant square-matrix power through an ordered variable sequence.
 *
 * The scalar spectral powers are integrated before the matrix is reconstructed. Arbitrary integration constants are
 * omitted so that callers can append the appropriate constant matrix after matrix simplification and beautification.
 *
 * @param A           Constant square matrix.
 * @param exponent    Symbolic exponent.
 * @param count       Number of variables in @p wrts.
 * @param wrts        Ordered integration variables.
 * @return            Newly allocated antiderivative matrix on success, or NULL when no supported spectral rule is available.
 */
matrix_t *mat_integrate_pow_expr_sequence(const matrix_t *A, const expr_t *exponent, size_t count,
                                          expr_t *const *wrts);

/* -------------------------------------------------------------------------
   Debugging / I/O
   ------------------------------------------------------------------------- */

/**
 * @brief Convert a matrix to newly allocated text.
 *
 * This is the preferred string-owning formatter. The caller owns the returned
 * string and must release it with string_free().
 */
string_t *mat_to_text(const matrix_t *A, mat_string_style_t style);

/**
 * @brief Convert a matrix to a newly allocated C string.
 *
 * Compatibility wrapper around mat_to_text(). The caller owns the returned
 * buffer and must release it with free().
 */
char *mat_to_string(const matrix_t *A, mat_string_style_t style);

/**
 * @brief Convert a matrix body to newly allocated text without serialising bindings.
 *
 * This formatter preserves symbolic entries but omits the outer binding wrapper. It is intended for interfaces that
 * present editable bindings separately. The caller owns the returned string and must release it with string_free().
 */
string_t *mat_body_to_text(const matrix_t *A, mat_string_style_t style);

/**
 * @brief Convert a matrix body to a newly allocated C string without serialising bindings.
 *
 * Compatibility wrapper around mat_body_to_text(). The caller owns the returned buffer and must release it with free().
 */
char *mat_body_to_string(const matrix_t *A, mat_string_style_t style);

/**
 * @brief Format matrix-aware text into a new string_t from a va_list.
 *
 * Supports ordinary string formatting plus matrix conversions:
 * `%m` inline pretty, `%M` inline scientific, `%ml` layout pretty, and
 * `%ML` layout scientific.
 */
string_t *mat_vsprintf_text(const char *fmt, va_list ap);

/**
 * @brief Format matrix-aware text into a new string_t.
 *
 * The caller owns the returned string and must release it with string_free().
 */
string_t *mat_sprintf_text(const char *fmt, ...);

int mat_sprintf(char *out, size_t out_size, const char *fmt, ...);
int mat_printf(const char *fmt, ...);
void mat_print(const matrix_t *A);

/**
 * @brief Serialise a matrix into a SQLite-ready payload.
 *
 * The payload uses the matrix text format that can be read back with
 * @c mat_from_text_expr(). On success, the caller owns @p out_type,
 * @p out_encoding, and @p out_data and must release them with
 * @c string_free() and @c free().
 *
 * @param A Matrix to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool mat_serialize(const matrix_t *A, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);

/**
 * @brief Reconstruct a matrix from a serialised payload.
 *
 * This deserialiser accepts matrices containing either numeric or symbolic
 * entries.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated matrix on success, otherwise @c NULL.
 */
matrix_t *mat_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);

#endif /* MATRIX_H */
