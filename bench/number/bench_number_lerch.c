/**
 * @file bench_number_lerch.c
 * @brief Separate fresh-process and changing-input timings for high-precision Lerch evaluation.
 *
 * Executes one child process at a time for cold samples, then changes every argument between samples in the
 * parent. Reports raw timings and medians as CSV; startup, argument construction and result validation are outside
 * the timer. No cache is reset, no timing baseline is changed and no memory instrumentation is used. Ordinary
 * number-module tests independently check accuracy. Optional arguments select precision (64--8192 bits) and an
 * odd sample count (3--21); defaults are 4096 bits and five samples.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "number-t.h"

enum { LERCH_BENCH_MAX_SAMPLES = 21 };

static uint64_t lerch_bench_nanoseconds(void)
{
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (uint64_t)value.tv_sec * 1000000000u + (uint64_t)value.tv_nsec;
}

static bool lerch_bench_sample(size_t bits, unsigned index, uint64_t *elapsed)
{
    num_set_default_prec_bits(bits);
    NUM_SCOPE(scope);
    number_t shift = num_create_from_frac(index, 128);
    number_t argument = num_add(num_create_from_string("0.3+0.125i"), shift);
    number_t order = num_add(num_create_from_string("2+0.125i"), shift);
    number_t offset = num_add(num_create_from_string("1.4+0.125i"), shift);
    if (num_set_prec_bits(&argument, bits) || num_set_prec_bits(&order, bits) || num_set_prec_bits(&offset, bits))
        return false;
    uint64_t start = lerch_bench_nanoseconds();
    number_t value = num_lerch_phi(argument, order, offset);
    *elapsed = lerch_bench_nanoseconds() - start;
    bool valid = num_is_finite(value) && num_get_prec_bits(value) >= bits;
    num_destroy(&value);
    return valid;
}

static bool lerch_bench_cold(const char *self, size_t bits, uint64_t *elapsed)
{
    int channel[2];
    if (pipe(channel)) return false;
    pid_t child = fork();
    if (child < 0) { close(channel[0]); close(channel[1]); return false; }
    if (!child) {
        close(channel[0]);
        if (dup2(channel[1], STDOUT_FILENO) < 0) _exit(127);
        close(channel[1]);
        char precision[32];
        snprintf(precision, sizeof precision, "%zu", bits);
        char *arguments[] = {(char *)self, "--cold-child", precision, NULL};
        execv("/proc/self/exe", arguments);
        _exit(127);
    }
    close(channel[1]);
    FILE *stream = fdopen(channel[0], "r");
    unsigned long long measured = 0;
    bool read_ok = stream && fscanf(stream, "%llu", &measured) == 1;
    if (stream) fclose(stream);
    else close(channel[0]);
    int status = 0;
    pid_t waited;
    do waited = waitpid(child, &status, 0); while (waited < 0 && errno == EINTR);
    *elapsed = (uint64_t)measured;
    return read_ok && waited == child && WIFEXITED(status) && !WEXITSTATUS(status);
}

static int lerch_bench_compare(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static bool lerch_bench_unsigned(const char *text, unsigned long *value)
{
    if (!text || !*text || *text == '-') return false;
    char *end = NULL;
    errno = 0;
    *value = strtoul(text, &end, 10);
    return !errno && end && !*end;
}

int main(int argc, char **argv)
{
    bool child = argc == 3 && !strcmp(argv[1], "--cold-child");
    unsigned long bits = 4096, samples = 5;
    if ((child && !lerch_bench_unsigned(argv[2], &bits)) ||
        (!child && (argc > 3 || (argc > 1 && !lerch_bench_unsigned(argv[1], &bits)) ||
                     (argc > 2 && !lerch_bench_unsigned(argv[2], &samples)))) ||
        bits < 64 || bits > 8192 || samples < 3 || samples > LERCH_BENCH_MAX_SAMPLES || !(samples % 2)) {
        fprintf(stderr, "Usage: %s [bits:64..8192] [odd samples:3..21]\n", argv[0]);
        return 2;
    }
    if (child) {
        uint64_t elapsed;
        if (!lerch_bench_sample(bits, 0, &elapsed)) return 1;
        printf("%llu\n", (unsigned long long)elapsed);
        return 0;
    }
    uint64_t cold[LERCH_BENCH_MAX_SAMPLES], changing[LERCH_BENCH_MAX_SAMPLES];
    puts("mode,bits,sample,milliseconds");
    for (unsigned i = 0; i < samples; ++i) {
        if (!lerch_bench_cold(argv[0], bits, cold + i)) return 1;
        printf("cold,%lu,%u,%.6f\n", bits, i + 1, cold[i] / 1e6);
        fflush(stdout);
    }
    for (unsigned i = 0; i < samples; ++i) {
        if (!lerch_bench_sample(bits, i + 1, changing + i)) return 1;
        printf("changing,%lu,%u,%.6f\n", bits, i + 1, changing[i] / 1e6);
        fflush(stdout);
    }
    qsort(cold, samples, sizeof *cold, lerch_bench_compare);
    qsort(changing, samples, sizeof *changing, lerch_bench_compare);
    printf("cold_median,%lu,,%.6f\nchanging_median,%lu,,%.6f\n",
           bits, cold[samples / 2] / 1e6, bits, changing[samples / 2] / 1e6);
    return 0;
}
