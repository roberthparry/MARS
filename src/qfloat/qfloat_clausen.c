#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "qfloat.h"
#define MARS_SHARED_QFLOAT_INTERNAL_ACCESS
#include "internal/qfloat_internal.h"

/* Binary Payne--Hanek reduction using floor(2^1152/(2*pi)), least significant limb first.
 * The fixed 36-limb product retains over 120 guard bits even at DBL_MAX. */
static qfloat_t clausen_reduce_double(double x)
{
    static const uint32_t reciprocal[] = {
        0x47e35742u, 0x9afed7ecu, 0xe294a4bau, 0xcf41ce7du, 0xfaf97c5eu, 0x5d49eeb1u,
        0xa797fa8bu, 0xd3d18fd9u, 0xc9f2c26du, 0xdb4d9fb3u, 0xd6829b47u, 0xfbcbc462u,
        0xf7816603u, 0xc7fe25ffu, 0xef7e4a0eu, 0x272117e2u, 0x60d4ce7du, 0x4e64758eu,
        0xad17df90u, 0x3a671c09u, 0x4baed121u, 0xba208d7du, 0x2c4a69cfu, 0x3f877ac7u,
        0x82746487u, 0x01924bbau, 0x909374b8u, 0x6dc91b8eu, 0xf7aef158u, 0x7f9458eau,
        0x4f10e410u, 0x36d8a566u, 0x7d4d3770u, 0x7f09d5f4u, 0x9391054au, 0x28be60dbu
    };
    uint32_t product[38] = {0};
    int exponent;
    uint64_t mantissa;
    int point;
    bool complement;
    qfloat_t fraction = QF_ZERO;

    if (fabs(x) <= 3.0)
        return qf_from_double(x);
    mantissa = (uint64_t)ldexp(frexp(fabs(x), &exponent), 53);
    for (unsigned int j = 0u; j < 2u; ++j) {
        uint64_t digit = (mantissa >> (32u * j)) & UINT32_MAX;
        uint64_t carry = 0u;

        for (unsigned int k = 0u; k < 36u; ++k) {
            uint64_t term = (uint64_t)reciprocal[k] * digit + product[k + j] + carry;

            product[k + j] = (uint32_t)term;
            carry = term >> 32u;
        }
        product[36u + j] = (uint32_t)carry;
    }
    point = 1152 + 53 - exponent;
    complement = ((product[(point - 1) / 32] >> ((point - 1) % 32)) & 1u) != 0u;
    for (int j = 1; j <= 160; ++j) {
        int bit = point - j;
        bool set = ((product[bit / 32] >> (bit % 32)) & 1u) != 0u;

        if (set != complement)
            fraction = qf_add(fraction, qf_from_double(ldexp(1.0, -j)));
    }
    if (complement != (signbit(x) != 0))
        fraction = qf_neg(fraction);
    return qf_mul(fraction, QF_2PI);
}

/* Reduce an angle for the real and complex Clausen implementations. */
qfloat_t qf_clausen_reduce(qfloat_t x)
{
    qfloat_t r;

    if (qf_le(qf_abs(x), QF_PI))
        return x;
    if (qf_eq(qf_abs(x), QF_2PI))
        return QF_ZERO;
    r = qf_add(clausen_reduce_double(x.hi), clausen_reduce_double(x.lo));
    if (qf_gt(r, QF_PI))
        r = qf_sub(r, QF_2PI);
    if (qf_lt(r, qf_neg(QF_PI)))
        r = qf_add(r, QF_2PI);
    return r;
}

/* Evaluate the positive-angle log-sine expansion without cancellation at pi. */
static qfloat_t qf_clausen2_near_pi(qfloat_t x)
{
    qfloat_t d = qf_sub(QF_PI, x);
    qfloat_t ratio = qf_sqr(qf_div(d, QF_PI));
    qfloat_t power = ratio;
    qfloat_t quarter_power = qf_from_double(0.25);
    qfloat_t sum = QF_LN2;

    for (unsigned int j = 1u; j <= 64u; ++j) {
        qfloat_t coefficient = qf_mul(qf_zeta(qf_from_double(2.0 * j)), qf_sub(QF_ONE, quarter_power));
        qfloat_t term = qf_div(qf_mul(coefficient, power), qf_from_double((double)j * (2.0 * j + 1.0)));

        sum = qf_sub(sum, term);
        if (qf_le(qf_abs(term), qf_from_double(1e-34)))
            break;
        power = qf_mul(power, ratio);
        quarter_power = qf_mul_double(quarter_power, 0.25);
    }
    return qf_mul(d, sum);
}

