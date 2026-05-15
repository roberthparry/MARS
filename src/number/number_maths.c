#include "number.h"
#include "number_internal.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

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

typedef struct {
    const number_t *angle;
    number_const_id_t value_id;
    int sign;
    int imag;
} number_angle_fastpath_t;

typedef struct {
    const number_t *angle;
    number_const_id_t first_id;
    int first_sign;
    int first_imag;
    number_const_id_t second_id;
    int second_sign;
    int second_imag;
} number_angle_pair_fastpath_t;

static const number_angle_pair_fastpath_t number_sincos_fastpaths[];
static const number_angle_pair_fastpath_t number_sinhcosh_fastpaths[];

static int number_try_get_pure_imag(const number_t number,
                                    number_t *imag_out);

static int number_try_get_exact_int(const number_t number, int *out)
{
    char *text;
    char *end;
    long parsed;
    uint64_t mantissa;
    long exponent2;
    int sign;
    int value;

    if (!out || !num_is_integer(number) || !num_is_real(number) ||
        !num_is_finite(number))
        return 0;

    if (num_get_mantissa_u64(number, &mantissa)) {
        exponent2 = num_get_exponent2(number);
        if (exponent2 < 0 || exponent2 >= (long)(sizeof(int) * 8u - 1u) ||
            mantissa > ((uint64_t)INT_MAX >> exponent2))
            return 0;

        value = (int)(mantissa << exponent2);
        sign = num_get_sign(number);
        if (sign < 0)
            value = -value;
        *out = value;
        return 1;
    }

    text = num_to_string(number);
    if (!text)
        return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        parsed < (long)INT_MIN || parsed > (long)INT_MAX)
    {
        free(text);
        return 0;
    }
    free(text);
    *out = (int)parsed;
    return 1;
}

static number_t number_return_like_signed(const number_t *like,
                                          number_const_id_t id,
                                          int sign)
{
    return sign < 0
        ? number_neg_const_return_like(like, id)
        : number_const_return_like(like, id);
}

static number_t number_return_like_imag_signed(const number_t *like,
                                               number_const_id_t id,
                                               int sign)
{
    return sign < 0
        ? number_neg_const_return_like(like, id)
        : number_imag_const_return_like(like, id);
}

static int number_find_angle_fastpath(const number_t *value,
                                      const number_angle_fastpath_t *table,
                                      size_t count,
                                      const number_angle_fastpath_t **match_out)
{
    for (size_t i = 0; i < count; ++i) {
        if (number_matches_value(value, table[i].angle)) {
            *match_out = &table[i];
            return 1;
        }
    }
    return 0;
}

static const number_angle_fastpath_t number_exp_quarter_turn_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_I, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_NEG_ONE, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_I, -1, 0 }
};

static int number_exp_quarter_turn(const number_t *number,
                                   number_t *out)
{
    number_t real;
    number_t imag;
    number_t ratio;
    int quarter_turns;
    int mod4;
    double ratio_d;

    if (num_is_real(*number))
        return 0;

    real = num_real_part(*number);
    if (!num_is_zero(real)) {
        num_destroy(&real);
        return 0;
    }

    imag = num_imag_part(*number);
    ratio = num_div(imag, NUM_PI_2);
    num_destroy(&real);
    num_destroy(&imag);

    if (!number_try_get_exact_int(ratio, &quarter_turns)) {
        ratio_d = num_to_double(ratio);
        if (!isfinite(ratio_d) || ratio_d < (double)INT_MIN ||
            ratio_d > (double)INT_MAX) {
            num_destroy(&ratio);
            return 0;
        }
        double nearest = round(ratio_d);
        if (fabs(ratio_d - nearest) > 1e-12) {
            num_destroy(&ratio);
            return 0;
        }
        quarter_turns = (int)lround(ratio_d);
    }
    num_destroy(&ratio);

    mod4 = quarter_turns % 4;
    if (mod4 < 0)
        mod4 += 4;

    *out = number_return_like_signed(number,
        number_exp_quarter_turn_fastpaths[mod4].value_id,
        number_exp_quarter_turn_fastpaths[mod4].sign);
    return 1;
}

static int number_trig_real_fastpath(const number_t *number,
                                     const number_angle_fastpath_t *table,
                                     size_t count,
                                     number_t *out)
{
    const number_angle_fastpath_t *match;

    if (!number_find_angle_fastpath(number, table, count, &match))
        return 0;
    *out = num_scope_detach(match->imag
        ? number_return_like_imag_signed(number, match->value_id, match->sign)
        : number_return_like_signed(number, match->value_id, match->sign));
    return 1;
}

