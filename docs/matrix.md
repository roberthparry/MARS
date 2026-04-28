# `matrix_t`

`matrix_t` is a generic high-precision matrix type with pluggable element types
(`double`, `qfloat_t`, `qcomplex_t`, `dval_t *`) and pluggable storage kinds (dense, sparse,
identity, diagonal, upper triangular, lower triangular). All operations dispatch
through internal vtables; no type switches or storage switches appear in user code.

## Representation

`matrix_t` is an opaque struct. Clients hold a pointer and access it only through
the API. Internally each matrix carries:

- a pointer to an element vtable (arithmetic, conversion, formatting)
- a pointer to a storage vtable (construction, get/set, layout queries)
- a `rows × cols` size pair
- a storage payload whose layout depends on the chosen storage kind

## Capabilities

- element types: `double`, `qfloat_t` (~31–32 decimal digits), `qcomplex_t` (~31–32 decimal digits), `dval_t *` (symbolic differentiable values)
- storage kinds:
  - dense (fully materialised)
  - sparse (stores only non-zero elements explicitly)
  - identity (zero storage, materialises on write)
  - diagonal (stores only the main diagonal)
  - upper triangular (stores entries on and above the diagonal)
  - lower triangular (stores entries on and below the diagonal)
- arithmetic: scalar multiply/divide, matrix add, subtract, multiply
- structural: transpose, conjugate, Hermitian (conjugate transpose)
- linear algebra: determinant, inverse, eigenvalues, eigendecomposition, eigenvectors, solve, least-squares, rank, pseudoinverse, nullspace
- matrix norms: 1-norm, infinity-norm, Frobenius norm, 2-norm
- matrix factorisations: LU, QR, Cholesky, SVD, Schur
- condition number computation
- matrix functions: exp, sin, cos, tan, sinh, cosh, tanh, sqrt, log, asin, acos, atan, asinh, acosh, atanh, erf, erfc
- power functions: integer power (binary exponentiation), real power via exp/log
- all eigendecomposition computed at full `qfloat_t`/`qcomplex_t` precision regardless of element type; all matrix functions computed at full `qcomplex_t` precision

## `dval_t *` Matrices

`matrix_t` also supports symbolic `dval_t *` elements through `MAT_TYPE_DVAL`.
These matrices retain every stored `dval_t *` handle, so overwrites, copies,
materialisation, and destruction are reference-count safe.

What currently works for `dval` matrices:

- construction in dense, sparse, identity, diagonal, and compatible structured layouts
- element access, copy, transpose, conjugate
- add, subtract, multiply
- scalar multiply/divide through the normal promotion rules
- exact `1×1` and `2×2` inverse
- symbolic matrix functions for exact structured square inputs
  - diagonal matrices
  - upper- and lower-triangular matrices
  - repeated-diagonal triangular cases such as Jordan blocks
- symbolic pretty-printing with one shared binding footer for the whole matrix

What is still intentionally unsupported for `dval` matrices:

- general numerical inverse / solve / least-squares
- LU / QR / Cholesky / SVD / Schur
- numerical eigensolvers and pseudoinverse
- general dense Schur-based matrix functions on arbitrary `dval` inputs

The current design is to use `matrix<dval_t *>` for symbolic construction,
differentiation, and exact structured operations, then evaluate to a numeric
matrix type when you want the full numerical linear-algebra toolbox.

## Example

```c
#include <stdio.h>
#include "matrix.h"
#include "qcomplex.h"
#include "qfloat.h"

int main(void) {
    /* Hermitian 2×2 matrix with eigenvalues 1 and 4:
     *   [ 2    1+i ]
     *   [ 1-i  3   ]
     */
    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(2.0), qf_from_double(0.0)),
        qc_make(qf_from_double(1.0), qf_from_double(1.0)),
        qc_make(qf_from_double(1.0), qf_from_double(-1.0)),
        qc_make(qf_from_double(3.0), qf_from_double(0.0))
    };
    matrix_t *A = mat_create_qc(2, 2, A_vals);

    qcomplex_t eigenvalues[2];
    matrix_t  *evecs = NULL;
    mat_eigendecompose(A, eigenvalues, &evecs);

    qc_printf("eigenvalue[0] = %z\n", eigenvalues[0]);
    qc_printf("eigenvalue[1] = %z\n", eigenvalues[1]);

    mat_free(A);
    mat_free(evecs);
    return 0;
}
```

