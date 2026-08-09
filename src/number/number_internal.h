#ifndef MARS_NUMBER_INTERNAL_H
#define MARS_NUMBER_INTERNAL_H

#if !defined(MARS_NUMBER_INTERNAL_ACCESS) &&                                                                           \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "number_internal.h is private to the number module; include number.h instead."
#endif

#include <string.h>

#include <mpc.h>

#include "number.h"

typedef enum number_kind_t {
    NUMBER_INVALID,
    NUMBER_DOUBLE,
    NUMBER_QFLOAT,
    NUMBER_QCOMPLEX,
    NUMBER_MPZ,
    NUMBER_MPQ,
    NUMBER_MPFR,
    NUMBER_CDOUBLE,
    NUMBER_COMPLEX
} number_kind_t;

typedef enum number_math_family_t {
    NUMBER_MATH_INVALID,
    NUMBER_MATH_QREAL,
    NUMBER_MATH_QCOMPLEX,
    NUMBER_MATH_MPFR,
    NUMBER_MATH_COMPLEX
} number_math_family_t;

typedef enum number_const_id_t number_const_id_t;

/*
 * Representation vtable: payload lifetime, conversion, predicates and the
 * primitive same-backend operations needed throughout the number module.
 * Higher mathematical functions are dispatched by number_maths.c according
 * to math_family; they are deliberately not repeated in every backend vtable.
 */
typedef struct number_vtable_t {
    number_kind_t kind;
    number_math_family_t math_family;
    bool exact;
    bool is_complex;
    void (*destroy_payload)(number_t *number);
    void *(*scope_payload)(const number_t *number);
    void (*destroy_scope_payload)(void *payload);
    number_t *(*clone)(const number_t *number);
    number_t *(*const_prec)(const number_t *number, size_t precision_bits);
    string_t *(*to_text)(const number_t *number);
    bool (*is_immortal)(const number_t *number);
    bool (*immortal_id)(const number_t *number, number_const_id_t *id_out);
    bool (*is_real)(const number_t *number);
    bool (*is_zero)(const number_t *number);
    bool (*is_one)(const number_t *number);
    bool (*is_finite)(const number_t *number);
    bool (*is_nan)(const number_t *number);
    bool (*is_inf)(const number_t *number);
    bool (*eq_same)(const number_t *a, const number_t *b);
    bool (*eq_same_tol)(const number_t *a, const number_t *b);
    int (*cmp_same)(const number_t *a, const number_t *b);
    number_t (*const_like)(const number_t *like, number_const_id_t id);
    number_t (*imag_const_like)(const number_t *like, number_const_id_t id);
    string_t *(*format_inexact_text)(const number_t *number, bool scientific, int precision);
    int (*set_precision)(number_t *number, size_t precision_bits);
    size_t (*get_precision)(const number_t *number);
    size_t (*get_effective_precision)(const number_t *number);
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
typedef struct complex_t complex_t;
typedef struct number_mpz_t number_mpz_t;
typedef struct number_mpq_t number_mpq_t;
typedef struct number_mpfr_t number_mpfr_t;

typedef struct number_cdouble_t {
    double _Complex value;
    int constant_id;
    bool immortal;
} number_cdouble_t;

typedef struct {
    number_kind_t kind;
    union {
        double d;
        number_cdouble_t cd;
        qfloat_t qf;
        qcomplex_t qc;
        number_mpz_t *mpz;
        number_mpq_t *mpq;
        number_mpfr_t *mpfr;
        complex_t *cx;
    } value;
} number_private_t;

typedef struct complex_t {
    int constant_id;
    size_t precision_bits;
    number_t real;
    number_t imag;
    bool mpc_cache_valid;
    mpc_t mpc_cache;
} complex_t;

typedef enum number_binary_op_t { NUMBER_OP_ADD, NUMBER_OP_SUB, NUMBER_OP_MUL, NUMBER_OP_DIV } number_binary_op_t;

typedef enum number_const_id_t {
    NUMBER_CONST_ZERO,
    NUMBER_CONST_ONE,
    NUMBER_CONST_NEG_ONE,
    NUMBER_CONST_HALF,
    NUMBER_CONST_ONE_AND_HALF,
    NUMBER_CONST_ONE_THIRD,
    NUMBER_CONST_QUARTER,
    NUMBER_CONST_ONE_SIXTH,
    NUMBER_CONST_ONE_EIGHTH,
    NUMBER_CONST_ONE_TENTH,
    NUMBER_CONST_TWO,
    NUMBER_CONST_TEN,
    NUMBER_CONST_PI,
    NUMBER_CONST_2PI,
    NUMBER_CONST_PI_2,
    NUMBER_CONST_NEG_PI_2,
    NUMBER_CONST_PI_4,
    NUMBER_CONST_3PI_4,
    NUMBER_CONST_PI_6,
    NUMBER_CONST_PI_3,
    NUMBER_CONST_2_PI,
    NUMBER_CONST_E,
    NUMBER_CONST_INV_E,
    NUMBER_CONST_NEG_INV_E,
    NUMBER_CONST_LN2,
    NUMBER_CONST_LN10,
    NUMBER_CONST_INVLN2,
    NUMBER_CONST_EULER_MASCHERONI,
    NUMBER_CONST_PHI,
    NUMBER_CONST_SQRT_HALF,
    NUMBER_CONST_SQRT2,
    NUMBER_CONST_SQRT3,
    NUMBER_CONST_SQRT2_OVER_TWO,
    NUMBER_CONST_SQRT3_OVER_TWO,
    NUMBER_CONST_SQRT_2PI,
    NUMBER_CONST_SQRT_PI,
    NUMBER_CONST_SQRT_PI_OVER_TWO,
    NUMBER_CONST_SQRT1ONPI,
    NUMBER_CONST_2_SQRTPI,
    NUMBER_CONST_NEG_TWO_OVER_SQRT_PI,
    NUMBER_CONST_INV_SQRT_2PI,
    NUMBER_CONST_LOG_SQRT_2PI,
    NUMBER_CONST_LN_2PI,
    NUMBER_CONST_PI_SQUARED,
    NUMBER_CONST_2PI_CUBED,
    NUMBER_CONST_NAN,
    NUMBER_CONST_INF,
    NUMBER_CONST_NINF,
    NUMBER_CONST_I,
    NUMBER_CONST_NEG_I,
    NUMBER_CONST_COUNT
} number_const_id_t;

struct number_mpfr_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpfr_t value;
};

