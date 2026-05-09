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

typedef struct number_vtable_t {
    number_kind_t kind;
    bool exact;
    bool complex;
    void (*destroy_payload)(number_t *number);
    number_t *(*clone)(const number_t *number);
    char *(*to_string)(const number_t *number);
    bool (*is_real)(const number_t *number);
    bool (*is_zero)(const number_t *number);
    bool (*is_one)(const number_t *number);
    bool (*eq_same)(const number_t *a, const number_t *b);
    int (*set_precision)(number_t *number, size_t precision_bits);
    size_t (*get_precision)(const number_t *number);
    long (*get_exponent2)(const number_t *number);
    number_t *(*neg)(const number_t *number);
    number_t *(*inv)(const number_t *number);
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

static size_t number_default_precision_bits = 1024u;

static number_private_t *number_impl(number_t *number)
{
    return (number_private_t *)number;
}

static const number_private_t *number_impl_const(const number_t *number)
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

static bool number_is_valid_value(const number_t *number)
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
    return number && qf_eq(number_impl_const(number)->value.qc.im, QF_ZERO);
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
    double hi, lo, lead;
    int exp2;

    if (!number)
        return 0l;
    hi = fabs(number_impl_const(number)->value.qf.hi);
    lo = fabs(number_impl_const(number)->value.qf.lo);
    lead = hi != 0.0 ? hi : lo;
    exp2 = ilogb(lead);
    return exp2 == FP_ILOGB0 || exp2 == FP_ILOGBNAN ? 0l : (long)exp2;
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
    NUMBER_DOUBLE, false, false,
    number_destroy_none,
    number_clone_double,
    number_to_string_double,
    number_is_real_default,
    number_is_zero_double,
    number_is_one_double,
    number_eq_same_double,
    number_set_precision_noop,
    number_precision_fixed53,
    number_get_exponent2_double,
    number_neg_double,
    number_inv_double,
    number_add_same_double,
    number_sub_same_double,
    number_mul_same_double,
    number_div_same_double,
    number_exp_same_double,
    number_log_same_double,
    number_sqrt_same_double
};

static const number_vtable_t number_qfloat_vt = {
    NUMBER_QFLOAT, false, false,
    number_destroy_none,
    number_clone_qfloat,
    number_to_string_qfloat,
    number_is_real_default,
    number_is_zero_qfloat,
    number_is_one_qfloat,
    number_eq_same_qfloat,
    number_set_precision_noop,
    number_precision_fixed106,
    number_get_exponent2_qfloat,
    number_neg_qfloat,
    number_inv_qfloat,
    number_add_same_qfloat,
    number_sub_same_qfloat,
    number_mul_same_qfloat,
    number_div_same_qfloat,
    number_exp_same_qfloat,
    number_log_same_qfloat,
    number_sqrt_same_qfloat
};

static const number_vtable_t number_qcomplex_vt = {
    NUMBER_QCOMPLEX, false, true,
    number_destroy_none,
    number_clone_qcomplex,
    number_to_string_qcomplex,
    number_is_real_qcomplex,
    number_is_zero_qcomplex,
    number_is_one_qcomplex,
    number_eq_same_qcomplex,
    number_set_precision_noop,
    number_precision_fixed106,
    number_get_exponent2_zero,
    number_neg_qcomplex,
    number_inv_qcomplex,
    number_add_same_qcomplex,
    number_sub_same_qcomplex,
    number_mul_same_qcomplex,
    number_div_same_qcomplex,
    number_exp_same_qcomplex,
    number_log_same_qcomplex,
    number_sqrt_same_qcomplex
};

static const number_vtable_t number_mint_vt = {
    NUMBER_MINT, true, false,
    number_destroy_mint,
    number_clone_mint,
    number_to_string_mint,
    number_is_real_default,
    number_is_zero_mint,
    number_is_one_mint,
    number_eq_same_mint,
    number_set_precision_noop,
    number_precision_zero,
    number_get_exponent2_mint,
    number_neg_mint,
    NULL,
    number_add_same_mint,
    number_sub_same_mint,
    number_mul_same_mint,
    NULL,
    NULL,
    NULL,
    NULL
};

static const number_vtable_t number_mrational_vt = {
    NUMBER_MRATIONAL, true, false,
    number_destroy_mrational,
    number_clone_mrational,
    number_to_string_mrational,
    number_is_real_default,
    number_is_zero_mrational,
    number_is_one_mrational,
    number_eq_same_mrational,
    number_set_precision_noop,
    number_precision_zero,
    number_get_exponent2_mrational,
    number_neg_mrational,
    number_inv_mrational,
    number_add_same_mrational,
    number_sub_same_mrational,
    number_mul_same_mrational,
    number_div_same_mrational,
    NULL,
    NULL,
    NULL
};

static const number_vtable_t number_mfloat_vt = {
    NUMBER_MFLOAT, false, false,
    number_destroy_mfloat,
    number_clone_mfloat,
    number_to_string_mfloat,
    number_is_real_default,
    number_is_zero_mfloat,
    number_is_one_mfloat,
    number_eq_same_mfloat,
    number_set_precision_mfloat,
    number_get_precision_mfloat,
    number_get_exponent2_mfloat,
    number_neg_mfloat,
    number_inv_mfloat,
    number_add_same_mfloat,
    number_sub_same_mfloat,
    number_mul_same_mfloat,
    number_div_same_mfloat,
    number_exp_same_mfloat,
    number_log_same_mfloat,
    number_sqrt_same_mfloat
};