Expected output:

```text
eigenvalue[0] = 1 + 0i
eigenvalue[1] = 4 + 0i
```

---

## API Reference

All declarations are in `include/matrix.h`.

### Types

```c
typedef enum {
    MAT_NORM_1,      /* 1-norm (max column sum) */
    MAT_NORM_INF,    /* Infinity-norm (max row sum) */
    MAT_NORM_FRO,    /* Frobenius norm */
    MAT_NORM_2       /* 2-norm (largest singular value) */
} mat_norm_type_t;

typedef struct {
    matrix_t *P;  /* Permutation matrix */
    matrix_t *L;  /* Lower triangular */
    matrix_t *U;  /* Upper triangular */
} mat_lu_factor_t;

typedef struct {
    matrix_t *Q;  /* Unitary/orthogonal factor */
    matrix_t *R;  /* Upper triangular factor */
} mat_qr_factor_t;

typedef struct {
    matrix_t *L;  /* Lower triangular factor */
} mat_cholesky_t;

typedef struct {
    matrix_t *U;  /* Left singular vectors */
    matrix_t *S;  /* Diagonal singular values */
    matrix_t *V;  /* Right singular vectors */
} mat_svd_factor_t;

typedef struct {
    matrix_t *Q;  /* Unitary Schur vectors */
    matrix_t *T;  /* Upper-triangular Schur form */
} mat_schur_factor_t;
```

### Construction

#### Allocate without filling

Use these when you need to fill elements sparsely (e.g. a single diagonal or one
off-diagonal element). For bulk initialisation prefer the `mat_create_*` forms below.

| Function | Element type | Description |
|---|---|---|
| `mat_new_d(rows, cols)` | `double` | Allocate an uninitialised `rows × cols` matrix of doubles |
| `mat_new_qf(rows, cols)` | `qfloat_t` | Allocate an uninitialised `rows × cols` matrix of `qfloat_t` |
| `mat_new_qc(rows, cols)` | `qcomplex_t` | Allocate an uninitialised `rows × cols` matrix of `qcomplex_t` |
| `mat_new_dv(rows, cols)` | `dval_t *` | Allocate an uninitialised `rows × cols` matrix of retained `dval_t *` handles |
| `mat_new_sparse_d(rows, cols)` | `double` | Allocate an uninitialised sparse `rows × cols` matrix of doubles |
| `mat_new_sparse_qf(rows, cols)` | `qfloat_t` | Allocate an uninitialised sparse `rows × cols` matrix of `qfloat_t` |
| `mat_new_sparse_qc(rows, cols)` | `qcomplex_t` | Allocate an uninitialised sparse `rows × cols` matrix of `qcomplex_t` |
| `mat_new_sparse_dv(rows, cols)` | `dval_t *` | Allocate an uninitialised sparse `rows × cols` matrix of retained `dval_t *` handles |
| `matsq_new_d(n)` | `double` | Allocate an uninitialised `n × n` matrix of doubles |
| `matsq_new_qf(n)` | `qfloat_t` | Allocate an uninitialised `n × n` matrix of `qfloat_t` |
| `matsq_new_qc(n)` | `qcomplex_t` | Allocate an uninitialised `n × n` matrix of `qcomplex_t` |
| `matsq_new_dv(n)` | `dval_t *` | Allocate an uninitialised `n × n` matrix of retained `dval_t *` handles |

#### Allocate and fill from a flat array

| Function | Element type | Description |
|---|---|---|
| `mat_create_d(rows, cols, data)` | `double` | Allocate and fill from a row-major `double[]` |
| `mat_create_qf(rows, cols, data)` | `qfloat_t` | Allocate and fill from a row-major `qfloat_t[]` |
| `mat_create_qc(rows, cols, data)` | `qcomplex_t` | Allocate and fill from a row-major `qcomplex_t[]` |
| `mat_create_dv(rows, cols, data)` | `dval_t *` | Allocate and fill from a row-major `dval_t * []`; each handle is retained by the matrix |

#### Identity matrices

Identity matrices carry no element storage. The first write to any element
materialises the matrix as dense.

