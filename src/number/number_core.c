#include "number.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum number_kind_t {
    NUMBER_INVALID,
    NUMBER_DOUBLE,
    NUMBER_QFLOAT,
    NUMBER_QCOMPLEX,
    NUMBER_MINT,
    NUMBER_MRATIONAL,
    NUMBER_MFLOAT,
    NUMBER_MCOMPLEX
} number_kind_t;

typedef enum number_math_family_t {
    NUMBER_MATH_INVALID,
    NUMBER_MATH_QREAL,
    NUMBER_MATH_QCOMPLEX,
    NUMBER_MATH_MREAL,
    NUMBER_MATH_MCOMPLEX
} number_math_family_t;

typedef struct number_vtable_t {
    number_kind_t kind;
    number_math_family_t math_family;
    bool exact;
    bool complex;
    void (*destroy_payload)(number_t *number);
    number_t *(*clone)(const number_t *number);
    char *(*to_string)(const number_t *number);
    bool (*is_real)(const number_t *number);
    bool (*is_zero)(const number_t *number);
    bool (*is_one)(const number_t *number);
    bool (*is_finite)(const number_t *number);
    bool (*is_nan)(const number_t *number);
    bool (*is_inf)(const number_t *number);
    bool (*eq_same)(const number_t *a, const number_t *b);
    int (*cmp_same)(const number_t *a, const number_t *b);
    char *(*format_inexact)(const number_t *number, bool scientific, int precision);
    int (*set_precision)(number_t *number, size_t precision_bits);
    size_t (*get_precision)(const number_t *number);
    long (*get_exponent2)(const number_t *number);
    double (*to_double)(const number_t *number);
    qfloat_t (*to_qfloat)(const number_t *number);
    bool (*is_integer)(const number_t *number);
    size_t (*get_mantissa_bits)(const number_t *number);
    bool (*get_mantissa_u64)(const number_t *number, uint64_t *out);
    int (*sign)(const number_t *number);
    number_t *(*neg)(const number_t *number);
    number_t *(*abs_value)(const number_t *number);
    number_t *(*inv)(const number_t *number);
    number_t *(*conj_value)(const number_t *number);
    number_t *(*real_part)(const number_t *number);
    number_t *(*imag_part)(const number_t *number);
    number_t *(*arg_value)(const number_t *number);
    number_t *(*pow_int)(const number_t *number, int exponent);
    number_t *(*mul_pow10_value)(const number_t *number, int exponent10);
    number_t *(*ldexp_value)(const number_t *number, int exponent2);
    number_t *(*floor_value)(const number_t *number);
    int (*sincos_value)(const number_t *number, number_t *sin_out, number_t *cos_out);
    int (*sinhcosh_value)(const number_t *number, number_t *sinh_out, number_t *cosh_out);
    number_t *(*add_same)(const number_t *a, const number_t *b);
    number_t *(*sub_same)(const number_t *a, const number_t *b);
    number_t *(*mul_same)(const number_t *a, const number_t *b);
    number_t *(*div_same)(const number_t *a, const number_t *b);
    number_t *(*exp_same)(const number_t *number);
    number_t *(*log_same)(const number_t *number);
    number_t *(*sqrt_same)(const number_t *number);
} number_vtable_t;

typedef number_t *(*number_coerce_fn)(const number_t *number);

typedef struct {
    number_kind_t kind;
    union {
        double d;
        qfloat_t qf;
        qcomplex_t qc;
        mint_t *mi;
        mrational_t *mr;
        mfloat_t *mf;
        mcomplex_t *mc;
    } value;
} number_private_t;

typedef enum number_binary_op_t {
    NUMBER_OP_ADD,
    NUMBER_OP_SUB,
    NUMBER_OP_MUL,
    NUMBER_OP_DIV
} number_binary_op_t;

static const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_MCOMPLEX + 1] = {
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

static const number_kind_t number_math_family_target_kind_table[] = {
    [NUMBER_MATH_INVALID] = NUMBER_INVALID,
    [NUMBER_MATH_QREAL] = NUMBER_QFLOAT,
    [NUMBER_MATH_QCOMPLEX] = NUMBER_QCOMPLEX,
    [NUMBER_MATH_MREAL] = NUMBER_MFLOAT,
    [NUMBER_MATH_MCOMPLEX] = NUMBER_MCOMPLEX
};

static size_t number_default_precision_bits = 1024u;

static inline number_private_t *number_impl(number_t *number)
{
    return (number_private_t *)number;
}

static inline const number_private_t *number_impl_const(const number_t *number)
{
    return (const number_private_t *)number;
}

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

static number_t number_invalid(void)
{
    number_t number;

    memset(&number, 0, sizeof(number));
    number_impl(&number)->kind = NUMBER_INVALID;
    return number;
}

static inline bool number_is_valid_value(const number_t *number)
{
    return number != NULL && number_impl_const(number)->kind != NUMBER_INVALID;
}

static number_t number_take(number_t *boxed_number)
{
    number_t value;

    if (!boxed_number)
        return number_invalid();
    memcpy(&value, boxed_number, sizeof(value));
    free(boxed_number);
    return value;
}

static char *number_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text)
        return NULL;
    len = strlen(text);
    copy = malloc(len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
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

static number_t *number_wrap_double(double value);
static number_t *number_wrap_qfloat(qfloat_t value);
static number_t *number_wrap_qcomplex(qcomplex_t value);
static number_t *number_wrap_mint(mint_t *value);
static number_t *number_wrap_mrational(mrational_t *value);
static number_t *number_wrap_mfloat(mfloat_t *value);
static number_t *number_wrap_mcomplex(mcomplex_t *value);
static char *number_format_double(const number_t *number, bool scientific, int precision);
static char *number_format_qfloat(const number_t *number, bool scientific, int precision);
static char *number_format_qcomplex(const number_t *number, bool scientific, int precision);
static char *number_format_mfloat(const number_t *number, bool scientific, int precision);
static char *number_format_mcomplex(const number_t *number, bool scientific, int precision);
static void number_box_free(number_t *number);
static void number_assign(number_t *dst, number_t value);
static number_t *number_coerce(const number_t *number, number_kind_t target_kind);
static number_t *number_coerce_invalid(const number_t *number);
static number_t *number_coerce_clone_double(const number_t *number);
static number_t *number_coerce_double_to_qfloat(const number_t *number);
static number_t *number_coerce_double_to_qcomplex(const number_t *number);
static number_t *number_coerce_double_to_mfloat(const number_t *number);
static number_t *number_coerce_double_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_qfloat(const number_t *number);
static number_t *number_coerce_qfloat_to_qcomplex(const number_t *number);
static number_t *number_coerce_qfloat_to_mfloat(const number_t *number);
static number_t *number_coerce_qfloat_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_qcomplex(const number_t *number);
static number_t *number_coerce_qcomplex_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_mint(const number_t *number);
static number_t *number_coerce_mint_to_mrational(const number_t *number);
static number_t *number_coerce_mint_to_mfloat(const number_t *number);
static number_t *number_coerce_mint_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_mrational(const number_t *number);
static number_t *number_coerce_mrational_to_mfloat(const number_t *number);
static number_t *number_coerce_mrational_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_mfloat(const number_t *number);
static number_t *number_coerce_mfloat_to_mcomplex(const number_t *number);
static number_t *number_coerce_clone_mcomplex(const number_t *number);

static number_t number_wrap_mfloat_borrowed(const mfloat_t *value)
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

static number_t number_wrap_mfloat_with_precision(mfloat_t *value, size_t precision_bits)
{
    if (!value)
        return number_invalid();
    if (mf_set_precision(value, precision_bits) != 0) {
        mf_free(value);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(value));
}

static number_t number_wrap_mcomplex_with_precision(mcomplex_t *value, size_t precision_bits)
{
    if (!value)
        return number_invalid();
    if (mc_set_precision(value, precision_bits) != 0) {
        mc_free(value);
        return number_invalid();
    }
    return number_take(number_wrap_mcomplex(value));
}

static int number_set_precision_noop(number_t *number, size_t precision_bits)
{
    (void)number;
    return precision_bits == 0u ? -1 : 0;
}

static size_t number_precision_fixed53(const number_t *number)
{
    (void)number;
    return 53u;
}

static size_t number_precision_fixed106(const number_t *number)
{
    (void)number;
    return 106u;
}

static size_t number_precision_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

static void number_destroy_none(number_t *number)
{
    (void)number;
}

static void number_destroy_mint(number_t *number)
{
    if (number)
        mi_free(number_impl(number)->value.mi);
}

static void number_destroy_mrational(number_t *number)
{
    if (number)
        mr_free(number_impl(number)->value.mr);
}

static void number_destroy_mfloat(number_t *number)
{
    if (!number || !number_impl(number)->value.mf)
        return;
    mf_free(number_impl(number)->value.mf);
}

static void number_destroy_mcomplex(number_t *number)
{
    if (number)
        mc_free(number_impl(number)->value.mc);
}

static bool number_is_real_default(const number_t *number)
{
    return number != NULL;
}

static bool number_is_zero_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 0.0;
}

static bool number_is_zero_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ZERO);
}

static bool number_is_zero_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ZERO);
}

static bool number_is_zero_mint(const number_t *number)
{
    return number && mi_is_zero(number_impl_const(number)->value.mi);
}

static bool number_is_zero_mrational(const number_t *number)
{
    return number && mr_is_zero(number_impl_const(number)->value.mr);
}

static bool number_is_zero_mfloat(const number_t *number)
{
    return number && mf_is_zero(number_impl_const(number)->value.mf);
}

static bool number_is_zero_mcomplex(const number_t *number)
{
    return number && mc_is_zero(number_impl_const(number)->value.mc);
}

static bool number_is_one_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d == 1.0;
}

static bool number_is_one_qfloat(const number_t *number)
{
    return number && qf_eq(number_impl_const(number)->value.qf, QF_ONE);
}

static bool number_is_real_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO);
}

static bool number_is_one_qcomplex(const number_t *number)
{
    return number && qc_eq(number_impl_const(number)->value.qc, QC_ONE);
}

static bool number_is_one_mint(const number_t *number)
{
    return number && number_impl_const(number)->value.mi &&
        mi_cmp(number_impl_const(number)->value.mi, MI_ONE) == 0;
}

static bool number_is_one_mrational(const number_t *number)
{
    if (!number || !number_impl_const(number)->value.mr)
        return false;
    return mr_is_integer(number_impl_const(number)->value.mr) &&
        mi_cmp(mr_numerator(number_impl_const(number)->value.mr), MI_ONE) == 0;
}

static bool number_is_one_mfloat(const number_t *number)
{
    return number && mf_eq(number_impl_const(number)->value.mf, MF_ONE);
}

static bool number_is_real_mcomplex(const number_t *number)
{
    return number && mf_is_zero(mc_imag(number_impl_const(number)->value.mc));
}

static bool number_is_one_mcomplex(const number_t *number)
{
    return number && mc_eq(number_impl_const(number)->value.mc, MC_ONE);
}

static bool number_eq_same_double(const number_t *a, const number_t *b)
{
    return a && b && number_impl_const(a)->value.d == number_impl_const(b)->value.d;
}

static bool number_eq_same_qfloat(const number_t *a, const number_t *b)
{
    return a && b &&
        qf_eq(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf);
}

static bool number_eq_same_qcomplex(const number_t *a, const number_t *b)
{
    return a && b &&
        qc_eq(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc);
}

static bool number_eq_same_mint(const number_t *a, const number_t *b)
{
    return a && b &&
        mi_cmp(number_impl_const(a)->value.mi, number_impl_const(b)->value.mi) == 0;
}

static bool number_eq_same_mrational(const number_t *a, const number_t *b)
{
    return a && b &&
        mr_eq(number_impl_const(a)->value.mr, number_impl_const(b)->value.mr);
}

