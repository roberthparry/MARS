/**
 * @file number_zeta.c
 * @brief Guarded real and complex zeta sums, tails and bounded reusable state.
 *
 * @par Bounded logarithm reuse for shifted Dirichlet sums.
 *
 * Hurwitz zeta and its exponent derivative share log(a+n) values independently of the varying exponent. A single
 * exact offset key and direct integer indices avoid searches. A mutex protects copied cache entries; logarithms
 * are calculated outside it, so other evaluations are not locked behind transcendental work. Up to eight MiB of
 * significands are retained for at most 4096 indices and 16384 working bits. Changing the offset releases the old
 * entries. Larger requests and allocation or locking failures use direct evaluation, with identical mathematics.
 *
 * @par Finite Dirichlet sums with bounded reuse of integer powers.
 *
 * The number module's Riemann zeta and derivative kernels share each prime's n^(-s) value within an evaluation.
 * Composite powers are products of previously computed entries, reducing expensive logarithms and exponentials.
 * Prime logarithms are copied from the bounded coefficient cache across evaluations, including precision changes.
 * A once-initialised smallest-prime-factor sieve provides direct indexing. Per-call MPC storage is bounded to eight
 * MiB and 4096 indices; larger requests or allocation failures use the ordinary finite sum without retention.
 *
 * @par Bounded binomial blocks for finite shifted Dirichlet sums and order derivatives.
 *
 * Groups thirty-two consecutive bases about one centre and shares binomial-weighted moments between centres.
 * Symmetric centres make every odd moment zero; positive even moments need only sixteen scalar recurrences.
 * Analytic coefficient derivatives avoid dividing by a possibly zero rising factorial. Directed majorants cover
 * both polynomial tails, and an absolute arithmetic allowance checks the completed finite sum before rounding.
 * Unsupported domains and exhausted bounds select the owning zeta kernel's ordinary finite summation.
 * One small, mutex-protected finite-sum entry preserves warmed calls without affecting cold or changing inputs.
 *
 * @par Shared Euler–Maclaurin tails for real and complex zeta functions and their derivatives.
 *
 * Extends a finite Dirichlet sum with the integral, endpoint and Bernoulli corrections. A scaled rising product
 * advances through a quadratic recurrence, avoiding separate factorial products and negative endpoint powers.
 * Analytic differentiation advances alongside the value without division by a possibly zero rising product.
 * Absolute Bernoulli bounds size each correction's arithmetic. A checked high-precision path also shortens
 * rising-product state, propagating absolute errors after scaling by 32^(-k); accumulators retain full precision.
 * The owning number module supplies guarded MPC accumulators and its existing correction and accuracy budgets.
 *
 * @par MPFR Euler–Maclaurin evaluation of real zeta values and order derivatives.
 *
 * Keeps ordinary positive real sums, the integral and the Bernoulli recurrence in MPFR.
 * Moderate finite sums can share the number module's guarded binomial-block kernel.
 * Both outputs can share one evaluation. The correction budget and guard bits match the complex kernel;
 * unsupported real endpoints stay with its principal-branch continuation. Coefficients and shifted powers
 * use the number module's bounded caches, while all accumulators and temporary storage belong to this call.
 */
#include <pthread.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { ZETA_LOG_SLOTS = 4096, ZETA_LOG_MAX_BITS = 16384, ZETA_LOG_BYTES = 8 * 1024 * 1024 };
typedef struct {
    mpc_t value;
    mpfr_prec_t precision;
} zeta_log_entry_t;

static zeta_log_entry_t zeta_logs[ZETA_LOG_SLOTS];
static zeta_log_entry_t *zeta_log_owned[ZETA_LOG_SLOTS];
static mpc_t zeta_log_offset;
static bool zeta_log_ready;
static size_t zeta_log_count, zeta_log_bytes;
static pthread_mutex_t zeta_log_lock = PTHREAD_MUTEX_INITIALIZER;

static bool zeta_log_same(mpc_srcptr offset)
{
    if (!zeta_log_ready || mpc_cmp(zeta_log_offset, offset)) return false;
    /* Signed zeros select different sides of logarithmic branch cuts. */
    return (!mpfr_zero_p(mpc_realref(offset)) ||
            mpfr_signbit(mpc_realref(offset)) == mpfr_signbit(mpc_realref(zeta_log_offset))) &&
           (!mpfr_zero_p(mpc_imagref(offset)) ||
            mpfr_signbit(mpc_imagref(offset)) == mpfr_signbit(mpc_imagref(zeta_log_offset)));
}

static void zeta_log_clear(void)
{
    /* Only initialised entries own allocations; releasing them is necessarily proportional to their count. */
    for (size_t i = 0; i < zeta_log_count; ++i) {
        mpc_clear(zeta_log_owned[i]->value);
        zeta_log_owned[i]->precision = 0;
    }
    zeta_log_count = zeta_log_bytes = 0;
}

/* Copy an exact-key logarithm or calculate it without exposing cache pointers. */
void number_zeta_logarithm(mpc_ptr out, mpc_srcptr offset, unsigned long index)
{
    mpfr_prec_t precision = mpc_get_prec(out);
    bool eligible = index < ZETA_LOG_SLOTS && precision <= ZETA_LOG_MAX_BITS &&
                    mpc_get_prec(offset) <= ZETA_LOG_MAX_BITS &&
                    mpfr_number_p(mpc_realref(offset)) && mpfr_number_p(mpc_imagref(offset));
    if (eligible && !pthread_mutex_lock(&zeta_log_lock)) {
        zeta_log_entry_t *entry = zeta_logs + index;
        bool found = zeta_log_same(offset) && entry->precision >= precision;
        if (found) mpc_set(out, entry->value, MPC_RNDNN);
        pthread_mutex_unlock(&zeta_log_lock);
        if (found) return;
    }
    mpc_add_ui(out, offset, index, MPC_RNDNN);
    mpc_log(out, out, MPC_RNDNN);
    if (!eligible || !mpfr_number_p(mpc_realref(out)) || !mpfr_number_p(mpc_imagref(out)) ||
        pthread_mutex_lock(&zeta_log_lock)) return;
    if (!zeta_log_same(offset)) {
        zeta_log_clear();
        if (!zeta_log_ready) mpc_init2(zeta_log_offset, mpc_get_prec(offset));
        else mpc_set_prec(zeta_log_offset, mpc_get_prec(offset));
        mpc_set(zeta_log_offset, offset, MPC_RNDNN);
        zeta_log_ready = true;
    }
    zeta_log_entry_t *entry = zeta_logs + index;
    if (entry->precision < precision) {
        size_t bytes = 2u * mpfr_custom_get_size(precision);
        size_t previous = entry->precision ? 2u * mpfr_custom_get_size(entry->precision) : 0;
        size_t budget = ZETA_LOG_BYTES - 2u * mpfr_custom_get_size(mpc_get_prec(zeta_log_offset));
        if (zeta_log_bytes <= budget && bytes - previous <= budget - zeta_log_bytes) {
            if (!entry->precision) {
                mpc_init2(entry->value, precision);
                zeta_log_owned[zeta_log_count++] = entry;
            } else mpc_set_prec(entry->value, precision);
            mpc_set(entry->value, out, MPC_RNDNN);
            entry->precision = precision;
            zeta_log_bytes += bytes - previous;
        }
    }
    pthread_mutex_unlock(&zeta_log_lock);
}

static void __attribute__((destructor)) zeta_log_shutdown(void)
{
    zeta_log_clear();
    if (zeta_log_ready) mpc_clear(zeta_log_offset);
}

enum { ZETA_FACTOR_SLOTS = 4096, ZETA_POWER_BYTES = 8 * 1024 * 1024 };
static unsigned int zeta_factors[ZETA_FACTOR_SLOTS];
static pthread_once_t zeta_factors_once = PTHREAD_ONCE_INIT;

