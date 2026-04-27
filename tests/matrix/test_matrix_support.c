#include "test_matrix.h"

char current_matrix_input_label[128];
static matrix_t *current_matrix_input = NULL;

static matrix_t *clone_matrix_snapshot(const matrix_t *A)
{
    if (!A)
        return NULL;

    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);

    switch (mat_typeof(A))
    {
    case MAT_TYPE_DOUBLE:
    {
        double *data = malloc(rows * cols * sizeof(double));
        matrix_t *copy;
        if (!data)
            return NULL;
        mat_get_data(A, data);
        copy = mat_create_d(rows, cols, data);
        free(data);
        return copy;
    }
    case MAT_TYPE_QFLOAT:
    {
        qfloat_t *data = malloc(rows * cols * sizeof(qfloat_t));
        matrix_t *copy;
        if (!data)
            return NULL;
        mat_get_data(A, data);
        copy = mat_create_qf(rows, cols, data);
        free(data);
        return copy;
    }
    case MAT_TYPE_QCOMPLEX:
    {
        qcomplex_t *data = malloc(rows * cols * sizeof(qcomplex_t));
        matrix_t *copy;
        if (!data)
            return NULL;
        mat_get_data(A, data);
        copy = mat_create_qc(rows, cols, data);
        free(data);
        return copy;
    }
    }

    return NULL;
}

static int is_primary_matrix_label(const char *label)
{
    static const char *derived_prefixes[] = {
        "exp(", "sin(", "cos(", "tan(", "sinh(", "cosh(", "tanh(",
        "sqrt(", "log(", "asin(", "acos(", "atan(", "asinh(",
        "acosh(", "atanh(", "erf(", "erfc(", "transpose(", "conj(",
        "eigenvectors", "V ", NULL};

    if (!label || !label[0])
        return 0;
    if (label[0] < 'A' || label[0] > 'Z')
        return 0;

    for (size_t i = 0; derived_prefixes[i]; i++)
        if (strncmp(label, derived_prefixes[i], strlen(derived_prefixes[i])) == 0)
            return 0;

    size_t token_len = 0;
    while ((label[token_len] >= 'A' && label[token_len] <= 'Z') ||
           (label[token_len] >= 'a' && label[token_len] <= 'z') ||
           (label[token_len] >= '0' && label[token_len] <= '9'))
        token_len++;

    if (token_len == 0)
        return 0;

    while (label[token_len] == ' ')
        token_len++;

    if (label[token_len] == '\0' || label[token_len] == '(')
        return 1;

    if ((label[token_len] >= 'A' && label[token_len] <= 'Z') ||
        (label[token_len] >= 'a' && label[token_len] <= 'z'))
        return 1;

    if (label[0] == 'I' && label[token_len] == '+' && label[token_len + 1] != '\0')
        return 1;

    return 0;
}

static void remember_matrix_input(const char *label, matrix_t *A)
{
    if (!is_primary_matrix_label(label))
        return;

    snprintf(current_matrix_input_label, sizeof(current_matrix_input_label), "%s", label);
    mat_free(current_matrix_input);
    current_matrix_input = clone_matrix_snapshot(A);
}

void clear_matrix_input_context(void)
{
    current_matrix_input_label[0] = '\0';
    mat_free(current_matrix_input);
    current_matrix_input = NULL;
}

static void format_matrix_label(const char *label, char *out, size_t out_size)
{
    if (is_primary_matrix_label(label))
        snprintf(out, out_size, "input %s", label);
    else
        snprintf(out, out_size, "%s", label);
}

void d_to_coloured_string(double x, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else if (x >= tol)
        snprintf(out, out_size, C_RED "%.16g" C_RESET, x);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size)
{
    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%q", x);
    if (strcmp(buf, "0") == 0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_YELLOW "%s" C_RESET, buf);
}

