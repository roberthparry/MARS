#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "matrix.h"
#include "number.h"
#include "ustring.h"

typedef matrix_t *(*matrix_unary_function_t)(const matrix_t *matrix);

typedef struct {
    const char *operation;
    matrix_unary_function_t function;
} matrix_unary_operation_t;

static const matrix_unary_operation_t matrix_unary_operations[] = {
    {"exp",           mat_exp},
    {"log",           mat_log},
    {"log10",         mat_log10},
    {"sqrt",          mat_sqrt},
    {"sin",           mat_sin},
    {"cos",           mat_cos},
    {"tan",           mat_tan},
    {"asin",          mat_asin},
    {"acos",          mat_acos},
    {"atan",          mat_atan},
    {"sinh",          mat_sinh},
    {"cosh",          mat_cosh},
    {"tanh",          mat_tanh},
    {"asinh",         mat_asinh},
    {"acosh",         mat_acosh},
    {"atanh",         mat_atanh},
    {"erf",           mat_erf},
    {"erfc",          mat_erfc},
    {"erfinv",        mat_erfinv},
    {"erfcinv",       mat_erfcinv},
    {"gamma",         mat_gamma},
    {"lgamma",        mat_lgamma},
    {"digamma",       mat_digamma},
    {"trigamma",      mat_trigamma},
    {"tetragamma",    mat_tetragamma},
    {"gammainv",      mat_gammainv},
    {"normal_pdf",    mat_normal_pdf},
    {"normal_cdf",    mat_normal_cdf},
    {"normal_logpdf", mat_normal_logpdf},
    {"lambert_w0",    mat_lambert_w0},
    {"lambert_wm1",   mat_lambert_wm1},
    {"productlog",    mat_productlog},
    {"ei",            mat_ei},
    {"e1",            mat_e1},
};

static matrix_unary_function_t matrix_unary_function_for(const char *operation)
{
    for (size_t i = 0u; i < sizeof(matrix_unary_operations) / sizeof(matrix_unary_operations[0]); ++i) {
        if (strcmp(operation, matrix_unary_operations[i].operation) == 0)
            return matrix_unary_operations[i].function;
    }
    return NULL;
}

