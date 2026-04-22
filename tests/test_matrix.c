#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "matrix.h"
#include "qfloat.h"
#include "qcomplex.h"

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

static void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size)
{
    char buf[256];
    qf_to_string(x, buf, sizeof(buf));

    if (strcmp(buf, "0") == 0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_YELLOW "%s" C_RESET, buf);
}

static void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size)
{
    char buf[256];
    qc_to_string(z, buf, sizeof(buf));

    /* buf looks like: "2 + 3i" */
    char real[128], sign[8], imag[128];
    sscanf(buf, "%127s %7s %127s", real, sign, imag);

    snprintf(out, out_size,
        C_GREEN "%s" C_RESET " "
        C_WHITE "%s" C_RESET " "
        C_MAGENTA "%s" C_RESET,
        real, sign, imag
    );
}

/* ------------------------------------------------------------------ coloured qc/qf debug printers */

static void print_qc(const char *label, qcomplex_t z)
{
    char buf[256];
    qc_to_coloured_string(z, buf, sizeof(buf));
    printf("    %s = %s\n", label, buf);
    fflush(stdout);
}

static void print_qf(const char *label, qfloat_t x)
{
    char buf[256];
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
        for (size_t j = 0; j < cols; j++) {
            double v;
            char buf[256];
            mat_get(A, i, j, &v);
            d_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j]) w[j] = len;
        }

    /* print rows */
    for (size_t i = 0; i < rows; i++) {
        printf("      ");
        for (size_t j = 0; j < cols; j++) {
            double v;
            char buf[256];
            mat_get(A, i, j, &v);
            d_to_coloured_string(v, buf, sizeof(buf));
            printf(" (%*s)", (int)w[j], buf);
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
        for (size_t j = 0; j < cols; j++) {
            qfloat_t v;
            char buf[256];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j]) w[j] = len;
        }

    for (size_t i = 0; i < rows; i++) {
        printf("      ");
        for (size_t j = 0; j < cols; j++) {
            qfloat_t v;
            char buf[256];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            printf(" (%*s)", (int)w[j], buf);
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
        for (size_t j = 0; j < cols; j++) {
            qcomplex_t v;
            char buf[256];
            mat_get(A, i, j, &v);
            qc_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j]) w[j] = len;
        }

    for (size_t i = 0; i < rows; i++) {
        printf("      ");
        for (size_t j = 0; j < cols; j++) {
            qcomplex_t v;
            char buf[256];
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
    if (!ok) tests_failed++;

    printf(ok ? C_GREEN "  OK: %s\n" C_RESET
              : C_MAGENTA "  FAIL: %s\n" C_RESET, label);

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
    if (!ok) tests_failed++;

    printf(ok ? C_GREEN "  OK: %s\n" C_RESET
              : C_MAGENTA "  FAIL: %s\n" C_RESET, label);

    print_qf("got      ", got);
    print_qf("expected ", expected);
    printf("    error    = %.16g\n", err);
}

static void check_qc_val(const char *label, qcomplex_t got, qcomplex_t expected, double tol)
{
    double err = qf_to_double(qc_abs(qc_sub(got, expected)));
    int ok = err < tol;

    tests_run++;
    if (!ok) tests_failed++;

    printf(ok ? C_GREEN "  OK: %s\n" C_RESET
              : C_MAGENTA "  FAIL: %s\n" C_RESET, label);

    print_qc("got      ", got);
    print_qc("expected ", expected);
    printf("    error    = %.16g\n", err);
}

static void check_bool(const char *label, int cond)
{
    tests_run++;
    if (!cond) tests_failed++;

    printf(cond ? C_GREEN "  OK: %s\n" C_RESET
                : C_MAGENTA "  FAIL: %s\n" C_RESET, label);
}

/* ------------------------------------------------------------------ 1. creation */

static void test_creation(void)
{
    printf(C_CYAN "TEST: creation of all matrix types\n" C_RESET);

    matrix_t *Ad  = mat_create_d(2,3);
    matrix_t *Aqf = mat_create_qf(3,4);
    matrix_t *Aqc = mat_create_qc(4,5);

    check_bool("mat_create_d non-null",  Ad  != NULL);
    check_bool("mat_create_qf non-null", Aqf != NULL);
    check_bool("mat_create_qc non-null", Aqc != NULL);

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
    matrix_t *A = mat_create_d(2,2);
    double x = 3.0, y = 4.0;
    mat_set(A,0,0,&x);
    mat_set(A,1,1,&y);

    print_md("A", A);

    double v;
    mat_get(A,0,0,&v);
    check_d("read double A[0,0] = 3", v, 3.0, 1e-30);
    mat_get(A,1,1,&v);
    check_d("read double A[1,1] = 4", v, 4.0, 1e-30);

    mat_free(A);

    /* qfloat */
    matrix_t *B = mat_create_qf(2,2);
    qfloat_t qx = qf_from_double(1.25);
    qfloat_t qy = qf_from_double(-2.5);
    mat_set(B,0,1,&qx);
    mat_set(B,1,0,&qy);

    print_mqf("B", B);

    qfloat_t qv;
    mat_get(B,0,1,&qv);
    check_qf_val("read qfloat B[0,1] = 1.25", qv, qx, 1e-30);
    mat_get(B,1,0,&qv);
    check_qf_val("read qfloat B[1,0] = -2.5", qv, qy, 1e-30);

    mat_free(B);

    /* qcomplex */
    matrix_t *C = mat_create_qc(2,2);
    qcomplex_t z1 = qc_make(qf_from_double(2.0), qf_from_double(3.0));
    qcomplex_t z2 = qc_make(qf_from_double(-1.0), qf_from_double(0.5));
    mat_set(C,0,0,&z1);
    mat_set(C,1,1,&z2);

    print_mqc("C", C);

    qcomplex_t zv;
    mat_get(C,0,0,&zv);
    check_qc_val("read qcomplex C[0,0]", zv, z1, 1e-30);
    mat_get(C,1,1,&zv);
    check_qc_val("read qcomplex C[1,1]", zv, z2, 1e-30);

    mat_free(C);
}

/* ------------------------------------------------------------------ 3. writing */

static void test_writing(void)
{
    printf(C_CYAN "TEST: writing to all matrix types\n" C_RESET);

    /* double */
    matrix_t *A = mat_create_d(2,2);
    double x = 9.0;
    mat_set(A,1,0,&x);

    print_md("A after write", A);

    double v;
    mat_get(A,1,0,&v);
    check_d("write double A[1,0] = 9", v, 9.0, 1e-30);
    mat_free(A);

    /* qfloat */
    matrix_t *B = mat_create_qf(2,2);
    qfloat_t qx = qf_from_double(7.75);
    mat_set(B,0,1,&qx);

    print_mqf("B after write", B);

    qfloat_t qv;
    mat_get(B,0,1,&qv);
    check_qf_val("write qfloat B[0,1] = 7.75", qv, qx, 1e-30);
    mat_free(B);

    /* qcomplex */
    matrix_t *C = mat_create_qc(2,2);
    qcomplex_t z = qc_make(qf_from_double(1.0), qf_from_double(-3.0));
    mat_set(C,1,1,&z);

    print_mqc("C after write", C);

    qcomplex_t zv;
    mat_get(C,1,1,&zv);
    check_qc_val("write qcomplex C[1,1]", zv, z, 1e-30);
    mat_free(C);
}

/* ------------------------------------------------------------------ 4. add/sub (double only) */

static void test_add_sub(void)
{
    printf(C_CYAN "TEST: addition/subtraction\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    matrix_t *B = mat_create_d(2,2);

    double a11=1, a12=2, a21=3, a22=4;
    double b11=5, b12=6, b21=7, b22=8;

    mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
    mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

    mat_set(B,0,0,&b11); mat_set(B,0,1,&b12);
    mat_set(B,1,0,&b21); mat_set(B,1,1,&b22);

    print_md("A", A);
    print_md("B", B);

    matrix_t *C = mat_add(A,B);
    print_md("A+B", C);

    double v;
    mat_get(C,0,0,&v); check_d("add[0,0] = 6", v, 6, 1e-30);
    mat_get(C,1,1,&v); check_d("add[1,1] = 12", v, 12, 1e-30);

    matrix_t *D = mat_sub(A,B);
    print_md("A-B", D);

    mat_get(D,0,0,&v); check_d("sub[0,0] = -4", v, -4, 1e-30);
    mat_get(D,1,1,&v); check_d("sub[1,1] = -4", v, -4, 1e-30);

    mat_free(A); mat_free(B); mat_free(C); mat_free(D);
}

/* ------------------------------------------------------------------ 5. multiply (double only) */

static void test_multiply(void)
{
    printf(C_CYAN "TEST: multiplication\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    matrix_t *B = mat_create_d(2,2);

    double a11=1, a12=2, a21=3, a22=4;
    double b11=5, b12=6, b21=7, b22=8;

    mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
    mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

    mat_set(B,0,0,&b11); mat_set(B,0,1,&b12);
    mat_set(B,1,0,&b21); mat_set(B,1,1,&b22);

    print_md("A", A);
    print_md("B", B);

    matrix_t *C = mat_mul(A,B);
    print_md("A*B", C);

    double v;
    mat_get(C,0,0,&v); check_d("mul[0,0] = 19", v, 19, 1e-30);
    mat_get(C,1,1,&v); check_d("mul[1,1] = 50", v, 50, 1e-30);

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ 6. transpose/conjugate */

static void test_transpose_conjugate(void)
{
    printf(C_CYAN "TEST: transpose/conjugate\n" C_RESET);

    matrix_t *A = mat_create_d(2,3);
    double x = 1, y = 2, z = 3;
    mat_set(A,0,1,&x);
    mat_set(A,1,2,&y);
    mat_set(A,0,2,&z);

    print_md("A", A);

    matrix_t *T = mat_transpose(A);
    print_md("transpose(A)", T);

    double v;
    mat_get(T,1,0,&v); check_d("transpose T[1,0] = 1", v, 1, 1e-30);
    mat_get(T,2,1,&v); check_d("transpose T[2,1] = 2", v, 2, 1e-30);
    mat_get(T,2,0,&v); check_d("transpose T[2,0] = 3", v, 3, 1e-30);

    mat_free(A);
    mat_free(T);

    matrix_t *C = mat_create_qc(2,2);
    qcomplex_t z1 = qc_make(qf_from_double(2.0), qf_from_double(3.0));
    qcomplex_t z2 = qc_make(qf_from_double(-1.0), qf_from_double(4.0));
    mat_set(C,0,0,&z1);
    mat_set(C,1,1,&z2);

    print_mqc("C", C);

    matrix_t *K = mat_conj(C);
    print_mqc("conj(C)", K);

    qcomplex_t zv;
    mat_get(K,0,0,&zv);
    check_qc_val("conj C[0,0]", zv, qc_conj(z1), 1e-30);

    mat_get(K,1,1,&zv);
    check_qc_val("conj C[1,1]", zv, qc_conj(z2), 1e-30);

    mat_free(C);
    mat_free(K);
}

/* ------------------------------------------------------------------ 7. identity get */

static void test_identity_get(void)
{
    printf(C_CYAN "TEST: identity matrix get\n" C_RESET);

    matrix_t *I = matsq_ident_d(3);

    print_md("I", I);

    double v;
    mat_get(I,0,0,&v); check_d("I[0,0] = 1", v, 1, 1e-30);
    mat_get(I,1,1,&v); check_d("I[1,1] = 1", v, 1, 1e-30);
    mat_get(I,2,2,&v); check_d("I[2,2] = 1", v, 1, 1e-30);

    mat_get(I,0,1,&v); check_d("I[0,1] = 0", v, 0, 1e-30);
    mat_get(I,1,2,&v); check_d("I[1,2] = 0", v, 0, 1e-30);

    mat_free(I);
}

/* ------------------------------------------------------------------ 8. identity set (materialise) */

static void test_identity_set(void)
{
    printf(C_CYAN "TEST: identity matrix set (materialisation)\n" C_RESET);

    matrix_t *I = matsq_ident_d(3);

    print_md("I before write", I);

    double x = 7.0;
    mat_set(I,0,2,&x);

    print_md("I after write", I);

    double v;
    mat_get(I,0,2,&v); check_d("after write, I[0,2] = 7", v, 7.0, 1e-30);
    mat_get(I,1,1,&v); check_d("diagonal preserved", v, 1.0, 1e-30);

    mat_free(I);
}

/* ------------------------------------------------------------------ qfloat add/sub (mixed sizes) */

static void test_add_sub_qf(void)
{
    printf(C_CYAN "TEST: qfloat addition/subtraction (mixed sizes)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,3);
    matrix_t *B = mat_create_qf(2,3);

    qfloat_t a_vals[6] = {
        qf_from_double(1), qf_from_double(2), qf_from_double(3),
        qf_from_double(4), qf_from_double(5), qf_from_double(6)
    };
    qfloat_t b_vals[6] = {
        qf_from_double(10), qf_from_double(20), qf_from_double(30),
        qf_from_double(40), qf_from_double(50), qf_from_double(60)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            qfloat_t tmpA = a_vals[idx];
            qfloat_t tmpB = b_vals[idx];
            mat_set(A, i, j, &tmpA);
            mat_set(B, i, j, &tmpB);
            idx++;
        }

    print_mqf("A", A);
    print_mqf("B", B);

    matrix_t *C = mat_add(A, B);
    print_mqf("A+B", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            qfloat_t got;
            qfloat_t expected = qf_add(a_vals[idx], b_vals[idx]);
            mat_get(C, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "qfloat add[%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);
            idx++;
        }

    matrix_t *D = mat_sub(A, B);
    print_mqf("A-B", D);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            qfloat_t got;
            qfloat_t expected = qf_sub(a_vals[idx], b_vals[idx]);
            mat_get(D, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "qfloat sub[%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);
            idx++;
        }

    mat_free(A); mat_free(B); mat_free(C); mat_free(D);
}

/* ------------------------------------------------------------------ qcomplex add/sub (mixed sizes) */

static void test_add_sub_qc(void)
{
    printf(C_CYAN "TEST: qcomplex addition/subtraction (mixed sizes)\n" C_RESET);

    matrix_t *A = mat_create_qc(1,3);
    matrix_t *B = mat_create_qc(1,3);

    qcomplex_t a_vals[3] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(3), qf_from_double(4)),
        qc_make(qf_from_double(-1), qf_from_double(5))
    };
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(10), qf_from_double(-2)),
        qc_make(qf_from_double(0),  qf_from_double(7)),
        qc_make(qf_from_double(2),  qf_from_double(3))
    };

    for (size_t j = 0; j < 3; j++) {
        mat_set(A, 0, j, &a_vals[j]);
        mat_set(B, 0, j, &b_vals[j]);
    }

    print_mqc("A", A);
    print_mqc("B", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A+B", C);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_add(a_vals[j], b_vals[j]);
        mat_get(C, 0, j, &got);
        char label[64];
        snprintf(label, sizeof(label), "qcomplex add[0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    matrix_t *D = mat_sub(A, B);
    print_mqc("A-B", D);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_sub(a_vals[j], b_vals[j]);
        mat_get(D, 0, j, &got);
        char label[64];
        snprintf(label, sizeof(label), "qcomplex sub[0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C); mat_free(D);
}

/* ------------------------------------------------------------------ qfloat multiply (mixed sizes) */

static void test_multiply_qf(void)
{
    printf(C_CYAN "TEST: qfloat multiplication (mixed sizes)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,3);
    matrix_t *B = mat_create_qf(3,2);

    double a_raw[6] = { 1,2,3, 4,5,6 };
    double b_raw[6] = { 7,8, 9,10, 11,12 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            qfloat_t tmp = qf_from_double(a_raw[idx++]);
            mat_set(A, i, j, &tmp);
        }

    idx = 0;
    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t tmp = qf_from_double(b_raw[idx++]);
            mat_set(B, i, j, &tmp);
        }

    print_mqf("A", A);
    print_mqf("B", B);

    matrix_t *C = mat_mul(A, B);
    print_mqf("A*B", C);

    double expected_raw[4] = { 58, 64, 139, 154 };

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_from_double(expected_raw[idx++]);
            mat_get(C, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "qfloat mul[%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ qcomplex multiply (mixed sizes) */

static void test_multiply_qc(void)
{
    printf(C_CYAN "TEST: qcomplex multiplication (mixed sizes)\n" C_RESET);

    matrix_t *A = mat_create_qc(3,1);
    matrix_t *B = mat_create_qc(1,4);

    qcomplex_t a_vals[3] = {
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(2), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(3))
    };

    qcomplex_t b_vals[4] = {
        qc_make(qf_from_double(4), qf_from_double(0)),
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(-3), qf_from_double(1)),
        qc_make(qf_from_double(0), qf_from_double(-2))
    };

    for (size_t i = 0; i < 3; i++)
        mat_set(A, i, 0, &a_vals[i]);

    for (size_t j = 0; j < 4; j++)
        mat_set(B, 0, j, &b_vals[j]);

    print_mqc("A", A);
    print_mqc("B", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A*B", C);

    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 4; j++) {
            qcomplex_t got;
            qcomplex_t expected = qc_mul(a_vals[i], b_vals[j]);
            mat_get(C, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "qcomplex mul[%zu,%zu]", i, j);
            check_qc_val(label, got, expected, 1e-28);
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ tests_main */

int tests_main(void)
{
    RUN_TEST(test_creation, NULL);
    RUN_TEST(test_reading, NULL);
    RUN_TEST(test_writing, NULL);
    RUN_TEST(test_add_sub, NULL);
    RUN_TEST(test_multiply, NULL);
    RUN_TEST(test_transpose_conjugate, NULL);
    RUN_TEST(test_identity_get, NULL);
    RUN_TEST(test_identity_set, NULL);

    RUN_TEST(test_add_sub_qf, NULL);
    RUN_TEST(test_add_sub_qc, NULL);
    RUN_TEST(test_multiply_qf, NULL);
    RUN_TEST(test_multiply_qc, NULL);

    return tests_failed;
}
