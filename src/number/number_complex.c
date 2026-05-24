#include "number.h"
#include "number_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool number_eq_same_tol_complex(const number_t *a, const number_t *b);

bool number_is_real_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && num_is_zero(value->imag);
}

bool number_is_zero_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && num_is_zero(value->real) && num_is_zero(value->imag);
}

bool number_is_one_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && num_is_zero(value->imag) && num_eq(value->real, NUM_ONE);
}

bool number_is_finite_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && num_is_finite(value->real) && num_is_finite(value->imag);
}

bool number_is_nan_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return !value || num_is_nan(value->real) || num_is_nan(value->imag);
}

bool number_is_inf_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && (num_is_inf(value->real) || num_is_inf(value->imag));
}

bool number_eq_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);

    if (!av || !bv)
        return false;
    if (num_is_exact(av->real) && num_is_exact(av->imag) &&
        num_is_exact(bv->real) && num_is_exact(bv->imag))
        return num_eq(av->real, bv->real) && num_eq(av->imag, bv->imag);
    return number_eq_same_tol_complex(a, b);
}

bool number_eq_same_tol_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    size_t precision_bits;
    size_t candidate;

    if (!av || !bv)
        return false;
    precision_bits = num_get_prec_bits(*a);
    candidate = num_get_prec_bits(*b);
    if (candidate > precision_bits)
        precision_bits = candidate;
    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    return number_eq_same_tol_with_precision(&av->real, &bv->real, precision_bits) &&
           number_eq_same_tol_with_precision(&av->imag, &bv->imag, precision_bits);
}

int number_cmp_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    int rc;

    if (!av || !bv)
        return 0;
    rc = num_cmp(av->real, bv->real);
    return rc != 0 ? rc : num_cmp(av->imag, bv->imag);
}

int number_set_precision_complex(number_t *number, size_t precision_bits)
{
    complex_t *value = number ? number_impl(number)->value.cx : NULL;
    number_t real;
    number_t imag;
    number_const_id_t constant_id;
    bool had_cache;
    mpc_t cache_copy;

    if (!number || !value || precision_bits == 0u)
        return -1;
    real = num_const_prec(value->real, precision_bits);
    imag = num_const_prec(value->imag, precision_bits);
    if (!number_is_valid_value(&real) || !number_is_valid_value(&imag)) {
        num_destroy(&real);
        num_destroy(&imag);
        return -1;
    }
    constant_id = value->constant_id;
    had_cache = value->mpc_cache_valid;
    if (had_cache) {
        mpc_init2(cache_copy, (mpfr_prec_t)precision_bits);
        mpc_set(cache_copy, value->mpc_cache, MPC_RNDNN);
    }
    number_complex_clear_mpc_cache(value);
    num_destroy(&value->real);
    num_destroy(&value->imag);
    value->real = num_scope_detach(real);
    value->imag = num_scope_detach(imag);
    value->constant_id = constant_id;
    value->precision_bits = precision_bits;
    if (had_cache) {
        (void)number_complex_set_mpc_cache_from_mpc(value, cache_copy,
            precision_bits);
        mpc_clear(cache_copy);
    }
    return 0;
}

size_t number_get_precision_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    size_t real_bits;
    size_t imag_bits;

    if (!value)
        return 0u;
    real_bits = num_get_prec_bits(value->real);
    imag_bits = num_get_prec_bits(value->imag);
    if (real_bits > imag_bits)
        return real_bits;
    if (imag_bits > 0u)
        return imag_bits;
    return value->precision_bits;
}

bool number_is_integer_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value && num_is_zero(value->imag) && num_is_integer(value->real);
}

size_t number_get_mantissa_bits_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value ? num_get_mantissa_bits(value->real) : 0u;
}

bool number_get_mantissa_u64_complex(const number_t *number, uint64_t *out)
{
    const complex_t *value = number_complex_value(number);

    return value && out && num_get_mantissa_u64(value->real, out);
}