static const number_vtable_t number_mcomplex_vt = {
    NUMBER_MCOMPLEX, false, true,
    number_destroy_mcomplex,
    number_clone_mcomplex,
    number_to_string_mcomplex,
    number_is_real_mcomplex,
    number_is_zero_mcomplex,
    number_is_one_mcomplex,
    number_eq_same_mcomplex,
    number_set_precision_mcomplex,
    number_get_precision_mcomplex,
    number_get_exponent2_zero,
    number_neg_mcomplex,
    number_inv_mcomplex,
    number_add_same_mcomplex,
    number_sub_same_mcomplex,
    number_mul_same_mcomplex,
    number_div_same_mcomplex,
    number_exp_same_mcomplex,
    number_log_same_mcomplex,
    number_sqrt_same_mcomplex
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

#define NUMBER_DISPATCH_COUNT (sizeof(number_dispatch) / sizeof(number_dispatch[0]))
#define NUMBER_VT(number) \
    ((((size_t)(number_impl_const(number)->kind)) < NUMBER_DISPATCH_COUNT) \
        ? number_dispatch[number_impl_const(number)->kind] : NULL)

static void number_box_free(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = NUMBER_VT(number);
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
    avt = NUMBER_VT(a);
    bvt = NUMBER_VT(b);

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
    vt = NUMBER_VT(a);
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
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt ? number_take(vt->clone(&number)) : number_invalid();
}

void num_clear(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = NUMBER_VT(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    memset(number, 0, sizeof(*number));
    number_impl(number)->kind = NUMBER_INVALID;
}

bool num_is_exact(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->exact;
}

bool num_is_real(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->is_real && vt->is_real(&number);
}

bool num_is_zero(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->is_zero && vt->is_zero(&number);
}

bool num_is_one(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->is_one && vt->is_one(&number);
}

short num_get_sign(const number_t number)
{
    return (short)num_sign(number);
}

long num_get_exponent2(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    if (!number_is_valid_value(&number) || num_is_zero(number) ||
        !num_is_finite(number) || !num_is_real(number))
        return 0l;
    return vt && vt->get_exponent2 ? vt->get_exponent2(&number) : 0l;
}

int num_set_precision(number_t *number, size_t precision_bits)
{
    const number_vtable_t *vt = NUMBER_VT(number);

    return vt && vt->set_precision ? vt->set_precision(number, precision_bits) : -1;
}

size_t num_get_precision(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->get_precision ? vt->get_precision(&number) : 0u;
}

char *num_to_string(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

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
        const number_vtable_t *vt = NUMBER_VT(&a);
        if (vt && vt->eq_same)
            return vt->eq_same(&a, &b);
    }

    kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (!lhs || !rhs || number_impl_const(lhs)->kind != number_impl_const(rhs)->kind)
        goto done;
    {
        const number_vtable_t *vt = NUMBER_VT(lhs);
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
    const number_vtable_t *vt = NUMBER_VT(&number);

    return vt && vt->neg ? number_take(vt->neg(&number)) : number_invalid();
}

number_t num_inv(const number_t number)
{
    const number_vtable_t *vt = NUMBER_VT(&number);

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
        const number_vtable_t *vt = NUMBER_VT(&number);

        if (!vt)
            return number_invalid();
        if (vt->exp_same)
            return number_take(vt->exp_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = NUMBER_VT(promoted);
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
        const number_vtable_t *vt = NUMBER_VT(&number);

        if (!vt)
            return number_invalid();
        if (vt->log_same)
            return number_take(vt->log_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = NUMBER_VT(promoted);
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
        const number_vtable_t *vt = NUMBER_VT(&number);

        if (!vt)
            return number_invalid();
        if (vt->sqrt_same)
            return number_take(vt->sqrt_same(&number));
    }
    promoted = number_coerce(&number, NUMBER_MFLOAT);
    {
        const number_vtable_t *vt = NUMBER_VT(promoted);
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

static bool number_kind_is_q_only(number_kind_t kind)
{
    return kind == NUMBER_DOUBLE || kind == NUMBER_QFLOAT || kind == NUMBER_QCOMPLEX;
}

static bool number_kind_is_complex(number_kind_t kind)
{
    return kind == NUMBER_QCOMPLEX || kind == NUMBER_MCOMPLEX;
}

static bool number_is_finite_value(const number_t *number)
{
    const mfloat_t *real;
    const mfloat_t *imag;

    if (!number || !number_is_valid_value(number))
        return false;
    if (number_impl_const(number)->kind == NUMBER_DOUBLE)
        return isfinite(number_impl_const(number)->value.d);
    if (number_impl_const(number)->kind == NUMBER_QFLOAT)
        return !qf_isnan(number_impl_const(number)->value.qf) &&
            !qf_isinf(number_impl_const(number)->value.qf);
    if (number_impl_const(number)->kind == NUMBER_QCOMPLEX)
        return !qc_isnan(number_impl_const(number)->value.qc) &&
            !qc_isinf(number_impl_const(number)->value.qc);
    if (number_impl_const(number)->kind == NUMBER_MINT ||
        number_impl_const(number)->kind == NUMBER_MRATIONAL)
        return true;
    if (number_impl_const(number)->kind == NUMBER_MFLOAT)
        return mf_is_finite(number_impl_const(number)->value.mf);
    if (number_impl_const(number)->kind == NUMBER_MCOMPLEX) {
        if (mc_isnan(number_impl_const(number)->value.mc) ||
            mc_isinf(number_impl_const(number)->value.mc))
            return false;
        real = mc_real(number_impl_const(number)->value.mc);
        imag = mc_imag(number_impl_const(number)->value.mc);
        return mf_is_finite(real) && mf_is_finite(imag);
    }
    return false;
}

static bool number_is_nan_value(const number_t *number)
{
    if (!number || !number_is_valid_value(number))
        return true;
    if (number_impl_const(number)->kind == NUMBER_DOUBLE)
        return isnan(number_impl_const(number)->value.d);
    if (number_impl_const(number)->kind == NUMBER_QFLOAT)
        return qf_isnan(number_impl_const(number)->value.qf);
    if (number_impl_const(number)->kind == NUMBER_QCOMPLEX)
        return qc_isnan(number_impl_const(number)->value.qc);
    if (number_impl_const(number)->kind == NUMBER_MINT ||
        number_impl_const(number)->kind == NUMBER_MRATIONAL)
        return false;
    if (number_impl_const(number)->kind == NUMBER_MFLOAT) {
        const mfloat_t *value = number_impl_const(number)->value.mf;
        return mf_is_nan(value);
    }
    if (number_impl_const(number)->kind == NUMBER_MCOMPLEX)
        return mc_isnan(number_impl_const(number)->value.mc);
    return true;
}

static bool number_is_inf_value(const number_t *number)
{
    if (!number || !number_is_valid_value(number))
        return false;
    if (number_impl_const(number)->kind == NUMBER_DOUBLE)
        return isinf(number_impl_const(number)->value.d);
    if (number_impl_const(number)->kind == NUMBER_QFLOAT)
        return qf_isinf(number_impl_const(number)->value.qf);
    if (number_impl_const(number)->kind == NUMBER_QCOMPLEX)
        return qc_isinf(number_impl_const(number)->value.qc);
    if (number_impl_const(number)->kind == NUMBER_MINT ||
        number_impl_const(number)->kind == NUMBER_MRATIONAL)
        return false;
    if (number_impl_const(number)->kind == NUMBER_MFLOAT) {
        const mfloat_t *value = number_impl_const(number)->value.mf;
        return mf_is_inf(value);
    }
    if (number_impl_const(number)->kind == NUMBER_MCOMPLEX)
        return mc_isinf(number_impl_const(number)->value.mc);
    return false;
}

static int number_cmp_same_kind(const number_t *a, const number_t *b)
{
    if (!a || !b)
        return 0;
    if (number_impl_const(a)->kind == NUMBER_DOUBLE) {
        double av = number_impl_const(a)->value.d;
        double bv = number_impl_const(b)->value.d;
        return av < bv ? -1 : (av > bv ? 1 : 0);
    }
    if (number_impl_const(a)->kind == NUMBER_QFLOAT)
        return qf_cmp(number_impl_const(a)->value.qf, number_impl_const(b)->value.qf);
    if (number_impl_const(a)->kind == NUMBER_QCOMPLEX) {
        int rc = qf_cmp(number_impl_const(a)->value.qc.re, number_impl_const(b)->value.qc.re);
        return rc != 0 ? rc : qf_cmp(number_impl_const(a)->value.qc.im, number_impl_const(b)->value.qc.im);
    }
    if (number_impl_const(a)->kind == NUMBER_MINT)
        return mi_cmp(number_impl_const(a)->value.mi, number_impl_const(b)->value.mi);
    if (number_impl_const(a)->kind == NUMBER_MRATIONAL)
        return mr_cmp(number_impl_const(a)->value.mr, number_impl_const(b)->value.mr);
    if (number_impl_const(a)->kind == NUMBER_MFLOAT)
        return mf_cmp(number_impl_const(a)->value.mf, number_impl_const(b)->value.mf);
    if (number_impl_const(a)->kind == NUMBER_MCOMPLEX) {
        int rc = mf_cmp(mc_real(number_impl_const(a)->value.mc), mc_real(number_impl_const(b)->value.mc));
        return rc != 0 ? rc :
            mf_cmp(mc_imag(number_impl_const(a)->value.mc), mc_imag(number_impl_const(b)->value.mc));
    }
    return 0;
}

typedef qfloat_t (*number_qfloat_unary_fn)(qfloat_t);
typedef qcomplex_t (*number_qcomplex_unary_fn)(qcomplex_t);
typedef int (*number_mfloat_unary_mut_fn)(mfloat_t *);
typedef int (*number_mcomplex_unary_mut_fn)(mcomplex_t *);
typedef qfloat_t (*number_qfloat_binary_fn)(qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_binary_fn)(qcomplex_t, qcomplex_t);
typedef int (*number_mfloat_binary_mut_fn)(mfloat_t *, const mfloat_t *);
typedef int (*number_mcomplex_binary_mut_fn)(mcomplex_t *, const mcomplex_t *);
typedef qfloat_t (*number_qfloat_ternary_fn)(qfloat_t, qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_ternary_fn)(qcomplex_t, qcomplex_t, qcomplex_t);
typedef int (*number_mfloat_ternary_mut_fn)(mfloat_t *, const mfloat_t *, const mfloat_t *);
typedef int (*number_mcomplex_ternary_mut_fn)(mcomplex_t *, const mcomplex_t *, const mcomplex_t *);

static number_kind_t number_unary_math_kind(const number_t *number)
{
    number_kind_t kind;

    if (!number)
        return NUMBER_INVALID;
    kind = number_impl_const(number)->kind;
    if (kind == NUMBER_MCOMPLEX)
        return NUMBER_MCOMPLEX;
    if (kind == NUMBER_QCOMPLEX)
        return NUMBER_QCOMPLEX;
    if (kind == NUMBER_MFLOAT || kind == NUMBER_MINT || kind == NUMBER_MRATIONAL)
        return NUMBER_MFLOAT;
    if (kind == NUMBER_QFLOAT || kind == NUMBER_DOUBLE)
        return NUMBER_QFLOAT;
    return NUMBER_INVALID;
}

static number_kind_t number_binary_math_kind(const number_t *a, const number_t *b)
{
    number_kind_t ak, bk;

    if (!a || !b)
        return NUMBER_INVALID;
    ak = number_impl_const(a)->kind;
    bk = number_impl_const(b)->kind;
    if (ak == NUMBER_MCOMPLEX || bk == NUMBER_MCOMPLEX)
        return NUMBER_MCOMPLEX;
    if (ak == NUMBER_QCOMPLEX || bk == NUMBER_QCOMPLEX)
        return number_kind_is_q_only(ak) && number_kind_is_q_only(bk)
            ? NUMBER_QCOMPLEX : NUMBER_MCOMPLEX;
    if (ak == NUMBER_MFLOAT || bk == NUMBER_MFLOAT ||
        ak == NUMBER_MINT || bk == NUMBER_MINT ||
        ak == NUMBER_MRATIONAL || bk == NUMBER_MRATIONAL)
        return NUMBER_MFLOAT;
    if (ak == NUMBER_QFLOAT || bk == NUMBER_QFLOAT ||
        ak == NUMBER_DOUBLE || bk == NUMBER_DOUBLE)
        return NUMBER_QFLOAT;
    return NUMBER_INVALID;
}

static number_t number_apply_unary_math(const number_t number,
                                        number_qfloat_unary_fn qf_fn,
                                        number_qcomplex_unary_fn qc_fn,
                                        number_mfloat_unary_mut_fn mf_fn,
                                        number_mcomplex_unary_mut_fn mc_fn)
{
    number_kind_t kind;
    number_t *promoted = NULL;

    kind = number_unary_math_kind(&number);
    if (kind == NUMBER_QFLOAT && qf_fn) {
        qfloat_t value = number_impl_const(&number)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&number)->value.d)
            : number_impl_const(&number)->value.qf;
        return num_create_qfloat(qf_fn(value));
    }
    if (kind == NUMBER_QCOMPLEX && qc_fn) {
        qcomplex_t value = number_impl_const(&number)->value.qc;
        return num_create_qcomplex(qc_fn(value));
    }
    if (kind == NUMBER_MFLOAT && mf_fn) {
        promoted = number_coerce(&number, NUMBER_MFLOAT);
        if (!promoted || mf_fn(number_impl(promoted)->value.mf) != 0) {
            number_box_free(promoted);
            return number_invalid();
        }
        return number_take(promoted);
    }
    if (kind == NUMBER_MCOMPLEX && mc_fn) {
        promoted = number_coerce(&number, NUMBER_MCOMPLEX);
        if (!promoted || mc_fn(number_impl(promoted)->value.mc) != 0) {
            number_box_free(promoted);
            return number_invalid();
        }
        return number_take(promoted);
    }
    return number_invalid();
}

static number_t number_apply_binary_math(const number_t a,
                                         const number_t b,
                                         number_qfloat_binary_fn qf_fn,
                                         number_qcomplex_binary_fn qc_fn,
                                         number_mfloat_binary_mut_fn mf_fn,
                                         number_mcomplex_binary_mut_fn mc_fn)
{
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;

    kind = number_binary_math_kind(&a, &b);
    if (kind == NUMBER_QFLOAT && qf_fn) {
        qfloat_t av = number_impl_const(&a)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&a)->value.d)
            : number_impl_const(&a)->value.qf;
        qfloat_t bv = number_impl_const(&b)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&b)->value.d)
            : number_impl_const(&b)->value.qf;
        return num_create_qfloat(qf_fn(av, bv));
    }
    if (kind == NUMBER_QCOMPLEX && qc_fn) {
        qcomplex_t av = number_impl_const(&a)->kind == NUMBER_QCOMPLEX
            ? number_impl_const(&a)->value.qc
            : qc_make(number_impl_const(&a)->kind == NUMBER_DOUBLE
                          ? qf_from_double(number_impl_const(&a)->value.d)
                          : number_impl_const(&a)->value.qf,
                      QF_ZERO);
        qcomplex_t bv = number_impl_const(&b)->kind == NUMBER_QCOMPLEX
            ? number_impl_const(&b)->value.qc
            : qc_make(number_impl_const(&b)->kind == NUMBER_DOUBLE
                          ? qf_from_double(number_impl_const(&b)->value.d)
                          : number_impl_const(&b)->value.qf,
                      QF_ZERO);
        return num_create_qcomplex(qc_fn(av, bv));
    }
    if (kind == NUMBER_MFLOAT && mf_fn) {
        lhs = number_coerce(&a, NUMBER_MFLOAT);
        rhs = number_coerce(&b, NUMBER_MFLOAT);
        if (!lhs || !rhs || mf_fn(number_impl(lhs)->value.mf, number_impl_const(rhs)->value.mf) != 0) {
            number_box_free(lhs);
            number_box_free(rhs);
            return number_invalid();
        }
        number_box_free(rhs);
        return number_take(lhs);
    }
    if (kind == NUMBER_MCOMPLEX && mc_fn) {
        lhs = number_coerce(&a, NUMBER_MCOMPLEX);
        rhs = number_coerce(&b, NUMBER_MCOMPLEX);
        if (!lhs || !rhs || mc_fn(number_impl(lhs)->value.mc, number_impl_const(rhs)->value.mc) != 0) {
            number_box_free(lhs);
            number_box_free(rhs);
            return number_invalid();
        }
        number_box_free(rhs);
        return number_take(lhs);
    }
    return number_invalid();
}

