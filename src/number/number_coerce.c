#include "number.h"
#include "number_internal.h"

#include <complex.h>
#include <stdlib.h>

static bool number_kind_is_exact_real(number_kind_t kind)
{
    return kind == NUMBER_MPZ || kind == NUMBER_MPQ;
}

static bool number_kind_is_mpfr_family(number_kind_t kind)
{
    return number_kind_is_exact_real(kind) || kind == NUMBER_MPFR;
}

static qfloat_t qfloat_from_mpz_value(const number_mpz_t *value)
{
    char *text;
    qfloat_t out;

    if (!value)
        return QF_NAN;
    text = number_mpz_to_string(value);
    if (!text)
        return QF_NAN;
    out = qf_from_string(text);
    free(text);
    return out;
}

static qfloat_t qfloat_from_mpq_value(const number_mpq_t *value)
{
    char *num_text = NULL;
    char *den_text = NULL;
    qfloat_t numerator;
    qfloat_t denominator;
    qfloat_t out = QF_NAN;

    if (!value || number_mpq_ensure(value) != 0)
        return QF_NAN;
    num_text = mpz_get_str(NULL, 10, mpq_numref(value->value));
    den_text = mpz_get_str(NULL, 10, mpq_denref(value->value));
    if (num_text && den_text) {
        numerator = qf_from_string(num_text);
        denominator = qf_from_string(den_text);
        out = qf_div(numerator, denominator);
    }
    free(num_text);
    free(den_text);
    return out;
}

number_kind_t number_common_kind(const number_t *a,
                                 const number_t *b,
                                 number_binary_op_t op)
{
    number_kind_t ak;
    number_kind_t bk;

    if (!a || !b)
        return NUMBER_COMPLEX;
    ak = number_impl_const(a)->kind;
    bk = number_impl_const(b)->kind;
    if ((unsigned)ak > NUMBER_COMPLEX || (unsigned)bk > NUMBER_COMPLEX)
        return NUMBER_COMPLEX;
    if (ak == NUMBER_INVALID || bk == NUMBER_INVALID)
        return NUMBER_COMPLEX;
    if (ak == NUMBER_COMPLEX || bk == NUMBER_COMPLEX)
        return NUMBER_COMPLEX;
    if (ak == NUMBER_QCOMPLEX || bk == NUMBER_QCOMPLEX)
        return (number_kind_is_mpfr_family(ak) || number_kind_is_mpfr_family(bk))
            ? NUMBER_COMPLEX : NUMBER_QCOMPLEX;
    if (ak == NUMBER_CDOUBLE || bk == NUMBER_CDOUBLE) {
        if (ak == NUMBER_MPFR || bk == NUMBER_MPFR)
            return NUMBER_COMPLEX;
        if (ak == NUMBER_QFLOAT || bk == NUMBER_QFLOAT)
            return NUMBER_QCOMPLEX;
        return NUMBER_CDOUBLE;
    }
    if (ak == NUMBER_MPFR || bk == NUMBER_MPFR)
        return NUMBER_MPFR;
    if (ak == NUMBER_QFLOAT || bk == NUMBER_QFLOAT)
        return NUMBER_QFLOAT;
    if (ak == NUMBER_DOUBLE || bk == NUMBER_DOUBLE)
        return NUMBER_DOUBLE;
    if (op == NUMBER_OP_DIV)
        return NUMBER_MPQ;
    return ak == NUMBER_MPQ || bk == NUMBER_MPQ
        ? NUMBER_MPQ : NUMBER_MPZ;
}

static number_t *number_create_complex_from_number(const number_t *real)
{
    number_t real_component;
    number_t imag_component;
    complex_t *complex_value;
    size_t precision_bits;

    if (!real)
        return NULL;
    precision_bits = num_get_prec_bits(*real);
    real_component = number_complex_component_from_number(real, precision_bits);
    imag_component = number_complex_component_from_number(&NUM_ZERO, precision_bits);
    complex_value = number_complex_create(real_component, imag_component);
    if (!complex_value) {
        num_destroy(&real_component);
        num_destroy(&imag_component);
        return NULL;
    }
    complex_value->precision_bits = precision_bits;
    return number_wrap_complex(complex_value);
}

static number_t *number_coerce_invalid(const number_t *number)
{
    (void)number;
    return NULL;
}

static number_t *number_coerce_clone_double(const number_t *number)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d) : NULL;
}

static number_t *number_coerce_double_to_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_from_double(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_coerce_double_to_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(
                        qc_make(qf_from_double(number_impl_const(number)->value.d), QF_ZERO))
                  : NULL;
}

static number_t *number_coerce_double_to_cdouble(const number_t *number)
{
    return number ? number_wrap_cdouble(number_impl_const(number)->value.d) : NULL;
}

static number_t *number_coerce_double_to_mpfr(const number_t *number)
{
    number_mpfr_t *out;

    if (!number)
        return NULL;
    out = number_mpfr_from_double(number_impl_const(number)->value.d,
                                  number_default_precision_bits);
    return out ? number_wrap_mpfr(out) : NULL;
}