void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size)
{
    char re[128], im[128];
    qf_sprintf(re, sizeof(re), "%q", z.re);
    qf_sprintf(im, sizeof(im), "%q", qf_abs(z.im));
    const char *sign = (z.im.hi >= 0.0) ? "+" : "-";
    snprintf(out, out_size,
             C_GREEN "%s" C_RESET " " C_WHITE "%s" C_RESET " " C_MAGENTA "%si" C_RESET,
             re, sign, im);
}

void print_qc(const char *label, qcomplex_t z)
{
    char buf[512];
    qc_to_coloured_string(z, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

void print_qf(const char *label, qfloat_t x)
{
    char buf[512];
    qf_to_coloured_string(x, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

static void print_md_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            double v;
            char buf[256];
            mat_get(A, i, j, &v);
            d_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            double v;
            char buf[256];
            mat_get(A, i, j, &v);
            d_to_coloured_string(v, buf, sizeof(buf));
            printf(" %*s", (int)w[j], buf);
        }
        printf("\n");
    }

    printf("    " C_CYAN "]" C_RESET "\n");
    free(w);
}

void print_md(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_md_raw(display_label, A);
}

static void print_mqf_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            qfloat_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qfloat_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            printf(" %*s", (int)w[j], buf);
        }
        printf("\n");
    }

    printf("    " C_CYAN "]" C_RESET "\n");
    free(w);
}

void print_mqf(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_mqf_raw(display_label, A);
}

static void print_mqc_raw(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);
    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            qcomplex_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qc_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j])
                w[j] = len;
        }

    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qcomplex_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qc_to_coloured_string(v, buf, sizeof(buf));
            printf(" (%*s)", (int)w[j], buf);
        }
        printf("\n");
    }

    printf("    " C_CYAN "]" C_RESET "\n");
    free(w);
}

void print_mqc(const char *label, matrix_t *A)
{
    char display_label[160];
    remember_matrix_input(label, A);
    format_matrix_label(label, display_label, sizeof(display_label));
    print_mqc_raw(display_label, A);
}

void print_current_input_matrix(void)
{
    if (!current_matrix_input || current_matrix_input_label[0] == '\0')
        return;

    switch (mat_typeof(current_matrix_input))
    {
    case MAT_TYPE_DOUBLE:
        print_md_raw("input matrix", current_matrix_input);
        break;
    case MAT_TYPE_QFLOAT:
        print_mqf_raw("input matrix", current_matrix_input);
        break;
    case MAT_TYPE_QCOMPLEX:
        print_mqc_raw("input matrix", current_matrix_input);
        break;
    default:
        break;
    }
}

void check_d(const char *label, double got, double expected, double tol)
{
    double err = fabs(got - expected);
    int ok = err < tol;

    tests_run++;
    if (!ok)
        tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    char gbuf[256], ebuf[256];
    d_to_coloured_string(got, gbuf, sizeof(gbuf));
    d_to_coloured_string(expected, ebuf, sizeof(ebuf));

    printf("    got      = %s\n", gbuf);
    printf("    expected = %s\n", ebuf);
    printf("    error    = %.16g\n", err);
}

void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol)
{
    qfloat_t diff = qf_abs(qf_sub(got, expected));
    double err = diff.hi;
    int ok = err < tol;

    tests_run++;
    if (!ok)
        tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    print_qf("got      ", got);
    print_qf("expected ", expected);
    printf("    error    = %.16g\n", err);
}

void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol)
{
    double got_re = qf_to_double(got.re);
    double got_im = qf_to_double(got.im);
    double exp_re = qf_to_double(expected.re);
    double exp_im = qf_to_double(expected.im);
    int both_nan = isnan(got_re) && isnan(got_im) && isnan(exp_re) && isnan(exp_im);
    double err = both_nan ? 0.0 : qf_to_double(qc_abs(qc_sub(got, expected)));
    int ok = both_nan || err < tol;

    tests_run++;
    if (!ok)
        tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);

    print_qc("got      ", got);
    print_qc("expected ", expected);
    printf("    error    = %.16g\n", err);
}

void check_bool(const char *label, int cond)
{
    tests_run++;
    if (!cond)
        tests_failed++;

    printf(cond ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
                : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);
}
