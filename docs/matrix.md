# `matrix_t`

`matrix_t` is a generic high-precision matrix type with two public element
families: `number_t` for numeric work and `expr_t *` for symbolic work.
Numeric matrices can still hold exact rationals, floating-point values, and
complex values through the `number_t` layer. Storage remains pluggable
(`dense`, `sparse`, `identity`, `diagonal`, `upper triangular`, `lower triangular`).
All operations dispatch through internal vtables; no type switches or storage
switches appear in user code.

## Representation

`matrix_t` is an opaque struct. Clients hold a pointer and access it only through
the API. Internally each matrix carries:

- a pointer to an element vtable (arithmetic, conversion, formatting)
- a pointer to a storage vtable (construction, get/set, layout queries)
- a `rows × cols` size pair
- a storage payload whose layout depends on the chosen storage kind

## Capabilities

- element families:
  - `MAT_TYPE_NUMBER` backed by `number_t`, covering exact rational, floating, and complex numeric entries
  - `MAT_TYPE_EXPR` backed by retained `expr_t *` symbolic entries
- storage kinds:
  - dense (fully materialised)
  - sparse (stores only non-zero elements explicitly)
  - identity (zero storage, materialises on write)
  - diagonal (stores only the main diagonal)
  - upper triangular (stores entries on and above the diagonal)
  - lower triangular (stores entries on and below the diagonal)
- arithmetic: scalar multiply/divide, matrix add, subtract, multiply
- structural: transpose, conjugate, Hermitian (conjugate transpose)
- string I/O: parse matrices from strings, render them back to strings, and print them with matrix-aware format specifiers
- linear algebra: determinant, trace, characteristic polynomial, minimal polynomial, polynomial application, adjugate, inverse, block inverse, Schur complements, solve, block solve, eigenvalues, eigendecomposition, eigenvectors, eigenspaces, generalised eigenspaces, Jordan-chain helpers, rank, pseudoinverse, nullspace
- symbolic matrix calculus: entrywise derivatives via `mat_deriv(...)`, Jacobian helpers, plus derivative helpers for trace, determinant, inverse, block inverse, solve, and block solve
- matrix norms: 1-norm, infinity-norm, Frobenius norm, 2-norm
- matrix factorisations: LU, QR, Cholesky, SVD, Schur
- condition number computation
- matrix functions: named APIs for exp, sin, cos, tan, sinh, cosh, tanh, sqrt, log, log10, asin, acos, atan, asinh,
  acosh, atanh, erf, and erfc; complete matrix-expression input resolves every applicable unary expression function
- power functions: integer power (binary exponentiation), real power via exp/log, and exact symbolic powers of small
  numeric or symbolic matrices through spectral projectors
- numeric eigendecomposition and matrix functions are computed through the high-precision numeric `number_t` layer regardless of how the original numeric entries were written

## `expr_t *` Matrices

`matrix_t` also supports symbolic `expr_t *` elements through `MAT_TYPE_EXPR`.
These matrices retain every stored `expr_t *` handle, so overwrites, copies,
materialisation, and destruction are reference-count safe.

What currently works for `expr` matrices:

- construction in dense, sparse, identity, diagonal, and compatible structured layouts
- string-based construction through `mat_from_string(...)` and `mat_from_string_expr(...)` for real, complex, and symbolic matrices
- element access, copy, transpose, conjugate
- add, subtract, multiply
- scalar multiply/divide through the normal promotion rules
- exact determinant, trace, characteristic polynomial, minimal polynomial, polynomial application, adjugate
- exact inverse and solve, including larger dense symbolic cases
- exact symbolic pseudoinverse, least-squares, rank, and nullspace, including rank-deficient rectangular cases
- exact Schur complements, block inverses, and block solves, including symbolic `expr` block expressions when the leading block is invertible
- eigenspaces, generalised eigenspaces, Jordan chains, and Jordan block-size profiles for disciplined symbolic cases
- entrywise symbolic matrix derivatives with respect to a chosen `expr` variable
- row-major Jacobian extraction for matrix-valued symbolic outputs
- symbolic derivative helpers for `trace(A)`, `det(A)`, and `A^{-1}`
- symbolic derivative helper for `solve(A, B)`
- symbolic matrix functions for exact structured square inputs
  - diagonal matrices
  - upper- and lower-triangular matrices
  - repeated-diagonal triangular cases such as Jordan blocks
- symbolic pretty-printing with one shared binding footer for the whole matrix
- `mat_to_text(...)`, `mat_printf(...)`, and `mat_sprintf_text(...)` for matrix-aware symbolic and numeric output

What is still intentionally unsupported for `expr` matrices:

- general numerical inverse / solve / least-squares
- LU / QR / Cholesky / SVD / Schur
- numerical eigensolvers
- general dense Schur-based matrix functions on arbitrary `expr` inputs
- full general symbolic Jordan normal form / arbitrary dense symbolic spectral decomposition

The current design is to use `matrix<expr_t *>` for symbolic construction,
differentiation, and exact structured operations, then evaluate to a numeric
matrix type when you want the full numerical linear-algebra toolbox.

## Native Matrix-expression Grammar

`mat_expression_from_string(...)` owns the complete matrix-expression grammar.
Clients should pass input through unchanged rather than recognising matrix
function names or rewriting syntax themselves.

