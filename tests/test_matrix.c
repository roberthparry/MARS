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
        C_GREEN "%s" C_RESET " "
        C_WHITE "%s" C_RESET " "
        C_MAGENTA "%si" C_RESET,
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
        for (size_t j = 0; j < cols; j++) {
            qfloat_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qf_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j]) w[j] = len;
        }

    for (size_t i = 0; i < rows; i++) {
        printf("      ");
        for (size_t j = 0; j < cols; j++) {
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
        for (size_t j = 0; j < cols; j++) {
            qcomplex_t v;
            char buf[512];
            mat_get(A, i, j, &v);
            qc_to_coloured_string(v, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > w[j]) w[j] = len;
        }

    for (size_t i = 0; i < rows; i++) {
        printf("      ");
        for (size_t j = 0; j < cols; j++) {
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

/* ------------------------------------------------------------------ mixed-type add: double + qfloat */

static void test_add_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type addition (double + qfloat)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    matrix_t *B = mat_create_qf(2,2);

    double a_vals[4] = { 1.0, 2.0, 3.0, 4.0 };
    qfloat_t b_vals[4] = {
        qf_from_double(10), qf_from_double(20),
        qf_from_double(30), qf_from_double(40)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            mat_set(A, i, j, &a_vals[idx]);
            mat_set(B, i, j, &b_vals[idx]);
            idx++;
        }

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_add(A, B);
    print_mqf("A + B (qfloat result)", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_add(qf_from_double(a_vals[idx]), b_vals[idx]);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "mixed add d+qf [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type add: double + qcomplex */

static void test_add_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type addition (double + qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_d(1,3);
    matrix_t *B = mat_create_qc(1,3);

    double a_vals[3] = { 1.0, -2.0, 5.0 };
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(3), qf_from_double(4)),
        qc_make(qf_from_double(0), qf_from_double(-1)),
        qc_make(qf_from_double(2), qf_from_double(2))
    };

    for (size_t j = 0; j < 3; j++) {
        mat_set(A, 0, j, &a_vals[j]);
        mat_set(B, 0, j, &b_vals[j]);
    }

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A + B (qcomplex result)", C);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_add(qc_make(qf_from_double(a_vals[j]), QF_ZERO), b_vals[j]);
        mat_get(C, 0, j, &got);

        char label[64];
        snprintf(label, sizeof(label), "mixed add d+qc [0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type add: qfloat + qcomplex */

static void test_add_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type addition (qfloat + qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,1);
    matrix_t *B = mat_create_qc(2,1);

    qfloat_t a_vals[2] = { qf_from_double(1.5), qf_from_double(-3.25) };
    qcomplex_t b_vals[2] = {
        qc_make(qf_from_double(2), qf_from_double(1)),
        qc_make(qf_from_double(-1), qf_from_double(4))
    };

    for (size_t i = 0; i < 2; i++) {
        mat_set(A, i, 0, &a_vals[i]);
        mat_set(B, i, 0, &b_vals[i]);
    }

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_add(A, B);
    print_mqc("A + B (qcomplex result)", C);

    for (size_t i = 0; i < 2; i++) {
        qcomplex_t got;
        qcomplex_t expected = qc_add(qc_make(a_vals[i], QF_ZERO), b_vals[i]);
        mat_get(C, i, 0, &got);

        char label[64];
        snprintf(label, sizeof(label), "mixed add qf+qc [%zu,0]", i);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: double - qfloat */

static void test_sub_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (double - qfloat)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    matrix_t *B = mat_create_qf(2,2);

    double a_vals[4] = { 5.0,  7.0, -3.0,  2.0 };
    qfloat_t b_vals[4] = {
        qf_from_double(1.0), qf_from_double(2.5),
        qf_from_double(-4.0), qf_from_double(10.0)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            mat_set(A, i, j, &a_vals[idx]);
            mat_set(B, i, j, &b_vals[idx]);
            idx++;
        }

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqf("A - B (qfloat result)", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_sub(qf_from_double(a_vals[idx]), b_vals[idx]);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "mixed sub d-qf [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: double - qcomplex */

static void test_sub_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (double - qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_d(1,3);
    matrix_t *B = mat_create_qc(1,3);

    double a_vals[3] = { 10.0, -5.0, 3.0 };
    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(2), qf_from_double(1)),
        qc_make(qf_from_double(-3), qf_from_double(4)),
        qc_make(qf_from_double(0.5), qf_from_double(-2))
    };

    for (size_t j = 0; j < 3; j++) {
        mat_set(A, 0, j, &a_vals[j]);
        mat_set(B, 0, j, &b_vals[j]);
    }

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqc("A - B (qcomplex result)", C);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_sub(
            qc_make(qf_from_double(a_vals[j]), QF_ZERO),
            b_vals[j]
        );
        mat_get(C, 0, j, &got);

        char label[64];
        snprintf(label, sizeof(label), "mixed sub d-qc [0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type sub: qfloat - qcomplex */

static void test_sub_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type subtraction (qfloat - qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,1);
    matrix_t *B = mat_create_qc(2,1);

    qfloat_t a_vals[2] = {
        qf_from_double(4.5),
        qf_from_double(-1.25)
    };

    qcomplex_t b_vals[2] = {
        qc_make(qf_from_double(1), qf_from_double(3)),
        qc_make(qf_from_double(-2), qf_from_double(1))
    };

    for (size_t i = 0; i < 2; i++) {
        mat_set(A, i, 0, &a_vals[i]);
        mat_set(B, i, 0, &b_vals[i]);
    }

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_sub(A, B);
    print_mqc("A - B (qcomplex result)", C);

    for (size_t i = 0; i < 2; i++) {
        qcomplex_t got;
        qcomplex_t expected = qc_sub(
            qc_make(a_vals[i], QF_ZERO),
            b_vals[i]
        );
        mat_get(C, i, 0, &got);

        char label[64];
        snprintf(label, sizeof(label), "mixed sub qf-qc [%zu,0]", i);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: double * qfloat */

static void test_multiply_mixed_d_qf(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (double * qfloat)\n" C_RESET);

    matrix_t *A = mat_create_d(2,3);
    matrix_t *B = mat_create_qf(3,2);

    double a_vals[6] = { 1, 2, 3, 4, 5, 6 };
    qfloat_t b_vals[6] = {
        qf_from_double(7),  qf_from_double(8),
        qf_from_double(9),  qf_from_double(10),
        qf_from_double(11), qf_from_double(12)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            mat_set(A, i, j, &a_vals[idx]);
            idx++;
        }

    idx = 0;
    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 2; j++) {
            mat_set(B, i, j, &b_vals[idx]);
            idx++;
        }

    print_md("A (double)", A);
    print_mqf("B (qfloat)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqf("A * B (qfloat result)", C);

    double expected_raw[4] = { 58, 64, 139, 154 };

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_from_double(expected_raw[idx]);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "mixed mul d*qf [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: double * qcomplex */

static void test_multiply_mixed_d_qc(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (double * qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_d(1,3);
    matrix_t *B = mat_create_qc(3,2);

    double a_vals[3] = { 2.0, -1.0, 3.0 };
    qcomplex_t b_vals[6] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(0), qf_from_double(-1)),
        qc_make(qf_from_double(4), qf_from_double(0)),
        qc_make(qf_from_double(-2), qf_from_double(3)),
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(0), qf_from_double(5))
    };

    for (size_t j = 0; j < 3; j++)
        mat_set(A, 0, j, &a_vals[j]);

    size_t idx = 0;
    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 2; j++) {
            mat_set(B, i, j, &b_vals[idx]);
            idx++;
        }

    print_md("A (double)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A * B (qcomplex result)", C);

    qcomplex_t expected_vals[2] = { QC_ZERO, QC_ZERO };

    for (size_t k = 0; k < 3; k++) {
        qcomplex_t scalar = qc_make(qf_from_double(a_vals[k]), QF_ZERO);
        expected_vals[0] = qc_add(expected_vals[0], qc_mul(scalar, b_vals[k*2 + 0]));
        expected_vals[1] = qc_add(expected_vals[1], qc_mul(scalar, b_vals[k*2 + 1]));
    }

    for (size_t j = 0; j < 2; j++) {
        qcomplex_t got;
        mat_get(C, 0, j, &got);

        char label[64];
        snprintf(label, sizeof(label), "mixed mul d*qc [0,%zu]", j);
        check_qc_val(label, got, expected_vals[j], 1e-28);
    }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ mixed-type mul: qfloat * qcomplex */

static void test_multiply_mixed_qf_qc(void)
{
    printf(C_CYAN "TEST: mixed-type multiplication (qfloat * qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,1);
    matrix_t *B = mat_create_qc(1,3);

    qfloat_t a_vals[2] = {
        qf_from_double(2.5),
        qf_from_double(-1.0)
    };

    qcomplex_t b_vals[3] = {
        qc_make(qf_from_double(3), qf_from_double(1)),
        qc_make(qf_from_double(-2), qf_from_double(4)),
        qc_make(qf_from_double(0), qf_from_double(-3))
    };

    for (size_t i = 0; i < 2; i++)
        mat_set(A, i, 0, &a_vals[i]);

    for (size_t j = 0; j < 3; j++)
        mat_set(B, 0, j, &b_vals[j]);

    print_mqf("A (qfloat)", A);
    print_mqc("B (qcomplex)", B);

    matrix_t *C = mat_mul(A, B);
    print_mqc("A * B (qcomplex result)", C);

    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            qcomplex_t got;
            qcomplex_t expected = qc_mul(
                qc_make(a_vals[i], QF_ZERO),
                b_vals[j]
            );
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "mixed mul qf*qc [%zu,%zu]", i, j);
            check_qc_val(label, got, expected, 1e-28);
        }

    mat_free(A); mat_free(B); mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × double matrix */

static void test_scalar_mul_d_d(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * double matrix)\n" C_RESET);

    matrix_t *A = mat_create_d(2,3);
    double vals[6] = { 1,2,3, 4,5,6 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_md("A", A);

    double s = 2.0;
    matrix_t *C = mat_scalar_mul_d(s, A);
    print_md("2*A", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 3; j++) {
            double got;
            double expected = s * vals[idx];
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "scalar d*d [%zu,%zu]", i, j);
            check_d(label, got, expected, 1e-12);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × qfloat matrix */

static void test_scalar_mul_d_qf(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * qfloat matrix)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,2);
    qfloat_t vals[4] = {
        qf_from_double(1.5),
        qf_from_double(-2.0),
        qf_from_double(3.25),
        qf_from_double(4.0)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_mqf("A", A);

    double s = -3.0;
    matrix_t *C = mat_scalar_mul_d(s, A);
    print_mqf("-3*A", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_mul(qf_from_double(s), vals[idx]);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "scalar d*qf [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: double scalar × qcomplex matrix */

static void test_scalar_mul_d_qc(void)
{
    printf(C_CYAN "TEST: scalar multiply (double * qcomplex matrix)\n" C_RESET);

    matrix_t *A = mat_create_qc(1,3);
    qcomplex_t vals[3] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(-3), qf_from_double(1)),
        qc_make(qf_from_double(0.5), qf_from_double(-4))
    };

    for (size_t j = 0; j < 3; j++)
        mat_set(A, 0, j, &vals[j]);

    print_mqc("A", A);

    double s = 2.0;
    matrix_t *C = mat_scalar_mul_d(s, A);
    print_mqc("2*A", C);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_mul(
            qc_make(qf_from_double(s), QF_ZERO),
            vals[j]
        );
        mat_get(C, 0, j, &got);

        char label[64];
        snprintf(label, sizeof(label), "scalar d*qc [0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: qfloat scalar × double matrix */

static void test_scalar_mul_qf_d(void)
{
    printf(C_CYAN "TEST: scalar multiply (qfloat * double matrix)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    double vals[4] = { 2.0, -1.0, 4.0, 3.0 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_md("A", A);

    qfloat_t s = qf_from_double(0.5);
    matrix_t *C = mat_scalar_mul_qf(s, A);
    print_mqf("0.5*A", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_mul(s, qf_from_double(vals[idx]));
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "scalar qf*d [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar multiply: qcomplex scalar × qcomplex matrix */

static void test_scalar_mul_qc_qc(void)
{
    printf(C_CYAN "TEST: scalar multiply (qcomplex * qcomplex matrix)\n" C_RESET);

    matrix_t *A = mat_create_qc(2,1);
    qcomplex_t vals[2] = {
        qc_make(qf_from_double(1), qf_from_double(1)),
        qc_make(qf_from_double(2), qf_from_double(-3))
    };

    for (size_t i = 0; i < 2; i++)
        mat_set(A, i, 0, &vals[i]);

    print_mqc("A", A);

    qcomplex_t s = qc_make(qf_from_double(2), qf_from_double(3));
    matrix_t *C = mat_scalar_mul_qc(s, A);
    print_mqc("(2+3i)*A", C);

    for (size_t i = 0; i < 2; i++) {
        qcomplex_t got;
        qcomplex_t expected = qc_mul(s, vals[i]);
        mat_get(C, i, 0, &got);

        char label[64];
        snprintf(label, sizeof(label), "scalar qc*qc [%zu,0]", i);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ identity add/sub/mul: double */

static void test_identity_arith_d(void)
{
    printf(C_CYAN "TEST: identity arithmetic (double)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    double vals[4] = { 1, 2, 3, 4 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    matrix_t *I = matsq_ident_d(2);

    print_md("A", A);
    print_md("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_md("A + I", ApI);

    double expected_add[4] = { 2, 2, 3, 5 };
    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            double got;
            mat_get(ApI, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "d: A+I [%zu,%zu]", i, j);
            check_d(label, got, expected_add[idx++], 1e-12);
        }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_md("A - I", AmI);

    double expected_sub[4] = { 0, 2, 3, 3 };
    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            double got;
            mat_get(AmI, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "d: A-I [%zu,%zu]", i, j);
            check_d(label, got, expected_sub[idx++], 1e-12);
        }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_md("A * I", A_times_I);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            double got;
            mat_get(A_times_I, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "d: A*I [%zu,%zu]", i, j);
            check_d(label, got, vals[idx++], 1e-12);
        }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_md("I * A", I_times_A);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            double got;
            mat_get(I_times_A, i, j, &got);
            char label[64];
            snprintf(label, sizeof(label), "d: I*A [%zu,%zu]", i, j);
            check_d(label, got, vals[idx++], 1e-12);
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

    matrix_t *A = mat_create_qf(2,2);
    qfloat_t vals[4] = {
        qf_from_double(1.5),
        qf_from_double(2.0),
        qf_from_double(-1.0),
        qf_from_double(4.0)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    matrix_t *I = matsq_ident_qf(2);

    print_mqf("A", A);
    print_mqf("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_mqf("A + I", ApI);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = (i == j)
                ? qf_add(vals[idx], QF_ONE)
                : vals[idx];
            mat_get(ApI, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: A+I [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_mqf("A - I", AmI);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = (i == j)
                ? qf_sub(vals[idx], QF_ONE)
                : vals[idx];
            mat_get(AmI, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: A-I [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_mqf("A * I", A_times_I);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            mat_get(A_times_I, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: A*I [%zu,%zu]", i, j);
            check_qf_val(label, got, vals[idx++], 1e-28);
        }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_mqf("I * A", I_times_A);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            mat_get(I_times_A, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: I*A [%zu,%zu]", i, j);
            check_qf_val(label, got, vals[idx++], 1e-28);
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

    matrix_t *A = mat_create_qc(2,2);
    qcomplex_t vals[4] = {
        qc_make(qf_from_double(1), qf_from_double(2)),
        qc_make(qf_from_double(3), qf_from_double(-1)),
        qc_make(qf_from_double(0), qf_from_double(4)),
        qc_make(qf_from_double(-2), qf_from_double(3))
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    matrix_t *I = matsq_ident_qc(2);

    print_mqc("A", A);
    print_mqc("I", I);

    /* A + I */
    matrix_t *ApI = mat_add(A, I);
    print_mqc("A + I", ApI);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qcomplex_t got;
            qcomplex_t expected = (i == j)
                ? qc_add(vals[idx], QC_ONE)
                : vals[idx];
            mat_get(ApI, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qc: A+I [%zu,%zu]", i, j);
            check_qc_val(label, got, expected, 1e-28);

            idx++;
        }

    /* A - I */
    matrix_t *AmI = mat_sub(A, I);
    print_mqc("A - I", AmI);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qcomplex_t got;
            qcomplex_t expected = (i == j)
                ? qc_sub(vals[idx], QC_ONE)
                : vals[idx];
            mat_get(AmI, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qc: A-I [%zu,%zu]", i, j);
            check_qc_val(label, got, expected, 1e-28);

            idx++;
        }

    /* A * I */
    matrix_t *A_times_I = mat_mul(A, I);
    print_mqc("A * I", A_times_I);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qcomplex_t got;
            mat_get(A_times_I, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qc: A*I [%zu,%zu]", i, j);
            check_qc_val(label, got, vals[idx++], 1e-28);
        }

    /* I * A */
    matrix_t *I_times_A = mat_mul(I, A);
    print_mqc("I * A", I_times_A);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qcomplex_t got;
            mat_get(I_times_A, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qc: I*A [%zu,%zu]", i, j);
            check_qc_val(label, got, vals[idx++], 1e-28);
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
    printf(C_CYAN "TEST: scalar division (double / double)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    double vals[4] = { 4.0, -2.0, 9.0, 3.0 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_md("A", A);

    double s = 2.0;
    matrix_t *C = mat_scalar_div_d(s, A);
    print_md("A / 2", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            double got;
            double expected = vals[idx] / s;
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "d: A/2 [%zu,%zu]", i, j);
            check_d(label, got, expected, 1e-12);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar division: qfloat scalar */

static void test_scalar_div_qf_qf(void)
{
    printf(C_CYAN "TEST: scalar division (qfloat / qfloat)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,2);
    qfloat_t vals[4] = {
        qf_from_double(1.0),
        qf_from_double(2.0),
        qf_from_double(-3.0),
        qf_from_double(4.0)
    };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_mqf("A", A);

    qfloat_t s = qf_from_double(0.5);
    matrix_t *C = mat_scalar_div_qf(s, A);
    print_mqf("A / 0.5", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_div(vals[idx], s);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: A/0.5 [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar division: qcomplex scalar */

static void test_scalar_div_qc_qc(void)
{
    printf(C_CYAN "TEST: scalar division (qcomplex / qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qc(1,3);
    qcomplex_t vals[3] = {
        qc_make(qf_from_double(2), qf_from_double(1)),
        qc_make(qf_from_double(-3), qf_from_double(4)),
        qc_make(qf_from_double(1), qf_from_double(-2))
    };

    for (size_t j = 0; j < 3; j++)
        mat_set(A, 0, j, &vals[j]);

    print_mqc("A", A);

    qcomplex_t s = qc_make(qf_from_double(1), qf_from_double(1)); /* 1 + i */
    matrix_t *C = mat_scalar_div_qc(s, A);
    print_mqc("A / (1+i)", C);

    for (size_t j = 0; j < 3; j++) {
        qcomplex_t got;
        qcomplex_t expected = qc_div(vals[j], s);
        mat_get(C, 0, j, &got);

        char label[64];
        snprintf(label, sizeof(label), "qc: A/(1+i) [0,%zu]", j);
        check_qc_val(label, got, expected, 1e-28);
    }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar division: double scalar / qfloat matrix */

static void test_scalar_div_d_qf(void)
{
    printf(C_CYAN "TEST: scalar division (double / qfloat)\n" C_RESET);

    matrix_t *A = mat_create_qf(2,1);
    qfloat_t vals[2] = {
        qf_from_double(4.0),
        qf_from_double(-2.0)
    };

    for (size_t i = 0; i < 2; i++)
        mat_set(A, i, 0, &vals[i]);

    print_mqf("A", A);

    double s = -2.0;
    matrix_t *C = mat_scalar_div_d(s, A);
    print_mqf("A / -2", C);

    for (size_t i = 0; i < 2; i++) {
        qfloat_t got;
        qfloat_t expected = qf_div(vals[i], qf_from_double(s));
        mat_get(C, i, 0, &got);

        char label[64];
        snprintf(label, sizeof(label), "d: A/-2 [%zu,0]", i);
        check_qf_val(label, got, expected, 1e-28);
    }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ scalar division: qfloat scalar / double matrix */

static void test_scalar_div_qf_d(void)
{
    printf(C_CYAN "TEST: scalar division (qfloat / double)\n" C_RESET);

    matrix_t *A = mat_create_d(2,2);
    double vals[4] = { 1.0, -2.0, 4.0, 8.0 };

    size_t idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
            mat_set(A, i, j, &vals[idx++]);

    print_md("A", A);

    qfloat_t s = qf_from_double(4.0);
    matrix_t *C = mat_scalar_div_qf(s, A);
    print_mqf("A / 4", C);

    idx = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++) {
            qfloat_t got;
            qfloat_t expected = qf_div(qf_from_double(vals[idx]), s);
            mat_get(C, i, j, &got);

            char label[64];
            snprintf(label, sizeof(label), "qf: A/4 [%zu,%zu]", i, j);
            check_qf_val(label, got, expected, 1e-28);

            idx++;
        }

    mat_free(A);
    mat_free(C);
}

/* ------------------------------------------------------------------ determinant: double */

/* ------------------------------------------------------------------ determinant: double */

static void test_det_double(void)
{
    printf(C_CYAN "TEST: determinant (double)\n" C_RESET);

    /* 1×1 */
    {
        matrix_t *A = mat_create_d(1,1);
        double x = 7.0;
        mat_set(A,0,0,&x);

        print_md("A (1x1)", A);

        double det;
        mat_det(A, &det);
        check_d("det 1x1 = 7", det, 7.0, 1e-30);

        mat_free(A);
    }

    /* 2×2 */
    {
        matrix_t *A = mat_create_d(2,2);
        double a11=1, a12=2, a21=3, a22=4;
        mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
        mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

        print_md("A (2x2)", A);

        double det;
        mat_det(A, &det);
        check_d("det [[1 2][3 4]] = -2", det, -2.0, 1e-30);

        mat_free(A);
    }

    /* 3×3 */
    {
        matrix_t *A = mat_create_d(3,3);
        double vals[9] = {
            6, 1, 1,
            4, -2, 5,
            2, 8, 7
        };

        size_t k = 0;
        for (size_t i=0;i<3;i++)
            for (size_t j=0;j<3;j++)
                mat_set(A,i,j,&vals[k++]);

        print_md("A (3x3)", A);

        double det;
        mat_det(A, &det);
        check_d("det 3x3 example = -306", det, -306.0, 1e-30);

        mat_free(A);
    }

    /* singular */
    {
        matrix_t *A = mat_create_d(2,2);
        double a11=1, a12=2, a21=2, a22=4;
        mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
        mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

        print_md("A (singular)", A);

        double det;
        mat_det(A, &det);
        check_d("det singular = 0", det, 0.0, 1e-30);

        mat_free(A);
    }

    /* identity */
    {
        matrix_t *I = matsq_ident_d(4);

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

    matrix_t *A = mat_create_qf(2,2);

    qfloat_t a11 = qf_from_double(1.5);
    qfloat_t a12 = qf_from_double(2.0);
    qfloat_t a21 = qf_from_double(-3.0);
    qfloat_t a22 = qf_from_double(4.25);

    mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
    mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

    print_mqf("A (qfloat 2x2)", A);

    qfloat_t det;
    mat_det(A, &det);

    qfloat_t expected = qf_sub(qf_mul(a11,a22), qf_mul(a12,a21));
    check_qf_val("det qfloat 2x2", det, expected, 1e-28);

    mat_free(A);
}

/* ------------------------------------------------------------------ determinant: qcomplex */

static void test_det_qcomplex(void)
{
    printf(C_CYAN "TEST: determinant (qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qc(2,2);

    qcomplex_t z11 = qc_make(qf_from_double(1), qf_from_double(2));
    qcomplex_t z12 = qc_make(qf_from_double(3), qf_from_double(-1));
    qcomplex_t z21 = qc_make(qf_from_double(0), qf_from_double(4));
    qcomplex_t z22 = qc_make(qf_from_double(-2), qf_from_double(1));

    mat_set(A,0,0,&z11); mat_set(A,0,1,&z12);
    mat_set(A,1,0,&z21); mat_set(A,1,1,&z22);

    print_mqc("A (qcomplex 2x2)", A);

    qcomplex_t det;
    mat_det(A, &det);

    qcomplex_t expected =
        qc_sub(qc_mul(z11,z22), qc_mul(z12,z21));

    check_qc_val("det qcomplex 2x2", det, expected, 1e-28);

    mat_free(A);
}

/* ------------------------------------------------------------------ inverse: double */

static void test_inverse_double(void)
{
    printf(C_CYAN "TEST: matrix inverse (double)\n" C_RESET);

    /* 2×2 invertible */
    {
        matrix_t *A = mat_create_d(2,2);
        double a11=4, a12=7, a21=2, a22=6;
        mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
        mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

        print_md("A", A);

        matrix_t *Ai = mat_inverse(A);
        check_bool("inverse returned non-null", Ai != NULL);

        print_md("A^{-1}", Ai);

        /* Check A * A^{-1} = I */
        matrix_t *P = mat_mul(A, Ai);
        print_md("A * A^{-1}", P);

        double v;
        mat_get(P,0,0,&v); check_d("prod[0,0] = 1", v, 1, 1e-12);
        mat_get(P,1,1,&v); check_d("prod[1,1] = 1", v, 1, 1e-12);
        mat_get(P,0,1,&v); check_d("prod[0,1] = 0", v, 0, 1e-12);
        mat_get(P,1,0,&v); check_d("prod[1,0] = 0", v, 0, 1e-12);

        mat_free(A);
        mat_free(Ai);
        mat_free(P);
    }

    /* identity inverse */
    {
        matrix_t *I = matsq_ident_d(3);
        print_md("I", I);

        matrix_t *Ii = mat_inverse(I);
        check_bool("inverse(identity) non-null", Ii != NULL);

        print_md("I^{-1}", Ii);

        double v;
        mat_get(Ii,0,0,&v); check_d("I^{-1}[0,0] = 1", v, 1, 1e-30);
        mat_get(Ii,1,1,&v); check_d("I^{-1}[1,1] = 1", v, 1, 1e-30);
        mat_get(Ii,2,2,&v); check_d("I^{-1}[2,2] = 1", v, 1, 1e-30);

        mat_free(I);
        mat_free(Ii);
    }

    /* singular matrix */
    {
        matrix_t *A = mat_create_d(2,2);
        double a11=1, a12=2, a21=2, a22=4;
        mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
        mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

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

    matrix_t *A = mat_create_qf(2,2);

    qfloat_t a11 = qf_from_double(3.0);
    qfloat_t a12 = qf_from_double(1.0);
    qfloat_t a21 = qf_from_double(2.0);
    qfloat_t a22 = qf_from_double(1.0);

    mat_set(A,0,0,&a11); mat_set(A,0,1,&a12);
    mat_set(A,1,0,&a21); mat_set(A,1,1,&a22);

    print_mqf("A", A);

    matrix_t *Ai = mat_inverse(A);
    check_bool("inverse returned non-null", Ai != NULL);

    print_mqf("A^{-1}", Ai);

    /* Check A * A^{-1} = I */
    matrix_t *P = mat_mul(A, Ai);
    print_mqf("A * A^{-1}", P);

    qfloat_t v;
    mat_get(P,0,0,&v); check_qf_val("prod[0,0] = 1", v, QF_ONE, 1e-28);
    mat_get(P,1,1,&v); check_qf_val("prod[1,1] = 1", v, QF_ONE, 1e-28);

    qfloat_t z = QF_ZERO;
    mat_get(P,0,1,&v); check_qf_val("prod[0,1] = 0", v, z, 1e-28);
    mat_get(P,1,0,&v); check_qf_val("prod[1,0] = 0", v, z, 1e-28);

    mat_free(A);
    mat_free(Ai);
    mat_free(P);
}

/* ------------------------------------------------------------------ inverse: qcomplex */

static void test_inverse_qcomplex(void)
{
    printf(C_CYAN "TEST: matrix inverse (qcomplex)\n" C_RESET);

    matrix_t *A = mat_create_qc(2,2);

    qcomplex_t z11 = qc_make(qf_from_double(1), qf_from_double(1));
    qcomplex_t z12 = qc_make(qf_from_double(2), qf_from_double(-1));
    qcomplex_t z21 = qc_make(qf_from_double(0), qf_from_double(3));
    qcomplex_t z22 = qc_make(qf_from_double(4), qf_from_double(0));

    mat_set(A,0,0,&z11); mat_set(A,0,1,&z12);
    mat_set(A,1,0,&z21); mat_set(A,1,1,&z22);

    print_mqc("A", A);

    matrix_t *Ai = mat_inverse(A);
    check_bool("inverse returned non-null", Ai != NULL);

    print_mqc("A^{-1}", Ai);

    /* Check A * A^{-1} = I */
    matrix_t *P = mat_mul(A, Ai);
    print_mqc("A * A^{-1}", P);

    qcomplex_t got;

    mat_get(P,0,0,&got);
    check_qc_val("prod[0,0] = 1", got, QC_ONE, 1e-28);

    mat_get(P,1,1,&got);
    check_qc_val("prod[1,1] = 1", got, QC_ONE, 1e-28);

    qcomplex_t zero = QC_ZERO;

    mat_get(P,0,1,&got);
    check_qc_val("prod[0,1] = 0", got, zero, 1e-28);

    mat_get(P,1,0,&got);
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
        matrix_t *A = mat_create_d(2,3);
        double x = 1, y = 2, z = 3;
        mat_set(A,0,1,&x);
        mat_set(A,1,2,&y);
        mat_set(A,0,2,&z);

        print_md("A (double)", A);

        matrix_t *H = mat_hermitian(A);
        print_md("A^† (double)", H);

        double v;
        mat_get(H,1,0,&v); check_d("H[1,0] = 1", v, 1, 1e-30);
        mat_get(H,2,1,&v); check_d("H[2,1] = 2", v, 2, 1e-30);
        mat_get(H,2,0,&v); check_d("H[2,0] = 3", v, 3, 1e-30);

        mat_free(A);
        mat_free(H);
    }

    /* qcomplex: Hermitian = conjugate transpose */
    {
        matrix_t *A = mat_create_qc(2,2);

        qcomplex_t z11 = qc_make(qf_from_double(1), qf_from_double(2));
        qcomplex_t z12 = qc_make(qf_from_double(3), qf_from_double(-1));
        qcomplex_t z21 = qc_make(qf_from_double(4), qf_from_double(5));
        qcomplex_t z22 = qc_make(qf_from_double(-2), qf_from_double(0));

        mat_set(A,0,0,&z11);
        mat_set(A,0,1,&z12);
        mat_set(A,1,0,&z21);
        mat_set(A,1,1,&z22);

        print_mqc("A (qcomplex)", A);

        matrix_t *H = mat_hermitian(A);
        print_mqc("A^† (qcomplex)", H);

        qcomplex_t got;

        mat_get(H,0,0,&got); check_qc_val("H[0,0] = conj(1+2i)", got, qc_conj(z11), 1e-28);
        mat_get(H,1,1,&got); check_qc_val("H[1,1] = conj(-2+0i)", got, qc_conj(z22), 1e-28);
        mat_get(H,1,0,&got); check_qc_val("H[1,0] = conj(A[0,1])", got, qc_conj(z12), 1e-28);
        mat_get(H,0,1,&got); check_qc_val("H[0,1] = conj(A[1,0])", got, qc_conj(z21), 1e-28);

        mat_free(A);
        mat_free(H);
    }
}

/* ------------------------------------------------------------------ eigendecomposition: double */

static void test_eigen_d(void)
{
    printf(C_CYAN "TEST: eigendecomposition (double)\n" C_RESET);

    /* A = [[5, 2], [2, 8]] — eigenvalues 4 and 9 */
    matrix_t *A = mat_create_d(2, 2);
    double a00=5, a01=2, a10=2, a11=8;
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);

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
    for (size_t k = 0; k < 2; k++) {
        for (size_t i = 0; i < 2; i++) {
            double Av_ik = 0.0;
            for (size_t j = 0; j < 2; j++) {
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
    matrix_t *A = mat_create_qf(2, 2);
    qfloat_t a00 = qf_from_double(5);
    qfloat_t a01 = qf_from_double(2);
    qfloat_t a10 = qf_from_double(2);
    qfloat_t a11 = qf_from_double(8);
    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);

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
    for (size_t k = 0; k < 2; k++) {
        for (size_t i = 0; i < 2; i++) {
            qfloat_t Av_ik = QF_ZERO;
            for (size_t j = 0; j < 2; j++) {
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
    matrix_t *A = mat_create_qc(2, 2);
    qcomplex_t z00 = qc_make(qf_from_double(2),  QF_ZERO);
    qcomplex_t z01 = qc_make(qf_from_double(1),  qf_from_double(1));
    qcomplex_t z10 = qc_make(qf_from_double(1),  qf_from_double(-1));
    qcomplex_t z11 = qc_make(qf_from_double(3),  QF_ZERO);
    mat_set(A, 0, 0, &z00);
    mat_set(A, 0, 1, &z01);
    mat_set(A, 1, 0, &z10);
    mat_set(A, 1, 1, &z11);

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
    for (size_t k = 0; k < 2; k++) {
        for (size_t i = 0; i < 2; i++) {
            qcomplex_t Av_ik = QC_ZERO;
            for (size_t j = 0; j < 2; j++) {
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 2.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(1x1) not NULL", E != NULL);
        if (E) {
            print_md("exp(A)", E);
            double got;
            mat_get(E, 0, 0, &got);
            check_d("exp([[2]]) = e²", got, exp(2.0), 1e-12);
        }
        mat_free(A); mat_free(E);
    }

    /* 2×2 diagonal: exp(diag(a,b)) = diag(exp(a),exp(b)) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double a=1.0, b=2.0, z=0.0;
        mat_set(A, 0, 0, &a); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(diag) not NULL", E != NULL);
        if (E) {
            print_md("exp(A)", E);
            double e00, e01, e10, e11;
            mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
            check_d("exp(diag)[0,0] = e",   e00, exp(1.0), 1e-12);
            check_d("exp(diag)[1,1] = e²",  e11, exp(2.0), 1e-12);
            check_d("exp(diag)[0,1] = 0",   e01, 0.0,      1e-12);
            check_d("exp(diag)[1,0] = 0",   e10, 0.0,      1e-12);
        }
        mat_free(A); mat_free(E);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(sym) not NULL", E != NULL);
        if (E) {
            print_md("exp(A)", E);
            double e00, e01, e10, e11;
            mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
            double ch = cosh(1.0), sh = sinh(1.0);
            check_d("exp([[0,1],[1,0]])[0,0] = cosh(1)", e00, ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,1] = cosh(1)", e11, ch, 1e-12);
            check_d("exp([[0,1],[1,0]])[0,1] = sinh(1)", e01, sh, 1e-12);
            check_d("exp([[0,1],[1,0]])[1,0] = sinh(1)", e10, sh, 1e-12);
        }
        mat_free(A); mat_free(E);
    }

    /* zero matrix: exp(0) = I */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z = 0.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp(zero) not NULL", E != NULL);
        if (E) {
            print_md("exp(A)", E);
            double e00, e01, e10, e11;
            mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
            check_d("exp(0)[0,0] = 1", e00, 1.0, 1e-12);
            check_d("exp(0)[1,1] = 1", e11, 1.0, 1e-12);
            check_d("exp(0)[0,1] = 0", e01, 0.0, 1e-12);
            check_d("exp(0)[1,0] = 0", e10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(E);
    }
}

static void test_mat_exp_qf(void)
{
    printf(C_CYAN "TEST: mat_exp (qfloat)\n" C_RESET);

    /* 1×1: exp([[x]]) = [[exp(x)]] */
    {
        matrix_t *A = mat_create_qf(1, 1);
        qfloat_t v = qf_from_double(2.0);
        mat_set(A, 0, 0, &v);
        print_mqf("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(1x1) not NULL", E != NULL);
        if (E) {
            print_mqf("exp(A)", E);
            qfloat_t got;
            mat_get(E, 0, 0, &got);
            check_qf_val("qf exp([[2]]) = e²", got, qf_exp(qf_from_double(2.0)), 1e-25);
        }
        mat_free(A); mat_free(E);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        matrix_t *A = mat_create_qf(2, 2);
        qfloat_t z = QF_ZERO, o = QF_ONE;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_mqf("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qf(sym) not NULL", E != NULL);
        if (E) {
            print_mqf("exp(A)", E);
            qfloat_t e00, e01, e10, e11;
            mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
            /* cosh(1) = (e + 1/e) / 2, sinh(1) = (e - 1/e) / 2 */
            qfloat_t e1   = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two  = qf_from_double(2.0);
            qfloat_t ch   = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh   = qf_div(qf_sub(e1, inv1), two);
            check_qf_val("qf exp(sym)[0,0] = cosh(1)", e00, ch, 1e-25);
            check_qf_val("qf exp(sym)[1,1] = cosh(1)", e11, ch, 1e-25);
            check_qf_val("qf exp(sym)[0,1] = sinh(1)", e01, sh, 1e-25);
            check_qf_val("qf exp(sym)[1,0] = sinh(1)", e10, sh, 1e-25);
        }
        mat_free(A); mat_free(E);
    }
}

static void test_mat_exp_qc(void)
{
    printf(C_CYAN "TEST: mat_exp (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]]
     * eigenvalues ±1 → exp(A) = cosh(1)·I + sinh(1)·A */
    {
        matrix_t *A = mat_create_qc(2, 2);
        qcomplex_t z   = QC_ZERO;
        qcomplex_t pi  = qc_make(QF_ZERO,  QF_ONE);   /*  i */
        qcomplex_t ni  = qc_make(QF_ZERO,  qf_neg(QF_ONE)); /* -i */
        mat_set(A, 0, 0, &z);  mat_set(A, 0, 1, &pi);
        mat_set(A, 1, 0, &ni); mat_set(A, 1, 1, &z);
        print_mqc("A", A);
        matrix_t *E = mat_exp(A);
        check_bool("mat_exp qc(herm) not NULL", E != NULL);
        if (E) {
            print_mqc("exp(A)", E);
            qcomplex_t e00, e01, e10, e11;
            mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
            mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
            qfloat_t e1   = qf_exp(QF_ONE);
            qfloat_t inv1 = qf_div(QF_ONE, e1);
            qfloat_t two  = qf_from_double(2.0);
            qfloat_t ch   = qf_div(qf_add(e1, inv1), two);
            qfloat_t sh   = qf_div(qf_sub(e1, inv1), two);
            qcomplex_t ch_c = qc_make(ch, QF_ZERO);
            qcomplex_t ish  = qc_make(QF_ZERO, sh);   /* i·sinh(1) */
            qcomplex_t nish = qc_make(QF_ZERO, qf_neg(sh)); /* -i·sinh(1) */
            check_qc_val("qc exp(herm)[0,0] = cosh(1)",   e00, ch_c, 1e-25);
            check_qc_val("qc exp(herm)[1,1] = cosh(1)",   e11, ch_c, 1e-25);
            check_qc_val("qc exp(herm)[0,1] = i·sinh(1)", e01, ish,  1e-25);
            check_qc_val("qc exp(herm)[1,0] = -i·sinh(1)",e10, nish, 1e-25);
        }
        mat_free(A); mat_free(E);
    }
}

static void test_mat_exp_null_safety(void)
{
    printf(C_CYAN "TEST: mat_exp null safety\n" C_RESET);
    check_bool("mat_exp(NULL) = NULL", mat_exp(NULL) == NULL);

    matrix_t *A = mat_create_d(2, 3);
    check_bool("mat_exp(non-square) = NULL", mat_exp(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_sin */

static void test_mat_sin_d(void)
{
    printf(C_CYAN "TEST: mat_sin (double)\n" C_RESET);

    /* 1×1: sin([[π/6]]) = [[0.5]] */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = M_PI / 6.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(1x1) not NULL", S != NULL);
        if (S) {
            print_md("sin(A)", S);
            double got;
            mat_get(S, 0, 0, &got);
            check_d("sin([[π/6]]) = 0.5", got, 0.5, 1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 diagonal: sin(diag(0, π/2)) = diag(0, 1) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, h=M_PI/2.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &h);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(diag) not NULL", S != NULL);
        if (S) {
            print_md("sin(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            check_d("sin(diag)[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sin(diag)[1,1] = 1", s11, 1.0, 1e-12);
            check_d("sin(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sin(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(sym) not NULL", S != NULL);
        if (S) {
            print_md("sin(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            double s1 = sin(1.0);
            check_d("sin([[0,1],[1,0]])[0,0] = 0",      s00, 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[1,1] = 0",      s11, 0.0, 1e-12);
            check_d("sin([[0,1],[1,0]])[0,1] = sin(1)", s01, s1,  1e-12);
            check_d("sin([[0,1],[1,0]])[1,0] = sin(1)", s10, s1,  1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* zero matrix: sin(0) = 0 */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z = 0.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin(zero) not NULL", S != NULL);
        if (S) {
            print_md("sin(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            check_d("sin(0)[0,0] = 0", s00, 0.0, 1e-12);
            check_d("sin(0)[1,1] = 0", s11, 0.0, 1e-12);
            check_d("sin(0)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sin(0)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(S);
    }
}

static void test_mat_sin_qf(void)
{
    printf(C_CYAN "TEST: mat_sin (qfloat)\n" C_RESET);

    /* 1×1: sin([[π/6]]) = [[0.5]] */
    {
        matrix_t *A = mat_create_qf(1, 1);
        qfloat_t v = qf_div(QF_PI, qf_from_double(6.0));
        mat_set(A, 0, 0, &v);
        print_mqf("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(1x1) not NULL", S != NULL);
        if (S) {
            print_mqf("sin(A)", S);
            qfloat_t got;
            mat_get(S, 0, 0, &got);
            check_qf_val("qf sin([[π/6]]) = 0.5", got, qf_from_double(0.5), 1e-25);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → sin(A) = sin(1)·A */
    {
        matrix_t *A = mat_create_qf(2, 2);
        qfloat_t z = QF_ZERO, o = QF_ONE;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_mqf("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qf(sym) not NULL", S != NULL);
        if (S) {
            print_mqf("sin(A)", S);
            qfloat_t s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            qfloat_t s1 = qf_sin(QF_ONE);
            check_qf_val("qf sin(sym)[0,0] = 0",      s00, QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[1,1] = 0",      s11, QF_ZERO, 1e-25);
            check_qf_val("qf sin(sym)[0,1] = sin(1)", s01, s1,      1e-25);
            check_qf_val("qf sin(sym)[1,0] = sin(1)", s10, s1,      1e-25);
        }
        mat_free(A); mat_free(S);
    }
}

static void test_mat_sin_qc(void)
{
    printf(C_CYAN "TEST: mat_sin (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * A² = I, so sin(A) = sin(1)·A = [[0, i·sin(1)], [-i·sin(1), 0]] */
    {
        matrix_t *A = mat_create_qc(2, 2);
        qcomplex_t z  = QC_ZERO;
        qcomplex_t pi = qc_make(QF_ZERO,  QF_ONE);
        qcomplex_t ni = qc_make(QF_ZERO,  qf_neg(QF_ONE));
        mat_set(A, 0, 0, &z);  mat_set(A, 0, 1, &pi);
        mat_set(A, 1, 0, &ni); mat_set(A, 1, 1, &z);
        print_mqc("A", A);
        matrix_t *S = mat_sin(A);
        check_bool("mat_sin qc(herm) not NULL", S != NULL);
        if (S) {
            print_mqc("sin(A)", S);
            qcomplex_t s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            qfloat_t s1 = qf_sin(QF_ONE);
            qcomplex_t zero_c = QC_ZERO;
            qcomplex_t ish    = qc_make(QF_ZERO, s1);
            qcomplex_t nish   = qc_make(QF_ZERO, qf_neg(s1));
            check_qc_val("qc sin(herm)[0,0] = 0",         s00, zero_c, 1e-25);
            check_qc_val("qc sin(herm)[1,1] = 0",         s11, zero_c, 1e-25);
            check_qc_val("qc sin(herm)[0,1] = i·sin(1)",  s01, ish,    1e-25);
            check_qc_val("qc sin(herm)[1,0] = -i·sin(1)", s10, nish,   1e-25);
        }
        mat_free(A); mat_free(S);
    }
}

static void test_mat_sin_null_safety(void)
{
    printf(C_CYAN "TEST: mat_sin null safety\n" C_RESET);
    check_bool("mat_sin(NULL) = NULL", mat_sin(NULL) == NULL);

    matrix_t *A = mat_create_d(2, 3);
    check_bool("mat_sin(non-square) = NULL", mat_sin(A) == NULL);
    mat_free(A);
}

/* ------------------------------------------------------------------ mat_cos */

static void test_mat_cos_d(void)
{
    printf(C_CYAN "TEST: mat_cos (double)\n" C_RESET);

    /* 1×1: cos([[π/3]]) = 0.5 */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = M_PI / 3.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(1x1) not NULL", C != NULL);
        if (C) {
            print_md("cos(A)", C);
            double got;
            mat_get(C, 0, 0, &got);
            check_d("cos([[π/3]]) = 0.5", got, 0.5, 1e-12);
        }
        mat_free(A); mat_free(C);
    }

    /* 2×2 diagonal: cos(diag(0, π)) = diag(1, -1) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, p=M_PI;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &p);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(diag) not NULL", C != NULL);
        if (C) {
            print_md("cos(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            check_d("cos(diag)[0,0] =  1", c00,  1.0, 1e-12);
            check_d("cos(diag)[1,1] = -1", c11, -1.0, 1e-12);
            check_d("cos(diag)[0,1] =  0", c01,  0.0, 1e-12);
            check_d("cos(diag)[1,0] =  0", c10,  0.0, 1e-12);
        }
        mat_free(A); mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos(sym) not NULL", C != NULL);
        if (C) {
            print_md("cos(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            double c1 = cos(1.0);
            check_d("cos([[0,1],[1,0]])[0,0] = cos(1)", c00, c1,  1e-12);
            check_d("cos([[0,1],[1,0]])[1,1] = cos(1)", c11, c1,  1e-12);
            check_d("cos([[0,1],[1,0]])[0,1] = 0",      c01, 0.0, 1e-12);
            check_d("cos([[0,1],[1,0]])[1,0] = 0",      c10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(C);
    }
}

static void test_mat_cos_qf(void)
{
    printf(C_CYAN "TEST: mat_cos (qfloat)\n" C_RESET);

    /* 1×1: cos([[π/3]]) = 0.5 */
    {
        matrix_t *A = mat_create_qf(1, 1);
        qfloat_t v = qf_div(QF_PI, qf_from_double(3.0));
        mat_set(A, 0, 0, &v);
        print_mqf("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(1x1) not NULL", C != NULL);
        if (C) {
            print_mqf("cos(A)", C);
            qfloat_t got;
            mat_get(C, 0, 0, &got);
            check_qf_val("qf cos([[π/3]]) = 0.5", got, qf_from_double(0.5), 1e-25);
        }
        mat_free(A); mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]] → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_create_qf(2, 2);
        qfloat_t z = QF_ZERO, o = QF_ONE;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_mqf("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qf(sym) not NULL", C != NULL);
        if (C) {
            print_mqf("cos(A)", C);
            qfloat_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            check_qf_val("qf cos(sym)[0,0] = cos(1)", c00, c1,     1e-25);
            check_qf_val("qf cos(sym)[1,1] = cos(1)", c11, c1,     1e-25);
            check_qf_val("qf cos(sym)[0,1] = 0",      c01, QF_ZERO, 1e-25);
            check_qf_val("qf cos(sym)[1,0] = 0",      c10, QF_ZERO, 1e-25);
        }
        mat_free(A); mat_free(C);
    }
}

static void test_mat_cos_qc(void)
{
    printf(C_CYAN "TEST: mat_cos (qcomplex)\n" C_RESET);

    /* Hermitian 2×2: A = [[0, i], [-i, 0]], eigenvalues ±1.
     * cos is even → cos(A) = cos(1)·I */
    {
        matrix_t *A = mat_create_qc(2, 2);
        qcomplex_t z  = QC_ZERO;
        qcomplex_t pi = qc_make(QF_ZERO,  QF_ONE);
        qcomplex_t ni = qc_make(QF_ZERO,  qf_neg(QF_ONE));
        mat_set(A, 0, 0, &z);  mat_set(A, 0, 1, &pi);
        mat_set(A, 1, 0, &ni); mat_set(A, 1, 1, &z);
        print_mqc("A", A);
        matrix_t *C = mat_cos(A);
        check_bool("mat_cos qc(herm) not NULL", C != NULL);
        if (C) {
            print_mqc("cos(A)", C);
            qcomplex_t c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            qfloat_t c1 = qf_cos(QF_ONE);
            qcomplex_t c1_c    = qc_make(c1, QF_ZERO);
            qcomplex_t zero_c  = QC_ZERO;
            check_qc_val("qc cos(herm)[0,0] = cos(1)", c00, c1_c,   1e-25);
            check_qc_val("qc cos(herm)[1,1] = cos(1)", c11, c1_c,   1e-25);
            check_qc_val("qc cos(herm)[0,1] = 0",      c01, zero_c, 1e-25);
            check_qc_val("qc cos(herm)[1,0] = 0",      c10, zero_c, 1e-25);
        }
        mat_free(A); mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tan */

static void test_mat_tan_d(void)
{
    printf(C_CYAN "TEST: mat_tan (double)\n" C_RESET);

    /* 1×1: tan([[π/4]]) = 1 */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = M_PI / 4.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(1x1) not NULL", T != NULL);
        if (T) {
            print_md("tan(A)", T);
            double got;
            mat_get(T, 0, 0, &got);
            check_d("tan([[π/4]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A); mat_free(T);
    }

    /* 2×2 diagonal: tan(diag(0, π/4)) = diag(0, 1) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, h=M_PI/4.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &h);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(diag) not NULL", T != NULL);
        if (T) {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00); mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10); mat_get(T, 1, 1, &t11);
            check_d("tan(diag)[0,0] = 0", t00, 0.0, 1e-12);
            check_d("tan(diag)[1,1] = 1", t11, 1.0, 1e-12);
            check_d("tan(diag)[0,1] = 0", t01, 0.0, 1e-12);
            check_d("tan(diag)[1,0] = 0", t10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tan is odd → tan(A) = tan(1)·A */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tan(A);
        check_bool("mat_tan(sym) not NULL", T != NULL);
        if (T) {
            print_md("tan(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00); mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10); mat_get(T, 1, 1, &t11);
            double t1 = tan(1.0);
            check_d("tan([[0,1],[1,0]])[0,0] = 0",      t00, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[1,1] = 0",      t11, 0.0, 1e-12);
            check_d("tan([[0,1],[1,0]])[0,1] = tan(1)", t01, t1,  1e-12);
            check_d("tan([[0,1],[1,0]])[1,0] = tan(1)", t10, t1,  1e-12);
        }
        mat_free(A); mat_free(T);
    }
}

/* ------------------------------------------------------------------ mat_sinh */

static void test_mat_sinh_d(void)
{
    printf(C_CYAN "TEST: mat_sinh (double)\n" C_RESET);

    /* 1×1: sinh([[0]]) = 0 */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(1x1) not NULL", S != NULL);
        if (S) {
            print_md("sinh(A)", S);
            double got;
            mat_get(S, 0, 0, &got);
            check_d("sinh([[0]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 diagonal: sinh(diag(0, 1)) = diag(0, sinh(1)) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(diag) not NULL", S != NULL);
        if (S) {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            check_d("sinh(diag)[0,0] = 0",       s00, 0.0,     1e-12);
            check_d("sinh(diag)[1,1] = sinh(1)",  s11, sinh(1.0), 1e-12);
            check_d("sinh(diag)[0,1] = 0",        s01, 0.0,     1e-12);
            check_d("sinh(diag)[1,0] = 0",        s10, 0.0,     1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * sinh is odd → sinh(A) = sinh(1)·A */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *S = mat_sinh(A);
        check_bool("mat_sinh(sym) not NULL", S != NULL);
        if (S) {
            print_md("sinh(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            double sh = sinh(1.0);
            check_d("sinh([[0,1],[1,0]])[0,0] = 0",       s00, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[1,1] = 0",       s11, 0.0, 1e-12);
            check_d("sinh([[0,1],[1,0]])[0,1] = sinh(1)", s01, sh,  1e-12);
            check_d("sinh([[0,1],[1,0]])[1,0] = sinh(1)", s10, sh,  1e-12);
        }
        mat_free(A); mat_free(S);
    }
}

/* ------------------------------------------------------------------ mat_cosh */

static void test_mat_cosh_d(void)
{
    printf(C_CYAN "TEST: mat_cosh (double)\n" C_RESET);

    /* 1×1: cosh([[0]]) = 1 */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(1x1) not NULL", C != NULL);
        if (C) {
            print_md("cosh(A)", C);
            double got;
            mat_get(C, 0, 0, &got);
            check_d("cosh([[0]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A); mat_free(C);
    }

    /* 2×2 diagonal: cosh(diag(0, 1)) = diag(1, cosh(1)) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(diag) not NULL", C != NULL);
        if (C) {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            check_d("cosh(diag)[0,0] = 1",       c00, 1.0,      1e-12);
            check_d("cosh(diag)[1,1] = cosh(1)", c11, cosh(1.0), 1e-12);
            check_d("cosh(diag)[0,1] = 0",       c01, 0.0,      1e-12);
            check_d("cosh(diag)[1,0] = 0",       c10, 0.0,      1e-12);
        }
        mat_free(A); mat_free(C);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * cosh is even → cosh(A) = cosh(1)·I */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *C = mat_cosh(A);
        check_bool("mat_cosh(sym) not NULL", C != NULL);
        if (C) {
            print_md("cosh(A)", C);
            double c00, c01, c10, c11;
            mat_get(C, 0, 0, &c00); mat_get(C, 0, 1, &c01);
            mat_get(C, 1, 0, &c10); mat_get(C, 1, 1, &c11);
            double ch = cosh(1.0);
            check_d("cosh([[0,1],[1,0]])[0,0] = cosh(1)", c00, ch,  1e-12);
            check_d("cosh([[0,1],[1,0]])[1,1] = cosh(1)", c11, ch,  1e-12);
            check_d("cosh([[0,1],[1,0]])[0,1] = 0",       c01, 0.0, 1e-12);
            check_d("cosh([[0,1],[1,0]])[1,0] = 0",       c10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(C);
    }
}

/* ------------------------------------------------------------------ mat_tanh */

static void test_mat_tanh_d(void)
{
    printf(C_CYAN "TEST: mat_tanh (double)\n" C_RESET);

    /* 1×1: tanh([[0]]) = 0 */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(1x1) not NULL", T != NULL);
        if (T) {
            print_md("tanh(A)", T);
            double got;
            mat_get(T, 0, 0, &got);
            check_d("tanh([[0]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A); mat_free(T);
    }

    /* 2×2 diagonal: tanh(diag(0, 1)) = diag(0, tanh(1)) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &o);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(diag) not NULL", T != NULL);
        if (T) {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00); mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10); mat_get(T, 1, 1, &t11);
            check_d("tanh(diag)[0,0] = 0",       t00, 0.0,      1e-12);
            check_d("tanh(diag)[1,1] = tanh(1)", t11, tanh(1.0), 1e-12);
            check_d("tanh(diag)[0,1] = 0",       t01, 0.0,      1e-12);
            check_d("tanh(diag)[1,0] = 0",       t10, 0.0,      1e-12);
        }
        mat_free(A); mat_free(T);
    }

    /* 2×2 symmetric: A = [[0,1],[1,0]], eigenvalues ±1.
     * tanh is odd → tanh(A) = tanh(1)·A */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0;
        mat_set(A, 0, 0, &z); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &z);
        print_md("A", A);
        matrix_t *T = mat_tanh(A);
        check_bool("mat_tanh(sym) not NULL", T != NULL);
        if (T) {
            print_md("tanh(A)", T);
            double t00, t01, t10, t11;
            mat_get(T, 0, 0, &t00); mat_get(T, 0, 1, &t01);
            mat_get(T, 1, 0, &t10); mat_get(T, 1, 1, &t11);
            double th = tanh(1.0);
            check_d("tanh([[0,1],[1,0]])[0,0] = 0",       t00, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[1,1] = 0",       t11, 0.0, 1e-12);
            check_d("tanh([[0,1],[1,0]])[0,1] = tanh(1)", t01, th,  1e-12);
            check_d("tanh([[0,1],[1,0]])[1,0] = tanh(1)", t10, th,  1e-12);
        }
        mat_free(A); mat_free(T);
    }
}

static void test_mat_trig_null_safety(void)
{
    printf(C_CYAN "TEST: mat_cos/tan/sinh/cosh/tanh null safety\n" C_RESET);

    check_bool("mat_cos(NULL) = NULL",  mat_cos(NULL)  == NULL);
    check_bool("mat_tan(NULL) = NULL",  mat_tan(NULL)  == NULL);
    check_bool("mat_sinh(NULL) = NULL", mat_sinh(NULL) == NULL);
    check_bool("mat_cosh(NULL) = NULL", mat_cosh(NULL) == NULL);
    check_bool("mat_tanh(NULL) = NULL", mat_tanh(NULL) == NULL);

    matrix_t *A = mat_create_d(2, 3);
    check_bool("mat_cos(non-square) = NULL",  mat_cos(A)  == NULL);
    check_bool("mat_tan(non-square) = NULL",  mat_tan(A)  == NULL);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 4.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(1x1) not NULL", S != NULL);
        if (S) {
            print_md("sqrt(A)", S);
            double got; mat_get(S, 0, 0, &got);
            check_d("sqrt([[4]]) = 2", got, 2.0, 1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* 2×2 diagonal: sqrt(diag(1,4)) = diag(1,2) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, o=1.0, f=4.0;
        mat_set(A, 0, 0, &o); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &f);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(diag) not NULL", S != NULL);
        if (S) {
            print_md("sqrt(A)", S);
            double s00, s01, s10, s11;
            mat_get(S, 0, 0, &s00); mat_get(S, 0, 1, &s01);
            mat_get(S, 1, 0, &s10); mat_get(S, 1, 1, &s11);
            check_d("sqrt(diag)[0,0] = 1", s00, 1.0, 1e-12);
            check_d("sqrt(diag)[1,1] = 2", s11, 2.0, 1e-12);
            check_d("sqrt(diag)[0,1] = 0", s01, 0.0, 1e-12);
            check_d("sqrt(diag)[1,0] = 0", s10, 0.0, 1e-12);
        }
        mat_free(A); mat_free(S);
    }

    /* Verify: sqrt(A)^2 = A for [[2,1],[1,2]] (eigenvalues 1 and 3) */
    {
        matrix_t *A = mat_create_d(2, 2);
        double o=1.0, t=2.0;
        mat_set(A, 0, 0, &t); mat_set(A, 0, 1, &o);
        mat_set(A, 1, 0, &o); mat_set(A, 1, 1, &t);
        print_md("A", A);
        matrix_t *S = mat_sqrt(A);
        check_bool("mat_sqrt(sym) not NULL", S != NULL);
        if (S) {
            print_md("sqrt(A)", S);
            matrix_t *S2 = mat_mul(S, S);
            check_bool("sqrt(A)^2 not NULL", S2 != NULL);
            if (S2) {
                print_md("sqrt(A)^2", S2);
                double r00, r01, r10, r11;
                mat_get(S2, 0, 0, &r00); mat_get(S2, 0, 1, &r01);
                mat_get(S2, 1, 0, &r10); mat_get(S2, 1, 1, &r11);
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
    matrix_t *A = mat_create_qf(1, 1);
    qfloat_t v = qf_from_double(9.0);
    mat_set(A, 0, 0, &v);
    print_mqf("A", A);
    matrix_t *S = mat_sqrt(A);
    check_bool("qf mat_sqrt(1x1) not NULL", S != NULL);
    if (S) {
        print_mqf("sqrt(A)", S);
        qfloat_t got; mat_get(S, 0, 0, &got);
        check_qf_val("qf sqrt([[9]]) = 3", got, qf_from_double(3.0), 1e-25);
    }
    mat_free(A); mat_free(S);
}

/* ------------------------------------------------------------------ mat_log */

static void test_mat_log_d(void)
{
    printf(C_CYAN "TEST: mat_log (double)\n" C_RESET);

    /* 1×1: log([[1]]) = [[0]] */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 1.0;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log([[1]]) not NULL", L != NULL);
        if (L) {
            print_md("log(A)", L);
            double got; mat_get(L, 0, 0, &got);
            check_d("log([[1]]) = 0", got, 0.0, 1e-12);
        }
        mat_free(A); mat_free(L);
    }

    /* 1×1: log([[e]]) = [[1]] */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = M_E;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log([[e]]) not NULL", L != NULL);
        if (L) {
            print_md("log(A)", L);
            double got; mat_get(L, 0, 0, &got);
            check_d("log([[e]]) = 1", got, 1.0, 1e-12);
        }
        mat_free(A); mat_free(L);
    }

    /* exp(log(A)) = A for diagonal [[2,0],[0,3]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, t=2.0, th=3.0;
        mat_set(A, 0, 0, &t);  mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);  mat_set(A, 1, 1, &th);
        print_md("A", A);
        matrix_t *L = mat_log(A);
        check_bool("mat_log(diag) not NULL", L != NULL);
        if (L) {
            print_md("log(A)", L);
            matrix_t *E = mat_exp(L);
            check_bool("exp(log(A)) not NULL", E != NULL);
            if (E) {
                print_md("exp(log(A))", E);
                double e00, e01, e10, e11;
                mat_get(E, 0, 0, &e00); mat_get(E, 0, 1, &e01);
                mat_get(E, 1, 0, &e10); mat_get(E, 1, 1, &e11);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_asin(A);
        check_bool("mat_asin([[0]]) not NULL", R != NULL);
        if (R) {
            double got; mat_get(R, 0, 0, &got);
            check_d("asin([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* 1×1: asin([[0.5]]) = [[π/6]] */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_asin(A);
        check_bool("mat_asin([[0.5]]) not NULL", R != NULL);
        if (R) {
            print_md("asin(A)", R);
            double got; mat_get(R, 0, 0, &got);
            check_d("asin([[0.5]]) = π/6", got, M_PI / 6.0, 1e-12);
        }
        mat_free(A); mat_free(R);
    }

    /* sin(asin(A)) = A for 2×2 symmetric with small eigenvalues */
    {
        matrix_t *A = mat_create_d(2, 2);
        double v00=0.3, v01=0.1, v10=0.1, v11=0.2;
        mat_set(A, 0, 0, &v00); mat_set(A, 0, 1, &v01);
        mat_set(A, 1, 0, &v10); mat_set(A, 1, 1, &v11);
        print_md("A", A);
        matrix_t *S = mat_asin(A);
        check_bool("mat_asin(sym) not NULL", S != NULL);
        if (S) {
            print_md("asin(A)", S);
            matrix_t *R = mat_sin(S);
            check_bool("sin(asin(A)) not NULL", R != NULL);
            if (R) {
                print_md("sin(asin(A))", R);
                double r00, r01, r10, r11;
                mat_get(R, 0, 0, &r00); mat_get(R, 0, 1, &r01);
                mat_get(R, 1, 0, &r10); mat_get(R, 1, 1, &r11);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_acos(A);
        check_bool("mat_acos([[0.5]]) not NULL", R != NULL);
        if (R) {
            print_md("acos(A)", R);
            double got; mat_get(R, 0, 0, &got);
            check_d("acos([[0.5]]) = π/3", got, M_PI / 3.0, 1e-12);
        }
        mat_free(A); mat_free(R);
    }

    /* asin(A) + acos(A) = (π/2)·I for diagonal [[0.3, 0], [0, 0.4]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, v0=0.3, v1=0.4;
        mat_set(A, 0, 0, &v0); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z);  mat_set(A, 1, 1, &v1);
        print_md("A", A);
        matrix_t *AS = mat_asin(A);
        matrix_t *AC = mat_acos(A);
        check_bool("mat_asin+mat_acos: both not NULL", AS != NULL && AC != NULL);
        if (AS && AC) {
            matrix_t *SUM = mat_add(AS, AC);
            check_bool("asin+acos not NULL", SUM != NULL);
            if (SUM) {
                print_md("asin(A)+acos(A)", SUM);
                double s00, s01, s10, s11;
                mat_get(SUM, 0, 0, &s00); mat_get(SUM, 0, 1, &s01);
                mat_get(SUM, 1, 0, &s10); mat_get(SUM, 1, 1, &s11);
                check_d("asin+acos[0,0] = π/2", s00, M_PI / 2.0, 1e-10);
                check_d("asin+acos[1,1] = π/2", s11, M_PI / 2.0, 1e-10);
                check_d("asin+acos[0,1] = 0",   s01, 0.0,        1e-10);
                check_d("asin+acos[1,0] = 0",   s10, 0.0,        1e-10);
                mat_free(SUM);
            }
        }
        mat_free(AS); mat_free(AC); mat_free(A);
    }
}

/* ------------------------------------------------------------------ mat_atan */

static void test_mat_atan_d(void)
{
    printf(C_CYAN "TEST: mat_atan (double)\n" C_RESET);

    /* 1×1: atan([[0]]) = [[0]] */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_atan(A);
        check_bool("mat_atan([[0]]) not NULL", R != NULL);
        if (R) {
            double got; mat_get(R, 0, 0, &got);
            check_d("atan([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* 1×1: atan([[0.5]]) — use 0.5 (well within convergence radius) */
    {
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.5;
        mat_set(A, 0, 0, &v);
        print_md("A", A);
        matrix_t *R = mat_atan(A);
        check_bool("mat_atan([[0.5]]) not NULL", R != NULL);
        if (R) {
            print_md("atan(A)", R);
            double got; mat_get(R, 0, 0, &got);
            check_d("atan([[0.5]]) = atan(0.5)", got, atan(0.5), 1e-12);
        }
        mat_free(A); mat_free(R);
    }

    /* tan(atan(A)) = A for small diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, a=0.5, b=0.3;
        mat_set(A, 0, 0, &a); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atan(A);
        check_bool("mat_atan(diag) not NULL", T != NULL);
        if (T) {
            print_md("atan(A)", T);
            matrix_t *R = mat_tan(T);
            check_bool("tan(atan(A)) not NULL", R != NULL);
            if (R) {
                print_md("tan(atan(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00); mat_get(R, 1, 1, &r11);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_asinh(A);
        check_bool("mat_asinh([[0]]) not NULL", R != NULL);
        if (R) {
            double got; mat_get(R, 0, 0, &got);
            check_d("asinh([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* sinh(asinh(A)) = A for diagonal [[0.5, 0],[0, 0.3]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, a=0.5, b=0.3;
        mat_set(A, 0, 0, &a); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_asinh(A);
        check_bool("mat_asinh(diag) not NULL", S != NULL);
        if (S) {
            print_md("asinh(A)", S);
            matrix_t *R = mat_sinh(S);
            check_bool("sinh(asinh(A)) not NULL", R != NULL);
            if (R) {
                print_md("sinh(asinh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00); mat_get(R, 1, 1, &r11);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 1.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_acosh(A);
        check_bool("mat_acosh([[1]]) not NULL", R != NULL);
        if (R) {
            double got; mat_get(R, 0, 0, &got);
            check_d("acosh([[1]]) = 0", got, 0.0, 1e-10);
            mat_free(R);
        }
        mat_free(A);
    }

    /* cosh(acosh(A)) = A for diagonal [[2, 0],[0, 3]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, a=2.0, b=3.0;
        mat_set(A, 0, 0, &a); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *S = mat_acosh(A);
        check_bool("mat_acosh(diag) not NULL", S != NULL);
        if (S) {
            print_md("acosh(A)", S);
            matrix_t *R = mat_cosh(S);
            check_bool("cosh(acosh(A)) not NULL", R != NULL);
            if (R) {
                print_md("cosh(acosh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00); mat_get(R, 1, 1, &r11);
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
        matrix_t *A = mat_create_d(1, 1);
        double v = 0.0;
        mat_set(A, 0, 0, &v);
        matrix_t *R = mat_atanh(A);
        check_bool("mat_atanh([[0]]) not NULL", R != NULL);
        if (R) {
            double got; mat_get(R, 0, 0, &got);
            check_d("atanh([[0]]) = 0", got, 0.0, 1e-12);
            mat_free(R);
        }
        mat_free(A);
    }

    /* tanh(atanh(A)) = A for diagonal [[0.4, 0],[0, 0.2]] */
    {
        matrix_t *A = mat_create_d(2, 2);
        double z=0.0, a=0.4, b=0.2;
        mat_set(A, 0, 0, &a); mat_set(A, 0, 1, &z);
        mat_set(A, 1, 0, &z); mat_set(A, 1, 1, &b);
        print_md("A", A);
        matrix_t *T = mat_atanh(A);
        check_bool("mat_atanh(diag) not NULL", T != NULL);
        if (T) {
            print_md("atanh(A)", T);
            matrix_t *R = mat_tanh(T);
            check_bool("tanh(atanh(A)) not NULL", R != NULL);
            if (R) {
                print_md("tanh(atanh(A))", R);
                double r00, r11;
                mat_get(R, 0, 0, &r00); mat_get(R, 1, 1, &r11);
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
    check_bool("mat_sqrt(NULL)  = NULL", mat_sqrt(NULL)  == NULL);
    check_bool("mat_log(NULL)   = NULL", mat_log(NULL)   == NULL);
    check_bool("mat_asin(NULL)  = NULL", mat_asin(NULL)  == NULL);
    check_bool("mat_acos(NULL)  = NULL", mat_acos(NULL)  == NULL);
    check_bool("mat_atan(NULL)  = NULL", mat_atan(NULL)  == NULL);
    check_bool("mat_asinh(NULL) = NULL", mat_asinh(NULL) == NULL);
    check_bool("mat_acosh(NULL) = NULL", mat_acosh(NULL) == NULL);
    check_bool("mat_atanh(NULL) = NULL", mat_atanh(NULL) == NULL);

    matrix_t *A = mat_create_d(2, 3);
    check_bool("mat_sqrt(non-sq)  = NULL", mat_sqrt(A)  == NULL);
    check_bool("mat_log(non-sq)   = NULL", mat_log(A)   == NULL);
    check_bool("mat_asin(non-sq)  = NULL", mat_asin(A)  == NULL);
    check_bool("mat_acos(non-sq)  = NULL", mat_acos(A)  == NULL);
    check_bool("mat_atan(non-sq)  = NULL", mat_atan(A)  == NULL);
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
    matrix_t *A = mat_create_d(2, 2);
    double a00=3.0, a01=1.0, a10=0.0, a11=2.0;
    mat_set(A, 0, 0, &a00); mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10); mat_set(A, 1, 1, &a11);
    print_md("A", A);

    double eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("mat_eigendecompose_general: rc = 0", rc == 0);
    check_bool("mat_eigendecompose_general: V not NULL", V != NULL);

    if (V) {
        print_md("V (eigenvectors)", V);
        /* Verify A·v_j = lambda_j · v_j for each column */
        for (int j = 0; j < 2; j++) {
            double lam = eigenvalues[j];
            double v0, v1;
            mat_get(V, 0, j, &v0);
            mat_get(V, 1, j, &v1);
            /* [Av]_0 = 3*v0 + 1*v1, [Av]_1 = 2*v1 */
            double av0 = 3.0*v0 + 1.0*v1;
            double av1 = 2.0*v1;
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
    matrix_t *A = mat_create_qf(2, 2);
    qfloat_t a00 = qf_from_double(4.0), a01 = qf_from_double(1.0);
    qfloat_t a10 = QF_ZERO,             a11 = QF_ONE;
    mat_set(A, 0, 0, &a00); mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10); mat_set(A, 1, 1, &a11);
    print_mqf("A", A);

    qfloat_t eigenvalues[2];
    matrix_t *V = NULL;
    int rc = mat_eigendecompose(A, eigenvalues, &V);
    check_bool("qf eigendecompose_general: rc = 0", rc == 0);
    check_bool("qf eigendecompose_general: V not NULL", V != NULL);

    if (V) {
        print_mqf("V", V);
        for (int j = 0; j < 2; j++) {
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
    matrix_t *A = matsq_create_qc(2);

    qcomplex_t a00 = qc_make(qf_from_double(2.0), QF_ZERO);
    qcomplex_t a01 = qc_make(qf_from_double(1.0), qf_from_double( 1.0));
    qcomplex_t a10 = qc_make(qf_from_double(1.0), qf_from_double(-1.0));
    qcomplex_t a11 = qc_make(qf_from_double(3.0), QF_ZERO);

    mat_set(A, 0, 0, &a00);
    mat_set(A, 0, 1, &a01);
    mat_set(A, 1, 0, &a10);
    mat_set(A, 1, 1, &a11);

    print_mqc("A", A);

    qcomplex_t ev[2];
    matrix_t  *V = NULL;
    mat_eigendecompose(A, ev, &V);

    print_qc("eigenvalue[0]", ev[0]);
    print_qc("eigenvalue[1]", ev[1]);
    print_mqc("eigenvectors V (columns)", V);

    /* sort so ev[min] <= ev[max] */
    int e0_smaller = qf_to_double(ev[0].re) < qf_to_double(ev[1].re);
    qcomplex_t ev_min = e0_smaller ? ev[0] : ev[1];
    qcomplex_t ev_max = e0_smaller ? ev[1] : ev[0];
    size_t     k_min  = e0_smaller ? 0 : 1;
    size_t     k_max  = e0_smaller ? 1 : 0;

    check_qc_val("eigenvalue min = 1", ev_min, qc_make(qf_from_double(1.0), QF_ZERO), 1e-25);
    check_qc_val("eigenvalue max = 4", ev_max, qc_make(qf_from_double(4.0), QF_ZERO), 1e-25);

    /* verify A*v[k] = lambda[k]*v[k] for each eigenvector */
    for (size_t k = 0; k < 2; k++) {
        for (size_t i = 0; i < 2; i++) {
            qcomplex_t Av_ik = QC_ZERO;
            for (size_t j = 0; j < 2; j++) {
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
    (void)k_min; (void)k_max;

    mat_free(A);
    mat_free(V);
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

    RUN_TEST(test_eigen_d,  NULL);
    RUN_TEST(test_eigen_qf, NULL);
    RUN_TEST(test_eigen_qc, NULL);

    RUN_TEST(test_mat_exp_d,           NULL);
    RUN_TEST(test_mat_exp_qf,          NULL);
    RUN_TEST(test_mat_exp_qc,          NULL);
    RUN_TEST(test_mat_exp_null_safety, NULL);

    RUN_TEST(test_mat_sin_d,           NULL);
    RUN_TEST(test_mat_sin_qf,          NULL);
    RUN_TEST(test_mat_sin_qc,          NULL);
    RUN_TEST(test_mat_sin_null_safety, NULL);

    RUN_TEST(test_mat_cos_d,           NULL);
    RUN_TEST(test_mat_cos_qf,          NULL);
    RUN_TEST(test_mat_cos_qc,          NULL);

    RUN_TEST(test_mat_tan_d,           NULL);

    RUN_TEST(test_mat_sinh_d,          NULL);
    RUN_TEST(test_mat_cosh_d,          NULL);
    RUN_TEST(test_mat_tanh_d,          NULL);

    RUN_TEST(test_mat_trig_null_safety, NULL);

    RUN_TEST(test_mat_sqrt_d,          NULL);
    RUN_TEST(test_mat_sqrt_qf,         NULL);

    RUN_TEST(test_mat_log_d,           NULL);

    RUN_TEST(test_mat_asin_d,          NULL);
    RUN_TEST(test_mat_acos_d,          NULL);
    RUN_TEST(test_mat_atan_d,          NULL);

    RUN_TEST(test_mat_asinh_d,         NULL);
    RUN_TEST(test_mat_acosh_d,         NULL);
    RUN_TEST(test_mat_atanh_d,         NULL);

    RUN_TEST(test_mat_inv_trig_null_safety, NULL);

    RUN_TEST(test_eigen_general_d,     NULL);
    RUN_TEST(test_eigen_general_qf,    NULL);

    RUN_TEST(test_readme_example, NULL);

    return tests_failed;
}