| Function | Element type | Description |
|---|---|---|
| `mat_create_identity_d(n)` | `double` | `n × n` identity matrix of doubles |
| `mat_create_identity_qf(n)` | `qfloat_t` | `n × n` identity matrix of `qfloat_t` |
| `mat_create_identity_qc(n)` | `qcomplex_t` | `n × n` identity matrix of `qcomplex_t` |
| `mat_create_identity_dv(n)` | `dval_t *` | `n × n` identity matrix of symbolic ones and zeros |

#### Diagonal matrices

Diagonal matrices store only their main diagonal explicitly. They are useful
when every off-diagonal entry is known to be zero and you want that structure
to survive through compatible operations.

| Function | Element type | Description |
|---|---|---|
| `mat_create_diagonal_d(n, diagonal)` | `double` | `n × n` diagonal matrix of doubles |
| `mat_create_diagonal_qf(n, diagonal)` | `qfloat_t` | `n × n` diagonal matrix of `qfloat_t` |
| `mat_create_diagonal_qc(n, diagonal)` | `qcomplex_t` | `n × n` diagonal matrix of `qcomplex_t` |
| `mat_create_diagonal_dv(n, diagonal)` | `dval_t *` | `n × n` diagonal matrix of retained `dval_t *` handles |

### Destruction

- `void mat_free(matrix_t *A)` — free all memory owned by `A`, including element storage.

### Element Access

- `void mat_get(const matrix_t *A, size_t i, size_t j, void *out)` — write the element at row `i`, column `j` into the buffer `out`. `out` must be large enough for the element type. For `MAT_TYPE_DVAL`, the written value is a borrowed `dval_t *`.
- `void mat_set(matrix_t *A, size_t i, size_t j, const void *val)` — copy `val` into position `(i, j)`. For `MAT_TYPE_DVAL`, the matrix retains the incoming handle.
- `size_t mat_get_row_count(const matrix_t *A)` — number of rows.
- `size_t mat_get_col_count(const matrix_t *A)` — number of columns.

Sparse matrices are useful when most entries are zero and only a relatively
small number of values need to be stored explicitly. Typical examples include
diagonal matrices, banded matrices, graph adjacency matrices, and large linear
systems where each row contains only a few nonzero coefficients.

Use the sparse helpers when you want to inspect whether a matrix is currently
stored sparsely, count its nonzero entries, convert a dense matrix to sparse
form to save space, or materialise a sparse matrix as dense form for printing
or for algorithms that expect all entries to be stored explicitly.

- `bool mat_is_sparse(const matrix_t *A)` — returns true if matrix uses sparse storage.
- `size_t mat_nonzero_count(const matrix_t *A)` — number of non-zero elements.
- `matrix_t *mat_to_sparse(const matrix_t *A)` — convert to sparse storage.
- `matrix_t *mat_to_dense(const matrix_t *A)` — convert to dense storage.

Structural queries answer a mathematical question about the entries of a matrix
rather than merely describing its internal storage. They are useful when
checking factorisation results or deciding whether a specialised algorithm is
applicable.

- `bool mat_is_diagonal(const matrix_t *A)` — returns true when all off-diagonal entries are zero.
- `bool mat_is_upper_triangular(const matrix_t *A)` — returns true when all entries below the diagonal are zero.
- `bool mat_is_lower_triangular(const matrix_t *A)` — returns true when all entries above the diagonal are zero.

When an operation has an obviously structured result, the library tries to keep
that structure instead of immediately falling back to a fully materialised dense
matrix. In particular, factorisation outputs and compatible diagonal or
triangular matrix-function results preserve their natural layout. This also
applies to compatible `exp(log(A))` round-trips, so diagonal or triangular
inputs are not flattened merely because an internal cache is involved.

### Bulk Element Access

- `void mat_set_data(matrix_t *A, const void *data)` — copy all elements from a flat row-major buffer into `A`. The buffer must contain `rows × cols` elements of `A`'s element type. For `MAT_TYPE_DVAL`, each incoming handle is retained.
- `void mat_get_data(const matrix_t *A, void *data)` — copy all elements of `A` into a flat row-major buffer. The buffer must have space for `rows × cols` elements of `A`'s element type. For `MAT_TYPE_DVAL`, the copied handles are borrowed.

### Scalar Operations

Scalar functions return a new matrix. The scalar is broadcast to every element.
Mixed scalar/element types are supported; the result element type is the wider of
the two.