Compact literals use spaces or commas between columns and semicolons between
rows. Parentheses may group complete expressions. `+` and `-` operate on
equally sized matrices, `.` is ordered matrix multiplication, and `^` accepts
integer, fractional or symbolic exponents. Matrix division has no parser
operator because `A/B` does not say whether `A inv(B)` or `inv(B) A` is meant.

The native collision-free matrix-function table recognises these names:

| Canonical operation | Aliases |
|---|---|
| `inverse` | `inv` |
| `det` | `determinant` |
| `trace` | `tr` |
| `transpose` | `trans` |
| `hermitian` | `adjoint`, `ctranspose`, `conjtrans`, `conjugate_transpose` |

The Hermitian operation is the conjugate transpose. Its postfix spellings are
`^dagger`, `^H`, `^*`, `^†` and `†`. Determinants additionally accept paired
`|A|`, `||A||` and `‖A‖` delimiters. Missing closing bars are rejected, and the
result of a determinant or trace is a scalar.

When a scalar identity multiple is unambiguous, `lambdaI`, `lambda.I` and
`lambda*I` are accepted. Greek ASCII names, `@` aliases and Unicode spellings
normalise to the same binding, so `lambda`, `@lambda` and `λ` all denote λ.

Matrix entries use the scalar expression parser. Consequently `conj(z)`,
`conjugate(z)` and `z^*` are equivalent, as are `abs(z)` and paired `|z|`.
For complex scalars the latter is the modulus `sqrt(z*z^*)`.

`Dx(A)` differentiates each entry with respect to `x`, and `@S^x(A)` produces
an entrywise antiderivative. A matrix antiderivative has one independent
constant per entry and is formatted as `A(x) + C`, with `C` carrying indexed
constants such as `C₁₁` and `C₁₂`.

## Example

```c
#include <stdio.h>
#include "matrix.h"
#include "number.h"

int main(void) {
    /* Hermitian 2×2 matrix with eigenvalues 1 and 4:
     *   [ 2    1+i ]
     *   [ 1-i  3   ]
     */
    matrix_t *A = mat_from_string("(2, 1+i; 1-i, 3)");
    number_t eigenvalues[2] = {num_new(), num_new()};
    matrix_t *evecs = NULL;

    mat_eigendecompose(A, eigenvalues, &evecs);

    num_printf("eigenvalue[0] = %N\n", eigenvalues[0]);
    num_printf("eigenvalue[1] = %N\n", eigenvalues[1]);

    mat_free(A);
    mat_free(evecs);
    num_destroy(&eigenvalues[0]);
    num_destroy(&eigenvalues[1]);
    return 0;
}
```

Expected output:

```text
eigenvalue[0] = 1 + 0i
eigenvalue[1] = 4 + 0i
```

### String-based symbolic example

The string parser is especially handy when you want to build a symbolic matrix
directly from a compact mathematical expression. Here is a small spin-½
Hamiltonian with detuning `Δ` and coupling `Ω`. The input uses the ASCII alias
forms `@DELTA` and `@OMEGA`, so you do not need to type Greek letters at the
keyboard:

```c
#include "expression.h"
#include "matrix.h"

int main(void)
{
    mat_bindings_t *bindings = NULL;
    matrix_t *H = mat_from_string_expr(
        "{ (@DELTA, @OMEGA; @OMEGA, -@DELTA) | @DELTA = 1.5; @OMEGA = 0.25 }",
        &bindings);
    expr_t *delta = mat_bindings_get(bindings, "@DELTA");
    matrix_t *charpoly = mat_charpoly(H);
    expr_t *detH = NULL;
    expr_t *ddet_dDelta = mat_deriv_det(H, delta);
    string_t *H_text;
    string_t *p_text;
    string_t *det_text;
    string_t *ddet_text;

    mat_get(charpoly, 2, 0, &detH);

    H_text = mat_to_text(H, MAT_STRING_LAYOUT_PRETTY);
    p_text = mat_to_text(charpoly, MAT_STRING_INLINE_PRETTY);
    det_text = expr_to_text(detH, style_EXPRESSION);
    ddet_text = expr_to_text(ddet_dDelta, style_EXPRESSION);

    string_printf("%S\n", H_text);
    string_printf("characteristic polynomial coefficients = %S\n", p_text);
    string_printf("det(H) = %S\n", det_text);
    string_printf("d/dΔ det(H) = %S\n", ddet_text);

    string_free(H_text);
    string_free(p_text);
    string_free(det_text);
    string_free(ddet_text);
    mat_bindings_free(bindings);
    expr_free(ddet_dDelta);
    mat_free(charpoly);
    mat_free(H);
    return 0;
}
```

Illustrative output:

```text
{ (
  Δ    Ω
  Ω   -Δ
) | Δ = 1.5, Ω = 0.25 }
characteristic polynomial coefficients = (1; 0; -(Δ² + Ω²))
det(H) = { -Δ² - Ω² | Δ = 1.5; Ω = 0.25 }
d/dΔ det(H) = { -2Δ | Δ = 1.5 }
```

### Symbolic quantum-mechanics example

Here is a symbolic two-level Hamiltonian for a driven spin-½ system,

`H = Δσ_z + Ωσ_x`,

written in matrix form as

`(Δ, Ω; Ω, -Δ)`.

In code below, the matrix is entered with the keyboard-friendly aliases
`@DELTA` and `@OMEGA`, while the formatter still prints the normalised Greek
symbols in the output.

