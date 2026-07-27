#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* ============================================================
   Internal helpers
   ============================================================ */

typedef struct mat_fun_cache_entry {
    const matrix_t *key;
    matrix_t *spectral_Vq;
    number_t *spectral_evals;
    matrix_t *exp_preimage;
    struct mat_fun_cache_entry *next;
} mat_fun_cache_entry_t;

static mat_fun_cache_entry_t *mat_fun_cache_head = NULL;

static number_t mat_eval_number_scalar_number_local(void (*scalar_f)(void *out, const void *in),
                                                    const number_t *input)
{
    NUM_SCOPE(scope);
    number_t output = number_invalid();
    number_t safe_input = input ? num_clone(*input) : num_clone(NUM_ZERO);

    scalar_f(&output, &safe_input);
    return num_scope_detach(output);
}

static void mat_fun_number_array_invalidate(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0; i < count; ++i)
        values[i] = number_invalid();
}

static void mat_fun_number_array_destroy(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
}

static void mat_fun_cache_free_spectral_evals(number_t *values, size_t count)
{
    mat_fun_number_array_destroy(values, count);
    free(values);
}

static matrix_t *mat_fun_apply(const matrix_t *A,
                               void (*number_f)(void *out, const void *a),
                               void (*expr_f)(void *out, const void *a),
                               void (*native_f)(void *out, const void *a));
static matrix_t *mat_fun_expr_structured(const matrix_t *A,
                                         void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_elementwise_same_type(const matrix_t *A,
                                               void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_expr_uniform_diag_offdiag(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_expr_scalar_plus_rank_one(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in));
static matrix_t *mat_fun_expr_quartic_biquadratic_exact(const matrix_t *A,
                                                        void (*scalar_f)(void *out, const void *in));
static int expr_fun_coeffs_up_to_second(expr_t **c0,
                                        expr_t **c1,
                                        expr_t **c2,
                                        void (*scalar_f)(void *out, const void *in),
                                        expr_t *lambda);
static matrix_t *mat_exp_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_log_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_sqrt_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_sin_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_cos_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_tan_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_sinh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_cosh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_tanh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_asin_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_acos_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_asinh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_acosh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_atan_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_atanh_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_erf_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_erfc_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_erfinv_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_erfcinv_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_gamma_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_lgamma_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_digamma_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_trigamma_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_gammainv_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_normal_pdf_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_normal_cdf_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_normal_logpdf_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_lambert_w0_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_lambert_wm1_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_productlog_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_ei_number_triangular_equal_diag(const matrix_t *A);
static matrix_t *mat_e1_number_triangular_equal_diag(const matrix_t *A);

static matrix_t *mat_apply_unary(const matrix_t *A,
                                 void (*number_f)(void *out, const void *a),
                                 void (*expr_f)(void *out, const void *a),
                                 void (*native_f)(void *out, const void *a))
{
    return mat_fun_apply(A, number_f, expr_elem.fun ? expr_f : NULL, native_f);
}

static int mat_elem_supports_numeric_algorithms(const matrix_t *A)
{
    return A && !matrix_is_symbolic(A);
}

static void mat_free_if_distinct(matrix_t *A, const matrix_t *keep)
{
    if (A && A != keep)
        mat_free(A);
}

static void mat_free_power_seed_pair(matrix_t *power, matrix_t *seed)
{
    if (power == seed) {
        mat_free(seed);
        return;
    }

    mat_free(power);
    mat_free(seed);
}

static matrix_t *mat_fun_elementwise_same_type(const matrix_t *A,
                                               void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *elem;
    matrix_t *R;
    unsigned char in_raw[MATRIX_SCALAR_STORAGE_BYTES];
    unsigned char out_raw[MATRIX_SCALAR_STORAGE_BYTES];

    if (!A || !scalar_f)
        return NULL;

    elem = A->elem;
    if (!elem)
        return NULL;

    R = mat_create_elementwise_unary_result(A->rows, A->cols, elem, A);
    if (!R)
        return NULL;

    mat_value_init_zero(A, in_raw);
    mat_value_init_zero(R, out_raw);

    for (size_t i = 0; i < A->rows; ++i) {
        mat_value_destroy(A, in_raw);
        mat_value_destroy(R, out_raw);
        mat_value_init_zero(A, in_raw);
        mat_value_init_zero(R, out_raw);
        mat_get_owned(A, i, i, in_raw);
        scalar_f(out_raw, in_raw);
        mat_set(R, i, i, out_raw);
    }

    mat_value_destroy(A, in_raw);
    mat_value_destroy(R, out_raw);
    return R;
}

static bool expr_is_zero_local(const expr_t *dv)
{
    return !dv || expr_is_exact_zero(dv);
}

static expr_t *expr_fun_first_derivative_at_zero_local(
    void (*scalar_f)(void *out, const void *in))
{
    expr_t *zero = NULL;
    expr_t *c0 = NULL;
    expr_t *c1 = NULL;

    if (!scalar_f)
        return NULL;

    zero = expr_const_zero();
    if (!zero)
        return NULL;

    if (expr_fun_coeffs_up_to_second(&c0, &c1, NULL, scalar_f, zero) != 0) {
        expr_free(zero);
        return NULL;
    }

    expr_free(zero);
    expr_free(c0);
    return c1;
}

static bool expr_equal_exact_local(const expr_t *a, const expr_t *b)
{
    expr_t *diff;
    bool equal;

    if (a == b)
        return true;
    if (!a)
        return expr_is_zero_local(b);
    if (!b)
        return expr_is_zero_local(a);

    expr_retain((expr_t *)a);
    expr_retain((expr_t *)b);
    diff = expr_sub_simplify_owned((expr_t *)a, (expr_t *)b);
    if (!diff)
        return false;

    equal = expr_is_exact_zero(diff);
    expr_free(diff);
    return equal;
}

static expr_t *expr_mul_or_zero_owned_local(const expr_t *a, const expr_t *b)
{
    if (expr_is_zero_local(a) || expr_is_zero_local(b))
        return expr_const_zero();

    expr_retain((expr_t *)a);
    expr_retain((expr_t *)b);
    return expr_mul_simplify_owned((expr_t *)a, (expr_t *)b);
}

static bool mat_number_diagonal_equal_local(const matrix_t *A)
{
    NUM_SCOPE(scope);
    number_t diag0;
    bool equal = true;

    if (!A || A->rows != A->cols || A->rows == 0 || A->elem != &number_elem)
        return false;

    diag0 = mat_get_num(A, 0, 0);
    for (size_t i = 1; i < A->rows; ++i) {
        NUM_SCOPE(iter_scope);
        number_t diag_i = mat_get_num(A, i, i);
        if (!num_eq(diag_i, diag0))
            equal = false;
        if (!equal)
            break;
    }
    return equal;
}

static matrix_t *mat_number_strict_triangular_copy(const matrix_t *A, bool upper)
{
    size_t n;
    matrix_t *N;

    if (!A || A->rows != A->cols || A->elem != &number_elem)
        return NULL;

    n = A->rows;
    N = upper ? mat_create_upper_triangular_with_elem(n, n, &number_elem)
              : mat_create_lower_triangular_with_elem(n, n, &number_elem);
    if (!N)
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            bool active = upper ? (j > i) : (i > j);
            number_t value;

            if (!active)
                continue;

            value = mat_get_num(A, i, j);
            mat_set_num_owned(N, i, j, &value);
        }
    }

    return N;
}

static matrix_t *mat_number_triangular_mul(const matrix_t *A,
                                           const matrix_t *B,
                                           bool upper)
{
    size_t n;
    matrix_t *C;

    if (!A || !B || A->rows != A->cols || B->rows != B->cols ||
        A->rows != B->rows || A->elem != &number_elem || B->elem != &number_elem)
        return NULL;

    n = A->rows;
    C = upper ? mat_create_upper_triangular_with_elem(n, n, &number_elem)
              : mat_create_lower_triangular_with_elem(n, n, &number_elem);
    if (!C)
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            bool active = upper ? (j > i) : (i > j);
            number_t sum;
            size_t k_begin;
            size_t k_end;

            if (!active)
                continue;

            sum = NUM_ZERO;
            k_begin = upper ? i + 1u : j;
            k_end = upper ? j + 1u : i + 1u;

            for (size_t k = k_begin; k < k_end; ++k) {
                number_t aik = mat_get_num(A, i, k);
                number_t bkj = mat_get_num(B, k, j);
                number_t prod = num_mul(aik, bkj);
                number_t next = num_add(sum, prod);

                num_destroy(&prod);
                num_destroy(&bkj);
                num_destroy(&aik);
                num_destroy(&sum);
                sum = next;
            }

            if (!num_eq(sum, NUM_ZERO)) {
                mat_set_num_owned(C, i, j, &sum);
            } else {
                num_destroy(&sum);
            }
        }
    }

    return C;
}

static int mat_number_add_scaled_triangular(matrix_t *F,
                                            const matrix_t *Npower,
                                            const number_t coeff,
                                            bool upper)
{
    size_t n;

    if (!F || !Npower || F->rows != F->cols || Npower->rows != Npower->cols ||
        F->rows != Npower->rows || F->elem != &number_elem || Npower->elem != &number_elem)
        return -1;

    n = F->rows;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            bool active = upper ? (j > i) : (i > j);
            number_t np_ij;
            number_t term;
            number_t fij;
            number_t sum;

            if (!active)
                continue;

            np_ij = mat_get_num(Npower, i, j);
            if (num_eq(np_ij, NUM_ZERO)) {
                num_destroy(&np_ij);
                continue;
            }

            term = num_mul(coeff, np_ij);
            num_destroy(&np_ij);
            fij = mat_get_num(F, i, j);
            sum = num_add(fij, term);
            num_destroy(&fij);
            num_destroy(&term);
            mat_set_num_owned(F, i, j, &sum);
        }
    }

    return 0;
}

static matrix_t *mat_exp_number_triangular_equal_diag_upper(const matrix_t *T)
{
    size_t n;
    matrix_t *F = NULL;
    matrix_t *N = NULL;
    matrix_t *Npower = NULL;
    number_t lambda;
    number_t coeff0;

    if (!T || T->rows != T->cols || T->elem != &number_elem || !mat_is_upper_triangular(T))
        return NULL;

    n = T->rows;
    if (n == 0)
        return mat_copy_preserving_store(T);

    lambda = mat_get_num(T, 0, 0);
    coeff0 = num_exp(lambda);
    num_destroy(&lambda);

    F = mat_create_upper_triangular_with_elem(n, n, &number_elem);
    N = mat_number_strict_triangular_copy(T, true);
    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        num_destroy(&coeff0);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mat_set_num_clone(F, i, i, &coeff0);
    }

    Npower = N;
    for (size_t k = 1; k < n; ++k) {
        number_t k_num = num_create_from_long((long)k);
        number_t coeff = num_div(coeff0, k_num);

        for (size_t d = 1; d < k; ++d) {
            number_t d_num = num_create_from_long((long)d);
            number_t next = num_div(coeff, d_num);
            num_destroy(&coeff);
            num_destroy(&d_num);
            coeff = next;
        }

        if (mat_number_add_scaled_triangular(F, Npower, coeff, true) != 0) {
            num_destroy(&coeff);
            num_destroy(&k_num);
            mat_free_power_seed_pair(Npower, N);
            mat_free(F);
            num_destroy(&coeff0);
            return NULL;
        }

        num_destroy(&coeff);
        num_destroy(&k_num);

        if (k + 1u < n) {
            matrix_t *next = mat_number_triangular_mul(Npower, N, true);
            mat_free_if_distinct(Npower, N);
            Npower = next;
            if (!Npower) {
                mat_free(F);
                mat_free(N);
                num_destroy(&coeff0);
                return NULL;
            }
        }
    }

    mat_free_power_seed_pair(Npower, N);
    num_destroy(&coeff0);
    return F;
}

static matrix_t *mat_exp_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *T = NULL;
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_number_diagonal_equal_local(A))
        return NULL;

    if (mat_is_upper_triangular(A))
        return mat_exp_number_triangular_equal_diag_upper(A);
    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_exp_number_triangular_equal_diag_upper(T);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

static matrix_t *mat_number_series_from_scaled_strict_triangular(
    const matrix_t *A,
    bool upper,
    number_t diag_value,
    number_t scaled_coeff0,
    number_t (*next_coeff)(const number_t prev_coeff, size_t k, void *ctx),
    void *ctx)
{
    size_t n;
    matrix_t *F = NULL;
    matrix_t *S = NULL;
    matrix_t *Spower = NULL;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !next_coeff)
        return NULL;

    n = A->rows;
    F = upper ? mat_create_upper_triangular_with_elem(n, n, &number_elem)
              : mat_create_lower_triangular_with_elem(n, n, &number_elem);
    S = mat_number_strict_triangular_copy(A, upper);
    if (!F || !S) {
        mat_free(F);
        mat_free(S);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mat_set_num_clone(F, i, i, &diag_value);
    }

    if (n == 0) {
        mat_free(S);
        return F;
    }

    Spower = S;
    {
        number_t coeff = num_clone(scaled_coeff0);

        for (size_t k = 1; k < n; ++k) {
            if (mat_number_add_scaled_triangular(F, Spower, coeff, upper) != 0) {
                num_destroy(&coeff);
                mat_free_power_seed_pair(Spower, S);
                mat_free(F);
                return NULL;
            }

            if (k + 1u < n) {
                matrix_t *next = mat_number_triangular_mul(Spower, S, upper);
                number_t next_coeff_value = next_coeff(coeff, k + 1u, ctx);

                num_destroy(&coeff);
                coeff = next_coeff_value;
                mat_free_if_distinct(Spower, S);
                Spower = next;
                if (!Spower) {
                    num_destroy(&coeff);
                    mat_free(S);
                    mat_free(F);
                    return NULL;
                }
            } else {
                num_destroy(&coeff);
            }
        }
    }

    mat_free_power_seed_pair(Spower, S);
    return F;
}