static void zeta_factor_sieve(void)
{
    for (unsigned int prime = 2; prime < ZETA_FACTOR_SLOTS; ++prime) {
        if (zeta_factors[prime]) continue;
        zeta_factors[prime] = prime;
        if (prime > (ZETA_FACTOR_SLOTS - 1u) / prime) continue;
        for (unsigned int composite = prime * prime; composite < ZETA_FACTOR_SLOTS; composite += prime)
            if (!zeta_factors[composite]) zeta_factors[composite] = prime;
    }
}

/* Sum integer Dirichlet terms and, when requested, their analytic derivatives. */
void number_zeta_finite_sum(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, unsigned long endpoint)
{
    mpfr_prec_t precision = mpc_get_prec(sum);
    size_t bytes = sizeof(mpc_t) + 2u * mpfr_custom_get_size(precision);
    if (derivative) bytes += sizeof(mpfr_t) + mpfr_custom_get_size(precision);
    bool factorised = endpoint <= ZETA_FACTOR_SLOTS && endpoint <= ZETA_POWER_BYTES / bytes &&
                      pthread_once(&zeta_factors_once, zeta_factor_sieve) == 0;
    mpc_t *powers = factorised ? calloc(endpoint, sizeof(*powers)) : NULL;
    mpfr_t *logs = powers && derivative ? calloc(endpoint, sizeof(*logs)) : NULL;
    if (derivative && powers && !logs) { free(powers); powers = NULL; }
    mpc_t term, correction;
    mpfr_t logarithm;
    mpc_init2(term, precision);
    mpc_init2(correction, precision);
    mpfr_init2(logarithm, precision);
    mpc_set_ui(sum, 0u, MPC_RNDNN);
    if (derivative) mpc_set_ui(derivative, 0u, MPC_RNDNN);
    for (unsigned long n = 1; n < endpoint; ++n) {
        unsigned int prime = powers ? zeta_factors[n] : 0;
        if (powers) mpc_init2(powers[n], precision);
        if (logs) {
            mpfr_init2(logs[n], precision);
            if (n == 1) mpfr_set_zero(logs[n], 1);
            else if (prime != n) mpfr_add(logs[n], logs[prime], logs[n / prime], MPFR_RNDN);
            else number_integer_logarithm(logs[n], n);
        }
        mpc_ptr value = powers ? powers[n] : term;
        if (powers && n == 1) mpc_set_ui(value, 1u, MPC_RNDNN);
        else if (powers && prime != n) mpc_mul(value, powers[prime], powers[n / prime], MPC_RNDNN);
        else {
            if (logs) mpfr_set(logarithm, logs[n], MPFR_RNDN);
            else number_integer_logarithm(logarithm, n);
            mpc_mul_fr(value, argument, logarithm, MPC_RNDNN);
            mpc_neg(value, value, MPC_RNDNN);
            mpc_exp(value, value, MPC_RNDNN);
        }
        mpc_add(sum, sum, value, MPC_RNDNN);
        if (derivative) {
            if (logs) mpfr_set(logarithm, logs[n], MPFR_RNDN);
            else number_integer_logarithm(logarithm, n);
            mpc_mul_fr(correction, value, logarithm, MPC_RNDNN);
            mpc_sub(derivative, derivative, correction, MPC_RNDNN);
        }
    }
    /* Each per-call MPC value owns two allocations and must be cleared exactly once. */
    if (powers) {
        for (unsigned long n = 1; n < endpoint; ++n) mpc_clear(powers[n]);
        free(powers);
    }
    if (logs) {
        for (unsigned long n = 1; n < endpoint; ++n) mpfr_clear(logs[n]);
        free(logs);
    }
    mpfr_clear(logarithm);
    mpc_clear(correction);
    mpc_clear(term);
}
/* Sum integer Dirichlet terms and, when requested, their analytic derivatives. */
void number_zeta_finite_sum_real(mpfr_ptr sum, mpfr_ptr derivative, mpfr_srcptr argument, unsigned long endpoint)
{
    mpfr_prec_t precision = mpfr_get_prec(sum);
    size_t bytes = sizeof(mpfr_t) + mpfr_custom_get_size(precision);
    if (derivative) bytes += sizeof(mpfr_t) + mpfr_custom_get_size(precision);
    bool factorised = endpoint <= ZETA_FACTOR_SLOTS && endpoint <= ZETA_POWER_BYTES / bytes &&
                      pthread_once(&zeta_factors_once, zeta_factor_sieve) == 0;
    mpfr_t *powers = factorised ? calloc(endpoint, sizeof(*powers)) : NULL;
    mpfr_t *logs = powers && derivative ? calloc(endpoint, sizeof(*logs)) : NULL;
    if (derivative && powers && !logs) { free(powers); powers = NULL; }
    mpfr_t term, correction;
    mpfr_t logarithm;
    mpfr_init2(term, precision);
    mpfr_init2(correction, precision);
    mpfr_init2(logarithm, precision);
    mpfr_set_ui(sum, 0u, MPFR_RNDN);
    if (derivative) mpfr_set_ui(derivative, 0u, MPFR_RNDN);
    for (unsigned long n = 1; n < endpoint; ++n) {
        unsigned int prime = powers ? zeta_factors[n] : 0;
        if (powers) mpfr_init2(powers[n], precision);
        if (logs) {
            mpfr_init2(logs[n], precision);
            if (n == 1) mpfr_set_zero(logs[n], 1);
            else if (prime != n) mpfr_add(logs[n], logs[prime], logs[n / prime], MPFR_RNDN);
            else number_integer_logarithm(logs[n], n);
        }
        mpfr_ptr value = powers ? powers[n] : term;
        if (powers && n == 1) mpfr_set_ui(value, 1u, MPFR_RNDN);
        else if (powers && prime != n) mpfr_mul(value, powers[prime], powers[n / prime], MPFR_RNDN);
        else {
            if (logs) mpfr_set(logarithm, logs[n], MPFR_RNDN);
            else number_integer_logarithm(logarithm, n);
            mpfr_mul(value, argument, logarithm, MPFR_RNDN);
            mpfr_neg(value, value, MPFR_RNDN);
            mpfr_exp(value, value, MPFR_RNDN);
        }
        mpfr_add(sum, sum, value, MPFR_RNDN);
        if (derivative) {
            if (logs) mpfr_set(logarithm, logs[n], MPFR_RNDN);
            else number_integer_logarithm(logarithm, n);
            mpfr_mul(correction, value, logarithm, MPFR_RNDN);
            mpfr_sub(derivative, derivative, correction, MPFR_RNDN);
        }
    }
    /* Each per-call MPFR value owns one allocation and must be cleared exactly once. */
    if (powers) {
        for (unsigned long n = 1; n < endpoint; ++n) mpfr_clear(powers[n]);
        free(powers);
    }
    if (logs) {
        for (unsigned long n = 1; n < endpoint; ++n) mpfr_clear(logs[n]);
        free(logs);
    }
    mpfr_clear(logarithm);
    mpfr_clear(correction);
    mpfr_clear(term);
}

enum { ZETA_BLOCK = 32, ZETA_HALF = 16, ZETA_HALF_BITS = 4 };

static mpc_t shifted_argument, shifted_offset, shifted_value, shifted_slope;
static mpfr_prec_t shifted_precision;
static unsigned long shifted_count;
static bool shifted_derivative;
static pthread_mutex_t shifted_lock = PTHREAD_MUTEX_INITIALIZER;