static number_t number_apply_ternary_math(const number_t x,
                                          const number_t a,
                                          const number_t b,
                                          number_qfloat_ternary_fn qf_fn,
                                          number_qcomplex_ternary_fn qc_fn,
                                          number_mfloat_ternary_mut_fn mf_fn,
                                          number_mcomplex_ternary_mut_fn mc_fn)
{
    number_kind_t kind;
    number_t *nx = NULL;
    number_t *na = NULL;
    number_t *nb = NULL;

    kind = number_binary_math_kind(&x, &a);
    if (kind != NUMBER_MCOMPLEX && kind != NUMBER_QCOMPLEX)
        kind = number_binary_math_kind(&x, &b);
    else if (!number_kind_is_complex(number_impl_const(&b)->kind) &&
             !number_kind_is_q_only(number_impl_const(&b)->kind))
        kind = NUMBER_MCOMPLEX;

    if (kind == NUMBER_QFLOAT && qf_fn) {
        qfloat_t xv = number_impl_const(&x)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&x)->value.d)
            : number_impl_const(&x)->value.qf;
        qfloat_t av = number_impl_const(&a)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&a)->value.d)
            : number_impl_const(&a)->value.qf;
        qfloat_t bv = number_impl_const(&b)->kind == NUMBER_DOUBLE
            ? qf_from_double(number_impl_const(&b)->value.d)
            : number_impl_const(&b)->value.qf;
        return num_create_qfloat(qf_fn(xv, av, bv));
    }
    if (kind == NUMBER_QCOMPLEX && qc_fn) {
        qcomplex_t xv = number_impl_const(&x)->kind == NUMBER_QCOMPLEX
            ? number_impl_const(&x)->value.qc
            : qc_make(number_impl_const(&x)->kind == NUMBER_DOUBLE
                          ? qf_from_double(number_impl_const(&x)->value.d)
                          : number_impl_const(&x)->value.qf,
                      QF_ZERO);
        qcomplex_t av = number_impl_const(&a)->kind == NUMBER_QCOMPLEX
            ? number_impl_const(&a)->value.qc
            : qc_make(number_impl_const(&a)->kind == NUMBER_DOUBLE
                          ? qf_from_double(number_impl_const(&a)->value.d)
                          : number_impl_const(&a)->value.qf,
                      QF_ZERO);
        qcomplex_t bv = number_impl_const(&b)->kind == NUMBER_QCOMPLEX
            ? number_impl_const(&b)->value.qc
            : qc_make(number_impl_const(&b)->kind == NUMBER_DOUBLE
                          ? qf_from_double(number_impl_const(&b)->value.d)
                          : number_impl_const(&b)->value.qf,
                      QF_ZERO);
        return num_create_qcomplex(qc_fn(xv, av, bv));
    }
    if (kind == NUMBER_MFLOAT && mf_fn) {
        nx = number_coerce(&x, NUMBER_MFLOAT);
        na = number_coerce(&a, NUMBER_MFLOAT);
        nb = number_coerce(&b, NUMBER_MFLOAT);
        if (!nx || !na || !nb ||
            mf_fn(number_impl(nx)->value.mf,
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
    if (kind == NUMBER_MCOMPLEX && mc_fn) {
        nx = number_coerce(&x, NUMBER_MCOMPLEX);
        na = number_coerce(&a, NUMBER_MCOMPLEX);
        nb = number_coerce(&b, NUMBER_MCOMPLEX);
        if (!nx || !na || !nb ||
            mc_fn(number_impl(nx)->value.mc,
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
    return number_invalid();
}

static char *number_format_inexact(const number_t *number, bool scientific, int precision)
{
    int needed;
    char *out;
    char fmt[32];

    if (!number)
        return NULL;
    if (number_impl_const(number)->kind == NUMBER_DOUBLE) {
        double value = number_impl_const(number)->value.d;
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
    if (number_impl_const(number)->kind == NUMBER_QFLOAT) {
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
    if (number_impl_const(number)->kind == NUMBER_QCOMPLEX) {
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
    if (number_impl_const(number)->kind == NUMBER_MFLOAT) {
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
    if (number_impl_const(number)->kind == NUMBER_MCOMPLEX) {
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
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return NAN;
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return number_impl_const(&number)->value.d;
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return qf_to_double(number_impl_const(&number)->value.qf);
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return mf_to_double(number_impl_const(&number)->value.mf);
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
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return QF_NAN;
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return number_impl_const(&number)->value.qf;
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return qf_from_double(number_impl_const(&number)->value.d);
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return mf_to_qfloat(number_impl_const(&number)->value.mf);
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
            core = (num_is_exact(value) && !number_kind_is_complex(number_impl_const(&value)->kind))
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
    if (!number_is_valid_value(&number) || !num_is_real(number))
        return false;
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE) {
        double x = number_impl_const(&number)->value.d;
        return isfinite(x) && floor(x) == x;
    }
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return qf_eq(qf_floor(number_impl_const(&number)->value.qf), number_impl_const(&number)->value.qf);
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return qf_eq(number_impl_const(&number)->value.qc.im, QF_ZERO) &&
            qf_eq(qf_floor(number_impl_const(&number)->value.qc.re), number_impl_const(&number)->value.qc.re);
    if (number_impl_const(&number)->kind == NUMBER_MINT)
        return true;
    if (number_impl_const(&number)->kind == NUMBER_MRATIONAL)
        return mr_is_integer(number_impl_const(&number)->value.mr);
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT) {
        number_t copy = num_clone(number);
        number_t floored = num_floor(copy);
        bool rc = num_eq(copy, floored);
        num_clear(&copy);
        num_clear(&floored);
        return rc;
    }
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX) {
        number_t imag = num_imag_part(number);
        number_t real = num_real_part(number);
        number_t floored = num_floor(real);
        bool rc = num_is_zero(imag) && num_eq(real, floored);
        num_clear(&imag);
        num_clear(&real);
        num_clear(&floored);
        return rc;
    }
    return false;
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
    if (!number_is_valid_value(&number))
        return 0u;
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return isfinite(number_impl_const(&number)->value.d) && number_impl_const(&number)->value.d != 0.0 ? 53u : 0u;
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return (!qf_isnan(number_impl_const(&number)->value.qf) && !qf_isinf(number_impl_const(&number)->value.qf) &&
                !qf_eq(number_impl_const(&number)->value.qf, QF_ZERO)) ? 106u : 0u;
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return mf_get_mantissa_bits(number_impl_const(&number)->value.mf);
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX)
        return mf_get_mantissa_bits(mc_real(number_impl_const(&number)->value.mc));
    return 0u;
}

bool num_get_mantissa_u64(const number_t number, uint64_t *out)
{
    if (!out || !number_is_valid_value(&number))
        return false;
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return mf_get_mantissa_u64(number_impl_const(&number)->value.mf, out);
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX)
        return mf_get_mantissa_u64(mc_real(number_impl_const(&number)->value.mc), out);
    return false;
}

int num_sign(const number_t number)
{
    if (num_is_zero(number))
        return 0;
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return number_impl_const(&number)->value.d < 0.0 ? -1 : 1;
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return qf_lt(number_impl_const(&number)->value.qf, QF_ZERO) ? -1 : 1;
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return 0;
    if (number_impl_const(&number)->kind == NUMBER_MINT)
        return mi_is_negative(number_impl_const(&number)->value.mi) ? -1 : 1;
    if (number_impl_const(&number)->kind == NUMBER_MRATIONAL)
        return mi_is_negative(mr_numerator(number_impl_const(&number)->value.mr)) ? -1 : 1;
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return mf_get_sign(number_impl_const(&number)->value.mf);
    return 0;
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
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    int rc = 0;

    if (!number_is_valid_value(&a) || !number_is_valid_value(&b) ||
        !num_is_real(a) || !num_is_real(b))
        return 0;
    if (number_impl_const(&a)->kind == number_impl_const(&b)->kind)
        return number_cmp_same_kind(&a, &b);
    kind = number_kind_is_complex(number_impl_const(&a)->kind) ||
           number_kind_is_complex(number_impl_const(&b)->kind)
           ? number_binary_math_kind(&a, &b)
           : number_common_kind(&a, &b, NUMBER_OP_ADD);
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
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qfloat(qc_abs(number_impl_const(&number)->value.qc));
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX) {
        mcomplex_t *copy = mc_clone(number_impl_const(&number)->value.mc);
        number_t result;
        if (!copy || mc_abs(copy) != 0) {
            mc_free(copy);
            return number_invalid();
        }
        result = num_create_mfloat_prec(mc_real(copy), mc_get_precision(copy));
        mc_free(copy);
        return result;
    }
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(fabs(number_impl_const(&number)->value.d));
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(qf_abs(number_impl_const(&number)->value.qf));
    if (number_impl_const(&number)->kind == NUMBER_MINT) {
        mint_t *copy = mi_clone(number_impl_const(&number)->value.mi);
        if (!copy || mi_abs(copy) != 0) {
            mi_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mint(copy));
    }
    if (number_impl_const(&number)->kind == NUMBER_MRATIONAL) {
        mrational_t *copy = mr_clone(number_impl_const(&number)->value.mr);
        if (!copy || mr_abs(copy) != 0) {
            mr_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mrational(copy));
    }
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT) {
        mfloat_t *copy = mf_clone(number_impl_const(&number)->value.mf);
        if (!copy || mf_abs(copy) != 0) {
            mf_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mfloat(copy));
    }
    return number_invalid();
}

number_t num_conj(const number_t number)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (!number_kind_is_complex(number_impl_const(&number)->kind))
        return num_clone(number);
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qcomplex(qc_conj(number_impl_const(&number)->value.qc));
    {
        mcomplex_t *copy = mc_clone(number_impl_const(&number)->value.mc);
        if (!copy || mc_conj(copy) != 0) {
            mc_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mcomplex(copy));
    }
}

number_t num_real_part(const number_t number)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (!number_kind_is_complex(number_impl_const(&number)->kind))
        return num_clone(number);
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qfloat(number_impl_const(&number)->value.qc.re);
    return num_create_mfloat_prec(mc_real(number_impl_const(&number)->value.mc),
                                  mc_get_precision(number_impl_const(&number)->value.mc));
}

number_t num_imag_part(const number_t number)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(0.0);
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(QF_ZERO);
    if (number_impl_const(&number)->kind == NUMBER_MINT)
        return num_create_long(0L);
    if (number_impl_const(&number)->kind == NUMBER_MRATIONAL)
        return num_create_string("0");
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT)
        return num_create_mfloat_prec(MF_ZERO, num_get_precision(number));
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qfloat(number_impl_const(&number)->value.qc.im);
    return num_create_mfloat_prec(mc_imag(number_impl_const(&number)->value.mc),
                                  mc_get_precision(number_impl_const(&number)->value.mc));
}

