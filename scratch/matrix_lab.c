#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "matrix.h"
#include "number.h"
#include "ustring.h"

static matrix_t *matrix_real_copy_if_possible(const matrix_t *matrix)
{
    size_t rows;
    size_t cols;
    size_t count;
    number_t *values;
    matrix_t *real_matrix = NULL;

    if (!matrix || mat_typeof(matrix) != MAT_TYPE_NUMBER)
        return NULL;

    rows = mat_get_row_count(matrix);
    cols = mat_get_col_count(matrix);
    count = rows * cols;
    values = calloc(count, sizeof(*values));
    if (!values)
        return NULL;

    for (size_t i = 0u; i < count; ++i) {
        number_t value = mat_get_num(matrix, i / cols, i % cols);

        if (!num_is_real(value)) {
            num_destroy(&value);
            goto cleanup;
        }
        values[i] = num_real_part(value);
        num_destroy(&value);
    }

    real_matrix = mat_create(rows, cols, values);

cleanup:
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
    return real_matrix;
}

static char *dup_string(const char *text)
{
    size_t n;
    char *copy;

    if (!text)
        text = "";
    n = strlen(text);
    copy = malloc(n + 1u);
    if (copy)
        memcpy(copy, text, n + 1u);
    return copy;
}

static char *dup_trimmed_range(const char *start, const char *end)
{
    char *copy;
    size_t length;

    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    length = (size_t)(end - start);
    copy = malloc(length + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *matrix_algebraic_input(const char *text)
{
    const char *start = text;
    const char *end;
    const char *binding_separator = NULL;
    int parenthesis_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    if (!text)
        return NULL;
    end = text + strlen(text);
    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    if (end - start < 2 || *start != '{' || end[-1] != '}')
        return dup_trimmed_range(start, end);

    for (const char *cursor = start + 1; cursor < end - 1; ++cursor) {
        if (*cursor == '(')
            parenthesis_depth++;
        else if (*cursor == ')')
            parenthesis_depth--;
        else if (*cursor == '[')
            bracket_depth++;
        else if (*cursor == ']')
            bracket_depth--;
        else if (*cursor == '{')
            brace_depth++;
        else if (*cursor == '}')
            brace_depth--;
        else if (*cursor == '|' && parenthesis_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                 memchr(cursor + 1, '=', (size_t)((end - 1) - (cursor + 1))) != NULL)
            binding_separator = cursor;
    }
    return binding_separator ? dup_trimmed_range(start + 1, binding_separator) : dup_trimmed_range(start, end);
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    return expr_to_string(expr, style);
}

static char *number_text_dup(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *number_TeX_dup(number_t value)
{
    expr_t *expr = expr_new_const(num_clone(value));
    char *tex = expr ? expr_text_dup(expr, style_LATEX) : NULL;

    expr_free(expr);
    return tex ? tex : number_text_dup(value);
}

static const char *matrix_type_name(const matrix_t *matrix)
{
    return mat_typeof(matrix) == MAT_TYPE_EXPR ? "expr" : "number";
}

static void replace_same_length(char *text, const char *needle, const char *replacement)
{
    size_t needle_len;
    size_t replacement_len;
    char *pos;

    if (!text || !needle || !replacement)
        return;

    needle_len = strlen(needle);
    replacement_len = strlen(replacement);
    if (needle_len == 0u || needle_len != replacement_len)
        return;

    pos = strstr(text, needle);
    while (pos) {
        memcpy(pos, replacement, replacement_len);
        pos = strstr(pos + replacement_len, needle);
    }
}

static char *concat3(const char *a, const char *b, const char *c)
{
    size_t na = a ? strlen(a) : 0u;
    size_t nb = b ? strlen(b) : 0u;
    size_t nc = c ? strlen(c) : 0u;
    char *out = malloc(na + nb + nc + 1u);

    if (!out)
        return NULL;

    memcpy(out, a ? a : "", na);
    memcpy(out + na, b ? b : "", nb);
    memcpy(out + na + nb, c ? c : "", nc);
    out[na + nb + nc] = '\0';
    return out;
}

static char *matrix_scalar_lhs_TeX(const matrix_t *matrix, const char *operation)
{
    char *matrix_TeX = mat_to_string(matrix, MAT_STRING_LATEX);
    char *lhs;

    if (!matrix_TeX)
        return NULL;

    if (strcmp(operation, "det") == 0) {
        replace_same_length(matrix_TeX, "{bmatrix}", "{vmatrix}");
        return matrix_TeX;
    }

    if (strcmp(operation, "trace") == 0)
        lhs = concat3("\\operatorname{tr}\\!\\left(", matrix_TeX, "\\right)");
    else if (strcmp(operation, "rank") == 0)
        lhs = concat3("\\operatorname{rank}\\!\\left(", matrix_TeX, "\\right)");
    else
        lhs = dup_string(matrix_TeX);

    free(matrix_TeX);
    return lhs;
}

static void print_expr_values(const char *label, expr_t **values, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        char *text = values[i] ? expr_text_dup(values[i], style_EXPRESSION) : NULL;

        printf("%-12s λ%zu = %s\n", label, i + 1u, text ? text : "(null)");
        free(text);
    }
}

static void print_number_values(const char *label, number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        char *text = number_text_dup(values[i]);

        printf("%-12s λ%zu = %s\n", label, i + 1u, text ? text : "(null)");
        free(text);
    }
}

static void print_number_eigenvalues_TeX(number_t *values, size_t count)
{
    printf("tex         \\begin{aligned}");
    printf("\\textbf{eigenvalues}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        char *text = number_text_dup(values[i]);

        printf("\\lambda_{%zu} &= \\text{%s}", i + 1u, text ? text : "null");
        printf(" \\\\ ");
        free(text);
    }
    printf("\\end{aligned}\n");
}

static void print_matrix_fields(const matrix_t *matrix)
{
    char *inline_text = mat_body_to_string(matrix, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_body_to_string(matrix, MAT_STRING_LAYOUT_PRETTY);
    char *TeX_text = mat_body_to_string(matrix, MAT_STRING_LATEX);

    printf("kind        %s\n", matrix_type_name(matrix));
    printf("rows        %zu\n", mat_get_row_count(matrix));
    printf("cols        %zu\n", mat_get_col_count(matrix));
    printf("result      %s\n", inline_text ? inline_text : "(null)");
    printf("pretty      %s\n", pretty_text ? pretty_text : "(null)");
    printf("tex         %s\n", TeX_text ? TeX_text : "(null)");

    free(TeX_text);
    free(pretty_text);
    free(inline_text);
}

static void print_matrix_value_fields(const matrix_t *matrix)
{
    char *inline_text = mat_body_to_string(matrix, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_body_to_string(matrix, MAT_STRING_LAYOUT_PRETTY);
    char *TeX_text = mat_body_to_string(matrix, MAT_STRING_LATEX);

    printf("value       %s\n", inline_text ? inline_text : "(null)");
    printf("value_pretty  %s\n", pretty_text ? pretty_text : "(null)");
    printf("value_tex   %s\n", TeX_text ? TeX_text : "(null)");

    free(TeX_text);
    free(pretty_text);
    free(inline_text);
}

static void print_matrix_bindings(mat_bindings_t *bindings)
{
    size_t count = mat_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        const char *name = mat_bindings_name_at(bindings, i);
        expr_t *binding = mat_bindings_expr_at(bindings, i);
        char *value_text;

        if (!name || !binding)
            continue;
        {
            number_t value = expr_get_val(binding);

            value_text = number_text_dup(value);
            num_destroy(&value);
        }
        printf("%-12s %s\t%s\t%s\n", "binding", mat_bindings_is_constant_at(bindings, i) ? "constant" : "variable", name,
               value_text ? value_text : "(num_to_string failed)");
        free(value_text);
    }
}

static bool matrix_bindings_contains_name(mat_bindings_t *bindings, const char *name)
{
    return name && mat_bindings_get(bindings, name) != NULL;
}

static void print_additional_matrix_bindings(mat_bindings_t *bindings, mat_bindings_t *already_printed)
{
    size_t count = mat_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        const char *name = mat_bindings_name_at(bindings, i);
        expr_t *binding = mat_bindings_expr_at(bindings, i);
        char *value_text;

        if (!name || !binding || matrix_bindings_contains_name(already_printed, name))
            continue;
        {
            number_t value = expr_get_val(binding);

            value_text = number_text_dup(value);
            num_destroy(&value);
        }
        printf("%-12s %s\t%s\t%s\n", "binding", mat_bindings_is_constant_at(bindings, i) ? "constant" : "variable", name,
               value_text ? value_text : "(num_to_string failed)");
        free(value_text);
    }
}

static bool matrix_bindings_are_resolved(mat_bindings_t *bindings)
{
    size_t count = mat_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        expr_t *binding = mat_bindings_expr_at(bindings, i);
        number_t value;
        bool resolved;

        if (!binding)
            return false;
        value = expr_get_val(binding);
        resolved = !num_is_nan(value);
        num_destroy(&value);
        if (!resolved)
            return false;
    }
    return true;
}