static bool shifted_same(mpc_srcptr a, mpc_srcptr b)
{
    return !mpc_cmp(a, b) &&
        (!mpfr_zero_p(mpc_realref(a)) || mpfr_signbit(mpc_realref(a)) == mpfr_signbit(mpc_realref(b))) &&
        (!mpfr_zero_p(mpc_imagref(a)) || mpfr_signbit(mpc_imagref(a)) == mpfr_signbit(mpc_imagref(b)));
}

static bool shifted_copy(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    if (pthread_mutex_lock(&shifted_lock)) return false;
    bool found = shifted_precision >= mpc_get_prec(sum) && shifted_count == count &&
        (!derivative || shifted_derivative) && shifted_same(s, shifted_argument) && shifted_same(a, shifted_offset);
    if (found) {
        mpc_set(sum, shifted_value, MPC_RNDNN);
        if (derivative) mpc_set(derivative, shifted_slope, MPC_RNDNN);
    }
    pthread_mutex_unlock(&shifted_lock);
    return found;
}

static void shifted_save(mpc_srcptr sum, mpc_srcptr derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    if (mpc_get_prec(s) > 16384 || mpc_get_prec(a) > 16384 || pthread_mutex_lock(&shifted_lock)) return;
    if (!shifted_precision) {
        mpc_init2(shifted_argument, mpc_get_prec(s)); mpc_init2(shifted_offset, mpc_get_prec(a));
        mpc_init2(shifted_value, mpc_get_prec(sum)); mpc_init2(shifted_slope, mpc_get_prec(sum));
    } else {
        mpc_set_prec(shifted_argument, mpc_get_prec(s)); mpc_set_prec(shifted_offset, mpc_get_prec(a));
        mpc_set_prec(shifted_value, mpc_get_prec(sum)); mpc_set_prec(shifted_slope, mpc_get_prec(sum));
    }
    mpc_set(shifted_argument, s, MPC_RNDNN); mpc_set(shifted_offset, a, MPC_RNDNN);
    mpc_set(shifted_value, sum, MPC_RNDNN);
    if (derivative) mpc_set(shifted_slope, derivative, MPC_RNDNN);
    shifted_precision = mpc_get_prec(sum); shifted_count = count; shifted_derivative = derivative != NULL;
    pthread_mutex_unlock(&shifted_lock);
}

typedef struct _shifted_coefficients_t shifted_coefficients_t;
struct _shifted_coefficients_t {
    mpc_t *values, *derivatives;
    unsigned long count, capacity;
    mpc_t binomial, slope, factor, temporary;
    mpfr_t weights[ZETA_HALF], moment, scratch[4];
    mpfr_prec_t precision;
};

static bool shifted_coefficients_init(shifted_coefficients_t *c, mpfr_prec_t precision, bool derivative)
{
    c->count = 0; c->capacity = ((unsigned long)precision + 1024) / 2; c->precision = precision;
    c->values = calloc(c->capacity, sizeof *c->values);
    c->derivatives = derivative ? calloc(c->capacity, sizeof *c->derivatives) : NULL;
    if (!c->values || (derivative && !c->derivatives)) { free(c->values); free(c->derivatives); return false; }
    mpc_init2(c->binomial, precision); mpc_init2(c->slope, precision);
    mpc_init2(c->factor, precision); mpc_init2(c->temporary, precision);
    mpc_set_ui(c->binomial, 1u, MPC_RNDNN); mpc_set_ui(c->slope, 0u, MPC_RNDNN);
    for (unsigned k = 0; k < ZETA_HALF; ++k) {
        mpfr_init2(c->weights[k], precision); mpfr_set_ui(c->weights[k], 1u, MPFR_RNDN);
    }
    mpfr_init2(c->moment, precision);
    for (unsigned k = 0; k < 4; ++k) mpfr_init2(c->scratch[k], precision);
    return true;
}

static void shifted_coefficients_clear(shifted_coefficients_t *c)
{
    for (unsigned long j = 0; j < c->count; ++j) {
        mpc_clear(c->values[j]);
        if (c->derivatives) mpc_clear(c->derivatives[j]);
    }
    free(c->derivatives); free(c->values);
    for (unsigned k = 0; k < ZETA_HALF; ++k) mpfr_clear(c->weights[k]);
    for (unsigned k = 0; k < 4; ++k) mpfr_clear(c->scratch[k]);
    mpfr_clear(c->moment);
    mpc_clear(c->temporary); mpc_clear(c->factor); mpc_clear(c->slope); mpc_clear(c->binomial);
}

/* Every block starts beyond 32, so |16/centre| <= 16/47.75 < 3/8.
 * (3/8)^5 < 2^-7 gives an exact rational exponent bound without logarithms.
 * The 320-bit allowance covers polynomial growth and the coefficient recurrence's rounding counts. */
static mpfr_prec_t shifted_coefficient_precision(mpfr_prec_t work, unsigned long index)
{
    long adjustment = 320 - (long)(7ul * index / 5ul);
    return adjustment < 0 ? (adjustment < 128 - work ? 128 : work + adjustment) : work;
}

/* M_0=B, M_(2j+1)=0, M_(2j)=2*sum_(k=0)^(B/2-1)((2k+1)/B)^(2j).
 * q_(j+1)=q_j*(s+j)/(j+1), q'_(j+1)=(q'_j*(s+j)+q_j)/(j+1), including integral s. */
static bool shifted_coefficients(shifted_coefficients_t *c, mpc_srcptr s, unsigned long index)
{
    if (index >= c->capacity) return false;
    while (c->count <= index) {
        unsigned long slot = c->count;
        for (unsigned long j = slot ? 2 * slot - 1 : 1; j <= 2 * slot; ++j) {
            mpfr_prec_t precision = shifted_coefficient_precision(c->precision, j);
            mpfr_prec_round(mpc_realref(c->binomial), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(c->binomial), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_realref(c->slope), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(c->slope), precision, MPFR_RNDN);
            if (mpc_get_prec(c->factor) != precision) {
                mpc_set_prec(c->factor, precision); mpc_set_prec(c->temporary, precision);
                for (unsigned k = 0; k < 4; ++k) mpfr_set_prec(c->scratch[k], precision);
            }
            mpc_add_ui(c->factor, s, j - 1, MPC_RNDNN);
            if (c->derivatives) {
                number_mpc_mul_guarded(c->temporary, c->slope, c->factor, c->scratch);
                mpc_add(c->slope, c->temporary, c->binomial, MPC_RNDNN);
                mpc_div_ui(c->slope, c->slope, j, MPC_RNDNN);
            }
            number_mpc_mul_guarded(c->binomial, c->binomial, c->factor, c->scratch);
            mpc_div_ui(c->binomial, c->binomial, j, MPC_RNDNN);
        }
        if (!slot) mpfr_set_ui(c->moment, ZETA_BLOCK, MPFR_RNDN);
        else {
            mpfr_prec_t precision = mpc_get_prec(c->binomial);
            if (mpfr_get_prec(c->moment) != precision) mpfr_set_prec(c->moment, precision);
            mpfr_set_zero(c->moment, 1);
            for (unsigned k = 0; k < ZETA_HALF; ++k) {
                mpfr_prec_round(c->weights[k], precision, MPFR_RNDN);
                mpfr_mul_ui(c->weights[k], c->weights[k], (2 * k + 1) * (2 * k + 1), MPFR_RNDN);
                mpfr_div_2ui(c->weights[k], c->weights[k], 2 * (ZETA_HALF_BITS + 1), MPFR_RNDN);
                mpfr_add(c->moment, c->moment, c->weights[k], MPFR_RNDN);
            }
            mpfr_mul_2ui(c->moment, c->moment, 1u, MPFR_RNDN);
        }
        mpc_init2(c->values[slot], mpc_get_prec(c->binomial));
        mpc_mul_fr(c->values[slot], c->binomial, c->moment, MPC_RNDNN);
        if (c->derivatives) {
            mpc_init2(c->derivatives[slot], mpc_get_prec(c->binomial));
            mpc_mul_fr(c->derivatives[slot], c->slope, c->moment, MPC_RNDNN);
        }
        ++c->count;
    }
    return true;
}

