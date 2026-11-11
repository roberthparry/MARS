/**
 * @file test_number_caches.c
 * @brief Stateful checks of private numeric logarithm reuse and directed complex bounds.
 *
 * These number-module white-box fixtures compare cached logarithms with independent MPC evaluations across keys,
 * signed branch-cut zeros, precision changes and uncached indices. Integer logarithms also cover precision reuse,
 * direct calculation beyond the cache limits and invalid input. Concurrent callers exercise copied ownership
 * without running independent test cases or suites in parallel. Ordinary mathematical input cases remain in SQL.
 */
#include <mpc.h>
#include <pthread.h>

#include "number-t.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"
#include "test_harness.h"
#include "test_number.h"

static bool cache_log_matches(mpc_srcptr offset, unsigned long index, mpfr_prec_t precision)
{
    mpc_t value, reference, difference;
    mpc_init2(value, precision); mpc_init2(reference, precision + 64); mpc_init2(difference, precision + 64);
    mpfr_t error, bound;
    mpfr_inits2(precision + 64, error, bound, (mpfr_ptr)0);
    number_zeta_logarithm(value, offset, index);
    mpc_add_ui(reference, offset, index, MPC_RNDNN);
    mpc_log(reference, reference, MPC_RNDNN);
    mpc_sub(difference, value, reference, MPC_RNDNN);
    mpc_abs(error, difference, MPFR_RNDU);
    mpc_abs(bound, reference, MPFR_RNDD);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -(long)precision + 8, MPFR_RNDD);
    bool ok = mpfr_number_p(error) && mpfr_lessequal_p(error, bound);
    mpfr_clears(error, bound, (mpfr_ptr)0);
    mpc_clear(value); mpc_clear(reference); mpc_clear(difference);
    return ok;
}

typedef struct { unsigned index; bool ok; } cache_job_t;

static bool cache_integer_log_matches(unsigned long input, mpfr_prec_t precision)
{
    mpfr_t value, reference, error, bound;
    mpfr_inits2(precision, value, (mpfr_ptr)0);
    mpfr_inits2(precision + 64, reference, error, bound, (mpfr_ptr)0);
    bool ok = !number_integer_logarithm(value, input);
    mpfr_log_ui(reference, input, MPFR_RNDN);
    mpfr_sub(error, value, reference, MPFR_RNDU);
    mpfr_abs(error, error, MPFR_RNDU);
    mpfr_abs(bound, reference, MPFR_RNDD);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -(long)precision + 4, MPFR_RNDD);
    ok = ok && mpfr_number_p(error) && mpfr_lessequal_p(error, bound);
    mpfr_clears(value, reference, error, bound, (mpfr_ptr)0);
    return ok;
}

static void *cache_log_thread(void *data)
{
    cache_job_t *job = data;
    mpc_t offset;
    mpc_init2(offset, 256);
    mpc_set_d_d(offset, 0.75 + job->index, 0.125, MPC_RNDNN);
    job->ok = true;
    for (unsigned i = 0; i < 24 && job->ok; ++i)
        job->ok = cache_log_matches(offset, i, i & 1u ? 128 : 256) &&
                  cache_integer_log_matches(i + 1u, i & 1u ? 128 : 256);
    mpc_clear(offset);
    mpfr_free_cache();
    return NULL;
}