static bool matrix_bindings_have_variables(mat_bindings_t *bindings)
{
    size_t count = mat_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        if (!mat_bindings_is_constant_at(bindings, i))
            return true;
    }
    return false;
}

static bool matrix_bindings_have_resolved_values(mat_bindings_t *bindings)
{
    size_t count = mat_bindings_count(bindings);

    for (size_t i = 0u; i < count; ++i) {
        expr_t *binding = mat_bindings_expr_at(bindings, i);
        number_t value;
        bool resolved;

        if (!binding)
            continue;
        value = expr_get_val(binding);
        resolved = !num_is_nan(value);
        num_destroy(&value);
        if (resolved)
            return true;
    }
    return false;
}

static matrix_t *matrix_partially_evaluate(const matrix_t *matrix, mat_bindings_t *bindings)
{
    matrix_t *evaluated;
    size_t rows;
    size_t cols;

    if (!matrix || mat_typeof(matrix) != MAT_TYPE_EXPR)
        return NULL;
    rows = mat_get_row_count(matrix);
    cols = mat_get_col_count(matrix);
    evaluated = mat_new_expr(rows, cols);
    if (!evaluated)
        return NULL;

    for (size_t row = 0u; row < rows; ++row) {
        for (size_t col = 0u; col < cols; ++col) {
            expr_t *entry = NULL;

            mat_get(matrix, row, col, &entry);
            if (!entry)
                goto fail;
            expr_retain(entry);
            for (size_t binding_index = 0u; binding_index < mat_bindings_count(bindings); ++binding_index) {
                expr_t *binding = mat_bindings_expr_at(bindings, binding_index);
                number_t value;

                if (!binding)
                    continue;
                value = expr_get_val(binding);
                if (!num_is_nan(value)) {
                    expr_t *replacement = expr_new_const(num_clone(value));
                    expr_t *substituted = replacement ? expr_substitute(entry, binding, replacement) : NULL;

                    expr_free(replacement);
                    if (!substituted) {
                        num_destroy(&value);
                        expr_free(entry);
                        goto fail;
                    }
                    expr_free(entry);
                    entry = substituted;
                }
                num_destroy(&value);
            }
            {
                expr_t *simplified = expr_display_simplified(entry);

                if (simplified) {
                    expr_free(entry);
                    entry = simplified;
                }
            }
            mat_set(evaluated, row, col, &entry);
            expr_free(entry);
        }
    }
    return evaluated;

fail:
    mat_free(evaluated);
    return NULL;
}

