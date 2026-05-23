#include "number.h"
#include "number_internal.h"

static bool number_kind_is_exact_real(number_kind_t kind)
{
    return kind == NUMBER_MINT || kind == NUMBER_MRATIONAL;
}

static bool number_kind_is_mreal(number_kind_t kind)
{
    return number_kind_is_exact_real(kind) || kind == NUMBER_MFLOAT;
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
        return (number_kind_is_mreal(ak) || number_kind_is_mreal(bk))
            ? NUMBER_COMPLEX : NUMBER_QCOMPLEX;
    if (ak == NUMBER_MFLOAT || bk == NUMBER_MFLOAT ||
        (ak == NUMBER_DOUBLE && number_kind_is_exact_real(bk)) ||
        (bk == NUMBER_DOUBLE && number_kind_is_exact_real(ak)) ||
        (ak == NUMBER_QFLOAT && number_kind_is_exact_real(bk)) ||
        (bk == NUMBER_QFLOAT && number_kind_is_exact_real(ak)))
        return NUMBER_MFLOAT;
    if (ak == NUMBER_QFLOAT || bk == NUMBER_QFLOAT)
        return NUMBER_QFLOAT;
    if (ak == NUMBER_DOUBLE || bk == NUMBER_DOUBLE)
        return NUMBER_DOUBLE;
    if (op == NUMBER_OP_DIV)
        return NUMBER_MRATIONAL;
    return ak == NUMBER_MRATIONAL || bk == NUMBER_MRATIONAL
        ? NUMBER_MRATIONAL : NUMBER_MINT;
}

static number_t *number_create_complex_from_number(const number_t *real)
{
    number_t real_component;
    number_t imag_component;
    complex_t *complex_value;

    if (!real)
        return NULL;
    real_component = number_complex_component_from_number(real, num_get_prec_bits(*real));
    imag_component = number_complex_component_from_number(&NUM_ZERO, num_get_prec_bits(*real));
    complex_value = number_complex_create(real_component, imag_component);
    if (!complex_value) {
        num_destroy(&real_component);
        num_destroy(&imag_component);
        return NULL;
    }
    return number_wrap_complex(complex_value);
}

static mfloat_t *number_reprecision_mfloat(mfloat_t *value, size_t precision_bits)
{
    if (!value)
        return NULL;
    if (mf_set_precision(value, precision_bits) != 0) {
        mf_free(value);
        return NULL;
    }
    return value;
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

static number_t *number_coerce_double_to_mfloat(const number_t *number)
{
    return number ? number_wrap_mfloat(number_reprecision_mfloat(
                        mf_create_double(number_impl_const(number)->value.d),
                        number_default_precision_bits))
                  : NULL;
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

static number_t *number_coerce_qfloat_to_mfloat(const number_t *number)
{
    return number ? number_wrap_mfloat(number_reprecision_mfloat(
                        mf_create_qfloat(number_impl_const(number)->value.qf),
                        number_default_precision_bits))
                  : NULL;
}

static number_t *number_coerce_qfloat_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

static number_t *number_coerce_qcomplex_to_complex(const number_t *number)
{
    return number ? number_wrap_complex(number_complex_create_from_qcomplex(
                        number_impl_const(number)->value.qc, 106u)) : NULL;
}

static number_t *number_coerce_clone_mint(const number_t *number)
{
    mint_t *copy;

    if (!number)
        return NULL;
    copy = mi_clone(number_impl_const(number)->value.mi);
    return copy ? number_wrap_mint(copy) : NULL;
}

static number_t *number_coerce_mint_to_mrational(const number_t *number)
{
    mrational_t *tmp_rational;

    if (!number)
        return NULL;
    tmp_rational = mr_create_mints(number_impl_const(number)->value.mi, MI_ONE);
    return tmp_rational ? number_wrap_mrational(tmp_rational) : NULL;
}

static number_t *number_coerce_mint_to_mfloat(const number_t *number)
{
    mrational_t *tmp_rational;
    mfloat_t *tmp_float;

    if (!number)
        return NULL;
    tmp_rational = mr_create_mints(number_impl_const(number)->value.mi, MI_ONE);
    if (!tmp_rational)
        return NULL;
    tmp_float = mf_new_prec(number_default_precision_bits);
    if (!tmp_float || mf_set_mrational(tmp_float, tmp_rational) != 0) {
        mf_free(tmp_float);
        tmp_float = NULL;
    }
    mr_free(tmp_rational);
    return tmp_float ? number_wrap_mfloat(tmp_float) : NULL;
}

static number_t *number_coerce_mint_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number)
        return NULL;
    copy = mr_clone(number_impl_const(number)->value.mr);
    return copy ? number_wrap_mrational(copy) : NULL;
}

static number_t *number_coerce_mrational_to_mfloat(const number_t *number)
{
    mfloat_t *tmp_float;

    if (!number)
        return NULL;

    tmp_float = mf_new_prec(number_default_precision_bits);
    if (!tmp_float ||
        mf_set_mrational(tmp_float, number_impl_const(number)->value.mr) != 0) {
        mf_free(tmp_float);
        return NULL;
    }
    return number_wrap_mfloat(tmp_float);
}

static number_t *number_coerce_mrational_to_complex(const number_t *number)
{
    return number ? number_create_complex_from_number(number) : NULL;
}

static number_t *number_coerce_clone_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    return copy ? number_wrap_mfloat(copy) : NULL;
}

static number_t *number_coerce_mfloat_to_complex(const number_t *number)
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
        [NUMBER_MINT] = number_coerce_invalid,
        [NUMBER_MRATIONAL] = number_coerce_invalid,
        [NUMBER_MFLOAT] = number_coerce_invalid,
        [NUMBER_COMPLEX] = number_coerce_invalid
    },
    [NUMBER_DOUBLE] = {
        [NUMBER_DOUBLE] = number_coerce_clone_double,
        [NUMBER_QFLOAT] = number_coerce_double_to_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_double_to_qcomplex,
        [NUMBER_MFLOAT] = number_coerce_double_to_mfloat,
        [NUMBER_COMPLEX] = number_coerce_double_to_complex
    },
    [NUMBER_QFLOAT] = {
        [NUMBER_QFLOAT] = number_coerce_clone_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_qfloat_to_qcomplex,
        [NUMBER_MFLOAT] = number_coerce_qfloat_to_mfloat,
        [NUMBER_COMPLEX] = number_coerce_qfloat_to_complex
    },
    [NUMBER_QCOMPLEX] = {
        [NUMBER_QCOMPLEX] = number_coerce_clone_qcomplex,
        [NUMBER_COMPLEX] = number_coerce_qcomplex_to_complex
    },
    [NUMBER_MINT] = {
        [NUMBER_MINT] = number_coerce_clone_mint,
        [NUMBER_MRATIONAL] = number_coerce_mint_to_mrational,
        [NUMBER_MFLOAT] = number_coerce_mint_to_mfloat,
        [NUMBER_COMPLEX] = number_coerce_mint_to_complex
    },
    [NUMBER_MRATIONAL] = {
        [NUMBER_MRATIONAL] = number_coerce_clone_mrational,
        [NUMBER_MFLOAT] = number_coerce_mrational_to_mfloat,
        [NUMBER_COMPLEX] = number_coerce_mrational_to_complex
    },
    [NUMBER_MFLOAT] = {
        [NUMBER_MFLOAT] = number_coerce_clone_mfloat,
        [NUMBER_COMPLEX] = number_coerce_mfloat_to_complex
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