static matrix_t *mat_number_unary_taylor_from_expr_upper(
    const matrix_t *A,
    expr_t *(*build_expr)(const expr_t *))
{
    size_t n;
    matrix_t *F = NULL;
    matrix_t *N = NULL;
    matrix_t *Npower = NULL;
    number_t lambda;
    expr_t *x = NULL;
    expr_t **derivs = NULL;

    if (!A || !build_expr || A->rows != A->cols || A->elem != &number_elem ||
        !mat_is_upper_triangular(A))
        return NULL;

    n = A->rows;
    if (n == 0u)
        return mat_copy_preserving_store(A);

    lambda = mat_get_num(A, 0, 0);
    x = expr_new_named_var(num_clone(lambda), "x");
    if (!x) {
        num_destroy(&lambda);
        return NULL;
    }

    derivs = calloc(n, sizeof(*derivs));
    if (!derivs) {
        expr_free(x);
        num_destroy(&lambda);
        return NULL;
    }

    derivs[0] = build_expr(x);
    if (!derivs[0])
        goto fail;

    F = mat_create_upper_triangular_with_elem(n, n, &number_elem);
    N = mat_number_strict_triangular_copy(A, true);
    if (!F || !N)
        goto fail;

    {
        number_t diag_value = expr_eval(derivs[0]);

        for (size_t i = 0; i < n; ++i) {
            mat_set_num_clone(F, i, i, &diag_value);
        }
        num_destroy(&diag_value);
    }

    if (n == 1u) {
        free(derivs);
        expr_free(x);
        mat_free(N);
        num_destroy(&lambda);
        return F;
    }

    Npower = N;
    {
        number_t factorial = num_create_from_long(1);

        for (size_t k = 1; k < n; ++k) {
            number_t coeff;

            derivs[k] = expr_create_deriv(derivs[k - 1u], x);
            if (!derivs[k]) {
                num_destroy(&factorial);
                goto fail;
            }

            {
                number_t k_num = num_create_from_long((long)k);
                number_t next_factorial = num_mul(factorial, k_num);
                num_destroy(&k_num);
                num_destroy(&factorial);
                factorial = next_factorial;
            }

            coeff = expr_eval(derivs[k]);
            if (k > 1u) {
                number_t scaled = num_div(coeff, factorial);
                num_destroy(&coeff);
                coeff = scaled;
            }

            if (mat_number_add_scaled_triangular(F, Npower, coeff, true) != 0) {
                num_destroy(&coeff);
                num_destroy(&factorial);
                goto fail;
            }
            num_destroy(&coeff);

            if (k + 1u < n) {
                matrix_t *next = mat_number_triangular_mul(Npower, N, true);
                mat_free_if_distinct(Npower, N);
                Npower = next;
                if (!Npower) {
                    num_destroy(&factorial);
                    goto fail;
                }
            }
        }

        num_destroy(&factorial);
    }

    for (size_t i = 0; i < n; ++i)
        expr_free(derivs[i]);
    free(derivs);
    expr_free(x);
    mat_free_power_seed_pair(Npower, N);
    num_destroy(&lambda);
    return F;

fail:
    if (derivs) {
        for (size_t i = 0; i < n; ++i)
            expr_free(derivs[i]);
        free(derivs);
    }
    expr_free(x);
    mat_free_power_seed_pair(Npower, N);
    mat_free(F);
    num_destroy(&lambda);
    return NULL;
}

static matrix_t *mat_number_unary_taylor_from_expr(
    const matrix_t *A,
    expr_t *(*build_expr)(const expr_t *))
{
    matrix_t *T = NULL;
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!A || !build_expr || A->rows != A->cols || A->elem != &number_elem ||
        !mat_number_diagonal_equal_local(A))
        return NULL;

    if (mat_is_upper_triangular(A))
        return mat_number_unary_taylor_from_expr_upper(A, build_expr);
    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_number_unary_taylor_from_expr_upper(T, build_expr);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

typedef struct mat_number_log_series_ctx {
    number_t neg_one;
} mat_number_log_series_ctx_t;

static number_t mat_number_log_next_coeff(const number_t prev_coeff, size_t k, void *ctx_void)
{
    mat_number_log_series_ctx_t *ctx = (mat_number_log_series_ctx_t *)ctx_void;
    number_t ratio_num = num_create_from_long((long)(k - 1u));
    number_t ratio_den = num_create_from_long((long)k);
    number_t ratio = num_div(ratio_num, ratio_den);
    number_t signed_ratio = num_mul(ctx->neg_one, ratio);
    number_t next = num_mul(prev_coeff, signed_ratio);

    num_destroy(&signed_ratio);
    num_destroy(&ratio);
    num_destroy(&ratio_den);
    num_destroy(&ratio_num);
    return next;
}

static matrix_t *mat_log_number_triangular_equal_diag_upper(const matrix_t *A)
{
    number_t lambda;
    number_t diag_value;
    number_t inv_lambda;
    mat_number_log_series_ctx_t ctx;
    matrix_t *out;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_is_upper_triangular(A))
        return NULL;

    lambda = mat_get_num(A, 0, 0);
    if (num_eq(lambda, NUM_ZERO)) {
        num_destroy(&lambda);
        return NULL;
    }

    diag_value = num_log(lambda);
    inv_lambda = num_inv(lambda);
    ctx.neg_one = num_create_from_long(-1);
    out = mat_number_series_from_scaled_strict_triangular(A, true, diag_value, inv_lambda,
                                                          mat_number_log_next_coeff, &ctx);
    num_destroy(&ctx.neg_one);
    num_destroy(&inv_lambda);
    num_destroy(&diag_value);
    num_destroy(&lambda);
    return out;
}

static matrix_t *mat_log_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *T = NULL;
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_number_diagonal_equal_local(A))
        return NULL;

    if (mat_is_upper_triangular(A))
        return mat_log_number_triangular_equal_diag_upper(A);
    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_log_number_triangular_equal_diag_upper(T);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

typedef struct mat_number_sqrt_series_ctx {
    number_t alpha;
} mat_number_sqrt_series_ctx_t;

static number_t mat_number_sqrt_next_coeff(const number_t prev_coeff, size_t k, void *ctx_void)
{
    NUM_SCOPE(scope);
    mat_number_sqrt_series_ctx_t *ctx = (mat_number_sqrt_series_ctx_t *)ctx_void;
    number_t km1 = num_create_from_long((long)(k - 1u));
    number_t diff = num_sub(ctx->alpha, km1);
    number_t k_num = num_create_from_long((long)k);
    number_t ratio = num_div(diff, k_num);
    number_t next = num_mul(prev_coeff, ratio);

    return num_scope_detach(next);
}

static matrix_t *mat_sqrt_number_triangular_equal_diag_upper(const matrix_t *A)
{
    number_t lambda;
    number_t diag_value;
    number_t inv_lambda;
    number_t coeff0;
    mat_number_sqrt_series_ctx_t ctx;
    matrix_t *out;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_is_upper_triangular(A))
        return NULL;

    lambda = mat_get_num(A, 0, 0);
    if (num_eq(lambda, NUM_ZERO)) {
        num_destroy(&lambda);
        return NULL;
    }

    diag_value = num_sqrt(lambda);
    inv_lambda = num_inv(lambda);
    ctx.alpha = num_create_from_string("1/2");
    coeff0 = num_mul(diag_value, inv_lambda);
    {
        number_t seeded = num_mul(coeff0, ctx.alpha);
        num_destroy(&coeff0);
        coeff0 = seeded;
    }
    out = mat_number_series_from_scaled_strict_triangular(A, true, diag_value, coeff0,
                                                          mat_number_sqrt_next_coeff, &ctx);
    num_destroy(&ctx.alpha);
    num_destroy(&coeff0);
    num_destroy(&inv_lambda);
    num_destroy(&diag_value);
    num_destroy(&lambda);
    return out;
}

static matrix_t *mat_sqrt_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *T = NULL;
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_number_diagonal_equal_local(A))
        return NULL;

    if (mat_is_upper_triangular(A))
        return mat_sqrt_number_triangular_equal_diag_upper(A);
    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_sqrt_number_triangular_equal_diag_upper(T);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

static int expr_fun_coeffs_up_to_second(expr_t **c0,
                                        expr_t **c1,
                                        expr_t **c2,
                                        void (*scalar_f)(void *out, const void *in),
                                        expr_t *lambda)
{
    expr_t *u;
    expr_t *f_u = NULL;
    expr_t *df_u = NULL;
    expr_t *d2f_u = NULL;
    expr_t *tmp = NULL;

    if (!c0 || !scalar_f || !lambda)
        return -1;

    *c0 = NULL;
    if (c1)
        *c1 = NULL;
    if (c2)
        *c2 = NULL;

    {
        number_t lambda_value = expr_eval(lambda);

        u = expr_new_named_var(lambda_value, "u");
        num_destroy(&lambda_value);
    }
    if (!u)
        return -1;

    scalar_f(&f_u, &u);
    if (!f_u) {
        expr_free(u);
        return -1;
    }

    *c0 = expr_substitute(f_u, u, lambda);
    if (!*c0) {
        expr_free(f_u);
        expr_free(u);
        return -1;
    }

    if (c1) {
        df_u = expr_create_deriv(f_u, u);
        *c1 = expr_substitute(df_u, u, lambda);
        if (!*c1) {
            expr_free(df_u);
            expr_free(f_u);
            expr_free(u);
            expr_free(*c0);
            *c0 = NULL;
            return -1;
        }
    }

    if (c2) {
        d2f_u = expr_create_2nd_deriv(f_u, u, u);
        tmp = expr_substitute(d2f_u, u, lambda);
        if (!tmp) {
            expr_free(d2f_u);
            expr_free(df_u);
            expr_free(f_u);
            expr_free(u);
            expr_free(*c0);
            if (c1) {
                expr_free(*c1);
                *c1 = NULL;
            }
            *c0 = NULL;
            return -1;
        }
        *c2 = expr_mul_num(tmp, &NUM_HALF);
        expr_free(tmp);
        if (!*c2) {
            expr_free(d2f_u);
            expr_free(df_u);
            expr_free(f_u);
            expr_free(u);
            expr_free(*c0);
            if (c1) {
                expr_free(*c1);
                *c1 = NULL;
            }
            *c0 = NULL;
            return -1;
        }
    }

    expr_free(d2f_u);
    expr_free(df_u);
    expr_free(f_u);
    expr_free(u);
    return 0;
}

static matrix_t *mat_fun_triangular_equal_diag_expr(const matrix_t *T,
                                                    void (*scalar_f)(void *out, const void *in))
{
    size_t n = T->rows;
    matrix_t *F = mat_create_upper_triangular_with_elem(n, n, &expr_elem);
    matrix_t *N = mat_create_upper_triangular_with_elem(n, n, &expr_elem);
    expr_t *lambda = NULL;
    expr_t *c0 = NULL;
    expr_t *c1 = NULL;
    expr_t *c2 = NULL;

    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    mat_get(T, 0, 0, &lambda);
    if (expr_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, lambda) != 0) {
        mat_free(F);
        mat_free(N);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mat_set(F, i, i, &c0);
        for (size_t j = i; j < n; ++j) {
            expr_t *tij = NULL;
            const expr_t *zero = EXPR_ZERO;
            mat_get(T, i, j, &tij);
            if (i == j)
                mat_set(N, i, j, &zero);
            else
                mat_set(N, i, j, &tij);
        }
    }

    if (n >= 2) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                expr_t *nij = NULL;
                expr_t *term = NULL;
                mat_get_owned(N, i, j, &nij);
                term = expr_mul(c1, nij);
                expr_free(nij);
                if (!term) {
                    expr_free(c0);
                    expr_free(c1);
                    expr_free(c2);
                    mat_free(F);
                    mat_free(N);
                    return NULL;
                }
                mat_set(F, i, j, &term);
                expr_free(term);
            }
        }
    }

    if (n >= 3) {
        matrix_t *N2 = mat_mul(N, N);
        if (!N2) {
            expr_free(c0);
            expr_free(c1);
            expr_free(c2);
            mat_free(F);
            mat_free(N);
            return NULL;
        }

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 2; j < n; ++j) {
                expr_t *fij = NULL;
                expr_t *n2ij = NULL;
                expr_t *extra = NULL;
                expr_t *sum = NULL;
                mat_get_owned(F, i, j, &fij);
                mat_get_owned(N2, i, j, &n2ij);
                extra = expr_mul(c2, n2ij);
                expr_free(n2ij);
                sum = extra ? expr_add(fij, extra) : NULL;
                expr_free(fij);
                expr_free(extra);
                if (!sum) {
                    mat_free(N2);
                    expr_free(c0);
                    expr_free(c1);
                    expr_free(c2);
                    mat_free(F);
                    mat_free(N);
                    return NULL;
                }
                mat_set(F, i, j, &sum);
                expr_free(sum);
            }
        }

        mat_free(N2);
    }

    expr_free(c0);
    expr_free(c1);
    expr_free(c2);
    mat_free(N);
    return F;
}

static matrix_t *mat_sinh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *negA = NULL, *Epos = NULL, *Eneg = NULL, *diff = NULL, *R = NULL;
    number_t minus_one = num_create_from_long(-1);
    number_t half = num_create_from_string("1/2");

    negA = mat_scalar_mul((matrix_t *)A, &minus_one);
    Epos = mat_exp(A);
    Eneg = mat_exp(negA);
    diff = mat_sub(Epos, Eneg);
    if (diff)
        R = mat_scalar_mul(diff, &half);

    num_destroy(&minus_one);
    num_destroy(&half);
    mat_free(negA);
    mat_free(Epos);
    mat_free(Eneg);
    mat_free(diff);
    return R;
}