static char *expr_to_unbound_TeX(expr_t *expr)
{
    const char *prefix = "\\left\\{ ";
    const char *middle = " \\;\\middle|\\; ";
    char *tex = expr ? expr_text_dup(expr, style_LATEX) : NULL;
    char *mid;
    size_t prefix_len;
    size_t body_len;
    char *body;

    if (!tex)
        return NULL;

    prefix_len = strlen(prefix);
    mid = strstr(tex, middle);
    if (strncmp(tex, prefix, prefix_len) != 0 || !mid)
        return tex;

    body_len = (size_t)(mid - (tex + prefix_len));
    body = malloc(body_len + 1u);
    if (!body)
        return tex;

    memcpy(body, tex + prefix_len, body_len);
    body[body_len] = '\0';
    free(tex);
    return body;
}

static void print_expr_eigenvalues_TeX(expr_t **values, size_t count)
{
    printf("tex         \\begin{aligned}");
    printf("\\textbf{eigenvalues}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        char *tex = expr_to_unbound_TeX(values[i]);

        printf("\\lambda_{%zu} &= %s", i + 1u, tex ? tex : "\\text{null}");
        printf(" \\\\ ");
        free(tex);
    }
    printf("\\end{aligned}\n");
}

static void print_expr_eigenvector_column(const char *prefix, const matrix_t *eigenvectors, size_t column)
{
    size_t rows = mat_get_row_count(eigenvectors);

    printf("%s(", prefix);
    for (size_t row = 0; row < rows; ++row) {
        const expr_t *entry = NULL;
        char *text;

        mat_get(eigenvectors, row, column, &entry);
        text = entry ? expr_text_dup((expr_t *)entry, style_UNBOUND) : NULL;
        printf("%s%s", row ? "; " : "", text ? text : "(null)");
        free(text);
    }
    printf(")\n");
}

