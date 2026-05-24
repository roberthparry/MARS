#include <math.h>
#include <stdlib.h>

#include "number.h"
#include "number_internal.h"

#include <stdio.h>

bool number_eq_same_tol_mpfr(const number_t *a, const number_t *b);

typedef int (*number_mpfr_const_set_fn)(mpfr_t out);

static int number_mpfr_set_ui(mpfr_t out, unsigned long value)
{
    mpfr_set_ui(out, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_set_si(mpfr_t out, long value)
{
    mpfr_set_si(out, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_set_pi_fraction(mpfr_t out,
                                       unsigned long numerator,
                                       unsigned long denominator,
                                       int sign)
{
    mpfr_const_pi(out, MPFR_RNDN);
    if (numerator != 1u)
        mpfr_mul_ui(out, out, numerator, MPFR_RNDN);
    if (denominator != 1u)
        mpfr_div_ui(out, out, denominator, MPFR_RNDN);
    if (sign < 0)
        mpfr_neg(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_set_sqrt_ui_over_ui(mpfr_t out,
                                           unsigned long radicand,
                                           unsigned long denominator)
{
    mpfr_set_ui(out, radicand, MPFR_RNDN);
    mpfr_sqrt(out, out, MPFR_RNDN);
    if (denominator != 1u)
        mpfr_div_ui(out, out, denominator, MPFR_RNDN);
    return 0;
}

#define NUMBER_MPFR_CONST_UI(name, value)                  \
    static int name(mpfr_t out) { return number_mpfr_set_ui(out, (value)); }

#define NUMBER_MPFR_CONST_SI(name, value)                  \
    static int name(mpfr_t out) { return number_mpfr_set_si(out, (value)); }

#define NUMBER_MPFR_CONST_PI(name, numerator, denominator, sign)       \
    static int name(mpfr_t out)                                       \
    {                                                                 \
        return number_mpfr_set_pi_fraction(out, (numerator),          \
                                           (denominator), (sign));    \
    }

NUMBER_MPFR_CONST_UI(number_mpfr_const_zero, 0u)
NUMBER_MPFR_CONST_UI(number_mpfr_const_one, 1u)
NUMBER_MPFR_CONST_SI(number_mpfr_const_neg_one, -1l)
NUMBER_MPFR_CONST_UI(number_mpfr_const_two, 2u)
NUMBER_MPFR_CONST_PI(number_mpfr_const_pi, 1u, 1u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_2pi, 2u, 1u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_pi_2, 1u, 2u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_neg_pi_2, 1u, 2u, -1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_pi_4, 1u, 4u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_3pi_4, 3u, 4u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_pi_6, 1u, 6u, 1)
NUMBER_MPFR_CONST_PI(number_mpfr_const_pi_3, 1u, 3u, 1)

static int number_mpfr_const_half(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 1u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_quarter(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 2u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_one_eighth(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 3u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_one_and_half(mpfr_t out)
{
    mpfr_set_ui(out, 3u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 1u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_one_third(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_ui(out, out, 3u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_one_sixth(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_ui(out, out, 6u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_one_tenth(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_ui(out, out, 10u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_2_pi(mpfr_t out)
{
    mpfr_const_pi(out, MPFR_RNDN);
    mpfr_ui_div(out, 2u, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_e(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_exp(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_inv_e(mpfr_t out)
{
    number_mpfr_const_e(out);
    mpfr_ui_div(out, 1u, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_ln2(mpfr_t out)
{
    mpfr_const_log2(out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_ln10(mpfr_t out)
{
    mpfr_set_ui(out, 10u, MPFR_RNDN);
    mpfr_log(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_invln2(mpfr_t out)
{
    number_mpfr_const_ln2(out);
    mpfr_ui_div(out, 1u, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_euler_mascheroni(mpfr_t out)
{
    mpfr_const_euler(out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_phi(mpfr_t out)
{
    mpfr_set_ui(out, 5u, MPFR_RNDN);
    mpfr_sqrt(out, out, MPFR_RNDN);
    mpfr_add_ui(out, out, 1u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 1u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_sqrt_half(mpfr_t out)
{
    mpfr_set_ui(out, 1u, MPFR_RNDN);
    mpfr_div_2ui(out, out, 1u, MPFR_RNDN);
    mpfr_sqrt(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_sqrt2(mpfr_t out)
{
    return number_mpfr_set_sqrt_ui_over_ui(out, 2u, 1u);
}

static int number_mpfr_const_sqrt3(mpfr_t out)
{
    return number_mpfr_set_sqrt_ui_over_ui(out, 3u, 1u);
}

static int number_mpfr_const_sqrt2_over_two(mpfr_t out)
{
    return number_mpfr_set_sqrt_ui_over_ui(out, 2u, 2u);
}

static int number_mpfr_const_sqrt3_over_two(mpfr_t out)
{
    return number_mpfr_set_sqrt_ui_over_ui(out, 3u, 2u);
}

static int number_mpfr_const_sqrt_2pi(mpfr_t out)
{
    number_mpfr_const_2pi(out);
    mpfr_sqrt(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_sqrt_pi(mpfr_t out)
{
    number_mpfr_const_pi(out);
    mpfr_sqrt(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_sqrt_pi_over_two(mpfr_t out)
{
    number_mpfr_const_sqrt_pi(out);
    mpfr_div_2ui(out, out, 1u, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_sqrt1onpi(mpfr_t out)
{
    number_mpfr_const_pi(out);
    mpfr_ui_div(out, 1u, out, MPFR_RNDN);
    mpfr_sqrt(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_2_sqrtpi(mpfr_t out)
{
    number_mpfr_const_sqrt_pi(out);
    mpfr_ui_div(out, 2u, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_neg_two_over_sqrt_pi(mpfr_t out)
{
    number_mpfr_const_2_sqrtpi(out);
    mpfr_neg(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_inv_sqrt_2pi(mpfr_t out)
{
    number_mpfr_const_sqrt_2pi(out);
    mpfr_ui_div(out, 1u, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_log_sqrt_2pi(mpfr_t out)
{
    number_mpfr_const_sqrt_2pi(out);
    mpfr_log(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_ln_2pi(mpfr_t out)
{
    number_mpfr_const_2pi(out);
    mpfr_log(out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_pi_squared(mpfr_t out)
{
    number_mpfr_const_pi(out);
    mpfr_mul(out, out, out, MPFR_RNDN);
    return 0;
}

static int number_mpfr_const_2pi_cubed(mpfr_t out)
{
    mpfr_t two_pi;

    mpfr_init2(two_pi, mpfr_get_prec(out));
    number_mpfr_const_2pi(two_pi);
    mpfr_mul(out, two_pi, two_pi, MPFR_RNDN);
    mpfr_mul(out, out, two_pi, MPFR_RNDN);
    mpfr_clear(two_pi);
    return 0;
}

static int number_mpfr_const_nan(mpfr_t out)
{
    mpfr_set_nan(out);
    return 0;
}

static int number_mpfr_const_inf(mpfr_t out)
{
    mpfr_set_inf(out, 1);
    return 0;
}

static int number_mpfr_const_ninf(mpfr_t out)
{
    mpfr_set_inf(out, -1);
    return 0;
}

static const number_mpfr_const_set_fn number_mpfr_const_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = number_mpfr_const_zero,
    [NUMBER_CONST_ONE] = number_mpfr_const_one,
    [NUMBER_CONST_NEG_ONE] = number_mpfr_const_neg_one,
    [NUMBER_CONST_HALF] = number_mpfr_const_half,
    [NUMBER_CONST_ONE_AND_HALF] = number_mpfr_const_one_and_half,
    [NUMBER_CONST_ONE_THIRD] = number_mpfr_const_one_third,
    [NUMBER_CONST_QUARTER] = number_mpfr_const_quarter,
    [NUMBER_CONST_ONE_SIXTH] = number_mpfr_const_one_sixth,
    [NUMBER_CONST_ONE_EIGHTH] = number_mpfr_const_one_eighth,
    [NUMBER_CONST_ONE_TENTH] = number_mpfr_const_one_tenth,
    [NUMBER_CONST_TWO] = number_mpfr_const_two,
    [NUMBER_CONST_PI] = number_mpfr_const_pi,
    [NUMBER_CONST_2PI] = number_mpfr_const_2pi,
    [NUMBER_CONST_PI_2] = number_mpfr_const_pi_2,
    [NUMBER_CONST_NEG_PI_2] = number_mpfr_const_neg_pi_2,
    [NUMBER_CONST_PI_4] = number_mpfr_const_pi_4,
    [NUMBER_CONST_3PI_4] = number_mpfr_const_3pi_4,
    [NUMBER_CONST_PI_6] = number_mpfr_const_pi_6,
    [NUMBER_CONST_PI_3] = number_mpfr_const_pi_3,
    [NUMBER_CONST_2_PI] = number_mpfr_const_2_pi,
    [NUMBER_CONST_E] = number_mpfr_const_e,
    [NUMBER_CONST_INV_E] = number_mpfr_const_inv_e,
    [NUMBER_CONST_LN2] = number_mpfr_const_ln2,
    [NUMBER_CONST_LN10] = number_mpfr_const_ln10,
    [NUMBER_CONST_INVLN2] = number_mpfr_const_invln2,
    [NUMBER_CONST_EULER_MASCHERONI] = number_mpfr_const_euler_mascheroni,
    [NUMBER_CONST_PHI] = number_mpfr_const_phi,
    [NUMBER_CONST_SQRT_HALF] = number_mpfr_const_sqrt_half,
    [NUMBER_CONST_SQRT2] = number_mpfr_const_sqrt2,
    [NUMBER_CONST_SQRT3] = number_mpfr_const_sqrt3,
    [NUMBER_CONST_SQRT2_OVER_TWO] = number_mpfr_const_sqrt2_over_two,
    [NUMBER_CONST_SQRT3_OVER_TWO] = number_mpfr_const_sqrt3_over_two,
    [NUMBER_CONST_SQRT_2PI] = number_mpfr_const_sqrt_2pi,
    [NUMBER_CONST_SQRT_PI] = number_mpfr_const_sqrt_pi,
    [NUMBER_CONST_SQRT_PI_OVER_TWO] = number_mpfr_const_sqrt_pi_over_two,
    [NUMBER_CONST_SQRT1ONPI] = number_mpfr_const_sqrt1onpi,
    [NUMBER_CONST_2_SQRTPI] = number_mpfr_const_2_sqrtpi,
    [NUMBER_CONST_NEG_TWO_OVER_SQRT_PI] = number_mpfr_const_neg_two_over_sqrt_pi,
    [NUMBER_CONST_INV_SQRT_2PI] = number_mpfr_const_inv_sqrt_2pi,
    [NUMBER_CONST_LOG_SQRT_2PI] = number_mpfr_const_log_sqrt_2pi,
    [NUMBER_CONST_LN_2PI] = number_mpfr_const_ln_2pi,
    [NUMBER_CONST_PI_SQUARED] = number_mpfr_const_pi_squared,
    [NUMBER_CONST_2PI_CUBED] = number_mpfr_const_2pi_cubed,
    [NUMBER_CONST_NAN] = number_mpfr_const_nan,
    [NUMBER_CONST_INF] = number_mpfr_const_inf,
    [NUMBER_CONST_NINF] = number_mpfr_const_ninf
};

static int number_mpfr_set_const_id(mpfr_t out, number_const_id_t id)
{
    number_mpfr_const_set_fn fn;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return -1;
    fn = number_mpfr_const_table[id];
    return fn ? fn(out) : -1;
}

number_mpfr_t *number_mpfr_new_prec(size_t precision_bits)
{
    number_mpfr_t *out;

    out = calloc(1u, sizeof(*out));
    if (!out)
        return NULL;
    out->constant_id = NUMBER_CONST_COUNT;
    mpfr_init2(out->value, (mpfr_prec_t)(precision_bits
        ? precision_bits : number_default_precision_bits));
    out->initialised = true;
    mpfr_set_zero(out->value, 0);
    return out;
}

int number_mpfr_ensure(const number_mpfr_t *value, size_t precision_bits)
{
    number_mpfr_t *mutable_value = (number_mpfr_t *)value;
    mpfr_prec_t precision;

    if (!mutable_value)
        return -1;
    precision = (mpfr_prec_t)(precision_bits ? precision_bits
                                             : number_default_precision_bits);
    if (!mutable_value->initialised) {
        mpfr_init2(mutable_value->value, precision);
        mutable_value->initialised = true;
    }
    if (precision_bits != 0u && mpfr_get_prec(mutable_value->value) < precision)
        mpfr_prec_round(mutable_value->value, precision, MPFR_RNDN);
    if (mutable_value->constant_id != NUMBER_CONST_COUNT)
        return number_mpfr_set_const_id(mutable_value->value,
                                        mutable_value->constant_id);
    return 0;
}

mpfr_srcptr number_mpfr_value(const number_mpfr_t *value)
{
    if (number_mpfr_ensure(value, 0u) != 0)
        return NULL;
    return value->value;
}

number_mpfr_t *number_mpfr_from_mpfr(mpfr_srcptr value, size_t precision_bits)
{
    number_mpfr_t *out;

    if (!value)
        return NULL;
    out = number_mpfr_new_prec(precision_bits ? precision_bits
                                              : (size_t)mpfr_get_prec(value));
    if (!out)
        return NULL;
    mpfr_set(out->value, value, MPFR_RNDN);
    return out;
}

number_mpfr_t *number_mpfr_from_double(double value, size_t precision_bits)
{
    number_mpfr_t *out;

    out = number_mpfr_new_prec(precision_bits);
    if (!out)
        return NULL;
    mpfr_set_d(out->value, value, MPFR_RNDN);
    return out;
}

number_mpfr_t *number_mpfr_from_qfloat(qfloat_t value, size_t precision_bits)
{
    number_mpfr_t *out;

    out = number_mpfr_new_prec(precision_bits);
    if (!out)
        return NULL;
    mpfr_set_d(out->value, value.hi, MPFR_RNDN);
    if (value.lo != 0.0)
        mpfr_add_d(out->value, out->value, value.lo, MPFR_RNDN);
    return out;
}

number_mpfr_t *number_mpfr_from_string(const char *text, size_t precision_bits)
{
    number_mpfr_t *out;
    char *end = NULL;

    if (!text)
        return NULL;
    out = number_mpfr_new_prec(precision_bits);
    if (!out)
        return NULL;
    mpfr_strtofr(out->value, text, &end, 0, MPFR_RNDN);
    if (end == text || (end && *end != '\0')) {
        number_mpfr_free(out);
        return NULL;
    }
    return out;
}

number_mpfr_t *number_mpfr_from_const_id(number_const_id_t id,
                                         size_t precision_bits)
{
    number_mpfr_t *out;

    out = number_mpfr_new_prec(precision_bits ? precision_bits
                                              : number_default_precision_bits);
    if (!out)
        return NULL;
    out->constant_id = id;
    if (number_mpfr_set_const_id(out->value, id) != 0) {
        number_mpfr_free(out);
        return NULL;
    }
    return out;
}

number_mpfr_t *number_mpfr_clone(const number_mpfr_t *value)
{
    size_t precision_bits;

    if (!value || number_mpfr_ensure(value, 0u) != 0)
        return NULL;
    precision_bits = (size_t)mpfr_get_prec(value->value);
    return number_mpfr_from_mpfr(value->value, precision_bits);
}

void number_mpfr_free(number_mpfr_t *value)
{
    if (!value || value->immortal)
        return;
    if (value->initialised)
        mpfr_clear(value->value);
    free(value);
}

void number_destroy_mpfr(number_t *number)
{
    if (!number || !number_impl(number)->value.mpfr)
        return;
    number_mpfr_free(number_impl(number)->value.mpfr);
}

void *number_scope_payload_mpfr(const number_t *number)
{
    return number ? number_impl_const(number)->value.mpfr : NULL;
}

void number_destroy_scope_mpfr(void *payload)
{
    number_mpfr_free((number_mpfr_t *)payload);
}

bool number_is_zero_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return value && mpfr_zero_p(value) != 0;
}

bool number_is_one_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return value && mpfr_cmp_ui(value, 1u) == 0;
}

bool number_eq_same_mpfr(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_mpfr(a, b);
}

bool number_eq_same_tol_mpfr(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return number_eq_same_tol_with_precision(a, b,
        (a && vt && vt->get_precision) ? vt->get_precision(a) : 0u);
}

bool number_is_finite_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return value && mpfr_number_p(value) != 0;
}

bool number_is_nan_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return !value || mpfr_nan_p(value) != 0;
}

bool number_is_inf_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return value && mpfr_inf_p(value) != 0;
}

int number_cmp_same_mpfr(const number_t *a, const number_t *b)
{
    mpfr_srcptr av = a ? number_mpfr_value(number_impl_const(a)->value.mpfr) : NULL;
    mpfr_srcptr bv = b ? number_mpfr_value(number_impl_const(b)->value.mpfr) : NULL;

    if (!av || !bv || mpfr_nan_p(av) || mpfr_nan_p(bv))
        return 0;
    return mpfr_cmp(av, bv);
}

int number_set_precision_mpfr(number_t *number, size_t precision_bits)
{
    number_mpfr_t *value = number ? number_impl(number)->value.mpfr : NULL;

    if (!value || precision_bits == 0u || value->immortal)
        return -1;
    if (number_mpfr_ensure(value, precision_bits) != 0)
        return -1;
    mpfr_prec_round(value->value, (mpfr_prec_t)precision_bits, MPFR_RNDN);
    return 0;
}

size_t number_get_precision_mpfr(const number_t *number)
{
    number_mpfr_t *value = number ? number_impl_const(number)->value.mpfr : NULL;

    if (!value || number_mpfr_ensure(value, 0u) != 0)
        return 0u;
    return (size_t)mpfr_get_prec(value->value);
}

long number_get_exponent2_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    if (!value || !mpfr_regular_p(value))
        return 0l;
    return (long)mpfr_get_exp(value) - 1l;
}

double number_to_double_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    return value ? mpfr_get_d(value, MPFR_RNDN) : NAN;
}

qfloat_t number_to_qfloat_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;
    mpfr_t residual;
    qfloat_t out;

    if (!value)
        return QF_NAN;
    out.hi = mpfr_get_d(value, MPFR_RNDN);
    mpfr_init2(residual, mpfr_get_prec(value));
    mpfr_set(residual, value, MPFR_RNDN);
    mpfr_sub_d(residual, residual, out.hi, MPFR_RNDN);
    out.lo = mpfr_get_d(residual, MPFR_RNDN);
    mpfr_clear(residual);
    return out;
}

bool number_is_integer_mpfr(const number_t *number)
{
    NUM_SCOPE(scope);
    number_t copy;
    number_t floored;
    bool rc;

    if (!number)
        return false;
    copy = num_clone(*number);
    floored = num_floor(copy);
    rc = num_eq(copy, floored);
    return rc;
}

size_t number_get_mantissa_bits_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;
    mpz_t z;
    size_t out;

    if (!value || !mpfr_number_p(value) || mpfr_zero_p(value))
        return 0u;
    mpz_init(z);
    (void)mpfr_get_z_2exp(z, value);
    if (mpz_sgn(z) < 0)
        mpz_neg(z, z);
    {
        mp_bitcnt_t trailing_zero_bits = mpz_scan1(z, 0);

        if (trailing_zero_bits != (mp_bitcnt_t)-1)
            mpz_tdiv_q_2exp(z, z, trailing_zero_bits);
    }
    out = (size_t)mpz_sizeinbase(z, 2);
    mpz_clear(z);
    return out;
}

bool number_get_mantissa_u64_mpfr(const number_t *number, uint64_t *out)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;
    mpz_t z;

    if (!value || !out || !mpfr_number_p(value) || mpfr_zero_p(value))
        return false;
    mpz_init(z);
    (void)mpfr_get_z_2exp(z, value);
    if (mpz_sgn(z) < 0)
        mpz_neg(z, z);
    {
        mp_bitcnt_t trailing_zero_bits = mpz_scan1(z, 0);

        if (trailing_zero_bits != (mp_bitcnt_t)-1)
            mpz_tdiv_q_2exp(z, z, trailing_zero_bits);
    }
    if (mpz_sizeinbase(z, 2) > 64u) {
        mpz_clear(z);
        return false;
    }
    *out = (uint64_t)mpz_get_ui(z);
    mpz_clear(z);
    return true;
}

int number_sign_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;

    if (!value || mpfr_zero_p(value))
        return 0;
    return mpfr_sgn(value) < 0 ? -1 : 1;
}

char *number_to_string_mpfr(const number_t *number)
{
    mpfr_srcptr value = number ? number_mpfr_value(number_impl_const(number)->value.mpfr) : NULL;
    int digits = number ? (int)num_get_prec_digits(*number) : 0;
    int needed;
    char *out;
    char fmt[32];

    if (!value)
        return NULL;
    if (mpfr_nan_p(value))
        return number_strdup("NAN");
    if (mpfr_inf_p(value))
        return number_strdup(mpfr_sgn(value) < 0 ? "-inf" : "inf");
    if (digits <= 0)
        digits = 17;
    snprintf(fmt, sizeof(fmt), "%%.%dRg", digits);
    needed = mpfr_snprintf(NULL, 0u, fmt, value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (mpfr_snprintf(out, (size_t)needed + 1u, fmt, value) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

number_t *number_clone_mpfr(const number_t *number)
{
    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    return number_wrap_mpfr(number_mpfr_clone(number_impl_const(number)->value.mpfr));
}

number_t *number_neg_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy) {
        return NULL;
    }
    mpfr_neg(copy->value, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_inv_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy) {
        return NULL;
    }
    mpfr_ui_div(copy->value, 1u, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_abs_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy) {
        return NULL;
    }
    mpfr_abs(copy->value, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_imag_mpfr_zero(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_new_prec(number_get_precision_mpfr(number));
    if (!copy)
        return NULL;
    mpfr_set_zero(copy->value, 0);
    return number_wrap_mpfr(copy);
}

number_t *number_floor_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy) {
        return NULL;
    }
    mpfr_floor(copy->value, copy->value);
    return number_wrap_mpfr(copy);
}

number_t *number_pow_int_mpfr(const number_t *number, int exponent)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    mpfr_pow_si(copy->value, copy->value, exponent, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_ldexp_mpfr(const number_t *number, int exponent2)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    mpfr_mul_2si(copy->value, copy->value, exponent2, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

int number_sincos_mpfr(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    number_mpfr_t *s = NULL;
    number_mpfr_t *c = NULL;

    if (!number || !sin_out || !cos_out)
        return -1;
    s = number_mpfr_new_prec(number_get_precision_mpfr(number));
    c = number_mpfr_new_prec(number_get_precision_mpfr(number));
    if (!s || !c || number_mpfr_ensure(number_impl_const(number)->value.mpfr, 0u) != 0) {
        number_mpfr_free(s);
        number_mpfr_free(c);
        return -1;
    }
    mpfr_sin_cos(s->value, c->value, number_impl_const(number)->value.mpfr->value, MPFR_RNDN);
    *sin_out = number_take(number_wrap_mpfr(s));
    *cos_out = number_take(number_wrap_mpfr(c));
    return 0;
}

int number_sinhcosh_mpfr(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    number_mpfr_t *s = NULL;
    number_mpfr_t *c = NULL;

    if (!number || !sinh_out || !cosh_out)
        return -1;
    s = number_mpfr_new_prec(number_get_precision_mpfr(number));
    c = number_mpfr_new_prec(number_get_precision_mpfr(number));
    if (!s || !c || number_mpfr_ensure(number_impl_const(number)->value.mpfr, 0u) != 0) {
        number_mpfr_free(s);
        number_mpfr_free(c);
        return -1;
    }
    mpfr_sinh_cosh(s->value, c->value, number_impl_const(number)->value.mpfr->value, MPFR_RNDN);
    *sinh_out = number_take(number_wrap_mpfr(s));
    *cosh_out = number_take(number_wrap_mpfr(c));
    return 0;
}

int number_sincos_real_mpfr(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    number_t real;
    int rc;

    if (!number)
        return -1;
    if (number_kind_value(number) == NUMBER_MPFR)
        return number_sincos_mpfr(number, sin_out, cos_out);
    real = num_as_inexact_real_prec(*number, num_get_prec_bits(*number));
    rc = number_sincos_mpfr(&real, sin_out, cos_out);
    num_destroy(&real);
    return rc;
}

int number_sinhcosh_real_mpfr(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    number_t real;
    int rc;

    if (!number)
        return -1;
    if (number_kind_value(number) == NUMBER_MPFR)
        return number_sinhcosh_mpfr(number, sinh_out, cosh_out);
    real = num_as_inexact_real_prec(*number, num_get_prec_bits(*number));
    rc = number_sinhcosh_mpfr(&real, sinh_out, cosh_out);
    num_destroy(&real);
    return rc;
}

number_t *number_mul_pow10_mpfr(const number_t *number, int exponent10)
{
    number_mpfr_t *copy;
    mpfr_t scale;
    unsigned int exponent;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    exponent = exponent10 < 0 ? (unsigned int)-exponent10 : (unsigned int)exponent10;
    mpfr_init2(scale, mpfr_get_prec(copy->value));
    mpfr_ui_pow_ui(scale, 10u, exponent, MPFR_RNDN);
    if (exponent10 < 0)
        mpfr_div(copy->value, copy->value, scale, MPFR_RNDN);
    else
        mpfr_mul(copy->value, copy->value, scale, MPFR_RNDN);
    mpfr_clear(scale);
    return number_wrap_mpfr(copy);
}

number_t *number_add_same_mpfr(const number_t *a, const number_t *b)
{
    number_mpfr_t *copy;

    if (!a || !b)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(number_impl_const(b)->value.mpfr, number_get_precision_mpfr(a)) != 0) {
        number_mpfr_free(copy);
        return NULL;
    }
    mpfr_add(copy->value, copy->value, number_impl_const(b)->value.mpfr->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_sub_same_mpfr(const number_t *a, const number_t *b)
{
    number_mpfr_t *copy;

    if (!a || !b)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(number_impl_const(b)->value.mpfr, number_get_precision_mpfr(a)) != 0) {
        number_mpfr_free(copy);
        return NULL;
    }
    mpfr_sub(copy->value, copy->value, number_impl_const(b)->value.mpfr->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_mul_same_mpfr(const number_t *a, const number_t *b)
{
    number_mpfr_t *copy;

    if (!a || !b)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(number_impl_const(b)->value.mpfr, number_get_precision_mpfr(a)) != 0) {
        number_mpfr_free(copy);
        return NULL;
    }
    mpfr_mul(copy->value, copy->value, number_impl_const(b)->value.mpfr->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_div_same_mpfr(const number_t *a, const number_t *b)
{
    number_mpfr_t *copy;

    if (!a || !b)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(number_impl_const(b)->value.mpfr, number_get_precision_mpfr(a)) != 0) {
        number_mpfr_free(copy);
        return NULL;
    }
    mpfr_div(copy->value, copy->value, number_impl_const(b)->value.mpfr->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_exp_same_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    mpfr_exp(copy->value, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_log_same_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    mpfr_log(copy->value, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_sqrt_same_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number || !number_impl_const(number)->value.mpfr)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    if (!copy)
        return NULL;
    mpfr_sqrt(copy->value, copy->value, MPFR_RNDN);
    return number_wrap_mpfr(copy);
}

number_t *number_const_prec_mpfr(const number_t *number, size_t precision_bits)
{
    number_mpfr_t *value = number ? number_impl_const(number)->value.mpfr : NULL;
    number_mpfr_t *out;

    if (!value)
        return NULL;
    if (value->constant_id != NUMBER_CONST_COUNT) {
        out = number_mpfr_new_prec(precision_bits);
        if (!out)
            return NULL;
        out->constant_id = value->constant_id;
        if (number_mpfr_set_const_id(out->value, value->constant_id) != 0) {
            number_mpfr_free(out);
            return NULL;
        }
        return number_wrap_mpfr(out);
    }
    if (number_mpfr_ensure(value, 0u) != 0)
        return NULL;
    return number_wrap_mpfr(number_mpfr_from_mpfr(value->value, precision_bits));
}
