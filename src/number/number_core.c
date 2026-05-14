#include "number.h"
#include "number_internal.h"
#include "internal/mint_internal.h"
#include "internal/mcomplex_internal.h"
#include "mfloat/mfloat_internal.h"
#include "mrational/mrational_internal.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_MCOMPLEX + 1] = {
    [NUMBER_MATH_INVALID] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_MREAL] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_MCOMPLEX] = NUMBER_MATH_INVALID
    },
    [NUMBER_MATH_QREAL] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_QREAL,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_MREAL] = NUMBER_MATH_MREAL,
        [NUMBER_MATH_MCOMPLEX] = NUMBER_MATH_MCOMPLEX
    },
    [NUMBER_MATH_QCOMPLEX] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_MREAL] = NUMBER_MATH_MCOMPLEX,
        [NUMBER_MATH_MCOMPLEX] = NUMBER_MATH_MCOMPLEX
    },
    [NUMBER_MATH_MREAL] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_MREAL,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_MCOMPLEX,
        [NUMBER_MATH_MREAL] = NUMBER_MATH_MREAL,
        [NUMBER_MATH_MCOMPLEX] = NUMBER_MATH_MCOMPLEX
    },
    [NUMBER_MATH_MCOMPLEX] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_MCOMPLEX,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_MCOMPLEX,
        [NUMBER_MATH_MREAL] = NUMBER_MATH_MCOMPLEX,
        [NUMBER_MATH_MCOMPLEX] = NUMBER_MATH_MCOMPLEX
    }
};

const number_kind_t number_math_family_target_kind_table[] = {
    [NUMBER_MATH_INVALID] = NUMBER_INVALID,
    [NUMBER_MATH_QREAL] = NUMBER_QFLOAT,
    [NUMBER_MATH_QCOMPLEX] = NUMBER_QCOMPLEX,
    [NUMBER_MATH_MREAL] = NUMBER_MFLOAT,
    [NUMBER_MATH_MCOMPLEX] = NUMBER_MCOMPLEX
};

size_t number_default_precision_bits = 1024u;

_Static_assert(sizeof(number_private_t) <= sizeof(number_t),
    "number_t public storage is too small for internal representation");
_Static_assert(_Alignof(number_private_t) <= _Alignof(number_t),
    "number_t public storage alignment is too small for internal representation");

static number_t *number_alloc(number_kind_t kind)
{
    number_t *number;

    number = calloc(1, sizeof(*number));
    if (!number)
        return NULL;
    number_impl(number)->kind = kind;
    return number;
}

number_t number_invalid(void)
{
    number_t number;

    memset(&number, 0, sizeof(number));
    number_impl(&number)->kind = NUMBER_INVALID;
    return number;
}

number_t number_take(number_t *boxed_number)
{
    number_t value;

    if (!boxed_number)
        return number_invalid();
    memcpy(&value, boxed_number, sizeof(value));
    free(boxed_number);
    return value;
}

static const char *number_skip_ws(const char *text)
{
    if (!text)
        return NULL;
    while (*text && isspace((unsigned char)*text))
        text++;
    return text;
}

static bool number_has_char_ci(const char *text, char needle)
{
    unsigned char want;

    if (!text)
        return false;
    want = (unsigned char)tolower((unsigned char)needle);
    for (; *text; ++text) {
        if ((unsigned char)tolower((unsigned char)*text) == want)
            return true;
    }
    return false;
}

static bool number_is_decimal_text(const char *text)
{
    return text && (strchr(text, '.') || strchr(text, 'e') || strchr(text, 'E') ||
        number_has_char_ci(text, 'n') || number_has_char_ci(text, 'f'));
}


number_t number_wrap_mfloat_borrowed(const mfloat_t *value)
{
    number_t *number;

    if (!value)
        return number_invalid();
    number = number_alloc(NUMBER_MFLOAT);
    if (!number)
        return number_invalid();
    number_impl(number)->value.mf = (mfloat_t *)value;
    return number_take(number);
}

number_t number_wrap_mfloat_with_precision(mfloat_t *value, size_t precision_bits)
{
    if (!value)
        return number_invalid();
    if (mf_set_precision(value, precision_bits) != 0) {
        mf_free(value);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(value));
}

number_t number_wrap_mcomplex_with_precision(mcomplex_t *value, size_t precision_bits)
{
    if (!value)
        return number_invalid();
    if (mc_set_precision(value, precision_bits) != 0) {
        mc_free(value);
        return number_invalid();
    }
    return number_take(number_wrap_mcomplex(value));
}

 int number_set_precision_noop(number_t *number, size_t precision_bits)
{
    (void)number;
    return precision_bits == 0u ? -1 : 0;
}

 size_t number_precision_fixed53(const number_t *number)
{
    (void)number;
    return 53u;
}

 size_t number_precision_fixed106(const number_t *number)
{
    (void)number;
    return 106u;
}

 size_t number_precision_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

void number_destroy_none(number_t *number)
{
    (void)number;
}

void number_destroy_mint(number_t *number)
{
    if (!number)
        return;
    if (mint_is_immortal(number_impl(number)->value.mi))
        return;
    if (number)
        mi_free(number_impl(number)->value.mi);
}

void number_destroy_mrational(number_t *number)
{
    if (!number)
        return;
    if (number_impl(number)->value.mr && number_impl(number)->value.mr->immortal)
        return;
    if (number)
        mr_free(number_impl(number)->value.mr);
}

void number_destroy_mfloat(number_t *number)
{
    if (!number || !number_impl(number)->value.mf)
        return;
    mf_free(number_impl(number)->value.mf);
}

void number_destroy_mcomplex(number_t *number)
{
    if (!number || !number_impl(number)->value.mc)
        return;
    if (mcomplex_is_immortal(number_impl(number)->value.mc))
        return;
    mc_free(number_impl(number)->value.mc);
}

 number_t number_const_like_double(const number_t *like, number_const_id_t id);
 number_t number_const_like_qfloat(const number_t *like, number_const_id_t id);
 number_t number_const_like_qcomplex(const number_t *like, number_const_id_t id);
 number_t number_const_like_mexact(const number_t *like, number_const_id_t id);
 number_t number_const_like_mfloat(const number_t *like, number_const_id_t id);
 number_t number_const_like_mcomplex(const number_t *like, number_const_id_t id);

 bool number_is_real_default(const number_t *number)
{
    return number != NULL;
}

 bool number_value_is_immortal_double(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_value_is_immortal_qfloat(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_value_is_immortal_qcomplex(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_value_is_immortal_mint(const number_t *number)
{
    return number && number_impl_const(number)->value.mi &&
        mint_is_immortal(number_impl_const(number)->value.mi);
}

 bool number_value_is_immortal_mrational(const number_t *number)
{
    return number && number_impl_const(number)->value.mr &&
        number_impl_const(number)->value.mr->immortal;
}

 bool number_value_is_immortal_mfloat(const number_t *number)
{
    return number && number_impl_const(number)->value.mf &&
        mfloat_is_immortal(number_impl_const(number)->value.mf);
}

 bool number_value_is_immortal_mcomplex(const number_t *number)
{
    return number && number_impl_const(number)->value.mc &&
        mcomplex_is_immortal(number_impl_const(number)->value.mc);
}

static bool number_value_is_immortal(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (!number || !number_is_valid_value(number) || !vt || !vt->is_immortal)
        return false;
    return vt->is_immortal(number);
}

 bool number_is_zero_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 0.0;
}

 bool number_is_zero_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ZERO);
}

 bool number_is_zero_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ZERO);
}

 bool number_is_zero_mint(const number_t *number)
{
    return number && mi_is_zero(number_impl_const(number)->value.mi);
}

 bool number_is_zero_mrational(const number_t *number)
{
    return number && mr_is_zero(number_impl_const(number)->value.mr);
}

 bool number_is_zero_mfloat(const number_t *number)
{
    return number && mf_is_zero(number_impl_const(number)->value.mf);
}

 bool number_is_zero_mcomplex(const number_t *number)
{
    return number && mc_is_zero(number_impl_const(number)->value.mc);
}

 bool number_is_one_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 1.0;
}

 bool number_is_one_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ONE);
}

 bool number_is_real_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO);
}

 bool number_is_one_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ONE);
}

 bool number_is_one_mint(const number_t *number)
{
    return number && number_impl_const(number)->value.mi &&
        mi_cmp(number_impl_const(number)->value.mi, MI_ONE) == 0;
}

 bool number_is_one_mrational(const number_t *number)
{
    if (!number || !number_impl_const(number)->value.mr)
        return false;
    return mr_is_integer(number_impl_const(number)->value.mr) &&
        mi_cmp(mr_numerator(number_impl_const(number)->value.mr), MI_ONE) == 0;
}

 bool number_is_one_mfloat(const number_t *number)
{
    return number && mf_eq(number_impl_const(number)->value.mf, MF_ONE);
}

 bool number_is_real_mcomplex(const number_t *number)
{
    return number && mf_is_zero(mc_imag(number_impl_const(number)->value.mc));
}

 bool number_is_one_mcomplex(const number_t *number)
{
    return number && mc_eq(number_impl_const(number)->value.mc, MC_ONE);
}

 bool number_eq_same_double(const number_t *a, const number_t *b)
{
    return a && b && number_impl_const(a)->value.d == number_impl_const(b)->value.d;
}

 bool number_eq_same_qfloat(const number_t *a, const number_t *b)
{
    return a && b &&
        qf_eq(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf);
}

 bool number_eq_same_qcomplex(const number_t *a, const number_t *b)
{
    return a && b &&
        qc_eq(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc);
}

 bool number_eq_same_mint(const number_t *a, const number_t *b)
{
    return a && b &&
        mi_cmp(number_impl_const(a)->value.mi, number_impl_const(b)->value.mi) == 0;
}

 bool number_eq_same_mrational(const number_t *a, const number_t *b)
{
    return a && b &&
        mr_eq(number_impl_const(a)->value.mr, number_impl_const(b)->value.mr);
}

 bool number_eq_same_mfloat(const number_t *a, const number_t *b)
{
    return a && b &&
        mf_eq(number_impl_const(a)->value.mf, number_impl_const(b)->value.mf);
}

 bool number_eq_same_mcomplex(const number_t *a, const number_t *b)
{
    return a && b &&
        mc_eq(number_impl_const(a)->value.mc, number_impl_const(b)->value.mc);
}

