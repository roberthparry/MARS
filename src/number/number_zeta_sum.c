/**
 * @file number_zeta_sum.c
 * @brief Finite Dirichlet sums with bounded reuse of integer powers.
 *
 * The number module's Riemann zeta and derivative kernels share each prime's n^(-s) value within an evaluation.
 * Composite powers are products of previously computed entries, reducing expensive logarithms and exponentials.
 * Prime logarithms are copied from the bounded coefficient cache across evaluations, including precision changes.
 * A once-initialised smallest-prime-factor sieve provides direct indexing. Per-call MPC storage is bounded to eight
 * MiB and 4096 indices; larger requests or allocation failures use the ordinary finite sum without retention.
 */
#include <pthread.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

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
