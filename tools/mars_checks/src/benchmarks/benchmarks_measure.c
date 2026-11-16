/**
 * @file benchmarks_measure.c
 * @brief Isolated, checked monotonic measurements of typed mathematical operations.
 *
 * Arguments and output storage are prepared outside timed intervals. Warm measurements amortise clock costs
 * through calibration; cold measurements fork from an unevaluated worker, and changing measurements perturb
 * eligible inputs before each single timed call. Result checks and destruction happen after each timed batch.
 * The parent process supplies a hard deadline, so a non-cooperative algorithm cannot stall the complete run.
 */
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "number-t.h"
#include "qfloat-t.h"
#include "qcomplex-t.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

const char benchmark_protocol[] =
    "CLOCK_MONOTONIC; warmup=100ms; target_batch=5ms; max_batch=1048576; storage_chunk=512";

/* Return stable identities so cold, varying and warmed observations never share a baseline. */
const char *benchmark_protocol_for(const char *mode)
{
    if (!strcmp(mode, "warm")) return benchmark_protocol;
    if (!strcmp(mode, "cold"))
        return "CLOCK_MONOTONIC; cold=fork_from_unevaluated_worker; warmup=0; iterations=1; setup=untimed";
    if (!strcmp(mode, "changing"))
        return "CLOCK_MONOTONIC; changing=eligible_real_parts+(sample+1)/128; warmup=0; iterations=1; setup=untimed";
    return NULL;
}

typedef struct {
    const benchmark_operation_t *operation;
    number_t number[6];
    qfloat_t real[6];
    qcomplex_t complex_values[6];
    volatile double sink;
    bool exact;
    bool integer;
    unsigned bits;
    bool precision_limited;
} benchmark_state_t;

static bool benchmark_now(uint64_t *value)
{
    struct timespec stamp;
    if (clock_gettime(CLOCK_MONOTONIC, &stamp) || stamp.tv_sec < 0 ||
        (uint64_t)stamp.tv_sec > (UINT64_MAX - (uint64_t)stamp.tv_nsec) / 1000000000u)
        return false;
    *value = (uint64_t)stamp.tv_sec * 1000000000u + (uint64_t)stamp.tv_nsec;
    return true;
}

static bool benchmark_real_finite(qfloat_t x)
{
    return !qf_isnan(x) && !qf_isinf(x);
}

static bool benchmark_complex_finite(qcomplex_t z)
{
    return benchmark_real_finite(z.re) && benchmark_real_finite(z.im);
}

static bool benchmark_batch_number(benchmark_state_t *state, unsigned iterations, uint64_t *duration)
{
    number_t output[512];
    const number_t *a = state->number;
    const benchmark_operation_t *op = state->operation;
    uint64_t start, end;
    if (!benchmark_now(&start)) return false;
    if (op->arity == 1)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->number.n1(a[0]);
    else if (op->arity == 2)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->number.n2(a[0], a[1]);
    else if (op->arity == 3)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->number.n3(a[0], a[1], a[2]);
    else if (op->arity == 6)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->number.n6(a[0], a[1], a[2], a[3], a[4], a[5]);
    else return false;
    if (!benchmark_now(&end) || end < start) return false;
    bool ok = true;
    for (unsigned i = 0; i < iterations; ++i) {
        ok = num_is_finite(output[i]) && ok;
        if (state->exact)
            ok = num_is_exact(output[i]) && num_is_integer(output[i]) == state->integer && ok;
        else if (num_is_finite(output[i]) && !num_is_exact(output[i]) && num_get_prec_bits(output[i]) < state->bits) {
            state->precision_limited = true;
            ok = false;
        }
        num_destroy(output + i);
    }
    *duration = end - start;
    return ok;
}

static bool benchmark_batch_real(benchmark_state_t *state, unsigned iterations, uint64_t *duration)
{
    qfloat_t output[512];
    const qfloat_t *a = state->real;
    const benchmark_operation_t *op = state->operation;
    uint64_t start, end;
    if (!benchmark_now(&start)) return false;
    if (op->arity == 1)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->real.r1(a[0]);
    else if (op->arity == 2)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->real.r2(a[0], a[1]);
    else if (op->arity == 3)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->real.r3(a[0], a[1], a[2]);
    else if (op->arity == 6)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->real.r6(a[0], a[1], a[2], a[3], a[4], a[5]);
    else return false;
    if (!benchmark_now(&end) || end < start) return false;
    bool ok = true;
    for (unsigned i = 0; i < iterations; ++i) {
        ok = benchmark_real_finite(output[i]) && ok;
        state->sink += qf_to_double(output[i]);
    }
    *duration = end - start;
    return ok;
}