static void print_expr_eigenvector_column_TeX(const matrix_t *eigenvectors, size_t column)
{
    size_t rows = mat_get_row_count(eigenvectors);

    printf("\\left(");
    for (size_t row = 0; row < rows; ++row) {
        const expr_t *entry = NULL;
        char *tex;

        mat_get(eigenvectors, row, column, &entry);
        tex = expr_to_unbound_TeX((expr_t *)entry);
        printf("%s%s", row ? ",\\; " : "", tex ? tex : "\\text{null}");
        free(tex);
    }
    printf("\\right)^{T}");
}

static void print_number_eigenvector_column(const char *prefix, const matrix_t *eigenvectors, size_t column)
{
    size_t rows = mat_get_row_count(eigenvectors);

    printf("%s(", prefix);
    for (size_t row = 0; row < rows; ++row) {
        number_t entry;
        char *text = NULL;

        entry = mat_get_num(eigenvectors, row, column);
        text = number_text_dup(entry);
        printf("%s%s", row ? "; " : "", text ? text : "(null)");
        free(text);
        num_destroy(&entry);
    }
    printf(")\n");
}

static void print_number_eigenvector_column_TeX(const matrix_t *eigenvectors, size_t column)
{
    size_t rows = mat_get_row_count(eigenvectors);

    printf("\\left(");
    for (size_t row = 0; row < rows; ++row) {
        number_t entry;
        char *text;

        entry = mat_get_num(eigenvectors, row, column);
        text = number_text_dup(entry);
        printf("%s\\text{%s}", row ? ",\\; " : "", text ? text : "null");
        free(text);
        num_destroy(&entry);
    }
    printf("\\right)^{T}");
}