number_t num_arg(const number_t number)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(atan2(0.0, number_impl_const(&number)->value.d));
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(qf_atan2(QF_ZERO, number_impl_const(&number)->value.qf));
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qfloat(qc_arg(number_impl_const(&number)->value.qc));
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX) {
        mfloat_t *arg = mf_clone(mc_imag(number_impl_const(&number)->value.mc));
        if (!arg || mf_atan2(arg, mc_real(number_impl_const(&number)->value.mc)) != 0) {
            mf_free(arg);
            return number_invalid();
        }
        return number_take(number_wrap_mfloat(arg));
    }
    {
        number_t zero = num_create_mfloat_prec(MF_ZERO, num_get_precision(number) ? num_get_precision(number) : number_default_precision_bits);
        number_t real = number_kind_is_complex(number_impl_const(&number)->kind) ? num_real_part(number) : num_clone(number);
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
    if (!number_is_valid_value(&base))
        return number_invalid();
    if (number_impl_const(&base)->kind == NUMBER_DOUBLE)
        return num_create_double(pow(number_impl_const(&base)->value.d, (double)exponent));
    if (number_impl_const(&base)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(qf_pow_int(number_impl_const(&base)->value.qf, exponent));
    if (number_impl_const(&base)->kind == NUMBER_QCOMPLEX) {
        qcomplex_t e = qc_make(qf_from_double((double)exponent), QF_ZERO);
        return num_create_qcomplex(qc_pow(number_impl_const(&base)->value.qc, e));
    }
    if (number_impl_const(&base)->kind == NUMBER_MINT && exponent >= 0) {
        mint_t *copy = mi_clone(number_impl_const(&base)->value.mi);
        if (!copy || mi_pow(copy, (unsigned long)exponent) != 0) {
            mi_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mint(copy));
    }
    if (number_impl_const(&base)->kind == NUMBER_MFLOAT) {
        mfloat_t *copy = mf_clone(number_impl_const(&base)->value.mf);
        if (!copy || mf_pow_int(copy, exponent) != 0) {
            mf_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mfloat(copy));
    }
    if (number_impl_const(&base)->kind == NUMBER_MCOMPLEX) {
        mcomplex_t *copy = mc_clone(number_impl_const(&base)->value.mc);
        if (!copy || mc_pow_int(copy, exponent) != 0) {
            mc_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mcomplex(copy));
    }
    {
        number_t expnum = num_create_long(exponent);
        number_t result = num_pow(base, expnum);
        num_clear(&expnum);
        return result;
    }
}

number_t num_ldexp(const number_t number, int exponent2)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(ldexp(number_impl_const(&number)->value.d, exponent2));
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(qf_ldexp(number_impl_const(&number)->value.qf, exponent2));
    if (number_impl_const(&number)->kind == NUMBER_QCOMPLEX)
        return num_create_qcomplex(qc_ldexp(number_impl_const(&number)->value.qc, exponent2));
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT) {
        mfloat_t *copy = mf_clone(number_impl_const(&number)->value.mf);
        if (!copy || mf_ldexp(copy, exponent2) != 0) {
            mf_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mfloat(copy));
    }
    if (number_impl_const(&number)->kind == NUMBER_MCOMPLEX) {
        mcomplex_t *copy = mc_clone(number_impl_const(&number)->value.mc);
        if (!copy || mc_ldexp(copy, exponent2) != 0) {
            mc_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mcomplex(copy));
    }
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
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(floor(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_floor, qc_floor, mf_floor, mc_floor);
}
number_t num_mul_pow10(const number_t number, int exponent10)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(number_impl_const(&number)->value.d * pow(10.0, (double)exponent10));
    if (number_impl_const(&number)->kind == NUMBER_QFLOAT)
        return num_create_qfloat(qf_mul_pow10(number_impl_const(&number)->value.qf, exponent10));
    if (number_impl_const(&number)->kind == NUMBER_MFLOAT) {
        mfloat_t *copy = mf_clone(number_impl_const(&number)->value.mf);
        if (!copy || mf_mul_pow10(copy, exponent10) != 0) {
            mf_free(copy);
            return number_invalid();
        }
        return number_take(number_wrap_mfloat(copy));
    }
    return num_mul(number, num_pow10(exponent10));
}
number_t num_hypot(const number_t a, const number_t b) { return number_apply_binary_math(a, b, qf_hypot, qc_hypot, mf_hypot, mc_hypot); }