static matrix_t *mat_cosh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *negA = NULL, *Epos = NULL, *Eneg = NULL, *sum = NULL, *R = NULL;
    number_t minus_one = num_create_from_long(-1);
    number_t half = num_create_from_string("1/2");

    negA = mat_scalar_mul((matrix_t *)A, &minus_one);
    Epos = mat_exp(A);
    Eneg = mat_exp(negA);
    sum = mat_add(Epos, Eneg);
    if (sum)
        R = mat_scalar_mul(sum, &half);

    num_destroy(&minus_one);
    num_destroy(&half);
    mat_free(negA);
    mat_free(Epos);
    mat_free(Eneg);
    mat_free(sum);
    return R;
}

static matrix_t *mat_sin_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *iA = NULL, *neg_iA = NULL, *Epos = NULL, *Eneg = NULL, *diff = NULL, *R = NULL;
    number_t i_unit = num_create_from_string("i");
    number_t neg_i_unit = num_neg(i_unit);
    number_t two = num_create_from_long(2);
    number_t two_i = num_mul(two, i_unit);
    number_t inv_two_i = num_inv(two_i);

    iA = mat_scalar_mul((matrix_t *)A, &i_unit);
    neg_iA = mat_scalar_mul((matrix_t *)A, &neg_i_unit);
    Epos = mat_exp(iA);
    Eneg = mat_exp(neg_iA);
    diff = mat_sub(Epos, Eneg);
    if (diff)
        R = mat_scalar_mul(diff, &inv_two_i);

    num_destroy(&i_unit);
    num_destroy(&neg_i_unit);
    num_destroy(&two);
    num_destroy(&two_i);
    num_destroy(&inv_two_i);
    mat_free(iA);
    mat_free(neg_iA);
    mat_free(Epos);
    mat_free(Eneg);
    mat_free(diff);
    return R;
}

static matrix_t *mat_cos_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *iA = NULL, *neg_iA = NULL, *Epos = NULL, *Eneg = NULL, *sum = NULL, *R = NULL;
    number_t i_unit = num_create_from_string("i");
    number_t neg_i_unit = num_neg(i_unit);
    number_t half = num_create_from_string("1/2");

    iA = mat_scalar_mul((matrix_t *)A, &i_unit);
    neg_iA = mat_scalar_mul((matrix_t *)A, &neg_i_unit);
    Epos = mat_exp(iA);
    Eneg = mat_exp(neg_iA);
    sum = mat_add(Epos, Eneg);
    if (sum)
        R = mat_scalar_mul(sum, &half);

    num_destroy(&i_unit);
    num_destroy(&neg_i_unit);
    num_destroy(&half);
    mat_free(iA);
    mat_free(neg_iA);
    mat_free(Epos);
    mat_free(Eneg);
    mat_free(sum);
    return R;
}

static matrix_t *mat_tan_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *S = mat_sin_number_triangular_equal_diag(A);
    matrix_t *C = mat_cos_number_triangular_equal_diag(A);
    matrix_t *Cinv = NULL;
    matrix_t *R = NULL;

    if (C)
        Cinv = mat_inverse(C);
    if (S && Cinv)
        R = mat_mul(S, Cinv);

    mat_free(S);
    mat_free(C);
    mat_free(Cinv);
    return R;
}

static matrix_t *mat_tanh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *S = mat_sinh_number_triangular_equal_diag(A);
    matrix_t *C = mat_cosh_number_triangular_equal_diag(A);
    matrix_t *Cinv = NULL;
    matrix_t *R = NULL;

    if (C)
        Cinv = mat_inverse(C);
    if (S && Cinv)
        R = mat_mul(S, Cinv);

    mat_free(S);
    mat_free(C);
    mat_free(Cinv);
    return R;
}

static matrix_t *mat_asinh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *AA = NULL, *I = NULL, *sum = NULL, *root = NULL, *inner = NULL, *R = NULL;

    AA = mat_mul(A, A);
    I = mat_create_identity(A ? A->rows : 0u);
    if (AA && I)
        sum = mat_add(AA, I);
    if (sum)
        root = mat_sqrt(sum);
    if (root)
        inner = mat_add(A, root);
    if (inner)
        R = mat_log(inner);

    mat_free(AA);
    mat_free(I);
    mat_free(sum);
    mat_free(root);
    mat_free(inner);
    return R;
}

static matrix_t *mat_asin_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_asin);
}

static matrix_t *mat_acos_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_acos);
}

static matrix_t *mat_acosh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *I = NULL, *AmI = NULL, *ApI = NULL, *root_m = NULL, *root_p = NULL;
    matrix_t *prod = NULL, *inner = NULL, *R = NULL;

    I = mat_create_identity(A ? A->rows : 0u);
    if (I) {
        AmI = mat_sub(A, I);
        ApI = mat_add(A, I);
    }
    if (AmI)
        root_m = mat_sqrt(AmI);
    if (ApI)
        root_p = mat_sqrt(ApI);
    if (root_m && root_p)
        prod = mat_mul(root_m, root_p);
    if (prod)
        inner = mat_add(A, prod);
    if (inner)
        R = mat_log(inner);

    mat_free(I);
    mat_free(AmI);
    mat_free(ApI);
    mat_free(root_m);
    mat_free(root_p);
    mat_free(prod);
    mat_free(inner);
    return R;
}

static matrix_t *mat_atan_number_triangular_equal_diag_upper(const matrix_t *A)
{
    size_t n;
    matrix_t *F = NULL;
    matrix_t *N = NULL;
    matrix_t *Npower = NULL;
    number_t lambda;
    number_t diag_value;
    number_t i_unit;
    number_t neg_i_unit;
    number_t two;
    number_t two_i;
    number_t inv_two_i;
    number_t one_plus_i_lambda;
    number_t one_minus_i_lambda;
    number_t p_plus;
    number_t p_minus;
    number_t p_plus_power;
    number_t p_minus_power;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_is_upper_triangular(A))
        return NULL;

    n = A->rows;
    if (n == 0u)
        return mat_copy_preserving_store(A);

    lambda = mat_get_num(A, 0, 0);
    diag_value = num_atan(lambda);
    i_unit = num_create_from_string("i");
    neg_i_unit = num_neg(i_unit);
    two = num_create_from_long(2);
    two_i = num_mul(two, i_unit);
    inv_two_i = num_inv(two_i);
    {
        number_t i_lambda = num_mul(i_unit, lambda);
        number_t neg_i_lambda = num_mul(neg_i_unit, lambda);

        one_plus_i_lambda = num_add(NUM_ONE, i_lambda);
        one_minus_i_lambda = num_add(NUM_ONE, neg_i_lambda);
        num_destroy(&neg_i_lambda);
        num_destroy(&i_lambda);
    }
    p_plus = num_div(i_unit, one_plus_i_lambda);
    p_minus = num_div(neg_i_unit, one_minus_i_lambda);
    p_plus_power = num_clone(p_plus);
    p_minus_power = num_clone(p_minus);

    F = mat_create_upper_triangular_with_elem(n, n, &number_elem);
    N = mat_number_strict_triangular_copy(A, true);
    if (!F || !N) {
        mat_free(F);
        mat_free(N);
        num_destroy(&p_minus_power);
        num_destroy(&p_plus_power);
        num_destroy(&p_minus);
        num_destroy(&p_plus);
        num_destroy(&one_minus_i_lambda);
        num_destroy(&one_plus_i_lambda);
        num_destroy(&inv_two_i);
        num_destroy(&two_i);
        num_destroy(&two);
        num_destroy(&neg_i_unit);
        num_destroy(&i_unit);
        num_destroy(&diag_value);
        num_destroy(&lambda);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mat_set_num_clone(F, i, i, &diag_value);
    }

    Npower = N;
    for (size_t k = 1; k < n; ++k) {
        number_t diff = num_sub(p_plus_power, p_minus_power);
        number_t coeff = num_mul(inv_two_i, diff);

        num_destroy(&diff);
        if ((k % 2u) == 0u) {
            number_t neg = num_neg(coeff);
            num_destroy(&coeff);
            coeff = neg;
        }
        if (k > 1u) {
            number_t k_num = num_create_from_long((long)k);
            number_t scaled = num_div(coeff, k_num);
            num_destroy(&k_num);
            num_destroy(&coeff);
            coeff = scaled;
        }

        if (mat_number_add_scaled_triangular(F, Npower, coeff, true) != 0) {
            num_destroy(&coeff);
            mat_free_power_seed_pair(Npower, N);
            mat_free(F);
            num_destroy(&p_minus_power);
            num_destroy(&p_plus_power);
            num_destroy(&p_minus);
            num_destroy(&p_plus);
            num_destroy(&one_minus_i_lambda);
            num_destroy(&one_plus_i_lambda);
            num_destroy(&inv_two_i);
            num_destroy(&two_i);
            num_destroy(&two);
            num_destroy(&neg_i_unit);
            num_destroy(&i_unit);
            num_destroy(&diag_value);
            num_destroy(&lambda);
            return NULL;
        }
        num_destroy(&coeff);

        if (k + 1u < n) {
            matrix_t *next = mat_number_triangular_mul(Npower, N, true);
            number_t next_plus = num_mul(p_plus_power, p_plus);
            number_t next_minus = num_mul(p_minus_power, p_minus);

            num_destroy(&p_plus_power);
            num_destroy(&p_minus_power);
            p_plus_power = next_plus;
            p_minus_power = next_minus;
            mat_free_if_distinct(Npower, N);
            Npower = next;
            if (!Npower) {
                mat_free(F);
                mat_free(N);
                num_destroy(&p_minus_power);
                num_destroy(&p_plus_power);
                num_destroy(&p_minus);
                num_destroy(&p_plus);
                num_destroy(&one_minus_i_lambda);
                num_destroy(&one_plus_i_lambda);
                num_destroy(&inv_two_i);
                num_destroy(&two_i);
                num_destroy(&two);
                num_destroy(&neg_i_unit);
                num_destroy(&i_unit);
                num_destroy(&diag_value);
                num_destroy(&lambda);
                return NULL;
            }
        }
    }

    mat_free_power_seed_pair(Npower, N);
    num_destroy(&p_minus_power);
    num_destroy(&p_plus_power);
    num_destroy(&p_minus);
    num_destroy(&p_plus);
    num_destroy(&one_minus_i_lambda);
    num_destroy(&one_plus_i_lambda);
    num_destroy(&inv_two_i);
    num_destroy(&two_i);
    num_destroy(&two);
    num_destroy(&neg_i_unit);
    num_destroy(&i_unit);
    num_destroy(&diag_value);
    num_destroy(&lambda);
    return F;
}

static matrix_t *mat_atan_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *T = NULL;
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!A || A->rows != A->cols || A->elem != &number_elem || !mat_number_diagonal_equal_local(A))
        return NULL;

    if (mat_is_upper_triangular(A))
        return mat_atan_number_triangular_equal_diag_upper(A);
    if (!mat_is_lower_triangular(A))
        return NULL;

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_atan_number_triangular_equal_diag_upper(T);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

static matrix_t *mat_atanh_number_triangular_equal_diag(const matrix_t *A)
{
    matrix_t *I = NULL, *plus = NULL, *minus = NULL, *Lp = NULL, *Lm = NULL, *diff = NULL, *R = NULL;
    number_t half = num_create_from_string("1/2");

    I = mat_create_identity(A ? A->rows : 0u);
    if (I) {
        plus = mat_add(I, A);
        minus = mat_sub(I, A);
    }
    if (plus)
        Lp = mat_log(plus);
    if (minus)
        Lm = mat_log(minus);
    if (Lp && Lm)
        diff = mat_sub(Lp, Lm);
    if (diff)
        R = mat_scalar_mul(diff, &half);

    num_destroy(&half);
    mat_free(I);
    mat_free(plus);
    mat_free(minus);
    mat_free(Lp);
    mat_free(Lm);
    mat_free(diff);
    return R;
}

static matrix_t *mat_erf_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_erf);
}

static matrix_t *mat_erfc_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_erfc);
}

static matrix_t *mat_erfinv_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_erfinv);
}

static matrix_t *mat_erfcinv_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_erfcinv);
}

static matrix_t *mat_gamma_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_gamma);
}

static matrix_t *mat_lgamma_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_lgamma);
}

static matrix_t *mat_digamma_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_digamma);
}

static matrix_t *mat_trigamma_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_trigamma);
}

static matrix_t *mat_gammainv_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_gammainv);
}

static matrix_t *mat_normal_pdf_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_normal_pdf);
}

static matrix_t *mat_normal_cdf_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_normal_cdf);
}

static matrix_t *mat_normal_logpdf_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_normal_logpdf);
}

static matrix_t *mat_lambert_w0_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_lambert_w0);
}

static matrix_t *mat_lambert_wm1_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_lambert_wm1);
}

static matrix_t *mat_productlog_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_lambert_w0);
}

static matrix_t *mat_ei_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_ei);
}

static matrix_t *mat_e1_number_triangular_equal_diag(const matrix_t *A)
{
    return mat_number_unary_taylor_from_expr(A, expr_e1);
}

