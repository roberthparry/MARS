#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "matrix.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

/* ------------------------------------------------------------------ coloured scalar printers */

static void d_to_coloured_string(double x, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

static void d_to_coloured_err_string(double x, double tol, char *out, size_t out_size)
{
    if (x == 0.0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else if (x >= tol)
        snprintf(out, out_size, C_RED "%.16g" C_RESET, x);
    else
        snprintf(out, out_size, C_WHITE "%.16g" C_RESET, x);
}

static void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size)
{
    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%q", x);
    if (strcmp(buf, "0") == 0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_YELLOW "%s" C_RESET, buf);
}

static void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size)
{
    char re[128], im[128];
    qf_sprintf(re, sizeof(re), "%q", z.re);
    qf_sprintf(im, sizeof(im), "%q", qf_abs(z.im));
    const char *sign = (z.im.hi >= 0.0) ? "+" : "-";
    snprintf(out, out_size,
             C_GREEN "%s" C_RESET " " C_WHITE "%s" C_RESET " " C_MAGENTA "%si" C_RESET,
             re, sign, im);
}

/* ------------------------------------------------------------------ coloured qc/qf debug printers */

static void print_qc(const char *label, qcomplex_t z)
{
    char buf[512];
    qc_to_coloured_string(z, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

static void print_qf(const char *label, qfloat_t x)
{
    char buf[512];
    qf_to_coloured_string(x, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

/* ------------------------------------------------------------------ pretty, coloured matrix printers */

static void print_md(const char *label, matrix_t *A)
{
    size_t rows = mat_get_row_count(A);
    size_t cols = mat_get_col_count(A);

    printf("    %s = " C_CYAN "[" C_RESET "\n", label);

    size_t *w = calloc(cols, sizeof(size_t));

    /* compute widths */
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

    /* print rows */
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

static void print_mqf(const char *label, matrix_t *A)
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

static void print_mqc(const char *label, matrix_t *A)
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

/* ------------------------------------------------------------------ checkers */

static void check_d(const char *label, double got, double expected, double tol)
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

static void check_qf_val(const char *label, qfloat_t got, qfloat_t expected, double tol)
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

static void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol)
{
    double err = qf_to_double(qc_abs(qc_sub(got, expected)));
    int ok = err < tol;

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

static void check_bool(const char *label, int cond)
{
    tests_run++;
    if (!cond)
        tests_failed++;

    printf(cond ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
                : C_BOLD C_RED "  FAIL: %s\n" C_RESET,
           label);
}

/* ------------------------------------------------------------------ 1. creation */

static void test_creation(void)
{
    printf(C_CYAN "TEST: creation of all matrix types\n" C_RESET);

    matrix_t *Ad = mat_new_d(2, 3);
    matrix_t *Aqf = mat_new_qf(3, 4);
    matrix_t *Aqc = mat_new_qc(4, 5);

    check_bool("mat_new_d non-null", Ad != NULL);
    check_bool("mat_new_qf non-null", Aqf != NULL);
    check_bool("mat_new_qc non-null", Aqc != NULL);

    print_md("Ad", Ad);
    print_mqf("Aqf", Aqf);
    print_mqc("Aqc", Aqc);

    mat_free(Ad);
    mat_free(Aqf);
    mat_free(Aqc);
}

/* ------------------------------------------------------------------ 2. reading */

static void test_reading(void)
{
    printf(C_CYAN "TEST: reading from all matrix types\n" C_RESET);

    /* double */
    {
        const double vals[4] = {
            3.0, 0.0,
            0.0, 4.0};
        matrix_t *A = mat_create_d(2, 2, vals);
        print_md("A", A);

        double out[4];
        mat_get_data(A, out);

        check_d("read double A[0,0] = 3", out[0], 3.0, 1e-30);
        check_d("read double A[1,1] = 4", out[3], 4.0, 1e-30);

        mat_free(A);
    }

    /* qfloat */
    {
        qfloat_t vals[4] = {
            QF_ZERO, qf_from_double(1.25),
            qf_from_double(-2.5), QF_ZERO};
        matrix_t *B = mat_create_qf(2, 2, vals);
        print_mqf("B", B);

        qfloat_t out[4];
        mat_get_data(B, out);

        check_qf_val("read qfloat B[0,1] = 1.25", out[1], vals[1], 1e-30);
        check_qf_val("read qfloat B[1,0] = -2.5", out[2], vals[2], 1e-30);

        mat_free(B);
    }

    /* qcomplex */
    {
        qcomplex_t z1 = qc_make(qf_from_double(2.0), qf_from_double(3.0));
        qcomplex_t z2 = qc_make(qf_from_double(-1.0), qf_from_double(0.5));

        qcomplex_t vals[4] = {
            z1, QC_ZERO,
            QC_ZERO, z2};

        matrix_t *C = mat_create_qc(2, 2, vals);
        print_mqc("C", C);

        qcomplex_t out[4];
        mat_get_data(C, out);

        check_qc_val("read qcomplex C[0,0]", out[0], z1, 1e-30);
        check_qc_val("read qcomplex C[1,1]", out[3], z2, 1e-30);

        mat_free(C);
    }
}

/* ------------------------------------------------------------------ 3. writing */

static void test_writing(void)
{
    printf(C_CYAN "TEST: writing to all matrix types\n" C_RESET);

    /* double */
    {
        matrix_t *A = mat_new_d(2, 2);
        double x = 9.0;
        mat_set(A, 1, 0, &x);

        print_md("A after write", A);

        double vals[4];
        mat_get_data(A, vals);
        check_d("write double A[1,0] = 9", vals[2], 9.0, 1e-30);

        mat_free(A);
    }

    /* qfloat */
    {
        matrix_t *B = mat_new_qf(2, 2);
        qfloat_t qx = qf_from_double(7.75);
        mat_set(B, 0, 1, &qx);

        print_mqf("B after write", B);

        qfloat_t vals[4];
        mat_get_data(B, vals);
        check_qf_val("write qfloat B[0,1] = 7.75", vals[1], qx, 1e-30);

        mat_free(B);
    }

    /* qcomplex */
    {
        matrix_t *C = mat_new_qc(2, 2);
        qcomplex_t z = qc_make(qf_from_double(1.0), qf_from_double(-3.0));
        mat_set(C, 1, 1, &z);

        print_mqc("C after write", C);

        qcomplex_t vals[4];
        mat_get_data(C, vals);
        check_qc_val("write qcomplex C[1,1]", vals[3], z, 1e-30);

        mat_free(C);
    }
}

/* ------------------------------------------------------------------ 4. add/sub (double only) */

static void test_add_sub(void)
{
    printf(C_CYAN "TEST: addition/subtraction\n" C_RESET);

    const double A_vals[4] = {1, 2, 3, 4};
    const double B_vals[4] = {5, 6, 7, 8};

    matrix_t *A = mat_create_d(2, 2, A_vals);
    matrix_t *B = mat_create_d(2, 2, B_vals);

    print_md("A", A);
    print_md("B", B);

    matrix_t *C = mat_add(A, B);
    print_md("A+B", C);

    double c_vals[4];
    mat_get_data(C, c_vals);
    check_d("add[0,0] = 6", c_vals[0], 6, 1e-30);
    check_d("add[1,1] = 12", c_vals[3], 12, 1e-30);

    matrix_t *D = mat_sub(A, B);
    print_md("A-B", D);

    double d_vals[4];
    mat_get_data(D, d_vals);
    check_d("sub[0,0] = -4", d_vals[0], -4, 1e-30);
    check_d("sub[1,1] = -4", d_vals[3], -4, 1e-30);

    mat_free(A);
    mat_free(B);
    mat_free(C);
    mat_free(D);
}

/* ------------------------------------------------------------------ 5. multiply (double only) */

static void test_multiply(void)
{
    printf(C_CYAN "TEST: multiplication\n" C_RESET);

    double A_vals[4] = {1, 2, 3, 4};
    double B_vals[4] = {5, 6, 7, 8};
    matrix_t *A = mat_create_d(2, 2, A_vals);
    matrix_t *B = mat_create_d(2, 2, B_vals);

    print_md("A", A);
    print_md("B", B);

    matrix_t *C = mat_mul(A, B);
    print_md("A*B", C);

    double v;
    mat_get(C, 0, 0, &v);
    check_d("mul[0,0] = 19", v, 19, 1e-30);
    mat_get(C, 1, 1, &v);
    check_d("mul[1,1] = 50", v, 50, 1e-30);

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ 6. transpose/conjugate */

static void test_transpose_conjugate(void)
{
    printf(C_CYAN "TEST: transpose/conjugate\n" C_RESET);

    matrix_t *A = mat_new_d(2, 3);
    double x = 1, y = 2, z = 3;
    mat_set(A, 0, 1, &x);
    mat_set(A, 1, 2, &y);
    mat_set(A, 0, 2, &z);

    print_md("A", A);

    matrix_t *T = mat_transpose(A);
    print_md("transpose(A)", T);

    double v;
    mat_get(T, 1, 0, &v);
    check_d("transpose T[1,0] = 1", v, 1, 1e-30);
    mat_get(T, 2, 1, &v);
    check_d("transpose T[2,1] = 2", v, 2, 1e-30);
    mat_get(T, 2, 0, &v);
    check_d("transpose T[2,0] = 3", v, 3, 1e-30);

    mat_free(A);
    mat_free(T);

    matrix_t *C = mat_new_qc(2, 2);
    qcomplex_t z1 = qc_make(qf_from_double(2.0), qf_from_double(3.0));
    qcomplex_t z2 = qc_make(qf_from_double(-1.0), qf_from_double(4.0));
    mat_set(C, 0, 0, &z1);
    mat_set(C, 1, 1, &z2);

    print_mqc("C", C);

    matrix_t *K = mat_conj(C);
    print_mqc("conj(C)", K);

    qcomplex_t zv;
    mat_get(K, 0, 0, &zv);
    check_qc_val("conj C[0,0]", zv, qc_conj(z1), 1e-30);

    mat_get(K, 1, 1, &zv);
    check_qc_val("conj C[1,1]", zv, qc_conj(z2), 1e-30);

    mat_free(C);
    mat_free(K);
}

/* ------------------------------------------------------------------ 7. identity get */

static void test_identity_get(void)
{
    printf(C_CYAN "TEST: identity matrix get\n" C_RESET);

    matrix_t *I = mat_create_identity_d(3);
    print_md("I", I);

    double vals[9];
    mat_get_data(I, vals);

    check_d("I[0,0] = 1", vals[0], 1, 1e-30);
    check_d("I[1,1] = 1", vals[4], 1, 1e-30);
    check_d("I[2,2] = 1", vals[8], 1, 1e-30);

    check_d("I[0,1] = 0", vals[1], 0, 1e-30);
    check_d("I[1,2] = 0", vals[5], 0, 1e-30);

    mat_free(I);
}

/* ------------------------------------------------------------------ 8. identity set (materialise) */

static void test_identity_set(void)
{
    printf(C_CYAN "TEST: identity matrix set (materialisation)\n" C_RESET);

    matrix_t *I = mat_create_identity_d(3);

    print_md("I before write", I);

    double x = 7.0;
    mat_set(I, 0, 2, &x);

    print_md("I after write", I);

    double vals[9];
    mat_get_data(I, vals);

    check_d("after write, I[0,2] = 7", vals[2], 7.0, 1e-30);
    check_d("diagonal preserved", vals[4], 1.0, 1e-30);

    mat_free(I);
}

/* ------------------------------------------------------------------ qfloat add/sub (mixed sizes) */

static void test_add_sub_qf(void)
{
    printf(C_CYAN "TEST: qfloat addition/subtraction (mixed sizes)\n" C_RESET);

    qfloat_t a_vals[6] = {
        qf_from_double(1), qf_from_double(2), qf_from_double(3),
        qf_from_double(4), qf_from_double(5), qf_from_double(6)};
    qfloat_t b_vals[6] = {
        qf_from_double(10), qf_from_double(20), qf_from_double(30),
        qf_from_double(40), qf_from_double(50), qf_from_double(60)};

    matrix_t *A = mat_create_qf(2, 3, a_vals);
    matrix_t *B = mat_create_qf(2, 3, b_vals);

    print_mqf("A", A);
    print_mqf("B", B);

    matrix_t *C = mat_add(A, B);
    print_mqf("A+B", C);

    qfloat_t c_vals[6];
    mat_get_data(C, c_vals);

    for (size_t k = 0; k < 6; k++)
    {
        qfloat_t expected = qf_add(a_vals[k], b_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "qfloat add[%zu,%zu]", k / 3, k % 3);
        check_qf_val(label, c_vals[k], expected, 1e-28);
    }

    matrix_t *D = mat_sub(A, B);
    print_mqf("A-B", D);

    qfloat_t d_vals[6];
    mat_get_data(D, d_vals);

    for (size_t k = 0; k < 6; k++)
    {
        qfloat_t expected = qf_sub(a_vals[k], b_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "qfloat sub[%zu,%zu]", k / 3, k % 3);
        check_qf_val(label, d_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
    mat_free(D);
}

/* ------------------------------------------------------------------ qcomplex add/sub (mixed sizes) */

static void test_add_sub_qc(void)
{
    printf(C_CYAN "TEST: qcomplex addition/subtraction (mixed sizes)\n" C_RESET);

    qcomplex_t a_vals[3] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(3), qf_from_double(4)),
        qc_make(qf_from_double(-1), qf_from_double(5))};
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(10), qf_from_double(-2)),
        qc_make(qf_from_double(0), qf_from_double(7)),
        qc_make(qf_from_double(2), qf_from_double(3))};
    matrix_t *A = mat_create_qc(1, 3, a_vals);
    matrix_t *B = mat_create_qc(1, 3, b_vals);

    print_mqc("A", A);
    print_mqc("B", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A+B", C);

    qcomplex_t C_vals[3];
    mat_get_data(C, C_vals);
    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_add(a_vals[j], b_vals[j]);
        char label[64];
        snprintf(label, sizeof(label), "qcomplex add[0,%zu]", j);
        check_qc_val(label, C_vals[j], expected, 1e-28);
    }

    matrix_t *D = mat_sub(A, B);
    print_mqc("A-B", D);

    qcomplex_t D_vals[3];
    mat_get_data(D, D_vals);
    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_sub(a_vals[j], b_vals[j]);
        char label[64];
        snprintf(label, sizeof(label), "qcomplex sub[0,%zu]", j);
        check_qc_val(label, D_vals[j], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
    mat_free(D);
}

/* ------------------------------------------------------------------ qfloat multiply (mixed sizes) */

static void test_multiply_qf(void)
{
    printf(C_CYAN "TEST: qfloat multiplication (mixed sizes)\n" C_RESET);

    double a_raw[6] = {1, 2, 3, 4, 5, 6};
    double b_raw[6] = {7, 8, 9, 10, 11, 12};

    qfloat_t A_vals[6];
    qfloat_t B_vals[6];

    for (size_t k = 0; k < 6; k++)
    {
        A_vals[k] = qf_from_double(a_raw[k]);
        B_vals[k] = qf_from_double(b_raw[k]);
    }

    matrix_t *A = mat_create_qf(2, 3, A_vals);
    matrix_t *B = mat_create_qf(3, 2, B_vals);

    print_mqf("A", A);
    print_mqf("B", B);

    matrix_t *C = mat_mul(A, B);
    print_mqf("A*B", C);

    double expected_raw[4] = {58, 64, 139, 154};

    qfloat_t C_vals[4];
    mat_get_data(C, C_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_from_double(expected_raw[k]);
        char label[64];
        snprintf(label, sizeof(label), "qfloat mul[%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, C_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ qcomplex multiply (mixed sizes) */

static void test_multiply_qc(void)
{
    printf(C_CYAN "TEST: qcomplex multiplication (mixed sizes)\n" C_RESET);

    qcomplex_t a_vals[3] = {
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(2), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(3))};

    qcomplex_t b_vals[4] = {
        qc_make(qf_from_double(4), qf_from_double(0)),
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(-3), qf_from_double(1)),
        qc_make(qf_from_double(0), qf_from_double(-2))};

    matrix_t *A = mat_create_qc(3, 1, a_vals);
    matrix_t *B = mat_create_qc(1, 4, b_vals);

    print_mqc("A", A);
    print_mqc("B", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A*B", C);

    qcomplex_t C_vals[12];
    mat_get_data(C, C_vals);

    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 4; j++)
        {
            size_t k = i * 4 + j;
            qcomplex_t expected = qc_mul(a_vals[i], b_vals[j]);
            char label[64];
            snprintf(label, sizeof(label), "qcomplex mul[%zu,%zu]", i, j);
            check_qc_val(label, C_vals[k], expected, 1e-28);
        }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type add: double + qfloat */

static void test_add_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type addition (double + qfloat)\n" C_RESET);

    double a_vals[4] = {1.0, 2.0, 3.0, 4.0};
    qfloat_t b_vals[4] = {
        qf_from_double(10), qf_from_double(20),
        qf_from_double(30), qf_from_double(40)};

    matrix_t *A = mat_create_d(2, 2, a_vals);
    matrix_t *B = mat_create_qf(2, 2, b_vals);

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_add(A, B);
    print_mqf("A + B (qfloat result)", C);

    qfloat_t C_vals[4];
    mat_get_data(C, C_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_add(qf_from_double(a_vals[k]), b_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "mixed add d+qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, C_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type add: double + qcomplex */

static void test_add_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type addition (double + qcomplex)\n" C_RESET);

    double a_vals[3] = {1.0, -2.0, 5.0};
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(3), qf_from_double(4)),
        qc_make(qf_from_double(0), qf_from_double(-1)),
        qc_make(qf_from_double(2), qf_from_double(2))};

    matrix_t *A = mat_create_d(1, 3, a_vals);
    matrix_t *B = mat_create_qc(1, 3, b_vals);

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A + B (qcomplex result)", C);

    qcomplex_t C_vals[3];
    mat_get_data(C, C_vals);

    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_add(
            qc_make(qf_from_double(a_vals[j]), QF_ZERO),
            b_vals[j]);
        char label[64];
        snprintf(label, sizeof(label), "mixed add d+qc [0,%zu]", j);
        check_qc_val(label, C_vals[j], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type add: qfloat + qcomplex */

static void test_add_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type addition (qfloat + qcomplex)\n" C_RESET);

    qfloat_t a_vals[2] = {qf_from_double(1.5), qf_from_double(-3.25)};
    qcomplex_t b_vals[2] = {
        qc_make(qf_from_double(2), qf_from_double(1)),
        qc_make(qf_from_double(-1), qf_from_double(4))};

    matrix_t *A = mat_create_qf(2, 1, a_vals);
    matrix_t *B = mat_create_qc(2, 1, b_vals);

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A + B (qcomplex result)", C);

    qcomplex_t C_vals[2];
    mat_get_data(C, C_vals);

    for (size_t i = 0; i < 2; i++)
    {
        qcomplex_t expected = qc_add(
            qc_make(a_vals[i], QF_ZERO),
            b_vals[i]);
        char label[64];
        snprintf(label, sizeof(label), "mixed add qf+qc [%zu,0]", i);
        check_qc_val(label, C_vals[i], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: double - qfloat */

static void test_sub_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (double - qfloat)\n" C_RESET);

    double a_vals[4] = {5.0, 7.0, -3.0, 2.0};
    qfloat_t b_vals[4] = {
        qf_from_double(1.0), qf_from_double(2.5),
        qf_from_double(-4.0), qf_from_double(10.0)};

    matrix_t *A = mat_create_d(2, 2, a_vals);
    matrix_t *B = mat_create_qf(2, 2, b_vals);

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqf("A - B (qfloat result)", C);

    qfloat_t C_vals[4];
    mat_get_data(C, C_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_sub(qf_from_double(a_vals[k]), b_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "mixed sub d-qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, C_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: double - qcomplex */

static void test_sub_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (double - qcomplex)\n" C_RESET);

    double a_vals[3] = {10.0, -5.0, 3.0};
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(2), qf_from_double(1)),
        qc_make(qf_from_double(-3), qf_from_double(4)),
        qc_make(qf_from_double(0.5), qf_from_double(-2))};

    matrix_t *A = mat_create_d(1, 3, a_vals);
    matrix_t *B = mat_create_qc(1, 3, b_vals);

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqc("A - B (qcomplex result)", C);

    qcomplex_t C_vals[3];
    mat_get_data(C, C_vals);

    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_sub(
            qc_make(qf_from_double(a_vals[j]), QF_ZERO),
            b_vals[j]);
        char label[64];
        snprintf(label, sizeof(label), "mixed sub d-qc [0,%zu]", j);
        check_qc_val(label, C_vals[j], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: qfloat - qcomplex */

static void test_sub_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (qfloat - qcomplex)\n" C_RESET);

    qfloat_t a_vals[2] = {
        qf_from_double(4.5),
        qf_from_double(-1.25)};

    qcomplex_t b_vals[2] = {
        qc_make(qf_from_double(1), qf_from_double(3)),
        qc_make(qf_from_double(-2), qf_from_double(1))};

    matrix_t *A = mat_create_qf(2, 1, a_vals);
    matrix_t *B = mat_create_qc(2, 1, b_vals);

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqc("A - B (qcomplex result)", C);

    qcomplex_t C_vals[2];
    mat_get_data(C, C_vals);

    for (size_t i = 0; i < 2; i++)
    {
        qcomplex_t expected = qc_sub(
            qc_make(a_vals[i], QF_ZERO),
            b_vals[i]);
        char label[64];
        snprintf(label, sizeof(label), "mixed sub qf-qc [%zu,0]", i);
        check_qc_val(label, C_vals[i], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: double * qfloat */

static void test_multiply_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (double * qfloat)\n" C_RESET);

    double a_vals[6] = {1, 2, 3, 4, 5, 6};
    qfloat_t b_vals[6] = {
        qf_from_double(7), qf_from_double(8),
        qf_from_double(9), qf_from_double(10),
        qf_from_double(11), qf_from_double(12)};

    matrix_t *A = mat_create_d(2, 3, a_vals);
    matrix_t *B = mat_create_qf(3, 2, b_vals);

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqf("A * B (qfloat result)", C);

    double expected_raw[4] = {58, 64, 139, 154};

    qfloat_t C_vals[4];
    mat_get_data(C, C_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_from_double(expected_raw[k]);
        char label[64];
        snprintf(label, sizeof(label), "mixed mul d*qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, C_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: double * qcomplex */

static void test_multiply_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (double * qcomplex)\n" C_RESET);

    double a_vals[3] = {2.0, -1.0, 3.0};
    qcomplex_t b_vals[6] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(0), qf_from_double(-1)),
        qc_make(qf_from_double(4), qf_from_double(0)),
        qc_make(qf_from_double(-2), qf_from_double(3)),
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(0), qf_from_double(5))};

    matrix_t *A = mat_create_d(1, 3, a_vals);
    matrix_t *B = mat_create_qc(3, 2, b_vals);

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A * B (qcomplex result)", C);

    qcomplex_t C_vals[6];
    mat_get_data(C, C_vals);

    for (size_t i = 0; i < 1; i++)
        for (size_t j = 0; j < 2; j++)
        {
            size_t k = i * 2 + j;
            qcomplex_t expected = QC_ZERO;
            for (size_t t = 0; t < 3; t++)
            {
                qcomplex_t term = qc_mul(
                    qc_make(qf_from_double(a_vals[t]), QF_ZERO),
                    b_vals[t * 2 + j]);
                expected = qc_add(expected, term);
            }
            char label[64];
            snprintf(label, sizeof(label), "mixed mul d*qc [%zu,%zu]", i, j);
            check_qc_val(label, C_vals[k], expected, 1e-28);
        }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: qfloat * qcomplex */

static void test_multiply_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (qfloat * qcomplex)\n" C_RESET);

    qfloat_t a_vals[2] = {
        qf_from_double(2.5),
        qf_from_double(-1.0)};

    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(3), qf_from_double(1)),
        qc_make(qf_from_double(-2), qf_from_double(4)),
        qc_make(qf_from_double(0), qf_from_double(-3))};

    matrix_t *A = mat_create_qf(2, 1, a_vals);
    matrix_t *B = mat_create_qc(1, 3, b_vals);

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A * B (qcomplex result)", C);

    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++)
        {
            qcomplex_t got;
            qcomplex_t expected = qc_mul(
                qc_make(a_vals[i], QF_ZERO),
                b_vals[j]);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "mixed mul qf*qc [%zu,%zu]", i, j);
            check_qc_val(label, got, expected, 1e-28);
        }

    mat_free(A);
    mat_free(B);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × double matrix */

static void test_scalar_mul_d_d(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * double matrix)\n" C_RESET);

    const double A_vals[4] = {
        1.0, 2.0,
        3.0, 4.0};
    const double alpha = 2.5;

    matrix_t *A = mat_create_d(2, 2, A_vals);
    print_md("A", A);

    matrix_t *B = mat_scalar_mul_d(A, alpha);
    print_md("alpha * A", B);

    double B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        double expected = alpha * A_vals[k];
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*d [%zu,%zu]", k / 2, k % 2);
        check_d(label, B_vals[k], expected, 1e-30);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × qfloat matrix */

static void test_scalar_mul_d_qf(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * qfloat matrix)\n" C_RESET);

    qfloat_t A_vals[4] = {
        qf_from_double(1.0), qf_from_double(-2.0),
        qf_from_double(3.5), qf_from_double(0.5)};
    const double alpha = -1.25;

    matrix_t *A = mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_mul_d(A, alpha);
    print_mqf("alpha * A (qfloat)", B);

    qfloat_t B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_mul(qf_from_double(alpha), A_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × qcomplex matrix */

static void test_scalar_mul_d_qc(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * qcomplex matrix)\n" C_RESET);

    qcomplex_t A_vals[3] = {
        qc_make(qf_from_double(1.0), qf_from_double(2.0)),
        qc_make(qf_from_double(-3.0), qf_from_double(0.5)),
        qc_make(qf_from_double(0.0), qf_from_double(-1.0))};
    const double alpha = 0.75;

    matrix_t *A = mat_create_qc(1, 3, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_mul_d(A, alpha);
    print_mqc("alpha * A (qcomplex)", B);

    qcomplex_t B_vals[3];
    mat_get_data(B, B_vals);

    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_mul(
            qc_make(qf_from_double(alpha), QF_ZERO),
            A_vals[j]);
        char label[64];
        snprintf(label, sizeof(label), "scalar mul d*qc [0,%zu]", j);
        check_qc_val(label, B_vals[j], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar multiply: qfloat scalar × double matrix */

static void test_scalar_mul_qf_d(void)
{
    printf(C_CYAN "TEST: scalar multiply (qfloat * double matrix)\n" C_RESET);

    const double A_vals[6] = {
        1.0, -2.0, 3.0,
        0.5, 4.0, -1.0};
    qfloat_t alpha = qf_from_double(1.75);

    matrix_t *A = mat_create_d(2, 3, A_vals);
    print_md("A (double)", A);

    matrix_t *B = mat_scalar_mul_qf(A, alpha);
    print_mqf("alpha * A (qfloat)", B);

    qfloat_t B_vals[6];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 6; k++)
    {
        qfloat_t expected = qf_mul(alpha, qf_from_double(A_vals[k]));
        char label[64];
        snprintf(label, sizeof(label), "scalar mul qf*d [%zu,%zu]", k / 3, k % 3);
        check_qf_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar multiply: qcomplex scalar × qcomplex matrix */

static void test_scalar_mul_qc_qc(void)
{
    printf(C_CYAN "TEST: scalar multiply (qcomplex * qcomplex matrix)\n" C_RESET);

    qcomplex_t A_vals[4] = {
        QC_ONE,
        qc_make(qf_from_double(2.0), qf_from_double(-1.0)),
        qc_make(qf_from_double(0.0), qf_from_double(3.0)),
        qc_make(qf_from_double(-1.5), qf_from_double(0.5))};

    qcomplex_t alpha = qc_make(qf_from_double(0.5), qf_from_double(2.0));

    matrix_t *A = mat_create_qc(2, 2, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_mul_qc(A, alpha);
    print_mqc("alpha * A (qcomplex)", B);

    qcomplex_t B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qcomplex_t expected = qc_mul(alpha, A_vals[k]);
        char label[64];
        snprintf(label, sizeof(label), "scalar mul qc*qc [%zu,%zu]", k / 2, k % 2);
        check_qc_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ identity add/sub/mul: double */

static void test_identity_arith_d(void)
{
    printf(C_CYAN "TEST: identity arithmetic (double)\n" C_RESET);

    double vals[4] = {1, 2, 3, 4};
    matrix_t *A = mat_create_d(2, 2, vals);
    matrix_t *I = mat_create_identity_d(2);

    print_md("A", A);
    print_md("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_md("A + I", ApI);

    double expected_add[4] = {2, 2, 3, 5};
    double got_add[4];
    mat_get_data(ApI, got_add);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "d: A+I [%zu,%zu]", k / 2, k % 2);
        check_d(label, got_add[k], expected_add[k], 1e-12);
    }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_md("A - I", AmI);

    double expected_sub[4] = {0, 2, 3, 3};
    double got_sub[4];
    mat_get_data(AmI, got_sub);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "d: A-I [%zu,%zu]", k / 2, k % 2);
        check_d(label, got_sub[k], expected_sub[k], 1e-12);
    }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_md("A * I", A_times_I);

    double got_ai[4];
    mat_get_data(A_times_I, got_ai);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "d: A*I [%zu,%zu]", k / 2, k % 2);
        check_d(label, got_ai[k], vals[k], 1e-12);
    }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_md("I * A", I_times_A);

    double got_ia[4];
    mat_get_data(I_times_A, got_ia);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "d: I*A [%zu,%zu]", k / 2, k % 2);
        check_d(label, got_ia[k], vals[k], 1e-12);
    }

    mat_free(A);
    mat_free(I);
    mat_free(ApI);
    mat_free(AmI);
    mat_free(A_times_I);
    mat_free(I_times_A);
}

/* ------------------------------------------------------------------ identity add/sub/mul: qfloat */

static void test_identity_arith_qf(void)
{
    printf(C_CYAN "TEST: identity arithmetic (qfloat)\n" C_RESET);

    qfloat_t vals[4] = {
        qf_from_double(1.5),
        qf_from_double(2.0),
        qf_from_double(-1.0),
        qf_from_double(4.0)};
    matrix_t *A = mat_create_qf(2, 2, vals);
    matrix_t *I = mat_create_identity_qf(2);

    print_mqf("A", A);
    print_mqf("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_mqf("A + I", ApI);

    qfloat_t expected_add[4] = {
        qf_add(vals[0], QF_ONE), vals[1],
        vals[2], qf_add(vals[3], QF_ONE)};
    qfloat_t got_add[4];
    mat_get_data(ApI, got_add);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qf: A+I [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, got_add[k], expected_add[k], 1e-28);
    }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_mqf("A - I", AmI);

    qfloat_t expected_sub[4] = {
        qf_sub(vals[0], QF_ONE), vals[1],
        vals[2], qf_sub(vals[3], QF_ONE)};
    qfloat_t got_sub[4];
    mat_get_data(AmI, got_sub);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qf: A-I [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, got_sub[k], expected_sub[k], 1e-28);
    }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_mqf("A * I", A_times_I);

    qfloat_t got_ai[4];
    mat_get_data(A_times_I, got_ai);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qf: A*I [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, got_ai[k], vals[k], 1e-28);
    }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_mqf("I * A", I_times_A);

    qfloat_t got_ia[4];
    mat_get_data(I_times_A, got_ia);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qf: I*A [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, got_ia[k], vals[k], 1e-28);
    }

    mat_free(A);
    mat_free(I);
    mat_free(ApI);
    mat_free(AmI);
    mat_free(A_times_I);
    mat_free(I_times_A);
}

/* ------------------------------------------------------------------ identity add/sub/mul: qcomplex */

static void test_identity_arith_qc(void)
{
    printf(C_CYAN "TEST: identity arithmetic (qcomplex)\n" C_RESET);

    qcomplex_t vals[4] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(3), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(4)),
        qc_make(qf_from_double(-2), qf_from_double(3))};
    matrix_t *A = mat_create_qc(2, 2, vals);
    matrix_t *I = mat_create_identity_qc(2);

    print_mqc("A", A);
    print_mqc("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_mqc("A + I", ApI);

    qcomplex_t expected_add[4] = {
        qc_add(vals[0], QC_ONE), vals[1],
        vals[2], qc_add(vals[3], QC_ONE)};
    qcomplex_t got_add[4];
    mat_get_data(ApI, got_add);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qc: A+I [%zu,%zu]", k / 2, k % 2);
        check_qc_val(label, got_add[k], expected_add[k], 1e-28);
    }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_mqc("A - I", AmI);

    qcomplex_t expected_sub[4] = {
        qc_sub(vals[0], QC_ONE), vals[1],
        vals[2], qc_sub(vals[3], QC_ONE)};
    qcomplex_t got_sub[4];
    mat_get_data(AmI, got_sub);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qc: A-I [%zu,%zu]", k / 2, k % 2);
        check_qc_val(label, got_sub[k], expected_sub[k], 1e-28);
    }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_mqc("A * I", A_times_I);

    qcomplex_t got_ai[4];
    mat_get_data(A_times_I, got_ai);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qc: A*I [%zu,%zu]", k / 2, k % 2);
        check_qc_val(label, got_ai[k], vals[k], 1e-28);
    }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_mqc("I * A", I_times_A);

    qcomplex_t got_ia[4];
    mat_get_data(I_times_A, got_ia);
    for (size_t k = 0; k < 4; k++)
    {
        char label[64];
        snprintf(label, sizeof(label), "qc: I*A [%zu,%zu]", k / 2, k % 2);
        check_qc_val(label, got_ia[k], vals[k], 1e-28);
    }

    mat_free(A);
    mat_free(I);
    mat_free(ApI);
    mat_free(AmI);
    mat_free(A_times_I);
    mat_free(I_times_A);
}