/* q_j(S)+q'_j(S) <= q_j(S+1) for S>=0, by the positive-coefficient Taylor expansion of q_j.
 * This single majorant bounds both order polynomials without dividing by a rising factorial. */
static unsigned long shifted_degree(mpfr_ptr tail, mpfr_srcptr radius, mpfr_srcptr order, mpfr_prec_t work)
{
    mpfr_t coefficient, ratio, denominator, tolerance;
    mpfr_inits2(64, coefficient, ratio, denominator, tolerance, (mpfr_ptr)0);
    mpfr_set_ui(coefficient, ZETA_BLOCK, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -work + 96, MPFR_RNDD);
    unsigned long degree = 0;
    for (unsigned long j = 1; j < (unsigned long)work + 1024; ++j) {
        mpfr_add_ui(ratio, order, j, MPFR_RNDU);
        mpfr_div_ui(ratio, ratio, j, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_mul(coefficient, coefficient, ratio, MPFR_RNDU);
        if (j < 16 || j % 8) continue;
        mpfr_add_ui(ratio, order, 1u, MPFR_RNDU); mpfr_div_ui(ratio, ratio, j + 1, MPFR_RNDU);
        mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_ui_sub(denominator, 1u, ratio, MPFR_RNDD);
        if (mpfr_sgn(denominator) <= 0) continue;
        mpfr_div(tail, ratio, denominator, MPFR_RNDU); mpfr_mul(tail, tail, coefficient, MPFR_RNDU);
        if (mpfr_lessequal_p(tail, tolerance)) { degree = j; break; }
    }
    mpfr_clears(coefficient, ratio, denominator, tolerance, (mpfr_ptr)0);
    return degree;
}

static void shifted_polynomial(mpc_ptr value, mpc_ptr derivative, mpc_srcptr x,
                                const shifted_coefficients_t *c, unsigned long degree, mpfr_srcptr radius)
{
    mpfr_prec_t work = c->precision;
    mpfr_t logarithm, exponent, scratch[4];
    mpfr_inits2(64, logarithm, exponent, (mpfr_ptr)0);
    for (unsigned i = 0; i < 4; ++i) mpfr_init2(scratch[i], work);
    mpfr_log2(logarithm, radius, MPFR_RNDU);
    mpc_t argument, term;
    mpc_init2(argument, work); mpc_init2(term, work);
    mpc_set_ui(value, 0u, MPC_RNDNN);
    if (derivative) mpc_set_ui(derivative, 0u, MPC_RNDNN);
    for (unsigned long j = degree + 1; j-- > 0;) {
        /* 2*j<9728, |s|<=16: |q_(2j)|+|q'_(2j)| <= (2*j+17)^17 < 2^240. Moments add at most five bits.
         * The 320-bit allowance keeps shortened weighted absolute errors below 2^(-work-64). */
        mpfr_mul_ui(exponent, logarithm, j, MPFR_RNDU);
        long adjustment = mpfr_get_si(exponent, MPFR_RNDU) + 320;
        mpfr_prec_t precision = adjustment < 0 ? (adjustment < 128 - work ? 128 : work + adjustment) : work;
        mpfr_prec_round(mpc_realref(value), precision, MPFR_RNDN);
        mpfr_prec_round(mpc_imagref(value), precision, MPFR_RNDN);
        if (derivative) {
            mpfr_prec_round(mpc_realref(derivative), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(derivative), precision, MPFR_RNDN);
        }
        if (mpc_get_prec(term) != precision) { mpc_set_prec(term, precision); mpc_set_prec(argument, precision); }
        for (unsigned i = 0; i < 4; ++i)
            if (mpfr_get_prec(scratch[i]) != precision) mpfr_set_prec(scratch[i], precision);
        mpc_set(argument, x, MPC_RNDNN);
        number_mpc_mul_guarded(value, value, argument, scratch);
        mpc_set(term, c->values[j], MPC_RNDNN); mpc_add(value, value, term, MPC_RNDNN);
        if (derivative) {
            number_mpc_mul_guarded(derivative, derivative, argument, scratch);
            mpc_set(term, c->derivatives[j], MPC_RNDNN); mpc_add(derivative, derivative, term, MPC_RNDNN);
        }
    }
    mpc_clear(term); mpc_clear(argument);
    for (unsigned i = 0; i < 4; ++i) mpfr_clear(scratch[i]);
    mpfr_clears(logarithm, exponent, (mpfr_ptr)0);
}

/* Attempt the finite sum without changing the surrounding Euler–Maclaurin continuation. */
bool number_zeta_shifted_sum(mpc_ptr out, mpc_ptr out_derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    mpfr_prec_t precision = mpc_get_prec(out), work = precision + 384;
    if (precision < 1024 || precision > 8320 || count < 64 || count > 4096 ||
        mpfr_cmp_d(mpc_realref(a), 0.25) < 0) return false;
    mpfr_t radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary;
    mpfr_inits2(64, radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary,
                 (mpfr_ptr)0);
    mpc_abs(order, s, MPFR_RNDU); mpc_abs(size, a, MPFR_RNDU);
    bool ok = mpfr_number_p(order) && mpfr_number_p(size) && mpfr_cmp_ui(order, 16u) <= 0 &&
              mpfr_cmp_ui(size, 16u) <= 0;
    if (!ok) goto bounds_done;
    if (shifted_copy(out, out_derivative, s, a, count)) goto bounds_done;
    shifted_coefficients_t coefficients;
    if (!shifted_coefficients_init(&coefficients, work, out_derivative != NULL)) { ok = false; goto bounds_done; }
    mpc_t sum, derivative, centre, anchor, anchor_offset, logarithm, polynomial, polynomial_slope, x, term;
    mpc_init2(sum, work); mpc_init2(derivative, work); mpc_init2(centre, work); mpc_init2(anchor, work);
    mpc_init2(logarithm, work); mpc_init2(polynomial, work); mpc_init2(polynomial_slope, work);
    mpc_init2(x, work); mpc_init2(term, work);
    mpc_init2(anchor_offset, work);
    mpc_set(anchor_offset, a, MPC_RNDNN);
    mpfr_add_d(mpc_realref(anchor_offset), mpc_realref(anchor_offset), 0.5, MPFR_RNDN);
    mpc_set_ui(sum, 0u, MPC_RNDNN); mpc_set_ui(derivative, 0u, MPC_RNDNN);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(error, 1);
    unsigned long maximum_degree = 0;
    for (unsigned long k = 0; ok && k < count;) {
        if (k < ZETA_BLOCK || count - k < ZETA_BLOCK) {
            number_dirichlet_power(term, a, s, k);
            mpc_add(sum, sum, term, MPC_RNDNN); number_mpc_abs_upper(norm, term);
            mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
            if (out_derivative) {
                number_zeta_logarithm(logarithm, a, k);
                mpc_mul(term, term, logarithm, MPC_RNDNN); mpc_sub(derivative, derivative, term, MPC_RNDNN);
                number_mpc_abs_upper(norm, term); mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
            }
            ++k;
            continue;
        }
        mpc_add_ui(centre, a, k + ZETA_HALF, MPC_RNDNN);
        mpfr_sub_d(mpc_realref(centre), mpc_realref(centre), 0.5, MPFR_RNDN);
        mpc_abs(size, centre, MPFR_RNDD); mpfr_ui_div(radius, ZETA_HALF, size, MPFR_RNDU);
        if (mpfr_cmp_d(radius, 0.375) > 0) { ok = false; break; }
        unsigned long degree = shifted_degree(tail, radius, order, work);
        if (!degree || !shifted_coefficients(&coefficients, s, degree / 2)) { ok = false; break; }
        if (degree > maximum_degree) maximum_degree = degree;
        mpc_ui_div(x, ZETA_HALF, centre, MPC_RNDNN); mpc_neg(x, x, MPC_RNDNN);
        mpc_sqr(x, x, MPC_RNDNN);
        mpfr_sqr(temporary, radius, MPFR_RNDU);
        shifted_polynomial(polynomial, out_derivative ? polynomial_slope : NULL, x, &coefficients, degree / 2, temporary);
        number_dirichlet_power(anchor, anchor_offset, s, k + ZETA_HALF - 1);
        number_zeta_logarithm(logarithm, anchor_offset, k + ZETA_HALF - 1);
        number_mpc_abs_upper(norm, anchor); number_mpc_abs_upper(logarithm_size, logarithm);
        mpfr_add_ui(logarithm_size, logarithm_size, 1u, MPFR_RNDU);
        mpfr_mul(norm, norm, logarithm_size, MPFR_RNDU);
        mpfr_mul(tail, tail, norm, MPFR_RNDU); mpfr_add(error, error, tail, MPFR_RNDU);
        mpfr_ui_sub(temporary, 1u, radius, MPFR_RNDD);
        mpfr_add_ui(size, order, 1u, MPFR_RNDU); mpfr_neg(size, size, MPFR_RNDN);
        mpfr_pow(temporary, temporary, size, MPFR_RNDU);
        mpfr_mul(norm, norm, temporary, MPFR_RNDU); mpfr_mul_ui(norm, norm, ZETA_BLOCK, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
        mpc_mul(term, anchor, polynomial, MPC_RNDNN); mpc_add(sum, sum, term, MPC_RNDNN);
        if (out_derivative) {
            mpc_mul(term, logarithm, polynomial, MPC_RNDNN);
            mpc_sub(term, polynomial_slope, term, MPC_RNDNN);
            mpc_mul(term, anchor, term, MPC_RNDNN); mpc_add(derivative, derivative, term, MPC_RNDNN);
        }
        k += ZETA_BLOCK;
    }
    mpfr_mul_ui(bound, absolute_sum, maximum_degree + 1, MPFR_RNDU);
    mpfr_mul_ui(bound, bound, maximum_degree + 1, MPFR_RNDU);
    mpfr_mul_ui(bound, bound, count + 1, MPFR_RNDU); mpfr_mul_ui(bound, bound, count + 1, MPFR_RNDU);
    /* Quadratic counts cover coefficient/moment recurrences, Horner errors, input copies and accumulation.
     * All full-width state has 384 extra bits; the shortened products have a separate 320-bit growth allowance. */
    mpfr_mul_2si(bound, bound, -work + 96, MPFR_RNDU); mpfr_add(error, error, bound, MPFR_RNDU);
    mpfr_set_ui_2exp(bound, 1u, -precision - 32, MPFR_RNDD);
    ok = ok && mpfr_number_p(error) && mpfr_lessequal_p(error, bound) &&
        mpfr_number_p(mpc_realref(sum)) && mpfr_number_p(mpc_imagref(sum)) &&
        (!out_derivative || (mpfr_number_p(mpc_realref(derivative)) && mpfr_number_p(mpc_imagref(derivative))));
    if (ok) {
        mpc_set(out, sum, MPC_RNDNN);
        if (out_derivative) mpc_set(out_derivative, derivative, MPC_RNDNN);
        shifted_save(out, out_derivative, s, a, count);
    }
    mpc_clear(term); mpc_clear(x); mpc_clear(polynomial_slope); mpc_clear(polynomial);
    mpc_clear(logarithm); mpc_clear(anchor_offset); mpc_clear(anchor); mpc_clear(centre); mpc_clear(derivative); mpc_clear(sum);
    shifted_coefficients_clear(&coefficients);
bounds_done:
    mpfr_clears(radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary, (mpfr_ptr)0);
    return ok;
}

static void __attribute__((destructor)) shifted_shutdown(void)
{
    if (!shifted_precision) return;
    mpc_clear(shifted_argument); mpc_clear(shifted_offset); mpc_clear(shifted_value); mpc_clear(shifted_slope);
}

static bool zeta_tail_small(mpc_srcptr term, mpfr_prec_t target, unsigned guard)
{
    mpfr_exp_t bound = -(mpfr_exp_t)(target + guard) - 1;
    return mpfr_number_p(mpc_realref(term)) && mpfr_number_p(mpc_imagref(term)) &&
           (mpfr_zero_p(mpc_realref(term)) || mpfr_get_exp(mpc_realref(term)) <= bound) &&
           (mpfr_zero_p(mpc_imagref(term)) || mpfr_get_exp(mpc_imagref(term)) <= bound);
}

static mpfr_exp_t zeta_tail_exponent(mpc_srcptr value)
{
    bool real_zero = mpfr_zero_p(mpc_realref(value)), imaginary_zero = mpfr_zero_p(mpc_imagref(value));
    if (real_zero && imaginary_zero) return 0;
    mpfr_exp_t real = real_zero ? mpfr_get_exp(mpc_imagref(value)) : mpfr_get_exp(mpc_realref(value));
    mpfr_exp_t imaginary = imaginary_zero ? real : mpfr_get_exp(mpc_imagref(value));
    return (real > imaginary ? real : imaginary) + 1;
}

/* |B_(2k)/(2k)!| < 4/32^k, since zeta(2k)<2 and (2*pi)^2>32. Together with an upper exponent of
 * each multiplier, this keeps coefficient and product roundoff below 2^(-work-28) per correction. Both
 * derivative and value accumulators stay at full precision; no rising-product state is rounded down. */
static mpfr_prec_t zeta_tail_precision(mpc_srcptr scaled, mpc_srcptr derivative, size_t index,
                                     mpfr_prec_t work)
{
    if (work < 640 || work > 65536 || index > 65536 ||
        !mpfr_number_p(mpc_realref(scaled)) || !mpfr_number_p(mpc_imagref(scaled))) return work;
    mpfr_exp_t exponent = zeta_tail_exponent(scaled);
    if (derivative) {
        if (!mpfr_number_p(mpc_realref(derivative)) || !mpfr_number_p(mpc_imagref(derivative))) return work;
        mpfr_exp_t other = zeta_tail_exponent(derivative);
        if (other > exponent) exponent = other;
    }
    mpfr_exp_t adjustment = exponent - 5 * (mpfr_exp_t)index + 32;
    if (adjustment >= 0) return work;
    return adjustment < 128 - work ? 128 : work + adjustment;
}

/* Add one guarded tail, using each endpoint exponential only once. */
static int zeta_tail_evaluate(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, mpc_srcptr endpoint,
                              mpfr_prec_t target, size_t budget, unsigned guard, bool shortened)
{
    mpfr_prec_t work = mpc_get_prec(sum);
    mpc_t logarithm, base, inverse, inverse_square, pole, term, scaled, scaled_derivative;
    mpc_t factor, increment, second_difference, factor_derivative, derivative_increment, temporary;
    mpc_t rounded_factor, rounded_derivative;
    mpfr_t bernoulli, scratch[4], state_error, total_error, unit_error, norm, multiplier, bound;
    if (shortened) {
        for (unsigned i = 0; i < 4; ++i) mpfr_init2(scratch[i], work);
        mpfr_inits2(64, state_error, total_error, unit_error, norm, multiplier, bound, (mpfr_ptr)0);
        /* With at most 8192 steps, full-state factor differences accumulate O(k^2) errors. The checked
         * state, multiplier and accumulator magnitudes are at most 1, 512 and 65536 respectively.
         * At least 128 working guard bits leave more than 80 bits for an absolute per-step allowance
         * in the 32^(-k) scale. Shortened state/coefficient errors have a further 32-bit allowance. */
        mpfr_set_ui_2exp(unit_error, 1u, -target - 80, MPFR_RNDU);
        mpfr_set(state_error, unit_error, MPFR_RNDU); mpfr_set(total_error, unit_error, MPFR_RNDU);
        mpc_init2(rounded_factor, work); mpc_init2(rounded_derivative, work);
    }
    mpc_init2(logarithm, work); mpc_init2(base, work); mpc_init2(inverse, work);
    mpc_init2(inverse_square, work); mpc_init2(pole, work); mpc_init2(term, work);
    mpc_init2(scaled, work); mpc_init2(scaled_derivative, work); mpc_init2(factor, work);
    mpc_init2(increment, work); mpc_init2(second_difference, work); mpc_init2(factor_derivative, work);
    mpc_init2(derivative_increment, work); mpc_init2(temporary, work);
    mpfr_init2(bernoulli, work);
    int status = -1;

    mpc_sub_ui(pole, argument, 1u, MPC_RNDNN);
    if (mpc_cmp_si_si(pole, 0, 0) == 0) {
        mpfr_set_inf(mpc_realref(sum), 1);
        mpfr_set_zero(mpc_imagref(sum), 1);
        if (derivative) {
            mpfr_set_inf(mpc_realref(derivative), -1);
            mpfr_set_zero(mpc_imagref(derivative), 1);
        }
        status = 0;
        goto done;
    }
    mpc_log(logarithm, endpoint, MPC_RNDNN);
    mpc_mul(base, argument, logarithm, MPC_RNDNN);
    mpc_neg(base, base, MPC_RNDNN);
    mpc_exp(base, base, MPC_RNDNN);
    mpc_ui_div(inverse, 1u, endpoint, MPC_RNDNN);
    mpc_sqr(inverse_square, inverse, MPC_RNDNN);

    mpc_mul(term, base, endpoint, MPC_RNDNN);
    mpc_div(term, term, pole, MPC_RNDNN);
    mpc_add(sum, sum, term, MPC_RNDNN);
    if (derivative) {
        mpc_ui_div(temporary, 1u, pole, MPC_RNDNN);
        mpc_add(temporary, temporary, logarithm, MPC_RNDNN);
        mpc_mul(temporary, temporary, term, MPC_RNDNN);
        mpc_sub(derivative, derivative, temporary, MPC_RNDNN);
    }
    mpc_div_2ui(term, base, 1u, MPC_RNDNN);
    mpc_add(sum, sum, term, MPC_RNDNN);
    if (derivative) {
        mpc_mul(temporary, term, logarithm, MPC_RNDNN);
        mpc_sub(derivative, derivative, temporary, MPC_RNDNN);
    }

    /* U_k = (s)_(2k-1) z^(-s-2k+1), Q_k = (s+2k-1)(s+2k)/z^2. */
    mpc_mul(base, base, inverse, MPC_RNDNN);
    mpc_mul(scaled, base, argument, MPC_RNDNN);
    if (derivative) {
        mpc_mul(temporary, argument, logarithm, MPC_RNDNN);
        mpc_ui_sub(temporary, 1u, temporary, MPC_RNDNN);
        mpc_mul(scaled_derivative, base, temporary, MPC_RNDNN);
        mpc_mul_ui(factor_derivative, argument, 2u, MPC_RNDNN);
        mpc_add_ui(factor_derivative, factor_derivative, 3u, MPC_RNDNN);
        mpc_mul(factor_derivative, factor_derivative, inverse_square, MPC_RNDNN);
        mpc_mul_ui(derivative_increment, inverse_square, 4u, MPC_RNDNN);
    }
    mpc_add_ui(factor, argument, 1u, MPC_RNDNN);
    mpc_add_ui(temporary, argument, 2u, MPC_RNDNN);
    mpc_mul(factor, factor, temporary, MPC_RNDNN);
    mpc_mul(factor, factor, inverse_square, MPC_RNDNN);
    mpc_mul_ui(increment, argument, 4u, MPC_RNDNN);
    mpc_add_ui(increment, increment, 10u, MPC_RNDNN);
    mpc_mul(increment, increment, inverse_square, MPC_RNDNN);
    mpc_mul_ui(second_difference, inverse_square, 8u, MPC_RNDNN);

    for (size_t k = 1u; k <= budget; ++k) {
        mpfr_prec_t precision = zeta_tail_precision(scaled, derivative ? scaled_derivative : NULL, k, work);
        if (mpfr_get_prec(bernoulli) != precision) mpfr_set_prec(bernoulli, precision);
        if (mpc_get_prec(term) != precision) mpc_set_prec(term, precision);
        if (shortened) {
            number_mpc_abs_upper(norm, sum);
            if (!mpfr_number_p(norm) || mpfr_cmp_ui(norm, 65536u) > 0) break;
            if (derivative) {
                number_mpc_abs_upper(norm, derivative);
                if (!mpfr_number_p(norm) || mpfr_cmp_ui(norm, 65536u) > 0) break;
            }
            number_mpc_abs_upper(norm, scaled);
            if (derivative) {
                number_mpc_abs_upper(bound, scaled_derivative);
                if (mpfr_greater_p(bound, norm)) mpfr_set(norm, bound, MPFR_RNDU);
            }
            mpfr_div_2ui(norm, norm, 5u * k, MPFR_RNDU);
            if (!mpfr_number_p(norm) || mpfr_cmp_ui(norm, 1u) > 0) break;
            /* After scaling U_k by 32^(-k), |B_(2k)/(2k)!|*32^k < 4. State roundoff is
             * absolute in this scale, so it remains meaningful at zeros and under cancellation. */
            mpfr_mul_ui(bound, state_error, 4u, MPFR_RNDU);
            mpfr_add(total_error, total_error, bound, MPFR_RNDU);
            mpfr_add(total_error, total_error, unit_error, MPFR_RNDU);
            mpfr_prec_round(mpc_realref(scaled), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(scaled), precision, MPFR_RNDN);
            if (derivative) {
                mpfr_prec_round(mpc_realref(scaled_derivative), precision, MPFR_RNDN);
                mpfr_prec_round(mpc_imagref(scaled_derivative), precision, MPFR_RNDN);
            }
            if (mpc_get_prec(temporary) != precision) mpc_set_prec(temporary, precision);
            if (mpc_get_prec(rounded_factor) != precision) {
                mpc_set_prec(rounded_factor, precision); mpc_set_prec(rounded_derivative, precision);
            }
            mpc_set(rounded_factor, factor, MPC_RNDNN);
            if (derivative) mpc_set(rounded_derivative, factor_derivative, MPC_RNDNN);
            for (unsigned i = 0; i < 4; ++i)
                if (mpfr_get_prec(scratch[i]) != precision) mpfr_set_prec(scratch[i], precision);
        }
        if (number_coefficient_normalised(bernoulli, k)) break;
        mpc_mul_fr(term, scaled, bernoulli, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        bool small = zeta_tail_small(term, target, guard);
        if (derivative) {
            mpc_mul_fr(term, scaled_derivative, bernoulli, MPC_RNDNN);
            mpc_add(derivative, derivative, term, MPC_RNDNN);
            small = small && zeta_tail_small(term, target, guard);
        }
        if (k >= 4u && small) {
            if (!shortened) status = 0;
            else {
                mpfr_set_ui_2exp(bound, 1u, -target - 32, MPFR_RNDD);
                if (mpfr_number_p(total_error) && mpfr_lessequal_p(total_error, bound)) status = 0;
            }
            break;
        }
        if (shortened) {
            number_mpc_abs_upper(multiplier, factor);
            if (derivative) {
                number_mpc_abs_upper(norm, factor_derivative);
                mpfr_add(multiplier, multiplier, norm, MPFR_RNDU);
            }
            mpfr_div_2ui(multiplier, multiplier, 5u, MPFR_RNDU);
            if (!mpfr_number_p(multiplier) || mpfr_cmp_ui(multiplier, 16u) > 0) break;
            mpfr_mul(state_error, state_error, multiplier, MPFR_RNDU);
            mpfr_add(state_error, state_error, unit_error, MPFR_RNDU);
        }
        if (derivative) {
            if (shortened) {
                number_mpc_mul_guarded(temporary, scaled, rounded_derivative, scratch);
                number_mpc_mul_guarded(scaled_derivative, scaled_derivative, rounded_factor, scratch);
            } else {
                mpc_mul(temporary, scaled, factor_derivative, MPC_RNDNN);
                mpc_mul(scaled_derivative, scaled_derivative, factor, MPC_RNDNN);
            }
            mpc_add(scaled_derivative, scaled_derivative, temporary, MPC_RNDNN);
            mpc_add(factor_derivative, factor_derivative, derivative_increment, MPC_RNDNN);
        }
        if (shortened) number_mpc_mul_guarded(scaled, scaled, rounded_factor, scratch);
        else mpc_mul(scaled, scaled, factor, MPC_RNDNN);
        mpc_add(factor, factor, increment, MPC_RNDNN);
        mpc_add(increment, increment, second_difference, MPC_RNDNN);
    }

done:
    if (shortened && !status) {
        number_mpc_abs_upper(norm, sum);
        bool bounded = mpfr_number_p(norm) && mpfr_cmp_ui(norm, 65536u) <= 0;
        if (derivative) {
            number_mpc_abs_upper(norm, derivative);
            bounded = bounded && mpfr_number_p(norm) && mpfr_cmp_ui(norm, 65536u) <= 0;
        }
        if (!bounded) status = -1;
    }
    if (shortened) {
        mpfr_clears(state_error, total_error, unit_error, norm, multiplier, bound, (mpfr_ptr)0);
        for (unsigned i = 0; i < 4; ++i) mpfr_clear(scratch[i]);
        mpc_clear(rounded_derivative); mpc_clear(rounded_factor);
    }
    mpfr_clear(bernoulli);
    mpc_clear(temporary); mpc_clear(derivative_increment); mpc_clear(factor_derivative);
    mpc_clear(second_difference); mpc_clear(increment); mpc_clear(factor);
    mpc_clear(scaled_derivative); mpc_clear(scaled); mpc_clear(term); mpc_clear(pole);
    mpc_clear(inverse_square); mpc_clear(inverse); mpc_clear(base); mpc_clear(logarithm);
    return status;
}

/* Commit shortened state only after its propagated arithmetic error fits the guarded absolute budget. */
bool number_zeta_tail_short(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, mpc_srcptr endpoint,
                            mpfr_prec_t target, size_t budget, unsigned guard)
{
    mpfr_prec_t work = mpc_get_prec(sum);
    if (target < 1024 || target > 8192 || work < target + 128 || work > target + 512 ||
        budget > 8192 || mpfr_cmp_ui(mpc_realref(endpoint), 64u) < 0) return false;
    mpfr_t norm, bound;
    mpfr_inits2(64, norm, bound, (mpfr_ptr)0);
    number_mpc_abs_upper(norm, argument);
    bool ok = mpfr_number_p(norm) && mpfr_cmp_ui(norm, 16u) <= 0;
    number_mpc_abs_upper(norm, sum);
    ok = ok && mpfr_number_p(norm) && mpfr_cmp_ui(norm, 65536u) <= 0;
    if (derivative) {
        number_mpc_abs_upper(norm, derivative);
        ok = ok && mpfr_number_p(norm) && mpfr_cmp_ui(norm, 65536u) <= 0;
    }
    number_mpc_abs_upper(norm, endpoint);
    ok = ok && mpfr_number_p(norm) && mpfr_cmp_ui(norm, 65536u) <= 0;
    mpfr_abs(norm, mpc_imagref(endpoint), MPFR_RNDU);
    mpfr_div_2ui(bound, mpc_realref(endpoint), 2u, MPFR_RNDD);
    ok = ok && mpfr_lessequal_p(norm, bound);
    mpfr_clears(norm, bound, (mpfr_ptr)0);
    if (!ok) return false;
    mpc_t value, slope;
    mpc_init2(value, work); mpc_init2(slope, work);
    mpc_set(value, sum, MPC_RNDNN);
    if (derivative) mpc_set(slope, derivative, MPC_RNDNN);
    ok = !zeta_tail_evaluate(value, derivative ? slope : NULL, argument, endpoint, target, budget, guard, true);
    if (ok) {
        mpc_set(sum, value, MPC_RNDNN);
        if (derivative) mpc_set(derivative, slope, MPC_RNDNN);
    }
    mpc_clear(slope); mpc_clear(value);
    return ok;
}

/* Add the checked tail when supported, retaining the full-state recurrence as the general fallback. */
int number_zeta_tail(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, mpc_srcptr endpoint,
                     mpfr_prec_t target, size_t budget, unsigned guard)
{
    if (derivative && number_zeta_tail_short(sum, derivative, argument, endpoint, target, budget, guard)) return 0;
    return zeta_tail_evaluate(sum, derivative, argument, endpoint, target, budget, guard, false);
}

static bool zeta_real_tail_small(mpfr_srcptr term, mpfr_prec_t target, unsigned guard)
{
    return mpfr_number_p(term) && (mpfr_zero_p(term) || mpfr_get_exp(term) <= -target - guard - 1);
}

/* |B_(2k)/(2k)!| < 4/32^k; only coefficient products lose bits, never the recurrence or accumulator. */
static mpfr_prec_t zeta_real_tail_precision(mpfr_srcptr scaled, mpfr_srcptr derivative,
                                           size_t index, mpfr_prec_t work)
{
    if (work < 640 || work > 65536 || index > 65536 || !mpfr_number_p(scaled)) return work;
    mpfr_exp_t exponent = mpfr_zero_p(scaled) ? 0 : mpfr_get_exp(scaled);
    if (derivative) {
        if (!mpfr_number_p(derivative)) return work;
        mpfr_exp_t other = mpfr_zero_p(derivative) ? 0 : mpfr_get_exp(derivative);
        if (other > exponent) exponent = other;
    }
    mpfr_exp_t adjustment = exponent - 5 * (mpfr_exp_t)index + 32;
    if (adjustment >= 0) return work;
    return adjustment < 128 - work ? 128 : work + adjustment;
}

/* Add one guarded tail, using each endpoint exponential only once. */
static int zeta_real_tail(mpfr_ptr sum, mpfr_ptr derivative, mpfr_srcptr argument, mpfr_srcptr endpoint,
                     mpfr_prec_t target, size_t budget, unsigned guard)
{
    mpfr_prec_t work = mpfr_get_prec(sum);
    mpfr_t logarithm, base, inverse, inverse_square, pole, term, scaled, scaled_derivative;
    mpfr_t factor, increment, second_difference, factor_derivative, derivative_increment, temporary;
    mpfr_t bernoulli;
    mpfr_init2(logarithm, work); mpfr_init2(base, work); mpfr_init2(inverse, work);
    mpfr_init2(inverse_square, work); mpfr_init2(pole, work); mpfr_init2(term, work);
    mpfr_init2(scaled, work); mpfr_init2(scaled_derivative, work); mpfr_init2(factor, work);
    mpfr_init2(increment, work); mpfr_init2(second_difference, work); mpfr_init2(factor_derivative, work);
    mpfr_init2(derivative_increment, work); mpfr_init2(temporary, work);
    mpfr_init2(bernoulli, work);
    int status = -1;

    mpfr_sub_ui(pole, argument, 1u, MPFR_RNDN);
    if (mpfr_zero_p(pole)) {
        mpfr_set_inf(sum, 1);
        if (derivative) {
            mpfr_set_inf(derivative, -1);
        }
        status = 0;
        goto done;
    }
    mpfr_log(logarithm, endpoint, MPFR_RNDN);
    mpfr_mul(base, argument, logarithm, MPFR_RNDN);
    mpfr_neg(base, base, MPFR_RNDN);
    mpfr_exp(base, base, MPFR_RNDN);
    mpfr_ui_div(inverse, 1u, endpoint, MPFR_RNDN);
    mpfr_sqr(inverse_square, inverse, MPFR_RNDN);

    mpfr_mul(term, base, endpoint, MPFR_RNDN);
    mpfr_div(term, term, pole, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    if (derivative) {
        mpfr_ui_div(temporary, 1u, pole, MPFR_RNDN);
        mpfr_add(temporary, temporary, logarithm, MPFR_RNDN);
        mpfr_mul(temporary, temporary, term, MPFR_RNDN);
        mpfr_sub(derivative, derivative, temporary, MPFR_RNDN);
    }
    mpfr_div_2ui(term, base, 1u, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    if (derivative) {
        mpfr_mul(temporary, term, logarithm, MPFR_RNDN);
        mpfr_sub(derivative, derivative, temporary, MPFR_RNDN);
    }

    /* U_k = (s)_(2k-1) z^(-s-2k+1), Q_k = (s+2k-1)(s+2k)/z^2. */
    mpfr_mul(base, base, inverse, MPFR_RNDN);
    mpfr_mul(scaled, base, argument, MPFR_RNDN);
    if (derivative) {
        mpfr_mul(temporary, argument, logarithm, MPFR_RNDN);
        mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDN);
        mpfr_mul(scaled_derivative, base, temporary, MPFR_RNDN);
        mpfr_mul_ui(factor_derivative, argument, 2u, MPFR_RNDN);
        mpfr_add_ui(factor_derivative, factor_derivative, 3u, MPFR_RNDN);
        mpfr_mul(factor_derivative, factor_derivative, inverse_square, MPFR_RNDN);
        mpfr_mul_ui(derivative_increment, inverse_square, 4u, MPFR_RNDN);
    }
    mpfr_add_ui(factor, argument, 1u, MPFR_RNDN);
    mpfr_add_ui(temporary, argument, 2u, MPFR_RNDN);
    mpfr_mul(factor, factor, temporary, MPFR_RNDN);
    mpfr_mul(factor, factor, inverse_square, MPFR_RNDN);
    mpfr_mul_ui(increment, argument, 4u, MPFR_RNDN);
    mpfr_add_ui(increment, increment, 10u, MPFR_RNDN);
    mpfr_mul(increment, increment, inverse_square, MPFR_RNDN);
    mpfr_mul_ui(second_difference, inverse_square, 8u, MPFR_RNDN);

    for (size_t k = 1u; k <= budget; ++k) {
        mpfr_prec_t precision = zeta_real_tail_precision(scaled, derivative ? scaled_derivative : NULL, k, work);
        if (mpfr_get_prec(bernoulli) != precision) mpfr_set_prec(bernoulli, precision);
        if (mpfr_get_prec(term) != precision) mpfr_set_prec(term, precision);
        if (number_coefficient_normalised(bernoulli, k)) break;
        mpfr_mul(term, scaled, bernoulli, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        bool small = zeta_real_tail_small(term, target, guard);
        if (derivative) {
            mpfr_mul(term, scaled_derivative, bernoulli, MPFR_RNDN);
            mpfr_add(derivative, derivative, term, MPFR_RNDN);
            small = small && zeta_real_tail_small(term, target, guard);
        }
        if (k >= 4u && small) {
            status = 0;
            break;
        }
        if (derivative) {
            mpfr_mul(temporary, scaled, factor_derivative, MPFR_RNDN);
            mpfr_mul(scaled_derivative, scaled_derivative, factor, MPFR_RNDN);
            mpfr_add(scaled_derivative, scaled_derivative, temporary, MPFR_RNDN);
            mpfr_add(factor_derivative, factor_derivative, derivative_increment, MPFR_RNDN);
        }
        mpfr_mul(scaled, scaled, factor, MPFR_RNDN);
        mpfr_add(factor, factor, increment, MPFR_RNDN);
        mpfr_add(increment, increment, second_difference, MPFR_RNDN);
    }

done:
    mpfr_clear(bernoulli);
    mpfr_clear(temporary); mpfr_clear(derivative_increment); mpfr_clear(factor_derivative);
    mpfr_clear(second_difference); mpfr_clear(increment); mpfr_clear(factor);
    mpfr_clear(scaled_derivative); mpfr_clear(scaled); mpfr_clear(term); mpfr_clear(pole);
    mpfr_clear(inverse_square); mpfr_clear(inverse); mpfr_clear(base); mpfr_clear(logarithm);
    return status;
}

/* Evaluate a real finite sum and its optional derivative before adding the shared guarded tail. */
int number_zeta_real(mpfr_ptr out, mpfr_ptr derivative, mpfr_srcptr argument, mpfr_srcptr offset,
                     size_t budget, unsigned guard)
{
    if (!mpfr_number_p(argument) || (offset && (!mpfr_number_p(offset) || mpfr_sgn(offset) <= 0))) return -1;
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 128;
    unsigned long count = 128ul + (target > 512 ? (unsigned long)(target - 512) / 8ul : 0ul);
    mpfr_t sum, slope, endpoint, term;
    mpfr_inits2(work, sum, slope, endpoint, term, (mpfr_ptr)0);
    if (!offset) {
        number_zeta_finite_sum_real(sum, derivative ? slope : NULL, argument, count);
        mpfr_set_ui(endpoint, count, MPFR_RNDN);
    } else {
        mpc_t key_offset, key_exponent, value, log_value;
        mpc_init2(key_offset, work); mpc_init2(key_exponent, work);
        mpc_init2(value, work); mpc_init2(log_value, work);
        mpc_set_fr(key_offset, offset, MPC_RNDNN); mpc_set_fr(key_exponent, argument, MPC_RNDNN);
        mpfr_set_zero(sum, 1); mpfr_set_zero(slope, 1);
        bool blocked = (!mpfr_integer_p(argument) || derivative) &&
            number_zeta_shifted_sum(value, derivative ? log_value : NULL, key_exponent, key_offset, count);
        if (blocked) {
            mpfr_set(sum, mpc_realref(value), MPFR_RNDN);
            if (derivative) mpfr_set(slope, mpc_realref(log_value), MPFR_RNDN);
        }
        for (unsigned long n = 0; !blocked && n < count; ++n) {
            number_dirichlet_power(value, key_offset, key_exponent, n);
            mpfr_add(sum, sum, mpc_realref(value), MPFR_RNDN);
            if (derivative) {
                number_zeta_logarithm(log_value, key_offset, n);
                mpfr_mul(term, mpc_realref(value), mpc_realref(log_value), MPFR_RNDN);
                mpfr_sub(slope, slope, term, MPFR_RNDN);
            }
        }
        mpfr_add_ui(endpoint, offset, count, MPFR_RNDN);
        mpc_clear(log_value); mpc_clear(value); mpc_clear(key_exponent); mpc_clear(key_offset);
    }
    int status = zeta_real_tail(sum, derivative ? slope : NULL, argument, endpoint, target, budget, guard);
    if (!status) {
        mpfr_set(out, sum, MPFR_RNDN);
        if (derivative) mpfr_set(derivative, slope, MPFR_RNDN);
    }
    mpfr_clears(sum, slope, endpoint, term, (mpfr_ptr)0);
    return status;
}