static matrix_t *mat_fun_expr_diagonalizable_2x2(const matrix_t *A,
                                                 void (*scalar_f)(void *out, const void *in))
{
    expr_t *eigenvalues[2] = {NULL, NULL};
    expr_t *mapped[2] = {NULL, NULL};
    matrix_t *V = NULL;
    matrix_t *FD = NULL;
    matrix_t *VF = NULL;
    matrix_t *Vinv = NULL;
    matrix_t *R = NULL;

    if (!A || !scalar_f || A->rows != 2 || A->cols != 2 || !matrix_is_symbolic(A))
        return NULL;

    if (mat_eigendecompose_expr(A, eigenvalues, &V) != 0 || !V)
        goto fail;

    FD = mat_create_diagonal_with_elem(2, &expr_elem);
    if (!FD)
        goto fail;

    for (size_t i = 0; i < 2; ++i) {
        scalar_f(&mapped[i], &eigenvalues[i]);
        if (!mapped[i])
            goto fail;
        mat_set(FD, i, i, &mapped[i]);
    }

    VF = mat_mul(V, FD);
    if (!VF)
        goto fail;

    Vinv = mat_inverse(V);
    if (!Vinv)
        goto fail;

    R = mat_mul(VF, Vinv);

fail:
    for (size_t i = 0; i < 2; ++i) {
        expr_free(eigenvalues[i]);
        expr_free(mapped[i]);
    }
    mat_free(V);
    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);
    return R;
}

static bool mat_expr_block_is_exact_zero(const matrix_t *A,
                                         size_t row0, size_t rows,
                                         size_t col0, size_t cols)
{
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            expr_t *entry = NULL;

            mat_get(A, row0 + i, col0 + j, &entry);
            if (!expr_is_zero_local(entry))
                return false;
        }
    }

    return true;
}

static matrix_t *mat_expr_permute_principal_local(const matrix_t *A,
                                                  const size_t *perm)
{
    matrix_t *P;

    if (!A || !perm || A->rows != A->cols)
        return NULL;

    P = mat_create_dense_with_elem(A->rows, A->cols, &expr_elem);
    if (!P)
        return NULL;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *entry = NULL;

            mat_get(A, perm[i], perm[j], &entry);
            mat_set(P, i, j, &entry);
        }
    }

    return P;
}

static matrix_t *mat_expr_extract_block_local(const matrix_t *A,
                                              size_t row0, size_t rows,
                                              size_t col0, size_t cols)
{
    matrix_t *B;

    if (!A || row0 + rows > A->rows || col0 + cols > A->cols)
        return NULL;

    B = mat_create_dense_with_elem(rows, cols, &expr_elem);
    if (!B)
        return NULL;

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            expr_t *entry = NULL;

            mat_get(A, row0 + i, col0 + j, &entry);
            mat_set(B, i, j, &entry);
        }
    }

    return B;
}

static bool mat_expr_insert_block_local(matrix_t *A, size_t row0, size_t col0,
                                        const matrix_t *B)
{
    if (!A || !B || row0 + B->rows > A->rows || col0 + B->cols > A->cols)
        return false;

    for (size_t i = 0; i < B->rows; ++i) {
        for (size_t j = 0; j < B->cols; ++j) {
            expr_t *entry = NULL;

            mat_get(B, i, j, &entry);
            mat_set(A, row0 + i, col0 + j, &entry);
        }
    }

    return true;
}

static int mat_expr_component_partition(const matrix_t *A,
                                        size_t **perm_out,
                                        size_t *count_out)
{
    size_t n;
    size_t *perm = NULL;
    bool *seen = NULL;
    size_t perm_len = 0;
    size_t comp_count = 0;

    if (!A || !perm_out || !count_out || A->rows != A->cols)
        return -1;

    *perm_out = NULL;
    *count_out = 0;

    n = A->rows;
    if (n == 0)
        return 0;

    perm = malloc(n * sizeof(*perm));
    seen = calloc(n, sizeof(*seen));
    if (!perm || !seen)
        goto fail;

    for (size_t root = 0; root < n; ++root) {
        size_t *queue = NULL;
        size_t qh = 0;
        size_t qt = 0;

        if (seen[root])
            continue;

        queue = malloc(n * sizeof(*queue));
        if (!queue)
            goto fail;

        queue[qt++] = root;
        seen[root] = true;

        while (qh < qt) {
            size_t u = queue[qh++];

            perm[perm_len++] = u;
            for (size_t v = 0; v < n; ++v) {
                expr_t *uv = NULL;
                expr_t *vu = NULL;
                bool connected;

                if (seen[v])
                    continue;

                mat_get(A, u, v, &uv);
                mat_get(A, v, u, &vu);
                connected = !expr_is_zero_local(uv) || !expr_is_zero_local(vu);
                if (!connected)
                    continue;

                seen[v] = true;
                queue[qt++] = v;
            }
        }

        free(queue);
        comp_count++;
    }

    *perm_out = perm;
    *count_out = comp_count;
    free(seen);
    return 0;

fail:
    free(perm);
    free(seen);
    return -1;
}

static matrix_t *mat_fun_expr_block_diagonal(const matrix_t *A,
                                             void (*scalar_f)(void *out, const void *in))
{
    size_t n;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    n = A->rows;
    for (size_t split = 1; split < n; ++split) {
        matrix_t *A11 = NULL;
        matrix_t *A22 = NULL;
        matrix_t *F11 = NULL;
        matrix_t *F22 = NULL;
        matrix_t *F = NULL;

        if (!mat_expr_block_is_exact_zero(A, 0, split, split, n - split) ||
            !mat_expr_block_is_exact_zero(A, split, n - split, 0, split))
            continue;

        A11 = mat_expr_extract_block_local(A, 0, split, 0, split);
        A22 = mat_expr_extract_block_local(A, split, n - split, split, n - split);
        if (!A11 || !A22)
            goto next_split;

        F11 = mat_fun_expr_structured(A11, scalar_f);
        F22 = mat_fun_expr_structured(A22, scalar_f);
        if (!F11 || !F22)
            goto next_split;

        F = mat_create_dense_with_elem(n, n, &expr_elem);
        if (!F)
            goto next_split;

        if (!mat_expr_insert_block_local(F, 0, 0, F11) ||
            !mat_expr_insert_block_local(F, split, split, F22)) {
            mat_free(F);
            F = NULL;
            goto next_split;
        }

        mat_free(A11);
        mat_free(A22);
        mat_free(F11);
        mat_free(F22);
        return F;

next_split:
        mat_free(A11);
        mat_free(A22);
        mat_free(F11);
        mat_free(F22);
    }

    return NULL;
}

static matrix_t *mat_fun_expr_permuted_block_diagonal(const matrix_t *A,
                                                      void (*scalar_f)(void *out, const void *in))
{
    size_t *perm = NULL;
    size_t comp_count = 0;
    matrix_t *P = NULL;
    matrix_t *FP = NULL;
    matrix_t *F = NULL;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    if (mat_expr_component_partition(A, &perm, &comp_count) != 0)
        goto fail;
    if (comp_count <= 1)
        goto fail;

    P = mat_expr_permute_principal_local(A, perm);
    if (!P)
        goto fail;

    FP = mat_fun_expr_block_diagonal(P, scalar_f);
    if (!FP)
        goto fail;

    F = mat_create_dense_with_elem(A->rows, A->cols, &expr_elem);
    if (!F)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *entry = NULL;

            mat_get(FP, i, j, &entry);
            mat_set(F, perm[i], perm[j], &entry);
        }
    }

fail:
    free(perm);
    mat_free(P);
    mat_free(FP);
    return F;
}

static matrix_t *mat_fun_expr_uniform_diag_offdiag(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in))
{
    matrix_t *out = NULL;
    expr_t *diag = NULL;
    expr_t *offdiag = NULL;
    expr_t *alpha = NULL;
    expr_t *beta = NULL;
    expr_t *fa = NULL;
    expr_t *fb = NULL;
    expr_t *delta = NULL;
    size_t n;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows < 2)
        return NULL;

    n = A->rows;
    mat_get(A, 0, 0, &diag);
    mat_get(A, 0, 1, &offdiag);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            expr_t *entry = NULL;
            bool same;

            mat_get(A, i, j, &entry);
            same = (i == j)
                ? expr_equal_exact_local(diag, entry)
                : expr_equal_exact_local(offdiag, entry);
            if (!same)
                goto fail;
        }
    }

    expr_retain(diag ? diag : EXPR_ZERO);
    expr_retain(offdiag ? offdiag : EXPR_ZERO);
    alpha = expr_sub_simplify_owned(diag ? diag : EXPR_ZERO, offdiag ? offdiag : EXPR_ZERO);
    if (!alpha)
        goto fail;

    expr_retain(diag ? diag : EXPR_ZERO);
    expr_retain(offdiag ? offdiag : EXPR_ZERO);
    number_t n_minus_one = num_create_from_long((long)(n - 1));
    expr_t *scaled_offdiag = expr_mul_simplify_owned(
        expr_new_const(n_minus_one),
        offdiag ? offdiag : EXPR_ZERO);
    num_destroy(&n_minus_one);
    if (!scaled_offdiag)
        goto fail;
    beta = expr_add_simplify_owned(diag ? diag : EXPR_ZERO, scaled_offdiag);
    scaled_offdiag = NULL;
    if (!beta)
        goto fail;

    scalar_f(&fa, &alpha);
    scalar_f(&fb, &beta);
    if (!fa || !fb)
        goto fail;

    expr_retain(fb);
    expr_retain(fa);
    delta = expr_sub_simplify_owned(fb, fa);
    if (!delta)
        goto fail;
    number_t n_num = num_create_from_long((long)n);
    expr_t *scaled_delta = expr_div_num(delta, &n_num);
    num_destroy(&n_num);
    expr_free(delta);
    delta = expr_simplify_owned(scaled_delta);
    if (!delta)
        goto fail;

    out = mat_new_expr(n, n);
    if (!out)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            expr_t *entry = NULL;

            if (i == j) {
                expr_retain(fa);
                expr_retain(delta);
                entry = expr_add_simplify_owned(fa, delta);
            } else {
                expr_retain(delta);
                entry = expr_simplify_owned(delta);
            }

            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            expr_free(entry);
        }
    }

    expr_free(alpha);
    expr_free(beta);
    expr_free(fa);
    expr_free(fb);
    expr_free(delta);
    return out;

fail:
    mat_free(out);
    expr_free(alpha);
    expr_free(beta);
    expr_free(fa);
    expr_free(fb);
    expr_free(delta);
    return NULL;
}

