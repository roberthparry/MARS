/**
 * @file benchmarks_arguments.c
 * @brief Shared operand spelling for mathematical workers and saved benchmark reports.
 *
 * The catalogue stores real literals and a mask identifying operands with an imaginary component. Workers and
 * reports use the same imaginary literal. Historical reporting uses the saved literals and mask, rather than
 * today's catalogue. Integer orders remain real; no mathematical expression is evaluated while formatting.
 */
#include <string.h>

#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

const char benchmark_imaginary_literal[] = "0.125";

/* Spell one worker operand without changing its authored real component. */
string_t *benchmark_argument_literal(const char *real, bool complex_value, bool imaginary)
{
    if (!real) return NULL;
    return complex_value ? string_sprintf("%s+%si", real, imaginary ? benchmark_imaginary_literal : "0") :
                           string_new_with(real);
}

/* Reconstruct the actual operand values represented by one stored benchmark case. */
string_t *benchmark_arguments_text(const char *arguments, const char *backend, unsigned complex_mask)
{
    if (!arguments || !backend) return NULL;
    if ((!strcmp(backend, "qcomplex") || !strcmp(backend, "mpc")) && complex_mask) {
        string_t *source = string_new_with(arguments), *out = string_new();
        size_t count = 0;
        string_t **parts = source ? string_split(source, ",", &count) : NULL;
        bool ok = out && parts && count > 0 && count <= 6 && (complex_mask >> count) == 0;
        for (size_t i = 0; ok && i < count; ++i) {
            string_trim(parts[i]);
            bool imaginary = (complex_mask & (1u << i)) != 0;
            string_t *value = benchmark_argument_literal(string_c_str(parts[i]), imaginary, imaginary);
            if (i) ok = string_append_cstr(out, ", ") >= 0;
            ok = value && ok && string_append_cstr(out, string_c_str(value)) >= 0;
            string_free(value);
        }
        string_split_free(parts, count);
        string_free(source);
        if (!ok) { string_free(out); return NULL; }
        return out;
    }
    return string_new_with(arguments);
}
