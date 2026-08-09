#include <math.h>
#include <stdio.h>
#include <time.h>

#include "number.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

enum { REAL_BATCHES = 600, REAL_STEPS = 64, COMPLEX_BATCHES = 500, COMPLEX_STEPS = 48 };

static volatile double g_sink = 0.0;

static double bench_now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double bench_real_manual(void)
{
    number_t seed = num_create_from_string("1.23456789012345678901234567890123456789");
    number_t scale = num_create_from_string("0.87500000000000000000000000000000000001");
    number_t bias = num_create_from_string("0.03125000000000000000000000000000000000");
    double start = bench_now_seconds();

    for (int batch = 0; batch < REAL_BATCHES; ++batch) {
        number_t x = num_clone(seed);

        for (int step = 0; step < REAL_STEPS; ++step) {
            number_t prev = x;
            number_t t1 = num_mul(x, scale);
            number_t t2 = num_add(t1, bias);
            number_t t3 = num_mul(t2, t2);
            number_t t4 = num_sub(t3, bias);
            x = t4;

            num_destroy(&t3);
            num_destroy(&t2);
            num_destroy(&t1);
            num_destroy(&prev);
        }

        g_sink += num_to_double(x);
        num_destroy(&x);
    }

    num_destroy(&bias);
    num_destroy(&scale);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static double bench_real_scoped(void)
{
    number_t seed = num_create_from_string("1.23456789012345678901234567890123456789");
    number_t scale = num_create_from_string("0.87500000000000000000000000000000000001");
    number_t bias = num_create_from_string("0.03125000000000000000000000000000000000");
    double start = bench_now_seconds();

    for (int batch = 0; batch < REAL_BATCHES; ++batch) {
        num_scope_t *scope = num_scope_enter();

        number_t x = num_clone(seed);

        for (int step = 0; step < REAL_STEPS; ++step) {
            number_t t1 = num_mul(x, scale);
            number_t t2 = num_add(t1, bias);
            number_t t3 = num_mul(t2, t2);
            x = num_sub(t3, bias);
        }

        g_sink += num_to_double(x);
        num_scope_leave(&scope);
    }

    num_destroy(&bias);
    num_destroy(&scale);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static double bench_real_scoped_rolling(void)
{
    number_t seed = num_create_from_string("1.23456789012345678901234567890123456789");
    number_t scale = num_create_from_string("0.87500000000000000000000000000000000001");
    number_t bias = num_create_from_string("0.03125000000000000000000000000000000000");
    double start = bench_now_seconds();

    for (int batch = 0; batch < REAL_BATCHES; ++batch) {
        num_scope_t *scope = num_scope_enter();
        num_scope_t *saved_scope = number_scope_suspend();
        number_t x = num_clone(seed);
        number_scope_resume(saved_scope);

        for (int step = 0; step < REAL_STEPS; ++step) {
            number_t prev = x;
            number_t t1 = num_mul(x, scale);
            number_t t2 = num_add(t1, bias);
            number_t t3 = num_mul(t2, t2);
            saved_scope = number_scope_suspend();
            x = num_sub(t3, bias);
            number_scope_resume(saved_scope);
            num_destroy(&t3);
            num_destroy(&t2);
            num_destroy(&t1);
            num_destroy(&prev);
        }

        g_sink += num_to_double(x);
        num_destroy(&x);
        num_scope_leave(&scope);
    }

    num_destroy(&bias);
    num_destroy(&scale);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static double bench_complex_manual(void)
{
    number_t seed = num_create_from_string("0.812345678901234567890123456789 + 0.134567890123456789012345678901i");
    number_t a = num_create_from_string("0.625000000000000000000000000000 + 0.187500000000000000000000000000i");
    number_t b = num_create_from_string("0.093750000000000000000000000000 - 0.062500000000000000000000000000i");
    number_t c = num_create_from_string("0.500000000000000000000000000000 + 0.125000000000000000000000000000i");
    number_t d = num_create_from_string("0.015625000000000000000000000000 + 0.031250000000000000000000000000i");
    double start = bench_now_seconds();

    for (int batch = 0; batch < COMPLEX_BATCHES; ++batch) {
        number_t z = num_clone(seed);

        for (int step = 0; step < COMPLEX_STEPS; ++step) {
            number_t prev = z;
            number_t t1 = num_mul(z, a);
            number_t t2 = num_add(t1, b);
            number_t t3 = num_mul(t2, c);
            number_t t4 = num_add(t3, d);
            z = t4;

            num_destroy(&t3);
            num_destroy(&t2);
            num_destroy(&t1);
            num_destroy(&prev);
        }

        number_t mag = num_abs(z);
        g_sink += num_to_double(mag);
        num_destroy(&mag);
        num_destroy(&z);
    }

    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static double bench_complex_scoped(void)
{
    number_t seed = num_create_from_string("0.812345678901234567890123456789 + 0.134567890123456789012345678901i");
    number_t a = num_create_from_string("0.625000000000000000000000000000 + 0.187500000000000000000000000000i");
    number_t b = num_create_from_string("0.093750000000000000000000000000 - 0.062500000000000000000000000000i");
    number_t c = num_create_from_string("0.500000000000000000000000000000 + 0.125000000000000000000000000000i");
    number_t d = num_create_from_string("0.015625000000000000000000000000 + 0.031250000000000000000000000000i");
    double start = bench_now_seconds();

    for (int batch = 0; batch < COMPLEX_BATCHES; ++batch) {
        num_scope_t *scope = num_scope_enter();

        number_t z = num_clone(seed);

        for (int step = 0; step < COMPLEX_STEPS; ++step) {
            number_t t1 = num_mul(z, a);
            number_t t2 = num_add(t1, b);
            number_t t3 = num_mul(t2, c);
            z = num_add(t3, d);
        }

        number_t mag = num_abs(z);
        g_sink += num_to_double(mag);
        num_scope_leave(&scope);
    }

    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static double bench_complex_scoped_rolling(void)
{
    number_t seed = num_create_from_string("0.812345678901234567890123456789 + 0.134567890123456789012345678901i");
    number_t a = num_create_from_string("0.625000000000000000000000000000 + 0.187500000000000000000000000000i");
    number_t b = num_create_from_string("0.093750000000000000000000000000 - 0.062500000000000000000000000000i");
    number_t c = num_create_from_string("0.500000000000000000000000000000 + 0.125000000000000000000000000000i");
    number_t d = num_create_from_string("0.015625000000000000000000000000 + 0.031250000000000000000000000000i");
    double start = bench_now_seconds();

    for (int batch = 0; batch < COMPLEX_BATCHES; ++batch) {
        num_scope_t *scope = num_scope_enter();
        num_scope_t *saved_scope = number_scope_suspend();
        number_t z = num_clone(seed);
        number_scope_resume(saved_scope);

        for (int step = 0; step < COMPLEX_STEPS; ++step) {
            number_t prev = z;
            number_t t1 = num_mul(z, a);
            number_t t2 = num_add(t1, b);
            number_t t3 = num_mul(t2, c);
            saved_scope = number_scope_suspend();
            z = num_add(t3, d);
            number_scope_resume(saved_scope);
            num_destroy(&t3);
            num_destroy(&t2);
            num_destroy(&t1);
            num_destroy(&prev);
        }

        saved_scope = number_scope_suspend();
        number_t mag = num_abs(z);
        number_scope_resume(saved_scope);
        g_sink += num_to_double(mag);
        num_destroy(&mag);
        num_destroy(&z);
        num_scope_leave(&scope);
    }

    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&seed);
    return bench_now_seconds() - start;
}

static void bench_report(const char *label, double manual_s, double scoped_s)
{
    double speedup = (scoped_s > 0.0) ? (manual_s / scoped_s) : 0.0;
    printf("%-24s manual=%8.3f ms  scoped=%8.3f ms  ratio=%6.3fx\n", label, manual_s * 1000.0, scoped_s * 1000.0,
           speedup);
}

int main(void)
{
    double real_manual = bench_real_manual();
    double real_scoped = bench_real_scoped();
    double real_scoped_rolling = bench_real_scoped_rolling();
    double complex_manual = bench_complex_manual();
    double complex_scoped = bench_complex_scoped();
    double complex_scoped_rolling = bench_complex_scoped_rolling();

    bench_report("real chain", real_manual, real_scoped);
    bench_report("real scoped+roll", real_manual, real_scoped_rolling);
    bench_report("complex chain", complex_manual, complex_scoped);
    bench_report("complex scoped+roll", complex_manual, complex_scoped_rolling);
    printf("sink=%.17g\n", g_sink);
    return !isfinite(g_sink);
}
