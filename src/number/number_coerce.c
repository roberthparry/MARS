#include "number.h"
#include "number_internal.h"

static const number_kind_t number_common_kind_table[][NUMBER_MCOMPLEX + 1] = {
    [NUMBER_INVALID] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_DOUBLE] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_DOUBLE,
        [NUMBER_QFLOAT] = NUMBER_QFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_QFLOAT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_QFLOAT,
        [NUMBER_QFLOAT] = NUMBER_QFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_QCOMPLEX] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_QCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_QCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MINT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MINT,
        [NUMBER_MRATIONAL] = NUMBER_MRATIONAL,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MRATIONAL] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MRATIONAL,
        [NUMBER_MRATIONAL] = NUMBER_MRATIONAL,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MFLOAT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MCOMPLEX] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    }
};

static const number_kind_t number_common_kind_div_table[][NUMBER_MCOMPLEX + 1] = {
    [NUMBER_INVALID] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_DOUBLE] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_DOUBLE,
        [NUMBER_QFLOAT] = NUMBER_QFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_QFLOAT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_QFLOAT,
        [NUMBER_QFLOAT] = NUMBER_QFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_QCOMPLEX] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_QCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_QCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_QCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MINT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MRATIONAL,
        [NUMBER_MRATIONAL] = NUMBER_MRATIONAL,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MRATIONAL] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MRATIONAL,
        [NUMBER_MRATIONAL] = NUMBER_MRATIONAL,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MFLOAT] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MFLOAT,
        [NUMBER_QFLOAT] = NUMBER_MFLOAT,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MFLOAT,
        [NUMBER_MRATIONAL] = NUMBER_MFLOAT,
        [NUMBER_MFLOAT] = NUMBER_MFLOAT,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    },
    [NUMBER_MCOMPLEX] = {
        [NUMBER_INVALID] = NUMBER_MCOMPLEX,
        [NUMBER_DOUBLE] = NUMBER_MCOMPLEX,
        [NUMBER_QFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_QCOMPLEX] = NUMBER_MCOMPLEX,
        [NUMBER_MINT] = NUMBER_MCOMPLEX,
        [NUMBER_MRATIONAL] = NUMBER_MCOMPLEX,
        [NUMBER_MFLOAT] = NUMBER_MCOMPLEX,
        [NUMBER_MCOMPLEX] = NUMBER_MCOMPLEX
    }
};

number_kind_t number_common_kind(const number_t *a,
                                 const number_t *b,
                                 number_binary_op_t op)
{
    number_kind_t ak;
    number_kind_t bk;
    const number_kind_t (*table)[NUMBER_MCOMPLEX + 1];

    if (!a || !b)
        return NUMBER_MCOMPLEX;
    ak = number_impl_const(a)->kind;
    bk = number_impl_const(b)->kind;
    if ((unsigned)ak > NUMBER_MCOMPLEX || (unsigned)bk > NUMBER_MCOMPLEX)
        return NUMBER_MCOMPLEX;
    table = op == NUMBER_OP_DIV ? number_common_kind_div_table
                                : number_common_kind_table;
    return table[ak][bk];
}