static void print_eigendecomposition_expr_fields(expr_t **eigenvalues, size_t count, const matrix_t *eigenvectors)
{
    char *inline_text = mat_to_string(eigenvectors, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(eigenvectors, MAT_STRING_LAYOUT_PRETTY);
    char *TeX_text = mat_to_string(eigenvectors, MAT_STRING_LATEX);

    printf("kind        %s\n", matrix_type_name(eigenvectors));
    printf("rows        %zu\n", mat_get_row_count(eigenvectors));
    printf("cols        %zu\n", mat_get_col_count(eigenvectors));
    printf("result      eigenvalues\n");
    for (size_t i = 0; i < count; ++i) {
        char *text = eigenvalues[i] ? expr_text_dup(eigenvalues[i], style_UNBOUND) : NULL;

        printf("result      λ%zu = %s\n", i + 1u, text ? text : "(null)");
        free(text);
    }
    printf("result      eigenvectors\n");
    for (size_t i = 0; i < count; ++i) {
        printf("result      v%zu = ", i + 1u);
        print_expr_eigenvector_column("", eigenvectors, i);
    }
    printf("result      V = %s\n", inline_text ? inline_text : "(null)");

    printf("pretty      eigenvalues\n");
    for (size_t i = 0; i < count; ++i) {
        char *text = eigenvalues[i] ? expr_text_dup(eigenvalues[i], style_UNBOUND) : NULL;

        printf("  λ%zu = %s\n", i + 1u, text ? text : "(null)");
        free(text);
    }
    printf("eigenvectors\n");
    for (size_t i = 0; i < count; ++i) {
        printf("  v%zu = ", i + 1u);
        print_expr_eigenvector_column("", eigenvectors, i);
    }

    printf("tex         \\begin{aligned}");
    printf("\\textbf{eigenvalues}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        char *tex = expr_to_unbound_TeX(eigenvalues[i]);

        printf("\\lambda_{%zu} &= %s", i + 1u, tex ? tex : "\\text{null}");
        printf(" \\\\ ");
        free(tex);
    }
    printf("\\textbf{eigenvectors}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        printf("v_{%zu} &= ", i + 1u);
        print_expr_eigenvector_column_TeX(eigenvectors, i);
        printf(" \\\\ ");
    }
    printf("\\end{aligned}\n");

    free(TeX_text);
    free(pretty_text);
    free(inline_text);
}

static void print_eigendecomposition_number_fields(number_t *eigenvalues, size_t count, const matrix_t *eigenvectors)
{
    char *inline_text = mat_to_string(eigenvectors, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(eigenvectors, MAT_STRING_LAYOUT_PRETTY);
    char *TeX_text = mat_to_string(eigenvectors, MAT_STRING_LATEX);

    printf("kind        %s\n", matrix_type_name(eigenvectors));
    printf("rows        %zu\n", mat_get_row_count(eigenvectors));
    printf("cols        %zu\n", mat_get_col_count(eigenvectors));
    printf("result      eigenvalues\n");
    for (size_t i = 0; i < count; ++i) {
        char *text = number_text_dup(eigenvalues[i]);

        printf("result      λ%zu = %s\n", i + 1u, text ? text : "(null)");
        free(text);
    }
    printf("result      eigenvectors\n");
    for (size_t i = 0; i < count; ++i) {
        printf("result      v%zu = ", i + 1u);
        print_number_eigenvector_column("", eigenvectors, i);
    }
    printf("result      V = %s\n", inline_text ? inline_text : "(null)");

    printf("pretty      eigenvalues\n");
    for (size_t i = 0; i < count; ++i) {
        char *text = number_text_dup(eigenvalues[i]);

        printf("  λ%zu = %s\n", i + 1u, text ? text : "(null)");
        free(text);
    }
    printf("eigenvectors\n");
    for (size_t i = 0; i < count; ++i) {
        printf("  v%zu = ", i + 1u);
        print_number_eigenvector_column("", eigenvectors, i);
    }

    printf("tex         \\begin{aligned}");
    printf("\\textbf{eigenvalues}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        char *text = number_text_dup(eigenvalues[i]);

        printf("\\lambda_{%zu} &= \\text{%s}", i + 1u, text ? text : "null");
        printf(" \\\\ ");
        free(text);
    }
    printf("\\textbf{eigenvectors}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        printf("v_{%zu} &= ", i + 1u);
        print_number_eigenvector_column_TeX(eigenvectors, i);
        printf(" \\\\ ");
    }
    printf("\\end{aligned}\n");

    free(TeX_text);
    free(pretty_text);
    free(inline_text);
}

static void print_expr_scalar_field(const matrix_t *matrix, const char *operation, const char *label, expr_t *expr)
{
    char *text = expr ? expr_text_dup(expr, style_EXPRESSION) : NULL;
    char *tex = expr ? expr_text_dup(expr, style_LATEX) : NULL;
    char *lhs_TeX = matrix ? matrix_scalar_lhs_TeX(matrix, operation) : NULL;

    printf("%-12s %s\n", label, text ? text : "(null)");
    if (lhs_TeX && tex)
        printf("tex         %s = %s\n", lhs_TeX, tex);
    else
        printf("tex         %s\n", tex ? tex : "(null)");

    free(lhs_TeX);
    free(tex);
    free(text);
    expr_free(expr);
}

static void print_number_scalar_field(const matrix_t *matrix, const char *operation, const char *label, number_t value)
{
    char *text = number_text_dup(value);
    char *tex = number_TeX_dup(value);
    char *lhs_TeX = matrix_scalar_lhs_TeX(matrix, operation);

    printf("%-12s %s\n", label, text ? text : "(null)");
    if (lhs_TeX && tex)
        printf("tex         %s = %s\n", lhs_TeX, tex);
    else if (tex)
        printf("tex         %s\n", tex);

    free(lhs_TeX);
    free(tex);
    free(text);
    num_destroy(&value);
}

static int run_scalar_operation(const matrix_t *matrix, const char *operation)
{
    if (strcmp(operation, "trace") == 0) {
        if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
            expr_t *trace = NULL;

            if (mat_trace_expr(matrix, &trace) != 0 || !trace)
                return 1;
            print_expr_scalar_field(matrix, operation, "value", trace);
            return 0;
        } else {
            number_t trace = num_new();

            if (mat_trace(matrix, &trace) != 0)
                return 1;
            print_number_scalar_field(matrix, operation, "value", trace);
            return 0;
        }
    }

    if (strcmp(operation, "det") == 0) {
        if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
            expr_t *det = NULL;

            if (mat_det_expr(matrix, &det) != 0 || !det)
                return 1;
            print_expr_scalar_field(matrix, operation, "value", det);
            return 0;
        } else {
            number_t det = num_new();

            if (mat_det(matrix, &det) != 0)
                return 1;
            print_number_scalar_field(matrix, operation, "value", det);
            return 0;
        }
    }

    if (strcmp(operation, "rank") == 0) {
        int rank = mat_rank(matrix);
        char *lhs_TeX = matrix_scalar_lhs_TeX(matrix, operation);

        printf("value       %d\n", rank);
        if (lhs_TeX)
            printf("tex         %s = %d\n", lhs_TeX, rank);
        else
            printf("tex         %d\n", rank);
        free(lhs_TeX);
        return 0;
    }

    return 1;
}