static matrix_t *mat_fun_expr_scalar_plus_rank_one(const matrix_t *A,
                                                   void (*scalar_f)(void *out, const void *in))
{
    matrix_t *out = NULL;
    expr_t **u = NULL;
    expr_t **v = NULL;
    expr_t *alpha = NULL;
    expr_t *lambda = NULL;
    expr_t *f_alpha = NULL;
    expr_t *f_beta = NULL;
    expr_t *coeff = NULL;
    size_t n;
    size_t p = 0;
    size_t q = 0;
    bool found_pivot = false;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows < 3)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    n = A->rows;
    u = calloc(n, sizeof(*u));
    v = calloc(n, sizeof(*v));
    if (!u || !v)
        goto fail;

    for (size_t i = 0; i < n && !found_pivot; ++i) {
        for (size_t j = 0; j < n; ++j) {
            expr_t *entry = NULL;

            if (i == j)
                continue;
            mat_get(A, i, j, &entry);
            if (!expr_is_zero_local(entry)) {
                p = i;
                q = j;
                found_pivot = true;
                break;
            }
        }
    }
    if (!found_pivot)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        expr_t *entry = NULL;

        if (i == q)
            continue;
        mat_get(A, i, q, &entry);
        if (!expr_is_zero_local(entry)) {
            expr_retain(entry);
            u[i] = entry;
        }
    }

    v[q] = expr_const_one();
    if (!v[q])
        goto fail;

    for (size_t j = 0; j < n; ++j) {
        expr_t *apj = NULL;
        expr_t *apq = NULL;

        if (j == p || j == q)
            continue;
        mat_get(A, p, j, &apj);
        if (expr_is_zero_local(apj))
            continue;
        mat_get(A, p, q, &apq);
        if (expr_is_zero_local(apq))
            goto fail;

        expr_retain(apj);
        expr_retain(apq);
        v[j] = expr_div_simplify_owned(apj, apq);
        if (!v[j])
            goto fail;
    }

    for (size_t i = 0; i < n; ++i) {
        expr_t *aii = NULL;
        expr_t *diag_corr = NULL;
        expr_t *cand = NULL;
        bool usable = (i != p && i != q);

        if (!usable)
            continue;

        diag_corr = expr_mul_or_zero_owned_local(u[i], v[i]);
        if (!diag_corr)
            goto fail;

        mat_get(A, i, i, &aii);
        expr_retain(aii ? aii : EXPR_ZERO);
        cand = expr_sub_simplify_owned(aii ? aii : EXPR_ZERO, diag_corr);
        if (!cand)
            goto fail;

        if (!alpha) {
            alpha = cand;
        } else {
            bool same = expr_equal_exact_local(alpha, cand);
            expr_free(cand);
            if (!same)
                goto fail;
        }
    }
    if (!alpha)
        goto fail;

    expr_t *app = NULL;
    expr_t *diag_p = NULL;
    expr_t *aqq = NULL;
    expr_t *diag_q = NULL;
    expr_t *apq = NULL;

    mat_get(A, p, p, &app);
    expr_retain(app ? app : EXPR_ZERO);
    expr_retain(alpha);
    diag_p = expr_sub_simplify_owned(app ? app : EXPR_ZERO, alpha);
    if (!diag_p)
        goto fail;

    mat_get(A, p, q, &apq);
    if (expr_is_zero_local(apq))
        goto fail;
    expr_retain(diag_p);
    expr_retain(apq);
    v[p] = expr_div_simplify_owned(diag_p, apq);
    expr_free(diag_p);
    if (!v[p])
        goto fail;

    mat_get(A, q, q, &aqq);
    expr_retain(aqq ? aqq : EXPR_ZERO);
    expr_retain(alpha);
    diag_q = expr_sub_simplify_owned(aqq ? aqq : EXPR_ZERO, alpha);
    if (!diag_q)
        goto fail;
    u[q] = diag_q;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            expr_t *aij = NULL;
            expr_t *expected = NULL;

            mat_get(A, i, j, &aij);
            if (i == j) {
                expr_t *diag_corr = expr_mul_or_zero_owned_local(u[i], v[i]);
                if (!diag_corr)
                    goto fail;
                expr_retain(alpha);
                expected = expr_add_simplify_owned(alpha, diag_corr);
            } else {
                expected = expr_mul_or_zero_owned_local(u[i], v[j]);
            }

            if (!expected)
                goto fail;
            if (!expr_equal_exact_local(aij, expected)) {
                expr_free(expected);
                goto fail;
            }
            expr_free(expected);
        }
    }

    lambda = expr_const_zero();
    if (!lambda)
        goto fail;
    for (size_t i = 0; i < n; ++i) {
        expr_t *term = expr_mul_or_zero_owned_local(u[i], v[i]);
        expr_t *next;

        if (!term)
            goto fail;
        next = expr_add_simplify_owned(lambda, term);
        if (!next)
            goto fail;
        lambda = next;
    }

    if (expr_is_exact_zero(lambda)) {
        if (expr_fun_coeffs_up_to_second(&f_alpha, &coeff, NULL, scalar_f, alpha) != 0)
            goto fail;
    } else {
        expr_t *beta = NULL;
        expr_t *num = NULL;

        expr_retain(alpha);
        expr_retain(lambda);
        beta = expr_add_simplify_owned(alpha, lambda);
        if (!beta)
            goto fail;

        scalar_f(&f_alpha, &alpha);
        scalar_f(&f_beta, &beta);
        expr_free(beta);
        if (!f_alpha || !f_beta)
            goto fail;

        expr_retain(f_beta);
        expr_retain(f_alpha);
        num = expr_sub_simplify_owned(f_beta, f_alpha);
        if (!num)
            goto fail;
        expr_retain(lambda);
        coeff = expr_div_simplify_owned(num, lambda);
        if (!coeff)
            goto fail;
    }

    out = mat_new_expr(n, n);
    if (!out)
        goto fail;

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            expr_t *aij = NULL;
            expr_t *entry = NULL;

            if (i == j) {
                expr_t *diag_delta = NULL;

                mat_get(A, i, i, &aij);
                expr_retain(aij ? aij : EXPR_ZERO);
                expr_retain(alpha);
                diag_delta = expr_sub_simplify_owned(aij ? aij : EXPR_ZERO, alpha);
                if (!diag_delta)
                    goto fail;

                if (expr_is_zero_local(diag_delta)) {
                    expr_free(diag_delta);
                    entry = expr_clone_for_storage(f_alpha);
                } else {
                    expr_t *scaled = NULL;

                    expr_retain(coeff);
                    expr_retain(diag_delta);
                    scaled = expr_mul_simplify_owned(coeff, diag_delta);
                    expr_free(diag_delta);
                    if (!scaled)
                        goto fail;
                    expr_retain(f_alpha);
                    entry = expr_add_simplify_owned(f_alpha, scaled);
                }
            } else {
                mat_get(A, i, j, &aij);
                if (expr_is_zero_local(aij)) {
                    entry = expr_const_zero();
                } else {
                    expr_retain(coeff);
                    expr_retain(aij);
                    entry = expr_mul_simplify_owned(coeff, aij);
                }
            }

            if (!entry)
                goto fail;
            mat_set(out, i, j, &entry);
            expr_free(entry);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        expr_free(u[i]);
        expr_free(v[i]);
    }
    free(u);
    free(v);
    expr_free(alpha);
    expr_free(lambda);
    expr_free(f_alpha);
    expr_free(f_beta);
    expr_free(coeff);
    return out;

fail:
    mat_free(out);
    if (u) {
        for (size_t i = 0; i < n; ++i)
            expr_free(u[i]);
    }
    if (v) {
        for (size_t i = 0; i < n; ++i)
            expr_free(v[i]);
    }
    free(u);
    free(v);
    expr_free(alpha);
    expr_free(lambda);
    expr_free(f_alpha);
    expr_free(f_beta);
    expr_free(coeff);
    return NULL;
}

static matrix_t *mat_fun_expr_cubic_linear_exact(const matrix_t *A,
                                                 void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *A3 = NULL;
    matrix_t *out = NULL;
    expr_t *s = NULL;
    expr_t *root = NULL;
    expr_t *f0 = NULL;
    expr_t *fp = NULL;
    expr_t *fm = NULL;
    expr_t *c0 = NULL;
    expr_t *c1 = NULL;
    expr_t *c2 = NULL;
    bool saw_nonzero = false;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows == 0)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    A3 = A2 ? mat_mul(A2, A) : NULL;
    if (!A2 || !A3)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            expr_t *a3ij = NULL;

            mat_get(A, i, j, &aij);
            mat_get(A3, i, j, &a3ij);
            if (expr_is_zero_local(aij)) {
                if (!expr_is_zero_local(a3ij))
                    goto fail;
                continue;
            }

            saw_nonzero = true;
            expr_retain(a3ij);
            expr_retain(aij);
            expr_t *cand = expr_div_simplify_owned(a3ij, aij);
            if (!cand)
                goto fail;

            if (!s) {
                s = cand;
            } else {
                bool same = expr_equal_exact_local(s, cand);
                expr_free(cand);
                if (!same)
                    goto fail;
            }
        }
    }

    if (!saw_nonzero || !s)
        goto fail;

    if (expr_is_exact_zero(s)) {
        expr_t *zero = expr_const_zero();

        if (!zero)
            goto fail;
        if (expr_fun_coeffs_up_to_second(&c0, &c1, &c2, scalar_f, zero) != 0) {
            expr_free(zero);
            goto fail;
        }
        expr_free(zero);
    } else {
        expr_t *zero = expr_const_zero();
        expr_t *neg_root = NULL;
        expr_t *two_root = NULL;
        expr_t *two_s = NULL;
        expr_t *sum = NULL;
        expr_t *diff = NULL;
        expr_t *tmp = NULL;

        if (!zero)
            goto fail;

        root = expr_sqrt(s);
        root = expr_simplify_owned(root);
        if (!root) {
            expr_free(zero);
            goto fail;
        }

        expr_retain(root);
        neg_root = expr_sub_simplify_owned(expr_const_zero(), root);
        if (!neg_root) {
            expr_free(zero);
            goto fail;
        }

        scalar_f(&f0, &zero);
        scalar_f(&fp, &root);
        scalar_f(&fm, &neg_root);
        expr_free(zero);
        if (!f0 || !fp || !fm)
            goto fail;

        c0 = expr_simplify_owned(f0);
        f0 = NULL;
        if (!c0)
            goto fail;

        expr_retain(fp);
        expr_retain(fm);
        diff = expr_sub_simplify_owned(fp, fm);
        if (!diff)
            goto fail;

        expr_retain(root);
        two_root = expr_mul_simplify_owned(expr_const_long(2), root);
        if (!two_root)
            goto fail;
        c1 = expr_div_simplify_owned(diff, two_root);
        diff = NULL;
        two_root = NULL;
        if (!c1)
            goto fail;

        expr_retain(fp);
        expr_retain(fm);
        sum = expr_add_simplify_owned(fp, fm);
        if (!sum)
            goto fail;

        expr_retain(c0);
        tmp = expr_mul_simplify_owned(expr_const_long(2), c0);
        if (!tmp)
            goto fail;
        sum = expr_sub_simplify_owned(sum, tmp);
        tmp = NULL;
        if (!sum)
            goto fail;

        expr_retain(s);
        two_s = expr_mul_simplify_owned(expr_const_long(2), s);
        if (!two_s)
            goto fail;
        c2 = expr_div_simplify_owned(sum, two_s);
        sum = NULL;
        two_s = NULL;
        if (!c2)
            goto fail;

        expr_free(neg_root);
    }

    out = mat_new_expr(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            expr_t *a2ij = NULL;
            expr_t *term1 = NULL;
            expr_t *term2 = NULL;
            expr_t *entry = NULL;

            mat_get(A, i, j, &aij);
            mat_get(A2, i, j, &a2ij);

            expr_retain(c1);
            expr_retain(aij ? aij : EXPR_ZERO);
            term1 = expr_mul_simplify_owned(c1, aij ? aij : EXPR_ZERO);
            if (!term1)
                goto fail;

            expr_retain(c2);
            expr_retain(a2ij ? a2ij : EXPR_ZERO);
            term2 = expr_mul_simplify_owned(c2, a2ij ? a2ij : EXPR_ZERO);
            if (!term2)
                goto fail;

            entry = expr_add_simplify_owned(term1, term2);
            term1 = NULL;
            term2 = NULL;
            if (!entry)
                goto fail;

            if (i == j) {
                expr_retain(c0);
                entry = expr_add_simplify_owned(c0, entry);
                if (!entry)
                    goto fail;
            }

            mat_set(out, i, j, &entry);
            expr_free(entry);
        }
    }

    mat_free(A2);
    mat_free(A3);
    expr_free(s);
    expr_free(root);
    expr_free(f0);
    expr_free(fp);
    expr_free(fm);
    expr_free(c0);
    expr_free(c1);
    expr_free(c2);
    return out;

fail:
    mat_free(A2);
    mat_free(A3);
    mat_free(out);
    expr_free(s);
    expr_free(root);
    expr_free(f0);
    expr_free(fp);
    expr_free(fm);
    expr_free(c0);
    expr_free(c1);
    expr_free(c2);
    return NULL;
}

