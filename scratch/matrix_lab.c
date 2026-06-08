#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "matrix.h"
#include "number.h"
#include "ustring.h"

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

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    string_t *text = expr_to_text(expr, style);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *number_text_dup(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static const char *matrix_type_name(const matrix_t *matrix)
{
    return mat_typeof(matrix) == MAT_TYPE_EXPR ? "expr" : "number";
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

static void print_number_eigenvalues_tex(number_t *values, size_t count)
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
    char *inline_text = mat_to_string(matrix, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(matrix, MAT_STRING_LAYOUT_PRETTY);
    char *tex_text = mat_to_string(matrix, MAT_STRING_TEX);

    printf("kind        %s\n", matrix_type_name(matrix));
    printf("rows        %zu\n", mat_get_row_count(matrix));
    printf("cols        %zu\n", mat_get_col_count(matrix));
    printf("result      %s\n", inline_text ? inline_text : "(null)");
    printf("pretty      %s\n", pretty_text ? pretty_text : "(null)");
    printf("tex         %s\n", tex_text ? tex_text : "(null)");

    free(tex_text);
    free(pretty_text);
    free(inline_text);
}

static char *expr_to_unbound_tex(expr_t *expr)
{
    const char *prefix = "\\left\\{ ";
    const char *middle = " \\;\\middle|\\; ";
    char *tex = expr ? expr_text_dup(expr, style_TEX) : NULL;
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

static void print_expr_eigenvalues_tex(expr_t **values, size_t count)
{
    printf("tex         \\begin{aligned}");
    printf("\\textbf{eigenvalues}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        char *tex = expr_to_unbound_tex(values[i]);

        printf("\\lambda_{%zu} &= %s", i + 1u, tex ? tex : "\\text{null}");
        printf(" \\\\ ");
        free(tex);
    }
    printf("\\end{aligned}\n");
}

static void print_expr_eigenvector_column(const char *prefix,
                                          const matrix_t *eigenvectors,
                                          size_t column)
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

static void print_expr_eigenvector_column_tex(const matrix_t *eigenvectors,
                                              size_t column)
{
    size_t rows = mat_get_row_count(eigenvectors);

    printf("\\left(");
    for (size_t row = 0; row < rows; ++row) {
        const expr_t *entry = NULL;
        char *tex;

        mat_get(eigenvectors, row, column, &entry);
        tex = expr_to_unbound_tex((expr_t *)entry);
        printf("%s%s", row ? ",\\; " : "", tex ? tex : "\\text{null}");
        free(tex);
    }
    printf("\\right)^{T}");
}

static void print_number_eigenvector_column(const char *prefix,
                                            const matrix_t *eigenvectors,
                                            size_t column)
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

static void print_number_eigenvector_column_tex(const matrix_t *eigenvectors,
                                                size_t column)
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

static void print_eigendecomposition_expr_fields(expr_t **eigenvalues,
                                                 size_t count,
                                                 const matrix_t *eigenvectors)
{
    char *inline_text = mat_to_string(eigenvectors, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(eigenvectors, MAT_STRING_LAYOUT_PRETTY);
    char *tex_text = mat_to_string(eigenvectors, MAT_STRING_TEX);

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
        char *tex = expr_to_unbound_tex(eigenvalues[i]);

        printf("\\lambda_{%zu} &= %s", i + 1u, tex ? tex : "\\text{null}");
        printf(" \\\\ ");
        free(tex);
    }
    printf("\\textbf{eigenvectors}\\quad&\\\\ ");
    for (size_t i = 0; i < count; ++i) {
        printf("v_{%zu} &= ", i + 1u);
        print_expr_eigenvector_column_tex(eigenvectors, i);
        printf(" \\\\ ");
    }
    printf("\\end{aligned}\n");

    free(tex_text);
    free(pretty_text);
    free(inline_text);
}

static void print_eigendecomposition_number_fields(number_t *eigenvalues,
                                                   size_t count,
                                                   const matrix_t *eigenvectors)
{
    char *inline_text = mat_to_string(eigenvectors, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(eigenvectors, MAT_STRING_LAYOUT_PRETTY);
    char *tex_text = mat_to_string(eigenvectors, MAT_STRING_TEX);

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
        print_number_eigenvector_column_tex(eigenvectors, i);
        printf(" \\\\ ");
    }
    printf("\\end{aligned}\n");

    free(tex_text);
    free(pretty_text);
    free(inline_text);
}

static void print_expr_field(const char *label, expr_t *expr)
{
    char *text = expr ? expr_text_dup(expr, style_EXPRESSION) : NULL;
    char *tex = expr ? expr_text_dup(expr, style_TEX) : NULL;

    printf("%-12s %s\n", label, text ? text : "(null)");
    printf("tex         %s\n", tex ? tex : "(null)");

    free(tex);
    free(text);
    expr_free(expr);
}

static void print_number_field(const char *label, number_t value)
{
    char *text = number_text_dup(value);

    printf("%-12s %s\n", label, text ? text : "(null)");

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
            print_expr_field("value", trace);
            return 0;
        } else {
            number_t trace = num_new();

            if (mat_trace(matrix, &trace) != 0)
                return 1;
            print_number_field("value", trace);
            return 0;
        }
    }

    if (strcmp(operation, "det") == 0) {
        if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
            expr_t *det = NULL;

            if (mat_det_expr(matrix, &det) != 0 || !det)
                return 1;
            print_expr_field("value", det);
            return 0;
        } else {
            number_t det = num_new();

            if (mat_det(matrix, &det) != 0)
                return 1;
            print_number_field("value", det);
            return 0;
        }
    }

    if (strcmp(operation, "rank") == 0) {
        int rank = mat_rank(matrix);

        printf("value       %d\n", rank);
        printf("tex         %d\n", rank);
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
        print_expr_eigenvalues_tex(eigenvalues, n);
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
        print_number_eigenvalues_tex(eigenvalues, n);
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
    int precision = argc > 3 ? atoi(argv[3]) : 64;
    const char *operand = argc > 4 ? argv[4] : NULL;
    mat_bindings_t *bindings = NULL;
    matrix_t *matrix = NULL;
    matrix_t *other = NULL;
    matrix_t *result = NULL;
    int rc = 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    matrix = mat_from_string_expr(input, &bindings);
    if (!matrix) {
        fprintf(stderr, "Could not parse matrix input\n");
        goto cleanup;
    }

    printf("input       %s\n", input);
    printf("operation   %s\n", operation);

    if (strcmp(operation, "eval") == 0) {
        result = mat_evaluate(matrix);
    } else if (strcmp(operation, "simplify") == 0) {
        result = mat_simplify_symbolic(matrix);
    } else if (strcmp(operation, "inverse") == 0) {
        result = mat_inverse(matrix);
    } else if (strcmp(operation, "charpoly") == 0) {
        result = mat_charpoly(matrix);
    } else if (strcmp(operation, "solve") == 0) {
        if (!operand) {
            fprintf(stderr, "Solve needs a right-hand-side matrix\n");
            goto cleanup;
        }
        other = mat_from_string_expr(operand, NULL);
        if (!other) {
            fprintf(stderr, "Could not parse solve operand\n");
            goto cleanup;
        }
        printf("operand     %s\n", operand);
        result = mat_solve(matrix, other);
    } else if (strcmp(operation, "trace") == 0 ||
               strcmp(operation, "det") == 0 ||
               strcmp(operation, "rank") == 0) {
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

    if (!result) {
        fprintf(stderr, "Matrix operation failed\n");
        goto cleanup;
    }

    print_matrix_fields(result);
    rc = 0;

cleanup:
    mat_free(result);
    mat_free(other);
    mat_free(matrix);
    mat_bindings_free(bindings);
    return rc;
}
