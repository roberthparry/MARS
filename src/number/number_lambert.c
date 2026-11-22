/**
 * @file number_lambert.c
 * @brief Private polylogarithmic Lambert kernel for q-function derivatives and finite sections.
 *
 * Evaluates L(a,q,z;r,s;N) = sum_{n=0}^{N-1} (n+z)^r Li_s(a q^(n+z)). SIZE_MAX selects the
 * infinite series. Negative polylogarithm orders use Eulerian polynomials, not numerical differentiation.
 * Finite sections are evaluated before any infinite-tail cancellation, including on the unit circle.
 * Infinite sections require |q|<1; an absolute geometric/polynomial tail majorant controls
 * truncation independently of cancellation. Scratch numbers and the temporary precision are call-local.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

#include "number-t.h"
#include "computation-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { LAMBERT_MAX_ORDER = 32, LAMBERT_MAX_WEIGHT = 64, LAMBERT_MAX_TERMS = 100000 };

/* Typed fixed-precision arguments retain their backend policy instead of paying for the global MPFR precision. */
static bool lambert_native(number_t a, number_t q, number_t z, unsigned weight, long order,
                           size_t count, number_t *out)
{
    const number_t values[] = {a, q, z};
    bool fixed = false, extended = false, complex_input = false;
    qcomplex_t arguments[3];
    for (unsigned j = 0; j < 3; ++j) {
        number_kind_t kind = number_impl_const(&values[j])->kind;
        if (kind == NUMBER_MPFR || kind == NUMBER_COMPLEX) return false;
        fixed = fixed || kind == NUMBER_DOUBLE || kind == NUMBER_QFLOAT ||
                         kind == NUMBER_CDOUBLE || kind == NUMBER_QCOMPLEX;
        extended = extended || kind == NUMBER_QFLOAT;
        complex_input = complex_input || kind == NUMBER_CDOUBLE || kind == NUMBER_QCOMPLEX || !num_is_real(values[j]);
        arguments[j] = number_value_to_qcomplex(&values[j]);
        if (qc_isinf(arguments[j]) || qc_isnan(arguments[j])) return false;
    }
    if (!fixed) return false;
    qcomplex_t r = qc_make(qf_from_double(weight), QF_ZERO), s = qc_make(qf_from_double(order), QF_ZERO);
    qcomplex_t n = qc_make(count == SIZE_MAX ? QF_INF : qf_from_double((double)count), QF_ZERO);
    qcomplex_t result = qc_lambert_S(arguments[0], arguments[1], arguments[2], r, s, n);
    *out = complex_input || !qf_eq(qc_imag(result), QF_ZERO) ? num_create_from_qcomplex(result) :
           extended ? num_create_from_qfloat(qc_real(result)) : num_create_from_double(qf_to_double(qc_real(result)));
    return true;
}

/* Eulerian coefficients are exact integers, including the constant polynomial for Li_0. */
static void lambert_coefficients(number_t *coefficients, unsigned degree)
{
    for (unsigned j = 0; j <= degree; ++j) coefficients[j] = num_clone(j == 0 ? NUM_ONE : NUM_ZERO);
    for (unsigned p = 2; p <= degree; ++p) {
        for (unsigned j = p - 1; j > 0; --j) {
            number_t a = num_mul_long(coefficients[j], (long)j + 1);
            number_t b = num_mul_long(coefficients[j - 1], (long)p - j);
            number_t next = num_add(a, b);
            num_destroy(&a); num_destroy(&b); num_destroy(&coefficients[j]);
            coefficients[j] = next;
        }
    }
}

static number_t lambert_polylog(number_t w, long order, const number_t *coefficients)
{
    NUM_SCOPE(scope);
    if (order > 0) {
        number_t s = num_create_from_long(order);
        return num_scope_detach(num_polylog(s, w));
    }
    unsigned p = (unsigned)-order;
    number_t polynomial = num_clone(coefficients[p ? p - 1 : 0]);
    for (unsigned j = p > 0 ? p - 1 : 0; j > 0; --j) {
        number_t product = num_mul(polynomial, w);
        number_t next = num_add(product, coefficients[j - 1]);
        num_destroy(&product); num_destroy(&polynomial); polynomial = next;
    }
    number_t numerator = num_mul(w, polynomial);
    number_t one_minus = num_sub(NUM_ONE, w);
    number_t denominator = num_pow_int(one_minus, (int)p + 1);
    return num_scope_detach(num_div(numerator, denominator));
}

/* A positive majorant bounds every omitted term; no small computed term is taken as convergence. */
static bool lambert_tail_small(number_t w, double radius, double z_size, size_t n,
                               unsigned weight, long order, size_t bits)
{
    NUM_SCOPE(scope);
    number_t magnitude = num_abs(w);
    if (num_is_zero(magnitude)) return true;
    double rho = nextafter(num_to_double(magnitude), INFINITY);
    double distance = nextafter((double)n + z_size, INFINITY);
    if (!(rho < 1) || !(distance > 0)) return false;
    double ratio = nextafter(radius * pow(1 + 1 / distance, weight), INFINITY);
    if (!(ratio < 1)) return false;
    unsigned p = order < 0 ? (unsigned)-order : 0;
    double log_bound = (double)num_get_exponent2(magnitude) + 2 + weight * log2(distance) -
        (p + 1) * log2(1 - rho) + log2(ratio / (1 - ratio));
    for (unsigned j = 2; j <= p; ++j) log_bound += log2(j);
    return log_bound + 4 < -(double)bits - 16;
}

