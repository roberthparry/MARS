#ifndef MATRIX_INTERNAL_H
#define MATRIX_INTERNAL_H

#if !defined(MARS_MATRIX_INTERNAL_ACCESS) &&                                                                           \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "matrix_internal.h is private to the matrix module; include matrix.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "matrix.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

/* ============================================================
   Element kinds
   ============================================================ */

#define MATRIX_SCALAR_STORAGE_BYTES 64u

typedef enum { ELEM_NUMBER = 0, ELEM_EXPR = 1, ELEM_MAX } elem_kind;

/* ============================================================
   Scalar function vtable (per element type)
   ============================================================ */

struct elem_fun_vtable {
    /* elementary functions */
    void (*exp)(void *out, const void *a);
    void (*sin)(void *out, const void *a);
    void (*cos)(void *out, const void *a);
    void (*tan)(void *out, const void *a);

    void (*sinh)(void *out, const void *a);
    void (*cosh)(void *out, const void *a);
    void (*tanh)(void *out, const void *a);

    void (*sqrt)(void *out, const void *a);
    void (*log)(void *out, const void *a);

    /* inverse trig */
    void (*asin)(void *out, const void *a);
    void (*acos)(void *out, const void *a);
    void (*atan)(void *out, const void *a);

    /* inverse hyperbolic */
    void (*asinh)(void *out, const void *a);
    void (*acosh)(void *out, const void *a);
    void (*atanh)(void *out, const void *a);

    /* special functions */
    void (*erf)(void *out, const void *a);
    void (*erfc)(void *out, const void *a);
    void (*erfinv)(void *out, const void *a);
    void (*erfcinv)(void *out, const void *a);
    void (*gamma)(void *out, const void *a);
    void (*lgamma)(void *out, const void *a);
    void (*digamma)(void *out, const void *a);
    void (*trigamma)(void *out, const void *a);
    void (*tetragamma)(void *out, const void *a);
    void (*gammainv)(void *out, const void *a);
    void (*normal_pdf)(void *out, const void *a);
    void (*normal_cdf)(void *out, const void *a);
    void (*normal_logpdf)(void *out, const void *a);
    void (*lambert_w0)(void *out, const void *a);
    void (*lambert_wm1)(void *out, const void *a);
    void (*productlog)(void *out, const void *a);
    void (*ei)(void *out, const void *a);
    void (*e1)(void *out, const void *a);
};

/* ============================================================
   Element vtable
   ============================================================ */

struct elem_vtable {
    size_t size;
    elem_kind kind;
    mat_type_t public_type;

    /* storage lifetime */
    void (*init_zero_slot)(void *slot);
    void (*copy_value)(void *dst, const void *src);
    void (*destroy_value)(void *slot);
    void (*simplify_value)(void *slot);
    bool (*is_structural_zero)(const void *val);

    /* arithmetic */
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
    void (*inv)(void *out, const void *a);

    /* scalar queries */
    void (*conj_elem)(void *out, const void *a);

    /* constants */
    const void *zero;
    const void *one;

    /* comparison */
    int (*cmp)(const void *a, const void *b);

    /* printing */
    string_t *(*format_scalar_text)(const void *val, int scientific);

    const struct elem_fun_vtable *fun;
};

/* ============================================================
   Storage vtable
   ============================================================ */

struct store_vtable {
    struct matrix_t *(*create)(size_t rows, size_t cols, const struct elem_vtable *elem);

    bool (*alloc)(struct matrix_t *A);
    void (*free)(struct matrix_t *A);

    void (*get)(const struct matrix_t *A, size_t i, size_t j, void *out);
    void (*set)(struct matrix_t *A, size_t i, size_t j, const void *val);
    void (*swap_rows)(struct matrix_t *A, size_t r1, size_t r2);
    void (*row_eliminate_from)(struct matrix_t *A, size_t dst_row, size_t src_row, size_t col_start,
                               const void *factor);

    void (*materialise)(struct matrix_t *A);
    bool (*is_sparse_storage)(const struct matrix_t *A);
    bool (*is_sparse_like)(const struct matrix_t *A);
    bool (*is_diagonal)(const struct matrix_t *A);
    bool (*is_upper_triangular)(const struct matrix_t *A);
    bool (*is_lower_triangular)(const struct matrix_t *A);
    size_t (*nonzero_count)(const struct matrix_t *A);
    const struct store_vtable *(*elementwise_unary_store)(const struct matrix_t *A);
    const struct store_vtable *(*transpose_store)(const struct matrix_t *A);
};

struct mat_precision_bucket;

typedef struct matrix_meta_t {
    size_t numeric_min_precision_bits;
    size_t numeric_inexact_count;
    struct mat_precision_bucket *numeric_precision_hist;
} matrix_meta_t;