This is a pleasing example because the algebra stays exact:

- `tr(H) = 0`
- `H² = (Δ² + Ω²) I`
- the characteristic polynomial is `λ² - (Δ² + Ω²)`
- the eigenvalues are `±sqrt(Δ² + Ω²)`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expression.h"
#include "matrix.h"

int main(void)
{
    mat_bindings_t *bindings = NULL;
    number_t delta = num_create_from_double(1.5);
    number_t omega = num_create_from_double(0.25);
    matrix_t *H = mat_from_string_expr(
        "(@DELTA, @OMEGA; @OMEGA, -@DELTA)",
        &bindings);
    matrix_t *H2 = mat_pow_int(H, 2);
    matrix_t *P = mat_charpoly(H);
    expr_t *evals[2] = {NULL, NULL};
    expr_t *trace = NULL;
    expr_t *c2 = NULL;
    string_t *trace_text = NULL;
    string_t *c2_text = NULL;
    string_t *eval0_text = NULL;
    string_t *eval1_text = NULL;

    expr_set_val(mat_bindings_get(bindings, "@DELTA"), delta);
    expr_set_val(mat_bindings_get(bindings, "@OMEGA"), omega);

    mat_eigenvalues(H, evals);
    mat_trace(H, &trace);
    mat_get(P, 2, 0, &c2);
    trace_text = expr_to_text(trace, style_EXPRESSION);
    c2_text = expr_to_text(c2, style_EXPRESSION);
    eval0_text = expr_to_text(evals[0], style_EXPRESSION);
    eval1_text = expr_to_text(evals[1], style_EXPRESSION);

    mat_printf("H = %ml\n", H);
    mat_printf("H² = %m\n", H2);
    printf("tr(H) = %s\n", string_c_str(trace_text));
    printf("charpoly constant term = %s\n", string_c_str(c2_text));
    printf("eigenvalues = %s, %s\n",
           string_c_str(eval0_text),
           string_c_str(eval1_text));

    string_free(eval1_text);
    string_free(eval0_text);
    string_free(c2_text);
    string_free(trace_text);
    num_destroy(&omega);
    num_destroy(&delta);
    mat_bindings_free(bindings);
    mat_free(P);
    mat_free(H2);
    mat_free(H);
    return 0;
}
```

Illustrative output:

```text
H = { (
  Δ  Ω
  Ω -Δ
) | Δ = 1.5, Ω = 0.25 }
H² = { (Δ² + Ω², -ΔΩ + ΔΩ; -ΔΩ + ΔΩ, Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }
tr(H) = { 0 }
charpoly constant term = { -(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }
eigenvalues = { √(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }, { -√(Δ² + Ω²) | Δ = 1.5, Ω = 0.25 }
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

typedef struct mat_bindings_t mat_bindings_t;

typedef enum {
    MAT_STRING_INLINE_SCIENTIFIC,
    MAT_STRING_INLINE_PRETTY,
    MAT_STRING_LAYOUT_SCIENTIFIC,
    MAT_STRING_LAYOUT_PRETTY
} mat_string_style_t;
```

### Construction

#### Allocate without filling

Use these when you need to fill elements sparsely (e.g. a single diagonal or one
off-diagonal element). For bulk initialisation prefer the `mat_create_*` forms below.

| Function | Element type | Description |
|---|---|---|
| `mat_new(rows, cols)` | `number_t` | Allocate an uninitialised `rows × cols` matrix of `number_t` values |
| `mat_new_expr(rows, cols)` | `expr_t *` | Allocate an uninitialised `rows × cols` matrix of retained `expr_t *` handles |
| `mat_new_sparse(rows, cols)` | `number_t` | Allocate an uninitialised sparse `rows × cols` matrix of `number_t` values |
| `mat_new_sparse_expr(rows, cols)` | `expr_t *` | Allocate an uninitialised sparse `rows × cols` matrix of retained `expr_t *` handles |
| `matsq_new(n)` | `number_t` | Allocate an uninitialised `n × n` matrix of `number_t` values |

#### Allocate and fill from a flat array

| Function | Element type | Description |
|---|---|---|
| `mat_create(rows, cols, data)` | `number_t` | Allocate and fill from a row-major `number_t[]`; each entry is cloned into matrix-owned storage |
| `mat_create_expr(rows, cols, data)` | `expr_t *` | Allocate and fill from a row-major `expr_t * []`; each handle is retained by the matrix |

#### Identity matrices

Identity matrices carry no element storage. The first write to any element
materialises the matrix as dense.

| Function | Element type | Description |
|---|---|---|
| `mat_create_identity(n)` | `number_t` | `n × n` identity matrix of exact numeric ones and zeros |
| `mat_create_identity_expr(n)` | `expr_t *` | `n × n` identity matrix of symbolic ones and zeros |

#### Diagonal matrices

Diagonal matrices store only their main diagonal explicitly. They are useful
when every off-diagonal entry is known to be zero and you want that structure
to survive through compatible operations.

| Function | Element type | Description |
|---|---|---|
| `mat_create_diagonal(n, diagonal)` | `number_t` | `n × n` diagonal matrix of cloned `number_t` values |
| `mat_create_diagonal_expr(n, diagonal)` | `expr_t *` | `n × n` diagonal matrix of retained `expr_t *` handles |

### Destruction

- `void mat_free(matrix_t *A)` — free all memory owned by `A`, including element storage.

### Element Access

- `void mat_get(const matrix_t *A, size_t i, size_t j, void *out)` — low-level accessor that writes the entry at row `i`, column `j` into `out` using the matrix's native stored element representation. The caller must pass a pointer to the matching underlying type. For numeric matrices this means reading the concrete stored numeric element directly; for `MAT_TYPE_EXPR`, the written value is a borrowed `expr_t *`.
- `number_t mat_get_num(const matrix_t *A, size_t i, size_t j)` — high-level numeric accessor that always returns an owning `number_t`. Use this when you want a uniform numeric read regardless of the matrix's underlying storage type, or when reading a symbolic entry as its current evaluated numeric value.
- `void mat_set(matrix_t *A, size_t i, size_t j, const void *val)` — copy `val` into position `(i, j)`. For `MAT_TYPE_EXPR`, the matrix retains the incoming handle.
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

- `void mat_set_data(matrix_t *A, const number_t *data)` — set every entry of a numeric matrix from a row-major `number_t` buffer.
- `void mat_set_data_expr(matrix_t *A, expr_t *const *data)` — set every entry of a symbolic matrix from a row-major `expr_t *` buffer. Each handle is retained by the matrix.
- `void mat_get_data(const matrix_t *A, number_t *data)` — read every entry of a numeric matrix into a row-major `number_t` buffer. Each returned `number_t` is owning and should later be destroyed with `num_destroy(...)`.
- `void mat_get_data_expr(const matrix_t *A, expr_t **data)` — read every entry of a symbolic matrix into a row-major `expr_t *` buffer. The returned handles are borrowed from the matrix.
- `void mat_set_data_raw(matrix_t *A, const void *data)` / `void mat_get_data_raw(const matrix_t *A, void *data)` — low-level generic bulk accessors that operate on the matrix's native public element family after you have already inspected `mat_typeof(A)`.

Rule of thumb:

- use `mat_set_data(...)` / `mat_get_data(...)` for ordinary numeric code
- use `mat_set_data_expr(...)` / `mat_get_data_expr(...)` for ordinary symbolic code
- use `*_raw` only in generic helper code that already knows, via `mat_typeof(...)`, whether it is handling a `number_t` matrix or a `expr_t *` matrix and wants one branch-local bulk access path

`*_raw` is not a more powerful numeric API. It is just the branch-after-type-check option for code that wants to stay generic over the two public matrix element families.

### Scalar Operations

Scalar functions return a new matrix. The scalar is broadcast to every element.
Mixed scalar/element types are supported; the result element type is the wider of
the two.

| Function | Scalar type | Description |
|---|---|---|
| `mat_scalar_mul(A, s)` | `number_t` | `s * A` |
| `mat_scalar_div(A, s)` | `number_t` | `A / s` |

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

#### Matrix Derivative

```c
matrix_t *mat_deriv(const matrix_t *A, expr_t *wrt);
```

Returns the entrywise derivative of `A` with respect to `wrt`, so the output
has the same shape as `A` and each entry is

```text
∂A[i,j] / ∂wrt
```

For symbolic `MAT_TYPE_EXPR` matrices, the returned matrix is newly allocated
and continues to track later changes to any variables referenced by the
differentiated expressions.

For non-`expr` matrices, `A` is treated as a constant matrix and the result is a
newly allocated zero matrix with the same shape and element type as `A`.

#### Matrix Calculus Helpers

```c
expr_t   *mat_deriv_trace_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
expr_t   *mat_deriv_det_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
expr_t   *mat_deriv_trace(const matrix_t *A, expr_t *wrt);
expr_t   *mat_deriv_det(const matrix_t *A, expr_t *wrt);
expr_t   *mat_deriv_trace_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
expr_t   *mat_deriv_det_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_inverse(const matrix_t *A, expr_t *wrt);
matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_block_inverse(const matrix_t *A, size_t split, expr_t *wrt);
matrix_t *mat_deriv_block_inverse_by_name(const matrix_t *A, size_t split, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_solve(const matrix_t *A, const matrix_t *B, expr_t *wrt);
matrix_t *mat_deriv_solve_by_name(const matrix_t *A, const matrix_t *B, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_block_solve(const matrix_t *A, const matrix_t *B, size_t split, expr_t *wrt);
matrix_t *mat_deriv_block_solve_by_name(const matrix_t *A, const matrix_t *B, size_t split, mat_bindings_t *bindings, const char *name);
matrix_t *mat_jacobian_by_names(const matrix_t *A, mat_bindings_t *bindings, const char *const *names, size_t nnames);
matrix_t *mat_jacobian(const matrix_t *A, expr_t *const *vars, size_t nvars);
```

These helpers build on the symbolic `expr` matrix layer:

- `mat_deriv_by_name(A, bindings, n, "x")` looks up a parsed binding and differentiates entrywise with respect to it
- `mat_deriv_trace_by_name(...)` and `mat_deriv_det_by_name(...)` do the same for `trace(A)` and `det(A)`
- `mat_deriv_trace(A, wrt)` returns the derivative of `trace(A)` as a symbolic scalar
- `mat_deriv_det(A, wrt)` returns the derivative of `det(A)` as a symbolic scalar
- `mat_deriv_inverse_by_name(...)` and `mat_deriv_block_inverse_by_name(...)` resolve a parsed binding name first, then differentiate the inverse helper
- `mat_deriv_inverse(A, wrt)` returns the derivative of `A^{-1}` as a symbolic matrix
- `mat_deriv_block_inverse(A, split, wrt)` returns the derivative of the symbolic block inverse of `A`
- `mat_deriv_solve_by_name(...)` and `mat_deriv_block_solve_by_name(...)` resolve a parsed binding name first, then differentiate the corresponding solve helper
- `mat_deriv_solve(A, B, wrt)` returns the derivative of the symbolic solution of `A X = B`
- `mat_deriv_block_solve(A, B, split, wrt)` returns the derivative of the symbolic block solution of `A X = B`
- `mat_jacobian_by_names(A, bindings, n, names, nnames)` resolves parsed binding names first, then builds the row-major Jacobian
- `mat_jacobian(A, vars, nvars)` returns a row-major Jacobian matrix with
  `rows(A) * cols(A)` rows and `nvars` columns

For symbolic `MAT_TYPE_EXPR` matrices, these helpers return newly allocated
symbolic results that continue to track later variable updates.

For non-`expr` matrices, they treat the matrix inputs as constants:

- `mat_deriv_trace(...)` and `mat_deriv_det(...)` return symbolic zero
- `mat_deriv_inverse(...)`, `mat_deriv_block_inverse(...)`, `mat_deriv_solve(...)`, and `mat_deriv_block_solve(...)`
  return zero matrices with the corresponding numeric shape and element type
- `mat_jacobian(...)` returns a zero symbolic Jacobian matrix of the same
  row-major shape it would use for a symbolic matrix

For `mat_jacobian(...)`, row `i * cols(A) + j` corresponds to the entry
`A[i,j]`, and column `k` corresponds to differentiation with respect to
`vars[k]`.

### Linear Algebra

#### Determinant

```c
int mat_det(const matrix_t *A, number_t *determinant);
```

Computes the determinant of square matrix `A` and writes an owning numeric
result into `*determinant`.

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

For `MAT_TYPE_EXPR`, inverse support now includes diagonal and triangular cases,
exact dense `2×2` / `3×3`, and the larger dense symbolic elimination path used
by the matrix tests.

#### Schur Complement

```c
matrix_t *mat_schur_complement(const matrix_t *A, size_t split);
```

Partitions a square matrix as

```text
A = [ A11  A12 ]
    [ A21  A22 ]
```

with `A11` of size `split × split`, and returns the Schur complement

```text
A22 - A21 A11^{-1} A12
```

as a newly allocated matrix. This is exact for supported symbolic `expr`
inputs as long as the leading block `A11` is invertible.

#### Block Inverse

```c
matrix_t *mat_block_inverse(const matrix_t *A, size_t split);
```

Uses the same top-left partition and Schur-complement algebra to build the full
inverse of `A` from block formulas. This is particularly useful for symbolic
workflows because it exposes block structure instead of forcing a single dense
elimination path from the outset.

#### Block Solve

```c
matrix_t *mat_block_solve(const matrix_t *A, const matrix_t *B, size_t split);
```

Solves `A X = B` using the same block partition and Schur-complement reduction.
It returns a newly allocated `X`, and is exact for supported symbolic `expr`
inputs when the leading block and the resulting Schur complement are invertible.

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
int mat_eigenvalues(const matrix_t *A, number_t *eigenvalues);
```

Computes all eigenvalues of square matrix `A` and writes them into the
caller-allocated `number_t[n]` buffer `eigenvalues`.

- **Hermitian matrices** — cyclic Jacobi sweep on the numeric `number_t` path;
  eigenvalues are real.
- **General matrices** — Hessenberg reduction followed by implicit QR. Eigenvalues
  may be complex even when `A` has real elements, and are returned through
  `number_t`.

Return values: `0` on success, negative on error.

#### Eigendecomposition

```c
int mat_eigendecompose(const matrix_t *A,
                       number_t *eigenvalues,
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
`||A*X - B||`.

- Numeric matrix types use a QR-based solve for full-column-rank overdetermined
  systems and fall back to the Moore-Penrose pseudoinverse for rank-deficient
  or underdetermined cases.
- `MAT_TYPE_EXPR` supports exact symbolic least-squares through exact symbolic
  pseudoinverses, including rank-deficient rectangular systems.

Example:

```c
number_t A_data[] = {
    num_create_from_double(0.0), num_create_from_double(1.0),
    num_create_from_double(1.0), num_create_from_double(1.0),
    num_create_from_double(2.0), num_create_from_double(1.0)
};
number_t B_data[] = {
    num_create_from_double(1.0),
    num_create_from_double(3.0),
    num_create_from_double(5.1)
};

matrix_t *A = mat_create(3, 2, A_data);
matrix_t *B = mat_create(3, 1, B_data);
matrix_t *X = mat_least_squares(A, B);

mat_print(X);

for (size_t i = 0; i < 6; ++i)
    num_destroy(&A_data[i]);
for (size_t i = 0; i < 3; ++i)
    num_destroy(&B_data[i]);
mat_free(X);
mat_free(B);
mat_free(A);
```

Returns a newly allocated matrix, or NULL on error.

#### Rank and Pseudoinverse

```c
int mat_rank(const matrix_t *A);
```

Computes the rank of matrix `A`.

- For numeric element types, rank is computed via SVD. The cutoff between zero
  and nonzero singular values is chosen from the matrix size, the largest
  singular value, and the element type's numerical precision.
- For `MAT_TYPE_EXPR`, rank is computed exactly by symbolic elimination using
  exact-zero checks on the reduced expression entries.

Returns the rank (non-negative integer), or a negative value on error.

```c
matrix_t *mat_pseudoinverse(const matrix_t *A);
```

Computes the Moore-Penrose pseudoinverse `A⁺`. The pseudoinverse generalises
the ordinary matrix inverse to rectangular or singular matrices. It is useful
for least-squares problems, minimum-norm solutions, projection operators, and
for working with matrices that do not admit a true inverse.

In particular, the pseudoinverse is useful when solving systems that are
overdetermined, underdetermined, or rank-deficient. In those settings it
provides the standard inverse-like object used to compute best-fit or
minimum-norm solutions.

When `A` is square and nonsingular, the pseudoinverse coincides with the
ordinary inverse.

- Numeric matrix types compute the pseudoinverse via SVD.
- `MAT_TYPE_EXPR` supports exact symbolic pseudoinverses for rectangular and
  rank-deficient inputs through exact full-rank factorisation.

Returns a newly allocated matrix, or NULL on error.

```c
matrix_t *mat_nullspace(const matrix_t *A);
```

Computes a basis for the nullspace of `A` (all `x` such that `A*x = 0`).

- Numeric element types build the basis from the eigendecomposition of `Aᵀ*A`.
- `MAT_TYPE_EXPR` computes the nullspace basis exactly by symbolic reduction.

Returns a matrix whose columns span the nullspace, or NULL on error.

#### Matrix Norms

```c
int mat_norm(const matrix_t *A, mat_norm_type_t type, number_t *out);
```

Computes a matrix norm. The norm type is specified by `type`:

| Type | Norm |
|---|---|
| `MAT_NORM_1` | 1-norm (max column sum) |
| `MAT_NORM_INF` | Infinity-norm (max row sum) |
| `MAT_NORM_FRO` | Frobenius norm |
| `MAT_NORM_2` | 2-norm (largest singular value) |

Returns 0 on success, negative on error. The result is written to `*out` as an
owning `number_t`.

```c
int mat_condition_number(const matrix_t *A, mat_norm_type_t type, number_t *out);
```

Computes the condition number of `A` in the specified norm and writes it to
`*out` as an owning `number_t`. The condition number measures how sensitive a
linear system involving `A` is to perturbations: small values indicate a
well-conditioned problem, while large values indicate numerical sensitivity.
For singular matrices the result is infinity. Returns 0 on success, negative
on error.

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
These factors are returned as numeric `matrix_t` values (`MAT_TYPE_NUMBER`)
even when the Schur data is complex, since `number_t` already covers complex
entries. Returns 0 on success, negative on error. Use
`mat_schur_factor_free` to release the decomposition.

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

Numeric results are returned as `MAT_TYPE_NUMBER` matrices, so exact, floating,
and complex values all come back through the public `number_t` layer.

For `MAT_TYPE_EXPR`, the story is different:

- general dense Schur-based matrix functions remain unsupported
- exact symbolic matrix functions are implemented for structured inputs where the
  result can be expressed entrywise without numerical approximation
- when you want to continue numerically beyond that exact symbolic boundary,
  first evaluate the current symbolic matrix into a `number_t` snapshot with
  `mat_evaluate(...)`, then apply the usual numeric matrix function.
- that exact path also includes:
  - dense symbolic matrices with one common diagonal value and one common
    off-diagonal value, handled exactly for any matrix size through the
    rank-one projector formula
  - dense symbolic matrices of the form `A = αI + uvᵀ`, handled exactly for
    `n×n` inputs with `n ≥ 3` through the rank-one update formula
  - diagonalizable dense `2×2` symbolic matrices, evaluated via symbolic
    eigendecomposition rather than numerical Schur reduction
  - denser symbolic families satisfying an exact quadratic relation
    `A² = pA + qI`, which covers some dense `3×3` cases without falling back to
    numerical evaluation
  - denser symbolic families satisfying an exact cubic-linear relation
    `A³ = sA`, which covers another disciplined subset of dense `3×3` cases
    through an exact quadratic polynomial in `A`
  - the dense symbolic `4×4` path-style family with zero diagonal and one
    common nearest-neighbour coupling, recognised as an exact biquadratic
    quartic case `A⁴ = sA² + tI` without attempting a full general quartic
    solver
  - block-diagonal symbolic matrices whose diagonal blocks already belong to an
    exact symbolic family, which gives a disciplined exact route for useful
    `4×4` cases without attempting a full general quartic solver
  - permutation-similar block-diagonal symbolic matrices, where an exact
    principal reordering exposes independent symbolic blocks before the matrix
    function is applied blockwise and permuted back

| Function | Description |
|---|---|
| `mat_exp(A)` | Matrix exponential `eˢ` |
| `mat_log(A)` | Matrix (principal) logarithm |
| `mat_log10(A)` | Matrix common logarithm, computed as `mat_log(A) / log(10)` |
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

#### Number power

```c
matrix_t *mat_pow(const matrix_t *A, const number_t *s);
```

Computes `Aˢ = exp(s · log(A))` for a borrowed numeric exponent `s`. Requires
`A` to have a well-defined matrix logarithm — positive definite real matrices
always satisfy this. Returns NULL on any error (NULL input, NULL exponent,
non-square matrix, or `mat_log` failure).

### Debugging / I/O

- `void mat_print(const matrix_t *A)` — print the matrix to standard output.

For `MAT_TYPE_EXPR`, `mat_print` renders symbolic entries and prints one shared
binding footer for the whole matrix rather than repeating a binding block for
every cell.

### String Construction and Output

```c
typedef struct mat_bindings_t mat_bindings_t;

matrix_t *mat_from_string(const char *s);
matrix_t *mat_from_string_expr(const char *s, mat_bindings_t **bnd_out);
matrix_t *mat_expression_from_string(const char *text, mat_bindings_t **bnd_out, const char **operation_out);
expr_t *mat_bindings_get(mat_bindings_t *bnd, const char *name);
void mat_bindings_free(mat_bindings_t *bnd);
matrix_t *mat_deriv_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
expr_t *mat_deriv_trace_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
expr_t *mat_deriv_det_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_inverse_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_block_inverse_by_name(const matrix_t *A, size_t split, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_solve_by_name(const matrix_t *A, const matrix_t *B, mat_bindings_t *bindings, const char *name);
matrix_t *mat_deriv_block_solve_by_name(const matrix_t *A, const matrix_t *B, size_t split, mat_bindings_t *bindings, const char *name);
matrix_t *mat_jacobian_by_names(const matrix_t *A, mat_bindings_t *bindings, const char *const *names, size_t nnames);
string_t *mat_to_text(const matrix_t *A, mat_string_style_t style);
char *mat_to_string(const matrix_t *A, mat_string_style_t style);
string_t *mat_vsprintf_text(const char *fmt, va_list ap);
string_t *mat_sprintf_text(const char *fmt, ...);
int mat_sprintf(char *out, size_t out_size, const char *fmt, ...);
int mat_printf(const char *fmt, ...);
```

`mat_from_string(...)` and `mat_from_string_expr(...)` accept three main forms:

- purely numeric matrices such as `(1, 2; 3, 4)`
- wrapped symbolic matrices such as `{ (x, 1; 1, c1) | x = 2; c1 = 3 }`
- bare symbolic matrices such as `(c1, c2*y; x, y)`

Numeric input produces a `MAT_TYPE_NUMBER` matrix. Complex syntax such as
`1+i` or exact rational syntax such as `2/3` is preserved through the
underlying `number_t` values. Symbolic input produces a `MAT_TYPE_EXPR`
matrix.

`mat_from_string(...)` always returns a numeric matrix (`MAT_TYPE_NUMBER`) or
`NULL`:

- purely numeric input returns a numeric matrix
- symbolic input is accepted and evaluated to numeric only if every referenced
  symbol resolves to a concrete numeric value
- if any required symbol remains unbound (for example bare variables or
  partially bound wrapped input), it returns `NULL`

`mat_from_string_expr(...)` is the symbolic-capable entry point. It returns a
`MAT_TYPE_EXPR` matrix for symbolic input and optionally returns bindings
through `bnd_out`.

`mat_expression_from_string(...)` is the complete matrix-expression entry
point. It recognises matrix literals, grouped unary signs, matrix products,
`inverse(...)`, registered unary expression functions such as `exp(...)` and
`sin(...)`, and entrywise calculus forms such as `Dx(...)` and `@S^x(...)`.
This parsing and evaluation belongs to MARSlib; clients pass the input string
through unchanged. When requested, `operation_out` identifies the explicit
operation that was recognised.

Function names and aliases are resolved by the expression module's perfect
hash registry. Matrix parsing therefore has no separate linear name scan and
no duplicate list of accepted spellings: `ln(...)`, `log(...)`, `gamma(...)`,
`Γ(...)`, and every other registered spelling have the same identity in both
expression and matrix input. Expression constructs the scalar function on each
numeric or symbolic diagonal value and evaluates it when the value is numeric;
Matrix is responsible for the matrix decomposition and reconstruction. Symbolic diagonal values retain
the same variable and constant nodes as the original matrix, and
`mat_expression_from_string(...)` returns those live bindings to its caller.

Rows are separated by semicolons. Columns may be separated by commas or by
unambiguous top-level whitespace, so `(1 2; 4 5)` and `(1, 2; 4, 5)` both
produce `(1, 2; 4, 5)`. Whitespace around a scalar operator remains within the
entry. The outer `{ ... | ... }` wrapper, when present, still carries one
binding section for the entire matrix. Bindings are not attached per element.

For symbolic entries, the matrix parser delegates each cell expression to
`expr_from_expression_string(...)` after a small amount of matrix-specific
normalisation. In practice this means each entry accepts the same expression
operators as `expr`, including:

- implicit multiplication such as `xy` and explicit `*`
- Unicode or ASCII subscripts such as `x₀` and `x_0`
- powers such as `x²`, `x^2`, `x^1.5`, and `sin^2(x)`
- the usual unary and binary `expr` functions supported by `expr_from_string(...)`

For bare symbolic input, `mat_from_string_expr(...)` treats the whole matrix as
if it had an implicit
matrix-wide binding block with no assigned values yet. In other words, a bare
symbolic matrix behaves like the wrapped form with every discovered symbol
initialised to `NaN`, ready to be filled in later through the returned
bindings.

In that bare form, the unbound-symbol inference rule matches `expr` parsing:

- built-in valued constants: `e`, `pi`, `π`, `@pi`, `@phi`, and `@gamma`
- constant placeholders: `a`, `b`, `c`, `d`
- indexed constant placeholders such as `a1`, `b2`, `c_7`, and `d₃`
- variables: everything else, including `x`, `y`, `radius`, `Δ`, `Ω`, `τ`,
  `@tau`, and bracketed identifiers like `[radius]`

The built-in-value inference is exact-name only. For example, `@pi` becomes
the built-in constant `π`, but `@pi1`, `@pi2`, and `@pi_3` normalise to
`π₁`, `π₂`, and `π₃` and remain ordinary symbolic variables.

Repeated occurrences of the same normalised symbol name anywhere in one parsed
matrix resolve to the same underlying symbolic leaf. Reusing the same name as
both a variable and a constant in one parse is rejected.

Symbolic names may be written either directly, such as `Δ` and `Ω`, or through
the `@name` aliases already used by `expr`, such as `@DELTA`, `@OMEGA`, `@pi`,
and `@tau`.

Bracketed `expr` identifiers such as `[radius]` are available inside matrix
entries and in the matrix-wide binding section, for example
`{ ([radius], [scale]*x; y, [offset]) | x = 2; [radius] = 3, [scale] = 4, [offset] = 7 }`.
Returned binding names are normalised to the bracket contents, so
`mat_bindings_get(bindings, "[radius]")` and `mat_bindings_get(bindings, "radius")`
both resolve the same symbol.

For compatibility, the older bracket-row forms such as `[[1 2][3 4]]` are still
accepted on input, but the separator-based parenthesised form above is now the
canonical matrix syntax and the format emitted by `mat_to_text(...)`.

If `bnd_out` is non-NULL, `mat_from_string_expr(...)` returns an opaque bindings
object for the names actually referenced by the matrix entries. Use
`mat_bindings_get(...)` to retrieve the borrowed underlying `expr_t *` leaf,
and release the bindings object itself with `mat_bindings_free(...)` when it
is no longer needed. The borrowed `expr_t *` handles remain valid only while
the matrix returned by `mat_from_string_expr(...)` remains alive.

To assign a value, look up the binding and update the returned `expr_t *`
through the ordinary `expr` API:

```c
mat_bindings_t *bindings = NULL;
number_t x = num_create_from_double(2.0);
matrix_t *A = mat_from_string_expr("(x, c1; x*y, [radius])", &bindings);

expr_set_val(mat_bindings_get(bindings, "x"), x);

num_destroy(&x);
mat_bindings_free(bindings);
mat_free(A);
```

If you want to stay at the matrix layer, `mat_deriv_by_name(...)`,
`mat_deriv_trace_by_name(...)`, `mat_deriv_det_by_name(...)`,
`mat_deriv_inverse_by_name(...)`, `mat_deriv_block_inverse_by_name(...)`,
`mat_deriv_solve_by_name(...)`, `mat_deriv_block_solve_by_name(...)`, and
`mat_jacobian_by_names(...)` let you differentiate directly against those
returned binding names without manually extracting the underlying `expr_t *`
symbol first.

`mat_to_text(...)` allocates a freshly formatted `string_t` which the caller
owns and must release with `string_free(...)`.

`mat_to_string(...)` is retained as a compatibility wrapper for C APIs that
still require an owned `char *`; callers must release that buffer with
`free(...)`.

For symbolic matrices, `mat_to_text(...)` omits the outer `{ ... | ... }`
wrapper when every discovered binding is still `NaN`. Once any binding has a
concrete value, the shared matrix-wide binding footer is printed again.

`mat_sprintf_text(...)`, `mat_sprintf(...)`, and `mat_printf(...)` understand
matrix-specific format specifiers:

- `%M`  — inline scientific matrix
- `%m`  — inline pretty matrix
- `%ML` — layout scientific matrix
- `%ml` — layout pretty matrix

---

## Design Notes

### Vtable Dispatch

Every `matrix_t` carries two vtable pointers:

- **element vtable** — arithmetic (`add`, `sub`, `mul`, `div`, `conj`, …),
  conversion helpers, and formatting. One instance per internal element backend.
- **storage vtable** — `get`/`set` and any type-specific fast paths. One instance
  per storage kind.

This means all code paths (arithmetic, eigendecomposition, printing) are generic:
they call through function pointers and never inspect the element type or storage
kind directly.

### Precision of Linear Algebra

**Eigendecomposition, Hermitian path** — cyclic Jacobi sweep. Rotation parameters
(τ, t, c, s) are always real, eigenvalues are real, and eigenvectors are
orthonormal.

**Eigendecomposition, general path** — Hessenberg reduction (Householder
reflectors) followed by implicit single-shift QR (Francis/Wilkinson). The
algorithm works through the numeric `number_t` layer and handles all numeric
matrices uniformly. Hermitian detection compares `A[i,j]` against `conj(A[j,i])`
within a tolerance relative to the Frobenius norm; matrices that pass this test
take the faster Jacobi path automatically.

**Matrix functions** — all use the Schur + Parlett path regardless of whether
the matrix is Hermitian. Numeric results are returned through `MAT_TYPE_NUMBER`.

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
