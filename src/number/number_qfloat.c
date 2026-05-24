#include <stdlib.h>

#include "number.h"
#include "number_internal.h"

bool number_is_zero_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ZERO);
}

bool number_is_one_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ONE);
}

bool number_eq_same_qfloat(const number_t *a, const number_t *b)
{
    return a && b &&
        qf_eq(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf);
}

bool number_eq_same_tol_qfloat(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 106u);
}

bool number_is_finite_qfloat(const number_t *number)
{
    return number && !qf_isnan(number_impl_const(number)->value.qf) &&
        !qf_isinf(number_impl_const(number)->value.qf);
}

bool number_is_nan_qfloat(const number_t *number)
{
    return !number || qf_isnan(number_impl_const(number)->value.qf);
}

bool number_is_inf_qfloat(const number_t *number)
{
    return number && qf_isinf(number_impl_const(number)->value.qf);
}

int number_cmp_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? qf_cmp(number_impl_const(a)->value.qf,
                             number_impl_const(b)->value.qf) : 0;
}

long number_get_exponent2_qfloat(const number_t *number)
{
    if (!number)
        return 0l;
    return qf_get_exponent2(number_impl_const(number)->value.qf);
}

double number_to_double_qfloat(const number_t *number)
{
    return number ? qf_to_double(number_impl_const(number)->value.qf) : NAN;
}

qfloat_t number_to_qfloat_qfloat(const number_t *number)
{
    return number ? number_impl_const(number)->value.qf : QF_NAN;
}

bool number_is_integer_qfloat(const number_t *number)
{
    return number && qf_eq(qf_floor(number_impl_const(number)->value.qf),
                           number_impl_const(number)->value.qf);
}

size_t number_get_mantissa_bits_qfloat(const number_t *number)
{
    return number &&
            !qf_isnan(number_impl_const(number)->value.qf) &&
            !qf_isinf(number_impl_const(number)->value.qf) &&
            !qf_eq(number_impl_const(number)->value.qf, QF_ZERO) ? 106u : 0u;
}

int number_sign_qfloat(const number_t *number)
{
    return number && qf_lt(number_impl_const(number)->value.qf, QF_ZERO) ? -1 : 1;
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

number_t *number_clone_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(number_impl_const(number)->value.qf) : NULL;
}

number_t *number_neg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_neg(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_inv_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_div(QF_ONE, number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_abs_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_abs(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_imag_qfloat_zero(const number_t *number)
{
    (void)number;
    return number_wrap_qfloat(QF_ZERO);
}

number_t *number_arg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_atan2(QF_ZERO, number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_floor_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_floor(number_impl_const(number)->value.qf)) : NULL;
}

number_t *number_pow_int_qfloat(const number_t *number, int exponent)
{
    return number ? number_wrap_qfloat(qf_pow_int(number_impl_const(number)->value.qf, exponent)) : NULL;
}

number_t *number_ldexp_qfloat(const number_t *number, int exponent2)
{
    return number ? number_wrap_qfloat(qf_ldexp(number_impl_const(number)->value.qf, exponent2)) : NULL;
}

int number_sincos_qfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    *sin_out = number_take(number_wrap_qfloat(qf_sin(number_impl_const(number)->value.qf)));
    *cos_out = number_take(number_wrap_qfloat(qf_cos(number_impl_const(number)->value.qf)));
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

number_t *number_mul_pow10_qfloat(const number_t *number, int exponent10)
{
    return number ? number_wrap_qfloat(qf_mul_pow10(number_impl_const(number)->value.qf, exponent10)) : NULL;
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
