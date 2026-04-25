# `matrix_t`

`matrix_t` is a generic high-precision matrix type with pluggable element types
(`double`, `qfloat_t`, `qcomplex_t`) and pluggable storage kinds (dense, identity,
diagonal). All operations dispatch through internal vtables; no type switches or
storage switches appear in user code.

## Representation

`matrix_t` is an opaque struct. Clients hold a pointer and access it only through
the API. Internally each matrix carries:

- a pointer to an element vtable (arithmetic, conversion, formatting)
- a pointer to a storage vtable (get/set, iteration)
- a `rows × cols` size pair
- a storage payload (a flat `double[]`, `qfloat_t[]`, or `qcomplex_t[]` for dense
  matrices; nothing for identity matrices)

## Capabilities

- element types: `double`, `qfloat_t` (~31–32 decimal digits), `qcomplex_t` (~31–32 decimal digits)
- storage kinds: dense (fully materialised), identity (zero storage, materialises on write)
- arithmetic: scalar multiply/divide, matrix add, subtract, multiply
- structural: transpose, conjugate, Hermitian (conjugate transpose)
- linear algebra: determinant, inverse, eigenvalues, eigendecomposition, eigenvectors
- matrix functions: exp, sin, cos, tan, sinh, cosh, tanh, sqrt, log, asin, acos, atan, asinh, acosh, atanh
- all linear algebra computed at full `qfloat_t` precision regardless of element type

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
    matrix_t *A = matsq_create_qc(2);

    qcomplex_t a00 = qc_make(qf_from_double(2.0), QF_ZERO);
    qcomplex_t a01 = qc_make(qf_from_double(1.0), qf_from_double( 1.0));
    qcomplex_t a10 = qc_make(qf_from_double(1.0), qf_from_double(-1.0));
    qcomplex_t a11 = qc_make(qf_from_double(3.0), QF_ZERO);

    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);

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

### Construction

#### Rectangular dense matrices

| Function | Element type | Description |
|---|---|---|
| `mat_create_d(rows, cols)` | `double` | Allocate a `rows × cols` dense matrix of doubles |
| `mat_create_qf(rows, cols)` | `qfloat_t` | Allocate a `rows × cols` dense matrix of `qfloat_t` |
| `mat_create_qc(rows, cols)` | `qcomplex_t` | Allocate a `rows × cols` dense matrix of `qcomplex_t` |

#### Square dense matrices

| Function | Element type | Description |
|---|---|---|
| `matsq_create_d(n)` | `double` | Allocate an `n × n` dense matrix of doubles |
| `matsq_create_qf(n)` | `qfloat_t` | Allocate an `n × n` dense matrix of `qfloat_t` |
| `matsq_create_qc(n)` | `qcomplex_t` | Allocate an `n × n` dense matrix of `qcomplex_t` |

#### Identity matrices

Identity matrices carry no element storage. The first write to an off-diagonal
element materialises the matrix as dense.

| Function | Element type | Description |
|---|---|---|
| `matsq_ident_d(n)` | `double` | `n × n` identity matrix of doubles |
| `matsq_ident_qf(n)` | `qfloat_t` | `n × n` identity matrix of `qfloat_t` |
| `matsq_ident_qc(n)` | `qcomplex_t` | `n × n` identity matrix of `qcomplex_t` |

### Destruction

- `void mat_free(matrix_t *A)` — free all memory owned by `A`, including element storage.

### Element Access

- `void mat_get(const matrix_t *A, size_t i, size_t j, void *out)` — write the element at row `i`, column `j` into the buffer `out`. `out` must be large enough for the element type (8, 16, or 32 bytes).
- `void mat_set(matrix_t *A, size_t i, size_t j, const void *val)` — copy `val` into position `(i, j)`.
- `size_t mat_get_row_count(const matrix_t *A)` — number of rows.
- `size_t mat_get_col_count(const matrix_t *A)` — number of columns.

### Scalar Operations

Scalar functions return a new matrix. The scalar is broadcast to every element.
Mixed scalar/element types are supported; the result element type is the wider of
the two.

| Function | Scalar type | Description |
|---|---|---|
| `mat_scalar_mul_d(s, A)` | `double` | `s * A` |
| `mat_scalar_mul_qf(s, A)` | `qfloat_t` | `s * A` |
| `mat_scalar_mul_qc(s, A)` | `qcomplex_t` | `s * A` |
| `mat_scalar_div_d(s, A)` | `double` | `A / s` |
| `mat_scalar_div_qf(s, A)` | `qfloat_t` | `A / s` |
| `mat_scalar_div_qc(s, A)` | `qcomplex_t` | `A / s` |

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
- **General matrices** — Schur decomposition path; eigenvectors are the columns of
  the Schur factor matrix transformed back to the original basis.