/* ============================================================
   Matrix object (opaque in matrix.h)
   ============================================================ */

struct matrix_t {
    size_t rows;
    size_t cols;
    size_t nnz;
    matrix_meta_t meta;

    const struct elem_vtable *elem;
    const struct store_vtable *store;

    void **data; /* row pointers for dense/sparse; NULL for identity */
};

/* ============================================================
   Element vtable instances (defined in matrix_vtables.c)
   ============================================================ */

extern const struct elem_vtable number_elem;
extern const struct elem_vtable expr_elem;

bool elem_supports_numeric_algorithms(const struct elem_vtable *elem);
bool elem_is_structural_zero(const struct elem_vtable *elem, const void *val);
void elem_init_zero_value(const struct elem_vtable *elem, void *slot);
void elem_copy_value(const struct elem_vtable *elem, void *dst, const void *src);
void elem_destroy_value(const struct elem_vtable *elem, void *slot);
void elem_simplify_value(const struct elem_vtable *elem, void *slot);
expr_t *expr_clone_for_storage(const expr_t *dv);
matrix_t *mat_finalize_symbolic_result(matrix_t *A);
int mat_simplify_symbolic_inplace(matrix_t *A);

typedef struct {
    expr_t **entries;
    expr_t **additive_constants;
    size_t count;
    expr_t *common_factor;
} mat_expr_beautification_t;

typedef expr_t *(*mat_spectral_expression_map_fn)(const expr_t *spectral_expression, void *context);

int mat_simplify_common_expression_factor(expr_t **entries, size_t count, expr_t **factor_out);
expr_t *mat_simplify_expression_for_beautification(const expr_t *entry);
expr_t *mat_simplify_post_calculus_expression(expr_t *entry);
matrix_t *mat_simplify_exact_principal_sqrt(const matrix_t *A);
matrix_t *mat_simplify_exact_half_integer_power(const matrix_t *A, long numerator);
matrix_t *mat_pow_expr_spectral_map(const matrix_t *A, const expr_t *exponent, mat_spectral_expression_map_fn map,
                                    void *context);
matrix_t *mat_integrate_append_constants(const matrix_t *antiderivative);
int mat_beautify_expression_matrix(const matrix_t *A, mat_expr_beautification_t *beautification);
void mat_expr_beautification_clear(mat_expr_beautification_t *beautification);

/* ============================================================
   Matrix construction helpers (internal)
   ============================================================ */

struct matrix_t *mat_create_dense_with_elem(size_t rows, size_t cols, const struct elem_vtable *elem);
struct matrix_t *mat_create_with_store(size_t rows, size_t cols, const struct elem_vtable *elem,
                                       const struct store_vtable *store);
struct matrix_t *mat_create_zero_with_elem(size_t rows, size_t cols, const struct elem_vtable *elem);
struct matrix_t *mat_create_sparse_with_elem(size_t rows, size_t cols, const struct elem_vtable *elem);
struct matrix_t *mat_create_identity_with_elem(size_t n, const struct elem_vtable *elem);
struct matrix_t *mat_create_diagonal_with_elem(size_t n, const struct elem_vtable *elem);
struct matrix_t *mat_create_upper_triangular_with_elem(size_t rows, size_t cols, const struct elem_vtable *elem);
struct matrix_t *mat_create_lower_triangular_with_elem(size_t rows, size_t cols, const struct elem_vtable *elem);
struct matrix_t *mat_create_elementwise_unary_result(size_t rows, size_t cols, const struct elem_vtable *elem,
                                                     const struct matrix_t *layout_src);
struct matrix_t *mat_create_transpose_result(size_t rows, size_t cols, const struct elem_vtable *elem,
                                             const struct matrix_t *layout_src);
struct matrix_t *mat_copy_with_store(const struct matrix_t *A, const struct store_vtable *store);
struct matrix_t *mat_copy_preserving_store(const struct matrix_t *A);
struct matrix_t *mat_copy_as_dense(const struct matrix_t *A);
struct matrix_t *mat_convert_dense(const struct matrix_t *A, const struct elem_vtable *target);
bool mat_has_diagonal_structure(const struct matrix_t *A);
bool mat_has_upper_triangular_structure(const struct matrix_t *A);
bool mat_has_lower_triangular_structure(const struct matrix_t *A);
const struct elem_vtable *mat_binary_result_elem(const struct matrix_t *A, const struct matrix_t *B);
struct matrix_t *mat_convert_preserving_store(const struct matrix_t *A, const struct elem_vtable *target);
struct matrix_t *mat_convert_with_store(const struct matrix_t *A, const struct elem_vtable *target,
                                        const struct store_vtable *store);
const struct store_vtable *mat_sparse_factor_store(const struct matrix_t *A,
                                                   const struct store_vtable *structured_store);