static bool number_eq_same_mfloat(const number_t *a, const number_t *b)
{
    return a && b &&
        mf_eq(number_impl_const(a)->value.mf, number_impl_const(b)->value.mf);
}

static bool number_eq_same_mcomplex(const number_t *a, const number_t *b)
{
    return a && b &&
        mc_eq(number_impl_const(a)->value.mc, number_impl_const(b)->value.mc);
}

static bool number_is_finite_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d);
}

static bool number_is_finite_qfloat(const number_t *number)
{
    return number && !qf_isnan(number_impl_const(number)->value.qf) &&
        !qf_isinf(number_impl_const(number)->value.qf);
}

static bool number_is_finite_qcomplex(const number_t *number)
{
    return number && !qc_isnan(number_impl_const(number)->value.qc) &&
        !qc_isinf(number_impl_const(number)->value.qc);
}

static bool number_is_finite_exact(const number_t *number)
{
    return number != NULL;
}

static bool number_is_finite_mfloat(const number_t *number)
{
    return number && mf_is_finite(number_impl_const(number)->value.mf);
}

static bool number_is_finite_mcomplex(const number_t *number)
{
    return number && !mc_isnan(number_impl_const(number)->value.mc) &&
        !mc_isinf(number_impl_const(number)->value.mc) &&
        mf_is_finite(mc_real(number_impl_const(number)->value.mc)) &&
        mf_is_finite(mc_imag(number_impl_const(number)->value.mc));
}

static bool number_is_nan_double(const number_t *number)
{
    return !number || isnan(number_impl_const(number)->value.d);
}

static bool number_is_nan_qfloat(const number_t *number)
{
    return !number || qf_isnan(number_impl_const(number)->value.qf);
}

static bool number_is_nan_qcomplex(const number_t *number)
{
    return !number || qc_isnan(number_impl_const(number)->value.qc);
}

static bool number_is_nan_exact(const number_t *number)
{
    (void)number;
    return false;
}

static bool number_is_nan_mfloat(const number_t *number)
{
    return !number || mf_is_nan(number_impl_const(number)->value.mf);
}

static bool number_is_nan_mcomplex(const number_t *number)
{
    return !number || mc_isnan(number_impl_const(number)->value.mc);
}

static bool number_is_inf_double(const number_t *number)
{
    return number && isinf(number_impl_const(number)->value.d);
}

static bool number_is_inf_qfloat(const number_t *number)
{
    return number && qf_isinf(number_impl_const(number)->value.qf);
}

static bool number_is_inf_qcomplex(const number_t *number)
{
    return number && qc_isinf(number_impl_const(number)->value.qc);
}

static bool number_is_inf_exact(const number_t *number)
{
    (void)number;
    return false;
}

static bool number_is_inf_mfloat(const number_t *number)
{
    return number && mf_is_inf(number_impl_const(number)->value.mf);
}

static bool number_is_inf_mcomplex(const number_t *number)
{
    return number && mc_isinf(number_impl_const(number)->value.mc);
}

static int number_cmp_same_double(const number_t *a, const number_t *b)
{
    double av, bv;

    if (!a || !b)
        return 0;
    av = number_impl_const(a)->value.d;
    bv = number_impl_const(b)->value.d;
    return av < bv ? -1 : (av > bv ? 1 : 0);
}

static int number_cmp_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? qf_cmp(number_impl_const(a)->value.qf,
                             number_impl_const(b)->value.qf) : 0;
}

static int number_cmp_same_qcomplex(const number_t *a, const number_t *b)
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

static int number_cmp_same_mint(const number_t *a, const number_t *b)
{
    return (a && b) ? mi_cmp(number_impl_const(a)->value.mi,
                             number_impl_const(b)->value.mi) : 0;
}

static int number_cmp_same_mrational(const number_t *a, const number_t *b)
{
    return (a && b) ? mr_cmp(number_impl_const(a)->value.mr,
                             number_impl_const(b)->value.mr) : 0;
}

static int number_cmp_same_mfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? mf_cmp(number_impl_const(a)->value.mf,
                             number_impl_const(b)->value.mf) : 0;
}

static int number_cmp_same_mcomplex(const number_t *a, const number_t *b)
{
    int rc;

    if (!a || !b)
        return 0;
    rc = mf_cmp(mc_real(number_impl_const(a)->value.mc),
                mc_real(number_impl_const(b)->value.mc));
    return rc != 0 ? rc : mf_cmp(mc_imag(number_impl_const(a)->value.mc),
                                 mc_imag(number_impl_const(b)->value.mc));
}

static int number_set_precision_mfloat(number_t *number, size_t precision_bits)
{
    return number && number_impl(number)->value.mf ?
        mf_set_precision(number_impl(number)->value.mf, precision_bits) : -1;
}

static size_t number_get_precision_mfloat(const number_t *number)
{
    return number && number_impl_const(number)->value.mf ?
        mf_get_precision(number_impl_const(number)->value.mf) : 0u;
}

static int number_set_precision_mcomplex(number_t *number, size_t precision_bits)
{
    return number && number_impl(number)->value.mc ?
        mc_set_precision(number_impl(number)->value.mc, precision_bits) : -1;
}

static size_t number_get_precision_mcomplex(const number_t *number)
{
    return number && number_impl_const(number)->value.mc ?
        mc_get_precision(number_impl_const(number)->value.mc) : 0u;
}

static long number_get_exponent2_double(const number_t *number)
{
    int exp2;

    if (!number)
        return 0l;
    exp2 = ilogb(fabs(number_impl_const(number)->value.d));
    return exp2 == FP_ILOGB0 || exp2 == FP_ILOGBNAN ? 0l : (long)exp2;
}

static long number_get_exponent2_qfloat(const number_t *number)
{
    if (!number)
        return 0l;
    return qf_get_exponent2(number_impl_const(number)->value.qf);
}

static long number_get_exponent2_zero(const number_t *number)
{
    (void)number;
    return 0l;
}

static long number_get_exponent2_mint(const number_t *number)
{
    return number ? (long)mi_bit_length(number_impl_const(number)->value.mi) - 1l : 0l;
}

static long number_get_exponent2_mrational(const number_t *number)
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

static long number_get_exponent2_mfloat(const number_t *number)
{
    return number ? mf_get_exponent2(number_impl_const(number)->value.mf) : 0l;
}

static double number_to_double_double(const number_t *number)
{
    return number ? number_impl_const(number)->value.d : NAN;
}

static double number_to_double_qfloat(const number_t *number)
{
    return number ? qf_to_double(number_impl_const(number)->value.qf) : NAN;
}

static double number_to_double_mfloat(const number_t *number)
{
    return number ? mf_to_double(number_impl_const(number)->value.mf) : NAN;
}

static qfloat_t number_to_qfloat_double(const number_t *number)
{
    return number ? qf_from_double(number_impl_const(number)->value.d) : QF_NAN;
}

static qfloat_t number_to_qfloat_qfloat(const number_t *number)
{
    return number ? number_impl_const(number)->value.qf : QF_NAN;
}

static qfloat_t number_to_qfloat_mfloat(const number_t *number)
{
    return number ? mf_to_qfloat(number_impl_const(number)->value.mf) : QF_NAN;
}

static bool number_is_integer_double(const number_t *number)
{
    double x;

    if (!number)
        return false;
    x = number_impl_const(number)->value.d;
    return isfinite(x) && floor(x) == x;
}

static bool number_is_integer_qfloat(const number_t *number)
{
    return number && qf_eq(qf_floor(number_impl_const(number)->value.qf),
                           number_impl_const(number)->value.qf);
}

static bool number_is_integer_qcomplex(const number_t *number)
{
    return number && qf_eq(qc_imag(number_impl_const(number)->value.qc), QF_ZERO) &&
        qf_eq(qf_floor(qc_real(number_impl_const(number)->value.qc)),
              qc_real(number_impl_const(number)->value.qc));
}

static bool number_is_integer_mint(const number_t *number)
{
    return number != NULL;
}

static bool number_is_integer_mrational(const number_t *number)
{
    return number && mr_is_integer(number_impl_const(number)->value.mr);
}

static bool number_is_integer_mfloat(const number_t *number)
{
    number_t copy;
    number_t floored;
    bool rc;

    if (!number)
        return false;
    copy = num_clone(*number);
    floored = num_floor(copy);
    rc = num_eq(copy, floored);
    num_clear(&copy);
    num_clear(&floored);
    return rc;
}

static bool number_is_integer_mcomplex(const number_t *number)
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
    num_clear(&imag);
    num_clear(&real);
    num_clear(&floored);
    return rc;
}

static size_t number_get_mantissa_bits_double(const number_t *number)
{
    return number && isfinite(number_impl_const(number)->value.d) &&
            number_impl_const(number)->value.d != 0.0 ? 53u : 0u;
}

static size_t number_get_mantissa_bits_qfloat(const number_t *number)
{
    return number &&
            !qf_isnan(number_impl_const(number)->value.qf) &&
            !qf_isinf(number_impl_const(number)->value.qf) &&
            !qf_eq(number_impl_const(number)->value.qf, QF_ZERO) ? 106u : 0u;
}

static size_t number_get_mantissa_bits_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

static size_t number_get_mantissa_bits_mfloat(const number_t *number)
{
    return number ? mf_get_mantissa_bits(number_impl_const(number)->value.mf) : 0u;
}

static size_t number_get_mantissa_bits_mcomplex(const number_t *number)
{
    return number ? mf_get_mantissa_bits(mc_real(number_impl_const(number)->value.mc)) : 0u;
}

static bool number_get_mantissa_u64_false(const number_t *number, uint64_t *out)
{
    (void)number;
    (void)out;
    return false;
}

static bool number_get_mantissa_u64_mfloat(const number_t *number, uint64_t *out)
{
    return number && out &&
        mf_get_mantissa_u64(number_impl_const(number)->value.mf, out);
}

static bool number_get_mantissa_u64_mcomplex(const number_t *number, uint64_t *out)
{
    return number && out &&
        mf_get_mantissa_u64(mc_real(number_impl_const(number)->value.mc), out);
}

static int number_sign_double(const number_t *number)
{
    return number && number_impl_const(number)->value.d < 0.0 ? -1 : 1;
}

static int number_sign_qfloat(const number_t *number)
{
    return number && qf_lt(number_impl_const(number)->value.qf, QF_ZERO) ? -1 : 1;
}

static int number_sign_zero(const number_t *number)
{
    (void)number;
    return 0;
}

static int number_sign_mint(const number_t *number)
{
    return number && mi_is_negative(number_impl_const(number)->value.mi) ? -1 : 1;
}

static int number_sign_mrational(const number_t *number)
{
    return number && mi_is_negative(mr_numerator(number_impl_const(number)->value.mr)) ? -1 : 1;
}

static int number_sign_mfloat(const number_t *number)
{
    return number ? mf_get_sign(number_impl_const(number)->value.mf) : 0;
}

static char *number_to_string_double(const number_t *number)
{
    char buf[64];

    if (!number)
        return NULL;
    snprintf(buf, sizeof(buf), "%.17g", number_impl_const(number)->value.d);
    return number_strdup(buf);
}

static char *number_to_string_qfloat(const number_t *number)
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

static char *number_to_string_qcomplex(const number_t *number)
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

static char *number_to_string_mint(const number_t *number)
{
    return number ? mi_to_string(number_impl_const(number)->value.mi) : NULL;
}

static char *number_to_string_mrational(const number_t *number)
{
    return number ? mr_to_string(number_impl_const(number)->value.mr) : NULL;
}

static char *number_to_string_mfloat(const number_t *number)
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

static char *number_to_string_mcomplex(const number_t *number)
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

static number_t *number_clone_double(const number_t *number)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d) : NULL;
}

static number_t *number_clone_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(number_impl_const(number)->value.qf) : NULL;
}