static matrix_t *mat_fun_expr_quartic_biquadratic_exact(const matrix_t *A,
                                                        void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *A3 = NULL;
    matrix_t *A4 = NULL;
    matrix_t *out = NULL;
    expr_t *s = NULL;
    expr_t *t = NULL;
    expr_t *disc = NULL;
    expr_t *root = NULL;
    expr_t *mu1 = NULL;
    expr_t *mu2 = NULL;
    expr_t *r1 = NULL;
    expr_t *r2 = NULL;
    expr_t *nr1 = NULL;
    expr_t *nr2 = NULL;
    expr_t *fp1 = NULL;
    expr_t *fm1 = NULL;
    expr_t *fp2 = NULL;
    expr_t *fm2 = NULL;
    expr_t *g1 = NULL;
    expr_t *g2 = NULL;
    expr_t *h1 = NULL;
    expr_t *h2 = NULL;
    expr_t *e0 = NULL;
    expr_t *e1 = NULL;
    expr_t *o0 = NULL;
    expr_t *o1 = NULL;
    expr_t *step = NULL;
    expr_t *step_sq = NULL;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows != 4)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    A3 = A2 ? mat_mul(A2, A) : NULL;
    A4 = A2 ? mat_mul(A2, A2) : NULL;
    if (!A2 || !A3 || !A4)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            bool should_match_step = (i + 1 == j) || (j + 1 == i);

            mat_get(A, i, j, &aij);
            if (i == j) {
                if (!expr_is_zero_local(aij))
                    goto fail;
                continue;
            }

            if (should_match_step) {
                if (expr_is_zero_local(aij))
                    goto fail;
                if (!step) {
                    expr_retain(aij);
                    step = aij;
                } else if (!expr_equal_exact_local(step, aij)) {
                    goto fail;
                }
            } else if (!expr_is_zero_local(aij)) {
                goto fail;
            }
        }
    }

    if (!step)
        goto fail;

    expr_retain(step);
    expr_retain(step);
    step_sq = expr_mul_simplify_owned(step, step);
    if (!step_sq)
        goto fail;
    expr_retain(step_sq);
    s = expr_mul_simplify_owned(expr_const_long(3), step_sq);
    if (!s)
        goto fail;

    expr_retain(step_sq);
    expr_retain(step_sq);
    expr_t *step_four = expr_mul_simplify_owned(step_sq, step_sq);
    if (!step_four)
        goto fail;
    t = expr_sub_simplify_owned(expr_const_zero(), step_four);
    if (!t)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *a2ij = NULL;
            expr_t *a4ij = NULL;
            expr_t *expected = NULL;
            expr_t *scaled = NULL;

            mat_get(A2, i, j, &a2ij);
            mat_get(A4, i, j, &a4ij);
            expr_retain(s);
            expr_retain(a2ij ? a2ij : EXPR_ZERO);
            scaled = expr_mul_simplify_owned(s, a2ij ? a2ij : EXPR_ZERO);
            if (!scaled)
                goto fail;

            if (i == j) {
                expr_retain(t);
                expected = expr_add_simplify_owned(scaled, t);
            } else {
                expected = scaled;
            }

            if (!expected)
                goto fail;
            if (!expr_equal_exact_local(a4ij, expected)) {
                expr_free(expected);
                goto fail;
            }
            expr_free(expected);
        }
    }

    expr_retain(s);
    expr_retain(s);
    disc = expr_mul_simplify_owned(s, s);
    if (!disc)
        goto fail;

    expr_retain(t);
    expr_t *four_t = expr_mul_simplify_owned(expr_const_long(4), t);
    if (!four_t)
        goto fail;
    disc = expr_add_simplify_owned(disc, four_t);
    if (!disc || expr_is_exact_zero(disc))
        goto fail;

    root = expr_sqrt(disc);
    root = expr_simplify_owned(root);
    if (!root)
        goto fail;

    expr_retain(s);
    expr_retain(root);
    expr_t *sum = expr_add_simplify_owned(s, root);
    if (!sum)
        goto fail;
    mu1 = expr_div_simplify_owned(sum, expr_const_long(2));
    sum = NULL;
    mu1 = expr_simplify_owned(mu1);
    if (!mu1)
        goto fail;

    expr_retain(s);
    expr_retain(root);
    expr_t *diff = expr_sub_simplify_owned(s, root);
    if (!diff)
        goto fail;
    mu2 = expr_div_simplify_owned(diff, expr_const_long(2));
    diff = NULL;
    mu2 = expr_simplify_owned(mu2);
    if (!mu2 || expr_equal_exact_local(mu1, mu2))
        goto fail;

    r1 = expr_sqrt(mu1);
    r1 = expr_simplify_owned(r1);
    r2 = expr_sqrt(mu2);
    r2 = expr_simplify_owned(r2);
    if (!r1 || !r2)
        goto fail;

    expr_retain(r1);
    nr1 = expr_sub_simplify_owned(expr_const_zero(), r1);
    expr_retain(r2);
    nr2 = expr_sub_simplify_owned(expr_const_zero(), r2);
    if (!nr1 || !nr2)
        goto fail;

    scalar_f(&fp1, &r1);
    scalar_f(&fm1, &nr1);
    scalar_f(&fp2, &r2);
    scalar_f(&fm2, &nr2);
    if (!fp1 || !fm1 || !fp2 || !fm2)
        goto fail;

    expr_retain(fp1);
    expr_retain(fm1);
    g1 = expr_add_simplify_owned(fp1, fm1);
    if (g1) {
        expr_t *half = expr_div_simplify_owned(g1, expr_const_long(2));
        g1 = NULL;
        g1 = half ? expr_simplify_owned(half) : NULL;
    }
    if (!g1)
        goto fail;

    expr_retain(fp2);
    expr_retain(fm2);
    g2 = expr_add_simplify_owned(fp2, fm2);
    if (g2) {
        expr_t *half = expr_div_simplify_owned(g2, expr_const_long(2));
        g2 = NULL;
        g2 = half ? expr_simplify_owned(half) : NULL;
    }
    if (!g2)
        goto fail;

    if (expr_is_exact_zero(mu1)) {
        h1 = expr_fun_first_derivative_at_zero_local(scalar_f);
    } else {
        expr_retain(fp1);
        expr_retain(fm1);
        expr_t *num = expr_sub_simplify_owned(fp1, fm1);
        expr_t *den = NULL;

        if (!num)
            goto fail;
        expr_retain(r1);
        den = expr_mul_simplify_owned(expr_const_long(2), r1);
        if (!den)
            goto fail;
        h1 = expr_div_simplify_owned(num, den);
    }
    if (!h1)
        goto fail;

    if (expr_is_exact_zero(mu2)) {
        h2 = expr_fun_first_derivative_at_zero_local(scalar_f);
    } else {
        expr_retain(fp2);
        expr_retain(fm2);
        expr_t *num = expr_sub_simplify_owned(fp2, fm2);
        expr_t *den = NULL;

        if (!num)
            goto fail;
        expr_retain(r2);
        den = expr_mul_simplify_owned(expr_const_long(2), r2);
        if (!den)
            goto fail;
        h2 = expr_div_simplify_owned(num, den);
    }
    if (!h2)
        goto fail;

    expr_t *num = NULL;
    expr_t *den = NULL;
    expr_t *e1mu;

    expr_retain(g1);
    expr_retain(g2);
    num = expr_sub_simplify_owned(g1, g2);
    expr_retain(mu1);
    expr_retain(mu2);
    den = expr_sub_simplify_owned(mu1, mu2);
    e1 = expr_div_simplify_owned(num, den);
    if (!e1)
        goto fail;

    expr_retain(e1);
    expr_retain(mu1);
    e1mu = expr_mul_simplify_owned(e1, mu1);
    if (!e1mu)
        goto fail;
    expr_retain(g1);
    e0 = expr_sub_simplify_owned(g1, e1mu);
    if (!e0)
        goto fail;

    expr_t *num2 = NULL;
    expr_t *den2 = NULL;
    expr_t *o1mu;

    expr_retain(h1);
    expr_retain(h2);
    num2 = expr_sub_simplify_owned(h1, h2);
    expr_retain(mu1);
    expr_retain(mu2);
    den2 = expr_sub_simplify_owned(mu1, mu2);
    o1 = expr_div_simplify_owned(num2, den2);
    if (!o1)
        goto fail;

    expr_retain(o1);
    expr_retain(mu1);
    o1mu = expr_mul_simplify_owned(o1, mu1);
    if (!o1mu)
        goto fail;
    expr_retain(h1);
    o0 = expr_sub_simplify_owned(h1, o1mu);
    if (!o0)
        goto fail;

    out = mat_new_expr(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            expr_t *a2ij = NULL;
            expr_t *a3ij = NULL;
            expr_t *entry = NULL;
            expr_t *term = NULL;

            if (i == j) {
                expr_retain(e0);
                entry = expr_simplify_owned(e0);
                if (!entry)
                    goto fail;
            } else {
                entry = expr_const_zero();
                if (!entry)
                    goto fail;
            }

            mat_get(A, i, j, &aij);
            expr_retain(o0);
            expr_retain(aij ? aij : EXPR_ZERO);
            term = expr_mul_simplify_owned(o0, aij ? aij : EXPR_ZERO);
            if (!term) {
                expr_free(entry);
                goto fail;
            }
            entry = expr_add_simplify_owned(entry, term);
            if (!entry)
                goto fail;

            mat_get(A2, i, j, &a2ij);
            expr_retain(e1);
            expr_retain(a2ij ? a2ij : EXPR_ZERO);
            term = expr_mul_simplify_owned(e1, a2ij ? a2ij : EXPR_ZERO);
            if (!term) {
                expr_free(entry);
                goto fail;
            }
            entry = expr_add_simplify_owned(entry, term);
            if (!entry)
                goto fail;

            mat_get(A3, i, j, &a3ij);
            expr_retain(o1);
            expr_retain(a3ij ? a3ij : EXPR_ZERO);
            term = expr_mul_simplify_owned(o1, a3ij ? a3ij : EXPR_ZERO);
            if (!term) {
                expr_free(entry);
                goto fail;
            }
            entry = expr_add_simplify_owned(entry, term);
            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            expr_free(entry);
        }
    }

    mat_free(A2);
    mat_free(A3);
    mat_free(A4);
    expr_free(s);
    expr_free(t);
    expr_free(step);
    expr_free(step_sq);
    expr_free(disc);
    expr_free(root);
    expr_free(mu1);
    expr_free(mu2);
    expr_free(r1);
    expr_free(r2);
    expr_free(nr1);
    expr_free(nr2);
    expr_free(fp1);
    expr_free(fm1);
    expr_free(fp2);
    expr_free(fm2);
    expr_free(g1);
    expr_free(g2);
    expr_free(h1);
    expr_free(h2);
    expr_free(e0);
    expr_free(e1);
    expr_free(o0);
    expr_free(o1);
    return out;

fail:
    mat_free(A2);
    mat_free(A3);
    mat_free(A4);
    mat_free(out);
    expr_free(s);
    expr_free(t);
    expr_free(step);
    expr_free(step_sq);
    expr_free(disc);
    expr_free(root);
    expr_free(mu1);
    expr_free(mu2);
    expr_free(r1);
    expr_free(r2);
    expr_free(nr1);
    expr_free(nr2);
    expr_free(fp1);
    expr_free(fm1);
    expr_free(fp2);
    expr_free(fm2);
    expr_free(g1);
    expr_free(g2);
    expr_free(h1);
    expr_free(h2);
    expr_free(e0);
    expr_free(e1);
    expr_free(o0);
    expr_free(o1);
    return NULL;
}

static matrix_t *mat_fun_expr_quadratic_exact(const matrix_t *A,
                                              void (*scalar_f)(void *out, const void *in))
{
    matrix_t *A2 = NULL;
    matrix_t *out = NULL;
    expr_t *p = NULL;
    expr_t *q = NULL;
    expr_t *disc = NULL;
    expr_t *root = NULL;
    expr_t *lambda1 = NULL;
    expr_t *lambda2 = NULL;
    expr_t *f1 = NULL;
    expr_t *f2 = NULL;
    expr_t *c0 = NULL;
    expr_t *c1 = NULL;
    bool saw_offdiag = false;

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols || A->rows == 0)
        return NULL;
    if (mat_is_upper_triangular(A) || mat_is_lower_triangular(A))
        return NULL;

    A2 = mat_mul(A, A);
    if (!A2)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            expr_t *a2ij = NULL;

            if (i == j)
                continue;

            mat_get(A, i, j, &aij);
            mat_get(A2, i, j, &a2ij);
            if (expr_is_zero_local(aij)) {
                if (!expr_is_zero_local(a2ij))
                    goto fail;
                continue;
            }

            saw_offdiag = true;
            expr_retain(a2ij);
            expr_retain(aij);
            expr_t *cand = expr_div_simplify_owned(a2ij, aij);
            if (!cand)
                goto fail;

            if (!p) {
                p = cand;
            } else {
                bool same = expr_equal_exact_local(p, cand);
                expr_free(cand);
                if (!same)
                    goto fail;
            }
        }
    }

    if (!saw_offdiag || !p)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        expr_t *aii = NULL;
        expr_t *a2ii = NULL;
        expr_t *p_aii = NULL;
        expr_t *cand = NULL;

        mat_get(A, i, i, &aii);
        mat_get(A2, i, i, &a2ii);
        expr_retain(p);
        expr_retain(aii ? aii : EXPR_ZERO);
        p_aii = expr_mul_simplify_owned(p, aii ? aii : EXPR_ZERO);
        if (!p_aii)
            goto fail;

        expr_retain(a2ii ? a2ii : EXPR_ZERO);
        cand = expr_sub_simplify_owned(a2ii ? a2ii : EXPR_ZERO, p_aii);
        if (!cand)
            goto fail;

        if (!q) {
            q = cand;
        } else {
            bool same = expr_equal_exact_local(q, cand);
            expr_free(cand);
            if (!same)
                goto fail;
        }
    }

    if (!q)
        goto fail;

    expr_retain(p);
    expr_retain(p);
    disc = expr_mul_simplify_owned(p, p);
    if (!disc)
        goto fail;

    expr_retain(q);
    expr_t *four_q = expr_mul_simplify_owned(expr_const_long(4), q);
    if (!four_q)
        goto fail;
    disc = expr_add_simplify_owned(disc, four_q);
    if (!disc)
        goto fail;

    if (expr_is_exact_zero(disc)) {
        expr_t *lambda = NULL;

        expr_retain(p);
        lambda = expr_div_simplify_owned(p, expr_const_long(2));
        lambda = expr_simplify_owned(lambda);
        if (!lambda)
            goto fail;

        if (expr_fun_coeffs_up_to_second(&f1, &c1, NULL, scalar_f, lambda) != 0)
            goto fail;

        expr_retain(c1);
        expr_retain(lambda);
        expr_t *lambda_df = expr_mul_simplify_owned(c1, lambda);
        if (!lambda_df)
            goto fail;

        expr_retain(f1);
        c0 = expr_sub_simplify_owned(f1, lambda_df);
        expr_free(lambda);
        if (!c0 || !c1)
            goto fail;
    } else {
        expr_t *sum = NULL;
        expr_t *diff = NULL;
        expr_t *num = NULL;
        expr_t *den = NULL;

        root = expr_sqrt(disc);
        root = expr_simplify_owned(root);
        if (!root)
            goto fail;

        expr_retain(p);
        expr_retain(root);
        sum = expr_add_simplify_owned(p, root);
        if (!sum)
            goto fail;

        expr_retain(p);
        expr_retain(root);
        diff = expr_sub_simplify_owned(p, root);
        if (!diff)
            goto fail;

        lambda1 = expr_div_simplify_owned(sum, expr_const_long(2));
        lambda1 = expr_simplify_owned(lambda1);
        sum = NULL;
        lambda2 = expr_div_simplify_owned(diff, expr_const_long(2));
        lambda2 = expr_simplify_owned(lambda2);
        diff = NULL;
        if (!lambda1 || !lambda2)
            goto fail;

        scalar_f(&f1, &lambda1);
        scalar_f(&f2, &lambda2);
        if (!f1 || !f2)
            goto fail;

        expr_retain(f1);
        expr_retain(f2);
        num = expr_sub_simplify_owned(f1, f2);
        expr_retain(lambda1);
        expr_retain(lambda2);
        den = expr_sub_simplify_owned(lambda1, lambda2);
        c1 = expr_div_simplify_owned(num, den);
        if (!c1)
            goto fail;

        expr_retain(c1);
        expr_retain(lambda1);
        expr_t *c1_l1 = expr_mul_simplify_owned(c1, lambda1);
        if (!c1_l1)
            goto fail;

        expr_retain(f1);
        c0 = expr_sub_simplify_owned(f1, c1_l1);
        if (!c0)
            goto fail;
    }

    out = mat_new_expr(A->rows, A->cols);
    if (!out)
        goto fail;

    for (size_t i = 0; i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            expr_t *aij = NULL;
            expr_t *term = NULL;
            expr_t *entry = NULL;

            mat_get(A, i, j, &aij);
            expr_retain(c1);
            expr_retain(aij ? aij : EXPR_ZERO);
            term = expr_mul_simplify_owned(c1, aij ? aij : EXPR_ZERO);
            if (!term)
                goto fail;

            if (i == j) {
                expr_retain(c0);
                entry = expr_add_simplify_owned(c0, term);
            } else {
                entry = term;
            }

            if (!entry)
                goto fail;

            mat_set(out, i, j, &entry);
            expr_free(entry);
        }
    }

    mat_free(A2);
    expr_free(p);
    expr_free(q);
    expr_free(disc);
    expr_free(root);
    expr_free(lambda1);
    expr_free(lambda2);
    expr_free(f1);
    expr_free(f2);
    expr_free(c0);
    expr_free(c1);
    return out;

