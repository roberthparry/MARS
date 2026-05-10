#ifndef MARS_NUMBER_INTERNAL_H
#define MARS_NUMBER_INTERNAL_H

#include <string.h>

#include "number.h"

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

extern const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_MCOMPLEX + 1];
extern const number_kind_t number_math_family_target_kind_table[];
extern const number_vtable_t *const number_dispatch[];
extern const size_t number_dispatch_count;
extern size_t number_default_precision_bits;

static inline number_private_t *number_impl(number_t *number)
{
    return (number_private_t *)number;
}

static inline const number_private_t *number_impl_const(const number_t *number)
{
    return (const number_private_t *)number;
}

static inline bool number_is_valid_value(const number_t *number)
{
    return number != NULL && number_impl_const(number)->kind != NUMBER_INVALID;
}

static inline const number_vtable_t *number_vt(const number_t *number)
{
    size_t kind;

    if (!number)
        return NULL;
    kind = (size_t)number_impl_const(number)->kind;
    return kind < number_dispatch_count
        ? number_dispatch[kind] : NULL;
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

number_t number_invalid(void);
number_t number_take(number_t *boxed_number);
char *number_strdup(const char *text);
void number_box_free(number_t *number);
void number_assign(number_t *dst, number_t value);
char *number_format_double(const number_t *number, bool scientific, int precision);
char *number_format_qfloat(const number_t *number, bool scientific, int precision);
char *number_format_qcomplex(const number_t *number, bool scientific, int precision);
char *number_format_mfloat(const number_t *number, bool scientific, int precision);
char *number_format_mcomplex(const number_t *number, bool scientific, int precision);
number_t *number_wrap_double(double value);
number_t *number_wrap_qfloat(qfloat_t value);
number_t *number_wrap_qcomplex(qcomplex_t value);
number_t *number_wrap_mint(mint_t *value);
number_t *number_wrap_mrational(mrational_t *value);
number_t *number_wrap_mfloat(mfloat_t *value);
number_t *number_wrap_mcomplex(mcomplex_t *value);
number_kind_t number_common_kind(const number_t *a, const number_t *b,
                                 number_binary_op_t op);
number_t *number_coerce(const number_t *number, number_kind_t target_kind);

#endif