struct number_mpz_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpz_t value;
};

struct number_mpq_t {
    number_const_id_t constant_id;
    bool immortal;
    bool initialised;
    mpq_t value;
};

extern const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_COMPLEX + 1];
extern const number_kind_t number_math_family_target_kind_table[];
/* Concrete backend vtables (defined in number_vtables.c). */
extern const number_vtable_t number_double_vt;
extern const number_vtable_t number_qfloat_vt;
extern const number_vtable_t number_qcomplex_vt;
extern const number_vtable_t number_mpz_vt;
extern const number_vtable_t number_mpq_vt;
extern const number_vtable_t number_mpfr_vt;
extern const number_vtable_t number_cdouble_vt;
extern const number_vtable_t number_complex_vt;
/* Backend vtable registry (defined in number_vtables.c). */
extern const number_vtable_t *const number_dispatch[];
extern const size_t number_dispatch_count;
extern size_t number_default_precision_bits;

number_t *number_wrap_complex(complex_t *value);
number_t number_take_mpz(number_mpz_t *value);
number_t number_take_mpq(number_mpq_t *value);
number_t number_take_mpfr(number_mpfr_t *value);
number_t number_complex_component_from_number(const number_t *value, size_t precision_bits);
complex_t *number_complex_create(number_t real, number_t imag);
complex_t *number_complex_clone(const complex_t *value);
complex_t *number_complex_create_from_qcomplex(qcomplex_t value, size_t precision_bits);
const number_t *number_complex_real_ref(const complex_t *value);
const number_t *number_complex_imag_ref(const complex_t *value);
number_const_id_t number_complex_const_id(const complex_t *value);

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
    return kind < number_dispatch_count ? number_dispatch[kind] : NULL;
}

static inline number_kind_t number_kind_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) && vt ? vt->kind : NUMBER_INVALID;
}

static inline bool number_same_kind_value(const number_t *a, const number_t *b)
{
    number_kind_t ak = number_kind_value(a);
    number_kind_t bk = number_kind_value(b);

    return ak != NUMBER_INVALID && ak == bk;
}