fail:
    mat_free(A2);
    if (out)
        mat_free(out);
    expr_free(p);
    expr_free(q);
    expr_free(disc);
    expr_free(root);
    expr_free(lambda1);
    expr_free(lambda2);
    expr_free(f1);
    expr_free(f2);
    expr_free(c0);
    expr_free(c1);
    return NULL;
}

static matrix_t *mat_fun_expr_structured(const matrix_t *A,
                                         void (*scalar_f)(void *out, const void *in))
{
    NUM_SCOPE(scope);
    size_t n;
    matrix_t *T;
    matrix_t *FT;
    matrix_t *out;
    expr_t *diag0 = NULL;
    number_t tol = num_create_from_double(1e-24);

    if (!A || !scalar_f || !matrix_is_symbolic(A))
        return NULL;
    if (A->rows != A->cols) {
        return NULL;
    }
    if (mat_is_upper_triangular(A)) {
        if (A->rows == 0) {
            return mat_copy_preserving_store(A);
        }
        if (mat_is_diagonal(A)) {
            return mat_fun_elementwise_same_type(A, scalar_f);
        }

        n = A->rows;
        mat_get_owned(A, 0, 0, &diag0);
        for (size_t i = 1; i < n; ++i) {
            NUM_SCOPE(iter_scope);
            expr_t *diag_i = NULL;
            number_t diag0_num;
            number_t diag_i_num;
            number_t diff_num;
            number_t absdiff;
            mat_get_owned(A, i, i, &diag_i);
            diag0_num = expr_eval(diag0);
            diag_i_num = expr_eval(diag_i);
            diff_num = num_sub(diag_i_num, diag0_num);
            absdiff = num_abs(diff_num);
            expr_free(diag_i);
            if (num_lt(tol, absdiff)) {
                expr_free(diag0);
                return (A->rows == 2 && A->cols == 2)
                    ? mat_fun_expr_diagonalizable_2x2(A, scalar_f) : NULL;
            }
        }
        expr_free(diag0);
        return mat_fun_triangular_equal_diag_expr(A, scalar_f);
    }

    if (!mat_is_lower_triangular(A)) {
        matrix_t *block_diag = mat_fun_expr_block_diagonal(A, scalar_f);
        if (block_diag)
            return block_diag;
        matrix_t *perm_block_diag = mat_fun_expr_permuted_block_diagonal(A, scalar_f);
        if (perm_block_diag)
            return perm_block_diag;
        matrix_t *uniform = mat_fun_expr_uniform_diag_offdiag(A, scalar_f);
        if (uniform)
            return uniform;
        matrix_t *rank_one = mat_fun_expr_scalar_plus_rank_one(A, scalar_f);
        if (rank_one)
            return rank_one;
        matrix_t *cubic_linear = mat_fun_expr_cubic_linear_exact(A, scalar_f);
        if (cubic_linear)
            return cubic_linear;
        matrix_t *quartic_biquadratic = mat_fun_expr_quartic_biquadratic_exact(A, scalar_f);
        if (quartic_biquadratic)
            return quartic_biquadratic;
        matrix_t *quadratic = mat_fun_expr_quadratic_exact(A, scalar_f);
        if (quadratic)
            return quadratic;
        if (A->rows == 2 && A->cols == 2)
            return mat_fun_expr_diagonalizable_2x2(A, scalar_f);
        return NULL;
    }

    T = mat_transpose(A);
    if (!T)
        return NULL;
    FT = mat_fun_expr_structured(T, scalar_f);
    out = FT ? mat_transpose(FT) : NULL;
    mat_free(T);
    mat_free(FT);
    return out;
}

static matrix_t *mat_fun_apply(const matrix_t *A,
                               void (*number_scalar_f)(void *out, const void *in),
                               void (*expr_scalar_f)(void *out, const void *in),
                               void (*native_scalar_f)(void *out, const void *in))
{
    if (matrix_is_symbolic(A))
        return expr_scalar_f ? mat_fun_expr_structured(A, expr_scalar_f) : NULL;
    if (A && A->rows == A->cols && native_scalar_f && mat_is_diagonal(A))
        return mat_fun_elementwise_same_type(A, native_scalar_f);
    return mat_fun_schur(A, number_scalar_f);
}

static mat_fun_cache_entry_t *mat_fun_cache_find(const matrix_t *A)
{
    for (mat_fun_cache_entry_t *it = mat_fun_cache_head; it; it = it->next)
        if (it->key == A)
            return it;
    return NULL;
}

void mat_fun_cache_forget(const matrix_t *A)
{
    mat_fun_cache_entry_t **link = &mat_fun_cache_head;

    while (*link) {
        mat_fun_cache_entry_t *entry = *link;
        if (entry->key != A) {
            link = &entry->next;
            continue;
        }

        *link = entry->next;
        mat_free(entry->spectral_Vq);
        mat_fun_cache_free_spectral_evals(entry->spectral_evals, A->rows);
        mat_free(entry->exp_preimage);
        free(entry);
        return;
    }
}

/* Scale every element of A in-place by the number scalar r. */
static void mat_scale_number(matrix_t *A, const number_t *r)
{
    const struct elem_vtable *e = A->elem;
    unsigned char r_raw[MATRIX_SCALAR_STORAGE_BYTES] = {0}, v[MATRIX_SCALAR_STORAGE_BYTES] = {0}, scaled[MATRIX_SCALAR_STORAGE_BYTES] = {0};

    mat_raw_value_from_number(e, r_raw, r);
    elem_init_zero_value(e, scaled);
    for (size_t i = 0; i < A->rows; i++)
        for (size_t j = 0; j < A->cols; j++) {
            mat_get(A, i, j, v);
            e->mul(scaled, r_raw, v);
            mat_set(A, i, j, scaled);
        }
    elem_destroy_value(e, scaled);
    elem_destroy_value(e, r_raw);

    mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
    if (!cache)
        return;

    if (cache->spectral_evals) {
        for (size_t i = 0; i < A->rows; ++i) {
            number_t scaled = num_mul(cache->spectral_evals[i], *r);
            num_destroy(&cache->spectral_evals[i]);
            cache->spectral_evals[i] = scaled;
        }
    }

    if (cache->exp_preimage) {
        mat_free(cache->exp_preimage);
        cache->exp_preimage = NULL;
    }
}

static void mat_attach_spectral_cache(matrix_t *A,
                                      const matrix_t *Vq,
                                      const number_t *evals)
{
    if (!A || !Vq || !evals || A->rows != A->cols)
        return;

    matrix_t *Vcopy = mat_copy_preserving_store(Vq);
    number_t *ecopy = calloc(A->rows, sizeof(*ecopy));
    if (!Vcopy || !ecopy) {
        mat_free(Vcopy);
        free(ecopy);
        return;
    }

    mat_fun_number_array_invalidate(ecopy, A->rows);
    for (size_t i = 0; i < A->rows; ++i)
        ecopy[i] = num_clone(evals[i]);

    mat_fun_cache_entry_t *entry = mat_fun_cache_find(A);
    if (!entry) {
        entry = calloc(1, sizeof(*entry));
        if (!entry) {
            mat_free(Vcopy);
            mat_fun_cache_free_spectral_evals(ecopy, A->rows);
            return;
        }
        entry->key = A;
        entry->next = mat_fun_cache_head;
        mat_fun_cache_head = entry;
    }

    mat_free(entry->spectral_Vq);
    mat_fun_cache_free_spectral_evals(entry->spectral_evals, A->rows);

    entry->spectral_Vq = Vcopy;
    entry->spectral_evals = ecopy;
}

static matrix_t *mat_fun_from_spectral_cache(const matrix_t *A,
                                             void (*scalar_f)(void *out, const void *in))
{
    size_t n = A->rows;
    const struct elem_vtable *orig_elem = A->elem;
    mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
    matrix_t *FD = mat_create_diagonal_with_elem(n, &number_elem);
    number_t *mapped = calloc(n, sizeof(*mapped));
    if (!cache || !cache->spectral_Vq || !cache->spectral_evals || !FD || !mapped) {
        mat_free(FD);
        free(mapped);
        return NULL;
    }
    mat_fun_number_array_invalidate(mapped, n);

    for (size_t i = 0; i < n; ++i) {
        mapped[i] = mat_eval_number_scalar_number_local(scalar_f, &cache->spectral_evals[i]);
        mat_set(FD, i, i, &mapped[i]);
    }

    matrix_t *VF = mat_mul(cache->spectral_Vq, FD);
    matrix_t *Vinv = mat_inverse(cache->spectral_Vq);
    matrix_t *R = (VF && Vinv) ? mat_mul(VF, Vinv) : NULL;

    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);
    if (!R) {
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return NULL;
    }

    mat_attach_spectral_cache(R, cache->spectral_Vq, mapped);

    if (orig_elem == R->elem) {
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return R;
    }

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) {
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        mat_free(R);
        return NULL;
    }

    mat_attach_spectral_cache(out, cache->spectral_Vq, mapped);
    mat_fun_number_array_destroy(mapped, n);
    free(mapped);
    mat_free(R);
    return out;
}

static void mat_set_exp_preimage_cache(matrix_t *A, const matrix_t *preimage)
{
    if (!A || !preimage)
        return;

    matrix_t *copy = mat_copy_preserving_store(preimage);
    if (!copy)
        return;

    mat_fun_cache_entry_t *entry = mat_fun_cache_find(A);
    if (!entry) {
        entry = calloc(1, sizeof(*entry));
        if (!entry) {
            mat_free(copy);
            return;
        }
        entry->key = A;
        entry->next = mat_fun_cache_head;
        mat_fun_cache_head = entry;
    }

    mat_free(entry->exp_preimage);
    entry->exp_preimage = copy;
}

static matrix_t *mat_fun_hermitian(const matrix_t *A,
                                   void (*scalar_f)(void *out, const void *in))
{
    size_t n = A->rows;
    const struct elem_vtable *orig_elem = A->elem;
    number_t *eval_buf = calloc(n, sizeof(*eval_buf));
    number_t *mapped = calloc(n, sizeof(*mapped));
    if (!eval_buf || !mapped) {
        free(eval_buf);
        free(mapped);
        return NULL;
    }
    mat_fun_number_array_invalidate(eval_buf, n);
    mat_fun_number_array_invalidate(mapped, n);

    matrix_t *V = NULL;
    matrix_t *Vq = NULL;
    if (mat_eigendecompose(A, eval_buf, &V) != 0) {
        mat_fun_number_array_destroy(eval_buf, n);
        free(eval_buf);
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return NULL;
    }

    Vq = mat_copy_preserving_store(V);
    if (!Vq) {
        mat_free(V);
        mat_fun_number_array_destroy(eval_buf, n);
        free(eval_buf);
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return NULL;
    }

    matrix_t *FD = mat_create_diagonal_with_elem(n, &number_elem);
    if (!FD) {
        mat_free(Vq);
        mat_free(V);
        mat_fun_number_array_destroy(eval_buf, n);
        free(eval_buf);
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        mapped[i] = mat_eval_number_scalar_number_local(scalar_f, &eval_buf[i]);
        mat_set(FD, i, i, &mapped[i]);
    }

    matrix_t *VF = mat_mul(Vq, FD);
    matrix_t *Vinv = mat_inverse(Vq);
    matrix_t *R = (VF && Vinv) ? mat_mul(VF, Vinv) : NULL;

    mat_free(FD);
    mat_free(VF);
    mat_free(Vinv);

    if (!R) {
        mat_free(Vq);
        mat_free(V);
        mat_fun_number_array_destroy(eval_buf, n);
        free(eval_buf);
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        return NULL;
    }

    mat_attach_spectral_cache(R, Vq, mapped);

    mat_fun_number_array_destroy(eval_buf, n);
    free(eval_buf);

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) {
        mat_fun_number_array_destroy(mapped, n);
        free(mapped);
        mat_free(Vq);
        mat_free(V);
        mat_free(R);
        return NULL;
    }

    mat_attach_spectral_cache(out, Vq, mapped);
    mat_free(Vq);
    mat_free(V);
    mat_fun_number_array_destroy(mapped, n);
    free(mapped);
    mat_free(R);
    return out;
}

static int mat_is_upper_triangular_local(const matrix_t *A)
{
    number_t tol = num_create_from_double(1e-30);

    if (!A || A->rows != A->cols)
        return 0;

    for (size_t i = 1; i < A->rows; ++i) {
        for (size_t j = 0; j < i; ++j) {
            unsigned char raw[MATRIX_SCALAR_STORAGE_BYTES];
            number_t z;
            number_t absz;
            mat_get(A, i, j, raw);
            z = mat_raw_value_to_number(A->elem, raw);
            absz = num_abs(z);
            num_destroy(&z);
            if (num_lt(tol, absz)) {
                num_destroy(&absz);
                num_destroy(&tol);
                return 0;
            }
            num_destroy(&absz);
        }
    }

    num_destroy(&tol);
    return 1;
}

static matrix_t *mat_fun_upper_triangular(const matrix_t *A,
                                          void (*scalar_f)(void *out, const void *in))
{
    const struct elem_vtable *orig_elem = A->elem;
    matrix_t *T = mat_copy_preserving_store(A);
    matrix_t *FT = NULL;
    matrix_t *out = NULL;

    if (!T)
        return NULL;

    FT = mat_fun_triangular(T, scalar_f);
    if (!FT) {
        mat_free(T);
        return NULL;
    }

    out = mat_convert_with_store(FT, orig_elem, FT->store);
    if (!out) {
        mat_free(T);
        mat_free(FT);
        return NULL;
    }

    mat_free(T);
    mat_free(FT);
    return out;
}