/* Evaluate a finite section or an absolutely convergent generalised Lambert series at the active precision. */
number_t number_lambert_series(number_t a, number_t q, number_t z, unsigned weight, long order, size_t count)
{
    NUM_SCOPE(scope);
    if (weight > LAMBERT_MAX_WEIGHT || order < -LAMBERT_MAX_ORDER || order > LAMBERT_MAX_ORDER ||
        (count != SIZE_MAX && count > LAMBERT_MAX_TERMS) ||
        !num_is_finite(a) || !num_is_finite(q) || !num_is_finite(z)) return num_clone(NUM_NAN);
    if (count == 0 || num_is_zero(a)) return num_clone(NUM_ZERO);
    if (num_is_zero(q)) return num_clone(NUM_NAN);
    number_t native;
    if (lambert_native(a, q, z, weight, order, count, &native)) return num_scope_detach(native);
    size_t saved = num_get_default_prec_bits(), bits = saved;
    const number_t inputs[] = {a, q, z};
    for (unsigned j = 0; j < 3; ++j) {
        size_t input_bits = num_get_prec_bits(inputs[j]);
        if (input_bits > bits) bits = input_bits;
    }
    if (bits > 65536 - 128) return num_clone(NUM_NAN);
    num_set_default_prec_bits(bits + 96);
    number_t logarithm = num_log(q), exponent = num_mul(z, logarithm), initial = num_exp(exponent);
    number_t w = num_mul(a, initial), q_size = num_abs(q), z_abs = num_abs(z);
    double radius = nextafter(num_to_double(q_size), INFINITY), z_size = nextafter(num_to_double(z_abs), INFINITY);
    number_t w_size = num_abs(w);
    bool infinite = count == SIZE_MAX;
    bool ok = num_is_finite(w) && (!infinite || (radius < 1 && isfinite(z_size)));
    num_destroy(&w_size);
    number_t coefficients[LAMBERT_MAX_ORDER + 1];
    unsigned degree = order < 0 ? (unsigned)-order : 0;
    lambert_coefficients(coefficients, degree);
    number_t sum = num_clone(NUM_ZERO);
    size_t limit = infinite ? LAMBERT_MAX_TERMS : count;
    bool finished = false;
    for (size_t n = 0; ok && n < limit; ++n) {
        NUM_SCOPE(term_scope);
        computation_t *control = computation_current();
        if (control && !computation_checkpoint(control, 1)) { ok = false; break; }
        number_t coordinate = num_add_long(z, (long)n), power = num_pow_int(coordinate, (int)weight);
        number_t li = lambert_polylog(w, order, coefficients), term = num_mul(power, li);
        number_t next = num_add(sum, term);
        ok = num_is_finite(next);
        num_destroy(&sum); sum = num_scope_detach(next);
        if ((radius < 1 && n > 0 && lambert_tail_small(w, radius, z_size, n, weight, order, bits)) ||
            (!infinite && n + 1 == count)) {
            finished = true;
            break;
        }
        number_t next_w = num_mul(w, q);
        num_destroy(&w); w = num_scope_detach(next_w);
    }
    for (unsigned j = 0; j <= degree; ++j) num_destroy(&coefficients[j]);
    num_set_default_prec_bits(saved);
    if (!ok || !finished) { num_destroy(&sum); return num_clone(NUM_NAN); }
    return num_scope_detach(sum);
}

/* Expose the documented MARS convention with scalar argument validation and ordinary number ownership. */
number_t num_lambert_S(number_t a, number_t q, number_t z, number_t weight, number_t order, number_t count)
{
    bool infinite = num_eq(count, NUM_INF);
    number_t minimum = num_create_from_long(-32), maximum_order = num_create_from_long(32);
    number_t maximum_weight = num_create_from_long(64), maximum_count = num_create_from_long(100000);
    bool valid = num_is_real(weight) && num_is_integer(weight) && num_ge(weight, NUM_ZERO) &&
                 num_le(weight, maximum_weight) && num_is_real(order) && num_is_integer(order) &&
                 num_ge(order, minimum) && num_le(order, maximum_order) &&
                 (infinite || (num_is_real(count) && num_is_integer(count) && num_ge(count, NUM_ZERO) &&
                               num_le(count, maximum_count)));
    num_destroy(&minimum); num_destroy(&maximum_order); num_destroy(&maximum_weight); num_destroy(&maximum_count);
    if (!valid)
        return num_clone(NUM_NAN);
    /* Every validated integer is small enough to be represented exactly by binary64, including inexact backends. */
    return number_lambert_series(a, q, z, (unsigned)num_to_double(weight), (long)num_to_double(order),
                                  infinite ? SIZE_MAX : (size_t)num_to_double(count));
}