static bool benchmark_batch_complex(benchmark_state_t *state, unsigned iterations, uint64_t *duration)
{
    qcomplex_t output[512];
    const qcomplex_t *a = state->complex_values;
    const benchmark_operation_t *op = state->operation;
    uint64_t start, end;
    if (!benchmark_now(&start)) return false;
    if (op->arity == 1)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->complex_fn.c1(a[0]);
    else if (op->arity == 2)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->complex_fn.c2(a[0], a[1]);
    else if (op->arity == 3)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->complex_fn.c3(a[0], a[1], a[2]);
    else if (op->arity == 6)
        for (unsigned i = 0; i < iterations; ++i)
            output[i] = op->complex_fn.c6(a[0], a[1], a[2], a[3], a[4], a[5]);
    else return false;
    if (!benchmark_now(&end) || end < start) return false;
    bool ok = true;
    for (unsigned i = 0; i < iterations; ++i) {
        ok = benchmark_complex_finite(output[i]) && ok;
        state->sink += qf_to_double(output[i].re);
    }
    *duration = end - start;
    return ok;
}

/* Sum checked chunks without timing result disposal or allowing calibration to enlarge live output storage. */
static bool benchmark_batch(benchmark_state_t *state, unsigned iterations, uint64_t *duration,
                             bool (*chunk)(benchmark_state_t *, unsigned, uint64_t *))
{
    *duration = 0;
    for (unsigned completed = 0; completed < iterations;) {
        unsigned count = iterations - completed;
        if (count > 512u) count = 512u;
        uint64_t elapsed = 0;
        if (!chunk(state, count, &elapsed) || elapsed > UINT64_MAX - *duration) return false;
        *duration += elapsed;
        completed += count;
    }
    return true;
}

