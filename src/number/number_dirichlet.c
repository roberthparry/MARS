/**
 * @file number_dirichlet.c
 * @brief Bounded reuse of shifted inverse powers in convergent Dirichlet series.
 *
 * Retains one exact offset/exponent pair with direct integer indexing, up to 4096 entries and eight MiB of
 * significands. Higher-precision entries supply rounded copies to lower requests with the same exact key. Misses
 * are evaluated outside the mutex; changing either key releases old entries. Unsupported retention sizes use the
 * identical uncached arithmetic, and no borrowed cache pointers escape.
 */
#include <pthread.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { DIRICHLET_SLOTS = 4096, DIRICHLET_MAX_BITS = 16384, DIRICHLET_BYTES = 8 * 1024 * 1024 };
typedef struct {
    mpc_t value;
    mpfr_prec_t precision;
} dirichlet_entry_t;

static dirichlet_entry_t dirichlet_entries[DIRICHLET_SLOTS];
static dirichlet_entry_t *dirichlet_owned[DIRICHLET_SLOTS];
static mpc_t dirichlet_offset, dirichlet_exponent;
static bool dirichlet_ready;
static size_t dirichlet_count, dirichlet_bytes;
static pthread_mutex_t dirichlet_lock = PTHREAD_MUTEX_INITIALIZER;

static bool dirichlet_same_value(mpc_srcptr a, mpc_srcptr b)
{
    return !mpc_cmp(a, b) &&
        (!mpfr_zero_p(mpc_realref(a)) || mpfr_signbit(mpc_realref(a)) == mpfr_signbit(mpc_realref(b))) &&
        (!mpfr_zero_p(mpc_imagref(a)) || mpfr_signbit(mpc_imagref(a)) == mpfr_signbit(mpc_imagref(b)));
}

static bool dirichlet_same(mpc_srcptr offset, mpc_srcptr exponent)
{
    return dirichlet_ready && dirichlet_same_value(offset, dirichlet_offset) &&
           dirichlet_same_value(exponent, dirichlet_exponent);
}

static void dirichlet_clear(void)
{
    /* Only populated entries own allocations; releasing those allocations necessarily visits each owner. */
    for (size_t i = 0; i < dirichlet_count; ++i) {
        mpc_clear(dirichlet_owned[i]->value);
        dirichlet_owned[i]->precision = 0;
    }
    dirichlet_count = dirichlet_bytes = 0;
}

/* Copy a retained inverse power, or calculate and optionally retain it without locking transcendental work. */
void number_dirichlet_power(mpc_ptr out, mpc_srcptr offset, mpc_srcptr exponent, unsigned long index)
{
    mpfr_prec_t precision = mpc_get_prec(out);
    bool eligible = index < DIRICHLET_SLOTS && precision <= DIRICHLET_MAX_BITS &&
        mpc_get_prec(offset) <= DIRICHLET_MAX_BITS && mpc_get_prec(exponent) <= DIRICHLET_MAX_BITS &&
        mpfr_number_p(mpc_realref(offset)) && mpfr_number_p(mpc_imagref(offset)) &&
        mpfr_number_p(mpc_realref(exponent)) && mpfr_number_p(mpc_imagref(exponent));
    if (eligible && !pthread_mutex_lock(&dirichlet_lock)) {
        dirichlet_entry_t *entry = dirichlet_entries + index;
        bool found = dirichlet_same(offset, exponent) && entry->precision >= precision;
        if (found) mpc_set(out, entry->value, MPC_RNDNN);
        pthread_mutex_unlock(&dirichlet_lock);
        if (found) return;
    }
    mpc_t base, negative;
    mpc_init2(base, precision); mpc_init2(negative, mpc_get_prec(exponent));
    mpc_add_ui(base, offset, index, MPC_RNDNN);
    mpc_neg(negative, exponent, MPC_RNDNN);
    mpc_pow(out, base, negative, MPC_RNDNN);
    mpc_clear(base); mpc_clear(negative);
    if (!eligible || !mpfr_number_p(mpc_realref(out)) || !mpfr_number_p(mpc_imagref(out)) ||
        pthread_mutex_lock(&dirichlet_lock)) return;
    if (!dirichlet_same(offset, exponent)) {
        dirichlet_clear();
        if (!dirichlet_ready) {
            mpc_init2(dirichlet_offset, mpc_get_prec(offset));
            mpc_init2(dirichlet_exponent, mpc_get_prec(exponent));
        } else {
            mpc_set_prec(dirichlet_offset, mpc_get_prec(offset));
            mpc_set_prec(dirichlet_exponent, mpc_get_prec(exponent));
        }
        mpc_set(dirichlet_offset, offset, MPC_RNDNN); mpc_set(dirichlet_exponent, exponent, MPC_RNDNN);
        dirichlet_ready = true;
    }
    dirichlet_entry_t *entry = dirichlet_entries + index;
    if (entry->precision < precision) {
        size_t bytes = 2u * mpfr_custom_get_size(precision);
        size_t previous = entry->precision ? 2u * mpfr_custom_get_size(entry->precision) : 0;
        size_t budget = DIRICHLET_BYTES - 2u * mpfr_custom_get_size(mpc_get_prec(offset)) -
                                         2u * mpfr_custom_get_size(mpc_get_prec(exponent));
        if (dirichlet_bytes <= budget && bytes - previous <= budget - dirichlet_bytes) {
            if (!entry->precision) {
                mpc_init2(entry->value, precision);
                dirichlet_owned[dirichlet_count++] = entry;
            } else mpc_set_prec(entry->value, precision);
            mpc_set(entry->value, out, MPC_RNDNN);
            entry->precision = precision;
            dirichlet_bytes += bytes - previous;
        }
    }
    pthread_mutex_unlock(&dirichlet_lock);
}

static void __attribute__((destructor)) dirichlet_shutdown(void)
{
    dirichlet_clear();
    if (dirichlet_ready) { mpc_clear(dirichlet_offset); mpc_clear(dirichlet_exponent); }
}
