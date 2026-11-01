/**
 * @file TeX_stack_layout.c
 * @brief Structural multiline, indentation and relation alignment for RPN notation.
 *
 * Retains authored row and relation objects so layout never parses rendered TeX.
 * Lists supply arbitrary column boundaries and spaced, left-aligned array cells.
 * Native mathematics remains opaque;
 * these routines arrange notation without evaluating or rewriting its algebra.
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/* Preserve relation operands for alignment whilst retaining the established spelling. */
TeX_t *TeX_object_relation(const TeX_t *left, const TeX_t *right, TeX_relation_t relation)
{
    TeX_t *selector = TeX_compose_integer(relation);
    const TeX_t *items[] = {left, selector, right};
    TeX_t *rendered = selector ? TeX_compose_relation(left, right, relation) : NULL;
    TeX_t *result = TeX_object_rendered(TeX_OBJECT_RELATION, items, 3, rendered);
    TeX_free(selector);
    return TeX_classify(result, TeX_NODE_RELATION);
}

static const TeX_t *TeX_row_body(const TeX_t *row, unsigned long long *indent)
{
    *indent = 0;
    while (TeX_object_kind(row) == TeX_OBJECT_INDENT) {
        size_t count;
        const TeX_t *const *items = TeX_object_items(row, &count);
        long points;
        if (count != 2 || !TeX_integer_value(items[1], &points) || points < 0)
            return NULL;
        *indent += (unsigned long)points;
        row = items[0];
    }
    return row;
}

static bool TeX_column_count(const TeX_t *const *rows, size_t count, size_t *columns)
{
    size_t width = 0;
    bool explicit = false;
    /* The array preamble needs the widest authored row before any cells are emitted. */
    for (size_t i = 0; i < count; ++i) {
        unsigned long long indent;
        const TeX_t *row = TeX_row_body(rows[i], &indent);
        if (!row)
            return false;
        TeX_object_kind_t kind = TeX_object_kind(row);
        size_t cells = kind == TeX_OBJECT_RELATION ? 2 : 1;
        if (kind == TeX_OBJECT_LIST) {
            TeX_object_items(row, &cells);
            if (!cells)
                return false;
            explicit = true;
        }
        if (cells > width)
            width = cells;
    }
    *columns = explicit ? width : 0;
    return true;
}

static bool TeX_append_row(string_t *source, const TeX_t *row, bool align, bool table)
{
    unsigned long long indent;
    row = TeX_row_body(row, &indent);
    if (!row)
        return false;
    TeX_object_kind_t kind = TeX_object_kind(row);
    size_t count;
    const TeX_t *const *items = TeX_object_items(row, &count);
    bool columns = align && kind == TeX_OBJECT_LIST;
    bool relation = align && kind == TeX_OBJECT_RELATION;
    if ((!columns && !relation && !table && string_append_cstr(source, "&")) ||
        (indent && string_append_format(source, "\\hspace{%llupt}", indent) < 0))
        return false;
    if (columns) {
        if (!count) {
            errno = EINVAL;
            return false;
        }
        /* Every supplied cell contributes once; selection itself is directly indexed. */
        for (size_t i = 0; i < count; ++i)
            if ((i && string_append_cstr(source, " & ")) ||
                string_append_cstr(source, TeX_source(items[i])))
                return false;
        return true;
    }
    if (relation) {
        long selector;
        if (count != 3 || !TeX_integer_value(items[1], &selector))
            return false;
        const char *operator = TeX_relation_operator((TeX_relation_t)selector);
        if (!operator)
            return false;
        if (*operator == ' ')
            ++operator;
        return string_append_format(source, "%s &%s%s", TeX_source(items[0]), operator,
                                     TeX_source(items[2])) >= 0;
    }
    return row && !string_append_cstr(source, TeX_source(row));
}

/* Render row boundaries and alignment markers from structural objects alone. */
TeX_t *TeX_object_lines(const TeX_t *const *rows, size_t count, bool align)
{
    if (!rows || !count) {
        errno = EINVAL;
        return NULL;
    }
    size_t columns = 0;
    if (align && !TeX_column_count(rows, count, &columns)) {
        errno = EINVAL;
        return NULL;
    }
    string_t *source = string_new_with(columns ? "\\begin{array}{" : "\\begin{aligned}");
    if (!source)
        return NULL;
    for (size_t i = 0; i < columns; ++i)
        if (string_append_cstr(source, "l"))
            goto fail;
    if (columns && string_append_cstr(source, "}"))
        goto fail;
    for (size_t i = 0; i < count; ++i)
        if ((i && string_append_cstr(source, " \\\\ ")) || !TeX_append_row(source, rows[i], align, columns != 0))
            goto fail;
    if (string_append_cstr(source, columns ? "\\end{array}" : "\\end{aligned}"))
        goto fail;
    TeX_t *rendered = TeX_compose_raw(string_c_str(source));
    string_free(source);
    return TeX_object_rendered(align ? TeX_OBJECT_ALIGNED : TeX_OBJECT_LINES, rows, count, rendered);
fail:
    string_free(source);
    if (!errno)
        errno = EINVAL;
    return NULL;
}

/* Flatten existing row collections when a later RPN operation adds another line. */
TeX_t *TeX_object_newline(const TeX_t *first, const TeX_t *second)
{
    TeX_object_kind_t a = TeX_object_kind(first), b = TeX_object_kind(second);
    bool a_block = a == TeX_OBJECT_LINES || a == TeX_OBJECT_ALIGNED;
    bool b_block = b == TeX_OBJECT_LINES || b == TeX_OBJECT_ALIGNED;
    size_t a_count = 1, b_count = 1;
    const TeX_t *const *a_rows = a_block ? TeX_object_items(first, &a_count) : &first;
    const TeX_t *const *b_rows = b_block ? TeX_object_items(second, &b_count) : &second;
    if (a_count > SIZE_MAX - b_count || a_count + b_count > SIZE_MAX / sizeof(TeX_t *)) {
        errno = EOVERFLOW;
        return NULL;
    }
    const TeX_t **rows = malloc((a_count + b_count) * sizeof *rows);
    if (!rows)
        return NULL;
    memcpy(rows, a_rows, a_count * sizeof *rows);
    memcpy(rows + a_count, b_rows, b_count * sizeof *rows);
    TeX_t *result = TeX_object_lines(rows, a_count + b_count,
                                    a == TeX_OBJECT_ALIGNED || b == TeX_OBJECT_ALIGNED);
    free(rows);
    return result;
}

/* Indentation retains its numeric operand and original body for later row alignment. */
TeX_t *TeX_object_indent(const TeX_t *body, unsigned spacing)
{
    if (!body) {
        errno = EINVAL;
        return NULL;
    }
    TeX_t *points = TeX_compose_integer(spacing);
    const TeX_t *items[] = {body, points};
    string_t *source = points ? string_sprintf("\\hspace{%upt}%s", spacing, TeX_source(body)) : NULL;
    TeX_t *rendered = source ? TeX_compose_raw(string_c_str(source)) : NULL;
    string_free(source);
    TeX_t *result = TeX_object_rendered(TeX_OBJECT_INDENT, items, 2, rendered);
    TeX_free(points);
    return TeX_classify(result, TeX_kind(body));
}