static number_t *number_clone_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(number_impl_const(number)->value.qc) : NULL;
}

static number_t *number_clone_mint(const number_t *number)
{
    mint_t *copy;

    if (!number || !number_impl_const(number)->value.mi)
        return NULL;
    copy = mi_clone(number_impl_const(number)->value.mi);
    return copy ? number_wrap_mint(copy) : NULL;
}

static number_t *number_clone_mrational(const number_t *number)
{
    mrational_t *copy;

    if (!number || !number_impl_const(number)->value.mr)
        return NULL;
    copy = mr_clone(number_impl_const(number)->value.mr);
    return copy ? number_wrap_mrational(copy) : NULL;
}

static number_t *number_clone_mfloat(const number_t *number)
{
    mfloat_t *copy;

    if (!number || !number_impl_const(number)->value.mf)
        return NULL;
    copy = mf_clone(number_impl_const(number)->value.mf);
    return copy ? number_wrap_mfloat(copy) : NULL;
}

static number_t *number_clone_mcomplex(const number_t *number)
{
    mcomplex_t *copy;

    if (!number || !number_impl_const(number)->value.mc)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    return copy ? number_wrap_mcomplex(copy) : NULL;
}

static number_t *number_neg_double(const number_t *number)
{
    return number ? number_wrap_double(-number_impl_const(number)->value.d) : NULL;
}