static int number_hyperbolic_imag_fastpath(const number_t *number,
                                           const number_angle_fastpath_t *table,
                                           size_t count,
                                           number_t *out)
{
    number_t imag;
    const number_angle_fastpath_t *match;

    if (!number_try_get_pure_imag(*number, &imag))
        return 0;
    if (!number_find_angle_fastpath(&imag, table, count, &match)) {
        num_destroy(&imag);
        return 0;
    }
    num_destroy(&imag);
    *out = match->imag
        ? number_return_like_imag_signed(number, match->value_id, match->sign)
        : number_return_like_signed(number, match->value_id, match->sign);
    return 1;
}

static int number_trig_real_pair_fastpath(const number_t *number,
                                          const number_angle_pair_fastpath_t *table,
                                          size_t count,
                                          number_t *first_out,
                                          number_t *second_out)
{
    for (size_t i = 0; i < count; ++i) {
        if (!number_matches_value(number, table[i].angle))
            continue;
        *first_out = num_scope_detach(table[i].first_imag
            ? number_return_like_imag_signed(number, table[i].first_id, table[i].first_sign)
            : number_return_like_signed(number, table[i].first_id, table[i].first_sign));
        *second_out = num_scope_detach(table[i].second_imag
            ? number_return_like_imag_signed(number, table[i].second_id, table[i].second_sign)
            : number_return_like_signed(number, table[i].second_id, table[i].second_sign));
        return 1;
    }
    return 0;
}

static int number_hyperbolic_imag_pair_fastpath(const number_t *number,
                                                const number_angle_pair_fastpath_t *table,
                                                size_t count,
                                                number_t *first_out,
                                                number_t *second_out)
{
    number_t imag;

    if (!number_try_get_pure_imag(*number, &imag))
        return 0;
    for (size_t i = 0; i < count; ++i) {
        if (!number_matches_value(&imag, table[i].angle))
            continue;
        num_destroy(&imag);
        *first_out = table[i].first_imag
            ? number_return_like_imag_signed(number, table[i].first_id, table[i].first_sign)
            : number_return_like_signed(number, table[i].first_id, table[i].first_sign);
        *second_out = table[i].second_imag
            ? number_return_like_imag_signed(number, table[i].second_id, table[i].second_sign)
            : number_return_like_signed(number, table[i].second_id, table[i].second_sign);
        return 1;
    }
    num_destroy(&imag);
    return 0;
}

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
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(number)))
        : number_invalid();
}