/* ------------------------------------------------------------------ scalar division: double scalar */

static void test_scalar_div_d_d(void)
{
    printf(C_CYAN "TEST: scalar division (double / double matrix)\n" C_RESET);

    const double A_vals[4] = {
        2.0, -4.0,
        5.0, 10.0};
    const double alpha = 2.0;

    matrix_t *A = mat_create_d(2, 2, A_vals);
    print_md("A", A);

    matrix_t *B = mat_scalar_div_d(A, alpha);
    print_md("A / alpha", B);

    double B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        double expected = A_vals[k] / alpha;
        char label[64];
        snprintf(label, sizeof(label), "scalar div d/d [%zu,%zu]", k / 2, k % 2);
        check_d(label, B_vals[k], expected, 1e-30);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar division: qfloat scalar */

static void test_scalar_div_qf_qf(void)
{
    printf(C_CYAN "TEST: scalar division (qfloat / qfloat matrix)\n" C_RESET);

    qfloat_t A_vals[4] = {
        qf_from_double(3.0), qf_from_double(-6.0),
        qf_from_double(1.5), qf_from_double(0.75)};
    qfloat_t alpha = qf_from_double(1.5);

    matrix_t *A = mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_div_qf(A, alpha);
    print_mqf("A / alpha (qfloat)", B);

    qfloat_t B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_div(A_vals[k], alpha);
        char label[64];
        snprintf(label, sizeof(label), "scalar div qf/qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar division: qcomplex scalar */

static void test_scalar_div_qc_qc(void)
{
    printf(C_CYAN "TEST: scalar division (qcomplex / qcomplex matrix)\n" C_RESET);

    qcomplex_t A_vals[3] = {
        QC_ONE,
        qc_make(qf_from_double(2.0), qf_from_double(3.0)),
        qc_make(qf_from_double(-1.0), qf_from_double(0.5))};

    qcomplex_t alpha = qc_make(qf_from_double(0.5), qf_from_double(1.0));

    matrix_t *A = mat_create_qc(1, 3, A_vals);
    print_mqc("A (qcomplex)", A);

    matrix_t *B = mat_scalar_div_qc(A, alpha);
    print_mqc("A / alpha (qcomplex)", B);

    qcomplex_t B_vals[3];
    mat_get_data(B, B_vals);

    for (size_t j = 0; j < 3; j++)
    {
        qcomplex_t expected = qc_div(A_vals[j], alpha);
        char label[64];
        snprintf(label, sizeof(label), "scalar div qc/qc [0,%zu]", j);
        check_qc_val(label, B_vals[j], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar division: double scalar / qfloat matrix */

static void test_scalar_div_d_qf(void)
{
    printf(C_CYAN "TEST: scalar division (double / qfloat matrix)\n" C_RESET);

    qfloat_t A_vals[4] = {
        qf_from_double(2.0),
        qf_from_double(-4.0),
        qf_from_double(5.0),
        qf_from_double(10.0)};
    const double alpha = 2.0;

    matrix_t *A = mat_create_qf(2, 2, A_vals);
    print_mqf("A (qfloat)", A);

    matrix_t *B = mat_scalar_div_d(A, alpha);
    print_mqf("A / alpha (qfloat)", B);

    qfloat_t B_vals[4];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 4; k++)
    {
        qfloat_t expected = qf_div(A_vals[k], qf_from_double(alpha));
        char label[64];
        snprintf(label, sizeof(label), "scalar div d/qf [%zu,%zu]", k / 2, k % 2);
        check_qf_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ scalar division: qfloat scalar / double matrix */

static void test_scalar_div_qf_d(void)
{
    printf(C_CYAN "TEST: scalar division (qfloat / double matrix)\n" C_RESET);

    const double A_vals[6] = {
        1.0, -2.0, 4.0,
        0.5, 3.0, -1.0};
    qfloat_t alpha = qf_from_double(2.0);

    matrix_t *A = mat_create_d(2, 3, A_vals);
    print_md("A (double)", A);

    matrix_t *B = mat_scalar_div_qf(A, alpha);
    print_mqf("A / alpha (qfloat)", B);

    qfloat_t B_vals[6];
    mat_get_data(B, B_vals);

    for (size_t k = 0; k < 6; k++)
    {
        qfloat_t expected = qf_div(qf_from_double(A_vals[k]), alpha);
        char label[64];
        snprintf(label, sizeof(label), "scalar div qf/d [%zu,%zu]", k / 3, k % 3);
        check_qf_val(label, B_vals[k], expected, 1e-28);
    }

    mat_free(A);
    mat_free(B);
}

/* ------------------------------------------------------------------ determinant: double */

static void test_det_double(void)
{
    printf(C_CYAN "TEST: determinant (double)\n" C_RESET);

    /* -------------------------------------------------------------- 1×1 */
    {
        const double vals[1] = {7.0};
        matrix_t *A = mat_create_d(1, 1, vals);

        print_md("A (1x1)", A);

        double det;
        mat_det(A, &det);
        check_d("det 1x1 = 7", det, 7.0, 1e-30);

        mat_free(A);
    }

    /* -------------------------------------------------------------- 2×2 */
    {
        const double vals[4] = {
            1, 2,
            3, 4};
        matrix_t *A = mat_create_d(2, 2, vals);

        print_md("A (2x2)", A);

        double det;
        mat_det(A, &det);
        check_d("det [[1 2][3 4]] = -2", det, -2.0, 1e-30);

        mat_free(A);
    }

    /* -------------------------------------------------------------- 3×3 */
    {
        const double vals[9] = {
            6, 1, 1,
            4, -2, 5,
            2, 8, 7};
        matrix_t *A = mat_create_d(3, 3, vals);

        print_md("A (3x3)", A);

        double det;
        mat_det(A, &det);
        check_d("det 3x3 example = -306", det, -306.0, 1e-30);

        mat_free(A);
    }

    /* -------------------------------------------------------------- singular */
    {
        const double vals[4] = {
            1, 2,
            2, 4};
        matrix_t *A = mat_create_d(2, 2, vals);

        print_md("A (singular)", A);

        double det;
        mat_det(A, &det);
        check_d("det singular = 0", det, 0.0, 1e-30);

        mat_free(A);
    }

    /* -------------------------------------------------------------- identity */
    {
        matrix_t *I = mat_create_identity_d(4);

        print_md("I (identity)", I);

        double det;
        mat_det(I, &det);
        check_d("det identity = 1", det, 1.0, 1e-30);

        mat_free(I);
    }
}

/* ------------------------------------------------------------------ determinant: qfloat */

static void test_det_qfloat(void)
{
    printf(C_CYAN "TEST: determinant (qfloat)\n" C_RESET);

    /* original values preserved */
    qfloat_t vals[4] = {
        qf_from_double(1.5), qf_from_double(2.0),
        qf_from_double(-3.0), qf_from_double(4.25)};

    matrix_t *A = mat_create_qf(2, 2, vals);
    print_mqf("A (qfloat 2x2)", A);

    qfloat_t det;
    mat_det(A, &det);

    qfloat_t expected =
        qf_sub(qf_mul(vals[0], vals[3]),
               qf_mul(vals[1], vals[2]));

    check_qf_val("det qfloat 2x2", det, expected, 1e-28);

    mat_free(A);
}

/* ------------------------------------------------------------------ determinant: qcomplex */

static void test_det_qcomplex(void)
{
    printf(C_CYAN "TEST: determinant (qcomplex)\n" C_RESET);

    /* original values preserved */
    qcomplex_t vals[4] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(3), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(4)),
        qc_make(qf_from_double(-2), qf_from_double(1))};

    matrix_t *A = mat_create_qc(2, 2, vals);
    print_mqc("A (qcomplex 2x2)", A);

    qcomplex_t det;
    mat_det(A, &det);

    qcomplex_t expected =
        qc_sub(qc_mul(vals[0], vals[3]),
               qc_mul(vals[1], vals[2]));

    check_qc_val("det qcomplex 2x2", det, expected, 1e-28);

    mat_free(A);
}

/* ------------------------------------------------------------------ inverse: double */

static void test_inverse_double(void)
{
    printf(C_CYAN "TEST: matrix inverse (double)\n" C_RESET);

    /* 2×2 invertible */
    {
        double A_vals[4] = {4, 7, 2, 6};
        matrix_t *A = mat_create_d(2, 2, A_vals);

        print_md("A", A);

        matrix_t *Ai = mat_inverse(A);
        check_bool("inverse returned non-null", Ai != NULL);

        print_md("A^{-1}", Ai);

        /* Check A * A^{-1} = I */
        matrix_t *P = mat_mul(A, Ai);
        print_md("A * A^{-1}", P);

        double v;
        mat_get(P, 0, 0, &v);
        check_d("prod[0,0] = 1", v, 1, 1e-12);
        mat_get(P, 1, 1, &v);
        check_d("prod[1,1] = 1", v, 1, 1e-12);
        mat_get(P, 0, 1, &v);
        check_d("prod[0,1] = 0", v, 0, 1e-12);
        mat_get(P, 1, 0, &v);
        check_d("prod[1,0] = 0", v, 0, 1e-12);

        mat_free(A);
        mat_free(Ai);
        mat_free(P);
    }

    /* identity inverse */
    {
        matrix_t *I = mat_create_identity_d(3);
        print_md("I", I);

        matrix_t *Ii = mat_inverse(I);
        check_bool("inverse(identity) non-null", Ii != NULL);

        print_md("I^{-1}", Ii);

        double v;
        mat_get(Ii, 0, 0, &v);
        check_d("I^{-1}[0,0] = 1", v, 1, 1e-30);
        mat_get(Ii, 1, 1, &v);
        check_d("I^{-1}[1,1] = 1", v, 1, 1e-30);
        mat_get(Ii, 2, 2, &v);
        check_d("I^{-1}[2,2] = 1", v, 1, 1e-30);

        mat_free(I);
        mat_free(Ii);
    }

    /* singular matrix */
    {
        double A_vals[4] = {1, 2, 2, 4};
        matrix_t *A = mat_create_d(2, 2, A_vals);

        print_md("A (singular)", A);

        matrix_t *Ai = mat_inverse(A);
        check_bool("inverse(singular) = NULL", Ai == NULL);

        mat_free(A);
    }
}

/* ------------------------------------------------------------------ inverse: qfloat */

static void test_inverse_qfloat(void)
{
    printf(C_CYAN "TEST: matrix inverse (qfloat)\n" C_RESET);

    qfloat_t A_vals[4] = {
        qf_from_double(3.0), qf_from_double(1.0),
        qf_from_double(2.0), qf_from_double(1.0)};
    matrix_t *A = mat_create_qf(2, 2, A_vals);

    print_mqf("A", A);

    matrix_t *Ai = mat_inverse(A);
    check_bool("inverse returned non-null", Ai != NULL);

    print_mqf("A^{-1}", Ai);

    /* Check A * A^{-1} = I */
    matrix_t *P = mat_mul(A, Ai);
    print_mqf("A * A^{-1}", P);

    qfloat_t v;
    mat_get(P, 0, 0, &v);
    check_qf_val("prod[0,0] = 1", v, QF_ONE, 1e-28);
    mat_get(P, 1, 1, &v);
    check_qf_val("prod[1,1] = 1", v, QF_ONE, 1e-28);

    qfloat_t z = QF_ZERO;
    mat_get(P, 0, 1, &v);
    check_qf_val("prod[0,1] = 0", v, z, 1e-28);
    mat_get(P, 1, 0, &v);
    check_qf_val("prod[1,0] = 0", v, z, 1e-28);

    mat_free(A);
    mat_free(Ai);
    mat_free(P);
}

/* ------------------------------------------------------------------ inverse: qcomplex */

static void test_inverse_qcomplex(void)
{
    printf(C_CYAN "TEST: matrix inverse (qcomplex)\n" C_RESET);

    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(2), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(3)),
        qc_make(qf_from_double(4), qf_from_double(0))};
    matrix_t *A = mat_create_qc(2, 2, A_vals);

    print_mqc("A", A);

    matrix_t *Ai = mat_inverse(A);
    check_bool("inverse returned non-null", Ai != NULL);

    print_mqc("A^{-1}", Ai);

    /* Check A * A^{-1} = I */
    matrix_t *P = mat_mul(A, Ai);
    print_mqc("A * A^{-1}", P);

    qcomplex_t got;

    mat_get(P, 0, 0, &got);
    check_qc_val("prod[0,0] = 1", got, QC_ONE, 1e-28);

    mat_get(P, 1, 1, &got);
    check_qc_val("prod[1,1] = 1", got, QC_ONE, 1e-28);

    qcomplex_t zero = QC_ZERO;

    mat_get(P, 0, 1, &got);
    check_qc_val("prod[0,1] = 0", got, zero, 1e-28);

    mat_get(P, 1, 0, &got);
    check_qc_val("prod[1,0] = 0", got, zero, 1e-28);

    mat_free(A);
    mat_free(Ai);
    mat_free(P);
}

/* ------------------------------------------------------------------ Hermitian (conjugate transpose) */

static void test_hermitian_op(void)
{
    printf(C_CYAN "TEST: Hermitian operator (A -> A^†)\n" C_RESET);

    /* double: Hermitian = transpose */
    {
        matrix_t *A = mat_new_d(2, 3);
        double x = 1, y = 2, z = 3;
        mat_set(A, 0, 1, &x);
        mat_set(A, 1, 2, &y);
        mat_set(A, 0, 2, &z);

        print_md("A (double)", A);

        matrix_t *H = mat_hermitian(A);
        print_md("A^† (double)", H);

        double v;
        mat_get(H, 1, 0, &v);
        check_d("H[1,0] = 1", v, 1, 1e-30);
        mat_get(H, 2, 1, &v);
        check_d("H[2,1] = 2", v, 2, 1e-30);
        mat_get(H, 2, 0, &v);
        check_d("H[2,0] = 3", v, 3, 1e-30);

        mat_free(A);
        mat_free(H);
    }

    /* qcomplex: Hermitian = conjugate transpose */
    {
        qcomplex_t z11 = qc_make(qf_from_double(1), qf_from_double(2));
        qcomplex_t z12 = qc_make(qf_from_double(3), qf_from_double(-1));
        qcomplex_t z21 = qc_make(qf_from_double(4), qf_from_double(5));
        qcomplex_t z22 = qc_make(qf_from_double(-2), qf_from_double(0));
        qcomplex_t A_vals[4] = {z11, z12, z21, z22};
        matrix_t *A = mat_create_qc(2, 2, A_vals);

        print_mqc("A (qcomplex)", A);

        matrix_t *H = mat_hermitian(A);
        print_mqc("A^† (qcomplex)", H);

        qcomplex_t got;

        mat_get(H, 0, 0, &got);
        check_qc_val("H[0,0] = conj(1+2i)", got, qc_conj(z11), 1e-28);
        mat_get(H, 1, 1, &got);
        check_qc_val("H[1,1] = conj(-2+0i)", got, qc_conj(z22), 1e-28);
        mat_get(H, 1, 0, &got);
        check_qc_val("H[1,0] = conj(A[0,1])", got, qc_conj(z12), 1e-28);
        mat_get(H, 0, 1, &got);
        check_qc_val("H[0,1] = conj(A[1,0])", got, qc_conj(z21), 1e-28);

        mat_free(A);
        mat_free(H);
    }
}

/* ------------------------------------------------------------------ eigendecomposition: double */

static void test_eigen_d(void)
{
    printf(C_CYAN "TEST: eigendecomposition (double)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    double A_vals[4] = {5, 2, 2, 8};
    matrix_t *A = mat_create_d(2, 2, A_vals);

    print_md("A", A);

    /* eigenvalues only */
    double ev[2];
    mat_eigenvalues(A, ev);
    printf("    eigenvalues (mat_eigenvalues): [%.15g, %.15g]\n", ev[0], ev[1]);

    double lmin = fmin(ev[0], ev[1]);
    double lmax = fmax(ev[0], ev[1]);
    check_d("eigenvalue min = 4", lmin, 4.0, 1e-10);
    check_d("eigenvalue max = 9", lmax, 9.0, 1e-10);

    /* full decomposition */
    double ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_md("eigenvectors V (columns)", V);
    printf("    eigenvalues (mat_eigendecompose): [%.15g, %.15g]\n", ev2[0], ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] for each column k */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            double Av_ik = 0.0;
            for (size_t j = 0; j < 2; j++)
            {
                double aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik += aij * vjk;
            }
            double vik;
            mat_get(V, i, k, &vik);
            double lv_ik = ev2[k] * vik;

            char label[64];
            snprintf(label, sizeof(label), "d: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_d(label, Av_ik, lv_ik, 1e-10);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_md("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qfloat */

static void test_eigen_qf(void)
{
    printf(C_CYAN "TEST: eigendecomposition (qfloat)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    qfloat_t A_vals[4] = {
        qf_from_double(5), qf_from_double(2),
        qf_from_double(2), qf_from_double(8)};
    matrix_t *A = mat_create_qf(2, 2, A_vals);

    print_mqf("A", A);

    /* eigenvalues only */
    qfloat_t ev[2];
    mat_eigenvalues(A, ev);
    print_qf("eigenvalue[0]", ev[0]);
    print_qf("eigenvalue[1]", ev[1]);

    int e0_smaller = qf_to_double(ev[0]) < qf_to_double(ev[1]);
    qfloat_t ev_min = e0_smaller ? ev[0] : ev[1];
    qfloat_t ev_max = e0_smaller ? ev[1] : ev[0];
    check_qf_val("eigenvalue min = 4", ev_min, qf_from_double(4.0), 1e-25);
    check_qf_val("eigenvalue max = 9", ev_max, qf_from_double(9.0), 1e-25);

    /* full decomposition */
    qfloat_t ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_mqf("eigenvectors V (columns)", V);
    print_qf("eigenvalue2[0]", ev2[0]);
    print_qf("eigenvalue2[1]", ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            qfloat_t Av_ik = QF_ZERO;
            for (size_t j = 0; j < 2; j++)
            {
                qfloat_t aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik = qf_add(Av_ik, qf_mul(aij, vjk));
            }
            qfloat_t vik;
            mat_get(V, i, k, &vik);
            qfloat_t lv_ik = qf_mul(ev2[k], vik);

            char label[64];
            snprintf(label, sizeof(label), "qf: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_qf_val(label, Av_ik, lv_ik, 1e-25);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mqf("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ eigendecomposition: qcomplex */

static void test_eigen_qc(void)
{
    printf(C_CYAN "TEST: eigendecomposition (qcomplex Hermitian)\n" C_RESET);

    /* A = [[2, 1+i], [1-i, 3]] — eigenvalues 1 and 4 */
    qcomplex_t A_vals[4] = {
        qc_make(qf_from_double(2), QF_ZERO),
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(1), qf_from_double(-1)),
        qc_make(qf_from_double(3), QF_ZERO)};
    matrix_t *A = mat_create_qc(2, 2, A_vals);

    print_mqc("A", A);

    /* eigenvalues only */
    qcomplex_t ev[2];
    mat_eigenvalues(A, ev);
    print_qc("eigenvalue[0]", ev[0]);
    print_qc("eigenvalue[1]", ev[1]);

    int e0_smaller = qf_to_double(ev[0].re) < qf_to_double(ev[1].re);
    qcomplex_t ev_min = e0_smaller ? ev[0] : ev[1];
    qcomplex_t ev_max = e0_smaller ? ev[1] : ev[0];
    qcomplex_t exp_min = qc_make(qf_from_double(1), QF_ZERO);
    qcomplex_t exp_max = qc_make(qf_from_double(4), QF_ZERO);
    check_qc_val("eigenvalue min = 1+0i", ev_min, exp_min, 1e-25);
    check_qc_val("eigenvalue max = 4+0i", ev_max, exp_max, 1e-25);

    /* full decomposition */
    qcomplex_t ev2[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev2, &V);

    print_mqc("eigenvectors V (columns)", V);
    print_qc("eigenvalue2[0]", ev2[0]);
    print_qc("eigenvalue2[1]", ev2[1]);

    /* verify A*v[k] = lambda[k]*v[k] */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            qcomplex_t Av_ik = QC_ZERO;
            for (size_t j = 0; j < 2; j++)
            {
                qcomplex_t aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik = qc_add(Av_ik, qc_mul(aij, vjk));
            }
            qcomplex_t vik;
            mat_get(V, i, k, &vik);
            qcomplex_t lv_ik = qc_mul(ev2[k], vik);

            char label[64];
            snprintf(label, sizeof(label), "qc: (Av)[%zu,%zu] = lv[%zu,%zu]", i, k, i, k);
            check_qc_val(label, Av_ik, lv_ik, 1e-25);
        }
    }

    /* eigenvectors only */
    matrix_t *V2 = mat_eigenvectors(A);
    print_mqc("eigenvectors (mat_eigenvectors)", V2);

    mat_free(A);
    mat_free(V);
    mat_free(V2);
}

/* ------------------------------------------------------------------ mat_exp */

static void test_mat_exp_d(void)
{
    printf(C_CYAN "TEST: mat_exp (double)\n" C_RESET);

    /* 1×1: exp([[x]]) = [[exp(x)]] */
    {
        double v = 2.0;
        matrix_t *A = mat_create_d(1, 1, &v);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(1x1) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double got;
            mat_get(E, 0, 0, &got);
            check_d("exp([[2]]) = e²", got, exp(2.0), 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }

    /* 2×2 diagonal: exp(diag(a,b)) = diag(exp(a),exp(b)) */
    {
        double A_vals[4] = {1.0, 0.0, 0.0, 2.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(diag) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            check_d("exp(diag)[0,0] = e", e[0], exp(1.0), 1e-12);
            check_d("exp(diag)[1,1] = e²", e[3], exp(2.0), 1e-12);
            check_d("exp(diag)[0,1] = 0", e[1], 0.0, 1e-12);
            check_d("exp(diag)[1,0] = 0", e[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(sym) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            double ch = cosh(1.0), sh = sinh(1.0);
            check_d("exp([[0,1],[1,0]])[0,0] = cosh(1)", e[0], ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,1] = cosh(1)", e[3], ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[0,1] = sinh(1)", e[1], sh, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,0] = sinh(1)", e[2], sh, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }

    /* zero matrix: exp(0) = I */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(zero) not NULL", E != NULL);
        if (E)
        {
            print_md("exp(A)", E);
            double e[4];
            mat_get_data(E, e);
            check_d("exp(0)[0,0] = 1", e[0], 1.0, 1e-12);
            check_d("exp(0)[1,1] = 1", e[3], 1.0, 1e-12);
            check_d("exp(0)[0,1] = 0", e[1], 0.0, 1e-12);
            check_d("exp(0)[1,0] = 0", e[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_qf(void)
{
    printf(C_CYAN "TEST: mat_exp (qfloat)\n" C_RESET);

    /* 1×1: exp([[x]]) = [[exp(x)]] */
    {
        qfloat_t v = qf_from_double(2.0);
        matrix_t *A = mat_create_qf(1, 1, &v);
        print_mqf("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(1x1) not NULL", E != NULL);
        if (E)
        {
            print_mqf("exp(A)", E);
            qfloat_t got;
            mat_get(E, 0, 0, &got);
            check_qf_val("qf exp([[2]]) = e²", got, qf_exp(qf_from_double(2.0)), 1e-25);
        }
        mat_free(A);
        mat_free(E);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        qfloat_t A_vals[4] = {QF_ZERO, QF_ONE, QF_ONE, QF_ZERO};
        matrix_t *A = mat_create_qf(2, 2, A_vals);
        print_mqf("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(sym) not NULL", E != NULL);
        if (E)
        {
            print_mqf("exp(A)", E);
            qfloat_t e[4];
            mat_get_data(E, e);
            /* cosh(1) = (e + 1/e) / 2, sinh(1) = (e - 1/e) / 2 */
            qfloat_t e1 = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t ch = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh = qf_div(qf_sub(e1, inv1), two);
            check_qf_val("qf exp(sym)[0,0] = cosh(1)", e[0], ch, 1e-25);
            check_qf_val("qf exp(sym)[1,1] = cosh(1)", e[3], ch, 1e-25);
            check_qf_val("qf exp(sym)[0,1] = sinh(1)", e[1], sh, 1e-25);
            check_qf_val("qf exp(sym)[1,0] = sinh(1)", e[2], sh, 1e-25);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_qc(void)
{
    printf(C_CYAN "TEST: mat_exp (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        qcomplex_t A_vals[4] = {
            QC_ZERO,
            qc_make(QF_ZERO, QF_ONE),
            qc_make(QF_ZERO, qf_neg(QF_ONE)),
            QC_ZERO};
        matrix_t *A = mat_create_qc(2, 2, A_vals);
        print_mqc("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qc(herm) not NULL", E != NULL);
        if (E)
        {
            print_mqc("exp(A)", E);
            qcomplex_t e[4];
            mat_get_data(E, e);
            qfloat_t e1 = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two = qf_from_double(2.0);
            qfloat_t ch = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh = qf_div(qf_sub(e1, inv1), two);
            qcomplex_t ch_c = qc_make(ch, QF_ZERO);
            qcomplex_t ish = qc_make(QF_ZERO, sh);
            qcomplex_t nish = qc_make(QF_ZERO, qf_neg(sh));
            check_qc_val("qc exp(herm)[0,0] = cosh(1)", e[0], ch_c, 1e-25);
            check_qc_val("qc exp(herm)[1,1] = cosh(1)", e[3], ch_c, 1e-25);
            check_qc_val("qc exp(herm)[0,1] = i·sinh(1)", e[1], ish, 1e-25);
            check_qc_val("qc exp(herm)[1,0] = -i·sinh(1)", e[2], nish, 1e-25);
        }
        mat_free(A);
        mat_free(E);
    }
}

static void test_mat_exp_null_safety(void)
{
    printf(C_CYAN "TEST: mat_exp null safety\n" C_RESET);
    check_bool("mat_exp(NULL) = NULL", mat_exp(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_exp(non-square) = NULL", mat_exp(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sin */

static void test_mat_sin_d(void)
{
    printf(C_CYAN "TEST: mat_sin (double)\n" C_RESET);

    /* 1×1: sin([[π/6]]) = [[0.5]] */
    {
        double v = M_PI / 6.0;
        matrix_t *A = mat_create_d(1, 1, &v);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(1x1) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double got;
            mat_get(S, 0, 0, &got);
            check_d("sin([[π/6]]) = 0.5", got, 0.5, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 diagonal: sin(diag(0, π/2)) = diag(0, 1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI / 2.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            check_d("sin(diag)[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin(diag)[1,1] = 1", s[3], 1.0, 1e-12);
            check_d("sin(diag)[0,1] = 0", s[1], 0.0, 1e-12);
            check_d("sin(diag)[1,0] = 0", s[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            double s1 = sin(1.0);
            check_d("sin([[0,1],[1,0]])[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[1,1] = 0", s[3], 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[0,1] = sin(1)", s[1], s1, 1e-12);
            check_d("sin([[0,1],[1,0]])[1,0] = sin(1)", s[2], s1, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* zero matrix: sin(0) = 0 */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(zero) not NULL", S != NULL);
        if (S)
        {
            print_md("sin(A)", S);
            double s[4];
            mat_get_data(S, s);
            check_d("sin(0)[0,0] = 0", s[0], 0.0, 1e-12);
            check_d("sin(0)[1,1] = 0", s[3], 0.0, 1e-12);
            check_d("sin(0)[0,1] = 0", s[1], 0.0, 1e-12);
            check_d("sin(0)[1,0] = 0", s[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_qf(void)
{
    printf(C_CYAN "TEST: mat_sin (qfloat)\n" C_RESET);

    /* 1×1: sin([[π/6]]) = [[0.5]] */
    {
        qfloat_t v = qf_div(QF_PI, qf_from_double(6.0));
        matrix_t *A = mat_create_qf(1, 1, &v);
        print_mqf("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(1x1) not NULL", S != NULL);
        if (S)
        {
            print_mqf("sin(A)", S);
            qfloat_t got;
            mat_get(S, 0, 0, &got);
            check_qf_val("qf sin([[π/6]]) = 0.5", got, qf_from_double(0.5), 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → sin(A) = sin(1)·A */
    {
        qfloat_t A_vals[4] = {QF_ZERO, QF_ONE, QF_ONE, QF_ZERO};
        matrix_t *A = mat_create_qf(2, 2, A_vals);
        print_mqf("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(sym) not NULL", S != NULL);
        if (S)
        {
            print_mqf("sin(A)", S);
            qfloat_t s[4];
            mat_get_data(S, s);
            qfloat_t s1 = qf_sin(QF_ONE);
            check_qf_val("qf sin(sym)[0,0] = 0", s[0], QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[1,1] = 0", s[3], QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[0,1] = sin(1)", s[1], s1, 1e-25);
            check_qf_val("qf sin(sym)[1,0] = sin(1)", s[2], s1, 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_qc(void)
{
    printf(C_CYAN "TEST: mat_sin (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A = [[0, i·sin(1)], [-i·sin(1), 0]] */
    {
        qcomplex_t A_vals[4] = {
            QC_ZERO,
            qc_make(QF_ZERO, QF_ONE),
            qc_make(QF_ZERO, qf_neg(QF_ONE)),
            QC_ZERO};
        matrix_t *A = mat_create_qc(2, 2, A_vals);
        print_mqc("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qc(herm) not NULL", S != NULL);
        if (S)
        {
            print_mqc("sin(A)", S);
            qcomplex_t s[4];
            mat_get_data(S, s);
            qfloat_t s1 = qf_sin(QF_ONE);
            qcomplex_t zero_c = QC_ZERO;
            qcomplex_t ish = qc_make(QF_ZERO, s1);
            qcomplex_t nish = qc_make(QF_ZERO, qf_neg(s1));
            check_qc_val("qc sin(herm)[0,0] = 0", s[0], zero_c, 1e-25);
            check_qc_val("qc sin(herm)[1,1] = 0", s[3], zero_c, 1e-25);
            check_qc_val("qc sin(herm)[0,1] = i·sin(1)", s[1], ish, 1e-25);
            check_qc_val("qc sin(herm)[1,0] = -i·sin(1)", s[2], nish, 1e-25);
        }
        mat_free(A);
        mat_free(S);
    }
}

static void test_mat_sin_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sin null safety\n" C_RESET);
    check_bool("mat_sin(NULL) = NULL", mat_sin(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_sin(non-square) = NULL", mat_sin(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_cos */

static void test_mat_cos_d(void)
{
    printf(C_CYAN "TEST: mat_cos (double)\n" C_RESET);

    /* 1×1: cos([[π/3]]) = 0.5 */
    {
        double v = M_PI / 3.0;
        matrix_t *A = mat_create_d(1, 1, &v);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(1x1) not NULL", C != NULL);
        if (C)
        {
            print_md("cos(A)", C);
            double got;
            mat_get(C, 0, 0, &got);
            check_d("cos([[π/3]]) = 0.5", got, 0.5, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 diagonal: cos(diag(0, π)) = diag(1, -1) */
    {
        double A_vals[4] = {0.0, 0.0, 0.0, M_PI};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(diag) not NULL", C != NULL);
        if (C)
        {
            print_md("cos(A)", C);
            double c[4];
            mat_get_data(C, c);
            check_d("cos(diag)[0,0] =  1", c[0], 1.0, 1e-12);
            check_d("cos(diag)[1,1] = -1", c[3], -1.0, 1e-12);
            check_d("cos(diag)[0,1] =  0", c[1], 0.0, 1e-12);
            check_d("cos(diag)[1,0] =  0", c[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        double A_vals[4] = {0.0, 1.0, 1.0, 0.0};
        matrix_t *A = mat_create_d(2, 2, A_vals);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(sym) not NULL", C != NULL);
        if (C)
        {
            print_md("cos(A)", C);
            double c[4];
            mat_get_data(C, c);
            double c1 = cos(1.0);
            check_d("cos([[0,1],[1,0]])[0,0] = cos(1)", c[0], c1, 1e-12);
            check_d("cos([[0,1],[1,0]])[1,1] = cos(1)", c[3], c1, 1e-12);
            check_d("cos([[0,1],[1,0]])[0,1] = 0", c[1], 0.0, 1e-12);
            check_d("cos([[0,1],[1,0]])[1,0] = 0", c[2], 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }
}

static void test_mat_cos_qf(void)
{
    printf(C_CYAN "TEST: mat_cos (qfloat)\n" C_RESET);

    /* 1×1: cos([[π/3]]) = 0.5 */
    {
        matrix_t *A = mat_new_qf(1, 1);
        qfloat_t v = qf_div(QF_PI, qf_from_double(3.0));
        mat_set(A, 0, 0, &v);
        print_mqf("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(1x1) not NULL", C != NULL);
        if (C)
        {
            print_mqf("cos(A)", C);
            qfloat_t got;
            mat_get(C, 0, 0, &got);
            check_qf_val("qf cos([[π/3]]) = 0.5", got, qf_from_double(0.5), 1e-25);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_new_qf(2, 2);
        qfloat_t z = QF_ZERO, o = QF_ONE;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_mqf("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(sym) not NULL", C != NULL);
        if (C)
        {
            print_mqf("cos(A)", C);
            qfloat_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            check_qf_val("qf cos(sym)[0,0] = cos(1)", c00, c1, 1e-25);
            check_qf_val("qf cos(sym)[1,1] = cos(1)", c11, c1, 1e-25);
            check_qf_val("qf cos(sym)[0,1] = 0", c01, QF_ZERO, 1e-25);
            check_qf_val("qf cos(sym)[1,0] = 0", c10, QF_ZERO, 1e-25);
        }
        mat_free(A);
        mat_free(C);
    }
}

static void test_mat_cos_qc(void)
{
    printf(C_CYAN "TEST: mat_cos (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_new_qc(2, 2);
        qcomplex_t z = QC_ZERO;
        qcomplex_t pi = qc_make(QF_ZERO, QF_ONE);
        qcomplex_t ni = qc_make(QF_ZERO, qf_neg(QF_ONE));
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &pi);
        mat_set(A, 1, 0, &ni);
        mat_set(A, 1, 1, &z);
        print_mqc("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qc(herm) not NULL", C != NULL);
        if (C)
        {
            print_mqc("cos(A)", C);
            qcomplex_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            qcomplex_t c1_c = qc_make(c1, QF_ZERO);
            qcomplex_t zero_c = QC_ZERO;
            check_qc_val("qc cos(herm)[0,0] = cos(1)", c00, c1_c, 1e-25);
            check_qc_val("qc cos(herm)[1,1] = cos(1)", c11, c1_c, 1e-25);
            check_qc_val("qc cos(herm)[0,1] = 0", c01, zero_c, 1e-25);
            check_qc_val("qc cos(herm)[1,0] = 0", c10, zero_c, 1e-25);
        }
        mat_free(A);
        mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tan */

static void test_mat_tan_d(void)
{
    printf(C_CYAN "TEST: mat_tan (double)\n" C_RESET);

    /* 1×1: tan([[π/4]]) = 1 */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = M_PI / 4.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(1x1) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            double got;
            mat_get(T, 0, 0, &got);
            check_d("tan([[π/4]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 diagonal: tan(diag(0, π/4)) = diag(0, 1) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, h = M_PI / 4.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &h);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            check_d("tan(diag)[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tan(diag)[1,1] = 1", t11, 1.0, 1e-12);
            check_d("tan(diag)[0,1] = 0", t01, 0.0, 1e-12);
            check_d("tan(diag)[1,0] = 0", t10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tan is odd → tan(A) = tan(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(sym) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            double t1 = tan(1.0);
            check_d("tan([[0,1],[1,0]])[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[1,1] = 0", t11, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[0,1] = tan(1)", t01, t1, 1e-12);
            check_d("tan([[0,1],[1,0]])[1,0] = tan(1)", t10, t1, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }
}

/* ------------------------------------------------------------------ mat_sinh */

static void test_mat_sinh_d(void)
{
    printf(C_CYAN "TEST: mat_sinh (double)\n" C_RESET);

    /* 1×1: sinh([[0]]) = 0 */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(1x1) not NULL", S != NULL);
        if (S)
        {
            print_md("sinh(A)", S);
            double got;
            mat_get(S, 0, 0, &got);
            check_d("sinh([[0]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 diagonal: sinh(diag(0, 1)) = diag(0, sinh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("sinh(diag)[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sinh(diag)[1,1] = sinh(1)", s11, sinh(1.0), 1e-12);
            check_d("sinh(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sinh(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * sinh is odd → sinh(A) = sinh(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            double sh = sinh(1.0);
            check_d("sinh([[0,1],[1,0]])[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[1,1] = 0", s11, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[0,1] = sinh(1)", s01, sh, 1e-12);
            check_d("sinh([[0,1],[1,0]])[1,0] = sinh(1)", s10, sh, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }
}

/* ------------------------------------------------------------------ mat_cosh */

static void test_mat_cosh_d(void)
{
    printf(C_CYAN "TEST: mat_cosh (double)\n" C_RESET);

    /* 1×1: cosh([[0]]) = 1 */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(1x1) not NULL", C != NULL);
        if (C)
        {
            print_md("cosh(A)", C);
            double got;
            mat_get(C, 0, 0, &got);
            check_d("cosh([[0]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 diagonal: cosh(diag(0, 1)) = diag(1, cosh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(diag) not NULL", C != NULL);
        if (C)
        {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            check_d("cosh(diag)[0,0] = 1", c00, 1.0, 1e-12);
            check_d("cosh(diag)[1,1] = cosh(1)", c11, cosh(1.0), 1e-12);
            check_d("cosh(diag)[0,1] = 0", c01, 0.0, 1e-12);
            check_d("cosh(diag)[1,0] = 0", c10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cosh is even → cosh(A) = cosh(1)·I */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(sym) not NULL", C != NULL);
        if (C)
        {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00);
            mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10);
            mat_get(C, 1, 1, &c11);
            double ch = cosh(1.0);
            check_d("cosh([[0,1],[1,0]])[0,0] = cosh(1)", c00, ch, 1e-12);
            check_d("cosh([[0,1],[1,0]])[1,1] = cosh(1)", c11, ch, 1e-12);
            check_d("cosh([[0,1],[1,0]])[0,1] = 0", c01, 0.0, 1e-12);
            check_d("cosh([[0,1],[1,0]])[1,0] = 0", c10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tanh */

static void test_mat_tanh_d(void)
{
    printf(C_CYAN "TEST: mat_tanh (double)\n" C_RESET);

    /* 1×1: tanh([[0]]) = 0 */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(1x1) not NULL", T != NULL);
        if (T)
        {
            print_md("tanh(A)", T);
            double got;
            mat_get(T, 0, 0, &got);
            check_d("tanh([[0]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 diagonal: tanh(diag(0, 1)) = diag(0, tanh(1)) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            check_d("tanh(diag)[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tanh(diag)[1,1] = tanh(1)", t11, tanh(1.0), 1e-12);
            check_d("tanh(diag)[0,1] = 0", t01, 0.0, 1e-12);
            check_d("tanh(diag)[1,0] = 0", t10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tanh is odd → tanh(A) = tanh(1)·A */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0;
        mat_set(A, 0, 0, &z);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(sym) not NULL", T != NULL);
        if (T)
        {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00);
            mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10);
            mat_get(T, 1, 1, &t11);
            double th = tanh(1.0);
            check_d("tanh([[0,1],[1,0]])[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[1,1] = 0", t11, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[0,1] = tanh(1)", t01, th, 1e-12);
            check_d("tanh([[0,1],[1,0]])[1,0] = tanh(1)", t10, th, 1e-12);
        }
        mat_free(A);
        mat_free(T);
    }
}

static void test_mat_trig_null_safety(void)
{
    printf(C_CYAN "TEST: mat_cos/tan/sinh/cosh/tanh null safety\n" C_RESET);

    check_bool("mat_cos(NULL) = NULL", mat_cos(NULL) == NULL);
    check_bool("mat_tan(NULL) = NULL", mat_tan(NULL) == NULL);
    check_bool("mat_sinh(NULL) = NULL", mat_sinh(NULL) == NULL);
    check_bool("mat_cosh(NULL) = NULL", mat_cosh(NULL) == NULL);
    check_bool("mat_tanh(NULL) = NULL", mat_tanh(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_cos(non-square) = NULL", mat_cos(A) == NULL);
    check_bool("mat_tan(non-square) = NULL", mat_tan(A) == NULL);
    check_bool("mat_sinh(non-square) = NULL", mat_sinh(A) == NULL);
    check_bool("mat_cosh(non-square) = NULL", mat_cosh(A) == NULL);
    check_bool("mat_tanh(non-square) = NULL", mat_tanh(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sqrt */

static void test_mat_sqrt_d(void)
{
    printf(C_CYAN "TEST: mat_sqrt (double)\n" C_RESET);

    /* 1×1: sqrt([[4]]) = [[2]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 4.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(1x1) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            double got;
            mat_get(S, 0, 0, &got);
            check_d("sqrt([[4]]) = 2", got, 2.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* 2×2 diagonal: sqrt(diag(1,4)) = diag(1,2) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, o = 1.0, f = 4.0;
        mat_set(A, 0, 0, &o);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &f);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00);
            mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10);
            mat_get(S, 1, 1, &s11);
            check_d("sqrt(diag)[0,0] = 1", s00, 1.0, 1e-12);
            check_d("sqrt(diag)[1,1] = 2", s11, 2.0, 1e-12);
            check_d("sqrt(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sqrt(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(S);
    }

    /* Verify: sqrt(A)^2 = A for [[2,1],[1,2]] (eigenvalues 1 and 3) */
    {
        matrix_t *A = mat_new_d(2, 2);
        double o = 1.0, t = 2.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o);
        mat_set(A, 1, 1, &t);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            matrix_t *S2 = mat_mul(S, S);
            check_bool("sqrt(A)^2 not NULL", S2 != NULL);
            if (S2)
            {
                print_md("sqrt(A)^2", S2);
                double r00, r01, r10, r11;
                mat_get(S2, 0, 0, &r00);
                mat_get(S2, 0, 1, &r01);
                mat_get(S2, 1, 0, &r10);
                mat_get(S2, 1, 1, &r11);
                check_d("sqrt(A)^2[0,0] = 2", r00, 2.0, 1e-10);
                check_d("sqrt(A)^2[1,1] = 2", r11, 2.0, 1e-10);
                check_d("sqrt(A)^2[0,1] = 1", r01, 1.0, 1e-10);
                check_d("sqrt(A)^2[1,0] = 1", r10, 1.0, 1e-10);
                mat_free(S2);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_sqrt_qf(void)
{
    printf(C_CYAN "TEST: mat_sqrt (qfloat)\n" C_RESET);

    /* 1×1: sqrt([[9]]) = [[3]] */
    matrix_t *A = mat_new_qf(1, 1);
    qfloat_t v = qf_from_double(9.0);
    mat_set(A, 0, 0, &v);
    print_mqf("A", A);
    matrix_t *S = mat_sqrt(A);
    check_bool("qf mat_sqrt(1x1) not NULL", S != NULL);
    if (S)
    {
        print_mqf("sqrt(A)", S);
        qfloat_t got;
        mat_get(S, 0, 0, &got);
        check_qf_val("qf sqrt([[9]]) = 3", got, qf_from_double(3.0), 1e-25);
    }
    mat_free(A);
    mat_free(S);
}

/* ------------------------------------------------------------------ mat_log */

static void test_mat_log_d(void)
{
    printf(C_CYAN "TEST: mat_log (double)\n" C_RESET);

    /* 1×1: log([[1]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 1.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log([[1]]) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            double got;
            mat_get(L, 0, 0, &got);
            check_d("log([[1]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A);
        mat_free(L);
    }

    /* 1×1: log([[e]]) = [[1]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = M_E;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log([[e]]) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            double got;
            mat_get(L, 0, 0, &got);
            check_d("log([[e]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A);
        mat_free(L);
    }

    /* exp(log(A)) = A for diagonal [[2,0],[0,3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, t = 2.0, th = 3.0;
        mat_set(A, 0, 0, &t);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &th);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log(diag) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *E = mat_exp(L);
            check_bool("exp(log(A)) not NULL", E != NULL);
            if (E)
            {
                print_md("exp(log(A))", E);
                double e00, e01, e10, e11;
                mat_get(E, 0, 0, &e00);
                mat_get(E, 0, 1, &e01);
                mat_get(E, 1, 0, &e10);
                mat_get(E, 1, 1, &e11);
                check_d("exp(log(diag))[0,0] = 2", e00, 2.0, 1e-10);
                check_d("exp(log(diag))[1,1] = 3", e11, 3.0, 1e-10);
                check_d("exp(log(diag))[0,1] = 0", e01, 0.0, 1e-10);
                check_d("exp(log(diag))[1,0] = 0", e10, 0.0, 1e-10);
                mat_free(E);
            }
            mat_free(L);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_asin / mat_acos */

static void test_mat_asin_d(void)
{
    printf(C_CYAN "TEST: mat_asin (double)\n" C_RESET);

    /* 1×1: asin([[0]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_asin(A);
        check_bool("mat_asin([[0]]) not NULL", R != NULL);
        if (R)
        {
            double got;
            mat_get(R, 0, 0, &got);
            check_d("asin([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* 1×1: asin([[0.5]]) = [[π/6]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_asin(A);
        check_bool("mat_asin([[0.5]]) not NULL", R != NULL);
        if (R)
        {
            print_md("asin(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("asin([[0.5]]) = π/6", got, M_PI / 6.0, 1e-12);
        }
        mat_free(A);
        mat_free(R);
    }

    /* sin(asin(A)) = A for 2×2 symmetric with small eigenvalues */
    {
        matrix_t *A = mat_new_d(2, 2);
        double v00 = 0.3, v01 = 0.1, v10 = 0.1, v11 = 0.2;
        mat_set(A, 0, 0, &v00);
        mat_set(A, 0, 1, &v01);
        mat_set(A, 1, 0, &v10);
        mat_set(A, 1, 1, &v11);
        print_md("A", A);
        matrix_t *S = mat_asin(A);
        check_bool("mat_asin(sym) not NULL", S != NULL);
        if (S)
        {
            print_md("asin(A)", S);
            matrix_t *R = mat_sin(S);
            check_bool("sin(asin(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sin(asin(A))", R);
                double r00, r01, r10, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 0, 1, &r01);
                mat_get(R, 1, 0, &r10);
                mat_get(R, 1, 1, &r11);
                check_d("sin(asin(A))[0,0] = 0.3", r00, 0.3, 1e-10);
                check_d("sin(asin(A))[0,1] = 0.1", r01, 0.1, 1e-10);
                check_d("sin(asin(A))[1,0] = 0.1", r10, 0.1, 1e-10);
                check_d("sin(asin(A))[1,1] = 0.2", r11, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_acos_d(void)
{
    printf(C_CYAN "TEST: mat_acos (double)\n" C_RESET);

    /* 1×1: acos([[0.5]]) = [[π/3]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_acos(A);
        check_bool("mat_acos([[0.5]]) not NULL", R != NULL);
        if (R)
        {
            print_md("acos(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("acos([[0.5]]) = π/3", got, M_PI / 3.0, 1e-12);
        }
        mat_free(A);
        mat_free(R);
    }

    /* asin(A) + acos(A) = (π/2)·I for diagonal [[0.3, 0], [0, 0.4]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, v0 = 0.3, v1 = 0.4;
        mat_set(A, 0, 0, &v0);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &v1);
        print_md("A", A);
        matrix_t *AS = mat_asin(A);
        matrix_t *AC = mat_acos(A);
        check_bool("mat_asin+mat_acos: both not NULL", AS != NULL && AC != NULL);
        if (AS && AC)
        {
            matrix_t *SUM = mat_add(AS, AC);
            check_bool("asin+acos not NULL", SUM != NULL);
            if (SUM)
            {
                print_md("asin(A)+acos(A)", SUM);
                double s00, s01, s10, s11;
                mat_get(SUM, 0, 0, &s00);
                mat_get(SUM, 0, 1, &s01);
                mat_get(SUM, 1, 0, &s10);
                mat_get(SUM, 1, 1, &s11);
                check_d("asin+acos[0,0] = π/2", s00, M_PI / 2.0, 1e-10);
                check_d("asin+acos[1,1] = π/2", s11, M_PI / 2.0, 1e-10);
                check_d("asin+acos[0,1] = 0", s01, 0.0, 1e-10);
                check_d("asin+acos[1,0] = 0", s10, 0.0, 1e-10);
                mat_free(SUM);
            }
        }
        mat_free(AS);
        mat_free(AC);
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_atan */

static void test_mat_atan_d(void)
{
    printf(C_CYAN "TEST: mat_atan (double)\n" C_RESET);

    /* 1×1: atan([[0]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_atan(A);
        check_bool("mat_atan([[0]]) not NULL", R != NULL);
        if (R)
        {
            double got;
            mat_get(R, 0, 0, &got);
            check_d("atan([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* 1×1: atan([[0.5]]) — use 0.5 (well within convergence radius) */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_atan(A);
        check_bool("mat_atan([[0.5]]) not NULL", R != NULL);
        if (R)
        {
            print_md("atan(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("atan([[0.5]]) = atan(0.5)", got, atan(0.5), 1e-12);
        }
        mat_free(A);
        mat_free(R);
    }

    /* tan(atan(A)) = A for small diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atan(A);
        check_bool("mat_atan(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("atan(A)", T);
            matrix_t *R = mat_tan(T);
            check_bool("tan(atan(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("tan(atan(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("tan(atan(A))[0,0] = 0.5", r00, 0.5, 1e-10);
                check_d("tan(atan(A))[1,1] = 0.3", r11, 0.3, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_asinh / mat_acosh / mat_atanh */

static void test_mat_asinh_d(void)
{
    printf(C_CYAN "TEST: mat_asinh (double)\n" C_RESET);

    /* 1×1: asinh([[0]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_asinh(A);
        check_bool("mat_asinh([[0]]) not NULL", R != NULL);
        if (R)
        {
            double got;
            mat_get(R, 0, 0, &got);
            check_d("asinh([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* sinh(asinh(A)) = A for diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.5, b = 0.3;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("mat_asinh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sinh(asinh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("sinh(asinh(A))[0,0] = 0.5", r00, 0.5, 1e-10);
                check_d("sinh(asinh(A))[1,1] = 0.3", r11, 0.3, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_acosh_d(void)
{
    printf(C_CYAN "TEST: mat_acosh (double)\n" C_RESET);

    /* 1×1: acosh([[1]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 1.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_acosh(A);
        check_bool("mat_acosh([[1]]) not NULL", R != NULL);
        if (R)
        {
            double got;
            mat_get(R, 0, 0, &got);
            check_d("acosh([[1]]) = 0", got, 0.0, 1e-10);
            mat_free(R);
        }
        mat_free(A);
    }

    /* cosh(acosh(A)) = A for diagonal [[2, 0],[0, 3]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 2.0, b = 3.0;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_acosh(A);
        check_bool("mat_acosh(diag) not NULL", S != NULL);
        if (S)
        {
            print_md("acosh(A)", S);
            matrix_t *R = mat_cosh(S);
            check_bool("cosh(acosh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("cosh(acosh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("cosh(acosh(A))[0,0] = 2", r00, 2.0, 1e-10);
                check_d("cosh(acosh(A))[1,1] = 3", r11, 3.0, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }
}

static void test_mat_atanh_d(void)
{
    printf(C_CYAN "TEST: mat_atanh (double)\n" C_RESET);

    /* 1×1: atanh([[0]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_atanh(A);
        check_bool("mat_atanh([[0]]) not NULL", R != NULL);
        if (R)
        {
            double got;
            mat_get(R, 0, 0, &got);
            check_d("atanh([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* tanh(atanh(A)) = A for diagonal [[0.4, 0],[0, 0.2]] */
    {
        matrix_t *A = mat_new_d(2, 2);
        double z = 0.0, a = 0.4, b = 0.2;
        mat_set(A, 0, 0, &a);
        mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);
        mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atanh(A);
        check_bool("mat_atanh(diag) not NULL", T != NULL);
        if (T)
        {
            print_md("atanh(A)", T);
            matrix_t *R = mat_tanh(T);
            check_bool("tanh(atanh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("tanh(atanh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00);
                mat_get(R, 1, 1, &r11);
                check_d("tanh(atanh(A))[0,0] = 0.4", r00, 0.4, 1e-10);
                check_d("tanh(atanh(A))[1,1] = 0.2", r11, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

static void test_mat_inv_trig_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sqrt/log/asin/acos/atan/asinh/acosh/atanh null safety\n" C_RESET);
    check_bool("mat_sqrt(NULL)  = NULL", mat_sqrt(NULL) == NULL);
    check_bool("mat_log(NULL)   = NULL", mat_log(NULL) == NULL);
    check_bool("mat_asin(NULL)  = NULL", mat_asin(NULL) == NULL);
    check_bool("mat_acos(NULL)  = NULL", mat_acos(NULL) == NULL);
    check_bool("mat_atan(NULL)  = NULL", mat_atan(NULL) == NULL);
    check_bool("mat_asinh(NULL) = NULL", mat_asinh(NULL) == NULL);
    check_bool("mat_acosh(NULL) = NULL", mat_acosh(NULL) == NULL);
    check_bool("mat_atanh(NULL) = NULL", mat_atanh(NULL) == NULL);

    matrix_t *A = mat_new_d(2, 3);
    check_bool("mat_sqrt(non-sq)  = NULL", mat_sqrt(A) == NULL);
    check_bool("mat_log(non-sq)   = NULL", mat_log(A) == NULL);
    check_bool("mat_asin(non-sq)  = NULL", mat_asin(A) == NULL);
    check_bool("mat_acos(non-sq)  = NULL", mat_acos(A) == NULL);
    check_bool("mat_atan(non-sq)  = NULL", mat_atan(A) == NULL);
    check_bool("mat_asinh(non-sq) = NULL", mat_asinh(A) == NULL);
    check_bool("mat_acosh(non-sq) = NULL", mat_acosh(A) == NULL);
    check_bool("mat_atanh(non-sq) = NULL", mat_atanh(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ general eigendecompose */

static void test_eigen_general_d(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, double)\n" C_RESET);

    /* A = [[3, 1], [0, 2]] — upper triangular, eigenvalues 2 and 3.
     * eigenvector for λ=3: [1, 0]
     * eigenvector for λ=2: [-1, 1] (normalised) */
    matrix_t *A = mat_new_d(2, 2);
    double a00 = 3.0, a01 = 1.0, a10 = 0.0, a11 = 2.0;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);
    print_md("A", A);

    double eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("mat_eigendecompose_general: rc = 0", rc == 0);
    check_bool("mat_eigendecompose_general: V not NULL", V != NULL);

    if (V)
    {
        print_md("V (eigenvectors)", V);
        /* Verify A·v_j = lambda_j · v_j for each column */
        for (int j = 0; j < 2; j++)
        {
            double lam = eigenvalues[j];
            double v0, v1;
            mat_get(V, 0, j, &v0);
            mat_get(V, 1, j, &v1);
            /* [Av]_0 = 3*v0 + 1*v1, [Av]_1 = 2*v1 */
            double av0 = 3.0 * v0 + 1.0 * v1;
            double av1 = 2.0 * v1;
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "(Av)[0,%d] = lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "(Av)[1,%d] = lam*v[1,%d]", j, j);
            check_d(label0, av0, lam * v0, 1e-10);
            check_d(label1, av1, lam * v1, 1e-10);
        }
        mat_free(V);
    }
    mat_free(A);
}

static void test_eigen_general_qf(void)
{
    printf(C_CYAN "TEST: eigendecompose general (non-Hermitian, qfloat)\n" C_RESET);

    /* A = [[4, 1], [0, 1]] — eigenvalues 1 and 4 */
    matrix_t *A = mat_new_qf(2, 2);
    qfloat_t a00 = qf_from_double(4.0), a01 = qf_from_double(1.0);
    qfloat_t a10 = QF_ZERO, a11 = QF_ONE;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);
    print_mqf("A", A);

    qfloat_t eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("qf eigendecompose_general: rc = 0", rc == 0);
    check_bool("qf eigendecompose_general: V not NULL", V != NULL);

    if (V)
    {
        print_mqf("V", V);
        for (int j = 0; j < 2; j++)
        {
            qfloat_t lam = eigenvalues[j];
            qfloat_t v0, v1;
            mat_get(V, 0, j, &v0);
            mat_get(V, 1, j, &v1);
            qfloat_t av0 = qf_add(qf_mul(a00, v0), qf_mul(a01, v1));
            qfloat_t av1 = qf_mul(a11, v1);
            qfloat_t lv0 = qf_mul(lam, v0);
            qfloat_t lv1 = qf_mul(lam, v1);
            char label0[64], label1[64];
            snprintf(label0, sizeof(label0), "qf (Av)[0,%d]=lam*v[0,%d]", j, j);
            snprintf(label1, sizeof(label1), "qf (Av)[1,%d]=lam*v[1,%d]", j, j);
            check_qf_val(label0, av0, lv0, 1e-25);
            check_qf_val(label1, av1, lv1, 1e-25);
        }
        mat_free(V);
    }
    mat_free(A);
}

/* ------------------------------------------------------------------ README example */

static void test_readme_example(void)
{
    printf(C_CYAN "TEST: README example — Hermitian eigendecomposition\n" C_RESET);

    /* Matrix from docs/matrix.md:
     *   [ 2    1+i ]
     *   [ 1-i  3   ]
     * Eigenvalues: 1 and 4
     */
    matrix_t *A = matsq_new_qc(2);

    qcomplex_t a00 = qc_make(qf_from_double(2.0), QF_ZERO);
    qcomplex_t a01 = qc_make(qf_from_double(1.0), qf_from_double(1.0));
    qcomplex_t a10 = qc_make(qf_from_double(1.0), qf_from_double(-1.0));
    qcomplex_t a11 = qc_make(qf_from_double(3.0), QF_ZERO);

    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);

    print_mqc("A", A);

    qcomplex_t ev[2];
    matrix_t *V = NULL;
    mat_eigendecompose(A, ev, &V);

    print_qc("eigenvalue[0]", ev[0]);
    print_qc("eigenvalue[1]", ev[1]);
    print_mqc("eigenvectors V (columns)", V);

    /* sort so ev[min] <= ev[max] */
    int e0_smaller = qf_to_double(ev[0].re) < qf_to_double(ev[1].re);
    qcomplex_t ev_min = e0_smaller ? ev[0] : ev[1];
    qcomplex_t ev_max = e0_smaller ? ev[1] : ev[0];
    size_t k_min = e0_smaller ? 0 : 1;
    size_t k_max = e0_smaller ? 1 : 0;

    check_qc_val("eigenvalue min = 1", ev_min, qc_make(qf_from_double(1.0), QF_ZERO), 1e-25);
    check_qc_val("eigenvalue max = 4", ev_max, qc_make(qf_from_double(4.0), QF_ZERO), 1e-25);

    /* verify A*v[k] = lambda[k]*v[k] for each eigenvector */
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < 2; i++)
        {
            qcomplex_t Av_ik = QC_ZERO;
            for (size_t j = 0; j < 2; j++)
            {
                qcomplex_t aij, vjk;
                mat_get(A, i, j, &aij);
                mat_get(V, j, k, &vjk);
                Av_ik = qc_add(Av_ik, qc_mul(aij, vjk));
            }
            qcomplex_t vik;
            mat_get(V, i, k, &vik);
            qcomplex_t lv_ik = qc_mul(ev[k], vik);
            char label[64];
            snprintf(label, sizeof(label), "(Av)[%zu,%zu] = lambda*v[%zu,%zu]", i, k, i, k);
            check_qc_val(label, Av_ik, lv_ik, 1e-25);
        }
    }
    (void)k_min;
    (void)k_max;

    mat_free(A);
    mat_free(V);
}

/* ------------------------------------------------------------------ generic matrix check (double) */

static void check_mat_d(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    double *g   = malloc(n * sizeof(double));
    double *e   = malloc(n * sizeof(double));
    double *err = malloc(n * sizeof(double));
    size_t *wg  = calloc(cols, sizeof(size_t));
    size_t *we  = calloc(cols, sizeof(size_t));
    size_t *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = fabs(g[k] - e[k]);
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    char buf[256];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            d_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            d_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ matrix comparison helper */

static void check_mat2x2_d(const char *label,
                           matrix_t *R,
                           double e00, double e01,
                           double e10, double e11,
                           double tol)
{
    double ev[4] = {e00, e01, e10, e11};
    matrix_t *E = mat_create_d(2, 2, ev);
    check_mat_d(label, R, E, tol);
    mat_free(E);
}

/* ------------------------------------------------------------------ matrix identity helper (n×n double) */

static void check_mat_identity_d(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_d(n);
    check_mat_d(label, R, I, tol);
    mat_free(I);
}

/* ------------------------------------------------------------------ matrix comparison helpers (qfloat) */

static void check_mat_qf(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    qfloat_t *g   = malloc(n * sizeof(qfloat_t));
    qfloat_t *e   = malloc(n * sizeof(qfloat_t));
    double   *err = malloc(n * sizeof(double));
    size_t   *wg  = calloc(cols, sizeof(size_t));
    size_t   *we  = calloc(cols, sizeof(size_t));
    size_t   *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = qf_abs(qf_sub(g[k], e[k])).hi;
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    char buf[512];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            qf_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            qf_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qf_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qf_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" %*s", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ matrix comparison helpers (qcomplex) */

static void check_mat_qc(const char *label, matrix_t *got, matrix_t *expected_mat, double tol)
{
    tests_run++;
    if (!got || !expected_mat)
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (NULL)\n" C_RESET, label);
        return;
    }

    size_t rows = mat_get_row_count(got);
    size_t cols = mat_get_col_count(got);

    if (rows != mat_get_row_count(expected_mat) || cols != mat_get_col_count(expected_mat))
    {
        tests_failed++;
        printf(C_BOLD C_RED "  FAIL: %s (dimension mismatch)\n" C_RESET, label);
        return;
    }

    size_t n = rows * cols;
    qcomplex_t *g   = malloc(n * sizeof(qcomplex_t));
    qcomplex_t *e   = malloc(n * sizeof(qcomplex_t));
    double     *err = malloc(n * sizeof(double));
    size_t     *wg  = calloc(cols, sizeof(size_t));
    size_t     *we  = calloc(cols, sizeof(size_t));
    size_t     *werr = calloc(cols, sizeof(size_t));
    if (!g || !e || !err || !wg || !we || !werr)
    {
        free(g); free(e); free(err); free(wg); free(we); free(werr);
        return;
    }

    mat_get_data(got, g);
    mat_get_data(expected_mat, e);

    double max_err = 0.0;
    for (size_t k = 0; k < n; k++)
    {
        err[k] = qf_to_double(qc_abs(qc_sub(g[k], e[k])));
        if (err[k] > max_err) max_err = err[k];
    }

    int ok = max_err < tol;
    if (!ok) tests_failed++;

    printf(ok ? C_BOLD C_GREEN "  OK: %s\n" C_RESET
              : C_BOLD C_RED "  FAIL: %s\n" C_RESET, label);

    char buf[512];
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
        {
            size_t l;
            qc_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > wg[j])   wg[j]   = l;
            qc_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            l = strlen(buf); if (l > we[j])   we[j]   = l;
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            l = strlen(buf); if (l > werr[j]) werr[j] = l;
        }

    printf("    got = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qc_to_coloured_string(g[i*cols+j], buf, sizeof(buf));
            printf(" (%*s)", (int)wg[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    expected = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            qc_to_coloured_string(e[i*cols+j], buf, sizeof(buf));
            printf(" (%*s)", (int)we[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    printf("    error = " C_CYAN "[" C_RESET "\n");
    for (size_t i = 0; i < rows; i++)
    {
        printf("      ");
        for (size_t j = 0; j < cols; j++)
        {
            d_to_coloured_err_string(err[i*cols+j], tol, buf, sizeof(buf));
            printf(" %*s", (int)werr[j], buf);
        }
        printf("\n");
    }
    printf("    " C_CYAN "]" C_RESET "\n");

    free(g); free(e); free(err);
    free(wg); free(we); free(werr);
}

/* ------------------------------------------------------------------ identity helpers (qfloat / qcomplex) */

static void check_mat_identity_qf(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_qf(n);
    check_mat_qf(label, R, I, tol);
    mat_free(I);
}

static void check_mat_identity_qc(const char *label, matrix_t *R, size_t n, double tol)
{
    matrix_t *I = mat_create_identity_qc(n);
    check_mat_qc(label, R, I, tol);
    mat_free(I);
}

/* ------------------------------------------------------------------ nilpotent matrix tests */

/*
 * N = [[0,1],[0,0]] is nilpotent: N² = 0.  Every Taylor series truncates
 * after the linear term, giving exact closed-form results for all functions.
 */
static void test_mat_nilpotent_d(void)
{
    printf(C_CYAN "TEST: nilpotent matrix N=[[0,1],[0,0]] exact values\n" C_RESET);

    double nvals[4] = {0.0, 1.0, 0.0, 0.0};
    matrix_t *N = mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;

    /* exp(N) = I + N = [[1,1],[0,1]] */
    {
        matrix_t *R = mat_exp(N);
        print_md("exp(N)", R);
        check_mat2x2_d("exp(N)", R, 1.0, 1.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* sin(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_sin(N);
        print_md("sin(N)", R);
        check_mat2x2_d("sin(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* cos(N) = I = [[1,0],[0,1]] */
    {
        matrix_t *R = mat_cos(N);
        print_md("cos(N)", R);
        check_mat2x2_d("cos(N)", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* tan(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_tan(N);
        print_md("tan(N)", R);
        check_mat2x2_d("tan(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* sinh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_sinh(N);
        print_md("sinh(N)", R);
        check_mat2x2_d("sinh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* cosh(N) = I = [[1,0],[0,1]] */
    {
        matrix_t *R = mat_cosh(N);
        print_md("cosh(N)", R);
        check_mat2x2_d("cosh(N)", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* tanh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_tanh(N);
        print_md("tanh(N)", R);
        check_mat2x2_d("tanh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* asin(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_asin(N);
        print_md("asin(N)", R);
        check_mat2x2_d("asin(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* acos(N) = π/2·I - N = [[π/2,-1],[0,π/2]] */
    {
        matrix_t *R = mat_acos(N);
        print_md("acos(N)", R);
        check_mat2x2_d("acos(N)", R, M_PI / 2.0, -1.0, 0.0, M_PI / 2.0, 1e-12);
        mat_free(R);
    }

    /* atan(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_atan(N);
        print_md("atan(N)", R);
        check_mat2x2_d("atan(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* asinh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_asinh(N);
        print_md("asinh(N)", R);
        check_mat2x2_d("asinh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* atanh(N) = N = [[0,1],[0,0]] */
    {
        matrix_t *R = mat_atanh(N);
        print_md("atanh(N)", R);
        check_mat2x2_d("atanh(N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* erf(N) = (2/√π)·N = [[0, 2/√π],[0,0]] */
    {
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erf(N);
        print_md("erf(N)", R);
        check_mat2x2_d("erf(N)", R, 0.0, two_over_sqrtpi, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* erfc(N) = I - (2/√π)·N = [[1, -2/√π],[0,1]] */
    {
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erfc(N);
        print_md("erfc(N)", R);
        check_mat2x2_d("erfc(N)", R, 1.0, -two_over_sqrtpi, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    mat_free(N);

    /* sqrt(I+N) = I + N/2 = [[1,0.5],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        matrix_t *R = mat_sqrt(IN);
        print_md("sqrt(I+N)", R);
        check_mat2x2_d("sqrt(I+N)", R, 1.0, 0.5, 0.0, 1.0, 1e-12);
        mat_free(IN);
        mat_free(R);
    }

    /* log(I+N) = N = [[0,1],[0,0]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        matrix_t *R = mat_log(IN);
        print_md("log(I+N)", R);
        check_mat2x2_d("log(I+N)", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(IN);
        mat_free(R);
    }
}

/* ------------------------------------------------------------------ algebraic identity tests */

/*
 * For any square matrix A:
 *   sin²(A) + cos²(A) = I
 *   cosh²(A) - sinh²(A) = I
 *   exp(A) · exp(-A) = I
 * These hold exactly (up to floating-point rounding) because the doubling
 * formulas used internally preserve Pythagorean relations.
 */
static void test_mat_algebraic_ids_d(void)
{
    printf(C_CYAN "TEST: algebraic identities for non-diagonal matrices\n" C_RESET);

    /* A = [[0.3, 0.1],[0.1, 0.4]] — symmetric, non-diagonal */
    double avals[4] = {0.3, 0.1, 0.1, 0.4};
    matrix_t *A = mat_create_d(2, 2, avals);
    check_bool("A allocated", A != NULL);
    if (!A)
        return;
    print_md("A", A);

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) not NULL", S != NULL);
        check_bool("cos(A) not NULL", C != NULL);
        if (S && C)
        {
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            check_bool("sin²(A) not NULL", S2 != NULL);
            check_bool("cos²(A) not NULL", C2 != NULL);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                print_md("sin²(A)+cos²(A)", I);
                check_mat2x2_d("sin²+cos²=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
                mat_free(I);
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("cosh(A) not NULL", CH != NULL);
        check_bool("sinh(A) not NULL", SH != NULL);
        if (CH && SH)
        {
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            check_bool("cosh²(A) not NULL", CH2 != NULL);
            check_bool("sinh²(A) not NULL", SH2 != NULL);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                print_md("cosh²(A)-sinh²(A)", I);
                check_mat2x2_d("cosh²-sinh²=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
                mat_free(I);
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        double negvals[4] = {-0.3, -0.1, -0.1, -0.4};
        matrix_t *negA = mat_create_d(2, 2, negvals);
        matrix_t *EnA = mat_exp(negA);
        check_bool("exp(A) not NULL", E != NULL);
        check_bool("exp(-A) not NULL", EnA != NULL);
        if (E && EnA)
        {
            matrix_t *I = mat_mul(E, EnA);
            print_md("exp(A)·exp(-A)", I);
            check_mat2x2_d("exp(A)·exp(-A)=I", I, 1.0, 0.0, 0.0, 1.0, 1e-10);
            mat_free(I);
        }
        mat_free(E);
        mat_free(negA);
        mat_free(EnA);
    }

    mat_free(A);
}

/* ------------------------------------------------------------------ round-trip tests */

static void test_mat_roundtrips_d(void)
{
    printf(C_CYAN "TEST: round-trip identities for non-diagonal matrices\n" C_RESET);

    /* exp(log(A)) = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *L = mat_log(A);
        check_bool("log(A) not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("exp(log(A))", R);
                check_mat2x2_d("exp(log(A))=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
                mat_free(R);
            }
            mat_free(L);
        }
        mat_free(A);
    }

    /* sinh(asinh(A)) = A for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("asinh(A) not NULL", S != NULL);
        if (S)
        {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("sinh(asinh(A))", R);
                check_mat2x2_d("sinh(asinh(A))=A", R, 0.3, 0.1, 0.1, 0.4, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }

    /* sqrt(A)·sqrt(A) = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A (pos-def)", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("sqrt(A) not NULL", S != NULL);
        if (S)
        {
            print_md("sqrt(A)", S);
            matrix_t *R = mat_mul(S, S);
            check_bool("sqrt(A)·sqrt(A) not NULL", R != NULL);
            if (R)
            {
                print_md("sqrt(A)²", R);
                check_mat2x2_d("sqrt(A)²=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
                mat_free(R);
            }
            mat_free(S);
        }
        mat_free(A);
    }

    /* atan(tan(A)) = A for small symmetric A = [[0.3,0.1],[0.1,0.2]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.2};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("tan(A) not NULL", T != NULL);
        if (T)
        {
            print_md("tan(A)", T);
            matrix_t *R = mat_atan(T);
            check_bool("atan(tan(A)) not NULL", R != NULL);
            if (R)
            {
                print_md("atan(tan(A))", R);
                check_mat2x2_d("atan(tan(A))=A", R, 0.3, 0.1, 0.1, 0.2, 1e-10);
                mat_free(R);
            }
            mat_free(T);
        }
        mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_pow_int tests */

static void test_mat_pow_int_d(void)
{
    printf(C_CYAN "TEST: mat_pow_int (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_pow_int(NULL,0) = NULL", mat_pow_int(NULL, 0) == NULL);

    double nvals[4] = {0.0, 1.0, 0.0, 0.0};
    matrix_t *N = mat_create_d(2, 2, nvals);
    check_bool("N allocated", N != NULL);
    if (!N)
        return;

    /* N^0 = I */
    {
        matrix_t *R = mat_pow_int(N, 0);
        print_md("N^0", R);
        check_mat2x2_d("N^0=I", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* N^1 = N */
    {
        matrix_t *R = mat_pow_int(N, 1);
        print_md("N^1", R);
        check_mat2x2_d("N^1=N", R, 0.0, 1.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    /* N^2 = 0 */
    {
        matrix_t *R = mat_pow_int(N, 2);
        print_md("N^2", R);
        check_mat2x2_d("N^2=0", R, 0.0, 0.0, 0.0, 0.0, 1e-12);
        mat_free(R);
    }

    mat_free(N);

    /* (I+N)^n = I + n·N  for upper-triangular Jordan blocks */
    double invals[4] = {1.0, 1.0, 0.0, 1.0};
    matrix_t *IN = mat_create_d(2, 2, invals);
    check_bool("I+N allocated", IN != NULL);
    if (!IN)
        return;

    /* (I+N)^0 = I */
    {
        matrix_t *R = mat_pow_int(IN, 0);
        print_md("(I+N)^0", R);
        check_mat2x2_d("(I+N)^0=I", R, 1.0, 0.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^2 = I + 2N = [[1,2],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, 2);
        print_md("(I+N)^2", R);
        check_mat2x2_d("(I+N)^2", R, 1.0, 2.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^3 = I + 3N = [[1,3],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, 3);
        print_md("(I+N)^3", R);
        check_mat2x2_d("(I+N)^3", R, 1.0, 3.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^-1 = I - N = [[1,-1],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, -1);
        print_md("(I+N)^-1", R);
        check_mat2x2_d("(I+N)^-1", R, 1.0, -1.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    /* (I+N)^-2 = I - 2N = [[1,-2],[0,1]] */
    {
        matrix_t *R = mat_pow_int(IN, -2);
        print_md("(I+N)^-2", R);
        check_mat2x2_d("(I+N)^-2", R, 1.0, -2.0, 0.0, 1.0, 1e-12);
        mat_free(R);
    }

    mat_free(IN);

    /* diagonal matrix: diag(2,3)^4 = diag(16,81) */
    {
        double dvals[4] = {2.0, 0.0, 0.0, 3.0};
        matrix_t *D = mat_create_d(2, 2, dvals);
        matrix_t *R = mat_pow_int(D, 4);
        print_md("diag(2,3)^4", R);
        check_mat2x2_d("diag(2,3)^4=diag(16,81)", R, 16.0, 0.0, 0.0, 81.0, 1e-12);
        mat_free(D);
        mat_free(R);
    }
}

/* ------------------------------------------------------------------ mat_pow tests */

static void test_mat_pow_d(void)
{
    printf(C_CYAN "TEST: mat_pow (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_pow(NULL,1.0) = NULL", mat_pow(NULL, 1.0) == NULL);

    /* (I+N)^0.5 = I + 0.5·N = [[1,0.5],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        matrix_t *R = mat_pow(IN, 0.5);
        print_md("(I+N)^0.5", R);
        check_mat2x2_d("(I+N)^0.5", R, 1.0, 0.5, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
    }

    /* (I+N)^2.0 = I + 2N = [[1,2],[0,1]] */
    {
        double invals[4] = {1.0, 1.0, 0.0, 1.0};
        matrix_t *IN = mat_create_d(2, 2, invals);
        matrix_t *R = mat_pow(IN, 2.0);
        print_md("(I+N)^2.0", R);
        check_mat2x2_d("(I+N)^2.0", R, 1.0, 2.0, 0.0, 1.0, 1e-10);
        mat_free(IN);
        mat_free(R);
    }

    /* A^1.0 = A for positive-definite A = [[2,0.5],[0.5,2]] */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        matrix_t *R = mat_pow(A, 1.0);
        print_md("A^1.0", R);
        check_mat2x2_d("A^1.0=A", R, 2.0, 0.5, 0.5, 2.0, 1e-10);
        mat_free(A);
        mat_free(R);
    }

    /* pow(pow_int): (A^2.0)[i,j] ≈ (A²)[i,j] for positive-definite A */
    {
        double avals[4] = {2.0, 0.5, 0.5, 2.0};
        matrix_t *A = mat_create_d(2, 2, avals);
        matrix_t *Rp = mat_pow(A, 2.0);
        matrix_t *Ri = mat_pow_int(A, 2);
        check_bool("A^2.0 not NULL", Rp != NULL);
        check_bool("A^2   not NULL", Ri != NULL);
        if (Rp && Ri)
        {
            double p00, p01, i00, i01;
            mat_get(Rp, 0, 0, &p00);
            mat_get(Rp, 0, 1, &p01);
            mat_get(Ri, 0, 0, &i00);
            mat_get(Ri, 0, 1, &i01);
            check_d("A^2.0[0,0] = A²[0,0]", p00, i00, 1e-10);
            check_d("A^2.0[0,1] = A²[0,1]", p01, i01, 1e-10);
        }
        mat_free(A);
        mat_free(Rp);
        mat_free(Ri);
    }
}

/* ------------------------------------------------------------------ mat_erf tests */

static void test_mat_erf_d(void)
{
    printf(C_CYAN "TEST: mat_erf (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_erf(NULL) = NULL", mat_erf(NULL) == NULL);

    /* 1×1: erf([[0.5]]) = [[erf(0.5)]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_erf(A);
        check_bool("mat_erf([[0.5]]) not NULL", R != NULL);
        if (R)
        {
            print_md("erf(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("erf([[0.5]])", got, erf(0.5), 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* 1×1: erf([[0]]) = [[0]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_erf(A);
        check_bool("mat_erf([[0]]) not NULL", R != NULL);
        if (R)
        {
            print_md("erf(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("erf([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* nilpotent: erf(N) = (2/√π)·N = [[0, 2/√π],[0,0]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = mat_create_d(2, 2, nvals);
        print_md("N (nilpotent)", N);
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erf(N);
        print_md("erf(N)", R);
        check_mat2x2_d("erf(N)=(2/√π)N", R, 0.0, two_over_sqrtpi, 0.0, 0.0, 1e-12);
        mat_free(N);
        mat_free(R);
    }

    /* odd symmetry: erf(-A) = -erf(A) for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        double navals[4] = {-0.3, -0.1, -0.1, -0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        matrix_t *nA = mat_create_d(2, 2, navals);
        print_md("A", A);
        print_md("-A", nA);
        matrix_t *E = mat_erf(A);
        matrix_t *En = mat_erf(nA);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erf(-A) not NULL", En != NULL);
        if (E && En)
        {
            print_md("erf(A)", E);
            print_md("erf(-A)", En);
            double e00, en00, e01, en01;
            mat_get(E, 0, 0, &e00);
            mat_get(E, 0, 1, &e01);
            mat_get(En, 0, 0, &en00);
            mat_get(En, 0, 1, &en01);
            check_d("erf(-A)[0,0] = -erf(A)[0,0]", en00, -e00, 1e-12);
            check_d("erf(-A)[0,1] = -erf(A)[0,1]", en01, -e01, 1e-12);
        }
        mat_free(A);
        mat_free(nA);
        mat_free(E);
        mat_free(En);
    }
}

/* ------------------------------------------------------------------ mat_erfc tests */

static void test_mat_erfc_d(void)
{
    printf(C_CYAN "TEST: mat_erfc (double)\n" C_RESET);

    /* null safety */
    check_bool("mat_erfc(NULL) = NULL", mat_erfc(NULL) == NULL);

    /* 1×1: erfc([[0.5]]) = [[erfc(0.5)]] */
    {
        matrix_t *A = mat_new_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_erfc(A);
        check_bool("mat_erfc([[0.5]]) not NULL", R != NULL);
        if (R)
        {
            print_md("erfc(A)", R);
            double got;
            mat_get(R, 0, 0, &got);
            check_d("erfc([[0.5]])", got, erfc(0.5), 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* nilpotent: erfc(N) = I - (2/√π)·N = [[1,-2/√π],[0,1]] */
    {
        double nvals[4] = {0.0, 1.0, 0.0, 0.0};
        matrix_t *N = mat_create_d(2, 2, nvals);
        print_md("N (nilpotent)", N);
        double two_over_sqrtpi = 2.0 / sqrt(M_PI);
        matrix_t *R = mat_erfc(N);
        print_md("erfc(N)", R);
        check_mat2x2_d("erfc(N)=I-(2/√π)N", R, 1.0, -two_over_sqrtpi, 0.0, 1.0, 1e-12);
        mat_free(N);
        mat_free(R);
    }

    /* erf(A) + erfc(A) = I for symmetric A = [[0.3,0.1],[0.1,0.4]] */
    {
        double avals[4] = {0.3, 0.1, 0.1, 0.4};
        matrix_t *A = mat_create_d(2, 2, avals);
        print_md("A", A);
        matrix_t *E = mat_erf(A);
        matrix_t *EC = mat_erfc(A);
        check_bool("erf(A) not NULL", E != NULL);
        check_bool("erfc(A) not NULL", EC != NULL);
        if (E && EC)
        {
            matrix_t *Sum = mat_add(E, EC);
            print_md("erf(A)+erfc(A)", Sum);
            check_mat2x2_d("erf+erfc=I", Sum, 1.0, 0.0, 0.0, 1.0, 1e-12);
            mat_free(Sum);
        }
        mat_free(A);
        mat_free(E);
        mat_free(EC);
    }
}

/* ------------------------------------------------------------------ mat_typeof tests */

static void test_mat_typeof(void)
{
    printf(C_CYAN "TEST: mat_typeof\n" C_RESET);

    matrix_t *Ad = matsq_new_d(2);
    matrix_t *Aqf = matsq_new_qf(2);
    matrix_t *Aqc = matsq_new_qc(2);

    check_bool("mat_typeof(double)   = MAT_TYPE_DOUBLE", mat_typeof(Ad) == MAT_TYPE_DOUBLE);
    check_bool("mat_typeof(qfloat)   = MAT_TYPE_QFLOAT", mat_typeof(Aqf) == MAT_TYPE_QFLOAT);
    check_bool("mat_typeof(qcomplex) = MAT_TYPE_QCOMPLEX", mat_typeof(Aqc) == MAT_TYPE_QCOMPLEX);

    matrix_t *Id = mat_create_identity_d(2);
    matrix_t *Iqf = mat_create_identity_qf(2);
    matrix_t *Iqc = mat_create_identity_qc(2);

    check_bool("mat_typeof(identity double)   = MAT_TYPE_DOUBLE", mat_typeof(Id) == MAT_TYPE_DOUBLE);
    check_bool("mat_typeof(identity qfloat)   = MAT_TYPE_QFLOAT", mat_typeof(Iqf) == MAT_TYPE_QFLOAT);
    check_bool("mat_typeof(identity qcomplex) = MAT_TYPE_QCOMPLEX", mat_typeof(Iqc) == MAT_TYPE_QCOMPLEX);

    /* matrix functions preserve element type */
    double dvals[4] = {0.5, 0.1, 0.1, 0.6};
    matrix_t *A_d = mat_create_d(2, 2, dvals);

    qfloat_t qfvals[4] = {
        qf_from_double(0.5), qf_from_double(0.1),
        qf_from_double(0.1), qf_from_double(0.6)};
    matrix_t *A_qf = mat_create_qf(2, 2, qfvals);

    matrix_t *E_d = mat_exp(A_d);
    matrix_t *E_qf = mat_exp(A_qf);

    check_bool("mat_exp(double) → double", E_d != NULL && mat_typeof(E_d) == MAT_TYPE_DOUBLE);
    check_bool("mat_exp(qfloat) → qfloat", E_qf != NULL && mat_typeof(E_qf) == MAT_TYPE_QFLOAT);

    mat_free(Ad);
    mat_free(Aqf);
    mat_free(Aqc);
    mat_free(Id);
    mat_free(Iqf);
    mat_free(Iqc);
    mat_free(A_d);
    mat_free(A_qf);
    mat_free(E_d);
    mat_free(E_qf);
}

/* ------------------------------------------------------------------ 3×3 matrix function tests */

/*
 * Asymmetric upper-triangular A is not Hermitian, so it takes the general
 * QR Schur path.  The Schur factor T equals A itself (Q=I), giving a
 * non-diagonal 3×3 T.  For indices (i=0, j=2) the Parlett recurrence uses
 * the cross-term Σ_{k=1}^{1}(T[0,k]*F[k,2] - F[0,k]*T[k,2]) — the only
 * path that is never exercised by 2×2 tests.
 */
static void test_mat_fun_3x3(void)
{
    printf(C_CYAN "TEST: 3×3 double matrix functions — general SPD\n" C_RESET);

    /* 3×3 symmetric positive definite — all off-diagonal entries nonzero, */
    /* exercises the full Q·f(T)·Q* round-trip in mat_fun_schur.            */
    double avals[9] = {
        4.0, 2.0, 1.0,
        2.0, 3.0, 1.5,
        1.0, 1.5, 2.0};
    double negavals[9] = {
        -4.0, -2.0, -1.0,
        -2.0, -3.0, -1.5,
        -1.0, -1.5, -2.0};

    matrix_t *A = mat_create_d(3, 3, avals);
    matrix_t *negA = mat_create_d(3, 3, negavals);

    check_bool("A 3×3 allocated", A != NULL);
    check_bool("negA 3×3 allocated", negA != NULL);
    if (!A || !negA)
    {
        mat_free(A);
        mat_free(negA);
        return;
    }

    print_md("A (3×3 upper-triangular)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("exp(A) not NULL", E != NULL);
        check_bool("exp(-A) not NULL", En != NULL);
        if (E && En)
        {
            print_md("exp(A)", E);
            print_md("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                    check_mat_identity_d("3×3 exp(A)·exp(-A)=I", I, 3, 1e-10);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) 3×3 not NULL", S != NULL);
        check_bool("cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_md("sin(A)", S);
            print_md("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_d("3×3 sin²(A)+cos²(A)=I", I, 3, 1e-10);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_md("cosh(A)", CH);
            print_md("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("3×3 cosh²(A)-sinh²(A)=I", I, 3, 1e-10);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A (A has positive diagonal → positive eigenvalues) */
    {
        matrix_t *L = mat_log(A);
        check_bool("log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("3×3 exp(log(A))=A", R, A, 1e-10);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 4×4 matrix function tests */

/*
 * Upper-triangular 4×4 with eigenvalues {1,2,3,4} exercises every level of the
 * Parlett recurrence: element (0,3) accumulates cross-terms over k=1,2, which
 * is unreachable with a 3×3 matrix.
 */
static void test_mat_fun_4x4(void)
{
    printf(C_CYAN "TEST: 4×4 double matrix functions — general SPD\n" C_RESET);

    /* 4×4 symmetric positive definite — diagonally dominant ensures SPD,   */
    /* dense off-diagonal entries exercise the full Q·f(T)·Q* round-trip.   */
    double avals[16] = {
        5.0, 1.0, 0.5, 0.2,
        1.0, 4.0, 1.0, 0.5,
        0.5, 1.0, 3.0, 1.0,
        0.2, 0.5, 1.0, 2.0};
    double negavals[16] = {
        -5.0, -1.0, -0.5, -0.2,
        -1.0, -4.0, -1.0, -0.5,
        -0.5, -1.0, -3.0, -1.0,
        -0.2, -0.5, -1.0, -2.0};

    matrix_t *A = mat_create_d(4, 4, avals);
    matrix_t *negA = mat_create_d(4, 4, negavals);

    check_bool("A 4×4 allocated", A != NULL);
    check_bool("negA 4×4 allocated", negA != NULL);
    if (!A || !negA)
    {
        mat_free(A);
        mat_free(negA);
        return;
    }

    print_md("A (4×4 upper-triangular)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("exp(A) 4×4 not NULL", E != NULL);
        check_bool("exp(-A) 4×4 not NULL", En != NULL);
        if (E && En)
        {
            print_md("exp(A)", E);
            print_md("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_d("4×4 exp(A)·exp(-A)=I", I, 4, 1e-8);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("sin(A) 4×4 not NULL", S != NULL);
        check_bool("cos(A) 4×4 not NULL", C != NULL);
        if (S && C)
        {
            print_md("sin(A)", S);
            print_md("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_d("4×4 sin²(A)+cos²(A)=I", I, 4, 1e-8);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("cosh(A) 4×4 not NULL", CH != NULL);
        check_bool("sinh(A) 4×4 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_md("cosh(A)", CH);
            print_md("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_d("4×4 cosh²(A)-sinh²(A)=I", I, 4, 1e-8);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A (A has positive diagonal) */
    {
        matrix_t *L = mat_log(A);
        check_bool("log(A) 4×4 not NULL", L != NULL);
        if (L)
        {
            print_md("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("exp(log(A)) 4×4 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("4×4 exp(log(A))=A", R, A, 1e-8);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    /* sqrt(A)·sqrt(A) = A (A has positive diagonal) */
    {
        matrix_t *Sq = mat_sqrt(A);
        check_bool("sqrt(A) 4×4 not NULL", Sq != NULL);
        if (Sq)
        {
            print_md("sqrt(A)", Sq);
            matrix_t *R = mat_mul(Sq, Sq);
            check_bool("sqrt(A)² 4×4 not NULL", R != NULL);
            if (R)
            {
                check_mat_d("4×4 sqrt(A)²=A", R, A, 1e-8);
                mat_free(R);
            }
            mat_free(Sq);
        }
    }

    /* erf(A) + erfc(A) = I */
    {
        matrix_t *E = mat_erf(A);
        matrix_t *EC = mat_erfc(A);
        check_bool("erf(A) 4×4 not NULL", E != NULL);
        check_bool("erfc(A) 4×4 not NULL", EC != NULL);
        if (E && EC)
        {
            print_md("erf(A)", E);
            print_md("erfc(A)", EC);
            matrix_t *I = mat_add(E, EC);
            if (I)
            {
                check_mat_identity_d("4×4 erf(A)+erfc(A)=I", I, 4, 1e-8);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(EC);
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 3×3 qfloat matrix function tests */

static void test_mat_fun_3x3_qf(void)
{
    printf(C_CYAN "TEST: 3×3 qfloat matrix functions — general SPD\n" C_RESET);

    qfloat_t avals[9] = {
        qf_from_double(4.0), qf_from_double(2.0), qf_from_double(1.0),
        qf_from_double(2.0), qf_from_double(3.0), qf_from_double(1.5),
        qf_from_double(1.0), qf_from_double(1.5), qf_from_double(2.0)};
    qfloat_t negavals[9] = {
        qf_from_double(-4.0), qf_from_double(-2.0), qf_from_double(-1.0),
        qf_from_double(-2.0), qf_from_double(-3.0), qf_from_double(-1.5),
        qf_from_double(-1.0), qf_from_double(-1.5), qf_from_double(-2.0)};

    matrix_t *A    = mat_create_qf(3, 3, avals);
    matrix_t *negA = mat_create_qf(3, 3, negavals);

    check_bool("qf A 3×3 allocated", A != NULL);
    check_bool("qf negA 3×3 allocated", negA != NULL);
    if (!A || !negA) { mat_free(A); mat_free(negA); return; }

    print_mqf("A (3×3 SPD qfloat)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("qf exp(A) 3×3 not NULL", E != NULL);
        check_bool("qf exp(-A) 3×3 not NULL", En != NULL);
        if (E && En)
        {
            print_mqf("exp(A)", E);
            print_mqf("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_qf("3×3 qf exp(A)·exp(-A)=I", I, 3, 1e-25);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("qf sin(A) 3×3 not NULL", S != NULL);
        check_bool("qf cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_mqf("sin(A)", S);
            print_mqf("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_qf("3×3 qf sin²(A)+cos²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("qf cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("qf sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_mqf("cosh(A)", CH);
            print_mqf("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_qf("3×3 qf cosh²(A)-sinh²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A */
    {
        matrix_t *L = mat_log(A);
        check_bool("qf log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_mqf("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("qf exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_qf("3×3 qf exp(log(A))=A", R, A, 1e-25);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ 3×3 qcomplex matrix function tests */

static void test_mat_fun_3x3_qc(void)
{
    printf(C_CYAN "TEST: 3×3 qcomplex matrix functions — Hermitian positive definite\n" C_RESET);

    /* Hermitian positive definite — diagonally dominant with Hermitian off-diagonal */
    qcomplex_t avals[9] = {
        qc_make(qf_from_double(4.0), QF_ZERO),
        qc_make(qf_from_double(1.0), qf_from_double( 0.5)),
        qc_make(qf_from_double(0.5), qf_from_double(-0.3)),
        qc_make(qf_from_double(1.0), qf_from_double(-0.5)),
        qc_make(qf_from_double(3.0), QF_ZERO),
        qc_make(qf_from_double(0.8), qf_from_double( 0.2)),
        qc_make(qf_from_double(0.5), qf_from_double( 0.3)),
        qc_make(qf_from_double(0.8), qf_from_double(-0.2)),
        qc_make(qf_from_double(2.0), QF_ZERO)};
    qcomplex_t negavals[9] = {
        qc_make(qf_from_double(-4.0), QF_ZERO),
        qc_make(qf_from_double(-1.0), qf_from_double(-0.5)),
        qc_make(qf_from_double(-0.5), qf_from_double( 0.3)),
        qc_make(qf_from_double(-1.0), qf_from_double( 0.5)),
        qc_make(qf_from_double(-3.0), QF_ZERO),
        qc_make(qf_from_double(-0.8), qf_from_double(-0.2)),
        qc_make(qf_from_double(-0.5), qf_from_double(-0.3)),
        qc_make(qf_from_double(-0.8), qf_from_double( 0.2)),
        qc_make(qf_from_double(-2.0), QF_ZERO)};

    matrix_t *A    = mat_create_qc(3, 3, avals);
    matrix_t *negA = mat_create_qc(3, 3, negavals);

    check_bool("qc A 3×3 allocated", A != NULL);
    check_bool("qc negA 3×3 allocated", negA != NULL);
    if (!A || !negA) { mat_free(A); mat_free(negA); return; }

    print_mqc("A (3×3 HPD qcomplex)", A);

    /* exp(A) · exp(-A) = I */
    {
        matrix_t *E = mat_exp(A);
        matrix_t *En = mat_exp(negA);
        check_bool("qc exp(A) 3×3 not NULL", E != NULL);
        check_bool("qc exp(-A) 3×3 not NULL", En != NULL);
        if (E && En)
        {
            print_mqc("exp(A)", E);
            print_mqc("exp(-A)", En);
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_qc("3×3 qc exp(A)·exp(-A)=I", I, 3, 1e-25);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(En);
    }

    /* sin²(A) + cos²(A) = I */
    {
        matrix_t *S = mat_sin(A);
        matrix_t *C = mat_cos(A);
        check_bool("qc sin(A) 3×3 not NULL", S != NULL);
        check_bool("qc cos(A) 3×3 not NULL", C != NULL);
        if (S && C)
        {
            print_mqc("sin(A)", S);
            print_mqc("cos(A)", C);
            matrix_t *S2 = mat_mul(S, S);
            matrix_t *C2 = mat_mul(C, C);
            if (S2 && C2)
            {
                matrix_t *I = mat_add(S2, C2);
                if (I)
                {
                    check_mat_identity_qc("3×3 qc sin²(A)+cos²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(S2);
            mat_free(C2);
        }
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A);
        matrix_t *SH = mat_sinh(A);
        check_bool("qc cosh(A) 3×3 not NULL", CH != NULL);
        check_bool("qc sinh(A) 3×3 not NULL", SH != NULL);
        if (CH && SH)
        {
            print_mqc("cosh(A)", CH);
            print_mqc("sinh(A)", SH);
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_qc("3×3 qc cosh²(A)-sinh²(A)=I", I, 3, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* exp(log(A)) = A */
    {
        matrix_t *L = mat_log(A);
        check_bool("qc log(A) 3×3 not NULL", L != NULL);
        if (L)
        {
            print_mqc("log(A)", L);
            matrix_t *R = mat_exp(L);
            check_bool("qc exp(log(A)) 3×3 not NULL", R != NULL);
            if (R)
            {
                check_mat_qc("3×3 qc exp(log(A))=A", R, A, 1e-25);
                mat_free(R);
            }
            mat_free(L);
        }
    }

    mat_free(A);
    mat_free(negA);
}

/* ------------------------------------------------------------------ error-handling tests */

static void test_mat_error_handling(void)
{
    printf(C_CYAN "TEST: error handling — NULL and non-square inputs\n" C_RESET);

    /* mat_det */
    {
        double out = 0.0;
        check_bool("mat_det(NULL) = -1", mat_det(NULL, &out) == -1);
        matrix_t *rect = mat_new_d(2, 3);
        check_bool("mat_det(2×3) = -2", mat_det(rect, &out) == -2);
        mat_free(rect);
    }

    /* mat_inverse */
    {
        check_bool("mat_inverse(NULL) = NULL", mat_inverse(NULL) == NULL);
        matrix_t *rect = mat_new_d(3, 2);
        check_bool("mat_inverse(3×2) = NULL", mat_inverse(rect) == NULL);
        mat_free(rect);
    }

    /* mat_eigenvalues */
    {
        double ev[4];
        check_bool("mat_eigenvalues(NULL) < 0", mat_eigenvalues(NULL, ev) < 0);
        matrix_t *rect = mat_new_d(2, 3);
        check_bool("mat_eigenvalues(2×3) < 0", mat_eigenvalues(rect, ev) < 0);
        mat_free(rect);
    }

    /* mat_eigendecompose */
    {
        double ev[4];
        matrix_t *evecs = NULL;
        check_bool("mat_eigendecompose(NULL) < 0",
                   mat_eigendecompose(NULL, ev, &evecs) < 0);
        matrix_t *rect = mat_new_d(2, 3);
        check_bool("mat_eigendecompose(2×3) < 0",
                   mat_eigendecompose(rect, ev, &evecs) < 0);
        mat_free(rect);
    }

    /* dimension mismatch in arithmetic */
    {
        matrix_t *A = mat_new_d(2, 3);
        matrix_t *B = mat_new_d(3, 2);
        matrix_t *C = mat_new_d(4, 4);

        check_bool("mat_add(2×3, 3×2) = NULL", mat_add(A, B) == NULL);
        check_bool("mat_sub(2×3, 3×2) = NULL", mat_sub(A, B) == NULL);
        check_bool("mat_mul(2×3, 4×4) = NULL", mat_mul(A, C) == NULL);

        mat_free(A);
        mat_free(B);
        mat_free(C);
    }

    /* mat_pow_int on non-square */
    {
        matrix_t *rect = mat_new_d(2, 3);
        check_bool("mat_pow_int(2×3, 2) = NULL", mat_pow_int(rect, 2) == NULL);
        mat_free(rect);
    }
}

/* ------------------------------------------------------------------ qf/qc variants of under-tested functions */

static void test_mat_fun_qf_qc(void)
{
    printf(C_CYAN "TEST: qf/qc variants of tan, hyperbolic, log, erf, erfc, pow_int\n" C_RESET);

    qfloat_t qfvals[4] = {
        qf_from_double(0.3), qf_from_double(0.1),
        qf_from_double(0.1), qf_from_double(0.4)};
    matrix_t *A_qf = mat_create_qf(2, 2, qfvals);
    check_bool("A_qf allocated", A_qf != NULL);
    if (!A_qf)
        return;

    /* tan: cos(A)·tan(A) = sin(A) */
    {
        matrix_t *T = mat_tan(A_qf);
        matrix_t *S = mat_sin(A_qf);
        matrix_t *C = mat_cos(A_qf);
        check_bool("qf tan(A) not NULL", T != NULL);
        if (T && S && C)
        {
            matrix_t *CT = mat_mul(C, T);
            if (CT)
            {
                check_mat_qf("qf 2×2 cos·tan=sin", CT, S, 1e-25);
                mat_free(CT);
            }
        }
        mat_free(T);
        mat_free(S);
        mat_free(C);
    }

    /* cosh²(A) - sinh²(A) = I */
    {
        matrix_t *CH = mat_cosh(A_qf);
        matrix_t *SH = mat_sinh(A_qf);
        check_bool("qf cosh(A) not NULL", CH != NULL);
        check_bool("qf sinh(A) not NULL", SH != NULL);
        if (CH && SH)
        {
            matrix_t *CH2 = mat_mul(CH, CH);
            matrix_t *SH2 = mat_mul(SH, SH);
            if (CH2 && SH2)
            {
                matrix_t *I = mat_sub(CH2, SH2);
                if (I)
                {
                    check_mat_identity_qf("qf 2×2 cosh²-sinh²=I", I, 2, 1e-25);
                    mat_free(I);
                }
            }
            mat_free(CH2);
            mat_free(SH2);
        }
        mat_free(CH);
        mat_free(SH);
    }

    /* tanh: cosh(A)·tanh(A) = sinh(A) */
    {
        matrix_t *TH = mat_tanh(A_qf);
        matrix_t *CH = mat_cosh(A_qf);
        matrix_t *SH = mat_sinh(A_qf);
        check_bool("qf tanh(A) not NULL", TH != NULL);
        if (TH && CH && SH)
        {
            matrix_t *CT = mat_mul(CH, TH);
            if (CT)
            {
                check_mat_qf("qf 2×2 cosh·tanh=sinh", CT, SH, 1e-25);
                mat_free(CT);
            }
        }
        mat_free(TH);
        mat_free(CH);
        mat_free(SH);
    }

    /* log: exp(log(A)) = A for positive-definite A */
    {
        qfloat_t pdvals[4] = {
            qf_from_double(2.0), qf_from_double(0.5),
            qf_from_double(0.5), qf_from_double(2.0)};
        matrix_t *PD = mat_create_qf(2, 2, pdvals);
        matrix_t *L = mat_log(PD);
        check_bool("qf log(PD) not NULL", L != NULL);
        if (L)
        {
            matrix_t *R = mat_exp(L);
            check_bool("qf exp(log(PD)) not NULL", R != NULL);
            if (R)
            {
                check_mat_qf("qf 2×2 exp(log(A))=A", R, PD, 1e-25);
                mat_free(R);
            }
            mat_free(L);
        }
        mat_free(PD);
    }

    /* erf + erfc = I */
    {
        matrix_t *E = mat_erf(A_qf);
        matrix_t *EC = mat_erfc(A_qf);
        check_bool("qf erf(A) not NULL", E != NULL);
        check_bool("qf erfc(A) not NULL", EC != NULL);
        if (E && EC)
        {
            matrix_t *I = mat_add(E, EC);
            if (I)
            {
                check_mat_identity_qf("qf 2×2 erf+erfc=I", I, 2, 1e-25);
                mat_free(I);
            }
        }
        mat_free(E);
        mat_free(EC);
    }

    /* pow_int for qf: diag(2,3)^3 = diag(8,27) */
    {
        qfloat_t dvals[4] = {
            qf_from_double(2.0), QF_ZERO,
            QF_ZERO, qf_from_double(3.0)};
        matrix_t *D = mat_create_qf(2, 2, dvals);
        matrix_t *R = mat_pow_int(D, 3);
        check_bool("qf pow_int(diag(2,3),3) not NULL", R != NULL);
        if (R)
        {
            qfloat_t r00, r11, r01, r10;
            mat_get(R, 0, 0, &r00);
            mat_get(R, 1, 1, &r11);
            mat_get(R, 0, 1, &r01);
            mat_get(R, 1, 0, &r10);
            check_qf_val("qf diag(2,3)^3 [0,0]=8", r00, qf_from_double(8.0), 1e-25);
            check_qf_val("qf diag(2,3)^3 [1,1]=27", r11, qf_from_double(27.0), 1e-25);
            check_qf_val("qf diag(2,3)^3 [0,1]=0", r01, QF_ZERO, 1e-25);
            check_qf_val("qf diag(2,3)^3 [1,0]=0", r10, QF_ZERO, 1e-25);
            mat_free(R);
        }
        mat_free(D);
    }

    /* qcomplex: exp(A_qc)·exp(-A_qc) = I */
    {
        qcomplex_t qcvals[4] = {
            qc_make(qf_from_double(0.3), QF_ZERO),
            qc_make(qf_from_double(0.1), QF_ZERO),
            qc_make(qf_from_double(0.1), QF_ZERO),
            qc_make(qf_from_double(0.4), QF_ZERO)};
        qcomplex_t neg_qcvals[4] = {
            qc_make(qf_from_double(-0.3), QF_ZERO),
            qc_make(qf_from_double(-0.1), QF_ZERO),
            qc_make(qf_from_double(-0.1), QF_ZERO),
            qc_make(qf_from_double(-0.4), QF_ZERO)};
        matrix_t *A_qc = mat_create_qc(2, 2, qcvals);
        matrix_t *negA_qc = mat_create_qc(2, 2, neg_qcvals);
        matrix_t *E = mat_exp(A_qc);
        matrix_t *En = mat_exp(negA_qc);
        check_bool("qc exp(A) not NULL", E != NULL);
        check_bool("qc exp(-A) not NULL", En != NULL);
        if (E && En)
        {
            matrix_t *I = mat_mul(E, En);
            if (I)
            {
                check_mat_identity_qc("qc 2×2 exp(A)·exp(-A)=I", I, 2, 1e-25);
                mat_free(I);
            }
        }
        mat_free(A_qc);
        mat_free(negA_qc);
        mat_free(E);
        mat_free(En);
    }

    mat_free(A_qf);
}

/* ------------------------------------------------------------------ tests_main */

int tests_main(void)
{
    /* core matrix tests */
    RUN_TEST(test_creation, NULL);
    RUN_TEST(test_reading, NULL);
    RUN_TEST(test_writing, NULL);
    RUN_TEST(test_add_sub, NULL);
    RUN_TEST(test_multiply, NULL);
    RUN_TEST(test_transpose_conjugate, NULL);
    RUN_TEST(test_identity_get, NULL);
    RUN_TEST(test_identity_set, NULL);

    /* same-type qfloat/qcomplex tests */
    RUN_TEST(test_add_sub_qf, NULL);
    RUN_TEST(test_add_sub_qc, NULL);
    RUN_TEST(test_multiply_qf, NULL);
    RUN_TEST(test_multiply_qc, NULL);

    /* mixed-type ADD tests */
    RUN_TEST(test_add_mixed_d_qf, NULL);
    RUN_TEST(test_add_mixed_d_qc, NULL);
    RUN_TEST(test_add_mixed_qf_qc, NULL);

    /* mixed-type SUB tests */
    RUN_TEST(test_sub_mixed_d_qf, NULL);
    RUN_TEST(test_sub_mixed_d_qc, NULL);
    RUN_TEST(test_sub_mixed_qf_qc, NULL);

    /* mixed-type MUL tests */
    RUN_TEST(test_multiply_mixed_d_qf, NULL);
    RUN_TEST(test_multiply_mixed_d_qc, NULL);
    RUN_TEST(test_multiply_mixed_qf_qc, NULL);

    RUN_TEST(test_scalar_mul_d_d, NULL);
    RUN_TEST(test_scalar_mul_d_qf, NULL);
    RUN_TEST(test_scalar_mul_d_qc, NULL);
    RUN_TEST(test_scalar_mul_qf_d, NULL);
    RUN_TEST(test_scalar_mul_qc_qc, NULL);

    RUN_TEST(test_identity_arith_d, NULL);
    RUN_TEST(test_identity_arith_qf, NULL);
    RUN_TEST(test_identity_arith_qc, NULL);

    RUN_TEST(test_scalar_div_d_d, NULL);
    RUN_TEST(test_scalar_div_qf_qf, NULL);
    RUN_TEST(test_scalar_div_qc_qc, NULL);
    RUN_TEST(test_scalar_div_d_qf, NULL);
    RUN_TEST(test_scalar_div_qf_d, NULL);

    RUN_TEST(test_det_double, NULL);
    RUN_TEST(test_det_qfloat, NULL);
    RUN_TEST(test_det_qcomplex, NULL);

    RUN_TEST(test_inverse_double, NULL);
    RUN_TEST(test_inverse_qfloat, NULL);
    RUN_TEST(test_inverse_qcomplex, NULL);

    RUN_TEST(test_hermitian_op, NULL);

    RUN_TEST(test_eigen_d, NULL);
    RUN_TEST(test_eigen_qf, NULL);
    RUN_TEST(test_eigen_qc, NULL);

    RUN_TEST(test_mat_exp_d, NULL);
    RUN_TEST(test_mat_exp_qf, NULL);
    RUN_TEST(test_mat_exp_qc, NULL);
    RUN_TEST(test_mat_exp_null_safety, NULL);

    RUN_TEST(test_mat_sin_d, NULL);
    RUN_TEST(test_mat_sin_qf, NULL);
    RUN_TEST(test_mat_sin_qc, NULL);
    RUN_TEST(test_mat_sin_null_safety, NULL);

    RUN_TEST(test_mat_cos_d, NULL);
    RUN_TEST(test_mat_cos_qf, NULL);
    RUN_TEST(test_mat_cos_qc, NULL);

    RUN_TEST(test_mat_tan_d, NULL);

    RUN_TEST(test_mat_sinh_d, NULL);
    RUN_TEST(test_mat_cosh_d, NULL);
    RUN_TEST(test_mat_tanh_d, NULL);

    RUN_TEST(test_mat_trig_null_safety, NULL);

    RUN_TEST(test_mat_sqrt_d, NULL);
    RUN_TEST(test_mat_sqrt_qf, NULL);

    RUN_TEST(test_mat_log_d, NULL);

    RUN_TEST(test_mat_asin_d, NULL);
    RUN_TEST(test_mat_acos_d, NULL);
    RUN_TEST(test_mat_atan_d, NULL);

    RUN_TEST(test_mat_asinh_d, NULL);
    RUN_TEST(test_mat_acosh_d, NULL);
    RUN_TEST(test_mat_atanh_d, NULL);

    RUN_TEST(test_mat_inv_trig_null_safety, NULL);

    RUN_TEST(test_eigen_general_d, NULL);
    RUN_TEST(test_eigen_general_qf, NULL);

    RUN_TEST(test_mat_nilpotent_d, NULL);
    RUN_TEST(test_mat_algebraic_ids_d, NULL);
    RUN_TEST(test_mat_roundtrips_d, NULL);

    RUN_TEST(test_mat_pow_int_d, NULL);
    RUN_TEST(test_mat_pow_d, NULL);

    RUN_TEST(test_mat_erf_d, NULL);
    RUN_TEST(test_mat_erfc_d, NULL);

    RUN_TEST(test_mat_fun_qf_qc, NULL);
    RUN_TEST(test_mat_error_handling, NULL);
    RUN_TEST(test_mat_fun_3x3, NULL);
    RUN_TEST(test_mat_fun_4x4, NULL);
    RUN_TEST(test_mat_fun_3x3_qf, NULL);
    RUN_TEST(test_mat_fun_3x3_qc, NULL);
    RUN_TEST(test_mat_typeof, NULL);

    RUN_TEST(test_readme_example, NULL);

    return tests_failed;
}
