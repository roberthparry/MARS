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
- matrix functions: exp, sin, cos, tan, sinh, cosh, tanh, sqrt, log, asin, acos, atan, asinh, acosh, atanh, erf, erfc
- power functions: integer power (binary exponentiation), real power via exp/log
- all eigendecomposition computed at full `qfloat_t`/`qcomplex_t` precision regardless of element type; all matrix functions computed at full `qcomplex_t` precision

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
    matrix_t *A = matsq_new_qc(2);

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

#### Allocate without filling

Use these when you need to fill elements sparsely (e.g. a single diagonal or one
off-diagonal element). For bulk initialisation prefer the `mat_create_*` forms below.

| Function | Element type | Description |
|---|---|---|
| `mat_new_d(rows, cols)` | `double` | Allocate an uninitialised `rows × cols` matrix of doubles |
| `mat_new_qf(rows, cols)` | `qfloat_t` | Allocate an uninitialised `rows × cols` matrix of `qfloat_t` |
| `mat_new_qc(rows, cols)` | `qcomplex_t` | Allocate an uninitialised `rows × cols` matrix of `qcomplex_t` |
| `matsq_new_d(n)` | `double` | Allocate an uninitialised `n × n` matrix of doubles |
| `matsq_new_qf(n)` | `qfloat_t` | Allocate an uninitialised `n × n` matrix of `qfloat_t` |
| `matsq_new_qc(n)` | `qcomplex_t` | Allocate an uninitialised `n × n` matrix of `qcomplex_t` |

#### Allocate and fill from a flat array

| Function | Element type | Description |
|---|---|---|
| `mat_create_d(rows, cols, data)` | `double` | Allocate and fill from a row-major `double[]` |
| `mat_create_qf(rows, cols, data)` | `qfloat_t` | Allocate and fill from a row-major `qfloat_t[]` |
| `mat_create_qc(rows, cols, data)` | `qcomplex_t` | Allocate and fill from a row-major `qcomplex_t[]` |

#### Identity matrices

Identity matrices carry no element storage. The first write to any element
materialises the matrix as dense.

| Function | Element type | Description |
|---|---|---|
| `mat_create_identity_d(n)` | `double` | `n × n` identity matrix of doubles |
| `mat_create_identity_qf(n)` | `qfloat_t` | `n × n` identity matrix of `qfloat_t` |
| `mat_create_identity_qc(n)` | `qcomplex_t` | `n × n` identity matrix of `qcomplex_t` |

### Destruction

- `void mat_free(matrix_t *A)` — free all memory owned by `A`, including element storage.

### Element Access

- `void mat_get(const matrix_t *A, size_t i, size_t j, void *out)` — write the element at row `i`, column `j` into the buffer `out`. `out` must be large enough for the element type (8, 16, or 32 bytes).
- `void mat_set(matrix_t *A, size_t i, size_t j, const void *val)` — copy `val` into position `(i, j)`.
- `size_t mat_get_row_count(const matrix_t *A)` — number of rows.
- `size_t mat_get_col_count(const matrix_t *A)` — number of columns.

### Bulk Element Access

- `void mat_set_data(matrix_t *A, const void *data)` — copy all elements from a flat row-major buffer into `A`. The buffer must contain `rows × cols` elements of `A`'s element type.
- `void mat_get_data(const matrix_t *A, void *data)` — copy all elements of `A` into a flat row-major buffer. The buffer must have space for `rows × cols` elements of `A`'s element type.

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

### Matrix Functions

All matrix functions accept a square matrix and return a newly allocated result,
or NULL on error (NULL input, non-square input, or internal allocation failure).

Every matrix function uses the same algorithm: Schur decomposition followed by
the Parlett recurrence on the triangular Schur factor.

1. Compute `A = Q T Q*` (Schur decomposition; T is upper triangular, Q is unitary).
2. Apply the scalar function element-wise to the diagonal of T and propagate off-diagonal
   entries via the Parlett recurrence: `f(T)_{ij} = T_{ij}(f(T_{ii}) − f(T_{jj})) / (T_{ii} − T_{jj})`.
   When `T_{ii} = T_{jj}` the recurrence uses a numerical derivative of the scalar function.
3. Reconstruct `f(A) = Q · f(T) · Q*`.

All internal arithmetic uses `qcomplex_t`. If the input matrix has a narrower element
type the result is converted back to that type before returning.

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