char *number_to_string_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    char *real = NULL;
    char *imag = NULL;
    char *out = NULL;
    bool imag_negative;
    size_t len;

    if (!value)
        return NULL;
    if (num_is_zero(value->imag))
        return num_to_string(value->real);
    if (num_is_zero(value->real)) {
        if (num_eq(value->imag, NUM_ONE))
            return number_strdup("i");
        if (num_eq(value->imag, NUM_NEG_ONE))
            return number_strdup("-i");
        imag = num_to_string(value->imag);
        if (!imag)
            return NULL;
        len = strlen(imag) + 2u;
        out = malloc(len);
        if (out)
            snprintf(out, len, "%si", imag);
        free(imag);
        return out;
    }

    real = num_to_string(value->real);
    imag_negative = num_cmp(value->imag, NUM_ZERO) < 0;
    {
        number_t abs_imag = num_abs(value->imag);
        imag = num_to_string(abs_imag);
        num_destroy(&abs_imag);
    }
    if (!real || !imag)
        goto done;
    if (strcmp(imag, "1") == 0) {
        free(imag);
        imag = number_strdup("");
        if (!imag)
            goto done;
    }
    len = strlen(real) + strlen(imag) + 6u;
    out = malloc(len);
    if (out)
        snprintf(out, len, "%s %c %si", real, imag_negative ? '-' : '+', imag);

done:
    free(real);
    free(imag);
    return out;
}

number_t *number_clone_complex(const number_t *number)
{
    complex_t *copy;

    if (!number || !number_impl_const(number)->value.cx)
        return NULL;
    copy = number_complex_clone(number_impl_const(number)->value.cx);
    return copy ? number_wrap_complex(copy) : NULL;
}

number_t *number_neg_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    number_t real;
    number_t imag;

    if (!value)
        return NULL;
    real = num_neg(value->real);
    imag = num_neg(value->imag);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_inv_complex(const number_t *number)
{
    const complex_t *z = number_complex_value(number);
    number_t ac;
    number_t bd;
    number_t denom;
    number_t real_num;
    number_t imag_num;
    number_t real;
    number_t imag;

    if (!z)
        return NULL;
    ac = num_mul(z->real, z->real);
    bd = num_mul(z->imag, z->imag);
    denom = num_add(ac, bd);
    real_num = num_clone(z->real);
    imag_num = num_neg(z->imag);
    real = num_div(real_num, denom);
    imag = num_div(imag_num, denom);
    num_destroy(&ac);
    num_destroy(&bd);
    num_destroy(&denom);
    num_destroy(&real_num);
    num_destroy(&imag_num);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_abs_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    number_t real2;
    number_t imag2;
    number_t sum;
    number_t out;
    number_t *boxed;

    if (!value)
        return NULL;
    real2 = num_mul(value->real, value->real);
    imag2 = num_mul(value->imag, value->imag);
    sum = num_add(real2, imag2);
    out = num_sqrt(sum);
    num_destroy(&real2);
    num_destroy(&imag2);
    num_destroy(&sum);
    boxed = number_box_value(num_clone(out));
    num_destroy(&out);
    return boxed;
}

number_t *number_conj_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    number_t real;
    number_t imag;

    if (!value)
        return NULL;
    real = num_clone(value->real);
    imag = num_neg(value->imag);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_real_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value ? number_box_value(num_clone(value->real)) : NULL;
}

number_t *number_imag_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);

    return value ? number_box_value(num_clone(value->imag)) : NULL;
}

number_t *number_arg_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    number_t imag;
    number_t real;
    number_t out;
    number_t *boxed;

    if (!value)
        return NULL;
    imag = num_as_inexact_real_prec(value->imag, num_get_prec_bits(*number));
    real = num_as_inexact_real_prec(value->real, num_get_prec_bits(*number));
    out = num_atan2(imag, real);
    boxed = number_box_value(num_clone(out));
    num_destroy(&imag);
    num_destroy(&real);
    num_destroy(&out);
    return boxed;
}