static number_t *number_create_mcomplex_from_mfloat(const mfloat_t *real)
{
    mcomplex_t *complex_value;

    if (!real)
        return NULL;
    complex_value = mc_create(real, MF_ZERO);
    return complex_value ? number_wrap_mcomplex(complex_value) : NULL;
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

static mcomplex_t *number_reprecision_mcomplex(mcomplex_t *value, size_t precision_bits)
{
    if (!value)
        return NULL;
    if (mc_set_precision(value, precision_bits) != 0) {
        mc_free(value);
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

static number_t *number_coerce_double_to_mcomplex(const number_t *number)
{
    number_t *real;
    number_t *result;

    if (!number)
        return NULL;
    real = number_coerce_double_to_mfloat(number);
    if (!real)
        return NULL;
    result = number_create_mcomplex_from_mfloat(number_impl_const(real)->value.mf);
    number_box_free(real);
    return result;
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

static number_t *number_coerce_qfloat_to_mcomplex(const number_t *number)
{
    number_t *real;
    number_t *result;

    if (!number)
        return NULL;
    real = number_coerce_qfloat_to_mfloat(number);
    if (!real)
        return NULL;
    result = number_create_mcomplex_from_mfloat(number_impl_const(real)->value.mf);
    number_box_free(real);
    return result;
}

static number_t *number_coerce_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

static number_t *number_coerce_qcomplex_to_mcomplex(const number_t *number)
{
    return number ? number_wrap_mcomplex(number_reprecision_mcomplex(
                        mc_create_qcomplex(number_impl_const(number)->value.qc),
                        number_default_precision_bits))
                  : NULL;
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

static number_t *number_coerce_mint_to_mcomplex(const number_t *number)
{
    number_t *real;
    number_t *result;

    if (!number)
        return NULL;
    real = number_coerce_mint_to_mfloat(number);
    if (!real)
        return NULL;
    result = number_create_mcomplex_from_mfloat(number_impl_const(real)->value.mf);
    number_box_free(real);
    return result;
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

static number_t *number_coerce_mrational_to_mcomplex(const number_t *number)
{
    number_t *real;
    number_t *result;

    if (!number)
        return NULL;
    real = number_coerce_mrational_to_mfloat(number);
    if (!real)
        return NULL;
    result = number_create_mcomplex_from_mfloat(number_impl_const(real)->value.mf);
    number_box_free(real);
    return result;
}

static number_t *number_coerce_clone_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    return copy ? number_wrap_mfloat(copy) : NULL;
}

static number_t *number_coerce_mfloat_to_mcomplex(const number_t *number)
{
    return number ? number_create_mcomplex_from_mfloat(number_impl_const(number)->value.mf) : NULL;
}

static number_t *number_coerce_clone_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    return copy ? number_wrap_mcomplex(copy) : NULL;
}

static const number_coerce_fn number_coerce_dispatch[][NUMBER_MCOMPLEX + 1] = {
    [NUMBER_INVALID] = {
        [NUMBER_INVALID] = number_coerce_invalid,
        [NUMBER_DOUBLE] = number_coerce_invalid,
        [NUMBER_QFLOAT] = number_coerce_invalid,
        [NUMBER_QCOMPLEX] = number_coerce_invalid,
        [NUMBER_MINT] = number_coerce_invalid,
        [NUMBER_MRATIONAL] = number_coerce_invalid,
        [NUMBER_MFLOAT] = number_coerce_invalid,
        [NUMBER_MCOMPLEX] = number_coerce_invalid
    },
    [NUMBER_DOUBLE] = {
        [NUMBER_DOUBLE] = number_coerce_clone_double,
        [NUMBER_QFLOAT] = number_coerce_double_to_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_double_to_qcomplex,
        [NUMBER_MFLOAT] = number_coerce_double_to_mfloat,
        [NUMBER_MCOMPLEX] = number_coerce_double_to_mcomplex
    },
    [NUMBER_QFLOAT] = {
        [NUMBER_QFLOAT] = number_coerce_clone_qfloat,
        [NUMBER_QCOMPLEX] = number_coerce_qfloat_to_qcomplex,
        [NUMBER_MFLOAT] = number_coerce_qfloat_to_mfloat,
        [NUMBER_MCOMPLEX] = number_coerce_qfloat_to_mcomplex
    },
    [NUMBER_QCOMPLEX] = {
        [NUMBER_QCOMPLEX] = number_coerce_clone_qcomplex,
        [NUMBER_MCOMPLEX] = number_coerce_qcomplex_to_mcomplex
    },
    [NUMBER_MINT] = {
        [NUMBER_MINT] = number_coerce_clone_mint,
        [NUMBER_MRATIONAL] = number_coerce_mint_to_mrational,
        [NUMBER_MFLOAT] = number_coerce_mint_to_mfloat,
        [NUMBER_MCOMPLEX] = number_coerce_mint_to_mcomplex
    },
    [NUMBER_MRATIONAL] = {
        [NUMBER_MRATIONAL] = number_coerce_clone_mrational,
        [NUMBER_MFLOAT] = number_coerce_mrational_to_mfloat,
        [NUMBER_MCOMPLEX] = number_coerce_mrational_to_mcomplex
    },
    [NUMBER_MFLOAT] = {
        [NUMBER_MFLOAT] = number_coerce_clone_mfloat,
        [NUMBER_MCOMPLEX] = number_coerce_mfloat_to_mcomplex
    },
    [NUMBER_MCOMPLEX] = {
        [NUMBER_MCOMPLEX] = number_coerce_clone_mcomplex
    }
};

number_t *number_coerce(const number_t *number, number_kind_t target_kind)
{
    number_kind_t source_kind;
    number_coerce_fn fn;

    if (!number)
        return NULL;
    source_kind = number_impl_const(number)->kind;
    if ((size_t)source_kind > NUMBER_MCOMPLEX || (size_t)target_kind > NUMBER_MCOMPLEX)
        return NULL;
    fn = number_coerce_dispatch[source_kind][target_kind];
    return fn ? fn(number) : NULL;
}
