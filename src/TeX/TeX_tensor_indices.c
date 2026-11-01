/**
 * @file TeX_tensor_indices.c
 * @brief Structural component indices and comma or semicolon derivative annotations.
 *
 * Expands authored index groups in their supplied order and joins derivative
 * labels to an adjacent script of the same variance. The resulting slots use
 * the common tensor compositor. No generated TeX is parsed, no connection is
 * selected, and no derivative or contraction is evaluated.
 */
#include <errno.h>

#include "array-t.h"
#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/* Keep punctuation separate from its owned label or ordered derivative group. */
TeX_t *TeX_object_derivative_index(const TeX_t *label, bool covariant)
{
    TeX_object_kind_t kind = TeX_object_kind(label);
    if (kind == TeX_OBJECT_COMMA || kind == TeX_OBJECT_SEMICOLON) {
        size_t count;
        label = TeX_object_items(label, &count)[0];
    }
    size_t count;
    TeX_object_items(label, &count);
    if (!label || (TeX_object_kind(label) == TeX_OBJECT_LIST && !count)) {
        errno = EINVAL;
        return NULL;
    }
    string_t *source = string_sprintf("%c%s", covariant ? ';' : ',', TeX_source(label));
    TeX_t *rendered = source ? TeX_compose_raw(string_c_str(source)) : NULL;
    string_free(source);
    return TeX_object_rendered(covariant ? TeX_OBJECT_SEMICOLON : TeX_OBJECT_COMMA, &label, 1, rendered);
}

typedef struct {
    TeX_index_t index; /**< Borrowed original label and its selected placement. */
    string_t *joined;  /**< Owned script buffer, allocated only when punctuation or joining is needed. */
    TeX_t *rendered;   /**< Owned final notation for joined, rather than original, labels. */
} TeX_index_buffer_t;

static void TeX_release_index_buffer(void *entry)
{
    TeX_index_buffer_t *buffer = entry;
    string_free(buffer->joined);
    TeX_free(buffer->rendered);
}

static bool TeX_append_index(array_t *slots, const TeX_t *label, TeX_index_position_t position,
                             char punctuation, bool continuation)
{
    size_t count = array_size(slots);
    TeX_index_buffer_t *previous = count ? array_get(slots, count - 1) : NULL;
    const char separator[] = {punctuation, '\0'};
    bool merge = previous && previous->index.position == position && (punctuation || continuation);
    const char *prefix = punctuation ? separator : merge ? "\\," : "";
    if (merge) {
        if (!previous->joined)
            previous->joined = string_new_with(TeX_source(previous->index.value));
        return previous->joined && !string_append_cstr(previous->joined, prefix) &&
               !string_append_cstr(previous->joined, TeX_source(label));
    }
    TeX_index_buffer_t index = {{label, position}, NULL, NULL};
    if (punctuation) {
        index.joined = string_sprintf("%s%s", prefix, TeX_source(label));
        if (!index.joined)
            return false;
    }
    if (array_append_carray(slots, &index, 1))
        return true;
    TeX_release_index_buffer(&index);
    return false;
}

static bool TeX_collect_index(array_t *slots, const TeX_t *label, int variance,
                              char punctuation, bool continuation)
{
    TeX_object_kind_t kind = TeX_object_kind(label);
    bool derivative = punctuation || continuation;
    /* Unwrap bounded structural marks; outer placement and punctuation take precedence. */
    while (kind == TeX_OBJECT_UPPER || kind == TeX_OBJECT_LOWER ||
           kind == TeX_OBJECT_COMMA || kind == TeX_OBJECT_SEMICOLON) {
        if ((kind == TeX_OBJECT_UPPER || kind == TeX_OBJECT_LOWER) && variance < 0)
            variance = kind == TeX_OBJECT_UPPER ? TeX_INDEX_UPPER : TeX_INDEX_LOWER;
        if (kind == TeX_OBJECT_COMMA || kind == TeX_OBJECT_SEMICOLON) {
            if (!punctuation)
                punctuation = kind == TeX_OBJECT_COMMA ? ',' : ';';
            derivative = true;
        }
        size_t count;
        label = TeX_object_items(label, &count)[0];
        kind = TeX_object_kind(label);
    }
    if (derivative && kind == TeX_OBJECT_LIST) {
        size_t count;
        const TeX_t *const *members = TeX_object_items(label, &count);
        if (!count) {
            errno = EINVAL;
            return false;
        }
        /* Each derivative direction contributes once; its authored order is never changed. */
        for (size_t i = 0; i < count; ++i)
            if (!TeX_collect_index(slots, members[i], variance, i ? '\0' : punctuation, i || continuation))
                return false;
        return true;
    }
    return TeX_append_index(slots, label, variance < 0 ? TeX_INDEX_LOWER : (TeX_index_position_t)variance,
                             punctuation, continuation);
}

static TeX_t *TeX_render_index_buffers(const TeX_t *base, array_t *buffers)
{
    array_t *indices = array_create(sizeof(TeX_index_t), NULL, NULL);
    if (!indices)
        return NULL;
    size_t count = array_size(buffers);
    for (size_t i = 0; i < count; ++i) {
        TeX_index_buffer_t *buffer = array_get(buffers, i);
        if (buffer->joined) {
            buffer->rendered = TeX_compose_raw(string_c_str(buffer->joined));
            if (!buffer->rendered)
                goto fail;
        }
        const TeX_index_t index = {buffer->rendered ? buffer->rendered : buffer->index.value,
                                   buffer->index.position};
        if (!array_append_carray(indices, &index, 1))
            goto fail;
    }
    TeX_t *result = TeX_compose_tensor(base, count ? array_get(indices, 0) : NULL, count);
    array_destroy(indices);
    return result;
fail:
    array_destroy(indices);
    return NULL;
}

/* Prepare script buffers without copying original labels or repeatedly copying growing derivative groups. */
TeX_t *TeX_compose_tensor_indices(const TeX_t *base, const TeX_t *list)
{
    if (!base || TeX_object_kind(list) != TeX_OBJECT_LIST) {
        errno = EINVAL;
        return NULL;
    }
    size_t count;
    const TeX_t *const *members = TeX_object_items(list, &count);
    array_t *slots = array_create(sizeof(TeX_index_buffer_t), NULL, TeX_release_index_buffer);
    if (!slots)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if (!TeX_collect_index(slots, members[i], -1, '\0', false)) {
            array_destroy(slots);
            return NULL;
        }
    TeX_t *result = TeX_render_index_buffers(base, slots);
    array_destroy(slots);
    return TeX_classify(result, TeX_NODE_IDENTIFIER);
}