| Function | Scalar type | Description |
|---|---|---|
| `mat_scalar_mul_d(A, s)` | `double` | `s * A` |
| `mat_scalar_mul_qf(A, s)` | `qfloat_t` | `s * A` |
| `mat_scalar_mul_qc(A, s)` | `qcomplex_t` | `s * A` |
| `mat_scalar_div_d(A, s)` | `double` | `A / s` |
| `mat_scalar_div_qf(A, s)` | `qfloat_t` | `A / s` |
| `mat_scalar_div_qc(A, s)` | `qcomplex_t` | `A / s` |

### Matrix Operations

All binary operations require conforming dimensions (same shape for add/sub,
compatible shapes for mul). They return a newly allocated matrix.

| Function | Description |
|---|---|
| `mat_add(A, B)` | `A + B` — element-wise addition |
| `mat_sub(A, B)` | `A - B` — element-wise subtraction |
| `mat_mul(A, B)` | `A * B` — matrix multiplication |
| `mat_transpose(A)` | Transpose: `(A^T)_{ij} = A_{ji}` |
| `mat_conj(A)` | Element-wise complex conjugate |
| `mat_hermitian(A)` | Conjugate transpose: `A^† = conj(A)^T` |

### Linear Algebra

#### Determinant

```c
int mat_det(const matrix_t *A, void *determinant);
```

Computes the determinant of square matrix `A` and writes it into `determinant`
(caller-allocated, sized for the element type).

Return values:

| Code | Meaning |
|---|---|
| `0` | Success |
| `-1` | `A` is NULL |
| `-2` | `A` is not square |
| `-3` | Allocation failure |

#### Inverse

```c
matrix_t *mat_inverse(const matrix_t *A);
```

Returns a newly allocated matrix containing the inverse of `A`, or NULL if `A`
is NULL, not square, or singular.

For `MAT_TYPE_DVAL`, the current exact inverse support is limited to `1×1` and
`2×2` matrices.

#### Solve

```c
matrix_t *mat_solve(const matrix_t *A, const matrix_t *B);
```

Solves the linear matrix equation `A X = B` for `X`. The coefficient matrix
`A` must be square and nonsingular, while `B` may contain one or more
right-hand sides.

When `A` is diagonal, upper triangular, or lower triangular, the library uses
direct substitution rather than first treating the problem as a dense general
system. General sparse systems go through LU factorisation followed by
triangular substitution on sparse-like working matrices. Diagonal solves also preserve compatible
right-hand-side layouts, so a sparse `B` stays sparse when the solution has
the same zero pattern.

#### Eigenvalues

```c
int mat_eigenvalues(const matrix_t *A, void *eigenvalues);
```

Computes all eigenvalues of square matrix `A` and writes them into the
caller-allocated buffer `eigenvalues`, which must hold `n` values of the matrix's
element type.

- **Hermitian matrices** — cyclic Jacobi sweep computed entirely in `qfloat_t`
  precision (~31 decimal digits); eigenvalues are real.
- **General matrices** — Hessenberg reduction followed by implicit single-shift QR
  (Francis algorithm); all internal arithmetic uses `qcomplex_t`. Eigenvalues may
  be complex even when `A` has real elements.

Return values: `0` on success, negative on error.

#### Eigendecomposition

```c
int mat_eigendecompose(const matrix_t *A,
                       void *eigenvalues,
                       matrix_t **eigenvectors);
```

Computes eigenvalues and eigenvectors simultaneously. `eigenvalues` may be NULL
if only eigenvectors are needed. On success `*eigenvectors` is set to a newly
allocated `n × n` matrix whose columns are the eigenvectors.

- **Hermitian matrices** — Jacobi path; eigenvectors are orthonormal.
- **General matrices** — Schur decomposition path; eigenvectors are computed by
  back-substitution from the upper triangular Schur factor, then multiplied by
  the unitary Schur factor Q to transform back to the original basis.

Return values: `0` on success, negative on error.

#### Eigenvectors only

```c
matrix_t *mat_eigenvectors(const matrix_t *A);
```

Convenience wrapper around `mat_eigendecompose` that discards the eigenvalues.
Returns a newly allocated eigenvector matrix, or NULL on error.

#### Linear Systems

```c
matrix_t *mat_solve(const matrix_t *A, const matrix_t *B);
```

Solves `A * X = B` for `X` using Gaussian elimination with partial pivoting.
`A` must be square and `B` must have the same number of rows as `A`. Returns a
newly allocated matrix `X`, or NULL on error.

