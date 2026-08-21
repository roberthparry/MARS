#include <mpfr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void bench_digamma(mpfr_prec_t prec, int iters)
{
    mpfr_t src;
    mpfr_t value;
    uint64_t start;
    uint64_t end;
    double avg_ms;
    double avg_us;

    mpfr_init2(src, prec);
    mpfr_init2(value, prec);
    mpfr_set_ui(src, 2u, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    mpfr_digamma(value, value, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mpfr_digamma(value, value, MPFR_RNDN);
        if (i + 1 < iters)
            mpfr_set(value, src, MPFR_RNDN);
    }
    end = now_ns();

    avg_ms = ((double)(end - start) / (double)iters) / 1000000.0;
    avg_us = avg_ms * 1000.0;
    printf("mpfr_digamma(2) bits=%-4lu avg_us=%10.3f avg_ms=%10.3f\n", (unsigned long)prec, avg_us, avg_ms);

    mpfr_clear(src);
    mpfr_clear(value);
}

static void bench_Ei_value(const char *label, unsigned long input, mpfr_prec_t prec, int iters)
{
    mpfr_t src;
    mpfr_t value;
    uint64_t start;
    uint64_t end;
    double avg_ms;
    double avg_us;

    mpfr_init2(src, prec);
    mpfr_init2(value, prec);
    mpfr_set_ui(src, input, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    mpfr_eint(value, value, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mpfr_eint(value, value, MPFR_RNDN);
        if (i + 1 < iters)
            mpfr_set(value, src, MPFR_RNDN);
    }
    end = now_ns();

    avg_ms = ((double)(end - start) / (double)iters) / 1000000.0;
    avg_us = avg_ms * 1000.0;
    printf("%-16s bits=%-4lu avg_us=%10.3f avg_ms=%10.3f\n", label, (unsigned long)prec, avg_us, avg_ms);

    mpfr_clear(src);
    mpfr_clear(value);
}

static void bench_exp_value(const char *label, const char *input, mpfr_prec_t prec, int iters)
{
    mpfr_t src;
    mpfr_t value;
    uint64_t start;
    uint64_t end;
    double avg_ms;
    double avg_us;

    mpfr_init2(src, prec);
    mpfr_init2(value, prec);
    mpfr_set_str(src, input, 10, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    mpfr_exp(value, value, MPFR_RNDN);
    mpfr_set(value, src, MPFR_RNDN);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mpfr_exp(value, value, MPFR_RNDN);
        if (i + 1 < iters)
            mpfr_set(value, src, MPFR_RNDN);
    }
    end = now_ns();

    avg_ms = ((double)(end - start) / (double)iters) / 1000000.0;
    avg_us = avg_ms * 1000.0;
    printf("%-18s bits=%-4lu avg_us=%10.3f avg_ms=%10.3f\n", label, (unsigned long)prec, avg_us, avg_ms);

    mpfr_clear(src);
    mpfr_clear(value);
}

static void bench_binary_value(const char *label, const char *lhs_text, const char *rhs_text, mpfr_prec_t prec,
                               int iters, int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t))
{
    mpfr_t lhs_src;
    mpfr_t rhs;
    mpfr_t value;
    uint64_t start;
    uint64_t end;
    double avg_ms;
    double avg_us;

    mpfr_init2(lhs_src, prec);
    mpfr_init2(rhs, prec);
    mpfr_init2(value, prec);
    mpfr_set_str(lhs_src, lhs_text, 10, MPFR_RNDN);
    mpfr_set_str(rhs, rhs_text, 10, MPFR_RNDN);
    mpfr_set(value, lhs_src, MPFR_RNDN);

    fn(value, value, rhs, MPFR_RNDN);
    mpfr_set(value, lhs_src, MPFR_RNDN);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        fn(value, value, rhs, MPFR_RNDN);
        if (i + 1 < iters)
            mpfr_set(value, lhs_src, MPFR_RNDN);
    }
    end = now_ns();

    avg_ms = ((double)(end - start) / (double)iters) / 1000000.0;
    avg_us = avg_ms * 1000.0;
    printf("%-22s bits=%-4lu avg_us=%10.3f avg_ms=%10.3f\n", label, (unsigned long)prec, avg_us, avg_ms);

    mpfr_clear(lhs_src);
    mpfr_clear(rhs);
    mpfr_clear(value);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s digamma|ei|ei5|exp|arith\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "digamma") == 0) {
        bench_digamma(256u, 150);
        bench_digamma(512u, 80);
        bench_digamma(768u, 40);
        bench_digamma(1024u, 20);
        return 0;
    }

    if (strcmp(argv[1], "ei") == 0) {
        bench_Ei_value("mpfr_eint(1)", 1u, 256u, 8);
        bench_Ei_value("mpfr_eint(1)", 1u, 512u, 4);
        bench_Ei_value("mpfr_eint(1)", 1u, 768u, 2);
        bench_Ei_value("mpfr_eint(1)", 1u, 1024u, 1);
        return 0;
    }

    if (strcmp(argv[1], "ei5") == 0) {
        bench_Ei_value("mpfr_eint(5)", 5u, 256u, 8);
        bench_Ei_value("mpfr_eint(5)", 5u, 512u, 4);
        bench_Ei_value("mpfr_eint(5)", 5u, 768u, 2);
        bench_Ei_value("mpfr_eint(5)", 5u, 1024u, 1);
        return 0;
    }

    if (strcmp(argv[1], "exp") == 0) {
        bench_exp_value("mpfr_exp(1.23456789)", "1.23456789", 256u, 80);
        bench_exp_value("mpfr_exp(1.23456789)", "1.23456789", 512u, 40);
        bench_exp_value("mpfr_exp(1.23456789)", "1.23456789", 768u, 20);
        bench_exp_value("mpfr_exp(1.23456789)", "1.23456789", 1024u, 10);
        return 0;
    }

    if (strcmp(argv[1], "arith") == 0) {
        bench_binary_value("mpfr_add", "1.23456789", "2.345678", 256u, 400, mpfr_add);
        bench_binary_value("mpfr_sub", "1.23456789", "2.345678", 256u, 400, mpfr_sub);
        bench_binary_value("mpfr_mul", "1.23456789", "2.345678", 256u, 400, mpfr_mul);
        bench_binary_value("mpfr_div", "1.23456789", "2.345678", 256u, 200, mpfr_div);
        bench_binary_value("mpfr_add", "1.23456789", "2.345678", 512u, 200, mpfr_add);
        bench_binary_value("mpfr_sub", "1.23456789", "2.345678", 512u, 200, mpfr_sub);
        bench_binary_value("mpfr_mul", "1.23456789", "2.345678", 512u, 200, mpfr_mul);
        bench_binary_value("mpfr_div", "1.23456789", "2.345678", 512u, 100, mpfr_div);
        bench_binary_value("mpfr_add", "1.23456789", "2.345678", 768u, 100, mpfr_add);
        bench_binary_value("mpfr_sub", "1.23456789", "2.345678", 768u, 100, mpfr_sub);
        bench_binary_value("mpfr_mul", "1.23456789", "2.345678", 768u, 100, mpfr_mul);
        bench_binary_value("mpfr_div", "1.23456789", "2.345678", 768u, 50, mpfr_div);
        bench_binary_value("mpfr_add", "1.23456789", "2.345678", 1024u, 50, mpfr_add);
        bench_binary_value("mpfr_sub", "1.23456789", "2.345678", 1024u, 50, mpfr_sub);
        bench_binary_value("mpfr_mul", "1.23456789", "2.345678", 1024u, 50, mpfr_mul);
        bench_binary_value("mpfr_div", "1.23456789", "2.345678", 1024u, 20, mpfr_div);
        return 0;
    }

    fprintf(stderr, "unknown bench '%s'\n", argv[1]);
    return 2;
}