static int run_eigenvalue_operation(const matrix_t *matrix)
{
    size_t n = mat_get_row_count(matrix);

    if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
        expr_t **eigenvalues = calloc(n, sizeof(*eigenvalues));
        int rc;

        if (!eigenvalues)
            return 1;
        rc = mat_eigenvalues_expr(matrix, eigenvalues);
        if (rc != 0) {
            free(eigenvalues);
            return 1;
        }
        print_expr_values("value", eigenvalues, n);
        print_expr_eigenvalues_TeX(eigenvalues, n);
        for (size_t i = 0; i < n; ++i)
            expr_free(eigenvalues[i]);
        free(eigenvalues);
        return 0;
    } else {
        number_t *eigenvalues = calloc(n, sizeof(*eigenvalues));
        int rc;

        if (!eigenvalues)
            return 1;
        for (size_t i = 0; i < n; ++i)
            eigenvalues[i] = num_new();
        rc = mat_eigenvalues(matrix, eigenvalues);
        if (rc != 0) {
            for (size_t i = 0; i < n; ++i)
                num_destroy(&eigenvalues[i]);
            free(eigenvalues);
            return 1;
        }
        print_number_values("value", eigenvalues, n);
        print_number_eigenvalues_TeX(eigenvalues, n);
        for (size_t i = 0; i < n; ++i)
            num_destroy(&eigenvalues[i]);
        free(eigenvalues);
        return 0;
    }
}

static int run_eigendecompose_operation(const matrix_t *matrix)
{
    size_t n = mat_get_row_count(matrix);
    matrix_t *eigenvectors = NULL;

    if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
        expr_t **eigenvalues = calloc(n, sizeof(*eigenvalues));
        int rc;

        if (!eigenvalues)
            return 1;
        rc = mat_eigendecompose_expr(matrix, eigenvalues, &eigenvectors);
        if (rc != 0 || !eigenvectors) {
            mat_free(eigenvectors);
            for (size_t i = 0; i < n; ++i)
                expr_free(eigenvalues[i]);
            free(eigenvalues);
            return 1;
        }
        print_eigendecomposition_expr_fields(eigenvalues, n, eigenvectors);
        mat_free(eigenvectors);
        for (size_t i = 0; i < n; ++i)
            expr_free(eigenvalues[i]);
        free(eigenvalues);
        return 0;
    } else {
        number_t *eigenvalues = calloc(n, sizeof(*eigenvalues));
        int rc;

        if (!eigenvalues)
            return 1;
        for (size_t i = 0; i < n; ++i)
            eigenvalues[i] = num_new();
        rc = mat_eigendecompose(matrix, eigenvalues, &eigenvectors);
        if (rc != 0 || !eigenvectors) {
            mat_free(eigenvectors);
            for (size_t i = 0; i < n; ++i)
                num_destroy(&eigenvalues[i]);
            free(eigenvalues);
            return 1;
        }
        print_eigendecomposition_number_fields(eigenvalues, n, eigenvectors);
        mat_free(eigenvectors);
        for (size_t i = 0; i < n; ++i)
            num_destroy(&eigenvalues[i]);
        free(eigenvalues);
        return 0;
    }
}