static inline number_math_family_t number_math_family_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) && vt ? vt->math_family : NUMBER_MATH_INVALID;
}

static inline number_math_family_t number_math_family_binary(number_math_family_t a, number_math_family_t b)
{
    return (unsigned)a <= NUMBER_MATH_COMPLEX && (unsigned)b <= NUMBER_MATH_COMPLEX
               ? number_math_family_binary_table[a][b]
               : NUMBER_MATH_INVALID;
}

static inline number_kind_t number_math_family_target_kind(number_math_family_t family)
{
    return (unsigned)family <= NUMBER_MATH_COMPLEX ? number_math_family_target_kind_table[family] : NUMBER_INVALID;
}

static inline qfloat_t number_value_to_qfloat(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (!number)
        return QF_NAN;
    return vt && vt->to_qfloat ? vt->to_qfloat(number) : num_to_qfloat(*number);
}

static inline qcomplex_t number_value_to_qcomplex(const number_t *number)
{
    double _Complex cd;
    const complex_t *cx;

    if (!number)
        return QC_NAN;
    if (number_kind_value(number) == NUMBER_QCOMPLEX)
        return number_impl_const(number)->value.qc;
    if (number_kind_value(number) == NUMBER_CDOUBLE) {
        cd = number_impl_const(number)->value.cd.value;
        return qc_make(qf_from_double(__real__ cd), qf_from_double(__imag__ cd));
    }
    if (number_kind_value(number) == NUMBER_COMPLEX) {
        cx = number_impl_const(number)->value.cx;
        return cx ? qc_make(number_value_to_qfloat(&cx->real), number_value_to_qfloat(&cx->imag)) : QC_NAN;
    }
    return qc_make(number_value_to_qfloat(number), QF_ZERO);
}

number_t number_invalid(void);
bool num_is_immortal(number_t number);
bool num_get_small_rational(number_t number, long *numerator, long *denominator);
bool num_is_inexact_real_backend(number_t number);
number_t num_as_inexact_real_prec(number_t number, size_t precision_bits);
number_t number_take(number_t *boxed_number);
number_t *number_box_value(number_t value);
char *number_strdup(const char *text);
char *number_cstring_from_text(string_t *text);
void number_box_free(number_t *number);
void number_assign(number_t *dst, number_t value);
void number_scope_register_value(const number_t *number);
int number_scope_unregister_value(const number_t *number);
num_scope_t *number_scope_suspend(void);
void number_scope_resume(num_scope_t *scope);
void num_scope_resume_cleanup(num_scope_t **scope);

#define NUM_SCOPE_SUSPEND(name)                                                                                        \
    __attribute__((cleanup(num_scope_resume_cleanup))) num_scope_t *(name) = number_scope_suspend()

