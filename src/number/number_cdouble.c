#include "number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include "ustring.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double complex number_cdouble_value(const number_t *number)
{
    return number ? number_impl_const(number)->value.cd.value : NAN + NAN * I;
}

static number_t number_cdouble_const(number_const_id_t id, double complex value)
{
    number_t out = num_create_from_cdouble(value);

    if (number_is_valid_value(&out)) {
        number_impl(&out)->value.cd.constant_id = id;
        number_impl(&out)->value.cd.immortal = true;
    }
    return out;
}

static string_t *number_format_cdouble_value_text(double value,
                                                  bool scientific,
                                                  int precision)
{
    char fmt[32];

    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dE" : "%%.%dg", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%.16E" : "%%.17g");
    return string_sprintf(fmt, value);
}

string_t *number_format_cdouble_text(const number_t *number,
                                     bool scientific,
                                     int precision)
{
    double complex value;
    double real;
    double imag;
    string_t *real_text = NULL;
    string_t *imag_text = NULL;
    string_t *out = NULL;
    const char *sign;

    if (!number)
        return NULL;
    value = number_cdouble_value(number);
    real = creal(value);
    imag = cimag(value);
    if (imag == 0.0)
        return number_format_cdouble_value_text(real, scientific, precision);
    if (real == 0.0) {
        if (imag == 1.0)
            return string_new_with("i");
        if (imag == -1.0)
            return string_new_with("-i");
        imag_text = number_format_cdouble_value_text(imag, scientific, precision);
        if (!imag_text)
            return NULL;
        out = string_sprintf("%Si", imag_text);
        string_free(imag_text);
        return out;
    }

    real_text = number_format_cdouble_value_text(real, scientific, precision);
    imag_text = number_format_cdouble_value_text(fabs(imag), scientific, precision);
    if (!real_text || !imag_text)
        goto done;
    sign = imag < 0.0 ? "-" : "+";
    {
        out = fabs(imag) == 1.0
            ? string_sprintf("%S %s i", real_text, sign)
            : string_sprintf("%S %s %Si", real_text, sign, imag_text);
    }

done:
    string_free(real_text);
    string_free(imag_text);
    return out;
}

string_t *number_to_text_cdouble(const number_t *number)
{
    return number_format_cdouble_text(number, false, -1);
}

bool number_is_zero_cdouble(const number_t *number)
{
    return number && creal(number_cdouble_value(number)) == 0.0 &&
        cimag(number_cdouble_value(number)) == 0.0;
}

bool number_is_real_cdouble(const number_t *number)
{
    return number && cimag(number_cdouble_value(number)) == 0.0;
}

bool number_is_one_cdouble(const number_t *number)
{
    return number && creal(number_cdouble_value(number)) == 1.0 &&
        cimag(number_cdouble_value(number)) == 0.0;
}

bool number_eq_same_cdouble(const number_t *a, const number_t *b)
{
    return a && b && number_cdouble_value(a) == number_cdouble_value(b);
}

bool number_eq_same_tol_cdouble(const number_t *a, const number_t *b)
{
    return number_eq_same_tol_with_precision(a, b, 53u);
}

bool number_is_finite_cdouble(const number_t *number)
{
    double complex value = number_cdouble_value(number);

    return number && isfinite(creal(value)) && isfinite(cimag(value));
}

bool number_is_nan_cdouble(const number_t *number)
{
    double complex value = number_cdouble_value(number);

    return !number || isnan(creal(value)) || isnan(cimag(value));
}

bool number_is_inf_cdouble(const number_t *number)
{
    double complex value = number_cdouble_value(number);

    return number && (isinf(creal(value)) || isinf(cimag(value)));
}

int number_cmp_same_cdouble(const number_t *a, const number_t *b)
{
    double complex av;
    double complex bv;

    if (!a || !b)
        return 0;
    av = number_cdouble_value(a);
    bv = number_cdouble_value(b);
    if (creal(av) < creal(bv))
        return -1;
    if (creal(av) > creal(bv))
        return 1;
    if (cimag(av) < cimag(bv))
        return -1;
    if (cimag(av) > cimag(bv))
        return 1;
    return 0;
}

bool number_is_integer_cdouble(const number_t *number)
{
    double complex value = number_cdouble_value(number);
    double real = creal(value);

    return number && cimag(value) == 0.0 && isfinite(real) && floor(real) == real;
}