static number_t number_apply_unary_qcomplex(const number_t *number,
                                            const number_unary_math_ops_t *ops)
{
    return ops && ops->qcomplex && number
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(number)))
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
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(a),
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
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(a),
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
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(x),
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
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(x),
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
           number_kind_value(&number) == NUMBER_DOUBLE && d_fn
        ? num_create_from_double(d_fn(number_impl_const(&number)->value.d))
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
           number_kind_value(&a) == NUMBER_DOUBLE &&
           number_kind_value(&b) == NUMBER_DOUBLE && d_fn
        ? num_create_from_double(d_fn(number_impl_const(&a)->value.d,
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

number_t num_exp(const number_t number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;
    number_t root;
    number_t root2;
    number_t out;

    if (num_eq(number, NUM_ZERO))
        return number_const_return_like(&number, NUMBER_CONST_ONE);
    if (num_eq(number, NUM_ONE))
        return number_const_return_like(&number, NUMBER_CONST_E);
    if (num_eq(number, NUM_NEG_ONE))
        return number_const_return_like(&number, NUMBER_CONST_INV_E);
    if (num_eq(number, NUM_HALF)) {
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        number_t out = num_sqrt(e);

        num_destroy(&e);
        return out;
    }
    if (num_eq(number, NUM_QUARTER)) {
        number_t out;
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        root = num_sqrt(e);
        num_destroy(&e);
        out = num_sqrt(root);
        num_destroy(&root);
        return out;
    }
    if (num_eq(number, NUM_ONE_EIGHTH)) {
        number_t out;
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        root = num_sqrt(e);
        num_destroy(&e);
        root2 = num_sqrt(root);
        out = num_sqrt(root2);
        num_destroy(&root);
        num_destroy(&root2);
        return out;
    }
    if (number_exp_quarter_turn(&number, &out))
        return out;

    vt = number_vt(&number);
    if (!vt)
        return number_invalid();
    if (vt->exp_same)
        return number_take(vt->exp_same(&number));

    promoted = number_coerce(&number, NUMBER_MFLOAT);
    vt = number_vt(promoted);
    if (!vt || !vt->exp_same)
        goto done;
    result = vt->exp_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_log(const number_t number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;
    number_t neg_i;
    number_t out;
    number_t tmp;

    if (num_eq(number, NUM_ONE))
        return number_const_return_like(&number, NUMBER_CONST_ZERO);
    if (num_eq(number, NUM_E))
        return number_const_return_like(&number, NUMBER_CONST_ONE);
    if (num_eq(number, NUM_INV_E))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
    if (num_eq(number, NUM_TWO))
        return number_const_return_like(&number, NUMBER_CONST_LN2);
    if (num_eq(number, NUM_HALF))
        return number_neg_const_return_like(&number, NUMBER_CONST_LN2);
    if (num_eq(number, NUM_I))
        return number_imag_const_return_like(&number, NUMBER_CONST_PI_2);
    if (num_eq(number, NUM_NEG_ONE))
        return number_imag_const_return_like(&number, NUMBER_CONST_PI);
    neg_i = number_neg_const_return_like(&number, NUMBER_CONST_I);
    if (num_eq(number, neg_i)) {
        num_destroy(&neg_i);
        out = number_imag_const_return_like(&number, NUMBER_CONST_PI_2);
        tmp = num_neg(out);
        num_destroy(&out);
        return tmp;
    }
    num_destroy(&neg_i);

    vt = number_vt(&number);
    if (!vt)
        return number_invalid();
    if (vt->log_same)
        return number_take(vt->log_same(&number));

    promoted = number_coerce(&number, NUMBER_MFLOAT);
    vt = number_vt(promoted);
    if (!vt || !vt->log_same)
        goto done;
    result = vt->log_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_sqrt(const number_t number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;

    vt = number_vt(&number);
    if (!vt)
        return number_invalid();
    if (vt->sqrt_same)
        return number_take(vt->sqrt_same(&number));

    promoted = number_coerce(&number, NUMBER_MFLOAT);
    vt = number_vt(promoted);
    if (!vt || !vt->sqrt_same)
        goto done;
    result = vt->sqrt_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_pow(const number_t base, const number_t exponent)
{
    number_t half;
    number_t quarter;
    number_t eighth;
    number_t root;
    int exp_int;

    if (!number_is_valid_value(&base) || !number_is_valid_value(&exponent))
        return number_invalid();

    if (num_eq(exponent, NUM_ZERO))
        return number_const_return_like(&base, NUMBER_CONST_ONE);
    if (num_eq(exponent, NUM_ONE))
        return num_clone(base);
    if (num_eq(exponent, NUM_NEG_ONE))
        return num_inv(base);
    if (num_eq(exponent, NUM_TWO))
        return num_sqr(base);

    half = NUM_HALF;
    if (num_eq(exponent, half))
        return num_sqrt(base);
    quarter = NUM_QUARTER;
    if (num_eq(exponent, quarter)) {
        number_t result;
        root = num_sqrt(base);
        result = num_sqrt(root);
        num_destroy(&root);
        return result;
    }
    eighth = NUM_ONE_EIGHTH;
    if (num_eq(exponent, eighth)) {
        number_t root2;
        number_t result;
        root = num_sqrt(base);
        root2 = num_sqrt(root);
        result = num_sqrt(root2);
        num_destroy(&root);
        num_destroy(&root2);
        return result;
    }

    if (number_try_get_exact_int(exponent, &exp_int))
        return num_pow_int(base, exp_int);

    return number_apply_binary_math(base, exponent, qf_pow, qc_pow, mf_pow, mc_pow);
}

number_t num_pow_int(const number_t base, int exponent)
{
    const number_vtable_t *vt = number_vt(&base);
    number_t expnum;
    number_t result;

    if (!number_is_valid_value(&base))
        return number_invalid();
    if (vt && vt->pow_int)
        return number_take(vt->pow_int(&base, exponent));

    expnum = num_create_from_long(exponent);
    result = number_apply_binary_math(base, expnum, qf_pow, qc_pow, mf_pow, mc_pow);
    num_destroy(&expnum);
    return result;
}

number_t num_ldexp(const number_t number, int exponent2)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t two;
    number_t scale;
    number_t result;

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->ldexp_value)
        return number_take(vt->ldexp_value(&number, exponent2));

    two = num_create_from_long(2);
    scale = num_pow_int(two, exponent2);
    result = num_mul(number, scale);
    num_destroy(&two);
    num_destroy(&scale);
    return result;
}

number_t num_sqr(const number_t number)
{
    if (!number_is_valid_value(&number))
        return number_invalid();
    if (!num_is_real(number))
        return num_mul(number, number);
    return number_apply_unary_math(number, qf_sqr, NULL, mf_sqr, NULL);
}

number_t num_floor(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (vt && vt->floor_value)
        return number_take(vt->floor_value(&number));
    return number_apply_unary_math(number, qf_floor, qc_floor, mf_floor, mc_floor);
}

number_t num_pow10(int exponent10)
{
    return number_wrap_mfloat_with_precision(mf_pow10(exponent10),
        number_default_precision_bits);
}

number_t num_mul_pow10(const number_t number, int exponent10)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t scale;
    number_t result;

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->mul_pow10_value)
        return number_take(vt->mul_pow10_value(&number, exponent10));
    scale = num_pow10(exponent10);
    result = num_mul(number, scale);
    num_destroy(&scale);
    return result;
}

number_t num_hypot(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_hypot, qc_hypot, mf_hypot, mc_hypot);
}