static number_t *number_neg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_neg(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_neg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_neg(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_neg_mint(const number_t *number)
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

static number_t *number_neg_mrational(const number_t *number)
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

static number_t *number_neg_mfloat(const number_t *number)
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

static number_t *number_neg_mcomplex(const number_t *number)
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

static number_t *number_inv_double(const number_t *number)
{
    return number ? number_wrap_double(1.0 / number_impl_const(number)->value.d) : NULL;
}

static number_t *number_inv_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_div(QF_ONE, number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_inv_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_div(QC_ONE, number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_inv_mrational(const number_t *number)
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

static number_t *number_inv_mfloat(const number_t *number)
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

static number_t *number_inv_mcomplex(const number_t *number)
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

static number_t *number_abs_double(const number_t *number)
{
    return number ? number_wrap_double(fabs(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_abs_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_abs(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_abs_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_abs(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_abs_mint(const number_t *number)
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

static number_t *number_abs_mrational(const number_t *number)
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

static number_t *number_abs_mfloat(const number_t *number)
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

static number_t *number_abs_mcomplex(const number_t *number)
{
    mcomplex_t *copy;
    number_t *result;
    mfloat_t *real_copy;

    if (!number)
        return NULL;
    copy = mc_clone(number_impl_const(number)->value.mc);
    if (!copy || mc_abs(copy) != 0) {
        mc_free(copy);
        return NULL;
    }
    real_copy = mf_clone(mc_real(copy));
    result = real_copy ? number_wrap_mfloat(real_copy) : NULL;
    mc_free(copy);
    return result;
}

static number_t *number_conj_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_conj(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_conj_mcomplex(const number_t *number)
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

static number_t *number_real_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_real(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_real_mcomplex(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(mc_real(number_impl_const(number)->value.mc));
    return copy ? number_wrap_mfloat(copy) : NULL;
}

static number_t *number_imag_double_zero(const number_t *number)
{
    (void)number;
    return number_wrap_double(0.0);
}

static number_t *number_imag_qfloat_zero(const number_t *number)
{
    (void)number;
    return number_wrap_qfloat(QF_ZERO);
}

static number_t *number_imag_mint_zero(const number_t *number)
{
    (void)number;
    return number_wrap_mint(mi_create_long(0L));
}

static number_t *number_imag_mrational_zero(const number_t *number)
{
    (void)number;
    return number_wrap_mrational(mr_create_mints(MI_ZERO, MI_ONE));
}

static number_t *number_imag_mfloat_zero(const number_t *number)
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

static number_t *number_imag_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_imag(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_imag_mcomplex(const number_t *number)
{
    mfloat_t *copy;

    if (!number)
        return NULL;
    copy = mf_clone(mc_imag(number_impl_const(number)->value.mc));
    return copy ? number_wrap_mfloat(copy) : NULL;
}

static number_t *number_arg_double(const number_t *number)
{
    return number ? number_wrap_double(atan2(0.0, number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_arg_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_atan2(QF_ZERO, number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_arg_qcomplex(const number_t *number)
{
    return number ? number_wrap_qfloat(qc_arg(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_arg_mcomplex(const number_t *number)
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

static number_t *number_floor_double(const number_t *number)
{
    return number ? number_wrap_double(floor(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_floor_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_floor(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_floor_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_floor(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_floor_mfloat(const number_t *number)
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

static number_t *number_floor_mcomplex(const number_t *number)
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

static number_t *number_pow_int_double(const number_t *number, int exponent)
{
    return number ? number_wrap_double(pow(number_impl_const(number)->value.d, (double)exponent)) : NULL;
}

static number_t *number_pow_int_qfloat(const number_t *number, int exponent)
{
    return number ? number_wrap_qfloat(qf_pow_int(number_impl_const(number)->value.qf, exponent)) : NULL;
}

static number_t *number_pow_int_qcomplex(const number_t *number, int exponent)
{
    return number ? number_wrap_qcomplex(qc_pow(number_impl_const(number)->value.qc,
        qc_make(qf_from_double((double)exponent), QF_ZERO))) : NULL;
}

static number_t *number_pow_int_mint(const number_t *number, int exponent)
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

static number_t *number_pow_int_mfloat(const number_t *number, int exponent)
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

static number_t *number_pow_int_mcomplex(const number_t *number, int exponent)
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

static number_t *number_ldexp_double(const number_t *number, int exponent2)
{
    return number ? number_wrap_double(ldexp(number_impl_const(number)->value.d, exponent2)) : NULL;
}

static number_t *number_ldexp_qfloat(const number_t *number, int exponent2)
{
    return number ? number_wrap_qfloat(qf_ldexp(number_impl_const(number)->value.qf, exponent2)) : NULL;
}

static number_t *number_ldexp_qcomplex(const number_t *number, int exponent2)
{
    return number ? number_wrap_qcomplex(qc_ldexp(number_impl_const(number)->value.qc, exponent2)) : NULL;
}

static number_t *number_ldexp_mfloat(const number_t *number, int exponent2)
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

static number_t *number_ldexp_mcomplex(const number_t *number, int exponent2)
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

static int number_sincos_double(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    number_assign(sin_out, number_take(number_wrap_double(sin(number_impl_const(number)->value.d))));
    number_assign(cos_out, number_take(number_wrap_double(cos(number_impl_const(number)->value.d))));
    return 0;
}

static int number_sincos_qfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    if (!number || !sin_out || !cos_out)
        return -1;
    number_assign(sin_out, number_take(number_wrap_qfloat(qf_sin(number_impl_const(number)->value.qf))));
    number_assign(cos_out, number_take(number_wrap_qfloat(qf_cos(number_impl_const(number)->value.qf))));
    return 0;
}

static int number_sincos_mfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
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
    number_assign(sin_out, number_take(number_wrap_mfloat(s)));
    number_assign(cos_out, number_take(number_wrap_mfloat(c)));
    return 0;
}

static int number_sinhcosh_double(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    if (!number || !sinh_out || !cosh_out)
        return -1;
    number_assign(sinh_out, number_take(number_wrap_double(sinh(number_impl_const(number)->value.d))));
    number_assign(cosh_out, number_take(number_wrap_double(cosh(number_impl_const(number)->value.d))));
    return 0;
}

static int number_sinhcosh_qfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    if (!number || !sinh_out || !cosh_out)
        return -1;
    number_assign(sinh_out, number_take(number_wrap_qfloat(qf_sinh(number_impl_const(number)->value.qf))));
    number_assign(cosh_out, number_take(number_wrap_qfloat(qf_cosh(number_impl_const(number)->value.qf))));
    return 0;
}

static int number_sinhcosh_mfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
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
    number_assign(sinh_out, number_take(number_wrap_mfloat(s)));
    number_assign(cosh_out, number_take(number_wrap_mfloat(c)));
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
    number_assign(first_out, number_take(number_wrap_mfloat(first)));
    number_assign(second_out, number_take(number_wrap_mfloat(second)));
    return 0;
}

static int number_sincos_real_mfloat(const number_t *number, number_t *sin_out, number_t *cos_out)
{
    return number_pair_real_mfloat(number, sin_out, cos_out, mf_sincos);
}

static int number_sinhcosh_real_mfloat(const number_t *number, number_t *sinh_out, number_t *cosh_out)
{
    return number_pair_real_mfloat(number, sinh_out, cosh_out, mf_sinhcosh);
}

static number_t *number_mul_pow10_double(const number_t *number, int exponent10)
{
    return number ? number_wrap_double(number_impl_const(number)->value.d * pow(10.0, (double)exponent10)) : NULL;
}

static number_t *number_mul_pow10_qfloat(const number_t *number, int exponent10)
{
    return number ? number_wrap_qfloat(qf_mul_pow10(number_impl_const(number)->value.qf, exponent10)) : NULL;
}

static number_t *number_mul_pow10_mfloat(const number_t *number, int exponent10)
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

static number_t *number_add_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d + number_impl_const(b)->value.d) : NULL;
}

static number_t *number_sub_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d - number_impl_const(b)->value.d) : NULL;
}

static number_t *number_mul_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d * number_impl_const(b)->value.d) : NULL;
}

static number_t *number_div_same_double(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_double(number_impl_const(a)->value.d / number_impl_const(b)->value.d) : NULL;
}

static number_t *number_add_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_add(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

static number_t *number_sub_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_sub(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

static number_t *number_mul_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_mul(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

static number_t *number_div_same_qfloat(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qfloat(qf_div(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf)) : NULL;
}

static number_t *number_add_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_add(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

static number_t *number_sub_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_sub(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

static number_t *number_mul_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_mul(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

static number_t *number_div_same_qcomplex(const number_t *a, const number_t *b)
{
    return (a && b) ? number_wrap_qcomplex(qc_div(number_impl_const(a)->value.qc, number_impl_const(b)->value.qc)) : NULL;
}

static number_t *number_add_same_mint(const number_t *a, const number_t *b)
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

static number_t *number_sub_same_mint(const number_t *a, const number_t *b)
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

static number_t *number_mul_same_mint(const number_t *a, const number_t *b)
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

static number_t *number_add_same_mrational(const number_t *a, const number_t *b)
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

static number_t *number_sub_same_mrational(const number_t *a, const number_t *b)
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

static number_t *number_mul_same_mrational(const number_t *a, const number_t *b)
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

static number_t *number_div_same_mrational(const number_t *a, const number_t *b)
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

static number_t *number_add_same_mfloat(const number_t *a, const number_t *b)
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

static number_t *number_sub_same_mfloat(const number_t *a, const number_t *b)
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

static number_t *number_mul_same_mfloat(const number_t *a, const number_t *b)
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

static number_t *number_div_same_mfloat(const number_t *a, const number_t *b)
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

static number_t *number_add_same_mcomplex(const number_t *a, const number_t *b)
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

static number_t *number_sub_same_mcomplex(const number_t *a, const number_t *b)
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

static number_t *number_mul_same_mcomplex(const number_t *a, const number_t *b)
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

static number_t *number_div_same_mcomplex(const number_t *a, const number_t *b)
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

static number_t *number_exp_same_double(const number_t *number)
{
    return number ? number_wrap_double(exp(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_log_same_double(const number_t *number)
{
    return number ? number_wrap_double(log(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_sqrt_same_double(const number_t *number)
{
    return number ? number_wrap_double(sqrt(number_impl_const(number)->value.d)) : NULL;
}

static number_t *number_exp_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_exp(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_log_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_log(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_sqrt_same_qfloat(const number_t *number)
{
    return number ? number_wrap_qfloat(qf_sqrt(number_impl_const(number)->value.qf)) : NULL;
}

static number_t *number_exp_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_exp(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_log_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_log(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_sqrt_same_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_sqrt(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_exp_same_mfloat(const number_t *number)
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

static number_t *number_log_same_mfloat(const number_t *number)
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

static number_t *number_sqrt_same_mfloat(const number_t *number)
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

static number_t *number_exp_same_mcomplex(const number_t *number)
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

static number_t *number_log_same_mcomplex(const number_t *number)
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

static number_t *number_sqrt_same_mcomplex(const number_t *number)
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

static const number_vtable_t number_double_vt = {
    .kind = NUMBER_DOUBLE,
    .math_family = NUMBER_MATH_QREAL,
    .exact = false,
    .complex = false,
    .destroy_payload = number_destroy_none,
    .clone = number_clone_double,
    .to_string = number_to_string_double,
    .is_real = number_is_real_default,
    .is_zero = number_is_zero_double,
    .is_one = number_is_one_double,
    .is_finite = number_is_finite_double,
    .is_nan = number_is_nan_double,
    .is_inf = number_is_inf_double,
    .eq_same = number_eq_same_double,
    .cmp_same = number_cmp_same_double,
    .format_inexact = number_format_double,
    .set_precision = number_set_precision_noop,
    .get_precision = number_precision_fixed53,
    .get_exponent2 = number_get_exponent2_double,
    .to_double = number_to_double_double,
    .to_qfloat = number_to_qfloat_double,
    .is_integer = number_is_integer_double,
    .get_mantissa_bits = number_get_mantissa_bits_double,
    .get_mantissa_u64 = number_get_mantissa_u64_false,
    .sign = number_sign_double,
    .neg = number_neg_double,
    .abs_value = number_abs_double,
    .inv = number_inv_double,
    .conj_value = number_clone_double,
    .real_part = number_clone_double,
    .imag_part = number_imag_double_zero,
    .arg_value = number_arg_double,
    .pow_int = number_pow_int_double,
    .mul_pow10_value = number_mul_pow10_double,
    .ldexp_value = number_ldexp_double,
    .floor_value = number_floor_double,
    .sincos_value = number_sincos_double,
    .sinhcosh_value = number_sinhcosh_double,
    .add_same = number_add_same_double,
    .sub_same = number_sub_same_double,
    .mul_same = number_mul_same_double,
    .div_same = number_div_same_double,
    .exp_same = number_exp_same_double,
    .log_same = number_log_same_double,
    .sqrt_same = number_sqrt_same_double
};

static const number_vtable_t number_qfloat_vt = {
    .kind = NUMBER_QFLOAT,
    .math_family = NUMBER_MATH_QREAL,
    .exact = false,
    .complex = false,
    .destroy_payload = number_destroy_none,
    .clone = number_clone_qfloat,
    .to_string = number_to_string_qfloat,
    .is_real = number_is_real_default,
    .is_zero = number_is_zero_qfloat,
    .is_one = number_is_one_qfloat,
    .is_finite = number_is_finite_qfloat,
    .is_nan = number_is_nan_qfloat,
    .is_inf = number_is_inf_qfloat,
    .eq_same = number_eq_same_qfloat,
    .cmp_same = number_cmp_same_qfloat,
    .format_inexact = number_format_qfloat,
    .set_precision = number_set_precision_noop,
    .get_precision = number_precision_fixed106,
    .get_exponent2 = number_get_exponent2_qfloat,
    .to_double = number_to_double_qfloat,
    .to_qfloat = number_to_qfloat_qfloat,
    .is_integer = number_is_integer_qfloat,
    .get_mantissa_bits = number_get_mantissa_bits_qfloat,
    .get_mantissa_u64 = number_get_mantissa_u64_false,
    .sign = number_sign_qfloat,
    .neg = number_neg_qfloat,
    .abs_value = number_abs_qfloat,
    .inv = number_inv_qfloat,
    .conj_value = number_clone_qfloat,
    .real_part = number_clone_qfloat,
    .imag_part = number_imag_qfloat_zero,
    .arg_value = number_arg_qfloat,
    .pow_int = number_pow_int_qfloat,
    .mul_pow10_value = number_mul_pow10_qfloat,
    .ldexp_value = number_ldexp_qfloat,
    .floor_value = number_floor_qfloat,
    .sincos_value = number_sincos_qfloat,
    .sinhcosh_value = number_sinhcosh_qfloat,
    .add_same = number_add_same_qfloat,
    .sub_same = number_sub_same_qfloat,
    .mul_same = number_mul_same_qfloat,
    .div_same = number_div_same_qfloat,
    .exp_same = number_exp_same_qfloat,
    .log_same = number_log_same_qfloat,
    .sqrt_same = number_sqrt_same_qfloat
};

static const number_vtable_t number_qcomplex_vt = {
    .kind = NUMBER_QCOMPLEX,
    .math_family = NUMBER_MATH_QCOMPLEX,
    .exact = false,
    .complex = true,
    .destroy_payload = number_destroy_none,
    .clone = number_clone_qcomplex,
    .to_string = number_to_string_qcomplex,
    .is_real = number_is_real_qcomplex,
    .is_zero = number_is_zero_qcomplex,
    .is_one = number_is_one_qcomplex,
    .is_finite = number_is_finite_qcomplex,
    .is_nan = number_is_nan_qcomplex,
    .is_inf = number_is_inf_qcomplex,
    .eq_same = number_eq_same_qcomplex,
    .cmp_same = number_cmp_same_qcomplex,
    .format_inexact = number_format_qcomplex,
    .set_precision = number_set_precision_noop,
    .get_precision = number_precision_fixed106,
    .get_exponent2 = number_get_exponent2_zero,
    .to_double = NULL,
    .to_qfloat = NULL,
    .is_integer = number_is_integer_qcomplex,
    .get_mantissa_bits = number_get_mantissa_bits_zero,
    .get_mantissa_u64 = number_get_mantissa_u64_false,
    .sign = number_sign_zero,
    .neg = number_neg_qcomplex,
    .abs_value = number_abs_qcomplex,
    .inv = number_inv_qcomplex,
    .conj_value = number_conj_qcomplex,
    .real_part = number_real_qcomplex,
    .imag_part = number_imag_qcomplex,
    .arg_value = number_arg_qcomplex,
    .pow_int = number_pow_int_qcomplex,
    .mul_pow10_value = NULL,
    .ldexp_value = number_ldexp_qcomplex,
    .floor_value = number_floor_qcomplex,
    .sincos_value = NULL,
    .sinhcosh_value = NULL,
    .add_same = number_add_same_qcomplex,
    .sub_same = number_sub_same_qcomplex,
    .mul_same = number_mul_same_qcomplex,
    .div_same = number_div_same_qcomplex,
    .exp_same = number_exp_same_qcomplex,
    .log_same = number_log_same_qcomplex,
    .sqrt_same = number_sqrt_same_qcomplex
};

static const number_vtable_t number_mint_vt = {
    .kind = NUMBER_MINT,
    .math_family = NUMBER_MATH_MREAL,
    .exact = true,
    .complex = false,
    .destroy_payload = number_destroy_mint,
    .clone = number_clone_mint,
    .to_string = number_to_string_mint,
    .is_real = number_is_real_default,
    .is_zero = number_is_zero_mint,
    .is_one = number_is_one_mint,
    .is_finite = number_is_finite_exact,
    .is_nan = number_is_nan_exact,
    .is_inf = number_is_inf_exact,
    .eq_same = number_eq_same_mint,
    .cmp_same = number_cmp_same_mint,
    .format_inexact = NULL,
    .set_precision = number_set_precision_noop,
    .get_precision = number_precision_zero,
    .get_exponent2 = number_get_exponent2_mint,
    .to_double = NULL,
    .to_qfloat = NULL,
    .is_integer = number_is_integer_mint,
    .get_mantissa_bits = number_get_mantissa_bits_zero,
    .get_mantissa_u64 = number_get_mantissa_u64_false,
    .sign = number_sign_mint,
    .neg = number_neg_mint,
    .abs_value = number_abs_mint,
    .inv = NULL,
    .conj_value = number_clone_mint,
    .real_part = number_clone_mint,
    .imag_part = number_imag_mint_zero,
    .arg_value = NULL,
    .pow_int = number_pow_int_mint,
    .mul_pow10_value = NULL,
    .ldexp_value = NULL,
    .floor_value = NULL,
    .sincos_value = number_sincos_real_mfloat,
    .sinhcosh_value = number_sinhcosh_real_mfloat,
    .add_same = number_add_same_mint,
    .sub_same = number_sub_same_mint,
    .mul_same = number_mul_same_mint,
    .div_same = NULL,
    .exp_same = NULL,
    .log_same = NULL,
    .sqrt_same = NULL
};

static const number_vtable_t number_mrational_vt = {
    .kind = NUMBER_MRATIONAL,
    .math_family = NUMBER_MATH_MREAL,
    .exact = true,
    .complex = false,
    .destroy_payload = number_destroy_mrational,
    .clone = number_clone_mrational,
    .to_string = number_to_string_mrational,
    .is_real = number_is_real_default,
    .is_zero = number_is_zero_mrational,
    .is_one = number_is_one_mrational,
    .is_finite = number_is_finite_exact,
    .is_nan = number_is_nan_exact,
    .is_inf = number_is_inf_exact,
    .eq_same = number_eq_same_mrational,
    .cmp_same = number_cmp_same_mrational,
    .format_inexact = NULL,
    .set_precision = number_set_precision_noop,
    .get_precision = number_precision_zero,
    .get_exponent2 = number_get_exponent2_mrational,
    .to_double = NULL,
    .to_qfloat = NULL,
    .is_integer = number_is_integer_mrational,
    .get_mantissa_bits = number_get_mantissa_bits_zero,
    .get_mantissa_u64 = number_get_mantissa_u64_false,
    .sign = number_sign_mrational,
    .neg = number_neg_mrational,
    .abs_value = number_abs_mrational,
    .inv = number_inv_mrational,
    .conj_value = number_clone_mrational,
    .real_part = number_clone_mrational,
    .imag_part = number_imag_mrational_zero,
    .arg_value = NULL,
    .pow_int = NULL,
    .mul_pow10_value = NULL,
    .ldexp_value = NULL,
    .floor_value = NULL,
    .sincos_value = number_sincos_real_mfloat,
    .sinhcosh_value = number_sinhcosh_real_mfloat,
    .add_same = number_add_same_mrational,
    .sub_same = number_sub_same_mrational,
    .mul_same = number_mul_same_mrational,
    .div_same = number_div_same_mrational,
    .exp_same = NULL,
    .log_same = NULL,
    .sqrt_same = NULL
};

static const number_vtable_t number_mfloat_vt = {
    .kind = NUMBER_MFLOAT,
    .math_family = NUMBER_MATH_MREAL,
    .exact = false,
    .complex = false,
    .destroy_payload = number_destroy_mfloat,
    .clone = number_clone_mfloat,
    .to_string = number_to_string_mfloat,
    .is_real = number_is_real_default,
    .is_zero = number_is_zero_mfloat,
    .is_one = number_is_one_mfloat,
    .is_finite = number_is_finite_mfloat,
    .is_nan = number_is_nan_mfloat,
    .is_inf = number_is_inf_mfloat,
    .eq_same = number_eq_same_mfloat,
    .cmp_same = number_cmp_same_mfloat,
    .format_inexact = number_format_mfloat,
    .set_precision = number_set_precision_mfloat,
    .get_precision = number_get_precision_mfloat,
    .get_exponent2 = number_get_exponent2_mfloat,
    .to_double = number_to_double_mfloat,
    .to_qfloat = number_to_qfloat_mfloat,
    .is_integer = number_is_integer_mfloat,
    .get_mantissa_bits = number_get_mantissa_bits_mfloat,
    .get_mantissa_u64 = number_get_mantissa_u64_mfloat,
    .sign = number_sign_mfloat,
    .neg = number_neg_mfloat,
    .abs_value = number_abs_mfloat,
    .inv = number_inv_mfloat,
    .conj_value = number_clone_mfloat,
    .real_part = number_clone_mfloat,
    .imag_part = number_imag_mfloat_zero,
    .arg_value = NULL,
    .pow_int = number_pow_int_mfloat,
    .mul_pow10_value = number_mul_pow10_mfloat,
    .ldexp_value = number_ldexp_mfloat,
    .floor_value = number_floor_mfloat,
    .sincos_value = number_sincos_mfloat,
    .sinhcosh_value = number_sinhcosh_mfloat,
    .add_same = number_add_same_mfloat,
    .sub_same = number_sub_same_mfloat,
    .mul_same = number_mul_same_mfloat,
    .div_same = number_div_same_mfloat,
    .exp_same = number_exp_same_mfloat,
    .log_same = number_log_same_mfloat,
    .sqrt_same = number_sqrt_same_mfloat
};

static const number_vtable_t number_mcomplex_vt = {
    .kind = NUMBER_MCOMPLEX,
    .math_family = NUMBER_MATH_MCOMPLEX,
    .exact = false,
    .complex = true,
    .destroy_payload = number_destroy_mcomplex,
    .clone = number_clone_mcomplex,
    .to_string = number_to_string_mcomplex,
    .is_real = number_is_real_mcomplex,
    .is_zero = number_is_zero_mcomplex,
    .is_one = number_is_one_mcomplex,
    .is_finite = number_is_finite_mcomplex,
    .is_nan = number_is_nan_mcomplex,
    .is_inf = number_is_inf_mcomplex,
    .eq_same = number_eq_same_mcomplex,
    .cmp_same = number_cmp_same_mcomplex,
    .format_inexact = number_format_mcomplex,
    .set_precision = number_set_precision_mcomplex,
    .get_precision = number_get_precision_mcomplex,
    .get_exponent2 = number_get_exponent2_zero,
    .to_double = NULL,
    .to_qfloat = NULL,
    .is_integer = number_is_integer_mcomplex,
    .get_mantissa_bits = number_get_mantissa_bits_mcomplex,
    .get_mantissa_u64 = number_get_mantissa_u64_mcomplex,
    .sign = number_sign_zero,
    .neg = number_neg_mcomplex,
    .abs_value = number_abs_mcomplex,
    .inv = number_inv_mcomplex,
    .conj_value = number_conj_mcomplex,
    .real_part = number_real_mcomplex,
    .imag_part = number_imag_mcomplex,
    .arg_value = number_arg_mcomplex,
    .pow_int = number_pow_int_mcomplex,
    .mul_pow10_value = NULL,
    .ldexp_value = number_ldexp_mcomplex,
    .floor_value = number_floor_mcomplex,
    .sincos_value = NULL,
    .sinhcosh_value = NULL,
    .add_same = number_add_same_mcomplex,
    .sub_same = number_sub_same_mcomplex,
    .mul_same = number_mul_same_mcomplex,
    .div_same = number_div_same_mcomplex,
    .exp_same = number_exp_same_mcomplex,
    .log_same = number_log_same_mcomplex,
    .sqrt_same = number_sqrt_same_mcomplex
};

static const number_vtable_t *const number_dispatch[] = {
    [NUMBER_DOUBLE] = &number_double_vt,
    [NUMBER_QFLOAT] = &number_qfloat_vt,
    [NUMBER_QCOMPLEX] = &number_qcomplex_vt,
    [NUMBER_MINT] = &number_mint_vt,
    [NUMBER_MRATIONAL] = &number_mrational_vt,
    [NUMBER_MFLOAT] = &number_mfloat_vt,
    [NUMBER_MCOMPLEX] = &number_mcomplex_vt
};

static inline const number_vtable_t *number_vt(const number_t *number)
{
    size_t kind;

    if (!number)
        return NULL;
    kind = (size_t)number_impl_const(number)->kind;
    return kind < (sizeof(number_dispatch) / sizeof(number_dispatch[0]))
        ? number_dispatch[kind] : NULL;
}

static void number_box_free(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = number_vt(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    free(number);
}

static number_t *number_wrap_double(double value)
{
    number_t *number = number_alloc(NUMBER_DOUBLE);

    if (number)
        number_impl(number)->value.d = value;
    return number;
}

static number_t *number_wrap_qfloat(qfloat_t value)
{
    number_t *number = number_alloc(NUMBER_QFLOAT);

    if (number)
        number_impl(number)->value.qf = value;
    return number;
}

static number_t *number_wrap_qcomplex(qcomplex_t value)
{
    number_t *number = number_alloc(NUMBER_QCOMPLEX);

    if (number)
        number_impl(number)->value.qc = value;
    return number;
}

static number_t *number_wrap_mint(mint_t *value)
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

static number_t *number_wrap_mrational(mrational_t *value)
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

static number_t *number_wrap_mfloat(mfloat_t *value)
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

static number_t *number_wrap_mcomplex(mcomplex_t *value)
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

static number_kind_t number_common_kind(const number_t *a,
                                        const number_t *b,
                                        number_binary_op_t op)
{
    number_kind_t ak, bk;
    const number_vtable_t *avt, *bvt;

    if (!a || !b)
        return NUMBER_MCOMPLEX;
    ak = number_impl_const(a)->kind;
    bk = number_impl_const(b)->kind;
    avt = number_vt(a);
    bvt = number_vt(b);

    if (ak == NUMBER_MCOMPLEX || bk == NUMBER_MCOMPLEX)
        return NUMBER_MCOMPLEX;
    if (ak == NUMBER_QCOMPLEX || bk == NUMBER_QCOMPLEX) {
        if (ak == NUMBER_DOUBLE || ak == NUMBER_QFLOAT || ak == NUMBER_QCOMPLEX) {
            if (bk == NUMBER_DOUBLE || bk == NUMBER_QFLOAT || bk == NUMBER_QCOMPLEX)
                return NUMBER_QCOMPLEX;
        }
        return NUMBER_MCOMPLEX;
    }
    if (ak == NUMBER_MFLOAT || bk == NUMBER_MFLOAT)
        return NUMBER_MFLOAT;
    if (ak == NUMBER_MRATIONAL || bk == NUMBER_MRATIONAL) {
        if (avt && bvt && avt->exact && bvt->exact)
            return NUMBER_MRATIONAL;
        return NUMBER_MFLOAT;
    }
    if (ak == NUMBER_MINT || bk == NUMBER_MINT) {
        if (ak == NUMBER_MINT && bk == NUMBER_MINT)
            return op == NUMBER_OP_DIV ? NUMBER_MRATIONAL : NUMBER_MINT;
        if ((ak == NUMBER_DOUBLE || ak == NUMBER_QFLOAT) ||
            (bk == NUMBER_DOUBLE || bk == NUMBER_QFLOAT))
            return NUMBER_MFLOAT;
    }
    if (ak == NUMBER_QFLOAT || bk == NUMBER_QFLOAT) {
        if ((avt && avt->exact) || (bvt && bvt->exact))
            return NUMBER_MFLOAT;
        return NUMBER_QFLOAT;
    }
    return NUMBER_DOUBLE;
}

static number_t *number_create_mcomplex_from_mfloat(const mfloat_t *real)
{
    mcomplex_t *complex_value;

    if (!real)
        return NULL;
    complex_value = mc_create(real, MF_ZERO);
    return complex_value ? number_wrap_mcomplex(complex_value) : NULL;
}

static number_t *number_coerce_invalid(const number_t *number)
{
    (void)number;
    return NULL;
}

static number_t *number_coerce_clone_double(const number_t *number)
{
    return number_clone_double(number);
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
    return number ? number_wrap_mfloat(mf_create_double(number_impl_const(number)->value.d)) : NULL;
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
    return number_clone_qfloat(number);
}

static number_t *number_coerce_qfloat_to_qcomplex(const number_t *number)
{
    return number ? number_wrap_qcomplex(qc_make(number_impl_const(number)->value.qf, QF_ZERO)) : NULL;
}

static number_t *number_coerce_qfloat_to_mfloat(const number_t *number)
{
    return number ? number_wrap_mfloat(mf_create_qfloat(number_impl_const(number)->value.qf)) : NULL;
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
    return number_clone_qcomplex(number);
}

static number_t *number_coerce_qcomplex_to_mcomplex(const number_t *number)
{
    return number ? number_wrap_mcomplex(mc_create_qcomplex(number_impl_const(number)->value.qc)) : NULL;
}

static number_t *number_coerce_clone_mint(const number_t *number)
{
    return number_clone_mint(number);
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
    tmp_float = mf_create_mrational(tmp_rational);
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
    return number_clone_mrational(number);
}

static number_t *number_coerce_mrational_to_mfloat(const number_t *number)
{
    return number ? number_wrap_mfloat(mf_create_mrational(number_impl_const(number)->value.mr)) : NULL;
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
    return number_clone_mfloat(number);
}

static number_t *number_coerce_mfloat_to_mcomplex(const number_t *number)
{
    return number ? number_create_mcomplex_from_mfloat(number_impl_const(number)->value.mf) : NULL;
}

static number_t *number_coerce_clone_mcomplex(const number_t *number)
{
    return number_clone_mcomplex(number);
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

static number_t *number_coerce(const number_t *number, number_kind_t target_kind)
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
    if (!lhs || !rhs || number_impl_const(lhs)->kind != number_impl_const(rhs)->kind)
        goto done;
    result = number_apply_binary_same_kind(lhs, rhs, op);

done:
    number_box_free(lhs);
    number_box_free(rhs);
    return result;
}

number_t num_create_double(double value)
{
    return number_take(number_wrap_double(value));
}

number_t num_create_qfloat(qfloat_t value)
{
    return number_take(number_wrap_qfloat(value));
}

number_t num_create_qcomplex(qcomplex_t value)
{
    return number_take(number_wrap_qcomplex(value));
}

number_t num_create_mint(const mint_t *value)
{
    return value ? number_take(number_wrap_mint(mi_clone(value))) : number_invalid();
}

number_t num_create_mrational(const mrational_t *value)
{
    return value ? number_take(number_wrap_mrational(mr_clone(value))) : number_invalid();
}

number_t num_create_mfloat(const mfloat_t *value)
{
    return value ? number_wrap_mfloat_with_precision(mf_clone(value),
        number_default_precision_bits) : number_invalid();
}

number_t num_create_mfloat_prec(const mfloat_t *value, size_t precision_bits)
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

number_t num_create_mfloat_digits(const mfloat_t *value, size_t significant_digits)
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

number_t num_create_mcomplex(const mcomplex_t *value)
{
    return value ? number_wrap_mcomplex_with_precision(mc_clone(value),
        number_default_precision_bits) : number_invalid();
}

number_t num_create_mcomplex_prec(const mcomplex_t *value, size_t precision_bits)
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

number_t num_create_mcomplex_digits(const mcomplex_t *value, size_t significant_digits)
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

number_t num_create_string(const char *text)
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

number_t num_pi(void)
{
    return number_wrap_mfloat_borrowed(MF_PI);
}

number_t num_e(void)
{
    return number_wrap_mfloat_borrowed(MF_E);
}

number_t num_euler_mascheroni(void)
{
    return number_wrap_mfloat_borrowed(MF_EULER_MASCHERONI);
}

number_t num_max(void)
{
    return number_wrap_mfloat_with_precision(mf_max(), number_default_precision_bits);
}

number_t num_pow10(int exponent10)
{
    return number_wrap_mfloat_with_precision(mf_pow10(exponent10),
        number_default_precision_bits);
}

number_t num_clone(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt ? number_take(vt->clone(&number)) : number_invalid();
}

void num_clear(number_t *number)
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

int num_set_precision(number_t *number, size_t precision_bits)
{
    const number_vtable_t *vt = number_vt(number);

    return vt && vt->set_precision ? vt->set_precision(number, precision_bits) : -1;
}

size_t num_get_precision(const number_t number)
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
    if (number_impl_const(&a)->kind == number_impl_const(&b)->kind) {
        const number_vtable_t *vt = number_vt(&a);
        if (vt && vt->eq_same)
            return vt->eq_same(&a, &b);
    }

    kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (!lhs || !rhs || number_impl_const(lhs)->kind != number_impl_const(rhs)->kind)
        goto done;
    {
        const number_vtable_t *vt = number_vt(lhs);
        if (!vt || !vt->eq_same)
            goto done;
        eq = vt->eq_same(lhs, rhs);
    }

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
    if (number_impl_const(&number)->kind == NUMBER_MINT) {
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

number_t num_exp(const number_t number)
{
    number_t *promoted = NULL;
    number_t *result = NULL;

    {
        const number_vtable_t *vt = number_vt(&number);

        if (!vt)
            return number_invalid();
        if (vt->exp_same)
            return number_take(vt->exp_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = number_vt(promoted);
        if (!vt || !vt->exp_same)
            goto done;
        result = vt->exp_same(promoted);
    }
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_log(const number_t number)
{
    number_t *promoted = NULL;
    number_t *result = NULL;

    {
        const number_vtable_t *vt = number_vt(&number);

        if (!vt)
            return number_invalid();
        if (vt->log_same)
            return number_take(vt->log_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = number_vt(promoted);
        if (!vt || !vt->log_same)
            goto done;
        result = vt->log_same(promoted);
    }
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_sqrt(const number_t number)
{
    number_t *promoted = NULL;
    number_t *result = NULL;

    {
        const number_vtable_t *vt = number_vt(&number);

        if (!vt)
            return number_invalid();
        if (vt->sqrt_same)
            return number_take(vt->sqrt_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = number_vt(promoted);
        if (!vt || !vt->sqrt_same)
            goto done;
        result = vt->sqrt_same(promoted);
    }
done:
    number_box_free(promoted);
    return number_take(result);
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

static void number_assign(number_t *dst, number_t value)
{
    if (!dst) {
        num_clear(&value);
        return;
    }
    num_clear(dst);
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

static inline number_math_family_t number_math_family_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) && vt
        ? vt->math_family : NUMBER_MATH_INVALID;
}

static inline number_math_family_t number_math_family_binary(number_math_family_t a,
                                                             number_math_family_t b)
{
    return (unsigned)a <= NUMBER_MATH_MCOMPLEX &&
        (unsigned)b <= NUMBER_MATH_MCOMPLEX
        ? number_math_family_binary_table[a][b] : NUMBER_MATH_INVALID;
}

static inline number_kind_t number_math_family_target_kind(number_math_family_t family)
{
    return (unsigned)family <= NUMBER_MATH_MCOMPLEX
        ? number_math_family_target_kind_table[family] : NUMBER_INVALID;
}

static inline qfloat_t number_value_to_qfloat(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && vt && vt->to_qfloat ? vt->to_qfloat(number) : QF_NAN;
}

static inline qcomplex_t number_value_to_qcomplex(const number_t *number)
{
    return number && number_math_family_value(number) == NUMBER_MATH_QCOMPLEX
        ? number_impl_const(number)->value.qc
        : qc_make(number_value_to_qfloat(number), QF_ZERO);
}

typedef qfloat_t (*number_qfloat_unary_fn)(qfloat_t);
typedef qcomplex_t (*number_qcomplex_unary_fn)(qcomplex_t);
typedef int (*number_mfloat_unary_mut_fn)(mfloat_t *);
typedef int (*number_mcomplex_unary_mut_fn)(mcomplex_t *);
typedef double (*number_double_unary_fn)(double);
typedef qfloat_t (*number_qfloat_binary_fn)(qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_binary_fn)(qcomplex_t, qcomplex_t);
typedef int (*number_mfloat_binary_mut_fn)(mfloat_t *, const mfloat_t *);
typedef int (*number_mcomplex_binary_mut_fn)(mcomplex_t *, const mcomplex_t *);
typedef double (*number_double_binary_fn)(double, double);
typedef qfloat_t (*number_qfloat_ternary_fn)(qfloat_t, qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_ternary_fn)(qcomplex_t, qcomplex_t, qcomplex_t);
typedef int (*number_mfloat_ternary_mut_fn)(mfloat_t *, const mfloat_t *, const mfloat_t *);
typedef int (*number_mcomplex_ternary_mut_fn)(mcomplex_t *, const mcomplex_t *, const mcomplex_t *);

typedef struct {
    number_qfloat_unary_fn qreal;
    number_qcomplex_unary_fn qcomplex;
    number_mfloat_unary_mut_fn mreal;
    number_mcomplex_unary_mut_fn mcomplex;
} number_unary_math_ops_t;

typedef struct {
    number_qfloat_binary_fn qreal;
    number_qcomplex_binary_fn qcomplex;
    number_mfloat_binary_mut_fn mreal;
    number_mcomplex_binary_mut_fn mcomplex;
} number_binary_math_ops_t;

typedef struct {
    number_qfloat_ternary_fn qreal;
    number_qcomplex_ternary_fn qcomplex;
    number_mfloat_ternary_mut_fn mreal;
    number_mcomplex_ternary_mut_fn mcomplex;
} number_ternary_math_ops_t;

typedef number_t (*number_unary_math_apply_fn)(const number_t *number,
                                               const number_unary_math_ops_t *ops);
typedef number_t (*number_binary_math_apply_fn)(const number_t *a,
                                                const number_t *b,
                                                number_kind_t target_kind,
                                                const number_binary_math_ops_t *ops);
typedef number_t (*number_ternary_math_apply_fn)(const number_t *x,
                                                 const number_t *a,
                                                 const number_t *b,
                                                 number_kind_t target_kind,
                                                 const number_ternary_math_ops_t *ops);

static number_t number_apply_unary_qreal(const number_t *number,
                                         const number_unary_math_ops_t *ops)
{
    return ops && ops->qreal && number
        ? num_create_qfloat(ops->qreal(number_value_to_qfloat(number)))
        : number_invalid();
}

static number_t number_apply_unary_qcomplex(const number_t *number,
                                            const number_unary_math_ops_t *ops)
{
    return ops && ops->qcomplex && number
        ? num_create_qcomplex(ops->qcomplex(number_value_to_qcomplex(number)))
        : number_invalid();
}

static number_t number_apply_unary_mreal(const number_t *number,
                                         const number_unary_math_ops_t *ops)
{
    number_t *promoted = NULL;

    if (!ops || !ops->mreal || !number)
        return number_invalid();
    promoted = number_coerce(number, NUMBER_MFLOAT);
    if (!promoted || ops->mreal(number_impl(promoted)->value.mf) != 0) {
        number_box_free(promoted);
        return number_invalid();
    }
    return number_take(promoted);
}

static number_t number_apply_unary_mcomplex(const number_t *number,
                                            const number_unary_math_ops_t *ops)
{
    number_t *promoted = NULL;

    if (!ops || !ops->mcomplex || !number)
        return number_invalid();
    promoted = number_coerce(number, NUMBER_MCOMPLEX);
    if (!promoted || ops->mcomplex(number_impl(promoted)->value.mc) != 0) {
        number_box_free(promoted);
        return number_invalid();
    }
    return number_take(promoted);
}

static number_t number_apply_binary_qreal(const number_t *a,
                                          const number_t *b,
                                          number_kind_t target_kind,
                                          const number_binary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qreal && a && b
        ? num_create_qfloat(ops->qreal(number_value_to_qfloat(a),
            number_value_to_qfloat(b)))
        : number_invalid();
}

static number_t number_apply_binary_qcomplex(const number_t *a,
                                             const number_t *b,
                                             number_kind_t target_kind,
                                             const number_binary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qcomplex && a && b
        ? num_create_qcomplex(ops->qcomplex(number_value_to_qcomplex(a),
            number_value_to_qcomplex(b)))
        : number_invalid();
}

static number_t number_apply_binary_mreal(const number_t *a,
                                          const number_t *b,
                                          number_kind_t target_kind,
                                          const number_binary_math_ops_t *ops)
{
    number_t *lhs = NULL;
    number_t *rhs = NULL;

    if (!ops || !ops->mreal || !a || !b)
        return number_invalid();
    lhs = number_coerce(a, target_kind);
    rhs = number_coerce(b, target_kind);
    if (!lhs || !rhs ||
        ops->mreal(number_impl(lhs)->value.mf, number_impl_const(rhs)->value.mf) != 0) {
        number_box_free(lhs);
        number_box_free(rhs);
        return number_invalid();
    }
    number_box_free(rhs);
    return number_take(lhs);
}

static number_t number_apply_binary_mcomplex(const number_t *a,
                                             const number_t *b,
                                             number_kind_t target_kind,
                                             const number_binary_math_ops_t *ops)
{
    number_t *lhs = NULL;
    number_t *rhs = NULL;

    if (!ops || !ops->mcomplex || !a || !b)
        return number_invalid();
    lhs = number_coerce(a, target_kind);
    rhs = number_coerce(b, target_kind);
    if (!lhs || !rhs ||
        ops->mcomplex(number_impl(lhs)->value.mc, number_impl_const(rhs)->value.mc) != 0) {
        number_box_free(lhs);
        number_box_free(rhs);
        return number_invalid();
    }
    number_box_free(rhs);
    return number_take(lhs);
}

static number_t number_apply_ternary_qreal(const number_t *x,
                                           const number_t *a,
                                           const number_t *b,
                                           number_kind_t target_kind,
                                           const number_ternary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qreal && x && a && b
        ? num_create_qfloat(ops->qreal(number_value_to_qfloat(x),
            number_value_to_qfloat(a),
            number_value_to_qfloat(b)))
        : number_invalid();
}

static number_t number_apply_ternary_qcomplex(const number_t *x,
                                              const number_t *a,
                                              const number_t *b,
                                              number_kind_t target_kind,
                                              const number_ternary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qcomplex && x && a && b
        ? num_create_qcomplex(ops->qcomplex(number_value_to_qcomplex(x),
            number_value_to_qcomplex(a),
            number_value_to_qcomplex(b)))
        : number_invalid();
}

static number_t number_apply_ternary_mreal(const number_t *x,
                                           const number_t *a,
                                           const number_t *b,
                                           number_kind_t target_kind,
                                           const number_ternary_math_ops_t *ops)
{
    number_t *nx = NULL;
    number_t *na = NULL;
    number_t *nb = NULL;

    if (!ops || !ops->mreal || !x || !a || !b)
        return number_invalid();
    nx = number_coerce(x, target_kind);
    na = number_coerce(a, target_kind);
    nb = number_coerce(b, target_kind);
    if (!nx || !na || !nb ||
        ops->mreal(number_impl(nx)->value.mf,
                   number_impl_const(na)->value.mf,
                   number_impl_const(nb)->value.mf) != 0) {
        number_box_free(nx);
        number_box_free(na);
        number_box_free(nb);
        return number_invalid();
    }
    number_box_free(na);
    number_box_free(nb);
    return number_take(nx);
}

static number_t number_apply_ternary_mcomplex(const number_t *x,
                                              const number_t *a,
                                              const number_t *b,
                                              number_kind_t target_kind,
                                              const number_ternary_math_ops_t *ops)
{
    number_t *nx = NULL;
    number_t *na = NULL;
    number_t *nb = NULL;

    if (!ops || !ops->mcomplex || !x || !a || !b)
        return number_invalid();
    nx = number_coerce(x, target_kind);
    na = number_coerce(a, target_kind);
    nb = number_coerce(b, target_kind);
    if (!nx || !na || !nb ||
        ops->mcomplex(number_impl(nx)->value.mc,
                      number_impl_const(na)->value.mc,
                      number_impl_const(nb)->value.mc) != 0) {
        number_box_free(nx);
        number_box_free(na);
        number_box_free(nb);
        return number_invalid();
    }
    number_box_free(na);
    number_box_free(nb);
    return number_take(nx);
}

static number_t number_apply_unary_math(const number_t number,
                                        number_qfloat_unary_fn qf_fn,
                                        number_qcomplex_unary_fn qc_fn,
                                        number_mfloat_unary_mut_fn mf_fn,
                                        number_mcomplex_unary_mut_fn mc_fn)
{
    static const number_unary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_unary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_unary_qcomplex,
        [NUMBER_MATH_MREAL] = number_apply_unary_mreal,
        [NUMBER_MATH_MCOMPLEX] = number_apply_unary_mcomplex
    };
    const number_unary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mreal = mf_fn,
        .mcomplex = mc_fn
    };
    number_math_family_t family = number_math_family_value(&number);

    return (unsigned)family <= NUMBER_MATH_MCOMPLEX && dispatch[family]
        ? dispatch[family](&number, &ops)
        : number_invalid();
}

static number_t number_apply_unary_math_with_double(const number_t number,
                                                    number_double_unary_fn d_fn,
                                                    number_qfloat_unary_fn qf_fn,
                                                    number_qcomplex_unary_fn qc_fn,
                                                    number_mfloat_unary_mut_fn mf_fn,
                                                    number_mcomplex_unary_mut_fn mc_fn)
{
    return number_is_valid_value(&number) &&
           number_impl_const(&number)->kind == NUMBER_DOUBLE && d_fn
        ? num_create_double(d_fn(number_impl_const(&number)->value.d))
        : number_apply_unary_math(number, qf_fn, qc_fn, mf_fn, mc_fn);
}

static number_t number_apply_binary_math(const number_t a,
                                         const number_t b,
                                         number_qfloat_binary_fn qf_fn,
                                         number_qcomplex_binary_fn qc_fn,
                                         number_mfloat_binary_mut_fn mf_fn,
                                         number_mcomplex_binary_mut_fn mc_fn)
{
    static const number_binary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_binary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_binary_qcomplex,
        [NUMBER_MATH_MREAL] = number_apply_binary_mreal,
        [NUMBER_MATH_MCOMPLEX] = number_apply_binary_mcomplex
    };
    const number_binary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mreal = mf_fn,
        .mcomplex = mc_fn
    };
    number_math_family_t family = number_math_family_binary(
        number_math_family_value(&a),
        number_math_family_value(&b));
    number_kind_t target_kind = number_math_family_target_kind(family);

    return (unsigned)family <= NUMBER_MATH_MCOMPLEX && dispatch[family]
        ? dispatch[family](&a, &b, target_kind, &ops)
        : number_invalid();
}

static number_t number_apply_binary_math_with_double(const number_t a,
                                                     const number_t b,
                                                     number_double_binary_fn d_fn,
                                                     number_qfloat_binary_fn qf_fn,
                                                     number_qcomplex_binary_fn qc_fn,
                                                     number_mfloat_binary_mut_fn mf_fn,
                                                     number_mcomplex_binary_mut_fn mc_fn)
{
    return number_is_valid_value(&a) && number_is_valid_value(&b) &&
           number_impl_const(&a)->kind == NUMBER_DOUBLE &&
           number_impl_const(&b)->kind == NUMBER_DOUBLE && d_fn
        ? num_create_double(d_fn(number_impl_const(&a)->value.d,
            number_impl_const(&b)->value.d))
        : number_apply_binary_math(a, b, qf_fn, qc_fn, mf_fn, mc_fn);
}

static number_t number_apply_ternary_math(const number_t x,
                                          const number_t a,
                                          const number_t b,
                                          number_qfloat_ternary_fn qf_fn,
                                          number_qcomplex_ternary_fn qc_fn,
                                          number_mfloat_ternary_mut_fn mf_fn,
                                          number_mcomplex_ternary_mut_fn mc_fn)
{
    static const number_ternary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_ternary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_ternary_qcomplex,
        [NUMBER_MATH_MREAL] = number_apply_ternary_mreal,
        [NUMBER_MATH_MCOMPLEX] = number_apply_ternary_mcomplex
    };
    const number_ternary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mreal = mf_fn,
        .mcomplex = mc_fn
    };
    number_math_family_t family = number_math_family_binary(
        number_math_family_value(&x),
        number_math_family_value(&a));
    number_kind_t target_kind;

    family = number_math_family_binary(family, number_math_family_value(&b));
    target_kind = number_math_family_target_kind(family);
    return (unsigned)family <= NUMBER_MATH_MCOMPLEX && dispatch[family]
        ? dispatch[family](&x, &a, &b, target_kind, &ops)
        : number_invalid();
}

static char *number_format_double(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    double value;

    if (!number)
        return NULL;
    value = number_impl_const(number)->value.d;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dE" : "%%.%dg", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%.16E" : "%%.17g");
    needed = snprintf(NULL, 0, fmt, value);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    snprintf(out, (size_t)needed + 1u, fmt, value);
    return out;
}

static char *number_format_qfloat(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dQ" : "%%.%dq", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Q" : "%%q");
    needed = qf_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.qf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    qf_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.qf);
    return out;
}

static char *number_format_qcomplex(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dZ" : "%%.%dz", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%Z" : "%%z");
    needed = qc_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.qc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    qc_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.qc);
    return out;
}

static char *number_format_mfloat(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dMF" : "%%.%dmf", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%MF" : "%%mf");
    needed = mf_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.mf);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    mf_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.mf);
    return out;
}

static char *number_format_mcomplex(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (precision >= 0)
        snprintf(fmt, sizeof(fmt), scientific ? "%%.%dMZ" : "%%.%dmz", precision);
    else
        snprintf(fmt, sizeof(fmt), scientific ? "%%MZ" : "%%mz");
    needed = mc_sprintf(NULL, 0u, fmt, number_impl_const(number)->value.mc);
    if (needed < 0)
        return NULL;
    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    mc_sprintf(out, (size_t)needed + 1u, fmt, number_impl_const(number)->value.mc);
    return out;
}

static char *number_format_inexact(const number_t *number, bool scientific, int precision)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (!number)
        return NULL;
    if (vt && vt->format_inexact)
        return vt->format_inexact(number, scientific, precision);
    return NULL;
}

number_t num_new(void)
{
    return number_take(number_wrap_mfloat(mf_new_prec(number_default_precision_bits)));
}

number_t num_new_prec(size_t precision_bits)
{
    return precision_bits == 0u ? number_invalid() :
        number_take(number_wrap_mfloat(mf_new_prec(precision_bits)));
}

number_t num_create_long(long value)
{
    return number_take(number_wrap_mint(mi_create_long(value)));
}

int num_set_default_precision(size_t precision_bits)
{
    if (precision_bits == 0u)
        return -1;
    number_default_precision_bits = precision_bits;
    return 0;
}

size_t num_get_default_precision(void)
{
    return number_default_precision_bits;
}

int num_set_default_precision_digits(size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_default_precision(bits);
}

size_t num_get_default_precision_digits(void)
{
    return number_bits_to_digits(number_default_precision_bits);
}

int num_set_precision_digits(number_t *number, size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_precision(number, bits);
}

size_t num_get_precision_digits(const number_t number)
{
    return number_bits_to_digits(num_get_precision(number));
}

int num_set_long(number_t *number, long value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_long(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_double(number_t *number, double value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_double(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_qfloat(number_t *number, qfloat_t value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_qfloat(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_mrational(number_t *number, const mrational_t *value)
{
    if (!number || !value)
        return -1;
    number_assign(number, num_create_mrational(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_string(number_t *number, const char *text)
{
    if (!number || !text)
        return -1;
    number_assign(number, num_create_string(text));
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
    {
        double value = mf_to_double(number_impl_const(tmp)->value.mf);
        number_box_free(tmp);
        return value;
    }
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
    {
        qfloat_t value = mf_to_qfloat(number_impl_const(tmp)->value.mf);
        number_box_free(tmp);
        return value;
    }
}

int num_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap)
{
    va_list ap_local;
    const char *p = fmt;
    size_t pos = 0u;
    int total = 0;

    if (!fmt)
        return -1;
    va_copy(ap_local, ap);
    while (*p) {
        char tmp[512];
        int width = 0;
        int precision = -1;
        int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
        char spec;
        char *core = NULL;
        int core_len;
        int pad;

        if (*p != '%') {
            if (out && pos + 1u < out_size)
                out[pos] = *p;
            pos += 1u;
            total += 1;
            ++p;
            continue;
        }
        ++p;
        if (*p == '%') {
            if (out && pos + 1u < out_size)
                out[pos] = '%';
            pos += 1u;
            total += 1;
            ++p;
            continue;
        }
        while (*p == '-' || *p == '0' || *p == '+' || *p == ' ' || *p == '#') {
            left |= (*p == '-');
            zero |= (*p == '0');
            plus |= (*p == '+');
            space |= (*p == ' ');
            alt |= (*p == '#');
            ++p;
        }
        while (isdigit((unsigned char)*p)) {
            width = width * 10 + (*p - '0');
            ++p;
        }
        if (*p == '.') {
            precision = 0;
            ++p;
            while (isdigit((unsigned char)*p)) {
                precision = precision * 10 + (*p - '0');
                ++p;
            }
        }
        while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 't' || *p == 'j' || *p == 'L')
            ++p;
        spec = *p ? *p++ : '\0';

        if (spec == 'n' || spec == 'N') {
            number_t value = va_arg(ap_local, number_t);
            core = (num_is_exact(value) && !number_vt(&value)->complex)
                ? num_to_string(value)
                : number_format_inexact(&value, spec == 'N', precision);
        } else if (spec == 'd' || spec == 'i') {
            snprintf(tmp, sizeof(tmp), "%d", va_arg(ap_local, int));
            core = number_strdup(tmp);
        } else if (spec == 'u') {
            snprintf(tmp, sizeof(tmp), "%u", va_arg(ap_local, unsigned int));
            core = number_strdup(tmp);
        } else if (spec == 'f' || spec == 'g' || spec == 'e' || spec == 'E') {
            double value = va_arg(ap_local, double);
            if (precision >= 0)
                snprintf(tmp, sizeof(tmp), (spec == 'f') ? "%.*f" : (spec == 'g') ? "%.*g" : (spec == 'e') ? "%.*e" : "%.*E",
                         precision, value);
            else
                snprintf(tmp, sizeof(tmp), (spec == 'f') ? "%f" : (spec == 'g') ? "%g" : (spec == 'e') ? "%e" : "%E",
                         value);
            core = number_strdup(tmp);
        } else if (spec == 'c') {
            tmp[0] = (char)va_arg(ap_local, int);
            tmp[1] = '\0';
            core = number_strdup(tmp);
        } else if (spec == 's') {
            const char *value = va_arg(ap_local, const char *);
            core = number_strdup(value ? value : "(null)");
        } else if (spec == 'p') {
            snprintf(tmp, sizeof(tmp), "%p", va_arg(ap_local, void *));
            core = number_strdup(tmp);
        } else {
            tmp[0] = '%';
            tmp[1] = spec ? spec : '\0';
            tmp[2] = '\0';
            core = number_strdup(tmp);
        }

        if (!core) {
            va_end(ap_local);
            return -1;
        }

        if ((plus || space) && core[0] != '-' &&
            (spec == 'n' || spec == 'N' || spec == 'f' || spec == 'g' || spec == 'e' || spec == 'E' || spec == 'd' || spec == 'i')) {
            char *prefixed = malloc(strlen(core) + 2u);
            if (!prefixed) {
                free(core);
                va_end(ap_local);
                return -1;
            }
            prefixed[0] = plus ? '+' : ' ';
            strcpy(prefixed + 1, core);
            free(core);
            core = prefixed;
        }
        core_len = (int)strlen(core);
        pad = width > core_len ? width - core_len : 0;
        if (!left) {
            char fill = zero ? '0' : ' ';
            while (pad-- > 0) {
                if (out && pos + 1u < out_size)
                    out[pos] = fill;
                ++pos;
                ++total;
            }
        }
        for (int i = 0; i < core_len; ++i) {
            if (out && pos + 1u < out_size)
                out[pos] = core[i];
            ++pos;
            ++total;
        }
        if (left) {
            pad = width > core_len ? width - core_len : 0;
            while (pad-- > 0) {
                if (out && pos + 1u < out_size)
                    out[pos] = ' ';
                ++pos;
                ++total;
            }
        }
        free(core);
        (void)alt;
    }
    if (out_size > 0u) {
        if (out) {
            size_t term = pos < out_size ? pos : out_size - 1u;
            out[term] = '\0';
        }
    }
    va_end(ap_local);
    return total;
}

int num_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = num_vsprintf(out, out_size, fmt, ap);
    va_end(ap);
    return n;
}

int num_printf(const char *fmt, ...)
{
    int needed;
    int written;
    char *buf;
    va_list ap;

    va_start(ap, fmt);
    needed = num_vsprintf(NULL, 0u, fmt, ap);
    va_end(ap);
    if (needed < 0)
        return needed;
    buf = malloc((size_t)needed + 1u);
    if (!buf)
        return -1;
    va_start(ap, fmt);
    written = num_vsprintf(buf, (size_t)needed + 1u, fmt, ap);
    va_end(ap);
    if (written >= 0)
        fputs(buf, stdout);
    free(buf);
    return written;
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
    if (number_impl_const(&a)->kind == number_impl_const(&b)->kind)
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
    {
        number_t zero = num_create_mfloat_prec(MF_ZERO, num_get_precision(number) ? num_get_precision(number) : number_default_precision_bits);
        number_t real = vt && vt->complex ? num_real_part(number) : num_clone(number);
        number_t result = num_atan2(zero, real);
        num_clear(&zero);
        num_clear(&real);
        return result;
    }
}

number_t num_add_mrational(const number_t number, const mrational_t *value)
{
    number_t rhs = num_create_mrational(value);
    number_t result = num_add(number, rhs);
    num_clear(&rhs);
    return result;
}

number_t num_add_long(const number_t number, long value)
{
    number_t rhs = num_create_long(value);
    number_t result = num_add(number, rhs);
    num_clear(&rhs);
    return result;
}

number_t num_mul_long(const number_t number, long value)
{
    number_t rhs = num_create_long(value);
    number_t result = num_mul(number, rhs);
    num_clear(&rhs);
    return result;
}

number_t num_mul_mrational(const number_t number, const mrational_t *value)
{
    number_t rhs = num_create_mrational(value);
    number_t result = num_mul(number, rhs);
    num_clear(&rhs);
    return result;
}

number_t num_pow(const number_t base, const number_t exponent)
{
    return number_apply_binary_math(base, exponent, qf_pow, qc_pow, mf_pow, mc_pow);
}

number_t num_pow_int(const number_t base, int exponent)
{
    const number_vtable_t *vt = number_vt(&base);

    if (!number_is_valid_value(&base))
        return number_invalid();
    if (vt && vt->pow_int)
        return number_take(vt->pow_int(&base, exponent));
    {
        number_t expnum = num_create_long(exponent);
        number_t result = num_pow(base, expnum);
        num_clear(&expnum);
        return result;
    }
}

number_t num_ldexp(const number_t number, int exponent2)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->ldexp_value)
        return number_take(vt->ldexp_value(&number, exponent2));
    {
        number_t two = num_create_long(2);
        number_t scale = num_pow_int(two, exponent2);
        number_t result = num_mul(number, scale);

        num_clear(&two);
        num_clear(&scale);
        return result;
    }
}

number_t num_sqr(const number_t number) { return number_apply_unary_math(number, qf_sqr, NULL, mf_sqr, NULL); }
number_t num_floor(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (vt && vt->floor_value)
        return number_take(vt->floor_value(&number));
    return number_apply_unary_math(number, qf_floor, qc_floor, mf_floor, mc_floor);
}
number_t num_mul_pow10(const number_t number, int exponent10)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->mul_pow10_value)
        return number_take(vt->mul_pow10_value(&number, exponent10));
    return num_mul(number, num_pow10(exponent10));
}
number_t num_hypot(const number_t a, const number_t b) { return number_apply_binary_math(a, b, qf_hypot, qc_hypot, mf_hypot, mc_hypot); }

int num_sincos(const number_t x, number_t *sin_out, number_t *cos_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sin_out || !cos_out || !number_is_valid_value(&x))
        return -1;
    if (vt && vt->sincos_value)
        return vt->sincos_value(&x, sin_out, cos_out);
    return -1;
}

number_t num_sin(const number_t number)
{
    return number_apply_unary_math_with_double(number, sin, qf_sin, qc_sin, mf_sin, mc_sin);
}
number_t num_cos(const number_t number)
{
    return number_apply_unary_math_with_double(number, cos, qf_cos, qc_cos, mf_cos, mc_cos);
}
number_t num_tan(const number_t number)
{
    return number_apply_unary_math_with_double(number, tan, qf_tan, qc_tan, mf_tan, mc_tan);
}
number_t num_atan(const number_t number)
{
    return number_apply_unary_math_with_double(number, atan, qf_atan, qc_atan, mf_atan, mc_atan);
}
number_t num_atan2(const number_t y, const number_t x)
{
    return number_apply_binary_math_with_double(y, x, atan2, qf_atan2, qc_atan2, mf_atan2, mc_atan2);
}
number_t num_asin(const number_t number)
{
    return number_apply_unary_math_with_double(number, asin, qf_asin, qc_asin, mf_asin, mc_asin);
}
number_t num_acos(const number_t number)
{
    return number_apply_unary_math_with_double(number, acos, qf_acos, qc_acos, mf_acos, mc_acos);
}
number_t num_sinh(const number_t number)
{
    return number_apply_unary_math_with_double(number, sinh, qf_sinh, qc_sinh, mf_sinh, mc_sinh);
}
number_t num_cosh(const number_t number)
{
    return number_apply_unary_math_with_double(number, cosh, qf_cosh, qc_cosh, mf_cosh, mc_cosh);
}

int num_sinhcosh(const number_t x, number_t *sinh_out, number_t *cosh_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sinh_out || !cosh_out || !number_is_valid_value(&x))
        return -1;
    if (vt && vt->sinhcosh_value)
        return vt->sinhcosh_value(&x, sinh_out, cosh_out);
    return -1;
}

number_t num_tanh(const number_t number)
{
    return number_apply_unary_math_with_double(number, tanh, qf_tanh, qc_tanh, mf_tanh, mc_tanh);
}
number_t num_asinh(const number_t number)
{
    return number_apply_unary_math_with_double(number, asinh, qf_asinh, qc_asinh, mf_asinh, mc_asinh);
}
number_t num_acosh(const number_t number)
{
    return number_apply_unary_math_with_double(number, acosh, qf_acosh, qc_acosh, mf_acosh, mc_acosh);
}
number_t num_atanh(const number_t number)
{
    return number_apply_unary_math_with_double(number, atanh, qf_atanh, qc_atanh, mf_atanh, mc_atanh);
}

number_t num_gamma(const number_t number)
{
    return number_apply_unary_math(number, qf_gamma, qc_gamma, mf_gamma, mc_gamma);
}

number_t num_lgamma(const number_t number)
{
    return number_apply_unary_math(number, NULL, NULL, mf_lgamma, mc_lgamma);
}

number_t num_digamma(const number_t number)
{
    return number_apply_unary_math(number, qf_digamma, qc_digamma, mf_digamma, mc_digamma);
}

number_t num_trigamma(const number_t number)
{
    return number_apply_unary_math(number, qf_trigamma, qc_trigamma, mf_trigamma, mc_trigamma);
}

number_t num_tetragamma(const number_t number)
{
    return number_apply_unary_math(number, qf_tetragamma, qc_tetragamma, mf_tetragamma, mc_tetragamma);
}

number_t num_gammainv(const number_t number)
{
    return number_apply_unary_math(number, qf_gammainv, qc_gammainv, mf_gammainv, mc_gammainv);
}

number_t num_erf(const number_t number)
{
    return number_apply_unary_math(number, qf_erf, qc_erf, mf_erf, mc_erf);
}

number_t num_erfc(const number_t number)
{
    return number_apply_unary_math(number, qf_erfc, qc_erfc, mf_erfc, mc_erfc);
}

number_t num_erfinv(const number_t number)
{
    return number_apply_unary_math(number, qf_erfinv, qc_erfinv, mf_erfinv, mc_erfinv);
}

number_t num_erfcinv(const number_t number)
{
    return number_apply_unary_math(number, qf_erfcinv, qc_erfcinv, mf_erfcinv, mc_erfcinv);
}

number_t num_lambert_w0(const number_t number)
{
    return number_apply_unary_math(number, qf_lambert_w0, NULL, mf_lambert_w0, mc_lambert_w0);
}

number_t num_lambert_wm1(const number_t number)
{
    return number_apply_unary_math(number, qf_lambert_wm1, qc_lambert_wm1, mf_lambert_wm1, mc_lambert_wm1);
}

number_t num_beta(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_beta, qc_beta, mf_beta, mc_beta);
}

number_t num_logbeta(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_logbeta, qc_logbeta, mf_logbeta, mc_logbeta);
}

number_t num_binomial(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_binomial, qc_binomial, mf_binomial, mc_binomial);
}

number_t num_beta_pdf(const number_t x, const number_t a, const number_t b)
{
    return number_apply_ternary_math(x, a, b, qf_beta_pdf, qc_beta_pdf, mf_beta_pdf, mc_beta_pdf);
}

number_t num_logbeta_pdf(const number_t x, const number_t a, const number_t b)
{
    return number_apply_ternary_math(x, a, b, qf_logbeta_pdf, qc_logbeta_pdf, mf_logbeta_pdf, mc_logbeta_pdf);
}

number_t num_normal_pdf(const number_t number)
{
    return number_apply_unary_math(number, qf_normal_pdf, qc_normal_pdf, mf_normal_pdf, mc_normal_pdf);
}

number_t num_normal_cdf(const number_t number)
{
    return number_apply_unary_math(number, qf_normal_cdf, qc_normal_cdf, mf_normal_cdf, mc_normal_cdf);
}

number_t num_normal_logpdf(const number_t number)
{
    return number_apply_unary_math(number, qf_normal_logpdf, qc_normal_logpdf, mf_normal_logpdf, mc_normal_logpdf);
}

number_t num_productlog(const number_t number)
{
    return number_apply_unary_math(number, qf_productlog, qc_productlog, mf_productlog, mc_productlog);
}

number_t num_gammainc_lower(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_lower, qc_gammainc_lower, mf_gammainc_lower, mc_gammainc_lower);
}

number_t num_gammainc_upper(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_upper, qc_gammainc_upper, mf_gammainc_upper, mc_gammainc_upper);
}

number_t num_gammainc_P(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_P, qc_gammainc_P, mf_gammainc_P, mc_gammainc_P);
}

number_t num_gammainc_Q(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_Q, qc_gammainc_Q, mf_gammainc_Q, mc_gammainc_Q);
}

number_t num_ei(const number_t number)
{
    return number_apply_unary_math(number, qf_ei, qc_ei, mf_ei, mc_ei);
}

number_t num_e1(const number_t number)
{
    return number_apply_unary_math(number, qf_e1, qc_e1, mf_e1, mc_e1);
}