static char *duplicate_range(const char *start, const char *end)
{
    size_t length;
    char *copy;

    if (!start || !end || end < start)
        return NULL;

    length = (size_t)(end - start);
    copy = malloc(length + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *parenthesised_matrix_literal(const char *start, const char *end)
{
    size_t length = (size_t)(end - start);
    char *matrix_text = malloc(length + 3u);

    if (!matrix_text)
        return NULL;
    matrix_text[0] = '(';
    memcpy(matrix_text + 1u, start, length);
    matrix_text[length + 1u] = ')';
    matrix_text[length + 2u] = '\0';
    return matrix_text;
}

static char *matrix_literal_text(const char *start, const char *end)
{
    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    if (start == end)
        return NULL;

    if (*start == '(' || *start == '[' || *start == '{')
        return duplicate_range(start, end);
    return parenthesised_matrix_literal(start, end);
}

static bool direct_unary_operation_name(const char *start, const char *open, const char **operation_out)
{
    size_t input_length = (size_t)(open - start);

    if (input_length == strlen("inverse") && strncmp(start, "inverse", input_length) == 0) {
        *operation_out = "inverse";
        return true;
    }

    for (size_t i = 0u; i < sizeof(matrix_unary_operations) / sizeof(matrix_unary_operations[0]); ++i) {
        size_t name_length = strlen(matrix_unary_operations[i].operation);

        if (input_length == name_length && strncmp(start, matrix_unary_operations[i].operation, name_length) == 0) {
            *operation_out = matrix_unary_operations[i].operation;
            return true;
        }
    }
    return false;
}

static bool direct_matrix_calculus_name(const char *start, const char *open, bool *integrate_out, char *variable,
                                        size_t variable_size)
{
    const char *variable_start;
    size_t variable_length;

    if (!start || !open || !integrate_out || !variable || variable_size == 0u)
        return false;

    if (open - start > 1 && *start == 'D') {
        *integrate_out = false;
        variable_start = start + 1;
    } else if (open - start > 3 && strncmp(start, "@S^", 3u) == 0) {
        *integrate_out = true;
        variable_start = start + 3;
    } else {
        return false;
    }

    variable_length = (size_t)(open - variable_start);
    if (variable_length == 0u || variable_length >= variable_size)
        return false;
    memcpy(variable, variable_start, variable_length);
    variable[variable_length] = '\0';
    return true;
}

static void trim_matrix_expression_span(const char **start, const char **end)
{
    while (*start < *end && isspace((unsigned char)**start))
        (*start)++;
    while (*end > *start && isspace((unsigned char)(*end)[-1]))
        (*end)--;
}

static const char *matrix_expression_matching_parenthesis(const char *open, const char *end)
{
    int depth = 0;

    if (!open || open >= end || *open != '(')
        return NULL;

    for (const char *cursor = open; cursor < end; ++cursor) {
        if (*cursor == '(')
            depth++;
        else if (*cursor == ')') {
            depth--;
            if (depth == 0)
                return cursor;
            if (depth < 0)
                return NULL;
        }
    }
    return NULL;
}

static const char *matrix_expression_product_operator(const char *start, const char *end)
{
    const char *product = NULL;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    for (const char *cursor = start; cursor < end; ++cursor) {
        unsigned char c = (unsigned char)*cursor;

        if (c == '(')
            paren_depth++;
        else if (c == ')')
            paren_depth--;
        else if (c == '[')
            bracket_depth++;
        else if (c == ']')
            bracket_depth--;
        else if (c == '{')
            brace_depth++;
        else if (c == '}')
            brace_depth--;
        else if (c == '.' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            bool decimal_point = cursor > start && cursor + 1 < end && isdigit((unsigned char)cursor[-1]) &&
                                 isdigit((unsigned char)cursor[1]);

            if (!decimal_point)
                product = cursor;
        }
    }
    return product;
}

static matrix_t *parse_matrix_span(const char *start, const char *end, mat_bindings_t **bindings)
{
    char *matrix_text = matrix_literal_text(start, end);
    matrix_t *matrix;

    if (!matrix_text)
        return NULL;
    matrix = mat_from_string_expr(matrix_text, bindings);
    free(matrix_text);
    return matrix;
}

static matrix_t *evaluate_matrix_calculus_literal(const char *start, const char *end, const char *variable, bool integrate)
{
    char *matrix_text = matrix_literal_text(start, end);
    mat_bindings_t *bindings = NULL;
    matrix_t *matrix = NULL;
    matrix_t *result = NULL;
    expr_t *temporary_variable = NULL;
    expr_t *wrt;

    if (!matrix_text)
        return NULL;
    matrix = mat_from_string_expr(matrix_text, &bindings);
    free(matrix_text);
    if (!matrix)
        goto cleanup;

    wrt = mat_bindings_get(bindings, variable);
    if (!wrt) {
        temporary_variable = expr_new_named_var(NUM_ZERO, variable);
        wrt = temporary_variable;
    }
    if (wrt)
        result = integrate ? mat_integrate(matrix, wrt) : mat_deriv(matrix, wrt);

cleanup:
    expr_free(temporary_variable);
    mat_bindings_free(bindings);
    mat_free(matrix);
    return result;
}

static matrix_t *evaluate_matrix_expression_span(const char *start, const char *end, bool allow_literal,
                                                 const char **operation_out, bool *operation_matched)
{
    const char *product;
    const char *open;
    const char *close;
    const char *unary_operation = NULL;
    char calculus_variable[128];
    bool integrate = false;
    matrix_t *left = NULL;
    matrix_t *right = NULL;
    matrix_t *result = NULL;

    trim_matrix_expression_span(&start, &end);
    if (operation_matched)
        *operation_matched = false;
    if (start == end)
        return NULL;

    product = matrix_expression_product_operator(start, end);
    if (product) {
        if (operation_matched)
            *operation_matched = true;
        if (operation_out)
            *operation_out = "multiply";

        left = evaluate_matrix_expression_span(start, product, true, NULL, NULL);
        right = evaluate_matrix_expression_span(product + 1, end, true, NULL, NULL);
        if (!left || !right) {
            fprintf(stderr, "Could not parse matrix multiplication operand\n");
            goto cleanup;
        }
        if (mat_get_col_count(left) != mat_get_row_count(right)) {
            fprintf(stderr, "Matrix multiplication requires matching inner dimensions; received %zux%zu and %zux%zu\n",
                    mat_get_row_count(left), mat_get_col_count(left), mat_get_row_count(right), mat_get_col_count(right));
            goto cleanup;
        }
        result = mat_mul(left, right);
        goto cleanup;
    }

    open = memchr(start, '(', (size_t)(end - start));
    if (open && direct_matrix_calculus_name(start, open, &integrate, calculus_variable, sizeof(calculus_variable))) {
        close = matrix_expression_matching_parenthesis(open, end);
        if (!close || close + 1 != end) {
            if (operation_matched)
                *operation_matched = true;
            fprintf(stderr, "Could not parse matrix calculus input\n");
            return NULL;
        }
        if (operation_matched)
            *operation_matched = true;
        if (operation_out)
            *operation_out = "eval";
        result = evaluate_matrix_calculus_literal(open + 1, close, calculus_variable, integrate);
        if (!result)
            fprintf(stderr, "Could not %s matrix entries with respect to %s\n", integrate ? "integrate" : "differentiate",
                    calculus_variable);
        return result;
    }

    if (open && direct_unary_operation_name(start, open, &unary_operation)) {
        close = matrix_expression_matching_parenthesis(open, end);
        if (!close || close + 1 != end) {
            if (operation_matched)
                *operation_matched = true;
            fprintf(stderr, "Could not parse matrix function input\n");
            return NULL;
        }
        if (operation_matched)
            *operation_matched = true;
        if (operation_out)
            *operation_out = unary_operation;

        left = evaluate_matrix_expression_span(open + 1, close, true, NULL, NULL);
        if (!left) {
            fprintf(stderr, "Could not parse matrix function argument\n");
            goto cleanup;
        }
        if (mat_get_row_count(left) != mat_get_col_count(left)) {
            fprintf(stderr, "Matrix function %s requires a square matrix; received %zux%zu\n", unary_operation,
                    mat_get_row_count(left), mat_get_col_count(left));
            goto cleanup;
        }
        if (strcmp(unary_operation, "inverse") == 0)
            result = mat_inverse(left);
        else {
            matrix_unary_function_t function = matrix_unary_function_for(unary_operation);

            result = function ? function(left) : NULL;
        }
        goto cleanup;
    }

    if (allow_literal)
        return parse_matrix_span(start, end, NULL);
    return NULL;

cleanup:
    mat_free(right);
    mat_free(left);
    return result;
}

static matrix_t *evaluate_direct_matrix_expression(const char *input, const char **operation_out, bool *matched_out)
{
    if (matched_out)
        *matched_out = false;
    if (!input || !operation_out || !matched_out)
        return NULL;
    return evaluate_matrix_expression_span(input, input + strlen(input), false, operation_out, matched_out);
}

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
    char *inline_text = mat_to_string(matrix, MAT_STRING_INLINE_PRETTY);
    char *pretty_text = mat_to_string(matrix, MAT_STRING_LAYOUT_PRETTY);
    char *TeX_text = mat_to_string(matrix, MAT_STRING_LATEX);

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
    char *lhs_TeX = matrix_scalar_lhs_TeX(matrix, operation);

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
    const char *matrix_input = input;
    int precision = argc > 3 ? atoi(argv[3]) : 64;
    const char *operand = argc > 4 ? argv[4] : NULL;
    const char *operand_input = operand;
    bool direct_operation = false;
    mat_bindings_t *bindings = NULL;
    matrix_t *matrix = NULL;
    matrix_t *other = NULL;
    matrix_t *result = NULL;
    matrix_unary_function_t unary_function = NULL;
    int rc = 1;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    result = evaluate_direct_matrix_expression(input, &operation, &direct_operation);
    if (direct_operation) {
        printf("input       %s\n", input);
        printf("operation   %s\n", operation);
        if (!result)
            goto cleanup;
        goto result_ready;
    }

    matrix = parse_matrix_span(matrix_input, matrix_input + strlen(matrix_input), &bindings);
    if (!matrix) {
        fprintf(stderr, "Could not parse matrix input\n");
        goto cleanup;
    }

    printf("input       %s\n", input);
    printf("operation   %s\n", operation);
    unary_function = matrix_unary_function_for(operation);

    if (strcmp(operation, "eval") == 0) {
        result = mat_evaluate(matrix);
    } else if (strcmp(operation, "simplify") == 0) {
        result = mat_simplify_symbolic(matrix);
    } else if (unary_function) {
        if (mat_get_row_count(matrix) != mat_get_col_count(matrix)) {
            fprintf(stderr, "Matrix function %s requires a square matrix; received %zux%zu\n", operation,
                    mat_get_row_count(matrix), mat_get_col_count(matrix));
            goto cleanup;
        }
        result = unary_function(matrix);
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
        if (!operand_input) {
            fprintf(stderr, "%s needs a right-hand-side matrix\n", strcmp(operation, "solve") == 0 ? "Solve" : "Multiply");
            goto cleanup;
        }
        other = parse_matrix_span(operand_input, operand_input + strlen(operand_input), NULL);
        if (!other) {
            fprintf(stderr, "Could not parse %s operand\n", operation);
            goto cleanup;
        }
        printf("operand     %s\n", operand_input);
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
        if (unary_function)
            fprintf(stderr, "Matrix function %s failed\n", operation);
        else
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

    print_matrix_fields(result);
    rc = 0;

cleanup:
    mat_free(result);
    mat_free(other);
    mat_free(matrix);
    mat_bindings_free(bindings);
    return rc;
}