```c
matrix_t *mat_least_squares(const matrix_t *A, const matrix_t *B);
```

Computes a best-fit solution to `A * X = B`. When the system does not admit a
clean exact solve, this returns the `X` that minimises the residual norm
`||A*X - B||`. Full-column-rank overdetermined systems use a QR-based solve.
Rank-deficient or underdetermined systems fall back to the Moore-Penrose
pseudoinverse so the result remains well defined.

Example:

```c
double A_data[] = {
    0.0, 1.0,
    1.0, 1.0,
    2.0, 1.0
};
double B_data[] = {
    1.0,
    3.0,
    5.1
};

matrix_t *A = mat_create_d(3, 2, A_data);
matrix_t *B = mat_create_d(3, 1, B_data);
matrix_t *X = mat_least_squares(A, B);

mat_print(X);

mat_free(X);
mat_free(B);
mat_free(A);
```

Returns a newly allocated matrix, or NULL on error.

#### Rank and Pseudoinverse

```c
int mat_rank(const matrix_t *A);
```

Computes the rank of matrix `A` via SVD. Returns the rank (non-negative integer),
or negative on error. The cutoff between zero and nonzero singular values is
chosen from the matrix size, the largest singular value, and the element type's
numerical precision.

```c
matrix_t *mat_pseudoinverse(const matrix_t *A);
```

Computes the Moore-Penrose pseudoinverse `A⁺` via SVD. The pseudoinverse
generalises the ordinary matrix inverse to rectangular or singular matrices.
It is useful for least-squares problems, minimum-norm solutions, projection
operators, and for working with matrices that do not admit a true inverse.

In particular, the pseudoinverse is useful when solving systems that are
overdetermined, underdetermined, or rank-deficient. In those settings it
provides the standard inverse-like object used to compute best-fit or
minimum-norm solutions.

When `A` is square and nonsingular, the pseudoinverse coincides with the
ordinary inverse. Returns a newly allocated matrix, or NULL on error.

```c
matrix_t *mat_nullspace(const matrix_t *A);
```

Computes a basis for the nullspace of `A` (all `x` such that `A*x = 0`) via the
eigendecomposition of `Aᵀ*A`. Returns a matrix whose columns span the nullspace,
or NULL on error.

#### Matrix Norms

```c
int mat_norm(const matrix_t *A, mat_norm_type_t type, qfloat_t *out);
```

Computes a matrix norm. The norm type is specified by `type`:

| Type | Norm |
|---|---|
| `MAT_NORM_1` | 1-norm (max column sum) |
| `MAT_NORM_INF` | Infinity-norm (max row sum) |
| `MAT_NORM_FRO` | Frobenius norm |
| `MAT_NORM_2` | 2-norm (largest singular value) |

Returns 0 on success, negative on error. The result is written to `*out`.

```c
int mat_condition_number(const matrix_t *A, mat_norm_type_t type, qfloat_t *out);
```

Computes the condition number of `A` in the specified norm and writes it to
`*out`. The condition number measures how sensitive a linear system involving
`A` is to perturbations: small values indicate a well-conditioned problem,
while large values indicate numerical sensitivity. For singular matrices the
result is infinity. Returns 0 on success, negative on error.

#### Matrix Factorisations

##### LU Factorisation

```c
int mat_lu_factor(const matrix_t *A, mat_lu_factor_t *out);
void mat_lu_factor_free(mat_lu_factor_t *out);
```

Computes `P * A = L * U` where `P` is a permutation matrix, `L` is lower triangular
with unit diagonal, and `U` is upper triangular. The result is stored in the
`mat_lu_factor_t` struct containing `P`, `L`, and `U`. For sparse-like inputs,
the permutation and triangular factors keep sparse storage where possible, and
the factorisation uses sparse row operations for elimination rather than first
materialising the working matrices densely.
Returns 0 on success, negative on error. Use
`mat_lu_factor_free` to release the factorisation.

##### QR Factorisation

```c
int mat_qr_factor(const matrix_t *A, mat_qr_factor_t *out);
void mat_qr_factor_free(mat_qr_factor_t *out);
```

Computes `A = Q * R` where `Q` is unitary (orthogonal for real matrices) and `R`
is upper triangular. The result is stored in the `mat_qr_factor_t` struct containing
`Q` and `R`. Returns 0 on success, negative on error. Use `mat_qr_factor_free` to
release the factorisation.