void mat_get_owned(const struct matrix_t *A, size_t i, size_t j, void *out);
void mat_value_init_zero(const struct matrix_t *A, void *slot);
void mat_value_destroy(const struct matrix_t *A, void *slot);
number_t mat_raw_value_to_number(const struct elem_vtable *elem, const void *value);
void mat_raw_value_from_number(const struct elem_vtable *elem, void *out, const number_t *value);
void mat_set_num_owned(struct matrix_t *A, size_t i, size_t j, number_t *value);

static inline void mat_set_num_clone(struct matrix_t *A, size_t i, size_t j, const number_t *value)
{
    number_t copy = num_is_immortal(*value) ? *value : num_clone(*value);
    mat_set_num_owned(A, i, j, &copy);
}
size_t mat_cached_numeric_precision_bits(const struct matrix_t *A);
void mat_numeric_precision_note_set(struct matrix_t *A, const void *old_val, const void *new_val);
void mat_numeric_precision_release(struct matrix_t *A);
void dense_swap_rows(struct matrix_t *A, size_t r1, size_t r2);
struct matrix_t *mat_extract_block(const struct matrix_t *A, size_t row0, size_t rows, size_t col0, size_t cols);
bool mat_insert_block(struct matrix_t *A, size_t row0, size_t col0, const struct matrix_t *B);
struct matrix_t *mat_build_block_2x2(const struct matrix_t *B11, const struct matrix_t *B12, const struct matrix_t *B21,
                                     const struct matrix_t *B22);
struct matrix_t *mat_const_identity_with_elem(size_t n, const struct elem_vtable *elem, const void *scalar);
matrix_t *mat_create_direct_solve_result(const matrix_t *A, const matrix_t *B, const struct elem_vtable *elem);

/* Shared structured store used outside matrix_core.c (defined in matrix_vtables.c). */
extern const struct store_vtable dense_store;
extern const struct store_vtable sparse_store;
extern const struct store_vtable identity_store;
extern const struct store_vtable diagonal_store;
extern const struct store_vtable upper_triangular_store;
extern const struct store_vtable lower_triangular_store;

/* ============================================================
   Convenience accessor
   ============================================================ */

static inline const struct elem_vtable *elem_of(const struct matrix_t *A)
{
    return A ? A->elem : NULL;
}

static inline bool elem_is_symbolic(const struct elem_vtable *elem)
{
    return elem && elem->kind == ELEM_EXPR;
}

static inline bool matrix_is_symbolic(const struct matrix_t *A)
{
    return elem_is_symbolic(elem_of(A));
}

/* ============================================================
   Schur decomposition API (internal use by matrix_maths.c)
   ============================================================ */

/**
 * Compute the Schur decomposition A = Q T Q*.
 *
 * A must be square. Q and T are allocated on success.
 * Returns 0 on success, nonzero on failure.
 */
int mat_schur_factor(const matrix_t *A, mat_schur_factor_t *out);

/**
 * Free the Q and T matrices inside a mat_schur_factor_t.
 */
void mat_schur_factor_free(mat_schur_factor_t *S);

/* ============================================================
   Shared exact symbolic helpers for numeric decomposition glue
   ============================================================ */

matrix_t *mat_nullspace_expr_exact(const matrix_t *A);
int mat_rank_expr_exact(const matrix_t *A);
matrix_t *mat_pseudoinverse_expr_exact(const matrix_t *A);
int mat_eigendecompose_expr(const matrix_t *A, expr_t **eigenvalues, matrix_t **eigenvectors);
matrix_t *mat_solve_expr_exact(const matrix_t *A, const matrix_t *B);
int mat_det_expr_exact(const matrix_t *A, expr_t **determinant);
matrix_t *mat_inverse_expr_exact(const matrix_t *A);
matrix_t *mat_charpoly_numeric(const matrix_t *A);
matrix_t *mat_charpoly_expr(const matrix_t *A);
matrix_t *mat_minpoly_expr(const matrix_t *A);
matrix_t *mat_adjugate_exact(const matrix_t *A);

/* ============================================================
   Matrix functions via Schur + Parlett (internal)
   ============================================================ */

/* Apply scalar function f to an upper triangular matrix T. */
matrix_t *mat_fun_triangular(const matrix_t *T, void (*scalar_f)(void *out, const void *in));

/* High-level Schur-based matrix function engine. */
matrix_t *mat_fun_schur(const matrix_t *A, void (*scalar_f)(void *out, const void *in));

/* Internal Hermitian detector used to select stable fast paths. */
bool mat_is_hermitian(const matrix_t *A);

/* Drop any internal matrix-function cache associated with A. */
void mat_fun_cache_forget(const matrix_t *A);

#endif /* MATRIX_INTERNAL_H */