/* Verify cache keys, precision upgrades, bounded fallbacks and concurrent copied ownership. */
void run_number_cache_tests(void)
{
    mpc_t offset;
    mpc_init2(offset, 512);
    mpc_set_d_d(offset, -1.25, 0.0, MPC_RNDNN);
    bool ok = cache_log_matches(offset, 0, 128);
    mpfr_set_zero(mpc_imagref(offset), -1);
    ok = cache_log_matches(offset, 0, 128) && ok;
    mpfr_set_zero(mpc_imagref(offset), 1);
    ok = cache_log_matches(offset, 0, 256) && ok;
    ok = cache_log_matches(offset, 0, 128) && ok;
    mpc_set_d_d(offset, 0.75, 0.125, MPC_RNDNN);
    ok = cache_log_matches(offset, 511, 4096) && ok;
    ok = cache_log_matches(offset, 511, 256) && ok;
    ok = cache_log_matches(offset, 5000, 256) && ok;
    mpc_set_prec(offset, 32768);
    mpc_set_d_d(offset, 0.75, 0.125, MPC_RNDNN);
    ok = cache_log_matches(offset, 0, 256) && ok;
    mpc_clear(offset);

    ok = cache_integer_log_matches(1, 128) && ok;
    ok = cache_integer_log_matches(577, 128) && ok;
    ok = cache_integer_log_matches(577, 4096) && ok;
    ok = cache_integer_log_matches(577, 128) && ok;
    ok = cache_integer_log_matches(5000, 256) && ok;
    ok = cache_integer_log_matches(577, 32768) && ok;
    mpfr_t invalid;
    mpfr_init2(invalid, 128);
    ok = number_integer_logarithm(invalid, 0) == -1 && mpfr_nan_p(invalid) && ok;
    mpfr_clear(invalid);

    pthread_t workers[4];
    cache_job_t jobs[4] = {{0}};
    unsigned started = 0;
    for (unsigned i = 0; i < 4; ++i) {
        jobs[i].index = i;
        if (pthread_create(workers + i, NULL, cache_log_thread, jobs + i)) break;
        ++started;
    }
    ok = started == 4 && ok;
    for (unsigned i = 0; i < started; ++i) ok = !pthread_join(workers[i], NULL) && jobs[i].ok && ok;

    mpc_t input;
    mpc_init2(input, 128);
    mpfr_t lower, upper, exact;
    mpfr_inits2(128, lower, upper, exact, (mpfr_ptr)0);
    mpc_set_d_d(input, 3.0, -4.0, MPC_RNDNN);
    number_mpc_abs_lower(lower, input); number_mpc_abs_upper(upper, input); mpc_abs(exact, input, MPFR_RNDN);
    ok = mpfr_cmp(lower, exact) <= 0 && mpfr_cmp(exact, upper) <= 0 && ok;
    mpfr_set_nan(mpc_imagref(input));
    number_mpc_abs_lower(lower, input); number_mpc_abs_upper(upper, input);
    ok = mpfr_nan_p(lower) && mpfr_nan_p(upper) && ok;
    mpc_set_si(input, -3, MPC_RNDNN);
    number_mpc_abs_lower(lower, input); number_mpc_abs_upper(upper, input);
    ok = !mpfr_cmp_ui(lower, 3u) && !mpfr_cmp_ui(upper, 3u) && ok;
    mpc_set_ui(input, 0u, MPC_RNDNN);
    number_mpc_abs_lower(lower, input); number_mpc_abs_upper(upper, input);
    ok = mpfr_zero_p(lower) && mpfr_zero_p(upper) && ok;
    mpfr_set_nan(mpc_realref(input));
    number_mpc_abs_lower(lower, input); number_mpc_abs_upper(upper, input);
    ok = mpfr_nan_p(lower) && mpfr_nan_p(upper) && ok;
    mpfr_clears(lower, upper, exact, (mpfr_ptr)0);
    mpc_clear(input);

    mpfr_t term, scale, reference;
    mpfr_inits2(128, term, scale, reference, (mpfr_ptr)0);
    const mpfr_prec_t precisions[] = {1, 32, 128, 4096};
    for (size_t i = 0; i < sizeof precisions / sizeof *precisions; ++i) {
        for (unsigned factor = 0; factor < 5; ++factor) {
            mpfr_set_ui_2exp(scale, 1u, -10000, MPFR_RNDN);
            for (long offset = -3; offset <= 3; ++offset) {
                mpfr_set_ui_2exp(term, 3u, -10000 - precisions[i] - factor + offset, MPFR_RNDN);
                bool small = number_series_small(term, scale, precisions[i], factor);
                mpfr_mul_2si(reference, term, (long)precisions[i] + factor, MPFR_RNDU);
                ok = (!small || mpfr_lessequal_p(reference, scale)) && ok;
            }
        }
    }
    mpfr_set_zero(term, 1); mpfr_set_zero(scale, 1);
    ok = number_series_small(term, scale, 32, 0) && ok;
    mpfr_set_ui(term, 1u, MPFR_RNDN);
    ok = !number_series_small(term, scale, 32, 0) && ok;
    mpfr_set_inf(scale, 1);
    ok = !number_series_small(term, scale, 32, 0) && ok;
    mpfr_set_ui(scale, 1u, MPFR_RNDN); mpfr_neg(term, term, MPFR_RNDN);
    ok = !number_series_small(term, scale, 32, 0) && ok;
    mpfr_set_nan(term);
    ok = !number_series_small(term, scale, 32, 0) && ok;
    mpfr_clears(term, scale, reference, (mpfr_ptr)0);
    ASSERT_TRUE(ok);
}
