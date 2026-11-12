/**
 * @file number_zeta_logs.c
 * @brief Bounded logarithm reuse for shifted Dirichlet sums.
 *
 * Hurwitz zeta and its exponent derivative share log(a+n) values independently of the varying exponent. A single
 * exact offset key and direct integer indices avoid searches. A mutex protects copied cache entries; logarithms
 * are calculated outside it, so other evaluations are not locked behind transcendental work. Up to eight MiB of
 * significands are retained for at most 4096 indices and 16384 working bits. Changing the offset releases the old
 * entries. Larger requests and allocation or locking failures use direct evaluation, with identical mathematics.
 */
#include <pthread.h>

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