/* ============================================================
   Schur-based matrix function engine
   ============================================================ */

matrix_t *mat_fun_schur(const matrix_t *A,
                        void (*scalar_f)(void *out, const void *in))
{
    if (!A || !scalar_f)
        return NULL;
    if (!mat_elem_supports_numeric_algorithms(A))
        return NULL;

    if (A->rows != A->cols)
        return NULL;

    if (A->rows == 1) {
        const struct elem_vtable *orig_elem = A->elem;
        matrix_t *out = mat_create_diagonal_with_elem(1, orig_elem);
        number_t in_num = number_invalid();
        number_t out_num = number_invalid();
        unsigned char raw_in[MATRIX_SCALAR_STORAGE_BYTES] = {0}, raw_out[MATRIX_SCALAR_STORAGE_BYTES] = {0};

        if (!out)
            return NULL;

        mat_get(A, 0, 0, raw_in);
        in_num = mat_raw_value_to_number(orig_elem, raw_in);
        out_num = mat_eval_number_scalar_number_local(scalar_f, &in_num);
        mat_raw_value_from_number(orig_elem, raw_out, &out_num);
        num_destroy(&out_num);
        num_destroy(&in_num);
        mat_set(out, 0, 0, raw_out);
        return out;
    }

    mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
    if (cache && cache->spectral_Vq && cache->spectral_evals)
        return mat_fun_from_spectral_cache(A, scalar_f);

    if (mat_is_hermitian(A))
        return mat_fun_hermitian(A, scalar_f);

    if (mat_is_upper_triangular_local(A))
        return mat_fun_upper_triangular(A, scalar_f);

    const struct elem_vtable *orig_elem = A->elem;
    mat_schur_factor_t S;
    int schur_rc = mat_schur_factor(A, &S);
    if (schur_rc != 0) {
        fprintf(stderr, "[mat_fun_schur] mat_schur_factor returned %d for %zu×%zu matrix\n",
                schur_rc, A->rows, A->cols);
        return NULL;
    }

    matrix_t *FT = mat_fun_triangular(S.T, scalar_f);
    if (!FT) {
        mat_schur_factor_free(&S);
        return NULL;
    }

    /* Q f(T) Q* */
    matrix_t *QFT = mat_mul(S.Q, FT);
    if (!QFT) {
        mat_free(FT);
        mat_schur_factor_free(&S);
        return NULL;
    }

    matrix_t *QH = mat_hermitian(S.Q);
    if (!QH) {
        mat_free(FT);
        mat_free(QFT);
        mat_schur_factor_free(&S);
        return NULL;
    }

    matrix_t *R = mat_mul(QFT, QH);   /* qcomplex result */

    mat_free(FT);
    mat_free(QFT);
    mat_free(QH);
    mat_schur_factor_free(&S);

    if (!R) return NULL;

    matrix_t *out = mat_convert_with_store(R, orig_elem, R->store);
    if (!out) { mat_free(R); return NULL; }

    mat_free(R);
    return out;
}

matrix_t *mat_exp(const matrix_t *A)
{
    matrix_t *structured = NULL;

    if (A) {
        mat_fun_cache_entry_t *cache = mat_fun_cache_find(A);
        if (cache && cache->exp_preimage)
            return mat_copy_preserving_store(cache->exp_preimage);
        if (A->elem == &number_elem &&
            A->rows == A->cols &&
            (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
            mat_number_diagonal_equal_local(A)) {
            structured = mat_exp_number_triangular_equal_diag(A);
            if (structured)
                return structured;
        }
    }
    return mat_apply_unary(A, number_elem.fun->exp, expr_elem.fun->exp,
                           A && A->elem && A->elem->fun ? A->elem->fun->exp : NULL);
}

matrix_t *mat_log(const matrix_t *A)
{
    matrix_t *structured = NULL;

    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        structured = mat_log_number_triangular_equal_diag(A);
        if (structured) {
            mat_set_exp_preimage_cache(structured, A);
            return structured;
        }
    }

    matrix_t *R = mat_apply_unary(A, number_elem.fun->log, expr_elem.fun->log,
                                  A && A->elem && A->elem->fun ? A->elem->fun->log : NULL);
    if (R)
        mat_set_exp_preimage_cache(R, A);
    return R;
}

matrix_t *mat_log10(const matrix_t *A)
{
    matrix_t *L = mat_log(A);
    matrix_t *R = NULL;
    number_t log_ten;

    if (!L)
        return NULL;

    log_ten = num_const(NUM_LN10);
    R = mat_scalar_div(L, &log_ten);

    num_destroy(&log_ten);
    mat_free(L);

    return R;
}

matrix_t *mat_sin(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_sin_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->sin, expr_elem.fun->sin,
                           A && A->elem && A->elem->fun ? A->elem->fun->sin : NULL);
}

matrix_t *mat_cos(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_cos_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->cos, expr_elem.fun->cos,
                           A && A->elem && A->elem->fun ? A->elem->fun->cos : NULL);
}

matrix_t *mat_tan(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_tan_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->tan, expr_elem.fun->tan,
                           A && A->elem && A->elem->fun ? A->elem->fun->tan : NULL);
}

matrix_t *mat_sinh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_sinh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->sinh, expr_elem.fun->sinh,
                           A && A->elem && A->elem->fun ? A->elem->fun->sinh : NULL);
}

matrix_t *mat_cosh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_cosh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->cosh, expr_elem.fun->cosh,
                           A && A->elem && A->elem->fun ? A->elem->fun->cosh : NULL);
}

matrix_t *mat_tanh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_tanh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->tanh, expr_elem.fun->tanh,
                           A && A->elem && A->elem->fun ? A->elem->fun->tanh : NULL);
}

matrix_t *mat_sqrt(const matrix_t *A)
{
    matrix_t *structured = NULL;

    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        structured = mat_sqrt_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }

    return mat_apply_unary(A, number_elem.fun->sqrt, expr_elem.fun->sqrt,
                           A && A->elem && A->elem->fun ? A->elem->fun->sqrt : NULL);
}

matrix_t *mat_asin(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_asin_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->asin, expr_elem.fun->asin,
                           A && A->elem && A->elem->fun ? A->elem->fun->asin : NULL);
}

matrix_t *mat_acos(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_acos_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->acos, expr_elem.fun->acos,
                           A && A->elem && A->elem->fun ? A->elem->fun->acos : NULL);
}

matrix_t *mat_atan(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_atan_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->atan, expr_elem.fun->atan,
                           A && A->elem && A->elem->fun ? A->elem->fun->atan : NULL);
}

matrix_t *mat_asinh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_asinh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->asinh, expr_elem.fun->asinh,
                           A && A->elem && A->elem->fun ? A->elem->fun->asinh : NULL);
}

matrix_t *mat_acosh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_acosh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->acosh, expr_elem.fun->acosh,
                           A && A->elem && A->elem->fun ? A->elem->fun->acosh : NULL);
}

matrix_t *mat_atanh(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_atanh_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->atanh, expr_elem.fun->atanh,
                           A && A->elem && A->elem->fun ? A->elem->fun->atanh : NULL);
}

matrix_t *mat_erf(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_erf_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->erf, expr_elem.fun->erf,
                           A && A->elem && A->elem->fun ? A->elem->fun->erf : NULL);
}

matrix_t *mat_erfc(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_erfc_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->erfc, expr_elem.fun->erfc,
                           A && A->elem && A->elem->fun ? A->elem->fun->erfc : NULL);
}

matrix_t *mat_erfinv(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_erfinv_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->erfinv, expr_elem.fun->erfinv,
                           A && A->elem && A->elem->fun ? A->elem->fun->erfinv : NULL);
}

matrix_t *mat_erfcinv(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_erfcinv_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->erfcinv, expr_elem.fun->erfcinv,
                           A && A->elem && A->elem->fun ? A->elem->fun->erfcinv : NULL);
}

matrix_t *mat_gamma(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_gamma_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->gamma, expr_elem.fun->gamma,
                           A && A->elem && A->elem->fun ? A->elem->fun->gamma : NULL);
}

matrix_t *mat_lgamma(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_lgamma_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->lgamma, expr_elem.fun->lgamma,
                           A && A->elem && A->elem->fun ? A->elem->fun->lgamma : NULL);
}

matrix_t *mat_digamma(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_digamma_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->digamma, expr_elem.fun->digamma,
                           A && A->elem && A->elem->fun ? A->elem->fun->digamma : NULL);
}

matrix_t *mat_trigamma(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_trigamma_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->trigamma, expr_elem.fun->trigamma,
                           A && A->elem && A->elem->fun ? A->elem->fun->trigamma : NULL);
}

matrix_t *mat_tetragamma(const matrix_t *A)
{
    return mat_apply_unary(A, number_elem.fun->tetragamma, expr_elem.fun->tetragamma,
                           A && A->elem && A->elem->fun ? A->elem->fun->tetragamma : NULL);
}

matrix_t *mat_gammainv(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_gammainv_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->gammainv, expr_elem.fun->gammainv,
                           A && A->elem && A->elem->fun ? A->elem->fun->gammainv : NULL);
}

matrix_t *mat_normal_pdf(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_normal_pdf_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->normal_pdf, expr_elem.fun->normal_pdf,
                           A && A->elem && A->elem->fun ? A->elem->fun->normal_pdf : NULL);
}

matrix_t *mat_normal_cdf(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_normal_cdf_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->normal_cdf, expr_elem.fun->normal_cdf,
                           A && A->elem && A->elem->fun ? A->elem->fun->normal_cdf : NULL);
}

matrix_t *mat_normal_logpdf(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_normal_logpdf_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->normal_logpdf, expr_elem.fun->normal_logpdf,
                           A && A->elem && A->elem->fun ? A->elem->fun->normal_logpdf : NULL);
}

matrix_t *mat_lambert_w0(const matrix_t *A)
{
    if (A && A->elem == &expr_elem && matrix_is_symbolic(A))
        return mat_fun_elementwise_same_type(A, expr_elem.fun->lambert_w0);

    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_lambert_w0_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->lambert_w0, expr_elem.fun->lambert_w0,
                           A && A->elem && A->elem->fun ? A->elem->fun->lambert_w0 : NULL);
}

matrix_t *mat_lambert_wm1(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_lambert_wm1_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->lambert_wm1, expr_elem.fun->lambert_wm1,
                           A && A->elem && A->elem->fun ? A->elem->fun->lambert_wm1 : NULL);
}

matrix_t *mat_productlog(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_productlog_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->productlog, expr_elem.fun->productlog,
                           A && A->elem && A->elem->fun ? A->elem->fun->productlog : NULL);
}

matrix_t *mat_ei(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_ei_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->ei, expr_elem.fun->ei,
                           A && A->elem && A->elem->fun ? A->elem->fun->ei : NULL);
}

matrix_t *mat_e1(const matrix_t *A)
{
    if (A &&
        A->elem == &number_elem &&
        A->rows == A->cols &&
        (mat_is_upper_triangular(A) || mat_is_lower_triangular(A)) &&
        mat_number_diagonal_equal_local(A)) {
        matrix_t *structured = mat_e1_number_triangular_equal_diag(A);
        if (structured)
            return structured;
    }
    return mat_apply_unary(A, number_elem.fun->e1, expr_elem.fun->e1,
                           A && A->elem && A->elem->fun ? A->elem->fun->e1 : NULL);
}


/* ============================================================
   mat_pow_int  —  binary exponentiation
   Negative exponents invert A first.
   ============================================================ */

matrix_t *mat_pow_int(const matrix_t *A, int n)
{
    matrix_t *simplified = NULL;

    if (!A || A->rows != A->cols) return NULL;
    if (!mat_elem_supports_numeric_algorithms(A) && n < 0) return NULL;
    size_t sz = A->rows;
    const struct elem_vtable *e = A->elem;

    matrix_t *base;
    unsigned int p;
    if (n < 0) {
        base = mat_inverse(A);
        if (!base) return NULL;
        p = (unsigned int)(-(long long)n);
    } else {
        base = mat_copy_preserving_store(A);
        if (!base) return NULL;
        p = (unsigned int)n;
    }

    matrix_t *result = mat_create_identity_with_elem(sz, e);
    if (!result) { mat_free(base); return NULL; }

    while (p > 0u) {
        if (p & 1u) {
            matrix_t *tmp = mat_mul(result, base);
            mat_free(result);
            if (!tmp) { mat_free(base); return NULL; }
            result = tmp;
        }
        p >>= 1u;
        if (p > 0u) {
            matrix_t *tmp = mat_mul(base, base);
            mat_free(base);
            if (!tmp) { mat_free(result); return NULL; }
            base = tmp;
        }
    }

    mat_free(base);

    if (result->elem != &expr_elem)
        return result;

    simplified = mat_simplify_symbolic(result);
    mat_free(result);
    return simplified;
}

/* ============================================================
   mat_pow  —  A^s = exp(s · log(A))
   Requires A to admit a principal logarithm (positive definite).
   ============================================================ */

matrix_t *mat_pow(const matrix_t *A, const number_t *s)
{
    matrix_t *result = NULL;

    if (!A || A->rows != A->cols) return NULL;
    if (!s) return NULL;
    if (matrix_is_symbolic(A))
        return NULL;
    if (!mat_elem_supports_numeric_algorithms(A)) return NULL;

    matrix_t *L = mat_log(A);
    if (!L) return NULL;

    mat_scale_number(L, s);
    result = mat_exp(L);
    mat_free(L);
    return result;
}
