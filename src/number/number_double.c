#include "number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include "ustring.h"

#include <math.h>
#include <stdio.h>

bool number_is_zero_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 0.0;
}

bool number_is_one_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 1.0;
}

bool number_eq_same_double(const number_t *a, const number_t *b)
{
    return a && b && number_impl_const(a)->value.d == number_impl_const(b)->value.d;
}

bool number_eq_same_tol_double(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 53u);
}

bool number_is_finite_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d);
}

bool number_is_nan_double(const number_t *number)
{
    return !number || isnan(number_impl_const(number)->value.d);
}

bool number_is_inf_double(const number_t *number)
{
    return number && isinf(number_impl_const(number)->value.d);
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

long number_get_exponent2_double(const number_t *number)
{
    int exp2;

    if (!number)
        return 0l;
    exp2 = ilogb(fabs(number_impl_const(number)->value.d));
    return exp2 == FP_ILOGB0 || exp2 == FP_ILOGBNAN ? 0l : (long)exp2;
}

double number_to_double_double(const number_t *number)
{
    return number ? number_impl_const(number)->value.d : NAN;
}

qfloat_t number_to_qfloat_double(const number_t *number)
{
    return number ? qf_from_double(number_impl_const(number)->value.d) : QF_NAN;
}

bool number_is_integer_double(const number_t *number)
{
    double x;

    if (!number)
        return false;
    x = number_impl_const(number)->value.d;
    return isfinite(x) && floor(x) == x;
}

size_t number_get_mantissa_bits_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d) &&
            number_impl_const(number)->value.d != 0.0 ? 53u : 0u;
}

int number_sign_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d < 0.0 ? -1 : 1;
}

string_t *number_to_text_double(const number_t *number)
{
    if (!number)
        return NULL;
    return string_sprintf("%.17g", number_impl_const(number)->value.d);
}

number_t *number_clone_double(const number_t *number)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d) : NULL;
}

number_t *number_neg_double(const number_t *number)
{
    return number ? number_wrap_double(-number_impl_const(number)->value.d) : NULL;
}

number_t *number_inv_double(const number_t *number)
{
    return number ? number_wrap_double(1.0 / number_impl_const(number)->value.d) : NULL;
}

number_t *number_abs_double(const number_t *number)
{
    return number ? number_wrap_double(fabs(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_imag_double_zero(const number_t *number)
{
    (void)number;
    return number_wrap_double(0.0);
}

number_t *number_arg_double(const number_t *number)
{
    return number ? number_wrap_double(atan2(0.0, number_impl_const(number)->value.d)) : NULL;
}

number_t *number_floor_double(const number_t *number)
{
    return number ? number_wrap_double(floor(number_impl_const(number)->value.d)) : NULL;
}

number_t *number_pow_int_double(const number_t *number, int exponent)
{
    return number ? number_wrap_double(pow(number_impl_const(number)->value.d, (double)exponent)) : NULL;
}

number_t *number_ldexp_double(const number_t *number, int exponent2)
{
    return number ? number_wrap_double(ldexp(number_impl_const(number)->value.d, exponent2)) : NULL;
}

int number_sincos_double(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    *sin_out = number_take(number_wrap_double(sin(number_impl_const(number)->value.d)));
    *cos_out = number_take(number_wrap_double(cos(number_impl_const(number)->value.d)));
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

number_t *number_mul_pow10_double(const number_t *number, int exponent10)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d * pow(10.0, (double)exponent10)) : NULL;
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