number_t *number_clone_cdouble(const number_t *number)
{
    number_t *copy;

    if (!number)
        return NULL;
    copy = number_wrap_cdouble(number_cdouble_value(number));
    if (copy)
        number_impl(copy)->value.cd = number_impl_const(number)->value.cd;
    return copy;
}

number_t *number_const_prec_cdouble(const number_t *number,
                                        size_t precision_bits)
{
    (void)precision_bits;
    return number_clone_cdouble(number);
}

number_t *number_neg_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(-number_cdouble_value(number)) : NULL;
}

number_t *number_inv_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(1.0 / number_cdouble_value(number)) : NULL;
}

number_t *number_abs_cdouble(const number_t *number)
{
    return number ? number_wrap_double(cabs(number_cdouble_value(number))) : NULL;
}

number_t *number_conj_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(conj(number_cdouble_value(number))) : NULL;
}

number_t *number_real_cdouble(const number_t *number)
{
    return number ? number_wrap_double(creal(number_cdouble_value(number))) : NULL;
}

number_t *number_imag_cdouble(const number_t *number)
{
    return number ? number_wrap_double(cimag(number_cdouble_value(number))) : NULL;
}

number_t *number_arg_cdouble(const number_t *number)
{
    return number ? number_wrap_double(carg(number_cdouble_value(number))) : NULL;
}

number_t *number_floor_cdouble(const number_t *number)
{
    double complex value;

    if (!number)
        return NULL;
    value = number_cdouble_value(number);
    return number_wrap_cdouble(floor(creal(value)) + floor(cimag(value)) * I);
}

number_t *number_pow_int_cdouble(const number_t *number, int exponent)
{
    return number
        ? number_wrap_cdouble(cpow(number_cdouble_value(number), (double)exponent))
        : NULL;
}

number_t *number_mul_pow10_cdouble(const number_t *number, int exponent10)
{
    return number
        ? number_wrap_cdouble(number_cdouble_value(number) *
                              pow(10.0, (double)exponent10))
        : NULL;
}

number_t *number_ldexp_cdouble(const number_t *number, int exponent2)
{
    double complex value;

    if (!number)
        return NULL;
    value = number_cdouble_value(number);
    return number_wrap_cdouble(ldexp(creal(value), exponent2) +
                               ldexp(cimag(value), exponent2) * I);
}

number_t *number_add_same_cdouble(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_cdouble(number_cdouble_value(a) +
                                          number_cdouble_value(b)) : NULL;
}

number_t *number_sub_same_cdouble(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_cdouble(number_cdouble_value(a) -
                                          number_cdouble_value(b)) : NULL;
}

number_t *number_mul_same_cdouble(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_cdouble(number_cdouble_value(a) *
                                          number_cdouble_value(b)) : NULL;
}

number_t *number_div_same_cdouble(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_cdouble(number_cdouble_value(a) /
                                          number_cdouble_value(b)) : NULL;
}

number_t *number_exp_same_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(cexp(number_cdouble_value(number))) : NULL;
}

number_t *number_log_same_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(clog(number_cdouble_value(number))) : NULL;
}

number_t *number_sqrt_same_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(csqrt(number_cdouble_value(number))) : NULL;
}

number_t number_const_like_cdouble(const number_t *like, number_const_id_t id)
{
    (void)like;
    if (number_const_has_cdouble(id))
        return number_cdouble_const(id, number_const_cdouble_value(id));
    return number_invalid();
}

number_t number_imag_const_like_cdouble(const number_t *like,
                                            number_const_id_t id)
{
    (void)like;
    if (id == NUMBER_CONST_ONE)
        return number_cdouble_const(NUMBER_CONST_I, I);
    if (id == NUMBER_CONST_NEG_ONE)
        return number_cdouble_const(NUMBER_CONST_NEG_I, -I);
    if (id == NUMBER_CONST_NAN)
        return num_create_from_cdouble(NAN * I);
    if (number_const_has_cdouble(id))
        return num_create_from_cdouble(creal(number_const_cdouble_value(id)) * I);
    return number_invalid();
}

bool number_value_is_immortal_cdouble(const number_t *number)
{
    return number && number_impl_const(number)->value.cd.immortal &&
        number_impl_const(number)->value.cd.constant_id < NUMBER_CONST_COUNT;
}

bool number_immortal_id_cdouble(const number_t *number,
                                    number_const_id_t *id_out)
{
    if (!number_value_is_immortal_cdouble(number) || !id_out)
        return false;
    *id_out = number_impl_const(number)->value.cd.constant_id;
    return true;
}