int main(int argc, char **argv)
{
    const char *input = argc > 1 ? argv[1] : "(1, 2; 3, 4)";
    const char *operation = argc > 2 ? argv[2] : "eval";
    const char *parsed_operation = NULL;
    int precision = argc > 3 ? atoi(argv[3]) : 64;
    const char *operand = argc > 4 ? argv[4] : NULL;
    mat_bindings_t *bindings = NULL;
    mat_bindings_t *algebraic_bindings = NULL;
    mat_bindings_t *result_bindings = NULL;
    matrix_t *matrix = NULL;
    matrix_t *bound_matrix = NULL;
    matrix_t *other = NULL;
    matrix_t *result = NULL;
    expr_t *scalar_result = NULL;
    expr_t *bound_scalar_result = NULL;
    char *algebraic_input = NULL;
    bool bound_matrix_is_result = false;
    int rc = 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    if (mat_expression_evaluate(input, &bindings, &parsed_operation, &matrix, &scalar_result) != 0) {
        if (parsed_operation)
            operation = parsed_operation;
        printf("input       %s\n", input);
        printf("operation   %s\n", operation);
        fprintf(stderr, "Could not parse matrix input\n");
        goto cleanup;
    }

    if (matrix_bindings_have_resolved_values(bindings)) {
        const char *algebraic_operation = NULL;
        matrix_t *algebraic_matrix = NULL;
        expr_t *algebraic_scalar = NULL;

        algebraic_input = matrix_algebraic_input(input);
        if (algebraic_input && strcmp(algebraic_input, input) != 0 &&
            mat_expression_evaluate(algebraic_input, &algebraic_bindings, &algebraic_operation, &algebraic_matrix,
                                    &algebraic_scalar) == 0 &&
            ((matrix && algebraic_matrix) || (scalar_result && algebraic_scalar))) {
            bound_matrix = matrix;
            matrix = algebraic_matrix;
            algebraic_matrix = NULL;
            bound_scalar_result = scalar_result;
            scalar_result = algebraic_scalar;
            algebraic_scalar = NULL;
        }
        mat_free(algebraic_matrix);
        expr_free(algebraic_scalar);
    }

    if (scalar_result) {
        number_t evaluated_value = num_clone(NUM_NAN);

        operation = parsed_operation ? parsed_operation : operation;
        printf("input       %s\n", input);
        printf("operation   %s\n", operation);
        if (matrix_bindings_are_resolved(bindings))
            evaluated_value = expr_eval(bound_scalar_result ? bound_scalar_result : scalar_result);
        print_expr_scalar_field(NULL, operation, "result", scalar_result);
        scalar_result = NULL;
        if (!num_is_nan(evaluated_value)) {
            char *value_text = number_text_dup(evaluated_value);

            printf("value       %s\n", value_text ? value_text : "(null)");
            free(value_text);
        }
        num_destroy(&evaluated_value);
        print_matrix_bindings(bindings);
        rc = 0;
        goto cleanup;
    }

    if (parsed_operation) {
        operation = parsed_operation;
        printf("input       %s\n", input);
        printf("operation   %s\n", operation);
        result = matrix;
        matrix = NULL;
        bound_matrix_is_result = bound_matrix != NULL;
        goto result_ready;
    }

    printf("input       %s\n", input);
    printf("operation   %s\n", operation);

    if (strcmp(operation, "eval") == 0) {
        if (bound_matrix) {
            result = matrix;
            matrix = NULL;
            bound_matrix_is_result = true;
        } else if (mat_typeof(matrix) == MAT_TYPE_EXPR &&
            (!matrix_bindings_are_resolved(bindings) || !matrix_bindings_have_variables(bindings))) {
            result = matrix;
            matrix = NULL;
        } else {
            result = mat_evaluate(matrix);
        }
    } else if (strcmp(operation, "simplify") == 0) {
        result = mat_simplify_symbolic(matrix);
    } else if (strcmp(operation, "inverse") == 0) {
        if (mat_get_row_count(matrix) != mat_get_col_count(matrix)) {
            fprintf(stderr, "Matrix inverse requires a square matrix; received %zux%zu\n", mat_get_row_count(matrix),
                    mat_get_col_count(matrix));
            goto cleanup;
        }
        result = mat_inverse(matrix);
    } else if (strcmp(operation, "charpoly") == 0) {
        result = mat_charpoly(matrix);
    } else if (strcmp(operation, "solve") == 0 || strcmp(operation, "multiply") == 0) {
        if (!operand) {
            fprintf(stderr, "%s needs a right-hand-side matrix\n", strcmp(operation, "solve") == 0 ? "Solve" : "Multiply");
            goto cleanup;
        }
        other = mat_expression_from_string(operand, NULL, NULL);
        if (!other) {
            fprintf(stderr, "Could not parse %s operand\n", operation);
            goto cleanup;
        }
        printf("operand     %s\n", operand);
        if (strcmp(operation, "multiply") == 0 && mat_get_col_count(matrix) != mat_get_row_count(other)) {
            fprintf(stderr, "Matrix multiplication requires matching inner dimensions; received %zux%zu and %zux%zu\n",
                    mat_get_row_count(matrix), mat_get_col_count(matrix), mat_get_row_count(other), mat_get_col_count(other));
            goto cleanup;
        }
        result = strcmp(operation, "solve") == 0 ? mat_solve(matrix, other) : mat_mul(matrix, other);
    } else if (strcmp(operation, "trace") == 0 || strcmp(operation, "det") == 0 || strcmp(operation, "rank") == 0) {
        rc = run_scalar_operation(matrix, operation);
        goto cleanup;
    } else if (strcmp(operation, "eigenvalues") == 0) {
        rc = run_eigenvalue_operation(matrix);
        goto cleanup;
    } else if (strcmp(operation, "eigendecompose") == 0) {
        rc = run_eigendecompose_operation(matrix);
        goto cleanup;
    } else {
        fprintf(stderr, "Unknown matrix operation: %s\n", operation);
        goto cleanup;
    }

result_ready:
    if (!result) {
        fprintf(stderr, "Matrix operation failed\n");
        goto cleanup;
    }

    if (mat_typeof(result) == MAT_TYPE_NUMBER) {
        matrix_t *real_result = matrix_real_copy_if_possible(result);

        if (real_result) {
            mat_free(result);
            result = real_result;
        }
    }

    result_bindings = mat_bindings_from_matrix(result);
    print_matrix_fields(result);
    if (bound_matrix_is_result) {
        matrix_t *value_matrix = mat_typeof(bound_matrix) == MAT_TYPE_EXPR
                                     ? matrix_partially_evaluate(bound_matrix, bindings)
                                     : NULL;

        print_matrix_value_fields(value_matrix ? value_matrix : bound_matrix);
        mat_free(value_matrix);
    }
    if (bound_matrix_is_result) {
        print_matrix_bindings(bindings);
        print_additional_matrix_bindings(result_bindings, bindings);
    } else {
        print_matrix_bindings(result_bindings);
        print_additional_matrix_bindings(bindings, result_bindings);
    }
    rc = 0;

cleanup:
    mat_free(result);
    mat_free(other);
    mat_free(matrix);
    mat_free(bound_matrix);
    mat_bindings_free(algebraic_bindings);
    mat_bindings_free(result_bindings);
    mat_bindings_free(bindings);
    expr_free(scalar_result);
    expr_free(bound_scalar_result);
    free(algebraic_input);
    return rc;
}