static int number_try_get_pure_imag(const number_t number,
                                    number_t *imag_out)
{
    NUM_SCOPE(scope);
    number_t real;

    if (num_is_real(number))
        return 0;
    real = num_real_part(number);
    if (!num_is_zero(real)) {
        return 0;
    }
    *imag_out = num_scope_detach(num_imag_part(number));
    return 1;
}

static const number_angle_fastpath_t number_sin_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 }
};

static const number_angle_fastpath_t number_cos_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI, NUMBER_CONST_NEG_ONE, 1, 0 }
};

static const number_angle_pair_fastpath_t number_sincos_fastpaths[] = {
    { &NUM_ZERO,   NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI,     NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_NEG_ONE,        1, 0 },
    { &NUM_2PI,    NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI_6,   NUMBER_CONST_HALF,           1, 0, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4,   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0,
                   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3,   NUMBER_CONST_SQRT3_OVER_TWO, 1, 0, NUMBER_CONST_HALF,           1, 0 },
    { &NUM_PI_2,   NUMBER_CONST_ONE,            1, 0, NUMBER_CONST_ZERO,           1, 0 },
    { &NUM_3PI_4,  NUMBER_CONST_SQRT2_OVER_TWO, 1, 0,
                   NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 }
};

static const number_angle_fastpath_t number_tanh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_I, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3, 1, 1 },
    { &NUM_3PI_4, NUMBER_CONST_I, -1, 0 }
};

static const number_angle_fastpath_t number_sinh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_HALF, 1, 1 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 1 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3_OVER_TWO, 1, 1 },
    { &NUM_PI_2, NUMBER_CONST_I, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 1 }
};

static const number_angle_fastpath_t number_cosh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI, NUMBER_CONST_NEG_ONE, 1, 0 }
};

static const number_angle_pair_fastpath_t number_sinhcosh_fastpaths[] = {
    { &NUM_ZERO,   NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI_6,   NUMBER_CONST_HALF,           1, 1, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4,   NUMBER_CONST_SQRT2_OVER_TWO, 1, 1,
                   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3,   NUMBER_CONST_SQRT3_OVER_TWO, 1, 1, NUMBER_CONST_HALF,           1, 0 },
    { &NUM_PI_2,   NUMBER_CONST_ONE,            1, 1, NUMBER_CONST_ZERO,           1, 0 },
    { &NUM_3PI_4,  NUMBER_CONST_SQRT2_OVER_TWO, 1, 1,
                   NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI,     NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_NEG_ONE,        1, 0 },
    { &NUM_2PI,    NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 }
};

int num_sincos(const number_t x, number_t *sin_out, number_t *cos_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sin_out || !cos_out || !number_is_valid_value(&x))
        return -1;
    if (number_trig_real_pair_fastpath(&x, number_sincos_fastpaths,
            sizeof(number_sincos_fastpaths) / sizeof(number_sincos_fastpaths[0]),
            sin_out, cos_out))
        return 0;
    if (vt && vt->sincos_value)
        return vt->sincos_value(&x, sin_out, cos_out);
    return -1;
}