/* Fork every sample from a parent that has prepared operands but has never evaluated the operation. */
static bool benchmark_cold(benchmark_state_t *state, uint64_t *duration,
                            bool (*chunk)(benchmark_state_t *, unsigned, uint64_t *))
{
    int channel[2];
    if (pipe(channel)) return false;
    pid_t child = fork();
    if (child < 0) { close(channel[0]); close(channel[1]); return false; }
    struct { uint64_t duration; unsigned ok, limited; } result = {0};
    if (!child) {
        close(channel[0]);
        result.ok = chunk(state, 1, &result.duration);
        result.limited = state->precision_limited;
        ssize_t sent;
        do sent = write(channel[1], &result, sizeof result); while (sent < 0 && errno == EINTR);
        close(channel[1]);
        _exit(sent == sizeof result ? 0 : 1);
    }
    close(channel[1]);
    size_t received = 0;
    while (received < sizeof result) {
        ssize_t count = read(channel[0], (char *)&result + received, sizeof result - received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        received += (size_t)count;
    }
    close(channel[0]);
    int status = 0;
    pid_t waited;
    do waited = waitpid(child, &status, 0); while (waited < 0 && errno == EINTR);
    state->precision_limited = result.limited != 0;
    *duration = result.duration;
    return received == sizeof result && waited == child && WIFEXITED(status) && !WEXITSTATUS(status) && result.ok;
}

/* Move permitted real parts by one exact binary fraction before timing the next independent input. */
static bool benchmark_change(benchmark_state_t *state)
{
    number_t step = num_create_from_frac(1, 128);
    bool ok = true;
    for (unsigned i = 0; i < state->operation->arity; ++i) {
        if (state->operation->complex_mask && !(state->operation->complex_mask & (1u << i))) continue;
        number_t next = num_add(state->number[i], step);
        num_destroy(state->number + i);
        state->number[i] = next;
        ok = num_is_finite(next) && ok;
    }
    num_destroy(&step);
    return ok;
}

static bool benchmark_prepare(benchmark_state_t *state, const char *backend, unsigned bits, const char *operands)
{
    const benchmark_operation_t *op = state->operation;
    string_t *text = operands ? string_new_with(operands) : NULL;
    size_t count = 0;
    string_t **parts = text ? string_split(text, ",", &count) : NULL;
    bool ok = !operands || (parts && count == op->arity);
    for (unsigned i = 0; ok && i < op->arity; ++i) {
        if (parts) string_trim(parts[i]);
        const char *argument = parts ? string_c_str(parts[i]) : op->arguments[i];
        if (state->exact) {
            state->number[i] = num_create_from_string(argument);
            if (!num_is_finite(state->number[i]) || !num_is_exact(state->number[i]) ||
                (state->integer && !num_is_integer(state->number[i])) ||
                (!state->integer && !i && num_is_integer(state->number[i]))) ok = false;
            continue;
        }
        state->real[i] = qf_from_string(argument);
        bool has_imaginary = (op->complex_mask & (1u << i)) != 0;
        qfloat_t imaginary = has_imaginary ? qf_from_string(benchmark_imaginary_literal) : QF_ZERO;
        state->complex_values[i] = qc_make(state->real[i], imaginary);
        if (!strcmp(backend, "number-double")) {
            state->number[i] = num_create_from_double(qf_to_double(state->real[i]));
        } else if (!strcmp(backend, "mpfr") || !strcmp(backend, "mpc")) {
            bool complex_value = !strcmp(backend, "mpc") &&
                                 ((op->complex_mask & (1u << i)) || !op->complex_mask);
            string_t *literal = benchmark_argument_literal(argument, complex_value, has_imaginary);
            number_t parsed = num_create_from_text(literal), zero = num_new_with_prec_bits(bits);
            state->number[i] = num_add(parsed, zero);
            num_destroy(&parsed);
            num_destroy(&zero);
            string_free(literal);
            if (!num_is_finite(state->number[i])) ok = false;
        }
    }
    string_split_free(parts, count);
    string_free(text);
    return ok;
}

/* Measure one operation without touching persistent benchmark or test storage. */
int benchmark_measure(const string_t *root, int argc, char **argv)
{
    (void)root;
    if (argc < 4 || argc > 6) return 2;
    const char *mode = argc == 6 ? argv[5] : "warm";
    if (!benchmark_protocol_for(mode)) return 2;
    bool cold = !strcmp(mode, "cold"), changing = !strcmp(mode, "changing");
    const benchmark_operation_t *op = benchmark_operation(argv[0]);
    char *end = NULL;
    unsigned bits = (unsigned)strtoul(argv[2], &end, 10);
    if (!op || !end || *end || bits > 16384) return 2;
    unsigned samples = (unsigned)strtoul(argv[3], &end, 10);
    if (!end || *end || samples < 3 || samples > 101) return 2;
    const char *backend = argv[1];
    bool real = !strcmp(backend, "qfloat"), complex_backend = !strcmp(backend, "qcomplex");
    bool exact = benchmark_backend_exact(backend), integer = !strcmp(backend, "exact-integer");
    bool number = !strcmp(backend, "number-double") || !strcmp(backend, "mpfr") || !strcmp(backend, "mpc") || exact;
    if (changing && (op->integer_only || (strcmp(backend, "mpfr") && strcmp(backend, "mpc")))) return 3;
    if (!real && !complex_backend && !number) return 2;
    if ((op->integer_only && !integer) || (exact && !op->integer_only && argc < 5)) return 3;
    if ((!bits && !exact) || (exact && bits)) return 2;
    if (real && !benchmark_operation_supported(op, false)) return 3;
    if (complex_backend && !benchmark_operation_supported(op, true)) return 3;
    if ((real || complex_backend) && bits != 106) return 2;
    if (!strcmp(backend, "number-double") && bits != 53) return 2;
    struct timespec resolution;
    /* Keep these bounded exact workloads below the library's rational rounding threshold. */
    if (clock_getres(CLOCK_MONOTONIC, &resolution) || num_set_default_prec_bits(bits ? bits : 16384)) return 1;
    benchmark_state_t state = {.operation = op, .exact = exact, .integer = integer, .bits = bits};
    bool ok = benchmark_prepare(&state, backend, bits, argc >= 5 ? argv[4] : NULL);
    bool (*chunk)(benchmark_state_t *, unsigned, uint64_t *) =
        real ? benchmark_batch_real : complex_backend ? benchmark_batch_complex : benchmark_batch_number;
    unsigned iterations = 1;
    uint64_t duration = 0;
    uint64_t warm_start = 0, warm_end = 0;
    ok = benchmark_now(&warm_start) && ok;
    warm_end = warm_start;
    while (!cold && !changing && ok && warm_end >= warm_start && warm_end - warm_start < 100000000) {
        if (ok) ok = benchmark_batch(&state, 1, &duration, chunk) && benchmark_now(&warm_end);
    }
    if (!cold && !changing && ok) ok = benchmark_batch(&state, 1, &duration, chunk);
    while (!cold && !changing && ok && duration < 5000000 && iterations < 1048576u) {
        iterations *= 2;
        ok = benchmark_batch(&state, iterations, &duration, chunk);
    }
    double values[101];
    for (unsigned i = 0; ok && i < samples; ++i) {
        if (changing) ok = benchmark_change(&state);
        if (ok) ok = cold ? benchmark_cold(&state, &duration, chunk)
                          : benchmark_batch(&state, iterations, &duration, chunk);
        values[i] = (double)duration / iterations;
    }
    for (unsigned i = 0; i < op->arity; ++i) num_destroy(state.number + i);
    if (!ok) {
        if (state.precision_limited) {
            fputs("The result backend does not preserve the requested precision.\n", stderr);
            return 3;
        }
        if (errno == ENOTSUP) return 3;
        fputs("Non-finite mathematical result or monotonic clock failure.\n", stderr);
        return 1;
    }
    printf("%u %llu", iterations,
           (unsigned long long)resolution.tv_sec * 1000000000ull + (unsigned long long)resolution.tv_nsec);
    for (unsigned i = 0; i < samples; ++i) printf(" %.17g", values[i]);
    return putchar('\n') == EOF ? 1 : 0;
}