static number_t *number_coerce_double_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(number_impl_const(number)->value.qf) : NULL;
}

static number_t *number_coerce_qfloat_to_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_make(number_impl_const(number)->value.qf, QF_ZERO)) : NULL;
}

static number_t *number_coerce_qfloat_to_mpfr(const number_t *number)
{
    number_mpfr_t *out;

    if (!number)
        return NULL;
    out = number_mpfr_from_qfloat(number_impl_const(number)->value.qf,
                                  number_default_precision_bits);
    return out ? number_wrap_mpfr(out) : NULL;
}

static number_t *number_coerce_qfloat_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

static number_t *number_coerce_clone_cdouble(const number_t *number)
{
    number_t *copy;

    if (!number)
        return NULL;
    copy = number_wrap_cdouble(number_impl_const(number)->value.cd.value);
    if (copy)
        number_impl(copy)->value.cd = number_impl_const(number)->value.cd;
    return copy;
}

static number_t *number_coerce_cdouble_to_qcomplex(const number_t *number)
{
    double complex value;

    if (!number)
        return NULL;
    value = number_impl_const(number)->value.cd.value;
    return number_wrap_qcomplex(qc_make(qf_from_double(creal(value)),
                                        qf_from_double(cimag(value))));
}

static number_t *number_coerce_cdouble_to_complex(const number_t *number)
{
    qcomplex_t value;

    if (!number)
        return NULL;
    value = qc_make(qf_from_double(creal(number_impl_const(number)->value.cd.value)),
                    qf_from_double(cimag(number_impl_const(number)->value.cd.value)));
    return number_wrap_complex(number_complex_create_from_qcomplex(value, 53u));
}

static number_t *number_coerce_qcomplex_to_complex(const number_t *number)
{
    return number ? number_wrap_complex(number_complex_create_from_qcomplex(
                        number_impl_const(number)->value.qc, 106u)) : NULL;
}

static number_t *number_coerce_clone_mpz(const number_t *number)
{
    number_mpz_t *copy;

    if (!number)
        return NULL;
    copy = number_mpz_clone(number_impl_const(number)->value.mpz);
    return copy ? number_wrap_mpz(copy) : NULL;
}

static number_t *number_coerce_mpz_to_mpq(const number_t *number)
{
    number_mpq_t *out;
    mpq_t tmp;

    if (!number)
        return NULL;
    if (number_mpz_ensure(number_impl_const(number)->value.mpz) != 0)
        return NULL;
    mpq_init(tmp);
    mpz_set(mpq_numref(tmp), number_impl_const(number)->value.mpz->value);
    mpz_set_ui(mpq_denref(tmp), 1u);
    mpq_canonicalize(tmp);
    out = number_mpq_from_mpq(tmp);
    mpq_clear(tmp);
    return out ? number_wrap_mpq(out) : NULL;
}

static number_t *number_coerce_mpz_to_double(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpz_value(number_impl_const(number)->value.mpz);
    return qf_isnan(value) ? NULL : number_wrap_double(qf_to_double(value));
}

static number_t *number_coerce_mpz_to_qfloat(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpz_value(number_impl_const(number)->value.mpz);
    return qf_isnan(value) ? NULL : number_wrap_qfloat(value);
}

static number_t *number_coerce_mpz_to_cdouble(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpz_value(number_impl_const(number)->value.mpz);
    return qf_isnan(value) ? NULL : number_wrap_cdouble(qf_to_double(value));
}

static number_t *number_coerce_mpz_to_mpfr(const number_t *number)
{
    number_mpfr_t *out;

    if (!number)
        return NULL;
    out = number_mpfr_new_prec(number_default_precision_bits);
    if (!out)
        return NULL;
    (void)mpfr_set_z(out->value, number_mpz_value(number_impl_const(number)->value.mpz),
                     MPFR_RNDN);
    return number_wrap_mpfr(out);
}

static number_t *number_coerce_mpz_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_mpq(const number_t *number)
{
    number_mpq_t *copy;

    if (!number)
        return NULL;
    copy = number_mpq_clone(number_impl_const(number)->value.mpq);
    return copy ? number_wrap_mpq(copy) : NULL;
}

static number_t *number_coerce_mpq_to_double(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpq_value(number_impl_const(number)->value.mpq);
    return qf_isnan(value) ? NULL : number_wrap_double(qf_to_double(value));
}

static number_t *number_coerce_mpq_to_qfloat(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpq_value(number_impl_const(number)->value.mpq);
    return qf_isnan(value) ? NULL : number_wrap_qfloat(value);
}

static number_t *number_coerce_mpq_to_cdouble(const number_t *number)
{
    qfloat_t value;
    number_const_id_t id;

    if (!number)
        return NULL;
    value = number_const_id_from_immortal(number, &id)
        ? number_const_qfloat(id)
        : qfloat_from_mpq_value(number_impl_const(number)->value.mpq);
    return qf_isnan(value) ? NULL : number_wrap_cdouble(qf_to_double(value));
}