static bool number_eq_same_tol_with_precision(const number_t *a,
                                              const number_t *b,
                                              size_t precision_bits)
{
    number_t delta;
    number_t diff;
    number_t one;
    number_t tolerance;
    bool rc;

    if (!a || !b || precision_bits == 0u)
        return false;
    delta = num_sub(*a, *b);
    diff = num_abs(delta);
    one = number_create_exact_mfloat_long_prec(1, precision_bits);
    tolerance = num_ldexp(one, 4 - (int)precision_bits);
    rc = num_cmp(diff, tolerance) <= 0;
    num_destroy(&delta);
    num_destroy(&diff);
    num_destroy(&one);
    num_destroy(&tolerance);
    return rc;
}

 bool number_eq_same_tol_double(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 53u);
}

 bool number_eq_same_tol_qfloat(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 106u);
}

 bool number_eq_same_tol_qcomplex(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 106u);
}

 bool number_eq_same_tol_mint(const number_t *a, const number_t *b)
{
    return number_eq_same_mint(a, b);
}

 bool number_eq_same_tol_mrational(const number_t *a, const number_t *b)
{
    return number_eq_same_mrational(a, b);
}

 bool number_eq_same_tol_mfloat(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return number_eq_same_tol_with_precision(a, b,
        (a && vt && vt->get_precision) ? vt->get_precision(a) : 0u);
}

 bool number_eq_same_tol_mcomplex(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return number_eq_same_tol_with_precision(a, b,
        (a && vt && vt->get_precision) ? vt->get_precision(a) : 0u);
}

 bool number_is_finite_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d);
}

 bool number_is_finite_qfloat(const number_t *number)
{
    return number && !qf_isnan(number_impl_const(number)->value.qf) &&
        !qf_isinf(number_impl_const(number)->value.qf);
}

 bool number_is_finite_qcomplex(const number_t *number)
{
    return number && !qc_isnan(number_impl_const(number)->value.qc) &&
        !qc_isinf(number_impl_const(number)->value.qc);
}

 bool number_is_finite_exact(const number_t *number)
{
    return number != NULL;
}

 bool number_is_finite_mfloat(const number_t *number)
{
    return number && mf_is_finite(number_impl_const(number)->value.mf);
}

 bool number_is_finite_mcomplex(const number_t *number)
{
    return number && !mc_isnan(number_impl_const(number)->value.mc) &&
        !mc_isinf(number_impl_const(number)->value.mc) &&
        mf_is_finite(mc_real(number_impl_const(number)->value.mc)) &&
        mf_is_finite(mc_imag(number_impl_const(number)->value.mc));
}

 bool number_is_nan_double(const number_t *number)
{
    return !number || isnan(number_impl_const(number)->value.d);
}

 bool number_is_nan_qfloat(const number_t *number)
{
    return !number || qf_isnan(number_impl_const(number)->value.qf);
}

 bool number_is_nan_qcomplex(const number_t *number)
{
    return !number || qc_isnan(number_impl_const(number)->value.qc);
}

 bool number_is_nan_exact(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_is_nan_mfloat(const number_t *number)
{
    return !number || mf_is_nan(number_impl_const(number)->value.mf);
}

 bool number_is_nan_mcomplex(const number_t *number)
{
    return !number || mc_isnan(number_impl_const(number)->value.mc);
}

 bool number_is_inf_double(const number_t *number)
{
    return number && isinf(number_impl_const(number)->value.d);
}

 bool number_is_inf_qfloat(const number_t *number)
{
    return number && qf_isinf(number_impl_const(number)->value.qf);
}

 bool number_is_inf_qcomplex(const number_t *number)
{
    return number && qc_isinf(number_impl_const(number)->value.qc);
}

 bool number_is_inf_exact(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_is_inf_mfloat(const number_t *number)
{
    return number && mf_is_inf(number_impl_const(number)->value.mf);
}

 bool number_is_inf_mcomplex(const number_t *number)
{
    return number && mc_isinf(number_impl_const(number)->value.mc);
}

 int number_cmp_same_double(const number_t *a, const number_t *b)
{
    double av, bv;

    if (!a || !b)
        return 0;
    av = number_impl_const(a)->value.d;
    bv = number_impl_const(b)->value.d;
    return av < bv ? -1 : (av > bv ? 1 : 0);
}

 int number_cmp_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? qf_cmp(number_impl_const(a)->value.qf,
                             number_impl_const(b)->value.qf) : 0;
}

 int number_cmp_same_qcomplex(const number_t *a, const number_t *b)
{
    qcomplex_t left, right;
    int rc;

    if (!a || !b)
        return 0;
    left = number_impl_const(a)->value.qc;
    right = number_impl_const(b)->value.qc;
    rc = qf_cmp(qc_real(left), qc_real(right));
    return rc != 0 ? rc : qf_cmp(qc_imag(left), qc_imag(right));
}

 int number_cmp_same_mint(const number_t *a, const number_t *b)
{
    return (a && b) ? mi_cmp(number_impl_const(a)->value.mi,
                             number_impl_const(b)->value.mi) : 0;
}

 int number_cmp_same_mrational(const number_t *a, const number_t *b)
{
    return (a && b) ? mr_cmp(number_impl_const(a)->value.mr,
                             number_impl_const(b)->value.mr) : 0;
}

 int number_cmp_same_mfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? mf_cmp(number_impl_const(a)->value.mf,
                             number_impl_const(b)->value.mf) : 0;
}

 int number_cmp_same_mcomplex(const number_t *a, const number_t *b)
{
    int rc;

    if (!a || !b)
        return 0;
    rc = mf_cmp(mc_real(number_impl_const(a)->value.mc),
                mc_real(number_impl_const(b)->value.mc));
    return rc != 0 ? rc : mf_cmp(mc_imag(number_impl_const(a)->value.mc),
                                 mc_imag(number_impl_const(b)->value.mc));
}

 int number_set_precision_mfloat(number_t *number, size_t precision_bits)
{
    return number && number_impl(number)->value.mf ?
        mf_set_precision(number_impl(number)->value.mf, precision_bits) : -1;
}

 size_t number_get_precision_mfloat(const number_t *number)
{
    return number && number_impl_const(number)->value.mf ?
        mf_get_precision(number_impl_const(number)->value.mf) : 0u;
}

 int number_set_precision_mcomplex(number_t *number, size_t precision_bits)
{
    return number && number_impl(number)->value.mc ?
        mc_set_precision(number_impl(number)->value.mc, precision_bits) : -1;
}

 size_t number_get_precision_mcomplex(const number_t *number)
{
    return number && number_impl_const(number)->value.mc ?
        mc_get_precision(number_impl_const(number)->value.mc) : 0u;
}

 long number_get_exponent2_double(const number_t *number)
{
    int exp2;

    if (!number)
        return 0l;
    exp2 = ilogb(fabs(number_impl_const(number)->value.d));
    return exp2 == FP_ILOGB0 || exp2 == FP_ILOGBNAN ? 0l : (long)exp2;
}

 long number_get_exponent2_qfloat(const number_t *number)
{
    if (!number)
        return 0l;
    return qf_get_exponent2(number_impl_const(number)->value.qf);
}

 long number_get_exponent2_zero(const number_t *number)
{
    (void)number;
    return 0l;
}

 long number_get_exponent2_mint(const number_t *number)
{
    return number ? (long)mi_bit_length(number_impl_const(number)->value.mi) - 1l : 0l;
}

 long number_get_exponent2_mrational(const number_t *number)
{
    mint_t *num;
    mint_t *den;
    long exp2 = 0l;

    if (!number)
        return 0l;
    num = mi_clone(mr_numerator(number_impl_const(number)->value.mr));
    den = mi_clone(mr_denominator(number_impl_const(number)->value.mr));
    if (!num || !den || mi_abs(num) != 0 || mi_is_zero(num) || mi_is_zero(den)) {
        mi_free(num);
        mi_free(den);
        return 0l;
    }
    exp2 = (long)mi_bit_length(num) - (long)mi_bit_length(den);
    if (exp2 >= 0) {
        mint_t *scaled_den = mi_clone(den);
        if (scaled_den && mi_shl(scaled_den, exp2) == 0 && mi_cmp(num, scaled_den) < 0)
            --exp2;
        mi_free(scaled_den);
    }
    else {
        mint_t *scaled_num = mi_clone(num);
        long shift = -exp2;
        if (scaled_num && mi_shl(scaled_num, shift) == 0 && mi_cmp(scaled_num, den) < 0)
            --exp2;
        mi_free(scaled_num);
    }
    mi_free(num);
    mi_free(den);
    return exp2;
}

 long number_get_exponent2_mfloat(const number_t *number)
{
    return number ? mf_get_exponent2(number_impl_const(number)->value.mf) : 0l;
}

 double number_to_double_double(const number_t *number)
{
    return number ? number_impl_const(number)->value.d : NAN;
}

 double number_to_double_qfloat(const number_t *number)
{
    return number ? qf_to_double(number_impl_const(number)->value.qf) : NAN;
}

 double number_to_double_mfloat(const number_t *number)
{
    return number ? mf_to_double(number_impl_const(number)->value.mf) : NAN;
}

 qfloat_t number_to_qfloat_double(const number_t *number)
{
    return number ? qf_from_double(number_impl_const(number)->value.d) : QF_NAN;
}

 qfloat_t number_to_qfloat_qfloat(const number_t *number)
{
    return number ? number_impl_const(number)->value.qf : QF_NAN;
}

 qfloat_t number_to_qfloat_mfloat(const number_t *number)
{
    return number ? mf_to_qfloat(number_impl_const(number)->value.mf) : QF_NAN;
}

 bool number_is_integer_double(const number_t *number)
{
    double x;

    if (!number)
        return false;
    x = number_impl_const(number)->value.d;
    return isfinite(x) && floor(x) == x;
}

 bool number_is_integer_qfloat(const number_t *number)
{
    return number && qf_eq(qf_floor(number_impl_const(number)->value.qf),
                           number_impl_const(number)->value.qf);
}

 bool number_is_integer_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO) &&
        qf_eq(qf_floor(qc_real(number_impl_const(number)->value.qc)),
              qc_real(number_impl_const(number)->value.qc));
}

 bool number_is_integer_mint(const number_t *number)
{
    return number != NULL;
}

 bool number_is_integer_mrational(const number_t *number)
{
    return number && mr_is_integer(number_impl_const(number)->value.mr);
}

 bool number_is_integer_mfloat(const number_t *number)
{
    number_t copy;
    number_t floored;
    bool rc;

    if (!number)
        return false;
    copy = num_clone(*number);
    floored = num_floor(copy);
    rc = num_eq(copy, floored);
    num_destroy(&copy);
    num_destroy(&floored);
    return rc;
}

 bool number_is_integer_mcomplex(const number_t *number)
{
    number_t imag;
    number_t real;
    number_t floored;
    bool rc;

    if (!number)
        return false;
    imag = num_imag_part(*number);
    real = num_real_part(*number);
    floored = num_floor(real);
    rc = num_is_zero(imag) && num_eq(real, floored);
    num_destroy(&imag);
    num_destroy(&real);
    num_destroy(&floored);
    return rc;
}

 size_t number_get_mantissa_bits_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d) &&
            number_impl_const(number)->value.d != 0.0 ? 53u : 0u;
}

 size_t number_get_mantissa_bits_qfloat(const number_t *number)
{
    return number &&
            !qf_isnan(number_impl_const(number)->value.qf) &&
            !qf_isinf(number_impl_const(number)->value.qf) &&
            !qf_eq(number_impl_const(number)->value.qf, QF_ZERO) ? 106u : 0u;
}

 size_t number_get_mantissa_bits_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

 size_t number_get_mantissa_bits_mfloat(const number_t *number)
{
    return number ? mf_get_mantissa_bits(number_impl_const(number)->value.mf) : 0u;
}

 size_t number_get_mantissa_bits_mcomplex(const number_t *number)
{
    return number ? mf_get_mantissa_bits(mc_real(number_impl_const(number)->value.mc)) : 0u;
}

 bool number_get_mantissa_u64_false(const number_t *number, uint64_t *out)
{
    (void)number;
    (void)out;
    return false;
}

 bool number_get_mantissa_u64_mfloat(const number_t *number, uint64_t *out)
{
    return number && out &&
        mf_get_mantissa_u64(number_impl_const(number)->value.mf, out);
}

 bool number_get_mantissa_u64_mcomplex(const number_t *number, uint64_t *out)
{
    return number && out &&
        mf_get_mantissa_u64(mc_real(number_impl_const(number)->value.mc), out);
}

 int number_sign_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d < 0.0 ? -1 : 1;
}

 int number_sign_qfloat(const number_t *number)
{
    return number && qf_lt(number_impl_const(number)->value.qf, QF_ZERO) ? -1 : 1;
}

 int number_sign_zero(const number_t *number)
{
    (void)number;
    return 0;
}

 int number_sign_mint(const number_t *number)
{
    return number && mi_is_negative(number_impl_const(number)->value.mi) ? -1 : 1;
}

 int number_sign_mrational(const number_t *number)
{
    return number && mi_is_negative(mr_numerator(number_impl_const(number)->value.mr)) ? -1 : 1;
}

 int number_sign_mfloat(const number_t *number)
{
    return number ? mf_get_sign(number_impl_const(number)->value.mf) : 0;
}

