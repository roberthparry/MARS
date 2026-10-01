#include <stdlib.h>

#include "number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include "ustring.h"

/* Extract the complex payload through its original storage representation. */
qcomplex_t number_inline_qcomplex(number_t number)
{
    number_inline_qcomplex_bits_t bits;

    bits.words[0] = number.storage[1];
    bits.words[1] = number.storage[2];
    bits.words[2] = number.storage[3];
    bits.words[3] = number.storage[4];
    return bits.value;
}

/* Extract the stored real component without changing its precision. */
qfloat_t number_inline_qcomplex_real(number_t number)
{
    number_inline_qfloat_bits_t bits;

    bits.words[0] = number.storage[1];
    bits.words[1] = number.storage[2];
    return bits.value;
}

/* Extract the stored imaginary component without changing its precision. */
qfloat_t number_inline_qcomplex_imag(number_t number)
{
    number_inline_qfloat_bits_t bits;

    bits.words[0] = number.storage[3];
    bits.words[1] = number.storage[4];
    return bits.value;
}

/* Construct a complex payload without converting or rounding either component. */
number_t number_inline_make_qcomplex_parts(qfloat_t real, qfloat_t imag)
{
    number_t number;
    number_inline_qfloat_bits_t real_bits;
    number_inline_qfloat_bits_t imag_bits;

    real_bits.value = real;
    imag_bits.value = imag;
    number.storage[0] = NUMBER_QCOMPLEX;
    number.storage[1] = real_bits.words[0];
    number.storage[2] = real_bits.words[1];
    number.storage[3] = imag_bits.words[0];
    number.storage[4] = imag_bits.words[1];
    return number;
}

bool number_is_zero_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ZERO);
}

bool number_is_real_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO);
}

bool number_is_one_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ONE);
}

bool number_eq_same_qcomplex(const number_t *a, const number_t *b)
{
    return a && b && qc_eq(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc);
}

bool number_eq_same_tol_qcomplex(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 106u);
}

bool number_is_finite_qcomplex(const number_t *number)
{
    return number && !qc_isnan(number_impl_const(number)->value.qc) && !qc_isinf(number_impl_const(number)->value.qc);
}

bool number_is_nan_qcomplex(const number_t *number)
{
    return !number || qc_isnan(number_impl_const(number)->value.qc);
}

bool number_is_inf_qcomplex(const number_t *number)
{
    return number && qc_isinf(number_impl_const(number)->value.qc);
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

bool number_is_integer_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO) &&
           qf_eq(qf_floor(qc_real(number_impl_const(number)->value.qc)), qc_real(number_impl_const(number)->value.qc));
}

string_t *number_to_text_qcomplex(const number_t *number)
{
    if (!number)
        return NULL;
    return qc_sprintf_text("%z", number_impl_const(number)->value.qc);
}

number_t *number_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

number_t *number_neg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_neg(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_inv_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_div(QC_ONE, number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_abs_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_abs(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_conj_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_conj(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_real_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_real(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_imag_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_imag(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_arg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_arg(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_floor_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_floor(number_impl_const(number)->value.qc)) : NULL;
}

number_t *number_pow_int_qcomplex(const number_t *number, int exponent)
{
    return number ? number_wrap_qcomplex(
                        qc_pow(number_impl_const(number)->value.qc, qc_make(qf_from_double((double)exponent), QF_ZERO)))
                  : NULL;
}

number_t *number_ldexp_qcomplex(const number_t *number, int exponent2)
{
    return number ? number_wrap_qcomplex(qc_ldexp(number_impl_const(number)->value.qc, exponent2)) : NULL;
}

number_t *number_add_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_add(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc))
                    : NULL;
}

number_t *number_sub_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_sub(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc))
                    : NULL;
}

number_t *number_mul_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_mul(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc))
                    : NULL;
}

number_t *number_div_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_div(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc))
                    : NULL;
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