string_t *number_format_double_text(const number_t *number, bool scientific, int precision);
string_t *number_format_cdouble_text(const number_t *number, bool scientific, int precision);
string_t *number_format_qfloat_text(const number_t *number, bool scientific, int precision);
string_t *number_format_qcomplex_text(const number_t *number, bool scientific, int precision);
string_t *number_format_mpfr_text(const number_t *number, bool scientific, int precision);
string_t *number_format_complex_text(const number_t *number, bool scientific, int precision);
char *number_to_cstring(const number_t *number);
number_t *number_wrap_double(double value);
number_t *number_wrap_cdouble(double _Complex value);
number_t *number_wrap_qfloat(qfloat_t value);
number_t *number_wrap_qcomplex(qcomplex_t value);
number_t *number_wrap_mpz(number_mpz_t *value);
number_t *number_wrap_mpq(number_mpq_t *value);
number_t *number_wrap_mpfr(number_mpfr_t *value);
number_t *number_wrap_complex_parts(number_t real, number_t imag);
number_mpz_t *number_mpz_from_const_id(number_const_id_t id);
number_mpz_t *number_mpz_new(void);
number_mpz_t *number_mpz_from_long(long value);
number_mpz_t *number_mpz_from_mpz(mpz_srcptr value);
number_mpz_t *number_mpz_from_text(const string_t *text);
number_mpz_t *number_mpz_clone(const number_mpz_t *value);
void number_mpz_free(number_mpz_t *value);
int number_mpz_ensure(const number_mpz_t *value);
mpz_srcptr number_mpz_value(const number_mpz_t *value);
string_t *number_mpz_to_text(const number_mpz_t *value);
bool number_mpz_get_long(const number_mpz_t *value, long *out);
size_t number_mpz_bit_length(const number_mpz_t *value);
int number_mpz_cmp(const number_mpz_t *a, const number_mpz_t *b);
bool number_mpz_is_zero(const number_mpz_t *value);
bool number_mpz_is_negative(const number_mpz_t *value);
number_mpq_t *number_mpq_from_const_id(number_const_id_t id);
number_mpq_t *number_mpq_new(void);
number_mpq_t *number_mpq_from_frac_long(long numerator, long denominator);
number_mpq_t *number_mpq_from_mpq(mpq_srcptr value);
number_mpq_t *number_mpq_from_text(const string_t *text);
number_mpq_t *number_mpq_clone(const number_mpq_t *value);
void number_mpq_free(number_mpq_t *value);
int number_mpq_ensure(const number_mpq_t *value);
mpq_srcptr number_mpq_value(const number_mpq_t *value);
string_t *number_mpq_to_text(const number_mpq_t *value);
bool number_mpq_get_small_fraction(const number_mpq_t *value, long *numerator, long *denominator);
int number_mpq_cmp(const number_mpq_t *a, const number_mpq_t *b);
bool number_mpq_is_zero(const number_mpq_t *value);
bool number_mpq_is_one(const number_mpq_t *value);
bool number_mpq_is_integer(const number_mpq_t *value);
bool number_mpq_is_negative(const number_mpq_t *value);
number_mpfr_t *number_mpfr_new_prec(size_t precision_bits);
number_mpfr_t *number_mpfr_from_const_id(number_const_id_t id, size_t precision_bits);
number_mpfr_t *number_mpfr_from_double(double value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_qfloat(qfloat_t value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_mpfr(mpfr_srcptr value, size_t precision_bits);
number_mpfr_t *number_mpfr_from_text(const string_t *text, size_t precision_bits);
number_mpfr_t *number_mpfr_clone(const number_mpfr_t *value);
void number_mpfr_free(number_mpfr_t *value);
int number_mpfr_ensure(const number_mpfr_t *value, size_t precision_bits);
mpfr_srcptr number_mpfr_value(const number_mpfr_t *value);
const complex_t *number_complex_value(const number_t *number);
int number_complex_get_mpc(mpc_t out, const complex_t *value, size_t precision_bits);
number_t *number_wrap_complex_mpc(mpc_srcptr value, size_t precision_bits);
void number_complex_clear_mpc_cache(complex_t *value);
int number_complex_set_mpc_cache_from_mpc(complex_t *value, mpc_srcptr source, size_t precision_bits);
bool number_eq_same_tol_with_precision(const number_t *a, const number_t *b, size_t precision_bits);
number_kind_t number_common_kind(const number_t *a, const number_t *b, number_binary_op_t op);
number_t *number_coerce(const number_t *number, number_kind_t target_kind);
bool number_matches_value(const number_t *reference, const number_t *target);
bool number_const_id_from_immortal(const number_t *number, number_const_id_t *id_out);
qfloat_t number_const_qfloat(number_const_id_t id);
qcomplex_t number_const_qcomplex(number_const_id_t id);
number_t number_const_mpfr_exact(number_const_id_t id);
bool number_const_has_double(number_const_id_t id);
double number_const_double_value(number_const_id_t id);
bool number_const_has_cdouble(number_const_id_t id);
double _Complex number_const_cdouble_value(number_const_id_t id);
bool number_const_has_ldexp(number_const_id_t id);
int number_const_ldexp_value(number_const_id_t id);
void number_constants_shutdown(void);
number_t number_create_exact_mpfr_long_prec(long value, size_t precision_bits);
number_t number_create_exact_mpfr_dyadic_prec(long numerator, int exponent2, size_t precision_bits);
number_t number_const_return_like(const number_t *like, number_const_id_t id);
number_t number_neg_const_return_like(const number_t *like, number_const_id_t id);
number_t number_imag_const_return_like(const number_t *like, number_const_id_t id);
number_t number_const_like(const number_t *like, number_const_id_t id);
number_t num_lommel_s_derivative_internal(const number_t mu, const number_t nu, const number_t argument);

#endif