##### Cholesky Factorisation

```c
int mat_cholesky(const matrix_t *A, mat_cholesky_t *out);
void mat_cholesky_free(mat_cholesky_t *out);
```

Computes the Cholesky factorisation `A = L * L^H` for Hermitian positive-definite
matrices. The result is stored in the `mat_cholesky_t` struct containing `L`.
For sparse-like inputs, the returned factor keeps sparse storage. Returns 0 on
success, negative on error (including when `A` is not positive-definite). Use
`mat_cholesky_free` to release the factorisation.

##### Singular Value Factorisation

```c
int mat_svd_factor(const matrix_t *A, mat_svd_factor_t *out);
void mat_svd_factor_free(mat_svd_factor_t *out);
```

Computes the singular value factorisation

`A = U * S * V^H`

where `U` and `V` contain left and right singular vectors, and `S` contains the
singular values on its diagonal. The SVD is useful for rank analysis, numerical
conditioning, pseudoinverses, least-squares problems, and for working robustly
with rectangular or rank-deficient matrices.

The result is stored in the `mat_svd_factor_t` struct containing `U`, `S`, and
`V`. Returns 0 on success, negative on error. Use `mat_svd_factor_free` to
release the factorisation.

##### Schur Factorisation

```c
int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out);
void mat_schur_factor_free(mat_schur_factor_t *out);
```

Computes the Schur factorisation

`A = Q * T * Q^H`

where `Q` is unitary and `T` is upper triangular. This is the standard stable
representation used for non-Hermitian eigenvalue problems and for matrix
functions such as `exp(A)`, `log(A)`, and `sqrt(A)`.

The result is stored in the `mat_schur_factor_t` struct containing `Q` and `T`.
These factors are returned as `qcomplex` matrices even when `A` is real-valued,
since a general real matrix may have complex Schur data. Returns 0 on success,
negative on error. Use `mat_schur_factor_free` to release the decomposition.

### Matrix Functions

All matrix functions accept a square matrix and return a newly allocated result,
or NULL on error (NULL input, non-square input, unsupported element type, or
internal allocation failure).

Every matrix function uses the same algorithm: Schur decomposition followed by
the Parlett recurrence on the triangular Schur factor.

1. Compute `A = Q T Q*` (Schur decomposition; T is upper triangular, Q is unitary).
2. Apply the scalar function element-wise to the diagonal of T and propagate off-diagonal
   entries via the Parlett recurrence: `f(T)_{ij} = T_{ij}(f(T_{ii}) − f(T_{jj})) / (T_{ii} − T_{jj})`.
   When `T_{ii} = T_{jj}` the recurrence uses a numerical derivative of the scalar function.
3. Reconstruct `f(A) = Q · f(T) · Q*`.

All internal arithmetic uses `qcomplex_t`. If the input matrix has a narrower element
type the result is converted back to that type before returning.

For `MAT_TYPE_DVAL`, the story is different:

- general dense Schur-based matrix functions remain unsupported
- exact symbolic matrix functions are implemented for structured inputs where the
  result can be expressed entrywise without numerical approximation

| Function | Description |
|---|---|
| `mat_exp(A)` | Matrix exponential `eˢ` |
| `mat_log(A)` | Matrix (principal) logarithm |
| `mat_sqrt(A)` | Matrix (principal) square root |
| `mat_sin(A)` | Matrix sine |
| `mat_cos(A)` | Matrix cosine |
| `mat_tan(A)` | Matrix tangent |
| `mat_sinh(A)` | Matrix hyperbolic sine |
| `mat_cosh(A)` | Matrix hyperbolic cosine |
| `mat_tanh(A)` | Matrix hyperbolic tangent |
| `mat_asin(A)` | Matrix arc sine |
| `mat_acos(A)` | Matrix arc cosine |
| `mat_atan(A)` | Matrix arc tangent |
| `mat_asinh(A)` | Matrix inverse hyperbolic sine |
| `mat_acosh(A)` | Matrix inverse hyperbolic cosine |
| `mat_atanh(A)` | Matrix inverse hyperbolic tangent |
| `mat_erf(A)` | Matrix error function |
| `mat_erfc(A)` | Matrix complementary error function |
| `mat_erfinv(A)` | Matrix inverse error function |
| `mat_erfcinv(A)` | Matrix inverse complementary error function |
| `mat_gamma(A)` | Matrix gamma function |
| `mat_lgamma(A)` | Matrix log gamma function |
| `mat_digamma(A)` | Matrix digamma function (psi) |
| `mat_trigamma(A)` | Matrix trigamma function |
| `mat_tetragamma(A)` | Matrix tetragamma function |
| `mat_gammainv(A)` | Matrix inverse gamma function |
| `mat_normal_pdf(A)` | Matrix normal probability density function |
| `mat_normal_cdf(A)` | Matrix normal cumulative distribution function |
| `mat_normal_logpdf(A)` | Matrix normal log probability density function |
| `mat_lambert_w0(A)` | Matrix Lambert W function (principal branch) |
| `mat_lambert_wm1(A)` | Matrix Lambert W function (-1 branch) |
| `mat_productlog(A)` | Matrix product logarithm (Lambert W) |
| `mat_ei(A)` | Matrix exponential integral Ei |
| `mat_e1(A)` | Matrix exponential integral E1 |