number_t *number_floor_complex(const number_t *number)
{
    const complex_t *value = number_complex_value(number);
    number_t real;
    number_t imag;

    if (!value)
        return NULL;
    real = num_floor(value->real);
    imag = num_floor(value->imag);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_pow_int_complex(const number_t *number, int exponent)
{
    number_t acc = NUM_ONE;
    number_t base;
    number_t tmp;
    number_t *boxed;
    int n = exponent < 0 ? -exponent : exponent;

    if (!number)
        return NULL;
    base = num_clone(*number);
    while (n-- > 0) {
        tmp = num_mul(acc, base);
        if (!num_is_immortal(acc))
            num_destroy(&acc);
        acc = tmp;
    }
    if (exponent < 0) {
        tmp = num_inv(acc);
        if (!num_is_immortal(acc))
            num_destroy(&acc);
        acc = tmp;
    }
    boxed = number_box_value(num_clone(acc));
    if (!num_is_immortal(acc))
        num_destroy(&acc);
    num_destroy(&base);
    return boxed;
}

number_t *number_ldexp_complex(const number_t *number, int exponent2)
{
    const complex_t *value = number_complex_value(number);
    number_t real;
    number_t imag;

    if (!value)
        return NULL;
    real = num_ldexp(value->real, exponent2);
    imag = num_ldexp(value->imag, exponent2);
    return number_wrap_complex_parts(real, imag);
}

typedef int (*number_complex_mpc_binary_fn)(mpc_ptr,
                                            mpc_srcptr,
                                            mpc_srcptr,
                                            mpc_rnd_t);

static size_t number_complex_binary_precision(const complex_t *av,
                                              const complex_t *bv)
{
    size_t precision_bits = number_default_precision_bits;
    size_t candidate;

    if (av && av->precision_bits > precision_bits)
        precision_bits = av->precision_bits;
    if (bv && bv->precision_bits > precision_bits)
        precision_bits = bv->precision_bits;
    if (av) {
        candidate = num_get_prec_bits(av->real);
        if (candidate > precision_bits)
            precision_bits = candidate;
        candidate = num_get_prec_bits(av->imag);
        if (candidate > precision_bits)
            precision_bits = candidate;
    }
    if (bv) {
        candidate = num_get_prec_bits(bv->real);
        if (candidate > precision_bits)
            precision_bits = candidate;
        candidate = num_get_prec_bits(bv->imag);
        if (candidate > precision_bits)
            precision_bits = candidate;
    }
    return precision_bits;
}

static bool number_complex_has_inexact_component(const complex_t *value)
{
    return value && (!num_is_exact(value->real) || !num_is_exact(value->imag));
}

static bool number_complex_component_blocks_mpc_fast_path(number_t value)
{
    return num_is_immortal(value) && !num_eq(value, NUM_ZERO);
}

static bool number_complex_blocks_mpc_fast_path(const complex_t *value)
{
    return value &&
           (number_complex_component_blocks_mpc_fast_path(value->real) ||
            number_complex_component_blocks_mpc_fast_path(value->imag));
}

static number_t *number_complex_binary_mpc(const complex_t *av,
                                           const complex_t *bv,
                                           number_complex_mpc_binary_fn fn)
{
    size_t precision_bits;
    mpc_t a_mpc;
    mpc_t b_mpc;
    mpc_t out_mpc;
    number_t *out = NULL;

    if (!av || !bv || !fn ||
        number_complex_blocks_mpc_fast_path(av) ||
        number_complex_blocks_mpc_fast_path(bv) ||
        (!number_complex_has_inexact_component(av) &&
         !number_complex_has_inexact_component(bv)))
        return NULL;

    precision_bits = number_complex_binary_precision(av, bv);
    mpc_init2(a_mpc, (mpfr_prec_t)precision_bits);
    mpc_init2(b_mpc, (mpfr_prec_t)precision_bits);
    mpc_init2(out_mpc, (mpfr_prec_t)precision_bits);

    if (number_complex_get_mpc(a_mpc, av, precision_bits) != 0 ||
        number_complex_get_mpc(b_mpc, bv, precision_bits) != 0)
        goto cleanup;

    (void)fn(out_mpc, a_mpc, b_mpc, MPC_RNDNN);
    out = number_wrap_complex_mpc(out_mpc, precision_bits);

cleanup:
    mpc_clear(out_mpc);
    mpc_clear(b_mpc);
    mpc_clear(a_mpc);
    return out;
}

typedef struct {
    long n;
    long d;
} number_small_fraction_t;

static long number_long_abs_for_gcd(long value)
{
    if (value == LONG_MIN)
        return LONG_MAX;
    return value < 0 ? -value : value;
}

static long number_long_gcd(long a, long b)
{
    a = number_long_abs_for_gcd(a);
    b = number_long_abs_for_gcd(b);
    while (b != 0) {
        long r = a % b;

        a = b;
        b = r;
    }
    return a == 0 ? 1 : a;
}

static bool number_small_fraction_from_number(const number_t *value,
                                              number_small_fraction_t *out)
{
    long n;
    long d;

    if (!value || !out || !num_get_small_rational(*value, &n, &d) || d == 0)
        return false;
    if (d < 0) {
        if (n == LONG_MIN || d == LONG_MIN)
            return false;
        n = -n;
        d = -d;
    }
    {
        long gcd = number_long_gcd(n, d);

        out->n = n / gcd;
        out->d = d / gcd;
    }
    return true;
}

static bool number_small_fraction_normalize(__int128 n,
                                            __int128 d,
                                            number_small_fraction_t *out)
{
    long ln;
    long ld;
    long gcd;

    if (!out || d == 0 || n < LONG_MIN || n > LONG_MAX ||
        d < LONG_MIN || d > LONG_MAX)
        return false;
    ln = (long)n;
    ld = (long)d;
    if (ld < 0) {
        if (ln == LONG_MIN || ld == LONG_MIN)
            return false;
        ln = -ln;
        ld = -ld;
    }
    gcd = number_long_gcd(ln, ld);
    out->n = ln / gcd;
    out->d = ld / gcd;
    return true;
}

static bool number_small_fraction_add(number_small_fraction_t a,
                                      number_small_fraction_t b,
                                      number_small_fraction_t *out)
{
    return number_small_fraction_normalize(
        (__int128)a.n * b.d + (__int128)b.n * a.d,
        (__int128)a.d * b.d,
        out);
}

static bool number_small_fraction_sub(number_small_fraction_t a,
                                      number_small_fraction_t b,
                                      number_small_fraction_t *out)
{
    return number_small_fraction_normalize(
        (__int128)a.n * b.d - (__int128)b.n * a.d,
        (__int128)a.d * b.d,
        out);
}

static bool number_small_fraction_mul(number_small_fraction_t a,
                                      number_small_fraction_t b,
                                      number_small_fraction_t *out)
{
    return number_small_fraction_normalize(
        (__int128)a.n * b.n,
        (__int128)a.d * b.d,
        out);
}

static bool number_small_fraction_div(number_small_fraction_t a,
                                      number_small_fraction_t b,
                                      number_small_fraction_t *out)
{
    return number_small_fraction_normalize(
        (__int128)a.n * b.d,
        (__int128)a.d * b.n,
        out);
}

static number_t *number_wrap_small_fraction(number_small_fraction_t value)
{
    if (value.d == 1)
        return number_wrap_mpz(number_mpz_from_long(value.n));
    return number_wrap_mpq(number_mpq_from_frac_long(value.n, value.d));
}

static number_t *number_wrap_complex_small_fraction_parts(
    number_small_fraction_t real,
    number_small_fraction_t imag)
{
    number_t *real_box;
    number_t *imag_box;
    number_t real_number;
    number_t imag_number;
    number_t *out;

    if (imag.n == 0)
        return number_wrap_small_fraction(real);

    real_box = number_wrap_small_fraction(real);
    imag_box = number_wrap_small_fraction(imag);
    if (!real_box || !imag_box) {
        number_box_free(real_box);
        number_box_free(imag_box);
        return NULL;
    }
    real_number = number_take(real_box);
    imag_number = number_take(imag_box);

    out = number_wrap_complex_parts(real_number, imag_number);
    if (!out) {
        num_destroy(&real_number);
        num_destroy(&imag_number);
    }
    return out;
}

static bool number_complex_small_fraction_components(
    const complex_t *av,
    const complex_t *bv,
    number_small_fraction_t *ar,
    number_small_fraction_t *ai,
    number_small_fraction_t *br,
    number_small_fraction_t *bi)
{
    return av && bv &&
           number_small_fraction_from_number(&av->real, ar) &&
           number_small_fraction_from_number(&av->imag, ai) &&
           number_small_fraction_from_number(&bv->real, br) &&
           number_small_fraction_from_number(&bv->imag, bi);
}

static bool number_small_fraction_components_are_integers(
    number_small_fraction_t ar,
    number_small_fraction_t ai,
    number_small_fraction_t br,
    number_small_fraction_t bi)
{
    return ar.d == 1 && ai.d == 1 && br.d == 1 && bi.d == 1;
}

static bool number_small_integer_from_number(const number_t *value, long *out)
{
    if (!value || !out || number_kind_value(value) != NUMBER_MPZ)
        return false;
    return number_mpz_get_long(number_impl_const(value)->value.mpz, out);
}

static bool number_complex_small_integer_components(const complex_t *av,
                                                    const complex_t *bv,
                                                    long *ar,
                                                    long *ai,
                                                    long *br,
                                                    long *bi)
{
    return av && bv &&
           number_small_integer_from_number(&av->real, ar) &&
           number_small_integer_from_number(&av->imag, ai) &&
           number_small_integer_from_number(&bv->real, br) &&
           number_small_integer_from_number(&bv->imag, bi);
}

static number_t *number_mul_same_complex_small_integer_values(
    long ar,
    long ai,
    long br,
    long bi)
{
    number_small_fraction_t real;
    number_small_fraction_t imag;

    if (!number_small_fraction_normalize(
            (__int128)ar * br - (__int128)ai * bi,
            1,
            &real) ||
        !number_small_fraction_normalize(
            (__int128)ar * bi + (__int128)ai * br,
            1,
            &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static number_t *number_div_same_complex_small_integer_values(
    long ar,
    long ai,
    long br,
    long bi)
{
    __int128 denom = (__int128)br * br + (__int128)bi * bi;
    number_small_fraction_t real;
    number_small_fraction_t imag;

    if (!number_small_fraction_normalize(
            (__int128)ar * br + (__int128)ai * bi,
            denom,
            &real) ||
        !number_small_fraction_normalize(
            (__int128)ai * br - (__int128)ar * bi,
            denom,
            &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static number_t *number_mul_same_complex_small_integer_parts(
    number_small_fraction_t ar,
    number_small_fraction_t ai,
    number_small_fraction_t br,
    number_small_fraction_t bi)
{
    number_small_fraction_t real;
    number_small_fraction_t imag;

    if (!number_small_fraction_normalize(
            (__int128)ar.n * br.n - (__int128)ai.n * bi.n,
            1,
            &real) ||
        !number_small_fraction_normalize(
            (__int128)ar.n * bi.n + (__int128)ai.n * br.n,
            1,
            &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static number_t *number_div_same_complex_small_integer_parts(
    number_small_fraction_t ar,
    number_small_fraction_t ai,
    number_small_fraction_t br,
    number_small_fraction_t bi)
{
    __int128 denom = (__int128)br.n * br.n + (__int128)bi.n * bi.n;
    number_small_fraction_t real;
    number_small_fraction_t imag;

    if (!number_small_fraction_normalize(
            (__int128)ar.n * br.n + (__int128)ai.n * bi.n,
            denom,
            &real) ||
        !number_small_fraction_normalize(
            (__int128)ai.n * br.n - (__int128)ar.n * bi.n,
            denom,
            &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static number_t *number_mul_same_complex_small_fraction(const complex_t *av,
                                                        const complex_t *bv)
{
    number_small_fraction_t ar, ai, br, bi;
    number_small_fraction_t ac, bd, ad, bc;
    number_small_fraction_t real, imag;
    long iar, iai, ibr, ibi;

    if (number_complex_small_integer_components(av, bv, &iar, &iai, &ibr, &ibi))
        return number_mul_same_complex_small_integer_values(iar, iai, ibr, ibi);
    if (!number_complex_small_fraction_components(av, bv, &ar, &ai, &br, &bi))
        return NULL;
    if (number_small_fraction_components_are_integers(ar, ai, br, bi))
        return number_mul_same_complex_small_integer_parts(ar, ai, br, bi);
    if (!number_small_fraction_mul(ar, br, &ac) ||
        !number_small_fraction_mul(ai, bi, &bd) ||
        !number_small_fraction_mul(ar, bi, &ad) ||
        !number_small_fraction_mul(ai, br, &bc) ||
        !number_small_fraction_sub(ac, bd, &real) ||
        !number_small_fraction_add(ad, bc, &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static number_t *number_div_same_complex_small_fraction(const complex_t *av,
                                                        const complex_t *bv)
{
    number_small_fraction_t ar, ai, br, bi;
    number_small_fraction_t c2, d2, denom;
    number_small_fraction_t ac, bd, bc, ad;
    number_small_fraction_t real_num, imag_num;
    number_small_fraction_t real, imag;
    long iar, iai, ibr, ibi;

    if (number_complex_small_integer_components(av, bv, &iar, &iai, &ibr, &ibi))
        return number_div_same_complex_small_integer_values(iar, iai, ibr, ibi);
    if (!number_complex_small_fraction_components(av, bv, &ar, &ai, &br, &bi))
        return NULL;
    if (number_small_fraction_components_are_integers(ar, ai, br, bi))
        return number_div_same_complex_small_integer_parts(ar, ai, br, bi);
    if (!number_small_fraction_mul(br, br, &c2) ||
        !number_small_fraction_mul(bi, bi, &d2) ||
        !number_small_fraction_add(c2, d2, &denom) ||
        !number_small_fraction_mul(ar, br, &ac) ||
        !number_small_fraction_mul(ai, bi, &bd) ||
        !number_small_fraction_mul(ai, br, &bc) ||
        !number_small_fraction_mul(ar, bi, &ad) ||
        !number_small_fraction_add(ac, bd, &real_num) ||
        !number_small_fraction_sub(bc, ad, &imag_num) ||
        !number_small_fraction_div(real_num, denom, &real) ||
        !number_small_fraction_div(imag_num, denom, &imag))
        return NULL;
    return number_wrap_complex_small_fraction_parts(real, imag);
}

static bool number_complex_is_imag_unit(const complex_t *value, int *sign_out)
{
    if (!value || !num_is_zero(value->real))
        return false;
    if (num_eq(value->imag, NUM_ONE)) {
        if (sign_out)
            *sign_out = 1;
        return true;
    }
    if (num_eq(value->imag, NUM_NEG_ONE)) {
        if (sign_out)
            *sign_out = -1;
        return true;
    }
    return false;
}

static bool number_complex_is_real_value(const complex_t *value)
{
    return value && num_is_zero(value->imag);
}

static number_t *number_complex_wrap_imag_scaled(number_t magnitude, int sign)
{
    number_t real;
    number_t imag;

    real = num_clone(NUM_ZERO);
    imag = sign < 0 ? num_neg(magnitude) : num_clone(magnitude);
    return number_wrap_complex_parts(real, imag);
}

static number_t *number_mul_same_complex_imag_unit(const complex_t *av,
                                                   const complex_t *bv)
{
    int sign;

    if (number_complex_is_imag_unit(av, &sign) && number_complex_is_real_value(bv))
        return number_complex_wrap_imag_scaled(bv->real, sign);
    if (number_complex_is_imag_unit(bv, &sign) && number_complex_is_real_value(av))
        return number_complex_wrap_imag_scaled(av->real, sign);
    return NULL;
}

number_t *number_add_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    number_t real;
    number_t imag;

    if (!av || !bv)
        return NULL;
    real = num_add(av->real, bv->real);
    imag = num_add(av->imag, bv->imag);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_sub_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    number_t real;
    number_t imag;

    if (!av || !bv)
        return NULL;
    real = num_sub(av->real, bv->real);
    imag = num_sub(av->imag, bv->imag);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_mul_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    number_t ac;
    number_t bd;
    number_t ad;
    number_t bc;
    number_t real;
    number_t imag;

    if (!av || !bv)
        return NULL;
    {
        number_t *fast = number_mul_same_complex_imag_unit(av, bv);
        if (fast)
            return fast;
    }
    {
        number_t *fast = number_mul_same_complex_small_fraction(av, bv);
        if (fast)
            return fast;
    }
    {
        number_t *fast = number_complex_binary_mpc(av, bv, mpc_mul);
        if (fast)
            return fast;
    }
    ac = num_mul(av->real, bv->real);
    bd = num_mul(av->imag, bv->imag);
    ad = num_mul(av->real, bv->imag);
    bc = num_mul(av->imag, bv->real);
    real = num_sub(ac, bd);
    imag = num_add(ad, bc);
    num_destroy(&ac);
    num_destroy(&bd);
    num_destroy(&ad);
    num_destroy(&bc);
    return number_wrap_complex_parts(real, imag);
}

number_t *number_div_same_complex(const number_t *a, const number_t *b)
{
    const complex_t *av = number_complex_value(a);
    const complex_t *bv = number_complex_value(b);
    number_t c2;
    number_t d2;
    number_t denom;
    number_t ac;
    number_t bd;
    number_t bc;
    number_t ad;
    number_t real_num;
    number_t imag_num;
    number_t real;
    number_t imag;

    if (!av || !bv)
        return NULL;
    {
        number_t *fast = number_div_same_complex_small_fraction(av, bv);
        if (fast)
            return fast;
    }
    {
        number_t *fast = number_complex_binary_mpc(av, bv, mpc_div);
        if (fast)
            return fast;
    }
    c2 = num_mul(bv->real, bv->real);
    d2 = num_mul(bv->imag, bv->imag);
    denom = num_add(c2, d2);
    ac = num_mul(av->real, bv->real);
    bd = num_mul(av->imag, bv->imag);
    bc = num_mul(av->imag, bv->real);
    ad = num_mul(av->real, bv->imag);
    real_num = num_add(ac, bd);
    imag_num = num_sub(bc, ad);
    real = num_div(real_num, denom);
    imag = num_div(imag_num, denom);
    num_destroy(&c2);
    num_destroy(&d2);
    num_destroy(&denom);
    num_destroy(&ac);
    num_destroy(&bd);
    num_destroy(&bc);
    num_destroy(&ad);
    num_destroy(&real_num);
    num_destroy(&imag_num);
    return number_wrap_complex_parts(real, imag);
}

static number_t *number_apply_complex_mpc_unary(const number_t *number,
                                                int (*fn)(mpc_ptr, mpc_srcptr, mpc_rnd_t))
{
    const complex_t *value = number_complex_value(number);
    size_t precision_bits;
    mpc_t in;
    mpc_t out;
    number_t *boxed = NULL;

    if (!value || !fn)
        return NULL;
    precision_bits = num_get_prec_bits(*number);
    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    mpc_init2(in, (mpfr_prec_t)precision_bits);
    mpc_init2(out, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(in, value, precision_bits) != 0)
        goto done;
    (void)fn(out, in, MPC_RNDNN);
    boxed = number_wrap_complex_mpc(out, precision_bits);

done:
    mpc_clear(out);
    mpc_clear(in);
    return boxed;
}

static int number_mpc_sqrt_adapter(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return mpc_sqrt(out, in, rnd);
}

number_t *number_exp_same_complex(const number_t *number)
{
    return number_apply_complex_mpc_unary(number, mpc_exp);
}

number_t *number_log_same_complex(const number_t *number)
{
    return number_apply_complex_mpc_unary(number, mpc_log);
}

number_t *number_sqrt_same_complex(const number_t *number)
{
    return number_apply_complex_mpc_unary(number, number_mpc_sqrt_adapter);
}