/* Evaluate integer-order Clausen functions using only native real arithmetic. */
qfloat_t qf_clausen(unsigned long order, qfloat_t theta)
{
    qfloat_t x, square, term, sum, harmonic, ratio;
    bool negative;
    unsigned long k;

    if (order == 0ul || qf_isnan(theta) || qf_isinf(theta))
        return QF_NAN;
    x = qf_clausen_reduce(theta);
    negative = (order & 1ul) == 0ul && qf_lt(x, QF_ZERO);
    x = qf_abs(x);
    if (qf_eq(x, QF_ZERO))
        return order == 1ul ? QF_INF : ((order & 1ul) ? qf_zeta(qf_from_double((double)order)) : QF_ZERO);
    if (qf_eq(x, QF_PI)) {
        if ((order & 1ul) == 0ul)
            return QF_ZERO;
        if (order == 1ul)
            return qf_neg(QF_LN2);
        if (order >= 128ul)
            return qf_neg(QF_ONE);
        return qf_neg(qf_mul(qf_sub(QF_ONE, qf_pow_int(QF_TWO, 1 - (int)order)),
                            qf_zeta(qf_from_double((double)order))));
    }
    if (order == 1ul) {
        if (qf_lt(x, qf_from_double(1e-100)))
            return qf_neg(qf_log(x));
        return qf_neg(qf_log(qf_mul_double(qf_sin(qf_mul_double(x, 0.5)), 2.0)));
    }
    if (order == 2ul && qf_gt(x, QF_PI_2)) {
        sum = qf_clausen2_near_pi(x);
        return negative ? qf_neg(sum) : sum;
    }
    /* The omitted Fourier tail is below 2^-127 here, independently of the angle. */
    if (order >= 128ul) {
        sum = (order & 1ul) ? qf_cos(x) : qf_sin(x);
        return negative ? qf_neg(sum) : sum;
    }

    /* Integer Jonquiere expansion, selecting the required parity before doing arithmetic.
     * See DLMF 25.12.12; the pole and logarithm combine into H_(n-1) - log(x).
     * Functional reflection of zeta gives a positive, geometrically convergent tail. */
    square = qf_sqr(x);
    k = (order - 1ul) & 1ul;
    term = k ? x : QF_ONE;
    sum = QF_ZERO;
    for (; k < order - 1ul; k += 2ul) {
        sum = qf_add(sum, qf_mul(term, qf_zeta(qf_from_double((double)(order - k)))));
        term = qf_neg(qf_div(qf_mul(term, square), qf_from_double((double)(k + 1ul) * (k + 2ul))));
    }
    harmonic = QF_ZERO;
    for (k = 1ul; k < order; ++k)
        harmonic = qf_add(harmonic, qf_div(QF_ONE, qf_from_double((double)k)));
    sum = qf_add(sum, qf_mul(term, qf_sub(harmonic, qf_log(x))));
    ratio = qf_sqr(qf_div(x, QF_2PI));
    term = qf_div(qf_mul_double(qf_mul(term, ratio), 2.0),
                  qf_from_double((double)order * (order + 1ul)));
    for (unsigned int j = 1u; j <= 64u; ++j) {
        qfloat_t add = qf_mul(term, qf_zeta(qf_from_double(2.0 * j)));

        sum = qf_add(sum, add);
        if (qf_le(qf_abs(add), qf_mul(qf_from_double(1e-34), qf_abs(sum))))
            break;
        term = qf_div(qf_mul_double(qf_mul(term, ratio), (2.0 * j) * (2.0 * j + 1.0)),
                      qf_from_double((order + 2.0 * j) * (order + 2.0 * j + 1.0)));
    }
    return negative ? qf_neg(sum) : sum;
}

/* Evaluate the order-two real Clausen function. */
qfloat_t qf_clausen2(qfloat_t theta)
{
    return qf_clausen(2ul, theta);
}