int num_sincos(const number_t x, number_t *sin_out, number_t *cos_out)
{
    if (!sin_out || !cos_out || !number_is_valid_value(&x))
        return -1;
    if (number_impl_const(&x)->kind == NUMBER_DOUBLE) {
        number_assign(sin_out, num_create_double(sin(number_impl_const(&x)->value.d)));
        number_assign(cos_out, num_create_double(cos(number_impl_const(&x)->value.d)));
        return 0;
    }
    if (number_impl_const(&x)->kind == NUMBER_QFLOAT) {
        number_assign(sin_out, num_create_qfloat(qf_sin(number_impl_const(&x)->value.qf)));
        number_assign(cos_out, num_create_qfloat(qf_cos(number_impl_const(&x)->value.qf)));
        return 0;
    }
    if (number_impl_const(&x)->kind == NUMBER_MFLOAT ||
        number_impl_const(&x)->kind == NUMBER_MINT ||
        number_impl_const(&x)->kind == NUMBER_MRATIONAL) {
        number_t *tmp = number_coerce(&x, NUMBER_MFLOAT);
        mfloat_t *s = NULL, *c = NULL;
        int rc;
        if (!tmp)
            return -1;
        s = mf_new_prec(mf_get_precision(number_impl_const(tmp)->value.mf));
        c = mf_new_prec(mf_get_precision(number_impl_const(tmp)->value.mf));
        rc = (!s || !c || mf_sincos(number_impl_const(tmp)->value.mf, s, c) != 0) ? -1 : 0;
        number_box_free(tmp);
        if (rc != 0) {
            mf_free(s);
            mf_free(c);
            return -1;
        }
        number_assign(sin_out, number_take(number_wrap_mfloat(s)));
        number_assign(cos_out, number_take(number_wrap_mfloat(c)));
        return 0;
    }
    return -1;
}