### Power Functions

#### Integer power

```c
matrix_t *mat_pow_int(const matrix_t *A, int n);
```

Computes `Aⁿ` via binary exponentiation. `n` may be zero (returns the
identity), positive, or negative (uses `mat_inverse` internally). Returns NULL
if `A` is NULL, not square, or inversion fails when `n < 0`.

#### Real power

```c
matrix_t *mat_pow(const matrix_t *A, double s);
```

Computes `Aˢ = exp(s · log(A))`. Requires `A` to have a well-defined matrix
logarithm — positive definite real matrices always satisfy this. Returns NULL
on any error (NULL input, non-square, `mat_log` failure).

### Debugging / I/O

- `void mat_print(const matrix_t *A)` — print the matrix to standard output.

For `MAT_TYPE_DVAL`, `mat_print` renders symbolic entries and prints one shared
binding footer for the whole matrix rather than repeating a binding block for
every cell.

---

## Design Notes

### Vtable Dispatch

Every `matrix_t` carries two vtable pointers:

- **element vtable** — arithmetic (`add`, `sub`, `mul`, `div`, `conj`, …),
  conversion to/from `qfloat_t`, and formatting. One instance per element type.
- **storage vtable** — `get`/`set` and any type-specific fast paths. One instance
  per storage kind.

This means all code paths (arithmetic, eigendecomposition, printing) are generic:
they call through function pointers and never inspect the element type or storage
kind directly.

### Precision of Linear Algebra

**Eigendecomposition, Hermitian path** — cyclic Jacobi sweep. Rotation parameters
(τ, t, c, s) are always real; they are computed through `qfloat_t` arithmetic,
giving ~31 decimal digits of precision regardless of the matrix's element type.
Eigenvalues are real and eigenvectors are orthonormal.

**Eigendecomposition, general path** — Hessenberg reduction (Householder
reflectors) followed by implicit single-shift QR (Francis/Wilkinson). All
internal arithmetic uses `qcomplex_t` flat arrays, so the algorithm handles real,
qfloat, and complex matrices uniformly. Hermitian detection compares `A[i,j]`
against `conj(A[j,i])` within a tolerance relative to the Frobenius norm; matrices
that pass this test take the faster Jacobi path automatically.

**Matrix functions** — all use the Schur + Parlett path regardless of whether the
matrix is Hermitian. Internal arithmetic is always `qcomplex_t`; the result is
cast back to the input element type before returning.

### Identity Storage

Identity matrices store no element data. Reading `(i, i)` returns one; reading
`(i, j)` for `i ≠ j` returns zero. The first write to any element transparently
materialises the matrix as dense.

### Sparse Storage

Sparse matrices use a hash-based storage scheme that stores only non-zero elements.
Reading a non-existent element returns zero; setting an element to zero removes it
from storage. The `mat_nonzero_count` function returns the number of stored non-zero elements.

Sparse matrices support efficient arithmetic operations:
- Addition and subtraction of two sparse matrices produce sparse results
- Multiplication of two sparse matrices uses the sparse-sparse algorithm
- Mixed operations (sparse × dense, dense × sparse) automatically convert as needed

Use `mat_to_sparse` to convert a dense matrix to sparse form, and `mat_to_dense`
to convert a sparse matrix to dense form. The `mat_is_sparse` function queries
whether a matrix uses sparse storage.
