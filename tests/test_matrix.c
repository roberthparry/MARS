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

/* test-local compact qfloat formatter */
static void qf_to_string_short(qfloat_t x, char *buf, size_t n)
{
    /* If the qfloat is exactly representable as a double, print short */
    if (x.lo == 0.0 && isfinite(x.hi)) {
        snprintf(buf, n, "%.16g", x.hi);
        return;
    }

    /* Otherwise fall back to full precision */
    qf_to_string(x, buf, n);
}

/* ------------------------------------------------------------------ updated qfloat coloured printer */

static void qf_to_coloured_string(qfloat_t x, char *out, size_t out_size)
{
    char shortbuf[256];
    char fullbuf[256];

    /* compact form */
    qf_to_string_short(x, shortbuf, sizeof(shortbuf));

    /* full precision */
    qf_to_string(x, fullbuf, sizeof(fullbuf));

    /* choose compact if it's short and has no exponent */
    const char *chosen =
        (strchr(shortbuf, 'e') == NULL && strlen(shortbuf) <= 12)
            ? shortbuf
            : fullbuf;

    if (strcmp(chosen, "0") == 0)
        snprintf(out, out_size, C_GREY "0" C_RESET);
    else
        snprintf(out, out_size, C_YELLOW "%s" C_RESET, chosen);
}

/* test-local compact qcomplex formatter */
static void qc_to_string_short(qcomplex_t z, char *buf, size_t n)
{
    char re[128], im[128];

    qf_to_string_short(z.re, re, sizeof(re));
    qf_to_string_short(z.im, im, sizeof(im));

    /* If both parts are short, use compact form */
    if (strlen(re) <= 12 && strlen(im) <= 12) {
        snprintf(buf, n, "%s %s %si",
                 re,
                 (z.im.hi >= 0 ? "+" : "-"),
                 (z.im.hi >= 0 ? im : im + 1));
        return;
    }

    /* Otherwise full precision */
    qc_to_string(z, buf, n);
}

/* ------------------------------------------------------------------ updated qcomplex coloured printer */

static void qc_to_coloured_string(qcomplex_t z, char *out, size_t out_size)
{
    char shortbuf[256];
    char fullbuf[256];

    qc_to_string_short(z, shortbuf, sizeof(shortbuf));
    qc_to_string(z, fullbuf, sizeof(fullbuf));

    /* parse short form */
    char r_short[128], s_short[8], i_short[128];
    sscanf(shortbuf, "%127s %7s %127s", r_short, s_short, i_short);

    /* choose compact only if both parts are short */
    const char *chosen =
        (strlen(r_short) <= 12 && strlen(i_short) <= 12)
            ? shortbuf
            : fullbuf;

    /* parse chosen */
    char real[128], sign[8], imag[128];
    sscanf(chosen, "%127s %7s %127s", real, sign, imag);

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

    return tests_failed;
}