number_t num_sin(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(sin(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_sin, qc_sin, mf_sin, mc_sin);
}
number_t num_cos(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(cos(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_cos, qc_cos, mf_cos, mc_cos);
}
number_t num_tan(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(tan(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_tan, qc_tan, mf_tan, mc_tan);
}
number_t num_atan(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(atan(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_atan, qc_atan, mf_atan, mc_atan);
}
number_t num_atan2(const number_t y, const number_t x)
{
    if (number_impl_const(&y)->kind == NUMBER_DOUBLE && number_impl_const(&x)->kind == NUMBER_DOUBLE)
        return num_create_double(atan2(number_impl_const(&y)->value.d, number_impl_const(&x)->value.d));
    return number_apply_binary_math(y, x, qf_atan2, qc_atan2, mf_atan2, mc_atan2);
}
number_t num_asin(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(asin(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_asin, qc_asin, mf_asin, mc_asin);
}
number_t num_acos(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(acos(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_acos, qc_acos, mf_acos, mc_acos);
}
number_t num_sinh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(sinh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_sinh, qc_sinh, mf_sinh, mc_sinh);
}
number_t num_cosh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(cosh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_cosh, qc_cosh, mf_cosh, mc_cosh);
}

int num_sinhcosh(const number_t x, number_t *sinh_out, number_t *cosh_out)
{
    if (!sinh_out || !cosh_out || !number_is_valid_value(&x))
        return -1;
    if (number_impl_const(&x)->kind == NUMBER_DOUBLE) {
        number_assign(sinh_out, num_create_double(sinh(number_impl_const(&x)->value.d)));
        number_assign(cosh_out, num_create_double(cosh(number_impl_const(&x)->value.d)));
        return 0;
    }
    if (number_impl_const(&x)->kind == NUMBER_QFLOAT) {
        number_assign(sinh_out, num_create_qfloat(qf_sinh(number_impl_const(&x)->value.qf)));
        number_assign(cosh_out, num_create_qfloat(qf_cosh(number_impl_const(&x)->value.qf)));
        return 0;
    }
    if (number_impl_const(&x)->kind == NUMBER_MFLOAT ||
        number_impl_const(&x)->kind == NUMBER_MINT ||
        number_impl_const(&x)->kind == NUMBER_MRATIONAL) {
        number_t *tmp = number_coerce(&x, NUMBER_MFLOAT);
        mfloat_t *s = NULL, *c = NULL;
        int rc;
        if (!tmp)
            return -1;
        s = mf_new_prec(mf_get_precision(number_impl_const(tmp)->value.mf));
        c = mf_new_prec(mf_get_precision(number_impl_const(tmp)->value.mf));
        rc = (!s || !c || mf_sinhcosh(number_impl_const(tmp)->value.mf, s, c) != 0) ? -1 : 0;
        number_box_free(tmp);
        if (rc != 0) {
            mf_free(s);
            mf_free(c);
            return -1;
        }
        number_assign(sinh_out, number_take(number_wrap_mfloat(s)));
        number_assign(cosh_out, number_take(number_wrap_mfloat(c)));
        return 0;
    }
    return -1;
}

number_t num_tanh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(tanh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_tanh, qc_tanh, mf_tanh, mc_tanh);
}
number_t num_asinh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(asinh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_asinh, qc_asinh, mf_asinh, mc_asinh);
}
number_t num_acosh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(acosh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_acosh, qc_acosh, mf_acosh, mc_acosh);
}
number_t num_atanh(const number_t number)
{
    if (number_impl_const(&number)->kind == NUMBER_DOUBLE)
        return num_create_double(atanh(number_impl_const(&number)->value.d));
    return number_apply_unary_math(number, qf_atanh, qc_atanh, mf_atanh, mc_atanh);
}

#define NUMBER_UNARY_SPECIAL_IMPL(name, qf_fn, qc_fn, mf_fn, mc_fn) \
number_t name(const number_t number) \
{ \
    return number_apply_unary_math(number, qf_fn, qc_fn, mf_fn, mc_fn); \
}

#define NUMBER_BINARY_SPECIAL_IMPL(name, qf_fn, qc_fn, mf_fn, mc_fn) \
number_t name(const number_t a, const number_t b) \
{ \
    return number_apply_binary_math(a, b, qf_fn, qc_fn, mf_fn, mc_fn); \
}

#define NUMBER_TERNARY_SPECIAL_IMPL(name, qf_fn, qc_fn, mf_fn, mc_fn) \
number_t name(const number_t x, const number_t a, const number_t b) \
{ \
    return number_apply_ternary_math(x, a, b, qf_fn, qc_fn, mf_fn, mc_fn); \
}

NUMBER_UNARY_SPECIAL_IMPL(num_gamma, qf_gamma, qc_gamma, mf_gamma, mc_gamma)
NUMBER_UNARY_SPECIAL_IMPL(num_lgamma, NULL, NULL, mf_lgamma, mc_lgamma)
NUMBER_UNARY_SPECIAL_IMPL(num_digamma, qf_digamma, qc_digamma, mf_digamma, mc_digamma)
NUMBER_UNARY_SPECIAL_IMPL(num_trigamma, qf_trigamma, qc_trigamma, mf_trigamma, mc_trigamma)
NUMBER_UNARY_SPECIAL_IMPL(num_tetragamma, qf_tetragamma, qc_tetragamma, mf_tetragamma, mc_tetragamma)
NUMBER_UNARY_SPECIAL_IMPL(num_gammainv, qf_gammainv, qc_gammainv, mf_gammainv, mc_gammainv)
NUMBER_UNARY_SPECIAL_IMPL(num_erf, qf_erf, qc_erf, mf_erf, mc_erf)
NUMBER_UNARY_SPECIAL_IMPL(num_erfc, qf_erfc, qc_erfc, mf_erfc, mc_erfc)
NUMBER_UNARY_SPECIAL_IMPL(num_erfinv, qf_erfinv, qc_erfinv, mf_erfinv, mc_erfinv)
NUMBER_UNARY_SPECIAL_IMPL(num_erfcinv, qf_erfcinv, qc_erfcinv, mf_erfcinv, mc_erfcinv)
NUMBER_UNARY_SPECIAL_IMPL(num_lambert_w0, qf_lambert_w0, NULL, mf_lambert_w0, mc_lambert_w0)
NUMBER_UNARY_SPECIAL_IMPL(num_lambert_wm1, qf_lambert_wm1, qc_lambert_wm1, mf_lambert_wm1, mc_lambert_wm1)
NUMBER_BINARY_SPECIAL_IMPL(num_beta, qf_beta, qc_beta, mf_beta, mc_beta)
NUMBER_BINARY_SPECIAL_IMPL(num_logbeta, qf_logbeta, qc_logbeta, mf_logbeta, mc_logbeta)
NUMBER_BINARY_SPECIAL_IMPL(num_binomial, qf_binomial, qc_binomial, mf_binomial, mc_binomial)
NUMBER_TERNARY_SPECIAL_IMPL(num_beta_pdf, qf_beta_pdf, qc_beta_pdf, mf_beta_pdf, mc_beta_pdf)
NUMBER_TERNARY_SPECIAL_IMPL(num_logbeta_pdf, qf_logbeta_pdf, qc_logbeta_pdf, mf_logbeta_pdf, mc_logbeta_pdf)
NUMBER_UNARY_SPECIAL_IMPL(num_normal_pdf, qf_normal_pdf, qc_normal_pdf, mf_normal_pdf, mc_normal_pdf)
NUMBER_UNARY_SPECIAL_IMPL(num_normal_cdf, qf_normal_cdf, qc_normal_cdf, mf_normal_cdf, mc_normal_cdf)
NUMBER_UNARY_SPECIAL_IMPL(num_normal_logpdf, qf_normal_logpdf, qc_normal_logpdf, mf_normal_logpdf, mc_normal_logpdf)
NUMBER_UNARY_SPECIAL_IMPL(num_productlog, qf_productlog, qc_productlog, mf_productlog, mc_productlog)
NUMBER_BINARY_SPECIAL_IMPL(num_gammainc_lower, qf_gammainc_lower, qc_gammainc_lower, mf_gammainc_lower, mc_gammainc_lower)
NUMBER_BINARY_SPECIAL_IMPL(num_gammainc_upper, qf_gammainc_upper, qc_gammainc_upper, mf_gammainc_upper, mc_gammainc_upper)
NUMBER_BINARY_SPECIAL_IMPL(num_gammainc_P, qf_gammainc_P, qc_gammainc_P, mf_gammainc_P, mc_gammainc_P)
NUMBER_BINARY_SPECIAL_IMPL(num_gammainc_Q, qf_gammainc_Q, qc_gammainc_Q, mf_gammainc_Q, mc_gammainc_Q)
NUMBER_UNARY_SPECIAL_IMPL(num_ei, qf_ei, qc_ei, mf_ei, mc_ei)
NUMBER_UNARY_SPECIAL_IMPL(num_e1, qf_e1, qc_e1, mf_e1, mc_e1)