static number_t *number_coerce_mpq_to_mpfr(const number_t *number)
{
    number_mpfr_t *out;

    if (!number)
        return NULL;
    out = number_mpfr_new_prec(number_default_precision_bits);
    if (!out)
        return NULL;
    if (number_mpq_ensure(number_impl_const(number)->value.mpq) != 0) {
        number_mpfr_free(out);
        return NULL;
    }
    (void)mpfr_set_q(out->value, number_impl_const(number)->value.mpq->value,
                     MPFR_RNDN);
    return number_wrap_mpfr(out);
}

static number_t *number_coerce_mpq_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_mpfr(const number_t *number)
{
    number_mpfr_t *copy;

    if (!number)
        return NULL;
    copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
    return copy ? number_wrap_mpfr(copy) : NULL;
}

static number_t *number_coerce_mpfr_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_complex(const number_t *number)
{
    complex_t *copy;

    if (!number)
        return NULL;
    copy = number_complex_clone(number_impl_const(number)->value.cx);
    return copy ? number_wrap_complex(copy) : NULL;
}

static const number_coerce_fn number_coerce_dispatch[][NUMBER_COMPLEX + 1] = {
    [NUMBER_INVALID] = {
        [NUMBER_INVALID] = number_coerce_invalid,
        [NUMBER_DOUBLE] = number_coerce_invalid,
        [NUMBER_QFLOAT] = number_coerce_invalid,
        [NUMBER_QCOMPLEX] = number_coerce_invalid,
        [NUMBER_MPZ] = number_coerce_invalid,
        [NUMBER_MPQ] = number_coerce_invalid,
        [NUMBER_MPFR] = number_coerce_invalid,
        [NUMBER_CDOUBLE] = number_coerce_invalid,
        [NUMBER_COMPLEX] = number_coerce_invalid
    },
    [NUMBER_DOUBLE] = {
        [NUMBER_DOUBLE] = number_coerce_clone_double,
        [NUMBER_QFLOAT] = number_coerce_double_to_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_double_to_qcomplex,
        [NUMBER_MPFR] = number_coerce_double_to_mpfr,
        [NUMBER_CDOUBLE] = number_coerce_double_to_cdouble,
        [NUMBER_COMPLEX] = number_coerce_double_to_complex
    },
    [NUMBER_QFLOAT] = {
        [NUMBER_QFLOAT] = number_coerce_clone_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_qfloat_to_qcomplex,
        [NUMBER_MPFR] = number_coerce_qfloat_to_mpfr,
        [NUMBER_COMPLEX] = number_coerce_qfloat_to_complex
    },
    [NUMBER_QCOMPLEX] = {
        [NUMBER_QCOMPLEX] = number_coerce_clone_qcomplex,
        [NUMBER_COMPLEX] = number_coerce_qcomplex_to_complex
    },
    [NUMBER_CDOUBLE] = {
        [NUMBER_QCOMPLEX] = number_coerce_cdouble_to_qcomplex,
        [NUMBER_CDOUBLE] = number_coerce_clone_cdouble,
        [NUMBER_COMPLEX] = number_coerce_cdouble_to_complex
    },
    [NUMBER_MPZ] = {
        [NUMBER_DOUBLE] = number_coerce_mpz_to_double,
        [NUMBER_QFLOAT] = number_coerce_mpz_to_qfloat,
        [NUMBER_CDOUBLE] = number_coerce_mpz_to_cdouble,
        [NUMBER_MPZ] = number_coerce_clone_mpz,
        [NUMBER_MPQ] = number_coerce_mpz_to_mpq,
        [NUMBER_MPFR] = number_coerce_mpz_to_mpfr,
        [NUMBER_COMPLEX] = number_coerce_mpz_to_complex
    },
    [NUMBER_MPQ] = {
        [NUMBER_DOUBLE] = number_coerce_mpq_to_double,
        [NUMBER_QFLOAT] = number_coerce_mpq_to_qfloat,
        [NUMBER_CDOUBLE] = number_coerce_mpq_to_cdouble,
        [NUMBER_MPQ] = number_coerce_clone_mpq,
        [NUMBER_MPFR] = number_coerce_mpq_to_mpfr,
        [NUMBER_COMPLEX] = number_coerce_mpq_to_complex
    },
    [NUMBER_MPFR] = {
        [NUMBER_MPFR] = number_coerce_clone_mpfr,
        [NUMBER_COMPLEX] = number_coerce_mpfr_to_complex
    },
    [NUMBER_COMPLEX] = {
        [NUMBER_COMPLEX] = number_coerce_clone_complex
    }
};

number_t *number_coerce(const number_t *number, number_kind_t target_kind)
{
    number_kind_t source_kind;
    number_coerce_fn fn;

    if (!number)
        return NULL;
    source_kind = number_impl_const(number)->kind;
    if ((size_t)source_kind > NUMBER_COMPLEX || (size_t)target_kind > NUMBER_COMPLEX)
        return NULL;
    fn = number_coerce_dispatch[source_kind][target_kind];
    return fn ? fn(number) : NULL;
}
