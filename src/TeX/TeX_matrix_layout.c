/**
 * @file TeX_matrix_layout.c
 * @brief Choose compact or separated Cartesian layout for callback-provided scalar matrices.
 *
 * Native providers supply independently owned contexts for whole entries and their real or imaginary components.
 * TeX measures the rendered entries and chooses presentation only; it never parses numeric or mathematical text
 * to recover components. All callback state remains owned by the caller throughout the synchronous call.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

static TeX_t *component_matrix(size_t rows, size_t columns, const TeX_scalar_matrix_provider_t *provider,
                               void *state, TeX_scalar_component_t component, bool *wide, bool *complex)
{
    size_t count = rows * columns;
    TeX_t **entries = calloc(count, sizeof *entries);
    TeX_t *matrix = NULL;
    if (!entries)
        return NULL;
    size_t row_width = 3u * (columns - 1u);
    for (size_t i = 0; i < count; ++i) {
        bool has_imaginary = false;
        TeX_context_t *context = provider->entry(state, i, component, &has_imaginary);
        entries[i] = context ? TeX_from_context(context) : NULL;
        TeX_context_free(context);
        if (!entries[i])
            goto cleanup;
        size_t width = strlen(TeX_source(entries[i]));
        row_width += width;
        *complex |= has_imaginary;
        *wide |= width > 64u;
        if ((i + 1u) % columns == 0u) {
            *wide |= row_width > 90u;
            row_width = 3u * (columns - 1u);
        }
    }
    matrix = TeX_compose_matrix((const TeX_t *const *)entries, rows, columns,
        component == TeX_SCALAR_WHOLE ? TeX_MATRIX_BRACKETS : TeX_MATRIX_PARENTHESES, 10);
cleanup:
    for (size_t i = 0; i < count; ++i)
        TeX_free(entries[i]);
    free(entries);
    return matrix;
}

/* Keep numeric component selection with the provider and matrix layout with TeX. */
TeX_t *TeX_scalar_matrix(size_t rows, size_t columns, const TeX_scalar_matrix_provider_t *provider, void *state)
{
    if (!rows || !columns || rows > SIZE_MAX / columns || rows * columns > 65536u || !provider || !provider->entry)
        return NULL;
    bool wide = false, complex = false;
    TeX_t *whole = component_matrix(rows, columns, provider, state, TeX_SCALAR_WHOLE, &wide, &complex);
    if (!whole || !wide || !complex)
        return whole;
    TeX_t *real = component_matrix(rows, columns, provider, state, TeX_SCALAR_REAL, &wide, &complex);
    TeX_t *imaginary = real ? component_matrix(rows, columns, provider, state, TeX_SCALAR_IMAGINARY, &wide, &complex) : NULL;
    TeX_t *separated = imaginary ? TeX_compose_cartesian_sum(real, imaginary) : NULL;
    TeX_free(imaginary);
    TeX_free(real);
    if (!separated)
        return whole;
    TeX_free(whole);
    return separated;
}