Return values: `0` on success, negative on error.

#### Eigenvectors only

```c
matrix_t *mat_eigenvectors(const matrix_t *A);
```

Convenience wrapper around `mat_eigendecompose` that discards the eigenvalues.
Returns a newly allocated eigenvector matrix, or NULL on error.

### Matrix Functions

All matrix functions accept a square matrix and return a newly allocated result,
or NULL on error (NULL input, non-square input, or internal allocation failure).

#### Via eigendecomposition (Hermitian matrices)

For Hermitian matrices these functions use the eigendecomposition
`A = V D V†`, apply the scalar function to each eigenvalue, then reconstruct:
`f(A) = V · diag(f(λᵢ)) · V†`.

| Function | Description |
|---|---|
| `mat_exp(A)` | Matrix exponential `exp(A)` |
| `mat_sin(A)` | Matrix sine `sin(A)` |
| `mat_cos(A)` | Matrix cosine `cos(A)` |
| `mat_tan(A)` | Matrix tangent `tan(A)` |
| `mat_sinh(A)` | Matrix hyperbolic sine `sinh(A)` |
| `mat_cosh(A)` | Matrix hyperbolic cosine `cosh(A)` |
| `mat_tanh(A)` | Matrix hyperbolic tangent `tanh(A)` |

#### Via Denman–Beavers iteration

```c
matrix_t *mat_sqrt(const matrix_t *A);
```

Computes the principal square root of `A` using the Denman–Beavers coupled
iteration `X_{k+1} = (X_k + Y_k⁻¹)/2`, `Y_{k+1} = (Y_k + X_k⁻¹)/2`, which
converges quadratically. The zero matrix is handled as a special case
(returns a zero matrix immediately). Returns NULL for singular inputs where no
square root exists.

#### Via repeated square root and Taylor series

```c
matrix_t *mat_log(const matrix_t *A);
```

Computes the principal matrix logarithm. Reduces `A` by repeated square-rooting
until `‖B − I‖_F ≤ 0.5`, computes `log(I + C) = C − C²/2 + C³/3 − …` via
Taylor series (convergence guaranteed in that neighbourhood), then multiplies
the result by `2^m` where `m` is the number of halvings.

#### Via Taylor series

The inverse trigonometric and inverse hyperbolic functions are computed by
power-series expansion. The series use `A²` as the step matrix, so each term
costs one additional matrix multiply. Convergence requires `‖A‖ < 1` for
`asin`, `atan`, `atanh`; `‖A‖ > 1` for `acosh` (which uses a different
formula); and `asinh` converges for all `A`.

| Function | Formula / series | Domain |
|---|---|---|
| `mat_asin(A)` | `A + (1/6)A³ + (3/40)A⁵ + …` | `‖A‖ ≤ 1` |
| `mat_acos(A)` | `(π/2)I − mat_asin(A)` | `‖A‖ ≤ 1` |
| `mat_atan(A)` | `A − (1/3)A³ + (1/5)A⁵ − …` | `‖A‖ < 1` |
| `mat_asinh(A)` | `A − (1/6)A³ + (3/40)A⁵ − …` | all `A` |
| `mat_acosh(A)` | `mat_log(A + mat_sqrt(A² − I))` | `‖A‖ ≥ 1` |
| `mat_atanh(A)` | `A + (1/3)A³ + (1/5)A⁵ + …` | `‖A‖ < 1` |

### Debugging / I/O

- `void mat_print(const matrix_t *A)` — print the matrix to standard output.

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

**Hermitian path** — cyclic Jacobi sweep. Rotation parameters (τ, t, c, s) are
always real; they are computed through `qfloat_t` arithmetic, giving ~31 decimal
digits of precision regardless of the matrix's element type. Eigenvalues are real
and eigenvectors are orthonormal.

**General path** — Hessenberg reduction (Householder reflectors) followed by
implicit single-shift QR (Francis/Wilkinson). All internal arithmetic uses
`qcomplex_t` flat arrays, so the algorithm handles real, qfloat, and complex
matrices uniformly. Hermitian detection compares `A[i,j]` against `conj(A[j,i])`
within a tolerance relative to the Frobenius norm; matrices that pass this test
take the faster Jacobi path automatically.

### Identity Storage

Identity matrices store no element data. Reading `(i, i)` returns one; reading
`(i, j)` for `i ≠ j` returns zero. The first write to any element transparently
materialises the matrix as dense, after which it behaves identically to a dense
matrix created with `matsq_create_*`.