number_t num_sin(const number_t number)
{
    number_t out;

    if (number_trig_real_fastpath(&number, number_sin_fastpaths,
            sizeof(number_sin_fastpaths) / sizeof(number_sin_fastpaths[0]), &out))
        return out;
    return number_apply_unary_math_with_double(number, sin, qf_sin, qc_sin, mf_sin, mc_sin);
}

number_t num_cos(const number_t number)
{
    number_t out;

    if (number_trig_real_fastpath(&number, number_cos_fastpaths,
            sizeof(number_cos_fastpaths) / sizeof(number_cos_fastpaths[0]), &out))
        return out;
    return number_apply_unary_math_with_double(number, cos, qf_cos, qc_cos, mf_cos, mc_cos);
}

number_t num_tan(const number_t number)
{
    if (num_eq(number, NUM_ZERO) || num_eq(number, NUM_PI) || num_eq(number, NUM_2PI))
        return number_const_return_like(&number, NUMBER_CONST_ZERO);
    if (num_eq(number, NUM_PI_6)) {
        number_t one = number_const_like(&number, NUMBER_CONST_ONE);
        number_t sqrt3 = number_const_like(&number, NUMBER_CONST_SQRT3);
        number_t out = num_div(one, sqrt3);

        num_destroy(&one);
        num_destroy(&sqrt3);
        return out;
    }
    if (num_eq(number, NUM_PI_4))
        return number_const_return_like(&number, NUMBER_CONST_ONE);
    if (num_eq(number, NUM_PI_3))
        return number_const_return_like(&number, NUMBER_CONST_SQRT3);
    if (num_eq(number, NUM_3PI_4))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
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
    number_t out;

    if (number_hyperbolic_imag_fastpath(&number, number_sinh_imag_fastpaths,
            sizeof(number_sinh_imag_fastpaths) / sizeof(number_sinh_imag_fastpaths[0]), &out))
        return out;
    return number_apply_unary_math_with_double(number, sinh, qf_sinh, qc_sinh, mf_sinh, mc_sinh);
}

number_t num_cosh(const number_t number)
{
    number_t out;

    if (number_hyperbolic_imag_fastpath(&number, number_cosh_imag_fastpaths,
            sizeof(number_cosh_imag_fastpaths) / sizeof(number_cosh_imag_fastpaths[0]), &out))
        return out;
    return number_apply_unary_math_with_double(number, cosh, qf_cosh, qc_cosh, mf_cosh, mc_cosh);
}

int num_sinhcosh(const number_t x, number_t *sinh_out, number_t *cosh_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sinh_out || !cosh_out || !number_is_valid_value(&x))
        return -1;
    if (number_hyperbolic_imag_pair_fastpath(&x, number_sinhcosh_fastpaths,
            sizeof(number_sinhcosh_fastpaths) / sizeof(number_sinhcosh_fastpaths[0]),
            sinh_out, cosh_out))
        return 0;
    if (vt && vt->sinhcosh_value)
        return vt->sinhcosh_value(&x, sinh_out, cosh_out);
    return -1;
}

number_t num_tanh(const number_t number)
{
    number_t imag;
    number_t out;

    if (number_hyperbolic_imag_fastpath(&number, number_tanh_imag_fastpaths,
            sizeof(number_tanh_imag_fastpaths) / sizeof(number_tanh_imag_fastpaths[0]), &out))
        return out;
    if (number_try_get_pure_imag(number, &imag)) {
        if (number_matches_value(&imag, &NUM_PI_6)) {
            number_t one = number_const_like(&number, NUMBER_CONST_ONE);
            number_t sqrt3 = number_const_like(&number, NUMBER_CONST_SQRT3);
            number_t inv = num_div(one, sqrt3);
            number_t imag_unit = number_const_like(&number, NUMBER_CONST_I);

            out = num_mul(imag_unit, inv);
            num_destroy(&one);
            num_destroy(&sqrt3);
            num_destroy(&inv);
            num_destroy(&imag_unit);
            num_destroy(&imag);
            return out;
        }
        num_destroy(&imag);
    }
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
    return number_apply_unary_math(number, qf_lgamma, qc_lgamma, mf_lgamma, mc_lgamma);
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
    return number_apply_unary_math(number, qf_lambert_w0, qc_productlog, mf_lambert_w0, mc_lambert_w0);
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