char *number_to_string_double(const number_t *number)
{
    char buf[64];

    if (!number)
        return NULL;
    snprintf(buf, sizeof(buf), "%.17g", number_impl_const(number)->value.d);
    return number_strdup(buf);
}

char *number_to_string_qfloat(const number_t *number)
{
    int needed;
    char *out;

    if (!number)
        return NULL;
    needed = qf_sprintf(NULL, 0u, "%q", number_impl_const(number)->value.qf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (qf_sprintf(out, (size_t)needed + 1u, "%q", number_impl_const(number)->value.qf) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

char *number_to_string_qcomplex(const number_t *number)
{
    int needed;
    char *out;

    if (!number)
        return NULL;
    needed = qc_sprintf(NULL, 0u, "%z", number_impl_const(number)->value.qc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (qc_sprintf(out, (size_t)needed + 1u, "%z", number_impl_const(number)->value.qc) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

char *number_to_string_mint(const number_t *number)
{
    return number ? mi_to_string(number_impl_const(number)->value.mi) : NULL;
}

char *number_to_string_mrational(const number_t *number)
{
    return number ? mr_to_string(number_impl_const(number)->value.mr) : NULL;
}

char *number_to_string_mfloat(const number_t *number)
{
    int needed;
    char *out;

    if (!number)
        return NULL;
    needed = mf_sprintf(NULL, 0u, "%mf", number_impl_const(number)->value.mf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (mf_sprintf(out, (size_t)needed + 1u, "%mf", number_impl_const(number)->value.mf) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

char *number_to_string_mcomplex(const number_t *number)
{
    int needed;
    char *out;

    if (!number)
        return NULL;
    needed = mc_sprintf(NULL, 0u, "%mz", number_impl_const(number)->value.mc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (mc_sprintf(out, (size_t)needed + 1u, "%mz", number_impl_const(number)->value.mc) < 0) {
        free(out);
        return NULL;
    }
    return out;
}

number_t *number_clone_double(const number_t *number)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d) : NULL;
}

number_t *number_clone_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(number_impl_const(number)->value.qf) : NULL;
}

number_t *number_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

number_t *number_clone_mint(const number_t *number)
{
    mint_t *copy;

    if (!number || !number_impl_const(number)->value.mi)
        return NULL;
    if (mint_is_immortal(number_impl_const(number)->value.mi))
        return number_wrap_mint(number_impl_const(number)->value.mi);
    copy = mi_clone(number_impl_const(number)->value.mi);
    return copy ? number_wrap_mint(copy) : NULL;
}

number_t *number_clone_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number || !number_impl_const(number)->value.mr)
        return NULL;
    if (number_impl_const(number)->value.mr->immortal)
        return number_wrap_mrational(number_impl_const(number)->value.mr);
    copy = mr_clone(number_impl_const(number)->value.mr);
    return copy ? number_wrap_mrational(copy) : NULL;
}

number_t *number_clone_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    if (mfloat_is_immortal(number_impl_const(number)->value.mf))
        return number_wrap_mfloat(number_impl_const(number)->value.mf);
    copy = mf_clone(number_impl_const(number)->value.mf);
    return copy ? number_wrap_mfloat(copy) : NULL;
}

number_t *number_clone_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    if (mcomplex_is_immortal(number_impl_const(number)->value.mc))
        return number_wrap_mcomplex(number_impl_const(number)->value.mc);
    copy = mc_clone(number_impl_const(number)->value.mc);
    return copy ? number_wrap_mcomplex(copy) : NULL;
}

number_t *number_neg_double(const number_t *number)
{
    return number ? number_wrap_double(-number_impl_const(number)->value.d) : NULL;
}

number_t *number_neg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_neg(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_neg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_neg(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_neg_mint(const number_t *number)
{
    mint_t *copy;

    if (!number || !number_impl_const(number)->value.mi)
        return NULL;
    copy = mi_clone(number_impl_const(number)->value.mi);
    if (!copy || mi_neg(copy) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_neg_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number || !number_impl_const(number)->value.mr)
        return NULL;
    copy = mr_clone(number_impl_const(number)->value.mr);
    if (!copy || mr_neg(copy) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_neg_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_neg(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_neg_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_neg(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_inv_double(const number_t *number)
{
    return number ? number_wrap_double(1.0 / number_impl_const(number)->value.d) : NULL;
}

number_t *number_inv_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_div(QF_ONE, number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_inv_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_div(QC_ONE, number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_inv_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number || !number_impl_const(number)->value.mr)
        return NULL;
    copy = mr_clone(number_impl_const(number)->value.mr);
    if (!copy || mr_inv(copy) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_inv_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_inv(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_inv_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_inv(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_abs_double(const number_t *number)
{
    return number ? number_wrap_double(fabs(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_abs_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_abs(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_abs_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_abs(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_abs_mint(const number_t *number)
{
    mint_t *copy;

    if (!number)
        return NULL;
    copy = mi_clone(number_impl_const(number)->value.mi);
    if (!copy || mi_abs(copy) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_abs_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number)
        return NULL;
    copy = mr_clone(number_impl_const(number)->value.mr);
    if (!copy || mr_abs(copy) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_abs_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_abs(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_abs_mcomplex(const number_t *number)
{
    mfloat_t *real_copy;
    mfloat_t *imag_copy;

    if (!number)
        return NULL;

    real_copy = mf_clone(mc_real(number_impl_const(number)->value.mc));
    imag_copy = mf_clone(mc_imag(number_impl_const(number)->value.mc));
    if (!real_copy || !imag_copy || mf_hypot(real_copy, imag_copy) != 0) {
        mf_free(imag_copy);
        mf_free(real_copy);
        return NULL;
    }
    mf_free(imag_copy);
    return number_wrap_mfloat(real_copy);
}

number_t *number_conj_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_conj(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_conj_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_conj(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_real_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_real(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_real_mcomplex(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(mc_real(number_impl_const(number)->value.mc));
    return copy ? number_wrap_mfloat(copy) : NULL;
}

number_t *number_imag_double_zero(const number_t *number)
{
    (void)number;
    return number_wrap_double(0.0);
}

number_t *number_imag_qfloat_zero(const number_t *number)
{
    (void)number;
    return number_wrap_qfloat(QF_ZERO);
}

number_t *number_imag_mint_zero(const number_t *number)
{
    (void)number;
    return number_wrap_mint(mi_create_long(0L));
}

number_t *number_imag_mrational_zero(const number_t *number)
{
    (void)number;
    return number_wrap_mrational(mr_create_mints(MI_ZERO, MI_ONE));
}

number_t *number_imag_mfloat_zero(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(MF_ZERO);
    if (!copy || mf_set_precision(copy, number_get_precision_mfloat(number)) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_imag_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_imag(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_imag_mcomplex(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(mc_imag(number_impl_const(number)->value.mc));
    return copy ? number_wrap_mfloat(copy) : NULL;
}

number_t *number_arg_double(const number_t *number)
{
    return number ? number_wrap_double(atan2(0.0, number_impl_const(number)->value.d)) : NULL;
}

number_t *number_arg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_atan2(QF_ZERO, number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_arg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_arg(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_arg_mcomplex(const number_t *number)
{
    mfloat_t *arg;

    if (!number)
        return NULL;
    arg = mf_clone(mc_imag(number_impl_const(number)->value.mc));
    if (!arg || mf_atan2(arg, mc_real(number_impl_const(number)->value.mc)) != 0) {
        mf_free(arg);
        return NULL;
    }
    return number_wrap_mfloat(arg);
}

number_t *number_floor_double(const number_t *number)
{
    return number ? number_wrap_double(floor(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_floor_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_floor(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_floor_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_floor(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_floor_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_floor(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_floor_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_floor(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_pow_int_double(const number_t *number, int exponent)
{
    return number ? number_wrap_double(pow(number_impl_const(number)->value.d, (double)exponent)) : NULL;
}

number_t *number_pow_int_qfloat(const number_t *number, int exponent)
{
    return number ? number_wrap_qfloat(qf_pow_int(number_impl_const(number)->value.qf, exponent)) : NULL;
}

number_t *number_pow_int_qcomplex(const number_t *number, int exponent)
{
    return number ? number_wrap_qcomplex(qc_pow(number_impl_const(number)->value.qc,
        qc_make(qf_from_double((double)exponent), QF_ZERO))) : NULL;
}

number_t *number_pow_int_mint(const number_t *number, int exponent)
{
    mint_t *copy;

    if (!number || exponent < 0)
        return NULL;
    copy = mi_clone(number_impl_const(number)->value.mi);
    if (!copy || mi_pow(copy, (unsigned long)exponent) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_pow_int_mfloat(const number_t *number, int exponent)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_pow_int(copy, exponent) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_pow_int_mcomplex(const number_t *number, int exponent)
{
    mcomplex_t *copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_pow_int(copy, exponent) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_ldexp_double(const number_t *number, int exponent2)
{
    return number ? number_wrap_double(ldexp(number_impl_const(number)->value.d, exponent2)) : NULL;
}

number_t *number_ldexp_qfloat(const number_t *number, int exponent2)
{
    return number ? number_wrap_qfloat(qf_ldexp(number_impl_const(number)->value.qf, exponent2)) : NULL;
}

number_t *number_ldexp_qcomplex(const number_t *number, int exponent2)
{
    return number ? number_wrap_qcomplex(qc_ldexp(number_impl_const(number)->value.qc, exponent2)) : NULL;
}

number_t *number_ldexp_mfloat(const number_t *number, int exponent2)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_ldexp(copy, exponent2) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_ldexp_mcomplex(const number_t *number, int exponent2)
{
    mcomplex_t *copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_ldexp(copy, exponent2) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

 int number_sincos_double(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    *sin_out = number_take(number_wrap_double(sin(number_impl_const(number)->value.d)));
    *cos_out = number_take(number_wrap_double(cos(number_impl_const(number)->value.d)));
    return 0;
}

 int number_sincos_qfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    *sin_out = number_take(number_wrap_qfloat(qf_sin(number_impl_const(number)->value.qf)));
    *cos_out = number_take(number_wrap_qfloat(qf_cos(number_impl_const(number)->value.qf)));
    return 0;
}

 int number_sincos_mfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    mfloat_t *s = NULL;
    mfloat_t *c = NULL;
    int rc;

    if (!number || !sin_out || !cos_out)
        return -1;
    s = mf_new_prec(mf_get_precision(number_impl_const(number)->value.mf));
    c = mf_new_prec(mf_get_precision(number_impl_const(number)->value.mf));
    rc = (!s || !c || mf_sincos(number_impl_const(number)->value.mf, s, c) != 0) ? -1 : 0;
    if (rc != 0) {
        mf_free(s);
        mf_free(c);
        return -1;
    }
    *sin_out = number_take(number_wrap_mfloat(s));
    *cos_out = number_take(number_wrap_mfloat(c));
    return 0;
}

 int number_sinhcosh_double(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    if (!number || !sinh_out || !cosh_out)
        return -1;
    *sinh_out = number_take(number_wrap_double(sinh(number_impl_const(number)->value.d)));
    *cosh_out = number_take(number_wrap_double(cosh(number_impl_const(number)->value.d)));
    return 0;
}

 int number_sinhcosh_qfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    if (!number || !sinh_out || !cosh_out)
        return -1;
    *sinh_out = number_take(number_wrap_qfloat(qf_sinh(number_impl_const(number)->value.qf)));
    *cosh_out = number_take(number_wrap_qfloat(qf_cosh(number_impl_const(number)->value.qf)));
    return 0;
}

 int number_sinhcosh_mfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    mfloat_t *s = NULL;
    mfloat_t *c = NULL;
    int rc;

    if (!number || !sinh_out || !cosh_out)
        return -1;
    s = mf_new_prec(mf_get_precision(number_impl_const(number)->value.mf));
    c = mf_new_prec(mf_get_precision(number_impl_const(number)->value.mf));
    rc = (!s || !c || mf_sinhcosh(number_impl_const(number)->value.mf, s, c) != 0) ? -1 : 0;
    if (rc != 0) {
        mf_free(s);
        mf_free(c);
        return -1;
    }
    *sinh_out = number_take(number_wrap_mfloat(s));
    *cosh_out = number_take(number_wrap_mfloat(c));
    return 0;
}

static int number_pair_real_mfloat(const number_t *number,
                                   number_t *first_out,
                                   number_t *second_out,
                                   int (*fn)(const mfloat_t *, mfloat_t *, mfloat_t *))
{
    number_t *tmp = NULL;
    mfloat_t *first = NULL;
    mfloat_t *second = NULL;
    size_t precision;
    int rc;

    if (!number || !first_out || !second_out || !fn)
        return -1;
    tmp = number_coerce(number, NUMBER_MFLOAT);
    if (!tmp)
        return -1;
    precision = mf_get_precision(number_impl_const(tmp)->value.mf);
    first = mf_new_prec(precision);
    second = mf_new_prec(precision);
    rc = (!first || !second || fn(number_impl_const(tmp)->value.mf, first, second) != 0) ? -1 : 0;
    number_box_free(tmp);
    if (rc != 0) {
        mf_free(first);
        mf_free(second);
        return -1;
    }
    *first_out = number_take(number_wrap_mfloat(first));
    *second_out = number_take(number_wrap_mfloat(second));
    return 0;
}

 int number_sincos_real_mfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    return number_pair_real_mfloat(number, sin_out, cos_out, mf_sincos);
}

 int number_sinhcosh_real_mfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    return number_pair_real_mfloat(number, sinh_out, cosh_out, mf_sinhcosh);
}

number_t *number_mul_pow10_double(const number_t *number, int exponent10)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d * pow(10.0, (double)exponent10)) : NULL;
}

number_t *number_mul_pow10_qfloat(const number_t *number, int exponent10)
{
    return number ? number_wrap_qfloat(qf_mul_pow10(number_impl_const(number)->value.qf, exponent10)) : NULL;
}

number_t *number_mul_pow10_mfloat(const number_t *number, int exponent10)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_mul_pow10(copy, exponent10) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_add_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d + number_impl_const(b)->value.d) : NULL;
}

number_t *number_sub_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d - number_impl_const(b)->value.d) : NULL;
}

number_t *number_mul_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d * number_impl_const(b)->value.d) : NULL;
}

number_t *number_div_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d / number_impl_const(b)->value.d) : NULL;
}

number_t *number_add_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_add(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

number_t *number_sub_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_sub(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

number_t *number_mul_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_mul(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

number_t *number_div_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_div(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

number_t *number_add_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_add(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

number_t *number_sub_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_sub(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

number_t *number_mul_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_mul(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

number_t *number_div_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_div(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

number_t *number_add_same_mint(const number_t *a, const number_t *b)
{
    mint_t *copy;

    if (!a || !b)
        return NULL;
    copy = mi_clone(number_impl_const(a)->value.mi);
    if (!copy || mi_add(copy, number_impl_const(b)->value.mi) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_sub_same_mint(const number_t *a, const number_t *b)
{
    mint_t *copy;

    if (!a || !b)
        return NULL;
    copy = mi_clone(number_impl_const(a)->value.mi);
    if (!copy || mi_sub(copy, number_impl_const(b)->value.mi) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_mul_same_mint(const number_t *a, const number_t *b)
{
    mint_t *copy;

    if (!a || !b)
        return NULL;
    copy = mi_clone(number_impl_const(a)->value.mi);
    if (!copy || mi_mul(copy, number_impl_const(b)->value.mi) != 0) {
        mi_free(copy);
        return NULL;
    }
    return number_wrap_mint(copy);
}

number_t *number_add_same_mrational(const number_t *a, const number_t *b)
{
    mrational_t *copy;

    if (!a || !b)
        return NULL;
    copy = mr_clone(number_impl_const(a)->value.mr);
    if (!copy || mr_add(copy, number_impl_const(b)->value.mr) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_sub_same_mrational(const number_t *a, const number_t *b)
{
    mrational_t *copy;

    if (!a || !b)
        return NULL;
    copy = mr_clone(number_impl_const(a)->value.mr);
    if (!copy || mr_sub(copy, number_impl_const(b)->value.mr) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_mul_same_mrational(const number_t *a, const number_t *b)
{
    mrational_t *copy;

    if (!a || !b)
        return NULL;
    copy = mr_clone(number_impl_const(a)->value.mr);
    if (!copy || mr_mul(copy, number_impl_const(b)->value.mr) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_div_same_mrational(const number_t *a, const number_t *b)
{
    mrational_t *copy;

    if (!a || !b)
        return NULL;
    copy = mr_clone(number_impl_const(a)->value.mr);
    if (!copy || mr_div(copy, number_impl_const(b)->value.mr) != 0) {
        mr_free(copy);
        return NULL;
    }
    return number_wrap_mrational(copy);
}

number_t *number_add_same_mfloat(const number_t *a, const number_t *b)
{
    mfloat_t *copy;

    if (!a || !b)
        return NULL;
    copy = mf_clone(number_impl_const(a)->value.mf);
    if (!copy || mf_add(copy, number_impl_const(b)->value.mf) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_sub_same_mfloat(const number_t *a, const number_t *b)
{
    mfloat_t *copy;

    if (!a || !b)
        return NULL;
    copy = mf_clone(number_impl_const(a)->value.mf);
    if (!copy || mf_sub(copy, number_impl_const(b)->value.mf) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_mul_same_mfloat(const number_t *a, const number_t *b)
{
    mfloat_t *copy;

    if (!a || !b)
        return NULL;
    copy = mf_clone(number_impl_const(a)->value.mf);
    if (!copy || mf_mul(copy, number_impl_const(b)->value.mf) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_div_same_mfloat(const number_t *a, const number_t *b)
{
    mfloat_t *copy;

    if (!a || !b)
        return NULL;
    copy = mf_clone(number_impl_const(a)->value.mf);
    if (!copy || mf_div(copy, number_impl_const(b)->value.mf) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_add_same_mcomplex(const number_t *a, const number_t *b)
{
    mcomplex_t *copy;

    if (!a || !b)
        return NULL;
    copy = mc_clone(number_impl_const(a)->value.mc);
    if (!copy || mc_add(copy, number_impl_const(b)->value.mc) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_sub_same_mcomplex(const number_t *a, const number_t *b)
{
    mcomplex_t *copy;

    if (!a || !b)
        return NULL;
    copy = mc_clone(number_impl_const(a)->value.mc);
    if (!copy || mc_sub(copy, number_impl_const(b)->value.mc) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_mul_same_mcomplex(const number_t *a, const number_t *b)
{
    mcomplex_t *copy;

    if (!a || !b)
        return NULL;
    copy = mc_clone(number_impl_const(a)->value.mc);
    if (!copy || mc_mul(copy, number_impl_const(b)->value.mc) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_div_same_mcomplex(const number_t *a, const number_t *b)
{
    mcomplex_t *copy;

    if (!a || !b)
        return NULL;
    copy = mc_clone(number_impl_const(a)->value.mc);
    if (!copy || mc_div(copy, number_impl_const(b)->value.mc) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_exp_same_double(const number_t *number)
{
    return number ? number_wrap_double(exp(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_log_same_double(const number_t *number)
{
    return number ? number_wrap_double(log(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_sqrt_same_double(const number_t *number)
{
    return number ? number_wrap_double(sqrt(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_exp_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_exp(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_log_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_log(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_sqrt_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_sqrt(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_exp_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_exp(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_log_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_log(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_sqrt_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_sqrt(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_exp_same_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_exp(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_log_same_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_log(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_sqrt_same_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    if (!copy || mf_sqrt(copy) != 0) {
        mf_free(copy);
        return NULL;
    }
    return number_wrap_mfloat(copy);
}

number_t *number_exp_same_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_exp(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_log_same_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_log(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

number_t *number_sqrt_same_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_sqrt(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    return number_wrap_mcomplex(copy);
}

void number_box_free(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = number_vt(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    free(number);
}

number_t *number_wrap_double(double value)
{
    number_t *number = number_alloc(NUMBER_DOUBLE);

    if (number)
        number_impl(number)->value.d = value;
    return number;
}

number_t *number_wrap_qfloat(qfloat_t value)
{
    number_t *number = number_alloc(NUMBER_QFLOAT);

    if (number)
        number_impl(number)->value.qf = value;
    return number;
}

number_t *number_wrap_qcomplex(qcomplex_t value)
{
    number_t *number = number_alloc(NUMBER_QCOMPLEX);

    if (number)
        number_impl(number)->value.qc = value;
    return number;
}

number_t *number_wrap_mint(mint_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MINT);
    if (!number) {
        mi_free(value);
        return NULL;
    }
    number_impl(number)->value.mi = value;
    return number;
}

number_t *number_wrap_mrational(mrational_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MRATIONAL);
    if (!number) {
        mr_free(value);
        return NULL;
    }
    number_impl(number)->value.mr = value;
    return number;
}

number_t *number_wrap_mfloat(mfloat_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MFLOAT);
    if (!number) {
        mf_free(value);
        return NULL;
    }
    number_impl(number)->value.mf = value;
    return number;
}

number_t *number_wrap_mcomplex(mcomplex_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MCOMPLEX);
    if (!number) {
        mc_free(value);
        return NULL;
    }
    number_impl(number)->value.mc = value;
    return number;
}

typedef number_t *(*number_binary_dispatch_fn)(const number_vtable_t *vt,
                                               const number_t *a,
                                               const number_t *b);

static number_t *number_apply_add_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->add_same ? vt->add_same(a, b) : NULL;
}

static number_t *number_apply_sub_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->sub_same ? vt->sub_same(a, b) : NULL;
}

static number_t *number_apply_mul_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->mul_same ? vt->mul_same(a, b) : NULL;
}

static number_t *number_apply_div_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->div_same ? vt->div_same(a, b) : NULL;
}

static const number_binary_dispatch_fn number_binary_dispatch[] = {
    [NUMBER_OP_ADD] = number_apply_add_same_kind,
    [NUMBER_OP_SUB] = number_apply_sub_same_kind,
    [NUMBER_OP_MUL] = number_apply_mul_same_kind,
    [NUMBER_OP_DIV] = number_apply_div_same_kind
};

static number_t *number_apply_binary_same_kind(const number_t *a,
                                               const number_t *b,
                                               number_binary_op_t op)
{
    const number_vtable_t *vt;
    number_binary_dispatch_fn fn;

    if (!a || !b || (size_t)op >= (sizeof(number_binary_dispatch) / sizeof(number_binary_dispatch[0])))
        return NULL;
    vt = number_vt(a);
    fn = number_binary_dispatch[op];
    return fn ? fn(vt, a, b) : NULL;
}

static number_t *number_apply_binary_generic(const number_t *a,
                                             const number_t *b,
                                             number_binary_op_t op)
{
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    number_t *result = NULL;

    if (!a || !b)
        return NULL;
    kind = number_common_kind(a, b, op);
    lhs = number_coerce(a, kind);
    rhs = number_coerce(b, kind);
    if (!lhs || !rhs || !number_same_kind_value(lhs, rhs))
        goto done;
    result = number_apply_binary_same_kind(lhs, rhs, op);

done:
    number_box_free(lhs);
    number_box_free(rhs);
    return result;
}

number_t num_create_from_double(double value)
{
    return number_take(number_wrap_double(value));
}

number_t num_create_from_qfloat(qfloat_t value)
{
    return number_take(number_wrap_qfloat(value));
}

number_t num_create_from_qcomplex(qcomplex_t value)
{
    return number_take(number_wrap_qcomplex(value));
}

number_t num_create_from_mint(const mint_t *value)
{
    return value ? number_take(number_wrap_mint(mi_clone(value))) : number_invalid();
}

number_t num_create_from_mrational(const mrational_t *value)
{
    return value ? number_take(number_wrap_mrational(mr_clone(value))) : number_invalid();
}

number_t num_create_from_mfloat(const mfloat_t *value)
{
    return value ? number_wrap_mfloat_with_precision(mf_clone(value),
        number_default_precision_bits) : number_invalid();
}

number_t num_create_from_mfloat_with_prec_bits(const mfloat_t *value, size_t precision_bits)
{
    mfloat_t *copy;

    if (!value)
        return number_invalid();
    copy = mf_clone(value);
    if (!copy)
        return number_invalid();
    if (mf_set_precision(copy, precision_bits) != 0) {
        mf_free(copy);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(copy));
}

number_t num_create_from_mfloat_with_prec_digits(const mfloat_t *value, size_t significant_digits)
{
    mfloat_t *copy;

    if (!value)
        return number_invalid();
    copy = mf_clone(value);
    if (!copy)
        return number_invalid();
    if (mf_set_precision_digits(copy, significant_digits) != 0) {
        mf_free(copy);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(copy));
}

number_t num_create_from_mcomplex(const mcomplex_t *value)
{
    return value ? number_wrap_mcomplex_with_precision(mc_clone(value),
        number_default_precision_bits) : number_invalid();
}

number_t num_create_from_mcomplex_with_prec_bits(const mcomplex_t *value, size_t precision_bits)
{
    mcomplex_t *copy;

    if (!value)
        return number_invalid();
    copy = mc_clone(value);
    if (!copy)
        return number_invalid();
    if (mc_set_precision(copy, precision_bits) != 0) {
        mc_free(copy);
        return number_invalid();
    }
    return number_take(number_wrap_mcomplex(copy));
}

number_t num_create_from_mcomplex_with_prec_digits(const mcomplex_t *value, size_t significant_digits)
{
    mcomplex_t *copy;

    if (!value)
        return number_invalid();
    copy = mc_clone(value);
    if (!copy)
        return number_invalid();
    if (mc_set_precision_digits(copy, significant_digits) != 0) {
        mc_free(copy);
        return number_invalid();
    }
    return number_take(number_wrap_mcomplex(copy));
}

number_t num_create_from_string(const char *text)
{
    const char *trimmed = number_skip_ws(text);

    if (!trimmed || *trimmed == '\0')
        return number_invalid();
    if (number_has_char_ci(trimmed, 'i'))
        return number_wrap_mcomplex_with_precision(mc_create_string(trimmed),
            number_default_precision_bits);
    if (strchr(trimmed, '/'))
        return number_take(number_wrap_mrational(mr_create_string(trimmed)));
    if (number_is_decimal_text(trimmed))
        return number_wrap_mfloat_with_precision(mf_create_string(trimmed),
            number_default_precision_bits);
    return number_take(number_wrap_mint(mi_create_string(trimmed)));
}

number_t num_clone(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt ? number_take(vt->clone(&number)) : number_invalid();
}

bool num_is_immortal(number_t number)
{
    return number_value_is_immortal(&number);
}

void num_destroy(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = number_vt(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    memset(number, 0, sizeof(*number));
    number_impl(number)->kind = NUMBER_INVALID;
}

bool num_is_exact(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->exact;
}

bool num_is_real(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_real && vt->is_real(&number);
}

bool num_is_zero(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_zero && vt->is_zero(&number);
}

bool num_is_one(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_one && vt->is_one(&number);
}

short num_get_sign(const number_t number)
{
    return (short)num_sign(number);
}

long num_get_exponent2(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number) || num_is_zero(number) ||
        !num_is_finite(number) || !num_is_real(number))
        return 0l;
    return vt && vt->get_exponent2 ? vt->get_exponent2(&number) : 0l;
}

int num_set_prec_bits(number_t *number, size_t precision_bits)
{
    const number_vtable_t *vt = number_vt(number);

    return vt && vt->set_precision ? vt->set_precision(number, precision_bits) : -1;
}

size_t num_get_prec_bits(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->get_precision ? vt->get_precision(&number) : 0u;
}

char *num_to_string(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->to_string ? vt->to_string(&number) : NULL;
}

bool num_eq(const number_t a, const number_t b)
{
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    bool eq = false;

    if (!number_is_valid_value(&a) || !number_is_valid_value(&b))
        return false;
    if (number_same_kind_value(&a, &b)) {
        const number_vtable_t *vt = number_vt(&a);
        if (vt && vt->eq_same)
            return vt->eq_same(&a, &b);
    }

    kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (!lhs || !rhs || !number_same_kind_value(lhs, rhs))
        goto done;
    const number_vtable_t *vt = number_vt(lhs);
    if (!vt || !vt->eq_same)
        goto done;
    eq = vt->eq_same(lhs, rhs);

done:
    number_box_free(lhs);
    number_box_free(rhs);
    return eq;
}

number_t num_neg(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->neg ? number_take(vt->neg(&number)) : number_invalid();
}

number_t num_inv(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!vt)
        return number_invalid();
    if (vt->inv)
        return number_take(vt->inv(&number));
    if (number_kind_value(&number) == NUMBER_MINT) {
        mrational_t *value = mr_create_mints(MI_ONE, number_impl_const(&number)->value.mi);

        return value ? number_take(number_wrap_mrational(value)) : number_invalid();
    }
    return number_invalid();
}

number_t num_add(const number_t a, const number_t b)
{
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_ADD));
}

number_t num_sub(const number_t a, const number_t b)
{
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_SUB));
}

number_t num_mul(const number_t a, const number_t b)
{
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_MUL));
}

number_t num_div(const number_t a, const number_t b)
{
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_DIV));
}

static size_t number_bits_to_digits(size_t precision_bits)
{
    if (precision_bits == 0u)
        return 0u;
    return (size_t)floor((double)precision_bits * 0.3010299956639812);
}

static size_t number_digits_to_bits(size_t significant_digits)
{
    if (significant_digits == 0u)
        return 0u;
    return (size_t)ceil((double)significant_digits * 3.3219280948873623);
}

void number_assign(number_t *dst, number_t value)
{
    if (!dst) {
        num_destroy(&value);
        return;
    }
    num_destroy(dst);
    *dst = value;
}

static inline bool number_is_finite_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) &&
        vt && vt->is_finite && vt->is_finite(number);
}

static inline bool number_is_nan_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return !number || !number_is_valid_value(number) ||
        (vt && vt->is_nan && vt->is_nan(number));
}

static inline bool number_is_inf_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) &&
        vt && vt->is_inf && vt->is_inf(number);
}

static inline int number_cmp_same_kind(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return a && b && vt && vt->cmp_same ? vt->cmp_same(a, b) : 0;
}

static inline bool number_eq_same_kind(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return a && b && vt && vt->eq_same ? vt->eq_same(a, b) : false;
}

bool number_matches_value(const number_t *reference, const number_t *target)
{
    const number_vtable_t *reference_vt;
    const number_vtable_t *target_vt;
    number_t *coerced_target;
    size_t precision_bits;
    bool matches;

    if (!reference || !target || !number_is_valid_value(reference) ||
        !number_is_valid_value(target))
        return false;
    reference_vt = number_vt(reference);
    if (!reference_vt)
        return false;
    coerced_target = number_coerce(target, number_impl_const(reference)->kind);
    if (!coerced_target)
        return false;
    target_vt = number_vt(coerced_target);
    precision_bits = reference_vt->get_precision
        ? reference_vt->get_precision(reference)
        : 0u;
    if (precision_bits != 0u && target_vt && target_vt->set_precision)
        target_vt->set_precision(coerced_target, precision_bits);
    matches = reference_vt->eq_same_tol
        ? reference_vt->eq_same_tol(reference, coerced_target)
        : number_eq_same_kind(reference, coerced_target);
    number_box_free(coerced_target);
    return matches;
}

static number_t number_const_mfloat_special(number_const_id_t id, size_t precision_bits)
{
    if (id == NUMBER_CONST_NEG_ONE)
        return number_create_exact_mfloat_long_prec(-1, precision_bits);
    if (number_const_has_ldexp(id))
        return number_create_exact_mfloat_dyadic_prec(
            1, number_const_ldexp_value(id), precision_bits);
    return number_invalid();
}

 number_t number_const_like_double(const number_t *like, number_const_id_t id)
{
    (void)like;
    if (number_const_has_double(id))
        return num_create_from_double(number_const_double_value(id));
    return number_invalid();
}

 number_t number_const_like_qfloat(const number_t *like, number_const_id_t id)
{
    (void)like;
    return num_create_from_qfloat(number_const_qfloat(id));
}

 number_t number_const_like_qcomplex(const number_t *like, number_const_id_t id)
{
    (void)like;
    return num_create_from_qcomplex(number_const_qcomplex(id));
}

 number_t number_const_like_mexact(const number_t *like, number_const_id_t id)
{
    size_t precision_bits;
    number_t exact;
    const mfloat_t *mf_value;

    (void)like;
    exact = number_const_mreal_exact(id);
    if (number_is_valid_value(&exact))
        return exact;
    precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I)
        return number_wrap_mcomplex_with_precision(
            mc_create(MF_ZERO, MF_ONE), precision_bits);
    mf_value = number_const_mfloat_value(id);
    return mf_value ? num_create_from_mfloat_with_prec_bits(mf_value, precision_bits) : number_invalid();
}

 number_t number_const_like_mfloat(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(like) : 0u;
    number_t out;
    const mfloat_t *mf_value;

    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I)
        return number_wrap_mcomplex_with_precision(
            mc_create(MF_ZERO, MF_ONE), precision_bits);
    out = number_const_mfloat_special(id, precision_bits);
    if (number_is_valid_value(&out))
        return out;
    mf_value = number_const_mfloat_value(id);
    return mf_value ? num_create_from_mfloat_with_prec_bits(mf_value, precision_bits) : number_invalid();
}

 number_t number_const_like_mcomplex(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(like) : 0u;
    number_t real_value;
    const mfloat_t *mf_value;

    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I)
        return number_wrap_mcomplex_with_precision(
            mc_create(MF_ZERO, MF_ONE), precision_bits);
    real_value = number_const_mfloat_special(id, precision_bits);
    if (number_is_valid_value(&real_value)) {
        number_t out = number_wrap_mcomplex_with_precision(
            mc_create(number_impl_const(&real_value)->value.mf, MF_ZERO), precision_bits);
        num_destroy(&real_value);
        return out;
    }
    mf_value = number_const_mfloat_value(id);
    return mf_value
        ? number_wrap_mcomplex_with_precision(mc_create(mf_value, MF_ZERO), precision_bits)
        : number_invalid();
}

number_t number_const_like(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt;

    if (!like || !number_is_valid_value(like) || (unsigned)id >= NUMBER_CONST_COUNT)
        return number_invalid();
    vt = number_vt(like);
    return vt && vt->const_like ? vt->const_like(like, id) : number_invalid();
}

number_t num_new(void)
{
    return number_take(number_wrap_mfloat(mf_new_prec(number_default_precision_bits)));
}

number_t num_new_with_prec_bits(size_t precision_bits)
{
    return precision_bits == 0u ? number_invalid() :
        number_take(number_wrap_mfloat(mf_new_prec(precision_bits)));
}

number_t num_create_from_long(long value)
{
    return number_take(number_wrap_mint(mi_create_long(value)));
}

int num_set_default_prec_bits(size_t precision_bits)
{
    if (precision_bits == 0u)
        return -1;
    number_default_precision_bits = precision_bits;
    return 0;
}

size_t num_get_default_prec_bits(void)
{
    return number_default_precision_bits;
}

int num_set_default_prec_digits(size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_default_prec_bits(bits);
}

size_t num_get_default_prec_digits(void)
{
    return number_bits_to_digits(number_default_precision_bits);
}

int num_set_prec_digits(number_t *number, size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_prec_bits(number, bits);
}

size_t num_get_prec_digits(const number_t number)
{
    return number_bits_to_digits(num_get_prec_bits(number));
}

int num_set_long(number_t *number, long value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_from_long(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_double(number_t *number, double value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_from_double(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_qfloat(number_t *number, qfloat_t value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_from_qfloat(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_mrational(number_t *number, const mrational_t *value)
{
    if (!number || !value)
        return -1;
    number_assign(number, num_create_from_mrational(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_from_string(number_t *number, const char *text)
{
    if (!number || !text)
        return -1;
    number_assign(number, num_create_from_string(text));
    return number_is_valid_value(number) ? 0 : -1;
}

double num_to_double(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return NAN;
    if (vt && vt->to_double)
        return vt->to_double(&number);
    tmp = number_coerce(&number, NUMBER_MFLOAT);
    if (!tmp)
        return NAN;
    double value = mf_to_double(number_impl_const(tmp)->value.mf);
    number_box_free(tmp);
    return value;
}

qfloat_t num_to_qfloat(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return QF_NAN;
    if (vt && vt->to_qfloat)
        return vt->to_qfloat(&number);
    tmp = number_coerce(&number, NUMBER_MFLOAT);
    if (!tmp)
        return QF_NAN;
    qfloat_t value = mf_to_qfloat(number_impl_const(tmp)->value.mf);
    number_box_free(tmp);
    return value;
}

bool num_is_integer(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return false;
    return vt && vt->is_integer ? vt->is_integer(&number) : false;
}

bool num_is_finite(const number_t number)
{
    return number_is_finite_value(&number);
}

bool num_is_nan(const number_t number)
{
    return number_is_nan_value(&number);
}

bool num_is_inf(const number_t number)
{
    return number_is_inf_value(&number);
}

size_t num_get_mantissa_bits(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return number_is_valid_value(&number) && vt && vt->get_mantissa_bits
        ? vt->get_mantissa_bits(&number) : 0u;
}

bool num_get_mantissa_u64(const number_t number, uint64_t *out)
{
    const number_vtable_t *vt = number_vt(&number);

    return out && number_is_valid_value(&number) && vt && vt->get_mantissa_u64
        ? vt->get_mantissa_u64(&number, out) : false;
}

int num_sign(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (num_is_zero(number))
        return 0;
    return vt && vt->sign ? vt->sign(&number) : 0;
}

bool num_lt(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) < 0;
}

bool num_le(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) <= 0;
}

bool num_gt(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) > 0;
}

bool num_ge(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) >= 0;
}

int num_cmp(const number_t a, const number_t b)
{
    number_math_family_t family;
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    int rc = 0;

    if (!number_is_valid_value(&a) || !number_is_valid_value(&b) ||
        !num_is_real(a) || !num_is_real(b))
        return 0;
    if (number_same_kind_value(&a, &b))
        return number_cmp_same_kind(&a, &b);
    family = number_math_family_binary(number_math_family_value(&a),
        number_math_family_value(&b));
    kind = number_math_family_target_kind(family);
    if (kind == NUMBER_INVALID || family == NUMBER_MATH_MREAL)
        kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (lhs && rhs)
        rc = number_cmp_same_kind(lhs, rhs);
    number_box_free(lhs);
    number_box_free(rhs);
    return rc;
}

number_t num_abs(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->abs_value ? number_take(vt->abs_value(&number)) : number_invalid();
}

number_t num_conj(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->conj_value ? number_take(vt->conj_value(&number)) : number_invalid();
}

number_t num_real_part(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->real_part ? number_take(vt->real_part(&number)) : number_invalid();
}

number_t num_imag_part(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->imag_part ? number_take(vt->imag_part(&number)) : number_invalid();
}

number_t num_arg(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->arg_value)
        return number_take(vt->arg_value(&number));
    number_t zero = number_create_exact_mfloat_long_prec(
        0, num_get_prec_bits(number) ? num_get_prec_bits(number) : number_default_precision_bits);
    number_t real = vt && vt->complex ? num_real_part(number) : num_clone(number);
    number_t result = num_atan2(zero, real);
    num_destroy(&zero);
    num_destroy(&real);
    return result;
}

number_t num_add_mrational(const number_t number, const mrational_t *value)
{
    number_t rhs = num_create_from_mrational(value);
    number_t result = num_add(number, rhs);
    num_destroy(&rhs);
    return result;
}

number_t num_add_long(const number_t number, long value)
{
    number_t rhs = num_create_from_long(value);
    number_t result = num_add(number, rhs);
    num_destroy(&rhs);
    return result;
}

number_t num_mul_long(const number_t number, long value)
{
    number_t rhs = num_create_from_long(value);
    number_t result = num_mul(number, rhs);
    num_destroy(&rhs);
    return result;
}

number_t num_mul_mrational(const number_t number, const mrational_t *value)
{
    number_t rhs = num_create_from_mrational(value);
    number_t result = num_mul(number, rhs);
    num_destroy(&rhs);
    return result;
}
